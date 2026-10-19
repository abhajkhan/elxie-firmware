# Elxie Code — Software Architecture

How the block-coding app is built, package by package, so it can be developed in Google Antigravity by several agents in parallel without drifting apart.

The robot side already exists in this repo (`firmware/`). This document covers everything the child and parent touch: the editor, the compiler, the simulator and the robot connection.

---

## 1. The big picture

```
                      spec/opcodes.json  (single source of truth)
                               │  tools/gen_opcodes.py
              ┌────────────────┴──────────────────┐
              ▼                                   ▼
   packages/protocol/opcodes.gen.ts     firmware/components/elx_vm/include/opcodes_gen.h

┌──────────────────────── apps/studio (React) ─────────────────────────┐
│                                                                       │
│  Blockly workspace ──► packages/compiler ──► .elxb image + source map │
│  (packages/blocks)          │                        │                │
│        ▲                    │ diagnostics            ├──► packages/simulator
│        │ highlight          ▼                        │     (same VM as the robot,
│        └───────── pc → block id ◄── TRACE ───┐       │      compiled to WASM)
│                                              │       ▼                │
│                         packages/protocol  RobotClient ── BLE / USB ──┼──► Robot
│                                                                       │    firmware/
└───────────────────────────────────────────────────────────────────────┘
```

**Guiding rules**

1. **The spec is the contract.** Blocks, compiler, simulator and firmware all derive from `spec/`. Nobody hardcodes an opcode or syscall number.
2. **One VM, three places.** The C++ VM in `firmware/components/elx_vm` runs on the robot, in PC tests, and (compiled to WASM) in the simulator. The simulator can't disagree with the robot about how a program behaves.
3. **Golden tests bind the implementations.** `spec/test-vectors/*.elxa` + expected traces are run by the firmware host tests, the WASM simulator and the TS compiler tests.
4. **The robot runs; the app edits and keeps the record** (same split as the Elxie framework). A program keeps working with the phone gone.

---

## 2. Tech stack

| Concern | Choice | Why |
|---|---|---|
| Language | TypeScript (strict) | shared types across packages |
| UI | React + Vite | fast dev loop, huge ecosystem, agents know it well |
| Block editor | Google **Blockly** (current major) + `@blockly/*` plugins | the standard; JSON block definitions; touch support; custom renderers for the Junior tier |
| State | Zustand | small, no boilerplate |
| Local storage | IndexedDB (`idb-keyval`) | projects offline; no `localStorage` size limits |
| Mobile | **Capacitor** (Android + iOS) wrapping the same web build | one codebase; native BLE plugin |
| BLE | Web Bluetooth (Chrome desktop / Android) · `@capacitor-community/bluetooth-le` (native apps) | iOS Safari has no Web Bluetooth → the iOS app must be the Capacitor build |
| USB | Web Serial (Chrome desktop) | dev + classroom fallback |
| Simulator VM | Emscripten → WASM from `firmware/components/elx_vm` | byte-identical behavior with the robot |
| Tests | Vitest (unit), Playwright (end-to-end on the simulator) | Antigravity's browser agent can drive the same flows |
| Monorepo | pnpm workspaces | simple, fast |
| i18n | `i18next`, strings in JSON | English first; Malayalam / Hindi later without code changes |

---

## 3. Repository layout

```
ElxieBlockCode/
├─ spec/                     ← contract (blocks, bytecode, protocol, golden tests)
├─ firmware/                 ← ESP32-S3 firmware, native ESP-IDF (done: VM, services, BLE, safety)
├─ tools/                    ← gen_opcodes.py, elxasm.py (reference assembler), elxie_cli.py
├─ tests/                    ← VM host tests, firmware host type-check
├─ packages/
│  ├─ protocol/              ← opcodes.gen.ts, frame codec, transports, RobotClient
│  ├─ compiler/              ← Blockly JSON → IR → bytecode (+ source map, diagnostics)
│  ├─ blocks/                ← block definitions, toolboxes per tier, generators, i18n
│  └─ simulator/             ← WASM VM + virtual robot + arenas
├─ apps/
│  └─ studio/                ← the React app (web + Capacitor shells)
├─ docs/                     ← this file, firmware notes, Antigravity playbook
├─ AGENTS.md                 ← standing rules for every Antigravity agent
└─ .agents/                  ← Antigravity workspace rules + workflows
```

