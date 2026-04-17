#include "rejoin_logic.h"

static void app_rejoin_state_reset(struct app_rejoin_state *state)
{
	state->procedure_started = false;
	state->stop_requested = false;
	state->retry_pending = false;
	state->attempt_count = 0U;
}

void app_rejoin_outcome_reset(struct app_rejoin_outcome *outcome)
{
	outcome->log_started = false;
	outcome->log_stopped = false;
	outcome->schedule_retry = false;
	outcome->stop_deferred = false;
	outcome->retry_delay_s = 0U;
}

void app_rejoin_process(struct app_rejoin_state *state,
			bool stack_initialised,
			bool joined,
			struct app_rejoin_outcome *outcome)
{
	uint32_t timeout_s;

	app_rejoin_outcome_reset(outcome);

	if (!stack_initialised || !state->procedure_started) {
		return;
	}

	if (state->stop_requested) {
		app_rejoin_state_reset(state);
		outcome->log_stopped = true;
		return;
	}

	if (state->retry_pending) {
		return;
	}

	if (joined) {
		return;
	}

	if (state->attempt_count >= 31U) {
		timeout_s = APP_REJOIN_INTERVAL_MAX_S;
	} else {
		timeout_s = 1U << state->attempt_count;
		if (timeout_s > APP_REJOIN_INTERVAL_MAX_S) {
			timeout_s = APP_REJOIN_INTERVAL_MAX_S;
		} else {
			state->attempt_count++;
		}
	}

	outcome->schedule_retry = true;
	outcome->retry_delay_s = timeout_s;
}

void app_rejoin_start(struct app_rejoin_state *state,
		      bool stack_initialised,
		      bool joined,
		      struct app_rejoin_outcome *outcome)
{
	struct app_rejoin_outcome process_outcome;

	app_rejoin_outcome_reset(outcome);

	if (!stack_initialised) {
		return;
	}

	if (joined) {
		return;
	}

	if (!state->procedure_started) {
		state->procedure_started = true;
		state->stop_requested = false;
		state->retry_pending = false;
		state->attempt_count = 0U;
		outcome->log_started = true;
	}

	app_rejoin_process(state, stack_initialised, joined, &process_outcome);
	outcome->log_stopped = process_outcome.log_stopped;
	outcome->schedule_retry = process_outcome.schedule_retry;
	outcome->stop_deferred = process_outcome.stop_deferred;
	outcome->retry_delay_s = process_outcome.retry_delay_s;
}

void app_rejoin_mark_retry_pending(struct app_rejoin_state *state)
{
	state->retry_pending = true;
}

void app_rejoin_mark_retry_fired(struct app_rejoin_state *state)
{
	state->retry_pending = false;
}

void app_rejoin_stop(struct app_rejoin_state *state,
		     bool cancel_succeeded,
		     struct app_rejoin_outcome *outcome)
{
	app_rejoin_outcome_reset(outcome);

	if (!state->procedure_started) {
		return;
	}

	if (cancel_succeeded) {
		app_rejoin_state_reset(state);
		outcome->log_stopped = true;
	} else {
		state->stop_requested = true;
		outcome->stop_deferred = true;
	}
}
