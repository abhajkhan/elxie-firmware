# Elxie hardware testbench

The testbench is a small separate program that checks every sensor and actuator on Elxie. Run it on a new or repaired robot before you put the main firmware on.

## Run it

1. Plug Elxie into USB and switch it on. Close the Test Console tab and any serial monitor, so the COM port is free.
2. Double-click **TESTBENCH.bat**. It builds the testbench (the first build takes a few minutes), flashes it and opens the live log.
3. Follow the yellow `>>>` lines in the log window:
   - IR sensors (6 s): move a hand or white paper under the line, edge and corner sensors.
   - Ultrasonic: hold a hand about 20 cm in front of Elxie.
   - Touch: touch the pad within 10 s.
   - Motors: **lift the wheels off the table** before the motor test starts.
   - Remote: turn the remote on, then move the joystick and press A, B and the stick (10 s).
4. When the summary appears, double-click **FLASH_HOME_FIRMWARE.bat** to put the main firmware back.

The whole run takes about 1½ minutes. To run the tests again without reflashing, hold the touch pad for 2 s.

## Live dashboard (web page)

To watch the tests in a webpage instead of the text log:

1. Double-click **TESTBENCH_UI.bat**. It builds and flashes the testbench, then opens `tools\testbench-ui\ElxieTestbench.html` in Edge. If the testbench is already on Elxie, double-click **OPEN_TESTBENCH_UI.bat** instead.
2. Press **Connect Elxie** and pick Elxie's COM port. Then press **Run all tests**, or **Restart Elxie** to start the run from the beginning.

The page shows:

- **Test sequence:** the 12 tests turn green, red, yellow or purple as they finish. Each one has a **Run** button to repeat just that test.
- **Instruction banner:** turns orange when you need to do something, such as "LIFT THE WHEELS".
- **Live sensors:** Elxie drawn from above, with each IR sensor at its real position and its raw value 10 times a second. Stuck sensors (0 or 4095) are outlined in red. You also see the distance ahead, touch, wheels and lights.
- **Tilt, compass, battery and remote** gauges.
- **Try the outputs:** spin each wheel for 1 s, set the light colour or beep the buzzer. **Stop all** turns everything off.
- **Log:** the same text as the console. **Save log** downloads it as a `.log` file.

Only one program can use the COM port at a time, so close the dashboard before you run TESTBENCH.bat or FLASH_HOME_FIRMWARE.bat. **Demo** shows the page with made-up data, without a robot.

## Reading the log

| Colour | Word | Meaning |
|---|---|---|
| green | PASS | measured and OK |
| red | FAIL | measured and wrong; the line says what to check |
| yellow | WARN | works, but a value looks unusual |
| magenta | CHECK | an output test: confirm you saw or heard it |

Every run is saved as `logs\testbench_<date>_<time>.log`, with a timestamp on each line. The build and flash output goes to `logs\testbench_build.log`.

## What is tested

| # | Part | Pass rule |
|---|---|---|
| 1 | I2C bus | IMU, compass and OLED all answer |
| 2 | OLED | shows white, text and a checkerboard (CHECK) |
| 3 | MPU6050 | gravity reads 0.8–1.2 g and the gyro is still at rest |
| 4 | QMC5883P | chip answers, values change and the field is in a normal range |
| 5 | 16 IR sensors | no channel stuck at 0 or 4095, and each one reacts to your hand |
| 6 | HC-SR04 | at least 7 of 10 pings give a valid distance |
| 7 | TTP223 touch | not stuck on, and a touch is seen within 10 s |
| 8 | Battery | 3.0–4.4 V |
| 9 | Buzzer | plays a rising scale (CHECK) |
| 10 | NeoPixels | red, green, blue and white, then LED 1..30 one by one (CHECK) |
| 11 | Motors | each wheel forward and back, then both; the log shows pin levels and battery dip (CHECK) |
| 12 | Remote | ESP-NOW packets arrive from the paired remote MAC |

## Notes

- Pins and constants come from `blockcode/firmware/main/board_config.h`, and the I2C, ADC, PWM and motor code from `blockcode/firmware/main/hal/`. The testbench always tests what the main firmware uses, so change pins only in `board_config.h`.
- The testbench uses the same partition table as the main firmware. Your saved programs and line calibration in the `storage` partition are not touched.
- The lights run dimmed, so the test is safe on USB power.

# Remote testbench (hand remote, ESP8266 D1 Mini)

