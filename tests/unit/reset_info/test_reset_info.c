#include <assert.h>
#include <string.h>
#include <zephyr/drivers/hwinfo.h>

#include "app_reboot.h"
#include "app_reset_info.h"

static uint8_t boot(uint32_t cause, uint8_t retained, struct app_reset_info *info, char *text)
{
	uint8_t stored = app_reset_info_decode(cause, retained, info);
	app_reset_info_format(info, text, APP_RESET_INFO_TEXT_SIZE);
	return stored;
}

static void test_power_on_starts_counting_from_zero(void)
{
	struct app_reset_info info;
	char text[APP_RESET_INFO_TEXT_SIZE];

	assert(boot(0U, 0U, &info, text) == 0U);
	assert(info.resets == 0U && info.reason == 0U);
	assert(strcmp(text, "power n0") == 0);
}

static void test_software_reset_reports_reason_and_counts(void)
{
	struct app_reset_info info;
	char text[APP_RESET_INFO_TEXT_SIZE];
	uint8_t retained = boot(0U, 0U, &info, text);

	retained = app_reset_info_with_reason(retained, APP_REBOOT_OTA_STALLED);
	retained = boot(RESET_SOFTWARE, retained, &info, text);
	assert(info.reason == APP_REBOOT_OTA_STALLED && info.resets == 1U);
	assert(strcmp(text, "sw7 n1") == 0);
	/* The reason is consumed; a later reset without one is not attributed to it. */
	assert((retained & APP_RESET_INFO_REASON_MASK) == 0U);

	boot(RESET_SOFTWARE, retained, &info, text);
	assert(strcmp(text, "sw0 n2") == 0); /* e.g. a Zephyr fatal error */
}

static void test_hardware_causes_ignore_stale_reason(void)
{
	struct app_reset_info info;
	char text[APP_RESET_INFO_TEXT_SIZE];
	uint8_t retained = app_reset_info_with_reason(1U << APP_RESET_INFO_COUNT_SHIFT,
						      APP_REBOOT_SENSOR_ALARM);

	boot(RESET_WATCHDOG, retained, &info, text);
	assert(info.reason == 0U && strcmp(text, "wdt n2") == 0);
	boot(RESET_CPU_LOCKUP, retained, &info, text);
	assert(strcmp(text, "lockup n2") == 0);
	boot(RESET_PIN, retained, &info, text);
	assert(strcmp(text, "pin n2") == 0);
	boot(RESET_DEBUG, 0U, &info, text);
	assert(strcmp(text, "debug n1") == 0);
	/* Watchdog wins over an accumulated software flag. */
	boot(RESET_WATCHDOG | RESET_SOFTWARE, retained, &info, text);
	assert(strcmp(text, "wdt n2") == 0);
}

static void test_retained_state_without_cause_is_unknown_reset(void)
{
	struct app_reset_info info;
	char text[APP_RESET_INFO_TEXT_SIZE];

	/* A bootloader that clears the cause register must not look like power-on. */
	boot(0U, 3U << APP_RESET_INFO_COUNT_SHIFT, &info, text);
	assert(info.resets == 4U && strcmp(text, "unknown n4") == 0);
}

static void test_counter_saturates_and_text_fits(void)
{
	struct app_reset_info info;
	char text[APP_RESET_INFO_TEXT_SIZE];
	uint8_t retained = 0U;

	for (int i = 0; i < 40; i++) {
		retained = app_reset_info_with_reason(retained, 15U);
		retained = boot(RESET_SOFTWARE, retained, &info, text);
	}
	assert(info.resets == APP_RESET_INFO_COUNT_MAX);
	assert(strcmp(text, "sw15 n15") == 0);
	boot(RESET_CPU_LOCKUP, retained, &info, text);
	assert(strcmp(text, "lockup n15") == 0);
	boot(RESET_LOW_POWER_WAKE, retained, &info, text);
	assert(strcmp(text, "unknown n15") == 0);
	assert(strlen(text) < APP_RESET_INFO_TEXT_SIZE && strlen(text) <= 16U);
}

static void test_report_record_encodes_location_description(void)
{
	uint8_t record[APP_RESET_INFO_REPORT_SIZE];
	const uint8_t expected[] = {0x10, 0x00, 0x42, 6, 'w', 'd', 't', ' ', 'n', '3'};

	assert(app_reset_info_report_record("wdt n3", record, sizeof(record)) == sizeof(expected));
	assert(memcmp(record, expected, sizeof(expected)) == 0);
	/* The longest summary fills the record exactly. */
	assert(app_reset_info_report_record("unknown n15", record, sizeof(record)) ==
	       APP_RESET_INFO_REPORT_SIZE - 1U);
	assert(app_reset_info_report_record("wdt n3", record, 9U) == 0U);
	assert(app_reset_info_report_record("longer than twelve", record, sizeof(record)) == 0U);
}

int main(void)
{
	test_power_on_starts_counting_from_zero();
	test_software_reset_reports_reason_and_counts();
	test_hardware_causes_ignore_stale_reason();
	test_retained_state_without_cause_is_unknown_reset();
	test_counter_saturates_and_text_fits();
	test_report_record_encodes_location_description();
	return 0;
}
