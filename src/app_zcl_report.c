#include "app_zcl_report.h"

#include <zcl/zb_zcl_power_config.h>
#include <zcl/zb_zcl_temp_measurement_addons.h>

#include "zcl/zb_zcl_concentration_measurement.h"

zb_zcl_status_t app_zcl_report_temperature(zb_uint8_t endpoint, int16_t centi_c)
{
	return zb_zcl_set_attr_val(endpoint,
				   ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
				   ZB_ZCL_CLUSTER_SERVER_ROLE,
				   ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
				   (zb_uint8_t *)&centi_c,
				   ZB_FALSE);
}

zb_zcl_status_t app_zcl_report_humidity(zb_uint8_t endpoint, int16_t centi_percent)
{
	return zb_zcl_set_attr_val(endpoint,
				   ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT,
				   ZB_ZCL_CLUSTER_SERVER_ROLE,
				   ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID,
				   (zb_uint8_t *)&centi_percent,
				   ZB_FALSE);
}

zb_zcl_status_t app_zcl_report_co2_fraction(zb_uint8_t endpoint, float fraction)
{
	return zb_zcl_set_attr_val(endpoint,
				   ZB_ZCL_CLUSTER_ID_CONCENTRATION_MEASUREMENT,
				   ZB_ZCL_CLUSTER_SERVER_ROLE,
				   ZB_ZCL_ATTR_CONCENTRATION_MEASUREMENT_VALUE_ID,
				   (zb_uint8_t *)&fraction,
				   ZB_FALSE);
}

zb_zcl_status_t app_zcl_report_battery_voltage(zb_uint8_t endpoint, uint8_t voltage_attribute)
{
	return zb_zcl_set_attr_val(endpoint,
				   ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
				   ZB_ZCL_CLUSTER_SERVER_ROLE,
				   ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID,
				   &voltage_attribute,
				   ZB_FALSE);
}

zb_zcl_status_t app_zcl_report_battery_percentage(zb_uint8_t endpoint, uint8_t percentage_attribute)
{
	return zb_zcl_set_attr_val(endpoint,
				   ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
				   ZB_ZCL_CLUSTER_SERVER_ROLE,
				   ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID,
				   &percentage_attribute,
				   ZB_FALSE);
}
