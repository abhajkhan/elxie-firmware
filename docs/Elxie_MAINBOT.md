Board: ESP32S3 Dev Module
Flash Size: 16 MB(128Mb)
Arduino Runs On: Core 1
Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS)
PSRAM: OPI PSRAM
Upload Speed 921600

---

# Elxie - MAINBOT Pinout Reference

ESP32-S3  
(external strip = 30 LEDs)

---

## Pinout Table

| Subsystem                    | Signal                                  | ESP32-S3 GPIO |
| ---------------------------- | --------------------------------------- | ------------- |
| **Motor driver (TB6612FNG)** | STBY                                    | 14            |
|                              | PWMA (motor A speed)                    | 18            |
|                              | AIN1                                    | 19            |
|                              | AIN2                                    | 20            |
|                              | PWMB (motor B speed)                    | 21            |
|                              | BIN1                                    | 47            |
|                              | BIN2                                    | 48            |
| **Sonar (HC-SR04)**          | TRIG                                    | 15            |
|                              | ECHO                                    | 16            |
| **I2C bus**                  | SDA                                     | 8             |
|                              | SCL                                     | 9             |
| **Mux (CD4067, IR bank)**    | S0                                      | 4             |
|                              | S1                                      | 5             |
|                              | S2                                      | 6             |
|                              | S3                                      | 17            |
|                              | SIG (shared analog in)                  | 1             |
| **Touch (TTP223)**           | signal                                  | 12            |
| **Buzzer (passive)**         | PWM out                                 | 13            |
| **External NeoPixel strip**  | data                                    | 10            |
| **Encoders**                 | left                                    | 39            |
|                              | right                                   | 40            |
| **Reserved**                 | freed from onboard NeoPixel, unassigned | 45            |

---

## I2C Device Addresses

- MPU6050 → `0x68`
- QMC5883P → `0x2C`
- OLED (SSD1306) → `0x3C`

---

## IR Mux Channels (wired)

These are CD4067 channel numbers selected via S0-S3, not separate GPIOs.  
All four channels share the single MUX_SIG analog input (GPIO 1).

| Position         | Mux channel |
| ---------------- | ----------- |
| Front-right (FR) | 15          |
| Front-left (FL)  | 9           |
| Rear-right (RR)  | 14          |
| Rear-left (RL)   | 11          |

⚠️ The full product brief calls for **16 IR channels** (8 front line-sensing array + 4 bottom-edge + 4 side-edge).

| Physical position  | Sensor label | CD4067 channel | Baseline | Hand value |   Δ |
| ------------------ | ------------ | -------------: | -------: | ---------: | --: |
| Front-left         | F1           |              0 |     4000 |       >300 |     |
| Front-left-center  | F2           |              1 |     4000 |       >300 |     |
| Front-center-left  | F3           |              2 |     4000 |       >300 |     |
| Front-center-right | F4           |              3 |     4000 |       >300 |     |
| Front-right-center | F5           |              4 |     4000 |       >300 |     |
| Front-right-center | F6           |              5 |     4000 |       >300 |     |
| Front-right        | F7           |              6 |     4000 |       >300 |     |
| Front-rightmost    | F8           |              7 |     4000 |       >300 |     |
| Bottom-front-left  | B1           |             10 |     4000 |       >200 |     |
| Bottom-front-right | B2           |              8 |     4000 |       >200 |     |
| Bottom-rear-left   | B3           |             12 |     4000 |       >200 |     |
| Bottom-rear-right  | B4           |             13 |     4000 |       >180 |     |
| Corner-front-left  | C1           |              9 |     4000 |       >200 |     |
| Corner-front-right | C2           |             15 |     4000 |       >200 |     |
| Corner-rear-left   | C3           |             11 |     4000 |       >200 |     |
| Corner-rear-right  | C4           |             14 |     4000 |       >180 |     |

The handvalue given is the value when I almost touch the corresponding sensor.

---

## Not Yet Wired

- Sound sensor / microphone

---

_Generated from the pin definitions in `Elxie_AP_WebControl_v0.2.ino` / `MAINBOT_test_bench_v1.2.ino`._
