#include "remote_link.h"

#include <string.h>

#include "board_config.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "hal/platform.h"

RemoteLink remote;

static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile RemoteInputPacket g_last;
static volatile uint32_t g_lastMs = 0;
static uint8_t g_peer[6] = {0};
static volatile bool g_havePeer = false;
static bool g_peerAdded = false;

static const uint8_t kRemoteMac[6] = REMOTE_MAC;

static void onRecvImpl(const uint8_t* mac, const uint8_t* data, int len) {
  if (memcmp(mac, kRemoteMac, 6) != 0) return;  // only our paired remote
  if (len < (int)sizeof(RemoteInputPacket) || data[0] != 0xE1 || data[1] != 1) return;
  portENTER_CRITICAL(&g_mux);
  memcpy((void*)&g_last, data, sizeof(RemoteInputPacket));
  memcpy(g_peer, mac, 6);
  g_havePeer = true;
  g_lastMs = millis();
  portEXIT_CRITICAL(&g_mux);
}

static void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  onRecvImpl(info->src_addr, data, len);
}

void RemoteLink::begin() {
  // Wi-Fi in station mode only as a carrier for ESP-NOW (never connects to an AP).
  esp_netif_init();
  esp_event_loop_create_default();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  if (esp_wifi_init(&cfg) != ESP_OK) {
    ESP_LOGE("remote", "wifi init failed");
    return;
  }
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) {
    ESP_LOGE("remote", "esp_now_init failed");
    return;
  }
  esp_now_register_recv_cb(onRecv);
}

void RemoteLink::update() {
  RemoteInputPacket p;
  uint32_t last;
  portENTER_CRITICAL(&g_mux);
  memcpy(&p, (const void*)&g_last, sizeof p);
  last = g_lastMs;
  portEXIT_CRITICAL(&g_mux);
  connected_ = last != 0 && (millis() - last) < REMOTE_TIMEOUT_MS;
  x_ = constrain(p.joyX, -100, 100);
  y_ = constrain(p.joyY, -100, 100);
  buttons_ = p.buttons;
  if (g_havePeer && !g_peerAdded) {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, g_peer, 6);
    peer.channel = ESPNOW_CHANNEL;
    peer.encrypt = false;
    peer.ifidx = WIFI_IF_STA;
    g_peerAdded = esp_now_add_peer(&peer) == ESP_OK;
  }
}

void RemoteLink::buzz(uint16_t ms, uint8_t strength) {
  if (!connected_ || !g_peerAdded) return;
  RemoteHapticPacket h{0xE1, 2, (uint16_t)min<uint16_t>(ms, 2000), strength};
  esp_now_send(g_peer, (const uint8_t*)&h, sizeof h);
}
