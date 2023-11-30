/**
 * TODO
 * Battery optimization
 *  Only measure sensors when ZigBee is connected
 * 	Long Poll https://developer.nordicsemi.com/nRF_Connect_SDK/doc/zboss/3.11.2.1/zigbee_prog_principles.html#zigbee_power_optimization
 *  Don't send values if they did not change
 * Fix
 *  Identify LED stops on
 */

#include <zephyr/types.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <soc.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/settings/settings.h>

#include <zboss_api.h>
#include <zboss_api_addons.h>
#include <zb_mem_config_med.h>
#include <zigbee/zigbee_app_utils.h>
#include <zigbee/zigbee_error_handler.h>
#include <zigbee/zigbee_zcl_scenes.h>
#include <zb_nrf_platform.h>
#include "zb_dimmable_light.h"
#include "zb_zcl_power_config.h"

// Sleep
#define SLEEP_INTERVAL_SECONDS 5 * 60			// HA minimum = 30s
#define BATTERY_REPORT_INTERVAL_SECONDS 2 * 60 * 60 // HA minimum = 3600s
#define BATTERY_SLEEP_CYCLES BATTERY_REPORT_INTERVAL_SECONDS / SLEEP_INTERVAL_SECONDS

// ZigBee
#define SCHNEGGI_ENDPOINT 0x01
#define BULB_INIT_BASIC_MANUF_NAME "FuZZi"
#define BULB_INIT_BASIC_MODEL_ID "Schneggi Sensor"
#define BULB_INIT_BASIC_DATE_CODE "20231029"
#define BULB_INIT_BASIC_LOCATION_DESC "Living Room"

typedef struct
{
	zb_int16_t measure_value;
	zb_int16_t min_measure_value;
	zb_int16_t max_measure_value;
} zb_zcl_rel_humidity_measurement_attr_t;

typedef struct
{
	zb_uint8_t battery_voltage;
	zb_uint8_t battery_size;
	zb_uint8_t battery_quantity;
	zb_uint8_t battery_rated_voltage;
	zb_uint8_t battery_alarm_mask;
	zb_uint8_t battery_voltage_min_threshold;
	zb_uint8_t battery_percentage_remaining;
	zb_uint8_t battery_voltage_threshold1;
	zb_uint8_t battery_voltage_threshold2;
	zb_uint8_t battery_voltage_threshold3;
	zb_uint8_t battery_percentage_min_threshold;
	zb_uint8_t battery_percentage_threshold1;
	zb_uint8_t battery_percentage_threshold2;
	zb_uint8_t battery_percentage_threshold3;
	zb_uint32_t battery_alarm_state;

} zb_zcl_power_config_attr_t;

typedef struct
{
	zb_zcl_basic_attrs_ext_t basic_attr;
	zb_zcl_identify_attrs_t identify_attr;
	zb_zcl_temp_measurement_attrs_t temp_measure_attrs;
	zb_zcl_rel_humidity_measurement_attr_t humidity_measure_attrs;
	zb_zcl_power_config_attr_t power_config_attr;
} schneggi_device_ctx_t;

static schneggi_device_ctx_t dev_ctx;

ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT(
	basic_attr_list,
	&dev_ctx.basic_attr.zcl_version,
	&dev_ctx.basic_attr.app_version,
	&dev_ctx.basic_attr.stack_version,
	&dev_ctx.basic_attr.hw_version,
	dev_ctx.basic_attr.mf_name,
	dev_ctx.basic_attr.model_id,
	dev_ctx.basic_attr.date_code,
	&dev_ctx.basic_attr.power_source,
	dev_ctx.basic_attr.location_id,
	&dev_ctx.basic_attr.ph_env,
	&dev_ctx.basic_attr.sw_ver);

ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST(
	identify_attr_list,
	&dev_ctx.identify_attr.identify_time);

ZB_ZCL_DECLARE_TEMP_MEASUREMENT_ATTRIB_LIST(
	temp_measurement_attr_list,
	&dev_ctx.temp_measure_attrs.measure_value,
	&dev_ctx.temp_measure_attrs.min_measure_value,
	&dev_ctx.temp_measure_attrs.max_measure_value,
	&dev_ctx.temp_measure_attrs.tolerance);