Dependency direction (no cycles): `studio → blocks, compiler, simulator, protocol`; `compiler → protocol` (constants only); `simulator → protocol` (constants only). `blocks` depends on nothing but Blockly and `protocol` constants.

---

## 4. Packages

### 4.1 `packages/protocol` — constants, codec, robot client

```
src/
  opcodes.gen.ts        GENERATED — never edit
  frame.ts              crc16, buildFrame(), FrameParser (resyncing, split-safe)
  messages.ts           typed encode/decode for every command/message in spec/03
  transport.ts          interface Transport { write(bytes); onData(cb); close(); }
  transports/
    webBluetooth.ts     navigator.bluetooth, service/char UUIDs from spec
    capacitorBle.ts     @capacitor-community/bluetooth-le
    webSerial.ts        navigator.serial
  RobotClient.ts
```

`RobotClient` API (mirrors `tools/elxie_cli.py`):

```ts
class RobotClient {
  static connect(t: Transport): Promise<RobotClient>;      // sends HELLO, stores RobotInfo
  info: RobotInfo;                                          // fw version, limits, capabilities
  supports(syscall: string): boolean;                       // from the INFO bitmap
  upload(image: Uint8Array, slot: number, onProgress?): Promise<void>;  // BEGIN/CHUNK/ACK/END, retry on BAD_SEQUENCE
  run(slot: number): Promise<void>;
  stop(): Promise<void>;
  listSlots(): Promise<SlotInfo[]>;
  deleteSlot(slot: number): Promise<void>;
  setAutorun(slot: number | null): Promise<void>;
  telemetry(periodMs: number): AsyncIterable<Telemetry> | Unsubscribe;
  trace(on: boolean): /* emits { running, threads: {state, pc}[], vars: number[] } */;
  on(event: 'status' | 'error' | 'disconnect', cb): Unsubscribe;
}
```

Rules: chunk size = negotiated MTU − 3 − 9; one outstanding command at a time (queue); 3 s timeout per command; auto-reconnect with backoff; errors mapped to child-friendly messages in the UI layer, not here.

### 4.2 `packages/blocks` — what the child sees

```
src/
  defs/*.json          Blockly JSON block definitions, one file per category
  junior/*.json        icon blocks (horizontal), each maps to a normal block + fixed args
  toolbox/junior.ts, explorer.ts, maker.ts
  enums.ts             dropdown options built from ENUMS in opcodes.gen.ts
  theme.ts             category colors, fonts, Junior renderer config
  i18n/en.json …       every visible string
  registry.ts          blockType → { kind: 'hat'|'stmt'|'reporter'|'bool', syscall?, event?, args[] }
```

The **registry** is the bridge to the compiler: it says, for each block type, which event or syscall it is and in what order its inputs become arguments. The compiler never guesses from the block's name.

Category colors (kid-readable, high contrast): Events amber, Motion blue, Face purple, Lights pink, Sound magenta, Sensors cyan, Remote teal, Pet orange, Control gold, Operators green, Variables red-orange.

Gating: a block is **hidden** if the child hasn't unlocked it (progression from the companion app), and **disabled with a tooltip** if the connected robot's firmware doesn't support it (`client.supports()`).

### 4.3 `packages/compiler` — blocks → bytecode

```
Blockly workspace JSON
   │  parse.ts        walk top blocks; each hat block = one script (sorted top→bottom, left→right)
   ▼
IR (ir.ts)            Script[] of typed nodes: Call(syscall,args) | Op | If | Repeat | Forever |
   │                  RepeatUntil | WaitUntil | Wait | SetVar | ChangeVar | Stop | Const | VarRef
   │  check.ts        types, variable count ≤ 32, script count ≤ 8, static stack depth ≤ 16,
   │                  unsupported blocks, empty inputs (use defaults), orphan blocks (ignored + warning)
   ▼
codegen.ts            patterns from spec/02-bytecode.md; always END per script
   ▼
assembler.ts          TS port of tools/elxasm.py: labels, PUSH sizing, string table, header, CRC-32
   ▼
{ image: Uint8Array, sourceMap: Map<pc, blockId>, diagnostics: Diagnostic[], stats }
```

