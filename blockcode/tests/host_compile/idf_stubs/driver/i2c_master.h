#pragma once
#include "esp_err.h"
#include "driver/gpio.h"
#include <stddef.h>
typedef int i2c_port_num_t; typedef struct i2c_master_bus_t* i2c_master_bus_handle_t; typedef struct i2c_master_dev_t* i2c_master_dev_handle_t;
#define I2C_NUM_0 0
typedef enum { I2C_CLK_SRC_DEFAULT = 0 } i2c_clock_source_t;
typedef enum { I2C_ADDR_BIT_LEN_7 = 0, I2C_ADDR_BIT_LEN_10 = 1 } i2c_addr_bit_len_t;
typedef struct { i2c_port_num_t i2c_port; gpio_num_t sda_io_num; gpio_num_t scl_io_num; union { i2c_clock_source_t clk_source; }; uint8_t glitch_ignore_cnt; int intr_priority; size_t trans_queue_depth; struct { uint32_t enable_internal_pullup: 1; } flags; } i2c_master_bus_config_t;
typedef struct { i2c_addr_bit_len_t dev_addr_length; uint16_t device_address; uint32_t scl_speed_hz; uint32_t scl_wait_us; struct { uint32_t disable_ack_check: 1; } flags; } i2c_device_config_t;
extern "C" {
esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t*, i2c_master_bus_handle_t*);
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t, const i2c_device_config_t*, i2c_master_dev_handle_t*);
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t, const uint8_t*, size_t, int);
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t, const uint8_t*, size_t, uint8_t*, size_t, int);
esp_err_t i2c_master_probe(i2c_master_bus_handle_t, uint16_t, int);
}
