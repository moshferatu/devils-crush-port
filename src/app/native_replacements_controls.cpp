// SPDX-License-Identifier: GPL-3.0-or-later

#include "native_replacements_internal.hpp"
#include "devils_crush/game_logic/actor.h"
#include "devils_crush/game_logic/controls.h"
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
replace_flipper_input_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xD466U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_flipper_input_gate gate{};
    if (!dc_evaluate_flipper_input_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &gate)) {
        throw std::runtime_error(
            "Native D466 replacement rejected the work-RAM snapshot.");
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
        [](
            RoutineReplacement& replacement,
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

    auto output = entry.registers;
    if (gate.matched_input == 0U) {
        output.program_counter = 0xD46EU;
        output.accumulator = gate.matched_input;
        output.status = static_cast<std::uint8_t>(
            output.status & static_cast<std::uint8_t>(~0x82U));
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);

        RoutineReplacement replacement{
            .return_instruction = {
                .physical_bank = 0x01U,
                .logical_pc = 0xD46EU,
            },
            .output_registers = output,
            .body_master_cycles = 36U,
        };
        auto checkpoint = entry.registers;
        checkpoint.accumulator = gate.global_input;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(
            replacement, 0xD466U, 0xD469U, 15U, checkpoint);
        checkpoint.accumulator = gate.matched_input;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(
            replacement, 0xD469U, 0xD46CU, 30U, checkpoint);
        append_checkpoint(
            replacement, 0xD46CU, 0xD46EU, 36U, output);
        absorb_return(
            replacement,
            work_ram,
            entry.registers.stack_pointer,
            {0x01U, 0xD46EU},
            36U,
            output);
        return replacement;
    }

    dc_flipper_activation activation{};
    if (!dc_prepare_flipper_activation(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &gate,
            &activation)) {
        throw std::runtime_error(
            "Native D466 activation rejected the work-RAM snapshot.");
    }
    if (activation.supported == 0U) {
        return std::nullopt;
    }

    output.program_counter = 0xC5E9U;
    output.accumulator = activation.dispatch_high;
    output.status = static_cast<std::uint8_t>(
        output.status & static_cast<std::uint8_t>(~0x83U));
    output.status = static_cast<std::uint8_t>(output.status | 0x80U);
    if (activation.compare_carry != 0U) {
        output.status = static_cast<std::uint8_t>(
            output.status | 0x01U);
    }

    const auto stack_high = entry.registers.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xC5E9U,
        },
        .output_registers = output,
        .body_master_cycles = 282U,
    };
    replacement.work_ram_writes = {
        {
            .instruction = {0x01U, 0xD471U},
            .logical_address = static_cast<std::uint16_t>(
                0x3E00U + entry.registers.x),
            .value = 0x01U,
            .master_cycle_offset = 63U,
        },
        {
            .instruction = {0x01U, 0xD474U},
            .logical_address = static_cast<std::uint16_t>(
                0x3A00U + entry.registers.x),
            .value = 0x01U,
            .master_cycle_offset = 78U,
        },
        {
            .instruction = {0x01U, 0xD48BU},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + stack_high),
            .value = 0xD4U,
            .master_cycle_offset = 102U,
        },
        {
            .instruction = {0x01U, 0xD48BU},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + stack_low),
            .value = 0x8DU,
            .master_cycle_offset = 102U,
        },
        {
            .instruction = {0x01U, 0xC5CAU},
            .logical_address = 0x2026U,
            .value = 0x8DU,
            .master_cycle_offset = 147U,
        },
        {
            .instruction = {0x01U, 0xC5CDU},
            .logical_address = 0x2027U,
            .value = 0xD4U,
            .master_cycle_offset = 171U,
        },
        {
            .instruction = {0x01U, 0xC5D9U},
            .logical_address = 0x2026U,
            .value = activation.dispatch_low,
            .master_cycle_offset = 216U,
        },
        {
            .instruction = {0x01U, 0xC5E1U},
            .logical_address = static_cast<std::uint16_t>(
                0x3F00U + entry.registers.x),
            .value = activation.dispatch_low,
            .master_cycle_offset = 255U,
        },
        {
            .instruction = {0x01U, 0xC5E6U},
            .logical_address = static_cast<std::uint16_t>(
                0x3F80U + entry.registers.x),
            .value = activation.dispatch_high,
            .master_cycle_offset = 282U,
        },
    };

    auto checkpoint = entry.registers;
    checkpoint.accumulator = gate.global_input;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        replacement, 0xD466U, 0xD469U, 15U, checkpoint);

    checkpoint.accumulator = gate.matched_input;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        replacement, 0xD469U, 0xD46CU, 30U, checkpoint);
    append_checkpoint(
        replacement, 0xD46CU, 0xD46FU, 42U, checkpoint);

    checkpoint.accumulator = 0x01U;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        replacement, 0xD46FU, 0xD471U, 48U, checkpoint);
    append_checkpoint(
        replacement, 0xD471U, 0xD474U, 63U, checkpoint);
    append_checkpoint(
        replacement, 0xD474U, 0xD477U, 78U, checkpoint);

    const auto compare_result = static_cast<std::uint8_t>(
        entry.registers.x - activation.current_slot);
    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status & static_cast<std::uint8_t>(~0xA3U));
    if (compare_result == 0U) {
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status | 0x02U);
    } else if ((compare_result & 0x80U) != 0U) {
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status | 0x80U);
    }
    if (activation.compare_carry != 0U) {
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status | 0x01U);
    }
    append_checkpoint(
        replacement, 0xD477U, 0xD479U, 90U, checkpoint);
    append_checkpoint(
        replacement, 0xD479U, 0xD48BU, 102U, checkpoint);

    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(
        replacement, 0xD48BU, 0xC5C9U, 123U, checkpoint);

    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer + 1U);
    checkpoint.accumulator = 0x8DU;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        replacement, 0xC5C9U, 0xC5CAU, 135U, checkpoint);
    append_checkpoint(
        replacement, 0xC5CAU, 0xC5CCU, 147U, checkpoint);

    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer + 1U);
    checkpoint.accumulator = 0xD4U;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        replacement, 0xC5CCU, 0xC5CDU, 159U, checkpoint);
    append_checkpoint(
        replacement, 0xC5CDU, 0xC5CFU, 171U, checkpoint);

    checkpoint.accumulator = activation.coroutine_countdown;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        replacement, 0xC5CFU, 0xC5D2U, 186U, checkpoint);
    append_checkpoint(
        replacement, 0xC5D2U, 0xC5D9U, 198U, checkpoint);

    checkpoint.status =
        set_nz(checkpoint.status, activation.dispatch_low);
    append_checkpoint(
        replacement, 0xC5D9U, 0xC5DBU, 216U, checkpoint);
    append_checkpoint(
        replacement, 0xC5DBU, 0xC5DFU, 228U, checkpoint);

    checkpoint.accumulator = activation.dispatch_low;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        replacement, 0xC5DFU, 0xC5E1U, 240U, checkpoint);
    append_checkpoint(
        replacement, 0xC5E1U, 0xC5E4U, 255U, checkpoint);

    checkpoint.accumulator = activation.dispatch_high;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        replacement, 0xC5E4U, 0xC5E6U, 267U, checkpoint);
    append_checkpoint(
        replacement, 0xC5E6U, 0xC5E9U, 282U, output);

    // The `RTS` this walk ends in front of belongs to the scheduler it ran in
    // line, but the frame it reads is this routine's caller's: 01:D48B staged a
    // `JSR` frame at $2100+S and $2100+S-1, and the scheduler's two `PLA`s at
    // 01:C5C9 and 01:C5CC took it straight back off to learn the address it was
    // called from. So the pointer is the entry's again by 01:C5E9, and the word
    // the return reads sits immediately above the frame this transcript staged
    // rather than on top of it.
    absorb_return(
        replacement,
        work_ram,
        entry.registers.stack_pointer,
        {0x01U, 0xC5E9U},
        282U,
        output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_flipper_release_arm(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xD4B3U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_flipper_release_arm release{};
    if (!dc_prepare_flipper_release_arm(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &release)) {
        throw std::runtime_error(
            "Native D4B3 replacement rejected the work-RAM snapshot.");
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

    const std::uint32_t scheduler_increment_cycle =
        release.decrements_countdown != 0U ? 207U : 186U;
    const std::uint32_t body_cycles =
        release.installs_dispatch == 0U
        ? 195U
        : (release.decrements_countdown != 0U ? 273U : 252U);

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    if (release.installs_dispatch != 0U) {
        output.accumulator = release.dispatch_high;
        output.status = set_nz(output.status, output.accumulator);
    } else {
        output.accumulator = release.coroutine_countdown;
        output.status =
            set_nz(output.status, release.coroutine_countdown_after);
    }

    const auto stack_high = entry.registers.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xC5E9U,
        },
        .output_registers = output,
        .body_master_cycles = body_cycles,
    };
    replacement.work_ram_writes = {
        {
            .instruction = {0x01U, 0xD4B8U},
            .logical_address = static_cast<std::uint16_t>(
                0x3C80U + entry.registers.x),
            .value = release.orientation_after,
            .master_cycle_offset = 36U,
        },
        {
            .instruction = {0x01U, 0xD4BDU},
            .logical_address = static_cast<std::uint16_t>(
                0x3A00U + entry.registers.x),
            .value = 0x01U,
            .master_cycle_offset = 57U,
        },
        {
            .instruction = {0x01U, 0xD4C0U},
            .logical_address = static_cast<std::uint16_t>(
                0x3E00U + entry.registers.x),
            .value = 0x01U,
            .master_cycle_offset = 72U,
        },
        {
            .instruction = {0x01U, 0xD4C3U},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + stack_high),
            .value = 0xD4U,
            .master_cycle_offset = 72U,
        },
        {
            .instruction = {0x01U, 0xD4C3U},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + stack_low),
            .value = 0xC5U,
            .master_cycle_offset = 72U,
        },
        {
            .instruction = {0x01U, 0xC5CAU},
            .logical_address = 0x2026U,
            .value = 0xC5U,
            .master_cycle_offset = 117U,
        },
        {
            .instruction = {0x01U, 0xC5CDU},
            .logical_address = 0x2027U,
            .value = 0xD4U,
            .master_cycle_offset = 141U,
        },
    };
    if (release.decrements_countdown != 0U) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xC5D4U},
            .logical_address = static_cast<std::uint16_t>(
                0x3D80U + entry.registers.x),
            .value = release.coroutine_countdown_after,
            .master_cycle_offset = 183U,
        });
    }
    if (release.installs_dispatch != 0U) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xC5D9U},
            .logical_address = 0x2026U,
            .value = release.dispatch_low,
            .master_cycle_offset = scheduler_increment_cycle,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xC5E1U},
            .logical_address = static_cast<std::uint16_t>(
                0x3F00U + entry.registers.x),
            .value = release.dispatch_low,
            .master_cycle_offset = scheduler_increment_cycle + 39U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xC5E6U},
            .logical_address = static_cast<std::uint16_t>(
                0x3F80U + entry.registers.x),
            .value = release.dispatch_high,
            .master_cycle_offset = scheduler_increment_cycle + 66U,
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
                .interrupt_sample_master_cycle_offset = master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x01U, instruction_pc},
                .registers = registers,
            });
        };

    auto checkpoint = entry.registers;
    checkpoint.accumulator = release.orientation_before;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xD4B3U, 0xD4B6U, 15U, checkpoint);
    checkpoint.accumulator = release.orientation_after;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xD4B6U, 0xD4B8U, 21U, checkpoint);
    append_checkpoint(0xD4B8U, 0xD4BBU, 36U, checkpoint);

    checkpoint.accumulator = 0x01U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xD4BBU, 0xD4BDU, 42U, checkpoint);
    append_checkpoint(0xD4BDU, 0xD4C0U, 57U, checkpoint);
    append_checkpoint(0xD4C0U, 0xD4C3U, 72U, checkpoint);

    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(0xD4C3U, 0xC5C9U, 93U, checkpoint);

    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer + 1U);
    checkpoint.accumulator = 0xC5U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC5C9U, 0xC5CAU, 105U, checkpoint);
    append_checkpoint(0xC5CAU, 0xC5CCU, 117U, checkpoint);

    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer + 1U);
    checkpoint.accumulator = 0xD4U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC5CCU, 0xC5CDU, 129U, checkpoint);
    append_checkpoint(0xC5CDU, 0xC5CFU, 141U, checkpoint);

    checkpoint.accumulator = release.coroutine_countdown;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC5CFU, 0xC5D2U, 156U, checkpoint);
    if (release.decrements_countdown == 0U) {
        append_checkpoint(0xC5D2U, 0xC5D9U, 168U, checkpoint);
    } else {
        append_checkpoint(0xC5D2U, 0xC5D4U, 162U, checkpoint);
        checkpoint.status =
            set_nz(checkpoint.status, release.coroutine_countdown_after);
        append_checkpoint(0xC5D4U, 0xC5D7U, 183U, checkpoint);
        if (release.installs_dispatch == 0U) {
            append_checkpoint(0xC5D7U, 0xC5E9U, 195U, output);
            absorb_return(
                replacement,
                work_ram,
                entry.registers.stack_pointer,
                {0x01U, 0xC5E9U},
                body_cycles,
                output);
            return replacement;
        }
        append_checkpoint(0xC5D7U, 0xC5D9U, 189U, checkpoint);
    }

    checkpoint.status = set_nz(checkpoint.status, release.dispatch_low);
    append_checkpoint(
        0xC5D9U, 0xC5DBU, scheduler_increment_cycle, checkpoint);
    append_checkpoint(
        0xC5DBU, 0xC5DFU, scheduler_increment_cycle + 12U, checkpoint);

    checkpoint.accumulator = release.dispatch_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC5DFU, 0xC5E1U, scheduler_increment_cycle + 24U, checkpoint);
    append_checkpoint(
        0xC5E1U, 0xC5E4U, scheduler_increment_cycle + 39U, checkpoint);

    checkpoint.accumulator = release.dispatch_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC5E4U, 0xC5E6U, scheduler_increment_cycle + 51U, checkpoint);
    append_checkpoint(
        0xC5E6U, 0xC5E9U, scheduler_increment_cycle + 66U, output);

    absorb_return(
        replacement,
        work_ram,
        entry.registers.stack_pointer,
        {0x01U, 0xC5E9U},
        body_cycles,
        output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_flipper_active_update(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr std::uint8_t kBank = 0x01U;
    const auto pc = entry.routine.logical_pc;
    if (entry.routine.physical_bank != kBank
        || (pc != 0xD48EU && pc != 0xD491U && pc != 0xD494U
            && pc != 0xD4C6U && pc != 0xD4C9U && pc != 0xD4CCU)) {
        return std::nullopt;
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
    const auto append_checkpoint = [](
                                       RoutineReplacement& replacement,
                                       const std::uint16_t instruction_pc,
                                       const std::uint16_t resume_pc,
                                       const std::uint32_t cycle,
                                       auto registers) {
        registers.program_counter = resume_pc;
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = cycle - 3U,
            .master_cycle_offset = cycle,
            .instruction = {kBank, instruction_pc},
            .registers = registers,
        });
    };

    // D48E/D491 and D4C6/D4C9 are deliberately crossed calls. D500 and the
    // CBAB collision route already have admitted native owners, while CBAB's
    // full canonical scope is too large for one transaction. Staging an
    // ordinary JSR frame lets those owners run and return to the next seam.
    if (pc == 0xD48EU || pc == 0xD491U
        || pc == 0xD4C6U || pc == 0xD4C9U) {
        const auto target = static_cast<std::uint16_t>(
            pc == 0xD48EU || pc == 0xD4C6U ? 0xD500U : 0xCBABU);
        const auto pushed = static_cast<std::uint16_t>(pc + 2U);
        const auto stack_high = entry.registers.stack_pointer;
        const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
        auto output = entry.registers;
        output.program_counter = target;
        output.stack_pointer = static_cast<std::uint8_t>(stack_high - 2U);

        RoutineReplacement replacement{
            .return_instruction = {kBank, target},
            .output_registers = output,
            .body_master_cycles = 21U,
        };
        replacement.work_ram_writes = {
            {
                .instruction = {kBank, pc},
                .logical_address = static_cast<std::uint16_t>(
                    0x2100U + stack_high),
                .value = static_cast<std::uint8_t>(pushed >> 8U),
                .master_cycle_offset = 0U,
            },
            {
                .instruction = {kBank, pc},
                .logical_address = static_cast<std::uint16_t>(
                    0x2100U + stack_low),
                .value = static_cast<std::uint8_t>(pushed & 0x00FFU),
                .master_cycle_offset = 0U,
            },
        };
        append_checkpoint(replacement, pc, target, 21U, output);
        return replacement;
    }

    if (pc == 0xD494U) {
        dc_flipper_active_head head{};
        if (!dc_classify_flipper_active_head(
                reinterpret_cast<const std::uint8_t*>(work_ram.data()),
                work_ram.size(),
                entry.registers.x,
                &head)) {
            throw std::runtime_error(
                "Native D494 replacement rejected the work-RAM snapshot.");
        }

        RoutineReplacement replacement{};
        auto checkpoint = entry.registers;
        checkpoint.accumulator = head.phase;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0xD494U, 0xD497U, 15U, checkpoint);

        const auto compare_result = static_cast<std::uint8_t>(
            head.phase - 3U);
        checkpoint.status = static_cast<std::uint8_t>(
            checkpoint.status & static_cast<std::uint8_t>(~0x83U));
        if (compare_result == 0U) {
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status | 0x02U);
        } else if ((compare_result & 0x80U) != 0U) {
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status | 0x80U);
        }
        if (head.phase >= 3U) {
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status | 0x01U);
        }
        append_checkpoint(replacement, 0xD497U, 0xD499U, 21U, checkpoint);

        if (head.path == DC_FLIPPER_ACTIVE_HEAD_ENDPOINT) {
            checkpoint.program_counter = 0xD4A4U;
            append_checkpoint(replacement, 0xD499U, 0xD4A4U, 33U, checkpoint);
            replacement.return_instruction = {kBank, 0xD4A4U};
            replacement.output_registers = checkpoint;
            replacement.body_master_cycles = 33U;
            return replacement;
        }

        append_checkpoint(replacement, 0xD499U, 0xD49BU, 27U, checkpoint);
        checkpoint.accumulator = head.global_input;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0xD49BU, 0xD49EU, 42U, checkpoint);
        checkpoint.accumulator = head.matched_input;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0xD49EU, 0xD4A1U, 57U, checkpoint);

        if (head.path == DC_FLIPPER_ACTIVE_HEAD_RELEASE) {
            checkpoint.program_counter = 0xD4B3U;
            append_checkpoint(replacement, 0xD4A1U, 0xD4B3U, 69U, checkpoint);
            replacement.return_instruction = {kBank, 0xD4B3U};
            replacement.output_registers = checkpoint;
            replacement.body_master_cycles = 69U;
            return replacement;
        }

        checkpoint.program_counter = 0xD4A3U;
        append_checkpoint(replacement, 0xD4A1U, 0xD4A3U, 63U, checkpoint);
        replacement.return_instruction = {kBank, 0xD4A3U};
        replacement.output_registers = checkpoint;
        replacement.body_master_cycles = 63U;
        absorb_return(
            replacement,
            work_ram,
            entry.registers.stack_pointer,
            {kBank, 0xD4A3U},
            63U,
            checkpoint);
        return replacement;
    }

    dc_flipper_active_tail tail{};
    if (!dc_classify_flipper_active_tail(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &tail)) {
        throw std::runtime_error(
            "Native D4CC replacement rejected the work-RAM snapshot.");
    }

    RoutineReplacement replacement{};
    auto checkpoint = entry.registers;
    checkpoint.accumulator = tail.phase;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0xD4CCU, 0xD4CFU, 15U, checkpoint);

    if (tail.path == DC_FLIPPER_ACTIVE_TAIL_RELEASE) {
        append_checkpoint(replacement, 0xD4CFU, 0xD4D1U, 21U, checkpoint);
        checkpoint.program_counter = 0xD458U;
        append_checkpoint(replacement, 0xD4D1U, 0xD458U, 33U, checkpoint);
        replacement.return_instruction = {kBank, 0xD458U};
        replacement.output_registers = checkpoint;
        replacement.body_master_cycles = 33U;
        return replacement;
    }

    append_checkpoint(replacement, 0xD4CFU, 0xD4D4U, 27U, checkpoint);
    checkpoint.accumulator = tail.global_input;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0xD4D4U, 0xD4D7U, 42U, checkpoint);
    checkpoint.accumulator = tail.matched_input;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0xD4D7U, 0xD4DAU, 57U, checkpoint);

    if (tail.path == DC_FLIPPER_ACTIVE_TAIL_IDLE_RETURN) {
        checkpoint.program_counter = 0xD4DCU;
        append_checkpoint(replacement, 0xD4DAU, 0xD4DCU, 63U, checkpoint);
        replacement.return_instruction = {kBank, 0xD4DCU};
        replacement.output_registers = checkpoint;
        replacement.body_master_cycles = 63U;
        absorb_return(
            replacement,
            work_ram,
            entry.registers.stack_pointer,
            {kBank, 0xD4DCU},
            63U,
            checkpoint);
        return replacement;
    }

    append_checkpoint(replacement, 0xD4DAU, 0xD4DDU, 69U, checkpoint);
    checkpoint.accumulator = tail.state_before;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0xD4DDU, 0xD4E0U, 84U, checkpoint);
    checkpoint.accumulator = tail.state_after;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0xD4E0U, 0xD4E2U, 90U, checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {kBank, 0xD4E2U},
        .logical_address = static_cast<std::uint16_t>(
            0x3C80U + entry.registers.x),
        .value = tail.state_after,
        .master_cycle_offset = 105U,
    });
    append_checkpoint(replacement, 0xD4E2U, 0xD4E5U, 105U, checkpoint);
    checkpoint.program_counter = 0xD46FU;
    append_checkpoint(replacement, 0xD4E5U, 0xD46FU, 117U, checkpoint);
    replacement.return_instruction = {kBank, 0xD46FU};
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = 117U;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_flipper_phase_step(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xD500U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_flipper_phase_step step{};
    if (!dc_advance_flipper_phase(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &step)) {
        throw std::runtime_error(
            "Native D500 replacement rejected the work-RAM snapshot.");
    }
    if (step.supported == 0U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xD524U;
    output.accumulator = step.phase;
    output.status = static_cast<std::uint8_t>(
        output.status & static_cast<std::uint8_t>(~0x82U));
    if (step.decrements_phase == 0U) {
        output.status = static_cast<std::uint8_t>(
            output.status & static_cast<std::uint8_t>(~0x01U));
        if (step.compare_carry != 0U) {
            output.status = static_cast<std::uint8_t>(
                output.status | 0x01U);
        }
    }
    if (step.phase_next == 0U) {
        output.status = static_cast<std::uint8_t>(
            output.status | 0x02U);
    } else if ((step.phase_next & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(
            output.status | 0x80U);
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xD524U,
        },
        .output_registers = output,
        .body_master_cycles =
            step.decrements_phase != 0U ? 126U : 138U,
    };
    replacement.work_ram_writes = {
        {
            .instruction = {0x01U, 0xD500U},
            .logical_address = static_cast<std::uint16_t>(
                0x3A00U + entry.registers.x),
            .value = 0U,
            .master_cycle_offset = 21U,
        },
        {
            .instruction = {0x01U, 0xD508U},
            .logical_address = static_cast<std::uint16_t>(
                0x3A00U + entry.registers.x),
            .value = step.reload,
            .master_cycle_offset = 57U,
        },
        {
            .instruction = {
                0x01U,
                static_cast<std::uint16_t>(
                    step.decrements_phase != 0U
                        ? 0xD521U
                        : 0xD517U),
            },
            .logical_address = static_cast<std::uint16_t>(
                0x3980U + entry.registers.x),
            .value = step.phase_next,
            .master_cycle_offset = 126U,
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

    auto checkpoint = entry.registers;
    checkpoint.status = set_nz(checkpoint.status, 0U);
    append_checkpoint(0xD500U, 0xD503U, 21U, checkpoint);
    append_checkpoint(0xD503U, 0xD505U, 27U, checkpoint);

    checkpoint.accumulator = step.reload;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xD505U, 0xD508U, 42U, checkpoint);
    append_checkpoint(0xD508U, 0xD50BU, 57U, checkpoint);

    checkpoint.accumulator = step.orientation;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xD50BU, 0xD50EU, 72U, checkpoint);
    if (step.decrements_phase != 0U) {
        append_checkpoint(0xD50EU, 0xD51CU, 84U, checkpoint);

        checkpoint.accumulator = step.phase;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xD51CU, 0xD51FU, 99U, checkpoint);
        append_checkpoint(0xD51FU, 0xD521U, 105U, checkpoint);
        append_checkpoint(0xD521U, 0xD524U, 126U, output);
        absorb_return(
            replacement,
            work_ram,
            entry.registers.stack_pointer,
            {0x01U, 0xD524U},
            126U,
            output);
        return replacement;
    }
    append_checkpoint(0xD50EU, 0xD510U, 78U, checkpoint);

    checkpoint.accumulator = step.phase;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xD510U, 0xD513U, 93U, checkpoint);

    const auto compare_result =
        static_cast<std::uint8_t>(step.phase - 3U);
    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status & static_cast<std::uint8_t>(~0xA3U));
    if (compare_result == 0U) {
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status | 0x02U);
    } else if ((compare_result & 0x80U) != 0U) {
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status | 0x80U);
    }
    if (step.compare_carry != 0U) {
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status | 0x01U);
    }
    append_checkpoint(0xD513U, 0xD515U, 99U, checkpoint);
    append_checkpoint(0xD515U, 0xD517U, 105U, checkpoint);
    append_checkpoint(0xD517U, 0xD51AU, 126U, output);
    append_checkpoint(0xD51AU, 0xD524U, 138U, output);
    absorb_return(
        replacement,
        work_ram,
        entry.registers.stack_pointer,
        {0x01U, 0xD524U},
        138U,
        output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_flipper_geometry_lookup(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xD081U,
    };
    constexpr std::size_t kGeometryTableOffset =
        0x01U * 0x2000U + (0xD2AEU & 0x1FFFU);
    constexpr std::size_t kGeometryTableSize = 0x80U;
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    if (work_ram.size() < 0x2000U
        || kGeometryTableOffset + kGeometryTableSize > rom.size()) {
        throw std::runtime_error(
            "Native D081 replacement rejected its memory snapshots.");
    }

    dc_flipper_geometry geometry{};
    if (!dc_lookup_flipper_geometry(
            std::to_integer<std::uint8_t>(work_ram[0x0078U]),
            reinterpret_cast<const std::uint8_t*>(
                rom.data() + kGeometryTableOffset),
            kGeometryTableSize,
            &geometry)) {
        throw std::runtime_error(
            "Native D081 replacement rejected the geometry table.");
    }

    auto output = entry.registers;
    output.program_counter = 0xD09DU;
    output.accumulator = geometry.second_high;
    output.status = static_cast<std::uint8_t>(
        output.status & static_cast<std::uint8_t>(~0xA3U));
    if (entry.registers.y == 0U) {
        output.status = static_cast<std::uint8_t>(
            output.status | 0x02U);
    } else if ((entry.registers.y & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(
            output.status | 0x80U);
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xD09DU,
        },
        .output_registers = output,
        .body_master_cycles = 159U,
    };
    replacement.work_ram_writes = {
        {
            .instruction = {0x01U, 0xD085U},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + entry.registers.stack_pointer),
            .value = entry.registers.y,
            .master_cycle_offset = 27U,
        },
        {
            .instruction = {0x01U, 0xD08BU},
            .logical_address = 0x207FU,
            .value = geometry.first_low,
            .master_cycle_offset = 66U,
        },
        {
            .instruction = {0x01U, 0xD090U},
            .logical_address = 0x2080U,
            .value = geometry.first_high,
            .master_cycle_offset = 93U,
        },
        {
            .instruction = {0x01U, 0xD095U},
            .logical_address = 0x207DU,
            .value = geometry.second_low,
            .master_cycle_offset = 120U,
        },
        {
            .instruction = {0x01U, 0xD09AU},
            .logical_address = 0x207EU,
            .value = geometry.second_high,
            .master_cycle_offset = 147U,
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

    auto checkpoint = entry.registers;
    checkpoint.accumulator = geometry.state;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xD081U, 0xD083U, 12U, checkpoint);

    checkpoint.accumulator =
        static_cast<std::uint8_t>(checkpoint.accumulator & 0x1FU);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xD083U, 0xD085U, 18U, checkpoint);

    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer - 1U);
    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status & ~0x20U);
    append_checkpoint(0xD085U, 0xD086U, 27U, checkpoint);

    checkpoint.accumulator = geometry.table_index;
    checkpoint.status = set_nz(
        static_cast<std::uint8_t>(checkpoint.status & ~0x01U),
        checkpoint.accumulator);
    append_checkpoint(0xD086U, 0xD087U, 33U, checkpoint);

    checkpoint.y = checkpoint.accumulator;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0xD087U, 0xD088U, 39U, checkpoint);

    checkpoint.accumulator = geometry.first_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xD088U, 0xD08BU, 54U, checkpoint);
    append_checkpoint(0xD08BU, 0xD08DU, 66U, checkpoint);

    checkpoint.accumulator = geometry.first_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xD08DU, 0xD090U, 81U, checkpoint);
    append_checkpoint(0xD090U, 0xD092U, 93U, checkpoint);

    checkpoint.accumulator = geometry.second_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xD092U, 0xD095U, 108U, checkpoint);
    append_checkpoint(0xD095U, 0xD097U, 120U, checkpoint);

    checkpoint.accumulator = geometry.second_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xD097U, 0xD09AU, 135U, checkpoint);
    append_checkpoint(0xD09AU, 0xD09CU, 147U, checkpoint);
    append_checkpoint(0xD09CU, 0xD09DU, 159U, output);

    // 01:D085 pushed Y and 01:D09C pulled it back, so the pointer at the return
    // is the entry's and the byte the push left at $2100+S sits one below the
    // frame the return reads.
    absorb_return(
        replacement,
        work_ram,
        entry.registers.stack_pointer,
        {0x01U, 0xD09DU},
        159U,
        output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_coroutine_scheduler(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xC5C9U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_coroutine_schedule schedule{};
    if (!dc_schedule_coroutine(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            entry.registers.stack_pointer,
            &schedule)) {
        throw std::runtime_error(
            "Native C5C9 replacement rejected the work-RAM snapshot.");
    }

    const auto countdown_offset = static_cast<std::uint16_t>(
        0x3D80U + entry.registers.x);
    const auto countdown_path_offset =
        schedule.decrements_countdown != 0U ? 21U : 0U;
    const auto carry_path_offset =
        schedule.dispatch_low_wrapped != 0U ? 12U : 0U;

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.stack_pointer =
        static_cast<std::uint8_t>(output.stack_pointer + 2U);
    output.accumulator = schedule.installs_dispatch != 0U
        ? schedule.dispatch_high
        : schedule.countdown;
    output.status = static_cast<std::uint8_t>(
        output.status & static_cast<std::uint8_t>(~0xA2U));
    const auto flag_value = schedule.installs_dispatch != 0U
        ? schedule.dispatch_high
        : schedule.countdown_after;
    if (flag_value == 0U) {
        output.status = static_cast<std::uint8_t>(
            output.status | 0x02U);
    } else if ((flag_value & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(
            output.status | 0x80U);
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xC5E9U,
        },
        .output_registers = output,
        .body_master_cycles = 102U,
    };
    replacement.work_ram_writes = {
        {
            .instruction = {0x01U, 0xC5CAU},
            .logical_address = 0x2026U,
            .value = schedule.stacked_return_low,
            .master_cycle_offset = 24U,
        },
        {
            .instruction = {0x01U, 0xC5CDU},
            .logical_address = 0x2027U,
            .value = schedule.stacked_return_high,
            .master_cycle_offset = 48U,
        },
    };
    if (schedule.decrements_countdown != 0U) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xC5D4U},
            .logical_address = countdown_offset,
            .value = schedule.countdown_after,
            .master_cycle_offset = 90U,
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
                .instruction = {0x01U, instruction_pc},
                .registers = registers,
            });
        };

    auto checkpoint = entry.registers;
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer + 1U);
    checkpoint.accumulator = schedule.stacked_return_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC5C9U, 0xC5CAU, 12U, checkpoint);
    append_checkpoint(0xC5CAU, 0xC5CCU, 24U, checkpoint);

    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer + 1U);
    checkpoint.accumulator = schedule.stacked_return_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC5CCU, 0xC5CDU, 36U, checkpoint);
    append_checkpoint(0xC5CDU, 0xC5CFU, 48U, checkpoint);

    checkpoint.accumulator = schedule.countdown;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC5CFU, 0xC5D2U, 63U, checkpoint);

    if (schedule.decrements_countdown != 0U) {
        append_checkpoint(0xC5D2U, 0xC5D4U, 69U, checkpoint);
        checkpoint.status =
            set_nz(checkpoint.status, schedule.countdown_after);
        append_checkpoint(0xC5D4U, 0xC5D7U, 90U, checkpoint);
        if (schedule.installs_dispatch == 0U) {
            append_checkpoint(0xC5D7U, 0xC5E9U, 102U, output);
            return replacement;
        }
        append_checkpoint(0xC5D7U, 0xC5D9U, 96U, checkpoint);
    } else {
        append_checkpoint(0xC5D2U, 0xC5D9U, 75U, checkpoint);
    }

    const auto increment_low_cycle =
        static_cast<std::uint32_t>(93U + countdown_path_offset);
    replacement.work_ram_writes.push_back({
        .instruction = {0x01U, 0xC5D9U},
        .logical_address = 0x2026U,
        .value = schedule.dispatch_low,
        .master_cycle_offset = increment_low_cycle,
    });
    if (schedule.dispatch_low_wrapped != 0U) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xC5DDU},
            .logical_address = 0x2027U,
            .value = schedule.dispatch_high,
            .master_cycle_offset =
                static_cast<std::uint32_t>(
                    117U + countdown_path_offset),
        });
    }
    replacement.work_ram_writes.push_back({
        .instruction = {0x01U, 0xC5E1U},
        .logical_address = static_cast<std::uint16_t>(
            0x3F00U + entry.registers.x),
        .value = schedule.dispatch_low,
        .master_cycle_offset =
            static_cast<std::uint32_t>(
                132U + countdown_path_offset + carry_path_offset),
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x01U, 0xC5E6U},
        .logical_address = static_cast<std::uint16_t>(
            0x3F80U + entry.registers.x),
        .value = schedule.dispatch_high,
        .master_cycle_offset =
            static_cast<std::uint32_t>(
                159U + countdown_path_offset + carry_path_offset),
    });
    replacement.body_master_cycles =
        replacement.work_ram_writes.back().master_cycle_offset;

    checkpoint.status =
        set_nz(checkpoint.status, schedule.dispatch_low);
    append_checkpoint(
        0xC5D9U,
        0xC5DBU,
        increment_low_cycle,
        checkpoint);

    const auto branch_cycle = static_cast<std::uint32_t>(
        increment_low_cycle
        + (schedule.dispatch_low_wrapped != 0U ? 6U : 12U));
    if (schedule.dispatch_low_wrapped != 0U) {
        append_checkpoint(0xC5DBU, 0xC5DDU, branch_cycle, checkpoint);
        checkpoint.status =
            set_nz(checkpoint.status, schedule.dispatch_high);
        append_checkpoint(
            0xC5DDU,
            0xC5DFU,
            branch_cycle + 18U,
            checkpoint);
    } else {
        append_checkpoint(0xC5DBU, 0xC5DFU, branch_cycle, checkpoint);
    }

    const auto load_low_cycle = static_cast<std::uint32_t>(
        117U + countdown_path_offset + carry_path_offset);
    checkpoint.accumulator = schedule.dispatch_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC5DFU,
        0xC5E1U,
        load_low_cycle,
        checkpoint);
    append_checkpoint(
        0xC5E1U,
        0xC5E4U,
        load_low_cycle + 15U,
        checkpoint);

    checkpoint.accumulator = schedule.dispatch_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC5E4U,
        0xC5E6U,
        load_low_cycle + 27U,
        checkpoint);
    append_checkpoint(
        0xC5E6U,
        0xC5E9U,
        replacement.body_master_cycles,
        output);
    return replacement;
}

