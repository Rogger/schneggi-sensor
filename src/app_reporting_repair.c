#include "app_reporting_repair.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app_reporting_repair, LOG_LEVEL_INF);

static bool is_send_slot(const zb_zcl_reporting_info_t *slot)
{
	return ZB_ZCL_GET_REPORTING_FLAG(slot, ZB_ZCL_REPORTING_SLOT_BUSY) &&
	       slot->direction == ZB_ZCL_CONFIGURE_REPORTING_SEND_REPORT;
}

static bool same_attribute(const zb_zcl_reporting_info_t *a, const zb_zcl_reporting_info_t *b)
{
	return a->ep == b->ep && a->cluster_id == b->cluster_id &&
	       a->cluster_role == b->cluster_role && a->attr_id == b->attr_id;
}

enum app_reporting_action app_reporting_slot_action(const zb_zcl_reporting_info_t *slots,
						    size_t count, size_t index,
						    app_reporting_attr_exists_t exists)
{
	const zb_zcl_reporting_info_t *slot = &slots[index];

	if (!is_send_slot(slot)) {
		return APP_REPORTING_KEEP;
	}
	if (!exists(slot->ep, slot->cluster_id, slot->cluster_role, slot->attr_id)) {
		return APP_REPORTING_RELEASE;
	}
	if (slot->manuf_code == ZB_ZCL_NON_MANUFACTURER_SPECIFIC) {
		return APP_REPORTING_KEEP;
	}
	/* A valid slot for the same attribute already reports it. */
	for (size_t i = 0; i < count; i++) {
		if (i != index && is_send_slot(&slots[i]) && same_attribute(&slots[i], slot) &&
		    slots[i].manuf_code == ZB_ZCL_NON_MANUFACTURER_SPECIFIC) {
			return APP_REPORTING_RELEASE;
		}
	}
	return APP_REPORTING_NORMALIZE;
}

void app_reporting_apply(zb_zcl_reporting_info_t *slot, enum app_reporting_action action)
{
	if (action == APP_REPORTING_NORMALIZE) {
		slot->manuf_code = ZB_ZCL_NON_MANUFACTURER_SPECIFIC;
	} else if (action == APP_REPORTING_RELEASE) {
		ZB_ZCL_CLR_ALL_REPORTING_FLAGS(slot);
	}
}

static bool standard_attr_exists(zb_uint8_t ep, zb_uint16_t cluster_id,
				 zb_uint8_t cluster_role, zb_uint16_t attr_id)
{
	return zb_zcl_get_attr_desc_manuf_a(ep, cluster_id, cluster_role, attr_id,
					    ZB_ZCL_NON_MANUFACTURER_SPECIFIC) != NULL;
}

unsigned int app_reporting_repair(const zb_af_device_ctx_t *device)
{
	unsigned int changed = 0;

	for (zb_uint8_t e = 0; e < device->ep_count; e++) {
		zb_af_endpoint_desc_t *endpoint = device->ep_desc_list[e];
		zb_zcl_reporting_info_t *slots = endpoint->reporting_info;

		for (size_t i = 0; slots != NULL && i < endpoint->rep_info_count; i++) {
			enum app_reporting_action action = app_reporting_slot_action(
				slots, endpoint->rep_info_count, i, standard_attr_exists);

			if (action == APP_REPORTING_KEEP) {
				continue;
			}
			LOG_WRN("%s reporting slot ep %u cluster 0x%04x attr 0x%04x manuf 0x%04x "
				"flags 0x%02x dst 0x%04x/%u",
				action == APP_REPORTING_NORMALIZE ? "Repairing" : "Releasing",
				slots[i].ep, slots[i].cluster_id, slots[i].attr_id,
				slots[i].manuf_code, slots[i].flags, slots[i].dst.short_addr,
				slots[i].dst.endpoint);
			app_reporting_apply(&slots[i], action);
			changed++;
		}
	}

	if (changed > 0U && zb_nvram_write_dataset(ZB_NVRAM_ZCL_REPORTING_DATA) != RET_OK) {
		LOG_WRN("Could not persist repaired reporting slots");
	}
	return changed;
}
