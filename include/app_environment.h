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

/* Called on the Zigbee thread with a ready SHTC3 device. */
void app_environment_update(struct app_environment *state,
			    const struct device *sensor, const struct i2c_dt_spec *bus,
			    uint8_t endpoint, uint32_t cycle, uint32_t refresh_cycles);

#endif
