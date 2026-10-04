#ifndef TEST_GPIO_H_
#define TEST_GPIO_H_
#include <zephyr/drivers/i2c.h>
struct gpio_dt_spec { const struct device *port; };
int gpio_pin_set_dt(const struct gpio_dt_spec *, int);
#endif
