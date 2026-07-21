#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_zcl_report.h"

#include <zcl/zb_zcl_power_config.h>
#include <zcl/zb_zcl_temp_measurement_addons.h>

#include "zcl/zb_zcl_concentration_measurement.h"

struct zcl_set_attr_call {
	zb_uint8_t endpoint;
	zb_uint16_t cluster_id;
	zb_uint8_t cluster_role;
	zb_uint16_t attr_id;
	zb_bool_t check_access;
	uint8_t value[sizeof(float)];
	size_t value_size;
};

static struct zcl_set_attr_call last_call;
static zb_zcl_status_t next_status = ZB_ZCL_STATUS_SUCCESS;

zb_zcl_status_t zb_zcl_set_attr_val(zb_uint8_t ep,
				    zb_uint16_t cluster_id,
				    zb_uint8_t cluster_role,
				    zb_uint16_t attr_id,
				    zb_uint8_t *value,
				    zb_bool_t check_access)
{
	last_call.endpoint = ep;
	last_call.cluster_id = cluster_id;
	last_call.cluster_role = cluster_role;
	last_call.attr_id = attr_id;
	last_call.check_access = check_access;

	if (cluster_id == ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT ||
	    cluster_id == ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT) {
		last_call.value_size = sizeof(int16_t);
	} else if (cluster_id == ZB_ZCL_CLUSTER_ID_CONCENTRATION_MEASUREMENT) {
		last_call.value_size = sizeof(float);
	} else {
		last_call.value_size = sizeof(uint8_t);
	}

	memcpy(last_call.value, value, last_call.value_size);
	return next_status;
}

static void reset_mock(zb_zcl_status_t status)
{
	memset(&last_call, 0, sizeof(last_call));
	next_status = status;
}

static void assert_common_mapping(zb_uint8_t endpoint,
				  zb_uint16_t cluster_id,
				  zb_uint16_t attr_id)
{
	assert(last_call.endpoint == endpoint);
	assert(last_call.cluster_id == cluster_id);
	assert(last_call.cluster_role == ZB_ZCL_CLUSTER_SERVER_ROLE);
	assert(last_call.attr_id == attr_id);
	assert(last_call.check_access == ZB_FALSE);
}

static void test_zcl_id_constants_match_spec_values(void)
{
	assert(ZB_ZCL_CLUSTER_ID_POWER_CONFIG == 0x0001U);
	assert(ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT == 0x0402U);
	assert(ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT == 0x0405U);
	assert(ZB_ZCL_CLUSTER_ID_CONCENTRATION_MEASUREMENT == 0x040DU);
	assert(ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID == 0x0000U);
	assert(ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID == 0x0000U);
	assert(ZB_ZCL_ATTR_CONCENTRATION_MEASUREMENT_VALUE_ID == 0x0000U);
	assert(ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID == 0x0020U);
	assert(ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID == 0x0021U);
}

static int16_t captured_i16(void)
{
	int16_t value;

	memcpy(&value, last_call.value, sizeof(value));
	return value;
}

static float captured_float(void)
{
	float value;

	memcpy(&value, last_call.value, sizeof(value));
	return value;
}

static void test_temperature_mapping(void)
{
	const zb_uint8_t endpoint = 11U;
	const int16_t value = -1234;
	const zb_zcl_status_t status = 0x42U;

	reset_mock(status);

	assert(app_zcl_report_temperature(endpoint, value) == status);
	assert_common_mapping(endpoint,
			      ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
			      ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID);
	assert(last_call.value_size == sizeof(value));
	assert(captured_i16() == value);
}

static void test_humidity_mapping(void)
{
	const zb_uint8_t endpoint = 12U;
	const int16_t value = 5678;
	const zb_zcl_status_t status = 0x43U;

	reset_mock(status);

	assert(app_zcl_report_humidity(endpoint, value) == status);
	assert_common_mapping(endpoint,
			      ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT,
			      ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID);
	assert(last_call.value_size == sizeof(value));
	assert(captured_i16() == value);
}

static void test_co2_mapping(void)
{
	const zb_uint8_t endpoint = 13U;
	const float value = 0.00042f;
	const zb_zcl_status_t status = 0x44U;

	reset_mock(status);

	assert(app_zcl_report_co2_fraction(endpoint, value) == status);
	assert_common_mapping(endpoint,
			      ZB_ZCL_CLUSTER_ID_CONCENTRATION_MEASUREMENT,
			      ZB_ZCL_ATTR_CONCENTRATION_MEASUREMENT_VALUE_ID);
	assert(last_call.value_size == sizeof(value));
	assert(captured_float() == value);
}

static void test_battery_voltage_mapping(void)
{
	const zb_uint8_t endpoint = 14U;
	const uint8_t value = 42U;
	const zb_zcl_status_t status = 0x45U;

	reset_mock(status);

	assert(app_zcl_report_battery_voltage(endpoint, value) == status);
	assert_common_mapping(endpoint,
			      ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
			      ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID);
	assert(last_call.value_size == sizeof(value));
	assert(last_call.value[0] == value);
}

static void test_battery_percentage_mapping(void)
{
	const zb_uint8_t endpoint = 15U;
	const uint8_t value = 198U;
	const zb_zcl_status_t status = 0x46U;

	reset_mock(status);

	assert(app_zcl_report_battery_percentage(endpoint, value) == status);
	assert_common_mapping(endpoint,
			      ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
			      ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID);
	assert(last_call.value_size == sizeof(value));
	assert(last_call.value[0] == value);
}

int main(void)
{
	test_zcl_id_constants_match_spec_values();
	test_temperature_mapping();
	test_humidity_mapping();
	test_co2_mapping();
	test_battery_voltage_mapping();
	test_battery_percentage_mapping();

	printf("zcl_report unit tests passed\n");
	return 0;
}
