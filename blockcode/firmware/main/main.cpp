// ============================================================================
//  Elxie Block Code — standalone block-coding firmware for the Elxie robot
//  ESP32-S3, ESP-IDF v5.3+
//
//  Core 1: "control" task, 100 Hz  sensors -> remote -> VM -> motion/sound/lights
//  Core 0: NimBLE host, Wi-Fi/ESP-NOW, OLED face task
//
//  Programs arrive over BLE (or USB) as validated bytecode, are stored in 8
//  slots on LittleFS, and run on the VM in components/elx_vm — the same code
//  as the app's simulator and the PC tests.
// ============================================================================
#include "app.h"
#include "board_config.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "hal/platform.h"
#include "hal/sensor_hub.h"
#include "link/protocol.h"
#include "nvs_flash.h"
#include "robot_host.h"
#include "services/face.h"
#include "services/lights.h"
#include "services/motion.h"
#include "services/remote_link.h"
#include "services/sound.h"
#include "services/storage.h"

static const char* TAG = "main";

static void controlTask(void*) {
  esp_task_wdt_add(nullptr);  // a stuck loop resets the robot (motors stop)
  TickType_t last = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(elx::LIMIT_TICK_MS);
  const uint32_t dt = elx::LIMIT_TICK_MS;
  for (;;) {
    link::poll();          // incoming bytes (BLE + USB)
    sensors.update(dt);
    remote.update();
    app::tick(dt);         // e-stop, launcher, calibration
    app::vm().tick();      // run block programs
    robotHost.update();    // finish pet reactions
    motion.update(dt);     // after the VM so new commands apply this tick
    sound.update();
    lights.update();
    link::periodic();      // telemetry / trace
    esp_task_wdt_reset();
    vTaskDelayUntil(&last, period);
  }
}

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "%s firmware %d.%d, bytecode v%d", ELX_FW_NAME, ELX_FW_MAJOR, ELX_FW_MINOR, elx::BYTECODE_VERSION);

  esp_err_t err = nvs_flash_init();  // needed by Wi-Fi and BLE
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
  }

  motion.begin();  // motors off as early as possible
  hal::initI2c();
  sound.begin();
  lights.begin();
  face.begin();
  face.message("Elxie Code", "hold still...");
  storage::begin();
  sensors.begin();
  sensors.calibrateGyro(1000);  // robot must be still for ~1 s at boot

  remote.begin();  // Wi-Fi + ESP-NOW before BLE (radio coexistence)
  link::begin();
  sound.playEffect(sfx::BOOT);
  face.show(elx::enums::face::HAPPY);

  app::begin();
  xTaskCreatePinnedToCore(controlTask, "control", 8192, nullptr, configMAX_PRIORITIES - 2, nullptr, 1);
}
