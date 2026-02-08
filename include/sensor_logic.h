#ifndef SENSOR_LOGIC_H
#define SENSOR_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

struct battery_level_point
{
	uint16_t lvl_pptt;
	uint16_t lvl_mV;
};

struct commissioning_state
{
	bool joining_signal_received;
	bool stack_initialised;
};

unsigned int battery_level_pptt(unsigned int batt_mV, const struct battery_level_point *curve);
uint16_t compute_battery_sleep_cycles(uint32_t battery_interval_seconds, uint32_t sleep_interval_seconds);
uint16_t co2_ppm_to_attr_u16(double ppm, uint16_t max_value);
void commissioning_on_skip_startup(struct commissioning_state *state);
void commissioning_on_steering_result(struct commissioning_state *state, bool success);
void commissioning_on_leave(struct commissioning_state *state);
bool commissioning_should_reset_on_parent_link_failure(const struct commissioning_state *state);

#endif
