#include <assert.h>
#include <zboss_api.h>
#include "../../../src/app_ota.c"

const struct device test_device = {0};
zb_zcl_device_callback_param_t test_cb;
alarm_cb registered_zcl;
static unsigned int poll_ms, poll_calls, reboots, restores, aborts, feeds, confirms;
static int confirm_result, library_result, watchdog_result;
static bool confirmed, ready = true;
static int current_signal, current_status;
static alarm_cb pending_download, pending_health;
static unsigned int download_delay;
static int schedule_result;
static unsigned int library_calls, turbo_ms, turbo_calls;
static alarm_cb pending_response;
static unsigned int response_delay;
static zb_uint8_t ota_status;
static unsigned int discards, installs, received_offset;
static int transfer_result;
static int install_result;
static unsigned int transfer_initializations;
static alarm_cb pending_install;

int app_ota_storage_init(void) { return 0; }
const struct ota_storage app_ota_storage = {0};
int ota_transfer_init(const struct ota_storage *storage)
{ assert(storage == &app_ota_storage); transfer_initializations++; return 0; }
int ota_transfer_start(const struct ota_storage *storage, const struct ota_image_id *id)
{ assert(storage == &app_ota_storage); (void)id; return transfer_result; }
int ota_transfer_receive(uint32_t offset, const uint8_t *data, size_t size)
{ (void)offset; (void)data; (void)size; return transfer_result; }
uint32_t ota_transfer_offset(void) { return received_offset; }
int ota_transfer_check(void) { return transfer_result; }
int ota_transfer_discard(void) { discards++; return 0; }
int boot_request_upgrade(int mode)
{ assert(mode == BOOT_UPGRADE_TEST); installs++; return install_result; }
int callback_schedule(alarm_cb cb, zb_uint8_t param)
{ assert(param == 0); pending_install = cb; return schedule_result; }
void set_offset(unsigned int ep, unsigned int cluster, unsigned int role,
                unsigned int attr, uint8_t *data, bool check)
{ assert(ep == 5 && cluster == 0x19 && role == 0 && attr == 2 && !check);
  assert(*(uint32_t *)data == received_offset); }

#if defined(OTA_USB_PROFILE)
#define POLL_IS(ms) (poll_calls == 0)
#define TURBO_IS(ms) (turbo_calls == 0)
#else
#define POLL_IS(ms) (poll_ms == (ms))
#define TURBO_IS(ms) (turbo_ms == (ms))
#endif

int alarm_schedule(alarm_cb cb, zb_uint8_t param, unsigned int delay)
{
  (void)param;
  if (schedule_result != RET_OK) { return schedule_result; }
  if (cb == download_timeout) { pending_download = cb; download_delay = delay; }
  if (cb == health_tick) { pending_health = cb; }
  if (cb == response_timeout) { pending_response = cb; response_delay = delay; }
  return schedule_result;
}
int alarm_cancel(alarm_cb cb, zb_uint8_t param)
{
  (void)param;
  if (cb == download_timeout) { pending_download = NULL; }
  if (cb == response_timeout) { pending_response = NULL; }
  return 0;
}
int signal_status(zb_bufid_t buf) { (void)buf; return current_status; }
zb_zdo_app_signal_type_t zb_get_app_signal(zb_bufid_t buf, void *hdr)
{ (void)buf; (void)hdr; return current_signal; }
void zb_set_node_descriptor_manufacturer_code_req(unsigned int id, void (*cb)(zb_ret_t))
{ assert(id == 0xFFF1); cb(RET_OK); }
void zb_zdo_pim_set_long_poll_interval(unsigned int ms) { poll_ms = ms; poll_calls++; }
void zb_zdo_pim_start_turbo_poll_continuous(unsigned int ms) { turbo_ms = ms; turbo_calls++; }
zb_uint8_t zb_zcl_ota_upgrade_get_ota_status(zb_uint8_t endpoint)
{ assert(endpoint == CONFIG_ZIGBEE_FOTA_ENDPOINT); return ota_status; }
bool device_is_ready(const struct device *dev) { (void)dev; return ready; }
int wdt_install_timeout(const struct device *dev, const struct wdt_timeout_cfg *cfg)
{ (void)dev; assert(cfg->window.max == 120000); return watchdog_result; }
int wdt_setup(const struct device *dev, int flags)
{ (void)dev; assert(flags == WDT_OPT_PAUSE_HALTED_BY_DBG); return 0; }
int wdt_feed(const struct device *dev, int channel)
{ (void)dev; (void)channel; feeds++; return 0; }
bool boot_is_img_confirmed(void) { return confirmed; }
int boot_write_img_confirmed(void)
{
  confirms++;
  if (confirm_result == 0) { confirmed = true; }
  return confirm_result;
}
void power_up_unused_ram(void) { restores++; }
void sys_reboot(int kind) { assert(kind == SYS_REBOOT_COLD); reboots++; }
int zigbee_fota_init(void (*callback)(const struct zigbee_fota_evt *))
{ assert(callback == ota_event); return 0; }
void zigbee_fota_abort(void) { aborts++; }
void zigbee_fota_signal_handler(zb_bufid_t buf) { (void)buf; }
void zigbee_fota_zcl_cb(zb_bufid_t buf)
{ (void)buf; library_calls++; test_cb.cb_param.ota_value_param.upgrade_status = library_result; }

