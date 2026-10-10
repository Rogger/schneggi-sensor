#ifndef APP_SCD4X_H_
#define APP_SCD4X_H_

#include "app_environment.h"

/* Fetch once and publish CO2 and temperature independently on their endpoints.
 * Called on the Zigbee thread with a ready SCD4x device.
 */
void app_scd4x_update(struct app_environment_report *temperature,
		     const struct device *sensor, uint8_t co2_endpoint,
		     uint8_t temperature_endpoint, uint32_t cycle,
		     uint32_t refresh_cycles);

#endif
