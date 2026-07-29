// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dc_rng_update {
    uint8_t constant_sum_low;
    uint8_t constant_sum_high;
    uint8_t doubled_low;
    uint8_t doubled_high;
    uint8_t frame_sum_low;
    uint8_t mixed_high;
    uint8_t seed_low;
    uint8_t seed_high;
    uint8_t final_carry;
    uint8_t final_overflow;
} dc_rng_update;

// Native equivalent of ROM routine 00:E8B3. It advances the free-running
// 16-bit seed using the frame counter at zero-page 00. The function is pure:
// callers commit the returned scratch and seed writes.
int dc_advance_rng(
    const uint8_t* work_ram,
    size_t work_ram_size,
    dc_rng_update* update);

#ifdef __cplusplus
} // extern "C"
#endif
