#include "app_scheduler.h"

zb_ret_t app_alarm_replace(zb_callback_t callback, zb_uint8_t param, uint32_t delay_ms)
{
	zb_ret_t ret = ZB_SCHEDULE_APP_ALARM_CANCEL(callback, ZB_ALARM_ALL_CB);
	if (ret != RET_OK && ret != RET_NOT_FOUND) {
		return ret;
	}

	/* The SDK macro multiplies in 32 bits and overflows above ~71 minutes.
	 * A uint32_t millisecond delay fits in zb_time_t after this conversion.
	 */
	zb_time_t delay = (zb_time_t)(((uint64_t)delay_ms * 1000U +
				      ZB_BEACON_INTERVAL_USEC - 1U) / ZB_BEACON_INTERVAL_USEC);
	return ZB_SCHEDULE_APP_ALARM(callback, param, delay);
}
