// Persistent storage (LittleFS): program slots, settings, IR calibration.
#pragma once
#include <stddef.h>
#include <stdint.h>

struct SlotInfo {
  bool used;
  uint16_t size;
  char name[17];
};

namespace storage {
bool begin();
bool saveSlot(uint8_t slot, const uint8_t* data, size_t len);
size_t loadSlot(uint8_t slot, uint8_t* buf, size_t cap);
bool deleteSlot(uint8_t slot);
SlotInfo slotInfo(uint8_t slot);
bool saveBlob(const char* path, const void* data, size_t len);
bool loadBlob(const char* path, void* data, size_t len);
}  // namespace storage
