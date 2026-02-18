#include <inttypes.h>
#include <soc.h>
#include <zephyr/types.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/util.h>
#include <dk_buttons_and_leds.h>
#include <ram_pwrdn.h>

#include <zboss_api.h>
#include <zboss_api_addons.h>
#include <zb_mem_config_med.h>
#include <zigbee/zigbee_app_utils.h>
#include <zigbee/zigbee_error_handler.h>
#include <zb_nrf_platform.h>
#include "nrf_802154.h"
#include "zb_dimmable_light.h"

#include <zcl/zb_zcl_power_config.h>
#include <zcl/zb_zcl_temp_measurement_addons.h>
#include <zcl/zb_zcl_basic_addons.h>
#include "zcl/zb_zcl_concentration_measurement.h"
#include "zigbee_signal_logic.h"

// Sleep
static const uint32_t SLEEP_INTERVAL_SECONDS = (uint32_t)CONFIG_SENSOR_UPDATE_INTERVAL_MINUTES * 60U;				   // HA minimum = 30s
static const uint32_t BATTERY_REPORT_INTERVAL_SECONDS = (uint32_t)CONFIG_BATTERY_UPDATE_INTERVAL_HOURS * 60U * 60U; // HA minimum = 3600s
static const uint32_t BATTERY_SLEEP_CYCLES = BATTERY_REPORT_INTERVAL_SECONDS / SLEEP_INTERVAL_SECONDS;

BUILD_ASSERT(CONFIG_SENSOR_UPDATE_INTERVAL_MINUTES > 0, "CONFIG_SENSOR_UPDATE_INTERVAL_MINUTES must be greater than zero");
BUILD_ASSERT(CONFIG_BATTERY_UPDATE_INTERVAL_HOURS > 0, "CONFIG_BATTERY_UPDATE_INTERVAL_HOURS must be greater than zero");
BUILD_ASSERT((uint32_t)CONFIG_BATTERY_UPDATE_INTERVAL_HOURS * 60U * 60U >=
				 (uint32_t)CONFIG_SENSOR_UPDATE_INTERVAL_MINUTES * 60U,
			 "CONFIG_BATTERY_UPDATE_INTERVAL_HOURS must not be shorter than CONFIG_SENSOR_UPDATE_INTERVAL_MINUTES");

#define ENABLE_SCD

// ZigBee
#define SCHNEGGI_ENDPOINT 0x01
#define BULB_INIT_BASIC_MANUF_NAME "FuZZi"
#define BULB_INIT_BASIC_MODEL_ID "Schneggi Sensor"
#define BULB_INIT_BASIC_DATE_CODE "20240810"

typedef struct
{
	zb_int16_t measure_value;
	zb_int16_t min_measure_value;
	zb_int16_t max_measure_value;
} zb_zcl_rel_humidity_measurement_attr_t;

typedef struct
{
	zb_uint32_t measure_value;
	zb_uint32_t min_measure_value;
	zb_uint32_t max_measure_value;
	zb_uint32_t tolerance;
} zb_zcl_concentration_measurement_attrs_t;

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
	zb_zcl_concentration_measurement_attrs_t concentration_measure_attrs;
	zb_zcl_power_config_attr_t power_config_attr;
} schneggi_device_ctx_t;

static schneggi_device_ctx_t dev_ctx;

static zb_uint32_t zcl_single_from_float(float value)
{
	union
	{
		float value_f;
		zb_uint32_t value_u32;
	} data = {.value_f = value};

	return data.value_u32;
}

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

ZB_ZCL_DECLARE_CONCENTRATION_MEASUREMENT_ATTRIB_LIST(concentration_measurement_attr_list,
													 &dev_ctx.concentration_measure_attrs.measure_value,
													 &dev_ctx.concentration_measure_attrs.min_measure_value,
													 &dev_ctx.concentration_measure_attrs.max_measure_value,
													 &dev_ctx.concentration_measure_attrs.tolerance);

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
	concentration_measurement_attr_list,
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

const struct device *shtc3;

#define LED_NODE DT_ALIAS(led)
const struct gpio_dt_spec led_spec = GPIO_DT_SPEC_GET(LED_NODE, gpios);

uint16_t buf;
struct adc_sequence sequence = {
	.buffer = &buf,
	.buffer_size = sizeof(buf)};

