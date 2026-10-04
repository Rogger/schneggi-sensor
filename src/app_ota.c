#include "app_ota.h"
#include "app_reboot.h"
#include "app_scheduler.h"
#include <zigbee/zigbee_fota.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app_ota, LOG_LEVEL_INF);

/* All Zigbee state and alarms below are accessed on the ZBOSS thread. */
static bool downloading;
static bool waiting_for_install;
static bool peripherals_ok;
static bool sleepy_device = true;
static uint32_t normal_poll_ms = CONFIG_ZIGBEE_LONG_POLL_INTERVAL_MS;
static const struct device *const watchdog = DEVICE_DT_GET(DT_ALIAS(watchdog0));
static int watchdog_channel;

static void manufacturer_set(zb_ret_t status)
{
  if (status != RET_OK) {
    LOG_ERR("Unable to set OTA manufacturer code: %d", status);
  }
}

static void download_timeout(zb_uint8_t unused);
static void response_timeout(zb_uint8_t unused);

static void end_download(void)
{
  downloading = false;
  waiting_for_install = false;
  ZB_SCHEDULE_APP_ALARM_CANCEL(download_timeout, ZB_ALARM_ALL_CB);
  ZB_SCHEDULE_APP_ALARM_CANCEL(response_timeout, ZB_ALARM_ALL_CB);
  if (sleepy_device) {
    zb_zdo_pim_set_long_poll_interval(normal_poll_ms);
  }
}

static void response_timeout(zb_uint8_t unused)
{
  ARG_UNUSED(unused);
  /* An indefinite deferral changes the stack state without an APPLY callback.
   * Only DOWNLOADED means the Upgrade End response has not been accepted. */
  if (!waiting_for_install ||
      zb_zcl_ota_upgrade_get_ota_status(CONFIG_ZIGBEE_FOTA_ENDPOINT) !=
        ZB_ZCL_OTA_UPGRADE_IMAGE_STATUS_DOWNLOADED) {
    return;
  }
  LOG_WRN("OTA server response missing; rebooting to reset the OTA session");
  zigbee_fota_abort();
  end_download();
  app_reboot();
}

static void download_timeout(zb_uint8_t unused)
{
  ARG_UNUSED(unused);
  if (!downloading) {
    return;
  }
  LOG_WRN("OTA transfer stalled; rebooting to reset the OTA session");
  zigbee_fota_abort();
  end_download();
  app_reboot();
}

static void download_activity(void)
{
  downloading = true;
  waiting_for_install = false;
  if (sleepy_device) {
    zb_zdo_pim_set_long_poll_interval(100);
  }
  if (app_alarm_replace(download_timeout, 0, 300000) != RET_OK) {
    /* A transfer must have a bounded exit path on both power profiles. */
    download_timeout(0);
  }
}

void app_ota_set_sleepy(bool sleepy)
{
  sleepy_device = sleepy;
}

void app_ota_set_long_poll(uint32_t interval_ms)
{
  normal_poll_ms = interval_ms;
  if (sleepy_device) {
    zb_zdo_pim_set_long_poll_interval(downloading ? 100 : interval_ms);
  }
}

static void ota_event(const struct zigbee_fota_evt *evt)
{
  switch (evt->id) {
  case ZIGBEE_FOTA_EVT_PROGRESS:
    LOG_DBG("OTA progress: %d%%", evt->dl.progress);
    break;
  case ZIGBEE_FOTA_EVT_FINISHED:
    end_download();
    app_reboot();
    break;
  case ZIGBEE_FOTA_EVT_ERROR:
    end_download();
    LOG_WRN("OTA transfer failed");
    break;
  }
}

