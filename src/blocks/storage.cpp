#include "storage.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <FFat.h>

#include "platform.h"
#include "vm/opcodes_gen.h"

static const char* TAG = "storage";
// Programs live on the FAT partition of app3M_fat9M_16MB (mounted at /ffat by FFat).
static const char* BASE = "/ffat";

namespace storage {

static void fullPath(const char* path, char* out, size_t n) { snprintf(out, n, "%s/blocks%s", BASE, path); }
static void slotPath(uint8_t slot, char* out, size_t n) { snprintf(out, n, "%s/blocks/slot%u.elxb", BASE, slot); }
static bool exists(const char* full) {
  struct stat st;
  return stat(full, &st) == 0;
}

bool begin() {
  // First boot: the partition is blank, so format it (takes a few seconds once).
  if (!FFat.begin(true, BASE)) {
    BLK_LOG(TAG, "FFat mount failed");
    return false;
  }
  mkdir("/ffat/blocks", 0775);  // keeps block files apart from anything the games store
  return true;
}

bool saveBlob(const char* path, const void* data, size_t len) {
  // Write to a temp file then rename, so a power cut never leaves half a file.
  char full[64], tmp[72];
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
  char full[64];
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
  char p[64];
  slotPath(slot, p, sizeof p);
  FILE* f = fopen(p, "rb");
  if (!f) return 0;
  size_t n = fread(buf, 1, cap, f);
  bool more = fgetc(f) != EOF;  // file larger than the buffer: reject
  fclose(f);
  return more ? 0 : n;
}

bool deleteSlot(uint8_t slot) {
  char p[64];
  slotPath(slot, p, sizeof p);
  return exists(p) && remove(p) == 0;
}

SlotInfo slotInfo(uint8_t slot) {
  SlotInfo info{};
  char p[64];
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
