#include <math.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "sensor_hub.h"
#include "robot_host.h"
#include "motion.h"
#include "remote_link.h"
#include "storage.h"
#include "ble_transport.h"
#include "blocks_config.h"
#include "platform.h"
#include "protocol.h"

using namespace proto;

namespace vmlink {

struct Channel {
  FrameParser parser;
  uint16_t telemetryMs = 0;
  uint32_t lastTelemetry = 0;
  bool trace = false;
  uint32_t lastTrace = 0;
  virtual void write(const uint8_t* d, size_t n) = 0;
  virtual bool up() = 0;
  virtual ~Channel() {}
};

// Home firmware: Blocks talk over Bluetooth only (the USB serial port carries
// the game firmware's log lines).
struct BleChannel : Channel {
  void write(const uint8_t* d, size_t n) override { ble::send(d, n); }
  bool up() override { return ble::connected(); }
};

static BleChannel g_ble;
static uint8_t g_out[MAX_PAYLOAD + 7];

static void send(Channel& ch, uint8_t type, const uint8_t* p, uint16_t n) {
  size_t len = buildFrame(type, p, n, g_out);
  ch.write(g_out, len);
}
static void sendAll(uint8_t type, const uint8_t* p, uint16_t n) {
  if (g_ble.up()) send(g_ble, type, p, n);
}
static void ack(Channel& ch, uint8_t cmd, uint16_t v = 0) {
  uint8_t p[3] = {cmd, (uint8_t)(v & 0xFF), (uint8_t)(v >> 8)};
  send(ch, ACK, p, 3);
}
static void nak(Channel& ch, uint8_t cmd, uint8_t err) {
  uint8_t p[2] = {cmd, err};
  send(ch, NAK, p, 2);
}

// ------------------------------------------------------------------ upload state
static uint8_t g_up[elx::LIMIT_MAX_PROGRAM_BYTES];
static uint16_t g_upTotal = 0, g_upGot = 0;
static uint32_t g_upCrc = 0;
static uint8_t g_upSlot = 0xFF;

static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void wr16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void wr32(uint8_t* p, uint32_t v) { for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i)); }

static void sendInfo(Channel& ch) {
  uint8_t p[64] = {0};
  p[0] = ELX_FW_MAJOR;
  p[1] = ELX_FW_MINOR;
  p[2] = elx::BYTECODE_VERSION;
  wr16(p + 3, elx::LIMIT_MAX_PROGRAM_BYTES);
  p[5] = elx::LIMIT_NUM_SLOTS;
  p[6] = elx::LIMIT_MAX_SCRIPTS;
  p[7] = elx::LIMIT_MAX_VARS;
  for (int id = 0; id < 256; ++id)  // capability bitmap: which syscalls this firmware runs
    if ((elx::SYSCALL_INFO[id].flags & elx::SYSF_VALID) && RobotHost::syscallSupported((uint8_t)id))
      p[8 + id / 8] |= (uint8_t)(1u << (id % 8));
  uint32_t ev = 0;  // event bitmap: ids 0..15 at offset 40, 16..31 at offset 60
  for (int e = 0; e < 32; ++e)
    if (RobotHost::eventSupported((uint8_t)e)) ev |= (1u << e);
  wr16(p + 40, (uint16_t)(ev & 0xFFFF));
  wr16(p + 60, (uint16_t)(ev >> 16));
  p[42] = NEOPIXEL_COUNT;
  p[43] = TOUCH_PAD_COUNT;
  strncpy((char*)p + 44, ELX_FW_NAME, 15);  // name: 16 bytes at 44..59
  send(ch, INFO, p, 64);
}

static void sendSlots(Channel& ch) {
  uint8_t p[1 + elx::LIMIT_NUM_SLOTS * 20] = {0};
  p[0] = elx::LIMIT_NUM_SLOTS;
  for (uint8_t s = 0; s < elx::LIMIT_NUM_SLOTS; ++s) {
    SlotInfo in = storage::slotInfo(s);
    uint8_t* e = p + 1 + s * 20;
    e[0] = s;
    e[1] = (in.used ? 1 : 0) | (app::autorunSlot() == s ? 2 : 0);
    wr16(e + 2, in.size);
    memcpy(e + 4, in.name, 16);
  }
  send(ch, SLOTS, p, sizeof p);
}

