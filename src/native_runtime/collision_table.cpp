// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/collision_table.hpp"

namespace devils_crush::native_runtime {

CollisionTablePass select_collision_table_pass(
    const std::uint8_t pass_state) noexcept
{
    if ((pass_state & 0x80U) == 0U) {
        return {
            .index = static_cast<std::uint8_t>(pass_state & 0x7FU),
            .next_pass_state = pass_state,
        };
    }

    const auto index = static_cast<std::uint8_t>((pass_state & 0x7FU) ^ 0x01U);
    return {
        .index = index,
        .next_pass_state = static_cast<std::uint8_t>(index | 0x80U),
        .alternating = true,
        .completes_before_record = index == 0U,
    };
}

CollisionTableAdvance advance_collision_table(
    CollisionTableState state,
    const CollisionTableRecord record) noexcept
{
    const auto previous = state;
    CollisionTableAdvance advance{.previous = previous, .state = state};
    if (record.table_entry >= 0xFEU || record.header_or_delta == 0xC0U) {
        return advance;
    }

    advance.pass = select_collision_table_pass(state.pass_state);
    state.pass_state = advance.pass.next_pass_state;
    if (advance.pass.completes_before_record) {
        advance.state = state;
        advance.disposition =
            CollisionTableAdvanceDisposition::alternating_pass_complete;
        return advance;
    }

    const auto is_header = (record.header_or_delta & 0x80U) != 0U;
    if (is_header) {
        state.collision_event = static_cast<std::uint8_t>(
            record.header_or_delta & 0x7FU);
        if ((record.table_entry & 0x80U) != 0U) {
            state.orientation_flags = static_cast<std::uint8_t>(
                state.orientation_flags & 0x7FU);
        } else {
            state.orientation_flags = static_cast<std::uint8_t>(
                state.orientation_flags | 0x80U);
        }
        state.record_cursor = static_cast<std::uint8_t>(
            state.record_cursor + 2U);
        advance.applied_delta = record.delta_after_header;
        advance.event_changed = true;
    } else {
        state.record_cursor = static_cast<std::uint8_t>(
            state.record_cursor + 1U);
        advance.applied_delta = record.header_or_delta;
    }

    switch (advance.applied_delta & 0x60U) {
    case 0x20U:
        state.position_x = static_cast<std::uint16_t>(state.position_x + 1U);
        break;
    case 0x40U:
        state.position_x = static_cast<std::uint16_t>(state.position_x - 1U);
        break;
    default:
        break;
    }
    switch (advance.applied_delta & 0x18U) {
    case 0x08U:
        state.position_y = static_cast<std::uint16_t>(state.position_y + 1U);
        break;
    case 0x10U:
        state.position_y = static_cast<std::uint16_t>(state.position_y - 1U);
        break;
    default:
        break;
    }

    advance.state = state;
    advance.disposition = CollisionTableAdvanceDisposition::advanced;
    return advance;
}

} // namespace devils_crush::native_runtime
