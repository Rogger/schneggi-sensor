#ifndef ZB_ZCL_TEMP_MEASUREMENT_ADDONS_H_
#define ZB_ZCL_TEMP_MEASUREMENT_ADDONS_H_

#include <zboss_api.h>

#define ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT ((zb_uint16_t)0x0402U)
#define ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT ((zb_uint16_t)0x0405U)

enum zb_zcl_temp_measurement_attr_e {
	ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID = 0x0000,
};

enum zb_zcl_rel_humidity_measurement_attr_e {
	ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID = 0x0000,
};

#endif /* ZB_ZCL_TEMP_MEASUREMENT_ADDONS_H_ */
