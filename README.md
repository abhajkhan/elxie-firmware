# Elxie firmware

Firmware and factory tools for **Elxie**, the Elxtron Labs smart toy robot (ESP32-S3) for children aged 5 to 12, and its hand remote (ESP8266). The web app lives in its own repo, `abhajkhan/elxie-ui`.

## What's where

| Path | What |
|---|---|
| `platformio.ini`, `src/`, `include/`, `lib/` | **Home firmware** for the robot: the five games, RC, the local OLED menu, the Elxie Blocks engine (`src/blocks/`) and remote pairing. PlatformIO, Arduino core 2.0.17. |
| `remote/firmware/` | Hand remote firmware (ESP8266 D1 Mini) with pairing |
| `remote/testbench/` | Remote hardware testbench |
| `testbench/` | Robot hardware testbench (ESP-IDF) |
| `tools/` | Live web dashboards for both testbenches (Web Serial, Chrome or Edge) |
| `scripts/` | PowerShell scripts behind the `.bat` files |
| `blockcode/` | Block Code platform: block catalog and bytecode spec, VM tests, assembler and CLI, stand-alone Blocks editor, and the earlier standalone ESP-IDF firmware (reference only) |
| `docs/` | Team design docs, `BLOCKS_README.md`, `TESTBENCH_README.md` |

## One-click tools (Windows, double-click in the repo root)

| File | Does |
|---|---|
| `BUILD_HOME_FIRMWARE.bat` | Builds the robot firmware |
| `FLASH_HOME_FIRMWARE.bat` | Builds and flashes the robot firmware, shows the boot log |
| `CHECK_ROBOT.bat` | Reads the robot's flash chip and a 25 s boot log into `logs\` |
| `CALIBRATE_COMPASS.bat` | Compass calibration, once per robot (it spins for 6 s) |
| `TILT_TEST.bat` | Checks the tilt (pitch/roll) settings on a robot |
| `TESTBENCH.bat` / `TESTBENCH_UI.bat` | Robot hardware testbench, with or without the dashboard |
| `FLASH_REMOTE_FIRMWARE.bat` | Flashes the hand remote |
| `REMOTE_TESTBENCH.bat` | Remote hardware testbench and dashboard |
| `PAIR_ROBOT_AND_REMOTE.bat` | Factory pairing of one robot with one remote |

The first run installs PlatformIO into `%USERPROFILE%\.platformio\penv`. The robot testbench also needs ESP-IDF 5.x.

See `docs/BLOCKS_README.md` for how Blocks mode and pairing work, and `docs/TESTBENCH_README.md` for the factory steps.
