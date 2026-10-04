#include "app_network.h"
#include "app_ota.h"
#include "app_reboot.h"
#include "app_scheduler.h"
#include "rejoin_logic.h"

#include <inttypes.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app_network, LOG_LEVEL_INF);

/* The signal handler and all callbacks run on the Zigbee thread. */
static struct app_zigbee_state app_state;
static struct app_rejoin_state rejoin_state;

static void start_network_steering(zb_uint8_t param);
static void execute_rejoin_outcome(const struct app_rejoin_outcome *outcome)
{
	if (outcome->log_started)
	{
		LOG_INF("Started network rejoin procedure");
	}

	if (outcome->log_stopped)
	{
		LOG_INF("Network rejoin procedure stopped");
	}

	if (outcome->schedule_retry)
	{
		if (app_alarm_replace(start_network_steering, ZB_FALSE,
				      outcome->retry_delay_s * 1000U) != RET_OK)
		{
			LOG_ERR("Unable to schedule network rejoin retry; restarting");
			app_reboot();
			return;
		}

		app_rejoin_mark_retry_pending(&rejoin_state);
		LOG_INF("Scheduled network rejoin retry in %" PRIu32 " s", outcome->retry_delay_s);
	}

	if (outcome->stop_deferred)
	{
		LOG_WRN("Unable to cancel pending rejoin attempt immediately");
	}
}

static void start_network_steering(zb_uint8_t param)
{
	struct app_rejoin_outcome outcome;

	ZVUNUSED(param);

	if (!app_rejoin_begin_retry(&rejoin_state, app_state.stack_initialised, ZB_JOINED(), &outcome))
	{
		execute_rejoin_outcome(&outcome);
		return;
	}
	LOG_INF("Starting network steering retry");

	if (bdb_start_top_level_commissioning(ZB_BDB_NETWORK_STEERING) != ZB_TRUE)
	{
		LOG_WRN("Failed to start network steering, scheduling next retry");
		app_rejoin_process(&rejoin_state, app_state.stack_initialised, ZB_JOINED(), &outcome);
		execute_rejoin_outcome(&outcome);
	}
}

static void start_network_rejoin(void)
{
	struct app_rejoin_outcome outcome;

	app_rejoin_start(&rejoin_state, app_state.stack_initialised, ZB_JOINED(), &outcome);
	execute_rejoin_outcome(&outcome);
}

static void stop_network_rejoin(void)
{
	zb_ret_t ret = ZB_SCHEDULE_APP_ALARM_CANCEL(start_network_steering, ZB_ALARM_ALL_CB);
	struct app_rejoin_outcome outcome;

	app_rejoin_stop(&rejoin_state, ret == RET_OK || ret == RET_NOT_FOUND, &outcome);
	execute_rejoin_outcome(&outcome);
}

static void execute_signal_actions(const struct app_zigbee_actions *actions,
				   zb_callback_t sensor_loop, bool sleepy)
{
	zb_bool_t comm_status = ZB_TRUE;

	if (actions->commissioning_mode == APP_COMMISSIONING_INITIALIZATION)
	{
		comm_status = bdb_start_top_level_commissioning(ZB_BDB_INITIALIZATION);
	}

	if (actions->commissioning_mode != APP_COMMISSIONING_NONE &&
		comm_status != ZB_TRUE)
	{
		LOG_ERR("Unable to initialize commissioning; restarting");
		app_reboot();
		return;
	}

	if (actions->restart)
	{
		LOG_ERR("Zigbee startup failed; restarting");
		app_reboot();
		return;
	}

	/* Release an obsolete retry before allocating the post-join sensor alarm.
	 * Otherwise an almost-full scheduler could cause an avoidable restart.
	 */
	if (actions->stop_rejoin)
	{
		stop_network_rejoin();
	}

	if (actions->schedule_sensor_loop &&
	    app_alarm_replace(sensor_loop, 0, actions->schedule_sensor_loop_delay_ms) != RET_OK)
	{
		LOG_ERR("Unable to start sensor measurements; restarting");
		app_reboot();
		return;
	}

	if (actions->set_long_poll_interval)
	{
		/* long poll uses milliseconds; keepalive uses beacon intervals */
		app_ota_set_long_poll(actions->long_poll_interval_ms);
	}

	if (actions->start_rejoin)
	{
		start_network_rejoin();
	}

	if (actions->request_sleep && sleepy)
	{
		zb_sleep_now();
	}
}

void app_network_handle_signal(enum app_zigbee_signal signal, bool status_ok,
			       bool parent_link_failure, zb_callback_t sensor_loop, bool sleepy)
{
	struct app_zigbee_actions actions;
	app_zigbee_handle_signal(&app_state, signal, status_ok, parent_link_failure, &actions);
	execute_signal_actions(&actions, sensor_loop, sleepy);
}
