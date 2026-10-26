#ifndef REPORTING_REPAIR_TEST_ZCL_REPORTING_H_
#define REPORTING_REPAIR_TEST_ZCL_REPORTING_H_
/* The fields of ZBOSS's zb_zcl_reporting_info_t that the repair reads. */
typedef struct zb_zcl_reporting_info_s {
	zb_uint8_t direction;
	zb_uint8_t ep;
	zb_uint16_t cluster_id;
	zb_uint8_t cluster_role;
	zb_uint16_t attr_id;
	zb_uint8_t flags;
	struct { zb_uint16_t short_addr; zb_uint8_t endpoint; zb_uint16_t profile_id; } dst;
	zb_uint16_t manuf_code;
} zb_zcl_reporting_info_t;
enum { ZB_ZCL_REPORTING_SLOT_BUSY = 0x01, ZB_ZCL_REPORT_ATTR = 0x02 };
#define ZB_ZCL_GET_REPORTING_FLAG(rep_info, f) ((rep_info)->flags & (zb_uint8_t)(f))
#define ZB_ZCL_CLR_ALL_REPORTING_FLAGS(rep_info) ((rep_info)->flags = 0)
#endif
