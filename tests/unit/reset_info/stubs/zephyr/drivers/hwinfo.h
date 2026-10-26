/* Reset cause flags with the values of Zephyr's <zephyr/drivers/hwinfo.h>. */
#define RESET_PIN (1U << 0)
#define RESET_SOFTWARE (1U << 1)
#define RESET_BROWNOUT (1U << 2)
#define RESET_POR (1U << 3)
#define RESET_WATCHDOG (1U << 4)
#define RESET_DEBUG (1U << 5)
#define RESET_LOW_POWER_WAKE (1U << 7)
#define RESET_CPU_LOCKUP (1U << 8)
