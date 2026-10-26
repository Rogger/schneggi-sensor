#ifndef APP_RESET_REPORT_H_
#define APP_RESET_REPORT_H_

#include <zboss_api.h>

/* A sleepy end device polls too rarely for ZHA to read attributes on demand.
 * Push the reset summary to the coordinator once per boot instead; zigpy
 * caches it, so it appears in the ZHA device diagnostics download.
 *
 * text must stay valid for the whole boot. Call app_reset_report_poll()
 * on the Zigbee thread; it retries on each call until a report is sent.
 */
void app_reset_report_init(zb_uint8_t endpoint, const char *text);
void app_reset_report_poll(void);

#endif /* APP_RESET_REPORT_H_ */
