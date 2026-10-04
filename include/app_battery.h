#ifndef APP_BATTERY_H_
#define APP_BATTERY_H_

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>

struct app_battery {
	const struct adc_dt_spec *adc;
	const struct gpio_dt_spec *enable;
	bool reported;
	uint32_t reported_cycle;
};

/* Called on the Zigbee thread. Failed samples or attribute writes are retried
 * on the next call; only a complete update starts a new reporting interval.
 */
int app_battery_update(struct app_battery *battery, uint8_t endpoint,
		       uint32_t cycle, uint32_t interval_cycles);

#endif
