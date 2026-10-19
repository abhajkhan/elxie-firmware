#include "uart_transport.h"

#include "board_config.h"
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "esp_log.h"

namespace uart_link {

static bool s_ok = false;

void begin() {
  const uart_port_t port = (uart_port_t)LINK_UART_NUM;
  uart_config_t cfg = {};
  cfg.baud_rate = LINK_UART_BAUD;
  cfg.data_bits = UART_DATA_8_BITS;
  cfg.parity = UART_PARITY_DISABLE;
  cfg.stop_bits = UART_STOP_BITS_1;
  cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  cfg.source_clk = UART_SCLK_DEFAULT;
  s_ok = uart_driver_install(port, 2048, 2048, 0, nullptr, 0) == ESP_OK && uart_param_config(port, &cfg) == ESP_OK;
  if (s_ok) {
    uart_vfs_dev_use_driver(LINK_UART_NUM);  // printf / ESP_LOG now go through the same driver
  } else {
    ESP_LOGE("uart", "UART%d driver install failed", LINK_UART_NUM);
  }
}

size_t read(uint8_t* buf, size_t max) {
  if (!s_ok) return 0;
  int n = uart_read_bytes((uart_port_t)LINK_UART_NUM, buf, max, 0);
  return n > 0 ? (size_t)n : 0;
}

void write(const uint8_t* data, size_t len) {
  if (s_ok) uart_write_bytes((uart_port_t)LINK_UART_NUM, data, len);
}

}  // namespace uart_link
