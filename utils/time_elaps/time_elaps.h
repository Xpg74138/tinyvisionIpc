#pragma once

#include <stdint.h>
#include <time.h>

uint32_t time_ms_elaps(uint32_t prev_tick);
uint32_t time_ms_get(void);

int64_t time_us_get(void);

