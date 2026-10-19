#include "ble_transport.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char* TAG = "ble";

namespace ble {

// UUIDs from spec/03-protocol.md, little-endian byte order:
//   service 8d3a0001-5e1c-4a8e-9b2d-e1a1e0c0de01, RX ...0002..., TX ...0003...
static const ble_uuid128_t kSvcUuid = BLE_UUID128_INIT(0x01, 0xde, 0xc0, 0xe0, 0xa1, 0xe1, 0x2d, 0x9b, 0x8e, 0x4a,
                                                       0x1c, 0x5e, 0x01, 0x00, 0x3a, 0x8d);
static const ble_uuid128_t kRxUuid = BLE_UUID128_INIT(0x01, 0xde, 0xc0, 0xe0, 0xa1, 0xe1, 0x2d, 0x9b, 0x8e, 0x4a,
                                                      0x1c, 0x5e, 0x02, 0x00, 0x3a, 0x8d);
static const ble_uuid128_t kTxUuid = BLE_UUID128_INIT(0x01, 0xde, 0xc0, 0xe0, 0xa1, 0xe1, 0x2d, 0x9b, 0x8e, 0x4a,
                                                      0x1c, 0x5e, 0x03, 0x00, 0x3a, 0x8d);

static uint16_t s_rxHandle = 0, s_txHandle = 0;
static volatile uint16_t s_conn = BLE_HS_CONN_HANDLE_NONE;
static volatile bool s_subscribed = false;
static volatile bool s_disconnected = false;
static uint8_t s_ownAddrType = 0;
static char s_name[24] = "Elxie";
static StreamBufferHandle_t s_rx = nullptr;

static struct ble_gatt_chr_def s_chrs[3];
static struct ble_gatt_svc_def s_svcs[2];

static int chrAccess(uint16_t, uint16_t attrHandle, struct ble_gatt_access_ctxt* ctxt, void*) {
  if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR && attrHandle == s_rxHandle) {
    uint8_t buf[512];
    uint16_t len = 0;
    if (ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof buf, &len) != 0) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    if (s_rx && len) xStreamBufferSend(s_rx, buf, len, 0);
    return 0;
  }
  return BLE_ATT_ERR_UNLIKELY;
}

static void advertise();

static int gapEvent(struct ble_gap_event* e, void*) {
  switch (e->type) {
    case BLE_GAP_EVENT_CONNECT:
      if (e->connect.status == 0) {
        s_conn = e->connect.conn_handle;
        ESP_LOGI(TAG, "connected");
      } else {
        advertise();
      }
      break;
    case BLE_GAP_EVENT_DISCONNECT:
      ESP_LOGI(TAG, "disconnected");
      s_conn = BLE_HS_CONN_HANDLE_NONE;
      s_subscribed = false;
      s_disconnected = true;
      advertise();
      break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
      advertise();
      break;
    case BLE_GAP_EVENT_SUBSCRIBE:
      if (e->subscribe.attr_handle == s_txHandle) s_subscribed = e->subscribe.cur_notify;
      break;
    case BLE_GAP_EVENT_MTU:
      ESP_LOGI(TAG, "MTU %u", e->mtu.value);
      break;
    default:
      break;
  }
  return 0;
}

static void advertise() {
  struct ble_hs_adv_fields f;
  memset(&f, 0, sizeof f);
  f.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
  f.uuids128 = &kSvcUuid;
  f.num_uuids128 = 1;
  f.uuids128_is_complete = 1;
  if (ble_gap_adv_set_fields(&f) != 0) ESP_LOGE(TAG, "adv fields");

  struct ble_hs_adv_fields rsp;
  memset(&rsp, 0, sizeof rsp);
  rsp.name = (const uint8_t*)s_name;
  rsp.name_len = (uint8_t)strlen(s_name);
  rsp.name_is_complete = 1;
  ble_gap_adv_rsp_set_fields(&rsp);

  struct ble_gap_adv_params p;
  memset(&p, 0, sizeof p);
  p.conn_mode = BLE_GAP_CONN_MODE_UND;
  p.disc_mode = BLE_GAP_DISC_MODE_GEN;
  int rc = ble_gap_adv_start(s_ownAddrType, nullptr, BLE_HS_FOREVER, &p, gapEvent, nullptr);
  if (rc != 0 && rc != BLE_HS_EALREADY) ESP_LOGE(TAG, "adv start rc=%d", rc);
}

static void onSync() {
  ble_hs_util_ensure_addr(0);
  ble_hs_id_infer_auto(0, &s_ownAddrType);
  advertise();
  ESP_LOGI(TAG, "advertising as %s", s_name);
}

static void onReset(int reason) { ESP_LOGW(TAG, "host reset, reason %d", reason); }

static void hostTask(void*) {
  nimble_port_run();  // returns only when nimble_port_stop() is called
  nimble_port_freertos_deinit();
}

void begin(const char* deviceName) {
  strncpy(s_name, deviceName, sizeof s_name - 1);
  s_rx = xStreamBufferCreate(2048, 1);

  if (nimble_port_init() != ESP_OK) {
    ESP_LOGE(TAG, "nimble_port_init failed");
    return;
  }
  ble_hs_cfg.sync_cb = onSync;
  ble_hs_cfg.reset_cb = onReset;

  memset(s_chrs, 0, sizeof s_chrs);
  s_chrs[0].uuid = &kRxUuid.u;
  s_chrs[0].access_cb = chrAccess;
  s_chrs[0].flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP;
  s_chrs[0].val_handle = &s_rxHandle;
  s_chrs[1].uuid = &kTxUuid.u;
  s_chrs[1].access_cb = chrAccess;
  s_chrs[1].flags = BLE_GATT_CHR_F_NOTIFY;
  s_chrs[1].val_handle = &s_txHandle;
  memset(s_svcs, 0, sizeof s_svcs);
  s_svcs[0].type = BLE_GATT_SVC_TYPE_PRIMARY;
  s_svcs[0].uuid = &kSvcUuid.u;
  s_svcs[0].characteristics = s_chrs;

  ble_svc_gap_init();
  ble_svc_gatt_init();
  if (ble_gatts_count_cfg(s_svcs) != 0 || ble_gatts_add_svcs(s_svcs) != 0) {
    ESP_LOGE(TAG, "GATT table registration failed");
    return;
  }
  ble_svc_gap_device_name_set(s_name);
  ble_att_set_preferred_mtu(247);
  nimble_port_freertos_init(hostTask);
}

bool connected() { return s_conn != BLE_HS_CONN_HANDLE_NONE && s_subscribed; }

size_t read(uint8_t* buf, size_t max) { return s_rx ? xStreamBufferReceive(s_rx, buf, max, 0) : 0; }

bool send(const uint8_t* data, size_t len) {
  uint16_t conn = s_conn;
  if (conn == BLE_HS_CONN_HANDLE_NONE || !s_subscribed) return false;
  uint16_t mtu = ble_att_mtu(conn);
  size_t chunk = mtu > 23 ? mtu - 3 : 20;  // frames may span notifications
  for (size_t off = 0; off < len; off += chunk) {
    size_t n = len - off < chunk ? len - off : chunk;
    struct os_mbuf* om = ble_hs_mbuf_from_flat(data + off, (uint16_t)n);
    if (!om) return false;  // out of buffers: drop (telemetry is best-effort; commands get retried)
    if (ble_gatts_notify_custom(conn, s_txHandle, om) != 0) return false;
  }
  return true;
}

bool takeDisconnectEvent() {
  bool d = s_disconnected;
  s_disconnected = false;
  return d;
}

}  // namespace ble
