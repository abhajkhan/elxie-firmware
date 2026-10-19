#pragma once
// Signatures from espressif/led_strip 3.0.x
#include "esp_err.h"
typedef struct led_strip_t* led_strip_handle_t;
typedef enum { LED_MODEL_WS2812, LED_MODEL_SK6812, LED_MODEL_WS2811, LED_MODEL_WS2816, LED_MODEL_INVALID } led_model_t;
typedef union { struct format_layout { uint32_t r_pos: 2; uint32_t g_pos: 2; uint32_t b_pos: 2; uint32_t w_pos: 2; uint32_t reserved: 19; uint32_t bytes_per_color: 2; uint32_t num_components: 3; } format; uint32_t format_id; } led_color_component_format_t;
typedef struct { int strip_gpio_num; uint32_t max_leds; led_model_t led_model; led_color_component_format_t color_component_format; struct led_strip_extra_flags { uint32_t invert_out: 1; } flags; } led_strip_config_t;
typedef enum { RMT_CLK_SRC_DEFAULT = 0 } rmt_clock_source_t;
typedef struct { rmt_clock_source_t clk_src; uint32_t resolution_hz; size_t mem_block_symbols; struct led_strip_rmt_extra_config { uint32_t with_dma: 1; } flags; } led_strip_rmt_config_t;
extern "C" {
esp_err_t led_strip_new_rmt_device(const led_strip_config_t*, const led_strip_rmt_config_t*, led_strip_handle_t*);
esp_err_t led_strip_set_pixel(led_strip_handle_t, uint32_t index, uint32_t red, uint32_t green, uint32_t blue);
esp_err_t led_strip_refresh(led_strip_handle_t);
esp_err_t led_strip_clear(led_strip_handle_t);
}
