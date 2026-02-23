#include "co2_zcl_logic.h"

uint32_t co2_zcl_single_from_float(float value)
{
	union
	{
		float value_f;
		uint32_t value_u32;
	} data = {.value_f = value};

	return data.value_u32;
}

float co2_zcl_single_to_float(uint32_t raw_value)
{
	union
	{
		uint32_t value_u32;
		float value_f;
	} data = {.value_u32 = raw_value};

	return data.value_f;
}

bool co2_zcl_single_is_nan(uint32_t raw_value)
{
	return ((raw_value & 0x7F800000u) == 0x7F800000u) &&
		   ((raw_value & 0x007FFFFFu) != 0u);
}

float co2_zcl_fraction_from_ppm(double ppm_value)
{
	if (ppm_value < 0.0) {
		return 0.0f;
	}

	if (ppm_value > CO2_ZCL_MAX_PPM) {
		return CO2_ZCL_MAX_FRACTION;
	}

	return (float)(ppm_value * 0.000001);
}

bool co2_zcl_is_valid_measured_raw(uint32_t measured_raw, uint32_t min_raw, uint32_t max_raw)
{
	float measured_value;

	if (co2_zcl_single_is_nan(measured_raw)) {
		return true;
	}

	measured_value = co2_zcl_single_to_float(measured_raw);
	if (measured_value < 0.0f || measured_value > 1.0f) {
		return false;
	}

	if (!co2_zcl_single_is_nan(min_raw) &&
		measured_value < co2_zcl_single_to_float(min_raw)) {
		return false;
	}

	if (!co2_zcl_single_is_nan(max_raw) &&
		measured_value > co2_zcl_single_to_float(max_raw)) {
		return false;
	}

	return true;
}

bool co2_zcl_is_valid_min_raw(uint32_t min_raw, uint32_t max_raw)
{
	float min_value;

	if (co2_zcl_single_is_nan(min_raw)) {
		return true;
	}

	min_value = co2_zcl_single_to_float(min_raw);
	if (min_value < 0.0f || min_value >= 1.0f) {
		return false;
	}

	if (!co2_zcl_single_is_nan(max_raw) &&
		!(min_value < co2_zcl_single_to_float(max_raw))) {
		return false;
	}

	return true;
}

bool co2_zcl_is_valid_max_raw(uint32_t max_raw, uint32_t min_raw)
{
	float max_value;

	if (co2_zcl_single_is_nan(max_raw)) {
		return true;
	}

	max_value = co2_zcl_single_to_float(max_raw);
	if (max_value <= 0.0f || max_value > 1.0f) {
		return false;
	}

	if (!co2_zcl_single_is_nan(min_raw) &&
		!(co2_zcl_single_to_float(min_raw) < max_value)) {
		return false;
	}

	return true;
}
