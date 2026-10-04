#include "zigbee_signal_logic.h"

void app_zigbee_actions_reset(struct app_zigbee_actions *actions)
{
	actions->commissioning_mode = APP_COMMISSIONING_NONE;
	actions->schedule_sensor_loop = false;
	actions->schedule_sensor_loop_delay_ms = 0U;
	actions->set_long_poll_interval = false;
	actions->long_poll_interval_ms = 0U;
	actions->start_rejoin = false;
	actions->stop_rejoin = false;
	actions->request_sleep = false;
	actions->restart = false;
}

void app_zigbee_handle_signal(struct app_zigbee_state *state,
			      enum app_zigbee_signal signal,
			      bool status_ok,
			      bool parent_link_failure,
			      struct app_zigbee_actions *actions)
{
	app_zigbee_actions_reset(actions);

	switch (signal)
	{
	case APP_ZIGBEE_SIGNAL_SKIP_STARTUP:
		if (status_ok)
		{
			state->stack_initialised = true;
			actions->commissioning_mode = APP_COMMISSIONING_INITIALIZATION;
		}
		else
		{
			actions->restart = true;
		}
		break;

	case APP_ZIGBEE_SIGNAL_DEVICE_FIRST_START:
		if (status_ok)
		{
			actions->start_rejoin = true;
		}
		else
		{
			actions->restart = true;
		}
		break;

	case APP_ZIGBEE_SIGNAL_DEVICE_REBOOT:
	case APP_ZIGBEE_SIGNAL_STEERING:
		if (status_ok)
		{
			actions->schedule_sensor_loop = true;
			actions->schedule_sensor_loop_delay_ms = 1000U;
			actions->set_long_poll_interval = true;
			actions->long_poll_interval_ms = APP_ZIGBEE_LONG_POLL_INTERVAL_MS;
			actions->stop_rejoin = true;
		}
		else
		{
			actions->start_rejoin = true;
		}
		break;

	case APP_ZIGBEE_SIGNAL_LEAVE:
		if (status_ok)
		{
			actions->start_rejoin = true;
		}
		break;

	case APP_ZIGBEE_SIGNAL_CAN_SLEEP:
		actions->request_sleep = true;
		break;

	case APP_ZIGBEE_SIGNAL_NLME_STATUS_INDICATION:
		if (parent_link_failure && state->stack_initialised)
		{
			actions->start_rejoin = true;
		}
		break;

	case APP_ZIGBEE_SIGNAL_OTHER:
	default:
		break;
	}
}
