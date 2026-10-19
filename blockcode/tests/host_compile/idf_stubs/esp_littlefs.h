#pragma once
#include "esp_err.h"
typedef struct { const char* base_path; const char* partition_label; void* partition; uint8_t format_if_mount_failed: 1; uint8_t read_only: 1; uint8_t dont_mount: 1; uint8_t grow_on_mount: 1; } esp_vfs_littlefs_conf_t;
extern "C" esp_err_t esp_vfs_littlefs_register(const esp_vfs_littlefs_conf_t* conf);
