#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "co2_zcl_logic.h"

static void assert_close(float actual, float expected, float tolerance)
{
	float diff = actual - expected;
	if (diff < 0.0f) {
		diff = -diff;
	}
	assert(diff <= tolerance);
}

static void test_fraction_from_ppm_clamps_and_scales(void)
{
	assert_close(co2_zcl_fraction_from_ppm(-1.0), 0.0f, 1e-9f);
	assert_close(co2_zcl_fraction_from_ppm(0.0), 0.0f, 1e-9f);
	assert_close(co2_zcl_fraction_from_ppm(400.0), 0.0004f, 1e-8f);
	assert_close(co2_zcl_fraction_from_ppm(CO2_ZCL_MAX_PPM), 1.0f, 1e-7f);
	assert_close(co2_zcl_fraction_from_ppm(CO2_ZCL_MAX_PPM + 1.0), 1.0f, 1e-7f);
}

static void test_single_helpers(void)
{
	assert(co2_zcl_single_is_nan(0x7FC00000u));
	assert(!co2_zcl_single_is_nan(0x7F800000u));

	assert(co2_zcl_single_from_float(0.0f) == 0x00000000u);
	assert(co2_zcl_single_from_float(1.0f) == 0x3F800000u);
	assert_close(co2_zcl_single_to_float(0x3A03126Fu), 0.0005f, 1e-7f);
}

static void test_measured_validator_accepts_nan_and_enforces_range(void)
{
	uint32_t nan_raw = 0x7FC00000u;
	uint32_t zero_raw = co2_zcl_single_from_float(0.0f);
	uint32_t one_raw = co2_zcl_single_from_float(1.0f);

	assert(co2_zcl_is_valid_measured_raw(nan_raw, nan_raw, nan_raw));
	assert(co2_zcl_is_valid_measured_raw(zero_raw, nan_raw, nan_raw));
	assert(co2_zcl_is_valid_measured_raw(one_raw, nan_raw, nan_raw));
	assert(!co2_zcl_is_valid_measured_raw(co2_zcl_single_from_float(-0.001f), nan_raw, nan_raw));
	assert(!co2_zcl_is_valid_measured_raw(co2_zcl_single_from_float(1.001f), nan_raw, nan_raw));
}

static void test_measured_validator_enforces_min_and_max(void)
{
	uint32_t min_raw = co2_zcl_single_from_float(0.2f);
	uint32_t max_raw = co2_zcl_single_from_float(0.8f);

	assert(!co2_zcl_is_valid_measured_raw(co2_zcl_single_from_float(0.1f), min_raw, max_raw));
	assert(co2_zcl_is_valid_measured_raw(co2_zcl_single_from_float(0.5f), min_raw, max_raw));
	assert(!co2_zcl_is_valid_measured_raw(co2_zcl_single_from_float(0.9f), min_raw, max_raw));
}

static void test_min_max_validators(void)
{
	uint32_t nan_raw = 0x7FC00000u;

	assert(co2_zcl_is_valid_min_raw(nan_raw, co2_zcl_single_from_float(0.7f)));
	assert(co2_zcl_is_valid_min_raw(co2_zcl_single_from_float(0.6f), nan_raw));
	assert(!co2_zcl_is_valid_min_raw(co2_zcl_single_from_float(1.0f), nan_raw));
	assert(!co2_zcl_is_valid_min_raw(co2_zcl_single_from_float(0.8f), co2_zcl_single_from_float(0.7f)));

	assert(co2_zcl_is_valid_max_raw(nan_raw, co2_zcl_single_from_float(0.5f)));
	assert(co2_zcl_is_valid_max_raw(co2_zcl_single_from_float(0.7f), nan_raw));
	assert(!co2_zcl_is_valid_max_raw(co2_zcl_single_from_float(0.0f), nan_raw));
	assert(!co2_zcl_is_valid_max_raw(co2_zcl_single_from_float(1.1f), nan_raw));
	assert(!co2_zcl_is_valid_max_raw(co2_zcl_single_from_float(0.4f), co2_zcl_single_from_float(0.5f)));
}

int main(void)
{
	test_fraction_from_ppm_clamps_and_scales();
	test_single_helpers();
	test_measured_validator_accepts_nan_and_enforces_range();
	test_measured_validator_enforces_min_and_max();
	test_min_max_validators();

	printf("co2_zcl_logic unit tests passed\n");
	return 0;
}
