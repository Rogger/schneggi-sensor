#ifndef OTA_TEST_STUBS_H
#define OTA_TEST_STUBS_H
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <stddef.h>
#define CONFIG_ZIGBEE_LONG_POLL_INTERVAL_MS 120000
#define CONFIG_ZIGBEE_FOTA_MANUFACTURER_ID 0xFFF1
#define CONFIG_ZIGBEE_FOTA_IMAGE_TYPE 0x8102
#define CONFIG_ZIGBEE_FOTA_ENDPOINT 5
#define ZB_ZCL_OTA_UPGRADE_IMAGE_STATUS_NORMAL 0
#define ZB_ZCL_OTA_UPGRADE_IMAGE_STATUS_DOWNLOADED 2
#define ZB_ZCL_OTA_UPGRADE_IMAGE_STATUS_WAITING_UPGRADE 3
#define ZB_ZCL_OTA_UPGRADE_IMAGE_STATUS_COUNT_DOWN 4
#define CONFIG_RAM_POWER_DOWN_LIBRARY 1
#define IS_ENABLED(x) (x)
#define ARG_UNUSED(x) (void)(x)
#define ZVUNUSED(x) (void)(x)
#define LOG_MODULE_REGISTER(...)
#define LOG_INF(...)
#define LOG_DBG(...)
#define LOG_WRN(...)
#define LOG_ERR(...)
#define LOG_LEVEL_INF 3
#define RET_OK 0
#define RET_NOT_FOUND -2
#define RET_NOT_IMPLEMENTED -1
#define ZB_ALARM_ANY_PARAM 0xff
#define ZB_ALARM_ALL_CB 0xfe
#define ZB_BEACON_INTERVAL_USEC 15360U
#define ZB_MILLISECONDS_TO_BEACON_INTERVAL(x) (x)
#define ZB_ZCL_OTA_UPGRADE_VALUE_CB_ID 1
#define ZB_ZDO_SIGNAL_SKIP_STARTUP 1
#define ZB_ZDO_SIGNAL_LEAVE 2
#define SYS_REBOOT_COLD 0
#define WDT_FLAG_RESET_SOC 1
#define WDT_OPT_PAUSE_HALTED_BY_DBG 2
typedef uint8_t zb_uint8_t;
typedef uint32_t zb_uint32_t;
typedef uint8_t zb_bufid_t;
typedef int zb_ret_t;
typedef int zb_zdo_app_signal_type_t;
enum {
 ZB_ZCL_OTA_UPGRADE_STATUS_START,
 ZB_ZCL_OTA_UPGRADE_STATUS_APPLY,
 ZB_ZCL_OTA_UPGRADE_STATUS_RECEIVE,
 ZB_ZCL_OTA_UPGRADE_STATUS_FINISH,
 ZB_ZCL_OTA_UPGRADE_STATUS_ABORT,
 ZB_ZCL_OTA_UPGRADE_STATUS_CHECK,
 ZB_ZCL_OTA_UPGRADE_STATUS_OK,
 ZB_ZCL_OTA_UPGRADE_STATUS_ERROR,
 ZB_ZCL_OTA_UPGRADE_STATUS_REQUIRE_MORE_IMAGE,
 ZB_ZCL_OTA_UPGRADE_STATUS_BUSY,
};
typedef struct {
 zb_uint8_t upgrade_status;
 union {
   struct { uint16_t manufacturer, image_type; uint32_t file_version, file_length; } start;
   struct { uint32_t file_offset; uint8_t data_length; uint8_t *block_data; } receive;
 } upgrade;
} zb_zcl_ota_upgrade_value_param_t;
typedef struct {
 int device_cb_id;
 int status;
 struct { zb_zcl_ota_upgrade_value_param_t ota_value_param; } cb_param;
} zb_zcl_device_callback_param_t;
extern zb_zcl_device_callback_param_t test_cb;
#define ZB_BUF_GET_PARAM(buf, type) ((type *)&test_cb)
typedef void (*zb_callback_t)(zb_uint8_t);
typedef zb_callback_t alarm_cb;
typedef uint32_t zb_time_t;
int alarm_schedule(alarm_cb cb, zb_uint8_t param, unsigned int delay);
int alarm_cancel(alarm_cb cb, zb_uint8_t param);
#define ZB_SCHEDULE_APP_ALARM alarm_schedule
#define ZB_SCHEDULE_APP_ALARM_CANCEL alarm_cancel
#define ZB_SCHEDULE_APP_CALLBACK callback_schedule
int callback_schedule(alarm_cb cb, zb_uint8_t param);
#define ZB_ZCL_CLUSTER_ID_OTA_UPGRADE 0x19
#define ZB_ZCL_CLUSTER_CLIENT_ROLE 0
#define ZB_ZCL_ATTR_OTA_UPGRADE_FILE_OFFSET_ID 2
#define ZB_FALSE 0
void set_offset(unsigned int ep, unsigned int cluster, unsigned int role,
                unsigned int attr, uint8_t *data, bool check);
#define ZB_ZCL_SET_ATTRIBUTE set_offset
#define BOOT_UPGRADE_TEST 0
int boot_request_upgrade(int mode);
extern alarm_cb registered_zcl;
#define ZB_ZCL_REGISTER_DEVICE_CB(cb) (registered_zcl = cb)
int signal_status(zb_bufid_t buf);
#define ZB_GET_APP_SIGNAL_STATUS signal_status
zb_zdo_app_signal_type_t zb_get_app_signal(zb_bufid_t buf, void *hdr);
void zb_set_node_descriptor_manufacturer_code_req(unsigned int id, void (*cb)(zb_ret_t));
void zb_zdo_pim_set_long_poll_interval(unsigned int ms);
void zb_zdo_pim_start_turbo_poll_continuous(unsigned int ms);
zb_uint8_t zb_zcl_ota_upgrade_get_ota_status(zb_uint8_t endpoint);
struct device { int unused; };
extern const struct device test_device;
#define DEVICE_DT_GET(x) (&test_device)
#define DT_ALIAS(x) 0
bool device_is_ready(const struct device *dev);
struct wdt_timeout_cfg {
 struct { int min; int max; } window;
 int flags;
};
int wdt_install_timeout(const struct device *, const struct wdt_timeout_cfg *);
int wdt_setup(const struct device *, int);
int wdt_feed(const struct device *, int);
bool boot_is_img_confirmed(void);
int boot_write_img_confirmed(void);
void power_up_unused_ram(void);
void sys_reboot(int kind);
enum zigbee_fota_evt_id { ZIGBEE_FOTA_EVT_PROGRESS, ZIGBEE_FOTA_EVT_FINISHED, ZIGBEE_FOTA_EVT_ERROR };
struct zigbee_fota_evt { enum zigbee_fota_evt_id id; struct { int progress; } dl; };
int zigbee_fota_init(void (*callback)(const struct zigbee_fota_evt *));
void zigbee_fota_abort(void);
void zigbee_fota_signal_handler(zb_bufid_t);
void zigbee_fota_zcl_cb(zb_bufid_t);
#endif
