#include <inttypes.h>
#include <soc.h>
#include <zephyr/types.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/pm/device_runtime.h>
#include "app_environment.h"
#include "app_battery.h"
#include "app_ota.h"
#include "app_reboot.h"
#include "app_scheduler.h"
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <ram_pwrdn.h>

#include <zboss_api.h>
#include <zboss_api_addons.h>
#include <zb_mem_config_med.h>
#include <zigbee/zigbee_app_utils.h>
#include <zigbee/zigbee_error_handler.h>
#include <zb_nrf_platform.h>
#include "nrf_802154.h"
#include "zb_schneggi_sensor.h"

#include <zcl/zb_zcl_power_config.h>
#include <zcl/zb_zcl_temp_measurement_addons.h>
#include <zcl/zb_zcl_basic_addons.h>
#include "zcl/zb_zcl_concentration_measurement.h"
#include "app_zcl_report.h"
#include "co2_zcl_logic.h"
#include "app_network.h"
#include "zigbee_signal_logic.h"

#define APP_HAS_SCD4X DT_HAS_COMPAT_STATUS_OKAY(sensirion_scd4x)

// Sleep
static const uint32_t SLEEP_INTERVAL_SECONDS = (uint32_t)CONFIG_SENSOR_UPDATE_INTERVAL_MINUTES * 60U;				   // HA minimum = 30s
#if !APP_HAS_SCD4X
static const uint32_t BATTERY_REPORT_INTERVAL_SECONDS = (uint32_t)CONFIG_BATTERY_UPDATE_INTERVAL_HOURS * 60U * 60U; // HA minimum = 3600s
static const uint32_t BATTERY_SLEEP_CYCLES =
	(BATTERY_REPORT_INTERVAL_SECONDS + SLEEP_INTERVAL_SECONDS - 1U) / SLEEP_INTERVAL_SECONDS;
#endif
static const uint32_t SENSOR_REFRESH_INTERVAL_SECONDS = 24U * 60U * 60U;
static const uint32_t SENSOR_REFRESH_CYCLES =
	(SENSOR_REFRESH_INTERVAL_SECONDS + SLEEP_INTERVAL_SECONDS - 1U) / SLEEP_INTERVAL_SECONDS;

#if defined(CONFIG_ZIGBEE_KEEPALIVE_TIMEOUT_MS)
#define APP_ZIGBEE_KEEPALIVE_TIMEOUT_MS ((uint32_t)CONFIG_ZIGBEE_KEEPALIVE_TIMEOUT_MS)
#else
#define APP_ZIGBEE_KEEPALIVE_TIMEOUT_MS (600000U)
#endif

#if defined(ED_AGING_TIMEOUT_64MIN)
#define APP_ZIGBEE_ED_TIMEOUT_VALUE ED_AGING_TIMEOUT_64MIN
#define APP_ZIGBEE_ED_TIMEOUT_DESC "64min"
#else
#define APP_ZIGBEE_ED_TIMEOUT_VALUE ED_AGING_TIMEOUT_32MIN
#define APP_ZIGBEE_ED_TIMEOUT_DESC "32min"
#endif

BUILD_ASSERT(CONFIG_SENSOR_UPDATE_INTERVAL_MINUTES > 0, "CONFIG_SENSOR_UPDATE_INTERVAL_MINUTES must be greater than zero");
#if !APP_HAS_SCD4X
BUILD_ASSERT(CONFIG_BATTERY_UPDATE_INTERVAL_HOURS > 0, "CONFIG_BATTERY_UPDATE_INTERVAL_HOURS must be greater than zero");
BUILD_ASSERT((uint32_t)CONFIG_BATTERY_UPDATE_INTERVAL_HOURS * 60U * 60U >=
					 (uint32_t)CONFIG_SENSOR_UPDATE_INTERVAL_MINUTES * 60U,
			 "CONFIG_BATTERY_UPDATE_INTERVAL_HOURS must not be shorter than CONFIG_SENSOR_UPDATE_INTERVAL_MINUTES");
