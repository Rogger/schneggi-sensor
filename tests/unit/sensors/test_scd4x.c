#include <assert.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include "app_scd4x.h"
#include "app_shtc3.h"
#include "app_zcl_report.h"
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/scd4x.h>
#include <zephyr/sys/crc.h>
#include <zcl/zb_zcl_reporting.h>
#include <zcl/zb_zcl_temp_measurement_addons.h>
#include "zcl/zb_zcl_concentration_measurement.h"

static const struct device scd, sht;
static const struct i2c_dt_spec bus = { .bus = &sht, .addr = 0x70 };
static const struct i2c_dt_spec scd_bus = { .bus = &scd, .addr = 0x62 };
static struct sensor_value scd_sample[3], sht_sample[2];
static int fetch_status, read_status[3];
static unsigned int fetches, reads[3], temp_writes[3], co2_writes;
static int16_t temperatures[3];
static float co2;
static zb_zcl_status_t co2_status, temp_status;
static zb_zcl_reporting_info_t reporting[3];
static unsigned int setting_calls, setting_failure;
static unsigned int warmups;
static int64_t uptime_ms;

int k_sleep(int duration)
{
	assert((duration == 1 || duration == 100) && setting_calls == 3 && setting_failure == 0);
	uptime_ms += duration;
	if (duration == 100) {
		warmups++;
	}
	return 0;
}

int64_t k_uptime_get(void)
{
	return uptime_ms;
}

int i2c_write_dt(const struct i2c_dt_spec *spec, const uint8_t *data, size_t size)
{
	assert(spec == &scd_bus && size == 2 && data[0] == 0xe4 && data[1] == 0xb8);
	assert(setting_calls == 3 && setting_failure == 0);
	return 0;
}

int i2c_read_dt(const struct i2c_dt_spec *spec, uint8_t *data, size_t size)
{
	assert(spec == &scd_bus && size == 3 && warmups <= 1);
	/* Upper status bits are set even when the low 11 readiness bits are zero. */
	data[0] = 0x80;
	data[1] = warmups ? 1 : 0;
	data[2] = warmups ? 0x93 : 0xa2;
	return 0;
}

uint8_t crc8(const uint8_t *src, size_t len, uint8_t polynomial,
	     uint8_t initial_value, bool reversed)
{
	assert(len == 2 && polynomial == 0x31 && initial_value == 0xff && !reversed);
	assert(src[0] == 0x80 && src[1] <= 1);
	return src[1] ? 0x93 : 0xa2;
}

int sensor_attr_set(const struct device *sensor, enum sensor_channel channel,
		    enum sensor_attribute attribute, const struct sensor_value *value)
{
	assert(sensor == &scd && value->val2 == 0);
	const int attributes[] = { SENSOR_ATTR_SCD4X_SENSOR_ALTITUDE,
		SENSOR_ATTR_SCD4X_TEMPERATURE_OFFSET, SENSOR_ATTR_SCD4X_AUTOMATIC_CALIB_ENABLE };
	const int values[] = {574, 0, 1};
	assert(setting_calls < 3 && (int)attribute == attributes[setting_calls]);
	assert(value->val1 == values[setting_calls]);
	assert(channel == (setting_calls == 1 ? SENSOR_CHAN_AMBIENT_TEMP : SENSOR_CHAN_CO2));
	setting_calls++;
	return setting_failure == setting_calls ? -EIO : 0;
}

static void test_calibration_setup(void)
{
	setting_calls = setting_failure = 0;
	warmups = 0;
	uptime_ms = 0;
	assert(app_scd4x_init(&scd, &scd_bus) == 0 && setting_calls == 3 && warmups == 1);
	assert(fetches == 1);
	for (setting_failure = 1; setting_failure <= 3; setting_failure++) {
		setting_calls = 0;
		warmups = 0;
		assert(app_scd4x_init(&scd, &scd_bus) == -EIO && setting_calls == setting_failure);
		assert(warmups == 0);
	}
}

int sensor_sample_fetch(const struct device *sensor)
{
	assert(sensor == &scd);
	fetches++;
	return fetch_status;
}

