#!/usr/bin/env python3
"""Elxie assembler / disassembler (reference implementation of the bytecode format).

The app's Blockly compiler must produce byte-identical output for the same
program; spec/test-vectors/*.elxa are the shared golden tests.

Usage:
    python tools/elxasm.py prog.elxa -o prog.elxb     # assemble
    python tools/elxasm.py prog.elxa --hex            # print hex
    python tools/elxasm.py --dis prog.elxb            # disassemble

Assembly syntax (one statement per line, ';' starts a comment):
    .name "Square dance"          program name (max 16 bytes)
    .var speed                    declare variable (indexes in order)
    .string hi "Hello!"           string constant, use as  push str.hi
    .script start                 start a new script (event + optional param)
    .script touched head          params may be numbers or enum names
    .script obstacle 15
    loop:                         label
        push 50                   number (I8/I16/F32 chosen automatically)
        push #FF8800              color -> 0xFF8800
        push face.happy           enum value from spec/opcodes.json
        load speed | store speed | change speed
        jmp loop | jz done | jnz loop
        sys drive_timed           syscall by name
        wait | end | stop_all | yield | pop | dup | add | lt | ...
"""
import argparse
import json
import os
import shlex
import struct
import sys
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPEC = json.load(open(os.path.join(ROOT, "spec", "opcodes.json")))

OPS = {o["name"].lower(): o for o in SPEC["opcodes"]}
OPS_BY_CODE = {o["code"]: o for o in SPEC["opcodes"]}
SYS = {s["name"]: s for s in SPEC["syscalls"]}
SYS_BY_ID = {s["id"]: s for s in SPEC["syscalls"]}
EVENTS = {e["name"].lower(): e for e in SPEC["events"]}
EVENTS_BY_ID = {e["id"]: e for e in SPEC["events"]}
ENUMS = SPEC["enums"]
LIMITS = SPEC["limits"]
SIZE = {"u8": 1, "i8": 1, "u16": 2, "i16": 2, "f32": 4}
FMT = {"u8": "<B", "i8": "<b", "u16": "<H", "i16": "<h", "f32": "<f"}
EVENT_ENUM = {"touched": "pad", "line": "zone", "edge": "corner", "tilted": "tilt", "remote_button": "button"}


class AsmError(Exception):
    pass


def _enum_value(token, default_enum=None):
    if "." in token:
        e, k = token.split(".", 1)
        if e in ENUMS and k in ENUMS[e]:
            return ENUMS[e][k]
    if default_enum and token in ENUMS[default_enum]:
        return ENUMS[default_enum][token]
    return None


def _number(token, strings):
    t = token
    if t.startswith("#") and len(t) == 7:
        return float(int(t[1:], 16))
    if t.startswith("str."):
        name = t[4:]
        if name not in strings:
            raise AsmError(f"unknown string '{name}'")
        return float(strings[name])
    if t.lower() in ("true", "false"):
        return 1.0 if t.lower() == "true" else 0.0
    ev = _enum_value(t)
    if ev is not None:
        return float(ev)
    try:
        return float(int(t, 0))
    except ValueError:
        try:
            return float(t)
        except ValueError:
            raise AsmError(f"unknown value '{t}' (number, #RRGGBB, str.name or enum.value)")


def _push(value):
    if value == int(value):
        v = int(value)
        if -128 <= v <= 127:
            return bytes([OPS["push_i8"]["code"]]) + struct.pack("<b", v)
        if -32768 <= v <= 32767:
            return bytes([OPS["push_i16"]["code"]]) + struct.pack("<h", v)
    return bytes([OPS["push_f32"]["code"]]) + struct.pack("<f", value)