#endif
BUILD_ASSERT(APP_ZIGBEE_KEEPALIVE_TIMEOUT_MS >= APP_ZIGBEE_LONG_POLL_INTERVAL_MS,
			 "CONFIG_ZIGBEE_KEEPALIVE_TIMEOUT_MS must be >= CONFIG_ZIGBEE_LONG_POLL_INTERVAL_MS");

// ZigBee
#define SCHNEGGI_ENDPOINT 0x01
#define SCHNEGGI_BASIC_MANUF_NAME "FuZZi"
#define SCHNEGGI_BASIC_MODEL_ID "Schneggi Sensor"
#define SCHNEGGI_BASIC_DATE_CODE "20240810"

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

#if !APP_HAS_SCD4X
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
#endif

typedef struct
{
	zb_zcl_basic_attrs_ext_t basic_attr;
	zb_zcl_identify_attrs_t identify_attr;
	zb_zcl_temp_measurement_attrs_t temp_measure_attrs;
	zb_zcl_rel_humidity_measurement_attr_t humidity_measure_attrs;
	zb_zcl_concentration_measurement_attrs_t concentration_measure_attrs;
#if !APP_HAS_SCD4X
	zb_zcl_power_config_attr_t power_config_attr;
#endif
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

#if APP_HAS_SCD4X
ZB_ZCL_DECLARE_CONCENTRATION_MEASUREMENT_ATTRIB_LIST(concentration_measurement_attr_list,
													 &dev_ctx.concentration_measure_attrs.measure_value,
													 &dev_ctx.concentration_measure_attrs.min_measure_value,
													 &dev_ctx.concentration_measure_attrs.max_measure_value,
													 &dev_ctx.concentration_measure_attrs.tolerance);
#endif

#if !APP_HAS_SCD4X
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
#endif

#if APP_HAS_SCD4X
#define SCHNEGGI_EXTRA_CLUSTER CONCENTRATION_MEASUREMENT
#define SCHNEGGI_EXTRA_ATTR_LIST concentration_measurement_attr_list
#define SCHNEGGI_EXTRA_REPORT_COUNT ZB_ZCL_CONCENTRATION_MEASUREMENT_REPORT_ATTR_COUNT
#define SCHNEGGI_DEVICE_VERSION 2
#else
#define SCHNEGGI_EXTRA_CLUSTER POWER_CONFIG
#define SCHNEGGI_EXTRA_ATTR_LIST power_config_attr_list
#define SCHNEGGI_EXTRA_REPORT_COUNT ZB_ZCL_POWER_CONFIG_REPORT_ATTR_COUNT
#define SCHNEGGI_DEVICE_VERSION 1
#endif

ZB_DECLARE_SCHNEGGI_CLUSTER_LIST(
	schneggi_clusters,
	basic_attr_list,
	identify_attr_list,
	temp_measurement_attr_list,
	humidity_measurement_attr_list,
	SCHNEGGI_EXTRA_CLUSTER,
	SCHNEGGI_EXTRA_ATTR_LIST);

ZB_DECLARE_SCHNEGGI_EP(
	schneggi_ep, SCHNEGGI_ENDPOINT, schneggi_clusters,
	SCHNEGGI_EXTRA_CLUSTER, SCHNEGGI_EXTRA_REPORT_COUNT, SCHNEGGI_DEVICE_VERSION);

extern zb_af_endpoint_desc_t zigbee_fota_client_ep;
BUILD_ASSERT(SCHNEGGI_ENDPOINT != CONFIG_ZIGBEE_FOTA_ENDPOINT);
ZBOSS_DECLARE_DEVICE_CTX_2_EP(
	device_ctx,
	zigbee_fota_client_ep,
	schneggi_ep);

// ADC
#if !APP_HAS_SCD4X
static const struct adc_dt_spec battery_adc = ADC_DT_SPEC_GET(DT_PATH(vbatt));
#endif

static const struct device *shtc3;
static const struct i2c_dt_spec shtc3_bus =
	I2C_DT_SPEC_GET(DT_COMPAT_GET_ANY_STATUS_OKAY(sensirion_shtcx));

#define LED_NODE DT_ALIAS(led)
static const struct gpio_dt_spec led_spec = GPIO_DT_SPEC_GET(LED_NODE, gpios);
static const struct gpio_dt_spec battery_monitor_enable = GPIO_DT_SPEC_GET(DT_PATH(vbatt), power_gpios);

#if !APP_HAS_SCD4X
static struct app_battery battery = {
	.adc = &battery_adc,
	.enable = &battery_monitor_enable,
};

static bool battery_monitor_ready;
#endif

#if APP_HAS_SCD4X
static const struct device *scd = DEVICE_DT_GET_ANY(sensirion_scd4x);
#else
static const struct device *scd;
#endif

LOG_MODULE_REGISTER(app, LOG_LEVEL_DBG);

static struct app_environment environment;

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

static void init_scd4x_device(void)
{
	if (!APP_HAS_SCD4X)
	{
		LOG_INF("SCD4X disabled by devicetree");
		return;
	}

	if (scd == NULL || device_is_ready(scd) == false)
	{
		LOG_ERR("Failed to initialize SCD4X device");
	}
	else
	{
		LOG_DBG("Found device %s.", scd->name);
	}
}

static void init_battery_monitor(void)
{
	int err;
	if (!gpio_is_ready_dt(&battery_monitor_enable))
	{
		LOG_ERR("Battery monitor GPIO not ready");
		return;
	}
	err = gpio_pin_configure_dt(&battery_monitor_enable, GPIO_OUTPUT_INACTIVE);
	if (err < 0)
	{
		LOG_ERR("Could not configure battery monitor GPIO (%d)", err);
		return;
	}
#if !APP_HAS_SCD4X
	if (!adc_is_ready_dt(&battery_adc))
	{
		LOG_ERR("Battery ADC not ready");
		return;
	}
	err = adc_channel_setup_dt(&battery_adc);
	if (err < 0)
	{
		LOG_ERR("Could not set up battery ADC (%d)", err);
		return;
	}

	battery_monitor_ready = true;
#endif
}

/**@brief Function for initializing all clusters attributes.
 */
static void init_clusters_attr(void)
{
	/* Basic cluster attributes data */
	dev_ctx.basic_attr.zcl_version = ZB_ZCL_VERSION;
	dev_ctx.basic_attr.power_source = APP_HAS_SCD4X ?
		ZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE : ZB_ZCL_BASIC_POWER_SOURCE_BATTERY;
	dev_ctx.basic_attr.app_version = 0x01;
	dev_ctx.basic_attr.stack_version = 0x03;
	dev_ctx.basic_attr.hw_version = 0x01;
	ZB_ZCL_SET_STRING_VAL(dev_ctx.basic_attr.sw_ver, CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION,
		ZB_ZCL_STRING_CONST_SIZE(CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION));

	ZB_ZCL_SET_STRING_VAL(
		dev_ctx.basic_attr.mf_name,
		SCHNEGGI_BASIC_MANUF_NAME,
		ZB_ZCL_STRING_CONST_SIZE(SCHNEGGI_BASIC_MANUF_NAME));

	ZB_ZCL_SET_STRING_VAL(
		dev_ctx.basic_attr.model_id,
		SCHNEGGI_BASIC_MODEL_ID,
		ZB_ZCL_STRING_CONST_SIZE(SCHNEGGI_BASIC_MODEL_ID));

	ZB_ZCL_SET_STRING_VAL(dev_ctx.basic_attr.date_code, SCHNEGGI_BASIC_DATE_CODE,
						  ZB_ZCL_STRING_CONST_SIZE(SCHNEGGI_BASIC_DATE_CODE));

	/* Identify cluster attributes data */
	dev_ctx.identify_attr.identify_time =
		ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;

#if !APP_HAS_SCD4X
	/* Battery power configuration */
	dev_ctx.power_config_attr.battery_voltage = ZB_ZCL_POWER_CONFIG_BATTERY_VOLTAGE_INVALID;
	dev_ctx.power_config_attr.battery_percentage_remaining = ZB_ZCL_POWER_CONFIG_BATTERY_REMAINING_UNKNOWN;
	dev_ctx.power_config_attr.battery_size = ZB_ZCL_POWER_CONFIG_BATTERY_SIZE_DEFAULT_VALUE;
	dev_ctx.power_config_attr.battery_quantity = 1;
#endif

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
		ZB_ZCL_CONCENTRATION_MEASUREMENT_MAX_VALUE_DEFAULT_VALUE;
	dev_ctx.concentration_measure_attrs.tolerance =
		co2_zcl_single_from_float(co2_zcl_fraction_from_ppm(100.0));
}

static bool identifying;

static void stop_identifying(void)
{
	identifying = false;
	gpio_pin_set_dt(&led_spec, 0);
}

static void toggle_identify_led(zb_uint8_t unused)
{
	ZVUNUSED(unused);
	/* A callback already dequeued when Identify stops must not restart blinking. */
	if (!identifying)
	{
		return;
	}
	gpio_pin_toggle_dt(&led_spec);
	if (app_alarm_replace(toggle_identify_led, 0, 100) != RET_OK)
	{
		LOG_WRN("Unable to schedule Identify blink");
		stop_identifying();
	}
}

static void identify_cb(zb_uint8_t active)
{
	if (active)
	{
		LOG_INF("Start identify");
		identifying = true;
		gpio_pin_set_dt(&led_spec, 1);
		if (app_alarm_replace(toggle_identify_led, 0, 100) != RET_OK)
		{
			LOG_WRN("Unable to start Identify blink");
			stop_identifying();
		}
	}
	else
	{
		LOG_INF("Stop identify");
		stop_identifying();
		ZB_SCHEDULE_APP_ALARM_CANCEL(toggle_identify_led, ZB_ALARM_ALL_CB);
	}
}

#if APP_HAS_SCD4X
static void update_scd4x_value(void)
{
	int err;

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
				}
				else if (measured_co2 > CO2_ZCL_MAX_PPM)
				{
					LOG_WRN("CO2 reading above maximum supported value (%.0f ppm), clamping",
							CO2_ZCL_MAX_PPM);
				}
				co2_attribute = co2_zcl_fraction_from_ppm(measured_co2);

				zb_zcl_status_t status =
					app_zcl_report_co2_fraction(SCHNEGGI_ENDPOINT, co2_attribute);
				if (status != ZB_ZCL_STATUS_SUCCESS)
				{
					LOG_ERR("Failed to set CO2 attribute: %d", status);
				}
			}
		}
	}
}
#endif

