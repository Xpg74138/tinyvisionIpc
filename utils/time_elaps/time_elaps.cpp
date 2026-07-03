#include "time_elaps.h"
#include <time.h>

uint32_t time_ms_elaps(uint32_t prev_tick)
{
    uint32_t act_time = time_ms_get();

    /*If there is no overflow in sys_time simple subtract*/
    if (act_time >= prev_tick) {
        prev_tick = act_time - prev_tick;
    }
    else {
        prev_tick = UINT32_MAX - prev_tick + 1;
        prev_tick += act_time;
    }

    return prev_tick;
}

uint32_t time_ms_get(void)
{
    static uint64_t start_ms = 0;
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
    if (start_ms == 0) {
		
        //start_ms = clock() / (CLOCKS_PER_SEC / 5000);
        start_ms = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    }

    uint64_t now_ms;
    //now_ms = clock() / (CLOCKS_PER_SEC / 5000);
    now_ms = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

    uint32_t time_ms = now_ms - start_ms;
    return time_ms;
}

int64_t time_us_get(void)
{
    static uint64_t start_us = 0;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    if (start_us == 0) {

        //start_ms = clock() / (CLOCKS_PER_SEC / 5000);
        start_us = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    }

    uint64_t now_us;
    //now_ms = clock() / (CLOCKS_PER_SEC / 5000);
    now_us = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;

    uint32_t time_us = now_us - start_us;

    return time_us;
}
