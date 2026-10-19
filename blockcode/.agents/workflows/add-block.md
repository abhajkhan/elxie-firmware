---
description: Add a new block end-to-end (spec → firmware → compiler → editor → simulator → tests)
---
Ask for: block text, category, tier(s), arguments with types/ranges, whether it waits, what it returns.
1. Append the syscall (or event) to `spec/opcodes.json` with the next free id in its category range. Add enum values if it has a dropdown.
2. `python3 tools/gen_opcodes.py`.
3. Firmware: implement it in `firmware/main/robot_host.cpp` (and a service if needed). Keep the loop non-blocking.
4. Add a golden program in `spec/test-vectors/` using it; run `tests/run_tests.sh --update` and check the trace makes sense.
5. Blocks: JSON definition in `packages/blocks/src/defs/<category>.json`, registry entry, toolbox entry for its tier(s), i18n strings.
6. Compiler: only needed if it's not a plain syscall; add a fixture matching the golden program.
7. Simulator: implement it in `SimRobot`.
8. Update `spec/01-blocks.md`. Run the `verify` workflow.