ZB_ZCL_DECLARE_REL_HUMIDITY_MEASUREMENT_ATTRIB_LIST(
	humidity_measurement_attr_list,
	&dev_ctx.humidity_measure_attrs.measure_value,
	&dev_ctx.humidity_measure_attrs.min_measure_value,
	&dev_ctx.humidity_measure_attrs.max_measure_value);

/* Define 'bat_num' as empty in order to declare default battery set attributes. */
/* According to Table 3-17 of ZCL specification, defining 'bat_num' as 2 or 3 allows */
/* to declare battery set attributes for BATTERY2 and BATTERY3 */
#define bat_num

ZB_ZCL_DECLARE_POWER_CONFIG_BATTERY_ATTRIB_LIST_EXT(
	power_config_attr_list,
	&dev_ctx.power_config_attr.battery_voltage,
	&dev_ctx.power_config_attr.battery_size,
	&dev_ctx.power_config_attr.battery_quantity,
	&dev_ctx.power_config_attr.battery_rated_voltage,
	&dev_ctx.power_config_attr.battery_alarm_mask,
	&dev_ctx.power_config_attr.battery_voltage_min_threshold,
	&dev_ctx.power_config_attr.battery_percentage_remaining,
	&dev_ctx.power_config_attr.battery_voltage_threshold1,
	&dev_ctx.power_config_attr.battery_voltage_threshold2,
	&dev_ctx.power_config_attr.battery_voltage_threshold3,
	&dev_ctx.power_config_attr.battery_percentage_min_threshold,
	&dev_ctx.power_config_attr.battery_percentage_threshold1,
	&dev_ctx.power_config_attr.battery_percentage_threshold2,
	&dev_ctx.power_config_attr.battery_percentage_threshold3,
	&dev_ctx.power_config_attr.battery_alarm_state);

ZB_DECLARE_DIMMABLE_LIGHT_CLUSTER_LIST(
	dimmable_light_clusters,
	basic_attr_list,
	identify_attr_list,
	temp_measurement_attr_list,
	humidity_measurement_attr_list,
	power_config_attr_list);

ZB_DECLARE_DIMMABLE_LIGHT_EP(
	schneggi_ep,
	SCHNEGGI_ENDPOINT,
	dimmable_light_clusters);

ZBOSS_DECLARE_DEVICE_CTX_1_EP(
	device_ctx,
	schneggi_ep);

// ADC
#if !DT_NODE_EXISTS(DT_PATH(zephyr_user)) || \
	!DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)
#error "No suitable devicetree overlay specified"
#endif

#define DT_SPEC_AND_COMMA(node_id, prop, idx) \
	ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

static const struct adc_dt_spec adc_channels[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels, DT_SPEC_AND_COMMA)};

// LED
#define PWM_LED_NODE DT_NODELABEL(pwm_led)

#if DT_NODE_HAS_STATUS(PWM_LED_NODE, okay)
static const struct pwm_dt_spec led_pwm = PWM_DT_SPEC_GET(PWM_LED_NODE);
#else
#error "Choose supported PWM driver"
#endif

#define LED_PWM_PERIOD_US (USEC_PER_SEC / 100U) // Led PWM period, calculated for 100 Hz signal - in microseconds.

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

const struct device *shtc3;

uint16_t buf;
struct adc_sequence sequence = {
	.buffer = &buf,
	.buffer_size = sizeof(buf)};

struct gpio_dt_spec battery_monitor_enable = GPIO_DT_SPEC_GET(DT_PATH(vbatt), power_gpios);

/* Button used to enter the Bulb into the Identify mode. */
#define IDENTIFY_MODE_BUTTON 1

/* Button to start Factory Reset */
// #define FACTORY_RESET_BUTTON IDENTIFY_MODE_BUTTON

static void init_pwm_led(void)
{
	if (!device_is_ready(led_pwm.dev))
	{
		LOG_ERR("Error: PWM device %s is not ready", led_pwm.dev->name);
	}
}

