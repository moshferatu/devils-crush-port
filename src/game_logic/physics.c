// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/game_logic/physics.h"

static uint32_t read_position(
    const uint8_t* const work_ram,
    const uint16_t fraction_offset,
    const uint8_t slot)
{
    return (uint32_t)work_ram[fraction_offset + slot]
        | ((uint32_t)work_ram[fraction_offset + 0x80U + slot] << 8U)
        | ((uint32_t)work_ram[fraction_offset + 0x100U + slot] << 16U);
}

static uint32_t integrate_component(
    const uint32_t position,
    const uint8_t increment_fraction,
    const uint8_t increment_integer)
{
    const uint32_t sign_extension =
        (increment_integer & 0x80U) != 0U ? 0xFF0000U : 0U;
    const uint32_t increment = (uint32_t)increment_fraction
        | ((uint32_t)increment_integer << 8U)
        | sign_extension;
    return (position + increment) & 0xFFFFFFU;
}

static uint16_t read_motion(
    const uint8_t* const work_ram,
    const uint16_t low_offset,
    const uint8_t slot)
{
    return (uint16_t)(
        (uint16_t)work_ram[low_offset + slot]
        | ((uint16_t)work_ram[low_offset + 0x80U + slot] << 8U));
}

static int32_t signed_word(const uint16_t value)
{
    return (value & 0x8000U) != 0U
        ? (int32_t)value - 0x10000L
        : (int32_t)value;
}

static int32_t signed_byte(const uint8_t value)
{
    return (value & 0x80U) != 0U
        ? (int32_t)value - 0x100L
        : (int32_t)value;
}

static uint16_t damp_component(
    const uint16_t component,
    const uint8_t divisor)
{
    const int32_t value = signed_word(component);
    const int32_t quotient = value / signed_byte(divisor);
    return (uint16_t)(
        (uint32_t)(value - quotient) & 0xFFFFU);
}

static uint16_t fixed_multiply(
    const uint16_t first,
    const uint16_t second)
{
    const int32_t first_signed = signed_word(first);
    const int32_t second_signed = signed_word(second);
    const uint32_t first_magnitude = (uint32_t)(
        first_signed < 0 ? -first_signed : first_signed);
    const uint32_t second_magnitude = (uint32_t)(
        second_signed < 0 ? -second_signed : second_signed);
    const uint16_t magnitude = (uint16_t)(
        ((first_magnitude * second_magnitude) >> 8U) & 0xFFFFU);
    return (first_signed < 0) != (second_signed < 0)
        ? (uint16_t)(0U - magnitude)
        : magnitude;
}

int dc_integrate_position(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_position_integration* const integration)
{
    static const uint16_t cache_offsets[DC_CACHED_POSITION_BYTE_COUNT] = {
        0x0088U,
        0x0089U,
        0x008AU,
        0x0085U,
        0x0086U,
        0x0087U,
    };
    static const uint16_t position_offsets[
        DC_INTEGRATED_POSITION_BYTE_COUNT] = {
        0x1280U,
        0x1300U,
        0x1380U,
        0x1100U,
        0x1180U,
        0x1200U,
    };

    if (work_ram == NULL || integration == NULL
        || work_ram_size < 0x2000U) {
        return 0;
    }

    const uint32_t second_position =
        read_position(work_ram, 0x1280U, slot);
    const uint32_t first_position =
        read_position(work_ram, 0x1100U, slot);
    const uint32_t integrated_second = integrate_component(
        second_position,
        work_ram[0x00BBU + slot],
        work_ram[0x00BCU + slot]);
    const uint32_t integrated_first = integrate_component(
        first_position,
        work_ram[0x00B9U + slot],
        work_ram[0x00BAU + slot]);

    for (size_t index = 0U;
         index < DC_CACHED_POSITION_BYTE_COUNT;
         ++index) {
        integration->cache_work_ram_offsets[index] =
            cache_offsets[index];
    }
    integration->cached_values[0] =
        (uint8_t)(second_position & 0xFFU);
    integration->cached_values[1] =
        (uint8_t)((second_position >> 8U) & 0xFFU);
    integration->cached_values[2] =
        (uint8_t)((second_position >> 16U) & 0xFFU);
    integration->cached_values[3] =
        (uint8_t)(first_position & 0xFFU);
    integration->cached_values[4] =
        (uint8_t)((first_position >> 8U) & 0xFFU);
    integration->cached_values[5] =
        (uint8_t)((first_position >> 16U) & 0xFFU);

    for (size_t index = 0U;
         index < DC_INTEGRATED_POSITION_BYTE_COUNT;
         ++index) {
        integration->position_work_ram_offsets[index] =
            (uint16_t)(position_offsets[index] + slot);
    }
    integration->position_values[0] =
        (uint8_t)(integrated_second & 0xFFU);
    integration->position_values[1] =
        (uint8_t)((integrated_second >> 8U) & 0xFFU);
    integration->position_values[2] =
        (uint8_t)((integrated_second >> 16U) & 0xFFU);
    integration->position_values[3] =
        (uint8_t)(integrated_first & 0xFFU);
    integration->position_values[4] =
        (uint8_t)((integrated_first >> 8U) & 0xFFU);
    integration->position_values[5] =
        (uint8_t)((integrated_first >> 16U) & 0xFFU);

    integration->cleared_motion_flags =
        (uint8_t)(work_ram[0x1C80U + slot] & 0xFDU);
    integration->first_increment_negative =
        (uint8_t)((work_ram[0x00BAU + slot] & 0x80U) != 0U);
    integration->second_increment_negative =
        (uint8_t)((work_ram[0x00BCU + slot] & 0x80U) != 0U);
    integration->integer_position_unchanged = (uint8_t)(
        (first_position & 0xFFFF00U)
            == (integrated_first & 0xFFFF00U)
        && (second_position & 0xFFFF00U)
            == (integrated_second & 0xFFFF00U));
    integration->final_motion_flags =
        integration->integer_position_unchanged != 0U
        ? (uint8_t)(integration->cleared_motion_flags | 0x02U)
        : integration->cleared_motion_flags;
    return 1;
}

