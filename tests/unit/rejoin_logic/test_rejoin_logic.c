#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "rejoin_logic.h"

static void assert_no_outcome(const struct app_rejoin_outcome *outcome)
{
	assert(outcome->log_started == false);
	assert(outcome->log_stopped == false);
	assert(outcome->schedule_retry == false);
	assert(outcome->stop_deferred == false);
	assert(outcome->retry_delay_s == 0U);
}

static void test_start_schedules_first_retry(void)
{
	struct app_rejoin_state state = {0};
	struct app_rejoin_outcome outcome;

	app_rejoin_start(&state, true, false, &outcome);

	assert(outcome.log_started == true);
	assert(outcome.schedule_retry == true);
	assert(outcome.retry_delay_s == 1U);
	assert(state.procedure_started == true);
	assert(state.retry_pending == false);
	assert(state.attempt_count == 1U);
}

static void test_joined_start_is_ignored_without_force(void)
{
	struct app_rejoin_state state = {0};
	struct app_rejoin_outcome outcome;

	app_rejoin_start(&state, true, true, &outcome);

	assert_no_outcome(&outcome);
	assert(state.procedure_started == false);
}

static void test_pending_retry_blocks_duplicate_schedule(void)
{
	struct app_rejoin_state state = {0};
	struct app_rejoin_outcome outcome;

	app_rejoin_start(&state, true, false, &outcome);
	app_rejoin_mark_retry_pending(&state);

	app_rejoin_process(&state, true, false, &outcome);

	assert_no_outcome(&outcome);
	assert(state.procedure_started == true);
	assert(state.retry_pending == true);
	assert(state.attempt_count == 1U);
}

static void test_backoff_grows_after_retry_fires(void)
{
	struct app_rejoin_state state = {0};
	struct app_rejoin_outcome outcome;

	app_rejoin_start(&state, true, false, &outcome);
	app_rejoin_mark_retry_pending(&state);
	app_rejoin_mark_retry_fired(&state);

	app_rejoin_process(&state, true, false, &outcome);

	assert(outcome.schedule_retry == true);
	assert(outcome.retry_delay_s == 2U);
	assert(state.attempt_count == 2U);
}

static void test_retry_delay_caps_at_maximum(void)
{
	struct app_rejoin_state state = {
		.procedure_started = true,
		.stop_requested = false,
		.retry_pending = false,
		.attempt_count = 10U,
	};
	struct app_rejoin_outcome outcome;

	app_rejoin_process(&state, true, false, &outcome);

	assert(outcome.schedule_retry == true);
	assert(outcome.retry_delay_s == APP_REJOIN_INTERVAL_MAX_S);
	assert(state.attempt_count == 10U);
}

static void test_stop_clears_state_when_cancel_succeeds(void)
{
	struct app_rejoin_state state = {
		.procedure_started = true,
		.stop_requested = false,
		.retry_pending = true,
		.attempt_count = 3U,
	};
	struct app_rejoin_outcome outcome;

	app_rejoin_stop(&state, true, &outcome);

	assert(outcome.log_stopped == true);
	assert(state.procedure_started == false);
	assert(state.stop_requested == false);
	assert(state.retry_pending == false);
	assert(state.attempt_count == 0U);
}

static void test_stop_defers_until_next_process_when_cancel_fails(void)
{
	struct app_rejoin_state state = {
		.procedure_started = true,
		.stop_requested = false,
		.retry_pending = false,
		.attempt_count = 3U,
	};
	struct app_rejoin_outcome outcome;

	app_rejoin_stop(&state, false, &outcome);

	assert(outcome.stop_deferred == true);
	assert(state.stop_requested == true);

	app_rejoin_process(&state, true, false, &outcome);

	assert(outcome.log_stopped == true);
	assert(state.procedure_started == false);
	assert(state.stop_requested == false);
	assert(state.retry_pending == false);
	assert(state.attempt_count == 0U);
}

int main(void)
{
	test_start_schedules_first_retry();
	test_joined_start_is_ignored_without_force();
	test_pending_retry_blocks_duplicate_schedule();
	test_backoff_grows_after_retry_fires();
	test_retry_delay_caps_at_maximum();
	test_stop_clears_state_when_cancel_succeeds();
	test_stop_defers_until_next_process_when_cancel_fails();

	printf("rejoin_logic unit tests passed\n");
	return 0;
}
