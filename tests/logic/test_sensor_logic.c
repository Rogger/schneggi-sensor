#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "sensor_logic.h"

static void assert_u32_eq(uint32_t expected, uint32_t actual, const char *name)
{
	if (expected != actual)
	{
		fprintf(stderr, "FAIL: %s expected=%u actual=%u\n", name, expected, actual);
		exit(1);
	}
}

static void test_battery_level_pptt(void)
{
	static const struct battery_level_point curve[] = {
		{10000, 4200},
		{5000, 3700},
		{0, 3200},
	};

	assert_u32_eq(10000, battery_level_pptt(4300, curve), "battery_above_max");
	assert_u32_eq(10000, battery_level_pptt(4200, curve), "battery_at_max");
	assert_u32_eq(0, battery_level_pptt(3000, curve), "battery_below_min");
	assert_u32_eq(5000, battery_level_pptt(3700, curve), "battery_midpoint_exact");
	assert_u32_eq(7500, battery_level_pptt(3950, curve), "battery_interpolate_upper");
	assert_u32_eq(2500, battery_level_pptt(3450, curve), "battery_interpolate_lower");
}

static void test_co2_ppm_to_attr_u16(void)
{
	const uint16_t max_val = 10000;

	assert_u32_eq(0, co2_ppm_to_attr_u16(-1.0, max_val), "co2_negative_clamped");
	assert_u32_eq(0, co2_ppm_to_attr_u16(0.0, max_val), "co2_zero");
	assert_u32_eq(412, co2_ppm_to_attr_u16(412.9, max_val), "co2_truncate_fraction");
	assert_u32_eq(10000, co2_ppm_to_attr_u16(10000.0, max_val), "co2_at_max");
	assert_u32_eq(10000, co2_ppm_to_attr_u16(10000.1, max_val), "co2_above_max_clamped");
}

int main(void)
{
	test_battery_level_pptt();
	test_co2_ppm_to_attr_u16();
	printf("All sensor_logic tests passed.\n");
	return 0;
}
