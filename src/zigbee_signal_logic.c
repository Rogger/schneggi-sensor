#include "zigbee_signal_logic.h"

void app_zigbee_actions_reset(struct app_zigbee_actions *actions)
{
	actions->commissioning_mode = APP_COMMISSIONING_NONE;
	actions->schedule_sensor_loop_cancel = false;
	actions->schedule_sensor_loop = false;
	actions->schedule_sensor_loop_delay_ms = 0U;
	actions->set_long_poll_interval = false;
	actions->long_poll_interval_ms = 0U;
	actions->request_sleep = false;
	actions->request_reset = false;
}

void app_zigbee_handle_signal(struct app_zigbee_state *state,
			      enum app_zigbee_signal signal,
			      bool status_ok,
			      bool leave_type_rejoin,
			      bool parent_link_failure,
			      struct app_zigbee_actions *actions)
{
	app_zigbee_actions_reset(actions);

	switch (signal)
	{
	case APP_ZIGBEE_SIGNAL_SKIP_STARTUP:
		state->stack_initialised = true;
		actions->commissioning_mode = APP_COMMISSIONING_INITIALIZATION;
		break;

	case APP_ZIGBEE_SIGNAL_DEVICE_FIRST_START:
		state->joining_signal_received = false;
		if (status_ok)
		{
			actions->commissioning_mode = APP_COMMISSIONING_NETWORK_STEERING;
		}
		break;

	case APP_ZIGBEE_SIGNAL_DEVICE_REBOOT:
		if (status_ok)
		{
			state->joining_signal_received = true;
			actions->schedule_sensor_loop_cancel = true;
			actions->schedule_sensor_loop = true;
			actions->schedule_sensor_loop_delay_ms = 1000U;
			actions->set_long_poll_interval = true;
			actions->long_poll_interval_ms = APP_ZIGBEE_LONG_POLL_INTERVAL_MS;
		}
		else
		{
			state->joining_signal_received = false;
			actions->commissioning_mode = APP_COMMISSIONING_NETWORK_STEERING;
		}
		break;

	case APP_ZIGBEE_SIGNAL_STEERING:
		if (status_ok)
		{
			state->joining_signal_received = true;
			actions->schedule_sensor_loop_cancel = true;
			actions->schedule_sensor_loop = true;
			actions->schedule_sensor_loop_delay_ms = 1000U;
			actions->set_long_poll_interval = true;
			actions->long_poll_interval_ms = APP_ZIGBEE_LONG_POLL_INTERVAL_MS;
		}
		else
		{
			state->joining_signal_received = false;
			actions->commissioning_mode = APP_COMMISSIONING_NETWORK_STEERING;
		}
		break;

	case APP_ZIGBEE_SIGNAL_LEAVE:
		if (status_ok)
		{
			if (leave_type_rejoin)
			{
				state->joining_signal_received = false;
			}

			actions->commissioning_mode = APP_COMMISSIONING_NETWORK_STEERING;
		}
		break;

	case APP_ZIGBEE_SIGNAL_CAN_SLEEP:
		actions->request_sleep = true;
		break;

	case APP_ZIGBEE_SIGNAL_NLME_STATUS_INDICATION:
		if (parent_link_failure &&
		    state->stack_initialised &&
		    !state->joining_signal_received)
		{
			actions->request_reset = true;
		}
		break;

	case APP_ZIGBEE_SIGNAL_OTHER:
	default:
		break;
	}
}
