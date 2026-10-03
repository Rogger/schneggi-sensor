#ifndef APP_ZCL_REPORT_H_
#define APP_ZCL_REPORT_H_

#include <stdint.h>
#include <stdbool.h>

#include <zboss_api.h>

zb_zcl_status_t app_zcl_report_temperature(zb_uint8_t endpoint, int16_t centi_c);
zb_zcl_status_t app_zcl_report_humidity(zb_uint8_t endpoint, int16_t centi_percent);
zb_zcl_status_t app_zcl_report_co2_fraction(zb_uint8_t endpoint, float fraction);
zb_zcl_status_t app_zcl_report_battery_voltage(zb_uint8_t endpoint, uint8_t voltage_attribute);
zb_zcl_status_t app_zcl_report_battery_percentage(zb_uint8_t endpoint, uint8_t percentage_attribute);
bool app_zcl_custom_s16_reporting_active(zb_uint8_t endpoint,
					 zb_uint16_t cluster_id,
					 zb_uint16_t attr_id);

#endif /* APP_ZCL_REPORT_H_ */
