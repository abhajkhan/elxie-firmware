#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <stdint.h>
#include <Arduino.h>
#include "../config/BoardConfig.h"

struct SensorSnapshot
{
    uint16_t cornerC1 = 0;
    uint16_t cornerC2 = 0;
    uint16_t cornerC3 = 0;
    uint16_t cornerC4 = 0;

    bool obstacleFrontLeft = false;
    bool obstacleFrontRight = false;
    bool obstacleRearLeft = false;
    bool obstacleRearRight = false;

    uint16_t bottomB1 = 0;
    uint16_t bottomB2 = 0;
    uint16_t bottomB3 = 0;
    uint16_t bottomB4 = 0;
    bool pitDetected = false;

    int frontDistanceCm = -1;
    uint16_t heading = 0;
    bool suddenMotion = false;
    bool touchActive = false;

    uint8_t batteryPercent = 100;
    uint32_t timestamp = 0;
};

class SensorManager
{
public:
    void begin();
    void update();

    SensorSnapshot getSnapshot() const { return snapshot; }

    long readSonarCm();
    int readMuxChannel(uint8_t channel);
    bool readMpu6050Accel(int16_t &ax, int16_t &ay, int16_t &az);
    bool detectSuddenMotion();
    uint8_t readQmc5883HeadingByte();
    bool isTouchActive();

    bool isMpu6050Present() const { return mpu6050Present; }
    bool isQmc5883Present() const { return qmc5883Present; }
    bool isOledPresent() const { return oledPresent; }

private:
    void i2cScan();
    void sonarInit();
    void muxInit();
    void mpu6050Init();
    void qmc5883Init();

    bool mpu6050Present = false;
    bool qmc5883Present = false;
    bool oledPresent = false;

    SensorSnapshot snapshot;
};

#endif // SENSOR_MANAGER_H
