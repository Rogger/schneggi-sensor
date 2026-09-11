#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
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

static void test_battery_attribute_conversions(void)
{
	assert(app_battery_millivolts_from_adc(450) == 4200);
	assert(app_battery_millivolts_from_adc(3600) == 33600);
	assert(app_battery_millivolts_from_adc(0) == 0);
	assert(app_battery_millivolts_from_adc(INT32_MAX) == INT32_MAX);
	assert(app_battery_millivolts_from_adc(INT32_MIN) == INT32_MIN);
	assert(app_battery_voltage_zcl_attribute(-100) == 0U);
	assert(app_battery_voltage_zcl_attribute(4200) == 42U);
	assert(app_battery_voltage_zcl_attribute(3270) == 32U);
	assert(app_battery_voltage_zcl_attribute(30000) == 255U);

	assert(app_battery_percentage_from_mv(4200U) == 100U);
	assert(app_battery_percentage_from_mv(4175U) == 97U);
	assert(app_battery_percentage_from_mv(3270U) == 0U);
	assert(app_battery_percentage_zcl_attribute(100U) == 200U);
	assert(app_battery_percentage_zcl_attribute(97U) == 194U);
	assert(app_battery_percentage_zcl_attribute(101U) == 200U);
	assert(app_battery_percentage_zcl_attribute(200U) == 200U);
}

static void test_report_cycle_wraparound_and_extreme_deltas(void)
{
	assert(!app_report_due_s16(true, 0, UINT32_MAX - 4U, 0, 1, 4U, 10U));
	assert(app_report_due_s16(true, 0, UINT32_MAX - 4U, 0, 1, 5U, 10U));
	assert(!app_report_due_s32(true, 0, UINT32_MAX - 4U, 0, 1, 4U, 10U));
	assert(app_report_due_s32(true, 0, UINT32_MAX - 4U, 0, 1, 5U, 10U));
	assert(!app_report_due_u8(true, 0, UINT32_MAX - 4U, 0, 1, 4U, 10U));
	assert(app_report_due_u8(true, 0, UINT32_MAX - 4U, 0, 1, 5U, 10U));
	assert(app_report_due_s32(true, INT32_MIN, 0, INT32_MAX, INT32_MAX, 0, 10));
	assert(app_report_due_s32(true, INT32_MAX, 0, INT32_MIN, INT32_MAX, 0, 10));
	assert(app_report_due_s16(true, INT16_MIN, 0, INT16_MAX, INT16_MAX, 0, 10));
	assert(app_report_due_s16(true, INT16_MAX, 0, INT16_MIN, INT16_MAX, 0, 10));
}

static void test_battery_curve_is_monotonic_and_bounded(void)
{
	unsigned int previous = 0;
	for (unsigned int mv = 0; mv <= 6000; ++mv) {
		unsigned int level = app_battery_level_pptt(mv);
		assert(level >= previous);
		assert(level <= 10000);
		previous = level;
	}
}

int main(void)
{
	test_report_cycle_wraparound_and_extreme_deltas();
	test_battery_curve_is_monotonic_and_bounded();
	test_report_due_when_value_is_invalid();
	test_report_due_on_threshold_delta();
	test_report_due_on_refresh_cycle();
	test_battery_curve_caps_and_interpolates();
	test_battery_attribute_conversions();

	printf("measurement_logic unit tests passed\n");
	return 0;
}