static void zcl_callback(zb_bufid_t bufid)
{
  zb_zcl_device_callback_param_t *cb = ZB_BUF_GET_PARAM(bufid, zb_zcl_device_callback_param_t);
  if (cb->device_cb_id != ZB_ZCL_OTA_UPGRADE_VALUE_CB_ID) {
    cb->status = RET_NOT_IMPLEMENTED;
    return;
  }
  zb_uint8_t status = cb->cb_param.ota_value_param.upgrade_status;
  if ((status == ZB_ZCL_OTA_UPGRADE_STATUS_START ||
       status == ZB_ZCL_OTA_UPGRADE_STATUS_RECEIVE) &&
      !boot_is_img_confirmed()) {
    /* Slot 1 still contains the rollback image during a trial boot. Reject
     * before the Nordic library can initialize or write the DFU target.
     * ABORT allows a later query to retry after confirmation; BUSY would
     * require explicitly resuming the suspended transfer. */
    cb->status = RET_OK;
    cb->cb_param.ota_value_param.upgrade_status = ZB_ZCL_OTA_UPGRADE_STATUS_ABORT;
    return;
  }
  zigbee_fota_zcl_cb(bufid);
  zb_uint8_t result = cb->cb_param.ota_value_param.upgrade_status;
  if ((status == ZB_ZCL_OTA_UPGRADE_STATUS_START ||
       status == ZB_ZCL_OTA_UPGRADE_STATUS_RECEIVE) &&
      result == ZB_ZCL_OTA_UPGRADE_STATUS_OK) {
    download_activity();
  } else if ((status == ZB_ZCL_OTA_UPGRADE_STATUS_CHECK ||
              status == ZB_ZCL_OTA_UPGRADE_STATUS_APPLY) &&
             result == ZB_ZCL_OTA_UPGRADE_STATUS_OK) {
    /* ZBOSS owns the upgrade-time countdown, including indefinite server
     * deferral. A completed image must not expire as a stalled download. */
    end_download();
    waiting_for_install = true;
    if (status == ZB_ZCL_OTA_UPGRADE_STATUS_CHECK) {
      /* A sleepy device polls briefly for Upgrade End; USB receivers stay on.
       * The response deadline applies to both profiles. */
      if (sleepy_device) {
        zb_zdo_pim_start_turbo_poll_continuous(30000);
      }
      if (app_alarm_replace(response_timeout, 0, 300000) != RET_OK) {
        response_timeout(0);
      }
    }
  } else if (result == ZB_ZCL_OTA_UPGRADE_STATUS_ERROR ||
             result == ZB_ZCL_OTA_UPGRADE_STATUS_ABORT) {
    end_download();
  }
}

static void health_tick(zb_uint8_t unused)
{
  ARG_UNUSED(unused);
  /* Reserve the next health tick before confirming a trial image. A full
   * scheduler must not leave a confirmed image without watchdog service.
   */
  if (app_alarm_replace(health_tick, 0, 30000) != RET_OK) {
    LOG_ERR("Unable to schedule watchdog service; restarting");
    app_reboot();
    return;
  }
  /* Reaching this alarm proves the stack scheduler runs. Do not require a
   * coordinator to be online to confirm a locally healthy image. */
  if (!boot_is_img_confirmed()) {
    if (!peripherals_ok || boot_write_img_confirmed() != 0) {
      LOG_ERR("Trial firmware failed startup checks; reverting");
      app_reboot();
      return;
    }
  }
  wdt_feed(watchdog, watchdog_channel);
}

void app_ota_startup_ready(bool ready)
{
  peripherals_ok = ready;
}

void app_ota_signal(zb_bufid_t bufid)
{
  zigbee_fota_signal_handler(bufid);
  zb_zdo_app_signal_type_t signal = zb_get_app_signal(bufid, NULL);
  if (signal == ZB_ZDO_SIGNAL_SKIP_STARTUP && ZB_GET_APP_SIGNAL_STATUS(bufid) == RET_OK) {
    zb_set_node_descriptor_manufacturer_code_req(CONFIG_ZIGBEE_FOTA_MANUFACTURER_ID,
                                                 manufacturer_set);
    if (app_alarm_replace(health_tick, 0, 10000) != RET_OK) {
      LOG_ERR("Unable to start watchdog service; restarting");
      app_reboot();
      return;
    }
  }
  if (signal == ZB_ZDO_SIGNAL_LEAVE && ZB_GET_APP_SIGNAL_STATUS(bufid) == RET_OK &&
      (downloading || waiting_for_install)) {
    zigbee_fota_abort();
    end_download();
    /* The Nordic abort resets only DFU parsing, not ZBOSS protocol state or
     * a scheduled FINISH callback. Reset both after main's leave handling. */
    app_reboot();
  }
}

int app_ota_init(void)
{
  if (!device_is_ready(watchdog)) {
    return -ENODEV;
  }
  struct wdt_timeout_cfg timeout = {
    .window = { .min = 0, .max = 120000 },
    .flags = WDT_FLAG_RESET_SOC,
  };
  watchdog_channel = wdt_install_timeout(watchdog, &timeout);
  if (watchdog_channel < 0) {
    return watchdog_channel;
  }
  int err = wdt_setup(watchdog, WDT_OPT_PAUSE_HALTED_BY_DBG);
  if (err) {
    return err;
  }
  err = zigbee_fota_init(ota_event);
  if (!err) {
    ZB_ZCL_REGISTER_DEVICE_CB(zcl_callback);
  }
  return err;
}
