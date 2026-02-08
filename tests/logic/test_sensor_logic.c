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

static void assert_true(int cond, const char *name)
{
	if (!cond)
	{
		fprintf(stderr, "FAIL: %s\n", name);
		exit(1);
	}
}

static const struct battery_level_point full_curve[] = {
	{10000, 4200},
	{9500, 4150},
	{9000, 4110},
	{8500, 4080},
	{8000, 4020},
	{7500, 3980},
	{7000, 3950},
	{6500, 3910},
	{6000, 3870},
	{5500, 3850},
	{5000, 3840},
	{4500, 3820},
	{4000, 3800},
	{3500, 3790},
	{3000, 3770},
	{2500, 3750},
	{2000, 3730},
	{1500, 3710},
	{1000, 3690},
	{500, 3610},
	{0, 3270},
};

static void test_battery_level_pptt_interpolation(void)
{
	static const struct battery_level_point simple_curve[] = {
		{10000, 4200},
		{5000, 3700},
		{0, 3200},
	};

	assert_u32_eq(10000, battery_level_pptt(4300, simple_curve), "battery_above_max");
	assert_u32_eq(10000, battery_level_pptt(4200, simple_curve), "battery_at_max");
	assert_u32_eq(0, battery_level_pptt(3000, simple_curve), "battery_below_min");
	assert_u32_eq(5000, battery_level_pptt(3700, simple_curve), "battery_midpoint_exact");
	assert_u32_eq(7500, battery_level_pptt(3950, simple_curve), "battery_interpolate_upper");
	assert_u32_eq(2500, battery_level_pptt(3450, simple_curve), "battery_interpolate_lower");
}

static void test_battery_level_pptt_exact_curve_points(void)
{
	for (size_t i = 0; i < sizeof(full_curve) / sizeof(full_curve[0]); i++)
	{
		char name[64];
		snprintf(name, sizeof(name), "battery_exact_point_%zu", i);
		assert_u32_eq(full_curve[i].lvl_pptt, battery_level_pptt(full_curve[i].lvl_mV, full_curve), name);
	}
}

static void test_battery_level_pptt_monotonicity(void)
{
	uint32_t last = 10001;
	for (int mv = 4200; mv >= 3200; mv -= 10)
	{
		uint32_t current = battery_level_pptt((uint32_t)mv, full_curve);
		assert_true(current <= last, "battery_monotonic_nonincreasing");
		last = current;
	}
}

static void test_co2_ppm_to_attr_u16(void)
{
	const uint16_t max_val = 10000;

	assert_u32_eq(0, co2_ppm_to_attr_u16(-1.0, max_val), "co2_negative_clamped");
	assert_u32_eq(0, co2_ppm_to_attr_u16(-0.01, max_val), "co2_negative_tiny_clamped");
	assert_u32_eq(0, co2_ppm_to_attr_u16(0.0, max_val), "co2_zero");
	assert_u32_eq(0, co2_ppm_to_attr_u16(0.99, max_val), "co2_sub_one_truncates_zero");
	assert_u32_eq(412, co2_ppm_to_attr_u16(412.9, max_val), "co2_truncate_fraction");
	assert_u32_eq(9999, co2_ppm_to_attr_u16(9999.99, max_val), "co2_below_max_fraction");
	assert_u32_eq(10000, co2_ppm_to_attr_u16(10000.0, max_val), "co2_at_max");
	assert_u32_eq(10000, co2_ppm_to_attr_u16(10000.01, max_val), "co2_above_max_tiny_clamped");
	assert_u32_eq(10000, co2_ppm_to_attr_u16(10000.1, max_val), "co2_above_max_clamped");
}

static void test_compute_battery_sleep_cycles(void)
{
	assert_u32_eq(48, compute_battery_sleep_cycles(4U * 60U * 60U, 5U * 60U), "cycles_normal_ratio");
	assert_u32_eq(1, compute_battery_sleep_cycles(3599U, 3600U), "cycles_floor_to_minimum");
	assert_u32_eq(1, compute_battery_sleep_cycles(3600U, 0U), "cycles_zero_sleep_guard");
	assert_u32_eq(2, compute_battery_sleep_cycles(600U, 300U), "cycles_exact_divisible");
	assert_u32_eq(2, compute_battery_sleep_cycles(601U, 300U), "cycles_non_divisible_floor");
}

static void test_commissioning_state_transitions(void)
{
	struct commissioning_state state = {0};

	assert_true(!commissioning_should_reset_on_parent_link_failure(&state), "comm_initial_no_reset");

	commissioning_on_skip_startup(&state);
	assert_true(state.stack_initialised, "comm_skip_sets_stack_initialized");
	assert_true(!state.joining_signal_received, "comm_skip_clears_joining");
	assert_true(commissioning_should_reset_on_parent_link_failure(&state), "comm_parent_fail_after_skip");

	commissioning_on_steering_result(&state, true);
	assert_true(state.joining_signal_received, "comm_steering_success_sets_joining");
	assert_true(!commissioning_should_reset_on_parent_link_failure(&state), "comm_no_reset_when_joining");

	commissioning_on_steering_result(&state, false);
	assert_true(!state.joining_signal_received, "comm_steering_fail_clears_joining");
	assert_true(commissioning_should_reset_on_parent_link_failure(&state), "comm_reset_after_steering_fail");

	commissioning_on_leave(&state);
	assert_true(!state.joining_signal_received, "comm_leave_clears_joining");
	assert_true(commissioning_should_reset_on_parent_link_failure(&state), "comm_reset_after_leave");
}

int main(void)
{
	test_battery_level_pptt_interpolation();
	test_battery_level_pptt_exact_curve_points();
	test_battery_level_pptt_monotonicity();
	test_co2_ppm_to_attr_u16();
	test_compute_battery_sleep_cycles();
	test_commissioning_state_transitions();
	printf("All sensor_logic tests passed.\n");
	return 0;
}
