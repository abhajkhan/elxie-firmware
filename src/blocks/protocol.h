// App <-> robot data link (BLE, and the same protocol over USB serial).
// Frame: A5 5A | type u8 | len u16 LE | payload[len] | crc16-CCITT(type..payload) u16 LE
// See spec/03-protocol.md for every command and payload layout.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "vm/opcodes_gen.h"

namespace proto {

// app -> robot
enum Cmd : uint8_t {
  HELLO = 0x01, PING = 0x02,
  UPLOAD_BEGIN = 0x10, UPLOAD_CHUNK = 0x11, UPLOAD_END = 0x12,
  RUN = 0x13, STOP = 0x14, LIST = 0x15, DELETE_SLOT = 0x16, SET_AUTORUN = 0x17,
  TELEMETRY = 0x20, TRACE = 0x21,
  CALIBRATE = 0x30,
};
// robot -> app
enum Msg : uint8_t {
  ACK = 0x80, INFO = 0x81, PONG = 0x82, SLOTS = 0x85, NAK = 0x8F,
  TELEMETRY_DATA = 0x90, TRACE_DATA = 0x91, STATUS = 0x92, ERROR = 0x93,
};
// NAK reasons (1..15 are elx::LoadError values)
enum Err : uint8_t {
  E_BAD_SLOT = 0x41, E_BAD_SEQUENCE = 0x42, E_TOO_LARGE = 0x43, E_CRC_MISMATCH = 0x44,
  E_STORAGE = 0x45, E_EMPTY_SLOT = 0x46, E_UNKNOWN_CMD = 0x47, E_BAD_ARGS = 0x48,
};

constexpr uint16_t MAX_PAYLOAD = 512;

uint16_t crc16(const uint8_t* d, size_t n, uint16_t crc = 0xFFFF);

class FrameParser {
 public:
  typedef void (*Handler)(void* ctx, uint8_t type, const uint8_t* payload, uint16_t len);
  void feed(uint8_t b, Handler h, void* ctx);

 private:
  uint8_t buf_[MAX_PAYLOAD + 7];
  uint16_t n_ = 0;
  uint16_t need_ = 0;
};

// Builds a frame into `out` (size >= len + 7). Returns frame length.
size_t buildFrame(uint8_t type, const uint8_t* payload, uint16_t len, uint8_t* out);

}  // namespace proto

namespace vmlink {
void begin(void (*onCommand)(uint8_t type));  // onCommand: called for every frame from the app
void poll();      // every loop iteration: parse incoming bytes
void periodic();  // every control tick: telemetry / trace
void notifyStatus();
void reportError(elx::VmError e, uint8_t script, uint16_t pc);
bool bleConnected();
}  // namespace vmlink
