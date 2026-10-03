#ifndef ZB_ZCL_REPORTING_H_
#define ZB_ZCL_REPORTING_H_

#include <zboss_api.h>

#define ZB_ZCL_NON_MANUFACTURER_SPECIFIC ((zb_uint16_t)0xFFFFU)

typedef struct {
	struct {
		zb_uint8_t endpoint;
	} dst;
} zb_zcl_reporting_info_t;

zb_zcl_reporting_info_t *zb_zcl_find_reporting_info_manuf(
	zb_uint8_t ep, zb_uint16_t cluster_id, zb_uint8_t cluster_role,
	zb_uint16_t attr_id, zb_uint16_t manuf_code);

#endif /* ZB_ZCL_REPORTING_H_ */
