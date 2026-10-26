#ifndef APP_REPORTING_REPAIR_H_
#define APP_REPORTING_REPAIR_H_

#include <stdbool.h>
#include <stddef.h>

#include <zboss_api.h>
#include <zcl/zb_zcl_reporting.h>

/* Field CO2 sensors sent empty, manufacturer-specific (code 0x0000) Report
 * Attributes frames for endpoint 2 several times per second. ZBOSS builds the
 * frame from a reporting slot whose manufacturer code does not match any
 * attribute, so the report carries no records and is retried endlessly.
 *
 * This firmware has no manufacturer-specific attributes. A send slot is
 * valid only for an existing standard attribute; anything else is repaired
 * (manufacturer code normalised) or released.
 */
enum app_reporting_action {
	APP_REPORTING_KEEP,
	APP_REPORTING_NORMALIZE,
	APP_REPORTING_RELEASE,
};

typedef bool (*app_reporting_attr_exists_t)(zb_uint8_t ep, zb_uint16_t cluster_id,
					    zb_uint8_t cluster_role, zb_uint16_t attr_id);

/* Decide what to do with slots[index] among the slots of one endpoint. */
enum app_reporting_action app_reporting_slot_action(const zb_zcl_reporting_info_t *slots,
						    size_t count, size_t index,
						    app_reporting_attr_exists_t exists);
void app_reporting_apply(zb_zcl_reporting_info_t *slot, enum app_reporting_action action);

/* Check every endpoint of the device on the Zigbee thread and persist any
 * change. Returns the number of slots changed.
 */
unsigned int app_reporting_repair(const zb_af_device_ctx_t *device);

#endif /* APP_REPORTING_REPAIR_H_ */