int app_shtc3_sample_fetch(const struct device *sensor, const struct i2c_dt_spec *spec)
{
	/* The SCD4x path must never send SHTC3 wake/sleep commands. */
	assert(sensor == &sht && spec == &bus);
	return 0;
}

int sensor_channel_get(const struct device *sensor, enum sensor_channel channel,
		       struct sensor_value *sample)
{
	assert(channel <= SENSOR_CHAN_CO2);
	if (sensor == &scd) {
		assert(channel != SENSOR_CHAN_HUMIDITY);
		reads[channel]++;
		*sample = scd_sample[channel];
		return read_status[channel];
	}
	assert(sensor == &sht && channel <= SENSOR_CHAN_HUMIDITY);
	*sample = sht_sample[channel];
	return 0;
}

zb_zcl_reporting_info_t *zb_zcl_find_reporting_info_manuf(
	zb_uint8_t ep, zb_uint16_t cluster, zb_uint8_t role, zb_uint16_t attr, zb_uint16_t manuf)
{
	assert((ep == 1 || ep == 2) && role == ZB_ZCL_CLUSTER_SERVER_ROLE && attr == 0);
	assert(manuf == ZB_ZCL_NON_MANUFACTURER_SPECIFIC);
	if (cluster == ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT) {
		assert(ep == 1);
		return NULL;
	}
	assert(cluster == ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT);
	return &reporting[ep];
}

zb_zcl_status_t zb_zcl_set_attr_val(zb_uint8_t ep, zb_uint16_t cluster,
	zb_uint8_t role, zb_uint16_t attr, zb_uint8_t *value, zb_bool_t check)
{
	assert(role == ZB_ZCL_CLUSTER_SERVER_ROLE && attr == 0 && check == ZB_FALSE);
	if (cluster == ZB_ZCL_CLUSTER_ID_CONCENTRATION_MEASUREMENT) {
		assert(ep == 1);
		co2_writes++;
		memcpy(&co2, value, sizeof(co2));
		return co2_status;
	}
	if (cluster == ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT) {
		assert(ep == 1);
		return ZB_ZCL_STATUS_SUCCESS;
	}
	assert(cluster == ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT && (ep == 1 || ep == 2));
	temp_writes[ep]++;
	memcpy(&temperatures[ep], value, sizeof(int16_t));
	return temp_status;
}

static void reset(void)
{
	fetch_status = co2_status = temp_status = 0;
	fetches = co2_writes = 0;
	memset(read_status, 0, sizeof(read_status));
	memset(reads, 0, sizeof(reads));
	memset(temp_writes, 0, sizeof(temp_writes));
	memset(reporting, 0, sizeof(reporting));
	scd_sample[SENSOR_CHAN_AMBIENT_TEMP] = (struct sensor_value){25, 150000};
	scd_sample[SENSOR_CHAN_CO2] = (struct sensor_value){1177, 0};
	sht_sample[SENSOR_CHAN_AMBIENT_TEMP] = (struct sensor_value){20, 150000};
	sht_sample[SENSOR_CHAN_HUMIDITY] = (struct sensor_value){50, 0};
}

static void update(struct app_environment_report *state, uint32_t cycle)
{
	app_scd4x_update(state, &scd, 1, 2, cycle, 144);
}

static void test_two_temperatures_and_one_fetch(void)
{
	reset();
	struct app_environment ambient = {0};
	struct app_environment_report temperature = {0};
	app_environment_update(&ambient, &sht, &bus, 1, 0, 144);
	update(&temperature, 0);
	assert(fetches == 1 && reads[SENSOR_CHAN_CO2] == 1 && reads[SENSOR_CHAN_AMBIENT_TEMP] == 1);
	assert(co2_writes == 1 && fabsf(co2 - 0.001177f) < 0.0000001f);
	assert(temp_writes[1] == 1 && temp_writes[2] == 1);
	assert(ambient.temperature.value == 2015 && temperature.value == 2515);
	assert(temperatures[1] == 2015 && temperatures[2] == 2515);
}

