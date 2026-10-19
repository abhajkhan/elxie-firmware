# 01 — Block catalog (LOCKED v1)

Status: **locked for bytecode v1** (agreed 30 Sep 2026). Machine-readable source: [`opcodes.json`](opcodes.json).
Changing a block's *look* is free. Changing what it *compiles to* means editing `opcodes.json`, regenerating, and updating the golden tests.

## Decisions this catalog is built on

| Decision | Choice |
|---|---|
| Ages | 5–15, in three editor tiers (below) — one compiler, one bytecode |
| Motion | No wheel encoders → **time-based driving** + **gyro-based turning** (IMU). Straight driving uses gyro heading-hold. |
| Concurrency | **Parallel scripts**, Scratch-style. Max 8 scripts per program. |
| IR sensors | 16 channels: 8 line array, 4 edge (corners), 4 depth (corners) |
| Touch | one TTP223 touch sensor (digital) |
| Magnetometer | compass heading + magnet detection (QMC5883L / HMC5883L auto-detected) |
| Lights | 30 NeoPixels |
| Remote | ESP-NOW joystick with push button + 2 buttons (A, B). **Hold A + B together = stop program** (always works) |

## Editor tiers

| Tier | Ages | Editor | What's in the toolbox |
|---|---|---|---|
| **Junior** | 5–7 | Icon-only horizontal blocks (ScratchJr-style), no typing, big touch targets | start / touched / obstacle / magnet events; drive forward/back for 1-2-3 s; turn left/right 90°; show face; lights color; play sound; react; repeat (2–10); forever; wait |
| **Explorer** | 8–11 | Blockly vertical blocks with text | Every **v1** block below except the ⚙︎ Maker ones |
| **Maker** | 12–15 | Blockly + read-only "code view" (Python-like text of the program) | Everything **v1**, incl. ⚙︎ blocks; later: expansion blocks |

Junior blocks are only different *visuals*: each maps to the same syscalls with fixed arguments, so programs move up a tier without conversion.

## Hat blocks (events) — each starts its own script

Edge-triggered: a script starts when its condition goes false → true. If that script is still running, the new trigger is ignored.

| Block | Event (id) | Param |
|---|---|---|
| when program starts | START (0) | — |
| when touched | TOUCHED (1) | 0 (one TTP223 sensor; param kept for future pads) |
| when obstacle closer than (10) cm | OBSTACLE (2) | cm 1–255 |
| when line detected [left/center/right/any] | LINE (3) | zone |
| when edge detected [front-left/…/any] | EDGE (4) | corner 1–4, 0 = any |
| when tilted [forward/backward/left/right] | TILTED (5) | tilt |
| when shaken | SHAKEN (6) | — |
| when remote button [A/B/stick] pressed | REMOTE_BUTTON (7) | button |
| when loud sound heard | LOUD (8) | threshold 0–100 (default 60) |
| when magnet detected | MAGNET (9) | strength threshold 0–100, 0 = default (25) |
| *later:* when I receive [msg] | MESSAGE (10) | — |

## Stack blocks and reporters (syscalls)

Arguments are pushed in the order shown; dropdowns compile to the enum numbers in `opcodes.json`.

### Motion (time + gyro)
| Block | Syscall (id) | Notes |
|---|---|---|
| drive [forward/backward] at (50)% for (1) sec | `drive_timed` (16) | waits; gyro keeps it straight |
| turn [left/right] (90)° | `turn_degrees` (17) | waits; gyro-controlled, times out safely |
| drive [forward/backward] at (50)% | `drive` (18) | keeps going |
| ⚙︎ set motors left (50)% right (50)% | `set_motors` (19) | −100…100 each |
| stop motors | `stop_motors` (20) | |
| turn to face heading (0)° | `turn_to_heading` (21) | reads the compass, turns the shortest way with the gyro; waits |

### Face (OLED)
| Block | Syscall |
|---|---|
| show face [neutral/happy/sad/angry/surprised/sleepy/love/dizzy] | `face_show` (32) |
| look [center/left/right/up/down] | `face_look` (33) |
| blink | `face_blink` (34) |
| show text ["Hi!"] — literal text only in v1 | `show_text` (35) |
| show number (n) | `show_number` (36) |
| clear face | `face_clear` (37) |

