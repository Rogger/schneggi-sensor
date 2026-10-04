#include <assert.h>
#include <errno.h>
#include <string.h>
#include "app_environment.h"
#include "app_shtc3.h"
#include "app_zcl_report.h"
#include <zephyr/drivers/sensor.h>
#include <zcl/zb_zcl_reporting.h>
#include <zcl/zb_zcl_temp_measurement_addons.h>

static const struct device device;
static const struct i2c_dt_spec bus = { .bus = &device, .addr = 0x70 };
static struct sensor_value samples[2];
static int fetch_status, read_status[2];
static unsigned int reads[2], writes[2], fetches;
static int16_t values[2];
static zb_zcl_status_t write_status[2];
static zb_zcl_reporting_info_t reporting[2];

static unsigned int channel_index(zb_uint16_t cluster)
{
	assert(cluster == ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT ||
	       cluster == ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT);
	return cluster == ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT ? 0 : 1;
}

int app_shtc3_sample_fetch(const struct device *dev, const struct i2c_dt_spec *spec)
{
	assert(dev == &device && spec == &bus);
	fetches++;
	return fetch_status;
}
int sensor_channel_get(const struct device *dev, enum sensor_channel channel,
		       struct sensor_value *sample)
{
	assert(dev == &device);
	unsigned int index = channel == SENSOR_CHAN_AMBIENT_TEMP ? 0 : 1;
	reads[index]++;
	*sample = samples[index];
	return read_status[index];
}
zb_zcl_reporting_info_t *zb_zcl_find_reporting_info_manuf(
	zb_uint8_t ep, zb_uint16_t cluster, zb_uint8_t role, zb_uint16_t attr, zb_uint16_t manuf)
{
	assert(ep == 7 && role == ZB_ZCL_CLUSTER_SERVER_ROLE && attr == 0);
	assert(manuf == ZB_ZCL_NON_MANUFACTURER_SPECIFIC);
	return &reporting[channel_index(cluster)];
}
zb_zcl_status_t zb_zcl_set_attr_val(zb_uint8_t ep, zb_uint16_t cluster,
	zb_uint8_t role, zb_uint16_t attr, zb_uint8_t *value, zb_bool_t check)
{
	assert(ep == 7 && role == ZB_ZCL_CLUSTER_SERVER_ROLE && attr == 0 && check == ZB_FALSE);
	unsigned int index = channel_index(cluster);
	writes[index]++;
	memcpy(&values[index], value, sizeof(int16_t));
	return write_status[index];
}

static void reset(void)
{
	fetch_status = 0;
	fetches = 0;
	memset(read_status, 0, sizeof(read_status));
	memset(write_status, 0, sizeof(write_status));
	memset(reads, 0, sizeof(reads));
	memset(writes, 0, sizeof(writes));
	memset(reporting, 0, sizeof(reporting));
	for (unsigned int i = 0; i < 2; ++i) {
		reporting[i].dst.endpoint = 1;
		reporting[i].u.send_info.min_interval = 5;
		reporting[i].u.send_info.def_min_interval = 5;
	}
	samples[0] = (struct sensor_value){20, 150000};
	samples[1] = (struct sensor_value){50, 290000};
}
static void update(struct app_environment *state, uint32_t cycle)
{
	app_environment_update(state, &device, &bus, 7, cycle, 144);
}

static void test_fallback_thresholds_and_refresh(void)
{
	struct app_environment state = {0};
	reset();
	update(&state, 0);
	assert(fetches == 1 && writes[0] == 1 && writes[1] == 1);
	assert(values[0] == 2015 && values[1] == 5029);
	samples[0].val2 = 240000;
	samples[1].val1 = 51;
	samples[1].val2 = 280000;
	update(&state, 1);
	assert(writes[0] == 1 && writes[1] == 1);
	samples[0].val2 = 250000;
	samples[1].val2 = 290000;
	update(&state, 2);
	assert(writes[0] == 2 && writes[1] == 2);
	update(&state, 145);
	assert(writes[0] == 2 && writes[1] == 2);
	update(&state, 146);
	assert(writes[0] == 3 && writes[1] == 3);
}