static void test_errors_are_independent_and_preserve_previous_state(void)
{
	reset();
	struct app_environment_report temperature = {0};
	fetch_status = -EIO;
	update(&temperature, 0);
	assert(!temperature.valid && co2_writes == 0 && temp_writes[2] == 0);
	assert(reads[SENSOR_CHAN_CO2] == 0 && reads[SENSOR_CHAN_AMBIENT_TEMP] == 0);
	fetch_status = 0;
	read_status[SENSOR_CHAN_CO2] = -EIO;
	update(&temperature, 1);
	assert(temperature.valid && temperature.cycle == 1 && co2_writes == 0);
	read_status[SENSOR_CHAN_CO2] = 0;
	read_status[SENSOR_CHAN_AMBIENT_TEMP] = -EIO;
	update(&temperature, 2);
	assert(co2_writes == 1 && temp_writes[2] == 1 && temperature.cycle == 1);
	read_status[SENSOR_CHAN_AMBIENT_TEMP] = 0;
	scd_sample[SENSOR_CHAN_AMBIENT_TEMP].val1++;
	co2_status = temp_status = 1;
	update(&temperature, 3);
	assert(temperature.value == 2515 && temperature.cycle == 1);
	temp_status = 0;
	update(&temperature, 4);
	assert(temperature.value == 2615 && temperature.cycle == 4);
	fetch_status = -EAGAIN;
	update(&temperature, 5);
	assert(temperature.value == 2615 && temperature.cycle == 4);
}

static void test_threshold_refresh_custom_reporting_and_range(void)
{
	reset();
	struct app_environment_report temperature = {0};
	update(&temperature, 0);
	scd_sample[SENSOR_CHAN_AMBIENT_TEMP].val2 = 240000;
	update(&temperature, 1);
	assert(temp_writes[2] == 1);
	scd_sample[SENSOR_CHAN_AMBIENT_TEMP].val2 = 250000;
	update(&temperature, 2);
	assert(temp_writes[2] == 2 && temperature.value == 2525);
	update(&temperature, 145);
	assert(temp_writes[2] == 2);
	update(&temperature, 146);
	assert(temp_writes[2] == 3);
	reporting[2].dst.endpoint = 1;
	reporting[2].u.send_info.delta.s16 = 1;
	scd_sample[SENSOR_CHAN_AMBIENT_TEMP] = (struct sensor_value){-5, -120000};
	update(&temperature, 147);
	assert(temp_writes[2] == 4 && temperature.value == -512);
	scd_sample[SENSOR_CHAN_AMBIENT_TEMP].val2 = -130000;
	update(&temperature, 148);
	assert(temp_writes[2] == 5 && temperature.value == -513);
	/* The SCD4x driver uses a negative integral part with a positive fraction. */
	scd_sample[SENSOR_CHAN_AMBIENT_TEMP] = (struct sensor_value){-5, 120000};
	update(&temperature, 149);
	assert(temp_writes[2] == 6 && temperature.value == -488);
	scd_sample[SENSOR_CHAN_AMBIENT_TEMP] = (struct sensor_value){INT32_MAX, 0};
	update(&temperature, 150);
	assert(temp_writes[2] == 6 && temperature.value == -488 && temperature.cycle == 149);
	assert(co2_writes == 9);
}

static void test_co2_clamping_preserved(void)
{
	reset();
	struct app_environment_report temperature = {0};
	scd_sample[SENSOR_CHAN_CO2] = (struct sensor_value){-1, 0};
	update(&temperature, 0);
	assert(co2 == 0.0f && temperature.valid);
	scd_sample[SENSOR_CHAN_CO2] = (struct sensor_value){INT32_MAX, 0};
	update(&temperature, 1);
	assert(co2 == 1.0f);
}

int main(void)
{
	test_calibration_setup();
	test_two_temperatures_and_one_fetch();
	test_errors_are_independent_and_preserve_previous_state();
	test_threshold_refresh_custom_reporting_and_range();
	test_co2_clamping_preserved();
	return 0;
}
