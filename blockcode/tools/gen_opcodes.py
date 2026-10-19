#!/usr/bin/env python3
"""Generate firmware + app constants from spec/opcodes.json.

    python tools/gen_opcodes.py

Writes:
  firmware/components/elx_vm/include/opcodes_gen.h          (C++ for the robot VM)
  packages/protocol/src/opcodes.gen.ts   (TypeScript for the app compiler)
  apps/studio/js/spec.gen.js             (Elxie Blocks editor)
Run it after every change to spec/opcodes.json and commit the outputs.
"""
import json
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPEC = json.load(open(os.path.join(ROOT, "spec", "opcodes.json")))

OPERAND_SIZE = {"u8": 1, "i8": 1, "u16": 2, "i16": 2, "f32": 4}


def c_ident(s):
    return re.sub(r"[^A-Za-z0-9]", "_", s).upper()


def gen_c():
    L = []
    w = L.append
    w("// AUTO-GENERATED from spec/opcodes.json by tools/gen_opcodes.py. DO NOT EDIT.")
    w("#pragma once")
    w("#include <stdint.h>")
    w("")
    w("namespace elx {")
    w("")
    w(f"constexpr uint8_t BYTECODE_VERSION = {SPEC['bytecode_version']};")
    for k, v in SPEC["limits"].items():
        w(f"constexpr uint32_t LIMIT_{k.upper()} = {v};")
    w("")
    w("enum Op : uint8_t {")
    for op in SPEC["opcodes"]:
        w(f"  OP_{op['name']} = {op['code']},")
    w("};")
    w("")
    w("// Instruction length in bytes (opcode + operands); 0 = invalid opcode.")
    lens = [0] * 256
    for op in SPEC["opcodes"]:
        lens[op["code"]] = 1 + sum(OPERAND_SIZE[t] for t in op["operands"])
    w("static const uint8_t OP_LENGTH[256] = {")
    for i in range(0, 256, 16):
        w("  " + ", ".join(str(x) for x in lens[i:i + 16]) + ",")
    w("};")
    w("")
    w("enum Event : uint8_t {")
    for e in SPEC["events"]:
        w(f"  EV_{e['name']} = {e['id']},")
    w("};")
    w("")
    w("enum Sys : uint8_t {")
    for s in SPEC["syscalls"]:
        w(f"  SYS_{c_ident(s['name'])} = {s['id']},")
    w("};")
    w("")
    w("constexpr uint8_t SYSF_VALID = 1, SYSF_RETURNS = 2, SYSF_BLOCKING = 4, SYSF_V1 = 8;")
    w("struct SyscallInfo { uint8_t argc; uint8_t flags; };")
    table = [(0, 0)] * 256
    for s in SPEC["syscalls"]:
        f = 1
        if s["returns"]:
            f |= 2
        if s["blocking"]:
            f |= 4
        if s["stage"] == "v1":
            f |= 8
        table[s["id"]] = (len(s["args"]), f)
    w("static const SyscallInfo SYSCALL_INFO[256] = {")
    for i in range(0, 256, 8):
        w("  " + ", ".join("{%d,%d}" % t for t in table[i:i + 8]) + ",")
    w("};")
    w("")
    w("// Block dropdown values. Use as elx::enums::face::HAPPY (kept in their own")
    w("// namespace so names like 'face' or 'sound' don't clash with firmware objects).")
    w("namespace enums {")
    for name, vals in SPEC["enums"].items():
        w(f"namespace {name} {{ enum : uint8_t {{ " +
          ", ".join(f"{c_ident(k)} = {v}" for k, v in vals.items()) + " }; }")
    w("}  // namespace enums")
    w("")
    w("#ifdef ELX_WITH_NAMES  // debug/PC builds only")
    names = ["nullptr"] * 256
    for sc in SPEC["syscalls"]:
        names[sc["id"]] = '"%s"' % sc["name"]
    w("static const char* const SYSCALL_NAMES[256] = {")
    for i in range(0, 256, 8):
        w("  " + ", ".join(names[i:i + 8]) + ",")
    w("};")
    ev = ["nullptr"] * 32
    for e in SPEC["events"]:
        ev[e["id"]] = '"%s"' % e["name"].lower()
    w("static const char* const EVENT_NAMES[32] = {" + ", ".join(ev) + "};")
    w("#endif")
    w("")
    w("enum class LoadError : uint8_t {")
    for k, v in SPEC["errors"]["load"].items():
        w(f"  {k} = {v},")
    w("};")
    w("enum class VmError : uint8_t {")
    for k, v in SPEC["errors"]["runtime"].items():
        w(f"  {k} = {v},")
    w("};")
    w("")
    w("}  // namespace elx")
    return "\n".join(L) + "\n"


def gen_ts():
    L = []
    w = L.append
    w("// AUTO-GENERATED from spec/opcodes.json by tools/gen_opcodes.py. DO NOT EDIT.")
    w(f"export const BYTECODE_VERSION = {SPEC['bytecode_version']};")
    w(f"export const LIMITS = {json.dumps(SPEC['limits'])} as const;")
    w("")
    w("export const Op = {")
    for op in SPEC["opcodes"]:
        w(f"  {op['name']}: {op['code']},")
    w("} as const;")
    w("export type OpName = keyof typeof Op;")
    w("")
    w("export const OP_OPERANDS: Record<number, readonly string[]> = {")
    for op in SPEC["opcodes"]:
        w(f"  {op['code']}: {json.dumps(op['operands'])},")
    w("};")
    w("")
    w("export const Event = {")
    for e in SPEC["events"]:
        w(f"  {e['name']}: {e['id']},")
    w("} as const;")
    w("")
    w("export interface SyscallDef { id: number; name: string; args: string[]; returns: boolean; blocking: boolean; stage: 'v1' | 'later'; category: string; }")
    w("export const SYSCALLS: Record<string, SyscallDef> = {")
    for s in SPEC["syscalls"]:
        d = {k: s[k] for k in ("id", "name", "args", "returns", "blocking", "stage", "category")}
        w(f"  {s['name']}: {json.dumps(d)},")
    w("};")
    w("")
    w(f"export const ENUMS = {json.dumps(SPEC['enums'], indent=2)} as const;")
    w("")
    w(f"export const LOAD_ERRORS = {json.dumps({v: k for k, v in SPEC['errors']['load'].items()})} as Record<number, string>;")
    w(f"export const RUNTIME_ERRORS = {json.dumps({v: k for k, v in SPEC['errors']['runtime'].items()})} as Record<number, string>;")
    return "\n".join(L) + "\n"


def write(rel, text):
    path = os.path.join(ROOT, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        f.write(text)
    print("wrote", rel)


def gen_studio_js():
    d = {k: SPEC[k] for k in ("bytecode_version", "limits", "opcodes", "events", "syscalls", "enums", "errors")}
    return ("// AUTO-GENERATED from spec/opcodes.json by tools/gen_opcodes.py. DO NOT EDIT.\n"
            "window.ELX_SPEC = " + json.dumps(d, separators=(",", ":")) + ";\n")


if __name__ == "__main__":
    write("firmware/components/elx_vm/include/opcodes_gen.h", gen_c())
    write("packages/protocol/src/opcodes.gen.ts", gen_ts())
    write("apps/studio/js/spec.gen.js", gen_studio_js())
