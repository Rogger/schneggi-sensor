#ifndef APP_SHTC3_H_
#define APP_SHTC3_H_

#include <zephyr/drivers/i2c.h>

/* NCS 2.9.2 SHTCx driver omits sleep on failed fetches. Preserve the fetch
 * error while attempting bounded, best-effort SHTC3 sleep cleanup.
 */
int app_shtc3_sample_fetch(const struct device *dev,
			  const struct i2c_dt_spec *bus);

#endif