static void test_failed_fetch_and_independent_channel_reads(void)
{
	struct app_environment state = {0};
	reset();
	fetch_status = -EIO;
	update(&state, 0);
	assert(reads[0] == 0 && reads[1] == 0 && !state.temperature.valid && !state.humidity.valid);
	fetch_status = 0;
	read_status[0] = -EIO;
	update(&state, 1);
	assert(writes[0] == 0 && writes[1] == 1);
	assert(!state.temperature.valid && state.humidity.valid);
	read_status[0] = 0;
	read_status[1] = -EIO;
	update(&state, 2);
	assert(writes[0] == 1 && writes[1] == 1);
	assert(state.temperature.cycle == 2 && state.humidity.cycle == 1);
}

static void test_failed_attribute_write_retries_without_advancing_baseline(void)
{
	struct app_environment state = {0};
	reset();
	write_status[0] = 1;
	update(&state, 0);
	assert(!state.temperature.valid && state.humidity.valid);
	write_status[0] = 0;
	update(&state, 1);
	assert(writes[0] == 2 && state.temperature.cycle == 1);
	write_status[0] = write_status[1] = 1;
	samples[0].val1++;
	samples[1].val1++;
	update(&state, 2);
	assert(state.temperature.value == 2015 && state.humidity.value == 5029);
	assert(state.temperature.cycle == 1 && state.humidity.cycle == 0);
	write_status[0] = write_status[1] = 0;
	update(&state, 3);
	assert(state.temperature.value == 2115 && state.humidity.value == 5129);
	assert(state.temperature.cycle == 3 && state.humidity.cycle == 3);
}

static void test_custom_reporting_and_reset_are_independent(void)
{
	struct app_environment state = {0};
	reset();
	update(&state, 0);
	reporting[0].u.send_info.delta.s16 = 1;
	samples[0].val2 += 10000;
	samples[1].val2 += 10000;
	update(&state, 1);
	assert(writes[0] == 2 && writes[1] == 1);
	assert(values[0] == 2016);
	/* Restoring defaults retains the destination endpoint in ZBOSS. */
	reporting[0].u.send_info.delta.s16 = 0;
	reporting[1].u.send_info.max_interval = 3600;
	samples[0].val2 += 10000;
	update(&state, 2);
	assert(writes[0] == 2 && writes[1] == 2 && values[1] == 5030);
	reporting[1].u.send_info.max_interval = 0;
	update(&state, 3);
	assert(writes[0] == 2 && writes[1] == 2);
}

static void test_fixed_point_conversion_and_bounds(void)
{
	struct app_environment state = {0};
	reset();
	reporting[0].u.send_info.delta.s16 = 1;
	/* Every exact hundredth across the SHTC3 temperature range is preserved. */
	for (int centi = -4500; centi <= 13000; ++centi) {
		samples[0] = (struct sensor_value){centi / 100, (centi % 100) * 10000};
		update(&state, 0);
		assert(values[0] == centi);
	}
	const struct sensor_value invalid[][2] = {
		{{INT32_MAX, 0}, {101, 0}}, {{INT32_MIN, 0}, {0, -1}},
		{{327, 670001}, {INT32_MAX, 0}}, {{-273, -150001}, {INT32_MIN, 0}},
	};
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
		unsigned int before_temp = writes[0], before_humidity = writes[1];
		samples[0] = invalid[i][0];
		samples[1] = invalid[i][1];
		update(&state, 144);
		assert(writes[0] == before_temp && writes[1] == before_humidity);
	}
	samples[0] = (struct sensor_value){-273, -150000};
	samples[1] = (struct sensor_value){100, 0};
	update(&state, 145);
	assert(values[0] == -27315 && values[1] == 10000);
	samples[0] = (struct sensor_value){327, 670000};
	samples[1] = (struct sensor_value){0, 0};
	update(&state, 146);
	assert(values[0] == INT16_MAX && values[1] == 0);
}

int main(void)
{
	test_fallback_thresholds_and_refresh();
	test_failed_fetch_and_independent_channel_reads();
	test_failed_attribute_write_retries_without_advancing_baseline();
	test_custom_reporting_and_reset_are_independent();
	test_fixed_point_conversion_and_bounds();
	return 0;
}
