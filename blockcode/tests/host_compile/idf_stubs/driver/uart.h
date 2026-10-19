#pragma once
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
typedef int uart_port_t; typedef void* QueueHandle_t;
enum { UART_NUM_0 = 0, UART_NUM_1 = 1 };
typedef enum { UART_DATA_8_BITS = 3 } uart_word_length_t; typedef enum { UART_PARITY_DISABLE = 0 } uart_parity_t;
typedef enum { UART_STOP_BITS_1 = 1 } uart_stop_bits_t; typedef enum { UART_HW_FLOWCTRL_DISABLE = 0 } uart_hw_flowcontrol_t;
typedef enum { UART_SCLK_DEFAULT = 0 } uart_sclk_t;
typedef struct { int baud_rate; uart_word_length_t data_bits; uart_parity_t parity; uart_stop_bits_t stop_bits; uart_hw_flowcontrol_t flow_ctrl; uint8_t rx_flow_ctrl_thresh; uart_sclk_t source_clk; } uart_config_t;
extern "C" {
esp_err_t uart_driver_install(uart_port_t, int rx_buffer_size, int tx_buffer_size, int queue_size, QueueHandle_t* uart_queue, int intr_alloc_flags);
esp_err_t uart_param_config(uart_port_t, const uart_config_t*);
int uart_write_bytes(uart_port_t, const void* src, size_t size);
int uart_read_bytes(uart_port_t, void* buf, uint32_t length, TickType_t ticks_to_wait);
}
