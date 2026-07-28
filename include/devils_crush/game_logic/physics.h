// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    DC_CACHED_POSITION_BYTE_COUNT = 6,
};

typedef struct dc_cached_position_restore {
    uint16_t work_ram_offsets[DC_CACHED_POSITION_BYTE_COUNT];
    uint8_t values[DC_CACHED_POSITION_BYTE_COUNT];
} dc_cached_position_restore;

// Native equivalent of ROM routine 00:F241. It restores one indexed
// physics slot from the six-byte pre-integration cache at work-RAM offsets
// 0085-008A. The function is pure: callers commit the returned writes.
int dc_restore_cached_position(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_cached_position_restore* restore);

#ifdef __cplusplus
} // extern "C"
#endif