- `Diagnostic = { blockId, severity, messageKey }` — shown as warnings on the block itself, in child language ("This loop needs a number").
- `codeView.ts` (Maker tier): renders the IR as Python-like text, read-only.
- **Golden test:** for each `spec/test-vectors/*.elxa` there is a matching `fixtures/*.blockly.json`; compiling it must produce **byte-identical** output to `elxasm.py`. This is the test that keeps the TS compiler honest.

### 4.4 `packages/simulator` — a virtual Elxie

```
wasm/sim_bridge.cpp    extern "C" API around firmware/components/elx_vm (provided, see file)
wasm/build.sh          emcc → vm.wasm + vm.js (ES module)
src/
  VmWasm.ts            loads WASM; implements the host imports in JS
  SimRobot.ts          differential-drive kinematics, gyro, sensor models
  Arena.ts             2D world: floor image (line tracks), table outline (edges), obstacles
  arenas/*.json        built-in arenas: free play, line oval, table edge, maze, parking
  render/Canvas2D.ts   top-down view; robot sprite with face + lights
  inputs.ts            click robot = touch; tilt/shake buttons; drag-a-magnet tool; on-screen joystick (+ push) and A/B buttons for the remote
```

The simulator ticks the VM every 10 ms of **simulated** time (can run faster than real time for challenge checking). Sensor models are simple and deterministic: line sensors sample the floor image under 8 points; edge sensors check whether their corner point is inside the table polygon; ultrasonic raycasts against obstacles; gyro = integrated heading + small noise (seeded); compass = arena "north" + heading; magnet strength falls off with distance to magnets placed in the arena.

`SimRobot` also exposes the same `Telemetry` shape as the real robot, so the live-sensor panel works identically on both.

### 4.5 `apps/studio` — the app

**Screens**

| Screen | Contents |
|---|---|
| Home | projects grid, "New", tier badge, robot connection chip |
| Editor | toolbox · canvas · right panel with tabs **Robot / Simulator / Sensors** · top bar: ▶ Run, ■ Stop, Save to robot, undo/redo |
| Robot | connect / reconnect, firmware info, slots manager (rename, delete, set autorun), calibration wizards (line, gyro, compass) |
| Challenges | list from the companion app; each opens the editor with an arena + goal |

**State (Zustand stores)**

- `projectStore` — current workspace JSON, name, tier, dirty flag, autosave to IndexedDB every 2 s.
- `robotStore` — `RobotClient | null`, connection state, info, slots, running slot, last error.
- `runStore` — target (`robot` | `sim`), running, last compiled `{image, sourceMap}`, highlighted block ids, variable values.
- `progressStore` — unlocked blocks, challenge stars (synced from/to the companion app).

**Core flows**

```
Run ▶ (robot)         compile → diagnostics? stop : upload(slot 0) → run(0) → trace(on)
                      TRACE pcs → sourceMap → highlight blocks; vars → variable monitor
Run ▶ (simulator)     compile → VmWasm.load(image) → start → same highlighting path
Save to robot         pick slot 1–7 + name (from project) → upload(slot) → listSlots()
Runtime ERROR         pc → sourceMap → select + shake that block, friendly message
Disconnect mid-run    program keeps running on the robot; UI shows "running on Elxie"
```

**Kid-first UX rules:** big touch targets (≥ 44 px), undo everywhere, no modal dead-ends, every error message says what to do next, Run is always one tap, Stop is always visible while running.

### 4.6 Integration with the companion app

Keep the studio a separate web package that the companion app can **embed** (WebView / iframe on web) or link to. Contract (JSON over `postMessage` or a shared package):

