#ifndef APP_OTA_H
#define APP_OTA_H

#include <stdbool.h>
#include <stdint.h>
#include <zboss_api.h>

int app_ota_init(void);
void app_ota_signal(zb_bufid_t bufid);
void app_ota_set_long_poll(uint32_t interval_ms);
void app_ota_startup_ready(bool peripherals_ready);

#endif
