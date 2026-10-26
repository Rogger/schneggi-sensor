#ifndef RESET_REPORT_TEST_ZBOSS_H_
#define RESET_REPORT_TEST_ZBOSS_H_
#include <stdbool.h>
#include <stdint.h>
#define LOG_MODULE_REGISTER(...)
#define LOG_INF(...)
#define LOG_WRN(...)
#define LOG_LEVEL_INF 3
#define RET_OK 0
#define RET_ERROR -1
#define ZB_APS_ADDR_MODE_16_ENDP_PRESENT 0x02U
#define ZB_AF_HA_PROFILE_ID 0x0104U
#define ZB_ZCL_CLUSTER_ID_BASIC 0x0000U
#define ZB_ZCL_CMD_REPORT_ATTRIB 0x0aU
typedef uint8_t zb_uint8_t;
typedef uint16_t zb_uint16_t;
typedef uint8_t zb_bufid_t;
typedef int zb_ret_t;
typedef void (*zb_callback_t)(zb_uint8_t);
typedef union { zb_uint16_t addr_short; uint8_t addr_long[8]; } zb_addr_u;
typedef struct { zb_ret_t status; } zb_zcl_command_send_status_t;

extern uint8_t test_buffer[64];
extern zb_zcl_command_send_status_t test_send_status;
extern zb_uint8_t test_seq;
bool test_joined(void);
#define ZB_JOINED() test_joined()
zb_ret_t zb_buf_get_out_delayed(zb_callback_t cb);
void zb_buf_free(zb_bufid_t buf);
#define ZB_BUF_GET_PARAM(buf, type) ((void)(buf), (type *)&test_send_status)
#define ZB_ZCL_START_PACKET(buf) ((void)(buf), test_buffer)
/* Frame control for a general server-to-client command without default response. */
#define ZB_ZCL_CONSTRUCT_GENERAL_COMMAND_RESP_FRAME_CONTROL(ptr) (*(ptr)++ = 0x18U)
#define ZB_ZCL_CONSTRUCT_COMMAND_HEADER(ptr, tsn, cmd) (*(ptr)++ = (tsn), *(ptr)++ = (cmd))
#define ZB_ZCL_GET_SEQ_NUM() (test_seq++)
zb_ret_t zb_zcl_finish_and_send_packet(zb_bufid_t buffer, zb_uint8_t *ptr,
				       const zb_addr_u *dst_addr, zb_uint8_t dst_addr_mode,
				       zb_uint8_t dst_ep, zb_uint8_t ep,
				       zb_uint16_t prof_id, zb_uint16_t cluster_id,
				       zb_callback_t cb);
#endif
