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
    // The main actor loop head. Each arrival either dispatches the slot
    // through the $3F00/$3F80 jump table or skips it, and only the skip run
    // is native: a dispatch enters an emulated handler this transaction
    // cannot call. The run therefore walks idle slots and hands control back
    // at the dispatch branch, the loop head, or the terminal comparison.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xC510U,
    };
    constexpr std::uint16_t kDispatchPc = 0xC51EU;
    constexpr std::uint16_t kLoopHeadPc = 0xC510U;
    constexpr std::uint16_t kTerminalPc = 0xC52BU;
    constexpr std::uint16_t kCounterAddress = 0x204DU;

    // Master cycles for the two skip shapes and the three exits, at three
    // master cycles per CPU cycle with taken branches costing four.
    constexpr std::uint32_t kInactiveSkipCycles = 75U;   // LDA BPL INC LDX CPX BNE
    constexpr std::uint32_t kMaskedSkipCycles = 108U;    // + LDA BMI AND BNE
    constexpr std::uint32_t kInactiveWriteOffset = 45U;  // after INC $4D
    constexpr std::uint32_t kMaskedWriteOffset = 78U;
    constexpr std::uint32_t kTerminalBranchSaving = 6U;  // BNE falls through
    constexpr std::uint32_t kDispatchNegativeCycles = 48U;
    constexpr std::uint32_t kDispatchMaskedCycles = 54U;

    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    // Decimal mode is never proven for this walk.
    if ((entry.registers.status & 0x08U) != 0U) {
        return std::nullopt;
    }
    // The loop reloads X from $4D every iteration, so an entry whose X has
    // drifted from the counter is a state this walk was never proven for.
    if (work_ram.size() <= kCounterAddress - 0x2000U
        || std::to_integer<std::uint8_t>(
               work_ram[kCounterAddress - 0x2000U])
            != entry.registers.x) {
        return std::nullopt;
    }

    // Keep the atomic body inside the entry's interrupt horizons. The budget
    // is the number of cheapest skips that still fit, so a long idle tail
    // resumes at the loop head instead of being declined outright.
    const auto horizon = std::min(
        entry.master_cycles_until_user_event,
        entry.master_cycles_until_timer_interrupt);
    // The horizon must cover the worst-case shape of a walked slot plus its
    // exit before any of this run is admitted. The same floor relaxed on the
    // sibling collision sweep changed its state census, so it is kept
    // conservative here too.
    if (horizon <= kMaskedSkipCycles + kDispatchMaskedCycles) {
        return std::nullopt;
    }
    const auto budget = std::min<std::uint32_t>(
        DC_ACTOR_SCAN_SLOT_COUNT,
        (horizon - kDispatchMaskedCycles - 1U) / kMaskedSkipCycles);
    if (budget == 0U) {
        return std::nullopt;
    }

    dc_actor_scan_run run{};
    if (!dc_classify_actor_scan_run(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            static_cast<std::uint8_t>(budget),
            &run)) {
        return std::nullopt;
    }
    // A run that walks nothing would resume where it started.
    if (run.exit == DC_ACTOR_SCAN_BUDGET && run.skipped == 0U) {
        return std::nullopt;
    }

    std::uint32_t cycle = 0U;
    std::vector<emulation::ReplacementMemoryWrite> writes;
    writes.reserve(run.skipped);
    for (std::uint8_t index = 0U; index < run.skipped; ++index) {
        const bool masked =
            run.skips[index] == static_cast<std::uint8_t>(
                DC_ACTOR_SCAN_SKIP_MASKED);
        writes.push_back({
            .instruction = {0x01U, 0xC523U},
            .logical_address = kCounterAddress,
            .value = static_cast<std::uint8_t>(entry.registers.x + index + 1U),
            .master_cycle_offset = cycle
                + (masked ? kMaskedWriteOffset : kInactiveWriteOffset),
        });
        cycle += masked ? kMaskedSkipCycles : kInactiveSkipCycles;
    }

    auto output = entry.registers;
    output.accumulator = run.accumulator;
    output.x = run.slot;

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

    switch (run.exit) {
    case DC_ACTOR_SCAN_DISPATCH_NEGATIVE:
        // LDA $3080,X left the attribute byte and its N flag standing.
        cycle += kDispatchNegativeCycles;
        output.status = set_nz(output.status, run.accumulator);
        output.program_counter = kDispatchPc;
        break;
    case DC_ACTOR_SCAN_DISPATCH_MASKED:
        // AND #$02 cleared the accumulator, so the BNE fell through.
        cycle += kDispatchMaskedCycles;
        output.status = set_nz(output.status, run.accumulator);
        output.program_counter = kDispatchPc;
        break;
    case DC_ACTOR_SCAN_COMPLETE:
        // The final CPX #$80 matched, so its BNE cost two cycles, not four.
        cycle -= kTerminalBranchSaving;
        output.status = set_nz(output.status, 0U);
        output.status = static_cast<std::uint8_t>(output.status | 0x01U);
        output.program_counter = kTerminalPc;
        break;
    case DC_ACTOR_SCAN_BUDGET:
        // Resting at the loop head after a taken BNE: CPX #$80 against a
        // slot below $80 borrows, leaving carry clear and the difference
        // negative.
        output.status = set_nz(
            output.status,
            static_cast<std::uint8_t>(run.slot - 0x80U));
        output.status = static_cast<std::uint8_t>(output.status & ~0x01U);
        output.program_counter = kLoopHeadPc;
        break;
    default:
        return std::nullopt;
    }

    if (cycle >= horizon) {
        return std::nullopt;
    }

    RoutineReplacement replacement{
        .return_instruction =
            {
                .physical_bank = 0x01U,
                .logical_pc = output.program_counter,
            },
        .output_registers = output,
        .body_master_cycles = cycle,
    };
    replacement.work_ram_writes = std::move(writes);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_actor_slot_reset(
    const RoutineEntry& entry)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xC574U,
    };
    if (entry.routine != kRoutine) {
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
    return replacement;
}
} // namespace devils_crush::app::detail