def assemble(text):
    """Return (image_bytes, source_map) where source_map maps code offset -> line number."""
    name = ""
    variables = {}
    lists = {}
    strings = {}          # name -> offset
    string_blob = bytearray()
    scripts = []          # (event_id, param, label_offset_placeholder)
    items = []            # parsed instructions: (line_no, mnemonic, arg)

    # Pass 0: directives + strings (strings must be known before sizing pushes)
    lines = text.splitlines()
    parsed = []
    for n, raw in enumerate(lines, 1):
        line = raw.split(";", 1)[0].strip()
        if not line:
            continue
        try:
            toks = shlex.split(line, posix=True)
        except ValueError as e:
            raise AsmError(f"line {n}: {e}")
        parsed.append((n, toks))
        if toks[0] == ".string":
            if len(toks) != 3:
                raise AsmError(f"line {n}: .string NAME \"text\"")
            strings[toks[1]] = len(string_blob)
            string_blob += toks[2].encode("utf-8") + b"\0"

    # Rule: every script ends with END (otherwise it would fall through into the
    # next script's code). Insert one wherever the author left it out.
    fixed = []
    in_script = False
    last = None
    for n, toks in parsed:
        if toks[0] == ".script":
            if in_script and last != "end":
                fixed.append((n, ["end"]))
            in_script, last = True, None
        elif not toks[0].startswith(".") and not (toks[0].endswith(":") and len(toks) == 1):
            last = toks[0].lower()
        fixed.append((n, toks))
    if in_script and last != "end":
        fixed.append((len(lines) + 1, ["end"]))
    parsed = fixed

    # Pass 1: layout
    labels = {}
    pc = 0
    for n, toks in parsed:
        head = toks[0]
        if head == ".name":
            name = toks[1]
            if len(name.encode()) > 16:
                raise AsmError(f"line {n}: name longer than 16 bytes")
        elif head == ".var":
            for v in toks[1:]:
                if v in variables:
                    raise AsmError(f"line {n}: duplicate var {v}")
                variables[v] = len(variables)
        elif head == ".list":
            for v in toks[1:]:
                if v in lists:
                    raise AsmError(f"line {n}: duplicate list {v}")
                lists[v] = len(lists)
        elif head == ".string":
            pass
        elif head == ".src":       # editor source-map marker: no code
            pass
        elif head == ".script":
            ev = toks[1].lower()
            if ev not in EVENTS:
                raise AsmError(f"line {n}: unknown event '{toks[1]}'")
            param = 0
            if len(toks) > 2:
                pv = _enum_value(toks[2], EVENT_ENUM.get(ev))
                param = pv if pv is not None else int(toks[2], 0)
                if not 0 <= param <= 255:
                    raise AsmError(f"line {n}: event param out of range")
            scripts.append((EVENTS[ev]["id"], param, pc))
        elif head.endswith(":") and len(toks) == 1:
            lab = head[:-1]
            if lab in labels:
                raise AsmError(f"line {n}: duplicate label {lab}")
            labels[lab] = pc
        else:
            m = head.lower()
            if m == "push":
                if len(toks) != 2:
                    raise AsmError(f"line {n}: push needs one value")
                size = len(_push(_number(toks[1], strings)))
            elif m in ("load", "store", "change", "jmp", "jz", "jnz", "sys"):
                size = 2 if m in ("load", "store", "change", "sys") else 3
            elif m.startswith("list_") and m in OPS:
                size = 2
            elif m in OPS and not OPS[m]["operands"]:
                size = 1
            else:
                raise AsmError(f"line {n}: unknown instruction '{head}'")
            items.append((n, m, toks[1:], pc))
            pc += size

    if not scripts:
        raise AsmError("program needs at least one .script")
    if len(scripts) > LIMITS["max_scripts"]:
        raise AsmError(f"too many scripts (max {LIMITS['max_scripts']})")
    if len(lists) > LIMITS["max_lists"]:
        raise AsmError(f"too many lists (max {LIMITS['max_lists']})")
    if len(variables) > LIMITS["max_vars"]:
        raise AsmError(f"too many variables (max {LIMITS['max_vars']})")

    # Pass 2: emit
    code = bytearray()
    source_map = {}
    for n, m, args, at in items:
        assert at == len(code)
        source_map[at] = n
        if m == "push":
            code += _push(_number(args[0], strings))
        elif m in ("load", "store", "change"):
            var = args[0]
            if var not in variables:
                raise AsmError(f"line {n}: unknown variable '{var}'")
            opname = {"load": "LOAD_VAR", "store": "STORE_VAR", "change": "CHANGE_VAR"}[m]
            code += bytes([OPS[opname.lower()]["code"], variables[var]])
        elif m.startswith("list_"):
            if args[0] not in lists:
                raise AsmError(f"line {n}: unknown list '{args[0]}'")
            code += bytes([OPS[m]["code"], lists[args[0]]])
        elif m in ("jmp", "jz", "jnz"):
            if args[0] not in labels:
                raise AsmError(f"line {n}: unknown label '{args[0]}'")
            code += bytes([OPS[m]["code"]]) + struct.pack("<H", labels[args[0]])
        elif m == "sys":
            s = SYS.get(args[0])
            if not s:
                raise AsmError(f"line {n}: unknown syscall '{args[0]}'")
            code += bytes([OPS["sys"]["code"], s["id"]])
        else:
            code += bytes([OPS[m]["code"]])

    header = bytearray(b"ELXB")
    header += struct.pack("<BBBBHH", SPEC["bytecode_version"], len(lists), len(scripts), len(variables),
                          len(code), len(string_blob))
    header += name.encode("utf-8").ljust(16, b"\0")
    for ev, param, off in scripts:
        header += struct.pack("<BBH", ev, param, off)
    body = bytes(header) + bytes(code) + bytes(string_blob)
    image = body + struct.pack("<I", zlib.crc32(body) & 0xFFFFFFFF)
    if len(image) > LIMITS["max_program_bytes"]:
        raise AsmError(f"program too large ({len(image)} > {LIMITS['max_program_bytes']} bytes)")
    return image, source_map


