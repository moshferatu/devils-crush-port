// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/game_logic/collision.h"

int dc_select_collision_table_pass(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    const uint16_t table_pointer,
    dc_collision_table_pass* const pass)
{
    if (work_ram == NULL || pass == NULL
        || work_ram_size < 0x2000U) {
        return 0;
    }

    pass->table_pointer = table_pointer;
    pass->collision_work_ram_offset =
        (uint16_t)(0x1880U + slot);
    pass->collision_state =
        work_ram[pass->collision_work_ram_offset];
    if ((pass->collision_state & 0x80U) == 0U) {
        pass->pass_index =
            (uint8_t)(pass->collision_state & 0x7FU);
        pass->collision_state_next = pass->collision_state;
        pass->returns_without_step = 0U;
        return 1;
    }
    pass->pass_index =
        (uint8_t)((pass->collision_state & 0x7FU) ^ 0x01U);
    pass->collision_state_next =
        (uint8_t)(pass->pass_index | 0x80U);
    pass->returns_without_step = (uint8_t)(
        pass->pass_index == 0U);
    return 1;
}

int dc_advance_collision_table_record(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    const uint16_t table_pointer,
    const uint8_t table_entry,
    const uint16_t motion_pointer,
    const uint8_t motion_header,
    const uint8_t motion_delta,
    dc_collision_table_record* const record)
{
    uint16_t horizontal;
    uint16_t vertical;
    uint8_t horizontal_direction;
    uint8_t vertical_direction;
    uint8_t effective_motion_delta;

    if (record == NULL
        || table_entry >= 0xFEU
        || motion_header == 0xC0U
        || !dc_select_collision_table_pass(
            work_ram,
            work_ram_size,
            slot,
            table_pointer,
            &record->pass)
        || record->pass.returns_without_step != 0U) {
        return 0;
    }

    record->motion_pointer = motion_pointer;
    record->event_work_ram_offset = (uint16_t)(0x1A80U + slot);
    record->orientation_work_ram_offset =
        (uint16_t)(0x1D00U + slot);
    record->cursor_work_ram_offset = (uint16_t)(0x1E80U + slot);
    record->horizontal_low_work_ram_offset =
        (uint16_t)(0x1180U + slot);
    record->horizontal_high_work_ram_offset =
        (uint16_t)(0x1200U + slot);
    record->vertical_low_work_ram_offset =
        (uint16_t)(0x1300U + slot);
    record->vertical_high_work_ram_offset =
        (uint16_t)(0x1380U + slot);
    record->table_entry = table_entry;
    record->motion_header = motion_header;
    record->motion_delta = motion_delta;
    record->orientation =
        work_ram[record->orientation_work_ram_offset];
    record->event_written = (uint8_t)(
        (motion_header & 0x80U) != 0U);
    if (record->event_written != 0U) {
        record->event = (uint8_t)(motion_header & 0x7FU);
        if ((table_entry & 0x80U) != 0U) {
            record->orientation =
                (uint8_t)(record->orientation & 0x7FU);
        } else {
            record->orientation =
                (uint8_t)(record->orientation | 0x80U);
        }
        record->cursor_after_header = (uint8_t)(
            work_ram[record->cursor_work_ram_offset] + 1U);
        record->cursor_after_delta =
            (uint8_t)(record->cursor_after_header + 1U);
        effective_motion_delta = motion_delta;
    } else {
        record->event = work_ram[record->event_work_ram_offset];
        record->cursor_after_header =
            work_ram[record->cursor_work_ram_offset];
        record->cursor_after_delta = (uint8_t)(
            record->cursor_after_header + 1U);
        effective_motion_delta = motion_header;
        record->motion_delta = effective_motion_delta;
    }

    horizontal = (uint16_t)(
        work_ram[record->horizontal_low_work_ram_offset]
        | ((uint16_t)work_ram[
               record->horizontal_high_work_ram_offset]
           << 8U));
    vertical = (uint16_t)(
        work_ram[record->vertical_low_work_ram_offset]
        | ((uint16_t)work_ram[
               record->vertical_high_work_ram_offset]
           << 8U));
    horizontal_direction =
        (uint8_t)(effective_motion_delta & 0x60U);
    vertical_direction =
        (uint8_t)(effective_motion_delta & 0x18U);
    record->horizontal_written = 0U;
    record->horizontal_high_written = 0U;
    record->vertical_written = 0U;
    record->vertical_high_written = 0U;
    if (horizontal_direction == 0x20U) {
        record->horizontal_high_written =
            (uint8_t)((horizontal & 0x00FFU) == 0x00FFU);
        horizontal = (uint16_t)(horizontal + 1U);
        record->horizontal_written = 1U;
    } else if (horizontal_direction == 0x40U) {
        record->horizontal_high_written =
            (uint8_t)((horizontal & 0x00FFU) == 0U);
        horizontal = (uint16_t)(horizontal - 1U);
        record->horizontal_written = 1U;
    }
    if (vertical_direction == 0x08U) {
        record->vertical_high_written =
            (uint8_t)((vertical & 0x00FFU) == 0x00FFU);
        vertical = (uint16_t)(vertical + 1U);
        record->vertical_written = 1U;
    } else if (vertical_direction == 0x10U) {
        record->vertical_high_written =
            (uint8_t)((vertical & 0x00FFU) == 0U);
        vertical = (uint16_t)(vertical - 1U);
        record->vertical_written = 1U;
    }
    record->horizontal_low = (uint8_t)(horizontal & 0x00FFU);
    record->horizontal_high = (uint8_t)(horizontal >> 8U);
    record->vertical_low = (uint8_t)(vertical & 0x00FFU);
    record->vertical_high = (uint8_t)(vertical >> 8U);
    return 1;
}

int dc_advance_collision_response(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_collision_response* const response)
{
    if (work_ram == NULL || response == NULL
        || work_ram_size < 0x2000U) {
        return 0;
    }

    response->response_work_ram_offset =
        (uint16_t)(0x1980U + slot);
    response->countdown_work_ram_offset =
        (uint16_t)(0x1A00U + slot);
    response->phase_work_ram_offset =
        (uint16_t)(0x1C80U + slot);
    response->event = work_ram[0x1A80U + slot];
    response->response_base =
        (uint8_t)((response->event & 0xFEU) << 1U);
    response->phase = work_ram[response->phase_work_ram_offset];
    response->response = (uint8_t)(
        response->response_base + (response->phase & 0x03U));
    response->countdown = (uint8_t)(
        work_ram[response->countdown_work_ram_offset] - 1U);
    response->countdown_reloaded =
        (uint8_t)(response->countdown == 0U);
    if (response->countdown_reloaded != 0U) {
        response->countdown = 4U;
        response->phase = (uint8_t)(response->phase + 1U);
    }
    return 1;
}
