#include <assert.h>
#include <errno.h>
#include "app_shtc3.h"

static const struct device sensor;
static const struct i2c_dt_spec bus = {&sensor, 0x70};
static int fetch_result, cleanup_result, step;

int sensor_sample_fetch(const struct device *dev)
{
	assert(dev == &sensor);
	assert(step++ == 0);
	return fetch_result;
}

int k_sleep(int duration)
{
	assert(step++ == 1);
	assert(duration == 13);
	return 0;
}

int i2c_write_dt(const struct i2c_dt_spec *spec, const uint8_t *data, size_t size)
{
	assert(step++ == 2);
	assert(spec == &bus);
	assert(size == 2 && data[0] == 0xb0 && data[1] == 0x98);
	return cleanup_result;
}

int main(void)
{
	/* Success adds no sleep delay or redundant command. */
	assert(app_shtc3_sample_fetch(&sensor, &bus) == 0);
	assert(step == 1);
	fetch_result = -EIO;
	step = 0;
	assert(app_shtc3_sample_fetch(&sensor, &bus) == -EIO);
	assert(step == 3);
	/* Cleanup failure neither masks the fetch error nor loops indefinitely. */
	cleanup_result = -ENXIO;
	step = 0;
	assert(app_shtc3_sample_fetch(&sensor, &bus) == -EIO);
	assert(step == 3);
	/* A subsequent successful sample still needs no recovery. */
	fetch_result = 0;
	step = 0;
	assert(app_shtc3_sample_fetch(&sensor, &bus) == 0);
	assert(step == 1);
	return 0;
}