static void update_sensor_values(uint32_t current_cycle)
{
	if (shtc3 != NULL && device_is_ready(shtc3))
	{
		app_environment_update(&environment, shtc3, &shtc3_bus,
				       SCHNEGGI_ENDPOINT, current_cycle, SENSOR_REFRESH_CYCLES);
	}
	else
	{
		LOG_WRN("SHTC3 device not ready, keeping previous values");
	}

#if APP_HAS_SCD4X
	update_scd4x_value();
#endif
}

static uint32_t measurement_cycles = 0;

static void sensor_loop(zb_bufid_t bufid)
{
	ZVUNUSED(bufid);
	uint32_t current_cycle = measurement_cycles++;

#if APP_HAS_SCD4X
	LOG_DBG("-- Loop %" PRIu32 " (%s)--", current_cycle, ZB_JOINED() ? "Connected" : "Disconnected");
#else
	LOG_DBG("-- Loop %" PRIu32 " / %" PRIu32 " (%s)--", current_cycle, BATTERY_SLEEP_CYCLES, ZB_JOINED() ? "Connected" : "Disconnected");
#endif

	if (app_alarm_replace(sensor_loop, 0, SLEEP_INTERVAL_SECONDS * 1000U) != RET_OK)
	{
		LOG_ERR("Unable to schedule sensor measurements; restarting");
		app_reboot();
		return;
	}

	update_sensor_values(current_cycle);

#if !APP_HAS_SCD4X
	if (battery_monitor_ready)
	{
		app_battery_update(&battery, SCHNEGGI_ENDPOINT, current_cycle, BATTERY_SLEEP_CYCLES);
	}
#endif

#if APP_HAS_SCD4X
	LOG_DBG("Next measurement in %" PRIu32 " seconds", SLEEP_INTERVAL_SECONDS);
#else
	LOG_DBG("Sleep for %" PRIu32 " seconds", SLEEP_INTERVAL_SECONDS);
#endif
}

