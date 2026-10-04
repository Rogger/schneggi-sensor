#include "app_battery.h"
#include "app_measurement_logic.h"
#include "app_zcl_report.h"

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app_battery, LOG_LEVEL_INF);

static int sample_millivolts(const struct app_battery *battery, int32_t *millivolts)
{
	uint16_t raw;
	struct adc_sequence sequence = {
		.buffer = &raw,
		.buffer_size = sizeof(raw),
	};
	int32_t value;
	int cleanup_err;
	int err = gpio_pin_set_dt(battery->enable, 1);

	if (err < 0) {
		goto disable;
	}
	k_sleep(K_MSEC(1));
	err = adc_sequence_init_dt(battery->adc, &sequence);
	if (err < 0) {
		goto disable;
	}
	err = adc_read(battery->adc->dev, &sequence);
	if (err < 0) {
		goto disable;
	}
	value = battery->adc->channel_cfg.differential ? (int16_t)raw : raw;
	err = adc_raw_to_millivolts_dt(battery->adc, &value);
	if (err < 0) {
		goto disable;
	}
	if (value < 0) {
		err = -ERANGE;
		goto disable;
	}
	*millivolts = app_battery_millivolts_from_adc(value);

disable:
	/* Always attempt to turn off the divider, including after an enable error.
	 * Preserve the original failure if cleanup also fails.
	 */
	cleanup_err = gpio_pin_set_dt(battery->enable, 0);
	if (cleanup_err < 0) {
		LOG_ERR("Could not disable battery monitor (%d)", cleanup_err);
		if (err == 0) {
			err = cleanup_err;
		}
	}
	return err;
}

int app_battery_update(struct app_battery *battery, uint8_t endpoint,
		       uint32_t cycle, uint32_t interval_cycles)
{
	if (battery->reported && cycle - battery->reported_cycle < interval_cycles) {
		return 0;
	}

	int32_t millivolts = 0;
	int err = sample_millivolts(battery, &millivolts);
	if (err < 0) {
		LOG_WRN("Battery measurement failed (%d); retrying next sensor cycle", err);
		return err;
	}

	uint8_t percentage = app_battery_percentage_from_mv((uint32_t)millivolts);
	LOG_INF("Battery: %d mV, %u%%", millivolts, percentage);
	zb_zcl_status_t voltage_status = app_zcl_report_battery_voltage(
		endpoint, app_battery_voltage_zcl_attribute(millivolts));
	zb_zcl_status_t percentage_status = app_zcl_report_battery_percentage(
		endpoint, app_battery_percentage_zcl_attribute(percentage));
	if (voltage_status != ZB_ZCL_STATUS_SUCCESS || percentage_status != ZB_ZCL_STATUS_SUCCESS) {
		LOG_WRN("Battery attribute update failed (voltage=%u, percentage=%u)",
			voltage_status, percentage_status);
		return -EIO;
	}

	battery->reported = true;
	battery->reported_cycle = cycle;
	return 0;
}
