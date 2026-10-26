#include "app_reset_info.h"

#include <zephyr/drivers/hwinfo.h>
#include <hal/nrf_power.h>

/* GPREGRET (index 0) carries Zephyr's sys_reboot() type for bootloaders.
 * GPREGRET2 (index 1) is unused by this application's MCUboot and Zephyr.
 */
#define APP_RESET_GPREGRET 1U

void app_reset_info_load(struct app_reset_info *info)
{
	uint32_t cause = 0U;
	uint8_t retained = (uint8_t)nrf_power_gpregret_get(NRF_POWER, APP_RESET_GPREGRET);

	if (hwinfo_get_reset_cause(&cause) != 0) {
		cause = 0U;
	}
	/* RESETREAS accumulates until cleared; keep only this boot's cause. */
	(void)hwinfo_clear_reset_cause();
	nrf_power_gpregret_set(NRF_POWER, APP_RESET_GPREGRET,
			       app_reset_info_decode(cause, retained, info));
}

void app_reset_info_set_reason(uint8_t reason)
{
	uint8_t retained = (uint8_t)nrf_power_gpregret_get(NRF_POWER, APP_RESET_GPREGRET);

	nrf_power_gpregret_set(NRF_POWER, APP_RESET_GPREGRET,
			       app_reset_info_with_reason(retained, reason));
}
