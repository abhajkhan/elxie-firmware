#pragma once
#include "esp_err.h"
typedef struct { int dummy; } wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() (wifi_init_config_t){ .dummy = 0 }
typedef enum { WIFI_STORAGE_FLASH, WIFI_STORAGE_RAM } wifi_storage_t;
typedef enum { WIFI_MODE_NULL, WIFI_MODE_STA } wifi_mode_t;
typedef enum { WIFI_SECOND_CHAN_NONE = 0 } wifi_second_chan_t;
typedef enum { WIFI_IF_STA = 0, WIFI_IF_AP } wifi_interface_t;
extern "C" {
esp_err_t esp_wifi_init(const wifi_init_config_t*); esp_err_t esp_wifi_set_storage(wifi_storage_t); esp_err_t esp_wifi_set_mode(wifi_mode_t);
esp_err_t esp_wifi_start(void); esp_err_t esp_wifi_get_mac(wifi_interface_t ifx, uint8_t mac[6]); esp_err_t esp_wifi_set_channel(uint8_t primary, wifi_second_chan_t second);
}
