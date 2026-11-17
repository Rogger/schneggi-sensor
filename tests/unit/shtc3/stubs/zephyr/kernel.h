#include <stdint.h>
#define K_MSEC(ms) (ms)
#define K_SECONDS(s) ((s) * 1000)
int k_sleep(int duration);
int64_t k_uptime_get(void);