```json
// companion → studio
{ "type": "init", "childId": "…", "tier": "explorer",
  "unlockedBlocks": ["drive_timed", "turn_degrees", "face_show", "..."],
  "challenge": { "id": "line-1", "arena": "line_oval", "goal": {"kind": "lap", "count": 1},
                 "stars": { "3": {"maxBlocks": 8}, "2": {"maxBlocks": 12}, "1": {} } } }
// studio → companion
{ "type": "challengeResult", "id": "line-1", "stars": 3, "blocks": 7, "timeMs": 9420 }
{ "type": "projectSaved", "projectId": "…", "slot": 2 }
```

---

## 5. Data formats

**Project file** (IndexedDB, exportable as `.elxie.json`):

```json
{ "format": 1, "id": "uuid", "name": "Square party", "tier": "explorer",
  "createdAt": "…", "updatedAt": "…",
  "workspace": { /* Blockly serialization JSON */ },
  "robotSlot": 2, "thumbnail": "data:image/png;base64,…" }
```

**Arena** (`packages/simulator/src/arenas/*.json`): `{ size_mm, floorImage, table: polygon[], obstacles: rect[], start: {x,y,heading}, goals?: [...] }`.

---

## 6. Testing strategy

| Layer | Test | Where |
|---|---|---|
| Spec | generated files up to date (`gen_opcodes.py` produces no diff) | CI |
| VM | golden traces + validator fuzz cases | `tests/run_tests.sh` (done) |
| Firmware | type-check against ESP-IDF 5.3 API stubs | `tests/host_compile/check.sh` (done); `idf.py build` in CI (espressif/idf Docker image) |
| Compiler | byte-identical output vs `elxasm.py` for every fixture; diagnostics snapshots | Vitest |
| Protocol | codec round-trip, split/merged frames, CRC, upload retry with a fake transport | Vitest |
| Simulator | WASM VM reproduces the golden traces exactly | Vitest (node) |
| App | "drag blocks → Run in simulator → robot moves" flows | Playwright / Antigravity browser agent |
| Hardware | bring-up checklist + `elxie_cli.py` scripts | `docs/FIRMWARE.md` |

---

## 7. Build order (milestones)

| # | Milestone | Done when |
|---|---|---|
| M0 | Workspace | pnpm monorepo, lint, Vitest, CI running `run_tests.sh` + `check.sh` |
| M1 | Protocol + CLI parity | `RobotClient` over Web Serial / Web Bluetooth does everything `elxie_cli.py` does |
| M2 | Compiler | all golden fixtures byte-identical; diagnostics |
| M3 | Editor MVP (Explorer) | Motion, Face, Lights, Sound, Control, Events blocks; Run on a real robot |
| M4 | Simulator | WASM VM + arena + highlight; runs every golden program identically |
| M5 | Full v1 blocks | sensors, remote, pet, variables, operators; capability gating; trace highlighting |
| M6 | Junior + Maker tiers | icon blocks, code view |
| M7 | Robot manager + calibration | slots, autorun, line/gyro calibration wizards |
| M8 | Challenges + companion contract | arenas, goal checkers, stars, `postMessage` bridge |
| M9 | Mobile | Capacitor Android + iOS with native BLE |

M1, M2 and M4 can run in parallel once M0 is in place — they only share `spec/`.

---

## 8. Risks and how the design handles them

| Risk | Mitigation |
|---|---|
| App and robot disagree on bytecode | one spec file, generated constants, shared golden tests, capability bitmap |
| New app, old robot | robot advertises supported syscalls; unsupported blocks greyed out; upload validated on the robot |
| Kids write infinite loops | VM yields on every backward jump + instruction budget; Stop always works (app, remote A + B, long-press touch) |
| Robot drives off the table | firmware cliff guard below the VM — programs can't disable it |
| iOS has no Web Bluetooth | Capacitor native build for iOS |
| BLE packet sizes | stream framing + MTU-aware chunking + per-chunk ACKs |
| Simulator drift from reality | same VM via WASM; sensor models calibrated against real telemetry recordings |
