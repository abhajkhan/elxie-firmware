// UART0 (through the board's USB-UART chip) as a byte stream for the app/PC link.
// The ESP-IDF console shares the same UART; frames and log lines coexist
// because the frame parser on both sides skips non-frame bytes.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace uart_link {
void begin();
size_t read(uint8_t* buf, size_t max);        // non-blocking
void write(const uint8_t* data, size_t len);
}  // namespace uart_link