static void init_shtc3_device(void)
{
	// Get a device structure from a devicetree node with compatible "sensirion,shtcx".
	shtc3 = DEVICE_DT_GET_ANY(sensirion_shtcx);

	if (shtc3 == NULL)
	{
		LOG_ERR("Error: No devicetree node found for Sensirion SHTCx.");
		return;
	}

	if (!device_is_ready(shtc3))
	{
		LOG_ERR("Device %s is not ready", shtc3->name);
		return;
	}

	LOG_INF("Found device %s.", shtc3->name);
}

void init_adc()
{
	int err;
	/* Configure channels individually prior to sampling. */
	for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++)
	{
		if (!adc_is_ready_dt(&adc_channels[i]))
		{
			LOG_ERR("ADC controller device %s not ready", adc_channels[i].dev->name);
			return;
		}

		err = adc_channel_setup_dt(&adc_channels[i]);
		if (err < 0)
		{
			LOG_ERR("Could not setup channel #%d (%d)", i, err);
			return;
		}
	}

	gpio_pin_configure_dt(&battery_monitor_enable, GPIO_OUTPUT);
}

/**@brief Function for initializing all clusters attributes.
 */
static void init_clusters_attr(void)
{
	/* Basic cluster attributes data */
	dev_ctx.basic_attr.zcl_version = ZB_ZCL_VERSION;
	dev_ctx.basic_attr.power_source = ZB_ZCL_BASIC_POWER_SOURCE_BATTERY;

	ZB_ZCL_SET_STRING_VAL(
		dev_ctx.basic_attr.mf_name,
		BULB_INIT_BASIC_MANUF_NAME,
		ZB_ZCL_STRING_CONST_SIZE(BULB_INIT_BASIC_MANUF_NAME));

	ZB_ZCL_SET_STRING_VAL(
		dev_ctx.basic_attr.model_id,
		BULB_INIT_BASIC_MODEL_ID,
		ZB_ZCL_STRING_CONST_SIZE(BULB_INIT_BASIC_MODEL_ID));

	/* Identify cluster attributes data. */
	dev_ctx.identify_attr.identify_time =
		ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;

	dev_ctx.power_config_attr.battery_voltage = ZB_ZCL_POWER_CONFIG_BATTERY_VOLTAGE_INVALID;
	dev_ctx.power_config_attr.battery_percentage_remaining = ZB_ZCL_POWER_CONFIG_BATTERY_REMAINING_UNKNOWN;
	dev_ctx.power_config_attr.battery_size = ZB_ZCL_POWER_CONFIG_BATTERY_SIZE_DEFAULT_VALUE;
	dev_ctx.power_config_attr.battery_quantity = 1;

	// dev_ctx.power_config_attr.battery_percentage_min_threshold = (uint8_t)(BATTERY_ALARM_MV/100) ;*/
}

/**@brief Sets brightness of the led
 *
 * @param[in] brightness_level Brightness level, allowed values 0 ... 255,
 *                             0 - turn off, 255 - full brightness.
 */
static void led_set_brightness(zb_uint8_t brightness_level)
{
	uint32_t pulse = brightness_level * LED_PWM_PERIOD_US / 255U;

	if (pwm_set_dt(&led_pwm, PWM_USEC(LED_PWM_PERIOD_US), PWM_USEC(pulse)))
	{
		LOG_ERR("Pwm led set fails");
		return;
	}
}

/**@brief Starts identifying the device.
 *
 * @param  bufid  Unused parameter, required by ZBOSS scheduler API.
 */
static void start_identifying(zb_bufid_t bufid)
{
	ZVUNUSED(bufid);

	if (ZB_JOINED())
	{
		/* Check if endpoint is in identifying mode,
		 * if not, put desired endpoint in identifying mode.
		 */
		if (dev_ctx.identify_attr.identify_time ==
			ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE)
		{

			zb_ret_t zb_err_code = zb_bdb_finding_binding_target(
				SCHNEGGI_ENDPOINT);

			if (zb_err_code == RET_OK)
			{
				LOG_INF("Enter identify mode");
			}
			else if (zb_err_code == RET_INVALID_STATE)
			{
				LOG_WRN("RET_INVALID_STATE - Cannot enter identify mode");
			}
			else
			{
				ZB_ERROR_CHECK(zb_err_code);
			}
		}
		else
		{
			LOG_INF("Cancel identify mode");
			zb_bdb_finding_binding_target_cancel();
		}
	}
	else
	{
		LOG_WRN("Device not in a network - cannot enter identify mode");
	}
}

