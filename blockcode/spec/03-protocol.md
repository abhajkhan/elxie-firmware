# 03 — App ↔ robot protocol (v1)

Same frames over **BLE** and **USB serial**. Reference client: `tools/elxie_cli.py`. Firmware: `firmware/main/link/`.

## BLE GATT

| | UUID | Properties |
|---|---|---|
| Service | `8d3a0001-5e1c-4a8e-9b2d-e1a1e0c0de01` | advertised; device name `Elxie-XXXX` |
| RX (app → robot) | `8d3a0002-5e1c-4a8e-9b2d-e1a1e0c0de01` | write, write-without-response |
| TX (robot → app) | `8d3a0003-5e1c-4a8e-9b2d-e1a1e0c0de01` | notify |

Both directions are **byte streams**: a frame may be split across several writes/notifications, and one write may hold several frames. Always feed bytes through a resynchronizing parser. The robot requests MTU 247.

USB serial: 115200 baud. The serial channel switches to binary mode after the first valid frame; before that only log text appears. Parsers must skip non-frame bytes.

## Frame

```
A5 5A | type u8 | len u16 LE | payload[len] | crc16 u16 LE
crc16 = CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) over type, len and payload
max payload 512 bytes
```

## Commands (app → robot)

| Type | Name | Payload | Reply |
|---|---|---|---|
| 0x01 | HELLO | — | INFO |
| 0x02 | PING | any | PONG (echo) |
| 0x10 | UPLOAD_BEGIN | slot u8, total u16, crc32 u32 | ACK |
| 0x11 | UPLOAD_CHUNK | offset u16, data… | ACK value = bytes received so far |
| 0x12 | UPLOAD_END | — | ACK (validated + saved) or NAK(load error) |
| 0x13 | RUN | slot u8 | ACK / NAK |
| 0x14 | STOP | — | ACK |
| 0x15 | LIST | — | SLOTS |
| 0x16 | DELETE | slot u8 | ACK |
| 0x17 | SET_AUTORUN | slot u8 (0xFF = none) | ACK |
| 0x20 | TELEMETRY | period_ms u16 (0 = off, min 50) | ACK, then TELEMETRY_DATA |
| 0x21 | TRACE | on u8 | ACK, then TRACE_DATA every 100 ms while running |
| 0x30 | CALIBRATE | 1 = start line calibration, 2 = finish + save, 3 = gyro, 4 = compass (robot spins by itself for 6 s, then saves) | ACK |

**Upload flow.** `BEGIN` → `CHUNK`s in order (each waits for its ACK; on `NAK BAD_SEQUENCE` resend from the ACK'd offset) → `END`. "Run now" in the app = upload to **slot 0**, then `RUN 0`. Slots 1–7 are the child's saved "tricks", launchable from the robot without the phone.

## Messages (robot → app)

| Type | Name | Payload |
|---|---|---|
| 0x80 | ACK | cmd u8, value u16 |
| 0x8F | NAK | cmd u8, error u8 |
| 0x81 | INFO | fw_major, fw_minor, bytecode_ver, max_prog u16, num_slots, max_scripts, max_vars, **syscall bitmap [32]** (offset 8), event bitmap u16 for events 0–15 (40), led_count (42), touch_pads (43), name[16] (44), event bitmap u16 for events 16–31 (60) — 64 bytes |
| 0x82 | PONG | echo |
| 0x85 | SLOTS | count u8, then per slot 20 bytes: slot, flags (bit0 used, bit1 autorun), size u16, name[16] |
| 0x90 | TELEMETRY_DATA | see below (44 bytes) |
| 0x91 | TRACE_DATA | running u8, n u8, n × {state u8 (0 idle, 1 run, 2 wait), pc u16}, nv u8, nv × float32 |
| 0x92 | STATUS | running u8, slot u8 (0xFF idle), last_error u8 — sent on every start/stop |
| 0x93 | ERROR | runtime error u8, script u8, pc u16 |

**TELEMETRY_DATA:** `t_ms u32 | line[8] | edge[4] | depth[4] | distance u16 (cm×10) | pitch i16 (°×10) | roll i16 | yaw i16 | touch bits u8 | sound u8 | joyX i8 | joyY i8 | buttons u16 | flags u8 (b0 remote, b1 imu ok, b2 shaken, b3 cliff guard) | motorL i8 | motorR i8 | line_pos i8 | mood u8 | reserved u8 | heading u16 (°×10) | magnet strength u8 | mag ok u8`

`touch bits` bit 0 = the TTP223. `buttons` bit 0 = A, bit 1 = B, bit 2 = joystick push.

## NAK error codes

1–15 = load errors from `opcodes.json` (`BAD_CRC`, `BAD_SYSCALL`, …). Link errors: `0x41 BAD_SLOT`, `0x42 BAD_SEQUENCE`, `0x43 TOO_LARGE`, `0x44 CRC_MISMATCH`, `0x45 STORAGE`, `0x46 EMPTY_SLOT`, `0x47 UNKNOWN_CMD`, `0x48 BAD_ARGS`.

## Coexistence with the full Elxie firmware

When block coding is merged into the main firmware (Pet / RC / Challenge), keep this service and these commands unchanged and add the game commands (results sync, level push) as new types in a separate range (0x40–0x6F). The app then talks to one protocol.