### Lights (NeoPixel)
| Block | Syscall |
|---|---|
| set all lights to [color] | `lights_all` (48) |
| set light (1–30) to [color] | `light_set` (49) |
| light effect [rainbow/pulse/blink/chase] [color] | `light_effect` (50) |
| set brightness (0–100)% | `lights_brightness` (51) — 100 % = the firmware's safe maximum |
| lights off | `lights_off` (52) |

Colors are numbers `0xRRGGBB` (exact in float32).

### Sound (buzzer)
| Block | Syscall | Notes |
|---|---|---|
| play note [C4] for (1) beats | `play_note` (64) | note = MIDI number (C4 = 60); waits |
| play sound [beep/happy/sad/alarm/level-up] | `play_sound` (65) | waits |
| rest for (1) beats | `rest` (66) | waits |
| set tempo (120) | `set_tempo` (67) | bpm |

### Sensors
| Block | Syscall | Returns |
|---|---|---|
| distance (cm) | `distance` (80) | 2–400 (400 = nothing seen) |
| line sensor (1–8) | `line_value` (81) | 0–100 how strongly it sees the line |
| ⚙︎ line position | `line_position` (82) | −100 (left) … +100 (right) |
| line under [left/center/right/any]? | `line_detected` (83) | true/false |
| edge at [corner/any]? | `edge_detected` (84) | true/false (edge OR depth sensor) |
| ⚙︎ floor depth at [corner] | `floor_depth` (85) | 0–100 |
| tilt [pitch/roll] | `tilt` (86) | degrees |
| shaken? | `is_shaken` (87) | true/false |
| touched? | `is_touched` (88) | true/false |
| sound level | `sound_level` (89) | 0–100 |
| timer · reset timer | `timer` (90) · `reset_timer` (91) | seconds, 0.1 s resolution |
| heading | `heading` (92) | 0–359°, 0 = magnetic north, clockwise (gyro-based if no magnetometer) |
| magnet near? | `magnet_near` (93) | true/false |
| magnet strength | `magnet_strength` (94) | 0–100 |

### Remote
| Block | Syscall | Returns |
|---|---|---|
| joystick [x/y] | `joystick` (96) | −100…100 (0 when disconnected) |
| button [A/B/stick] pressed? | `remote_button` (97) | true/false |
| buzz remote for (200) ms | `remote_buzz` (98) | |

### Pet (Elxie's signature)
| Block | Syscall | Notes |
|---|---|---|
| react [happy/sad/excited/scared/angry/sleepy/love] | `react` (112) | face + sound + lights for ~1.2 s; waits |
| set mood [mood] | `set_mood` (113) | the resting face |
| mood | `mood` (114) | reporter |

Bond and stars are **not** programmable — they belong to the companion app's progression.

## Control, operators, variables (compiled to opcodes, not syscalls)

| Block | Compiles to |
|---|---|
| wait (1) sec | `WAIT` |
| wait until ⟨ ⟩ | loop pattern (see 02-bytecode) |
| repeat (10) · forever · repeat until ⟨ ⟩ | loop patterns |
| if ⟨ ⟩ then · if-else | `JZ` / `JMP` |
| stop [this script] · stop [all] | `END` · `STOP_ALL` |
| + − × ÷ mod, round, abs, pick random (1) to (10) | `ADD … RANDOM` |
| < = > , and / or / not | `LT GT EQ AND OR NOT` |
| set / change / (variable) | `STORE_VAR` / `CHANGE_VAR` / `LOAD_VAR` (max 32 variables) |
| *later:* lists, My Blocks (functions), broadcast/receive, map/constrain | reserved |

## Expansion (later, ids reserved now)
`i2c_write` (224), `i2c_read` (225), `uart_send` (226), `uart_available` (227), `uart_read` (228) — the base for the robotic arm, camera and AI-voice module blocks.