def disassemble(image):
    out = []
    if image[:4] != b"ELXB":
        raise AsmError("not an ELXB image")
    ver, flags, ns, nv, clen, slen = struct.unpack_from("<BBBBHH", image, 4)
    name = image[12:28].rstrip(b"\0").decode("utf-8", "replace")
    crc_ok = zlib.crc32(image[:-4]) & 0xFFFFFFFF == struct.unpack_from("<I", image, len(image) - 4)[0]
    out.append(f"; ELXB v{ver} name='{name}' scripts={ns} vars={nv} code={clen} strings={slen} crc={'ok' if crc_ok else 'BAD'}")
    base = 28 + ns * 4
    entries = {}
    for i in range(ns):
        ev, param, off = struct.unpack_from("<BBH", image, 28 + i * 4)
        entries.setdefault(off, []).append(f".script {EVENTS_BY_ID.get(ev, {'name': ev})['name'].lower()} {param}")
    code = image[base:base + clen]
    pc = 0
    while pc < clen:
        for e in entries.get(pc, []):
            out.append(e)
        op = OPS_BY_CODE.get(code[pc])
        if not op:
            out.append(f"{pc:04x}: .byte {code[pc]}  ; INVALID")
            pc += 1
            continue
        args = []
        p = pc + 1
        for t in op["operands"]:
            v = struct.unpack_from(FMT[t], code, p)[0]
            p += SIZE[t]
            if op["name"] == "SYS":
                args.append(SYS_BY_ID.get(v, {"name": f"#{v}"})["name"])
            elif t == "f32":
                args.append(f"{v:g}")
            elif t == "u16":
                args.append(f"@{v:04x}")
            else:
                args.append(str(v))
        out.append(f"{pc:04x}: {op['name'].lower():10s} {' '.join(args)}")
        pc = p
    if slen:
        strs = image[base + clen:base + clen + slen].split(b"\0")[:-1]
        off = 0
        for s in strs:
            out.append(f"; str @{off}: {s.decode('utf-8', 'replace')!r}")
            off += len(s) + 1
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input")
    ap.add_argument("-o", "--output")
    ap.add_argument("--hex", action="store_true")
    ap.add_argument("--dis", action="store_true", help="disassemble an .elxb")
    a = ap.parse_args()
    try:
        if a.dis:
            print(disassemble(open(a.input, "rb").read()))
            return
        image, _ = assemble(open(a.input, encoding="utf-8").read())
    except AsmError as e:
        print(f"error: {e}", file=sys.stderr)
        sys.exit(1)
    if a.hex:
        print(image.hex())
    out = a.output or os.path.splitext(a.input)[0] + ".elxb"
    if not a.hex or a.output:
        open(out, "wb").write(image)
        print(f"{out}: {len(image)} bytes")


if __name__ == "__main__":
    main()
