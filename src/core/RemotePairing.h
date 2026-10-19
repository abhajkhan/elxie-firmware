#ifndef REMOTE_PAIRING_H
#define REMOTE_PAIRING_H

// Stage 5: pairing a hand remote with this robot.
// Starts from: touch pad held at power-on, the app ({"type":"pair_remote"}), or the
// USB serial command "pair". Shows progress on the OLED and LEDs and reports the result.
#include <Arduino.h>

class BleManager;
class BuzzerController;
class DisplayController;
class LedController;
class RobotController;

namespace pairing
{
    void begin(DisplayController &display, LedController &leds, BuzzerController &buzzer, RobotController &robot,
               BleManager &ble);
    void start();             // robot goes idle, pairing for 30 s
    void cancel();
    void forget();            // back to the BoardConfig remote
    bool active();            // pairing, or still showing the result: games/menus keep off the screen
    void update();            // every loop()
    void pollSerial();        // USB serial commands: pair | unpair | remote?
    bool bootTouchHeld();     // call at the end of setup(): true if touch was held ~3 s since power-on
    void markBootStart();     // call first thing in setup()
    String remoteMacString(); // "AA:BB:..", for device_info
} // namespace pairing

#endif