/**@brief Callback for button events.
 *
 * @param[in]   button_state  Bitmask containing the state of the buttons.
 * @param[in]   has_changed   Bitmask containing buttons that have changed their state.
 */
static void button_changed(uint32_t button_state, uint32_t has_changed)
{
	if (IDENTIFY_MODE_BUTTON & has_changed)
	{
		if (IDENTIFY_MODE_BUTTON & button_state)
		{
			/* Button changed its state to pressed */
		}
		else
		{
			/* Button changed its state to released */
			if (was_factory_reset_done())
			{
				/* The long press was for Factory Reset */
				LOG_DBG("After Factory Reset - ignore button release");
			}
			else
			{
				/* Button released before Factory Reset */

				/* Start identification mode */
				ZB_SCHEDULE_APP_CALLBACK(start_identifying, 0);
			}
		}
	}

	check_factory_reset_button(button_state, has_changed);
}

/**@brief Function to toggle the identify LED
 *
 * @param  bufid  Unused parameter, required by ZBOSS scheduler API.
 */
static void toggle_identify_led(zb_bufid_t bufid)
{
	static int blink_status;

	led_set_brightness(((++blink_status) % 2) ? (255U) : (0U));
	ZB_SCHEDULE_APP_ALARM(toggle_identify_led, bufid, ZB_MILLISECONDS_TO_BEACON_INTERVAL(100));
}

/**@brief Function to handle identify notification events on the first endpoint.
 *
 * @param  bufid  Unused parameter, required by ZBOSS scheduler API.
 */
static void identify_cb(zb_bufid_t bufid)
{
	zb_ret_t zb_err_code;

	if (bufid)
	{
		LOG_INF("Start identify");
		/* Schedule a self-scheduling function that will toggle the LED. */
		ZB_SCHEDULE_APP_CALLBACK(toggle_identify_led, bufid);
	}
	else
	{
		LOG_INF("Stop identify");
		/* Cancel the toggling function alarm and restore current Zigbee LED state. */
		zb_err_code = ZB_SCHEDULE_APP_ALARM_CANCEL(toggle_identify_led, ZB_ALARM_ANY_PARAM);
		ZVUNUSED(zb_err_code);
	}
}

/**@brief Zigbee stack event handler.
 *
 * @param[in]   bufid   Reference to the Zigbee stack buffer
 *                      used to pass signal.
 */
void zboss_signal_handler(zb_bufid_t bufid)
{
	/* Update network status LED. */
	// TODO zigbee_led_status_update(bufid, ZIGBEE_NETWORK_STATE_LED);

	/* No application-specific behavior is required.
	 * Call default signal handler.
	 */
	ZB_ERROR_CHECK(zigbee_default_signal_handler(bufid));

	/* All callbacks should either reuse or free passed buffers.
	 * If bufid == 0, the buffer is invalid (not passed).
	 */
	if (bufid)
	{
		zb_buf_free(bufid);
	}
}

void update_sensor_values()
{
	struct sensor_value temp, hum;
	int16_t temperature_attribute = 0;
	int16_t humidity_attribute = 0;
	double measured_temperature = 0;
	double measured_humidity = 0;
	int st = 0;

	sensor_sample_fetch(shtc3);
	st = sensor_channel_get(shtc3, SENSOR_CHAN_AMBIENT_TEMP, &temp);

	if (st == 0)
	{

		measured_temperature = sensor_value_to_double(&temp);
		temperature_attribute = (int16_t)(measured_temperature * 100);
		LOG_INF("Temperature: %.2f", measured_temperature);

		zb_zcl_status_t status = zb_zcl_set_attr_val(
			SCHNEGGI_ENDPOINT,
			ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
			ZB_ZCL_CLUSTER_SERVER_ROLE,
			ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
			(zb_uint8_t *)&temperature_attribute,
			ZB_FALSE);
		if (status)
		{
			LOG_ERR("Failed to set ZCL attribute: %d", status);
			return;
		}
	}
	else
	{
		LOG_ERR("Failed to read temperature sensor: %d", st);
		return;
	}

	st = sensor_channel_get(shtc3, SENSOR_CHAN_HUMIDITY, &hum);

	if (st == 0)
	{

		measured_humidity = sensor_value_to_double(&hum);
		humidity_attribute = (int16_t)(measured_humidity * 100);
		LOG_INF("Humidity: %.2f", measured_humidity);

		zb_zcl_status_t status = zb_zcl_set_attr_val(
			SCHNEGGI_ENDPOINT,
			ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT,
			ZB_ZCL_CLUSTER_SERVER_ROLE,
			ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID,
			(zb_uint8_t *)&humidity_attribute,
			ZB_FALSE);
		if (status)
		{
			LOG_ERR("Failed to set ZCL attribute: %d", status);
			return;
		}
	}
	else
	{
		LOG_ERR("Failed to read humidity sensor: %d", st);
		return;
	}
}

