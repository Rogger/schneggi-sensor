#include "app_reset_report.h"
#include "app_reset_info.h"

#include <string.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app_reset_report, LOG_LEVEL_INF);

/* ZHA and other coordinators serve the Home Automation profile on endpoint 1. */
#define COORDINATOR_ENDPOINT 1U

/* All functions run on the Zigbee thread. */
static enum { REPORT_IDLE, REPORT_PENDING, REPORT_DONE } report_state;
static zb_uint8_t report_endpoint;
static const char *report_text;

static void report_sent(zb_bufid_t bufid)
{
	const zb_zcl_command_send_status_t *sent =
		ZB_BUF_GET_PARAM(bufid, zb_zcl_command_send_status_t);

	if (sent->status == RET_OK) {
		LOG_INF("Reported last reset: %s", report_text);
		report_state = REPORT_DONE;
	} else {
		LOG_WRN("Reset report not delivered (%d); retrying", sent->status);
		report_state = REPORT_IDLE;
	}
	zb_buf_free(bufid);
}

static void send_report(zb_bufid_t bufid)
{
	uint8_t record[APP_RESET_INFO_REPORT_SIZE];
	size_t length = app_reset_info_report_record(report_text, record, sizeof(record));
	zb_addr_u coordinator = { .addr_short = 0x0000U };
	zb_uint8_t *ptr;

	if (length == 0U) {
		zb_buf_free(bufid);
		report_state = REPORT_DONE;
		return;
	}

	ptr = ZB_ZCL_START_PACKET(bufid);
	ZB_ZCL_CONSTRUCT_GENERAL_COMMAND_RESP_FRAME_CONTROL(ptr);
	ZB_ZCL_CONSTRUCT_COMMAND_HEADER(ptr, ZB_ZCL_GET_SEQ_NUM(), ZB_ZCL_CMD_REPORT_ATTRIB);
	memcpy(ptr, record, length);
	ptr += length;

	/* The callback owns the buffer only if the frame was queued. */
	if (zb_zcl_finish_and_send_packet(bufid, ptr, &coordinator,
					  ZB_APS_ADDR_MODE_16_ENDP_PRESENT, COORDINATOR_ENDPOINT,
					  report_endpoint, ZB_AF_HA_PROFILE_ID,
					  ZB_ZCL_CLUSTER_ID_BASIC, report_sent) != RET_OK) {
		zb_buf_free(bufid);
		report_state = REPORT_IDLE;
	}
}

void app_reset_report_init(zb_uint8_t endpoint, const char *text)
{
	report_endpoint = endpoint;
	report_text = text;
	report_state = REPORT_IDLE;
}

void app_reset_report_poll(void)
{
	if (report_state != REPORT_IDLE || report_text == NULL || !ZB_JOINED()) {
		return;
	}
	report_state = REPORT_PENDING;
	if (zb_buf_get_out_delayed(send_report) != RET_OK) {
		report_state = REPORT_IDLE;
	}
}
