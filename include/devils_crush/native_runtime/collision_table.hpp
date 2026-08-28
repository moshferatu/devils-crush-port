// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace devils_crush::native_runtime {

// Mutable state owned by one target's collision-table animation. Positions
// are the table's native 16-bit units; the record operation moves them by
// whole units, independently of ball physics.
struct CollisionTableState {
    std::uint8_t pass_state = 0;
    std::uint8_t collision_event = 0;
    std::uint8_t orientation_flags = 0;
    std::uint8_t collision_entry_cursor = 0;
    std::uint8_t record_cursor = 0;
    std::uint16_t position_x = 0;
    std::uint16_t position_y = 0;

    [[nodiscard]] bool operator==(const CollisionTableState&) const = default;
};

// Immutable behavior configuration for one actor's collision-table stream.
// The selector identifies a ROM-derived table at the asset boundary; it is
// table data, not a logical address or CPU register value.
struct CollisionTableProgram {
    std::uint8_t selector = 0;

    [[nodiscard]] bool operator==(const CollisionTableProgram&) const = default;
};

// A decoded collision-table record. A high-bit header introduces a new event
// and uses `delta_after_header`; otherwise `header_or_delta` is itself the
// movement delta and `delta_after_header` is not consumed.
struct CollisionTableRecord {
    std::uint8_t table_entry = 0;
    std::uint8_t header_or_delta = 0;
    std::uint8_t delta_after_header = 0;

    [[nodiscard]] bool operator==(const CollisionTableRecord&) const = default;
};

// The two cursors carried by one target's collision-table stream. The ROM
// adapter resolves these from bytes; native actor state owns their progress.
struct CollisionTableCursor {
    std::uint8_t entry = 0;
    std::uint8_t motion = 0;

    [[nodiscard]] bool operator==(const CollisionTableCursor&) const = default;
};

enum class CollisionTableRecordDisposition {
    invalid_data,
    end_of_table,
    record_ready,
};

// A decoded record or control result supplied by the ROM asset boundary.
// This type contains no mapped addresses, so native actor state can own its
// cursor transition directly.
struct CollisionTableRecordResolution {
    CollisionTableRecordDisposition disposition =
        CollisionTableRecordDisposition::invalid_data;
    CollisionTableCursor cursor;
    CollisionTableRecord record;
    bool restarted = false;

    [[nodiscard]] bool operator==(const CollisionTableRecordResolution&) const
        = default;
};

struct CollisionTablePass {
    std::uint8_t index = 0;
    std::uint8_t next_pass_state = 0;
    bool alternating = false;
    bool completes_before_record = false;

    [[nodiscard]] bool operator==(const CollisionTablePass&) const = default;
};

enum class CollisionTableAdvanceDisposition {
    invalid_record,
    alternating_pass_complete,
    advanced,
};

struct CollisionTableAdvance {
    CollisionTableState previous;
    CollisionTableState state;
    CollisionTablePass pass;
    CollisionTableAdvanceDisposition disposition =
        CollisionTableAdvanceDisposition::invalid_record;
    std::uint8_t applied_delta = 0;
    bool event_changed = false;

    [[nodiscard]] bool operator==(const CollisionTableAdvance&) const = default;
};

// Selects the next table pass. A high pass-state bit alternates the low
// seven-bit index and completes before reading a record when it wraps to zero.
[[nodiscard]] CollisionTablePass select_collision_table_pass(
    std::uint8_t pass_state) noexcept;

// Applies one decoded collision-table record. The two record sentinels are
// reported without mutation. Header records select a new collision event and
// orient the target from the table entry; either record form advances the
// cursor and optionally moves the target by one unit on each axis.
[[nodiscard]] CollisionTableAdvance advance_collision_table(
    CollisionTableState state,
    CollisionTableRecord record) noexcept;

} // namespace devils_crush::native_runtime