// What the registry dispatches at 01:C5C9. The scheduler above is run in line
// by twenty-seven walks in native_replacements_rules.cpp, each of which takes
// its `return_instruction` and its `body_master_cycles` for its own and several
// of which test that the body is exactly 159 cycles, so the routine itself has
// to stop in front of its `RTS` and report the address it stopped on. The entry
// the core takes is the one arrival nothing else is reading, so that is where
// the return is absorbed. Nothing composes this wrapper.
std::optional<RoutineReplacement> replace_coroutine_scheduler_entry(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    auto replacement = replace_coroutine_scheduler(entry, work_ram);
    if (!replacement || work_ram.size() < 0x2000U) {
        return replacement;
    }

    // The scheduler's first two instructions are the `PLA`s that take its own
    // return address off the stack, so the pointer it ends on is two above the
    // one it was entered with and the frame its `RTS` reads is its caller's
    // caller's. That is the pointer the return has to declare and the one the
    // word is read above -- the entry's would name the frame those `PLA`s have
    // already consumed.
    if (replacement->return_instruction.physical_bank != 0x01U
        || replacement->return_instruction.logical_pc != 0xC5E9U
        || replacement->output_registers.stack_pointer
            != static_cast<std::uint8_t>(
                entry.registers.stack_pointer + 2U)) {
        return replacement;
    }
    if (replacement->checkpoints.empty()
        && !absorbed_return_fits_atomically(
            entry, replacement->body_master_cycles)) {
        return replacement;
    }

    const auto return_cycle = replacement->body_master_cycles;
    const auto at_return = replacement->output_registers;
    absorb_return(
        *replacement,
        work_ram,
        at_return.stack_pointer,
        {0x01U, 0xC5E9U},
        return_cycle,
        at_return);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_active_nudge(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const dc_nudge_input_gate& gate)
{
    const bool active_start =
        gate.branch == DC_NUDGE_GATE_ACTIVE_START;
    const bool active_finish =
        gate.branch == DC_NUDGE_GATE_ACTIVE_FINISH;
    if ((!active_start && !active_finish)
        || (entry.registers.status & 0x28U) != 0U
        || entry.registers.mpr[0] != 0xFFU) {
        return std::nullopt;
    }

    const auto read = [&work_ram](const std::size_t offset) {
        return std::to_integer<std::uint8_t>(work_ram[offset]);
    };
    const auto original_counter = read(0x0CC5U);
    const auto counter_after =
        static_cast<std::uint8_t>(original_counter + 1U);
    const std::uint8_t vector_low =
        active_start ? 0U : read(0x0CC2U);
    const std::uint8_t vector_high =
        active_start ? 1U : read(0x0CC3U);
    const auto scratch_high = static_cast<std::uint8_t>(
        (vector_high >> 1U)
        | ((vector_high & 0x80U) != 0U ? 0x80U : 0U));
    const auto scratch_low = static_cast<std::uint8_t>(
        (vector_low >> 1U)
        | ((vector_high & 0x01U) != 0U ? 0x80U : 0U));
    const auto motion_low = read(0x1500U);
    const auto motion_high = read(0x1580U);
    const auto motion_low_sum = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(motion_low) + scratch_low);
    const bool motion_carry = motion_low_sum > 0xFFU;
    const auto motion_low_after =
        static_cast<std::uint8_t>(motion_low_sum & 0xFFU);
    const auto motion_high_after = static_cast<std::uint8_t>(
        static_cast<std::uint16_t>(motion_high)
        + static_cast<std::uint16_t>(scratch_high)
        + static_cast<std::uint16_t>(motion_carry));

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc =
                active_start ? 0xC43EU : 0xC456U,
        },
        .output_registers = entry.registers,
        .body_master_cycles =
            active_start ? 762U : 522U,
    };
    const auto append_write =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t logical_address,
            const std::uint8_t value,
            const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x01U, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
        };

    append_write(0xC3DDU, 0x2CC6U, gate.timer_after, 96U);
    if (active_start) {
        append_write(0xC3FFU, 0x2CC5U, counter_after, 222U);
        append_write(0xC40EU, 0x2CC2U, 0x00U, 276U);
        append_write(0xC413U, 0x2CC3U, 0x01U, 297U);
        append_write(0xC419U, 0x0000U, 0x0CU, 327U);
        append_write(0xC41EU, 0x0002U, 0x0FU, 351U);
        append_write(0xC423U, 0x0003U, 0x04U, 375U);
        append_write(
            0xC427U,
            static_cast<std::uint16_t>(
                0x2100U + entry.registers.stack_pointer),
            0xC4U,
            381U);
        append_write(
            0xC427U,
            static_cast<std::uint16_t>(
                0x2100U
                + static_cast<std::uint8_t>(
                    entry.registers.stack_pointer - 1U)),
            0x29U,
            381U);
        append_write(0xC466U, 0x202FU, vector_low, 477U);
        append_write(0xC46BU, 0x2030U, vector_high, 504U);
        append_write(0xC46EU, 0x2030U, scratch_high, 528U);
        append_write(0xC470U, 0x202FU, scratch_low, 546U);
        append_write(0xC479U, 0x3500U, motion_low_after, 600U);
        append_write(0xC481U, 0x3580U, motion_high_after, 642U);
        append_write(0xC42CU, 0x2CC4U, 0x01U, 684U);
        append_write(0xC434U, 0x2CC2U, 0x00U, 726U);
        append_write(0xC43BU, 0x2CC3U, 0xFFU, 762U);
    } else {
        append_write(0xC442U, 0x0000U, 0x0CU, 171U);
        append_write(0xC447U, 0x0002U, 0x0FU, 195U);
        append_write(0xC44CU, 0x0003U, 0x02U, 219U);
        append_write(
            0xC450U,
            static_cast<std::uint16_t>(
                0x2100U + entry.registers.stack_pointer),
            0xC4U,
            225U);
        append_write(
            0xC450U,
            static_cast<std::uint16_t>(
                0x2100U
                + static_cast<std::uint8_t>(
                    entry.registers.stack_pointer - 1U)),
            0x52U,
            225U);
        append_write(0xC466U, 0x202FU, vector_low, 321U);
        append_write(0xC46BU, 0x2030U, vector_high, 348U);
        append_write(0xC46EU, 0x2030U, scratch_high, 372U);
        append_write(0xC470U, 0x202FU, scratch_low, 390U);
        append_write(0xC479U, 0x3500U, motion_low_after, 444U);
        append_write(0xC481U, 0x3580U, motion_high_after, 486U);
        append_write(0xC453U, 0x2CC4U, 0x00U, 522U);
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
    const auto compare_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right) {
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0x01U));
            status = set_nz(
                status,
                static_cast<std::uint8_t>(left - right));
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
    const auto shift_left_status =
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
    const auto rotate_right_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t original,
            const std::uint8_t result) {
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0x01U));
            status = set_nz(status, result);
            if ((original & 0x01U) != 0U) {
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
                .instruction = {0x01U, instruction_pc},
                .registers = registers,
            });
        };

    auto checkpoint = entry.registers;
    checkpoint.accumulator = gate.session_flags;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC3CBU, 0xC3CDU, 12U, checkpoint);
    checkpoint.accumulator =
        static_cast<std::uint8_t>(gate.session_flags & 0x02U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC3CDU, 0xC3CFU, 18U, checkpoint);
    append_checkpoint(0xC3CFU, 0xC3D1U, 24U, checkpoint);
    checkpoint.accumulator = gate.tilt_lock;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC3D1U, 0xC3D4U, 39U, checkpoint);
    append_checkpoint(0xC3D4U, 0xC3D6U, 45U, checkpoint);
    checkpoint.accumulator = gate.session_flags;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC3D6U, 0xC3D8U, 57U, checkpoint);
    checkpoint.accumulator = gate.session_mask;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC3D8U, 0xC3DAU, 63U, checkpoint);
    append_checkpoint(0xC3DAU, 0xC3DDU, 75U, checkpoint);
    checkpoint.status = set_nz(checkpoint.status, gate.timer_after);
    append_checkpoint(0xC3DDU, 0xC3E0U, 96U, checkpoint);
    append_checkpoint(0xC3E0U, 0xC3EAU, 108U, checkpoint);
    checkpoint.accumulator = gate.nudge_phase;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC3EAU, 0xC3EDU, 123U, checkpoint);
    checkpoint.accumulator =
        static_cast<std::uint8_t>(gate.nudge_phase - 1U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC3EDU, 0xC3EEU, 129U, checkpoint);

    if (active_finish) {
        append_checkpoint(0xC3EEU, 0xC43FU, 141U, checkpoint);
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status | 0x04U);
        append_checkpoint(0xC43FU, 0xC440U, 147U, checkpoint);
        checkpoint.accumulator = 0x0CU;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC440U, 0xC442U, 153U, checkpoint);
        append_checkpoint(0xC442U, 0xC445U, 171U, checkpoint);
        checkpoint.accumulator = 0x0FU;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC445U, 0xC447U, 177U, checkpoint);
        append_checkpoint(0xC447U, 0xC44AU, 195U, checkpoint);
        checkpoint.accumulator = 0x02U;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC44AU, 0xC44CU, 201U, checkpoint);
        append_checkpoint(0xC44CU, 0xC44FU, 219U, checkpoint);
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status & ~0x04U);
        append_checkpoint(0xC44FU, 0xC450U, 225U, checkpoint);
        checkpoint.stack_pointer = static_cast<std::uint8_t>(
            checkpoint.stack_pointer - 2U);
        append_checkpoint(0xC450U, 0xC457U, 246U, checkpoint);
    } else {
        append_checkpoint(0xC3EEU, 0xC3F0U, 135U, checkpoint);
        checkpoint.y = gate.active_player;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
        append_checkpoint(0xC3F0U, 0xC3F3U, 150U, checkpoint);
        checkpoint.accumulator = gate.button_input;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC3F3U, 0xC3F6U, 165U, checkpoint);
        checkpoint.accumulator = gate.matched_input;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC3F6U, 0xC3F8U, 171U, checkpoint);
        append_checkpoint(0xC3F8U, 0xC3FBU, 183U, checkpoint);

        checkpoint.accumulator = read(0x0055U);
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC3FBU, 0xC3FDU, 195U, checkpoint);
        append_checkpoint(0xC3FDU, 0xC3FFU, 201U, checkpoint);
        checkpoint.status = set_nz(checkpoint.status, counter_after);
        append_checkpoint(0xC3FFU, 0xC402U, 222U, checkpoint);
        checkpoint.accumulator = counter_after;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC402U, 0xC405U, 237U, checkpoint);
        checkpoint.status = compare_status(
            checkpoint.status,
            checkpoint.accumulator,
            0x14U);
        append_checkpoint(0xC405U, 0xC407U, 243U, checkpoint);
        append_checkpoint(0xC407U, 0xC40CU, 255U, checkpoint);
        checkpoint.accumulator = 0U;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC40CU, 0xC40EU, 261U, checkpoint);
        append_checkpoint(0xC40EU, 0xC411U, 276U, checkpoint);
        checkpoint.accumulator = 1U;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC411U, 0xC413U, 282U, checkpoint);
        append_checkpoint(0xC413U, 0xC416U, 297U, checkpoint);
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status | 0x04U);
        append_checkpoint(0xC416U, 0xC417U, 303U, checkpoint);
        checkpoint.accumulator = 0x0CU;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC417U, 0xC419U, 309U, checkpoint);
        append_checkpoint(0xC419U, 0xC41CU, 327U, checkpoint);
        checkpoint.accumulator = 0x0FU;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC41CU, 0xC41EU, 333U, checkpoint);
        append_checkpoint(0xC41EU, 0xC421U, 351U, checkpoint);
        checkpoint.accumulator = 0x04U;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC421U, 0xC423U, 357U, checkpoint);
        append_checkpoint(0xC423U, 0xC426U, 375U, checkpoint);
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status & ~0x04U);
        append_checkpoint(0xC426U, 0xC427U, 381U, checkpoint);
        checkpoint.stack_pointer = static_cast<std::uint8_t>(
            checkpoint.stack_pointer - 2U);
        append_checkpoint(0xC427U, 0xC457U, 402U, checkpoint);
    }

    const auto helper_offset = active_start ? 156U : 0U;
    checkpoint.accumulator = read(0x1000U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC457U,
        0xC45AU,
        261U + helper_offset,
        checkpoint);
    append_checkpoint(
        0xC45AU,
        0xC45CU,
        267U + helper_offset,
        checkpoint);
    checkpoint.accumulator = read(0x1C80U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC45CU,
        0xC45FU,
        282U + helper_offset,
        checkpoint);
    checkpoint.accumulator =
        static_cast<std::uint8_t>(checkpoint.accumulator & 0x01U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC45FU,
        0xC461U,
        288U + helper_offset,
        checkpoint);
    append_checkpoint(
        0xC461U,
        0xC463U,
        294U + helper_offset,
        checkpoint);
    checkpoint.accumulator = vector_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC463U,
        0xC466U,
        309U + helper_offset,
        checkpoint);
    append_checkpoint(
        0xC466U,
        0xC468U,
        321U + helper_offset,
        checkpoint);
    checkpoint.accumulator = vector_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC468U,
        0xC46BU,
        336U + helper_offset,
        checkpoint);
    append_checkpoint(
        0xC46BU,
        0xC46DU,
        348U + helper_offset,
        checkpoint);

    const auto shifted_high =
        static_cast<std::uint8_t>(vector_high << 1U);
    checkpoint.accumulator = shifted_high;
    checkpoint.status = shift_left_status(
        checkpoint.status,
        vector_high,
        checkpoint.accumulator);
    append_checkpoint(
        0xC46DU,
        0xC46EU,
        354U + helper_offset,
        checkpoint);
    checkpoint.status = rotate_right_status(
        checkpoint.status,
        vector_high,
        scratch_high);
    append_checkpoint(
        0xC46EU,
        0xC470U,
        372U + helper_offset,
        checkpoint);
    checkpoint.status = rotate_right_status(
        checkpoint.status,
        vector_low,
        scratch_low);
    append_checkpoint(
        0xC470U,
        0xC472U,
        390U + helper_offset,
        checkpoint);

    checkpoint.x = 0U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.x);
    append_checkpoint(
        0xC472U,
        0xC473U,
        396U + helper_offset,
        checkpoint);
    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status & ~0x01U);
    append_checkpoint(
        0xC473U,
        0xC474U,
        402U + helper_offset,
        checkpoint);
    checkpoint.accumulator = motion_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC474U,
        0xC477U,
        417U + helper_offset,
        checkpoint);
    checkpoint.accumulator = motion_low_after;
    checkpoint.status = adc_status(
        checkpoint.status,
        motion_low,
        scratch_low,
        false,
        checkpoint.accumulator);
    append_checkpoint(
        0xC477U,
        0xC479U,
        429U + helper_offset,
        checkpoint);
    append_checkpoint(
        0xC479U,
        0xC47CU,
        444U + helper_offset,
        checkpoint);
    checkpoint.accumulator = motion_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC47CU,
        0xC47FU,
        459U + helper_offset,
        checkpoint);
    checkpoint.accumulator = motion_high_after;
    checkpoint.status = adc_status(
        checkpoint.status,
        motion_high,
        scratch_high,
        motion_carry,
        checkpoint.accumulator);
    append_checkpoint(
        0xC47FU,
        0xC481U,
        471U + helper_offset,
        checkpoint);
    append_checkpoint(
        0xC481U,
        0xC484U,
        486U + helper_offset,
        checkpoint);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 2U);
    append_checkpoint(
        0xC484U,
        active_start ? 0xC42AU : 0xC453U,
        507U + helper_offset,
        checkpoint);

    if (active_finish) {
        append_checkpoint(0xC453U, 0xC456U, 522U, checkpoint);
    } else {
        checkpoint.accumulator = 1U;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC42AU, 0xC42CU, 669U, checkpoint);
        append_checkpoint(0xC42CU, 0xC42FU, 684U, checkpoint);
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status | 0x01U);
        append_checkpoint(0xC42FU, 0xC430U, 690U, checkpoint);
        checkpoint.accumulator = 0U;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC430U, 0xC431U, 696U, checkpoint);
        auto subtraction = static_cast<std::uint8_t>(
            checkpoint.accumulator - 0U);
        checkpoint.status = sbc_status(
            checkpoint.status,
            checkpoint.accumulator,
            0U,
            true,
            subtraction);
        checkpoint.accumulator = subtraction;
        append_checkpoint(0xC431U, 0xC434U, 711U, checkpoint);
        append_checkpoint(0xC434U, 0xC437U, 726U, checkpoint);
        checkpoint.accumulator = 0U;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC437U, 0xC438U, 732U, checkpoint);
        subtraction = 0xFFU;
        checkpoint.status = sbc_status(
            checkpoint.status,
            checkpoint.accumulator,
            1U,
            true,
            subtraction);
        checkpoint.accumulator = subtraction;
        append_checkpoint(0xC438U, 0xC43BU, 747U, checkpoint);
        append_checkpoint(0xC43BU, 0xC43EU, 762U, checkpoint);
    }

    checkpoint.program_counter =
        replacement.return_instruction.logical_pc;
    replacement.output_registers = checkpoint;
    replacement.checkpoints.back().registers = checkpoint;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_nudge_input_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xC3CBU,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_nudge_input_gate gate{};
    if (!dc_evaluate_nudge_input_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &gate)) {
        throw std::runtime_error(
            "Native C3CB replacement rejected the work-RAM snapshot.");
    }
    if (gate.branch == DC_NUDGE_GATE_DEFER) {
        return std::nullopt;
    }
    if (const auto active = replace_active_nudge(
            entry,
            work_ram,
            gate)) {
        return active;
    }

    auto output = entry.registers;
    output.status = static_cast<std::uint8_t>(
        output.status & static_cast<std::uint8_t>(~0x82U));
    const auto session_return =
        gate.branch == DC_NUDGE_GATE_SESSION_RETURN;
    const auto timer_reload_return =
        gate.branch == DC_NUDGE_GATE_TIMER_RELOAD_RETURN;
    if (gate.branch == DC_NUDGE_GATE_SESSION_RETURN) {
        output.program_counter = 0xC3DCU;
        output.accumulator = gate.session_mask;
        if ((output.accumulator & 0x80U) != 0U) {
            output.status = static_cast<std::uint8_t>(
                output.status | 0x80U);
        }
    } else {
        output.program_counter = 0xC3FAU;
        output.accumulator = gate.matched_input;
        output.y = gate.active_player;
        output.status = static_cast<std::uint8_t>(
            output.status | 0x02U);
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = session_return ? 0xC3DCU : 0xC3FAU,
        },
        .output_registers = output,
        .body_master_cycles =
            session_return ? 69U : (timer_reload_return ? 207U : 177U),
    };
    if (!session_return) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xC3DDU},
            .logical_address = 0x2CC6U,
            .value = gate.timer_after,
            .master_cycle_offset = 96U,
        });
        if (timer_reload_return) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x01U, 0xC3E4U},
                .logical_address = 0x2CC6U,
                .value = 0x78U,
                .master_cycle_offset = 123U,
            });
            replacement.work_ram_writes.push_back({
                .instruction = {0x01U, 0xC3E7U},
                .logical_address = 0x2CC5U,
                .value = 0x00U,
                .master_cycle_offset = 138U,
            });
        }
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
                .instruction = {0x01U, instruction_pc},
                .registers = registers,
            });
        };

    auto checkpoint = entry.registers;
    checkpoint.accumulator = gate.session_flags;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC3CBU, 0xC3CDU, 12U, checkpoint);

    checkpoint.accumulator =
        static_cast<std::uint8_t>(gate.session_flags & 0x02U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC3CDU, 0xC3CFU, 18U, checkpoint);
    append_checkpoint(0xC3CFU, 0xC3D1U, 24U, checkpoint);

    checkpoint.accumulator = gate.tilt_lock;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC3D1U, 0xC3D4U, 39U, checkpoint);
    append_checkpoint(0xC3D4U, 0xC3D6U, 45U, checkpoint);

    checkpoint.accumulator = gate.session_flags;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC3D6U, 0xC3D8U, 57U, checkpoint);

    checkpoint.accumulator = gate.session_mask;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC3D8U, 0xC3DAU, 63U, checkpoint);
    if (session_return) {
        append_checkpoint(0xC3DAU, 0xC3DCU, 69U, output);
        absorb_return(
            replacement,
            work_ram,
            entry.registers.stack_pointer,
            {0x01U, 0xC3DCU},
            69U,
            output);
        return replacement;
    }
    append_checkpoint(0xC3DAU, 0xC3DDU, 75U, checkpoint);

    checkpoint.status = set_nz(checkpoint.status, gate.timer_after);
    append_checkpoint(0xC3DDU, 0xC3E0U, 96U, checkpoint);
    if (timer_reload_return) {
        append_checkpoint(0xC3E0U, 0xC3E2U, 102U, checkpoint);
        checkpoint.accumulator = 0x78U;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC3E2U, 0xC3E4U, 108U, checkpoint);
        append_checkpoint(0xC3E4U, 0xC3E7U, 123U, checkpoint);
        append_checkpoint(0xC3E7U, 0xC3EAU, 138U, checkpoint);
    } else {
        append_checkpoint(0xC3E0U, 0xC3EAU, 108U, checkpoint);
    }

    const auto continuation_offset =
        timer_reload_return ? 30U : 0U;
    checkpoint.accumulator = gate.nudge_phase;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC3EAU,
        0xC3EDU,
        123U + continuation_offset,
        checkpoint);

    checkpoint.accumulator =
        static_cast<std::uint8_t>(gate.nudge_phase - 1U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC3EDU,
        0xC3EEU,
        129U + continuation_offset,
        checkpoint);
    append_checkpoint(
        0xC3EEU,
        0xC3F0U,
        135U + continuation_offset,
        checkpoint);

    checkpoint.y = gate.active_player;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(
        0xC3F0U,
        0xC3F3U,
        150U + continuation_offset,
        checkpoint);

    checkpoint.accumulator = gate.button_input;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC3F3U,
        0xC3F6U,
        165U + continuation_offset,
        checkpoint);

    checkpoint.accumulator = gate.matched_input;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC3F6U,
        0xC3F8U,
        171U + continuation_offset,
        checkpoint);
    append_checkpoint(
        0xC3F8U,
        0xC3FAU,
        177U + continuation_offset,
        output);
    absorb_return(
        replacement,
        work_ram,
        entry.registers.stack_pointer,
        {0x01U, 0xC3FAU},
        177U + continuation_offset,
        output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_session_input_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xC49EU,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_session_input_gate gate{};
    if (!dc_evaluate_session_input_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &gate)) {
        throw std::runtime_error(
            "Native C49E replacement rejected the work-RAM snapshot.");
    }
    if (gate.returns_idle == 0U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC4C8U;
    output.accumulator = gate.run_input;
    output.y = gate.active_player;
    output.status = static_cast<std::uint8_t>(
        output.status & static_cast<std::uint8_t>(~0x82U));
    output.status = static_cast<std::uint8_t>(output.status | 0x02U);

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xC4C8U,
        },
        .output_registers = output,
        .body_master_cycles = 72U,
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

    auto checkpoint = entry.registers;
    checkpoint.accumulator = gate.session_flags;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC49EU, 0xC4A0U, 12U, checkpoint);

    checkpoint.accumulator =
        static_cast<std::uint8_t>(gate.session_flags & 0x02U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC4A0U, 0xC4A2U, 18U, checkpoint);
    append_checkpoint(0xC4A2U, 0xC4A4U, 24U, checkpoint);

    checkpoint.y = gate.active_player;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0xC4A4U, 0xC4A7U, 39U, checkpoint);

    checkpoint.accumulator = gate.button_input;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC4A7U, 0xC4AAU, 54U, checkpoint);

    checkpoint.accumulator = gate.run_input;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC4AAU, 0xC4ACU, 60U, checkpoint);
    append_checkpoint(0xC4ACU, 0xC4C8U, 72U, output);
    absorb_return(
        replacement,
        work_ram,
        entry.registers.stack_pointer,
        {0x01U, 0xC4C8U},
        72U,
        output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_ball_drain_threshold(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xDD6EU,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_ball_drain_threshold threshold{};
    if (!dc_classify_ball_drain_threshold(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &threshold)) {
        throw std::runtime_error(
            "Native DD6E replacement rejected the work-RAM snapshot.");
    }
    if (threshold.path == DC_BALL_DRAIN_THRESHOLD_DEFER) {
        return std::nullopt;
    }

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
    const auto sbc = [&set_nz](
                         std::uint8_t status,
                         const std::uint8_t left,
                         const std::uint8_t right) {
        const auto difference = static_cast<int>(left) - static_cast<int>(right)
            - ((status & 0x01U) != 0U ? 0 : 1);
        const auto value = static_cast<std::uint8_t>(difference);
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0x41U));
        status = set_nz(status, value);
        if (difference >= 0) {
            status = static_cast<std::uint8_t>(status | 0x01U);
        }
        if ((((left ^ value) & (left ^ right)) & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x40U);
        }
        return std::pair{value, status};
    };

    const auto append_checkpoint = [](RoutineReplacement& replacement,
                                       const std::uint16_t instruction_pc,
                                       const std::uint16_t resume_pc,
                                       const std::uint32_t master_cycle,
                                       auto registers) {
        registers.program_counter = resume_pc;
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = master_cycle - 3U,
            .master_cycle_offset = master_cycle,
            .instruction = {0x01U, instruction_pc},
            .registers = registers,
        });
    };

    auto checkpoint = entry.registers;
    checkpoint.accumulator = threshold.session_state;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);

    RoutineReplacement replacement{};
    append_checkpoint(replacement, 0xDD6EU, 0xDD70U, 12U, checkpoint);
    append_checkpoint(replacement, 0xDD70U, 0xDD72U, 18U, checkpoint);

    checkpoint.status = static_cast<std::uint8_t>(checkpoint.status | 0x01U);
    append_checkpoint(replacement, 0xDD72U, 0xDD73U, 24U, checkpoint);

    checkpoint.accumulator = threshold.position_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0xDD73U, 0xDD76U, 39U, checkpoint);

    auto arithmetic = sbc(
        checkpoint.status,
        checkpoint.accumulator,
        0xD0U);
    checkpoint.accumulator = arithmetic.first;
    checkpoint.status = arithmetic.second;
    append_checkpoint(replacement, 0xDD76U, 0xDD78U, 45U, checkpoint);

    checkpoint.accumulator = threshold.position_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0xDD78U, 0xDD7BU, 60U, checkpoint);

    arithmetic = sbc(
        checkpoint.status,
        checkpoint.accumulator,
        0x02U);
    checkpoint.accumulator = arithmetic.first;
    checkpoint.status = arithmetic.second;
    append_checkpoint(replacement, 0xDD7BU, 0xDD7DU, 66U, checkpoint);

    if (threshold.path == DC_BALL_DRAIN_THRESHOLD_DRAINED) {
        append_checkpoint(replacement, 0xDD7DU, 0xDD7FU, 72U, checkpoint);
        checkpoint.status = static_cast<std::uint8_t>(checkpoint.status | 0x01U);
        checkpoint.program_counter = 0xDD80U;
        append_checkpoint(replacement, 0xDD7FU, 0xDD80U, 78U, checkpoint);
        replacement.return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xDD80U};
        replacement.output_registers = checkpoint;
        replacement.body_master_cycles = 78U;
        absorb_return(
            replacement,
            work_ram,
            entry.registers.stack_pointer,
            {0x01U, 0xDD80U},
            78U,
            checkpoint);
        return replacement;
    }

    append_checkpoint(replacement, 0xDD7DU, 0xDD81U, 78U, checkpoint);
    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status & static_cast<std::uint8_t>(~0x01U));
    checkpoint.program_counter = 0xDD82U;
    append_checkpoint(replacement, 0xDD81U, 0xDD82U, 84U, checkpoint);
    replacement.return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xDD82U};
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = 84U;
    absorb_return(
        replacement,
        work_ram,
        entry.registers.stack_pointer,
        {0x01U, 0xDD82U},
        84U,
        checkpoint);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_remaining_ball_presentation(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kPresentationRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xC80AU,
    };
    constexpr RoutineKey kZeroBallRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xC7F4U,
    };
    if (entry.routine != kPresentationRoutine && entry.routine != kZeroBallRoutine) {
        return std::nullopt;
    }
    if (work_ram.size() < 0x2000U) {
        throw std::runtime_error(
            "Native remaining-ball coroutine replacement rejected the work-RAM snapshot.");
    }

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
    const auto append_checkpoint = [](RoutineReplacement& replacement,
                                      const std::uint16_t instruction_pc,
                                      const std::uint16_t resume_pc,
                                      const std::uint32_t master_cycle,
                                      auto registers) {
        registers.program_counter = resume_pc;
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = master_cycle - 3U,
            .master_cycle_offset = master_cycle,
            .instruction = {0x01U, instruction_pc},
            .registers = registers,
        });
    };

    const auto stack_high = entry.registers.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
    const auto timer_offset = static_cast<std::uint16_t>(
        0x3D80U + entry.registers.x);
    const auto dispatch_low_offset = static_cast<std::uint16_t>(
        0x3F00U + entry.registers.x);
    const auto dispatch_high_offset = static_cast<std::uint16_t>(
        0x3F80U + entry.registers.x);
    const auto entry_pc = entry.routine.logical_pc;
    const auto first_jsr_pc = static_cast<std::uint16_t>(entry_pc + 2U);
    const auto first_return_pc = static_cast<std::uint16_t>(entry_pc + 4U);
    const auto continuation_pc = static_cast<std::uint16_t>(entry_pc + 5U);
    const auto second_return_pc = static_cast<std::uint16_t>(entry_pc + 7U);
    const auto initial_timer = static_cast<std::uint8_t>(
        entry.routine == kZeroBallRoutine ? 0xFFU : 0x10U);
    const auto decremented_timer = static_cast<std::uint8_t>(initial_timer - 1U);
    const auto first_return_low = static_cast<std::uint8_t>(first_return_pc);
    const auto first_return_high = static_cast<std::uint8_t>(first_return_pc >> 8U);
    const auto continuation_low = static_cast<std::uint8_t>(continuation_pc);
    const auto continuation_high = static_cast<std::uint8_t>(continuation_pc >> 8U);
    const auto second_return_low = static_cast<std::uint8_t>(second_return_pc);

    auto output = entry.registers;
    output.accumulator = initial_timer;
    output.status = set_nz(output.status, decremented_timer);
    output.program_counter = 0xC5E9U;

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xC5E9U,
        },
        .output_registers = output,
        .body_master_cycles = 318U,
    };
    replacement.work_ram_writes = {
        {
            .instruction = {0x01U, first_jsr_pc},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + stack_high),
            .value = first_return_high,
            .master_cycle_offset = 6U,
        },
        {
            .instruction = {0x01U, first_jsr_pc},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + stack_low),
            .value = first_return_low,
            .master_cycle_offset = 6U,
        },
        {
            .instruction = {0x01U, 0xC5EAU},
            .logical_address = timer_offset,
            .value = initial_timer,
            .master_cycle_offset = 42U,
        },
        {
            .instruction = {0x01U, 0xC5EEU},
            .logical_address = 0x202FU,
            .value = first_return_low,
            .master_cycle_offset = 66U,
        },
        {
            .instruction = {0x01U, 0xC5F1U},
            .logical_address = 0x2030U,
            .value = first_return_high,
            .master_cycle_offset = 90U,
        },
        {
            .instruction = {0x01U, 0xC5F3U},
            .logical_address = 0x202FU,
            .value = continuation_low,
            .master_cycle_offset = 108U,
        },
        {
            .instruction = {0x01U, 0xC5FBU},
            .logical_address = dispatch_low_offset,
            .value = continuation_low,
            .master_cycle_offset = 147U,
        },
        {
            .instruction = {0x01U, 0xC600U},
            .logical_address = dispatch_high_offset,
            .value = continuation_high,
            .master_cycle_offset = 174U,
        },
        {
            .instruction = {0x01U, continuation_pc},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + stack_high),
            .value = continuation_high,
            .master_cycle_offset = 195U,
        },
        {
            .instruction = {0x01U, continuation_pc},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + stack_low),
            .value = second_return_low,
            .master_cycle_offset = 195U,
        },
        {
            .instruction = {0x01U, 0xC5CAU},
            .logical_address = 0x2026U,
            .value = second_return_low,
            .master_cycle_offset = 240U,
        },
        {
            .instruction = {0x01U, 0xC5CDU},
            .logical_address = 0x2027U,
            .value = continuation_high,
            .master_cycle_offset = 264U,
        },
        {
            .instruction = {0x01U, 0xC5D4U},
            .logical_address = timer_offset,
            .value = decremented_timer,
            .master_cycle_offset = 306U,
        },
    };

    auto checkpoint = entry.registers;
    checkpoint.accumulator = initial_timer;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, entry_pc, first_jsr_pc, 6U, checkpoint);

    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        entry.registers.stack_pointer - 2U);
    append_checkpoint(replacement, first_jsr_pc, 0xC5EAU, 27U, checkpoint);
    append_checkpoint(replacement, 0xC5EAU, 0xC5EDU, 42U, checkpoint);

    checkpoint.stack_pointer = stack_low;
    checkpoint.accumulator = first_return_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0xC5EDU, 0xC5EEU, 54U, checkpoint);
    append_checkpoint(replacement, 0xC5EEU, 0xC5F0U, 66U, checkpoint);

    checkpoint.stack_pointer = stack_high;
    checkpoint.accumulator = first_return_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0xC5F0U, 0xC5F1U, 78U, checkpoint);
    append_checkpoint(replacement, 0xC5F1U, 0xC5F3U, 90U, checkpoint);

    checkpoint.accumulator = first_return_high;
    checkpoint.status = set_nz(checkpoint.status, continuation_low);
    append_checkpoint(replacement, 0xC5F3U, 0xC5F5U, 108U, checkpoint);
    append_checkpoint(replacement, 0xC5F5U, 0xC5F9U, 120U, checkpoint);

    checkpoint.accumulator = continuation_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0xC5F9U, 0xC5FBU, 132U, checkpoint);
    append_checkpoint(replacement, 0xC5FBU, 0xC5FEU, 147U, checkpoint);

    checkpoint.accumulator = continuation_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0xC5FEU, 0xC600U, 159U, checkpoint);
    append_checkpoint(replacement, 0xC600U, 0xC603U, 174U, checkpoint);
    append_checkpoint(replacement, 0xC603U, continuation_pc, 195U, checkpoint);

    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        entry.registers.stack_pointer - 2U);
    append_checkpoint(replacement, continuation_pc, 0xC5C9U, 216U, checkpoint);

    checkpoint.stack_pointer = stack_low;
    checkpoint.accumulator = second_return_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0xC5C9U, 0xC5CAU, 228U, checkpoint);
    append_checkpoint(replacement, 0xC5CAU, 0xC5CCU, 240U, checkpoint);

    checkpoint.stack_pointer = entry.registers.stack_pointer;
    checkpoint.accumulator = continuation_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0xC5CCU, 0xC5CDU, 252U, checkpoint);
    append_checkpoint(replacement, 0xC5CDU, 0xC5CFU, 264U, checkpoint);

    checkpoint.accumulator = initial_timer;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0xC5CFU, 0xC5D2U, 279U, checkpoint);
    append_checkpoint(replacement, 0xC5D2U, 0xC5D4U, 285U, checkpoint);

    checkpoint.status = set_nz(checkpoint.status, decremented_timer);
    append_checkpoint(replacement, 0xC5D4U, 0xC5D7U, 306U, checkpoint);
    append_checkpoint(replacement, 0xC5D7U, 0xC5E9U, 318U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_remaining_ball_alternate_scheduler(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xC7EFU,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    if (work_ram.size() < 0x2000U) {
        throw std::runtime_error(
            "Native C7EF replacement rejected the work-RAM snapshot.");
    }

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
    const auto append_checkpoint = [](RoutineReplacement& replacement,
                                      const std::uint16_t instruction_pc,
                                      const std::uint16_t resume_pc,
                                      const std::uint32_t master_cycle,
                                      auto registers) {
        registers.program_counter = resume_pc;
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = master_cycle - 3U,
            .master_cycle_offset = master_cycle,
            .instruction = {0x01U, instruction_pc},
            .registers = registers,
        });
    };

    const auto stack_high = entry.registers.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    staged[0x0D32U] = static_cast<std::byte>(0x00U);
    staged[0x0D53U] = static_cast<std::byte>(0x01U);
    staged[0x0100U + stack_high] = static_cast<std::byte>(0xC8U);
    staged[0x0100U + stack_low] = static_cast<std::byte>(0x03U);

    auto checkpoint = entry.registers;
    checkpoint.accumulator = 0x01U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 2U);
    checkpoint.program_counter = 0xC5C9U;
    RoutineEntry scheduler_entry{
        .routine = {.physical_bank = 0x01U, .logical_pc = 0xC5C9U},
        .registers = checkpoint,
    };
    auto scheduler = replace_coroutine_scheduler(scheduler_entry, staged);
    if (!scheduler || scheduler->checkpoints.empty()) {
        return std::nullopt;
    }

    constexpr std::uint32_t kSchedulerOffset = 69U;
    RoutineReplacement replacement{};
    replacement.work_ram_writes = {
        {
            .instruction = {0x01U, 0xC7EFU},
            .logical_address = 0x2D32U,
            .value = 0x00U,
            .master_cycle_offset = 15U,
        },
        {
            .instruction = {0x01U, 0xC7FEU},
            .logical_address = 0x2D53U,
            .value = 0x01U,
            .master_cycle_offset = 48U,
        },
        {
            .instruction = {0x01U, 0xC801U},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_high),
            .value = 0xC8U,
            .master_cycle_offset = 48U,
        },
        {
            .instruction = {0x01U, 0xC801U},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_low),
            .value = 0x03U,
            .master_cycle_offset = 48U,
        },
    };
    for (auto write : scheduler->work_ram_writes) {
        write.master_cycle_offset += kSchedulerOffset;
        replacement.work_ram_writes.push_back(write);
    }

    checkpoint = entry.registers;
    append_checkpoint(replacement, 0xC7EFU, 0xC7F2U, 15U, checkpoint);
    append_checkpoint(replacement, 0xC7F2U, 0xC7FCU, 27U, checkpoint);
    checkpoint.accumulator = 0x01U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0xC7FCU, 0xC7FEU, 33U, checkpoint);
    append_checkpoint(replacement, 0xC7FEU, 0xC801U, 48U, checkpoint);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 2U);
    append_checkpoint(replacement, 0xC801U, 0xC5C9U, 69U, checkpoint);
    for (auto child_checkpoint : scheduler->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset += kSchedulerOffset;
        child_checkpoint.master_cycle_offset += kSchedulerOffset;
        replacement.checkpoints.push_back(child_checkpoint);
    }
    replacement.return_instruction = scheduler->return_instruction;
    replacement.output_registers = scheduler->output_registers;
    replacement.body_master_cycles =
        kSchedulerOffset + scheduler->body_master_cycles;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_zero_ball_countdown_tick(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xC7F9U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    if (work_ram.size() < 0x2000U) {
        throw std::runtime_error(
            "Native C7F9 replacement rejected the work-RAM snapshot.");
    }

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

    const auto stack_high = entry.registers.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    staged[0x0100U + stack_high] = static_cast<std::byte>(0xC7U);
    staged[0x0100U + stack_low] = static_cast<std::byte>(0xFBU);

    auto checkpoint = entry.registers;
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 2U);
    checkpoint.program_counter = 0xC5C9U;
    RoutineEntry scheduler_entry{
        .routine = {.physical_bank = 0x01U, .logical_pc = 0xC5C9U},
        .registers = checkpoint,
    };
    auto scheduler = replace_coroutine_scheduler(scheduler_entry, staged);
    if (!scheduler || scheduler->body_master_cycles != 102U
        || scheduler->work_ram_writes.size() != 3U) {
        // Countdown expiry starts C7FC's state transition. Its descendants
        // and interrupt-bearing forms need their own bounded transcript.
        return std::nullopt;
    }

    constexpr std::uint32_t kSchedulerOffset = 21U;
    RoutineReplacement replacement{};
    replacement.work_ram_writes = {
        {
            .instruction = {0x01U, 0xC7F9U},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_high),
            .value = 0xC7U,
            .master_cycle_offset = 0U,
        },
        {
            .instruction = {0x01U, 0xC7F9U},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_low),
            .value = 0xFBU,
            .master_cycle_offset = 0U,
        },
    };
    for (auto write : scheduler->work_ram_writes) {
        write.master_cycle_offset += kSchedulerOffset;
        replacement.work_ram_writes.push_back(write);
    }

    replacement.checkpoints.reserve(scheduler->checkpoints.size() + 1U);
    checkpoint = entry.registers;
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 2U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    checkpoint.program_counter = 0xC5C9U;
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 18U,
        .master_cycle_offset = 21U,
        .instruction = {0x01U, 0xC7F9U},
        .registers = checkpoint,
    });
    for (auto child_checkpoint : scheduler->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset += kSchedulerOffset;
        child_checkpoint.master_cycle_offset += kSchedulerOffset;
        replacement.checkpoints.push_back(child_checkpoint);
    }
    replacement.return_instruction = scheduler->return_instruction;
    replacement.output_registers = scheduler->output_registers;
    replacement.body_master_cycles =
        kSchedulerOffset + scheduler->body_master_cycles;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_zero_ball_expiry_scheduler(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xC7FCU,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    auto alternate_entry = entry;
    alternate_entry.routine = {.physical_bank = 0x01U, .logical_pc = 0xC7EFU};
    alternate_entry.registers.program_counter = 0xC7EFU;
    auto replacement = replace_remaining_ball_alternate_scheduler(
        alternate_entry,
        work_ram);
    if (!replacement || replacement->work_ram_writes.empty()
        || replacement->checkpoints.size() < 2U) {
        return std::nullopt;
    }

    // C7EF's first 27 cycles clear the alternate latch and branch to C7FC.
    // C7FC starts at the common scheduler-install tail immediately afterward.
    constexpr std::uint32_t kAlternatePrefixCycles = 27U;
    replacement->work_ram_writes.erase(replacement->work_ram_writes.begin());
    for (auto& write : replacement->work_ram_writes) {
        write.master_cycle_offset -= kAlternatePrefixCycles;
    }
    replacement->checkpoints.erase(
        replacement->checkpoints.begin(),
        replacement->checkpoints.begin() + 2);
    for (auto& checkpoint : replacement->checkpoints) {
        checkpoint.interrupt_sample_master_cycle_offset -=
            kAlternatePrefixCycles;
        checkpoint.master_cycle_offset -= kAlternatePrefixCycles;
    }
    replacement->body_master_cycles -= kAlternatePrefixCycles;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_zero_ball_presentation_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xC804U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    if (work_ram.size() < 0x2000U) {
        throw std::runtime_error(
            "Native C804 replacement rejected the work-RAM snapshot.");
    }

    const auto latch = std::to_integer<std::uint8_t>(work_ram[0x0D53U]);
    if (latch != 0U) {
        return std::nullopt;
    }
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
    const auto append_checkpoint = [](RoutineReplacement& replacement,
                                      const std::uint16_t instruction_pc,
                                      const std::uint16_t resume_pc,
                                      const std::uint32_t master_cycle,
                                      auto registers) {
        registers.program_counter = resume_pc;
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = master_cycle - 3U,
            .master_cycle_offset = master_cycle,
            .instruction = {0x01U, instruction_pc},
            .registers = registers,
        });
    };

    auto presentation_entry = entry;
    presentation_entry.routine = {.physical_bank = 0x01U, .logical_pc = 0xC80AU};
    presentation_entry.registers.program_counter = 0xC80AU;
    presentation_entry.registers.accumulator = latch;
    presentation_entry.registers.status = set_nz(
        presentation_entry.registers.status,
        latch);
    auto replacement = replace_remaining_ball_presentation(
        presentation_entry,
        work_ram);
    if (!replacement) {
        throw std::runtime_error(
            "Native C804 replacement could not compose C80A.");
    }

    constexpr std::uint32_t kPresentationEntryOffset = 27U;
    for (auto& write : replacement->work_ram_writes) {
        write.master_cycle_offset += kPresentationEntryOffset;
    }
    for (auto& checkpoint : replacement->checkpoints) {
        checkpoint.interrupt_sample_master_cycle_offset +=
            kPresentationEntryOffset;
        checkpoint.master_cycle_offset += kPresentationEntryOffset;
    }
    replacement->body_master_cycles += kPresentationEntryOffset;
    auto presentation_checkpoints = std::move(replacement->checkpoints);
    replacement->checkpoints.clear();
    replacement->checkpoints.reserve(presentation_checkpoints.size() + 2U);

    auto checkpoint = entry.registers;
    checkpoint.accumulator = latch;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(*replacement, 0xC804U, 0xC807U, 15U, checkpoint);
    append_checkpoint(*replacement, 0xC807U, 0xC80AU, 27U, checkpoint);
    replacement->checkpoints.insert(
        replacement->checkpoints.end(),
        presentation_checkpoints.begin(),
        presentation_checkpoints.end());
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_zero_ball_state_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xC812U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    if (work_ram.size() < 0x2000U) {
        throw std::runtime_error(
            "Native C812 replacement rejected the work-RAM snapshot.");
    }
    const auto ball_count = std::to_integer<std::uint8_t>(work_ram[0x0D52U]);
    if (ball_count != 0U) {
        return std::nullopt;
    }
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
    const auto append_checkpoint = [](RoutineReplacement& replacement,
                                      const std::uint16_t instruction_pc,
                                      const std::uint16_t resume_pc,
                                      const std::uint32_t master_cycle,
                                      auto registers) {
        registers.program_counter = resume_pc;
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = master_cycle - 3U,
            .master_cycle_offset = master_cycle,
            .instruction = {0x01U, instruction_pc},
            .registers = registers,
        });
    };

    const auto state_flags = static_cast<std::uint8_t>(
        std::to_integer<std::uint8_t>(work_ram[0x0053U]) | 0x20U);
    const auto stack_high = entry.registers.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    staged[0x0053U] = static_cast<std::byte>(state_flags);
    staged[0x0100U + stack_high] = static_cast<std::byte>(0xC8U);
    staged[0x0100U + stack_low] = static_cast<std::byte>(0x40U);

    auto checkpoint = entry.registers;
    checkpoint.accumulator = state_flags;
    checkpoint.status = set_nz(checkpoint.status, state_flags);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 2U);
    checkpoint.program_counter = 0xC5C9U;
    RoutineEntry scheduler_entry{
        .routine = {.physical_bank = 0x01U, .logical_pc = 0xC5C9U},
        .registers = checkpoint,
    };
    auto scheduler = replace_coroutine_scheduler(scheduler_entry, staged);
    if (!scheduler || scheduler->body_master_cycles != 159U) {
        return std::nullopt;
    }

    constexpr std::uint32_t kSchedulerOffset = 78U;
    RoutineReplacement replacement{};
    replacement.work_ram_writes = {
        {
            .instruction = {0x01U, 0xC83CU},
            .logical_address = 0x2053U,
            .value = state_flags,
            .master_cycle_offset = 57U,
        },
        {
            .instruction = {0x01U, 0xC83EU},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_high),
            .value = 0xC8U,
            .master_cycle_offset = 57U,
        },
        {
            .instruction = {0x01U, 0xC83EU},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_low),
            .value = 0x40U,
            .master_cycle_offset = 57U,
        },
    };
    for (auto write : scheduler->work_ram_writes) {
        write.master_cycle_offset += kSchedulerOffset;
        replacement.work_ram_writes.push_back(write);
    }

    checkpoint = entry.registers;
    checkpoint.accumulator = ball_count;
    checkpoint.status = set_nz(checkpoint.status, ball_count);
    append_checkpoint(replacement, 0xC812U, 0xC815U, 15U, checkpoint);
    append_checkpoint(replacement, 0xC815U, 0xC838U, 27U, checkpoint);
    checkpoint.accumulator = state_flags;
    checkpoint.status = set_nz(checkpoint.status, state_flags);
    append_checkpoint(replacement, 0xC838U, 0xC83AU, 36U, checkpoint);
    append_checkpoint(replacement, 0xC83AU, 0xC83CU, 42U, checkpoint);
    append_checkpoint(replacement, 0xC83CU, 0xC83EU, 57U, checkpoint);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 2U);
    append_checkpoint(replacement, 0xC83EU, 0xC5C9U, 78U, checkpoint);
    for (auto child_checkpoint : scheduler->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset += kSchedulerOffset;
        child_checkpoint.master_cycle_offset += kSchedulerOffset;
        replacement.checkpoints.push_back(child_checkpoint);
    }
    replacement.return_instruction = scheduler->return_instruction;
    replacement.output_registers = scheduler->output_registers;
    replacement.body_master_cycles =
        kSchedulerOffset + scheduler->body_master_cycles;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_remaining_ball_transition_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xC7EAU,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    if (work_ram.size() < 0x2000U) {
        throw std::runtime_error(
            "Native C7EA replacement rejected the work-RAM snapshot.");
    }

    const auto transition_latch = std::to_integer<std::uint8_t>(
        work_ram[0x0D32U]);

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
    const auto append_checkpoint = [](RoutineReplacement& replacement,
                                      const std::uint16_t instruction_pc,
                                      const std::uint16_t resume_pc,
                                      const std::uint32_t master_cycle,
                                      auto registers) {
        registers.program_counter = resume_pc;
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = master_cycle - 3U,
            .master_cycle_offset = master_cycle,
            .instruction = {0x01U, instruction_pc},
            .registers = registers,
        });
    };

    if (transition_latch != 0U) {
        auto alternate_entry = entry;
        alternate_entry.routine = {
            .physical_bank = 0x01U,
            .logical_pc = 0xC7EFU,
        };
        alternate_entry.registers.program_counter = 0xC7EFU;
        alternate_entry.registers.accumulator = transition_latch;
        alternate_entry.registers.status = set_nz(
            alternate_entry.registers.status,
            transition_latch);
        auto replacement = replace_remaining_ball_alternate_scheduler(
            alternate_entry,
            work_ram);
        if (!replacement) {
            throw std::runtime_error(
                "Native C7EA replacement could not compose C7EF.");
        }

        constexpr std::uint32_t kAlternateEntryOffset = 21U;
        for (auto& write : replacement->work_ram_writes) {
            write.master_cycle_offset += kAlternateEntryOffset;
        }
        for (auto& checkpoint : replacement->checkpoints) {
            checkpoint.interrupt_sample_master_cycle_offset +=
                kAlternateEntryOffset;
            checkpoint.master_cycle_offset += kAlternateEntryOffset;
        }
        replacement->body_master_cycles += kAlternateEntryOffset;
        auto alternate_checkpoints = std::move(replacement->checkpoints);
        replacement->checkpoints.clear();
        replacement->checkpoints.reserve(alternate_checkpoints.size() + 2U);

        auto checkpoint = entry.registers;
        checkpoint.accumulator = transition_latch;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(*replacement, 0xC7EAU, 0xC7EDU, 15U, checkpoint);
        append_checkpoint(*replacement, 0xC7EDU, 0xC7EFU, 21U, checkpoint);
        replacement->checkpoints.insert(
            replacement->checkpoints.end(),
            alternate_checkpoints.begin(),
            alternate_checkpoints.end());
        return replacement;
    }

    auto presentation_entry = entry;
    presentation_entry.routine = {
        .physical_bank = 0x01U,
        .logical_pc = 0xC80AU,
    };
    presentation_entry.registers.program_counter = 0xC80AU;
    presentation_entry.registers.accumulator = transition_latch;
    presentation_entry.registers.status = set_nz(
        presentation_entry.registers.status,
        transition_latch);
    auto replacement = replace_remaining_ball_presentation(
        presentation_entry,
        work_ram);
    if (!replacement) {
        throw std::runtime_error(
            "Native C7EA replacement could not compose C80A.");
    }

    constexpr std::uint32_t kPresentationEntryOffset = 27U;
    for (auto& write : replacement->work_ram_writes) {
        write.master_cycle_offset += kPresentationEntryOffset;
    }
    for (auto& checkpoint : replacement->checkpoints) {
        checkpoint.interrupt_sample_master_cycle_offset +=
            kPresentationEntryOffset;
        checkpoint.master_cycle_offset += kPresentationEntryOffset;
    }
    replacement->body_master_cycles += kPresentationEntryOffset;
    auto presentation_checkpoints = std::move(replacement->checkpoints);
    replacement->checkpoints.clear();
    replacement->checkpoints.reserve(presentation_checkpoints.size() + 2U);

    auto checkpoint = entry.registers;
    checkpoint.accumulator = transition_latch;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(*replacement, 0xC7EAU, 0xC7EDU, 15U, checkpoint);
    append_checkpoint(*replacement, 0xC7EDU, 0xC80AU, 27U, checkpoint);
    replacement->checkpoints.insert(
        replacement->checkpoints.end(),
        presentation_checkpoints.begin(),
        presentation_checkpoints.end());
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_remaining_ball_decision(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xC7E5U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    if (work_ram.size() < 0x2000U) {
        throw std::runtime_error(
            "Native C7E5 replacement rejected the work-RAM snapshot.");
    }

    const auto ball_count = std::to_integer<std::uint8_t>(
        work_ram[0x0D52U]);

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
    const auto append_checkpoint = [](RoutineReplacement& replacement,
                                      const std::uint16_t instruction_pc,
                                      const std::uint16_t resume_pc,
                                      const std::uint32_t master_cycle,
                                      auto registers) {
        registers.program_counter = resume_pc;
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = master_cycle - 3U,
            .master_cycle_offset = master_cycle,
            .instruction = {0x01U, instruction_pc},
            .registers = registers,
        });
    };

    if (ball_count == 0U) {
        auto scheduler_entry = entry;
        scheduler_entry.routine = {
            .physical_bank = 0x01U,
            .logical_pc = 0xC7F4U,
        };
        scheduler_entry.registers.program_counter = 0xC7F4U;
        scheduler_entry.registers.accumulator = ball_count;
        scheduler_entry.registers.status = set_nz(
            scheduler_entry.registers.status,
            ball_count);
        auto replacement = replace_remaining_ball_presentation(
            scheduler_entry,
            work_ram);
        if (!replacement) {
            throw std::runtime_error(
                "Native C7E5 replacement could not compose C7F4.");
        }

        constexpr std::uint32_t kSchedulerEntryOffset = 27U;
        for (auto& write : replacement->work_ram_writes) {
            write.master_cycle_offset += kSchedulerEntryOffset;
        }
        for (auto& checkpoint : replacement->checkpoints) {
            checkpoint.interrupt_sample_master_cycle_offset +=
                kSchedulerEntryOffset;
            checkpoint.master_cycle_offset += kSchedulerEntryOffset;
        }
        replacement->body_master_cycles += kSchedulerEntryOffset;
        auto scheduler_checkpoints = std::move(replacement->checkpoints);
        replacement->checkpoints.clear();
        replacement->checkpoints.reserve(scheduler_checkpoints.size() + 2U);

        auto checkpoint = entry.registers;
        checkpoint.accumulator = ball_count;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(*replacement, 0xC7E5U, 0xC7E8U, 15U, checkpoint);
        append_checkpoint(*replacement, 0xC7E8U, 0xC7F4U, 27U, checkpoint);
        replacement->checkpoints.insert(
            replacement->checkpoints.end(),
            scheduler_checkpoints.begin(),
            scheduler_checkpoints.end());
        return replacement;
    }

    auto transition_entry = entry;
    transition_entry.routine = {
        .physical_bank = 0x01U,
        .logical_pc = 0xC7EAU,
    };
    transition_entry.registers.program_counter = 0xC7EAU;
    transition_entry.registers.accumulator = ball_count;
    transition_entry.registers.status = set_nz(
        transition_entry.registers.status,
        ball_count);
    auto replacement = replace_remaining_ball_transition_gate(
        transition_entry,
        work_ram);
    if (!replacement) {
        return std::nullopt;
    }

    constexpr std::uint32_t kTransitionEntryOffset = 21U;
    for (auto& write : replacement->work_ram_writes) {
        write.master_cycle_offset += kTransitionEntryOffset;
    }
    for (auto& checkpoint : replacement->checkpoints) {
        checkpoint.interrupt_sample_master_cycle_offset +=
            kTransitionEntryOffset;
        checkpoint.master_cycle_offset += kTransitionEntryOffset;
    }
    replacement->body_master_cycles += kTransitionEntryOffset;
    auto transition_checkpoints = std::move(replacement->checkpoints);
    replacement->checkpoints.clear();
    replacement->checkpoints.reserve(transition_checkpoints.size() + 2U);

    auto checkpoint = entry.registers;
    checkpoint.accumulator = ball_count;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(*replacement, 0xC7E5U, 0xC7E8U, 15U, checkpoint);
    append_checkpoint(*replacement, 0xC7E8U, 0xC7EAU, 21U, checkpoint);
    replacement->checkpoints.insert(
        replacement->checkpoints.end(),
        transition_checkpoints.begin(),
        transition_checkpoints.end());
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_actor_camera_activation(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x00U,
        .logical_pc = 0xF4FCU,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    if ((entry.registers.status & 0x08U) != 0U) {
        return std::nullopt;
    }

    dc_actor_camera_activation activation{};
    if (!dc_classify_actor_camera_activation(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &activation)) {
        throw std::runtime_error(
            "Native F4FC replacement rejected the work-RAM snapshot.");
    }

    const auto return_pc =
        activation.path == DC_ACTOR_CAMERA_BELOW
        ? 0xF523U
        : (activation.path == DC_ACTOR_CAMERA_ABOVE
            ? 0xF545U
            : 0xF54EU);
    const auto body_cycles =
        activation.path == DC_ACTOR_CAMERA_EMPTY
        ? 27U
        : (activation.path == DC_ACTOR_CAMERA_BELOW
            ? 189U
            : (activation.path == DC_ACTOR_CAMERA_ABOVE
                ? 327U
                : 333U));

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x00U,
            .logical_pc = static_cast<std::uint16_t>(return_pc),
        },
        .output_registers = entry.registers,
        .body_master_cycles = body_cycles,
    };
    replacement.work_ram_writes.reserve(
        activation.path == DC_ACTOR_CAMERA_EMPTY
        ? 0U
        : (activation.path == DC_ACTOR_CAMERA_BELOW ? 3U : 5U));
    if (activation.path != DC_ACTOR_CAMERA_EMPTY) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x00U, 0xF506U},
            .logical_address = 0x2035U,
            .value = activation.lower_low,
            .master_cycle_offset = 57U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x00U, 0xF50CU},
            .logical_address = 0x2036U,
            .value = activation.lower_high,
            .master_cycle_offset = 87U,
        });
        if (activation.path != DC_ACTOR_CAMERA_BELOW) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x00U, 0xF529U},
                .logical_address = 0x2035U,
                .value = activation.upper_low,
                .master_cycle_offset = 195U,
            });
            replacement.work_ram_writes.push_back({
                .instruction = {0x00U, 0xF52EU},
                .logical_address = 0x2036U,
                .value = activation.upper_high,
                .master_cycle_offset = 225U,
            });
        }
        replacement.work_ram_writes.push_back({
            .instruction = {
                0x00U,
                activation.path == DC_ACTOR_CAMERA_BELOW
                    ? 0xF520U
                    : (activation.path == DC_ACTOR_CAMERA_ABOVE
                        ? 0xF542U
                        : 0xF54BU),
            },
            .logical_address = static_cast<std::uint16_t>(
                0x3080U + entry.registers.x),
            .value = activation.flags_after,
            .master_cycle_offset = body_cycles,
        });
    }

    const auto set_nz =
        [](std::uint8_t status, const std::uint8_t value) {
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xA2U));
            if (value == 0U) {
                status = static_cast<std::uint8_t>(
                    status | 0x02U);
            } else if ((value & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(
                    status | 0x80U);
            }
            return status;
        };
    const auto sbc =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right) {
            const auto difference =
                static_cast<int>(left)
                - static_cast<int>(right)
                - ((status & 0x01U) != 0U ? 0 : 1);
            const auto result =
                static_cast<std::uint8_t>(difference);
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0x41U));
            status = set_nz(status, result);
            if (difference >= 0) {
                status = static_cast<std::uint8_t>(
                    status | 0x01U);
            }
            if ((((left ^ result) & (left ^ right)) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(
                    status | 0x40U);
            }
            return std::pair{result, status};
        };
    const auto adc =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right) {
            const auto sum =
                static_cast<unsigned int>(left)
                + static_cast<unsigned int>(right)
                + ((status & 0x01U) != 0U ? 1U : 0U);
            const auto result =
                static_cast<std::uint8_t>(sum);
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0x41U));
            status = set_nz(status, result);
            if (sum > 0xFFU) {
                status = static_cast<std::uint8_t>(
                    status | 0x01U);
            }
            if (((~(left ^ right) & (left ^ result)) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(
                    status | 0x40U);
            }
            return std::pair{result, status};
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
    checkpoint.accumulator = activation.occupancy;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF4FCU, 0xF4FFU, 15U, checkpoint);
    if (activation.path == DC_ACTOR_CAMERA_EMPTY) {
        append_checkpoint(0xF4FFU, 0xF54EU, 27U, checkpoint);
        checkpoint.program_counter = 0xF54EU;
        replacement.output_registers = checkpoint;
        return replacement;
    }

    append_checkpoint(0xF4FFU, 0xF501U, 21U, checkpoint);
    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status | 0x01U);
    append_checkpoint(0xF501U, 0xF502U, 27U, checkpoint);

    checkpoint.accumulator = activation.anchor_low;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF502U, 0xF504U, 39U, checkpoint);
    auto arithmetic = sbc(
        checkpoint.status,
        checkpoint.accumulator,
        0x30U);
    checkpoint.accumulator = arithmetic.first;
    checkpoint.status = arithmetic.second;
    append_checkpoint(0xF504U, 0xF506U, 45U, checkpoint);
    append_checkpoint(0xF506U, 0xF508U, 57U, checkpoint);

    checkpoint.accumulator = activation.anchor_high;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF508U, 0xF50AU, 69U, checkpoint);
    arithmetic = sbc(
        checkpoint.status,
        checkpoint.accumulator,
        0x00U);
    checkpoint.accumulator = arithmetic.first;
    checkpoint.status = arithmetic.second;
    append_checkpoint(0xF50AU, 0xF50CU, 75U, checkpoint);
    append_checkpoint(0xF50CU, 0xF50EU, 87U, checkpoint);

    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status | 0x01U);
    append_checkpoint(0xF50EU, 0xF50FU, 93U, checkpoint);
    checkpoint.accumulator = activation.position_low;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF50FU, 0xF512U, 108U, checkpoint);
    arithmetic = sbc(
        checkpoint.status,
        checkpoint.accumulator,
        activation.lower_low);
    checkpoint.accumulator = arithmetic.first;
    checkpoint.status = arithmetic.second;
    append_checkpoint(0xF512U, 0xF514U, 120U, checkpoint);
    checkpoint.accumulator = activation.position_high;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF514U, 0xF517U, 135U, checkpoint);
    arithmetic = sbc(
        checkpoint.status,
        checkpoint.accumulator,
        activation.lower_high);
    checkpoint.accumulator = arithmetic.first;
    checkpoint.status = arithmetic.second;
    append_checkpoint(0xF517U, 0xF519U, 147U, checkpoint);

    if (activation.path == DC_ACTOR_CAMERA_BELOW) {
        append_checkpoint(0xF519U, 0xF51BU, 153U, checkpoint);
        checkpoint.accumulator = activation.flags_before;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xF51BU, 0xF51EU, 168U, checkpoint);
        checkpoint.accumulator = activation.flags_after;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xF51EU, 0xF520U, 174U, checkpoint);
        append_checkpoint(0xF520U, 0xF523U, 189U, checkpoint);
        checkpoint.program_counter = 0xF523U;
        replacement.output_registers = checkpoint;
        return replacement;
    }

    append_checkpoint(0xF519U, 0xF524U, 159U, checkpoint);
    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status & static_cast<std::uint8_t>(~0x01U));
    append_checkpoint(0xF524U, 0xF525U, 165U, checkpoint);
    checkpoint.accumulator = activation.anchor_low;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF525U, 0xF527U, 177U, checkpoint);
    arithmetic = adc(
        checkpoint.status,
        checkpoint.accumulator,
        0xF0U);
    checkpoint.accumulator = arithmetic.first;
    checkpoint.status = arithmetic.second;
    append_checkpoint(0xF527U, 0xF529U, 183U, checkpoint);
    append_checkpoint(0xF529U, 0xF52BU, 195U, checkpoint);
    checkpoint.accumulator = 0U;
    append_checkpoint(0xF52BU, 0xF52CU, 201U, checkpoint);
    arithmetic = adc(
        checkpoint.status,
        checkpoint.accumulator,
        activation.anchor_high);
    checkpoint.accumulator = arithmetic.first;
    checkpoint.status = arithmetic.second;
    append_checkpoint(0xF52CU, 0xF52EU, 213U, checkpoint);
    append_checkpoint(0xF52EU, 0xF530U, 225U, checkpoint);

    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status | 0x01U);
    append_checkpoint(0xF530U, 0xF531U, 231U, checkpoint);
    checkpoint.accumulator = activation.position_low;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF531U, 0xF534U, 246U, checkpoint);
    arithmetic = sbc(
        checkpoint.status,
        checkpoint.accumulator,
        activation.upper_low);
    checkpoint.accumulator = arithmetic.first;
    checkpoint.status = arithmetic.second;
    append_checkpoint(0xF534U, 0xF536U, 258U, checkpoint);
    checkpoint.accumulator = activation.position_high;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF536U, 0xF539U, 273U, checkpoint);
    arithmetic = sbc(
        checkpoint.status,
        checkpoint.accumulator,
        activation.upper_high);
    checkpoint.accumulator = arithmetic.first;
    checkpoint.status = arithmetic.second;
    append_checkpoint(0xF539U, 0xF53BU, 285U, checkpoint);

    const bool inside =
        activation.path == DC_ACTOR_CAMERA_INSIDE;
    append_checkpoint(
        0xF53BU,
        inside ? 0xF546U : 0xF53DU,
        inside ? 297U : 291U,
        checkpoint);
    checkpoint.accumulator = activation.flags_before;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        inside ? 0xF546U : 0xF53DU,
        inside ? 0xF549U : 0xF540U,
        inside ? 312U : 306U,
        checkpoint);
    checkpoint.accumulator = activation.flags_after;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        inside ? 0xF549U : 0xF540U,
        inside ? 0xF54BU : 0xF542U,
        inside ? 318U : 312U,
        checkpoint);
    append_checkpoint(
        inside ? 0xF54BU : 0xF542U,
        static_cast<std::uint16_t>(return_pc),
        body_cycles,
        checkpoint);
    checkpoint.program_counter =
        static_cast<std::uint16_t>(return_pc);
    replacement.output_registers = checkpoint;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_actor_scan_run(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The main actor loop. Each arrival either dispatches the slot through
    // the $3F00/$3F80 jump table or skips it, and only the skip run is
    // native: a dispatch enters an emulated handler this transaction cannot
    // call. The walk hands control back at the dispatch branch or at the
    // terminal comparison.
    //
    // It is declared as an instruction-boundary transcript so it can cross
    // interrupt horizons rather than be bounded by them. Priced atomically
    // the horizon here averages 265 master cycles, which declined 185,330 of
    // 321,839 arrivals outright and truncated the 136,509 it did admit to a
    // mean of 142 cycles.
    //
    //   C510 LDA $3000,X 15   C513 BPL $C523 6/12
    //   C515 LDA $3080,X 15   C518 BMI $C51E 6/12
    //   C51A AND #$02     6   C51C BNE $C523 6/12
    //   C51E BSR $C52C        (the dispatch; stays emulated)
    //   C523 INC $4D     18   C525 LDX $4D   12
    //   C527 CPX #$80     6   C529 BNE $C510 6/12
    //   C52B RTS              (stays emulated)
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xC510U,
    };
    constexpr std::uint16_t kSlotFlagsPc = 0xC510U;
    constexpr std::uint16_t kIdleBranchPc = 0xC513U;
    constexpr std::uint16_t kAttributesPc = 0xC515U;
    constexpr std::uint16_t kNegativeBranchPc = 0xC518U;
    constexpr std::uint16_t kMaskPc = 0xC51AU;
    constexpr std::uint16_t kMaskBranchPc = 0xC51CU;
    constexpr std::uint16_t kDispatchPc = 0xC51EU;
    constexpr std::uint16_t kIncrementPc = 0xC523U;
    constexpr std::uint16_t kReloadPc = 0xC525U;
    constexpr std::uint16_t kComparePc = 0xC527U;
    constexpr std::uint16_t kLoopBranchPc = 0xC529U;
    constexpr std::uint16_t kTerminalPc = 0xC52BU;
    constexpr std::uint16_t kCounterAddress = 0x204DU;
    constexpr std::uint16_t kSlotFlagsBase = 0x1000U;
    constexpr std::uint16_t kSlotAttributesBase = 0x1080U;

    constexpr std::uint32_t kImmediate = 6U;
    constexpr std::uint32_t kZeroPage = 12U;
    constexpr std::uint32_t kAbsoluteIndexed = 15U;
    constexpr std::uint32_t kReadModifyWrite = 18U;
    constexpr std::uint32_t kBranchTaken = 12U;
    constexpr std::uint32_t kBranchNotTaken = 6U;

    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    if (work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    // Neither decimal mode nor the memory-operand T flag is proven for this
    // walk; T in particular would redirect the AND at $C51A.
    if ((entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }
    // The loop reloads X from $4D every iteration, so an entry whose X has
    // drifted from the counter is a state this walk was never proven for.
    if (std::to_integer<std::uint8_t>(work_ram[kCounterAddress - 0x2000U])
        != entry.registers.x) {
        return std::nullopt;
    }

    dc_actor_scan_run run{};
    if (!dc_classify_actor_scan_run(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            static_cast<std::uint8_t>(DC_ACTOR_SCAN_SLOT_COUNT),
            &run)) {
        return std::nullopt;
    }
    // Given the whole slot count as its budget the classifier always reaches
    // a dispatch or the terminal comparison first, because the slot index
    // hits $80 no later than the skip count does. A budget exit here would
    // be a modelling error, not a state to run.
    if (run.exit == DC_ACTOR_SCAN_BUDGET) {
        return std::nullopt;
    }

    // Only N, Z, and C move here; the loop touches no other flag.
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

    auto registers = entry.registers;
    std::uint32_t cycle = 0U;
    RoutineReplacement replacement{};
    // Ten boundaries and one write per masked slot, plus a six-instruction
    // dispatch prefix: 1,286 checkpoints and 128 writes at the very worst,
    // inside the core's 2,048 and 512.
    replacement.checkpoints.reserve(
        static_cast<std::size_t>(run.skipped) * 10U + 6U);
    replacement.work_ram_writes.reserve(run.skipped);

    const auto boundary =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                // Every instruction in this loop samples the interrupt one
                // CPU cycle before its own last, which is where the core's
                // LastCycle sits. None of them touches a hardware port, so
                // none of them pays the VDC's stolen wait.
                .interrupt_sample_master_cycle_offset = cycle - 3U,
                .master_cycle_offset = cycle,
                .instruction = {kRoutine.physical_bank, instruction_pc},
                .registers = registers,
            });
        };
    const auto step =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t cycles) {
            cycle += cycles;
            boundary(instruction_pc, resume_pc);
        };
    // LDA $3000,X and LDA $3080,X.
    const auto load = [&](const std::uint16_t base, const std::uint8_t index) {
        const auto value = std::to_integer<std::uint8_t>(
            work_ram[static_cast<std::size_t>(base) + index]);
        registers.accumulator = value;
        registers.status = set_nz(registers.status, value);
        return value;
    };

    auto slot = entry.registers.x;
    for (std::uint8_t walked = 0U; walked < run.skipped; ++walked) {
        const auto flags = load(kSlotFlagsBase, slot);
        step(kSlotFlagsPc, kIdleBranchPc, kAbsoluteIndexed);
        if ((flags & 0x80U) == 0U) {
            step(kIdleBranchPc, kIncrementPc, kBranchTaken);
        } else {
            step(kIdleBranchPc, kAttributesPc, kBranchNotTaken);
            const auto attributes = load(kSlotAttributesBase, slot);
            step(kAttributesPc, kNegativeBranchPc, kAbsoluteIndexed);
            step(kNegativeBranchPc, kMaskPc, kBranchNotTaken);
            registers.accumulator =
                static_cast<std::uint8_t>(attributes & 0x02U);
            registers.status =
                set_nz(registers.status, registers.accumulator);
            step(kMaskPc, kMaskBranchPc, kImmediate);
            step(kMaskBranchPc, kIncrementPc, kBranchTaken);
        }

        // INC $4D leaves its own N and Z standing until LDX $4D reloads
        // them, and an interrupt can arrive between the two.
        const auto next = static_cast<std::uint8_t>(slot + 1U);
        registers.status = set_nz(registers.status, next);
        cycle += kReadModifyWrite;
        replacement.work_ram_writes.push_back({
            .instruction = {kRoutine.physical_bank, kIncrementPc},
            .logical_address = kCounterAddress,
            .value = next,
            .master_cycle_offset = cycle,
        });
        boundary(kIncrementPc, kReloadPc);
        registers.x = next;
        registers.status = set_nz(registers.status, next);
        step(kReloadPc, kComparePc, kZeroPage);
        // CPX #$80 against a slot below $80 borrows, leaving carry clear and
        // the difference negative.
        registers.status = set_nz(
            registers.status, static_cast<std::uint8_t>(next - 0x80U));
        registers.status = static_cast<std::uint8_t>(
            next >= 0x80U
                ? (registers.status | 0x01U)
                : (registers.status & static_cast<std::uint8_t>(~0x01U)));
        step(kComparePc, kLoopBranchPc, kImmediate);
        slot = next;
        if (slot == DC_ACTOR_SCAN_SLOT_COUNT) {
            step(kLoopBranchPc, kTerminalPc, kBranchNotTaken);
        } else {
            step(kLoopBranchPc, kSlotFlagsPc, kBranchTaken);
        }
    }

    switch (run.exit) {
    case DC_ACTOR_SCAN_DISPATCH_NEGATIVE:
        load(kSlotFlagsBase, slot);
        step(kSlotFlagsPc, kIdleBranchPc, kAbsoluteIndexed);
        step(kIdleBranchPc, kAttributesPc, kBranchNotTaken);
        load(kSlotAttributesBase, slot);
        step(kAttributesPc, kNegativeBranchPc, kAbsoluteIndexed);
        step(kNegativeBranchPc, kDispatchPc, kBranchTaken);
        break;
    case DC_ACTOR_SCAN_DISPATCH_MASKED: {
        load(kSlotFlagsBase, slot);
        step(kSlotFlagsPc, kIdleBranchPc, kAbsoluteIndexed);
        step(kIdleBranchPc, kAttributesPc, kBranchNotTaken);
        const auto attributes = load(kSlotAttributesBase, slot);
        step(kAttributesPc, kNegativeBranchPc, kAbsoluteIndexed);
        step(kNegativeBranchPc, kMaskPc, kBranchNotTaken);
        registers.accumulator =
            static_cast<std::uint8_t>(attributes & 0x02U);
        registers.status = set_nz(registers.status, registers.accumulator);
        step(kMaskPc, kMaskBranchPc, kImmediate);
        // AND #$02 cleared the accumulator, so the BNE fell through.
        step(kMaskBranchPc, kDispatchPc, kBranchNotTaken);
        break;
    }
    case DC_ACTOR_SCAN_COMPLETE:
        break;
    default:
        return std::nullopt;
    }

    // The transcript and the classifier must agree about where the walk ends
    // up; a disagreement is a modelling error, not a state to run.
    if (replacement.checkpoints.empty()
        || registers.accumulator != run.accumulator
        || registers.x != run.slot
        || slot != run.slot
        || registers.program_counter
            != (run.exit == DC_ACTOR_SCAN_COMPLETE ? kTerminalPc
                                                   : kDispatchPc)) {
        return std::nullopt;
    }

    replacement.return_instruction = {
        .physical_bank = kRoutine.physical_bank,
        .logical_pc = registers.program_counter,
    };
    replacement.output_registers = registers;
    replacement.body_master_cycles = cycle;

    // The counter reaching its limit ends the scan on its own `RTS`; a
    // dispatch leaves at `01:C51E` and is a handover, not a return. The walk
    // never touches the stack, so the pointer the entry carried is the one the
    // return is reached with, exactly as at the tail in the next routine.
    //
    // This is the family the absorbed return waited on a fork patch for, and
    // it is worth recording what the wait was about. This walk is both long
    // enough to be cut short often and re-entered at the same stack depth. A
    // transaction stages the return it promises when it is *installed*, and
    // the promise is redeemed at the boundary the machine lands on afterwards
    // -- so a transcript the core gives up on part-way through used to leave a
    // promise standing with nothing to redeem it, and the next arrival at the
    // same address and depth redeemed it instead, closing a live call with an
    // abandoned transcript's registers. 82,201 of 176,861 calls reported the
    // wrong Y and 42,753 the wrong MPR4/MPR5, with every cycle right.
    //
    // Withdrawing on the core's own `RTS` was tried and fixed three calls in
    // eight, because a cut-short scan usually leaves through the dispatch at
    // `01:C52C` and never reaches `01:C52B` at all. What fixes it is the core
    // saying when it abandons a transcript, which is
    // `patches/0020-huc6280-replacement-abandonment.patch`.
    if (registers.program_counter == kTerminalPc) {
        absorb_return(
            replacement,
            work_ram,
            entry.registers.stack_pointer,
            {kRoutine.physical_bank, kTerminalPc},
            cycle,
            registers);
    }
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_actor_scan_advance(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The main actor loop's own tail. The `01:C510` walk absorbs this
    // sequence for every slot it skips, but a dispatched slot leaves the
    // transaction at the handler, so the increment that follows the handler's
    // return is a separate arrival. That is the ~8.2M-cycle ceiling the sweep
    // slice recorded as structurally emulated; this closes it.
    //
    //   C523 INC $4D 18   C525 LDX $4D 12
    //   C527 CPX #$80 6   C529 BNE $C510 6/12
    //   C52B RTS          (stays emulated)
    //
    // There is no snapshot to classify beyond the counter itself: the whole
    // body is one increment and the loop's terminal comparison.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xC523U,
    };
    constexpr std::uint16_t kIncrementPc = 0xC523U;
    constexpr std::uint16_t kReloadPc = 0xC525U;
    constexpr std::uint16_t kComparePc = 0xC527U;
    constexpr std::uint16_t kLoopBranchPc = 0xC529U;
    constexpr std::uint16_t kLoopHeadPc = 0xC510U;
    constexpr std::uint16_t kTerminalPc = 0xC52BU;
    constexpr std::uint16_t kCounterAddress = 0x204DU;

    constexpr std::uint32_t kImmediate = 6U;
    constexpr std::uint32_t kZeroPage = 12U;
    constexpr std::uint32_t kReadModifyWrite = 18U;
    constexpr std::uint32_t kBranchTaken = 12U;
    constexpr std::uint32_t kBranchNotTaken = 6U;

    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    if (work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    // The memory-operand T flag would redirect the CPX comparison; neither it
    // nor decimal mode is proven for this tail.
    if ((entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    // Only N, Z, and C move here.
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

    auto registers = entry.registers;
    std::uint32_t cycle = 0U;
    RoutineReplacement replacement{};
    replacement.checkpoints.reserve(4U);
    replacement.work_ram_writes.reserve(1U);

    const auto boundary =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = cycle - 3U,
                .master_cycle_offset = cycle,
                .instruction = {kRoutine.physical_bank, instruction_pc},
                .registers = registers,
            });
        };
    const auto step =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t cycles) {
            cycle += cycles;
            boundary(instruction_pc, resume_pc);
        };

    // INC $4D leaves its own N and Z standing until LDX $4D reloads them, and
    // an interrupt can arrive between the two.
    const auto next = static_cast<std::uint8_t>(
        std::to_integer<std::uint8_t>(work_ram[kCounterAddress - 0x2000U])
        + 1U);
    registers.status = set_nz(registers.status, next);
    cycle += kReadModifyWrite;
    replacement.work_ram_writes.push_back({
        .instruction = {kRoutine.physical_bank, kIncrementPc},
        .logical_address = kCounterAddress,
        .value = next,
        .master_cycle_offset = cycle,
    });
    boundary(kIncrementPc, kReloadPc);

    registers.x = next;
    registers.status = set_nz(registers.status, next);
    step(kReloadPc, kComparePc, kZeroPage);

    // CPX #$80 against a counter below $80 borrows, leaving carry clear and
    // the difference negative.
    registers.status =
        set_nz(registers.status, static_cast<std::uint8_t>(next - 0x80U));
    registers.status = static_cast<std::uint8_t>(
        next >= 0x80U
            ? (registers.status | 0x01U)
            : (registers.status & static_cast<std::uint8_t>(~0x01U)));
    step(kComparePc, kLoopBranchPc, kImmediate);

    if (next == DC_ACTOR_SCAN_SLOT_COUNT) {
        step(kLoopBranchPc, kTerminalPc, kBranchNotTaken);
    } else {
        step(kLoopBranchPc, kLoopHeadPc, kBranchTaken);
    }

    replacement.return_instruction = {
        .physical_bank = kRoutine.physical_bank,
        .logical_pc = registers.program_counter,
    };
    replacement.output_registers = registers;
    replacement.body_master_cycles = cycle;

    // The counter reaching its limit ends the loop on its `RTS`; anything
    // short of that branches back to the loop head, which is a continuation
    // and not a return.
    if (registers.program_counter == kTerminalPc) {
        absorb_return(
            replacement,
            work_ram,
            entry.registers.stack_pointer,
            {kRoutine.physical_bank, kTerminalPc},
            cycle,
            registers);
    }
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_actor_dispatch_vector(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The dispatch half of the main actor loop. The loop's BSR lands here
    // with the slot in X; this copies that slot's handler vector out of the
    // $3F00/$3F80 jump table into the zero-page indirection at $2F/$30 and
    // jumps through it. The handler stays emulated, so the transcript ends
    // on the handler's own first instruction.
    //
    //   C52C LDA $3F00,X 15   C52F STA $2F 12
    //   C531 LDA $3F80,X 15   C534 STA $30 12
    //   C536 JMP ($202F) 21   (resumes inside the handler)
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xC52CU,
    };
    constexpr std::uint16_t kLoadLowPc = 0xC52CU;
    constexpr std::uint16_t kStoreLowPc = 0xC52FU;
    constexpr std::uint16_t kLoadHighPc = 0xC531U;
    constexpr std::uint16_t kStoreHighPc = 0xC534U;
    constexpr std::uint16_t kJumpPc = 0xC536U;
    constexpr std::uint16_t kVectorLowAddress = 0x202FU;
    constexpr std::uint16_t kVectorHighAddress = 0x2030U;

    constexpr std::uint32_t kZeroPage = 12U;
    constexpr std::uint32_t kAbsoluteIndexed = 15U;
    constexpr std::uint32_t kIndirectJump = 21U;

    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    if (work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    // The memory-operand T flag would redirect both loads; neither it nor
    // decimal mode is proven for this dispatch.
    if ((entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_actor_dispatch_vector vector{};
    if (!dc_classify_actor_dispatch_vector(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &vector)) {
        return std::nullopt;
    }

    // Only N and Z move here; the two stores and the jump touch no flag.
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

    auto registers = entry.registers;
    std::uint32_t cycle = 0U;
    RoutineReplacement replacement{};
    replacement.checkpoints.reserve(5U);
    replacement.work_ram_writes.reserve(2U);

    const auto boundary =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = cycle - 3U,
                .master_cycle_offset = cycle,
                .instruction = {kRoutine.physical_bank, instruction_pc},
                .registers = registers,
            });
        };
    const auto step =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t cycles) {
            cycle += cycles;
            boundary(instruction_pc, resume_pc);
        };
    const auto store =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t logical_address,
            const std::uint8_t value) {
            cycle += kZeroPage;
            replacement.work_ram_writes.push_back({
                .instruction = {kRoutine.physical_bank, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = cycle,
            });
            boundary(instruction_pc, resume_pc);
        };

    registers.accumulator = vector.vector_low;
    registers.status = set_nz(registers.status, vector.vector_low);
    step(kLoadLowPc, kStoreLowPc, kAbsoluteIndexed);
    store(kStoreLowPc, kLoadHighPc, kVectorLowAddress, vector.vector_low);
    registers.accumulator = vector.vector_high;
    registers.status = set_nz(registers.status, vector.vector_high);
    step(kLoadHighPc, kStoreHighPc, kAbsoluteIndexed);
    store(kStoreHighPc, kJumpPc, kVectorHighAddress, vector.vector_high);
    // JMP ($202F) reads back the pair just written, so the resume address is
    // the vector itself. The bank is whatever MPR already maps for it: the
    // jump changes no mapping.
    step(kJumpPc, vector.handler, kIndirectJump);

    replacement.return_instruction = {
        .physical_bank = entry.registers.mpr[vector.handler >> 13U],
        .logical_pc = vector.handler,
    };
    replacement.output_registers = registers;
    replacement.body_master_cycles = cycle;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_actor_queue_registration(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The main actor loop calls this once per dispatched slot, after the
    // handler has returned. It files the slot into one of the three
    // sprite-emission queues the bank-08 satellite emitter drains, or drops
    // it. Declared as an instruction-boundary transcript for the reason the
    // two bank-01 sweeps were: priced atomically a ~200-cycle body spends
    // most of its arrivals losing to the interrupt horizon.
    //
    //   C606 CPX #$00     6   C608 BEQ $C654 6/12
    //   C60A LDA $3080,X 15   C60D BPL $C654 6/12
    //   C60F LDX $4D     12   C611 LDA $3080,X 15
    //   C614 AND #$01     6   C616 BEQ $C627 6/12
    //   C618 LDA $3800,X 15   C61B BIT #$40   6
    //   C61D BEQ $C627  6/12  C61F AND #$BF   6
    //   C621 STA $3800,X 15   C624 JSR $F605  (stays emulated)
    //   C627 LDA $3080,X 15   C62A BIT #$20   6
    //   C62C BNE $C654  6/12  C62E BIT #$04   6
    //   C630 BEQ $C63E  6/12  C632 LDY $2CBC 15
    //   C635 INC $2CBC   21   C638 TXA        6
    //   C639 STA $2C3A,Y 15   C63C BRA $C654 12
    //   C63E BIT #$08     6   C640 BEQ $C64A 6/12
    //   C642 LDY $2CBA   15   C645 INC $2CBA 21
    //   C648 BRA $C650   12   C64A LDY $2CBB 15
    //   C64D DEC $2CBB   21   C650 TXA        6
    //   C651 STA $2BBA,Y 15   C654 RTS       (stays emulated)
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xC606U,
    };
    constexpr std::uint16_t kEntryComparePc = 0xC606U;
    constexpr std::uint16_t kIdleBranchPc = 0xC608U;
    constexpr std::uint16_t kEntryLoadPc = 0xC60AU;
    constexpr std::uint16_t kInactiveBranchPc = 0xC60DU;
    constexpr std::uint16_t kCursorLoadPc = 0xC60FU;
    constexpr std::uint16_t kAttributesPc = 0xC611U;
    constexpr std::uint16_t kClearMaskPc = 0xC614U;
    constexpr std::uint16_t kClearBranchPc = 0xC616U;
    constexpr std::uint16_t kStateLoadPc = 0xC618U;
    constexpr std::uint16_t kStateTestPc = 0xC61BU;
    constexpr std::uint16_t kStateBranchPc = 0xC61DU;
    constexpr std::uint16_t kClearPc = 0xC61FU;
    constexpr std::uint16_t kStatePc = 0xC621U;
    constexpr std::uint16_t kHelperPc = 0xC624U;
    constexpr std::uint16_t kReloadPc = 0xC627U;
    constexpr std::uint16_t kSuppressTestPc = 0xC62AU;
    constexpr std::uint16_t kSuppressBranchPc = 0xC62CU;
    constexpr std::uint16_t kFirstTestPc = 0xC62EU;
    constexpr std::uint16_t kFirstBranchPc = 0xC630U;
    constexpr std::uint16_t kFirstCursorPc = 0xC632U;
    constexpr std::uint16_t kFirstBumpPc = 0xC635U;
    constexpr std::uint16_t kFirstTransferPc = 0xC638U;
    constexpr std::uint16_t kFirstStorePc = 0xC639U;
    constexpr std::uint16_t kFirstExitPc = 0xC63CU;
    constexpr std::uint16_t kSecondTestPc = 0xC63EU;
    constexpr std::uint16_t kSecondBranchPc = 0xC640U;
    constexpr std::uint16_t kSecondCursorPc = 0xC642U;
    constexpr std::uint16_t kSecondBumpPc = 0xC645U;
    constexpr std::uint16_t kSecondJoinPc = 0xC648U;
    constexpr std::uint16_t kThirdCursorPc = 0xC64AU;
    constexpr std::uint16_t kThirdBumpPc = 0xC64DU;
    constexpr std::uint16_t kSharedTransferPc = 0xC650U;
    constexpr std::uint16_t kSharedStorePc = 0xC651U;
    constexpr std::uint16_t kTerminalPc = 0xC654U;

    constexpr std::uint16_t kSlotStateBase = 0x3800U;
    constexpr std::uint16_t kFirstQueueBase = 0x2C3AU;
    constexpr std::uint16_t kSharedQueueBase = 0x2BBAU;
    constexpr std::uint16_t kFirstCursorAddress = 0x2CBCU;
    constexpr std::uint16_t kSecondCursorAddress = 0x2CBAU;
    constexpr std::uint16_t kThirdCursorAddress = 0x2CBBU;

    constexpr std::uint32_t kImmediate = 6U;
    constexpr std::uint32_t kZeroPage = 12U;
    constexpr std::uint32_t kAbsoluteIndexed = 15U;
    // INC and DEC on an absolute address are seven cycles, one more than
    // the zero-page form the scan loop's own counter uses.
    constexpr std::uint32_t kAbsoluteReadModifyWrite = 21U;
    constexpr std::uint32_t kBranchTaken = 12U;
    constexpr std::uint32_t kBranchNotTaken = 6U;

    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    if (work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    // The T flag would redirect the AND and both BIT masks; neither it nor
    // decimal mode is proven for this routine.
    if ((entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_actor_queue_registration registration{};
    if (!dc_classify_actor_queue_registration(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &registration)) {
        return std::nullopt;
    }

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

    auto registers = entry.registers;
    std::uint32_t cycle = 0U;
    RoutineReplacement replacement{};
    // The longest path is twenty-one boundaries and two writes.
    replacement.checkpoints.reserve(21U);
    replacement.work_ram_writes.reserve(2U);

    const auto boundary =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = cycle - 3U,
                .master_cycle_offset = cycle,
                .instruction = {kRoutine.physical_bank, instruction_pc},
                .registers = registers,
            });
        };
    const auto step =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t cycles) {
            cycle += cycles;
            boundary(instruction_pc, resume_pc);
        };
    const auto write =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t cycles,
            const std::uint16_t logical_address,
            const std::uint8_t value) {
            cycle += cycles;
            replacement.work_ram_writes.push_back({
                .instruction = {kRoutine.physical_bank, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = cycle,
            });
            boundary(instruction_pc, resume_pc);
        };
    const auto load = [&](const std::uint8_t value) {
        registers.accumulator = value;
        registers.status = set_nz(registers.status, value);
    };
    // BIT against an immediate. Beetle clears N, V, and Z and then takes N
    // and V straight from the operand, in immediate mode as well as the
    // addressing modes where the 65C02 does, so only Z reports the test.
    const auto bit_immediate = [&](const std::uint8_t operand) {
        registers.status = static_cast<std::uint8_t>(
            registers.status & static_cast<std::uint8_t>(~0xC2U));
        if ((registers.accumulator & operand) == 0U) {
            registers.status =
                static_cast<std::uint8_t>(registers.status | 0x02U);
        }
        registers.status = static_cast<std::uint8_t>(
            registers.status | (operand & 0xC0U));
    };

    const auto walk = [&]() -> std::uint16_t {
        // CPX #$00 against an unsigned slot never borrows, so carry is set
        // and N and Z simply report X.
        registers.status = set_nz(registers.status, entry.registers.x);
        registers.status =
            static_cast<std::uint8_t>(registers.status | 0x01U);
        step(kEntryComparePc, kIdleBranchPc, kImmediate);
        if (registration.exit == DC_ACTOR_QUEUE_IDLE_SLOT) {
            step(kIdleBranchPc, kTerminalPc, kBranchTaken);
            return kTerminalPc;
        }
        step(kIdleBranchPc, kEntryLoadPc, kBranchNotTaken);

        load(registration.entry_attributes);
        step(kEntryLoadPc, kInactiveBranchPc, kAbsoluteIndexed);
        if (registration.exit == DC_ACTOR_QUEUE_INACTIVE) {
            step(kInactiveBranchPc, kTerminalPc, kBranchTaken);
            return kTerminalPc;
        }
        step(kInactiveBranchPc, kCursorLoadPc, kBranchNotTaken);

        // LDX $4D discards whatever the dispatched handler left in X.
        registers.x = registration.slot;
        registers.status = set_nz(registers.status, registration.slot);
        step(kCursorLoadPc, kAttributesPc, kZeroPage);
        load(registration.attributes);
        step(kAttributesPc, kClearMaskPc, kAbsoluteIndexed);
        load(static_cast<std::uint8_t>(registration.attributes & 0x01U));
        step(kClearMaskPc, kClearBranchPc, kImmediate);
        if ((registration.attributes & 0x01U) == 0U) {
            step(kClearBranchPc, kReloadPc, kBranchTaken);
        } else {
            step(kClearBranchPc, kStateLoadPc, kBranchNotTaken);
            load(registration.clear_state);
            step(kStateLoadPc, kStateTestPc, kAbsoluteIndexed);
            bit_immediate(0x40U);
            step(kStateTestPc, kStateBranchPc, kImmediate);
            if ((registration.clear_state & 0x40U) == 0U) {
                step(kStateBranchPc, kReloadPc, kBranchTaken);
            } else {
                step(kStateBranchPc, kClearPc, kBranchNotTaken);
                load(registration.cleared_state);
                step(kClearPc, kStatePc, kImmediate);
                write(
                    kStatePc,
                    kHelperPc,
                    kAbsoluteIndexed,
                    static_cast<std::uint16_t>(
                        kSlotStateBase + registration.slot),
                    registration.cleared_state);
                // The one-shot clear hands off to the emulated helper.
                return kHelperPc;
            }
        }

        load(registration.attributes);
        step(kReloadPc, kSuppressTestPc, kAbsoluteIndexed);
        bit_immediate(0x20U);
        step(kSuppressTestPc, kSuppressBranchPc, kImmediate);
        if (registration.exit == DC_ACTOR_QUEUE_SUPPRESSED) {
            step(kSuppressBranchPc, kTerminalPc, kBranchTaken);
            return kTerminalPc;
        }
        step(kSuppressBranchPc, kFirstTestPc, kBranchNotTaken);

        bit_immediate(0x04U);
        step(kFirstTestPc, kFirstBranchPc, kImmediate);
        if (registration.exit == DC_ACTOR_QUEUE_FIRST) {
            step(kFirstBranchPc, kFirstCursorPc, kBranchNotTaken);
            registers.y = registration.cursor;
            registers.status = set_nz(registers.status, registration.cursor);
            step(kFirstCursorPc, kFirstBumpPc, kAbsoluteIndexed);
            registers.status =
                set_nz(registers.status, registration.updated_cursor);
            write(
                kFirstBumpPc,
                kFirstTransferPc,
                kAbsoluteReadModifyWrite,
                kFirstCursorAddress,
                registration.updated_cursor);
            registers.accumulator = registers.x;
            registers.status = set_nz(registers.status, registers.x);
            step(kFirstTransferPc, kFirstStorePc, kImmediate);
            write(
                kFirstStorePc,
                kFirstExitPc,
                kAbsoluteIndexed,
                static_cast<std::uint16_t>(
                    kFirstQueueBase + registration.queue_index),
                registers.accumulator);
            step(kFirstExitPc, kTerminalPc, kBranchTaken);
            return kTerminalPc;
        }
        step(kFirstBranchPc, kSecondTestPc, kBranchTaken);

        bit_immediate(0x08U);
        step(kSecondTestPc, kSecondBranchPc, kImmediate);
        if (registration.exit == DC_ACTOR_QUEUE_SECOND) {
            step(kSecondBranchPc, kSecondCursorPc, kBranchNotTaken);
            registers.y = registration.cursor;
            registers.status = set_nz(registers.status, registration.cursor);
            step(kSecondCursorPc, kSecondBumpPc, kAbsoluteIndexed);
            registers.status =
                set_nz(registers.status, registration.updated_cursor);
            write(
                kSecondBumpPc,
                kSecondJoinPc,
                kAbsoluteReadModifyWrite,
                kSecondCursorAddress,
                registration.updated_cursor);
            step(kSecondJoinPc, kSharedTransferPc, kBranchTaken);
        } else {
            step(kSecondBranchPc, kThirdCursorPc, kBranchTaken);
            registers.y = registration.cursor;
            registers.status = set_nz(registers.status, registration.cursor);
            step(kThirdCursorPc, kThirdBumpPc, kAbsoluteIndexed);
            registers.status =
                set_nz(registers.status, registration.updated_cursor);
            write(
                kThirdBumpPc,
                kSharedTransferPc,
                kAbsoluteReadModifyWrite,
                kThirdCursorAddress,
                registration.updated_cursor);
        }

        registers.accumulator = registers.x;
        registers.status = set_nz(registers.status, registers.x);
        step(kSharedTransferPc, kSharedStorePc, kImmediate);
        write(
            kSharedStorePc,
            kTerminalPc,
            kAbsoluteIndexed,
            static_cast<std::uint16_t>(
                kSharedQueueBase + registration.queue_index),
            registers.accumulator);
        return kTerminalPc;
    };

    const auto resume = walk();
    // The transcript and the classifier must agree about where the routine
    // ends up; a disagreement is a modelling error, not a state to run.
    if (replacement.checkpoints.empty()
        || registers.program_counter != resume
        || (resume == kHelperPc)
            != (registration.exit == DC_ACTOR_QUEUE_CLEAR_REQUEST)
        || (resume != kTerminalPc && resume != kHelperPc)) {
        return std::nullopt;
    }

    replacement.return_instruction = {
        .physical_bank = kRoutine.physical_bank,
        .logical_pc = resume,
    };
    replacement.output_registers = registers;
    replacement.body_master_cycles = cycle;
    return replacement;
}