struct gpio_dt_spec battery_monitor_enable = GPIO_DT_SPEC_GET(DT_PATH(vbatt), power_gpios);

#if !DT_HAS_COMPAT_STATUS_OKAY(sensirion_scd4x)
#error "No sensirion,scd4x compatible node found in the device tree"
#endif

static const struct device *scd = DEVICE_DT_GET_ANY(sensirion_scd4x);

LOG_MODULE_REGISTER(app, LOG_LEVEL_DBG);

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

	LOG_DBG("Found device %s.", shtc3->name);
}

void init_scd4x_device(void)
{
	if (scd == NULL || device_is_ready(scd) == false)
	{
		LOG_ERR("Failed to initialize SCD4X device");
	}
	else
	{
		LOG_DBG("Found device %s.", scd->name);
	}
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
	dev_ctx.basic_attr.app_version = 0x01;
	dev_ctx.basic_attr.stack_version = 0x03;
	dev_ctx.basic_attr.hw_version = 0x01;

	ZB_ZCL_SET_STRING_VAL(
		dev_ctx.basic_attr.mf_name,
		BULB_INIT_BASIC_MANUF_NAME,
		ZB_ZCL_STRING_CONST_SIZE(BULB_INIT_BASIC_MANUF_NAME));

	ZB_ZCL_SET_STRING_VAL(
		dev_ctx.basic_attr.model_id,
		BULB_INIT_BASIC_MODEL_ID,
		ZB_ZCL_STRING_CONST_SIZE(BULB_INIT_BASIC_MODEL_ID));

	ZB_ZCL_SET_STRING_VAL(dev_ctx.basic_attr.date_code, BULB_INIT_BASIC_DATE_CODE,
						  ZB_ZCL_STRING_CONST_SIZE(BULB_INIT_BASIC_DATE_CODE));

	/* Identify cluster attributes data */
	dev_ctx.identify_attr.identify_time =
		ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;

	/* Power */
	dev_ctx.power_config_attr.battery_voltage = ZB_ZCL_POWER_CONFIG_BATTERY_VOLTAGE_INVALID;
	dev_ctx.power_config_attr.battery_percentage_remaining = ZB_ZCL_POWER_CONFIG_BATTERY_REMAINING_UNKNOWN;
	dev_ctx.power_config_attr.battery_size = ZB_ZCL_POWER_CONFIG_BATTERY_SIZE_DEFAULT_VALUE;
	dev_ctx.power_config_attr.battery_quantity = 1;

	/* Temperature */
	dev_ctx.temp_measure_attrs.measure_value = ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_UNKNOWN;
	dev_ctx.temp_measure_attrs.min_measure_value = ZB_ZCL_TEMP_MEASUREMENT_MIN_VALUE_DEFAULT_VALUE;
	dev_ctx.temp_measure_attrs.max_measure_value = ZB_ZCL_TEMP_MEASUREMENT_MAX_VALUE_DEFAULT_VALUE;
	dev_ctx.temp_measure_attrs.tolerance = ZB_ZCL_ATTR_TEMP_MEASUREMENT_TOLERANCE_MAX_VALUE;

	/* Humidity */
	dev_ctx.humidity_measure_attrs.measure_value = ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_UNKNOWN;
	dev_ctx.humidity_measure_attrs.min_measure_value =
		ZB_ZCL_REL_HUMIDITY_MEASUREMENT_MIN_VALUE_DEFAULT_VALUE;
	dev_ctx.humidity_measure_attrs.max_measure_value =
		ZB_ZCL_REL_HUMIDITY_MEASUREMENT_MAX_VALUE_DEFAULT_VALUE;

	/* CO2 */
	dev_ctx.concentration_measure_attrs.measure_value =
		ZB_ZCL_ATTR_CONCENTRATION_MEASUREMENT_VALUE_UNKNOWN;
	dev_ctx.concentration_measure_attrs.min_measure_value =
		ZB_ZCL_CONCENTRATION_MEASUREMENT_MIN_VALUE_DEFAULT_VALUE;
	dev_ctx.concentration_measure_attrs.max_measure_value =
		ZB_ZCL_CONCENTRATION_MEASUREMENT_MAX_VALUE_DEFAULT_VALUE; // 10 000ppm
	dev_ctx.concentration_measure_attrs.tolerance =
		zcl_single_from_float(100.0f * ZCL_CO2_MEASUREMENT_MEASURED_VALUE_MULTIPLIER);
}

