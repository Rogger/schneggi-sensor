#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "zigbee_signal_logic.h"

static void assert_no_connected_side_effects(const struct app_zigbee_actions *actions)
{
	assert(actions->schedule_sensor_loop == false);
	assert(actions->set_long_poll_interval == false);
	assert(actions->stop_rejoin == false);
}

static void test_device_first_start_schedules_rejoin(void)
{
	struct app_zigbee_state state = {
		.stack_initialised = true,
	};
	struct app_zigbee_actions actions;

	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_DEVICE_FIRST_START, true, false, &actions);

	assert(actions.commissioning_mode == APP_COMMISSIONING_NONE);
	assert(actions.start_rejoin == true);
	assert_no_connected_side_effects(&actions);
}

static void test_device_first_start_failure_requests_restart(void)
{
	struct app_zigbee_state state = {
		.stack_initialised = true,
	};
	struct app_zigbee_actions actions;

	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_DEVICE_FIRST_START, false, false, &actions);
	assert(actions.restart);

	assert(actions.commissioning_mode == APP_COMMISSIONING_NONE);
	assert(actions.start_rejoin == false);
	assert_no_connected_side_effects(&actions);
}

static void test_device_reboot_success_schedules_measurements(void)
{
	struct app_zigbee_state state = {
		.stack_initialised = true,
	};
	struct app_zigbee_actions actions;

	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_DEVICE_REBOOT, true, false, &actions);

	assert(actions.commissioning_mode == APP_COMMISSIONING_NONE);
	assert(actions.schedule_sensor_loop == true);
	assert(actions.schedule_sensor_loop_delay_ms == 1000U);
	assert(actions.set_long_poll_interval == true);
	assert(actions.long_poll_interval_ms == APP_ZIGBEE_LONG_POLL_INTERVAL_MS);
	assert(actions.stop_rejoin == true);
}

static void test_device_reboot_failure_restarts_commissioning(void)
{
	struct app_zigbee_state state = {
		.stack_initialised = true,
	};
	struct app_zigbee_actions actions;

	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_DEVICE_REBOOT, false, false, &actions);

	assert(actions.commissioning_mode == APP_COMMISSIONING_NONE);
	assert(actions.start_rejoin == true);
	assert_no_connected_side_effects(&actions);
}

static void test_steering_success_marks_connected_and_schedules_work(void)
{
	struct app_zigbee_state state = {
		.stack_initialised = true,
	};
	struct app_zigbee_actions actions;

	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_STEERING, true, false, &actions);

	assert(actions.commissioning_mode == APP_COMMISSIONING_NONE);
	assert(actions.schedule_sensor_loop == true);
	assert(actions.schedule_sensor_loop_delay_ms == 1000U);
	assert(actions.set_long_poll_interval == true);
	assert(actions.long_poll_interval_ms == APP_ZIGBEE_LONG_POLL_INTERVAL_MS);
	assert(actions.stop_rejoin == true);
}

static void test_steering_failure_retries(void)
{
	struct app_zigbee_state state = {
		.stack_initialised = true,
	};
	struct app_zigbee_actions actions;

	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_STEERING, false, false, &actions);

	assert(actions.commissioning_mode == APP_COMMISSIONING_NONE);
	assert(actions.start_rejoin == true);
	assert_no_connected_side_effects(&actions);
}

static void test_startup_initializes_before_steering(void)
{
	struct app_zigbee_state state = {
		.stack_initialised = false,
	};
	struct app_zigbee_actions actions;

	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_SKIP_STARTUP, true, false, &actions);
	assert(state.stack_initialised == true);
	assert(actions.commissioning_mode == APP_COMMISSIONING_INITIALIZATION);
	assert(actions.start_rejoin == false);
	assert_no_connected_side_effects(&actions);

	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_DEVICE_FIRST_START, true, false, &actions);
	assert(actions.commissioning_mode == APP_COMMISSIONING_NONE);
	assert(actions.start_rejoin == true);
	assert_no_connected_side_effects(&actions);

	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_STEERING, true, false, &actions);
	assert(actions.schedule_sensor_loop == true);
	assert(actions.set_long_poll_interval == true);
	assert(actions.stop_rejoin == true);

	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_STEERING, false, false, &actions);
	assert(actions.commissioning_mode == APP_COMMISSIONING_NONE);
	assert(actions.start_rejoin == true);
	assert_no_connected_side_effects(&actions);
}

static void test_startup_failure_does_not_mark_stack_initialised(void)
{
	struct app_zigbee_state state = {
		.stack_initialised = false,
	};
	struct app_zigbee_actions actions;

	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_SKIP_STARTUP, false, false, &actions);
	assert(actions.restart);

	assert(state.stack_initialised == false);
	assert(actions.commissioning_mode == APP_COMMISSIONING_NONE);
	assert(actions.start_rejoin == false);
	assert_no_connected_side_effects(&actions);
}

static void test_leave_starts_rejoin_without_reset(void)
{
	struct app_zigbee_state state = {
		.stack_initialised = true,
	};
	struct app_zigbee_actions actions;

	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_LEAVE, true, false, &actions);

	assert(actions.start_rejoin == true);
	assert(actions.commissioning_mode == APP_COMMISSIONING_NONE);
	assert_no_connected_side_effects(&actions);
}

static void test_parent_link_failure_starts_rejoin(void)
{
	struct app_zigbee_state state = {
		.stack_initialised = true,
	};
	struct app_zigbee_actions actions;

	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_NLME_STATUS_INDICATION, true, true, &actions);

	assert(actions.start_rejoin == true);
	assert(actions.stop_rejoin == false);
	assert(actions.request_sleep == false);
}

static void test_sleep_and_unhandled_signals_clear_previous_actions(void)
{
	struct app_zigbee_state state = { .stack_initialised = true };
	struct app_zigbee_actions actions;
	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_STEERING, true, false, &actions);
	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_SKIP_STARTUP, false, false, &actions);
	assert(actions.restart);
	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_CAN_SLEEP, true, false, &actions);
	assert(!actions.restart);
	assert(actions.request_sleep);
	assert_no_connected_side_effects(&actions);
	assert(!actions.start_rejoin);
	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_OTHER, true, false, &actions);
	assert(!actions.request_sleep);
	assert_no_connected_side_effects(&actions);
}

static void test_failed_leave_and_unrelated_link_status_preserve_connection(void)
{
	struct app_zigbee_state state = { .stack_initialised = true };
	struct app_zigbee_actions actions;
	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_LEAVE, false, false, &actions);
	assert(!actions.start_rejoin);
	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_NLME_STATUS_INDICATION, true, false, &actions);
	assert(!actions.start_rejoin);
	state.stack_initialised = false;
	app_zigbee_handle_signal(&state, APP_ZIGBEE_SIGNAL_NLME_STATUS_INDICATION, true, true, &actions);
	assert(!actions.start_rejoin);
}

int main(void)
{
	test_sleep_and_unhandled_signals_clear_previous_actions();
	test_failed_leave_and_unrelated_link_status_preserve_connection();
	test_device_first_start_schedules_rejoin();
	test_device_first_start_failure_requests_restart();
	test_device_reboot_success_schedules_measurements();
	test_device_reboot_failure_restarts_commissioning();
	test_steering_success_marks_connected_and_schedules_work();
	test_steering_failure_retries();
	test_startup_initializes_before_steering();
	test_startup_failure_does_not_mark_stack_initialised();
	test_leave_starts_rejoin_without_reset();
	test_parent_link_failure_starts_rejoin();

	printf("zigbee_signal_logic unit tests passed\n");
	return 0;
}
