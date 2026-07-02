#ifndef APP_MEASUREMENT_LOGIC_H_
#define APP_MEASUREMENT_LOGIC_H_

#include <stdbool.h>
#include <stdint.h>

bool app_report_due_s16(bool valid,
			int16_t previous_value,
			uint32_t previous_cycle,
			int16_t new_value,
			int16_t threshold,
			uint32_t current_cycle,
			uint32_t refresh_cycles);

bool app_report_due_s32(bool valid,
			int32_t previous_value,
			uint32_t previous_cycle,
			int32_t new_value,
			int32_t threshold,
			uint32_t current_cycle,
			uint32_t refresh_cycles);

bool app_report_due_u8(bool valid,
		       uint8_t previous_value,
		       uint32_t previous_cycle,
		       uint8_t new_value,
		       uint8_t threshold,
		       uint32_t current_cycle,
		       uint32_t refresh_cycles);

unsigned int app_battery_level_pptt(unsigned int batt_mv);

#endif /* APP_MEASUREMENT_LOGIC_H_ */
