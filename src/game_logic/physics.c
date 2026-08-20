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

// The ROM takes the magnitude with EOR #$FF / INC A, so $80 negates to itself
// and stays $80. Nothing downstream distinguishes it from any other value at
// or above the eight that selects four substeps.
static uint8_t motion_magnitude(const uint8_t high)
{
    return (high & 0x80U) != 0U ? (uint8_t)(0U - (uint32_t)high) : high;
}

// CMP #$80 followed by ROR high / ROR low shifts the carry the comparison set
// from the sign bit back into it, which makes the pair an arithmetic shift.
static uint16_t shift_motion_right(const uint16_t motion, const uint8_t shifts)
{
    uint16_t value = motion;
    for (uint8_t shift = 0U; shift < shifts; ++shift) {
        value = (uint16_t)((value >> 1U) | (value & 0x8000U));
    }
    return value;
}

int dc_split_motion_substeps(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_motion_substep_split* const split)
{
    if (work_ram == NULL || split == NULL || work_ram_size < 0x2000U
        || slot >= 0x80U) {
        return 0;
    }

    split->first_motion = read_motion(work_ram, 0x1400U, slot);
    split->second_motion = read_motion(work_ram, 0x1500U, slot);
    split->first_magnitude =
        motion_magnitude((uint8_t)(split->first_motion >> 8U));
    split->second_magnitude =
        motion_magnitude((uint8_t)(split->second_motion >> 8U));
    split->magnitude = split->second_magnitude >= split->first_magnitude
        ? split->second_magnitude
        : split->first_magnitude;

    if (split->magnitude >= 0x08U) {
        split->substeps = 4U;
        split->shifts = 2U;
    } else if (split->magnitude >= 0x04U) {
        split->substeps = 2U;
        split->shifts = 1U;
    } else {
        split->substeps = 1U;
        split->shifts = 0U;
    }

    split->first_step =
        shift_motion_right(split->first_motion, split->shifts);
    split->second_step =
        shift_motion_right(split->second_motion, split->shifts);
    return 1;
}

