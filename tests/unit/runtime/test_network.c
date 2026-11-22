#include <assert.h>
#include <string.h>
#include "../../../src/app_network.c"

struct pending_alarm { alarm_cb callback; zb_uint8_t param; unsigned int delay; };
static struct pending_alarm alarms[3];
static alarm_cb schedule_failure, cancel_failure;
static unsigned int commission_calls, last_mode, reboots, restores, sleep_calls, poll_calls;
static uint32_t poll_ms;
static bool joined, commission_ok;
static uint8_t reboot_reason;
static zb_zdo_app_signal_type_t raw_signal;
static zb_ret_t raw_status;
static zb_zdo_app_signal_hdr_t raw_header;
static unsigned int receiver_calls;
static bool rx_on_when_idle;
static int receiver_state;

static void sensor_callback(zb_uint8_t unused) { (void)unused; }
static void other_alarm(zb_uint8_t unused) { (void)unused; }

int alarm_schedule(alarm_cb cb, zb_uint8_t param, unsigned int delay)
{
	if (cb == schedule_failure) { return -ENOMEM; }
	for (size_t i = 0; i < 3; ++i) {
		if (!alarms[i].callback) {
			alarms[i] = (struct pending_alarm){cb, param, delay};
			return RET_OK;
		}
	}
	return -ENOMEM;
}
int alarm_cancel(alarm_cb cb, zb_uint8_t param)
{
	assert(param == ZB_ALARM_ALL_CB);
	if (cb == cancel_failure) { return -EBUSY; }
	int result = RET_NOT_FOUND;
	for (size_t i = 0; i < 3; ++i) {
		if (alarms[i].callback == cb) {
			alarms[i].callback = NULL;
			result = RET_OK;
		}
	}
	return result;
}
static unsigned int pending_count(alarm_cb cb)
{
	unsigned int result = 0;
	for (size_t i = 0; i < 3; ++i) { result += alarms[i].callback == cb; }
	return result;
}
static unsigned int pending_delay(alarm_cb cb)
{
	assert(pending_count(cb) == 1);
	for (size_t i = 0; i < 3; ++i) {
		if (alarms[i].callback == cb) { return alarms[i].delay; }
	}
	assert(false);
	return 0;
}
static void fire(alarm_cb cb)
{
	assert(pending_count(cb) == 1);
	for (size_t i = 0; i < 3; ++i) {
		if (alarms[i].callback == cb) {
			zb_uint8_t param = alarms[i].param;
			alarms[i].callback = NULL;
			cb(param);
			return;
		}
	}
}
zb_bool_t test_joined(void) { return joined; }
zb_bool_t bdb_start_top_level_commissioning(zb_uint8_t mode)
{
	commission_calls++;
	last_mode = mode;
	return commission_ok;
}
void zb_sleep_now(void) { sleep_calls++; }
zb_zdo_app_signal_type_t zb_get_app_signal(zb_bufid_t bufid, void *header)
{
	assert(bufid == 7);
	*(zb_zdo_app_signal_hdr_t **)header = &raw_header;
	return raw_signal;
}
int signal_status(zb_bufid_t bufid)
{
	assert(bufid == 7);
	return raw_status;
}
zb_bool_t zb_get_rx_on_when_idle(void) { return rx_on_when_idle; }
void zb_macll_trans_set_rx_on_off(int rx_on)
{
	receiver_calls++;
	receiver_state = rx_on;
}
void app_ota_set_long_poll(uint32_t ms) { poll_ms = ms; poll_calls++; }
void power_up_unused_ram(void) { restores++; }
void app_reset_info_set_reason(uint8_t reason) { reboot_reason = reason; }
void sys_reboot(int kind)
{
	assert(kind == SYS_REBOOT_COLD && restores == reboots + 1);
	reboots++;
}
static void reset(void)
{
	app_state = (struct app_zigbee_state){0};
	rejoin_state = (struct app_rejoin_state){0};
	memset(alarms, 0, sizeof(alarms));
	schedule_failure = cancel_failure = NULL;
	commission_calls = last_mode = reboots = restores = sleep_calls = poll_calls = 0;
	reboot_reason = APP_REBOOT_UNKNOWN;
	joined = false;
	commission_ok = true;
	receiver_calls = 0;
	receiver_state = -1;
	rx_on_when_idle = false;
}
static void dispatch(zb_zdo_app_signal_type_t event, bool ok, bool sleepy)
{
	raw_signal = event;
	raw_status = ok ? RET_OK : -EIO;
	raw_header.indication.nlme_status.status = ZB_NWK_COMMAND_STATUS_PARENT_LINK_FAILURE;
	app_network_handle_signal(7, sensor_callback, sleepy);
}
static void signal(zb_zdo_app_signal_type_t event, bool ok)
{
	dispatch(event, ok, true);
}
static void start(void)
{
	signal(ZB_ZDO_SIGNAL_SKIP_STARTUP, true);
	assert(commission_calls == 1 && last_mode == ZB_BDB_INITIALIZATION);
	assert(pending_delay(sensor_callback) == 66);
	signal(ZB_BDB_SIGNAL_DEVICE_FIRST_START, true);
	assert(pending_delay(start_network_steering) == 66 && rejoin_state.retry_pending);
}

