# Elxie Code

Block-coding platform for the **Elxie** robot (ESP32-S3), ages 5–12.

> **In this repo:** this folder is the Block Code platform (spec, VM, tools, editor, tests).
> The robot runs the **Home firmware** at the repo root, which contains the same VM in
> `src/blocks/vm/` (build/flash with `FLASH_HOME_FIRMWARE.bat` in the repo root).
> `firmware/` here is the earlier standalone ESP-IDF Block Code firmware, kept for reference
> and because the robot testbench (`../testbench`) uses its pin map and HAL. Do not flash it on pilot robots.

Kids snap blocks together in the app; the app compiles them to compact bytecode, sends it over BLE, and a small virtual machine inside the robot's firmware runs it — with or without the phone nearby.

## What's in this repo

| Path | What | Status |
|---|---|---|
| `spec/` | Locked block catalog, bytecode format, BLE protocol, golden test programs. `opcodes.json` is the single source of truth. | ✅ v1 |
| `firmware/` | Standalone block-coding firmware (native ESP-IDF 5.3+): VM, 8 program slots, BLE + USB link, gyro turns, compass, cliff guard, OLED face, 30-LED lights, sound, TTP223 touch, ESP-NOW remote, on-robot launcher | ✅ written, type-checked; **needs first `idf.py build` on your PC** |
| `tools/` | `gen_opcodes.py` (code generator), `elxasm.py` (reference assembler/disassembler), `elxie_cli.py` (USB/BLE client) | ✅ |
| `tests/` | VM golden traces + validator tests (ASan/UBSan), firmware type-check against ESP-IDF 5.3 APIs | ✅ passing |
| `packages/simulator/wasm/` | C bridge to run the robot's VM in the browser via WebAssembly | ✅ bridge; app side to build |
| `packages/protocol/src/opcodes.gen.ts` | Generated TS constants for the app | ✅ generated |
| `docs/ARCHITECTURE.md` | Software architecture for the app (editor, compiler, simulator, robot client) | ✅ |
| `docs/ANTIGRAVITY_PLAYBOOK.md` | Milestones + copy-paste prompts for Antigravity agents | ✅ |
| `AGENTS.md`, `.agents/` | Standing rules and workflows for Antigravity | ✅ |

## Quick start

```bash
# 1. Run the VM tests (needs g++ and python3)
tests/run_tests.sh
tests/host_compile/check.sh

# 2. Build and flash the firmware (ESP-IDF v5.3+)
cd firmware && idf.py set-target esp32s3 && idf.py build flash monitor

# 3. Talk to the robot from your PC (pip install pyserial bleak)
python tools/elxie_cli.py --port /dev/ttyACM0 info
python tools/elxie_cli.py --port /dev/ttyACM0 upload spec/test-vectors/03_parallel.elxa --slot 1 --run
python tools/elxie_cli.py --ble telemetry

# 4. Write a program by hand
python tools/elxasm.py my_program.elxa         # -> my_program.elxb
python tools/elxasm.py --dis my_program.elxb   # inspect bytecode
```

A tiny program in the assembly the compiler targets:

```
.name "Square dance"
.script start
    push 4
loop:
    dup
    push 0
    gt
    jz done
    push drive.forward
    push 50
    push 1
    sys drive_timed      ; drive forward at 50% for 1 s (gyro keeps it straight)
    push turn.right
    push 90
    sys turn_degrees     ; gyro-controlled 90° turn
    push 1
    sub
    jmp loop
done:
    pop
    push emotion.happy
    sys react
```

## Read next

1. `spec/01-blocks.md` — every block and what it does
2. `docs/ARCHITECTURE.md` — how the app is built
3. `docs/FIRMWARE.md` — firmware internals, pin assumptions, bring-up checklist
4. `docs/ANTIGRAVITY_PLAYBOOK.md` — how to build the rest with Antigravity
