#pragma once
#include <stdint.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NVS_NO_FREE_PAGES 0x110d
#define ESP_ERR_NVS_NEW_VERSION_FOUND 0x1110
#ifdef __cplusplus
extern "C" {
#endif
const char* esp_err_to_name(esp_err_t code);
void _esp_error_check_failed(esp_err_t rc, const char* file, int line, const char* function, const char* expression);
#ifdef __cplusplus
}
#endif
#define ESP_ERROR_CHECK(x) do { esp_err_t err_rc_ = (x); if (err_rc_ != ESP_OK) _esp_error_check_failed(err_rc_, __FILE__, __LINE__, __func__, #x); } while (0)