static void test_rejoin_lifecycle(void)
{
	reset();
	start();
	for (int i = 0; i < 10; ++i) { signal(ZB_NLME_STATUS_INDICATION, true); }
	assert(pending_delay(start_network_steering) == 66 && rejoin_state.attempt_count == 1);
	fire(start_network_steering);
	assert(commission_calls == 2 && last_mode == ZB_BDB_NETWORK_STEERING);
	assert(!rejoin_state.retry_pending);
	signal(ZB_BDB_SIGNAL_STEERING, false);
	assert(pending_delay(start_network_steering) == 131);
	joined = true;
	signal(ZB_BDB_SIGNAL_STEERING, true);
	assert(pending_count(start_network_steering) == 0 && !rejoin_state.procedure_started);
	assert(pending_delay(sensor_callback) == 66);
	assert(poll_calls == 1 && poll_ms == APP_ZIGBEE_LONG_POLL_INTERVAL_MS);
	/* A repeated join signal replaces, rather than duplicates, sampling. */
	signal(ZB_BDB_SIGNAL_DEVICE_REBOOT, true);
	assert(pending_count(sensor_callback) == 1 && reboots == 0);
}

static void test_rejected_commissioning_retries(void)
{
	reset();
	start();
	commission_ok = false;
	fire(start_network_steering);
	assert(pending_delay(start_network_steering) == 131 && rejoin_state.retry_pending);
	assert(reboots == 0);
}

static void test_join_reuses_the_retry_alarm_slot(void)
{
	reset();
	start();
	assert(alarm_schedule(other_alarm, 0, 100) == RET_OK);
	assert(alarm_schedule(other_alarm, 1, 200) == -ENOMEM);
	joined = true;
	signal(ZB_BDB_SIGNAL_STEERING, true);
	assert(reboots == 0 && pending_count(start_network_steering) == 0);
	assert(pending_count(sensor_callback) == 1 && pending_count(other_alarm) == 1);
}

static void test_alarm_failures_restart_instead_of_stalling(void)
{
	reset();
	signal(ZB_ZDO_SIGNAL_SKIP_STARTUP, true);
	schedule_failure = start_network_steering;
	signal(ZB_BDB_SIGNAL_DEVICE_FIRST_START, true);
	assert(reboots == 1 && !rejoin_state.retry_pending);
	assert(reboot_reason == APP_REBOOT_REJOIN_ALARM);

	reset();
	schedule_failure = sensor_callback;
	signal(ZB_ZDO_SIGNAL_SKIP_STARTUP, true);
	assert(reboots == 1 && reboot_reason == APP_REBOOT_SENSOR_ALARM);

	reset();
	start();
	schedule_failure = sensor_callback;
	joined = true;
	signal(ZB_BDB_SIGNAL_STEERING, true);
	assert(reboots == 1 && pending_count(sensor_callback) == 0);
	assert(reboot_reason == APP_REBOOT_SENSOR_ALARM);

	reset();
	start();
	fire(start_network_steering);
	schedule_failure = start_network_steering;
	signal(ZB_BDB_SIGNAL_STEERING, false);
	assert(reboots == 1 && !rejoin_state.retry_pending);

	reset();
	cancel_failure = sensor_callback;
	joined = true;
	signal(ZB_BDB_SIGNAL_DEVICE_REBOOT, true);
	assert(reboots == 1 && pending_count(sensor_callback) == 0);
}

