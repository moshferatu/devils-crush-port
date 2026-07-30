// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/game_logic/actor.h"

int dc_classify_actor_camera_activation(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_actor_camera_activation* const activation)
{
    uint16_t anchor;
    uint16_t lower;
    uint16_t position;
    uint16_t upper;

    if (work_ram == NULL || activation == NULL
        || work_ram_size < 0x2000U) {
        return 0;
    }

    activation->occupancy = work_ram[0x1000U + (size_t)slot];
    activation->position_low =
        work_ram[0x1300U + (size_t)slot];
    activation->position_high =
        work_ram[0x1380U + (size_t)slot];
    activation->anchor_low = work_ram[0x00A9U];
    activation->anchor_high = work_ram[0x00AAU];
    activation->flags_before =
        work_ram[0x1080U + (size_t)slot];

    anchor = (uint16_t)(
        (uint16_t)activation->anchor_low
        | ((uint16_t)activation->anchor_high << 8U));
    lower = (uint16_t)(anchor - 0x0030U);
    upper = (uint16_t)(anchor + 0x00F0U);
    position = (uint16_t)(
        (uint16_t)activation->position_low
        | ((uint16_t)activation->position_high << 8U));
    activation->lower_low = (uint8_t)lower;
    activation->lower_high = (uint8_t)(lower >> 8U);
    activation->upper_low = (uint8_t)upper;
    activation->upper_high = (uint8_t)(upper >> 8U);
    activation->flags_after = activation->flags_before;

    if (activation->occupancy == 0U) {
        activation->path = DC_ACTOR_CAMERA_EMPTY;
    } else if (((uint16_t)(position - lower) & 0x8000U) != 0U) {
        activation->flags_after =
            (uint8_t)(activation->flags_before & 0x7FU);
        activation->path = DC_ACTOR_CAMERA_BELOW;
    } else if (((uint16_t)(position - upper) & 0x8000U) != 0U) {
        activation->flags_after =
            (uint8_t)(activation->flags_before | 0x80U);
        activation->path = DC_ACTOR_CAMERA_INSIDE;
    } else {
        activation->flags_after =
            (uint8_t)(activation->flags_before & 0x7FU);
        activation->path = DC_ACTOR_CAMERA_ABOVE;
    }
    return 1;
}

int dc_prepare_actor_slot_reset(
    const uint8_t slot,
    dc_actor_slot_reset* const reset)
{
    size_t index;

    if (reset == NULL || slot > 0x7FU) {
        return 0;
    }

    reset->slot = slot;
    for (index = 0U; index < DC_ACTOR_SLOT_FIELD_COUNT; ++index) {
        reset->work_ram_offsets[index] = (uint16_t)(
            0x1000U + (uint16_t)slot + (uint16_t)(index * 0x80U));
    }
    return 1;
}
