#ifndef RUNTIME_TEST_ZBOSS_H_
#define RUNTIME_TEST_ZBOSS_H_
#include "../../ota/stubs/zboss_api.h"
#define ZB_BDB_INITIALIZATION 1
#define ZB_BDB_NETWORK_STEERING 2
#define ZB_TRUE 1
#define ZB_FALSE 0
typedef uint8_t zb_bool_t;
zb_bool_t test_joined(void);
#define ZB_JOINED() test_joined()
zb_bool_t bdb_start_top_level_commissioning(zb_uint8_t mode);
void zb_sleep_now(void);
#endif
