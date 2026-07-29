// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    DC_CACHED_POSITION_BYTE_COUNT = 6,
    DC_INTEGRATED_POSITION_BYTE_COUNT = 6,
};

typedef struct dc_position_integration {
    uint16_t cache_work_ram_offsets[DC_CACHED_POSITION_BYTE_COUNT];
    uint8_t cached_values[DC_CACHED_POSITION_BYTE_COUNT];
    uint16_t position_work_ram_offsets[DC_INTEGRATED_POSITION_BYTE_COUNT];
    uint8_t position_values[DC_INTEGRATED_POSITION_BYTE_COUNT];
    uint8_t cleared_motion_flags;
    uint8_t final_motion_flags;
    uint8_t first_increment_negative;
    uint8_t second_increment_negative;
    uint8_t integer_position_unchanged;
} dc_position_integration;

typedef struct dc_cached_position_restore {
    uint16_t work_ram_offsets[DC_CACHED_POSITION_BYTE_COUNT];
    uint8_t values[DC_CACHED_POSITION_BYTE_COUNT];
} dc_cached_position_restore;

typedef struct dc_motion_damping {
    uint16_t first_original;
    uint16_t second_original;
    uint16_t first_damped;
    uint16_t second_damped;
    uint16_t second_accelerated;
    uint8_t first_divisor;
    uint8_t second_divisor;
    uint8_t second_acceleration;
    uint8_t first_component_nonzero;
    uint8_t second_component_nonzero;
} dc_motion_damping;

typedef struct dc_motion_rotation {
    uint16_t first_input;
    uint16_t second_input;
    uint16_t cosine;
    uint16_t sine;
    uint16_t first_output;
    uint16_t second_output;
} dc_motion_rotation;

// Native equivalent of ROM routine 00:F1B4. It caches and integrates the two
// signed 16.8 position components for one indexed physics slot using the
// signed 8.8 increments at zero-page B9:BA and BB:BC. The function is pure:
// callers commit the returned writes.
int dc_integrate_position(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_position_integration* integration);

// Native equivalent of ROM routine 01:DD09. Each nonzero signed 8.8 motion
// component is reduced by its signed, truncating quotient using divisors 8C
// and 8D. The unsigned low-byte term at 8B is then added to the second
// component. The function is pure: callers commit the returned writes.
int dc_damp_and_accelerate_motion(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_motion_damping* damping);

// Native equivalent of ROM routine 01:D1A2. It rotates two signed 8.8
// motion components by signed 8.8 cosine/sine coefficients using the
// original sign-magnitude multiply and fractional truncation. The function
// is pure: callers commit the returned writes.
int dc_rotate_motion(
    const uint8_t* work_ram,
    size_t work_ram_size,
    dc_motion_rotation* rotation);

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
