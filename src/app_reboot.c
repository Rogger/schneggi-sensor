#include "app_reboot.h"

#include <zephyr/sys/reboot.h>
#include <zephyr/sys/util.h>
#include <ram_pwrdn.h>

void app_reboot(void)
{
	if (IS_ENABLED(CONFIG_RAM_POWER_DOWN_LIBRARY)) {
		power_up_unused_ram();
	}
	sys_reboot(SYS_REBOOT_COLD);
}
