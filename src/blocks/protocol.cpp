#include "protocol.h"

#include <string.h>

namespace proto {

uint16_t crc16(const uint8_t* d, size_t n, uint16_t crc) {
  for (size_t i = 0; i < n; ++i) {
    crc ^= (uint16_t)d[i] << 8;
    for (int k = 0; k < 8; ++k) crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
  }
  return crc;
}

size_t buildFrame(uint8_t type, const uint8_t* payload, uint16_t len, uint8_t* out) {
  out[0] = 0xA5;
  out[1] = 0x5A;
  out[2] = type;
  out[3] = (uint8_t)(len & 0xFF);
  out[4] = (uint8_t)(len >> 8);
  if (len) memcpy(out + 5, payload, len);
  uint16_t c = crc16(out + 2, 3 + len);
  out[5 + len] = (uint8_t)(c & 0xFF);
  out[6 + len] = (uint8_t)(c >> 8);
  return 7u + len;
}

// Byte-at-a-time parser that resynchronises on garbage (e.g. debug text on USB serial).
void FrameParser::feed(uint8_t b, Handler h, void* ctx) {
  if (n_ == 0) {
    if (b == 0xA5) buf_[n_++] = b;
    return;
  }
  if (n_ == 1) {
    if (b == 0x5A) buf_[n_++] = b;
    else n_ = (b == 0xA5) ? 1 : 0;
    return;
  }
  buf_[n_++] = b;
  if (n_ == 5) {
    uint16_t len = (uint16_t)(buf_[3] | (buf_[4] << 8));
    if (len > MAX_PAYLOAD) {
      n_ = 0;
      return;
    }
    need_ = (uint16_t)(len + 7);
  }
  if (n_ >= 5 && n_ == need_) {
    uint16_t len = (uint16_t)(need_ - 7);
    uint16_t got = (uint16_t)(buf_[5 + len] | (buf_[6 + len] << 8));
    if (crc16(buf_ + 2, 3 + len) == got) h(ctx, buf_[2], buf_ + 5, len);
    n_ = 0;
  }
}

}  // namespace proto