int dc_divide_signed_word(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t carry_in,
    dc_signed_division* const division)
{
    if (work_ram == NULL || division == NULL
        || work_ram_size < 0x2000U) {
        return 0;
    }

    division->dividend_input = (uint16_t)(
        (uint16_t)work_ram[0x002FU]
        | ((uint16_t)work_ram[0x0030U] << 8U));
    division->divisor_input = work_ram[0x002DU];
    division->dividend_negative = (uint8_t)(
        (division->dividend_input & 0x8000U) != 0U);
    division->divisor_negative = (uint8_t)(
        (division->divisor_input & 0x80U) != 0U);
    division->result_negative = (uint8_t)(
        division->dividend_negative
        != division->divisor_negative);
    division->initial_carry = (uint8_t)(carry_in != 0U);
    division->divisor_magnitude =
        division->divisor_negative != 0U
        ? (uint8_t)(0U - division->divisor_input)
        : division->divisor_input;

    uint8_t carry = division->initial_carry;
    if (division->dividend_negative != 0U) {
        const uint8_t low = (uint8_t)division->dividend_input;
        const uint8_t high = (uint8_t)(
            division->dividend_input >> 8U);
        const uint16_t low_difference = (uint16_t)(
            0x100U - (uint16_t)low);
        const uint8_t magnitude_low = (uint8_t)low_difference;
        carry = (uint8_t)(low == 0U);
        const uint16_t high_subtrahend = (uint16_t)high
            + (uint16_t)(carry == 0U);
        const uint8_t magnitude_high = (uint8_t)(
            0U - high_subtrahend);
        carry = (uint8_t)(high_subtrahend == 0U);
        division->dividend_magnitude = (uint16_t)(
            (uint16_t)magnitude_low
            | ((uint16_t)magnitude_high << 8U));
    } else {
        division->dividend_magnitude =
            division->dividend_input;
    }
    division->unsigned_initial_carry = carry;

    uint8_t quotient_low =
        (uint8_t)division->dividend_magnitude;
    uint8_t quotient_high = (uint8_t)(
        division->dividend_magnitude >> 8U);
    uint8_t remainder = 0U;

    {
        const uint8_t next_carry = (uint8_t)(
            (quotient_low & 0x80U) != 0U);
        quotient_low = (uint8_t)(
            (uint8_t)(quotient_low << 1U) | carry);
        carry = next_carry;
    }
    {
        const uint8_t next_carry = (uint8_t)(
            (quotient_high & 0x80U) != 0U);
        quotient_high = (uint8_t)(
            (uint8_t)(quotient_high << 1U) | carry);
        carry = next_carry;
    }

    for (size_t index = 0U;
         index < DC_UNSIGNED_DIVIDE_STEP_COUNT;
         ++index) {
        const uint8_t next_carry = (uint8_t)(
            (remainder & 0x80U) != 0U);
        remainder = (uint8_t)(
            (uint8_t)(remainder << 1U) | carry);
        carry = next_carry;
        division->steps[index].remainder_after_shift =
            remainder;

        division->steps[index].subtracts_divisor = (uint8_t)(
            remainder >= division->divisor_magnitude);
        if (division->steps[index].subtracts_divisor != 0U) {
            remainder = (uint8_t)(
                remainder - division->divisor_magnitude);
            carry = 1U;
        } else {
            carry = 0U;
        }
        division->steps[index].remainder_after_subtract =
            remainder;

        {
            const uint8_t low_carry = (uint8_t)(
                (quotient_low & 0x80U) != 0U);
            quotient_low = (uint8_t)(
                (uint8_t)(quotient_low << 1U) | carry);
            carry = low_carry;
        }
        {
            const uint8_t high_carry = (uint8_t)(
                (quotient_high & 0x80U) != 0U);
            quotient_high = (uint8_t)(
                (uint8_t)(quotient_high << 1U) | carry);
            carry = high_carry;
        }
        division->steps[index].quotient_after_roll =
            (uint16_t)(
                (uint16_t)quotient_low
                | ((uint16_t)quotient_high << 8U));
    }

    division->quotient_magnitude = (uint16_t)(
        (uint16_t)quotient_low
        | ((uint16_t)quotient_high << 8U));
    division->remainder = remainder;
    division->result =
        division->result_negative != 0U
        ? (uint16_t)(0U - division->quotient_magnitude)
        : division->quotient_magnitude;
    return 1;
}

