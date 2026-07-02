#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "app_measurement_logic.h"

static void test_report_due_when_value_is_invalid(void)
{
	assert(app_report_due_s16(false, 100, 0U, 100, 10, 0U, 10U));
	assert(app_report_due_s32(false, 1000, 0U, 1000, 100, 0U, 10U));
	assert(app_report_due_u8(false, 50U, 0U, 50U, 1U, 0U, 10U));
}

static void test_report_due_on_threshold_delta(void)
{
	assert(!app_report_due_s16(true, 100, 0U, 109, 10, 1U, 10U));
	assert(app_report_due_s16(true, 100, 0U, 110, 10, 1U, 10U));
	assert(app_report_due_s16(true, 100, 0U, 90, 10, 1U, 10U));

	assert(!app_report_due_s32(true, 3700, 0U, 3799, 100, 1U, 10U));
	assert(app_report_due_s32(true, 3700, 0U, 3800, 100, 1U, 10U));
	assert(app_report_due_s32(true, 3700, 0U, 3600, 100, 1U, 10U));

	assert(!app_report_due_u8(true, 50U, 0U, 50U, 1U, 1U, 10U));
	assert(app_report_due_u8(true, 50U, 0U, 51U, 1U, 1U, 10U));
	assert(app_report_due_u8(true, 50U, 0U, 49U, 1U, 1U, 10U));
}

static void test_report_due_on_refresh_cycle(void)
{
	assert(!app_report_due_s16(true, 100, 5U, 100, 10, 14U, 10U));
	assert(app_report_due_s16(true, 100, 5U, 100, 10, 15U, 10U));

	assert(!app_report_due_s32(true, 1000, 5U, 1000, 100, 14U, 10U));
	assert(app_report_due_s32(true, 1000, 5U, 1000, 100, 15U, 10U));

	assert(!app_report_due_u8(true, 50U, 5U, 50U, 1U, 14U, 10U));
	assert(app_report_due_u8(true, 50U, 5U, 50U, 1U, 15U, 10U));
}

static void test_battery_curve_caps_and_interpolates(void)
{
	assert(app_battery_level_pptt(4300U) == 10000U);
	assert(app_battery_level_pptt(4200U) == 10000U);
	assert(app_battery_level_pptt(4150U) == 9500U);
	assert(app_battery_level_pptt(3270U) == 0U);
	assert(app_battery_level_pptt(3000U) == 0U);
	assert(app_battery_level_pptt(4175U) == 9750U);
	assert(app_battery_level_pptt(4100U) == 8833U);
}

int main(void)
{
	test_report_due_when_value_is_invalid();
	test_report_due_on_threshold_delta();
	test_report_due_on_refresh_cycle();
	test_battery_curve_caps_and_interpolates();

	printf("measurement_logic unit tests passed\n");
	return 0;
}
