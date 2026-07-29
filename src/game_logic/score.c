// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/game_logic/score.h"

#include <stddef.h>
#include <stdint.h>

enum {
    DC_SCORE_LIMIT = 0x98967F,
    DC_SCORE_SLOT_COUNT = 7,
};

int dc_accumulate_score_ordinary(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    dc_score_update* const update)
{
    if (work_ram == NULL || update == NULL || work_ram_size < 0x2000U
        || work_ram[0x0CC7U] != 0U || work_ram[0x0055U] != 0U
        || work_ram[0x022DU] != 0U
        || (work_ram[0x0CC0U] & 0x40U) != 0U) {
        return 0;
    }

    const uint8_t slot = work_ram[0x0251U];
    if (slot >= DC_SCORE_SLOT_COUNT) {
        return 0;
    }

    const uint32_t previous =
        (uint32_t)work_ram[0x0205U + slot]
        | ((uint32_t)work_ram[0x020CU + slot] << 8U)
        | ((uint32_t)work_ram[0x0213U + slot] << 16U);
    const uint16_t award =
        (uint16_t)((uint16_t)work_ram[0x002FU]
            | ((uint16_t)work_ram[0x0030U] << 8U));
    const uint32_t score = previous + (uint32_t)award;
    if (score >= DC_SCORE_LIMIT) {
        return 0;
    }
    const uint32_t comparison_score =
        (uint32_t)work_ram[0x0200U]
        | ((uint32_t)work_ram[0x0207U] << 8U)
        | ((uint32_t)work_ram[0x020EU] << 16U);
    if (score >= comparison_score) {
        return 0;
    }

    update->slot = slot;
    update->score_low = (uint8_t)(score & 0xFFU);
    update->score_middle = (uint8_t)((score >> 8U) & 0xFFU);
    update->score_high = (uint8_t)((score >> 16U) & 0xFFU);
    update->dirty_flags = (uint8_t)(work_ram[0x0CC0U] | 0x01U);
    update->award = award;
    update->score = score;
    update->comparison_score = comparison_score;
    return 1;
}
