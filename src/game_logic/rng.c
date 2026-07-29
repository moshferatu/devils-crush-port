// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/game_logic/rng.h"

#include <stddef.h>
#include <stdint.h>

static uint8_t add_byte(
    const uint8_t left,
    const uint8_t right,
    uint8_t* const carry,
    uint8_t* const overflow)
{
    const uint16_t sum =
        (uint16_t)left + (uint16_t)right + (uint16_t)(*carry != 0U);
    const uint8_t result = (uint8_t)sum;
    *carry = (uint8_t)(sum > 0xFFU);
    *overflow = (uint8_t)(
        ((~(left ^ right) & (left ^ result)) & 0x80U) != 0U);
    return result;
}

int dc_advance_rng(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    dc_rng_update* const update)
{
    if (work_ram == NULL || update == NULL || work_ram_size < 0x2000U) {
        return 0;
    }

    const uint8_t seed_low = work_ram[0x022AU];
    const uint8_t seed_high = work_ram[0x022BU];
    uint8_t carry = 0U;
    uint8_t overflow = 0U;
    update->constant_sum_low =
        add_byte(seed_low, 0x3FU, &carry, &overflow);
    update->constant_sum_high =
        add_byte(seed_high, 0x50U, &carry, &overflow);

    carry = (uint8_t)((update->constant_sum_low & 0x80U) != 0U);
    update->doubled_low =
        (uint8_t)(update->constant_sum_low << 1U);
    update->doubled_high = (uint8_t)(
        (uint8_t)(update->constant_sum_high << 1U) | carry);

    carry = 0U;
    update->frame_sum_low =
        add_byte(seed_low, work_ram[0x0000U], &carry, &overflow);
    update->mixed_high =
        add_byte(seed_high, update->doubled_high, &carry, &overflow);

    carry = 0U;
    update->seed_high =
        add_byte(seed_low, update->mixed_high, &carry, &overflow);
    update->seed_low = update->frame_sum_low;
    update->final_carry = carry;
    update->final_overflow = overflow;
    return 1;
}