/**@brief Function to toggle the identify LED
 *
 * @param  bufid  Unused parameter, required by ZBOSS scheduler API.
 */
static void toggle_identify_led(zb_bufid_t bufid)
{
	gpio_pin_toggle_dt(&led_spec);
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
		gpio_pin_set_dt(&led_spec, 1);
		/* Schedule a self-scheduling function that will toggle the LED. */
		ZB_SCHEDULE_APP_CALLBACK(toggle_identify_led, bufid);
	}
	else
	{
		LOG_INF("Stop identify");
		/* Cancel the toggling function alarm and restore current Zigbee LED state. */
		zb_err_code = ZB_SCHEDULE_APP_ALARM_CANCEL(toggle_identify_led, ZB_ALARM_ANY_PARAM);
		gpio_pin_set_dt(&led_spec, 0);
		ZVUNUSED(zb_err_code);
	}
}

void update_sensor_values()
{
	int err = 0;

	if (shtc3 == NULL || !device_is_ready(shtc3))
	{
		LOG_WRN("SHTC3 device not ready, keeping previous values");
	}
	else
	{
		err = sensor_sample_fetch(shtc3);
		if (err)
		{
			LOG_WRN("Failed to fetch sample from SHTC3: %d, keeping previous values", err);
		}
		else
		{
			struct sensor_value temp;
			struct sensor_value hum;
			int16_t temperature_attribute = 0;
			int16_t humidity_attribute = 0;
			double measured_temperature = 0.0;
			double measured_humidity = 0.0;
			int st = sensor_channel_get(shtc3, SENSOR_CHAN_AMBIENT_TEMP, &temp);

			if (st == 0)
			{
				measured_temperature = sensor_value_to_double(&temp);
				temperature_attribute = (int16_t)(measured_temperature * 100);
				LOG_INF("Temperature: %.2f °C", measured_temperature);

				zb_zcl_status_t status = zb_zcl_set_attr_val(
					SCHNEGGI_ENDPOINT,
					ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
					ZB_ZCL_CLUSTER_SERVER_ROLE,
					ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
					(zb_uint8_t *)&temperature_attribute,
					ZB_FALSE);
				if (status != ZB_ZCL_STATUS_SUCCESS)
				{
					LOG_ERR("Failed to set temperature attribute: %d", status);
				}
			}
			else
			{
				LOG_WRN("Failed to read temperature sensor: %d, keeping previous value", st);
			}

			st = sensor_channel_get(shtc3, SENSOR_CHAN_HUMIDITY, &hum);
			if (st == 0)
			{
				measured_humidity = sensor_value_to_double(&hum);
				humidity_attribute = (int16_t)(measured_humidity * 100);
				LOG_INF("Humidity: %.2f RH", measured_humidity);

				zb_zcl_status_t status = zb_zcl_set_attr_val(
					SCHNEGGI_ENDPOINT,
					ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT,
					ZB_ZCL_CLUSTER_SERVER_ROLE,
					ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID,
					(zb_uint8_t *)&humidity_attribute,
					ZB_FALSE);
				if (status != ZB_ZCL_STATUS_SUCCESS)
				{
					LOG_ERR("Failed to set humidity attribute: %d", status);
				}
			}
			else
			{
				LOG_WRN("Failed to read humidity sensor: %d, keeping previous value", st);
			}
		}
	}

#ifdef ENABLE_SCD
	if (scd == NULL || !device_is_ready(scd))
	{
		LOG_WRN("SCD4X device not ready, keeping previous value");
	}
	else
	{
		err = sensor_sample_fetch(scd);
		if (err)
		{
			LOG_WRN("Failed to fetch sample from SCD4X: %d, keeping previous value", err);
		}
		else
		{
			double measured_co2 = 0.0;
			float co2_attribute = 0.0f;
			struct sensor_value sensor_value;

			err = sensor_channel_get(scd, SENSOR_CHAN_CO2, &sensor_value);
			if (err)
			{
				LOG_WRN("Failed to get SCD4X CO2: %d, keeping previous value", err);
			}
			else
			{
				measured_co2 = sensor_value_to_double(&sensor_value);
				LOG_INF("CO2: %.2f ppm", measured_co2);

				if (measured_co2 < 0.0)
				{
					LOG_WRN("CO2 reading below zero, clamping to zero");
					co2_attribute = 0.0f;
				}
				else if (measured_co2 > ZB_ZCL_ATTR_CONCENTRATION_MEASUREMENT_MAX_VALUE_MAX_VALUE)
				{
					LOG_WRN("CO2 reading above maximum supported value, clamping");
					co2_attribute = ZB_ZCL_ATTR_CONCENTRATION_MEASUREMENT_MAX_VALUE_MAX_VALUE *
									ZCL_CO2_MEASUREMENT_MEASURED_VALUE_MULTIPLIER;
				}
				else
				{
					co2_attribute = measured_co2 * ZCL_CO2_MEASUREMENT_MEASURED_VALUE_MULTIPLIER;
				}

				zb_zcl_status_t status =
					zb_zcl_set_attr_val(SCHNEGGI_ENDPOINT,
										ZB_ZCL_CLUSTER_ID_CONCENTRATION_MEASUREMENT,
										ZB_ZCL_CLUSTER_SERVER_ROLE,
										ZB_ZCL_ATTR_CONCENTRATION_MEASUREMENT_VALUE_ID,
										(zb_uint8_t *)&co2_attribute, ZB_FALSE);
				if (status != ZB_ZCL_STATUS_SUCCESS)
				{
					LOG_ERR("Failed to set CO2 attribute: %d", status);
				}
			}
		}
	}
#endif
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
				if (battery_voltage_mv < 0)
				{
					LOG_DBG("Not reporting negative voltage");
					goto cleanup;
				}

				uint8_t battery_attribute = (uint8_t)(battery_voltage_mv / 100);
				LOG_INF("Battery Voltage %d mV-> ZigBee Attribute Value: 0x%x", battery_voltage_mv, battery_attribute);
				zb_zcl_status_t status_battery_voltage = zb_zcl_set_attr_val(
					SCHNEGGI_ENDPOINT,
					ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
					ZB_ZCL_CLUSTER_SERVER_ROLE,
					ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID,
					(zb_uint8_t *)&battery_attribute,
					ZB_FALSE);
				if (status_battery_voltage)
				{
					LOG_ERR("Failed to set ZCL attribute: %d", status_battery_voltage);
					goto cleanup;
				}

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
				if (status_battery_percentage)
				{
					LOG_ERR("Failed to set ZCL attribute: %d", status_battery_percentage);
					goto cleanup;
				}
			}
		}
	}
