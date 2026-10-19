// ============================================================================
//  Elxie Blocks inside the Home firmware — block-engine configuration
//  Copied from firmware/main/board_config.h (the Block Code firmware), so the
//  blocks behave the same on both firmwares. Pins match src/config/BoardConfig.h.
//  Values still marked (CHECK) are tuning guesses to verify on the robot.
// ============================================================================
#pragma once

#define ELX_FW_NAME        "Elxie Home"
#define ELX_FW_MAJOR       0
#define ELX_FW_MINOR       1
#define ELX_BLE_NAME       "ELXIE"          // advertised name (suffix: last 2 MAC bytes)

// ---------------------------------------------------------------- I2C
#define PIN_I2C_SDA        8
#define PIN_I2C_SCL        9
#define I2C_FREQ_HZ        400000

// ---------------------------------------------------------------- OLED face
// SSD1306 128x64 over I2C. For SH1106 change the U8G2 constructor in face.cpp.
#define OLED_I2C_ADDR      0x3C
#define OLED_WIDTH         128
#define OLED_HEIGHT        64

// ---------------------------------------------------------------- IMU
// MPU6050
#define IMU_I2C_ADDR       0x68
#define IMU_I2C_ADDR_ALT   0x69    // tried if nothing answers at 0x68 (AD0 pulled high)
#define IMU_YAW_SIGN       (+1)    // +1 if turning LEFT gives positive gyro Z
#define IMU_PITCH_SIGN     (+1)    // +1 if nose-down reads as positive pitch
#define IMU_ROLL_SIGN      (+1)    // +1 if right-side-down reads as positive roll
#define TILT_EVENT_DEG     25      // "when tilted" threshold
#define SHAKE_G            0.7f    // |accel - 1g| above this counts as shaking
#define SHAKE_HOLD_MS      600     // "shaken?" stays true this long

// ---------------------------------------------------------------- Drive motors
// TB6612FNG, wired as in the game code (RobotController drives B = left, A = right,
// IN1 high = forward on both, no inversion). PWM via analogWrite (8 bit, 1 kHz) like the games.
#define PIN_MOTOR_STBY     14
#define PIN_MOTOR_L_PWM    21      // PWMB
#define PIN_MOTOR_L_IN1    47      // BIN1
#define PIN_MOTOR_L_IN2    48      // BIN2
#define PIN_MOTOR_R_PWM    18      // PWMA
#define PIN_MOTOR_R_IN1    19      // AIN1  (GPIO19/20 are the S3's native USB pins)
#define PIN_MOTOR_R_IN2    20      // AIN2
#define MOTOR_MIN_DUTY     0.28f   // fraction of full duty where wheels start turning
#define MOTOR_MAX_SPEED    100     // global cap in % (lower it for younger kids)
#define MOTOR_RAMP_PER_S   400.0f  // max change in %/s (smooth starts, protects gears)

// Gyro turning
#define TURN_KP            1.6f    // % speed per degree remaining
#define TURN_MIN_SPEED     28.0f   // %, enough to overcome friction
#define TURN_MAX_SPEED     70.0f
#define TURN_EARLY_STOP_DEG 3.0f   // stop this many degrees early to absorb coasting
#define HEADING_HOLD_KP    1.2f    // % per degree drift while driving straight

// ---------------------------------------------------------------- IR array
// 16 IR channels through a CD74HC4067 16:1 analog mux.
#define PIN_MUX_S0         4
#define PIN_MUX_S1         5
#define PIN_MUX_S2         6
#define PIN_MUX_S3         17
#define PIN_MUX_SIG        1       // ADC1_CH0
#define PIN_IR_EMITTER     -1      // emitters always on
#define MUX_SETTLE_US      6
// Mux channel of each sensor (from the board file). Order: FL, FR, BL, BR.
#define IR_LINE_CHANNELS   { 0, 1, 2, 3, 4, 5, 6, 7 }   // left -> right (CHECK order)
#define IR_EDGE_CHANNELS   { 10, 8, 12, 13 }             // "BOTTOM_CH": downward, table-edge
#define IR_DEPTH_CHANNELS  { 9, 15, 11, 14 }             // "CORNER_CH" C1..C4
#define IR_DARK_IS_HIGH    1       // 1: raw ADC rises when surface is dark / far
#define IR_RAW_WHITE       300     // default calibration (overridden by stored calibration)
#define IR_RAW_BLACK       3300
#define LINE_IS_DARK       1       // 1 = dark line on light floor, 0 = light line on dark floor
#define LINE_THRESHOLD     50      // normalized 0..100
#define EDGE_THRESHOLD     85      // edge sensor normalized value meaning "no floor" (CHECK)
#define DEPTH_THRESHOLD    70      // depth sensor normalized value meaning "drop ahead"

// ---------------------------------------------------------------- Ultrasonic (HC-SR04)
#define PIN_US_TRIG        15
#define PIN_US_ECHO        16      // 5 V sensor: needs a divider on ECHO
#define US_PERIOD_MS       60
#define US_MAX_CM          400

