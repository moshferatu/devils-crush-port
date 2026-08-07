// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/game_logic/sprite.h"

#include <string.h>

// Work RAM is addressed from logical $2000 on this machine.
#define DC_WORK_RAM_BASE 0x2000U

static int dc_sprite_rom_byte(
    const uint8_t* const sprite_rom,
    const size_t sprite_rom_size,
    const uint16_t address,
    uint8_t* const value)
{
    const size_t offset = (size_t)address - (size_t)DC_SPRITE_EMIT_ROM_BASE;

    if (address < DC_SPRITE_EMIT_ROM_BASE || offset >= sprite_rom_size) {
        return 0;
    }
    *value = sprite_rom[offset];
    return 1;
}

int dc_classify_sprite_emit_run(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t* const sprite_rom,
    const size_t sprite_rom_size,
    const uint8_t slot,
    dc_sprite_emit_run* const run)
{
    uint8_t descriptor_id;
    uint8_t frame_index;
    uint8_t pointer_low;
    uint8_t pointer_high;
    uint8_t count;
    uint8_t cursor;
    uint8_t attributes;
    uint8_t vertical_low;
    uint8_t vertical_high;
    uint8_t horizontal_low;
    uint8_t horizontal_high;
    uint8_t camera_vertical_low;
    uint8_t camera_vertical_high;
    uint8_t pattern_bias_low;
    uint8_t pattern_bias_high;
    uint8_t merge_low;
    uint8_t y;
    uint8_t carry;
    uint8_t accumulator;
    uint16_t descriptor_pointer;
    uint16_t frame_pointer;
    uint16_t entry_pointer;
    uint16_t index;
    uint8_t emitted;

    if (work_ram == NULL || run == NULL || sprite_rom == NULL
        || work_ram_size < 0x2000U
        || sprite_rom_size < DC_SPRITE_EMIT_ROM_SIZE) {
        return 0;
    }

    memset(run, 0, sizeof(*run));

    /* Every indexed actor table this routine reads is 0x80 entries wide. */
    if ((size_t)0x1F80U + (size_t)slot >= work_ram_size) {
        return 0;
    }

    /* LDY $3900,X : LDA $41AC,Y : LDA $41F1,Y. */
    descriptor_id = work_ram[0x1900U + (size_t)slot];
    if (!dc_sprite_rom_byte(
            sprite_rom,
            sprite_rom_size,
            (uint16_t)(0x41ACU + descriptor_id),
            &pointer_low)
        || !dc_sprite_rom_byte(
            sprite_rom,
            sprite_rom_size,
            (uint16_t)(0x41F1U + descriptor_id),
            &pointer_high)) {
        return 0;
    }
    descriptor_pointer = (uint16_t)(pointer_low | (pointer_high << 8));

    /* LDA $3980,X : ASL A : TAY : two LDA ($2D),Y. */
    frame_index = work_ram[0x1980U + (size_t)slot];
    y = (uint8_t)(frame_index << 1);
    frame_pointer = (uint16_t)(descriptor_pointer + y);
    if (!dc_sprite_rom_byte(
            sprite_rom, sprite_rom_size, frame_pointer, &pointer_low)
        || !dc_sprite_rom_byte(
            sprite_rom,
            sprite_rom_size,
            (uint16_t)(frame_pointer + 1U),
            &pointer_high)) {
        return 0;
    }
    entry_pointer = (uint16_t)(pointer_low | (pointer_high << 8));

    /* CLY : LDA ($2F),Y : CLC : ADC $4E : CMP #$40 : BCC. */
    if (!dc_sprite_rom_byte(
            sprite_rom, sprite_rom_size, entry_pointer, &count)) {
        return 0;
    }
    cursor = work_ram[0x4EU];
    if (count == 0U || count > DC_SPRITE_EMIT_MAX_ENTRIES) {
        return 0;
    }
    if ((uint8_t)(count + cursor) >= 0x40U) {
        return 0;
    }

    horizontal_low = work_ram[0x1180U + (size_t)slot];
    horizontal_high = work_ram[0x1200U + (size_t)slot];
    vertical_low = work_ram[0x1300U + (size_t)slot];
    vertical_high = work_ram[0x1380U + (size_t)slot];
    attributes = work_ram[0x1800U + (size_t)slot];
    merge_low = work_ram[0x1D00U + (size_t)slot];
    camera_vertical_low = work_ram[0xA9U];
    camera_vertical_high = work_ram[0xAAU];
    pattern_bias_low = work_ram[0x51U];
    pattern_bias_high = work_ram[0x52U];

    /* A bit-5 actor merges $3E00,X into the attribute high byte. */
    if ((attributes & 0x20U) != 0U) {
        return 0;
    }

    carry = 0U;
    accumulator = 0U;
    y = 1U;
    index = (uint16_t)(entry_pointer + 1U);
    for (emitted = 0U; emitted < count; ++emitted) {
        dc_sprite_emit_entry* const entry = &run->entries[emitted];
        uint8_t descriptor[6];
        uint16_t sum;
        uint8_t scratch_low;
        uint8_t scratch_high;
        size_t byte;

        for (byte = 0U; byte < sizeof(descriptor); ++byte) {
            if (!dc_sprite_rom_byte(
                    sprite_rom,
                    sprite_rom_size,
                    (uint16_t)(index + byte),
                    &descriptor[byte])) {
                return 0;
            }
        }

        /* LDA ($2F),Y : BMI : the signed vertical offset widens to 16 bits
           against the actor's own coordinate. */
        entry->vertical_negative = (descriptor[0] & 0x80U) != 0U ? 1U : 0U;
        sum = (uint16_t)descriptor[0] + (uint16_t)vertical_low;
        scratch_low = (uint8_t)sum;
        carry = (sum > 0xFFU) ? 1U : 0U;
        if (entry->vertical_negative) {
            sum = (uint16_t)vertical_high + 0xFFU + carry;
        } else {
            sum = (uint16_t)vertical_high + carry;
        }
        scratch_high = (uint8_t)sum;
        entry->widen_low = scratch_low;
        entry->widen_high = scratch_high;

        /* SEC : the camera origin subtracts out of the widened coordinate. */
        sum = (uint16_t)scratch_low - (uint16_t)camera_vertical_low;
        scratch_low = (uint8_t)sum;
        carry = (sum > 0xFFU) ? 0U : 1U;
        sum = (uint16_t)scratch_high - (uint16_t)camera_vertical_high
            - (uint16_t)(1U - carry);
        scratch_high = (uint8_t)sum;
        entry->camera_low = scratch_low;
        entry->camera_high = scratch_high;

        /* CLC : ADC #$40 : the table's vertical origin. */
        sum = (uint16_t)scratch_low + 0x40U;
        entry->vertical_low = (uint8_t)sum;
        carry = (sum > 0xFFU) ? 1U : 0U;
        sum = (uint16_t)scratch_high + carry;
        entry->vertical_high = (uint8_t)sum;
        carry = (sum > 0xFFU) ? 1U : 0U;

        run->vertical_scratch_low = scratch_low;
        run->vertical_scratch_high = scratch_high;

        /* The horizontal offset widens the same way, straight to the port. */
        entry->horizontal_negative = (descriptor[1] & 0x80U) != 0U ? 1U : 0U;
        sum = (uint16_t)descriptor[1] + (uint16_t)horizontal_low;
        entry->horizontal_low = (uint8_t)sum;
        carry = (sum > 0xFFU) ? 1U : 0U;
        if (entry->horizontal_negative) {
            sum = (uint16_t)horizontal_high + 0xFFU + carry;
        } else {
            sum = (uint16_t)horizontal_high + carry;
        }
        entry->horizontal_high = (uint8_t)sum;
        carry = (sum > 0xFFU) ? 1U : 0U;

        /* CLC : the pattern index biases by $51:$52. */
        sum = (uint16_t)descriptor[2] + (uint16_t)pattern_bias_low;
        entry->pattern_low = (uint8_t)sum;
        carry = (sum > 0xFFU) ? 1U : 0U;
        sum = (uint16_t)descriptor[3] + (uint16_t)pattern_bias_high
            + (uint16_t)carry;
        entry->pattern_high = (uint8_t)sum;
        /* Overflow is set when both addends share a sign the result does
           not; it is the last flag arithmetic before the return. */
        entry->pattern_overflow =
            (((descriptor[3] ^ (uint8_t)sum)
              & (pattern_bias_high ^ (uint8_t)sum) & 0x80U) != 0U)
            ? 1U
            : 0U;
        carry = (sum > 0xFFU) ? 1U : 0U;
        entry->pattern_carry = carry;

        /* BIT #$10 : a bit-4 actor merges $3D00,X into the attribute. */
        entry->attribute_low_merged = (attributes & 0x10U) != 0U ? 1U : 0U;
        entry->attribute_low = entry->attribute_low_merged
            ? (uint8_t)(descriptor[4] | merge_low)
            : descriptor[4];
        entry->attribute_high = descriptor[5];
        accumulator = entry->attribute_high;

        y = (uint8_t)(y + 6U);
        index = (uint16_t)(index + 6U);
        ++cursor;
    }

    run->count = count;
    run->descriptor_id = descriptor_id;
    run->frame_index = frame_index;
    run->descriptor_pointer = descriptor_pointer;
    run->entry_pointer = entry_pointer;
    run->cursor = cursor;
    run->final_y = y;
    run->accumulator = accumulator;
    run->carry = carry;
    run->overflow = run->entries[count - 1U].pattern_overflow;
    return 1;
}
