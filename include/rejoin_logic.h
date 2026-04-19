#ifndef REJOIN_LOGIC_H_
#define REJOIN_LOGIC_H_

#include <stdbool.h>
#include <stdint.h>

#define APP_REJOIN_INTERVAL_MAX_S (15U * 60U)

struct app_rejoin_state {
	bool procedure_started;
	bool stop_requested;
	bool retry_pending;
	uint8_t attempt_count;
};

struct app_rejoin_outcome {
	bool log_started;
	bool log_stopped;
	bool schedule_retry;
	bool stop_deferred;
	uint32_t retry_delay_s;
};

void app_rejoin_outcome_reset(struct app_rejoin_outcome *outcome);

void app_rejoin_start(struct app_rejoin_state *state,
		      bool stack_initialised,
		      bool joined,
		      struct app_rejoin_outcome *outcome);

void app_rejoin_process(struct app_rejoin_state *state,
			bool stack_initialised,
			bool joined,
			struct app_rejoin_outcome *outcome);

void app_rejoin_mark_retry_pending(struct app_rejoin_state *state);
void app_rejoin_mark_retry_fired(struct app_rejoin_state *state);

void app_rejoin_stop(struct app_rejoin_state *state,
		     bool cancel_succeeded,
		     struct app_rejoin_outcome *outcome);

#endif /* REJOIN_LOGIC_H_ */