// The three linkage instructions that hold the main actor loop together. Every
// other instruction between `01:C510` and `01:C654` is already native, so what
// is left is the call into the dispatch half, the call into the queue filer,
// and the queue filer's return. They are pure stack traffic and carry no game
// state, but they are also the loop's whole remaining cost, so each gets an
// exact transcript rather than staying emulated for want of a classifier.
//
// Mednafen pushes before it charges any cycle for JSR and BSR (huc6280_ops.inc
// cases $20 and $44 both PUSH twice, then ADDCYC), so both stack writes carry
// the instruction's *start* offset. RTS pops, which is a read, so it writes
// nothing at all.
namespace {

constexpr std::uint32_t kSubroutineCall = 21U;
constexpr std::uint32_t kBranchSubroutineCall = 24U;
constexpr std::uint32_t kSubroutineReturn = 21U;

// Push `value` as `instruction` would at `offset`, in hardware order: high
// byte at $2100|S, low byte at $2100|(S-1), stack pointer down two.
void push_return_address(
    RoutineReplacement& replacement,
    emulation::CpuRegisters& registers,
    const RoutineKey instruction,
    const std::uint16_t value,
    const std::uint32_t offset)
{
    replacement.work_ram_writes.push_back({
        .instruction = instruction,
        .logical_address = static_cast<std::uint16_t>(
            0x2100U + registers.stack_pointer),
        .value = static_cast<std::uint8_t>(value >> 8U),
        .master_cycle_offset = offset,
    });
    replacement.work_ram_writes.push_back({
        .instruction = instruction,
        .logical_address = static_cast<std::uint16_t>(
            0x2100U
            + static_cast<std::uint8_t>(registers.stack_pointer - 1U)),
        .value = static_cast<std::uint8_t>(value & 0x00FFU),
        .master_cycle_offset = offset,
    });
    registers.stack_pointer =
        static_cast<std::uint8_t>(registers.stack_pointer - 2U);
}

}  // namespace

