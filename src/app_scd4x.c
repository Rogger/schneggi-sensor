#include "app_scd4x.h"
#include "app_zcl_report.h"
#include "co2_zcl_logic.h"

#include <errno.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/scd4x.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/crc.h>

LOG_MODULE_REGISTER(app_scd4x, LOG_LEVEL_INF);

static int wait_for_first_sample(const struct device *sensor,
				 const struct i2c_dt_spec *bus)
{
	/* The native driver returns success with its unchanged cache when data
	 * is not ready. Query readiness explicitly before the first fetch.
	 * SCD4x datasheet sections 3.6.2 and 3.9.2: readiness reads do not consume
	 * the measurement, and no other code reads this sensor during startup.
	 */
	static const uint8_t command[] = {0xe4, 0xb8};
	const int64_t deadline = k_uptime_get() + 10000;

	while (k_uptime_get() < deadline) {
		uint8_t response[3];
		int err = i2c_write_dt(bus, command, sizeof(command));
		if (err) {
			return err;
		}
		k_sleep(K_MSEC(1));
		err = i2c_read_dt(bus, response, sizeof(response));
		if (err) {
			return err;
		}
		if (crc8(response, 2, 0x31, 0xff, false) != response[2]) {
			return -EIO;
		}
		uint16_t status = ((uint16_t)response[0] << 8) | response[1];
		if (status & 0x07ff) {
			/* The driver reads and CRC-checks all three measurement words. */
			return sensor_sample_fetch(sensor);
		}
		int64_t remaining = deadline - k_uptime_get();
		if (remaining <= 0) {
			break;
		}
		k_sleep(K_MSEC(remaining < 100 ? remaining : 100));
	}
	return -ETIMEDOUT;
}

int app_scd4x_init(const struct device *sensor, const struct i2c_dt_spec *bus)
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
		err = wait_for_first_sample(sensor, bus);
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
