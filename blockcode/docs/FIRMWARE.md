# Elxie Block Code firmware (ESP-IDF)

Standalone block-coding firmware for the Elxie robot (ESP32-S3), written on **native ESP-IDF v5.3+** (no Arduino). It does one job: receive block programs, store them, run them safely.

## Architecture

```
            Core 0                                   Core 1 ("control" task, 100 Hz)
  ┌────────────────────────┐        ┌───────────────────────────────────────────────┐
  │ NimBLE host   ─ bytes ─┼──────► │ link::poll()   frames → commands (BLE + USB)   │
  │ Wi-Fi/ESP-NOW ─ packets┼──────► │ sensors.update()   IR ×16, US, IMU, compass,   │
  │ face task (OLED, 25fps)│ ◄────── │                    TTP223, mic                 │
  └────────────────────────┘ state  │ remote.update()                                │
                                    │ app::tick()        e-stop, launcher, calibration│
                                    │ vm.tick()          block programs (elx_vm)     │
                                    │ motion / sound / lights .update()              │
                                    │ link::periodic()   telemetry + trace           │
                                    └───────────────────────────────────────────────┘
```

| Path | Contents |
|---|---|
| `main/board_config.h` | **every pin and tuning constant** — start here |
| `components/elx_vm/` | portable VM + validator as an IDF component (no IDF code inside; also used by PC tests and the WASM simulator) |
| `main/robot_host.*` | maps each syscall/event to the robot services |
| `main/hal/` | `platform` (I²C master bus, ADC one-shot, LEDC, GPIO, time), motors, sensor hub |
| `main/services/` | motion (gyro turns, heading hold, cliff guard), sound, lights (`led_strip`), OLED face (U8g2), ESP-NOW remote, LittleFS storage |
| `main/link/` | frame codec, command handler, NimBLE GATT server, USB-Serial-JTAG |
| `main/app.*` | run/stop, slots, autorun, launcher menu, emergency stop, calibration |
| `main/main.cpp` | `app_main`: init + the 100 Hz control task |
| `sdkconfig.defaults`, `partitions.csv` | BLE (NimBLE), coexistence, 1 kHz tick, USB console, 4 MB layout with a LittleFS partition |
| `main/idf_component.yml` | managed components: `espressif/led_strip`, `joltwallet/littlefs`, `u8g2` (fetched automatically) |

## Build & flash

Needs ESP-IDF **v5.3, 5.4 or 5.5**, and internet on the first build (the component manager downloads the three managed components).

```bash
cd firmware
idf.py set-target esp32s3      # once; creates sdkconfig from sdkconfig.defaults
idf.py build
idf.py -p COM5 flash monitor   # Linux: /dev/ttyACM0   macOS: /dev/cu.usbmodem*
```

In VS Code with the ESP-IDF extension: open the `firmware` folder → set target *esp32s3* → **Build** → **Flash** (UART or JTAG) → **Monitor**.

Notes:
- If you edit `sdkconfig.defaults` later, delete `sdkconfig` (and `build/`) so the new defaults apply.
- The partition table fits 4 MB flash; it works unchanged on 8/16 MB modules.
- Console and the app link use **UART0** (GPIO43/44) through the board's USB-UART chip, because GPIO19/20 (the S3's native USB) drive motor A (TB6612 AIN1/AIN2).

> **Not yet compiled with the Xtensa toolchain where it was written.** Every source is type-checked against the real ESP-IDF 5.3 / NimBLE / led_strip 3.x / esp_littlefs / U8g2 signatures (`tests/host_compile/check.sh`), but expect to fix small things on the first `idf.py build`.

## Assumptions to check against your schematic

Everything below lives in `board_config.h`:

