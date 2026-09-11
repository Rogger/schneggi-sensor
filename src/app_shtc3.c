#include "app_shtc3.h"
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app_shtc3, LOG_LEVEL_WRN);

int app_shtc3_sample_fetch(const struct device *dev,
			  const struct i2c_dt_spec *bus)
{
	int err = sensor_sample_fetch(dev);

	if (err != 0) {
		/* A failed command transfer may still have started a conversion.
		 * Wait beyond the 12.1 ms normal-mode maximum before sending sleep.
		 * SHTC3 datasheet: https://sensirion.com/file/datasheet_shtc3
		 */
		static const uint8_t sleep_command[] = {0xb0, 0x98};
		k_sleep(K_MSEC(13));
		int cleanup_err = i2c_write_dt(bus, sleep_command, sizeof(sleep_command));
		if (cleanup_err != 0) {
			LOG_WRN("SHTC3 sleep cleanup failed (%d)", cleanup_err);
		}
	}
	return err;
}
