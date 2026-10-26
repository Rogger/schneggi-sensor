#ifndef APP_REBOOT_H_
#define APP_REBOOT_H_

/* Stored across the reset and published as "sw<code>" (see README).
 * Values must fit in four bits and stay stable between releases.
 */
enum app_reboot_reason {
	APP_REBOOT_UNKNOWN = 0,         /* Not app_reboot(), e.g. a Zephyr fatal error. */
	APP_REBOOT_OTA_INIT = 1,        /* OTA storage or watchdog initialization failed. */
	APP_REBOOT_ZIGBEE_STARTUP = 2,  /* Stack startup or commissioning failed. */
	APP_REBOOT_SENSOR_ALARM = 3,    /* Measurement alarm could not be scheduled. */
	APP_REBOOT_REJOIN_ALARM = 4,    /* Rejoin retry alarm could not be scheduled. */
	APP_REBOOT_WATCHDOG_ALARM = 5,  /* Watchdog service alarm could not be scheduled. */
	APP_REBOOT_TRIAL_FAILED = 6,    /* Trial image failed its startup checks. */
	APP_REBOOT_OTA_STALLED = 7,     /* No accepted OTA block for five minutes. */
	APP_REBOOT_OTA_NO_RESPONSE = 8, /* Upgrade End response missing. */
	APP_REBOOT_OTA_INSTALL = 9,     /* Downloaded image is ready to install. */
	APP_REBOOT_OTA_LEAVE = 10,      /* Network leave during an OTA session. */
};

/* Restore powered-down RAM before handing control to the bootloader. */
void app_reboot(enum app_reboot_reason reason);

#endif
