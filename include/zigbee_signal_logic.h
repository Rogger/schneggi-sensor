#ifndef ZIGBEE_SIGNAL_LOGIC_H_
#define ZIGBEE_SIGNAL_LOGIC_H_

#include <stdbool.h>
#include <stdint.h>

enum app_zigbee_signal {
	APP_ZIGBEE_SIGNAL_SKIP_STARTUP,
	APP_ZIGBEE_SIGNAL_DEVICE_FIRST_START,
	APP_ZIGBEE_SIGNAL_DEVICE_REBOOT,
	APP_ZIGBEE_SIGNAL_STEERING,
	APP_ZIGBEE_SIGNAL_LEAVE,
	APP_ZIGBEE_SIGNAL_CAN_SLEEP,
	APP_ZIGBEE_SIGNAL_NLME_STATUS_INDICATION,
	APP_ZIGBEE_SIGNAL_OTHER,
};

enum app_commissioning_mode {
	APP_COMMISSIONING_NONE,
	APP_COMMISSIONING_INITIALIZATION,
	APP_COMMISSIONING_NETWORK_STEERING,
};

struct app_zigbee_state {
	bool joining_signal_received;
	bool stack_initialised;
};

struct app_zigbee_actions {
	enum app_commissioning_mode commissioning_mode;
	bool schedule_sensor_loop_cancel;
	bool schedule_sensor_loop;
	uint32_t schedule_sensor_loop_delay_ms;
	bool set_long_poll_interval;
	uint32_t long_poll_interval_ms;
	bool request_sleep;
	bool request_reset;
};

void app_zigbee_actions_reset(struct app_zigbee_actions *actions);

void app_zigbee_handle_signal(struct app_zigbee_state *state,
			      enum app_zigbee_signal signal,
			      bool status_ok,
			      bool leave_type_rejoin,
			      bool parent_link_failure,
			      struct app_zigbee_actions *actions);

#endif /* ZIGBEE_SIGNAL_LOGIC_H_ */