void zboss_signal_handler(zb_uint8_t param)
{
	zb_zdo_app_signal_hdr_t *header = NULL;
	zb_zdo_app_signal_type_t signal = zb_get_app_signal(param, &header);
	zb_ret_t status = ZB_GET_APP_SIGNAL_STATUS(param);
	enum app_zigbee_signal app_signal = APP_ZIGBEE_SIGNAL_OTHER;
	bool parent_link_failure = false;

	switch (signal)
	{
	case ZB_ZDO_SIGNAL_SKIP_STARTUP:
		LOG_DBG("Zigbee stack startup: %d", status);
		app_signal = APP_ZIGBEE_SIGNAL_SKIP_STARTUP;
		break;
	case ZB_BDB_SIGNAL_DEVICE_FIRST_START:
		LOG_DBG("First device startup: %d", status);
		app_signal = APP_ZIGBEE_SIGNAL_DEVICE_FIRST_START;
		break;
	case ZB_BDB_SIGNAL_DEVICE_REBOOT:
		LOG_INF("Reboot rejoin result: %d", status);
		app_signal = APP_ZIGBEE_SIGNAL_DEVICE_REBOOT;
		break;
	case ZB_BDB_SIGNAL_STEERING:
		LOG_INF("Network steering result: %d", status);
		app_signal = APP_ZIGBEE_SIGNAL_STEERING;
		break;
	case ZB_ZDO_SIGNAL_LEAVE:
		LOG_INF("Network leave result: %d", status);
		app_signal = APP_ZIGBEE_SIGNAL_LEAVE;
		break;
	case ZB_COMMON_SIGNAL_CAN_SLEEP:
		app_signal = APP_ZIGBEE_SIGNAL_CAN_SLEEP;
		break;
	case ZB_ZDO_SIGNAL_PRODUCTION_CONFIG_READY:
		LOG_DBG("Production config result: %d", status);
		break;
	case ZB_NLME_STATUS_INDICATION:
	{
		zb_zdo_signal_nlme_status_indication_params_t *indication =
			ZB_ZDO_SIGNAL_GET_PARAMS(header, zb_zdo_signal_nlme_status_indication_params_t);
		parent_link_failure =
			indication->nlme_status.status == ZB_NWK_COMMAND_STATUS_PARENT_LINK_FAILURE;
		if (parent_link_failure)
		{
			LOG_WRN("Parent link failure");
		}
		app_signal = APP_ZIGBEE_SIGNAL_NLME_STATUS_INDICATION;
		break;
	}
	default:
		LOG_INF("Unhandled signal %d. Status: %d", signal, status);
		break;
	}

	app_network_handle_signal(app_signal, status == RET_OK, parent_link_failure,
				  sensor_loop, !APP_HAS_SCD4X);

	/* Process leave/rejoin actions before OTA may reboot to reset its session. */
	app_ota_signal(param);
	if (param)
	{
		zb_buf_free(param);
	}
}

