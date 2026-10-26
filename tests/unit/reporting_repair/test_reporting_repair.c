#include <assert.h>
#include <string.h>

#include "app_reporting_repair.h"

#define TEMP 0x0402U
#define HUMIDITY 0x0405U
#define BUSY ZB_ZCL_REPORTING_SLOT_BUSY

static zb_zcl_attr_t attribute;
static unsigned int nvram_writes;
static zb_ret_t nvram_result;

/* Endpoints 1 and 2 serve measured value 0x0000 of their clusters. */
zb_zcl_attr_t *zb_zcl_get_attr_desc_manuf_a(zb_uint8_t ep, zb_uint16_t cluster_id,
					    zb_uint8_t cluster_role, zb_uint16_t attr_id,
					    zb_uint16_t manuf_code)
{
	(void)cluster_role;
	assert(manuf_code == ZB_ZCL_NON_MANUFACTURER_SPECIFIC);
	if (attr_id != 0x0000U) { return NULL; }
	if (ep == 1 && (cluster_id == TEMP || cluster_id == HUMIDITY)) { return &attribute; }
	if (ep == 2 && cluster_id == TEMP) { return &attribute; }
	return NULL;
}
zb_ret_t zb_nvram_write_dataset(int dataset)
{
	assert(dataset == ZB_NVRAM_ZCL_REPORTING_DATA);
	nvram_writes++;
	return nvram_result;
}

static zb_zcl_reporting_info_t slot(zb_uint8_t ep, zb_uint16_t cluster, zb_uint16_t manuf)
{
	zb_zcl_reporting_info_t s;
	memset(&s, 0, sizeof(s));
	s.direction = ZB_ZCL_CONFIGURE_REPORTING_SEND_REPORT;
	s.ep = ep;
	s.cluster_id = cluster;
	s.flags = BUSY | ZB_ZCL_REPORT_ATTR;
	s.dst.short_addr = 0x0000U;
	s.dst.endpoint = 1;
	s.manuf_code = manuf;
	return s;
}

static unsigned int repair(zb_zcl_reporting_info_t *ep1, zb_uint8_t ep1_count,
			   zb_zcl_reporting_info_t *ep2, zb_uint8_t ep2_count)
{
	zb_af_endpoint_desc_t first = {1, ep1_count, ep1};
	zb_af_endpoint_desc_t second = {2, ep2_count, ep2};
	zb_af_endpoint_desc_t *list[] = {&first, &second};
	zb_af_device_ctx_t device = {2, list};

	return app_reporting_repair(&device);
}

static void reset(void)
{
	nvram_writes = 0;
	nvram_result = RET_OK;
}

static void test_field_slot_with_manufacturer_zero_is_normalized(void)
{
	/* The CO2 sensors' endpoint 2 slot: manufacturer code 0x0000. */
	zb_zcl_reporting_info_t ep1[] = {slot(1, TEMP, ZB_ZCL_NON_MANUFACTURER_SPECIFIC)};
	zb_zcl_reporting_info_t ep2[] = {slot(2, TEMP, 0x0000U)};

	reset();
	assert(repair(ep1, 1, ep2, 1) == 1);
	assert(ep2[0].manuf_code == ZB_ZCL_NON_MANUFACTURER_SPECIFIC);
	/* The binding and pending report survive, so reporting resumes. */
	assert(ep2[0].flags == (BUSY | ZB_ZCL_REPORT_ATTR) && ep2[0].dst.endpoint == 1);
	assert(nvram_writes == 1);
	/* Repaired slots stay as they are on later cycles. */
	assert(repair(ep1, 1, ep2, 1) == 0 && nvram_writes == 1);
}

static void test_healthy_and_unused_slots_are_untouched(void)
{
	zb_zcl_reporting_info_t ep1[] = {
		slot(1, TEMP, ZB_ZCL_NON_MANUFACTURER_SPECIFIC),
		slot(1, HUMIDITY, ZB_ZCL_NON_MANUFACTURER_SPECIFIC),
		slot(1, 0x9999U, 0x0000U),
		slot(1, TEMP, 0x0000U),
	};
	ep1[2].flags = 0;  /* free slot with stale content */
	ep1[3].direction = ZB_ZCL_CONFIGURE_REPORTING_RECV_REPORT;

	reset();
	assert(repair(ep1, 4, NULL, 0) == 0);
	assert(ep1[2].manuf_code == 0x0000U && ep1[3].manuf_code == 0x0000U);
	assert(nvram_writes == 0);
}

static void test_slots_for_missing_attributes_are_released(void)
{
	zb_zcl_reporting_info_t ep2[] = {slot(2, HUMIDITY, ZB_ZCL_NON_MANUFACTURER_SPECIFIC)};

	reset();
	assert(repair(NULL, 0, ep2, 1) == 1);
	assert(ep2[0].flags == 0 && nvram_writes == 1);
}

static void test_duplicate_of_valid_slot_is_released(void)
{
	/* Normalizing would make two slots report the same attribute. */
	zb_zcl_reporting_info_t ep1[] = {
		slot(1, TEMP, 0x0000U),
		slot(1, TEMP, ZB_ZCL_NON_MANUFACTURER_SPECIFIC),
	};

	reset();
	assert(repair(ep1, 2, NULL, 0) == 1);
	assert(ep1[0].flags == 0);
	assert(ep1[1].flags == (BUSY | ZB_ZCL_REPORT_ATTR));
}

static void test_failed_persist_still_repairs_in_memory(void)
{
	zb_zcl_reporting_info_t ep2[] = {slot(2, TEMP, 0x1234U)};

	reset();
	nvram_result = RET_ERROR;
	assert(repair(NULL, 0, ep2, 1) == 1);
	assert(ep2[0].manuf_code == ZB_ZCL_NON_MANUFACTURER_SPECIFIC && nvram_writes == 1);
}

int main(void)
{
	test_field_slot_with_manufacturer_zero_is_normalized();
	test_healthy_and_unused_slots_are_untouched();
	test_slots_for_missing_attributes_are_released();
	test_duplicate_of_valid_slot_is_released();
	test_failed_persist_still_repairs_in_memory();
	return 0;
}
