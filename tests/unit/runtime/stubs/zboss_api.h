#ifndef RUNTIME_TEST_ZBOSS_H_
#define RUNTIME_TEST_ZBOSS_H_
#include "../../ota/stubs/zboss_api.h"
#define ZB_BDB_INITIALIZATION 1
#define ZB_BDB_NETWORK_STEERING 2
#define ZB_TRUE 1
#define ZB_FALSE 0
typedef uint8_t zb_bool_t;
enum {
	ZB_BDB_SIGNAL_DEVICE_FIRST_START = 3,
	ZB_BDB_SIGNAL_DEVICE_REBOOT,
	ZB_BDB_SIGNAL_TC_REJOIN_DONE,
	ZB_BDB_SIGNAL_STEERING,
	ZB_COMMON_SIGNAL_CAN_SLEEP,
	ZB_ZDO_SIGNAL_PRODUCTION_CONFIG_READY,
	ZB_NLME_STATUS_INDICATION,
};
#define ZB_NWK_COMMAND_STATUS_PARENT_LINK_FAILURE 1
typedef struct {
	struct { int status; } nlme_status;
} zb_zdo_signal_nlme_status_indication_params_t;
typedef struct {
	zb_zdo_signal_nlme_status_indication_params_t indication;
} zb_zdo_app_signal_hdr_t;
#define ZB_ZDO_SIGNAL_GET_PARAMS(header, type) ((type *)&(header)->indication)
zb_bool_t test_joined(void);
#define ZB_JOINED() test_joined()
zb_bool_t bdb_start_top_level_commissioning(zb_uint8_t mode);
void zb_sleep_now(void);
zb_bool_t zb_get_rx_on_when_idle(void);
#endif