cleanup:
	// Disable battery measurement
	gpio_pin_set_dt(&battery_monitor_enable, 0);
}

static uint32_t cycles = 0;

static void sensor_loop(zb_bufid_t bufid)
{
	ZVUNUSED(bufid);

	LOG_DBG("-- Loop %" PRIu32 " / %" PRIu32 " (%s)--", cycles, BATTERY_SLEEP_CYCLES, ZB_JOINED() ? "Connected" : "Disconnected");

	ZB_SCHEDULE_APP_ALARM_CANCEL(sensor_loop, ZB_ALARM_ANY_PARAM);
	zb_ret_t ret = ZB_SCHEDULE_APP_ALARM(sensor_loop, ZB_ALARM_ANY_PARAM,
										 ZB_MILLISECONDS_TO_BEACON_INTERVAL(
											 SLEEP_INTERVAL_SECONDS * 1000));
	if (ret != RET_OK)
	{
		LOG_ERR("Unable to schedule the sensor loop");
	}

	update_sensor_values();

	/* Update battery on startup (cycles==0) or oncce BATTERY_SLEEP_CYCLES is reached*/
	if (cycles == 0 || cycles == BATTERY_SLEEP_CYCLES)
	{
		update_battery();
		cycles = 1;
	}
	else
	{
		cycles++;
	}

	LOG_DBG("Sleep for %" PRIu32 " seconds", SLEEP_INTERVAL_SECONDS);
}

bool joining_signal_received = false;
bool stack_initialised = false;

