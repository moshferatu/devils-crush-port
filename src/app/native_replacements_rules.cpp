// SPDX-License-Identifier: GPL-3.0-or-later

#include "native_replacements_internal.hpp"
#include "devils_crush/game_logic/rng.h"
#include "devils_crush/game_logic/score.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace devils_crush::app::detail {

using emulation::ReplacementMemoryWrite;
using emulation::RoutineEntry;
using emulation::RoutineKey;
using emulation::RoutineReplacement;

[[nodiscard]] std::optional<RoutineReplacement>
replace_rng_update(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x00U,
        .logical_pc = 0xE8B3U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    if ((entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_rng_update update{};
    if (!dc_advance_rng(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &update)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xE8E6U;
    output.accumulator = update.seed_high;
    output.status =
        static_cast<std::uint8_t>(entry.registers.status & 0x3CU);
    if (update.final_carry != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x01U);
    }
    if (update.final_overflow != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x40U);
    }
    if (output.accumulator == 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);
    } else if ((output.accumulator & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x80U);
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x00U,
            .logical_pc = 0xE8E6U,
        },
        .output_registers = output,
        .body_master_cycles = 282U,
    };
    replacement.work_ram_writes = {
        {
            .instruction = {0x00U, 0xE8B9U},
            .logical_address = 0x2042U,
            .value = update.constant_sum_low,
            .master_cycle_offset = 39U,
        },
        {
            .instruction = {0x00U, 0xE8C0U},
            .logical_address = 0x2043U,
            .value = update.constant_sum_high,
            .master_cycle_offset = 72U,
        },
        {
            .instruction = {0x00U, 0xE8C2U},
            .logical_address = 0x2042U,
            .value = update.doubled_low,
            .master_cycle_offset = 90U,
        },
        {
            .instruction = {0x00U, 0xE8C4U},
            .logical_address = 0x2043U,
            .value = update.doubled_high,
            .master_cycle_offset = 108U,
        },
        {
            .instruction = {0x00U, 0xE8CCU},
            .logical_address = 0x2042U,
            .value = update.frame_sum_low,
            .master_cycle_offset = 153U,
        },
        {
            .instruction = {0x00U, 0xE8D3U},
            .logical_address = 0x2043U,
            .value = update.mixed_high,
            .master_cycle_offset = 192U,
        },
        {
            .instruction = {0x00U, 0xE8DBU},
            .logical_address = 0x222BU,
            .value = update.seed_high,
            .master_cycle_offset = 240U,
        },
        {
            .instruction = {0x00U, 0xE8E0U},
            .logical_address = 0x222AU,
            .value = update.seed_low,
            .master_cycle_offset = 267U,
        },
    };

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto adc_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const bool carry_in,
            const std::uint8_t result) {
            const auto sum = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(left)
                + static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC1U));
            status = set_nz(status, result);
            if (sum > 0xFFU) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if (((~(left ^ right) & (left ^ result)) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x40U);
            }
            return status;
        };
    const auto shift_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t original,
            const std::uint8_t result) {
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0x01U));
            status = set_nz(status, result);
            if ((original & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            return status;
        };
    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x00U, instruction_pc},
                .registers = registers,
            });
        };

    const auto seed_low =
        std::to_integer<std::uint8_t>(work_ram[0x022AU]);
    const auto seed_high =
        std::to_integer<std::uint8_t>(work_ram[0x022BU]);
    const auto frame_counter =
        std::to_integer<std::uint8_t>(work_ram[0x0000U]);
    const auto constant_low_sum = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(seed_low) + 0x3FU);
    const auto constant_low_carry = constant_low_sum > 0xFFU;
    const auto frame_low_sum = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(seed_low)
        + static_cast<std::uint16_t>(frame_counter));
    const auto frame_low_carry = frame_low_sum > 0xFFU;

    auto checkpoint = entry.registers;
    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status & ~0x01U);
    append_checkpoint(0xE8B3U, 0xE8B4U, 6U, checkpoint);

    checkpoint.accumulator = seed_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xE8B4U, 0xE8B7U, 21U, checkpoint);

    checkpoint.accumulator = update.constant_sum_low;
    checkpoint.status = adc_status(
        checkpoint.status,
        seed_low,
        0x3FU,
        false,
        checkpoint.accumulator);
    append_checkpoint(0xE8B7U, 0xE8B9U, 27U, checkpoint);
    append_checkpoint(0xE8B9U, 0xE8BBU, 39U, checkpoint);

    checkpoint.accumulator = seed_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xE8BBU, 0xE8BEU, 54U, checkpoint);

    checkpoint.accumulator = update.constant_sum_high;
    checkpoint.status = adc_status(
        checkpoint.status,
        seed_high,
        0x50U,
        constant_low_carry,
        checkpoint.accumulator);
    append_checkpoint(0xE8BEU, 0xE8C0U, 60U, checkpoint);
    append_checkpoint(0xE8C0U, 0xE8C2U, 72U, checkpoint);

    checkpoint.status = shift_status(
        checkpoint.status,
        update.constant_sum_low,
        update.doubled_low);
    append_checkpoint(0xE8C2U, 0xE8C4U, 90U, checkpoint);

    checkpoint.status = shift_status(
        checkpoint.status,
        update.constant_sum_high,
        update.doubled_high);
    append_checkpoint(0xE8C4U, 0xE8C6U, 108U, checkpoint);

    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status & ~0x01U);
    append_checkpoint(0xE8C6U, 0xE8C7U, 114U, checkpoint);

    checkpoint.accumulator = seed_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xE8C7U, 0xE8CAU, 129U, checkpoint);

    checkpoint.accumulator = update.frame_sum_low;
    checkpoint.status = adc_status(
        checkpoint.status,
        seed_low,
        frame_counter,
        false,
        checkpoint.accumulator);
    append_checkpoint(0xE8CAU, 0xE8CCU, 141U, checkpoint);
    append_checkpoint(0xE8CCU, 0xE8CEU, 153U, checkpoint);

    checkpoint.accumulator = seed_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xE8CEU, 0xE8D1U, 168U, checkpoint);

    checkpoint.accumulator = update.mixed_high;
    checkpoint.status = adc_status(
        checkpoint.status,
        seed_high,
        update.doubled_high,
        frame_low_carry,
        checkpoint.accumulator);
    append_checkpoint(0xE8D1U, 0xE8D3U, 180U, checkpoint);
    append_checkpoint(0xE8D3U, 0xE8D5U, 192U, checkpoint);

    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status & ~0x01U);
    append_checkpoint(0xE8D5U, 0xE8D6U, 198U, checkpoint);

    checkpoint.accumulator = seed_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xE8D6U, 0xE8D9U, 213U, checkpoint);

    checkpoint.accumulator = update.seed_high;
    checkpoint.status = adc_status(
        checkpoint.status,
        seed_low,
        update.mixed_high,
        false,
        checkpoint.accumulator);
    append_checkpoint(0xE8D9U, 0xE8DBU, 225U, checkpoint);
    append_checkpoint(0xE8DBU, 0xE8DEU, 240U, checkpoint);

    checkpoint.accumulator = update.frame_sum_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xE8DEU, 0xE8E0U, 252U, checkpoint);
    append_checkpoint(0xE8E0U, 0xE8E3U, 267U, checkpoint);
    append_checkpoint(0xE8E3U, 0xE8E6U, 282U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_score_accumulator(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x00U,
        .logical_pc = 0xF55CU,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    // Decimal and transfer modes alter the arithmetic instruction contract
    // and are not used by the observed gameplay callers.
    if ((entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_score_update update{};
    if (!dc_accumulate_score_ordinary(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &update)) {
        return std::nullopt;
    }

    bool carry = true;
    const auto subtract_byte =
        [&carry](const std::uint8_t accumulator,
                 const std::uint8_t operand) {
            const auto subtrahend = static_cast<std::uint16_t>(
                operand + (carry ? 0U : 1U));
            const auto result = static_cast<std::uint8_t>(
                static_cast<std::uint16_t>(accumulator) - subtrahend);
            const bool overflow =
                ((accumulator ^ result) & (accumulator ^ operand) & 0x80U)
                != 0U;
            carry = static_cast<std::uint16_t>(accumulator) >= subtrahend;
            return std::pair{result, overflow};
        };
    (void)subtract_byte(
        update.score_low,
        static_cast<std::uint8_t>(update.comparison_score & 0xFFU));
    (void)subtract_byte(
        update.score_middle,
        static_cast<std::uint8_t>(
            (update.comparison_score >> 8U) & 0xFFU));
    const auto [unused_comparison, overflow] =
        subtract_byte(
            update.score_high,
            static_cast<std::uint8_t>(
                (update.comparison_score >> 16U) & 0xFFU));
    (void)unused_comparison;

    auto output = entry.registers;
    output.program_counter = 0xF5A8U;
    output.accumulator = update.dirty_flags;
    output.y = update.slot;
    output.status =
        static_cast<std::uint8_t>(entry.registers.status & 0x3CU);
    if (carry) {
        output.status = static_cast<std::uint8_t>(output.status | 0x01U);
    }
    if (overflow) {
        output.status = static_cast<std::uint8_t>(output.status | 0x40U);
    }
    if (output.accumulator == 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);
    } else if ((output.accumulator & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x80U);
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x00U,
            .logical_pc = 0xF5A8U,
        },
        .output_registers = output,
        .body_master_cycles = 558U,
    };
    replacement.work_ram_writes = {
        {
            .instruction = {0x00U, 0xF587U},
            .logical_address =
                static_cast<std::uint16_t>(0x2205U + update.slot),
            .value = update.score_low,
            .master_cycle_offset = 108U,
        },
        {
            .instruction = {0x00U, 0xF58FU},
            .logical_address =
                static_cast<std::uint16_t>(0x220CU + update.slot),
            .value = update.score_middle,
            .master_cycle_offset = 150U,
        },
        {
            .instruction = {0x00U, 0xF597U},
            .logical_address =
                static_cast<std::uint16_t>(0x2213U + update.slot),
            .value = update.score_high,
            .master_cycle_offset = 186U,
        },
        {
            .instruction = {0x00U, 0xF59AU},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + entry.registers.stack_pointer),
            .value = 0xF5U,
            .master_cycle_offset = 186U,
        },
        {
            .instruction = {0x00U, 0xF59AU},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U
                + static_cast<std::uint8_t>(
                    entry.registers.stack_pointer - 1U)),
            .value = 0x9CU,
            .master_cycle_offset = 186U,
        },
        {
            .instruction = {0x00U, 0xF59DU},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + entry.registers.stack_pointer),
            .value = 0xF5U,
            .master_cycle_offset = 324U,
        },
        {
            .instruction = {0x00U, 0xF59DU},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U
                + static_cast<std::uint8_t>(
                    entry.registers.stack_pointer - 1U)),
            .value = 0x9FU,
            .master_cycle_offset = 324U,
        },
        {
            .instruction = {0x00U, 0xF5A5U},
            .logical_address = 0x2CC0U,
            .value = update.dirty_flags,
            .master_cycle_offset = 558U,
        },
    };

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto adc_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const bool carry_in,
            const std::uint8_t result) {
            const auto sum = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(left)
                + static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC1U));
            status = set_nz(status, result);
            if (sum > 0xFFU) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if (((~(left ^ right) & (left ^ result)) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x40U);
            }
            return status;
        };
    const auto sbc_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const bool carry_in,
            const std::uint8_t result) {
            const auto subtrahend = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in ? 0U : 1U));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC1U));
            status = set_nz(status, result);
            if (static_cast<std::uint16_t>(left) >= subtrahend) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if (((left ^ result) & (left ^ right) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x40U);
            }
            return status;
        };
    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x00U, instruction_pc},
                .registers = registers,
            });
        };
    const auto subtract_result =
        [](const std::uint8_t left,
           const std::uint8_t right,
           const bool carry_in) {
            const auto subtrahend = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in ? 0U : 1U));
            return static_cast<std::uint8_t>(
                static_cast<std::uint16_t>(left) - subtrahend);
        };

    const auto previous_score = update.score - update.award;
    const auto previous_low =
        static_cast<std::uint8_t>(previous_score & 0xFFU);
    const auto previous_middle =
        static_cast<std::uint8_t>((previous_score >> 8U) & 0xFFU);
    const auto previous_high =
        static_cast<std::uint8_t>((previous_score >> 16U) & 0xFFU);
    const auto award_low =
        static_cast<std::uint8_t>(update.award & 0xFFU);
    const auto award_high =
        static_cast<std::uint8_t>((update.award >> 8U) & 0xFFU);
    const auto low_sum = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(previous_low) + award_low);
    const bool low_carry = low_sum > 0xFFU;
    const auto middle_sum = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(previous_middle)
        + static_cast<std::uint16_t>(award_high)
        + static_cast<std::uint16_t>(low_carry));
    const bool middle_carry = middle_sum > 0xFFU;

    auto checkpoint = entry.registers;
    checkpoint.accumulator = 0U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF55CU, 0xF55FU, 15U, checkpoint);
    append_checkpoint(0xF55FU, 0xF561U, 21U, checkpoint);
    checkpoint.accumulator = 0U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF561U, 0xF563U, 33U, checkpoint);
    append_checkpoint(0xF563U, 0xF57EU, 45U, checkpoint);

    checkpoint.y = update.slot;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0xF57EU, 0xF581U, 60U, checkpoint);
    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status & ~0x01U);
    append_checkpoint(0xF581U, 0xF582U, 66U, checkpoint);
    checkpoint.accumulator = previous_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF582U, 0xF584U, 78U, checkpoint);
    checkpoint.accumulator = update.score_low;
    checkpoint.status = adc_status(
        checkpoint.status,
        previous_low,
        award_low,
        false,
        checkpoint.accumulator);
    append_checkpoint(0xF584U, 0xF587U, 93U, checkpoint);
    append_checkpoint(0xF587U, 0xF58AU, 108U, checkpoint);

    checkpoint.accumulator = previous_middle;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF58AU, 0xF58CU, 120U, checkpoint);
    checkpoint.accumulator = update.score_middle;
    checkpoint.status = adc_status(
        checkpoint.status,
        previous_middle,
        award_high,
        low_carry,
        checkpoint.accumulator);
    append_checkpoint(0xF58CU, 0xF58FU, 135U, checkpoint);
    append_checkpoint(0xF58FU, 0xF592U, 150U, checkpoint);

    checkpoint.accumulator = previous_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF592U, 0xF595U, 165U, checkpoint);
    checkpoint.accumulator = update.score_high;
    checkpoint.status = adc_status(
        checkpoint.status,
        previous_high,
        0U,
        middle_carry,
        checkpoint.accumulator);
    append_checkpoint(0xF595U, 0xF597U, 171U, checkpoint);
    append_checkpoint(0xF597U, 0xF59AU, 186U, checkpoint);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 2U);
    append_checkpoint(0xF59AU, 0xF5A9U, 207U, checkpoint);

    checkpoint.y = update.slot;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0xF5A9U, 0xF5ACU, 222U, checkpoint);
    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status | 0x01U);
    append_checkpoint(0xF5ACU, 0xF5ADU, 228U, checkpoint);

    bool saturation_carry = true;
    checkpoint.accumulator = update.score_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF5ADU, 0xF5B0U, 243U, checkpoint);
    auto subtraction = subtract_result(
        checkpoint.accumulator,
        0x7FU,
        saturation_carry);
    checkpoint.status = sbc_status(
        checkpoint.status,
        checkpoint.accumulator,
        0x7FU,
        saturation_carry,
        subtraction);
    saturation_carry = (checkpoint.status & 0x01U) != 0U;
    checkpoint.accumulator = subtraction;
    append_checkpoint(0xF5B0U, 0xF5B2U, 249U, checkpoint);

    checkpoint.accumulator = update.score_middle;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF5B2U, 0xF5B5U, 264U, checkpoint);
    subtraction = subtract_result(
        checkpoint.accumulator,
        0x96U,
        saturation_carry);
    checkpoint.status = sbc_status(
        checkpoint.status,
        checkpoint.accumulator,
        0x96U,
        saturation_carry,
        subtraction);
    saturation_carry = (checkpoint.status & 0x01U) != 0U;
    checkpoint.accumulator = subtraction;
    append_checkpoint(0xF5B5U, 0xF5B7U, 270U, checkpoint);

    checkpoint.accumulator = update.score_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF5B7U, 0xF5BAU, 285U, checkpoint);
    subtraction = subtract_result(
        checkpoint.accumulator,
        0x98U,
        saturation_carry);
    checkpoint.status = sbc_status(
        checkpoint.status,
        checkpoint.accumulator,
        0x98U,
        saturation_carry,
        subtraction);
    checkpoint.accumulator = subtraction;
    append_checkpoint(0xF5BAU, 0xF5BCU, 291U, checkpoint);
    append_checkpoint(0xF5BCU, 0xF5D5U, 303U, checkpoint);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 2U);
    append_checkpoint(0xF5D5U, 0xF59DU, 324U, checkpoint);

    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 2U);
    append_checkpoint(0xF59DU, 0xF5D6U, 345U, checkpoint);
    checkpoint.accumulator = 0U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF5D6U, 0xF5D9U, 360U, checkpoint);
    append_checkpoint(0xF5D9U, 0xF5DBU, 366U, checkpoint);

    const auto original_dirty_flags =
        std::to_integer<std::uint8_t>(work_ram[0x0CC0U]);
    checkpoint.accumulator = original_dirty_flags;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF5DBU, 0xF5DEU, 381U, checkpoint);
    checkpoint.accumulator =
        static_cast<std::uint8_t>(original_dirty_flags & 0x40U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF5DEU, 0xF5E0U, 387U, checkpoint);
    append_checkpoint(0xF5E0U, 0xF5E2U, 393U, checkpoint);
    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status | 0x01U);
    append_checkpoint(0xF5E2U, 0xF5E3U, 399U, checkpoint);

    bool comparison_carry = true;
    const auto comparison_low = static_cast<std::uint8_t>(
        update.comparison_score & 0xFFU);
    const auto comparison_middle = static_cast<std::uint8_t>(
        (update.comparison_score >> 8U) & 0xFFU);
    const auto comparison_high = static_cast<std::uint8_t>(
        (update.comparison_score >> 16U) & 0xFFU);

    checkpoint.accumulator = update.score_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF5E3U, 0xF5E6U, 414U, checkpoint);
    subtraction = subtract_result(
        checkpoint.accumulator,
        comparison_low,
        comparison_carry);
    checkpoint.status = sbc_status(
        checkpoint.status,
        checkpoint.accumulator,
        comparison_low,
        comparison_carry,
        subtraction);
    comparison_carry = (checkpoint.status & 0x01U) != 0U;
    checkpoint.accumulator = subtraction;
    append_checkpoint(0xF5E6U, 0xF5E9U, 429U, checkpoint);

    checkpoint.accumulator = update.score_middle;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF5E9U, 0xF5ECU, 444U, checkpoint);
    subtraction = subtract_result(
        checkpoint.accumulator,
        comparison_middle,
        comparison_carry);
    checkpoint.status = sbc_status(
        checkpoint.status,
        checkpoint.accumulator,
        comparison_middle,
        comparison_carry,
        subtraction);
    comparison_carry = (checkpoint.status & 0x01U) != 0U;
    checkpoint.accumulator = subtraction;
    append_checkpoint(0xF5ECU, 0xF5EFU, 459U, checkpoint);

    checkpoint.accumulator = update.score_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF5EFU, 0xF5F2U, 474U, checkpoint);
    subtraction = subtract_result(
        checkpoint.accumulator,
        comparison_high,
        comparison_carry);
    checkpoint.status = sbc_status(
        checkpoint.status,
        checkpoint.accumulator,
        comparison_high,
        comparison_carry,
        subtraction);
    checkpoint.accumulator = subtraction;
    append_checkpoint(0xF5F2U, 0xF5F5U, 489U, checkpoint);
    append_checkpoint(0xF5F5U, 0xF604U, 501U, checkpoint);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 2U);
    append_checkpoint(0xF604U, 0xF5A0U, 522U, checkpoint);

    checkpoint.accumulator = original_dirty_flags;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF5A0U, 0xF5A3U, 537U, checkpoint);
    checkpoint.accumulator = update.dirty_flags;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF5A3U, 0xF5A5U, 543U, checkpoint);
    append_checkpoint(0xF5A5U, 0xF5A8U, 558U, output);
    return replacement;
}
} // namespace devils_crush::app::detail