// ---------------------------------------------------------------- Touch: one TTP223 module (CHECK)
// TTP223 gives a clean digital output. Default module config = momentary, active HIGH.
#define TOUCH_PAD_COUNT    1
#define PIN_TOUCH          12
#define TOUCH_ACTIVE_HIGH  1       // 0 if the module's A-pad is bridged (active LOW)
#define TOUCH_DEBOUNCE_MS  30
#define TOUCH_LONGPRESS_MS 1500    // idle: hold to run the selected trick
#define TOUCH_STOP_MS      2000    // running: hold to stop the program

// ---------------------------------------------------------------- Magnetometer
// Auto-detected on the I2C bus, in this order:
//   QMC5883P (0x2C, the Elxie board's chip), QMC5883L (0x0D), HMC5883L (0x1E).
#define MAG_QMCP_ADDR      0x2C
#define MAG_QMC_ADDR       0x0D
#define MAG_HMC_ADDR       0x1E
#define MAG_X_SIGN         (+1)    // flip so heading increases when the robot turns clockwise
#define MAG_Y_SIGN         (-1)    // measured on Elxie (compass log, Oct 2026)
#define MAG_HEADING_OFFSET 83.0f   // degrees: sensor mounting angle + local declination
                                   // (declination in Kerala is only ~1-2 deg; mounting matters more)
#define MAG_READ_EVERY     4       // read every N control ticks (25 Hz)
#define MAGNET_NEAR_LEVEL  25      // magnet strength 0..100 counted as "magnet near"
#define MAGNET_FULL_RATIO  2.0f    // field this many times the Earth's = strength 100
#define MAG_CAL_SPIN_SPEED 35      // % motor speed while self-calibrating
#define MAG_CAL_MS         6000    // spin time for compass calibration

// ---------------------------------------------------------------- Microphone
// Not fitted on the Elxie board: sound_level reads 0 and the "when loud sound"
// block is reported as unsupported to the app. Set a pin to enable.
#define PIN_MIC            -1
#define MIC_SAMPLES        16
#define MIC_PP_QUIET       60
#define MIC_PP_LOUD        1800
#define LOUD_DEFAULT       60

// ---------------------------------------------------------------- Battery
#define PIN_BATTERY        7       // ADC1 (divider ratio CHECK)
#define BATT_DIVIDER       2.0f    // V_batt = V_pin * BATT_DIVIDER
#define BATT_EMPTY_V       3.3f
#define BATT_FULL_V        4.2f

// ---------------------------------------------------------------- Buzzer / LEDs
#define PIN_BUZZER         13      // passive piezo
#define NEOPIXEL_COUNT     30
#define NEOPIXEL_DEFAULT_BRIGHTNESS 30   // %, programs start here
#define NEOPIXEL_MAX_BRIGHTNESS     60   // %, hard cap: 30 LEDs at full white draw ~1.8 A

// ---------------------------------------------------------------- Remote (ESP-NOW, received by EspNowManager)
#define REMOTE_TIMEOUT_MS  500
// Remote: joystick (with push button) + 2 buttons. Bit positions in RemoteInputPacket.buttons.
#define REMOTE_BTN_A       0
#define REMOTE_BTN_B       1
#define REMOTE_BTN_STICK   2       // joystick push
#define REMOTE_STOP_HOLD_MS 800    // hold A + B together this long = stop program (always active)

// ---------------------------------------------------------------- Safety
#define CLIFF_GUARD        0       // 1 = block driving toward a detected table edge. Off: it also blocked
                                   //     all motion when the robot was lifted or the bottom sensors misread.
#define EDGE_USE_DEPTH     0       // corner sensors are NOT used for the cliff guard (only bottom ones)
#define TURN_TIMEOUT_MIN_MS 1500


// ---------------------------------------------------------------- Block behaviours (CHECK on the robot)
#define SPIN_SPEED          55      // % for "spin"
#define ARC_INNER_RATIO     0.35f   // inner wheel speed / outer wheel speed in "drive in an arc"
#define UNTIL_OBSTACLE_CM   15      // "move until obstacle"
#define UNTIL_MAX_MS        30000   // safety timeout for "move until"
#define FOLLOW_KP           0.9f    // line following: steering per unit of line position
#define FOLLOW_JUNCTION_N   6       // this many of 8 sensors on black = junction
#define FOLLOW_MAX_MS       60000   // "follow the line until a junction" safety timeout
#define CORNER_RAW_THRESHOLD 4000   // corner IR raw ADC value meaning "object near" (from Elxie board file)
#define CORNER_ACTIVE_HIGH  1       // 1: raw rises when an object is near
#define PICKUP_DEBOUNCE_MS  300     // all 4 bottom sensors see no floor this long = picked up
#define TOUCH_EVENT_LONG_MS 800     // "when long-pressed" (running programs; 2 s still stops the program)
#define TOUCH_DOUBLE_GAP_MS 400     // max gap between the taps of a double tap
#define BATTERY_LOW_DEFAULT 20      // % for "when battery low"
// LED index groups for the turn-signal effects (0-based, from the Elxie board file)
#define LIGHTS_RIGHT_GROUP  { 6, 7, 8 }
#define LIGHTS_LEFT_GROUP   { 18, 19, 20 }
