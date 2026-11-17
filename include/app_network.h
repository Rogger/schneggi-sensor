#ifndef APP_NETWORK_H_
#define APP_NETWORK_H_

#include <stdbool.h>
#include <zboss_api.h>

/* Apply network actions for a ZBOSS signal on the Zigbee thread. The caller
 * owns the signal buffer. sleepy controls whether CAN_SLEEP puts it to sleep.
 */
void app_network_handle_signal(zb_bufid_t bufid, zb_callback_t sensor_loop, bool sleepy);

#endif