/** A point in a battery discharge curve sequence.
 *
 * A discharge curve is defined as a sequence of these points, where
 * the first point has #lvl_pptt set to 10000 and the last point has
 * #lvl_pptt set to zero.  Both #lvl_pptt and #lvl_mV should be
 * monotonic decreasing within the sequence.
 */
struct battery_level_point
{
	/** Remaining life at #lvl_mV.
	 * 100 % -> 10000 */
	uint16_t lvl_pptt;

	/** Battery voltage at #lvl_pptt remaining life. */
	uint16_t lvl_mV;
};

/** Discharge curve for a li-poly battery
 *
 * See https://blog.ampow.com/lipo-voltage-chart/
 */
static const struct battery_level_point discharge_curve[] = {
	{10000, 4200},
	{9500, 4150},
	{9000, 4110},
	{8500, 4080},
	{8000, 4020},
	{7500, 3980},
	{7000, 3950},
	{6500, 3910},
	{6000, 3870},
	{5500, 3850},
	{5000, 3840},
	{4500, 3820},
	{4000, 3800},
	{3500, 3790},
	{3000, 3770},
	{2500, 3750},
	{2000, 3730},
	{1500, 3710},
	{1000, 3690},
	{0500, 3610},
	{0, 3270},
};

/**
 * @brief calculate the battery percentage based on the battery discharge curve
 * @param batt_mV Battery level in millivolts
 * @param curve Pointer to the battery discharge curve struct
 * @return Positive integer with the battery level in percentage
 **/
unsigned int battery_level_pptt(unsigned int batt_mV,
								const struct battery_level_point *curve)
{
	const struct battery_level_point *pb = curve;

	if (batt_mV >= pb->lvl_mV)
	{
		/* Measured voltage above highest point, cap at maximum. */
		return pb->lvl_pptt;
	}
	/* Go down to the last point at or below the measured voltage. */
	while ((pb->lvl_pptt > 0) && (batt_mV < pb->lvl_mV))
	{
		++pb;
	}
	if (batt_mV < pb->lvl_mV)
	{
		/* Below lowest point, cap at minimum */
		return pb->lvl_pptt;
	}

	/* Linear interpolation between below and above points. */
	const struct battery_level_point *pa = pb - 1;

	return pb->lvl_pptt + ((pa->lvl_pptt - pb->lvl_pptt) * (batt_mV - pb->lvl_mV) / (pa->lvl_mV - pb->lvl_mV));
}

