#include "app_reboot.h"
#include "app_reset_info.h"

#include <zephyr/sys/reboot.h>
#include <zephyr/sys/util.h>
#include <ram_pwrdn.h>

void app_reboot(enum app_reboot_reason reason)
{
	app_reset_info_set_reason((uint8_t)reason);
	if (IS_ENABLED(CONFIG_RAM_POWER_DOWN_LIBRARY)) {
		power_up_unused_ram();
	}
	sys_reboot(SYS_REBOOT_COLD);
}
