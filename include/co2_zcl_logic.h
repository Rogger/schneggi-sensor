#ifndef CO2_ZCL_LOGIC_H_
#define CO2_ZCL_LOGIC_H_

#include <stdbool.h>
#include <stdint.h>

#define CO2_ZCL_MAX_FRACTION 1.0f
#define CO2_ZCL_MAX_PPM 1000000.0

uint32_t co2_zcl_single_from_float(float value);
float co2_zcl_single_to_float(uint32_t raw_value);
bool co2_zcl_single_is_nan(uint32_t raw_value);

float co2_zcl_fraction_from_ppm(double ppm_value);

bool co2_zcl_is_valid_measured_raw(uint32_t measured_raw, uint32_t min_raw, uint32_t max_raw);
bool co2_zcl_is_valid_min_raw(uint32_t min_raw, uint32_t max_raw);
bool co2_zcl_is_valid_max_raw(uint32_t max_raw, uint32_t min_raw);

#endif /* CO2_ZCL_LOGIC_H_ */
