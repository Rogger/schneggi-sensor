#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "zigbee_signal_logic.h"

static void assert_no_connected_side_effects(const struct app_zigbee_actions *actions)
{
	assert(actions->schedule_sensor_loop_cancel == false);
	assert(actions->schedule_sensor_loop == false);
	assert(actions->set_long_poll_interval == false);
}

static void test_device_first_start_does_not_mark_joined(void)
{
	struct app_zigbee_state state = {
		.joining_signal_received = true,
		.stack_initialised = true,
	};
	struct app_zigbee_actions actions;

	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_DEVICE_FIRST_START, true, false, false, &actions);

	assert(state.joining_signal_received == false);
	assert(actions.commissioning_mode == APP_COMMISSIONING_NETWORK_STEERING);
	assert_no_connected_side_effects(&actions);
}

static void test_device_first_start_failure_does_not_start_commissioning(void)
{
	struct app_zigbee_state state = {
		.joining_signal_received = true,
		.stack_initialised = true,
	};
	struct app_zigbee_actions actions;

	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_DEVICE_FIRST_START, false, false, false, &actions);

	assert(state.joining_signal_received == false);
	assert(actions.commissioning_mode == APP_COMMISSIONING_NONE);
	assert_no_connected_side_effects(&actions);
}

static void test_device_reboot_success_marks_joined_and_does_not_restart_commissioning(void)
{
	struct app_zigbee_state state = {
		.joining_signal_received = false,
		.stack_initialised = true,
	};
	struct app_zigbee_actions actions;

	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_DEVICE_REBOOT, true, false, false, &actions);

	assert(state.joining_signal_received == true);
	assert(actions.commissioning_mode == APP_COMMISSIONING_NONE);
	assert(actions.schedule_sensor_loop_cancel == true);
	assert(actions.schedule_sensor_loop == true);
	assert(actions.schedule_sensor_loop_delay_ms == 1000U);
	assert(actions.set_long_poll_interval == true);
	assert(actions.long_poll_interval_ms == APP_ZIGBEE_LONG_POLL_INTERVAL_MS);
}

static void test_device_reboot_failure_restarts_commissioning(void)
{
	struct app_zigbee_state state = {
		.joining_signal_received = true,
		.stack_initialised = true,
	};
	struct app_zigbee_actions actions;

	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_DEVICE_REBOOT, false, false, false, &actions);

	assert(state.joining_signal_received == false);
	assert(actions.commissioning_mode == APP_COMMISSIONING_NETWORK_STEERING);
	assert_no_connected_side_effects(&actions);
}

static void test_steering_success_marks_connected_and_schedules_work(void)
{
	struct app_zigbee_state state = {
		.joining_signal_received = false,
		.stack_initialised = true,
	};
	struct app_zigbee_actions actions;

	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_STEERING, true, false, false, &actions);

	assert(state.joining_signal_received == true);
	assert(actions.commissioning_mode == APP_COMMISSIONING_NONE);
	assert(actions.schedule_sensor_loop_cancel == true);
	assert(actions.schedule_sensor_loop == true);
	assert(actions.schedule_sensor_loop_delay_ms == 1000U);
	assert(actions.set_long_poll_interval == true);
	assert(actions.long_poll_interval_ms == APP_ZIGBEE_LONG_POLL_INTERVAL_MS);
}

static void test_steering_failure_clears_connected_and_retries(void)
{
	struct app_zigbee_state state = {
		.joining_signal_received = true,
		.stack_initialised = true,
	};
	struct app_zigbee_actions actions;

	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_STEERING, false, false, false, &actions);

	assert(state.joining_signal_received == false);
	assert(actions.commissioning_mode == APP_COMMISSIONING_NETWORK_STEERING);
	assert_no_connected_side_effects(&actions);
}

static void test_startup_to_first_start_to_steering_only_sets_joined_on_steering(void)
{
	struct app_zigbee_state state = {
		.joining_signal_received = false,
		.stack_initialised = false,
	};
	struct app_zigbee_actions actions;

	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_SKIP_STARTUP, true, false, false, &actions);
	assert(state.stack_initialised == true);
	assert(state.joining_signal_received == false);
	assert(actions.commissioning_mode == APP_COMMISSIONING_INITIALIZATION);
	assert_no_connected_side_effects(&actions);

	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_DEVICE_FIRST_START, true, false, false, &actions);
	assert(state.joining_signal_received == false);
	assert(actions.commissioning_mode == APP_COMMISSIONING_NETWORK_STEERING);
	assert_no_connected_side_effects(&actions);

	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_STEERING, true, false, false, &actions);
	assert(state.joining_signal_received == true);
	assert(actions.schedule_sensor_loop == true);
	assert(actions.set_long_poll_interval == true);

	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_STEERING, false, false, false, &actions);
	assert(state.joining_signal_received == false);
	assert(actions.commissioning_mode == APP_COMMISSIONING_NETWORK_STEERING);
	assert_no_connected_side_effects(&actions);
}

int main(void)
{
	test_device_first_start_does_not_mark_joined();
	test_device_first_start_failure_does_not_start_commissioning();
	test_device_reboot_success_marks_joined_and_does_not_restart_commissioning();
	test_device_reboot_failure_restarts_commissioning();
	test_steering_success_marks_connected_and_schedules_work();
	test_steering_failure_clears_connected_and_retries();
	test_startup_to_first_start_to_steering_only_sets_joined_on_steering();

	printf("zigbee_signal_logic unit tests passed\n");
	return 0;
}