int main(void)
{
	LOG_INF("Schneggi sensor starting...");
	if (app_ota_init() != 0) {
		LOG_ERR("OTA/watchdog initialization failed");
		app_reboot();
		return -1;
	}

	/* Only the non-CO2 profiles enable runtime PM. TWIM takes/releases a
	 * runtime reference for each transfer, including sensor sleep cleanup.
	 */
	if (IS_ENABLED(CONFIG_PM_DEVICE_RUNTIME)) {
		int err = pm_device_runtime_enable(shtc3_bus.bus);
		if (err < 0) {
			LOG_ERR("Could not enable I2C runtime PM (%d)", err);
		}
	}

	init_shtc3_device();

	init_scd4x_device();

	/* Keep the divider disabled even when the USB profile never samples it. */
	init_battery_monitor();

	gpio_pin_configure_dt(&led_spec, GPIO_OUTPUT_INACTIVE);

	/* Power off unused sections of RAM to lower device power consumption. */
	if (IS_ENABLED(CONFIG_RAM_POWER_DOWN_LIBRARY))
	{
		LOG_DBG("Enable power down unused ram");
		power_down_unused_ram();
	}

	LOG_DBG("802.15.4 transmit power: %d dBm", nrf_802154_tx_power_get());
	LOG_DBG("ZB sleep threshold: %d ms", zb_get_sleep_threshold());

	/* The USB-powered CO2 device listens continuously; the battery variant sleeps. */
	app_ota_set_sleepy(!APP_HAS_SCD4X);
	zb_set_rx_on_when_idle(APP_HAS_SCD4X ? ZB_TRUE : ZB_FALSE);
	LOG_INF("Zigbee receiver on while idle: %s", APP_HAS_SCD4X ? "yes" : "no");

	// https://developer.nordicsemi.com/nRF_Connect_SDK/doc/zboss/3.11.2.1/zigbee_prog_principles.html#zigbee_power_optimization_sleepy
	zb_set_ed_timeout(APP_ZIGBEE_ED_TIMEOUT_VALUE);
	zb_set_keepalive_timeout(ZB_MILLISECONDS_TO_BEACON_INTERVAL(APP_ZIGBEE_KEEPALIVE_TIMEOUT_MS));
#if APP_HAS_SCD4X
	LOG_INF("Zigbee power profile: receiver on, keepalive=%u ms ed_timeout=%s",
		 APP_ZIGBEE_KEEPALIVE_TIMEOUT_MS, APP_ZIGBEE_ED_TIMEOUT_DESC);
#else
	LOG_INF("Zigbee power profile: long poll=%u ms keepalive=%u ms ed_timeout=%s",
		 APP_ZIGBEE_LONG_POLL_INTERVAL_MS,
		 APP_ZIGBEE_KEEPALIVE_TIMEOUT_MS,
		 APP_ZIGBEE_ED_TIMEOUT_DESC);
#endif

	ZB_AF_REGISTER_DEVICE_CTX(&device_ctx);

	init_clusters_attr();

	ZB_AF_SET_IDENTIFY_NOTIFICATION_HANDLER(SCHNEGGI_ENDPOINT, identify_cb);

	// Erase persistent storage
	zb_set_nvram_erase_at_start(ZB_FALSE);

	app_ota_startup_ready(device_is_ready(shtc3) &&
		(!APP_HAS_SCD4X || (scd != NULL && device_is_ready(scd)))
#if !APP_HAS_SCD4X
		&& battery_monitor_ready && adc_is_ready_dt(&battery_adc)
#endif
	);
	zigbee_enable();

	LOG_INF("Schneggi sensor started");

	k_sleep(K_FOREVER);

	return -1;
}
