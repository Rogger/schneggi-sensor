#include "app_reset_info.h"

#include <stdio.h>
#include <string.h>
#include <zephyr/drivers/hwinfo.h>

#define BASIC_LOCATION_DESCRIPTION_ID 0x0010U
#define ZCL_CHAR_STRING_TYPE 0x42U

uint8_t app_reset_info_decode(uint32_t cause, uint8_t retained, struct app_reset_info *info)
{
	uint8_t resets = retained >> APP_RESET_INFO_COUNT_SHIFT;

	/* No hardware cause and no retained state means power-on or brownout.
	 * A retained byte without a cause still proves a reset (e.g. if the
	 * bootloader cleared the cause register), so count it as unknown.
	 */
	if (cause == 0U && retained == 0U) {
		resets = 0U;
	} else if (resets < APP_RESET_INFO_COUNT_MAX) {
		resets++;
	}

	info->cause = cause;
	info->reason = (cause & RESET_SOFTWARE) ? (retained & APP_RESET_INFO_REASON_MASK) : 0U;
	info->resets = resets;
	return (uint8_t)(resets << APP_RESET_INFO_COUNT_SHIFT);
}

uint8_t app_reset_info_with_reason(uint8_t retained, uint8_t reason)
{
	return (uint8_t)((retained & ~APP_RESET_INFO_REASON_MASK) |
			 (reason & APP_RESET_INFO_REASON_MASK));
}

void app_reset_info_format(const struct app_reset_info *info, char *text, size_t size)
{
	char cause[8];

	/* Report the most specific cause if several flags are set. */
	if (info->cause & RESET_WATCHDOG) {
		snprintf(cause, sizeof(cause), "wdt");
	} else if (info->cause & RESET_CPU_LOCKUP) {
		snprintf(cause, sizeof(cause), "lockup");
	} else if (info->cause & RESET_SOFTWARE) {
		snprintf(cause, sizeof(cause), "sw%u", info->reason);
	} else if (info->cause & RESET_PIN) {
		snprintf(cause, sizeof(cause), "pin");
	} else if (info->cause & RESET_DEBUG) {
		snprintf(cause, sizeof(cause), "debug");
	} else if (info->cause == 0U && info->resets == 0U) {
		snprintf(cause, sizeof(cause), "power");
	} else {
		snprintf(cause, sizeof(cause), "unknown");
	}
	snprintf(text, size, "%s n%u", cause, info->resets);
}

size_t app_reset_info_report_record(const char *text, uint8_t *out, size_t size)
{
	size_t length = strlen(text);

	if (length > APP_RESET_INFO_TEXT_SIZE - 1U || size < 4U + length) {
		return 0U;
	}
	/* ZCL fields are little endian; a character string has a length prefix. */
	out[0] = (uint8_t)(BASIC_LOCATION_DESCRIPTION_ID & 0xffU);
	out[1] = (uint8_t)(BASIC_LOCATION_DESCRIPTION_ID >> 8);
	out[2] = ZCL_CHAR_STRING_TYPE;
	out[3] = (uint8_t)length;
	memcpy(&out[4], text, length);
	return 4U + length;
}
