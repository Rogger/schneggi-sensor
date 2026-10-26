#ifndef REPORTING_REPAIR_TEST_ZBOSS_H_
#define REPORTING_REPAIR_TEST_ZBOSS_H_
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define LOG_MODULE_REGISTER(...)
#define LOG_WRN(...)
#define LOG_LEVEL_INF 3
#define RET_OK 0
#define RET_ERROR -1
#define ZB_ZCL_NON_MANUFACTURER_SPECIFIC 0xFFFFU
#define ZB_ZCL_CONFIGURE_REPORTING_SEND_REPORT 0x00U
#define ZB_ZCL_CONFIGURE_REPORTING_RECV_REPORT 0x01U
#define ZB_NVRAM_ZCL_REPORTING_DATA 3
typedef uint8_t zb_uint8_t;
typedef uint16_t zb_uint16_t;
typedef int zb_ret_t;
typedef struct { int unused; } zb_zcl_attr_t;
struct zb_zcl_reporting_info_s;
typedef struct {
	zb_uint8_t ep;
	zb_uint8_t rep_info_count;
	struct zb_zcl_reporting_info_s *reporting_info;
} zb_af_endpoint_desc_t;
typedef struct {
	zb_uint8_t ep_count;
	zb_af_endpoint_desc_t **ep_desc_list;
} zb_af_device_ctx_t;
zb_zcl_attr_t *zb_zcl_get_attr_desc_manuf_a(zb_uint8_t ep, zb_uint16_t cluster_id,
					    zb_uint8_t cluster_role, zb_uint16_t attr_id,
					    zb_uint16_t manuf_code);
zb_ret_t zb_nvram_write_dataset(int dataset);
#endif
