
#define ZB_DEVICE_VERSION 1

/** Number of Cluster attributes */
#define ZB_DIMMABLE_LIGHT_IN_CLUSTER_NUM 6
#define ZB_DIMMABLE_LIGHT_OUT_CLUSTER_NUM 0

/** Number of attribute for reporting */
#define ZB_DIMMABLE_LIGHT_REPORT_ATTR_COUNT \
	(ZB_ZCL_TEMP_MEASUREMENT_REPORT_ATTR_COUNT + ZB_ZCL_REL_HUMIDITY_MEASUREMENT_REPORT_ATTR_COUNT + ZB_ZCL_CONCENTRATION_MEASUREMENT_REPORT_ATTR_COUNT + ZB_ZCL_POWER_CONFIG_REPORT_ATTR_COUNT)

#define ZB_DECLARE_DIMMABLE_LIGHT_CLUSTER_LIST(                                  \
	cluster_list_name,                                                           \
	basic_attr_list,                                                             \
	identify_attr_list,                                                          \
	temperature_measure_attr_list,                                               \
	humidity_measure_attr_list,                                                  \
	concentration_measure_attr_list,                                             \
	power_config_attr_list)                                                      \
	zb_zcl_cluster_desc_t cluster_list_name[] =                                  \
		{                                                                        \
			ZB_ZCL_CLUSTER_DESC(                                                 \
				ZB_ZCL_CLUSTER_ID_IDENTIFY,                                      \
				ZB_ZCL_ARRAY_SIZE(identify_attr_list, zb_zcl_attr_t),            \
				(identify_attr_list),                                            \
				ZB_ZCL_CLUSTER_SERVER_ROLE,                                      \
				ZB_ZCL_MANUF_CODE_INVALID),                                      \
			ZB_ZCL_CLUSTER_DESC(                                                 \
				ZB_ZCL_CLUSTER_ID_BASIC,                                         \
				ZB_ZCL_ARRAY_SIZE(basic_attr_list, zb_zcl_attr_t),               \
				(basic_attr_list),                                               \
				ZB_ZCL_CLUSTER_SERVER_ROLE,                                      \
				ZB_ZCL_MANUF_CODE_INVALID),                                      \
			ZB_ZCL_CLUSTER_DESC(                                                 \
				ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,                              \
				ZB_ZCL_ARRAY_SIZE(temperature_measure_attr_list, zb_zcl_attr_t), \
				(temperature_measure_attr_list),                                 \
				ZB_ZCL_CLUSTER_SERVER_ROLE,                                      \
				ZB_ZCL_MANUF_CODE_INVALID),                                      \
			ZB_ZCL_CLUSTER_DESC(                                                 \
				ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT,                      \
				ZB_ZCL_ARRAY_SIZE(humidity_measure_attr_list, zb_zcl_attr_t),    \
				(humidity_measure_attr_list),                                    \
				ZB_ZCL_CLUSTER_SERVER_ROLE,                                      \
				ZB_ZCL_MANUF_CODE_INVALID),                                      \
			ZB_ZCL_CLUSTER_DESC(                                                     \
				ZB_ZCL_CLUSTER_ID_CONCENTRATION_MEASUREMENT,                          \
				ZB_ZCL_ARRAY_SIZE(concentration_measure_attr_list, zb_zcl_attr_t),    \
				(concentration_measure_attr_list),                                    \
				ZB_ZCL_CLUSTER_SERVER_ROLE,                                          \
				ZB_ZCL_MANUF_CODE_INVALID),                                          \
			ZB_ZCL_CLUSTER_DESC(                                                 \
				ZB_ZCL_CLUSTER_ID_POWER_CONFIG,                                  \
				ZB_ZCL_ARRAY_SIZE(power_config_attr_list, zb_zcl_attr_t),        \
				(power_config_attr_list),                                        \
				ZB_ZCL_CLUSTER_SERVER_ROLE,                                      \
				ZB_ZCL_MANUF_CODE_INVALID)}

#define ZB_ZCL_DECLARE_HA_DIMMABLE_LIGHT_SIMPLE_DESC(ep_name, ep_id, in_clust_num, out_clust_num) \
	ZB_DECLARE_SIMPLE_DESC(in_clust_num, out_clust_num);                                          \
	ZB_AF_SIMPLE_DESC_TYPE(in_clust_num, out_clust_num)                                           \
	simple_desc_##ep_name =                                                                       \
		{                                                                                         \
			ep_id,                                                                                \
			ZB_AF_HA_PROFILE_ID,                                                                  \
			ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID,                                                   \
			ZB_DEVICE_VERSION,                                                                    \
			0,                                                                                    \
			in_clust_num,                                                                         \
			out_clust_num,                                                                        \
			{ZB_ZCL_CLUSTER_ID_BASIC,                                                             \
			 ZB_ZCL_CLUSTER_ID_IDENTIFY,                                                          \
			 ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,                                                  \
			 ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT,                                          \
			 ZB_ZCL_CLUSTER_ID_CONCENTRATION_MEASUREMENT,										  \
			 ZB_ZCL_CLUSTER_ID_POWER_CONFIG}}

#define ZB_DECLARE_DIMMABLE_LIGHT_EP(ep_name, ep_id, cluster_list)                                                     \
	ZB_ZCL_DECLARE_HA_DIMMABLE_LIGHT_SIMPLE_DESC(ep_name, ep_id,                                                       \
												 ZB_DIMMABLE_LIGHT_IN_CLUSTER_NUM, ZB_DIMMABLE_LIGHT_OUT_CLUSTER_NUM); \
	ZBOSS_DEVICE_DECLARE_REPORTING_CTX(reporting_info##ep_name,                                                        \
									   ZB_DIMMABLE_LIGHT_REPORT_ATTR_COUNT);                                           \
	ZB_AF_DECLARE_ENDPOINT_DESC(ep_name, ep_id, ZB_AF_HA_PROFILE_ID,                                                   \
								0,                                                                                     \
								NULL,                                                                                  \
								ZB_ZCL_ARRAY_SIZE(cluster_list, zb_zcl_cluster_desc_t), cluster_list,                  \
								(zb_af_simple_desc_1_1_t *)&simple_desc_##ep_name,                                     \
								ZB_DIMMABLE_LIGHT_REPORT_ATTR_COUNT,                                                   \
								reporting_info##ep_name,                                                               \
								0,                                                                                     \
								NULL)
