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
#define BATT_WARN_PCT 35
#define BATT_CLEAR_PCT 40
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
constexpr uint8_t REMOTE_MAC[6] = {0x18, 0xfe, 0x34, 0xf5, 0xe8, 0x66};

#endif // BOARD_CONFIG_H
