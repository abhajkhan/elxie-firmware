# Elxie Home firmware: games + Blocks

This repo is the team's game firmware (`abhajkhan/elxie-firmware`, from commit addb237) with the Elxie Blocks engine and remote pairing added. One robot runs the five games and also runs block programs from the web app's Blocks page. Both use the same Bluetooth connection.

## Build and flash (Windows)

From the repo root:

- **BUILD_HOME_FIRMWARE.bat** builds only.
- **FLASH_HOME_FIRMWARE.bat** builds, flashes and shows 12 s of boot log.

The first run installs PlatformIO into `%USERPROFILE%\.platformio\penv`. It uses Python from `py`, `python` or ESP-IDF's Python. The first build then downloads the ESP32 tools, which takes about 10 minutes. The COM port is picked automatically: the USB-serial port (CH340/CP210x), or the highest-numbered port.

Logs are saved to `logs\home_build.log` and `logs\home_flash.log`.

A healthy boot log contains these lines:

```
[Blocks] ready: bytecode v1, 8 slots
[BLE] Nordic UART Service started & advertising
```

## How Blocks mode works

| When | What happens |
|---|---|
| The app's Blocks page sends any frame on the Blocks service (`8d3a0001-…`) | `RobotController` goes to `TopLevelState::BLOCKS`. Any running game stops. The block engine takes over the motors, OLED, LEDs and buzzer. |
| A block program is running | The remote's joystick and buttons go to the program, not to RC or the local menu. Holding A + B on the remote, or holding touch for 2 s, stops the program. |
| The app starts RC or a game | Blocks mode ends and the game code takes the robot back. |
| The app disconnects and no program is running | After 3 s the robot returns to IDLE: the idle face and the remote menu. |
| **Double tap** on the touch pad while the robot is idle (no game, no app needed) | Opens **My Programs**: the programs saved on the robot. |

A program keeps running if the app disconnects.

## Home screen (robot idle)

- The robot shows **eyes** (the same eyes as Blocks): they blink and glance around by themselves (always normal eyes; the low-battery beep and the app show the battery). They replace the old happy-face emoji.
- **Remote menu**: the first joystick move or button press on the remote shows the menu. One item at a time, with a big icon: **joystick left / right** = next / previous, **stick press** = choose, **B** = back. 15 s without input (or B on the main menu) goes back to the eyes.
  - Main: **Games**, **Free Ride**, **My Programs**
  - Games: Colour Quest, Echo Memory, Reflex Dash, Driving Pro, Inverted Drive, then the level 1 to 6
- Only one part of the firmware draws on the screen at a time (the face engine at home, the games during a game or Free Ride), so the screen no longer flickers between the menu and the face.

## My Programs (on the robot, without the app)

- Open: double tap the touch pad while the robot is idle, or choose **My Programs** in the remote menu.
- Touch: **tap** = next program, **hold 1.5 s** = run it. Remote: joystick up/down = choose, **A** = run, **B** (bottom button) = back to the games, also while the app is connected.
- When the program ends the list comes back. After 30 s with no input (and no app) the robot goes back to the games.
- No programs saved yet: the screen says so; make one in the app first.

## Safety stops (Blocks)

| Stop | How |
|---|---|
| E-stop | Hold remote A + B, or hold the touch pad 2 s (a press that started the program does not count until released) |
| Lift / tilt stop | Tilted more than 60 degrees or upside down **while the wheels are driving**, for 0.3 s: motors and program stop, the face shows "Put me down". Uses the motion sensor, not the IR sensors (sunlight). Tilting a robot whose wheels are still does not stop anything, so "tilt" blocks keep working. Settings: `LIFT_STOP_DEG`, `LIFT_STOP_MS` in `blocks_config.h`. |

## What was added or changed

New, in `src/blocks/`:

- `vm/` holds the bytecode VM. It is byte-identical to `blockcode/firmware/components/elx_vm`.
- `link.cpp` and `protocol.cpp` are the app ↔ robot frames, Bluetooth only. The USB serial port keeps the game logs.
- `ble_transport.cpp` adds the Blocks GATT service to the game's Bluedroid server. The scan response carries the name and the Blocks UUID.
- These are ported from the Block Code firmware onto Arduino:
  - `sensor_hub`, `motion`, `robot_host` and `app` (the launcher was removed and autorun is ignored).
  - `sound` uses `tone()`.
  - `lights` uses LedController's `Adafruit_NeoPixel`.
  - `face` draws with U8g2 into RAM and sends through DisplayController's `Adafruit_SSD1306`.
  - `motors` uses `analogWrite`. Like the games, motor B is the left wheel and motor A the right wheel. Wheel direction comes from `MOTOR_A_INVERT` / `MOTOR_B_INVERT` in `src/config/BoardConfig.h` (the left motor is reversed on the Oct 2026 robots).
  - `storage` uses FFat at `/ffat/blocks`. The first boot formats the partition.
  - `remote_link` reads the ESP8266 remote's 7-byte packet from `EspNowManager`.
- `blocks.cpp` is the glue: begin, loop at 100 Hz, and entering and leaving Blocks mode.

Small edits to the team's files, each marked with a comment:

- `core/RobotTypes.h`: adds `TopLevelState::BLOCKS`.
- `core/RobotController.*`:
  - adds `enterBlocks()` and `exitBlocks()`.
  - In BLOCKS mode, ownership changes are ignored and input events are skipped.
