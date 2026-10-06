#include "app_environment.h"
#include "app_measurement_logic.h"
#include "app_shtc3.h"
#include "app_zcl_report.h"

#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zcl/zb_zcl_temp_measurement_addons.h>

LOG_MODULE_REGISTER(app_environment, LOG_LEVEL_INF);

struct channel {
	enum sensor_channel sensor_channel;
	uint16_t cluster;
	int16_t threshold;
	int16_t minimum;
	int16_t maximum;
	const char *name;
	zb_zcl_status_t (*publish)(zb_uint8_t endpoint, int16_t value);
};

static const struct channel temperature = {
	.sensor_channel = SENSOR_CHAN_AMBIENT_TEMP,
	.cluster = ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
	.threshold = 10,
	.minimum = -27315,
	.maximum = INT16_MAX,
	.name = "Temperature",
	.publish = app_zcl_report_temperature,
};

static const struct channel humidity = {
	.sensor_channel = SENSOR_CHAN_HUMIDITY,
	.cluster = ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT,
	.threshold = 100,
	.minimum = 0,
	.maximum = 10000,
	.name = "Humidity",
	.publish = app_zcl_report_humidity,
};

static void update_channel(struct app_environment_report *state,
			   const struct channel *channel, const struct device *sensor,
			   uint8_t endpoint, uint32_t cycle, uint32_t refresh_cycles)
{
	struct sensor_value sample;
	int err = sensor_channel_get(sensor, channel->sensor_channel, &sample);
	if (err != 0) {
		LOG_WRN("%s read failed (%d); keeping previous value", channel->name, err);
		return;
	}

	/* Keep fixed-point samples exact and check bounds before narrowing to the
	 * ZCL representation. Floating-point truncation can lose a hundredth.
	 */
	int64_t micro = sensor_value_to_micro(&sample);
	if (micro < (int64_t)channel->minimum * 10000 ||
	    micro > (int64_t)channel->maximum * 10000) {
		LOG_WRN("%s outside the ZCL range; keeping previous value", channel->name);
		return;
	}
	int16_t value = (int16_t)(micro / 10000);
	if (!app_zcl_custom_s16_reporting_active(endpoint, channel->cluster, 0) &&
	    !app_report_due_s16(state->valid, state->value, state->cycle,
			       value, channel->threshold, cycle, refresh_cycles)) {
		return;
	}

	zb_zcl_status_t status = channel->publish(endpoint, value);
	if (status != ZB_ZCL_STATUS_SUCCESS) {
		LOG_WRN("%s attribute update failed (%u)", channel->name, status);
		return;
	}
	LOG_INF("%s endpoint %u: %d hundredths", channel->name, endpoint, value);
	state->valid = true;
	state->value = value;
	state->cycle = cycle;
}

void app_environment_update_temperature(struct app_environment_report *state,
					const struct device *sensor, uint8_t endpoint,
					uint32_t cycle, uint32_t refresh_cycles)
{
	update_channel(state, &temperature, sensor, endpoint, cycle, refresh_cycles);
}

void app_environment_update(struct app_environment *state,
			    const struct device *sensor, const struct i2c_dt_spec *bus,
			    uint8_t endpoint, uint32_t cycle, uint32_t refresh_cycles)
{
	int err = app_shtc3_sample_fetch(sensor, bus);
	if (err != 0) {
		LOG_WRN("SHTC3 fetch failed (%d); keeping previous values", err);
		return;
	}
	app_environment_update_temperature(&state->temperature, sensor, endpoint, cycle, refresh_cycles);
	update_channel(&state->humidity, &humidity, sensor, endpoint, cycle, refresh_cycles);
}