The remote testbench checks the hand remote's joystick, buttons, battery, vibration motor and radio, live in a web page.

## Run it

1. Plug in **only the remote** over USB (unplug Elxie, so the right COM port is used). Close serial monitors.
2. Double-click **REMOTE_TESTBENCH.bat**. It builds the testbench (the first time PlatformIO downloads the ESP8266 tools), flashes it to the remote and opens `tools\remote-testbench-ui\RemoteTestbench.html` in Edge.
3. Press **Connect remote**, pick the remote's COM port, then press **Restart remote**. The 8 tests run by themselves; follow the orange banner.
4. When you are done, double-click **FLASH_REMOTE_FIRMWARE.bat** to put the normal remote firmware back.

If the testbench is already on the remote, **OPEN_REMOTE_TESTBENCH_UI.bat** just opens the page. **Demo** shows it with made-up data.

## What is tested

| # | Part | Pass rule |
|---|---|---|
| 1 | Joystick mux | the 4051 switches channels (X, Y and battery read differently) |
| 2 | Joystick centre | X and Y near 512 at rest, low noise |
| 3 | Joystick range | both axes reach below 200 and above 820 |
| 4 | Joystick direction | UP raises Y and RIGHT raises X, as the robot expects |
| 5 | Buttons | front (A), bottom (B) and stick press each seen, none stuck |
| 6 | Battery | 3.3–4.35 V (needs the divider values, see below) |
| 7 | Vibration motor | short, double and swell buzz (CHECK by feel) |
| 8 | Radio | ESP-NOW starts; the robot (90:70:69:14:D4:38) answers at least 3 of 4 packets |

The radio test prints **this remote's MAC address**, and tests the link to the robot the remote is paired with.

## Live page

- **Joystick pad:** knob, trail, the range reached so far and the games' dead zone. The X/Y bars show the raw values. Underneath you see what Blocks and the games make of the stick.
- **Buttons:** light up while pressed.
- **Radio:** packets per second, how many the robot answered, and alert packets from the robot. **Send to the robot like the real remote** makes the testbench drive the robot like the real remote, so lift Elxie's wheels first.
- **Battery:** raw value and voltage. Type the divider resistors, or measure the battery with a multimeter and press **Calibrate**. The page remembers it.
- **Vibration motor:** Short, Double, Swell, On and Off. **Stop all** turns everything off.
- **Pairing:** shows which robot the remote is paired with. **Pair with a robot** pairs with a robot that is in pairing mode, and **Forget robot** goes back to the default robot.

The pins are the same as `REMOTE_espnow.ino`. The battery divider (100 k / 100 k on mux channel 4) is set in `remote/testbench/src/main.cpp` (`CH_BATT`, `BATT_R_TOP`, `BATT_R_BOTTOM`).

## Factory pairing (one robot + one remote)

1. Flash the Home firmware on the robot (`FLASH_HOME_FIRMWARE.bat`) and the remote firmware on the remote (`FLASH_REMOTE_FIRMWARE.bat`).
2. Plug in **only the robot** over USB and double-click **PAIR_ROBOT_AND_REMOTE.bat**. The robot blinks blue for 30 s.
3. Hold **FRONT + BOTTOM** on the remote while switching it on.
4. The window shows `PAIRING_RESULT=OK` and the remote gives a double buzz. Pair one set at a time, so a remote can't pick the wrong robot.

## Compass calibration (once per robot)

1. The robot runs the Home firmware and its battery is switched on (the motors need it).
2. Plug in **only the robot** over USB and put it on a flat floor, at least 1 m from metal, magnets, speakers and laptops. Leave the cable loose.
3. Double-click **CALIBRATE_COMPASS.bat** and press Enter. The robot spins in place for 6 s.
4. The window shows `COMPASS_RESULT=OK` and the robot shows "Compass ready". The calibration is saved on the robot and survives reflashing. If it shows `FAILED`, move away from metal and run it again.

## Tilt test (pitch and roll settings)

Run it on one robot of each hardware build; the result applies to all robots with the same board.

1. Plug in the robot over USB and hold it in your hand.
2. Double-click **TILT_TEST.bat**, press Enter and follow the prompts (also on the robot's screen): level, then nose down about 45°, then right side down about 45°, 6 s each.
3. `TILT_RESULT=OK` means the settings are right. `CHANGE` prints the three values to put in `src/blocks/blocks_config.h` (`IMU_SWAP_PITCH_ROLL`, `IMU_PITCH_SIGN`, `IMU_ROLL_SIGN`).
