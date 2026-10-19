#pragma once
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
typedef struct { uint32_t tx_buffer_size; uint32_t rx_buffer_size; } usb_serial_jtag_driver_config_t;
#define USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT() (usb_serial_jtag_driver_config_t) { .tx_buffer_size = 256, .rx_buffer_size = 256 }
extern "C" {
esp_err_t usb_serial_jtag_driver_install(usb_serial_jtag_driver_config_t*);
int usb_serial_jtag_read_bytes(void* buf, uint32_t length, TickType_t ticks_to_wait);
int usb_serial_jtag_write_bytes(const void* src, size_t size, TickType_t ticks_to_wait);
}
