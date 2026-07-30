// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum dc_actor_camera_path {
    DC_ACTOR_CAMERA_EMPTY = 0,
    DC_ACTOR_CAMERA_BELOW = 1,
    DC_ACTOR_CAMERA_INSIDE = 2,
    DC_ACTOR_CAMERA_ABOVE = 3
} dc_actor_camera_path;

typedef struct dc_actor_camera_activation {
    uint8_t occupancy;
    uint8_t position_low;
    uint8_t position_high;
    uint8_t anchor_low;
    uint8_t anchor_high;
    uint8_t flags_before;
    uint8_t lower_low;
    uint8_t lower_high;
    uint8_t upper_low;
    uint8_t upper_high;
    uint8_t flags_after;
    dc_actor_camera_path path;
} dc_actor_camera_activation;

#define DC_ACTOR_SLOT_FIELD_COUNT 32U

typedef struct dc_actor_slot_reset {
    uint16_t work_ram_offsets[DC_ACTOR_SLOT_FIELD_COUNT];
    uint8_t slot;
} dc_actor_slot_reset;

// Native classification performed by ROM routine 00:F4FC. Empty slots are
// unchanged. Occupied slots set camera flag bit 7 exactly when their signed
// integer Y coordinate lies in [anchor - 48, anchor + 240).
int dc_classify_actor_camera_activation(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_actor_camera_activation* activation);

// Native destination map for ROM routine 01:C574. The routine clears one
// actor slot across 32 parallel arrays spaced 0x80 bytes apart.
int dc_prepare_actor_slot_reset(
    uint8_t slot,
    dc_actor_slot_reset* reset);

#ifdef __cplusplus
} // extern "C"
#endif
