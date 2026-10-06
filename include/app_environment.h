#ifndef APP_ENVIRONMENT_H_
#define APP_ENVIRONMENT_H_

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/drivers/i2c.h>

struct app_environment_report {
	bool valid;
	int16_t value;
	uint32_t cycle;
};

struct app_environment {
	struct app_environment_report temperature;
	struct app_environment_report humidity;
};

/* Publish temperature from an already fetched sample. Each endpoint needs its
 * own state; this does not fetch again or run SHTC3-specific sleep cleanup.
 */
void app_environment_update_temperature(struct app_environment_report *state,
					const struct device *sensor, uint8_t endpoint,
					uint32_t cycle, uint32_t refresh_cycles);

/* Called on the Zigbee thread with a ready SHTC3 device. */
void app_environment_update(struct app_environment *state,
			    const struct device *sensor, const struct i2c_dt_spec *bus,
			    uint8_t endpoint, uint32_t cycle, uint32_t refresh_cycles);

#endif
