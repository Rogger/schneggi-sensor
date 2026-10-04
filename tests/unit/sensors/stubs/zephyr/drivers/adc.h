#ifndef TEST_ADC_H_
#define TEST_ADC_H_
#include <stdbool.h>
#include <zephyr/drivers/i2c.h>
struct adc_dt_spec {
	const struct device *dev;
	struct { bool differential; } channel_cfg;
};
struct adc_sequence { void *buffer; size_t buffer_size; };
int adc_sequence_init_dt(const struct adc_dt_spec *, struct adc_sequence *);
int adc_read(const struct device *, const struct adc_sequence *);
int adc_raw_to_millivolts_dt(const struct adc_dt_spec *, int32_t *);
#endif
