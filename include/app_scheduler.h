#ifndef APP_SCHEDULER_H_
#define APP_SCHEDULER_H_

#include <stdint.h>
#include <zboss_api.h>

/* On the Zigbee thread, replace all alarms for callback with one new alarm.
 * Delays are milliseconds, including sampling intervals longer than 71 minutes.
 * The caller must handle an error: no replacement was scheduled.
 */
zb_ret_t app_alarm_replace(zb_callback_t callback, zb_uint8_t param, uint32_t delay_ms);

#endif
