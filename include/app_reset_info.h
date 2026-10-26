#ifndef APP_RESET_INFO_H_
#define APP_RESET_INFO_H_

#include <stddef.h>
#include <stdint.h>

/* The retained byte (nRF52840 GPREGRET2) survives soft, watchdog, lockup and
 * pin resets, but is cleared by power-on and brownout:
 *   bits 0-3: app_reboot() reason, valid after a software reset
 *   bits 4-7: resets since power-on, saturating at 15
 */
#define APP_RESET_INFO_REASON_MASK 0x0fU
#define APP_RESET_INFO_COUNT_SHIFT 4U
#define APP_RESET_INFO_COUNT_MAX 15U

/* Longest text is "unknown n15"; fits Basic LocationDescription (16). */
#define APP_RESET_INFO_TEXT_SIZE 13U

struct app_reset_info {
	uint32_t cause;  /* Zephyr hwinfo RESET_* flags of the last reset. */
	uint8_t reason;  /* enum app_reboot_reason, meaningful for software resets. */
	uint8_t resets;  /* Resets since power-on. */
};

/* Decode the boot state. Returns the retained byte to store for this boot. */
uint8_t app_reset_info_decode(uint32_t cause, uint8_t retained, struct app_reset_info *info);

/* Returns the retained byte with the reason replaced and the count kept. */
uint8_t app_reset_info_with_reason(uint8_t retained, uint8_t reason);

/* Short ZHA-readable summary, e.g. "wdt n3", "sw7 n1" or "power n0". */
void app_reset_info_format(const struct app_reset_info *info, char *text, size_t size);

/* Report Attributes record: attribute ID, type, length and text. */
#define APP_RESET_INFO_REPORT_SIZE (4U + APP_RESET_INFO_TEXT_SIZE - 1U)

/* Encode text as a Basic LocationDescription report record.
 * Returns the record length, or 0 if text or out does not fit.
 */
size_t app_reset_info_report_record(const char *text, uint8_t *out, size_t size);

/* Hardware access in app_reset_store.c. Load once at boot, before
 * anything can call app_reboot(); it clears the hardware reset cause.
 */
void app_reset_info_load(struct app_reset_info *info);
void app_reset_info_set_reason(uint8_t reason);

#endif /* APP_RESET_INFO_H_ */
