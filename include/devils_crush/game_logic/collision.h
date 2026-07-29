// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dc_collision_response {
    uint16_t response_work_ram_offset;
    uint16_t countdown_work_ram_offset;
    uint16_t phase_work_ram_offset;
    uint8_t event;
    uint8_t response_base;
    uint8_t response;
    uint8_t countdown;
    uint8_t phase;
    uint8_t countdown_reloaded;
} dc_collision_response;

typedef struct dc_collision_table_pass {
    uint16_t table_pointer;
    uint16_t collision_work_ram_offset;
    uint8_t collision_state;
    uint8_t collision_state_next;
    uint8_t pass_index;
    uint8_t returns_without_step;
} dc_collision_table_pass;

typedef struct dc_collision_table_record {
    dc_collision_table_pass pass;
    uint16_t motion_pointer;
    uint16_t event_work_ram_offset;
    uint16_t orientation_work_ram_offset;
    uint16_t cursor_work_ram_offset;
    uint16_t horizontal_low_work_ram_offset;
    uint16_t horizontal_high_work_ram_offset;
    uint16_t vertical_low_work_ram_offset;
    uint16_t vertical_high_work_ram_offset;
    uint8_t table_entry;
    uint8_t motion_header;
    uint8_t motion_delta;
    uint8_t event;
    uint8_t orientation;
    uint8_t cursor_after_header;
    uint8_t cursor_after_delta;
    uint8_t horizontal_low;
    uint8_t horizontal_high;
    uint8_t vertical_low;
    uint8_t vertical_high;
    uint8_t event_written;
    uint8_t horizontal_written;
    uint8_t horizontal_high_written;
    uint8_t vertical_written;
    uint8_t vertical_high_written;
} dc_collision_table_record;

// Native collision-table pass selection used by ROM routine 03:6966.
// The table pointer is supplied by the project-owned ROM metadata boundary.
// A negative collision-state byte alternates its low seven-bit pass index;
// wrapping that index to zero returns before interpreting another record.
// The function is pure: callers commit the returned collision-state write.
int dc_select_collision_table_pass(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    uint16_t table_pointer,
    dc_collision_table_pass* pass);

// Native bounded-record step used by ROM routine 03:6966 after pass
// selection. ROM-owned table metadata is supplied explicitly. The function
// applies an optional event/orientation header, signed unit motion flags,
// and the record-cursor advance without mutating work RAM. A byte with its
// high bit clear is a headerless continuation step and advances once.
int dc_advance_collision_table_record(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    uint16_t table_pointer,
    uint8_t table_entry,
    uint16_t motion_pointer,
    uint8_t motion_header,
    uint8_t motion_delta,
    dc_collision_table_record* record);

// Native equivalent of ROM routine 03:7FB1. It derives an actor response
// from the collision event and low phase bits, decrements the response
// countdown, and advances the phase when the countdown reloads. The
// function is pure: callers commit the returned writes.
int dc_advance_collision_response(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_collision_response* response);

#ifdef __cplusplus
}
#endif