int dc_damp_and_accelerate_motion(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_motion_damping* const damping)
{
    if (work_ram == NULL || damping == NULL
        || work_ram_size < 0x2000U) {
        return 0;
    }

    damping->first_original = read_motion(work_ram, 0x1400U, slot);
    damping->second_original = read_motion(work_ram, 0x1500U, slot);
    damping->first_divisor = work_ram[0x008CU];
    damping->second_divisor = work_ram[0x008DU];
    damping->second_acceleration = work_ram[0x008BU];
    damping->first_component_nonzero =
        (uint8_t)(damping->first_original != 0U);
    damping->second_component_nonzero =
        (uint8_t)(damping->second_original != 0U);

    if ((damping->first_component_nonzero != 0U
            && damping->first_divisor == 0U)
        || (damping->second_component_nonzero != 0U
            && damping->second_divisor == 0U)) {
        return 0;
    }

    damping->first_damped =
        damping->first_component_nonzero != 0U
        ? damp_component(
              damping->first_original,
              damping->first_divisor)
        : damping->first_original;
    damping->second_damped =
        damping->second_component_nonzero != 0U
        ? damp_component(
              damping->second_original,
              damping->second_divisor)
        : damping->second_original;
    damping->second_accelerated = (uint16_t)(
        (damping->second_damped
            + (uint16_t)damping->second_acceleration)
        & 0xFFFFU);
    return 1;
}

int dc_rotate_motion(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    dc_motion_rotation* const rotation)
{
    if (work_ram == NULL || rotation == NULL
        || work_ram_size < 0x2000U) {
        return 0;
    }

    rotation->first_input = (uint16_t)(
        (uint16_t)work_ram[0x00C0U]
        | ((uint16_t)work_ram[0x00C1U] << 8U));
    rotation->second_input = (uint16_t)(
        (uint16_t)work_ram[0x00BEU]
        | ((uint16_t)work_ram[0x00BFU] << 8U));
    rotation->cosine = (uint16_t)(
        (uint16_t)work_ram[0x007DU]
        | ((uint16_t)work_ram[0x007EU] << 8U));
    rotation->sine = (uint16_t)(
        (uint16_t)work_ram[0x007FU]
        | ((uint16_t)work_ram[0x0080U] << 8U));

    const uint16_t first_cosine =
        fixed_multiply(rotation->first_input, rotation->cosine);
    const uint16_t second_sine =
        fixed_multiply(rotation->second_input, rotation->sine);
    const uint16_t first_sine =
        fixed_multiply(rotation->first_input, rotation->sine);
    const uint16_t second_cosine =
        fixed_multiply(rotation->second_input, rotation->cosine);
    rotation->first_output = (uint16_t)(
        first_cosine - second_sine);
    rotation->second_output = (uint16_t)(
        first_sine + second_cosine);
    return 1;
}

int dc_restore_cached_position(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_cached_position_restore* const restore)
{
    static const uint16_t position_offsets[DC_CACHED_POSITION_BYTE_COUNT] = {
        0x1100,
        0x1180,
        0x1200,
        0x1280,
        0x1300,
        0x1380,
    };

    if (work_ram == NULL || restore == NULL || work_ram_size < 0x2000) {
        return 0;
    }

    for (size_t index = 0;
         index < DC_CACHED_POSITION_BYTE_COUNT;
         ++index) {
        restore->work_ram_offsets[index] =
            (uint16_t)(position_offsets[index] + slot);
        restore->values[index] = work_ram[0x0085 + index];
    }
    return 1;
}