void update_battery()
{
	int err;

	// Enable battery measurement
	gpio_pin_set_dt(&battery_monitor_enable, 1);
	k_sleep(K_MSEC(1));

	for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++)
	{
		int32_t val_mv;

		LOG_DBG("%s, channel %d: ",
				adc_channels[i].dev->name,
				adc_channels[i].channel_id);

		(void)adc_sequence_init_dt(&adc_channels[i], &sequence);

		err = adc_read(adc_channels[i].dev, &sequence);
		if (err < 0)
		{
			LOG_ERR("Could not read (%d)", err);
			continue;
		}

		/*
		 * If using differential mode, the 16 bit value
		 * in the ADC sample buffer should be a signed 2's
		 * complement value.
		 */
		if (adc_channels[i].channel_cfg.differential)
		{
			val_mv = (int32_t)((int16_t)buf);
		}
		else
		{
			val_mv = (int32_t)buf;
		}

		err = adc_raw_to_millivolts_dt(&adc_channels[i],
									   &val_mv);
		/* conversion to mV may not be supported, skip if not */
		if (err < 0)
		{
			LOG_ERR("(value in mV not available)");
		}
		else
		{

			int32_t battery_voltage_mv = val_mv * (1500000 + 180000) / 180000;

			if (i == 0)
			{
				uint8_t battery_attribute = (uint8_t)(battery_voltage_mv / 100);
				LOG_INF("Battery Voltage %d mV-> ZigBee Attribute Value: 0x%x", battery_voltage_mv, battery_attribute);
				zb_zcl_status_t status_battery_voltage = zb_zcl_set_attr_val(
					SCHNEGGI_ENDPOINT,
					ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
					ZB_ZCL_CLUSTER_SERVER_ROLE,
					ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID,
					(zb_uint8_t *)&battery_attribute,
					ZB_FALSE);

				uint32_t battery_percentage = battery_level_pptt(battery_voltage_mv, discharge_curve) / 100;
				uint8_t battery_percentage_attribute = (uint8_t)(battery_percentage * 2); // 3.3.2.2.3.2
				LOG_INF("Battery Percentage: %d -> ZigBee Attribute Value: 0x%x", battery_percentage, battery_percentage_attribute);
				zb_zcl_status_t status_battery_percentage = zb_zcl_set_attr_val(
					SCHNEGGI_ENDPOINT,
					ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
					ZB_ZCL_CLUSTER_SERVER_ROLE,
					ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID,
					(zb_uint8_t *)&battery_percentage_attribute,
					ZB_FALSE);
			}
		}
	}
	// Disable battery measurement
	gpio_pin_set_dt(&battery_monitor_enable, 0);
}

static void sensor_loop(zb_bufid_t bufid){
	static uint16_t cycles = 0;

	LOG_INF("Loop");
	update_sensor_values();
	LOG_DBG("%d", cycles % BATTERY_SLEEP_CYCLES);
	if (cycles % BATTERY_SLEEP_CYCLES == 0)
	{
		update_battery();
		cycles = 0;
	}
	cycles++;
	LOG_INF("Sleep for %d seconds", SLEEP_INTERVAL_SECONDS);
	
	ZB_SCHEDULE_APP_ALARM(sensor_loop, bufid, 
        ZB_MILLISECONDS_TO_BEACON_INTERVAL(
          SLEEP_INTERVAL_SECONDS * 1000));
}

int main(void)
{
	LOG_INF("Schneggi sensor starting...");

	init_shtc3_device();

	init_adc();

	// register_factory_reset_button(FACTORY_RESET_BUTTON);

	/* Power off unused sections of RAM to lower device power consumption. */
	if (IS_ENABLED(CONFIG_RAM_POWER_DOWN_LIBRARY))
	{
		power_down_unused_ram();
	}

	zb_set_ed_timeout(ED_AGING_TIMEOUT_64MIN);
  	zb_set_keepalive_timeout(ZB_MILLISECONDS_TO_BEACON_INTERVAL(300000));

	// RX on when Idle and power_source are required for the ZigBee capability AC mains = False
	zb_set_rx_on_when_idle(ZB_FALSE);

	// Turn off radio when sleeping https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/protocols/zigbee/configuring.html#sleepy-end-device-behavior
	zigbee_configure_sleepy_behavior(true);

	ZB_AF_REGISTER_DEVICE_CTX(&device_ctx);

	init_clusters_attr();

	ZB_AF_SET_IDENTIFY_NOTIFICATION_HANDLER(SCHNEGGI_ENDPOINT, identify_cb);
	zigbee_erase_persistent_storage(ZB_FALSE);

	/* Start Zigbee default thread */
	zigbee_enable();

	k_sleep(K_SECONDS(3));

	LOG_INF("Schneggi sensor started");

	ZB_SCHEDULE_APP_ALARM(sensor_loop, ZB_ALARM_ANY_PARAM, 
    ZB_MILLISECONDS_TO_BEACON_INTERVAL(0));
}