int dc_divide_unsigned_24(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t carry_in,
    dc_unsigned_24_division* const division)
{
    uint32_t quotient;
    uint8_t carry;
    uint8_t remainder;
    size_t index;

    if (work_ram == NULL || division == NULL
        || work_ram_size < 0x2000U) {
        return 0;
    }

    division->dividend_input = (uint32_t)work_ram[0x002EU]
        | ((uint32_t)work_ram[0x002FU] << 8U)
        | ((uint32_t)work_ram[0x0030U] << 16U);
    division->divisor = work_ram[0x002DU];
    division->initial_carry = (uint8_t)(carry_in != 0U);
    division->initial_roll_carry = (uint8_t)(
        (division->dividend_input & 0x800000U) != 0U);
    quotient = (uint32_t)(
        ((division->dividend_input << 1U)
         | division->initial_carry)
        & 0xFFFFFFU);
    division->dividend_after_initial_roll = quotient;
    carry = division->initial_roll_carry;
    remainder = 0U;
    division->subtraction_count = 0U;

    for (index = 0U;
         index < DC_UNSIGNED_24_DIVIDE_STEP_COUNT;
         ++index) {
        dc_unsigned_24_divide_step* const step =
            &division->steps[index];
        const uint8_t next_remainder_carry = (uint8_t)(
            (remainder & 0x80U) != 0U);
        remainder = (uint8_t)(
            (uint8_t)(remainder << 1U) | carry);
        carry = next_remainder_carry;
        step->remainder_after_shift = remainder;
        step->subtracts_divisor = (uint8_t)(
            remainder >= division->divisor);
        if (step->subtracts_divisor != 0U) {
            remainder = (uint8_t)(remainder - division->divisor);
            carry = 1U;
            division->subtraction_count = (uint8_t)(
                division->subtraction_count + 1U);
        } else {
            carry = 0U;
        }
        step->remainder_after_subtract = remainder;

        {
            const uint8_t next_quotient_carry = (uint8_t)(
                (quotient & 0x800000U) != 0U);
            quotient = (uint32_t)(
                ((quotient << 1U) | carry) & 0xFFFFFFU);
            carry = next_quotient_carry;
        }
        step->quotient_after_roll = quotient;
    }

    division->quotient = quotient;
    division->remainder = remainder;
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

int dc_negate_signed_word(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    dc_signed_word_negation* const negation)
{
    if (work_ram == NULL || negation == NULL
        || work_ram_size < 0x2000U) {
        return 0;
    }

    negation->input = (uint16_t)(
        (uint16_t)work_ram[0x002FU]
        | ((uint16_t)work_ram[0x0030U] << 8U));
    negation->result = (uint16_t)(0U - negation->input);
    negation->complemented_high =
        (uint8_t)(work_ram[0x0030U] ^ 0xFFU);
    negation->increments_high =
        (uint8_t)((negation->result & 0x00FFU) == 0U);
    return 1;
}

static void multiply_unsigned_values(
    const uint16_t accumulator,
    const uint16_t multiplicand,
    const uint8_t multiplier,
    dc_unsigned_multiply_round* const round)
{
    round->accumulator = accumulator;
    round->multiplicand = multiplicand;
    round->multiplier = multiplier;
    round->add_count = 0U;

    uint32_t shifted = (uint32_t)round->accumulator
        | ((uint32_t)round->multiplier << 16U);
    for (size_t index = 0U;
         index < DC_UNSIGNED_MULTIPLY_STEP_COUNT;
         ++index) {
        dc_unsigned_multiply_step* const step = &round->steps[index];
        step->adds_multiplicand =
            (uint8_t)((shifted & 0x800000U) != 0U);
        shifted = (shifted << 1U) & 0xFFFFFFU;
        step->low_after_shift = (uint8_t)shifted;
        step->high_after_shift = (uint8_t)(shifted >> 8U);
        step->multiplier_after_shift = (uint8_t)(shifted >> 16U);
        step->low_after_add = step->low_after_shift;
        step->high_after_add = step->high_after_shift;
        step->multiplier_after_add = step->multiplier_after_shift;
        step->add_carry = 0U;

        if (step->adds_multiplicand != 0U) {
            const uint32_t sum =
                (shifted & 0xFFFFU) + round->multiplicand;
            step->low_after_add = (uint8_t)sum;
            step->high_after_add = (uint8_t)(sum >> 8U);
            step->add_carry = (uint8_t)(sum > 0xFFFFU);
            if (step->add_carry != 0U) {
                step->multiplier_after_add =
                    (uint8_t)(step->multiplier_after_add + 1U);
            }
            shifted = (uint32_t)step->low_after_add
                | ((uint32_t)step->high_after_add << 8U)
                | ((uint32_t)step->multiplier_after_add << 16U);
            round->add_count = (uint8_t)(round->add_count + 1U);
        }
    }

    round->result = (uint16_t)(shifted & 0xFFFFU);
    round->multiplier_after = (uint8_t)(shifted >> 16U);
}

int dc_multiply_unsigned_bytes(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    dc_unsigned_byte_multiply* const multiply)
{
    uint8_t low;
    uint8_t high;
    size_t index;

    if (work_ram == NULL || multiply == NULL
        || work_ram_size < 0x2000U) {
        return 0;
    }

    multiply->multiplicand = work_ram[0x002FU];
    multiply->multiplier = work_ram[0x0030U];
    multiply->add_count = 0U;
    multiply->carry_count = 0U;
    low = 0U;
    high = multiply->multiplier;
    for (index = 0U; index < DC_UNSIGNED_MULTIPLY_STEP_COUNT; ++index) {
        dc_unsigned_byte_multiply_step* const step =
            &multiply->steps[index];
        const uint8_t low_carry = (uint8_t)(low >> 7U);
        const uint8_t adds = (uint8_t)(high >> 7U);
        uint16_t sum;

        low = (uint8_t)(low << 1U);
        high = (uint8_t)((high << 1U) | low_carry);
        step->low_after_shift = low;
        step->high_after_shift = high;
        step->adds_multiplicand = adds;
        step->add_carry = 0U;
        if (adds != 0U) {
            sum = (uint16_t)low + (uint16_t)multiply->multiplicand;
            low = (uint8_t)sum;
            step->add_carry = (uint8_t)(sum > 0xFFU);
            if (step->add_carry != 0U) {
                high = (uint8_t)(high + 1U);
                multiply->carry_count =
                    (uint8_t)(multiply->carry_count + 1U);
            }
            multiply->add_count =
                (uint8_t)(multiply->add_count + 1U);
        }
        step->low_after_add = low;
        step->high_after_add = high;
    }
    multiply->result_low = low;
    multiply->result_high = high;
    return 1;
}

int dc_multiply_unsigned_round(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    dc_unsigned_multiply_round* const round)
{
    if (work_ram == NULL || round == NULL
        || work_ram_size < 0x2000U) {
        return 0;
    }

    const uint16_t accumulator = (uint16_t)(
        (uint16_t)work_ram[0x0042U]
        | ((uint16_t)work_ram[0x0043U] << 8U));
    const uint16_t multiplicand = (uint16_t)(
        (uint16_t)work_ram[0x002DU]
        | ((uint16_t)work_ram[0x002EU] << 8U));
    multiply_unsigned_values(
        accumulator,
        multiplicand,
        work_ram[0x0030U],
        round);
    return 1;
}

int dc_multiply_signed_fixed(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    dc_signed_fixed_multiply* const multiply)
{
    if (work_ram == NULL || multiply == NULL
        || work_ram_size < 0x2000U) {
        return 0;
    }

    multiply->multiplicand_input = (uint16_t)(
        (uint16_t)work_ram[0x002DU]
        | ((uint16_t)work_ram[0x002EU] << 8U));
    multiply->multiplier_input = (uint16_t)(
        (uint16_t)work_ram[0x002FU]
        | ((uint16_t)work_ram[0x0030U] << 8U));
    multiply->negates_multiplicand = (uint8_t)(
        (multiply->multiplicand_input & 0x8000U) != 0U);
    multiply->negates_multiplier = (uint8_t)(
        (multiply->multiplier_input & 0x8000U) != 0U);
    multiply->multiplicand_magnitude =
        multiply->negates_multiplicand != 0U
        ? (uint16_t)(0U - multiply->multiplicand_input)
        : multiply->multiplicand_input;
    multiply->multiplier_magnitude =
        multiply->negates_multiplier != 0U
        ? (uint16_t)(0U - multiply->multiplier_input)
        : multiply->multiplier_input;
    multiply->sign_xor = (uint8_t)(
        (uint8_t)(multiply->multiplicand_input >> 8U)
        ^ (uint8_t)(multiply->multiplier_input >> 8U));
    multiply->runs_high_round = (uint8_t)(
        (multiply->multiplier_magnitude >> 8U) != 0U);
    multiply->high_round = (dc_unsigned_multiply_round){0};

    uint16_t accumulator = 0U;
    if (multiply->runs_high_round != 0U) {
        multiply_unsigned_values(
            accumulator,
            multiply->multiplicand_magnitude,
            (uint8_t)(multiply->multiplier_magnitude >> 8U),
            &multiply->high_round);
        accumulator = multiply->high_round.result;
    }
    multiply_unsigned_values(
        accumulator,
        multiply->multiplicand_magnitude,
        (uint8_t)multiply->multiplier_magnitude,
        &multiply->low_round);
    multiply->magnitude = (uint16_t)(
        (uint16_t)(multiply->low_round.result >> 8U)
        | ((uint16_t)multiply->low_round.multiplier_after << 8U));
    multiply->result = (multiply->sign_xor & 0x80U) != 0U
        ? (uint16_t)(0U - multiply->magnitude)
        : multiply->magnitude;
    return 1;
}

int dc_project_flipper_vectors(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_flipper_projection* const projection)
{
    if (work_ram == NULL || projection == NULL
        || work_ram_size < 0x2000U || slot >= 0x80U) {
        return 0;
    }

    projection->first_input = read_motion(work_ram, 0x1400U, slot);
    projection->second_input = read_motion(work_ram, 0x1500U, slot);
    projection->first_coefficient = (uint16_t)(
        (uint16_t)work_ram[0x007DU]
        | ((uint16_t)work_ram[0x007EU] << 8U));
    projection->second_coefficient = (uint16_t)(
        (uint16_t)work_ram[0x007FU]
        | ((uint16_t)work_ram[0x0080U] << 8U));
    projection->first_direct_product = fixed_multiply(
        projection->first_input,
        projection->first_coefficient);
    projection->second_cross_product = fixed_multiply(
        projection->second_input,
        projection->second_coefficient);
    projection->first_cross_product = fixed_multiply(
        projection->first_input,
        projection->second_coefficient);
    projection->second_direct_product = fixed_multiply(
        projection->second_input,
        projection->first_coefficient);
    projection->first_output = (uint16_t)(
        projection->first_direct_product
        + projection->second_cross_product);
    projection->second_output = (uint16_t)(
        (uint16_t)(0U - projection->first_cross_product)
        + projection->second_direct_product);
    return 1;
}

int dc_project_flipper_radius(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    dc_flipper_radius_projection* const projection)
{
    if (work_ram == NULL || projection == NULL
        || work_ram_size < 0x2000U) {
        return 0;
    }

    projection->radius = (uint16_t)(
        (uint16_t)work_ram[0x0031U]
        | ((uint16_t)work_ram[0x0032U] << 8U));
    projection->coefficient = (uint16_t)(
        (uint16_t)work_ram[0x007DU]
        | ((uint16_t)work_ram[0x007EU] << 8U));
    projection->output = fixed_multiply(
        projection->radius,
        projection->coefficient);
    return 1;
}

int dc_project_flipper_angle(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t source_slot,
    const uint8_t target_slot,
    dc_flipper_angle_projection* const projection)
{
    if (work_ram == NULL || projection == NULL
        || work_ram_size < 0x2000U
        || source_slot >= 0x80U || target_slot >= 0x80U) {
        return 0;
    }

    projection->target_flags =
        work_ram[0x1C80U + target_slot];
    projection->source_angle =
        work_ram[0x1180U + source_slot];
    projection->target_angle =
        work_ram[0x1180U + target_slot];
    projection->uses_wrapped_difference = (uint8_t)(
        (projection->target_flags & 0x01U) != 0U);
    projection->angle_difference =
        projection->uses_wrapped_difference != 0U
        ? (uint8_t)(
              projection->target_angle + 0x10U
              - projection->source_angle)
        : (uint8_t)(
              projection->source_angle
              - projection->target_angle);
    projection->returns_zero = (uint8_t)(
        (projection->angle_difference & 0x80U) != 0U);
    projection->scale = (uint16_t)(
        (uint16_t)work_ram[0x009EU]
        | ((uint16_t)work_ram[0x009FU] << 8U));
    projection->fixed_difference = (uint16_t)(
        (uint16_t)projection->angle_difference << 3U);
    projection->product = projection->returns_zero != 0U
        ? 0U
        : fixed_multiply(
              projection->fixed_difference,
              projection->scale);
    projection->negates_product = (uint8_t)(
        projection->returns_zero == 0U
        && (projection->target_flags & 0x80U) == 0U);
    projection->output = projection->returns_zero != 0U
        ? 0U
        : (projection->negates_product != 0U
              ? (uint16_t)(0U - projection->product)
              : projection->product);
    projection->increments_high = (uint8_t)(
        projection->negates_product != 0U
        && (projection->output & 0x00FFU) == 0U);
    return 1;
}

static uint16_t arithmetic_shift_right(
    uint16_t value,
    const uint8_t count)
{
    for (uint8_t index = 0U; index < count; ++index) {
        value = (uint16_t)(
            (value >> 1U) | (value & 0x8000U));
    }
    return value;
}

int dc_adjust_collision_motion(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    dc_collision_motion_adjustment* const adjustment)
{
    if (work_ram == NULL || adjustment == NULL
        || work_ram_size < 0x2000U) {
        return 0;
    }

    adjustment->input = (uint16_t)(
        (uint16_t)work_ram[0x00BEU]
        | ((uint16_t)work_ram[0x00BFU] << 8U));
    adjustment->mode = (uint8_t)(work_ram[0x0078U] & 0x60U);
    adjustment->effective_mode = adjustment->mode;
    if (adjustment->effective_mode == 0U
        && work_ram[0x0CC7U] != 0U) {
        adjustment->effective_mode = 0x20U;
    }
    adjustment->uses_quarter_scale = (uint8_t)(
        adjustment->effective_mode == 0x20U
        && work_ram[0x022CU] != 0U);
    adjustment->negative_input = (uint8_t)(
        (adjustment->input & 0x8000U) != 0U);
    adjustment->clamps_high = 0U;

    if (adjustment->effective_mode == 0x20U) {
        if (adjustment->uses_quarter_scale != 0U) {
            adjustment->output =
                arithmetic_shift_right(adjustment->input, 2U);
        } else {
            adjustment->output = (uint16_t)(
                arithmetic_shift_right(adjustment->input, 3U)
                + arithmetic_shift_right(adjustment->input, 4U));
        }
    } else if (adjustment->effective_mode == 0x40U) {
        adjustment->output =
            arithmetic_shift_right(adjustment->input, 1U);
    } else if (adjustment->effective_mode == 0x60U) {
        adjustment->output =
            arithmetic_shift_right(adjustment->input, 4U);
    } else if (adjustment->negative_input != 0U) {
        const uint8_t input_high =
            (uint8_t)(adjustment->input >> 8U);
        const uint8_t output_high =
            input_high < 0xFAU ? 0xFAU : input_high;
        adjustment->clamps_high =
            (uint8_t)(output_high != input_high);
        adjustment->output = (uint16_t)(
            (uint8_t)((uint8_t)adjustment->input
                      - work_ram[0x0081U])
            | ((uint16_t)output_high << 8U));
    } else {
        const uint16_t low_sum = (uint16_t)(
            (uint8_t)adjustment->input + work_ram[0x0081U]);
        const uint16_t high_sum = (uint16_t)(
            (uint8_t)(adjustment->input >> 8U)
            + work_ram[0x0082U]
            + (uint8_t)(low_sum > 0xFFU));
        const uint8_t output_high =
            high_sum >= 0x07U ? 0x06U : (uint8_t)high_sum;
        adjustment->clamps_high =
            (uint8_t)(high_sum >= 0x07U);
        adjustment->output = (uint16_t)(
            (uint8_t)low_sum
            | ((uint16_t)output_high << 8U));
    }
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
