#ifndef APP_NETWORK_H_
#define APP_NETWORK_H_

#include <zboss_api.h>
#include "zigbee_signal_logic.h"

/* Apply a decoded signal on the Zigbee thread. sensor_loop is started after
 * joining; sleepy selects whether CAN_SLEEP may put the device to sleep.
 */
void app_network_handle_signal(enum app_zigbee_signal signal, bool status_ok,
			       bool parent_link_failure, zb_callback_t sensor_loop, bool sleepy);

#endif