static void handle(void* ctx, uint8_t type, const uint8_t* p, uint16_t n) {
  Channel& ch = *static_cast<Channel*>(ctx);
  switch (type) {
    case HELLO: sendInfo(ch); break;
    case PING: send(ch, PONG, p, n); break;

    case UPLOAD_BEGIN: {
      if (n < 7) return nak(ch, type, E_BAD_ARGS);
      uint8_t slot = p[0];
      uint16_t total = rd16(p + 1);
      if (slot >= elx::LIMIT_NUM_SLOTS) return nak(ch, type, E_BAD_SLOT);
      if (total == 0 || total > sizeof g_up) return nak(ch, type, E_TOO_LARGE);
      g_upSlot = slot;
      g_upTotal = total;
      g_upGot = 0;
      g_upCrc = rd32(p + 3);
      ack(ch, type, 0);
      break;
    }
    case UPLOAD_CHUNK: {
      if (g_upSlot == 0xFF || n < 3) return nak(ch, type, E_BAD_SEQUENCE);
      uint16_t off = rd16(p);
      uint16_t len = (uint16_t)(n - 2);
      if (off != g_upGot) return nak(ch, type, E_BAD_SEQUENCE);  // app resends from g_upGot
      if ((uint32_t)off + len > g_upTotal) return nak(ch, type, E_TOO_LARGE);
      memcpy(g_up + off, p + 2, len);
      g_upGot = (uint16_t)(g_upGot + len);
      ack(ch, type, g_upGot);
      break;
    }
    case UPLOAD_END: {
      if (g_upSlot == 0xFF || g_upGot != g_upTotal) return nak(ch, type, E_BAD_SEQUENCE);
      uint8_t slot = g_upSlot;
      g_upSlot = 0xFF;
      if (elx::crc32(g_up, g_upTotal) != g_upCrc) return nak(ch, type, E_CRC_MISMATCH);
      elx::Program probe;
      elx::LoadError le = elx::parseProgram(g_up, g_upTotal, probe, RobotHost::syscallSupported,
                                            RobotHost::eventSupported);
      if (le != elx::LoadError::OK) return nak(ch, type, (uint8_t)le);
      if (!storage::saveSlot(slot, g_up, g_upTotal)) return nak(ch, type, E_STORAGE);
      app::slotsChanged();
      ack(ch, type, g_upTotal);
      break;
    }
    case RUN: {
      if (n < 1 || p[0] >= elx::LIMIT_NUM_SLOTS) return nak(ch, type, E_BAD_SLOT);
      elx::LoadError le;
      if (!app::runSlot(p[0], &le)) return nak(ch, type, le == elx::LoadError::OK ? (uint8_t)E_EMPTY_SLOT : (uint8_t)le);
      ack(ch, type, p[0]);
      break;
    }
    case STOP: app::stopProgram(); ack(ch, type); break;
    case LIST: sendSlots(ch); break;
    case DELETE_SLOT:
      if (n < 1 || p[0] >= elx::LIMIT_NUM_SLOTS) return nak(ch, type, E_BAD_SLOT);
      if (app::runningSlot() == p[0]) app::stopProgram();
      storage::deleteSlot(p[0]);
      if (app::autorunSlot() == p[0]) app::setAutorun(0xFF);
      app::slotsChanged();
      ack(ch, type, p[0]);
      break;
    case SET_AUTORUN:
      if (n < 1 || (p[0] != 0xFF && p[0] >= elx::LIMIT_NUM_SLOTS)) return nak(ch, type, E_BAD_SLOT);
      if (!app::setAutorun(p[0])) return nak(ch, type, E_STORAGE);
      ack(ch, type, p[0]);
      break;
    case TELEMETRY:
      if (n < 2) return nak(ch, type, E_BAD_ARGS);
      ch.telemetryMs = rd16(p);
      if (ch.telemetryMs && ch.telemetryMs < 50) ch.telemetryMs = 50;
      ack(ch, type, ch.telemetryMs);
      break;
    case TRACE:
      if (n < 1) return nak(ch, type, E_BAD_ARGS);
      ch.trace = p[0] != 0;
      ack(ch, type, ch.trace);
      break;
    case CALIBRATE:
      if (n < 1) return nak(ch, type, E_BAD_ARGS);
      if (p[0] == 1) { app::startIrCalibration(); ack(ch, type, 1); }
      else if (p[0] == 2) { app::finishIrCalibration() ? ack(ch, type, 2) : nak(ch, type, E_STORAGE); }
      else if (p[0] == 3) {
        if (app::running()) return nak(ch, type, E_BAD_SEQUENCE);
        sensors.calibrateGyro();
        ack(ch, type, 3);
      } else if (p[0] == 4) {
        app::startMagCalibration() ? ack(ch, type, 4) : nak(ch, type, E_BAD_SEQUENCE);
      } else nak(ch, type, E_BAD_ARGS);
      break;
    default:
      nak(ch, type, E_UNKNOWN_CMD);
  }
}

static void (*g_onCommand)(uint8_t type) = nullptr;

