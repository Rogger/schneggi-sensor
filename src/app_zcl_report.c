#include "app_zcl_report.h"

#include <stddef.h>

#include <zcl/zb_zcl_power_config.h>
#include <zcl/zb_zcl_temp_measurement_addons.h>
#include <zcl/zb_zcl_reporting.h>

#include "zcl/zb_zcl_concentration_measurement.h"

bool app_zcl_remote_reporting_configured(zb_uint8_t endpoint,
					 zb_uint16_t cluster_id,
					 zb_uint16_t attr_id)
{
	zb_zcl_reporting_info_t *info = zb_zcl_find_reporting_info_manuf(
		endpoint, cluster_id, ZB_ZCL_CLUSTER_SERVER_ROLE, attr_id,
		ZB_ZCL_NON_MANUFACTURER_SPECIFIC);

	/* ZBOSS also creates local default slots; only a peer's Configure Reporting
	 * supplies a destination endpoint. Keep the legacy thresholds until then.
	 */
	return info != NULL && info->dst.endpoint != 0U;
}

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