static void execute_signal_actions(const struct app_zigbee_actions *actions)
{
	zb_bool_t comm_status = ZB_TRUE;

	if (actions->commissioning_mode == APP_COMMISSIONING_INITIALIZATION)
	{
		comm_status = bdb_start_top_level_commissioning(ZB_BDB_INITIALIZATION);
	}
	else if (actions->commissioning_mode == APP_COMMISSIONING_NETWORK_STEERING)
	{
		comm_status = bdb_start_top_level_commissioning(ZB_BDB_NETWORK_STEERING);
	}

	if (actions->commissioning_mode != APP_COMMISSIONING_NONE &&
		comm_status != ZB_TRUE)
	{
		LOG_WRN("Commissioning error");
	}

	if (actions->schedule_sensor_loop_cancel)
	{
		ZB_SCHEDULE_APP_ALARM_CANCEL(sensor_loop, ZB_ALARM_ANY_PARAM);
	}

	if (actions->schedule_sensor_loop)
	{
		ZB_SCHEDULE_APP_ALARM(sensor_loop, ZB_ALARM_ANY_PARAM,
							  ZB_MILLISECONDS_TO_BEACON_INTERVAL(actions->schedule_sensor_loop_delay_ms));
	}

	if (actions->set_long_poll_interval)
	{
		zb_zdo_pim_set_long_poll_interval(actions->long_poll_interval_ms);
	}

	if (actions->request_sleep)
	{
		zb_sleep_now();
	}

	if (actions->request_reset)
	{
		zb_reset(0);
	}
}

void zboss_signal_handler(zb_uint8_t param)
{
	zb_zdo_app_signal_hdr_t *p_sg_p = NULL;
	zb_zdo_app_signal_type_t sig = zb_get_app_signal(param, &p_sg_p);
	zb_ret_t status = ZB_GET_APP_SIGNAL_STATUS(param);
	struct app_zigbee_state app_state = {
		.joining_signal_received = joining_signal_received,
		.stack_initialised = stack_initialised,
	};
	struct app_zigbee_actions actions;
	bool status_ok = (status == RET_OK);

	switch (sig)
	{
	case ZB_ZDO_SIGNAL_SKIP_STARTUP:
		LOG_DBG("> SKIP_STARTUP");
		LOG_DBG("ZigBee stack initialized. Start commissioning");
		app_zigbee_handle_signal(&app_state,
								 APP_ZIGBEE_SIGNAL_SKIP_STARTUP,
								 status_ok,
								 false,
								 false,
								 &actions);
		execute_signal_actions(&actions);
		break;

	case ZB_BDB_SIGNAL_DEVICE_FIRST_START:
		LOG_DBG("> DEVICE_FIRST_START");
		app_zigbee_handle_signal(&app_state,
								 APP_ZIGBEE_SIGNAL_DEVICE_FIRST_START,
								 status_ok,
								 false,
								 false,
								 &actions);
		execute_signal_actions(&actions);
		break;

	case ZB_BDB_SIGNAL_DEVICE_REBOOT:
		LOG_DBG("> DEVICE_REBOOT");
		if (status_ok)
		{
			LOG_INF("Joined network successfully on reboot.");
		}
		else
		{
			LOG_WRN("Reboot rejoin failed. Status: %d", status);
		}
		app_zigbee_handle_signal(&app_state,
								 APP_ZIGBEE_SIGNAL_DEVICE_REBOOT,
								 status_ok,
								 false,
								 false,
								 &actions);
		execute_signal_actions(&actions);
		break;

	case ZB_BDB_SIGNAL_STEERING:
		LOG_DBG("> STEERING");

		if (status_ok)
		{
			LOG_INF("Joined network successfully!");
		}
		else
		{
			LOG_WRN("Failed to join network. Status: %d", status);
		}
		app_zigbee_handle_signal(&app_state,
								 APP_ZIGBEE_SIGNAL_STEERING,
								 status_ok,
								 false,
								 false,
								 &actions);
		execute_signal_actions(&actions);
		break;

	case ZB_ZDO_SIGNAL_LEAVE:
		LOG_DBG("> LEAVE");

		if (status_ok)
		{
			zb_zdo_signal_leave_params_t *p_leave_params = ZB_ZDO_SIGNAL_GET_PARAMS(p_sg_p, zb_zdo_signal_leave_params_t);
			LOG_INF("Network left. Leave type: %d", p_leave_params->leave_type);
			app_zigbee_handle_signal(&app_state,
									 APP_ZIGBEE_SIGNAL_LEAVE,
									 status_ok,
									 p_leave_params->leave_type == ZB_NWK_LEAVE_TYPE_REJOIN,
									 false,
									 &actions);
			execute_signal_actions(&actions);
		}
		else
		{
			LOG_ERR("Unable to leave network. Status: %d", status);
			app_zigbee_handle_signal(&app_state,
									 APP_ZIGBEE_SIGNAL_LEAVE,
									 status_ok,
									 false,
									 false,
									 &actions);
			execute_signal_actions(&actions);
		}
		break;

	case ZB_COMMON_SIGNAL_CAN_SLEEP:
		app_zigbee_handle_signal(&app_state,
								 APP_ZIGBEE_SIGNAL_CAN_SLEEP,
								 status_ok,
								 false,
								 false,
								 &actions);
		execute_signal_actions(&actions);
		break;

	case ZB_ZDO_SIGNAL_PRODUCTION_CONFIG_READY:
		LOG_DBG("> PRODUCTION_CONFIG_READY");
		if (status != RET_OK)
		{
			LOG_DBG("Production config is not present or invalid");
		}
		else
		{
			LOG_DBG("Production config ready");
		}
		break;

	case ZB_NLME_STATUS_INDICATION:
	{
		LOG_DBG("> STATUS_INDICATION");
		LOG_DBG("NLME Status indication. Status: %d", status);

		zb_zdo_signal_nlme_status_indication_params_t *nlme_status_ind =
			ZB_ZDO_SIGNAL_GET_PARAMS(p_sg_p, zb_zdo_signal_nlme_status_indication_params_t);
		bool parent_link_failure =
			(nlme_status_ind->nlme_status.status == ZB_NWK_COMMAND_STATUS_PARENT_LINK_FAILURE);
		if (parent_link_failure)
		{
			LOG_WRN("Parent link failure");
		}
		app_zigbee_handle_signal(&app_state,
								 APP_ZIGBEE_SIGNAL_NLME_STATUS_INDICATION,
								 status_ok,
								 false,
								 parent_link_failure,
								 &actions);
		if (actions.request_reset)
		{
			LOG_WRN("Broken rejon procedure");
		}
		execute_signal_actions(&actions);
		break;
	}

	default:
		/* Unhandled signal. For more information see: zb_zdo_app_signal_type_e and zb_ret_e */
		LOG_INF("Unhandled signal %d. Status: %d", sig, status);
		break;
	}

	joining_signal_received = app_state.joining_signal_received;
	stack_initialised = app_state.stack_initialised;

	if (param)
	{
		zb_buf_free(param);
	}
}