static void test_startup_failures_restart(void)
{
	reset();
	signal(ZB_ZDO_SIGNAL_SKIP_STARTUP, false);
	assert(reboots == 1 && commission_calls == 0);
	assert(reboot_reason == APP_REBOOT_ZIGBEE_STARTUP);
	reset();
	signal(ZB_BDB_SIGNAL_DEVICE_FIRST_START, false);
	assert(reboots == 1 && commission_calls == 0);
	assert(reboot_reason == APP_REBOOT_ZIGBEE_STARTUP);
	reset();
	commission_ok = false;
	signal(ZB_ZDO_SIGNAL_SKIP_STARTUP, true);
	assert(reboots == 1 && commission_calls == 1);
	assert(reboot_reason == APP_REBOOT_ZIGBEE_STARTUP);
}

static void test_failed_cancel_is_guarded_when_callback_fires(void)
{
	reset();
	start();
	cancel_failure = start_network_steering;
	joined = true;
	signal(ZB_BDB_SIGNAL_STEERING, true);
	assert(rejoin_state.stop_requested && reboots == 0);
	assert(pending_count(start_network_steering) == 1);
	fire(start_network_steering);
	assert(commission_calls == 1 && !rejoin_state.procedure_started);
	assert(!rejoin_state.stop_requested);
}

static void test_sampling_survives_failed_reboot_join(void)
{
	/* After a reset the stack may report a failed reboot join while it still
	 * considers itself joined. No retry and no join signal follow, yet the
	 * measurement alarm started with the stack must keep running.
	 */
	reset();
	signal(ZB_ZDO_SIGNAL_SKIP_STARTUP, true);
	joined = true;
	signal(ZB_BDB_SIGNAL_DEVICE_REBOOT, false);
	assert(pending_count(start_network_steering) == 0);
	assert(pending_delay(sensor_callback) == 66 && reboots == 0);
}

static void test_sleep_respects_power_profile(void)
{
	reset();
	dispatch(ZB_COMMON_SIGNAL_CAN_SLEEP, true, false);
	assert(sleep_calls == 0);
	dispatch(ZB_COMMON_SIGNAL_CAN_SLEEP, true, true);
	assert(sleep_calls == 1);
	assert(receiver_calls == 0);
}

static void test_trust_center_rejoin(void)
{
	reset();
	start();
	fire(start_network_steering);
	signal(ZB_BDB_SIGNAL_TC_REJOIN_DONE, false);
	assert(reboots == 0 && rejoin_state.retry_pending);
	assert(pending_delay(start_network_steering) == 131);

	joined = true;
	signal(ZB_BDB_SIGNAL_TC_REJOIN_DONE, true);
	assert(!rejoin_state.retry_pending && !rejoin_state.procedure_started);
	assert(pending_count(start_network_steering) == 0);
	assert(pending_delay(sensor_callback) == 66);
	assert(poll_calls == 1 && poll_ms == APP_ZIGBEE_LONG_POLL_INTERVAL_MS);

	/* A boot-time rejoin can emit both signals. Keep one measurement alarm. */
	signal(ZB_BDB_SIGNAL_DEVICE_REBOOT, true);
	signal(ZB_BDB_SIGNAL_TC_REJOIN_DONE, true);
	assert(reboots == 0 && pending_count(sensor_callback) == 1);
}

static void test_commissioning_restores_configured_receiver(void)
{
	const zb_zdo_app_signal_type_t events[] = {
		ZB_BDB_SIGNAL_DEVICE_REBOOT, ZB_BDB_SIGNAL_TC_REJOIN_DONE, ZB_BDB_SIGNAL_STEERING,
	};
	for (size_t i = 0; i < sizeof(events) / sizeof(events[0]); ++i)
	{
		for (int rx_on = 0; rx_on <= 1; ++rx_on)
		{
			reset();
			rx_on_when_idle = rx_on;
			dispatch(events[i], false, !rx_on);
			assert(receiver_calls == 1 && receiver_state == rx_on);
			dispatch(events[i], true, !rx_on);
			assert(receiver_calls == 2 && receiver_state == rx_on);
		}
	}

	dispatch(ZB_ZDO_SIGNAL_LEAVE, true, false);
	assert(receiver_calls == 2);
}

int main(void)
{
	test_rejoin_lifecycle();
	test_rejected_commissioning_retries();
	test_join_reuses_the_retry_alarm_slot();
	test_alarm_failures_restart_instead_of_stalling();
	test_startup_failures_restart();
	test_failed_cancel_is_guarded_when_callback_fires();
	test_sampling_survives_failed_reboot_join();
	test_sleep_respects_power_profile();
	test_trust_center_rejoin();
	test_commissioning_restores_configured_receiver();
	return 0;
}
