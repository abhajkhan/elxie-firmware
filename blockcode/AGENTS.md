# AGENTS.md — standing rules for every agent in this workspace

You are working on **Elxie Block Code**, a block-coding platform (ages 5–12) for the Elxie robot (ESP32-S3).
Read `docs/ARCHITECTURE.md` before planning anything. Read the relevant `spec/*.md` before touching code in that area.

## The contract comes first
- `spec/opcodes.json` is the single source of truth for opcodes, syscalls, events, enums, limits and error codes.
- **Never hand-edit** `firmware/components/elx_vm/include/opcodes_gen.h` or `packages/protocol/src/opcodes.gen.ts`. Edit the JSON, run `python3 tools/gen_opcodes.py`, commit both.
- Never renumber an existing id. Only append. A behavior change to an existing opcode/syscall needs a `bytecode_version` bump and a note in `spec/02-bytecode.md`.
- `tools/elxasm.py` is the reference compiler backend; `tools/elxie_cli.py` is the reference protocol client. TypeScript must match them byte-for-byte.

## Always run before you say "done"
- `tests/run_tests.sh` — VM golden traces + validator (must pass).
- `tests/host_compile/check.sh` — firmware type-check against the ESP-IDF 5.3 API stubs (must pass). On a machine with ESP-IDF, also `cd firmware && idf.py build`.
- `pnpm -r test` once the TypeScript packages exist.
If you change VM behavior on purpose: `tests/run_tests.sh --update`, then show the trace diff in your summary.

## Boundaries
- `firmware/components/elx_vm/` stays **portable C++17**: no ESP-IDF, no FreeRTOS, no heap allocation. It is compiled for the robot, the PC tests and WebAssembly.
- Hardware constants go in `firmware/main/board_config.h`, nowhere else.
- Package dependency direction: `apps/studio → packages/*`; `compiler`, `simulator`, `blocks` may import only constants from `protocol`. No cycles.
- The compiler must follow every rule in `spec/02-bytecode.md` → "Compiler rules" (END per script, argument order, PUSH sizing, source map, stack-depth check).

## Code style
- TypeScript strict, no `any` in package APIs. React function components + hooks. Zustand for state.
- C++: no exceptions, no RTTI, fixed-size buffers, `constexpr` over macros where possible (except `board_config.h`).
- Small files, one responsibility each. Name things after the spec (`drive_timed`, `TRACE_DATA`).

## Product rules (it's for children)
- Every user-facing string goes through i18n and is written for an 8-year-old: short, friendly, says what to do next.
- Touch targets ≥ 44 px. Run and Stop are always one tap. Never lose a child's work (autosave).
- Safety features in firmware (cliff guard, e-stop, speed cap, validator) must never be made optional for programs.

## When unsure
Ask in the plan instead of guessing, especially about hardware pins, protocol changes, or anything that changes what an existing block does.
