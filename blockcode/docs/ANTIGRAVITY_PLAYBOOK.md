# Building Elxie Code in Antigravity — playbook

A step-by-step plan with copy-paste prompts. Each prompt is sized to be one agent task you can review.

## Setup (once)

1. Open the `ElxieBlockCode/` folder as a workspace in Antigravity.
2. Antigravity reads `AGENTS.md` (root) and `.agents/rules/*.md` automatically; the workflows in `.agents/workflows/` (`/verify`, `/add-block`, `/bump-firmware`) can be run as slash commands. If your version doesn't pick them up, paste their contents as the prompt instead.
3. Use **Planning mode** for every milestone task, and read the plan before approving. The plan should name the spec sections it follows.
4. Put the repo in git first. Review every agent change as a diff before accepting.

## How to run agents in parallel

After M0, these tasks only share `spec/`, so they can run as separate agents in Manager view:

```
             ┌─ Agent A: M1 protocol ─┐
M0 ─────────►├─ Agent B: M2 compiler ─┼──► M3 editor ──► M5 full blocks ──► M6 tiers ──► M8 ──► M9
             └─ Agent C: M4 simulator ┘                   M7 robot manager (parallel with M6)
```

Give each parallel agent its own branch or worktree. Merge in the order A → B → C, running `/verify` after each merge.

## Prompts

### M0 — Workspace
> Set up a pnpm monorepo in this workspace following docs/ARCHITECTURE.md §3: `packages/protocol`, `packages/compiler`, `packages/blocks`, `packages/simulator`, `apps/studio` (Vite + React + TS strict). `packages/protocol/src/opcodes.gen.ts` already exists and is generated; export it from the package index. Add Vitest to every package, ESLint + Prettier, and a GitHub Actions workflow that runs `python3 tools/gen_opcodes.py` with a git-diff check, `tests/run_tests.sh`, `tests/host_compile/check.sh` and `pnpm -r test`. No app features yet.

### M1 — Protocol (Agent A)
> Implement `packages/protocol` per docs/ARCHITECTURE.md §4.1 and spec/03-protocol.md: `frame.ts` (CRC-16/CCITT-FALSE, resyncing FrameParser that handles split and merged frames and skips junk bytes), typed encoders/decoders for every command and message, a `Transport` interface with Web Serial and Web Bluetooth implementations (UUIDs from the spec), and `RobotClient` with the exact behavior of `tools/elxie_cli.py` (HELLO/INFO with capability bitmap, chunked upload with per-chunk ACK and resend on BAD_SEQUENCE, run, stop, list, delete, autorun, telemetry and trace streams, status/error events). Tests: CRC vector `"123456789" → 0x29B1`, parser fuzz with random splits, a fake transport that simulates the robot's upload state machine. Add a tiny dev page in `apps/studio` at `/dev/robot` with Connect, Info, Upload file, Run, Stop and a live telemetry dump.

### M2 — Compiler (Agent B)
> Implement `packages/compiler` per docs/ARCHITECTURE.md §4.3 and every rule in spec/02-bytecode.md "Compiler rules". First port `tools/elxasm.py` to `assembler.ts` and prove it byte-identical on all `spec/test-vectors/*.elxa`. Then build IR, checker (limits, static stack depth, types, empty inputs, orphan blocks) and codegen with the documented control-flow patterns. Create `fixtures/*.blockly.json` for each test vector (use block types from the registry in packages/blocks — create a minimal registry stub if that package is empty) and assert byte-identical output. Also emit the source map and a Python-like `codeView`.

### M4 — Simulator (Agent C)
> Implement `packages/simulator` per docs/ARCHITECTURE.md §4.4. The C++ bridge `wasm/sim_bridge.cpp`, `wasm/host_imports.js` and `wasm/build.sh` already exist — build them with Emscripten (add an npm script; document emsdk setup). Write `VmWasm.ts` (loads the module, installs `Module.host`, uploads images, ticks at 10 ms simulated time) and a node test that runs every golden program and matches `spec/test-vectors/expected/*.trace` exactly (reuse the stimulus arguments from `tests/run_tests.sh`). Then `SimRobot` (differential drive, gyro, line/edge/depth sensors sampled from an arena, ultrasonic raycast, compass, magnet, touch/tilt/shake and remote joystick + A/B inputs) and a Canvas2D top-down renderer with the robot's face and lights. Start with arenas: free play, line oval, table edge.

### M3 — Editor MVP
> Build the Editor screen in `apps/studio` (docs/ARCHITECTURE.md §4.5) for the Explorer tier: Blockly workspace with Events, Motion, Face, Lights, Sound, Control blocks defined as JSON in `packages/blocks` from spec/01-blocks.md, category colors from §4.2, i18n strings in `en.json`. Top bar: Run (compile → upload slot 0 → run → trace on), Stop, Connect. Autosave projects to IndexedDB. Highlight running blocks from TRACE via the source map. Show compiler diagnostics on the blocks. Playwright test: build "repeat 4 { drive forward 1 s; turn right 90° }" by drag and drop and run it in the simulator.

### M5 — Full v1 block set
> Add the remaining v1 blocks from spec/01-blocks.md (Sensors, Remote, Pet, Operators, Variables, all event hats). Disable (with a tooltip) any block the connected robot doesn't support according to `client.supports()`. Add a Sensors tab showing live telemetry (robot or simulator) with kid-friendly gauges, and a variable monitor fed by TRACE. On runtime ERROR, select and shake the failing block with a friendly message.

### M6 — Junior and Maker tiers
> Junior (ages 5–7): horizontal icon blocks with a custom renderer, no typing, big targets; each block maps to a normal registry entry with fixed arguments (see spec/01-blocks.md "Editor tiers"). Maker (12–15): enable ⚙︎ blocks and a read-only code view panel from the compiler's `codeView`. Tier switch in project settings; a program written in a lower tier opens in a higher one unchanged.

### M7 — Robot manager
> Robot screen: connection, firmware info, slots list (name, size, autorun star), save current project to slot 1–7, delete, set autorun. Calibration wizard for line sensors (CALIBRATE 1 → guided "slide the robot" animation → CALIBRATE 2) gyro (CALIBRATE 3, "keep still") and compass (CALIBRATE 4, "put me on the floor, I'll spin").

### M8 — Challenges + companion contract
> Implement challenges per docs/ARCHITECTURE.md §4.6: arena + goal checker in the simulator (lap, reach zone, don't fall off, stop at obstacle), star rules (block count / time), and the `postMessage` contract with the companion app (init with unlocked blocks + challenge; emit challengeResult and projectSaved). Hide locked blocks from the toolbox.

### M9 — Mobile
> Wrap `apps/studio` with Capacitor for Android and iOS. Implement `capacitorBle.ts` with `@capacitor-community/bluetooth-le`, choose the transport at runtime, request permissions with kid-appropriate explanations, test upload + run on a real robot from both platforms.

## Review checklist for every agent result
- [ ] `/verify` is green, and the summary shows it.
- [ ] No hand edits to generated files; any spec change is explained.
- [ ] New behavior has a test (golden, unit or Playwright).
- [ ] User-facing strings are in i18n files and kid-friendly.
- [ ] Nothing weakens a firmware safety layer.
