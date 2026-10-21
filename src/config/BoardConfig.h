#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <stdint.h>
#include <stddef.h>

// ========================================
// DEVICE CONFIGURATION
// ========================================
#define DEVICE_NAME "ELXIE"
#define FIRMWARE_VERSION "3.0.0"

// ========================================
// MOTOR DRIVER (TB6612FNG) PINS & DEFAULTS
// ========================================
#define PIN_STBY 14
#define PIN_PWMA 18
#define PIN_AIN1 19
#define PIN_AIN2 20
#define PIN_PWMB 21
#define PIN_BIN1 47
#define PIN_BIN2 48

// Wheel direction. Motor A = RIGHT wheel, motor B = LEFT wheel.
// Set to 1 if that wheel turns backwards when driven forward. Used by the games,
// RC, the pet moves (MotorController) and Elxie Blocks (src/blocks/motors.cpp).
// Oct 2026 robots: the left motor is wired reversed (Forward made the robot spin left).
#define MOTOR_A_INVERT 0
#define MOTOR_B_INVERT 1

constexpr int DEFAULT_SPEED = 180;

// ========================================
// SONAR (HC-SR04) PINS
// ========================================
#define PIN_TRIG 15
#define PIN_ECHO 16

// ========================================
// I2C BUS PINS & DEVICE ADDRESSES
// ========================================
#define PIN_SDA 8
#define PIN_SCL 9

#define ADDR_MPU6050 0x68
#define ADDR_QMC5883P 0x2C
#define ADDR_OLED 0x3C

#define OLED_WIDTH 128
#define OLED_HEIGHT 64

// ========================================
// MULTIPLEXER (CD4067) PINS & CHANNELS
// ========================================
#define PIN_MUX_S0 4
#define PIN_MUX_S1 5
#define PIN_MUX_S2 6
#define PIN_MUX_S3 17
#define PIN_MUX_SIG 1

#define MUX_CH_C1 9
#define MUX_CH_C2 15
#define MUX_CH_C3 11
#define MUX_CH_C4 14

constexpr uint8_t CORNER_CH[4] = {MUX_CH_C1, MUX_CH_C2, MUX_CH_C3, MUX_CH_C4};
constexpr uint8_t BOTTOM_CH[4] = {10, 8, 12, 13}; // FL, FR, RL, RR

#define IR_OBSTACLE_THRESHOLD 4000

// ========================================
// TOUCH SENSOR (TTP223) PIN
// ========================================
#define PIN_TOUCH 12

// ========================================
// BATTERY SENSE PIN & THRESHOLDS
// ========================================
#define PIN_BATTERY 7
// 2 x 18650 in series. Divider: pack+ --100k-- GPIO7 --47k-- GND, so the pin sees
// pack x 47/147 (meter, Oct 2026: pack 7.72 V, pin 2.47 V).
#define BATT_CELLS 2
#define BATT_R_TOP_OHM 100000.0f  // battery side
#define BATT_R_BOTTOM_OHM 47000.0f // to GND
// The ESP32 ADC reads this high-impedance divider low (no capacitor on GPIO7): firmware
// 6.00 V vs meter 7.72 V. Re-measure (battery? vs meter) after adding a 100 nF cap
// GPIO7-GND, then set this back to about 1.0.
#define BATT_CAL 1.287f           // meter / reading
// Percent: straight line on the PACK voltage: 8.20 V = 100 %, 7.65 V = 5 % (0 % at ~7.62 V)
#define BATT_PACK_FULL_MV 8200
#define BATT_PACK_LOW_MV 7650
#define BATT_PACK_LOW_PCT 5
#define BATT_WARN_PCT 30          // low-battery beep at and below this (pack ~7.80 V)
#define BATT_CLEAR_PCT 35         // beep stops again above this
#define BATT_SAMPLE_MS 2000UL
#define BATT_ALERT_PERIOD_MS 60000UL

// ========================================
// BUZZER PIN & FREQUENCY
// ========================================
#define PIN_BUZZER 13
#define BUZZER_FREQ 2000

// ========================================
// NEOPIXEL STRIP CONFIGURATION & REGIONS
// ========================================
#define PIN_STRIP 10
#define NUM_STRIP_PIXELS 30

constexpr uint8_t RIGHT_PIXELS[] = {6, 7, 8};
constexpr uint8_t FRONT_PIXELS[] = {27, 28};
constexpr uint8_t LEFT_PIXELS[] = {18, 19, 20};
constexpr uint8_t BACK_PIXELS[] = {13, 12};

#define RIGHT_PIXEL_COUNT (sizeof(RIGHT_PIXELS) / sizeof(RIGHT_PIXELS[0]))
#define FRONT_PIXEL_COUNT (sizeof(FRONT_PIXELS) / sizeof(FRONT_PIXELS[0]))
#define LEFT_PIXEL_COUNT (sizeof(LEFT_PIXELS) / sizeof(LEFT_PIXELS[0]))
#define BACK_PIXEL_COUNT (sizeof(BACK_PIXELS) / sizeof(BACK_PIXELS[0]))

// ========================================
// ENCODER PINS
// ========================================
#define PIN_ENC_LEFT 39
#define PIN_ENC_RIGHT 40

// ========================================
// ESP-NOW CONFIGURATION
// ========================================
// 1 = a robot that was never paired ignores every remote (pair each set at the factory
//     with PAIR_ROBOT_AND_REMOTE.bat). Stops one stray remote driving someone else's robot.
// 0 = an unpaired robot listens to REMOTE_MAC below (development only).
#define REMOTE_REQUIRE_PAIRING 1
constexpr uint8_t REMOTE_MAC[6] = {0x18, 0xfe, 0x34, 0xf5, 0xe8, 0x66};

#endif // BOARD_CONFIG_H
