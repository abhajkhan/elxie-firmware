#include "remote_link.h"

#include <Arduino.h>
#include <stdlib.h>

#include "../communication/EspNowManager.h"
#include "blocks_config.h"

RemoteLink remote;

static int8_t axis(int16_t raw) {
  int d = raw - 512;            // raw ADC 0..1023, centre ~512
  if (abs(d) < 60) d = 0;        // dead zone around the centre
  d = d * 100 / 450;             // full deflection reads ~450 counts
  return (int8_t)constrain(d, -100, 100);
}

void RemoteLink::begin() {}

void RemoteLink::update() {
  EspNowManager& mgr = EspNowManager::getInstance();
  connected_ = mgr.isConnected();
  LegacyRemoteCommandPacket p;
  if (connected_ && mgr.peekLegacyCommand(p)) {
    x_ = axis(p.x);   // right +
    y_ = axis(p.y);   // up + (same orientation the games use)
    buttons_ = (uint16_t)((p.btn1 ? 1u << REMOTE_BTN_A : 0) | (p.btn2 ? 1u << REMOTE_BTN_B : 0) |
                          (p.sw ? 1u << REMOTE_BTN_STICK : 0));
  } else if (!connected_) {
    x_ = y_ = 0;
    buttons_ = 0;
  }
  // The remote buzzes when the alert byte CHANGES, so send "obstacle" then "ok".
  if (alertOffAt_ && (int32_t)(millis() - alertOffAt_) >= 0) {
    alertOffAt_ = 0;
    mgr.sendFeedback(0);  // ALERT_OK
  }
}

void RemoteLink::buzz(uint16_t ms, uint8_t) {
  if (ms == 0 || !connected_) return;
  EspNowManager::getInstance().sendFeedback(1);  // ALERT_OBSTACLE = short buzz
  alertOffAt_ = millis() + 150;
}
