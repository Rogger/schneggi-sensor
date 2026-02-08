#ifndef SENSOR_LOGIC_H
#define SENSOR_LOGIC_H

#include <stdint.h>

struct battery_level_point
{
	uint16_t lvl_pptt;
	uint16_t lvl_mV;
};

unsigned int battery_level_pptt(unsigned int batt_mV, const struct battery_level_point *curve);
uint16_t co2_ppm_to_attr_u16(double ppm, uint16_t max_value);

#endif
