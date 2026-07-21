#ifndef ZBOSS_API_H_
#define ZBOSS_API_H_

#include <stdbool.h>
#include <stdint.h>

typedef uint8_t zb_bool_t;
typedef uint8_t zb_uint8_t;
typedef uint16_t zb_uint16_t;
typedef uint32_t zb_uint32_t;
typedef int16_t zb_int16_t;
typedef uint8_t zb_ret_t;
typedef uint8_t zb_zcl_status_t;

#define ZB_FALSE ((zb_bool_t)false)
#define ZB_TRUE ((zb_bool_t)true)

#define ZB_ZCL_CLUSTER_SERVER_ROLE ((zb_uint8_t)0U)
#define ZB_ZCL_STATUS_SUCCESS ((zb_zcl_status_t)0U)

zb_zcl_status_t zb_zcl_set_attr_val(zb_uint8_t ep,
				    zb_uint16_t cluster_id,
				    zb_uint8_t cluster_role,
				    zb_uint16_t attr_id,
				    zb_uint8_t *value,
				    zb_bool_t check_access);

#endif /* ZBOSS_API_H_ */
