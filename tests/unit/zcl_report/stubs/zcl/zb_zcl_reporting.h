#ifndef ZB_ZCL_REPORTING_H_
#define ZB_ZCL_REPORTING_H_

#include <zboss_api.h>

#define ZB_ZCL_NON_MANUFACTURER_SPECIFIC ((zb_uint16_t)0xFFFFU)
#define ZB_ZCL_CONFIGURE_REPORTING_SEND_REPORT 0U
#define ZB_ZCL_CONFIGURE_REPORTING_RECV_REPORT 1U

typedef struct {
	zb_uint8_t direction;
	struct {
		zb_uint8_t endpoint;
	} dst;
	union {
		struct {
			zb_uint16_t min_interval;
			zb_uint16_t max_interval;
			union {
				zb_int16_t s16;
			} delta;
			zb_uint16_t def_min_interval;
			zb_uint16_t def_max_interval;
		} send_info;
	} u;
} zb_zcl_reporting_info_t;

zb_zcl_reporting_info_t *zb_zcl_find_reporting_info_manuf(
	zb_uint8_t ep, zb_uint16_t cluster_id, zb_uint8_t cluster_role,
	zb_uint16_t attr_id, zb_uint16_t manuf_code);

#endif /* ZB_ZCL_REPORTING_H_ */
