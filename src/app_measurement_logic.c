#include "app_measurement_logic.h"

#include <limits.h>

struct battery_level_point {
	uint16_t lvl_pptt;
	uint16_t lvl_mv;
};

static const struct battery_level_point lipo_discharge_curve[] = {
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

#define APP_BATTERY_DIVIDER_HIGH_OHM 1500000
#define APP_BATTERY_DIVIDER_LOW_OHM 180000

static uint32_t abs_diff_s32(int32_t left, int32_t right)
{
	int64_t diff = (int64_t)left - (int64_t)right;

	return diff >= 0 ? (uint32_t)diff : (uint32_t)-diff;
}

static uint16_t abs_diff_s16(int16_t left, int16_t right)
{
	return left >= right ? (uint16_t)(left - right) : (uint16_t)(right - left);
}

static uint8_t abs_diff_u8(uint8_t left, uint8_t right)
{
	return left >= right ? (uint8_t)(left - right) : (uint8_t)(right - left);
}

bool app_report_due_s16(bool valid,
			int16_t previous_value,
			uint32_t previous_cycle,
			int16_t new_value,
			int16_t threshold,
			uint32_t current_cycle,
			uint32_t refresh_cycles)
{
	if (!valid) {
		return true;
	}

	if (abs_diff_s16(new_value, previous_value) >= (uint16_t)threshold) {
		return true;
	}

	return (current_cycle - previous_cycle) >= refresh_cycles;
}

bool app_report_due_s32(bool valid,
			int32_t previous_value,
			uint32_t previous_cycle,
			int32_t new_value,
			int32_t threshold,
			uint32_t current_cycle,
			uint32_t refresh_cycles)
{
	if (!valid) {
		return true;
	}

	if (abs_diff_s32(new_value, previous_value) >= (uint32_t)threshold) {
		return true;
	}

	return (current_cycle - previous_cycle) >= refresh_cycles;
}

bool app_report_due_u8(bool valid,
		       uint8_t previous_value,
		       uint32_t previous_cycle,
		       uint8_t new_value,
		       uint8_t threshold,
		       uint32_t current_cycle,
		       uint32_t refresh_cycles)
{
	if (!valid) {
		return true;
	}

	if (abs_diff_u8(new_value, previous_value) >= threshold) {
		return true;
	}

	return (current_cycle - previous_cycle) >= refresh_cycles;
}

unsigned int app_battery_level_pptt(unsigned int batt_mv)
{
	const struct battery_level_point *pb = lipo_discharge_curve;

	if (batt_mv >= pb->lvl_mv) {
		return pb->lvl_pptt;
	}

	while ((pb->lvl_pptt > 0U) && (batt_mv < pb->lvl_mv)) {
		++pb;
	}

	if (batt_mv < pb->lvl_mv) {
		return pb->lvl_pptt;
	}

	const struct battery_level_point *pa = pb - 1;

	return pb->lvl_pptt +
	       ((pa->lvl_pptt - pb->lvl_pptt) * (batt_mv - pb->lvl_mv) /
		(pa->lvl_mv - pb->lvl_mv));
}

int32_t app_battery_millivolts_from_adc(int32_t adc_mv)
{
	int64_t battery_mv =
		(int64_t)adc_mv *
		(APP_BATTERY_DIVIDER_HIGH_OHM + APP_BATTERY_DIVIDER_LOW_OHM) /
		APP_BATTERY_DIVIDER_LOW_OHM;

	if (battery_mv > INT32_MAX) {
		return INT32_MAX;
	}

	if (battery_mv < INT32_MIN) {
		return INT32_MIN;
	}

	return (int32_t)battery_mv;
}

uint8_t app_battery_voltage_zcl_attribute(int32_t battery_mv)
{
	return (uint8_t)(battery_mv / 100);
}

uint8_t app_battery_percentage_from_mv(uint32_t battery_mv)
{
	return (uint8_t)(app_battery_level_pptt(battery_mv) / 100U);
}

uint8_t app_battery_percentage_zcl_attribute(uint8_t percentage)
{
	return (uint8_t)(percentage * 2U);
}
