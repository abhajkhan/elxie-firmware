# 02 — Bytecode format and VM semantics (v1)

The reference implementations are `tools/elxasm.py` (assembler) and `firmware/components/elx_vm/` (VM). Golden tests: `spec/test-vectors/`.

## Image layout (`.elxb`, little-endian)

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | magic `"ELXB"` |
| 4 | 1 | version (= 1) |
| 5 | 1 | flags (0, reserved) |
| 6 | 1 | num_scripts (1–8) |
| 7 | 1 | num_vars (0–32) |
| 8 | 2 | code_len |
| 10 | 2 | strings_len |
| 12 | 16 | name, UTF-8, NUL-padded |
| 28 | 4 × n | script table: `event u8, param u8, code_offset u16` |
| … | code_len | code |
| … | strings_len | NUL-terminated strings (referenced by byte offset) |
| end−4 | 4 | CRC-32 (IEEE, zlib) of every byte before it |

Max image size **4096 bytes**.

## Validation (robot side, before anything runs)

Magic, version, exact length, CRC, limits; every opcode valid; no instruction runs past the end; every jump and every script offset lands on an instruction start; every variable index < num_vars; every syscall and event supported by *this* firmware (capability bitmap in the `INFO` reply). A program that passes cannot crash the VM — only hit the runtime errors below.

## Values and instructions

- One type: **float32**. Booleans are 1/0. Colors are `0xRRGGBB`. Strings exist only as literal offsets for `show_text`.
- Stack machine, **16 slots per script**. Opcodes in `opcodes.json`.
- Kid-friendly math: `x ÷ 0 = 0`, `mod` is floored (sign follows the divisor, like Scratch), `=` is tolerant (0.1 + 0.2 = 0.3), `random a b` returns an integer when both ends are integers.

## Scheduling

- The VM ticks every **10 ms**. Each script is a thread (max 8).
- A thread runs until it: waits (`WAIT`, or a blocking syscall), yields, **takes a backward jump** (one loop iteration per tick → `forever` runs at 100 Hz, and a busy loop can't starve other scripts), ends, or spends 500 instructions.
- Blocking syscalls (`drive_timed`, `turn_degrees`, `play_note`, `play_sound`, `rest`, `react`) return a wait handle. Any newer command on the same resource (motors, buzzer, reaction) **supersedes** the old job and releases the waiting script.
- Events are edge-triggered and ignored while their script still runs.
- A program with no event scripts ends when all scripts end. A program with event scripts keeps listening until stopped.
- Runtime errors (`STACK_OVERFLOW`, `STACK_UNDERFLOW`, `BAD_SYSCALL`, `BAD_STRING`) stop the program, show the dizzy face, and send `ERROR {code, script, pc}` to the app.

## Compiler rules (the app's compiler MUST follow these)

1. **Every script ends with `END`** (otherwise it falls through into the next script's code). The assembler inserts it automatically.
2. Statement blocks leave the stack exactly as they found it; reporters push exactly one value.
3. Arguments are pushed in the order listed in `opcodes.json`.
4. Constants: integer in −128…127 → `PUSH_I8`; in −32768…32767 → `PUSH_I16`; anything else → `PUSH_F32`.
5. Script order in the table = order of hat blocks on the canvas, sorted top-to-bottom then left-to-right (deterministic output → golden tests).
6. Emit a **source map** `code_offset → block id` so `TRACE` pcs can highlight blocks.
7. Do a static stack-depth check (≤ 16) and report "this block is too deeply nested" instead of letting the robot hit `STACK_OVERFLOW`.

### Control-flow patterns

```
repeat N { body }          forever { body }        if C { A } else { B }
    <N>                    L:  <body>                  <C>
L:  dup                        jmp L                   jz ELSE
    push 0                                             <A>
    gt                     wait until C                jmp END
    jz E                   L:  <C>                 ELSE:
    <body>                     jnz E                   <B>
    push 1                     jmp L               END:
    sub                    E:
    jmp L                                          repeat until C { body }
E:  pop                                            L:  <C>
                                                       jnz E
                                                       <body>
                                                       jmp L
                                                   E:
```

The `repeat` counter lives on the stack, so nesting costs one stack slot per level.

## Versioning

- Append new opcodes/syscalls/events; **never renumber**.
- New syscalls don't need a version bump: the robot advertises what it supports, and the app greys out blocks the connected robot can't run.
- Bump `bytecode_version` only for breaking changes to the format or semantics.
