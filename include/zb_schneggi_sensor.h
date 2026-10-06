#ifndef ZB_SCHNEGGI_SENSOR_H_
#define ZB_SCHNEGGI_SENSOR_H_

/* Both variants have four common server clusters and one power-profile cluster.
 * Keep their on-air device versions and cluster sets stable across refactors.
 */
#define ZB_SCHNEGGI_IN_CLUSTER_COUNT 5
#define ZB_SCHNEGGI_OUT_CLUSTER_COUNT 0
#define ZB_SCHNEGGI_COMMON_REPORT_COUNT \
	(ZB_ZCL_TEMP_MEASUREMENT_REPORT_ATTR_COUNT + \
	 ZB_ZCL_REL_HUMIDITY_MEASUREMENT_REPORT_ATTR_COUNT)

/* ZBOSS token-pastes the cluster ID to find its initializer. Pass suffixes
 * (e.g. BASIC) so the ID is not expanded to a number before that paste.
 */
#define ZB_SCHNEGGI_SERVER_CLUSTER(name, attrs) \
	ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_##name, ZB_ZCL_ARRAY_SIZE(attrs, zb_zcl_attr_t), attrs, \
		ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID)

#define ZB_DECLARE_SCHNEGGI_CLUSTER_LIST(name, basic, identify, temperature, humidity, extra_id, extra) \
	zb_zcl_cluster_desc_t name[] = { \
		ZB_SCHNEGGI_SERVER_CLUSTER(IDENTIFY, identify), \
		ZB_SCHNEGGI_SERVER_CLUSTER(BASIC, basic), \
		ZB_SCHNEGGI_SERVER_CLUSTER(TEMP_MEASUREMENT, temperature), \
		ZB_SCHNEGGI_SERVER_CLUSTER(REL_HUMIDITY_MEASUREMENT, humidity), \
		ZB_SCHNEGGI_SERVER_CLUSTER(extra_id, extra), \
	}

#define ZB_SCHNEGGI_DECLARE_SIMPLE_DESC(in_count, out_count) \
	ZB_DECLARE_SIMPLE_DESC(in_count, out_count)
#define ZB_SCHNEGGI_CLUSTER_ID(name) ZB_ZCL_CLUSTER_ID_##name

#define ZB_DECLARE_SCHNEGGI_EP(ep_name, ep_id, clusters, extra_id, extra_reports, version) \
	ZB_SCHNEGGI_DECLARE_SIMPLE_DESC(ZB_SCHNEGGI_IN_CLUSTER_COUNT, ZB_SCHNEGGI_OUT_CLUSTER_COUNT); \
	ZB_AF_SIMPLE_DESC_TYPE(ZB_SCHNEGGI_IN_CLUSTER_COUNT, ZB_SCHNEGGI_OUT_CLUSTER_COUNT) \
		simple_desc_##ep_name = { \
			ep_id, ZB_AF_HA_PROFILE_ID, ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID, version, 0, \
			ZB_SCHNEGGI_IN_CLUSTER_COUNT, ZB_SCHNEGGI_OUT_CLUSTER_COUNT, \
			{ZB_ZCL_CLUSTER_ID_BASIC, ZB_ZCL_CLUSTER_ID_IDENTIFY, \
			 ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, \
			 ZB_SCHNEGGI_CLUSTER_ID(extra_id)}, \
		}; \
	ZBOSS_DEVICE_DECLARE_REPORTING_CTX(reporting_info##ep_name, \
		ZB_SCHNEGGI_COMMON_REPORT_COUNT + extra_reports); \
	ZB_AF_DECLARE_ENDPOINT_DESC(ep_name, ep_id, ZB_AF_HA_PROFILE_ID, 0, NULL, \
		ZB_ZCL_ARRAY_SIZE(clusters, zb_zcl_cluster_desc_t), clusters, \
		(zb_af_simple_desc_1_1_t *)&simple_desc_##ep_name, \
		ZB_SCHNEGGI_COMMON_REPORT_COUNT + extra_reports, reporting_info##ep_name, 0, NULL)

/* A separate standard temperature endpoint lets ZHA discover a second reading
 * without changing the existing ambient-temperature cluster on endpoint 1.
 */
#define ZB_DECLARE_SCHNEGGI_TEMPERATURE_CLUSTER_LIST(name, basic, identify, temperature) \
	zb_zcl_cluster_desc_t name[] = { \
		ZB_SCHNEGGI_SERVER_CLUSTER(BASIC, basic), \
		ZB_SCHNEGGI_SERVER_CLUSTER(IDENTIFY, identify), \
		ZB_SCHNEGGI_SERVER_CLUSTER(TEMP_MEASUREMENT, temperature), \
	}

#define ZB_DECLARE_SCHNEGGI_TEMPERATURE_EP(ep_name, ep_id, clusters) \
	ZB_DECLARE_SIMPLE_DESC(3, 0); \
	ZB_AF_SIMPLE_DESC_TYPE(3, 0) simple_desc_##ep_name = { \
		ep_id, ZB_AF_HA_PROFILE_ID, ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID, 1, 0, 3, 0, \
		{ZB_ZCL_CLUSTER_ID_BASIC, ZB_ZCL_CLUSTER_ID_IDENTIFY, ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT}, \
	}; \
	ZBOSS_DEVICE_DECLARE_REPORTING_CTX(reporting_info##ep_name, ZB_ZCL_TEMP_MEASUREMENT_REPORT_ATTR_COUNT); \
	ZB_AF_DECLARE_ENDPOINT_DESC(ep_name, ep_id, ZB_AF_HA_PROFILE_ID, 0, NULL, \
		ZB_ZCL_ARRAY_SIZE(clusters, zb_zcl_cluster_desc_t), clusters, \
		(zb_af_simple_desc_1_1_t *)&simple_desc_##ep_name, \
		ZB_ZCL_TEMP_MEASUREMENT_REPORT_ATTR_COUNT, reporting_info##ep_name, 0, NULL)

#endif
