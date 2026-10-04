#include <assert.h>
#include <errno.h>
#include <string.h>
#include "app_battery.h"
#include "app_zcl_report.h"

static const struct device device;
static struct adc_dt_spec adc = { .dev = &device };
static const struct gpio_dt_spec enable = { .port = &device };
static char calls[32];
static size_t call_count;
static char failure;
static int32_t measured_mv;
static int voltage_status, percentage_status;
static int cleanup_status;
static uint16_t raw_sample;
static int32_t expected_raw;

static int step(char name)
{
	assert(call_count + 1 < sizeof(calls));
	calls[call_count++] = name;
	calls[call_count] = '\0';
	return failure == name ? -EIO : 0;
}

int gpio_pin_set_dt(const struct gpio_dt_spec *spec, int value)
{
	assert(spec == &enable);
	int result = step(value ? 'E' : 'D');
	return !value && cleanup_status != 0 ? cleanup_status : result;
}
int k_sleep(int duration) { assert(duration == 1); return step('S'); }
int adc_sequence_init_dt(const struct adc_dt_spec *spec, struct adc_sequence *sequence)
{
	assert(spec == &adc && sequence->buffer_size == sizeof(uint16_t));
	return step('I');
}
int adc_read(const struct device *dev, const struct adc_sequence *sequence)
{
	assert(dev == &device);
	*(uint16_t *)sequence->buffer = raw_sample;
	return step('R');
}
int adc_raw_to_millivolts_dt(const struct adc_dt_spec *spec, int32_t *value)
{
	assert(spec == &adc && *value == expected_raw);
	*value = measured_mv;
	return step('C');
}
zb_zcl_status_t app_zcl_report_battery_voltage(zb_uint8_t ep, uint8_t value)
{
	assert(ep == 7 && value == 42);
	step('V');
	return voltage_status;
}
zb_zcl_status_t app_zcl_report_battery_percentage(zb_uint8_t ep, uint8_t value)
{
	assert(ep == 7 && value == 200);
	step('P');
	return percentage_status;
}

static void reset_calls(void)
{
	call_count = 0;
	calls[0] = '\0';
	failure = 0;
	measured_mv = 450;
	voltage_status = percentage_status = 0;
	cleanup_status = 0;
	adc.channel_cfg.differential = false;
	raw_sample = 123;
	expected_raw = 123;
}

static void test_sample_failures_retry_and_disable_divider(void)
{
	const struct { char stage; const char *calls; } cases[] = {
		{'E', "ED"}, {'I', "ESID"}, {'R', "ESIRD"},
		{'C', "ESIRCD"}, {'D', "ESIRCD"},
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
		struct app_battery battery = { .adc = &adc, .enable = &enable };
		reset_calls();
		failure = cases[i].stage;
		assert(app_battery_update(&battery, 7, 0, 144) == -EIO);
		assert(strcmp(calls, cases[i].calls) == 0 && !battery.reported);
		reset_calls();
		assert(app_battery_update(&battery, 7, 1, 144) == 0);
		assert(strcmp(calls, "ESIRCDVP") == 0);
		assert(battery.reported && battery.reported_cycle == 1);
		reset_calls();
		assert(app_battery_update(&battery, 7, 144, 144) == 0);
		assert(call_count == 0);
		assert(app_battery_update(&battery, 7, 145, 144) == 0);
		assert(strcmp(calls, "ESIRCDVP") == 0);
	}
}

static void test_invalid_measurement_does_not_report(void)
{
	struct app_battery battery = { .adc = &adc, .enable = &enable };
	reset_calls();
	measured_mv = -1;
	assert(app_battery_update(&battery, 7, 0, 144) == -ERANGE);
	assert(strcmp(calls, "ESIRCD") == 0 && !battery.reported);
	reset_calls();
	adc.channel_cfg.differential = true;
	raw_sample = (uint16_t)-123;
	expected_raw = -123;
	measured_mv = -1;
	assert(app_battery_update(&battery, 7, 1, 144) == -ERANGE);
	assert(strcmp(calls, "ESIRCD") == 0 && !battery.reported);
}

static void test_cleanup_failure_preserves_sample_error(void)
{
	struct app_battery battery = { .adc = &adc, .enable = &enable };
	reset_calls();
	failure = 'R';
	cleanup_status = -EPERM;
	assert(app_battery_update(&battery, 7, 0, 144) == -EIO);
	assert(strcmp(calls, "ESIRD") == 0 && !battery.reported);
}

static void test_attribute_failures_retry_without_starving_other_attribute(void)
{
	for (int failed_voltage = 0; failed_voltage < 2; ++failed_voltage) {
		struct app_battery battery = {
			.adc = &adc, .enable = &enable, .reported = true, .reported_cycle = 0,
		};
		reset_calls();
		voltage_status = failed_voltage ? 1 : 0;
		percentage_status = failed_voltage ? 0 : 1;
		assert(app_battery_update(&battery, 7, 144, 144) == -EIO);
		assert(strcmp(calls, "ESIRCDVP") == 0 && battery.reported_cycle == 0);
		reset_calls();
		assert(app_battery_update(&battery, 7, 145, 144) == 0);
		assert(strcmp(calls, "ESIRCDVP") == 0 && battery.reported_cycle == 145);
	}
}

static void test_schedule_wraparound(void)
{
	struct app_battery battery = {
		.adc = &adc, .enable = &enable, .reported = true, .reported_cycle = UINT32_MAX - 4,
	};
	reset_calls();
	assert(app_battery_update(&battery, 7, 4, 10) == 0 && call_count == 0);
	assert(app_battery_update(&battery, 7, 5, 10) == 0);
	assert(strcmp(calls, "ESIRCDVP") == 0 && battery.reported_cycle == 5);
}

int main(void)
{
	test_sample_failures_retry_and_disable_divider();
	test_invalid_measurement_does_not_report();
	test_cleanup_failure_preserves_sample_error();
	test_attribute_failures_retry_without_starving_other_attribute();
	test_schedule_wraparound();
	return 0;
}