- `communication/BleManager.*`: adds the Blocks service, scan response, MTU and disconnect hooks.
- `communication/EspNowManager.*`: adds `peekLegacyCommand()`, which reads the remote's packet without consuming it.
- `main.cpp`:
  - calls `blocks::begin()` and `blocks::loop()`.
  - In Blocks mode it skips `sensorManager.update()`, the local menu and remote menu input.
- `platformio.ini`:
  - The platform is pinned to `espressif32 @ 6.13.0` (Arduino core 2.0.17), because 7.x moved to core 3.x, where the ESP-NOW callback no longer compiles.
  - Builds with `-std=gnu++17`.
  - Adds `olikraus/U8g2`.
  - The fixed `COM10` port is removed.

## Settings

Block settings are in `src/blocks/blocks_config.h`. Game settings stay in `src/config/BoardConfig.h`.

The compass is set from the measured log: `MAG_Y_SIGN -1` and `MAG_HEADING_OFFSET 83`. Each robot still needs one compass calibration, done at the factory with **CALIBRATE_COMPASS.bat** (or "Compass (robot spins)" in the app). It is saved in `/ffat/blocks/magcal.bin` and survives reflashing.

The tilt blocks use `IMU_SWAP_PITCH_ROLL`, `IMU_PITCH_SIGN` and `IMU_ROLL_SIGN`. **TILT_TEST.bat** checks them on a real robot: hold the robot level, then nose down, then right side down, and it prints the right values.

## Factory checks over USB serial (115200)

| Command | What it does |
|---|---|
| `calmag` | Compass calibration: the robot spins in place for 6 s, then prints `MAGCAL OK ...` or `MAGCAL FAILED` and saves |
| `compass?` | Prints `COMPASS found/missing calibrated/not-calibrated` and the stored ranges |
| `tilttest` | Guided tilt test (level, nose down, right side down, 6 s each). Prints `TILT RESULT IMU_SWAP_PITCH_ROLL .. IMU_PITCH_SIGN .. IMU_ROLL_SIGN .. -> MATCHES/CHANGE blocks_config.h` |
| `tilt` | Live pitch, roll, raw accel and compass heading, 4 times a second. Send `tilt` again to stop. |
| `battery?` | Prints `BATTERY robot <V> V <%>% remote <V> V <%>%` (`remote none` if the remote is not reporting) |

These switch the robot into Blocks mode while they run. Holding remote A + B or the touch pad cancels the compass spin.

## Remote pairing (Stage 5)

Each robot listens to one hand remote, and each remote talks to one robot. Both remember the pairing in flash, so it survives power-off and reflashing the app.

**Unpaired means no control.** A robot that was never paired ignores every remote, and a remote that was never paired sends nothing: it gives two long buzzes at power-on and on every button press. So a stray remote can never drive someone else's robot. Pair every set at the factory. (For development, `REMOTE_REQUIRE_PAIRING 0` in `BoardConfig.h` and `REQUIRE_PAIRING 0` in `REMOTE_espnow.ino` bring back the old fixed addresses.)

**How to pair**

1. Put the robot into pairing mode, using any one of these:
   - Hold the touch pad while switching Elxie on, and keep holding for about 3 s.
   - From the app, send `{"type":"pair_remote"}`.
   - From USB, run **PAIR_ROBOT_AND_REMOTE.bat** (factory), or type `pair` in a serial monitor.

   The OLED shows "Pairing" and the LEDs blink blue for 30 s.
2. On the remote, hold **FRONT + BOTTOM** while switching it on. It buzzes twice, then ticks while it searches.
3. On success, the robot shows "Paired!" with green LEDs and a beep, and the remote gives a double buzz. If no robot answers, both keep their old pairing: the robot shows "No remote" in red and the remote gives one long buzz.

**USB serial commands (115200)**

| Command | What it does |
|---|---|
| `pair` | Start pairing mode |
| `cancel` | Stop pairing mode |
| `unpair` | Forget the paired remote. The robot then ignores all remotes until it is paired again. |
| `remote?` | Prints `REMOTE <mac> paired robot <robot mac>`, or `REMOTE none unpaired robot <robot mac>` |

**Bluetooth JSON**

| Message | Direction | Meaning |
|---|---|---|
| `pair_remote` | app → robot | Start pairing. The robot replies `pair_remote_ack`. |
| `cancel_pair_remote` | app → robot | Stop pairing mode. |
| `unpair_remote` | app → robot | Forget the paired remote. |
| `remote_paired` | robot → app | Sent when pairing ends. Payload: `result` (`paired`, `timeout`, `cancelled` or `forgotten`) and `remoteMac`. |
| `device_info` | robot → app | Now includes `remoteMac` and `remotePaired`. |

**On the radio**

| Packet | Size | Sent by | Meaning |
|---|---|---|---|
| `ELXB` beacon | 19 bytes | robot, every 200 ms while pairing | "I'm a robot in pairing mode" |
| `ELXR` request | 11 bytes | remote | "Pair with me" |
| `ELXK` confirm | 11 bytes | robot | "Paired" |

The packet layout is in `src/communication/ElxiePairingProtocol.h`, which must stay identical to `remote/firmware/src/ElxiePairingProtocol.h`. Normal remote packets are unchanged, so the games work exactly as before.
