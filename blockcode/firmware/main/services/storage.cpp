#include "storage.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_littlefs.h"
#include "esp_log.h"
#include "opcodes_gen.h"

static const char* TAG = "storage";
static const char* BASE = "/fs";

namespace storage {

static void fullPath(const char* path, char* out, size_t n) { snprintf(out, n, "%s%s", BASE, path); }
static void slotPath(uint8_t slot, char* out, size_t n) { snprintf(out, n, "%s/slot%u.elxb", BASE, slot); }
static bool exists(const char* full) {
  struct stat st;
  return stat(full, &st) == 0;
}

bool begin() {
  esp_vfs_littlefs_conf_t conf = {};
  conf.base_path = BASE;
  conf.partition_label = "storage";
  conf.format_if_mount_failed = true;  // first boot: format
  conf.dont_mount = false;
  esp_err_t err = esp_vfs_littlefs_register(&conf);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "LittleFS mount failed (%s)", esp_err_to_name(err));
    return false;
  }
  return true;
}

bool saveBlob(const char* path, const void* data, size_t len) {
  // Write to a temp file then rename, so a power cut never leaves half a file.
  char full[48], tmp[56];
  fullPath(path, full, sizeof full);
  snprintf(tmp, sizeof tmp, "%s.tmp", full);
  FILE* f = fopen(tmp, "wb");
  if (!f) return false;
  size_t w = fwrite(data, 1, len, f);
  fclose(f);
  if (w != len) {
    remove(tmp);
    return false;
  }
  if (exists(full)) remove(full);
  return rename(tmp, full) == 0;
}

bool loadBlob(const char* path, void* data, size_t len) {
  char full[48];
  fullPath(path, full, sizeof full);
  FILE* f = fopen(full, "rb");
  if (!f) return false;
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  bool ok = size == (long)len && fread(data, 1, len, f) == len;
  fclose(f);
  return ok;
}

bool saveSlot(uint8_t slot, const uint8_t* data, size_t len) {
  if (slot >= elx::LIMIT_NUM_SLOTS) return false;
  char p[16];
  snprintf(p, sizeof p, "/slot%u.elxb", slot);
  return saveBlob(p, data, len);
}

size_t loadSlot(uint8_t slot, uint8_t* buf, size_t cap) {
  if (slot >= elx::LIMIT_NUM_SLOTS) return 0;
  char p[48];
  slotPath(slot, p, sizeof p);
  FILE* f = fopen(p, "rb");
  if (!f) return 0;
  size_t n = fread(buf, 1, cap, f);
  bool more = fgetc(f) != EOF;  // file larger than the buffer: reject
  fclose(f);
  return more ? 0 : n;
}

bool deleteSlot(uint8_t slot) {
  char p[48];
  slotPath(slot, p, sizeof p);
  return exists(p) && remove(p) == 0;
}

SlotInfo slotInfo(uint8_t slot) {
  SlotInfo info{};
  char p[48];
  slotPath(slot, p, sizeof p);
  struct stat st;
  if (stat(p, &st) != 0) return info;
  info.used = true;
  info.size = (uint16_t)st.st_size;
  FILE* f = fopen(p, "rb");
  if (!f) return info;
  uint8_t hdr[28];
  if (fread(hdr, 1, sizeof hdr, f) == sizeof hdr) {
    memcpy(info.name, hdr + 12, 16);
    info.name[16] = 0;
  }
  fclose(f);
  return info;
}

}  // namespace storage