static void feedHandler(void* ctx, uint8_t type, const uint8_t* p, uint16_t n) {
  if (g_onCommand) g_onCommand(type);  // lets the glue switch the robot to Blocks mode
  handle(ctx, type, p, n);
}

// ------------------------------------------------------------------ periodic messages
static void sendTelemetry(Channel& ch) {
  const SensorState& s = sensors.state();
  uint8_t p[44] = {0};
  wr32(p, millis());
  memcpy(p + 4, s.line, 8);
  memcpy(p + 12, s.edge, 4);
  memcpy(p + 16, s.depth, 4);
  wr16(p + 20, (uint16_t)(s.distanceCm * 10));
  wr16(p + 22, (uint16_t)(int16_t)(s.pitch * 10));
  wr16(p + 24, (uint16_t)(int16_t)(s.roll * 10));
  float yaw = fmodf(s.yaw, 360.0f);
  if (yaw > 180) yaw -= 360;
  if (yaw < -180) yaw += 360;
  wr16(p + 26, (uint16_t)(int16_t)(yaw * 10));
  uint8_t touch = 0;
  for (uint8_t i = 0; i < TOUCH_PAD_COUNT; ++i) touch |= (uint8_t)(s.touch[i] << i);
  p[28] = touch;  // bit0 = TTP223
  p[29] = s.soundLevel;
  p[30] = (uint8_t)remote.joyX();
  p[31] = (uint8_t)remote.joyY();
  wr16(p + 32, remote.buttons());
  p[34] = (remote.connected() ? 1 : 0) | (s.imuOk ? 2 : 0) | (s.shaken ? 4 : 0) | (motion.guardActive() ? 8 : 0);
  p[35] = (uint8_t)(int8_t)motion.outLeft();
  p[36] = (uint8_t)(int8_t)motion.outRight();
  p[37] = (uint8_t)(int8_t)s.linePosition;
  p[38] = robotHost.mood();
  p[39] = s.batteryPct;
  wr16(p + 40, (uint16_t)(s.heading * 10));
  p[42] = s.magStrength;
  p[43] = s.magOk ? 1 : 0;
  send(ch, TELEMETRY_DATA, p, sizeof p);
}

static void sendTrace(Channel& ch) {
  elx::VM& vm = app::vm();
  uint8_t p[2 + elx::LIMIT_MAX_SCRIPTS * 3 + 1 + elx::LIMIT_MAX_VARS * 4];
  uint8_t nt = vm.numThreads();
  uint16_t k = 0;
  p[k++] = vm.running() ? 1 : 0;
  p[k++] = nt;
  for (uint8_t i = 0; i < nt; ++i) {
    const elx::Thread& t = vm.thread(i);
    p[k++] = (uint8_t)t.state;
    wr16(p + k, t.pc);  // app maps pc -> block id with the compiler's source map
    k += 2;
  }
  uint8_t nv = vm.loaded() ? vm.program().numVars : 0;
  p[k++] = nv;
  for (uint8_t v = 0; v < nv; ++v) {
    float f = vm.var(v);
    memcpy(p + k, &f, 4);
    k += 4;
  }
  send(ch, TRACE_DATA, p, k);
}

static void periodicFor(Channel& ch) {
  if (!ch.up()) return;
  uint32_t now = millis();
  if (ch.telemetryMs && now - ch.lastTelemetry >= ch.telemetryMs) {
    ch.lastTelemetry = now;
    sendTelemetry(ch);
  }
  if (ch.trace && app::running() && now - ch.lastTrace >= 100) {
    ch.lastTrace = now;
    sendTrace(ch);
  }
}

// ------------------------------------------------------------------ public API
void begin(void (*onCommand)(uint8_t type)) { g_onCommand = onCommand; }

void poll() {
  uint8_t buf[128];
  size_t n;
  while ((n = ble::read(buf, sizeof buf)) > 0)
    for (size_t i = 0; i < n; ++i) g_ble.parser.feed(buf[i], feedHandler, static_cast<Channel*>(&g_ble));
  if (ble::takeDisconnectEvent()) {  // stop streaming to a phone that left
    g_ble.telemetryMs = 0;
    g_ble.trace = false;
  }
}

void periodic() {
  periodicFor(g_ble);
}

void notifyStatus() {
  uint8_t p[3] = {(uint8_t)(app::running() ? 1 : 0), app::runningSlot(), (uint8_t)app::vm().lastError()};
  sendAll(proto::STATUS, p, 3);
}

void reportError(elx::VmError e, uint8_t script, uint16_t pc) {
  uint8_t p[4] = {(uint8_t)e, script, (uint8_t)(pc & 0xFF), (uint8_t)(pc >> 8)};
  sendAll(proto::ERROR, p, 4);
}

bool bleConnected() { return g_ble.up(); }

}  // namespace vmlink
