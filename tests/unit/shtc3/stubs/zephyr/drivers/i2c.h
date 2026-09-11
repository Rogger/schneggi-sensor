#ifndef TEST_I2C_H
#define TEST_I2C_H
#include <stddef.h>
#include <stdint.h>
struct device { int unused; };
struct i2c_dt_spec { const struct device *bus; uint16_t addr; };
int i2c_write_dt(const struct i2c_dt_spec *bus, const uint8_t *data, size_t size);
#endif
