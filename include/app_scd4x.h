#ifndef APP_SCD4X_H_
#define APP_SCD4X_H_

#include "app_environment.h"
#include <zephyr/drivers/i2c.h>

/* Apply calibration and fetch the first sample after checking data readiness.
 * Returns an error if readiness exceeds ten seconds or an I/O operation fails.
 */
int app_scd4x_init(const struct device *sensor, const struct i2c_dt_spec *bus);

/* Fetch once and publish CO2 and temperature independently on their endpoints.
 * Called on the Zigbee thread with a ready SCD4x device.
 */
void app_scd4x_update(struct app_environment_report *temperature,
		     const struct device *sensor, uint8_t co2_endpoint,
		     uint8_t temperature_endpoint, uint32_t cycle,
		     uint32_t refresh_cycles);

#endif
