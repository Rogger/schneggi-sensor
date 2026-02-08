#include "sensor_logic.h"

unsigned int battery_level_pptt(unsigned int batt_mV, const struct battery_level_point *curve)
{
	const struct battery_level_point *pb = curve;

	if (batt_mV >= pb->lvl_mV)
	{
		return pb->lvl_pptt;
	}

	while ((pb->lvl_pptt > 0) && (batt_mV < pb->lvl_mV))
	{
		++pb;
	}

	if (batt_mV < pb->lvl_mV)
	{
		return pb->lvl_pptt;
	}

	const struct battery_level_point *pa = pb - 1;

	return pb->lvl_pptt + ((pa->lvl_pptt - pb->lvl_pptt) * (batt_mV - pb->lvl_mV) / (pa->lvl_mV - pb->lvl_mV));
}

uint16_t co2_ppm_to_attr_u16(double ppm, uint16_t max_value)
{
	if (ppm < 0.0)
	{
		return 0;
	}

	if (ppm > max_value)
	{
		return max_value;
	}

	return (uint16_t)ppm;
}
