#pragma once
// Subset of ESP-IDF v5.3 NimBLE host headers (field names/types copied)
#include <stdint.h>
#include <limits.h>
struct os_mbuf;
#define BLE_HS_FOREVER INT32_MAX
#define BLE_HS_CONN_HANDLE_NONE 0xffff
#define BLE_HS_EALREADY 2
#define BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN 0x0d
#define BLE_ATT_ERR_UNLIKELY 0x0e
typedef struct { uint8_t type; } ble_uuid_t;
typedef struct { ble_uuid_t u; uint8_t value[16]; } ble_uuid128_t;
#define BLE_UUID_TYPE_128 128
#define BLE_UUID128_INIT(uuid128...) { .u = { .type = BLE_UUID_TYPE_128 }, .value = { uuid128 } }
struct ble_gatt_access_ctxt { uint8_t op; struct os_mbuf* om; };
#define BLE_GATT_ACCESS_OP_WRITE_CHR 1
typedef int ble_gatt_access_fn(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg);
typedef uint16_t ble_gatt_chr_flags;
#define BLE_GATT_CHR_F_WRITE_NO_RSP 0x0004
#define BLE_GATT_CHR_F_WRITE 0x0008
#define BLE_GATT_CHR_F_NOTIFY 0x0010
#define BLE_GATT_SVC_TYPE_PRIMARY 1
struct ble_gatt_dsc_def; struct ble_gatt_cpfd;
struct ble_gatt_chr_def { const ble_uuid_t* uuid; ble_gatt_access_fn* access_cb; void* arg; struct ble_gatt_dsc_def* descriptors; ble_gatt_chr_flags flags; uint8_t min_key_size; uint16_t* val_handle; struct ble_gatt_cpfd* cpfd; };
struct ble_gatt_svc_def { uint8_t type; const ble_uuid_t* uuid; const struct ble_gatt_svc_def** includes; const struct ble_gatt_chr_def* characteristics; };
struct ble_hs_adv_fields { uint8_t flags; const ble_uuid128_t* uuids128; uint8_t num_uuids128; unsigned uuids128_is_complete:1; const uint8_t* name; uint8_t name_len; unsigned name_is_complete:1; };
#define BLE_HS_ADV_F_DISC_GEN 0x02
#define BLE_HS_ADV_F_BREDR_UNSUP 0x04
struct ble_gap_adv_params { uint8_t conn_mode; uint8_t disc_mode; uint16_t itvl_min; uint16_t itvl_max; };
#define BLE_GAP_CONN_MODE_UND 2
#define BLE_GAP_DISC_MODE_GEN 2
#define BLE_GAP_EVENT_CONNECT 0
#define BLE_GAP_EVENT_DISCONNECT 1
#define BLE_GAP_EVENT_ADV_COMPLETE 9
#define BLE_GAP_EVENT_SUBSCRIBE 14
#define BLE_GAP_EVENT_MTU 15
struct ble_gap_event { uint8_t type; union {
  struct { int status; uint16_t conn_handle; } connect;
  struct { int reason; } disconnect;
  struct { uint16_t conn_handle; uint16_t attr_handle; uint8_t reason; uint8_t prev_notify:1; uint8_t cur_notify:1; } subscribe;
  struct { uint16_t conn_handle; uint16_t channel_id; uint16_t value; } mtu; }; };
typedef int ble_gap_event_fn(struct ble_gap_event* event, void* arg);
typedef struct { uint8_t type; uint8_t val[6]; } ble_addr_t;
typedef void ble_hs_sync_fn(void); typedef void ble_hs_reset_fn(int reason);
struct ble_hs_cfg { ble_hs_reset_fn* reset_cb; ble_hs_sync_fn* sync_cb; };
extern "C" {
extern struct ble_hs_cfg ble_hs_cfg;
int ble_hs_mbuf_to_flat(const struct os_mbuf* om, void* flat, uint16_t max_len, uint16_t* out_copy_len);
struct os_mbuf* ble_hs_mbuf_from_flat(const void* buf, uint16_t len);
int ble_gatts_notify_custom(uint16_t conn_handle, uint16_t att_handle, struct os_mbuf* om);
int ble_gatts_count_cfg(const struct ble_gatt_svc_def* defs); int ble_gatts_add_svcs(const struct ble_gatt_svc_def* svcs);
int ble_gap_adv_set_fields(const struct ble_hs_adv_fields* adv_fields); int ble_gap_adv_rsp_set_fields(const struct ble_hs_adv_fields* rsp_fields);
int ble_gap_adv_start(uint8_t own_addr_type, const ble_addr_t* direct_addr, int32_t duration_ms, const struct ble_gap_adv_params* adv_params, ble_gap_event_fn* cb, void* cb_arg);
int ble_hs_id_infer_auto(int privacy, uint8_t* out_addr_type);
uint16_t ble_att_mtu(uint16_t conn_handle); int ble_att_set_preferred_mtu(uint16_t mtu);
}
