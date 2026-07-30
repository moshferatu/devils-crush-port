// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/game_logic/collision.h"

int dc_classify_collision_scan_vertical_reject(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t target_slot,
    dc_collision_scan_vertical_reject* const reject)
{
    uint16_t subtrahend;
    uint16_t difference;
    uint16_t magnitude;

    if (work_ram == NULL || reject == NULL
        || work_ram_size < 0x2000U) {
        return 0;
    }

    reject->source_low = work_ram[0x0066U];
    reject->source_high = work_ram[0x0067U];
    reject->target_low = work_ram[0x1300U + (size_t)target_slot];
    reject->target_high = work_ram[0x1380U + (size_t)target_slot];
    reject->branch = DC_COLLISION_SCAN_VERTICAL_DEFER;
    reject->magnitude_low = 0U;
    reject->magnitude_high = 0U;

    reject->low_subtract_carry =
        (uint8_t)(reject->source_low >= reject->target_low);
    reject->difference_low =
        (uint8_t)(reject->source_low - reject->target_low);
    subtrahend = (uint16_t)reject->target_high
        + (uint16_t)(reject->low_subtract_carry == 0U);
    reject->high_subtract_carry =
        (uint8_t)((uint16_t)reject->source_high >= subtrahend);
    reject->difference_high = (uint8_t)(
        (uint16_t)reject->source_high - subtrahend);

    if (reject->difference_high == 0U) {
        if (reject->difference_low >= 0x80U) {
            reject->branch = DC_COLLISION_SCAN_VERTICAL_SAME_PAGE_LOW;
        }
        return 1;
    }
    if ((reject->difference_high & 0x80U) == 0U) {
        reject->branch = DC_COLLISION_SCAN_VERTICAL_POSITIVE_PAGE;
        return 1;
    }

    difference = (uint16_t)reject->difference_low
        | ((uint16_t)reject->difference_high << 8U);
    magnitude = (uint16_t)(0U - difference);
    reject->magnitude_low = (uint8_t)magnitude;
    reject->magnitude_high = (uint8_t)(magnitude >> 8U);
    if (reject->magnitude_high != 0U) {
        reject->branch = DC_COLLISION_SCAN_VERTICAL_NEGATIVE_PAGE;
    } else if (reject->magnitude_low >= 0x80U) {
        reject->branch = DC_COLLISION_SCAN_VERTICAL_NEGATIVE_LOW;
    }
    return 1;
}

int dc_prepare_collision_geometry(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t target_slot,
    const dc_collision_geometry_tables* const tables,
    dc_collision_geometry* const geometry)
{
    uint16_t position;
    uint16_t collision;

    if (work_ram == NULL || tables == NULL || geometry == NULL
        || target_slot >= 0x80U || work_ram_size < 0x2000U
        || !dc_classify_collision_scan_vertical_reject(
            work_ram,
            work_ram_size,
            target_slot,
            &geometry->vertical)) {
        return 0;
    }

    geometry->directory_index =
        work_ram[0x1900U + (size_t)target_slot];
    geometry->phase_index = (uint8_t)(
        work_ram[0x1980U + (size_t)target_slot] << 1U);
    geometry->position_rejected = 0U;
    geometry->vertical_offset_negative =
        (uint8_t)((tables->vertical_offset & 0x80U) != 0U);

    position = (uint16_t)(
        ((uint16_t)work_ram[0x1200U + (size_t)target_slot] << 8U)
        | work_ram[0x1180U + (size_t)target_slot]);
    position = (uint16_t)(position - 0x0020U);
    geometry->position_low = (uint8_t)position;
    geometry->position_high = (uint8_t)(position >> 8U);
    geometry->adjusted_position_low = geometry->position_low;
    geometry->collision_low = 0U;
    geometry->collision_high = 0U;

    if (geometry->vertical.branch
            != DC_COLLISION_SCAN_VERTICAL_DEFER
        || geometry->position_high != 0U) {
        geometry->position_rejected =
            (uint8_t)(geometry->position_high != 0U);
        return 1;
    }

    geometry->adjusted_position_low = (uint8_t)(
        geometry->position_low + tables->horizontal_offset);
    collision = (uint16_t)(
        ((uint16_t)work_ram[0x1380U + (size_t)target_slot] << 8U)
        | work_ram[0x1300U + (size_t)target_slot]);
    collision = (uint16_t)(
        collision + (int8_t)tables->vertical_offset);
    geometry->collision_low = (uint8_t)collision;
    geometry->collision_high = (uint8_t)(collision >> 8U);
    return 1;
}

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