int main(void)
{
	LOG_INF("Schneggi sensor starting...");

	init_shtc3_device();

#ifdef ENABLE_SCD
	init_scd4x_device();
#endif

	init_adc();

	gpio_pin_configure_dt(&led_spec, GPIO_OUTPUT_INACTIVE);

	/* Power off unused sections of RAM to lower device power consumption. */
	if (IS_ENABLED(CONFIG_RAM_POWER_DOWN_LIBRARY))
	{
		LOG_DBG("Enable power down unused ram");
		power_down_unused_ram();
	}

	// nrf_802154_tx_power_set(8); //8 dBm (max)
	LOG_DBG("802.15.4 transmit power: %d dBm", nrf_802154_tx_power_get());
	LOG_DBG("ZB sleep threshold: %d ms", zb_get_sleep_threshold());

	// RX on when Idle and power_source are required for the ZigBee capability AC mains = False
	// Turn off radio when sleeping https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/protocols/zigbee/configuring.html#sleepy-end-device-behavior
	zb_set_rx_on_when_idle(ZB_FALSE);
	LOG_INF("Enabled sleepy end device behavior.");

	// https://developer.nordicsemi.com/nRF_Connect_SDK/doc/zboss/3.11.2.1/zigbee_prog_principles.html#zigbee_power_optimization_sleepy
	zb_set_ed_timeout(ED_AGING_TIMEOUT_32MIN);
	zb_set_keepalive_timeout(ZB_MILLISECONDS_TO_BEACON_INTERVAL(300000));

	ZB_AF_REGISTER_DEVICE_CTX(&device_ctx);

	init_clusters_attr();

	ZB_AF_SET_IDENTIFY_NOTIFICATION_HANDLER(SCHNEGGI_ENDPOINT, identify_cb);

	// Erase persistent storage
	zb_set_nvram_erase_at_start(ZB_FALSE);

	zigbee_enable();

	LOG_INF("Schneggi sensor started");

	k_sleep(K_FOREVER);

	return -1;
}
