#include "app_scd4x.h"
#include "app_zcl_report.h"
#include "co2_zcl_logic.h"

#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/scd4x.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app_scd4x, LOG_LEVEL_INF);

int app_scd4x_init(const struct device *sensor)
{
	const struct sensor_value altitude = { .val1 = CONFIG_APP_SCD4X_ALTITUDE };
	const struct sensor_value offset = {0};
	const struct sensor_value calibration = { .val1 = 1 };
	int err = sensor_attr_set(sensor, SENSOR_CHAN_CO2,
		(enum sensor_attribute)SENSOR_ATTR_SCD4X_SENSOR_ALTITUDE, &altitude);
	if (!err) {
		err = sensor_attr_set(sensor, SENSOR_CHAN_AMBIENT_TEMP,
			(enum sensor_attribute)SENSOR_ATTR_SCD4X_TEMPERATURE_OFFSET, &offset);
	}
	if (!err) {
		err = sensor_attr_set(sensor, SENSOR_CHAN_CO2,
			(enum sensor_attribute)SENSOR_ATTR_SCD4X_AUTOMATIC_CALIB_ENABLE,
			&calibration);
	}
	if (!err) {
		/* Calibration restarts normal periodic measurements. The native
		 * driver returns success without changing its cache when not ready;
		 * wait for the first 5-second conversion before exposing that cache.
		 */
		k_sleep(K_MSEC(5000));
	}
	return err;
}

void app_scd4x_update(struct app_environment_report *temperature,
		     const struct device *sensor, uint8_t co2_endpoint,
		     uint8_t temperature_endpoint, uint32_t cycle,
		     uint32_t refresh_cycles)
{
	int err = sensor_sample_fetch(sensor);
	if (err != 0) {
		LOG_WRN("SCD4X fetch failed (%d); keeping previous values", err);
		return;
	}

	struct sensor_value sample;
	err = sensor_channel_get(sensor, SENSOR_CHAN_CO2, &sample);
	if (err != 0) {
		LOG_WRN("SCD4X CO2 read failed (%d); keeping previous value", err);
	} else {
		double ppm = sensor_value_to_double(&sample);
		LOG_INF("CO2: %.2f ppm", ppm);
		if (ppm < 0.0 || ppm > CO2_ZCL_MAX_PPM) {
			LOG_WRN("CO2 outside supported range; clamping");
		}
		zb_zcl_status_t status = app_zcl_report_co2_fraction(
			co2_endpoint, co2_zcl_fraction_from_ppm(ppm));
		if (status != ZB_ZCL_STATUS_SUCCESS) {
			LOG_WRN("CO2 attribute update failed (%u)", status);
		}
	}

	app_environment_update_temperature(temperature, sensor,
					   temperature_endpoint, cycle, refresh_cycles);
}
