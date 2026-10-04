#include <assert.h>
#include "app_scheduler.h"

static unsigned int pending, schedules;
static int cancel_result, schedule_result;
static zb_time_t alarm_delay;
static void callback(zb_uint8_t param) { (void)param; }

int alarm_cancel(alarm_cb cb, zb_uint8_t param)
{
	assert(cb == callback && param == ZB_ALARM_ALL_CB);
	if (cancel_result != RET_OK) { return cancel_result; }
	if (pending == 0) { return RET_NOT_FOUND; }
	pending = 0;
	return RET_OK;
}
int alarm_schedule(alarm_cb cb, zb_uint8_t param, unsigned int delay)
{
	assert(cb == callback && param == 42);
	schedules++;
	alarm_delay = delay;
	if (schedule_result != RET_OK) { return schedule_result; }
	pending++;
	return RET_OK;
}

int main(void)
{
	/* Exact ceil conversion, including the SDK overflow boundary and the
	 * maximum Kconfig sampling interval (24 hours).
	 */
	const struct { uint32_t milliseconds, beacons; } cases[] = {
		{0, 0}, {1, 1}, {15, 1}, {16, 2}, {100, 7}, {1000, 66},
		{10000, 652}, {30000, 1954}, {300000, 19532},
		{3600000, 234375}, {4320000, 281250}, {86400000, 5625000},
		{UINT32_MAX, 279620267},
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		assert(app_alarm_replace(callback, 42, cases[i].milliseconds) == RET_OK);
		assert(alarm_delay == cases[i].beacons && pending == 1);
	}

	/* Replacement removes every old alarm, not just the first match. */
	pending = 3;
	assert(app_alarm_replace(callback, 42, 100) == RET_OK && pending == 1);
	cancel_result = -EBUSY;
	unsigned int before_schedules = schedules;
	assert(app_alarm_replace(callback, 42, 100) == -EBUSY);
	assert(schedules == before_schedules && pending == 1);
	cancel_result = RET_OK;
	schedule_result = -ENOMEM;
	assert(app_alarm_replace(callback, 42, 100) == -ENOMEM && pending == 0);
	schedule_result = RET_OK;
	assert(app_alarm_replace(callback, 42, 100) == RET_OK && pending == 1);
	return 0;
}
