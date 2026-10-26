#include <assert.h>
#include <string.h>

#include "app_reset_report.h"

uint8_t test_buffer[64];
zb_zcl_command_send_status_t test_send_status;
zb_uint8_t test_seq;
static bool joined, alloc_ok, send_ok;
static zb_callback_t buffer_cb, sent_cb;
static unsigned int allocs, frees, sends;
static size_t frame_length;
static zb_uint16_t dst_short;
static zb_uint8_t dst_mode, dst_ep, src_ep;
static zb_uint16_t profile, cluster;

bool test_joined(void) { return joined; }
zb_ret_t zb_buf_get_out_delayed(zb_callback_t cb)
{
	allocs++;
	if (!alloc_ok) { return RET_ERROR; }
	buffer_cb = cb;
	return RET_OK;
}
void zb_buf_free(zb_bufid_t buf) { (void)buf; frees++; }
zb_ret_t zb_zcl_finish_and_send_packet(zb_bufid_t buffer, zb_uint8_t *ptr,
				       const zb_addr_u *dst_addr, zb_uint8_t dst_addr_mode,
				       zb_uint8_t dst_endpoint, zb_uint8_t ep,
				       zb_uint16_t prof_id, zb_uint16_t cluster_id,
				       zb_callback_t cb)
{
	(void)buffer;
	sends++;
	frame_length = (size_t)(ptr - test_buffer);
	dst_short = dst_addr->addr_short;
	dst_mode = dst_addr_mode;
	dst_ep = dst_endpoint;
	src_ep = ep;
	profile = prof_id;
	cluster = cluster_id;
	sent_cb = cb;
	return send_ok ? RET_OK : RET_ERROR;
}

static void reset(void)
{
	joined = alloc_ok = send_ok = true;
	buffer_cb = sent_cb = NULL;
	allocs = frees = sends = 0;
	test_seq = 7;
	memset(test_buffer, 0, sizeof(test_buffer));
	app_reset_report_init(1, "sw9 n1");
}

/* Run the delayed allocation and the send confirmation like ZBOSS would. */
static void deliver(zb_ret_t status)
{
	zb_callback_t cb = buffer_cb;

	assert(cb != NULL);
	buffer_cb = NULL;
	cb(3);
	if (sends > 0 && send_ok) {
		test_send_status.status = status;
		sent_cb(3);
	}
}

static void test_report_reaches_coordinator_once(void)
{
	const uint8_t expected[] = {0x18, 7, 0x0a, 0x10, 0x00, 0x42, 6,
				    's', 'w', '9', ' ', 'n', '1'};

	reset();
	app_reset_report_poll();
	assert(allocs == 1);
	/* No second buffer while the first report is in flight. */
	app_reset_report_poll();
	assert(allocs == 1);
	deliver(RET_OK);
	assert(sends == 1 && frees == 1);
	assert(frame_length == sizeof(expected));
	assert(memcmp(test_buffer, expected, sizeof(expected)) == 0);
	assert(dst_short == 0x0000U && dst_mode == ZB_APS_ADDR_MODE_16_ENDP_PRESENT);
	assert(dst_ep == 1 && src_ep == 1);
	assert(profile == ZB_AF_HA_PROFILE_ID && cluster == ZB_ZCL_CLUSTER_ID_BASIC);
	app_reset_report_poll();
	assert(allocs == 1);
}

static void test_waits_until_joined(void)
{
	reset();
	joined = false;
	app_reset_report_poll();
	assert(allocs == 0);
	joined = true;
	app_reset_report_poll();
	assert(allocs == 1);
}

static void test_failures_retry_on_next_poll(void)
{
	reset();
	alloc_ok = false;
	app_reset_report_poll();
	alloc_ok = true;
	app_reset_report_poll();
	assert(allocs == 2);

	/* A frame that could not be queued frees its buffer. */
	send_ok = false;
	deliver(RET_OK);
	assert(sends == 1 && frees == 1);
	send_ok = true;
	app_reset_report_poll();
	assert(allocs == 3);

	/* An undelivered frame, e.g. with the parent gone, is sent again. */
	deliver(RET_ERROR);
	assert(sends == 2 && frees == 2);
	app_reset_report_poll();
	assert(allocs == 4);
	deliver(RET_OK);
	app_reset_report_poll();
	assert(allocs == 4 && sends == 3);
}

int main(void)
{
	test_report_reaches_coordinator_once();
	test_waits_until_joined();
	test_failures_retry_on_next_poll();
	return 0;
}
