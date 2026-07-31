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

typedef enum dc_collision_scan_vertical_branch {
    DC_COLLISION_SCAN_VERTICAL_DEFER = 0,
    DC_COLLISION_SCAN_VERTICAL_POSITIVE_PAGE = 1,
    DC_COLLISION_SCAN_VERTICAL_SAME_PAGE_LOW = 2,
    DC_COLLISION_SCAN_VERTICAL_NEGATIVE_PAGE = 3,
    DC_COLLISION_SCAN_VERTICAL_NEGATIVE_LOW = 4
} dc_collision_scan_vertical_branch;

typedef struct dc_collision_scan_vertical_reject {
    uint8_t source_low;
    uint8_t source_high;
    uint8_t target_low;
    uint8_t target_high;
    uint8_t difference_low;
    uint8_t difference_high;
    uint8_t magnitude_low;
    uint8_t magnitude_high;
    uint8_t low_subtract_carry;
    uint8_t high_subtract_carry;
    dc_collision_scan_vertical_branch branch;
} dc_collision_scan_vertical_reject;

typedef struct dc_collision_geometry_tables {
    uint8_t first_directory_low;
    uint8_t first_directory_high;
    uint8_t second_directory_low;
    uint8_t second_directory_high;
    uint8_t first_geometry_low;
    uint8_t first_geometry_high;
    uint8_t second_geometry_low;
    uint8_t second_geometry_high;
    uint8_t horizontal_offset;
    uint8_t vertical_offset;
    uint8_t horizontal_limit;
    uint8_t vertical_limit;
} dc_collision_geometry_tables;

typedef struct dc_collision_geometry {
    dc_collision_scan_vertical_reject vertical;
    uint8_t directory_index;
    uint8_t phase_index;
    uint8_t position_low;
    uint8_t position_high;
    uint8_t adjusted_position_low;
    uint8_t collision_low;
    uint8_t collision_high;
    uint8_t position_rejected;
    uint8_t vertical_offset_negative;
} dc_collision_geometry;

typedef enum dc_collision_scan_bounds_branch {
    DC_COLLISION_SCAN_BOUNDS_DEFER = 0,
    DC_COLLISION_SCAN_BOUNDS_HORIZONTAL = 1,
    DC_COLLISION_SCAN_BOUNDS_VERTICAL_NEGATIVE = 2,
    DC_COLLISION_SCAN_BOUNDS_VERTICAL_PAGE = 3,
    DC_COLLISION_SCAN_BOUNDS_VERTICAL_LOW = 4
} dc_collision_scan_bounds_branch;

typedef struct dc_collision_scan_bounds_reject {
    uint8_t horizontal_source;
    uint8_t horizontal_target;
    uint8_t horizontal_limit;
    uint8_t horizontal_difference;
    uint8_t vertical_source_low;
    uint8_t vertical_source_high;
    uint8_t vertical_target_low;
    uint8_t vertical_target_high;
    uint8_t vertical_limit;
    uint8_t vertical_difference_low;
    uint8_t vertical_difference_high;
    uint8_t vertical_low_subtract_carry;
    uint8_t vertical_high_subtract_carry;
    dc_collision_scan_bounds_branch branch;
} dc_collision_scan_bounds_reject;

typedef struct dc_collision_mask_lookup {
    uint16_t first_table_pointer;
    uint16_t second_table_pointer;
    uint16_t mask_base_pointer;
    uint16_t mask_pointer;
    uint8_t table_index;
    uint8_t first_table_value;
    uint8_t second_table_value;
    uint8_t mask_row;
    uint8_t mask_column;
    uint8_t mask_value;
    uint8_t column_bit;
    uint8_t collision;
} dc_collision_mask_lookup;

typedef struct dc_collision_mask_coordinates {
    uint8_t side_flags_updated;
    uint8_t side_flags;
    uint8_t mask_column;
    uint8_t horizontal_cell;
    uint8_t rounded_horizontal_span;
    uint8_t mask_row;
    uint8_t vertical_cell;
} dc_collision_mask_coordinates;

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

// Native classification of CC1A's early vertical-separation returns. These
// paths compare the source coordinate in zero page $66:$67 with the indexed
// target coordinate at $3300/$3380,Y and return before collision-mask
// metadata is read. Near targets are reported as DEFER for assembly fallback.
int dc_classify_collision_scan_vertical_reject(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t target_slot,
    dc_collision_scan_vertical_reject* reject);

// Native state model for the near-target continuation of ROM leaf 01:CCD5.
// The host resolves the routine's ROM-owned pointer directories and
// descriptor through the current MPR map and supplies their selected bytes.
// The function remains pure and reports non-near vertical states unchanged
// for assembly fallback.
int dc_prepare_collision_geometry(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t target_slot,
    const dc_collision_geometry_tables* tables,
    dc_collision_geometry* geometry);

// Native classification of CC1A's post-CCD5 bounds checks. CCD5 leaves the
// adjusted candidate coordinate and one-sided limits in $68-$6C. The parent
// rejects horizontal overflow first, then negative, multi-page, or
// out-of-range vertical differences. In-range candidates are reported as
// DEFER for the still-emulated collision-mask and contact paths.
int dc_classify_collision_scan_bounds_reject(
    const uint8_t* work_ram,
    size_t work_ram_size,
    dc_collision_scan_bounds_reject* reject);

// Native state model for CC41-CC97 after all geometry bounds pass. It
// derives the optional side flag, sub-cell mask indices, cell coordinates,
// and the two unsigned-multiply inputs consumed before CC98.
int dc_prepare_collision_mask_coordinates(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t target_slot,
    dc_collision_mask_coordinates* coordinates);

// Native state model for CC98's final collision-mask lookup. The host
// resolves the three pointer-derived ROM bytes and the column bit through
// the current MPR map; this pure function derives all intermediate indices,
// pointers, and the final contact decision.
int dc_resolve_collision_mask_lookup(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t first_table_value,
    uint8_t second_table_value,
    uint8_t mask_value,
    uint8_t column_bit,
    dc_collision_mask_lookup* lookup);

#ifdef __cplusplus
}
#endif
