#include "SensorManager.h"
#include <Wire.h>
#include <math.h>

static long lastAccelMag = 0;

void SensorManager::begin()
{
    Wire.begin(PIN_SDA, PIN_SCL);
    i2cScan();
    sonarInit();
    muxInit();
    mpu6050Init();
    qmc5883Init();
    pinMode(PIN_TOUCH, INPUT);
}

void SensorManager::i2cScan()
{
    Serial.println("Scanning I2C bus...");

    for (uint8_t addr = 1; addr < 127; addr++)
    {
        Wire.beginTransmission(addr);

        if (Wire.endTransmission() == 0)
        {
            Serial.printf("  Found device at 0x%02X\n", addr);

            if (addr == ADDR_MPU6050)
                mpu6050Present = true;
            if (addr == ADDR_QMC5883P)
                qmc5883Present = true;
            if (addr == ADDR_OLED)
                oledPresent = true;
        }
    }

    Serial.printf("MPU6050 (0x68):  %s\n", mpu6050Present ? "OK" : "MISSING");
    Serial.printf("QMC5883P (0x2C): %s\n", qmc5883Present ? "OK" : "MISSING");
    Serial.printf("OLED (0x3C):     %s\n", oledPresent ? "OK" : "MISSING");
}

void SensorManager::sonarInit()
{
    pinMode(PIN_TRIG, OUTPUT);
    pinMode(PIN_ECHO, INPUT);
    digitalWrite(PIN_TRIG, LOW);
}

long SensorManager::readSonarCm()
{
    digitalWrite(PIN_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);

    long duration = pulseIn(PIN_ECHO, HIGH, 30000);
    if (duration == 0)
        return -1;

    return duration / 58;
}

void SensorManager::muxInit()
{
    pinMode(PIN_MUX_S0, OUTPUT);
    pinMode(PIN_MUX_S1, OUTPUT);
    pinMode(PIN_MUX_S2, OUTPUT);
    pinMode(PIN_MUX_S3, OUTPUT);
    analogReadResolution(12);
}

int SensorManager::readMuxChannel(uint8_t channel)
{
    digitalWrite(PIN_MUX_S0, channel & 0x01);
    digitalWrite(PIN_MUX_S1, (channel >> 1) & 0x01);
    digitalWrite(PIN_MUX_S2, (channel >> 2) & 0x01);
    digitalWrite(PIN_MUX_S3, (channel >> 3) & 0x01);
    delayMicroseconds(50);
    return analogRead(PIN_MUX_SIG);
}

void SensorManager::mpu6050Init()
{
    if (!mpu6050Present)
        return;

    Wire.beginTransmission(ADDR_MPU6050);
    Wire.write(0x6B);
    Wire.write(0x00);
    Wire.endTransmission();
}

bool SensorManager::readMpu6050Accel(int16_t &ax, int16_t &ay, int16_t &az)
{
    if (!mpu6050Present)
        return false;

    Wire.beginTransmission(ADDR_MPU6050);
    Wire.write(0x3B);

    if (Wire.endTransmission(false) != 0)
        return false;

    Wire.requestFrom((int)ADDR_MPU6050, 6);
    if (Wire.available() < 6)
        return false;

    ax = (Wire.read() << 8) | Wire.read();
    ay = (Wire.read() << 8) | Wire.read();
    az = (Wire.read() << 8) | Wire.read();

    return true;
}

bool SensorManager::detectSuddenMotion()
{
    int16_t ax, ay, az;
    if (!readMpu6050Accel(ax, ay, az))
        return false;

    long mag = (long)ax * ax + (long)ay * ay + (long)az * az;
    long delta = abs(mag - lastAccelMag);
    lastAccelMag = mag;

    return delta > 200000000L;
}

void SensorManager::qmc5883Init()
{
    if (!qmc5883Present)
        return;

    Wire.beginTransmission(ADDR_QMC5883P);
    Wire.write(0x0B);
    Wire.write(0x01);
    Wire.endTransmission();

    Wire.beginTransmission(ADDR_QMC5883P);
    Wire.write(0x09);
    Wire.write(0x1D);
    Wire.endTransmission();
}

uint8_t SensorManager::readQmc5883HeadingByte()
{
    if (!qmc5883Present)
        return 127;

    Wire.beginTransmission(ADDR_QMC5883P);
    Wire.write(0x00);

    if (Wire.endTransmission(false) != 0)
        return 127;

    Wire.requestFrom((int)ADDR_QMC5883P, 6);
    if (Wire.available() < 6)
        return 127;

    int16_t x = Wire.read() | (Wire.read() << 8);
    int16_t y = Wire.read() | (Wire.read() << 8);

    Wire.read();
    Wire.read();

    float headingRad = atan2((float)y, (float)x);
    if (headingRad < 0)
        headingRad += 2 * PI;

    float headingDeg = headingRad * 180.0 / PI;

    return (uint8_t)(headingDeg / 360.0 * 255.0);
}

bool SensorManager::isTouchActive()
{
    return digitalRead(PIN_TOUCH) == HIGH;
}

void SensorManager::update()
{
    snapshot.timestamp = millis();

    snapshot.cornerC1 = readMuxChannel(MUX_CH_C1);
    snapshot.cornerC2 = readMuxChannel(MUX_CH_C2);
    snapshot.cornerC3 = readMuxChannel(MUX_CH_C3);
    snapshot.cornerC4 = readMuxChannel(MUX_CH_C4);

    snapshot.obstacleFrontLeft = (snapshot.cornerC1 < IR_OBSTACLE_THRESHOLD);
    snapshot.obstacleFrontRight = (snapshot.cornerC2 < IR_OBSTACLE_THRESHOLD);
    snapshot.obstacleRearLeft = (snapshot.cornerC3 < IR_OBSTACLE_THRESHOLD);
    snapshot.obstacleRearRight = (snapshot.cornerC4 < IR_OBSTACLE_THRESHOLD);

    snapshot.bottomB1 = readMuxChannel(BOTTOM_CH[0]);
    snapshot.bottomB2 = readMuxChannel(BOTTOM_CH[1]);
    snapshot.bottomB3 = readMuxChannel(BOTTOM_CH[2]);
    snapshot.bottomB4 = readMuxChannel(BOTTOM_CH[3]);
    snapshot.pitDetected = (snapshot.bottomB1 > 1000 || snapshot.bottomB2 > 1000 || snapshot.bottomB3 > 1000 || snapshot.bottomB4 > 1000);

    snapshot.frontDistanceCm = readSonarCm();
    snapshot.heading = readQmc5883HeadingByte();
    snapshot.suddenMotion = detectSuddenMotion();
    snapshot.touchActive = isTouchActive();
}