static void command(int status, int result)
{
  test_cb.device_cb_id = ZB_ZCL_OTA_UPGRADE_VALUE_CB_ID;
  test_cb.cb_param.ota_value_param.upgrade_status = status;
  if (status == ZB_ZCL_OTA_UPGRADE_STATUS_START) {
    test_cb.cb_param.ota_value_param.upgrade.start.manufacturer = CONFIG_ZIGBEE_FOTA_MANUFACTURER_ID;
    test_cb.cb_param.ota_value_param.upgrade.start.image_type = CONFIG_ZIGBEE_FOTA_IMAGE_TYPE;
  }
  library_result = result;
  transfer_result = result == ZB_ZCL_OTA_UPGRADE_STATUS_OK ? 0 : -EIO;
  registered_zcl(1);
}

int main(void)
{
#if defined(OTA_USB_PROFILE)
  app_ota_set_sleepy(false);
#endif
  ready = false;
  assert(app_ota_init() == -ENODEV);
  ready = true;
  watchdog_result = -EINVAL;
  assert(app_ota_init() == -EINVAL);
  watchdog_result = 0;
  assert(app_ota_init() == 0);
  assert(transfer_initializations == 3);

  /* A trial image must not even enter the library's DFU processing path. */
  confirmed = false;
  command(ZB_ZCL_OTA_UPGRADE_STATUS_START, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
  assert(test_cb.status == RET_OK);
  assert(test_cb.cb_param.ota_value_param.upgrade_status == ZB_ZCL_OTA_UPGRADE_STATUS_ABORT);
  command(ZB_ZCL_OTA_UPGRADE_STATUS_RECEIVE, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
  assert(test_cb.cb_param.ota_value_param.upgrade_status == ZB_ZCL_OTA_UPGRADE_STATUS_ABORT);
  assert(library_calls == 0 && !downloading && !pending_download);
  confirmed = true;

  command(ZB_ZCL_OTA_UPGRADE_STATUS_START, ZB_ZCL_OTA_UPGRADE_STATUS_ABORT);
  assert(!downloading && POLL_IS(120000));
  command(ZB_ZCL_OTA_UPGRADE_STATUS_START, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
  assert(downloading && POLL_IS(100) && pending_download && download_delay == 19532);
  app_ota_set_long_poll(60000);
  assert(POLL_IS(100));
  command(ZB_ZCL_OTA_UPGRADE_STATUS_START, ZB_ZCL_OTA_UPGRADE_STATUS_BUSY);
  assert(downloading && pending_download);
  command(ZB_ZCL_OTA_UPGRADE_STATUS_RECEIVE, ZB_ZCL_OTA_UPGRADE_STATUS_ERROR);
  assert(!downloading && !pending_download && POLL_IS(60000));
  command(ZB_ZCL_OTA_UPGRADE_STATUS_START, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
  pending_download(0);
  assert(aborts == 1 && reboots == 1 && restores == 1 && POLL_IS(60000));

  command(ZB_ZCL_OTA_UPGRADE_STATUS_START, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
  struct zigbee_fota_evt event = { .id = ZIGBEE_FOTA_EVT_FINISHED };
  ota_event(&event);
  assert(reboots == 2 && restores == 2 && !pending_download);
  command(ZB_ZCL_OTA_UPGRADE_STATUS_START, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
  current_signal = ZB_ZDO_SIGNAL_LEAVE;
  current_status = -EIO;
  app_ota_signal(1);
  assert(downloading && pending_download && POLL_IS(100) && aborts == 1);
  current_status = RET_OK;
  app_ota_signal(1);
  assert(!downloading && aborts == 2 && reboots == 3);

  current_signal = ZB_ZDO_SIGNAL_SKIP_STARTUP;
  app_ota_signal(1);
  assert(pending_health);
  confirmed = false;
  app_ota_startup_ready(false);
  health_tick(0);
  assert(confirms == 0 && feeds == 0 && reboots == 4);
  app_ota_startup_ready(true);
  confirm_result = -EIO;
  health_tick(0);
  assert(confirms == 1 && feeds == 0 && reboots == 5);
  unsigned int calls_before_failed_trial = library_calls;
  command(ZB_ZCL_OTA_UPGRADE_STATUS_START, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
  assert(library_calls == calls_before_failed_trial && !downloading && !pending_download);
  confirm_result = 0;
  health_tick(0);
  assert(confirms == 2 && feeds == 1 && reboots == 5);
  confirmed = true;
  health_tick(0);
  assert(confirms == 2 && feeds == 2);

  /* After successful confirmation, downloads are permitted again. */
  unsigned int previous_calls = library_calls;
  command(ZB_ZCL_OTA_UPGRADE_STATUS_START, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
  assert(library_calls == previous_calls + 1 && downloading && pending_download);
  command(ZB_ZCL_OTA_UPGRADE_STATUS_CHECK, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
  assert(!downloading && waiting_for_install && !pending_download);
  assert(POLL_IS(60000) && TURBO_IS(30000));
  /* Indefinite deferral sends no APPLY. Passing ten minutes of health ticks
   * must keep the candidate available without rebooting or fast long polls. */
  for (int i = 0; i < 20; i++) { health_tick(0); }
  assert(reboots == 5 && waiting_for_install && !pending_download && POLL_IS(60000));
  command(ZB_ZCL_OTA_UPGRADE_STATUS_APPLY, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
  app_ota_set_long_poll(120000);
  for (int i = 0; i < 20; i++) { health_tick(0); }
  assert(reboots == 5 && waiting_for_install && !pending_download && POLL_IS(120000));
  download_timeout(0); /* A stale alarm must not expire a completed image. */
  assert(reboots == 5 && aborts == 2);
  command(ZB_ZCL_OTA_UPGRADE_STATUS_FINISH, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
  assert(installs == 1 && pending_install == install_ready);
  ota_event(&event);
  assert(reboots == 6 && !waiting_for_install && !pending_download);

  /* Validation failure still cleans up, and leaving while waiting aborts. */
  command(ZB_ZCL_OTA_UPGRADE_STATUS_START, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
  command(ZB_ZCL_OTA_UPGRADE_STATUS_CHECK, ZB_ZCL_OTA_UPGRADE_STATUS_ERROR);
  assert(!downloading && !waiting_for_install && !pending_download);
  command(ZB_ZCL_OTA_UPGRADE_STATUS_START, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
  command(ZB_ZCL_OTA_UPGRADE_STATUS_CHECK, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
  current_signal = ZB_ZDO_SIGNAL_LEAVE;
  app_ota_signal(1);
  assert(!waiting_for_install && aborts == 3 && !pending_download && reboots == 7);
  app_ota_set_long_poll(60000);
  schedule_result = -ENOMEM;
  command(ZB_ZCL_OTA_UPGRADE_STATUS_START, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
  assert(!downloading && !pending_download && POLL_IS(60000));
  assert(reboots == 8 && aborts == 4);

  schedule_result = 0;
  /* CHECK occurs in DOWNLOADED; a lost end response must recover, even though
   * the watchdog continues to be fed and the download itself is complete. */
  command(ZB_ZCL_OTA_UPGRADE_STATUS_START, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
  ota_status = ZB_ZCL_OTA_UPGRADE_IMAGE_STATUS_DOWNLOADED;
  command(ZB_ZCL_OTA_UPGRADE_STATUS_CHECK, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
  assert(pending_response && response_delay == 19532 && !pending_download);
  current_signal = ZB_ZDO_SIGNAL_LEAVE;
  current_status = -EIO;
  app_ota_signal(1);
  assert(waiting_for_install && pending_response && reboots == 8 && aborts == 4);
  current_status = RET_OK;
  pending_response(0);
  assert(reboots == 9 && aborts == 5 && !waiting_for_install && !pending_response);

  /* An explicit indefinite deferral has no APPLY callback. */
  command(ZB_ZCL_OTA_UPGRADE_STATUS_CHECK, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
  ota_status = ZB_ZCL_OTA_UPGRADE_IMAGE_STATUS_WAITING_UPGRADE;
  pending_response(0);
  assert(reboots == 9 && aborts == 5 && waiting_for_install);

  /* APPLY cancels the deadline; even a stale callback must honor countdown. */
  ota_status = ZB_ZCL_OTA_UPGRADE_IMAGE_STATUS_DOWNLOADED;
  command(ZB_ZCL_OTA_UPGRADE_STATUS_CHECK, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
  command(ZB_ZCL_OTA_UPGRADE_STATUS_APPLY, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
  ota_status = ZB_ZCL_OTA_UPGRADE_IMAGE_STATUS_COUNT_DOWN;
  assert(!pending_response);
  response_timeout(0);
  assert(reboots == 9 && waiting_for_install);

  /* Abort/error cleanup cancels the response alarm too. */
  command(ZB_ZCL_OTA_UPGRADE_STATUS_CHECK, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
  command(ZB_ZCL_OTA_UPGRADE_STATUS_ABORT, ZB_ZCL_OTA_UPGRADE_STATUS_ABORT);
  assert(!pending_response && !waiting_for_install);
  ota_status = ZB_ZCL_OTA_UPGRADE_IMAGE_STATUS_DOWNLOADED;
  response_timeout(0);
  assert(reboots == 9);

  schedule_result = -ENOMEM;
  command(ZB_ZCL_OTA_UPGRADE_STATUS_CHECK, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
  assert(reboots == 10 && aborts == 6 && !pending_response && !waiting_for_install);

  /* A successful idle leave needs no OTA reset. */
  schedule_result = 0;
  current_signal = ZB_ZDO_SIGNAL_LEAVE;
  current_status = RET_OK;
  app_ota_signal(1);
  assert(reboots == 10 && aborts == 6);

  /* Both indefinite and scheduled installs survive failed leaves, but a
   * successful leave resets the full SDK, including delayed FINISH callbacks. */
  const zb_uint8_t deferred_states[] = {
    ZB_ZCL_OTA_UPGRADE_IMAGE_STATUS_WAITING_UPGRADE,
    ZB_ZCL_OTA_UPGRADE_IMAGE_STATUS_COUNT_DOWN,
  };
  for (unsigned int i = 0; i < sizeof(deferred_states) / sizeof(deferred_states[0]); i++) {
    command(ZB_ZCL_OTA_UPGRADE_STATUS_CHECK, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
    ota_status = deferred_states[i];
    unsigned int before_reboots = reboots, before_aborts = aborts;
    current_status = -EIO;
    app_ota_signal(1);
    assert(waiting_for_install && pending_response);
    assert(reboots == before_reboots && aborts == before_aborts);
    current_status = RET_OK;
    app_ota_signal(1);
    assert(!waiting_for_install && !pending_response && !pending_download);
    assert(reboots == before_reboots + 1 && aborts == before_aborts + 1);
  }
  /* A scheduling failure must neither confirm a trial image nor keep feeding
   * its watchdog without a future health tick. Both entry paths restart.
   */
  unsigned int before_reboots = reboots, before_feeds = feeds, before_confirms = confirms;
  confirmed = false;
  schedule_result = -ENOMEM;
  health_tick(0);
  assert(reboots == before_reboots + 1 && feeds == before_feeds && confirms == before_confirms);
  current_signal = ZB_ZDO_SIGNAL_SKIP_STARTUP;
  current_status = RET_OK;
  app_ota_signal(1);
  assert(reboots == before_reboots + 2 && confirms == before_confirms);
  assert(restores == reboots);

  /* Resume advances ZBOSS's requested offset without calling the SDK writer. */
  confirmed = true;
  schedule_result = RET_OK;
  command(ZB_ZCL_OTA_UPGRADE_STATUS_START, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
  unsigned int before_calls = library_calls, before_discards = discards;
  received_offset = 8280;
  command(ZB_ZCL_OTA_UPGRADE_STATUS_RECEIVE, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
  assert(library_calls == before_calls && discards == before_discards);
  pending_download(0);
  assert(discards == before_discards); /* Timeout preserves the checkpoint. */
  command(ZB_ZCL_OTA_UPGRADE_STATUS_START, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
  command(ZB_ZCL_OTA_UPGRADE_STATUS_ABORT, ZB_ZCL_OTA_UPGRADE_STATUS_ABORT);
  assert(discards == before_discards); /* Server abort does too. */
  current_signal = ZB_ZDO_SIGNAL_LEAVE;
  current_status = -EIO;
  app_ota_signal(1);
  assert(discards == before_discards);
  current_status = RET_OK;
  app_ota_signal(1);
  assert(discards == before_discards + 1); /* Idle leave clears retained progress. */
  command(ZB_ZCL_OTA_UPGRADE_STATUS_RECEIVE, ZB_ZCL_OTA_UPGRADE_STATUS_ERROR);
  assert(discards == before_discards + 2);

  /* Never reboot or queue installation if marking the image fails. */
  before_reboots = reboots;
  pending_install = NULL;
  install_result = -EIO;
  command(ZB_ZCL_OTA_UPGRADE_STATUS_FINISH, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
  assert(test_cb.cb_param.ota_value_param.upgrade_status == ZB_ZCL_OTA_UPGRADE_STATUS_ERROR);
  assert(!pending_install && reboots == before_reboots);
  install_result = 0;
  schedule_result = -ENOMEM;
  command(ZB_ZCL_OTA_UPGRADE_STATUS_FINISH, ZB_ZCL_OTA_UPGRADE_STATUS_OK);
  assert(reboots == before_reboots + 1); /* A marked image still has a bounded reboot. */
  return 0;
}
