// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dc_score_update {
    uint8_t slot;
    uint8_t score_low;
    uint8_t score_middle;
    uint8_t score_high;
    uint8_t dirty_flags;
    uint16_t award;
    uint32_t score;
    uint32_t comparison_score;
} dc_score_update;

// Implements the ordinary, unsaturated 00:F55C score-addition path when the
// score-status helper takes its established early return. The function is
// pure: callers commit the returned writes.
int dc_accumulate_score_ordinary(
    const uint8_t* work_ram,
    size_t work_ram_size,
    dc_score_update* update);

#ifdef __cplusplus
} // extern "C"
#endif