[[nodiscard]] std::optional<RoutineReplacement>
replace_actor_dispatch_call(
    const RoutineEntry& entry)
{
    // C51E BSR $C52C 24 — the loop's call into the dispatch half. The target
    // is already native, so the transcript is the push pair and nothing else.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xC51EU,
    };
    constexpr std::uint16_t kPushedReturn = 0xC51FU;
    constexpr std::uint16_t kDispatchPc = 0xC52CU;

    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    auto registers = entry.registers;
    RoutineReplacement replacement{};
    replacement.work_ram_writes.reserve(2U);
    push_return_address(
        replacement,
        registers,
        kRoutine,
        kPushedReturn,
        0U);

    // BSR touches no flag, so the only register news is the new stack pointer.
    registers.program_counter = kDispatchPc;
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = kBranchSubroutineCall - 3U,
        .master_cycle_offset = kBranchSubroutineCall,
        .instruction = kRoutine,
        .registers = registers,
    });
    replacement.return_instruction = {
        .physical_bank = kRoutine.physical_bank,
        .logical_pc = kDispatchPc,
    };
    replacement.output_registers = registers;
    replacement.body_master_cycles = kBranchSubroutineCall;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_actor_queue_call(
    const RoutineEntry& entry)
{
    // C520 JSR $C606 21 — reached when the dispatched handler returns. The
    // queue filer it calls is itself native.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xC520U,
    };
    constexpr std::uint16_t kPushedReturn = 0xC522U;
    constexpr std::uint16_t kQueuePc = 0xC606U;

    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    auto registers = entry.registers;
    RoutineReplacement replacement{};
    replacement.work_ram_writes.reserve(2U);
    push_return_address(
        replacement,
        registers,
        kRoutine,
        kPushedReturn,
        0U);

    registers.program_counter = kQueuePc;
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = kSubroutineCall - 3U,
        .master_cycle_offset = kSubroutineCall,
        .instruction = kRoutine,
        .registers = registers,
    });
    replacement.return_instruction = {
        .physical_bank = kRoutine.physical_bank,
        .logical_pc = kQueuePc,
    };
    replacement.output_registers = registers;
    replacement.body_master_cycles = kSubroutineCall;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_actor_queue_return(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // C654 RTS 21 — the queue filer's return, and the last emulated
    // instruction in the loop. Both of the filer's exits reach it, so this is
    // where the loop hands back to the already-native tail at `01:C523`.
    //
    // The return address is read out of the stack rather than assumed: `C654`
    // is only this loop's return when the frame under the stack pointer is the
    // one `C520` pushed.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xC654U,
    };
    constexpr std::uint16_t kPushedReturn = 0xC522U;
    constexpr std::uint16_t kAdvancePc = 0xC523U;

    if (entry.routine != kRoutine || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }

    const auto low_offset = static_cast<std::size_t>(
        0x0100U
        + static_cast<std::uint8_t>(entry.registers.stack_pointer + 1U));
    const auto high_offset = static_cast<std::size_t>(
        0x0100U
        + static_cast<std::uint8_t>(entry.registers.stack_pointer + 2U));
    const auto pulled = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(work_ram[low_offset])
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(work_ram[high_offset]))
           << 8U));
    if (pulled != kPushedReturn) {
        return std::nullopt;
    }

    // RTS touches no flag either; it pops two bytes and resumes past them.
    auto registers = entry.registers;
    registers.stack_pointer =
        static_cast<std::uint8_t>(registers.stack_pointer + 2U);
    registers.program_counter = kAdvancePc;

    RoutineReplacement replacement{};
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = kSubroutineReturn - 3U,
        .master_cycle_offset = kSubroutineReturn,
        .instruction = kRoutine,
        .registers = registers,
    });
    replacement.return_instruction = {
        .physical_bank = kRoutine.physical_bank,
        .logical_pc = kAdvancePc,
    };
    replacement.output_registers = registers;
    replacement.body_master_cycles = kSubroutineReturn;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_actor_camera_sweep(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The loop around the already-native camera-activation test at `00:F4FC`,
    // which decides for each of the 127 actor slots whether bit 7 of its
    // attribute byte -- the bit `01:C510` and `08:407C` both gate on -- should
    // be set for this frame.
    //
    //   F4F2 CLX        6   F4F3 JSR $F4FC 21
    //   F4F6 INX        6   F4F7 CPX #$7F   6
    //   F4F9 BNE $F4F3 6/12 F4FB RTS        (stays emulated)
    //
    // Like the SAT submission driver this is one shared walk over the
    // addresses control arrives at with no replacement active: the loop's
    // initializer, and each of the three `RTS` instructions the child returns
    // through. Every walk ends on the next `JSR`, which the machine executes
    // and which re-enters the child's own replacement.
    //
    // Nothing here reads a memory operand or does arithmetic, so neither
    // decimal mode nor the T flag can reach this transcript and neither is
    // guarded against.
    constexpr std::uint8_t kBank = 0x00U;
    constexpr std::uint16_t kSweepInitPc = 0xF4F2U;
    constexpr std::uint16_t kCallPc = 0xF4F3U;
    constexpr std::uint16_t kAdvancePc = 0xF4F6U;
    constexpr std::uint16_t kComparePc = 0xF4F7U;
    constexpr std::uint16_t kLoopBranchPc = 0xF4F9U;
    constexpr std::uint16_t kTerminalPc = 0xF4FBU;
    constexpr std::uint16_t kChildPc = 0xF4FCU;
    // The child's three exits: off-camera clear, inside-camera clear, and the
    // below-camera set.
    constexpr std::uint16_t kClearedReturnPc = 0xF523U;
    constexpr std::uint16_t kInsideReturnPc = 0xF545U;
    constexpr std::uint16_t kBelowReturnPc = 0xF54EU;
    constexpr std::uint16_t kPushedReturn = 0xF4F5U;
    constexpr std::uint8_t kSlotCount = 0x7FU;

    constexpr std::uint32_t kImplied = 6U;
    constexpr std::uint32_t kImmediate = 6U;
    constexpr std::uint32_t kBranchTaken = 12U;
    constexpr std::uint32_t kBranchNotTaken = 6U;

    if (entry.routine.physical_bank != kBank
        || (entry.routine.logical_pc != kSweepInitPc
            && entry.routine.logical_pc != kClearedReturnPc
            && entry.routine.logical_pc != kInsideReturnPc
            && entry.routine.logical_pc != kBelowReturnPc)) {
        return std::nullopt;
    }
    if (work_ram.size() < 0x2000U) {
        return std::nullopt;
    }

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

    auto registers = entry.registers;
    std::uint32_t cycle = 0U;
    RoutineReplacement replacement{};
    replacement.checkpoints.reserve(5U);
    replacement.work_ram_writes.reserve(2U);

    const auto boundary =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = cycle - 3U,
                .master_cycle_offset = cycle,
                .instruction = {kBank, instruction_pc},
                .registers = registers,
            });
        };
    const auto step =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t cycles) {
            cycle += cycles;
            boundary(instruction_pc, resume_pc);
        };

    auto pc = entry.routine.logical_pc;
    for (;;) {
        switch (pc) {
        case kSweepInitPc:
            // CLX clears X without touching a flag.
            registers.x = 0U;
            step(kSweepInitPc, kCallPc, kImplied);
            pc = kCallPc;
            break;

        case kClearedReturnPc:
        case kInsideReturnPc:
        case kBelowReturnPc: {
            // These addresses are only this sweep's returns when the frame
            // under the stack pointer is the one the loop's JSR pushed.
            const auto low = std::to_integer<std::uint8_t>(
                work_ram[0x0100U
                    + static_cast<std::uint8_t>(
                        registers.stack_pointer + 1U)]);
            const auto high = std::to_integer<std::uint8_t>(
                work_ram[0x0100U
                    + static_cast<std::uint8_t>(
                        registers.stack_pointer + 2U)]);
            if (static_cast<std::uint16_t>(
                    low | (static_cast<std::uint16_t>(high) << 8U))
                != kPushedReturn) {
                return std::nullopt;
            }
            registers.stack_pointer =
                static_cast<std::uint8_t>(registers.stack_pointer + 2U);
            step(pc, kAdvancePc, kSubroutineReturn);
            pc = kAdvancePc;
            break;
        }

        case kAdvancePc:
            registers.x = static_cast<std::uint8_t>(registers.x + 1U);
            registers.status = set_nz(registers.status, registers.x);
            step(kAdvancePc, kComparePc, kImplied);
            registers.status = static_cast<std::uint8_t>(
                registers.x >= kSlotCount
                    ? (registers.status | 0x01U)
                    : (registers.status & static_cast<std::uint8_t>(~0x01U)));
            registers.status = set_nz(
                registers.status,
                static_cast<std::uint8_t>(registers.x - kSlotCount));
            step(kComparePc, kLoopBranchPc, kImmediate);
            if (registers.x == kSlotCount) {
                step(kLoopBranchPc, kTerminalPc, kBranchNotTaken);
                pc = kTerminalPc;
            } else {
                step(kLoopBranchPc, kCallPc, kBranchTaken);
                pc = kCallPc;
            }
            break;

        case kCallPc:
            push_return_address(
                replacement,
                registers,
                {kBank, kCallPc},
                kPushedReturn,
                cycle);
            step(kCallPc, kChildPc, kSubroutineCall);
            pc = kChildPc;
            break;

        default:
            // Unreachable: every pc the walk assigns is a case above.
            return std::nullopt;
        }

        if (pc == kChildPc || pc == kTerminalPc) {
            break;
        }
    }

    if (replacement.checkpoints.empty()
        || registers.program_counter != pc) {
        return std::nullopt;
    }

    replacement.return_instruction = {
        .physical_bank = kBank,
        .logical_pc = pc,
    };
    replacement.output_registers = registers;
    replacement.body_master_cycles = cycle;

    // 00:F4FB ends the sweep, and it is only ever reached from one of the
    // child's three `RTS` instructions -- 00:F4F2 always leaves through the
    // loop's own `JSR` instead. Those three entries begin by pulling the frame
    // that `JSR` pushed, which the arm above has already checked is the loop's,
    // so the pointer at 00:F4FB stands two bytes above the entry's and the
    // frame this absorbs is the one the sweep's caller pushed. Nothing composes
    // this walk.
    //
    // The atomic question is not asked because it cannot arise: a transcript
    // with no checkpoint was refused above, so this one always has at least the
    // return the walk entered on.
    if (pc == kTerminalPc
        && registers.stack_pointer
            == static_cast<std::uint8_t>(entry.registers.stack_pointer + 2U)) {
        absorb_return(
            replacement,
            work_ram,
            registers.stack_pointer,
            {kBank, kTerminalPc},
            cycle,
            registers);
    }
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_actor_slot_reset(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xC574U,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    if ((entry.registers.status & 0x08U) != 0U) {
        return std::nullopt;
    }

    dc_actor_slot_reset reset{};
    if (!dc_prepare_actor_slot_reset(
            entry.registers.x,
            &reset)) {
        return std::nullopt;
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xC594U,
        },
        .output_registers = entry.registers,
        .body_master_cycles = 3627U,
    };
    replacement.work_ram_writes.reserve(
        3U + 3U * DC_ACTOR_SLOT_FIELD_COUNT);
    replacement.work_ram_writes.push_back({
        .instruction = {0x01U, 0xC574U},
        .logical_address = static_cast<std::uint16_t>(
            0x2100U + entry.registers.stack_pointer),
        .value = entry.registers.y,
        .master_cycle_offset = 9U,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x01U, 0xC57AU},
        .logical_address = 0x202DU,
        .value = 0U,
        .master_cycle_offset = 42U,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x01U, 0xC57EU},
        .logical_address = 0x202EU,
        .value = 0x30U,
        .master_cycle_offset = 60U,
    });
    for (std::size_t index = 0U;
         index < DC_ACTOR_SLOT_FIELD_COUNT;
         ++index) {
        const auto iteration_offset =
            static_cast<std::uint32_t>(111U * index);
        const auto pointer_after = static_cast<std::uint16_t>(
            0x3000U + 0x80U * (index + 1U));
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xC581U},
            .logical_address = static_cast<std::uint16_t>(
                0x2000U + reset.work_ram_offsets[index]),
            .value = 0U,
            .master_cycle_offset = 87U + iteration_offset,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xC588U},
            .logical_address = 0x202DU,
            .value = static_cast<std::uint8_t>(pointer_after),
            .master_cycle_offset = 123U + iteration_offset,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xC58DU},
            .logical_address = 0x202EU,
            .value = static_cast<std::uint8_t>(
                pointer_after >> 8U),
            .master_cycle_offset = 153U + iteration_offset,
        });
    }

    const auto set_nz =
        [](std::uint8_t status, const std::uint8_t value) {
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xA2U));
            if (value == 0U) {
                status = static_cast<std::uint8_t>(
                    status | 0x02U);
            } else if ((value & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(
                    status | 0x80U);
            }
            return status;
        };
    const auto adc =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right) {
            const auto sum =
                static_cast<unsigned int>(left)
                + static_cast<unsigned int>(right)
                + ((status & 0x01U) != 0U ? 1U : 0U);
            const auto result =
                static_cast<std::uint8_t>(sum);
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0x41U));
            status = set_nz(status, result);
            if (sum > 0xFFU) {
                status = static_cast<std::uint8_t>(
                    status | 0x01U);
            }
            if (((~(left ^ right) & (left ^ result)) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(
                    status | 0x40U);
            }
            return std::pair{result, status};
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

    auto checkpoint = entry.registers;
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(
            checkpoint.stack_pointer - 1U);
    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status & static_cast<std::uint8_t>(~0x20U));
    append_checkpoint(0xC574U, 0xC575U, 9U, checkpoint);
    checkpoint.y = DC_ACTOR_SLOT_FIELD_COUNT;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0xC575U, 0xC577U, 15U, checkpoint);
    std::swap(checkpoint.x, checkpoint.y);
    append_checkpoint(0xC577U, 0xC578U, 24U, checkpoint);
    checkpoint.accumulator = 0U;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC578U, 0xC57AU, 30U, checkpoint);
    append_checkpoint(0xC57AU, 0xC57CU, 42U, checkpoint);
    checkpoint.accumulator = 0x30U;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC57CU, 0xC57EU, 48U, checkpoint);
    append_checkpoint(0xC57EU, 0xC580U, 60U, checkpoint);

    std::uint16_t pointer = 0x3000U;
    for (std::size_t index = 0U;
         index < DC_ACTOR_SLOT_FIELD_COUNT;
         ++index) {
        const auto iteration_offset =
            static_cast<std::uint32_t>(111U * index);
        checkpoint.accumulator = 0U;
        checkpoint.status = static_cast<std::uint8_t>(
            checkpoint.status & static_cast<std::uint8_t>(~0x20U));
        append_checkpoint(
            0xC580U,
            0xC581U,
            66U + iteration_offset,
            checkpoint);
        append_checkpoint(
            0xC581U,
            0xC583U,
            87U + iteration_offset,
            checkpoint);
        checkpoint.status = static_cast<std::uint8_t>(
            checkpoint.status
            & static_cast<std::uint8_t>(~0x21U));
        append_checkpoint(
            0xC583U,
            0xC584U,
            93U + iteration_offset,
            checkpoint);
        checkpoint.accumulator =
            static_cast<std::uint8_t>(pointer);
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(
            0xC584U,
            0xC586U,
            105U + iteration_offset,
            checkpoint);
        auto arithmetic = adc(
            checkpoint.status,
            checkpoint.accumulator,
            0x80U);
        checkpoint.accumulator = arithmetic.first;
        checkpoint.status = arithmetic.second;
        append_checkpoint(
            0xC586U,
            0xC588U,
            111U + iteration_offset,
            checkpoint);
        append_checkpoint(
            0xC588U,
            0xC58AU,
            123U + iteration_offset,
            checkpoint);
        checkpoint.accumulator = 0U;
        checkpoint.status = static_cast<std::uint8_t>(
            checkpoint.status & static_cast<std::uint8_t>(~0x20U));
        append_checkpoint(
            0xC58AU,
            0xC58BU,
            129U + iteration_offset,
            checkpoint);
        arithmetic = adc(
            checkpoint.status,
            checkpoint.accumulator,
            static_cast<std::uint8_t>(pointer >> 8U));
        pointer = static_cast<std::uint16_t>(
            pointer + 0x80U);
        checkpoint.accumulator = arithmetic.first;
        checkpoint.status = arithmetic.second;
        append_checkpoint(
            0xC58BU,
            0xC58DU,
            141U + iteration_offset,
            checkpoint);
        append_checkpoint(
            0xC58DU,
            0xC58FU,
            153U + iteration_offset,
            checkpoint);
        checkpoint.x =
            static_cast<std::uint8_t>(checkpoint.x - 1U);
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.x);
        append_checkpoint(
            0xC58FU,
            0xC590U,
            159U + iteration_offset,
            checkpoint);
        const bool repeats =
            index + 1U < DC_ACTOR_SLOT_FIELD_COUNT;
        append_checkpoint(
            0xC590U,
            repeats ? 0xC580U : 0xC592U,
            repeats
                ? 171U + iteration_offset
                : 3606U,
            checkpoint);
    }

    std::swap(checkpoint.x, checkpoint.y);
    append_checkpoint(0xC592U, 0xC593U, 3615U, checkpoint);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(
            checkpoint.stack_pointer + 1U);
    checkpoint.y = entry.registers.y;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.y);
    checkpoint.program_counter = 0xC594U;
    append_checkpoint(0xC593U, 0xC594U, 3627U, checkpoint);
    replacement.output_registers = checkpoint;

    // 01:C574 pushed Y and 01:C593 pulled it back, so the pointer at the return
    // is the entry's and the pushed byte is one below the frame it reads.
    absorb_return(
        replacement,
        work_ram,
        entry.registers.stack_pointer,
        {0x01U, 0xC594U},
        3627U,
        checkpoint);
    return replacement;
}
} // namespace devils_crush::app::detail
