#ifndef TEST_SENSOR_H_
#define TEST_SENSOR_H_
#include <zephyr/drivers/i2c.h>
enum sensor_channel { SENSOR_CHAN_AMBIENT_TEMP, SENSOR_CHAN_HUMIDITY };
struct sensor_value { int32_t val1; int32_t val2; };
int sensor_channel_get(const struct device *, enum sensor_channel, struct sensor_value *);
static inline int64_t sensor_value_to_micro(const struct sensor_value *value)
{
	return (int64_t)value->val1 * 1000000 + value->val2;
}
#endif
