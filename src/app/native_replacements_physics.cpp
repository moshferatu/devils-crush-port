// SPDX-License-Identifier: GPL-3.0-or-later

#include "native_replacements_internal.hpp"
#include "devils_crush/game_logic/collision.h"
#include "devils_crush/game_logic/physics.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace devils_crush::app::detail {

using emulation::ReplacementMemoryWrite;
using emulation::RoutineEntry;
using emulation::RoutineKey;
using emulation::RoutineReplacement;

[[nodiscard]] std::optional<RoutineReplacement>
replace_position_integration(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x00U,
        .logical_pc = 0xF1B4U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    const auto* const bytes =
        reinterpret_cast<const std::uint8_t*>(work_ram.data());
    dc_position_integration integration{};
    if (!dc_integrate_position(
            bytes,
            work_ram.size(),
            entry.registers.x,
            &integration)) {
        throw std::runtime_error(
            "Native F1B4 replacement rejected the work-RAM snapshot.");
    }
    // The traced game route uses slot zero. Decimal or T-mode arithmetic
    // would have different HuC6280 semantics, so those inputs stay emulated.
    if (entry.registers.x != 0U
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    constexpr std::array<std::size_t, 4> kComparedCacheIndexes{
        4U,
        5U,
        1U,
        2U,
    };
    std::optional<std::size_t> changed_integer_index;
    for (std::size_t index = 0U;
         index < kComparedCacheIndexes.size();
         ++index) {
        const auto byte_index = kComparedCacheIndexes[index];
        if (integration.cached_values[byte_index]
            != integration.position_values[byte_index]) {
            changed_integer_index = index;
            break;
        }
    }

    const auto sign_branch_count = static_cast<std::uint32_t>(
        integration.first_increment_negative
        + integration.second_increment_negative);
    const auto sign_branch_cycles = 12U * sign_branch_count;
    const std::uint8_t first_high_addend =
        integration.first_increment_negative != 0U ? 0xFFU : 0U;
    const std::uint8_t original_first_high =
        integration.cached_values[5];
    const std::uint8_t integrated_first_high =
        integration.position_values[5];
    const bool overflow =
        ((~(original_first_high ^ first_high_addend)
             & (original_first_high ^ integrated_first_high)
             & 0x80U)
            != 0U);

    auto output = entry.registers;
    output.program_counter = 0xF240U;
    output.status = static_cast<std::uint8_t>(
        entry.registers.status & 0x14U);
    if (overflow) {
        output.status |= 0x40U;
    }

    std::uint32_t body_master_cycles = 702U + sign_branch_cycles;
    if (changed_integer_index) {
        const auto byte_index =
            kComparedCacheIndexes[*changed_integer_index];
        const std::uint8_t left = integration.cached_values[byte_index];
        const std::uint8_t right = integration.position_values[byte_index];
        const std::uint8_t difference =
            static_cast<std::uint8_t>(left - right);
        output.accumulator = left;
        if (left >= right) {
            output.status |= 0x01U;
        }
        if (difference == 0U) {
            output.status |= 0x02U;
        }
        if ((difference & 0x80U) != 0U) {
            output.status |= 0x80U;
        }
        body_master_cycles =
            573U
            + 33U * static_cast<std::uint32_t>(*changed_integer_index)
            + sign_branch_cycles;
    } else {
        output.accumulator = integration.final_motion_flags;
        output.status |= 0x01U;
        if (integration.final_motion_flags == 0U) {
            output.status |= 0x02U;
        }
        if ((integration.final_motion_flags & 0x80U) != 0U) {
            output.status |= 0x80U;
        }
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x00U,
            .logical_pc = 0xF240U,
        },
        .output_registers = output,
        .body_master_cycles = body_master_cycles,
    };
    replacement.work_ram_writes.reserve(
        15U + sign_branch_count
        + static_cast<std::size_t>(!changed_integer_index.has_value()));

    // 00:F240 is this routine's only return and the walk from 00:F1B4 reaches
    // it without touching the stack, so the frame sits where the entry pointer
    // says and the pointer at the return is the entry's. Nothing composes this
    // replacement -- the registry is its only caller -- so it absorbs its own
    // return rather than needing an entry wrapper.
    //
    // There are two ways out of the body below and both arrive here with
    // `output_registers` and `body_master_cycles` already final, so the
    // absorption is expressed once and applied at whichever exit is taken.
    // Both append a checkpoint on 00:F240 immediately before returning, so the
    // atomic question cannot be what refuses an absorption here; it is asked
    // because a transcript with no checkpoint is not this routine's rule to
    // break.
    const auto absorb_f240 =
        [&entry, work_ram](RoutineReplacement& value) -> RoutineReplacement& {
            if (!value.checkpoints.empty()
                || absorbed_return_fits_atomically(
                    entry, value.body_master_cycles)) {
                absorb_return(
                    value,
                    work_ram,
                    entry.registers.stack_pointer,
                    {0x00U, 0xF240U},
                    value.body_master_cycles,
                    value.output_registers);
            }
            return value;
        };

    constexpr std::array<std::uint16_t, 6> kCacheStorePcs{
        0xF1B7U,
        0xF1BCU,
        0xF1C1U,
        0xF1C6U,
        0xF1CBU,
        0xF1D0U,
    };
    for (std::size_t index = 0U;
         index < kCacheStorePcs.size();
         ++index) {
        replacement.work_ram_writes.push_back({
            .instruction = {
                .physical_bank = 0x00U,
                .logical_pc = kCacheStorePcs[index],
            },
            .logical_address = static_cast<std::uint16_t>(
                0x2000U
                + integration.cache_work_ram_offsets[index]),
            .value = integration.cached_values[index],
            .master_cycle_offset =
                static_cast<std::uint32_t>(27U * (index + 1U)),
        });
    }
    replacement.work_ram_writes.push_back({
        .instruction = {0x00U, 0xF1D7U},
        .logical_address = 0x3C80U,
        .value = integration.cleared_motion_flags,
        .master_cycle_offset = 198U,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x00U, 0xF1DAU},
        .logical_address = 0x202BU,
        .value = 0x00U,
        .master_cycle_offset = 210U,
    });
    if (integration.second_increment_negative != 0U) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x00U, 0xF1E0U},
            .logical_address = 0x202BU,
            .value = 0xFFU,
            .master_cycle_offset = 246U,
        });
    }
    constexpr std::array<std::uint16_t, 6> kPositionStorePcs{
        0xF1E8U,
        0xF1F0U,
        0xF1F8U,
        0xF209U,
        0xF211U,
        0xF219U,
    };
    constexpr std::array<std::uint32_t, 6> kPositiveStoreCycles{
        282U,
        324U,
        366U,
        450U,
        492U,
        534U,
    };
    const auto second_sign_cycles =
        integration.second_increment_negative != 0U ? 12U : 0U;
    for (std::size_t index = 0U;
         index < kPositionStorePcs.size();
         ++index) {
        if (index == 3U) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x00U, 0xF1FBU},
                .logical_address = 0x202BU,
                .value = 0x00U,
                .master_cycle_offset = 378U + second_sign_cycles,
            });
            if (integration.first_increment_negative != 0U) {
                replacement.work_ram_writes.push_back({
                    .instruction = {0x00U, 0xF201U},
                    .logical_address = 0x202BU,
                    .value = 0xFFU,
                    .master_cycle_offset = 414U + second_sign_cycles,
                });
            }
        }
        replacement.work_ram_writes.push_back({
            .instruction = {
                .physical_bank = 0x00U,
                .logical_pc = kPositionStorePcs[index],
            },
            .logical_address = static_cast<std::uint16_t>(
                0x2000U
                + integration.position_work_ram_offsets[index]),
            .value = integration.position_values[index],
            .master_cycle_offset =
                kPositiveStoreCycles[index]
                + (index < 3U
                        ? second_sign_cycles
                        : sign_branch_cycles),
        });
    }
    if (!changed_integer_index) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x00U, 0xF23DU},
            .logical_address = 0x3C80U,
            .value = integration.final_motion_flags,
            .master_cycle_offset = body_master_cycles,
        });
    }

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
    const auto compare_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right) {
            const auto difference =
                static_cast<std::uint8_t>(left - right);
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0x01U));
            status = set_nz(status, difference);
            if (left >= right) {
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

    constexpr std::array<std::uint16_t, 6> kCacheLoadPcs{
        0xF1B4U,
        0xF1B9U,
        0xF1BEU,
        0xF1C3U,
        0xF1C8U,
        0xF1CDU,
    };
    constexpr std::array<std::uint16_t, 6> kCacheStoreResumePcs{
        0xF1B9U,
        0xF1BEU,
        0xF1C3U,
        0xF1C8U,
        0xF1CDU,
        0xF1D2U,
    };
    auto checkpoint = entry.registers;
    for (std::size_t index = 0U;
         index < kCacheLoadPcs.size();
         ++index) {
        checkpoint.accumulator = integration.cached_values[index];
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(
            kCacheLoadPcs[index],
            kCacheStorePcs[index],
            15U + 27U * static_cast<std::uint32_t>(index),
            checkpoint);
        append_checkpoint(
            kCacheStorePcs[index],
            kCacheStoreResumePcs[index],
            27U + 27U * static_cast<std::uint32_t>(index),
            checkpoint);
    }

    const auto original_motion_flags =
        std::to_integer<std::uint8_t>(work_ram[0x1C80U]);
    checkpoint.accumulator = original_motion_flags;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF1D2U, 0xF1D5U, 177U, checkpoint);
    checkpoint.accumulator = integration.cleared_motion_flags;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF1D5U, 0xF1D7U, 183U, checkpoint);
    append_checkpoint(0xF1D7U, 0xF1DAU, 198U, checkpoint);
    append_checkpoint(0xF1DAU, 0xF1DCU, 210U, checkpoint);

    const auto second_increment_fraction =
        std::to_integer<std::uint8_t>(work_ram[0x00BBU]);
    const auto second_increment_integer =
        std::to_integer<std::uint8_t>(work_ram[0x00BCU]);
    const auto first_increment_fraction =
        std::to_integer<std::uint8_t>(work_ram[0x00B9U]);
    const auto first_increment_integer =
        std::to_integer<std::uint8_t>(work_ram[0x00BAU]);

    checkpoint.accumulator = second_increment_integer;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF1DCU, 0xF1DEU, 222U, checkpoint);
    if (integration.second_increment_negative != 0U) {
        append_checkpoint(0xF1DEU, 0xF1E0U, 228U, checkpoint);
        checkpoint.status = set_nz(checkpoint.status, 0xFFU);
        append_checkpoint(0xF1E0U, 0xF1E2U, 246U, checkpoint);
    } else {
        append_checkpoint(0xF1DEU, 0xF1E2U, 234U, checkpoint);
    }

    const auto second_cycles =
        integration.second_increment_negative != 0U ? 12U : 0U;
    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status & ~0x01U);
    append_checkpoint(
        0xF1E2U,
        0xF1E3U,
        240U + second_cycles,
        checkpoint);

    checkpoint.accumulator = integration.cached_values[0];
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xF1E3U,
        0xF1E6U,
        255U + second_cycles,
        checkpoint);
    const auto second_fraction_sum = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(integration.cached_values[0])
        + second_increment_fraction);
    bool arithmetic_carry = second_fraction_sum > 0xFFU;
    checkpoint.accumulator = integration.position_values[0];
    checkpoint.status = adc_status(
        checkpoint.status,
        integration.cached_values[0],
        second_increment_fraction,
        false,
        checkpoint.accumulator);
    append_checkpoint(
        0xF1E6U,
        0xF1E8U,
        267U + second_cycles,
        checkpoint);
    append_checkpoint(
        0xF1E8U,
        0xF1EBU,
        282U + second_cycles,
        checkpoint);

    checkpoint.accumulator = integration.cached_values[1];
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xF1EBU,
        0xF1EEU,
        297U + second_cycles,
        checkpoint);
    const auto second_integer_sum = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(integration.cached_values[1])
        + static_cast<std::uint16_t>(second_increment_integer)
        + static_cast<std::uint16_t>(arithmetic_carry));
    const bool second_integer_carry = second_integer_sum > 0xFFU;
    checkpoint.accumulator = integration.position_values[1];
    checkpoint.status = adc_status(
        checkpoint.status,
        integration.cached_values[1],
        second_increment_integer,
        arithmetic_carry,
        checkpoint.accumulator);
    append_checkpoint(
        0xF1EEU,
        0xF1F0U,
        309U + second_cycles,
        checkpoint);
    append_checkpoint(
        0xF1F0U,
        0xF1F3U,
        324U + second_cycles,
        checkpoint);

    checkpoint.accumulator = integration.cached_values[2];
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xF1F3U,
        0xF1F6U,
        339U + second_cycles,
        checkpoint);
    const auto second_sign_extension = static_cast<std::uint8_t>(
        integration.second_increment_negative != 0U ? 0xFFU : 0x00U);
    checkpoint.accumulator = integration.position_values[2];
    checkpoint.status = adc_status(
        checkpoint.status,
        integration.cached_values[2],
        second_sign_extension,
        second_integer_carry,
        checkpoint.accumulator);
    append_checkpoint(
        0xF1F6U,
        0xF1F8U,
        351U + second_cycles,
        checkpoint);
    append_checkpoint(
        0xF1F8U,
        0xF1FBU,
        366U + second_cycles,
        checkpoint);
    append_checkpoint(
        0xF1FBU,
        0xF1FDU,
        378U + second_cycles,
        checkpoint);

    checkpoint.accumulator = first_increment_integer;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xF1FDU,
        0xF1FFU,
        390U + second_cycles,
        checkpoint);
    if (integration.first_increment_negative != 0U) {
        append_checkpoint(
            0xF1FFU,
            0xF201U,
            396U + second_cycles,
            checkpoint);
        checkpoint.status = set_nz(checkpoint.status, 0xFFU);
        append_checkpoint(
            0xF201U,
            0xF203U,
            414U + second_cycles,
            checkpoint);
    } else {
        append_checkpoint(
            0xF1FFU,
            0xF203U,
            402U + second_cycles,
            checkpoint);
    }

    const auto both_sign_cycles = sign_branch_cycles;
    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status & ~0x01U);
    append_checkpoint(
        0xF203U,
        0xF204U,
        408U + both_sign_cycles,
        checkpoint);

    checkpoint.accumulator = integration.cached_values[3];
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xF204U,
        0xF207U,
        423U + both_sign_cycles,
        checkpoint);
    const auto first_fraction_sum = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(integration.cached_values[3])
        + first_increment_fraction);
    arithmetic_carry = first_fraction_sum > 0xFFU;
    checkpoint.accumulator = integration.position_values[3];
    checkpoint.status = adc_status(
        checkpoint.status,
        integration.cached_values[3],
        first_increment_fraction,
        false,
        checkpoint.accumulator);
    append_checkpoint(
        0xF207U,
        0xF209U,
        435U + both_sign_cycles,
        checkpoint);
    append_checkpoint(
        0xF209U,
        0xF20CU,
        450U + both_sign_cycles,
        checkpoint);

    checkpoint.accumulator = integration.cached_values[4];
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xF20CU,
        0xF20FU,
        465U + both_sign_cycles,
        checkpoint);
    const auto first_integer_sum = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(integration.cached_values[4])
        + static_cast<std::uint16_t>(first_increment_integer)
        + static_cast<std::uint16_t>(arithmetic_carry));
    const bool first_integer_carry = first_integer_sum > 0xFFU;
    checkpoint.accumulator = integration.position_values[4];
    checkpoint.status = adc_status(
        checkpoint.status,
        integration.cached_values[4],
        first_increment_integer,
        arithmetic_carry,
        checkpoint.accumulator);
    append_checkpoint(
        0xF20FU,
        0xF211U,
        477U + both_sign_cycles,
        checkpoint);
    append_checkpoint(
        0xF211U,
        0xF214U,
        492U + both_sign_cycles,
        checkpoint);

    checkpoint.accumulator = integration.cached_values[5];
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xF214U,
        0xF217U,
        507U + both_sign_cycles,
        checkpoint);
    const auto first_sign_extension = static_cast<std::uint8_t>(
        integration.first_increment_negative != 0U ? 0xFFU : 0x00U);
    checkpoint.accumulator = integration.position_values[5];
    checkpoint.status = adc_status(
        checkpoint.status,
        integration.cached_values[5],
        first_sign_extension,
        first_integer_carry,
        checkpoint.accumulator);
    append_checkpoint(
        0xF217U,
        0xF219U,
        519U + both_sign_cycles,
        checkpoint);
    append_checkpoint(
        0xF219U,
        0xF21CU,
        534U + both_sign_cycles,
        checkpoint);

    constexpr std::array<std::uint16_t, 4> kCompareLoadPcs{
        0xF21CU,
        0xF223U,
        0xF22AU,
        0xF231U,
    };
    constexpr std::array<std::uint16_t, 4> kComparePcs{
        0xF21EU,
        0xF225U,
        0xF22CU,
        0xF233U,
    };
    constexpr std::array<std::uint16_t, 4> kCompareBranchPcs{
        0xF221U,
        0xF228U,
        0xF22FU,
        0xF236U,
    };
    constexpr std::array<std::uint16_t, 4> kCompareNextPcs{
        0xF223U,
        0xF22AU,
        0xF231U,
        0xF238U,
    };
    for (std::size_t index = 0U;
         index < kComparedCacheIndexes.size();
         ++index) {
        const auto byte_index = kComparedCacheIndexes[index];
        const auto comparison_cycles =
            33U * static_cast<std::uint32_t>(index)
            + both_sign_cycles;
        checkpoint.accumulator =
            integration.cached_values[byte_index];
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(
            kCompareLoadPcs[index],
            kComparePcs[index],
            546U + comparison_cycles,
            checkpoint);
        checkpoint.status = compare_status(
            checkpoint.status,
            checkpoint.accumulator,
            integration.position_values[byte_index]);
        append_checkpoint(
            kComparePcs[index],
            kCompareBranchPcs[index],
            561U + comparison_cycles,
            checkpoint);
        if (changed_integer_index
            && *changed_integer_index == index) {
            append_checkpoint(
                kCompareBranchPcs[index],
                0xF240U,
                573U + comparison_cycles,
                output);
            return absorb_f240(replacement);
        }
        append_checkpoint(
            kCompareBranchPcs[index],
            kCompareNextPcs[index],
            567U + comparison_cycles,
            checkpoint);
    }

    checkpoint.accumulator = integration.cleared_motion_flags;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xF238U,
        0xF23BU,
        681U + both_sign_cycles,
        checkpoint);
    checkpoint.accumulator = integration.final_motion_flags;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xF23BU,
        0xF23DU,
        687U + both_sign_cycles,
        checkpoint);
    append_checkpoint(
        0xF23DU,
        0xF240U,
        702U + both_sign_cycles,
        output);
    return absorb_f240(replacement);
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_signed_fixed_multiply(
    const RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<RoutineReplacement>
replace_signed_word_negation(
    const RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<RoutineReplacement>
replace_motion_rotation(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr std::size_t kMaximumReplacementWrites = 512U;
    constexpr std::size_t kMaximumReplacementCheckpoints = 2048U;
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xD1A2U,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || entry.registers.x >= 0x80U
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_motion_rotation rotation{};
    if (!dc_rotate_motion(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &rotation)) {
        throw std::runtime_error(
            "Native D1A2 replacement rejected the work-RAM snapshot.");
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xD227U,
        },
        .output_registers = entry.registers,
    };
    replacement.work_ram_writes.reserve(512U);
    replacement.checkpoints.reserve(512U);
    std::vector<std::byte> staged(work_ram.begin(), work_ram.end());

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
    const auto append_checkpoint =
        [&replacement](
            const std::uint8_t instruction_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {
                    instruction_bank,
                    instruction_pc,
                },
                .registers = registers,
            });
        };
    const auto append_write =
        [&replacement, &staged](
            const std::uint8_t instruction_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t logical_address,
            const std::uint8_t value,
            const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {
                    instruction_bank,
                    instruction_pc,
                },
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
            if (logical_address >= 0x2000U
                && logical_address < 0x4000U) {
                staged[logical_address - 0x2000U] =
                    static_cast<std::byte>(value);
            }
        };

    auto checkpoint = entry.registers;
    std::uint32_t cycle = 0U;
    const auto load_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            checkpoint.accumulator =
                std::to_integer<std::uint8_t>(staged[offset]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto store_zero_page =
        [&checkpoint,
         &cycle,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(0x2000U + offset),
                checkpoint.accumulator,
                cycle);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto load_indexed =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t logical_base) {
            cycle += 15U;
            checkpoint.accumulator = std::to_integer<std::uint8_t>(
                staged[logical_base - 0x2000U + checkpoint.x]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto store_indexed =
        [&checkpoint,
         &cycle,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t logical_base) {
            cycle += 15U;
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(
                    logical_base + checkpoint.x),
                checkpoint.accumulator,
                cycle);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto clear_carry =
        [&checkpoint, &cycle, &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc) {
            cycle += 6U;
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status
                & static_cast<std::uint8_t>(~0x21U));
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto add_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &adc_status,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            const auto left = checkpoint.accumulator;
            const auto right =
                std::to_integer<std::uint8_t>(staged[offset]);
            const auto carry_in =
                (checkpoint.status & 0x01U) != 0U;
            checkpoint.accumulator = static_cast<std::uint8_t>(
                static_cast<std::uint16_t>(left)
                + static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in));
            checkpoint.status = adc_status(
                checkpoint.status,
                left,
                right,
                carry_in,
                checkpoint.accumulator);
            cycle += 12U;
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto begin_jsr =
        [&checkpoint,
         &cycle,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t target_pc,
            const std::uint16_t pushed_return_pc) {
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(
                    0x2100U + checkpoint.stack_pointer),
                static_cast<std::uint8_t>(pushed_return_pc >> 8U),
                cycle);
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(
                    0x2100U
                    + static_cast<std::uint8_t>(
                        checkpoint.stack_pointer - 1U)),
                static_cast<std::uint8_t>(pushed_return_pc),
                cycle);
            checkpoint.stack_pointer = static_cast<std::uint8_t>(
                checkpoint.stack_pointer - 2U);
            cycle += 21U;
            append_checkpoint(
                0x01U,
                instruction_pc,
                target_pc,
                cycle,
                checkpoint);
        };
    const auto append_child =
        [&replacement,
         &staged,
         &checkpoint,
         &cycle,
         &append_checkpoint](
            const RoutineReplacement& child,
            const std::uint16_t resume_pc) {
            const auto child_offset = cycle;
            for (auto write : child.work_ram_writes) {
                write.master_cycle_offset += child_offset;
                replacement.work_ram_writes.push_back(write);
                staged[write.logical_address - 0x2000U] =
                    static_cast<std::byte>(write.value);
            }
            for (auto child_checkpoint : child.checkpoints) {
                child_checkpoint.interrupt_sample_master_cycle_offset +=
                    child_offset;
                child_checkpoint.master_cycle_offset += child_offset;
                replacement.checkpoints.push_back(child_checkpoint);
            }
            cycle += child.body_master_cycles;
            checkpoint = child.output_registers;
            checkpoint.stack_pointer = static_cast<std::uint8_t>(
                checkpoint.stack_pointer + 2U);
            cycle += 21U;
            append_checkpoint(
                child.return_instruction.physical_bank,
                child.return_instruction.logical_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto run_signed_multiply =
        [&checkpoint,
         &staged,
         &begin_jsr,
         &append_child](
            const std::uint16_t instruction_pc,
            const std::uint16_t pushed_return_pc,
            const std::uint16_t resume_pc) {
            begin_jsr(
                instruction_pc,
                0xE65CU,
                pushed_return_pc);
            RoutineEntry child_entry{
                .routine = {
                    .physical_bank = 0x00U,
                    .logical_pc = 0xE65CU,
                },
                .registers = checkpoint,
            };
            child_entry.registers.program_counter = 0xE65CU;
            auto child = replace_signed_fixed_multiply(
                child_entry,
                staged);
            if (!child || child->checkpoints.empty()) {
                return false;
            }
            append_child(*child, resume_pc);
            return true;
        };
    const auto run_negation =
        [&checkpoint,
         &staged,
         &begin_jsr,
         &append_child](
            const std::uint16_t instruction_pc,
            const std::uint16_t pushed_return_pc,
            const std::uint16_t resume_pc) {
            begin_jsr(
                instruction_pc,
                0xE837U,
                pushed_return_pc);
            RoutineEntry child_entry{
                .routine = {
                    .physical_bank = 0x00U,
                    .logical_pc = 0xE837U,
                },
                .registers = checkpoint,
            };
            child_entry.registers.program_counter = 0xE837U;
            auto child = replace_signed_word_negation(
                child_entry,
                staged);
            if (!child || child->checkpoints.empty()) {
                return false;
            }
            append_child(*child, resume_pc);
            return true;
        };

    load_zero_page(0xD1A2U, 0xD1A4U, 0x00C0U);
    store_zero_page(0xD1A4U, 0xD1A6U, 0x002FU);
    load_zero_page(0xD1A6U, 0xD1A8U, 0x00C1U);
    store_zero_page(0xD1A8U, 0xD1AAU, 0x0030U);
    load_zero_page(0xD1AAU, 0xD1ACU, 0x007DU);
    store_zero_page(0xD1ACU, 0xD1AEU, 0x002DU);
    load_zero_page(0xD1AEU, 0xD1B0U, 0x007EU);
    store_zero_page(0xD1B0U, 0xD1B2U, 0x002EU);
    if (!run_signed_multiply(
            0xD1B2U,
            0xD1B4U,
            0xD1B5U)) {
        return std::nullopt;
    }
    load_zero_page(0xD1B5U, 0xD1B7U, 0x002FU);
    store_indexed(0xD1B7U, 0xD1BAU, 0x3400U);
    load_zero_page(0xD1BAU, 0xD1BCU, 0x0030U);
    store_indexed(0xD1BCU, 0xD1BFU, 0x3480U);

    load_zero_page(0xD1BFU, 0xD1C1U, 0x00BEU);
    store_zero_page(0xD1C1U, 0xD1C3U, 0x002FU);
    load_zero_page(0xD1C3U, 0xD1C5U, 0x00BFU);
    store_zero_page(0xD1C5U, 0xD1C7U, 0x0030U);
    load_zero_page(0xD1C7U, 0xD1C9U, 0x007FU);
    store_zero_page(0xD1C9U, 0xD1CBU, 0x002DU);
    load_zero_page(0xD1CBU, 0xD1CDU, 0x0080U);
    store_zero_page(0xD1CDU, 0xD1CFU, 0x002EU);
    if (!run_signed_multiply(
            0xD1CFU,
            0xD1D1U,
            0xD1D2U)
        || !run_negation(
            0xD1D2U,
            0xD1D4U,
            0xD1D5U)) {
        return std::nullopt;
    }
    clear_carry(0xD1D5U, 0xD1D6U);
    load_indexed(0xD1D6U, 0xD1D9U, 0x3400U);
    add_zero_page(0xD1D9U, 0xD1DBU, 0x002FU);
    store_indexed(0xD1DBU, 0xD1DEU, 0x3400U);
    load_indexed(0xD1DEU, 0xD1E1U, 0x3480U);
    add_zero_page(0xD1E1U, 0xD1E3U, 0x0030U);
    store_indexed(0xD1E3U, 0xD1E6U, 0x3480U);

    load_zero_page(0xD1E6U, 0xD1E8U, 0x00C0U);
    store_zero_page(0xD1E8U, 0xD1EAU, 0x002FU);
    load_zero_page(0xD1EAU, 0xD1ECU, 0x00C1U);
    store_zero_page(0xD1ECU, 0xD1EEU, 0x0030U);
    load_zero_page(0xD1EEU, 0xD1F0U, 0x007FU);
    store_zero_page(0xD1F0U, 0xD1F2U, 0x002DU);
    load_zero_page(0xD1F2U, 0xD1F4U, 0x0080U);
    store_zero_page(0xD1F4U, 0xD1F6U, 0x002EU);
    if (!run_signed_multiply(
            0xD1F6U,
            0xD1F8U,
            0xD1F9U)) {
        return std::nullopt;
    }
    load_zero_page(0xD1F9U, 0xD1FBU, 0x002FU);
    store_indexed(0xD1FBU, 0xD1FEU, 0x3500U);
    load_zero_page(0xD1FEU, 0xD200U, 0x0030U);
    store_indexed(0xD200U, 0xD203U, 0x3580U);

    load_zero_page(0xD203U, 0xD205U, 0x00BEU);
    store_zero_page(0xD205U, 0xD207U, 0x002FU);
    load_zero_page(0xD207U, 0xD209U, 0x00BFU);
    store_zero_page(0xD209U, 0xD20BU, 0x0030U);
    load_zero_page(0xD20BU, 0xD20DU, 0x007DU);
    store_zero_page(0xD20DU, 0xD20FU, 0x002DU);
    load_zero_page(0xD20FU, 0xD211U, 0x007EU);
    store_zero_page(0xD211U, 0xD213U, 0x002EU);
    if (!run_signed_multiply(
            0xD213U,
            0xD215U,
            0xD216U)) {
        return std::nullopt;
    }
    clear_carry(0xD216U, 0xD217U);
    load_indexed(0xD217U, 0xD21AU, 0x3500U);
    add_zero_page(0xD21AU, 0xD21CU, 0x002FU);
    store_indexed(0xD21CU, 0xD21FU, 0x3500U);
    load_indexed(0xD21FU, 0xD222U, 0x3580U);
    add_zero_page(0xD222U, 0xD224U, 0x0030U);
    store_indexed(0xD224U, 0xD227U, 0x3580U);

    const auto first_output = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(
            staged[0x1400U + checkpoint.x])
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(
                   staged[0x1480U + checkpoint.x]))
           << 8U));
    const auto second_output = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(
            staged[0x1500U + checkpoint.x])
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(
                   staged[0x1580U + checkpoint.x]))
           << 8U));
    if (first_output != rotation.first_output
        || second_output != rotation.second_output
        || checkpoint.stack_pointer
            != entry.registers.stack_pointer) {
        throw std::runtime_error(
            "Native D1A2 replacement composition diverged from its "
            "state model.");
    }
    if (replacement.work_ram_writes.size()
            > kMaximumReplacementWrites
        || replacement.checkpoints.size()
            > kMaximumReplacementCheckpoints) {
        return std::nullopt;
    }
    checkpoint.program_counter = 0xD227U;
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = cycle;
    absorb_return(
        replacement,
        work_ram,
        entry.registers.stack_pointer,
        {0x01U, 0xD227U},
        cycle,
        checkpoint);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_flipper_radius_projection(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr std::size_t kMaximumReplacementWrites = 512U;
    constexpr std::size_t kMaximumReplacementCheckpoints = 2048U;
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xD612U,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_flipper_radius_projection projection{};
    if (!dc_project_flipper_radius(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &projection)) {
        throw std::runtime_error(
            "Native D612 replacement rejected the work-RAM snapshot.");
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xD62DU,
        },
        .output_registers = entry.registers,
    };
    replacement.work_ram_writes.reserve(512U);
    replacement.checkpoints.reserve(512U);
    std::vector<std::byte> staged(work_ram.begin(), work_ram.end());

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
    const auto append_checkpoint =
        [&replacement](
            const std::uint8_t instruction_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {
                    instruction_bank,
                    instruction_pc,
                },
                .registers = registers,
            });
        };
    const auto append_write =
        [&replacement, &staged](
            const std::uint8_t instruction_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t logical_address,
            const std::uint8_t value,
            const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {
                    instruction_bank,
                    instruction_pc,
                },
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
            staged[logical_address - 0x2000U] =
                static_cast<std::byte>(value);
        };

    auto checkpoint = entry.registers;
    std::uint32_t cycle = 0U;
    const auto load_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            checkpoint.accumulator =
                std::to_integer<std::uint8_t>(staged[offset]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto store_zero_page =
        [&checkpoint,
         &cycle,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(0x2000U + offset),
                checkpoint.accumulator,
                cycle);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };

    load_zero_page(0xD612U, 0xD614U, 0x0031U);
    store_zero_page(0xD614U, 0xD616U, 0x002FU);
    load_zero_page(0xD616U, 0xD618U, 0x0032U);
    store_zero_page(0xD618U, 0xD61AU, 0x0030U);
    load_zero_page(0xD61AU, 0xD61CU, 0x007DU);
    store_zero_page(0xD61CU, 0xD61EU, 0x002DU);
    load_zero_page(0xD61EU, 0xD620U, 0x007EU);
    store_zero_page(0xD620U, 0xD622U, 0x002EU);

    append_write(
        0x01U,
        0xD622U,
        static_cast<std::uint16_t>(
            0x2100U + checkpoint.stack_pointer),
        0xD6U,
        cycle);
    append_write(
        0x01U,
        0xD622U,
        static_cast<std::uint16_t>(
            0x2100U
            + static_cast<std::uint8_t>(
                checkpoint.stack_pointer - 1U)),
        0x24U,
        cycle);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 2U);
    cycle += 21U;
    append_checkpoint(
        0x01U,
        0xD622U,
        0xE65CU,
        cycle,
        checkpoint);

    RoutineEntry child_entry{
        .routine = {
            .physical_bank = 0x00U,
            .logical_pc = 0xE65CU,
        },
        .registers = checkpoint,
    };
    child_entry.registers.program_counter = 0xE65CU;
    auto child = replace_signed_fixed_multiply(
        child_entry,
        staged);
    if (!child || child->checkpoints.empty()) {
        return std::nullopt;
    }
    const auto child_offset = cycle;
    for (auto write : child->work_ram_writes) {
        write.master_cycle_offset += child_offset;
        replacement.work_ram_writes.push_back(write);
        staged[write.logical_address - 0x2000U] =
            static_cast<std::byte>(write.value);
    }
    for (auto child_checkpoint : child->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset +=
            child_offset;
        child_checkpoint.master_cycle_offset += child_offset;
        replacement.checkpoints.push_back(child_checkpoint);
    }
    cycle += child->body_master_cycles;
    checkpoint = child->output_registers;
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 2U);
    cycle += 21U;
    append_checkpoint(
        child->return_instruction.physical_bank,
        child->return_instruction.logical_pc,
        0xD625U,
        cycle,
        checkpoint);

    load_zero_page(0xD625U, 0xD627U, 0x002FU);
    store_zero_page(0xD627U, 0xD629U, 0x00C2U);
    load_zero_page(0xD629U, 0xD62BU, 0x0030U);
    store_zero_page(0xD62BU, 0xD62DU, 0x00C3U);

    const auto staged_output = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(staged[0x00C2U])
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(staged[0x00C3U]))
           << 8U));
    if (staged_output != projection.output
        || checkpoint.stack_pointer
            != entry.registers.stack_pointer) {
        throw std::runtime_error(
            "Native D612 replacement composition diverged from its "
            "state model.");
    }
    if (replacement.work_ram_writes.size()
            > kMaximumReplacementWrites
        || replacement.checkpoints.size()
            > kMaximumReplacementCheckpoints) {
        return std::nullopt;
    }
    checkpoint.program_counter = 0xD62DU;
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = cycle;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_flipper_angle_projection(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr std::size_t kMaximumReplacementWrites = 512U;
    constexpr std::size_t kMaximumReplacementCheckpoints = 2048U;
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xD62EU,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || entry.registers.x >= 0x80U
        || entry.registers.y >= 0x80U
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_flipper_angle_projection projection{};
    if (!dc_project_flipper_angle(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            entry.registers.y,
            &projection)) {
        throw std::runtime_error(
            "Native D62E replacement rejected the work-RAM snapshot.");
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xD68DU,
        },
        .output_registers = entry.registers,
    };
    replacement.work_ram_writes.reserve(512U);
    replacement.checkpoints.reserve(512U);
    std::vector<std::byte> staged(work_ram.begin(), work_ram.end());

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
            const auto difference = static_cast<std::int16_t>(
                static_cast<std::int16_t>(left)
                - static_cast<std::int16_t>(right)
                - static_cast<std::int16_t>(carry_in ? 0 : 1));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC1U));
            status = set_nz(status, result);
            if (difference >= 0) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if ((((left ^ right) & (left ^ result)) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x40U);
            }
            return status;
        };
    const auto append_checkpoint =
        [&replacement](
            const std::uint8_t instruction_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {
                    instruction_bank,
                    instruction_pc,
                },
                .registers = registers,
            });
        };
    const auto append_write =
        [&replacement, &staged](
            const std::uint8_t instruction_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t logical_address,
            const std::uint8_t value,
            const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {
                    instruction_bank,
                    instruction_pc,
                },
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
            staged[logical_address - 0x2000U] =
                static_cast<std::byte>(value);
        };

    auto checkpoint = entry.registers;
    std::uint32_t cycle = 0U;
    const auto load_indexed =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t logical_base,
            const std::uint8_t index) {
            cycle += 15U;
            checkpoint.accumulator = std::to_integer<std::uint8_t>(
                staged[logical_base - 0x2000U + index]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto load_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            checkpoint.accumulator =
                std::to_integer<std::uint8_t>(staged[offset]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto store_zero_page =
        [&checkpoint,
         &cycle,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(0x2000U + offset),
                checkpoint.accumulator,
                cycle);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto store_zero =
        [&checkpoint,
         &cycle,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(0x2000U + offset),
                0U,
                cycle);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto set_carry =
        [&checkpoint, &cycle, &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc) {
            cycle += 6U;
            checkpoint.status = static_cast<std::uint8_t>(
                (checkpoint.status
                 & static_cast<std::uint8_t>(~0x20U))
                | 0x01U);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto clear_carry =
        [&checkpoint, &cycle, &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc) {
            cycle += 6U;
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status
                & static_cast<std::uint8_t>(~0x21U));
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto subtract_indexed =
        [&checkpoint,
         &cycle,
         &staged,
         &sbc_status,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t logical_base,
            const std::uint8_t index) {
            const auto left = checkpoint.accumulator;
            const auto right = std::to_integer<std::uint8_t>(
                staged[logical_base - 0x2000U + index]);
            const auto carry_in =
                (checkpoint.status & 0x01U) != 0U;
            checkpoint.accumulator = static_cast<std::uint8_t>(
                static_cast<std::uint16_t>(left)
                - static_cast<std::uint16_t>(right)
                - static_cast<std::uint16_t>(carry_in ? 0U : 1U));
            checkpoint.status = sbc_status(
                checkpoint.status,
                left,
                right,
                carry_in,
                checkpoint.accumulator);
            cycle += 15U;
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };

    load_indexed(
        0xD62EU,
        0xD631U,
        0x3C80U,
        checkpoint.y);
    checkpoint.accumulator =
        static_cast<std::uint8_t>(checkpoint.accumulator & 0x01U);
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    cycle += 6U;
    append_checkpoint(
        0x01U,
        0xD631U,
        0xD633U,
        cycle,
        checkpoint);

    if (projection.uses_wrapped_difference != 0U) {
        cycle += 12U;
        append_checkpoint(
            0x01U,
            0xD633U,
            0xD641U,
            cycle,
            checkpoint);
        clear_carry(0xD641U, 0xD642U);
        load_indexed(
            0xD642U,
            0xD645U,
            0x3180U,
            checkpoint.y);
        const auto left = checkpoint.accumulator;
        checkpoint.accumulator = static_cast<std::uint8_t>(
            checkpoint.accumulator + 0x10U);
        checkpoint.status = adc_status(
            checkpoint.status,
            left,
            0x10U,
            false,
            checkpoint.accumulator);
        cycle += 6U;
        append_checkpoint(
            0x01U,
            0xD645U,
            0xD647U,
            cycle,
            checkpoint);
        set_carry(0xD647U, 0xD648U);
        subtract_indexed(
            0xD648U,
            0xD64BU,
            0x3180U,
            checkpoint.x);
        cycle += projection.returns_zero != 0U ? 12U : 6U;
        append_checkpoint(
            0x01U,
            0xD64BU,
            projection.returns_zero != 0U
                ? 0xD68EU
                : 0xD64DU,
            cycle,
            checkpoint);
    } else {
        cycle += 6U;
        append_checkpoint(
            0x01U,
            0xD633U,
            0xD635U,
            cycle,
            checkpoint);
        set_carry(0xD635U, 0xD636U);
        load_indexed(
            0xD636U,
            0xD639U,
            0x3180U,
            checkpoint.x);
        subtract_indexed(
            0xD639U,
            0xD63CU,
            0x3180U,
            checkpoint.y);
        cycle += projection.returns_zero != 0U ? 6U : 12U;
        append_checkpoint(
            0x01U,
            0xD63CU,
            projection.returns_zero != 0U
                ? 0xD63EU
                : 0xD64DU,
            cycle,
            checkpoint);
        if (projection.returns_zero != 0U) {
            cycle += 12U;
            append_checkpoint(
                0x01U,
                0xD63EU,
                0xD68EU,
                cycle,
                checkpoint);
        }
    }

    if (projection.returns_zero != 0U) {
        store_zero(0xD68EU, 0xD690U, 0x0033U);
        store_zero(0xD690U, 0xD692U, 0x0031U);
        store_zero(0xD692U, 0xD694U, 0x0032U);
        if (std::to_integer<std::uint8_t>(staged[0x0031U]) != 0U
            || std::to_integer<std::uint8_t>(staged[0x0032U]) != 0U
            || std::to_integer<std::uint8_t>(staged[0x0033U]) != 0U
            || replacement.work_ram_writes.size()
                > kMaximumReplacementWrites
            || replacement.checkpoints.size()
                > kMaximumReplacementCheckpoints) {
            throw std::runtime_error(
                "Native D62E zero-exit composition diverged from its "
                "state model.");
        }
        replacement.return_instruction.logical_pc = 0xD694U;
        checkpoint.program_counter = 0xD694U;
        replacement.output_registers = checkpoint;
        replacement.body_master_cycles = cycle;
        return replacement;
    }

    store_zero_page(0xD64DU, 0xD64FU, 0x0033U);
    store_zero(0xD64FU, 0xD651U, 0x002FU);
    constexpr std::array<std::uint16_t, 5> kShiftPcs{
        0xD651U,
        0xD654U,
        0xD657U,
        0xD65AU,
        0xD65DU,
    };
    constexpr std::array<std::uint16_t, 5> kRotatePcs{
        0xD652U,
        0xD655U,
        0xD658U,
        0xD65BU,
        0xD65EU,
    };
    for (std::size_t index = 0U;
         index < kShiftPcs.size();
         ++index) {
        const bool carry =
            (checkpoint.accumulator & 0x01U) != 0U;
        checkpoint.accumulator =
            static_cast<std::uint8_t>(checkpoint.accumulator >> 1U);
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        checkpoint.status = static_cast<std::uint8_t>(
            checkpoint.status & static_cast<std::uint8_t>(~0x01U));
        if (carry) {
            checkpoint.status =
                static_cast<std::uint8_t>(checkpoint.status | 0x01U);
        }
        cycle += 6U;
        append_checkpoint(
            0x01U,
            kShiftPcs[index],
            kRotatePcs[index],
            cycle,
            checkpoint);

        const auto old_value =
            std::to_integer<std::uint8_t>(staged[0x002FU]);
        const bool old_carry =
            (checkpoint.status & 0x01U) != 0U;
        const bool next_carry = (old_value & 0x01U) != 0U;
        const auto rotated = static_cast<std::uint8_t>(
            (old_value >> 1U) | (old_carry ? 0x80U : 0U));
        checkpoint.status = set_nz(checkpoint.status, rotated);
        checkpoint.status = static_cast<std::uint8_t>(
            checkpoint.status & static_cast<std::uint8_t>(~0x01U));
        if (next_carry) {
            checkpoint.status =
                static_cast<std::uint8_t>(checkpoint.status | 0x01U);
        }
        cycle += 18U;
        append_write(
            0x01U,
            kRotatePcs[index],
            0x202FU,
            rotated,
            cycle);
        append_checkpoint(
            0x01U,
            kRotatePcs[index],
            static_cast<std::uint16_t>(
                index + 1U < kShiftPcs.size()
                    ? kShiftPcs[index + 1U]
                    : 0xD660U),
            cycle,
            checkpoint);
    }

    store_zero_page(0xD660U, 0xD662U, 0x0030U);
    load_zero_page(0xD662U, 0xD664U, 0x009EU);
    store_zero_page(0xD664U, 0xD666U, 0x002DU);
    load_zero_page(0xD666U, 0xD668U, 0x009FU);
    store_zero_page(0xD668U, 0xD66AU, 0x002EU);

    append_write(
        0x01U,
        0xD66AU,
        static_cast<std::uint16_t>(
            0x2100U + checkpoint.stack_pointer),
        0xD6U,
        cycle);
    append_write(
        0x01U,
        0xD66AU,
        static_cast<std::uint16_t>(
            0x2100U
            + static_cast<std::uint8_t>(
                checkpoint.stack_pointer - 1U)),
        0x6CU,
        cycle);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 2U);
    cycle += 21U;
    append_checkpoint(
        0x01U,
        0xD66AU,
        0xE65CU,
        cycle,
        checkpoint);

    RoutineEntry child_entry{
        .routine = {
            .physical_bank = 0x00U,
            .logical_pc = 0xE65CU,
        },
        .registers = checkpoint,
    };
    child_entry.registers.program_counter = 0xE65CU;
    auto child = replace_signed_fixed_multiply(
        child_entry,
        staged);
    if (!child || child->checkpoints.empty()) {
        return std::nullopt;
    }
    const auto child_offset = cycle;
    for (auto write : child->work_ram_writes) {
        write.master_cycle_offset += child_offset;
        replacement.work_ram_writes.push_back(write);
        staged[write.logical_address - 0x2000U] =
            static_cast<std::byte>(write.value);
    }
    for (auto child_checkpoint : child->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset +=
            child_offset;
        child_checkpoint.master_cycle_offset += child_offset;
        replacement.checkpoints.push_back(child_checkpoint);
    }
    cycle += child->body_master_cycles;
    checkpoint = child->output_registers;
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 2U);
    cycle += 21U;
    append_checkpoint(
        child->return_instruction.physical_bank,
        child->return_instruction.logical_pc,
        0xD66DU,
        cycle,
        checkpoint);

    load_indexed(
        0xD66DU,
        0xD670U,
        0x3C80U,
        checkpoint.y);
    cycle += projection.negates_product != 0U ? 12U : 6U;
    append_checkpoint(
        0x01U,
        0xD670U,
        projection.negates_product != 0U
            ? 0xD67BU
            : 0xD672U,
        cycle,
        checkpoint);

    if (projection.negates_product == 0U) {
        load_zero_page(0xD672U, 0xD674U, 0x002FU);
        store_zero_page(0xD674U, 0xD676U, 0x0031U);
        load_zero_page(0xD676U, 0xD678U, 0x0030U);
        store_zero_page(0xD678U, 0xD67AU, 0x0032U);
        replacement.return_instruction.logical_pc = 0xD67AU;
    } else {
        load_zero_page(0xD67BU, 0xD67DU, 0x002FU);
        checkpoint.accumulator =
            static_cast<std::uint8_t>(checkpoint.accumulator ^ 0xFFU);
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        cycle += 6U;
        append_checkpoint(
            0x01U,
            0xD67DU,
            0xD67FU,
            cycle,
            checkpoint);
        store_zero_page(0xD67FU, 0xD681U, 0x0031U);
        load_zero_page(0xD681U, 0xD683U, 0x0030U);
        checkpoint.accumulator =
            static_cast<std::uint8_t>(checkpoint.accumulator ^ 0xFFU);
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        cycle += 6U;
        append_checkpoint(
            0x01U,
            0xD683U,
            0xD685U,
            cycle,
            checkpoint);
        store_zero_page(0xD685U, 0xD687U, 0x0032U);

        auto low = static_cast<std::uint8_t>(
            std::to_integer<std::uint8_t>(staged[0x0031U]) + 1U);
        checkpoint.status = set_nz(checkpoint.status, low);
        cycle += 18U;
        append_write(0x01U, 0xD687U, 0x2031U, low, cycle);
        append_checkpoint(
            0x01U,
            0xD687U,
            0xD689U,
            cycle,
            checkpoint);
        cycle += projection.increments_high != 0U ? 6U : 12U;
        append_checkpoint(
            0x01U,
            0xD689U,
            projection.increments_high != 0U
                ? 0xD68BU
                : 0xD68DU,
            cycle,
            checkpoint);
        if (projection.increments_high != 0U) {
            auto high = static_cast<std::uint8_t>(
                std::to_integer<std::uint8_t>(staged[0x0032U]) + 1U);
            checkpoint.status = set_nz(checkpoint.status, high);
            cycle += 18U;
            append_write(0x01U, 0xD68BU, 0x2032U, high, cycle);
            append_checkpoint(
                0x01U,
                0xD68BU,
                0xD68DU,
                cycle,
                checkpoint);
        }
    }

    const auto staged_output = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(staged[0x0031U])
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(staged[0x0032U]))
           << 8U));
    if (staged_output != projection.output
        || std::to_integer<std::uint8_t>(staged[0x0033U])
            != projection.angle_difference
        || checkpoint.stack_pointer
            != entry.registers.stack_pointer) {
        throw std::runtime_error(
            "Native D62E replacement composition diverged from its "
            "state model.");
    }
    if (replacement.work_ram_writes.size()
            > kMaximumReplacementWrites
        || replacement.checkpoints.size()
            > kMaximumReplacementCheckpoints) {
        return std::nullopt;
    }
    checkpoint.program_counter =
        replacement.return_instruction.logical_pc;
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = cycle;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_collision_motion_adjustment(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xCFD8U,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_collision_motion_adjustment adjustment{};
    if (!dc_adjust_collision_motion(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &adjustment)) {
        throw std::runtime_error(
            "Native CFD8 replacement rejected the work-RAM snapshot.");
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xD004U,
        },
        .output_registers = entry.registers,
    };
    replacement.work_ram_writes.reserve(16U);
    replacement.checkpoints.reserve(64U);
    std::vector<std::byte> staged(work_ram.begin(), work_ram.end());

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
    const auto compare_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right) {
            const auto result =
                static_cast<std::uint8_t>(left - right);
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0x01U));
            status = set_nz(status, result);
            if (left >= right) {
                status = static_cast<std::uint8_t>(status | 0x01U);
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
            const auto difference = static_cast<std::int16_t>(
                static_cast<std::int16_t>(left)
                - static_cast<std::int16_t>(right)
                - static_cast<std::int16_t>(carry_in ? 0 : 1));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC1U));
            status = set_nz(status, result);
            if (difference >= 0) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if ((((left ^ right) & (left ^ result)) & 0x80U) != 0U) {
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
                .instruction = {0x01U, instruction_pc},
                .registers = registers,
            });
        };
    const auto append_write =
        [&replacement, &staged](
            const std::uint16_t instruction_pc,
            const std::uint16_t offset,
            const std::uint8_t value,
            const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x01U, instruction_pc},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U + offset),
                .value = value,
                .master_cycle_offset = master_cycle,
            });
            staged[offset] = static_cast<std::byte>(value);
        };

    auto checkpoint = entry.registers;
    std::uint32_t cycle = 0U;
    const auto load_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            checkpoint.accumulator =
                std::to_integer<std::uint8_t>(staged[offset]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto load_absolute =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 15U;
            checkpoint.accumulator =
                std::to_integer<std::uint8_t>(staged[offset]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto store_zero_page =
        [&checkpoint,
         &cycle,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            append_write(
                instruction_pc,
                offset,
                checkpoint.accumulator,
                cycle);
            append_checkpoint(
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto compare_immediate =
        [&checkpoint,
         &cycle,
         &compare_status,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint8_t value) {
            cycle += 6U;
            checkpoint.status = compare_status(
                checkpoint.status,
                checkpoint.accumulator,
                value);
            append_checkpoint(
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto rotate_word =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_write,
         &append_checkpoint](
            const std::uint16_t compare_pc,
            const std::uint16_t high_pc,
            const std::uint16_t low_pc,
            const std::uint16_t resume_pc) {
            cycle += 6U;
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status
                & static_cast<std::uint8_t>(~0x21U));
            if (checkpoint.accumulator >= 0x80U) {
                checkpoint.status =
                    static_cast<std::uint8_t>(
                        checkpoint.status | 0x01U);
            }
            checkpoint.status = set_nz(
                checkpoint.status,
                static_cast<std::uint8_t>(
                    checkpoint.accumulator - 0x80U));
            append_checkpoint(
                compare_pc,
                high_pc,
                cycle,
                checkpoint);

            const auto old_high =
                std::to_integer<std::uint8_t>(staged[0x00BFU]);
            const bool high_carry =
                (checkpoint.status & 0x01U) != 0U;
            const bool low_bit = (old_high & 0x01U) != 0U;
            const auto high = static_cast<std::uint8_t>(
                (old_high >> 1U) | (high_carry ? 0x80U : 0U));
            checkpoint.status = set_nz(checkpoint.status, high);
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status & static_cast<std::uint8_t>(~0x01U));
            if (low_bit) {
                checkpoint.status =
                    static_cast<std::uint8_t>(
                        checkpoint.status | 0x01U);
            }
            cycle += 18U;
            append_write(high_pc, 0x00BFU, high, cycle);
            append_checkpoint(
                high_pc,
                low_pc,
                cycle,
                checkpoint);

            const auto old_low =
                std::to_integer<std::uint8_t>(staged[0x00BEU]);
            const bool word_carry =
                (checkpoint.status & 0x01U) != 0U;
            const bool next_carry = (old_low & 0x01U) != 0U;
            const auto low = static_cast<std::uint8_t>(
                (old_low >> 1U) | (word_carry ? 0x80U : 0U));
            checkpoint.status = set_nz(checkpoint.status, low);
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status & static_cast<std::uint8_t>(~0x01U));
            if (next_carry) {
                checkpoint.status =
                    static_cast<std::uint8_t>(
                        checkpoint.status | 0x01U);
            }
            cycle += 18U;
            append_write(low_pc, 0x00BEU, low, cycle);
            append_checkpoint(
                low_pc,
                resume_pc,
                cycle,
                checkpoint);
        };

    load_zero_page(0xCFD8U, 0xCFDAU, 0x0078U);
    checkpoint.accumulator =
        static_cast<std::uint8_t>(checkpoint.accumulator & 0x60U);
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    cycle += 6U;
    append_checkpoint(0xCFDAU, 0xCFDCU, cycle, checkpoint);
    compare_immediate(0xCFDCU, 0xCFDEU, 0x20U);

    if (adjustment.effective_mode == 0x20U
        && adjustment.mode == 0x20U) {
        cycle += 12U;
        append_checkpoint(0xCFDEU, 0xD017U, cycle, checkpoint);
    } else {
        cycle += 6U;
        append_checkpoint(0xCFDEU, 0xCFE0U, cycle, checkpoint);
        compare_immediate(0xCFE0U, 0xCFE2U, 0x40U);
        if (adjustment.effective_mode == 0x40U) {
            cycle += 12U;
            append_checkpoint(0xCFE2U, 0xD05DU, cycle, checkpoint);
        } else {
            cycle += 6U;
            append_checkpoint(0xCFE2U, 0xCFE4U, cycle, checkpoint);
            compare_immediate(0xCFE4U, 0xCFE6U, 0x60U);
            if (adjustment.effective_mode == 0x60U) {
                cycle += 12U;
                append_checkpoint(0xCFE6U, 0xD066U, cycle, checkpoint);
            } else {
                cycle += 6U;
                append_checkpoint(0xCFE6U, 0xCFE8U, cycle, checkpoint);
                load_absolute(0xCFE8U, 0xCFEBU, 0x0CC7U);
                cycle += adjustment.effective_mode == 0x20U
                    ? 12U
                    : 6U;
                append_checkpoint(
                    0xCFEBU,
                    adjustment.effective_mode == 0x20U
                        ? 0xD017U
                        : 0xCFEDU,
                    cycle,
                    checkpoint);
            }
        }
    }

    if (adjustment.effective_mode == 0U) {
        load_zero_page(0xCFEDU, 0xCFEFU, 0x00BFU);
        cycle += adjustment.negative_input != 0U ? 12U : 6U;
        append_checkpoint(
            0xCFEFU,
            adjustment.negative_input != 0U
                ? 0xD005U
                : 0xCFF1U,
            cycle,
            checkpoint);
        if (adjustment.negative_input == 0U) {
            cycle += 6U;
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status
                & static_cast<std::uint8_t>(~0x21U));
            append_checkpoint(0xCFF1U, 0xCFF2U, cycle, checkpoint);
            load_zero_page(0xCFF2U, 0xCFF4U, 0x00BEU);
            {
                const auto left = checkpoint.accumulator;
                const auto right =
                    std::to_integer<std::uint8_t>(staged[0x0081U]);
                const bool carry =
                    (checkpoint.status & 0x01U) != 0U;
                checkpoint.accumulator = static_cast<std::uint8_t>(
                    left + right + (carry ? 1U : 0U));
                checkpoint.status = adc_status(
                    checkpoint.status,
                    left,
                    right,
                    carry,
                    checkpoint.accumulator);
                cycle += 12U;
                append_checkpoint(0xCFF4U, 0xCFF6U, cycle, checkpoint);
            }
            store_zero_page(0xCFF6U, 0xCFF8U, 0x00BEU);
            load_zero_page(0xCFF8U, 0xCFFAU, 0x00BFU);
            {
                const auto left = checkpoint.accumulator;
                const auto right =
                    std::to_integer<std::uint8_t>(staged[0x0082U]);
                const bool carry =
                    (checkpoint.status & 0x01U) != 0U;
                checkpoint.accumulator = static_cast<std::uint8_t>(
                    left + right + (carry ? 1U : 0U));
                checkpoint.status = adc_status(
                    checkpoint.status,
                    left,
                    right,
                    carry,
                    checkpoint.accumulator);
                cycle += 12U;
                append_checkpoint(0xCFFAU, 0xCFFCU, cycle, checkpoint);
            }
            compare_immediate(0xCFFCU, 0xCFFEU, 0x07U);
            if (adjustment.clamps_high != 0U) {
                cycle += 6U;
                append_checkpoint(0xCFFEU, 0xD000U, cycle, checkpoint);
                checkpoint.accumulator = 0x06U;
                checkpoint.status =
                    set_nz(checkpoint.status, checkpoint.accumulator);
                cycle += 6U;
                append_checkpoint(0xD000U, 0xD002U, cycle, checkpoint);
            } else {
                cycle += 12U;
                append_checkpoint(0xCFFEU, 0xD002U, cycle, checkpoint);
            }
            store_zero_page(0xD002U, 0xD004U, 0x00BFU);
            replacement.return_instruction.logical_pc = 0xD004U;
        } else {
            cycle += 6U;
            checkpoint.status = static_cast<std::uint8_t>(
                (checkpoint.status
                 & static_cast<std::uint8_t>(~0x20U))
                | 0x01U);
            append_checkpoint(0xD005U, 0xD006U, cycle, checkpoint);
            load_zero_page(0xD006U, 0xD008U, 0x00BEU);
            {
                const auto left = checkpoint.accumulator;
                const auto right =
                    std::to_integer<std::uint8_t>(staged[0x0081U]);
                checkpoint.accumulator =
                    static_cast<std::uint8_t>(left - right);
                checkpoint.status = sbc_status(
                    checkpoint.status,
                    left,
                    right,
                    true,
                    checkpoint.accumulator);
                cycle += 12U;
                append_checkpoint(0xD008U, 0xD00AU, cycle, checkpoint);
            }
            store_zero_page(0xD00AU, 0xD00CU, 0x00BEU);
            load_zero_page(0xD00CU, 0xD00EU, 0x00BFU);
            compare_immediate(0xD00EU, 0xD010U, 0xFAU);
            if (adjustment.clamps_high != 0U) {
                cycle += 6U;
                append_checkpoint(0xD010U, 0xD012U, cycle, checkpoint);
                checkpoint.accumulator = 0xFAU;
                checkpoint.status =
                    set_nz(checkpoint.status, checkpoint.accumulator);
                cycle += 6U;
                append_checkpoint(0xD012U, 0xD014U, cycle, checkpoint);
            } else {
                cycle += 12U;
                append_checkpoint(0xD010U, 0xD014U, cycle, checkpoint);
            }
            store_zero_page(0xD014U, 0xD016U, 0x00BFU);
            replacement.return_instruction.logical_pc = 0xD016U;
        }
    } else if (adjustment.effective_mode == 0x20U) {
        load_absolute(0xD017U, 0xD01AU, 0x022CU);
        cycle += adjustment.uses_quarter_scale != 0U ? 12U : 6U;
        append_checkpoint(
            0xD01AU,
            adjustment.uses_quarter_scale != 0U
                ? 0xD04EU
                : 0xD01CU,
            cycle,
            checkpoint);
        if (adjustment.uses_quarter_scale == 0U) {
            load_zero_page(0xD01CU, 0xD01EU, 0x00BFU);
            rotate_word(0xD01EU, 0xD020U, 0xD022U, 0xD024U);
            rotate_word(0xD024U, 0xD026U, 0xD028U, 0xD02AU);
            rotate_word(0xD02AU, 0xD02CU, 0xD02EU, 0xD030U);
            load_zero_page(0xD030U, 0xD032U, 0x00BEU);
            store_zero_page(0xD032U, 0xD034U, 0x002DU);
            load_zero_page(0xD034U, 0xD036U, 0x00BFU);
            store_zero_page(0xD036U, 0xD038U, 0x002EU);
            load_zero_page(0xD038U, 0xD03AU, 0x00BFU);
            rotate_word(0xD03AU, 0xD03CU, 0xD03EU, 0xD040U);
            cycle += 6U;
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status
                & static_cast<std::uint8_t>(~0x21U));
            append_checkpoint(0xD040U, 0xD041U, cycle, checkpoint);
            load_zero_page(0xD041U, 0xD043U, 0x00BEU);
            {
                const auto left = checkpoint.accumulator;
                const auto right =
                    std::to_integer<std::uint8_t>(staged[0x002DU]);
                checkpoint.accumulator =
                    static_cast<std::uint8_t>(left + right);
                checkpoint.status = adc_status(
                    checkpoint.status,
                    left,
                    right,
                    false,
                    checkpoint.accumulator);
                cycle += 12U;
                append_checkpoint(0xD043U, 0xD045U, cycle, checkpoint);
            }
            store_zero_page(0xD045U, 0xD047U, 0x00BEU);
            load_zero_page(0xD047U, 0xD049U, 0x00BFU);
            {
                const auto left = checkpoint.accumulator;
                const auto right =
                    std::to_integer<std::uint8_t>(staged[0x002EU]);
                const bool carry =
                    (checkpoint.status & 0x01U) != 0U;
                checkpoint.accumulator = static_cast<std::uint8_t>(
                    left + right + (carry ? 1U : 0U));
                checkpoint.status = adc_status(
                    checkpoint.status,
                    left,
                    right,
                    carry,
                    checkpoint.accumulator);
                cycle += 12U;
                append_checkpoint(0xD049U, 0xD04BU, cycle, checkpoint);
            }
            store_zero_page(0xD04BU, 0xD04DU, 0x00BFU);
            replacement.return_instruction.logical_pc = 0xD04DU;
        } else {
            load_zero_page(0xD04EU, 0xD050U, 0x00BFU);
            rotate_word(0xD050U, 0xD052U, 0xD054U, 0xD056U);
            rotate_word(0xD056U, 0xD058U, 0xD05AU, 0xD05CU);
            replacement.return_instruction.logical_pc = 0xD05CU;
        }
    } else if (adjustment.effective_mode == 0x40U) {
        load_zero_page(0xD05DU, 0xD05FU, 0x00BFU);
        rotate_word(0xD05FU, 0xD061U, 0xD063U, 0xD065U);
        replacement.return_instruction.logical_pc = 0xD065U;
    } else {
        load_zero_page(0xD066U, 0xD068U, 0x00BFU);
        rotate_word(0xD068U, 0xD06AU, 0xD06CU, 0xD06EU);
        rotate_word(0xD06EU, 0xD070U, 0xD072U, 0xD074U);
        rotate_word(0xD074U, 0xD076U, 0xD078U, 0xD07AU);
        rotate_word(0xD07AU, 0xD07CU, 0xD07EU, 0xD080U);
        replacement.return_instruction.logical_pc = 0xD080U;
    }

    const auto staged_output = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(staged[0x00BEU])
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(staged[0x00BFU]))
           << 8U));
    if (staged_output != adjustment.output) {
        throw std::runtime_error(
            "Native CFD8 replacement composition diverged from its "
            "state model.");
    }
    checkpoint.program_counter =
        replacement.return_instruction.logical_pc;
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = cycle;

    // All six arms end on an `RTS` -- the routine is a mode switch whose every
    // branch runs to its own return -- and none of them touches the stack, so
    // one call is absorbed whichever arm ran. The address is copied out first
    // because absorbing overwrites the field it came from.
    const auto return_instruction = replacement.return_instruction;
    absorb_return(
        replacement,
        work_ram,
        entry.registers.stack_pointer,
        return_instruction,
        cycle,
        checkpoint);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_signed_division(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x00U,
        .logical_pc = 0xE905U,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_signed_division division{};
    if (!dc_divide_signed_word(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            static_cast<std::uint8_t>(
                entry.registers.status & 0x01U),
            &division)) {
        throw std::runtime_error(
            "Native E905 replacement rejected the work-RAM snapshot.");
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x00U,
            .logical_pc = 0xE937U,
        },
        .output_registers = entry.registers,
    };
    replacement.work_ram_writes.reserve(64U);
    replacement.checkpoints.reserve(192U);
    std::vector<std::byte> staged(work_ram.begin(), work_ram.end());

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
    const auto compare_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right) {
            const auto result =
                static_cast<std::uint8_t>(left - right);
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0x01U));
            status = set_nz(status, result);
            if (left >= right) {
                status = static_cast<std::uint8_t>(
                    status | 0x01U);
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
            const auto difference = static_cast<std::int16_t>(
                static_cast<std::int16_t>(left)
                - static_cast<std::int16_t>(right)
                - static_cast<std::int16_t>(carry_in ? 0 : 1));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC1U));
            status = set_nz(status, result);
            if (difference >= 0) {
                status = static_cast<std::uint8_t>(
                    status | 0x01U);
            }
            if ((((left ^ right) & (left ^ result)) & 0x80U)
                != 0U) {
                status = static_cast<std::uint8_t>(
                    status | 0x40U);
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
    const auto append_write =
        [&replacement, &staged](
            const std::uint16_t instruction_pc,
            const std::uint16_t logical_address,
            const std::uint8_t value,
            const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x00U, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
            if (logical_address >= 0x2000U
                && logical_address < 0x4000U) {
                staged[logical_address - 0x2000U] =
                    static_cast<std::byte>(value);
            }
        };

    auto checkpoint = entry.registers;
    std::uint32_t cycle = 0U;
    const auto load_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            checkpoint.accumulator =
                std::to_integer<std::uint8_t>(staged[offset]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto store_zero_page =
        [&checkpoint,
         &cycle,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            append_write(
                instruction_pc,
                static_cast<std::uint16_t>(0x2000U + offset),
                checkpoint.accumulator,
                cycle);
            append_checkpoint(
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto subtract_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &sbc_status,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            const auto left = checkpoint.accumulator;
            const auto right =
                std::to_integer<std::uint8_t>(staged[offset]);
            const bool carry =
                (checkpoint.status & 0x01U) != 0U;
            checkpoint.accumulator = static_cast<std::uint8_t>(
                static_cast<std::uint16_t>(left)
                - static_cast<std::uint16_t>(right)
                - static_cast<std::uint16_t>(carry ? 0U : 1U));
            checkpoint.status = sbc_status(
                checkpoint.status,
                left,
                right,
                carry,
                checkpoint.accumulator);
            cycle += 12U;
            append_checkpoint(
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto rotate_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            const auto original =
                std::to_integer<std::uint8_t>(staged[offset]);
            const bool carry_in =
                (checkpoint.status & 0x01U) != 0U;
            const auto result = static_cast<std::uint8_t>(
                static_cast<std::uint8_t>(original << 1U)
                | static_cast<std::uint8_t>(carry_in));
            checkpoint.status =
                set_nz(checkpoint.status, result);
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status
                & static_cast<std::uint8_t>(~0x01U));
            if ((original & 0x80U) != 0U) {
                checkpoint.status = static_cast<std::uint8_t>(
                    checkpoint.status | 0x01U);
            }
            cycle += 18U;
            append_write(
                instruction_pc,
                static_cast<std::uint16_t>(0x2000U + offset),
                result,
                cycle);
            append_checkpoint(
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };

    load_zero_page(0xE905U, 0xE907U, 0x002DU);
    store_zero_page(0xE907U, 0xE909U, 0x0046U);
    cycle += division.divisor_negative != 0U ? 6U : 12U;
    append_checkpoint(
        0xE909U,
        division.divisor_negative != 0U ? 0xE90BU : 0xE910U,
        cycle,
        checkpoint);
    if (division.divisor_negative != 0U) {
        checkpoint.accumulator = static_cast<std::uint8_t>(
            checkpoint.accumulator ^ 0xFFU);
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        cycle += 6U;
        append_checkpoint(0xE90BU, 0xE90DU, cycle, checkpoint);
        checkpoint.accumulator = static_cast<std::uint8_t>(
            checkpoint.accumulator + 1U);
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        cycle += 6U;
        append_checkpoint(0xE90DU, 0xE90EU, cycle, checkpoint);
        store_zero_page(0xE90EU, 0xE910U, 0x002DU);
    }

    load_zero_page(0xE910U, 0xE912U, 0x0030U);
    checkpoint.accumulator = static_cast<std::uint8_t>(
        checkpoint.accumulator
        ^ std::to_integer<std::uint8_t>(staged[0x0046U]));
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    cycle += 12U;
    append_checkpoint(0xE912U, 0xE914U, cycle, checkpoint);
    store_zero_page(0xE914U, 0xE916U, 0x0046U);
    load_zero_page(0xE916U, 0xE918U, 0x0030U);
    cycle += division.dividend_negative != 0U ? 6U : 12U;
    append_checkpoint(
        0xE918U,
        division.dividend_negative != 0U ? 0xE91AU : 0xE925U,
        cycle,
        checkpoint);
    if (division.dividend_negative != 0U) {
        cycle += 6U;
        checkpoint.status = static_cast<std::uint8_t>(
            (checkpoint.status
             & static_cast<std::uint8_t>(~0x20U))
            | 0x01U);
        append_checkpoint(0xE91AU, 0xE91BU, cycle, checkpoint);
        cycle += 6U;
        checkpoint.accumulator = 0U;
        append_checkpoint(0xE91BU, 0xE91CU, cycle, checkpoint);
        subtract_zero_page(0xE91CU, 0xE91EU, 0x002FU);
        store_zero_page(0xE91EU, 0xE920U, 0x002FU);
        cycle += 6U;
        checkpoint.accumulator = 0U;
        append_checkpoint(0xE920U, 0xE921U, cycle, checkpoint);
        subtract_zero_page(0xE921U, 0xE923U, 0x0030U);
        store_zero_page(0xE923U, 0xE925U, 0x0030U);
    }

    append_write(
        0xE925U,
        static_cast<std::uint16_t>(
            0x2100U + checkpoint.stack_pointer),
        0xE9U,
        cycle);
    append_write(
        0xE925U,
        static_cast<std::uint16_t>(
            0x2100U
            + static_cast<std::uint8_t>(
                checkpoint.stack_pointer - 1U)),
        0x27U,
        cycle);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 2U);
    cycle += 21U;
    append_checkpoint(0xE925U, 0xE8E7U, cycle, checkpoint);

    cycle += 9U;
    append_write(
        0xE8E7U,
        static_cast<std::uint16_t>(
            0x2100U + checkpoint.stack_pointer),
        checkpoint.x,
        cycle);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 1U);
    append_checkpoint(0xE8E7U, 0xE8E8U, cycle, checkpoint);

    cycle += 12U;
    append_write(0xE8E8U, 0x202EU, 0U, cycle);
    append_checkpoint(0xE8E8U, 0xE8EAU, cycle, checkpoint);

    checkpoint.x = 0x10U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.x);
    cycle += 6U;
    append_checkpoint(0xE8EAU, 0xE8ECU, cycle, checkpoint);
    rotate_zero_page(0xE8ECU, 0xE8EEU, 0x002FU);
    rotate_zero_page(0xE8EEU, 0xE8F0U, 0x0030U);

    for (std::size_t index = 0U;
         index < DC_UNSIGNED_DIVIDE_STEP_COUNT;
         ++index) {
        const auto& step = division.steps[index];
        rotate_zero_page(0xE8F0U, 0xE8F2U, 0x002EU);
        load_zero_page(0xE8F2U, 0xE8F4U, 0x002EU);

        checkpoint.status = compare_status(
            checkpoint.status,
            checkpoint.accumulator,
            std::to_integer<std::uint8_t>(staged[0x002DU]));
        cycle += 12U;
        append_checkpoint(0xE8F4U, 0xE8F6U, cycle, checkpoint);

        cycle += step.subtracts_divisor != 0U ? 6U : 12U;
        append_checkpoint(
            0xE8F6U,
            step.subtracts_divisor != 0U ? 0xE8F8U : 0xE8FCU,
            cycle,
            checkpoint);
        if (step.subtracts_divisor != 0U) {
            subtract_zero_page(0xE8F8U, 0xE8FAU, 0x002DU);
            store_zero_page(0xE8FAU, 0xE8FCU, 0x002EU);
        }
        rotate_zero_page(0xE8FCU, 0xE8FEU, 0x002FU);
        rotate_zero_page(0xE8FEU, 0xE900U, 0x0030U);

        checkpoint.x = static_cast<std::uint8_t>(
            checkpoint.x - 1U);
        checkpoint.status = set_nz(checkpoint.status, checkpoint.x);
        cycle += 6U;
        append_checkpoint(0xE900U, 0xE901U, cycle, checkpoint);
        const bool repeats =
            index + 1U < DC_UNSIGNED_DIVIDE_STEP_COUNT;
        cycle += repeats ? 12U : 6U;
        append_checkpoint(
            0xE901U,
            repeats ? 0xE8F0U : 0xE903U,
            cycle,
            checkpoint);
    }

    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 1U);
    checkpoint.x = std::to_integer<std::uint8_t>(
        staged[0x0100U + checkpoint.stack_pointer]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.x);
    cycle += 12U;
    append_checkpoint(0xE903U, 0xE904U, cycle, checkpoint);

    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 2U);
    cycle += 21U;
    append_checkpoint(0xE904U, 0xE928U, cycle, checkpoint);

    load_zero_page(0xE928U, 0xE92AU, 0x0046U);
    cycle += division.result_negative != 0U ? 6U : 12U;
    append_checkpoint(
        0xE92AU,
        division.result_negative != 0U ? 0xE92CU : 0xE937U,
        cycle,
        checkpoint);
    if (division.result_negative != 0U) {
        cycle += 6U;
        checkpoint.status = static_cast<std::uint8_t>(
            (checkpoint.status
             & static_cast<std::uint8_t>(~0x20U))
            | 0x01U);
        append_checkpoint(0xE92CU, 0xE92DU, cycle, checkpoint);
        cycle += 6U;
        checkpoint.accumulator = 0U;
        append_checkpoint(0xE92DU, 0xE92EU, cycle, checkpoint);
        subtract_zero_page(0xE92EU, 0xE930U, 0x002FU);
        store_zero_page(0xE930U, 0xE932U, 0x002FU);
        cycle += 6U;
        checkpoint.accumulator = 0U;
        append_checkpoint(0xE932U, 0xE933U, cycle, checkpoint);
        subtract_zero_page(0xE933U, 0xE935U, 0x0030U);
        store_zero_page(0xE935U, 0xE937U, 0x0030U);
    }

    const auto staged_result = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(staged[0x002FU])
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(staged[0x0030U]))
           << 8U));
    if (std::to_integer<std::uint8_t>(staged[0x002DU])
            != division.divisor_magnitude
        || std::to_integer<std::uint8_t>(staged[0x002EU])
            != division.remainder
        || staged_result != division.result
        || checkpoint.stack_pointer
            != entry.registers.stack_pointer) {
        throw std::runtime_error(
            "Native E905 replacement composition diverged from its "
            "state model.");
    }

    checkpoint.program_counter = 0xE937U;
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = cycle;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_motion_damping(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr std::size_t kMaximumReplacementWrites = 512U;
    constexpr std::size_t kMaximumReplacementCheckpoints = 2048U;
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xDD09U,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || entry.registers.x >= 0x80U
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_motion_damping damping{};
    if (!dc_damp_and_accelerate_motion(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &damping)) {
        throw std::runtime_error(
            "Native DD09 replacement rejected the work-RAM snapshot.");
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xDD6DU,
        },
        .output_registers = entry.registers,
    };
    replacement.work_ram_writes.reserve(160U);
    replacement.checkpoints.reserve(448U);
    std::vector<std::byte> staged(work_ram.begin(), work_ram.end());

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
                status = static_cast<std::uint8_t>(
                    status | 0x01U);
            }
            if (((~(left ^ right) & (left ^ result)) & 0x80U)
                != 0U) {
                status = static_cast<std::uint8_t>(
                    status | 0x40U);
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
            const auto difference = static_cast<std::int16_t>(
                static_cast<std::int16_t>(left)
                - static_cast<std::int16_t>(right)
                - static_cast<std::int16_t>(carry_in ? 0 : 1));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC1U));
            status = set_nz(status, result);
            if (difference >= 0) {
                status = static_cast<std::uint8_t>(
                    status | 0x01U);
            }
            if ((((left ^ right) & (left ^ result)) & 0x80U)
                != 0U) {
                status = static_cast<std::uint8_t>(
                    status | 0x40U);
            }
            return status;
        };
    const auto append_checkpoint =
        [&replacement](
            const std::uint8_t instruction_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {
                    instruction_bank,
                    instruction_pc,
                },
                .registers = registers,
            });
        };
    const auto append_write =
        [&replacement, &staged](
            const std::uint8_t instruction_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t logical_address,
            const std::uint8_t value,
            const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {
                    instruction_bank,
                    instruction_pc,
                },
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
            if (logical_address >= 0x2000U
                && logical_address < 0x4000U) {
                staged[logical_address - 0x2000U] =
                    static_cast<std::byte>(value);
            }
        };

    auto checkpoint = entry.registers;
    std::uint32_t cycle = 0U;
    const auto load_indexed =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t logical_base) {
            cycle += 15U;
            checkpoint.accumulator = std::to_integer<std::uint8_t>(
                staged[logical_base - 0x2000U + checkpoint.x]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto store_indexed =
        [&checkpoint,
         &cycle,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t logical_base) {
            cycle += 15U;
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(
                    logical_base + checkpoint.x),
                checkpoint.accumulator,
                cycle);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto load_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            checkpoint.accumulator =
                std::to_integer<std::uint8_t>(staged[offset]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto store_zero_page =
        [&checkpoint,
         &cycle,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(0x2000U + offset),
                checkpoint.accumulator,
                cycle);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto or_indexed =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t logical_base) {
            checkpoint.accumulator = static_cast<std::uint8_t>(
                checkpoint.accumulator
                | std::to_integer<std::uint8_t>(
                    staged[
                        logical_base - 0x2000U + checkpoint.x]));
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            cycle += 15U;
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto subtract_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &sbc_status,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            const auto left = checkpoint.accumulator;
            const auto right =
                std::to_integer<std::uint8_t>(staged[offset]);
            const bool carry =
                (checkpoint.status & 0x01U) != 0U;
            checkpoint.accumulator = static_cast<std::uint8_t>(
                static_cast<std::uint16_t>(left)
                - static_cast<std::uint16_t>(right)
                - static_cast<std::uint16_t>(carry ? 0U : 1U));
            checkpoint.status = sbc_status(
                checkpoint.status,
                left,
                right,
                carry,
                checkpoint.accumulator);
            cycle += 12U;
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto add_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &adc_status,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            const auto left = checkpoint.accumulator;
            const auto right =
                std::to_integer<std::uint8_t>(staged[offset]);
            const bool carry =
                (checkpoint.status & 0x01U) != 0U;
            checkpoint.accumulator = static_cast<std::uint8_t>(
                static_cast<std::uint16_t>(left)
                + static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry));
            checkpoint.status = adc_status(
                checkpoint.status,
                left,
                right,
                carry,
                checkpoint.accumulator);
            cycle += 12U;
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto add_indexed =
        [&checkpoint,
         &cycle,
         &staged,
         &adc_status,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t logical_base) {
            const auto left = checkpoint.accumulator;
            const auto right = std::to_integer<std::uint8_t>(
                staged[logical_base - 0x2000U + checkpoint.x]);
            const bool carry =
                (checkpoint.status & 0x01U) != 0U;
            checkpoint.accumulator = static_cast<std::uint8_t>(
                static_cast<std::uint16_t>(left)
                + static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry));
            checkpoint.status = adc_status(
                checkpoint.status,
                left,
                right,
                carry,
                checkpoint.accumulator);
            cycle += 15U;
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto begin_jsr =
        [&checkpoint,
         &cycle,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t pushed_return_pc) {
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(
                    0x2100U + checkpoint.stack_pointer),
                static_cast<std::uint8_t>(
                    pushed_return_pc >> 8U),
                cycle);
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(
                    0x2100U
                    + static_cast<std::uint8_t>(
                        checkpoint.stack_pointer - 1U)),
                static_cast<std::uint8_t>(pushed_return_pc),
                cycle);
            checkpoint.stack_pointer = static_cast<std::uint8_t>(
                checkpoint.stack_pointer - 2U);
            cycle += 21U;
            append_checkpoint(
                0x01U,
                instruction_pc,
                0xE905U,
                cycle,
                checkpoint);
        };
    const auto append_child =
        [&replacement,
         &staged,
         &checkpoint,
         &cycle,
         &append_checkpoint](
            const RoutineReplacement& child,
            const std::uint16_t resume_pc) {
            const auto child_offset = cycle;
            for (auto write : child.work_ram_writes) {
                write.master_cycle_offset += child_offset;
                replacement.work_ram_writes.push_back(write);
                staged[write.logical_address - 0x2000U] =
                    static_cast<std::byte>(write.value);
            }
            for (auto child_checkpoint : child.checkpoints) {
                child_checkpoint.interrupt_sample_master_cycle_offset +=
                    child_offset;
                child_checkpoint.master_cycle_offset += child_offset;
                replacement.checkpoints.push_back(child_checkpoint);
            }
            cycle += child.body_master_cycles;
            checkpoint = child.output_registers;
            checkpoint.stack_pointer = static_cast<std::uint8_t>(
                checkpoint.stack_pointer + 2U);
            cycle += 21U;
            append_checkpoint(
                child.return_instruction.physical_bank,
                child.return_instruction.logical_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto run_signed_division =
        [&checkpoint,
         &staged,
         &begin_jsr,
         &append_child](
            const std::uint16_t instruction_pc,
            const std::uint16_t pushed_return_pc,
            const std::uint16_t resume_pc) {
            begin_jsr(instruction_pc, pushed_return_pc);
            RoutineEntry child_entry{
                .routine = {
                    .physical_bank = 0x00U,
                    .logical_pc = 0xE905U,
                },
                .registers = checkpoint,
            };
            child_entry.registers.program_counter = 0xE905U;
            auto child = replace_signed_division(
                child_entry,
                staged);
            if (!child || child->checkpoints.empty()) {
                return false;
            }
            append_child(*child, resume_pc);
            return true;
        };

    load_indexed(0xDD09U, 0xDD0CU, 0x3400U);
    or_indexed(0xDD0CU, 0xDD0FU, 0x3480U);
    cycle += damping.first_component_nonzero != 0U ? 6U : 12U;
    append_checkpoint(
        0x01U,
        0xDD0FU,
        damping.first_component_nonzero != 0U
            ? 0xDD11U
            : 0xDD33U,
        cycle,
        checkpoint);
    if (damping.first_component_nonzero != 0U) {
        load_indexed(0xDD11U, 0xDD14U, 0x3400U);
        store_zero_page(0xDD14U, 0xDD16U, 0x002FU);
        load_indexed(0xDD16U, 0xDD19U, 0x3480U);
        store_zero_page(0xDD19U, 0xDD1BU, 0x0030U);
        load_zero_page(0xDD1BU, 0xDD1DU, 0x008CU);
        store_zero_page(0xDD1DU, 0xDD1FU, 0x002DU);
        if (!run_signed_division(
                0xDD1FU,
                0xDD21U,
                0xDD22U)) {
            return std::nullopt;
        }
        cycle += 6U;
        checkpoint.status = static_cast<std::uint8_t>(
            (checkpoint.status
             & static_cast<std::uint8_t>(~0x20U))
            | 0x01U);
        append_checkpoint(
            0x01U,
            0xDD22U,
            0xDD23U,
            cycle,
            checkpoint);
        load_indexed(0xDD23U, 0xDD26U, 0x3400U);
        subtract_zero_page(0xDD26U, 0xDD28U, 0x002FU);
        store_indexed(0xDD28U, 0xDD2BU, 0x3400U);
        load_indexed(0xDD2BU, 0xDD2EU, 0x3480U);
        subtract_zero_page(0xDD2EU, 0xDD30U, 0x0030U);
        store_indexed(0xDD30U, 0xDD33U, 0x3480U);
    }

    load_indexed(0xDD33U, 0xDD36U, 0x3500U);
    or_indexed(0xDD36U, 0xDD39U, 0x3580U);
    cycle += damping.second_component_nonzero != 0U ? 6U : 12U;
    append_checkpoint(
        0x01U,
        0xDD39U,
        damping.second_component_nonzero != 0U
            ? 0xDD3BU
            : 0xDD5DU,
        cycle,
        checkpoint);
    if (damping.second_component_nonzero != 0U) {
        load_indexed(0xDD3BU, 0xDD3EU, 0x3500U);
        store_zero_page(0xDD3EU, 0xDD40U, 0x002FU);
        load_indexed(0xDD40U, 0xDD43U, 0x3580U);
        store_zero_page(0xDD43U, 0xDD45U, 0x0030U);
        load_zero_page(0xDD45U, 0xDD47U, 0x008DU);
        store_zero_page(0xDD47U, 0xDD49U, 0x002DU);
        if (!run_signed_division(
                0xDD49U,
                0xDD4BU,
                0xDD4CU)) {
            return std::nullopt;
        }
        cycle += 6U;
        checkpoint.status = static_cast<std::uint8_t>(
            (checkpoint.status
             & static_cast<std::uint8_t>(~0x20U))
            | 0x01U);
        append_checkpoint(
            0x01U,
            0xDD4CU,
            0xDD4DU,
            cycle,
            checkpoint);
        load_indexed(0xDD4DU, 0xDD50U, 0x3500U);
        subtract_zero_page(0xDD50U, 0xDD52U, 0x002FU);
        store_indexed(0xDD52U, 0xDD55U, 0x3500U);
        load_indexed(0xDD55U, 0xDD58U, 0x3580U);
        subtract_zero_page(0xDD58U, 0xDD5AU, 0x0030U);
        store_indexed(0xDD5AU, 0xDD5DU, 0x3580U);
    }

    cycle += 6U;
    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status & static_cast<std::uint8_t>(~0x21U));
    append_checkpoint(
        0x01U,
        0xDD5DU,
        0xDD5EU,
        cycle,
        checkpoint);
    load_indexed(0xDD5EU, 0xDD61U, 0x3500U);
    add_zero_page(0xDD61U, 0xDD63U, 0x008BU);
    store_indexed(0xDD63U, 0xDD66U, 0x3500U);
    cycle += 6U;
    checkpoint.accumulator = 0U;
    append_checkpoint(
        0x01U,
        0xDD66U,
        0xDD67U,
        cycle,
        checkpoint);
    add_indexed(0xDD67U, 0xDD6AU, 0x3580U);
    store_indexed(0xDD6AU, 0xDD6DU, 0x3580U);

    const auto first_output = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(
            staged[0x1400U + checkpoint.x])
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(
                   staged[0x1480U + checkpoint.x]))
           << 8U));
    const auto second_output = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(
            staged[0x1500U + checkpoint.x])
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(
                   staged[0x1580U + checkpoint.x]))
           << 8U));
    if (first_output != damping.first_damped
        || second_output != damping.second_accelerated
        || checkpoint.stack_pointer
            != entry.registers.stack_pointer) {
        throw std::runtime_error(
            "Native DD09 replacement composition diverged from its "
            "state model.");
    }
    if (replacement.work_ram_writes.size()
            > kMaximumReplacementWrites
        || replacement.checkpoints.size()
            > kMaximumReplacementCheckpoints) {
        return std::nullopt;
    }

    checkpoint.program_counter = 0xDD6DU;
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = cycle;

    // The pointer is the entry's -- the check above throws rather than returns
    // if the composed children have not balanced their frames -- so the word
    // this return reads sits two bytes clear of the last `JSR` frame the walk
    // staged.
    absorb_return(
        replacement,
        work_ram,
        entry.registers.stack_pointer,
        {0x01U, 0xDD6DU},
        cycle,
        checkpoint);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_motion_substep_split(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xDC95U,
    };
    // The T flag would redirect `EOR #$FF` at a zero-page cell instead of the
    // accumulator, and the routine is reached by `JMP` from seven sites, so
    // decline rather than assume. D is refused on the same terms as 01:DD09.
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || entry.registers.x >= 0x80U
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_motion_substep_split split{};
    if (!dc_split_motion_substeps(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &split)) {
        throw std::runtime_error(
            "Native DC95 replacement rejected the work-RAM snapshot.");
    }

    RoutineReplacement replacement{};
    replacement.work_ram_writes.reserve(16U);
    replacement.checkpoints.reserve(48U);
    std::vector<std::byte> staged(work_ram.begin(), work_ram.end());

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0x82U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };

    auto checkpoint = entry.registers;
    std::uint32_t cycle = 0U;

    const auto append_checkpoint =
        [&replacement, &cycle](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = cycle - 3U,
                .master_cycle_offset = cycle,
                .instruction = {0x01U, instruction_pc},
                .registers = registers,
            });
        };
    const auto append_write =
        [&replacement, &staged, &cycle](
            const std::uint16_t instruction_pc,
            const std::uint16_t offset,
            const std::uint8_t value) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x01U, instruction_pc},
                .logical_address =
                    static_cast<std::uint16_t>(0x2000U + offset),
                .value = value,
                .master_cycle_offset = cycle,
            });
            staged[offset] = static_cast<std::byte>(value);
        };

    const auto load_indexed =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 15U;
            checkpoint.accumulator = std::to_integer<std::uint8_t>(
                staged[offset + checkpoint.x]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(instruction_pc, resume_pc, checkpoint);
        };
    const auto load_zero_page =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            checkpoint.accumulator =
                std::to_integer<std::uint8_t>(staged[offset]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(instruction_pc, resume_pc, checkpoint);
        };
    const auto store_zero_page =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset,
            const std::uint8_t value) {
            cycle += 12U;
            append_write(instruction_pc, offset, value);
            append_checkpoint(instruction_pc, resume_pc, checkpoint);
        };
    const auto load_accumulator_immediate =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint8_t value) {
            cycle += 6U;
            checkpoint.accumulator = value;
            checkpoint.status = set_nz(checkpoint.status, value);
            append_checkpoint(instruction_pc, resume_pc, checkpoint);
        };
    const auto load_index_immediate =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint8_t value) {
            cycle += 6U;
            checkpoint.y = value;
            checkpoint.status = set_nz(checkpoint.status, value);
            append_checkpoint(instruction_pc, resume_pc, checkpoint);
        };
    const auto compare = [&](const std::uint16_t instruction_pc,
                             const std::uint16_t resume_pc,
                             const std::uint8_t right,
                             const std::uint32_t cost) {
        cycle += cost;
        const auto difference = static_cast<std::uint8_t>(
            checkpoint.accumulator - right);
        checkpoint.status = static_cast<std::uint8_t>(
            checkpoint.status & static_cast<std::uint8_t>(~0x01U));
        checkpoint.status = set_nz(checkpoint.status, difference);
        if (checkpoint.accumulator >= right) {
            checkpoint.status =
                static_cast<std::uint8_t>(checkpoint.status | 0x01U);
        }
        append_checkpoint(instruction_pc, resume_pc, checkpoint);
    };
    const auto branch = [&](const std::uint16_t instruction_pc,
                            const std::uint16_t resume_pc,
                            const bool taken) {
        cycle += taken ? 12U : 6U;
        append_checkpoint(instruction_pc, resume_pc, checkpoint);
    };

    // `LDA <$BA; CMP #$80; ROR <$BA; ROR <$B9` shifts the sign bit the compare
    // parked in the carry back into the top of the word, which is what makes
    // the pair an arithmetic shift rather than a logical one. The routine has
    // three of these blocks laid end to end and enters at the one that gives it
    // the shift count it wants; each falls through into the next.
    const auto shift_block = [&](const std::uint16_t base) {
        const auto shift_word = [&](const std::uint16_t high_offset,
                                    const std::uint16_t low_offset,
                                    const std::uint16_t block_offset) {
            load_zero_page(
                static_cast<std::uint16_t>(base + block_offset),
                static_cast<std::uint16_t>(base + block_offset + 2U),
                high_offset);
            compare(
                static_cast<std::uint16_t>(base + block_offset + 2U),
                static_cast<std::uint16_t>(base + block_offset + 4U),
                0x80U,
                6U);

            const auto rotate = [&](const std::uint16_t instruction_pc,
                                    const std::uint16_t resume_pc,
                                    const std::uint16_t offset) {
                cycle += 18U;
                const auto value =
                    std::to_integer<std::uint8_t>(staged[offset]);
                const auto result = static_cast<std::uint8_t>(
                    (value >> 1U)
                    | ((checkpoint.status & 0x01U) != 0U ? 0x80U : 0x00U));
                checkpoint.status = static_cast<std::uint8_t>(
                    checkpoint.status & static_cast<std::uint8_t>(~0x01U));
                checkpoint.status = set_nz(checkpoint.status, result);
                if ((value & 0x01U) != 0U) {
                    checkpoint.status = static_cast<std::uint8_t>(
                        checkpoint.status | 0x01U);
                }
                append_write(instruction_pc, offset, result);
                append_checkpoint(instruction_pc, resume_pc, checkpoint);
            };
            rotate(
                static_cast<std::uint16_t>(base + block_offset + 4U),
                static_cast<std::uint16_t>(base + block_offset + 6U),
                high_offset);
            rotate(
                static_cast<std::uint16_t>(base + block_offset + 6U),
                static_cast<std::uint16_t>(base + block_offset + 8U),
                low_offset);
        };
        shift_word(0x00BAU, 0x00B9U, 0U);
        shift_word(0x00BCU, 0x00BBU, 8U);
    };

    load_indexed(0xDC95U, 0xDC98U, 0x1480U);
    const bool first_negative = (checkpoint.accumulator & 0x80U) != 0U;
    branch(0xDC98U, first_negative ? 0xDC9AU : 0xDC9DU, !first_negative);
    if (first_negative) {
        cycle += 6U;
        checkpoint.accumulator = static_cast<std::uint8_t>(
            checkpoint.accumulator ^ 0xFFU);
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xDC9AU, 0xDC9CU, checkpoint);

        cycle += 6U;
        checkpoint.accumulator =
            static_cast<std::uint8_t>(checkpoint.accumulator + 1U);
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xDC9CU, 0xDC9DU, checkpoint);
    }
    store_zero_page(0xDC9DU, 0xDC9FU, 0x0057U, checkpoint.accumulator);

    load_indexed(0xDC9FU, 0xDCA2U, 0x1580U);
    const bool second_negative = (checkpoint.accumulator & 0x80U) != 0U;
    branch(0xDCA2U, second_negative ? 0xDCA4U : 0xDCA7U, !second_negative);
    if (second_negative) {
        cycle += 6U;
        checkpoint.accumulator = static_cast<std::uint8_t>(
            checkpoint.accumulator ^ 0xFFU);
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xDCA4U, 0xDCA6U, checkpoint);

        cycle += 6U;
        checkpoint.accumulator =
            static_cast<std::uint8_t>(checkpoint.accumulator + 1U);
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xDCA6U, 0xDCA7U, checkpoint);
    }
    compare(
        0xDCA7U,
        0xDCA9U,
        std::to_integer<std::uint8_t>(staged[0x0057U]),
        12U);

    // Below is a keep, so the branch is taken when the second magnitude loses.
    const bool second_smaller = (checkpoint.status & 0x01U) == 0U;
    branch(0xDCA9U, second_smaller ? 0xDCADU : 0xDCABU, second_smaller);
    if (!second_smaller) {
        store_zero_page(0xDCABU, 0xDCADU, 0x0057U, checkpoint.accumulator);
    }

    load_indexed(0xDCADU, 0xDCB0U, 0x1400U);
    store_zero_page(0xDCB0U, 0xDCB2U, 0x00B9U, checkpoint.accumulator);
    load_indexed(0xDCB2U, 0xDCB5U, 0x1480U);
    store_zero_page(0xDCB5U, 0xDCB7U, 0x00BAU, checkpoint.accumulator);
    load_indexed(0xDCB7U, 0xDCBAU, 0x1500U);
    store_zero_page(0xDCBAU, 0xDCBCU, 0x00BBU, checkpoint.accumulator);
    load_indexed(0xDCBCU, 0xDCBFU, 0x1580U);
    store_zero_page(0xDCBFU, 0xDCC1U, 0x00BCU, checkpoint.accumulator);

    load_index_immediate(0xDCC1U, 0xDCC3U, 0x04U);
    load_zero_page(0xDCC3U, 0xDCC5U, 0x0057U);
    store_zero_page(0xDCC5U, 0xDCC7U, 0x0057U, checkpoint.y);
    compare(0xDCC7U, 0xDCC9U, 0x08U, 6U);

    const bool four_substeps = (checkpoint.status & 0x01U) != 0U;
    branch(0xDCC9U, four_substeps ? 0xDCE8U : 0xDCCBU, four_substeps);
    if (four_substeps) {
        shift_block(0xDCE8U);
        shift_block(0xDCF8U);
    } else {
        load_index_immediate(0xDCCBU, 0xDCCDU, 0x02U);
        store_zero_page(0xDCCDU, 0xDCCFU, 0x0057U, checkpoint.y);
        compare(0xDCCFU, 0xDCD1U, 0x04U, 6U);

        const bool two_substeps = (checkpoint.status & 0x01U) != 0U;
        branch(0xDCD1U, two_substeps ? 0xDCF8U : 0xDCD3U, two_substeps);
        if (two_substeps) {
            shift_block(0xDCF8U);
        } else {
            load_accumulator_immediate(0xDCD3U, 0xDCD5U, 0x01U);
            store_zero_page(0xDCD5U, 0xDCD7U, 0x0057U, checkpoint.accumulator);
        }
    }

    const auto staged_word = [&staged](const std::uint16_t low_offset,
                                       const std::uint16_t high_offset) {
        return static_cast<std::uint16_t>(
            std::to_integer<std::uint8_t>(staged[low_offset])
            | (static_cast<std::uint16_t>(
                   std::to_integer<std::uint8_t>(staged[high_offset]))
               << 8U));
    };
    if (staged_word(0x00B9U, 0x00BAU) != split.first_step
        || staged_word(0x00BBU, 0x00BCU) != split.second_step
        || std::to_integer<std::uint8_t>(staged[0x0057U]) != split.substeps
        || checkpoint.stack_pointer != entry.registers.stack_pointer) {
        throw std::runtime_error(
            "Native DC95 replacement composition diverged from its "
            "state model.");
    }

    const std::uint16_t return_pc =
        split.shifts == 0U ? 0xDCD7U : 0xDD08U;
    checkpoint.program_counter = return_pc;
    replacement.return_instruction = {0x01U, return_pc};
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = cycle;

    absorb_return(
        replacement,
        work_ram,
        entry.registers.stack_pointer,
        {0x01U, return_pc},
        cycle,
        checkpoint);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_unsigned_byte_multiply(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x00U,
        .logical_pc = 0xE569U,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || (entry.registers.status & 0x08U) != 0U) {
        return std::nullopt;
    }

    dc_unsigned_byte_multiply multiply{};
    if (!dc_multiply_unsigned_bytes(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &multiply)) {
        throw std::runtime_error(
            "Native E569 replacement rejected the work-RAM snapshot.");
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x00U,
            .logical_pc = 0xE57FU,
        },
        .output_registers = entry.registers,
    };
    replacement.work_ram_writes.reserve(
        static_cast<std::size_t>(10U + multiply.carry_count));
    replacement.checkpoints.reserve(
        static_cast<std::size_t>(
            45U + 2U * multiply.add_count
            + multiply.carry_count));

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
    const auto shift_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t original,
            const std::uint8_t result) {
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0x01U));
            status = set_nz(status, result);
            if ((original & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(
                    status | 0x01U);
            }
            return status;
        };
    const auto adc_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const std::uint8_t result) {
            const auto sum = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(left)
                + static_cast<std::uint16_t>(right));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC1U));
            status = set_nz(status, result);
            if (sum > 0xFFU) {
                status = static_cast<std::uint8_t>(
                    status | 0x01U);
            }
            if (((~(left ^ right) & (left ^ result)) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(
                    status | 0x40U);
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
    const auto append_write =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t logical_address,
            const std::uint8_t value,
            const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x00U, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
        };

    auto checkpoint = entry.registers;
    std::uint32_t cycle = 9U;
    append_write(
        0xE569U,
        static_cast<std::uint16_t>(
            0x2100U + checkpoint.stack_pointer),
        checkpoint.x,
        cycle);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 1U);
    append_checkpoint(0xE569U, 0xE56AU, cycle, checkpoint);

    cycle += 6U;
    checkpoint.accumulator = 0U;
    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status & static_cast<std::uint8_t>(~0x20U));
    append_checkpoint(0xE56AU, 0xE56BU, cycle, checkpoint);
    cycle += 6U;
    checkpoint.x = 8U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.x);
    append_checkpoint(0xE56BU, 0xE56DU, cycle, checkpoint);

    std::uint8_t low = 0U;
    std::uint8_t high = multiply.multiplier;
    for (std::size_t index = 0U;
         index < DC_UNSIGNED_MULTIPLY_STEP_COUNT;
         ++index) {
        const auto& step = multiply.steps[index];
        const auto low_before_shift = low;
        cycle += 6U;
        low = step.low_after_shift;
        checkpoint.accumulator = low;
        checkpoint.status = shift_status(
            checkpoint.status,
            low_before_shift,
            low);
        append_checkpoint(0xE56DU, 0xE56EU, cycle, checkpoint);

        const auto high_before_shift = high;
        cycle += 18U;
        high = step.high_after_shift;
        checkpoint.status = shift_status(
            checkpoint.status,
            high_before_shift,
            high);
        append_write(0xE56EU, 0x2030U, high, cycle);
        append_checkpoint(0xE56EU, 0xE570U, cycle, checkpoint);

        if (step.adds_multiplicand == 0U) {
            cycle += 12U;
            append_checkpoint(0xE570U, 0xE579U, cycle, checkpoint);
        } else {
            cycle += 6U;
            append_checkpoint(0xE570U, 0xE572U, cycle, checkpoint);
            cycle += 6U;
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status
                & static_cast<std::uint8_t>(~0x21U));
            append_checkpoint(0xE572U, 0xE573U, cycle, checkpoint);

            const auto low_before_add = low;
            cycle += 12U;
            low = step.low_after_add;
            checkpoint.accumulator = low;
            checkpoint.status = adc_status(
                checkpoint.status,
                low_before_add,
                multiply.multiplicand,
                low);
            append_checkpoint(0xE573U, 0xE575U, cycle, checkpoint);
            if (step.add_carry == 0U) {
                cycle += 12U;
                append_checkpoint(
                    0xE575U, 0xE579U, cycle, checkpoint);
            } else {
                cycle += 6U;
                append_checkpoint(
                    0xE575U, 0xE577U, cycle, checkpoint);
                cycle += 18U;
                high = step.high_after_add;
                checkpoint.status =
                    set_nz(checkpoint.status, high);
                append_write(0xE577U, 0x2030U, high, cycle);
                append_checkpoint(
                    0xE577U, 0xE579U, cycle, checkpoint);
            }
        }

        cycle += 6U;
        checkpoint.x = static_cast<std::uint8_t>(
            checkpoint.x - 1U);
        checkpoint.status = set_nz(checkpoint.status, checkpoint.x);
        append_checkpoint(0xE579U, 0xE57AU, cycle, checkpoint);
        cycle += checkpoint.x != 0U ? 12U : 6U;
        append_checkpoint(
            0xE57AU,
            checkpoint.x != 0U ? 0xE56DU : 0xE57CU,
            cycle,
            checkpoint);
    }

    cycle += 12U;
    append_write(0xE57CU, 0x202FU, low, cycle);
    append_checkpoint(0xE57CU, 0xE57EU, cycle, checkpoint);
    cycle += 12U;
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 1U);
    checkpoint.x = entry.registers.x;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.x);
    append_checkpoint(0xE57EU, 0xE57FU, cycle, checkpoint);

    const auto expected_cycles = static_cast<std::uint32_t>(
        471U + 24U * multiply.add_count
        + 12U * multiply.carry_count);
    if (cycle != expected_cycles
        || low != multiply.result_low
        || high != multiply.result_high) {
        throw std::runtime_error(
            "Native E569 transcript diverged from its state model.");
    }
    replacement.body_master_cycles = cycle;
    checkpoint.program_counter = 0xE57FU;
    replacement.output_registers = checkpoint;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_unsigned_multiply_round(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x00U,
        .logical_pc = 0xE624U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    if ((entry.registers.status & 0x08U) != 0U) {
        return std::nullopt;
    }

    dc_unsigned_multiply_round round{};
    if (!dc_multiply_unsigned_round(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &round)) {
        throw std::runtime_error(
            "Native E624 replacement rejected the work-RAM snapshot.");
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x00U,
            .logical_pc = 0xE65BU,
        },
        .output_registers = entry.registers,
        .body_master_cycles = static_cast<std::uint32_t>(
            600U + 84U * round.add_count),
    };
    replacement.work_ram_writes.reserve(
        static_cast<std::size_t>(24U + 3U * round.add_count));
    replacement.checkpoints.reserve(
        static_cast<std::size_t>(41U + 9U * round.add_count));

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
    const auto append_write =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t logical_address,
            const std::uint8_t value,
            const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x00U, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
        };

    auto checkpoint = entry.registers;
    std::uint32_t cycle = 6U;
    checkpoint.x = 4U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.x);
    append_checkpoint(0xE624U, 0xE626U, cycle, checkpoint);

    std::uint8_t low = static_cast<std::uint8_t>(round.accumulator);
    std::uint8_t high =
        static_cast<std::uint8_t>(round.accumulator >> 8U);
    std::uint8_t multiplier = round.multiplier;
    for (std::size_t outer = 0U; outer < 4U; ++outer) {
        for (std::size_t half = 0U; half < 2U; ++half) {
            const auto& step = round.steps[2U * outer + half];
            const auto low_shift_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE626U : 0xE63FU);
            const auto high_shift_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE628U : 0xE641U);
            const auto multiplier_shift_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE62AU : 0xE643U);
            const auto admission_branch_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE62CU : 0xE645U);
            const auto clear_carry_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE62EU : 0xE647U);
            const auto load_low_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE62FU : 0xE648U);
            const auto add_low_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE631U : 0xE64AU);
            const auto store_low_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE633U : 0xE64CU);
            const auto load_high_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE635U : 0xE64EU);
            const auto add_high_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE637U : 0xE650U);
            const auto store_high_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE639U : 0xE652U);
            const auto carry_branch_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE63BU : 0xE654U);
            const auto increment_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE63DU : 0xE656U);
            const auto end_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE63FU : 0xE658U);

            const auto low_before_shift = low;
            cycle += 18U;
            low = step.low_after_shift;
            checkpoint.status = shift_status(
                checkpoint.status,
                low_before_shift,
                low);
            append_write(low_shift_pc, 0x2042U, low, cycle);
            append_checkpoint(
                low_shift_pc,
                high_shift_pc,
                cycle,
                checkpoint);

            const auto high_before_shift = high;
            cycle += 18U;
            high = step.high_after_shift;
            checkpoint.status = shift_status(
                checkpoint.status,
                high_before_shift,
                high);
            append_write(high_shift_pc, 0x2043U, high, cycle);
            append_checkpoint(
                high_shift_pc,
                multiplier_shift_pc,
                cycle,
                checkpoint);

            const auto multiplier_before_shift = multiplier;
            cycle += 18U;
            multiplier = step.multiplier_after_shift;
            checkpoint.status = shift_status(
                checkpoint.status,
                multiplier_before_shift,
                multiplier);
            append_write(
                multiplier_shift_pc,
                0x2030U,
                multiplier,
                cycle);
            append_checkpoint(
                multiplier_shift_pc,
                admission_branch_pc,
                cycle,
                checkpoint);

            if (step.adds_multiplicand == 0U) {
                cycle += 12U;
                append_checkpoint(
                    admission_branch_pc,
                    end_pc,
                    cycle,
                    checkpoint);
                continue;
            }

            cycle += 6U;
            append_checkpoint(
                admission_branch_pc,
                clear_carry_pc,
                cycle,
                checkpoint);
            cycle += 6U;
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status
                & static_cast<std::uint8_t>(~0x21U));
            append_checkpoint(
                clear_carry_pc,
                load_low_pc,
                cycle,
                checkpoint);

            cycle += 12U;
            checkpoint.accumulator = low;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                load_low_pc,
                add_low_pc,
                cycle,
                checkpoint);

            cycle += 12U;
            const auto low_carry_in =
                (checkpoint.status & 0x01U) != 0U;
            checkpoint.accumulator = step.low_after_add;
            checkpoint.status = adc_status(
                checkpoint.status,
                low,
                static_cast<std::uint8_t>(round.multiplicand),
                low_carry_in,
                checkpoint.accumulator);
            low = step.low_after_add;
            append_checkpoint(
                add_low_pc,
                store_low_pc,
                cycle,
                checkpoint);

            cycle += 12U;
            append_write(store_low_pc, 0x2042U, low, cycle);
            append_checkpoint(
                store_low_pc,
                load_high_pc,
                cycle,
                checkpoint);

            cycle += 12U;
            checkpoint.accumulator = high;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                load_high_pc,
                add_high_pc,
                cycle,
                checkpoint);

            cycle += 12U;
            const auto high_carry_in =
                (checkpoint.status & 0x01U) != 0U;
            checkpoint.accumulator = step.high_after_add;
            checkpoint.status = adc_status(
                checkpoint.status,
                high,
                static_cast<std::uint8_t>(
                    round.multiplicand >> 8U),
                high_carry_in,
                checkpoint.accumulator);
            high = step.high_after_add;
            append_checkpoint(
                add_high_pc,
                store_high_pc,
                cycle,
                checkpoint);

            cycle += 12U;
            append_write(store_high_pc, 0x2043U, high, cycle);
            append_checkpoint(
                store_high_pc,
                carry_branch_pc,
                cycle,
                checkpoint);

            if (step.add_carry == 0U) {
                cycle += 12U;
                append_checkpoint(
                    carry_branch_pc,
                    end_pc,
                    cycle,
                    checkpoint);
                multiplier = step.multiplier_after_add;
                continue;
            }

            cycle += 6U;
            append_checkpoint(
                carry_branch_pc,
                increment_pc,
                cycle,
                checkpoint);
            cycle += 18U;
            multiplier = step.multiplier_after_add;
            checkpoint.status =
                set_nz(checkpoint.status, multiplier);
            append_write(increment_pc, 0x2030U, multiplier, cycle);
            append_checkpoint(
                increment_pc,
                end_pc,
                cycle,
                checkpoint);
        }

        cycle += 6U;
        checkpoint.x = static_cast<std::uint8_t>(
            checkpoint.x - 1U);
        checkpoint.status = set_nz(checkpoint.status, checkpoint.x);
        append_checkpoint(0xE658U, 0xE659U, cycle, checkpoint);

        const bool repeats = checkpoint.x != 0U;
        cycle += repeats ? 12U : 6U;
        append_checkpoint(
            0xE659U,
            repeats ? 0xE626U : 0xE65BU,
            cycle,
            checkpoint);
    }

    replacement.body_master_cycles = cycle;
    checkpoint.program_counter = 0xE65BU;
    replacement.output_registers = checkpoint;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_signed_fixed_multiply(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x00U,
        .logical_pc = 0xE65CU,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_signed_fixed_multiply multiply{};
    if (!dc_multiply_signed_fixed(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &multiply)) {
        throw std::runtime_error(
            "Native E65C replacement rejected the work-RAM snapshot.");
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x00U,
            .logical_pc = 0xE694U,
        },
        .output_registers = entry.registers,
    };
    replacement.work_ram_writes.reserve(128U);
    replacement.checkpoints.reserve(320U);
    std::vector<std::byte> staged(work_ram.begin(), work_ram.end());

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
    const auto sbc_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const bool carry_in,
            const std::uint8_t result) {
            const auto difference = static_cast<std::int16_t>(
                static_cast<std::int16_t>(left)
                - static_cast<std::int16_t>(right)
                - static_cast<std::int16_t>(carry_in ? 0 : 1));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC1U));
            status = set_nz(status, result);
            if (difference >= 0) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if ((((left ^ right) & (left ^ result)) & 0x80U) != 0U) {
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
    const auto append_write =
        [&replacement, &staged](
            const std::uint16_t instruction_pc,
            const std::uint16_t logical_address,
            const std::uint8_t value,
            const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x00U, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
            if (logical_address >= 0x2000U
                && logical_address < 0x4000U) {
                staged[logical_address - 0x2000U] =
                    static_cast<std::byte>(value);
            }
        };

    auto checkpoint = entry.registers;
    std::uint32_t cycle = 0U;
    const auto load_zero_page =
        [&checkpoint, &cycle, &staged, &set_nz, &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            checkpoint.accumulator =
                std::to_integer<std::uint8_t>(staged[offset]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto store_zero_page =
        [&checkpoint, &cycle, &append_write, &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            append_write(
                instruction_pc,
                static_cast<std::uint16_t>(0x2000U + offset),
                checkpoint.accumulator,
                cycle);
            append_checkpoint(
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto set_carry =
        [&checkpoint, &cycle, &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc) {
            cycle += 6U;
            checkpoint.status = static_cast<std::uint8_t>(
                (checkpoint.status
                    & static_cast<std::uint8_t>(~0x20U))
                | 0x01U);
            append_checkpoint(
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto clear_accumulator =
        [&checkpoint, &cycle, &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc) {
            cycle += 6U;
            checkpoint.accumulator = 0U;
            append_checkpoint(
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto subtract_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &sbc_status,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            const auto left = checkpoint.accumulator;
            const auto right =
                std::to_integer<std::uint8_t>(staged[offset]);
            const auto carry_in =
                (checkpoint.status & 0x01U) != 0U;
            checkpoint.accumulator = static_cast<std::uint8_t>(
                static_cast<std::uint16_t>(left)
                - static_cast<std::uint16_t>(right)
                - static_cast<std::uint16_t>(carry_in ? 0U : 1U));
            checkpoint.status = sbc_status(
                checkpoint.status,
                left,
                right,
                carry_in,
                checkpoint.accumulator);
            cycle += 12U;
            append_checkpoint(
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto append_jsr =
        [&checkpoint, &cycle, &append_write, &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t target_pc,
            const std::uint16_t pushed_return_pc) {
            append_write(
                instruction_pc,
                static_cast<std::uint16_t>(
                    0x2100U + checkpoint.stack_pointer),
                static_cast<std::uint8_t>(pushed_return_pc >> 8U),
                cycle);
            append_write(
                instruction_pc,
                static_cast<std::uint16_t>(
                    0x2100U
                    + static_cast<std::uint8_t>(
                        checkpoint.stack_pointer - 1U)),
                static_cast<std::uint8_t>(pushed_return_pc),
                cycle);
            checkpoint.stack_pointer = static_cast<std::uint8_t>(
                checkpoint.stack_pointer - 2U);
            cycle += 21U;
            append_checkpoint(
                instruction_pc,
                target_pc,
                cycle,
                checkpoint);
        };

    load_zero_page(0xE65CU, 0xE65EU, 0x0030U);
    store_zero_page(0xE65EU, 0xE660U, 0x0046U);
    if (multiply.negates_multiplier == 0U) {
        cycle += 12U;
        append_checkpoint(0xE660U, 0xE66DU, cycle, checkpoint);
    } else {
        cycle += 6U;
        append_checkpoint(0xE660U, 0xE662U, cycle, checkpoint);
        set_carry(0xE662U, 0xE663U);
        clear_accumulator(0xE663U, 0xE664U);
        subtract_zero_page(0xE664U, 0xE666U, 0x002FU);
        store_zero_page(0xE666U, 0xE668U, 0x002FU);
        clear_accumulator(0xE668U, 0xE669U);
        subtract_zero_page(0xE669U, 0xE66BU, 0x0030U);
        store_zero_page(0xE66BU, 0xE66DU, 0x0030U);
    }

    load_zero_page(0xE66DU, 0xE66FU, 0x002EU);
    cycle += 12U;
    checkpoint.accumulator = static_cast<std::uint8_t>(
        checkpoint.accumulator
        ^ std::to_integer<std::uint8_t>(staged[0x0046U]));
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xE66FU, 0xE671U, cycle, checkpoint);
    store_zero_page(0xE671U, 0xE673U, 0x0046U);
    load_zero_page(0xE673U, 0xE675U, 0x002EU);
    if (multiply.negates_multiplicand == 0U) {
        cycle += 12U;
        append_checkpoint(0xE675U, 0xE682U, cycle, checkpoint);
    } else {
        cycle += 6U;
        append_checkpoint(0xE675U, 0xE677U, cycle, checkpoint);
        set_carry(0xE677U, 0xE678U);
        clear_accumulator(0xE678U, 0xE679U);
        subtract_zero_page(0xE679U, 0xE67BU, 0x002DU);
        store_zero_page(0xE67BU, 0xE67DU, 0x002DU);
        clear_accumulator(0xE67DU, 0xE67EU);
        subtract_zero_page(0xE67EU, 0xE680U, 0x002EU);
        store_zero_page(0xE680U, 0xE682U, 0x002EU);
    }

    append_jsr(0xE682U, 0xE60BU, 0xE684U);

    cycle += 9U;
    append_write(
        0xE60BU,
        static_cast<std::uint16_t>(
            0x2100U + checkpoint.stack_pointer),
        checkpoint.x,
        cycle);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 1U);
    append_checkpoint(0xE60BU, 0xE60CU, cycle, checkpoint);

    cycle += 12U;
    append_write(0xE60CU, 0x2042U, 0U, cycle);
    append_checkpoint(0xE60CU, 0xE60EU, cycle, checkpoint);
    cycle += 12U;
    append_write(0xE60EU, 0x2043U, 0U, cycle);
    append_checkpoint(0xE60EU, 0xE610U, cycle, checkpoint);
    load_zero_page(0xE610U, 0xE612U, 0x0030U);

    const auto append_multiply_round =
        [&replacement,
         &staged,
         &checkpoint,
         &cycle,
         &append_jsr,
         &append_checkpoint](
            const std::uint16_t jsr_pc,
            const std::uint16_t pushed_return_pc,
            const std::uint16_t resume_pc) {
            append_jsr(jsr_pc, 0xE624U, pushed_return_pc);
            const auto child_offset = cycle;
            RoutineEntry child_entry{
                .routine = {
                    .physical_bank = 0x00U,
                    .logical_pc = 0xE624U,
                },
                .registers = checkpoint,
            };
            child_entry.registers.program_counter = 0xE624U;
            auto child = replace_unsigned_multiply_round(
                child_entry,
                staged);
            if (!child || child->checkpoints.empty()) {
                return false;
            }
            for (auto write : child->work_ram_writes) {
                write.master_cycle_offset += child_offset;
                replacement.work_ram_writes.push_back(write);
                staged[write.logical_address - 0x2000U] =
                    static_cast<std::byte>(write.value);
            }
            for (auto child_checkpoint : child->checkpoints) {
                child_checkpoint.interrupt_sample_master_cycle_offset +=
                    child_offset;
                child_checkpoint.master_cycle_offset += child_offset;
                replacement.checkpoints.push_back(child_checkpoint);
            }
            cycle += child->body_master_cycles;
            checkpoint = child->output_registers;
            checkpoint.stack_pointer = static_cast<std::uint8_t>(
                checkpoint.stack_pointer + 2U);
            cycle += 21U;
            append_checkpoint(
                child->return_instruction.logical_pc,
                resume_pc,
                cycle,
                checkpoint);
            return true;
        };

    if (multiply.runs_high_round == 0U) {
        cycle += 12U;
        append_checkpoint(0xE612U, 0xE617U, cycle, checkpoint);
    } else {
        cycle += 6U;
        append_checkpoint(0xE612U, 0xE614U, cycle, checkpoint);
        if (!append_multiply_round(
                0xE614U,
                0xE616U,
                0xE617U)) {
            return std::nullopt;
        }
    }

    load_zero_page(0xE617U, 0xE619U, 0x002FU);
    store_zero_page(0xE619U, 0xE61BU, 0x0030U);
    if (!append_multiply_round(
            0xE61BU,
            0xE61DU,
            0xE61EU)) {
        return std::nullopt;
    }
    load_zero_page(0xE61EU, 0xE620U, 0x0043U);
    store_zero_page(0xE620U, 0xE622U, 0x002FU);

    cycle += 12U;
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 1U);
    checkpoint.x = std::to_integer<std::uint8_t>(
        staged[0x0100U + checkpoint.stack_pointer]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.x);
    append_checkpoint(0xE622U, 0xE623U, cycle, checkpoint);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 2U);
    cycle += 21U;
    append_checkpoint(0xE623U, 0xE685U, cycle, checkpoint);

    load_zero_page(0xE685U, 0xE687U, 0x0046U);
    if ((multiply.sign_xor & 0x80U) == 0U) {
        cycle += 12U;
        append_checkpoint(0xE687U, 0xE694U, cycle, checkpoint);
    } else {
        cycle += 6U;
        append_checkpoint(0xE687U, 0xE689U, cycle, checkpoint);
        set_carry(0xE689U, 0xE68AU);
        clear_accumulator(0xE68AU, 0xE68BU);
        subtract_zero_page(0xE68BU, 0xE68DU, 0x002FU);
        store_zero_page(0xE68DU, 0xE68FU, 0x002FU);
        clear_accumulator(0xE68FU, 0xE690U);
        subtract_zero_page(0xE690U, 0xE692U, 0x0030U);
        store_zero_page(0xE692U, 0xE694U, 0x0030U);
    }

    const auto staged_result = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(staged[0x002FU])
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(staged[0x0030U]))
           << 8U));
    if (staged_result != multiply.result
        || checkpoint.stack_pointer
            != entry.registers.stack_pointer) {
        throw std::runtime_error(
            "Native E65C replacement composition diverged from its "
            "state model.");
    }
    checkpoint.program_counter = 0xE694U;
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = cycle;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_signed_word_negation(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x00U,
        .logical_pc = 0xE837U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_signed_word_negation negation{};
    if (!dc_negate_signed_word(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &negation)) {
        throw std::runtime_error(
            "Native E837 replacement rejected the work-RAM snapshot.");
    }

    const auto input_low =
        static_cast<std::uint8_t>(negation.input & 0xFFU);
    const auto input_high =
        static_cast<std::uint8_t>(negation.input >> 8U);
    const auto complemented_low =
        static_cast<std::uint8_t>(input_low ^ 0xFFU);
    const auto result_low =
        static_cast<std::uint8_t>(negation.result & 0xFFU);
    const auto result_high =
        static_cast<std::uint8_t>(negation.result >> 8U);

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

    auto output = entry.registers;
    output.program_counter = 0xE849U;
    output.accumulator = negation.complemented_high;
    output.status = set_nz(
        output.status,
        negation.increments_high != 0U ? result_high : result_low);

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x00U,
            .logical_pc = 0xE849U,
        },
        .output_registers = output,
        .body_master_cycles =
            negation.increments_high != 0U ? 102U : 90U,
    };
    replacement.work_ram_writes = {
        {
            .instruction = {0x00U, 0xE83BU},
            .logical_address = 0x202FU,
            .value = complemented_low,
            .master_cycle_offset = 30U,
        },
        {
            .instruction = {0x00U, 0xE841U},
            .logical_address = 0x2030U,
            .value = negation.complemented_high,
            .master_cycle_offset = 60U,
        },
        {
            .instruction = {0x00U, 0xE843U},
            .logical_address = 0x202FU,
            .value = result_low,
            .master_cycle_offset = 78U,
        },
    };
    if (negation.increments_high != 0U) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x00U, 0xE847U},
            .logical_address = 0x2030U,
            .value = result_high,
            .master_cycle_offset = 102U,
        });
    }

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

    auto checkpoint = entry.registers;
    checkpoint.accumulator = input_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xE837U, 0xE839U, 12U, checkpoint);

    checkpoint.accumulator = complemented_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xE839U, 0xE83BU, 18U, checkpoint);
    append_checkpoint(0xE83BU, 0xE83DU, 30U, checkpoint);

    checkpoint.accumulator = input_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xE83DU, 0xE83FU, 42U, checkpoint);

    checkpoint.accumulator = negation.complemented_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xE83FU, 0xE841U, 48U, checkpoint);
    append_checkpoint(0xE841U, 0xE843U, 60U, checkpoint);

    checkpoint.status = set_nz(checkpoint.status, result_low);
    append_checkpoint(0xE843U, 0xE845U, 78U, checkpoint);
    if (negation.increments_high == 0U) {
        append_checkpoint(0xE845U, 0xE849U, 90U, output);
        return replacement;
    }

    append_checkpoint(0xE845U, 0xE847U, 84U, checkpoint);
    append_checkpoint(0xE847U, 0xE849U, 102U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_flipper_projection(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr std::size_t kMaximumReplacementWrites = 512U;
    constexpr std::size_t kMaximumReplacementCheckpoints = 2048U;
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xD09EU,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || entry.registers.x >= 0x80U
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_flipper_projection projection{};
    if (!dc_project_flipper_vectors(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &projection)) {
        throw std::runtime_error(
            "Native D09E replacement rejected the work-RAM snapshot.");
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xD11FU,
        },
        .output_registers = entry.registers,
    };
    replacement.work_ram_writes.reserve(512U);
    replacement.checkpoints.reserve(512U);
    std::vector<std::byte> staged(work_ram.begin(), work_ram.end());

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
    const auto append_checkpoint =
        [&replacement](
            const std::uint8_t instruction_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {
                    instruction_bank,
                    instruction_pc,
                },
                .registers = registers,
            });
        };
    const auto append_write =
        [&replacement, &staged](
            const std::uint8_t instruction_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t logical_address,
            const std::uint8_t value,
            const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {
                    instruction_bank,
                    instruction_pc,
                },
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
            if (logical_address >= 0x2000U
                && logical_address < 0x4000U) {
                staged[logical_address - 0x2000U] =
                    static_cast<std::byte>(value);
            }
        };

    auto checkpoint = entry.registers;
    std::uint32_t cycle = 0U;
    const auto load_indexed =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t logical_base) {
            cycle += 15U;
            checkpoint.accumulator = std::to_integer<std::uint8_t>(
                staged[logical_base - 0x2000U + checkpoint.x]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto load_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            checkpoint.accumulator =
                std::to_integer<std::uint8_t>(staged[offset]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto store_zero_page =
        [&checkpoint,
         &cycle,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(0x2000U + offset),
                checkpoint.accumulator,
                cycle);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto clear_carry =
        [&checkpoint, &cycle, &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc) {
            cycle += 6U;
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status
                & static_cast<std::uint8_t>(~0x21U));
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto add_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &adc_status,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            const auto left = checkpoint.accumulator;
            const auto right =
                std::to_integer<std::uint8_t>(staged[offset]);
            const auto carry_in =
                (checkpoint.status & 0x01U) != 0U;
            checkpoint.accumulator = static_cast<std::uint8_t>(
                static_cast<std::uint16_t>(left)
                + static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in));
            checkpoint.status = adc_status(
                checkpoint.status,
                left,
                right,
                carry_in,
                checkpoint.accumulator);
            cycle += 12U;
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto begin_jsr =
        [&checkpoint,
         &cycle,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t target_pc,
            const std::uint16_t pushed_return_pc) {
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(
                    0x2100U + checkpoint.stack_pointer),
                static_cast<std::uint8_t>(pushed_return_pc >> 8U),
                cycle);
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(
                    0x2100U
                    + static_cast<std::uint8_t>(
                        checkpoint.stack_pointer - 1U)),
                static_cast<std::uint8_t>(pushed_return_pc),
                cycle);
            checkpoint.stack_pointer = static_cast<std::uint8_t>(
                checkpoint.stack_pointer - 2U);
            cycle += 21U;
            append_checkpoint(
                0x01U,
                instruction_pc,
                target_pc,
                cycle,
                checkpoint);
        };
    const auto append_child =
        [&replacement,
         &staged,
         &checkpoint,
         &cycle,
         &append_checkpoint](
            const RoutineReplacement& child,
            const std::uint16_t resume_pc) {
            const auto child_offset = cycle;
            for (auto write : child.work_ram_writes) {
                write.master_cycle_offset += child_offset;
                replacement.work_ram_writes.push_back(write);
                staged[write.logical_address - 0x2000U] =
                    static_cast<std::byte>(write.value);
            }
            for (auto child_checkpoint : child.checkpoints) {
                child_checkpoint.interrupt_sample_master_cycle_offset +=
                    child_offset;
                child_checkpoint.master_cycle_offset += child_offset;
                replacement.checkpoints.push_back(child_checkpoint);
            }
            cycle += child.body_master_cycles;
            checkpoint = child.output_registers;
            checkpoint.stack_pointer = static_cast<std::uint8_t>(
                checkpoint.stack_pointer + 2U);
            cycle += 21U;
            append_checkpoint(
                child.return_instruction.physical_bank,
                child.return_instruction.logical_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto run_signed_multiply =
        [&checkpoint,
         &staged,
         &begin_jsr,
         &append_child](
            const std::uint16_t instruction_pc,
            const std::uint16_t pushed_return_pc,
            const std::uint16_t resume_pc) {
            begin_jsr(
                instruction_pc,
                0xE65CU,
                pushed_return_pc);
            RoutineEntry child_entry{
                .routine = {
                    .physical_bank = 0x00U,
                    .logical_pc = 0xE65CU,
                },
                .registers = checkpoint,
            };
            child_entry.registers.program_counter = 0xE65CU;
            auto child = replace_signed_fixed_multiply(
                child_entry,
                staged);
            if (!child || child->checkpoints.empty()) {
                return false;
            }
            append_child(*child, resume_pc);
            return true;
        };
    const auto run_negation =
        [&checkpoint,
         &staged,
         &begin_jsr,
         &append_child](
            const std::uint16_t instruction_pc,
            const std::uint16_t pushed_return_pc,
            const std::uint16_t resume_pc) {
            begin_jsr(
                instruction_pc,
                0xE837U,
                pushed_return_pc);
            RoutineEntry child_entry{
                .routine = {
                    .physical_bank = 0x00U,
                    .logical_pc = 0xE837U,
                },
                .registers = checkpoint,
            };
            child_entry.registers.program_counter = 0xE837U;
            auto child = replace_signed_word_negation(
                child_entry,
                staged);
            if (!child || child->checkpoints.empty()) {
                return false;
            }
            append_child(*child, resume_pc);
            return true;
        };

    load_indexed(0xD09EU, 0xD0A1U, 0x3400U);
    store_zero_page(0xD0A1U, 0xD0A3U, 0x002FU);
    load_indexed(0xD0A3U, 0xD0A6U, 0x3480U);
    store_zero_page(0xD0A6U, 0xD0A8U, 0x0030U);
    load_zero_page(0xD0A8U, 0xD0AAU, 0x007DU);
    store_zero_page(0xD0AAU, 0xD0ACU, 0x002DU);
    load_zero_page(0xD0ACU, 0xD0AEU, 0x007EU);
    store_zero_page(0xD0AEU, 0xD0B0U, 0x002EU);
    if (!run_signed_multiply(
            0xD0B0U,
            0xD0B2U,
            0xD0B3U)) {
        return std::nullopt;
    }
    load_zero_page(0xD0B3U, 0xD0B5U, 0x002FU);
    store_zero_page(0xD0B5U, 0xD0B7U, 0x00C0U);
    load_zero_page(0xD0B7U, 0xD0B9U, 0x0030U);
    store_zero_page(0xD0B9U, 0xD0BBU, 0x00C1U);

    load_indexed(0xD0BBU, 0xD0BEU, 0x3500U);
    store_zero_page(0xD0BEU, 0xD0C0U, 0x002FU);
    load_indexed(0xD0C0U, 0xD0C3U, 0x3580U);
    store_zero_page(0xD0C3U, 0xD0C5U, 0x0030U);
    load_zero_page(0xD0C5U, 0xD0C7U, 0x007FU);
    store_zero_page(0xD0C7U, 0xD0C9U, 0x002DU);
    load_zero_page(0xD0C9U, 0xD0CBU, 0x0080U);
    store_zero_page(0xD0CBU, 0xD0CDU, 0x002EU);
    if (!run_signed_multiply(
            0xD0CDU,
            0xD0CFU,
            0xD0D0U)) {
        return std::nullopt;
    }
    clear_carry(0xD0D0U, 0xD0D1U);
    load_zero_page(0xD0D1U, 0xD0D3U, 0x00C0U);
    add_zero_page(0xD0D3U, 0xD0D5U, 0x002FU);
    store_zero_page(0xD0D5U, 0xD0D7U, 0x00C0U);
    load_zero_page(0xD0D7U, 0xD0D9U, 0x00C1U);
    add_zero_page(0xD0D9U, 0xD0DBU, 0x0030U);
    store_zero_page(0xD0DBU, 0xD0DDU, 0x00C1U);

    load_indexed(0xD0DDU, 0xD0E0U, 0x3400U);
    store_zero_page(0xD0E0U, 0xD0E2U, 0x002FU);
    load_indexed(0xD0E2U, 0xD0E5U, 0x3480U);
    store_zero_page(0xD0E5U, 0xD0E7U, 0x0030U);
    load_zero_page(0xD0E7U, 0xD0E9U, 0x007FU);
    store_zero_page(0xD0E9U, 0xD0EBU, 0x002DU);
    load_zero_page(0xD0EBU, 0xD0EDU, 0x0080U);
    store_zero_page(0xD0EDU, 0xD0EFU, 0x002EU);
    if (!run_signed_multiply(
            0xD0EFU,
            0xD0F1U,
            0xD0F2U)
        || !run_negation(
            0xD0F2U,
            0xD0F4U,
            0xD0F5U)) {
        return std::nullopt;
    }
    load_zero_page(0xD0F5U, 0xD0F7U, 0x002FU);
    store_zero_page(0xD0F7U, 0xD0F9U, 0x00BEU);
    load_zero_page(0xD0F9U, 0xD0FBU, 0x0030U);
    store_zero_page(0xD0FBU, 0xD0FDU, 0x00BFU);

    load_indexed(0xD0FDU, 0xD100U, 0x3500U);
    store_zero_page(0xD100U, 0xD102U, 0x002FU);
    load_indexed(0xD102U, 0xD105U, 0x3580U);
    store_zero_page(0xD105U, 0xD107U, 0x0030U);
    load_zero_page(0xD107U, 0xD109U, 0x007DU);
    store_zero_page(0xD109U, 0xD10BU, 0x002DU);
    load_zero_page(0xD10BU, 0xD10DU, 0x007EU);
    store_zero_page(0xD10DU, 0xD10FU, 0x002EU);
    if (!run_signed_multiply(
            0xD10FU,
            0xD111U,
            0xD112U)) {
        return std::nullopt;
    }
    clear_carry(0xD112U, 0xD113U);
    load_zero_page(0xD113U, 0xD115U, 0x00BEU);
    add_zero_page(0xD115U, 0xD117U, 0x002FU);
    store_zero_page(0xD117U, 0xD119U, 0x00BEU);
    load_zero_page(0xD119U, 0xD11BU, 0x00BFU);
    add_zero_page(0xD11BU, 0xD11DU, 0x0030U);
    store_zero_page(0xD11DU, 0xD11FU, 0x00BFU);

    const auto first_output = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(staged[0x00C0U])
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(staged[0x00C1U]))
           << 8U));
    const auto second_output = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(staged[0x00BEU])
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(staged[0x00BFU]))
           << 8U));
    if (first_output != projection.first_output
        || second_output != projection.second_output
        || checkpoint.stack_pointer
            != entry.registers.stack_pointer) {
        throw std::runtime_error(
            "Native D09E replacement composition diverged from its "
            "state model.");
    }
    if (replacement.work_ram_writes.size()
            > kMaximumReplacementWrites
        || replacement.checkpoints.size()
            > kMaximumReplacementCheckpoints) {
        return std::nullopt;
    }
    checkpoint.program_counter = 0xD11FU;
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = cycle;
    absorb_return(
        replacement,
        work_ram,
        entry.registers.stack_pointer,
        {0x01U, 0xD11FU},
        cycle,
        checkpoint);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_cached_position_restore(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x00U,
        .logical_pc = 0xF241U,
    };
    constexpr std::array<std::uint16_t, DC_CACHED_POSITION_BYTE_COUNT>
        kStoreInstructionPcs{
            0xF243U,
            0xF248U,
            0xF24DU,
            0xF252U,
            0xF257U,
            0xF25CU,
        };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_cached_position_restore restored{};
    if (!dc_restore_cached_position(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &restored)) {
        throw std::runtime_error(
            "Native F241 replacement rejected the work-RAM snapshot.");
    }

    auto output = entry.registers;
    output.program_counter = 0xF25FU;
    output.accumulator =
        restored.values[DC_CACHED_POSITION_BYTE_COUNT - 1U];
    output.status = static_cast<std::uint8_t>(
        output.status & static_cast<std::uint8_t>(~(0x80U | 0x02U)));
    if (output.accumulator == 0U) {
        output.status = static_cast<std::uint8_t>(
            output.status | 0x02U);
    }
    if ((output.accumulator & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(
            output.status | 0x80U);
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x00U,
            .logical_pc = 0xF25FU,
        },
        .output_registers = output,
        .body_master_cycles = 162U,
    };
    replacement.work_ram_writes.reserve(
        DC_CACHED_POSITION_BYTE_COUNT);
    for (std::size_t index = 0;
         index < DC_CACHED_POSITION_BYTE_COUNT;
         ++index) {
        replacement.work_ram_writes.push_back({
            .instruction = {
                .physical_bank = 0x00U,
                .logical_pc = kStoreInstructionPcs[index],
            },
            .logical_address = static_cast<std::uint16_t>(
                0x2000U + restored.work_ram_offsets[index]),
            .value = restored.values[index],
            .master_cycle_offset =
                static_cast<std::uint32_t>(27U * (index + 1U)),
        });
    }

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

    auto checkpoint = entry.registers;
    for (std::size_t index = 0;
         index < DC_CACHED_POSITION_BYTE_COUNT;
         ++index) {
        const auto load_pc = static_cast<std::uint16_t>(
            0xF241U + 5U * index);
        const auto store_pc = kStoreInstructionPcs[index];
        const auto load_cycle = static_cast<std::uint32_t>(
            12U + 27U * index);
        const auto store_cycle = static_cast<std::uint32_t>(
            27U * (index + 1U));

        checkpoint.accumulator = restored.values[index];
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(
            load_pc,
            store_pc,
            load_cycle,
            checkpoint);
        append_checkpoint(
            store_pc,
            static_cast<std::uint16_t>(store_pc + 3U),
            store_cycle,
            index + 1U == DC_CACHED_POSITION_BYTE_COUNT
                ? output
                : checkpoint);
    }

    // 00:F25F is this routine's only return, and the six loads and stores
    // between it and 00:F241 touch no stack, so the frame is where the entry
    // pointer says. Nothing composes this replacement.
    if (!replacement.checkpoints.empty()
        || absorbed_return_fits_atomically(
            entry, replacement.body_master_cycles)) {
        absorb_return(
            replacement,
            work_ram,
            entry.registers.stack_pointer,
            {0x00U, 0xF25FU},
            replacement.body_master_cycles,
            output);
    }
    return replacement;
}
} // namespace devils_crush::app::detail