- IR: 16 channels through a **CD74HC4067** mux (ch 0–7 line L→R, 8–11 edge FL/FR/BL/BR, 12–15 depth FL/FR/BL/BR), read on an **ADC1** pin. If your board uses direct ADC pins or a different mux, change `SensorHub::readIr()` only.
- IMU: **MPU6050/6500**-compatible at 0x68. For another IMU, replace `readImuRaw()` and the init block in `sensor_hub.cpp`.
- OLED: **SSD1306 128×64** at 0x3C on the same I²C bus (guarded by a mutex; a separate bus is better if you have the pins).
- Motors: **two PWM inputs per motor** (DRV8833 / TB6612 PWM-PWM mode). One side inverted.
- Touch: one **TTP223** module, digital output (active HIGH by default; `TOUCH_ACTIVE_HIGH`).
- Magnetometer: **QMC5883P (0x2C)** — the Elxie board — or QMC5883L (0x0D) / HMC5883L (0x1E), auto-detected on the shared I²C bus. Heading assumes the robot is level. Keep it away from the motors and speaker magnets if you can; motor current can look like a "magnet".
- LEDs: **30 NeoPixels**. Brightness is capped at `NEOPIXEL_MAX_BRIGHTNESS` (60 %) because 30 LEDs at full white draw about 1.8 A; the block's 100 % maps to that cap.
- Mic: analog electret amplifier on an ADC1 pin.
- Remote: joystick with push button + buttons A and B. Packets: see `services/remote_link.h` — adapt `onRecvImpl()` if your remote already has a format.

## Bring-up checklist (in this order)

1. **Boot:** serial shows the banner, OLED says "Elxie Code", boot jingle plays.
2. **Link:** close the IDF monitor first (it holds the port), then `python tools/elxie_cli.py --port <port> info` prints firmware + limits.
3. **Sensors:** `... telemetry` — push each touch pad, cover each IR sensor, wave a hand at the ultrasonic, tilt the robot. Fix polarities (`IR_DARK_IS_HIGH`, `IMU_*_SIGN`) until every value moves the right way.
4. **Line calibration:** `... calibrate line` and slide the robot across line + floor.
   **Compass calibration:** `... calibrate compass` — the robot spins by itself for 6 s on a flat floor away from metal. Then check `heading` in telemetry: turning the robot clockwise must increase it (else flip `MAG_X_SIGN` or `MAG_Y_SIGN`), and set `MAG_HEADING_OFFSET` so pointing north reads ~0.
5. **Motors:** upload a one-block program: `drive forward 50% for 1 s`. Wrong direction → flip `MOTOR_L_INVERT` / `MOTOR_R_INVERT`. Robot doesn't start at low speed → raise `MOTOR_MIN_DUTY`.
6. **Turns:** `turn right 90°` ×4 should end where it started. Overshoot → raise `TURN_EARLY_STOP_DEG`; undershoot/slow → raise `TURN_MIN_SPEED`. Turns the wrong way → flip `IMU_YAW_SIGN`.
7. **Straight driving:** 3 s forward should stay straight; if it weaves, lower `HEADING_HOLD_KP`.
8. **Cliff guard:** hold the robot's front over a table edge while a program drives forward: wheels must stop.
9. **Remote:** hold **A + B** together while a program runs → it stops. Joystick moves the launcher selection; A or a joystick push runs.
10. **Golden programs:** upload each `spec/test-vectors/*.elxa` and compare behavior with `tests/build/*.trace`.

## Safety layers (below the VM — programs can't turn them off)

- Program stop: app **Stop**, remote **A + B held together** (0.8 s), or **hold the touch sensor 2 s**.
- **Cliff guard:** wheels can't drive toward a detected edge/drop; a timed drive toward an edge ends early so the program continues.
- Speed cap (`MOTOR_MAX_SPEED`), acceleration ramp, turn timeouts.
- Validator rejects malformed or unsupported programs before they run.
- Motors stop when a program ends, errors, or is stopped. Loop watchdog reboots a stuck robot.

## On-robot launcher (no phone needed)

When idle the OLED lists saved tricks (slots 1–7, `*` marks autorun).
Remote: joystick up/down to choose, **A** or **joystick push** to run. Robot only: **tap** the touch sensor for next, **hold 1.5 s** to run.

## Merging into the full Elxie firmware later

`components/elx_vm/`, `main/robot_host.*` and `main/link/` are designed to drop into the main firmware as the "Code" state next to Pet / RC / Challenge. Keep the BLE service and command numbers; add game commands in a separate range (see `spec/03-protocol.md`).
