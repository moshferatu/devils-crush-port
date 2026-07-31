// SPDX-License-Identifier: GPL-3.0-or-later

#include "native_replacements_internal.hpp"
#include "devils_crush/game_logic/actor.h"
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
replace_giant_skeleton_eyeball_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x4DD9U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    // Transfer mode changes the HuC6280 immediate AND contract.
    if ((entry.registers.status & 0x20U) != 0U) {
        return std::nullopt;
    }

    dc_giant_skeleton_eyeball_gate gate{};
    if (!dc_classify_giant_skeleton_eyeball_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &gate)) {
        throw std::runtime_error(
            "Native 4DD9 replacement rejected the work-RAM snapshot.");
    }
    if (gate.path != DC_GIANT_SKELETON_EYEBALL_DORMANT) {
        return std::nullopt;
    }

    const auto set_nz =
        [](std::uint8_t status,
           const std::uint8_t value,
           const bool clear_transfer) {
            const auto mask = static_cast<std::uint8_t>(
                clear_transfer ? 0xA2U : 0x82U);
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~mask));
            if (value == 0U) {
                status = static_cast<std::uint8_t>(status | 0x02U);
            } else if ((value & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x80U);
            }
            return status;
        };

    auto output = entry.registers;
    output.program_counter = 0x4DE9U;
    output.accumulator = gate.final_animation_bit;
    output.status = set_nz(output.status, output.accumulator, true);

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x02U,
            .logical_pc = 0x4DE9U,
        },
        .output_registers = output,
        .body_master_cycles = 57U,
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
                .instruction = {0x02U, instruction_pc},
                .registers = registers,
            });
        };

    auto checkpoint = entry.registers;
    checkpoint.accumulator = gate.egg_completion_latch;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator, false);
    append_checkpoint(0x4DD9U, 0x4DDCU, 15U, checkpoint);

    checkpoint.accumulator = gate.egg_completion_bit;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator, true);
    append_checkpoint(0x4DDCU, 0x4DDEU, 21U, checkpoint);
    append_checkpoint(0x4DDEU, 0x4DE0U, 27U, checkpoint);

    checkpoint.accumulator = gate.session_flags;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator, false);
    append_checkpoint(0x4DE0U, 0x4DE2U, 39U, checkpoint);

    checkpoint.accumulator = gate.final_animation_bit;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator, true);
    append_checkpoint(0x4DE2U, 0x4DE4U, 45U, checkpoint);
    append_checkpoint(0x4DE4U, 0x4DE9U, 57U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_mercy_star_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x4FC1U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    if ((entry.registers.status & 0x20U) != 0U) {
        return std::nullopt;
    }

    dc_mercy_star_gate gate{};
    if (!dc_classify_mercy_star_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &gate)) {
        throw std::runtime_error(
            "Native 4FC1 replacement rejected the work-RAM snapshot.");
    }
    if (gate.path != DC_MERCY_STAR_NOT_LAST_BALL) {
        return std::nullopt;
    }

    const auto set_nz =
        [](std::uint8_t status, const std::uint8_t value) {
            status = static_cast<std::uint8_t>(status & 0x7DU);
            if (value == 0U) {
                status = static_cast<std::uint8_t>(status | 0x02U);
            } else if ((value & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x80U);
            }
            return status;
        };

    auto output = entry.registers;
    output.program_counter = 0x4FE1U;
    output.accumulator =
        static_cast<std::uint8_t>(gate.balls_remaining - 1U);
    output.status = set_nz(output.status, output.accumulator);

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x02U,
            .logical_pc = 0x4FE1U,
        },
        .output_registers = output,
        .body_master_cycles = 54U,
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
                .instruction = {0x02U, instruction_pc},
                .registers = registers,
            });
        };

    auto checkpoint = entry.registers;
    checkpoint.accumulator = gate.table_mode;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x4FC1U, 0x4FC4U, 15U, checkpoint);
    append_checkpoint(0x4FC4U, 0x4FC6U, 21U, checkpoint);

    checkpoint.accumulator = gate.balls_remaining;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x4FC6U, 0x4FC9U, 36U, checkpoint);

    checkpoint.accumulator = output.accumulator;
    checkpoint.status = output.status;
    append_checkpoint(0x4FC9U, 0x4FCAU, 42U, checkpoint);
    append_checkpoint(0x4FCAU, 0x4FE1U, 54U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_dragon_egg_controller(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x42F6U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_dragon_egg_controller controller{};
    if (!dc_classify_dragon_egg_controller(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &controller)) {
        throw std::runtime_error(
            "Native 42F6 replacement rejected the work-RAM snapshot.");
    }
    if (controller.path != DC_DRAGON_EGG_CONTROLLER_ACTIVE) {
        return std::nullopt;
    }

    const auto set_nz =
        [](std::uint8_t status, const std::uint8_t value) {
            status = static_cast<std::uint8_t>(status & 0x7DU);
            if (value == 0U) {
                status = static_cast<std::uint8_t>(status | 0x02U);
            } else if ((value & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x80U);
            }
            return status;
        };

    auto output = entry.registers;
    output.program_counter = 0x42FBU;
    output.accumulator = controller.live_egg_count;
    output.status = set_nz(output.status, output.accumulator);

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x02U,
            .logical_pc = 0x42FBU,
        },
        .output_registers = output,
        .body_master_cycles = 21U,
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
                .instruction = {0x02U, instruction_pc},
                .registers = registers,
            });
        };

    append_checkpoint(0x42F6U, 0x42F9U, 15U, output);
    append_checkpoint(0x42F9U, 0x42FBU, 21U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_big_star_feature_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x445BU,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_big_star_feature_gate gate{};
    if (!dc_classify_big_star_feature_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &gate)) {
        throw std::runtime_error(
            "Native 445B replacement rejected the work-RAM snapshot.");
    }
    if (gate.path != DC_BIG_STAR_FEATURE_GATE_IDLE) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0x4460U;
    output.accumulator = gate.pending_feature_bits;
    output.status = static_cast<std::uint8_t>(
        (output.status & 0x7DU) | 0x02U);

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x02U,
            .logical_pc = 0x4460U,
        },
        .output_registers = output,
        .body_master_cycles = 21U,
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
                .instruction = {0x02U, instruction_pc},
                .registers = registers,
            });
        };

    append_checkpoint(0x445BU, 0x445EU, 15U, output);
    append_checkpoint(0x445EU, 0x4460U, 21U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_walled_star_hit_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x6339U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_walled_star_hit_gate gate{};
    if (!dc_classify_walled_star_hit_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &gate)) {
        throw std::runtime_error(
            "Native 6339 replacement rejected the work-RAM snapshot.");
    }
    if (gate.path != DC_WALLED_STAR_HIT_GATE_IDLE) {
        return std::nullopt;
    }

    const auto set_nz =
        [](std::uint8_t status, const std::uint8_t value) {
            status = static_cast<std::uint8_t>(status & 0x7DU);
            if (value == 0U) {
                status = static_cast<std::uint8_t>(status | 0x02U);
            } else if ((value & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x80U);
            }
            return status;
        };

    auto collision_registers = entry.registers;
    collision_registers.program_counter = 0x633CU;
    collision_registers.accumulator = gate.collision_state;
    collision_registers.status =
        set_nz(collision_registers.status, collision_registers.accumulator);

    auto output = collision_registers;
    output.program_counter = 0x6375U;
    output.accumulator = gate.shared_access_latch;
    output.status = set_nz(output.status, output.accumulator);

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x6375U,
        },
        .output_registers = output,
        .body_master_cycles = 54U,
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
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    append_checkpoint(
        0x6339U,
        0x633CU,
        15U,
        collision_registers);
    append_checkpoint(
        0x633CU,
        0x6366U,
        27U,
        collision_registers);
    append_checkpoint(0x6366U, 0x6369U, 42U, output);
    append_checkpoint(0x6369U, 0x6375U, 54U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_tower_hit_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x60C3U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_tower_hit_gate gate{};
    if (!dc_classify_tower_hit_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &gate)) {
        throw std::runtime_error(
            "Native 60C3 replacement rejected the work-RAM snapshot.");
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

    auto after_collision_load = entry.registers;
    after_collision_load.accumulator = gate.collision_state;
    after_collision_load.status =
        set_nz(after_collision_load.status, after_collision_load.accumulator);

    auto output = after_collision_load;
    output.program_counter = 0x612AU;

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x612AU,
        },
        .output_registers = output,
        .body_master_cycles = 27U,
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
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    append_checkpoint(
        0x60C3U,
        0x60C6U,
        15U,
        after_collision_load);
    if (gate.path == DC_TOWER_HIT_GATE_IDLE) {
        append_checkpoint(0x60C6U, 0x612AU, 27U, output);
        return replacement;
    }
    if (work_ram.size() <= 0x1580U
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    append_checkpoint(
        0x60C6U,
        0x60C8U,
        21U,
        after_collision_load);
    auto after_collision_clear = after_collision_load;
    after_collision_clear.accumulator =
        static_cast<std::uint8_t>(gate.collision_state & 0x7FU);
    after_collision_clear.status = set_nz(
        after_collision_clear.status,
        after_collision_clear.accumulator);
    append_checkpoint(
        0x60C8U,
        0x60CAU,
        27U,
        after_collision_clear);
    append_checkpoint(
        0x60CAU,
        0x60CDU,
        42U,
        after_collision_clear);
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x60CAU},
        .logical_address =
            static_cast<std::uint16_t>(0x3800U + gate.slot),
        .value = after_collision_clear.accumulator,
        .master_cycle_offset = 42U,
    });

    output = after_collision_clear;
    output.accumulator =
        std::to_integer<std::uint8_t>(work_ram[0x1580U]);
    output.status = set_nz(output.status, output.accumulator);
    append_checkpoint(0x60CDU, 0x60D0U, 57U, output);
    if ((output.accumulator & 0x80U) == 0U) {
        output.program_counter = 0x612AU;
        replacement.return_instruction.logical_pc = 0x612AU;
        replacement.body_master_cycles = 69U;
        replacement.output_registers = output;
        append_checkpoint(0x60D0U, 0x612AU, 69U, output);
        return replacement;
    }

    output.program_counter = 0x60D2U;
    replacement.return_instruction.logical_pc = 0x60D2U;
    replacement.body_master_cycles = 63U;
    replacement.output_registers = output;
    append_checkpoint(0x60D0U, 0x60D2U, 63U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_tower_hit_continuation(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // Score and audio helpers remain emulated. This boundary begins after
    // their safe return and advances the routed Tower's native progress.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x60E0U,
    };
    if (entry.routine != kRoutine
        || entry.registers.x != 0x2DU
        || work_ram.size() <= 0x1FADU
        || std::to_integer<std::uint8_t>(work_ram[0x0D2BU]) != 0x01U
        || std::to_integer<std::uint8_t>(work_ram[0x0D3FU]) != 0x00U
        || std::to_integer<std::uint8_t>(work_ram[0x0CFCU]) != 0x00U
        || (entry.registers.status & 0x28U) != 0U) {
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
    const auto set_compare =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right) {
            status = set_nz(
                static_cast<std::uint8_t>(status & 0xFEU),
                static_cast<std::uint8_t>(left - right));
            if (left >= right) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            return status;
        };

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xC021U,
        },
        .body_master_cycles = 156U,
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
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };
    const auto add_write =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t logical_address,
            const std::uint8_t value,
            const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x03U, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
        };

    auto checkpoint = entry.registers;
    checkpoint.accumulator = 0x01U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x60E0U, 0x60E3U, 15U, checkpoint);
    checkpoint.accumulator = 0x00U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x60E3U, 0x60E4U, 21U, checkpoint);
    append_checkpoint(0x60E4U, 0x60E6U, 27U, checkpoint);

    checkpoint.status = set_nz(checkpoint.status, 0x01U);
    append_checkpoint(0x60E6U, 0x60E9U, 48U, checkpoint);
    add_write(0x60E6U, 0x2D3FU, 0x01U, 48U);

    checkpoint.accumulator = 0x01U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x60E9U, 0x60ECU, 63U, checkpoint);
    checkpoint.status =
        set_compare(checkpoint.status, checkpoint.accumulator, 0x0AU);
    append_checkpoint(0x60ECU, 0x60EEU, 69U, checkpoint);
    append_checkpoint(0x60EEU, 0x60F9U, 81U, checkpoint);

    checkpoint.accumulator = 0x00U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x60F9U, 0x60FCU, 96U, checkpoint);
    append_checkpoint(0x60FCU, 0x6112U, 108U, checkpoint);

    checkpoint.accumulator = 0x01U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x6112U, 0x6114U, 114U, checkpoint);
    append_checkpoint(0x6114U, 0x6117U, 129U, checkpoint);
    add_write(0x6114U, 0x2CFCU, 0x01U, 129U);

    checkpoint.accumulator = 0x04U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x6117U, 0x6119U, 135U, checkpoint);
    const auto stack_high = checkpoint.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
    add_write(
        0x6119U,
        static_cast<std::uint16_t>(0x2100U + stack_high),
        0x61U,
        135U);
    add_write(
        0x6119U,
        static_cast<std::uint16_t>(0x2100U + stack_low),
        0x1BU,
        135U);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(0x6119U, 0xC021U, 156U, checkpoint);

    replacement.output_registers = checkpoint;
    replacement.output_registers.program_counter = 0xC021U;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_tower_post_dispatch(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The interrupt-bearing C021 dispatcher remains emulated. Once it
    // returns, this boundary composes the shared native coroutine scheduler.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x611CU,
    };
    if (entry.routine != kRoutine
        || entry.registers.x != 0x2DU
        || work_ram.size() <= 0x1FADU
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    RoutineReplacement replacement{};
    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    auto checkpoint = entry.registers;
    const auto stack_high = checkpoint.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
    replacement.work_ram_writes = {
        {
            .instruction = {0x03U, 0x611CU},
            .logical_address =
                static_cast<std::uint16_t>(0x2100U + stack_high),
            .value = 0x61U,
            .master_cycle_offset = 0U,
        },
        {
            .instruction = {0x03U, 0x611CU},
            .logical_address =
                static_cast<std::uint16_t>(0x2100U + stack_low),
            .value = 0x1EU,
            .master_cycle_offset = 0U,
        },
    };
    staged[0x0100U + stack_high] = static_cast<std::byte>(0x61U);
    staged[0x0100U + stack_low] = static_cast<std::byte>(0x1EU);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);

    const auto append_checkpoint =
        [&replacement](
            const std::uint8_t physical_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {physical_bank, instruction_pc},
                .registers = registers,
            });
        };
    append_checkpoint(0x03U, 0x611CU, 0xC018U, 21U, checkpoint);
    append_checkpoint(0x01U, 0xC018U, 0xC5C9U, 33U, checkpoint);

    RoutineEntry scheduler_entry{
        .routine = {
            .physical_bank = 0x01U,
            .logical_pc = 0xC5C9U,
        },
        .registers = checkpoint,
    };
    auto scheduler = replace_coroutine_scheduler(scheduler_entry, staged);
    if (!scheduler || scheduler->checkpoints.empty()) {
        return std::nullopt;
    }

    constexpr std::uint32_t kSchedulerOffset = 33U;
    for (auto write : scheduler->work_ram_writes) {
        write.master_cycle_offset += kSchedulerOffset;
        replacement.work_ram_writes.push_back(write);
    }
    for (auto child_checkpoint : scheduler->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset +=
            kSchedulerOffset;
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
replace_dragon_head_hit_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x5A4BU,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_dragon_head_hit_gate gate{};
    if (!dc_classify_dragon_head_hit_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &gate)) {
        throw std::runtime_error(
            "Native 5A4B replacement rejected the work-RAM snapshot.");
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

    auto after_collision_load = entry.registers;
    after_collision_load.accumulator = gate.collision_state;
    after_collision_load.status =
        set_nz(after_collision_load.status, after_collision_load.accumulator);

    auto output = after_collision_load;
    output.program_counter = 0x5A73U;

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x02U,
            .logical_pc = 0x5A73U,
        },
        .output_registers = output,
        .body_master_cycles = 27U,
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
                .instruction = {0x02U, instruction_pc},
                .registers = registers,
            });
        };

    append_checkpoint(
        0x5A4BU,
        0x5A4EU,
        15U,
        after_collision_load);
    if (gate.path == DC_DRAGON_HEAD_HIT_GATE_IDLE) {
        append_checkpoint(0x5A4EU, 0x5A73U, 27U, output);
        return replacement;
    }
    if ((entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    append_checkpoint(
        0x5A4EU,
        0x5A50U,
        21U,
        after_collision_load);
    auto after_collision_clear = after_collision_load;
    after_collision_clear.accumulator =
        static_cast<std::uint8_t>(gate.collision_state & 0x7FU);
    after_collision_clear.status = set_nz(
        after_collision_clear.status,
        after_collision_clear.accumulator);
    append_checkpoint(
        0x5A50U,
        0x5A52U,
        27U,
        after_collision_clear);
    append_checkpoint(
        0x5A52U,
        0x5A55U,
        42U,
        after_collision_clear);
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x5A52U},
        .logical_address =
            static_cast<std::uint16_t>(0x3800U + gate.slot),
        .value = after_collision_clear.accumulator,
        .master_cycle_offset = 42U,
    });
    replacement.return_instruction.logical_pc = 0x5A55U;
    replacement.output_registers = after_collision_clear;
    replacement.output_registers.program_counter = 0x5A55U;
    replacement.body_master_cycles = 42U;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_dragon_head_hit_continuation(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // Shared score, audio, bonus, and appearance helpers remain emulated.
    // This boundary owns only the routed nonterminal eight-hit countdown.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x5A6EU,
    };
    constexpr std::uint8_t kDragonSlot = 0x13U;
    constexpr std::size_t kCounterOffset = 0x1C00U + kDragonSlot;
    if (entry.routine != kRoutine
        || entry.registers.x != kDragonSlot
        || work_ram.size() <= kCounterOffset
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    const auto counter =
        std::to_integer<std::uint8_t>(work_ram[kCounterOffset]);
    if (counter <= 1U || counter > 8U) {
        // Counter one enters the mouth-opening state machine at 5A74.
        return std::nullopt;
    }
    const auto decremented = static_cast<std::uint8_t>(counter - 1U);
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
    output.program_counter = 0x5A73U;
    output.status = set_nz(output.status, decremented);

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x02U,
            .logical_pc = 0x5A73U,
        },
        .output_registers = output,
        .body_master_cycles = 27U,
    };
    auto after_decrement = output;
    after_decrement.program_counter = 0x5A71U;
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 18U,
        .master_cycle_offset = 21U,
        .instruction = {0x02U, 0x5A6EU},
        .registers = after_decrement,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x5A6EU},
        .logical_address =
            static_cast<std::uint16_t>(0x3C00U + kDragonSlot),
        .value = decremented,
        .master_cycle_offset = 21U,
    });
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 24U,
        .master_cycle_offset = 27U,
        .instruction = {0x02U, 0x5A71U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_bumper_hit_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x78C7U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_bumper_hit_gate gate{};
    if (!dc_classify_bumper_hit_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &gate)) {
        throw std::runtime_error(
            "Native 78C7 replacement rejected the work-RAM snapshot.");
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

    auto after_collision_load = entry.registers;
    after_collision_load.accumulator = gate.collision_state;
    after_collision_load.status = set_nz(
        after_collision_load.status,
        after_collision_load.accumulator);

    auto output = after_collision_load;
    output.program_counter = 0x78E6U;

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x78E6U,
        },
        .output_registers = output,
        .body_master_cycles = 27U,
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
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    append_checkpoint(
        0x78C7U,
        0x78CAU,
        15U,
        after_collision_load);
    if (gate.path == DC_BUMPER_HIT_GATE_IDLE) {
        append_checkpoint(0x78CAU, 0x78E6U, 27U, output);
        return replacement;
    }

    constexpr std::size_t kPhaseBase = 0x1E80U;
    const auto phase_offset = kPhaseBase + gate.slot;
    if (work_ram.size() <= phase_offset
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }
    const auto phase =
        std::to_integer<std::uint8_t>(work_ram[phase_offset]);
    if (phase > 8U) {
        return std::nullopt;
    }

    append_checkpoint(
        0x78CAU,
        0x78CCU,
        21U,
        after_collision_load);

    auto checkpoint = after_collision_load;
    checkpoint.accumulator =
        static_cast<std::uint8_t>(gate.collision_state & 0x7FU);
    checkpoint.status = set_nz(
        checkpoint.status,
        checkpoint.accumulator);
    append_checkpoint(0x78CCU, 0x78CEU, 27U, checkpoint);
    append_checkpoint(0x78CEU, 0x78D1U, 42U, checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x78CEU},
        .logical_address =
            static_cast<std::uint16_t>(0x3800U + gate.slot),
        .value = checkpoint.accumulator,
        .master_cycle_offset = 42U,
    });

    checkpoint.accumulator = phase;
    checkpoint.status = set_nz(checkpoint.status, phase);
    append_checkpoint(0x78D1U, 0x78D4U, 57U, checkpoint);

    const auto next_phase = static_cast<std::uint8_t>(phase + 1U);
    checkpoint.status = set_nz(checkpoint.status, next_phase);
    append_checkpoint(0x78D4U, 0x78D7U, 78U, checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x78D4U},
        .logical_address =
            static_cast<std::uint16_t>(0x3E80U + gate.slot),
        .value = next_phase,
        .master_cycle_offset = 78U,
    });

    checkpoint.accumulator = static_cast<std::uint8_t>(phase << 1U);
    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status & static_cast<std::uint8_t>(~0x01U));
    if ((phase & 0x80U) != 0U) {
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status | 0x01U);
    }
    checkpoint.status = set_nz(
        checkpoint.status,
        checkpoint.accumulator);
    append_checkpoint(0x78D7U, 0x78D8U, 84U, checkpoint);

    checkpoint.y = checkpoint.accumulator;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0x78D8U, 0x78D9U, 90U, checkpoint);

    constexpr std::array<std::uint16_t, 9> kPhaseHandlers{
        0x78E7U,
        0x78E7U,
        0x78E7U,
        0x78E7U,
        0x78E7U,
        0x78E7U,
        0x7901U,
        0x7901U,
        0x792BU,
    };
    const auto handler = kPhaseHandlers[phase];
    checkpoint.accumulator =
        static_cast<std::uint8_t>(handler & 0x00FFU);
    checkpoint.status = set_nz(
        checkpoint.status,
        checkpoint.accumulator);
    append_checkpoint(0x78D9U, 0x78DCU, 105U, checkpoint);
    append_checkpoint(0x78DCU, 0x78DEU, 117U, checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x78DCU},
        .logical_address = 0x202FU,
        .value = checkpoint.accumulator,
        .master_cycle_offset = 117U,
    });

    checkpoint.accumulator =
        static_cast<std::uint8_t>(handler >> 8U);
    checkpoint.status = set_nz(
        checkpoint.status,
        checkpoint.accumulator);
    append_checkpoint(0x78DEU, 0x78E1U, 132U, checkpoint);
    append_checkpoint(0x78E1U, 0x78E3U, 144U, checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x78E1U},
        .logical_address = 0x2030U,
        .value = checkpoint.accumulator,
        .master_cycle_offset = 144U,
    });

    checkpoint.program_counter = handler;
    append_checkpoint(0x78E3U, handler, 165U, checkpoint);
    replacement.return_instruction.logical_pc = handler;
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = 165U;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_bumper_primary_progress(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The shared score and audio helpers remain emulated. This boundary
    // advances the six ordinary Bumper appearances before their renderer.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x78F5U,
    };
    if (entry.routine != kRoutine
        || (entry.registers.x != 0x14U
            && entry.registers.x != 0x15U)
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    const auto progress_offset =
        static_cast<std::size_t>(0x1B00U + entry.registers.x);
    const auto phase_offset =
        static_cast<std::size_t>(0x1E80U + entry.registers.x);
    if (work_ram.size() <= progress_offset
        || work_ram.size() <= phase_offset) {
        return std::nullopt;
    }
    const auto progress =
        std::to_integer<std::uint8_t>(work_ram[progress_offset]);
    const auto phase =
        std::to_integer<std::uint8_t>(work_ram[phase_offset]);
    if (progress > 5U
        || phase != static_cast<std::uint8_t>(progress + 1U)) {
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
    const auto next_progress =
        static_cast<std::uint8_t>(progress + 1U);

    auto after_increment = entry.registers;
    after_increment.program_counter = 0x78F8U;
    after_increment.status = set_nz(
        after_increment.status,
        next_progress);

    auto output = after_increment;
    output.program_counter = 0x78FBU;
    output.accumulator = next_progress;
    output.status = set_nz(output.status, output.accumulator);

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x78FBU,
        },
        .output_registers = output,
        .body_master_cycles = 36U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 18U,
        .master_cycle_offset = 21U,
        .instruction = {0x03U, 0x78F5U},
        .registers = after_increment,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x78F5U},
        .logical_address = static_cast<std::uint16_t>(
            0x3B00U + entry.registers.x),
        .value = next_progress,
        .master_cycle_offset = 21U,
    });
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 33U,
        .master_cycle_offset = 36U,
        .instruction = {0x03U, 0x78F8U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_bumper_secondary_progress(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The score, appearance, scheduler, and audio helpers remain emulated.
    // This boundary owns the routed seventh/eighth-hit damage counters.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x7912U,
    };
    if (entry.routine != kRoutine
        || (entry.registers.x != 0x14U
            && entry.registers.x != 0x15U)
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    const auto progress_offset =
        static_cast<std::size_t>(0x1B80U + entry.registers.x);
    const auto phase_offset =
        static_cast<std::size_t>(0x1E80U + entry.registers.x);
    const auto collision_offset =
        static_cast<std::size_t>(0x1800U + entry.registers.x);
    if (work_ram.size() <= progress_offset
        || work_ram.size() <= phase_offset
        || work_ram.size() <= collision_offset) {
        return std::nullopt;
    }
    const auto progress =
        std::to_integer<std::uint8_t>(work_ram[progress_offset]);
    const auto phase =
        std::to_integer<std::uint8_t>(work_ram[phase_offset]);
    const auto collision =
        std::to_integer<std::uint8_t>(work_ram[collision_offset]);
    if (progress > 1U
        || phase != static_cast<std::uint8_t>(progress + 7U)) {
        // Counter two enters the inactive ninth-hit destruction path.
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
    const auto set_compare =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right) {
            status = set_nz(
                static_cast<std::uint8_t>(status & 0xFEU),
                static_cast<std::uint8_t>(left - right));
            if (left >= right) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            return status;
        };
    const auto next_progress =
        static_cast<std::uint8_t>(progress + 1U);

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x792AU,
        },
        .body_master_cycles = 114U,
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
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    auto checkpoint = entry.registers;
    checkpoint.accumulator = progress;
    checkpoint.status = set_nz(checkpoint.status, progress);
    append_checkpoint(0x7912U, 0x7915U, 15U, checkpoint);

    checkpoint.status =
        set_compare(checkpoint.status, checkpoint.accumulator, 0x02U);
    append_checkpoint(0x7915U, 0x7917U, 21U, checkpoint);
    append_checkpoint(0x7917U, 0x7919U, 27U, checkpoint);

    checkpoint.status = set_nz(checkpoint.status, next_progress);
    append_checkpoint(0x7919U, 0x791CU, 48U, checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x7919U},
        .logical_address = static_cast<std::uint16_t>(
            0x3B80U + entry.registers.x),
        .value = next_progress,
        .master_cycle_offset = 48U,
    });

    checkpoint.accumulator = next_progress;
    checkpoint.status = set_nz(
        checkpoint.status,
        checkpoint.accumulator);
    append_checkpoint(0x791CU, 0x791FU, 63U, checkpoint);
    append_checkpoint(0x791FU, 0x7922U, 78U, checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x791FU},
        .logical_address = static_cast<std::uint16_t>(
            0x3980U + entry.registers.x),
        .value = next_progress,
        .master_cycle_offset = 78U,
    });

    checkpoint.accumulator = collision;
    checkpoint.status = set_nz(
        checkpoint.status,
        checkpoint.accumulator);
    append_checkpoint(0x7922U, 0x7925U, 93U, checkpoint);

    checkpoint.accumulator =
        static_cast<std::uint8_t>(collision | 0x40U);
    checkpoint.status = set_nz(
        checkpoint.status,
        checkpoint.accumulator);
    append_checkpoint(0x7925U, 0x7927U, 99U, checkpoint);
    append_checkpoint(0x7927U, 0x792AU, 114U, checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x7927U},
        .logical_address = static_cast<std::uint16_t>(
            0x3800U + entry.registers.x),
        .value = checkpoint.accumulator,
        .master_cycle_offset = 114U,
    });

    replacement.output_registers = checkpoint;
    replacement.output_registers.program_counter = 0x792AU;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_devil_shooter_hit_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x5116U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_devil_shooter_hit_gate gate{};
    if (!dc_classify_devil_shooter_hit_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &gate)) {
        throw std::runtime_error(
            "Native 5116 replacement rejected the work-RAM snapshot.");
    }
    auto output = entry.registers;
    output.program_counter =
        gate.path == DC_DEVIL_SHOOTER_HIT_GATE_IDLE
        ? 0x5123U
        : 0x511BU;
    output.accumulator = gate.collision_state;
    output.status = static_cast<std::uint8_t>(output.status & 0x7DU);
    if (output.accumulator == 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);
    } else if ((output.accumulator & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x80U);
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x02U,
            .logical_pc = output.program_counter,
        },
        .output_registers = output,
        .body_master_cycles =
            gate.path == DC_DEVIL_SHOOTER_HIT_GATE_IDLE ? 27U : 21U,
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
                .instruction = {0x02U, instruction_pc},
                .registers = registers,
            });
        };

    append_checkpoint(0x5116U, 0x5119U, 15U, output);
    append_checkpoint(
        0x5119U,
        output.program_counter,
        replacement.body_master_cycles,
        output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_devil_shooter_catch_continuation(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The catch's shared C087 helper intentionally remains emulated. This
    // boundary begins at its safe 52B8 return continuation.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x52B8U,
    };
    if (entry.routine != kRoutine
        || entry.registers.x != 0x0EU
        || work_ram.size() <= 0x1F8EU
        || (entry.registers.status & 0x28U) != 0U) {
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

    RoutineReplacement replacement{};
    const auto append_checkpoint =
        [&replacement](
            const std::uint8_t physical_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {physical_bank, instruction_pc},
                .registers = registers,
            });
        };
    const auto add_write =
        [&replacement](
            const std::uint8_t physical_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t logical_address,
            const std::uint8_t value,
            const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {physical_bank, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
        };

    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    auto checkpoint = entry.registers;
    append_checkpoint(0x02U, 0x52B8U, 0x52BBU, 15U, checkpoint);
    add_write(
        0x02U,
        0x52B8U,
        0x3180U,
        checkpoint.accumulator,
        15U);
    staged[0x1180U] = static_cast<std::byte>(checkpoint.accumulator);

    checkpoint.accumulator = 0x01U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x02U, 0x52BBU, 0x52BDU, 21U, checkpoint);
    append_checkpoint(0x02U, 0x52BDU, 0x52C0U, 36U, checkpoint);
    add_write(0x02U, 0x52BDU, 0x3200U, 0x01U, 36U);
    staged[0x1200U] = static_cast<std::byte>(0x01U);

    const auto scheduler_stack_high = checkpoint.stack_pointer;
    const auto scheduler_stack_low = static_cast<std::uint8_t>(
        scheduler_stack_high - 1U);
    add_write(
        0x02U,
        0x52C0U,
        static_cast<std::uint16_t>(0x2100U + scheduler_stack_high),
        0x52U,
        36U);
    add_write(
        0x02U,
        0x52C0U,
        static_cast<std::uint16_t>(0x2100U + scheduler_stack_low),
        0xC2U,
        36U);
    staged[0x0100U + scheduler_stack_high] =
        static_cast<std::byte>(0x52U);
    staged[0x0100U + scheduler_stack_low] =
        static_cast<std::byte>(0xC2U);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(0x02U, 0x52C0U, 0xC018U, 57U, checkpoint);
    append_checkpoint(0x01U, 0xC018U, 0xC5C9U, 69U, checkpoint);

    RoutineEntry scheduler_entry{
        .routine = {
            .physical_bank = 0x01U,
            .logical_pc = 0xC5C9U,
        },
        .registers = checkpoint,
    };
    auto scheduler = replace_coroutine_scheduler(
        scheduler_entry,
        staged);
    if (!scheduler || scheduler->checkpoints.empty()) {
        return std::nullopt;
    }

    constexpr std::uint32_t kSchedulerOffset = 69U;
    for (auto write : scheduler->work_ram_writes) {
        write.master_cycle_offset += kSchedulerOffset;
        replacement.work_ram_writes.push_back(write);
    }
    for (auto child_checkpoint : scheduler->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset +=
            kSchedulerOffset;
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
replace_skeleton_insect_left_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x54B9U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_skeleton_insect_left_gate gate{};
    if (!dc_classify_skeleton_insect_left_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &gate)) {
        throw std::runtime_error(
            "Native 54B9 replacement rejected the work-RAM snapshot.");
    }
    auto output = entry.registers;
    output.accumulator = gate.trigger_latch;
    output.status = static_cast<std::uint8_t>(output.status & 0x7DU);
    if (output.accumulator == 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);
    } else if ((output.accumulator & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x80U);
    }
    output.program_counter = gate.path == DC_SKELETON_INSECT_LEFT_GATE_IDLE
        ? 0x54BEU
        : 0x54BFU;

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x02U,
            .logical_pc = output.program_counter,
        },
        .output_registers = output,
        .body_master_cycles =
            gate.path == DC_SKELETON_INSECT_LEFT_GATE_IDLE ? 21U : 27U,
    };
    const auto append_checkpoint =
        [&replacement](
            const std::uint8_t physical_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {physical_bank, instruction_pc},
                .registers = registers,
            });
        };

    append_checkpoint(0x02U, 0x54B9U, 0x54BCU, 15U, output);
    append_checkpoint(
        0x02U,
        0x54BCU,
        output.program_counter,
        replacement.body_master_cycles,
        output);
    if (gate.path == DC_SKELETON_INSECT_LEFT_GATE_IDLE) {
        return replacement;
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
    const auto add_write =
        [&replacement](
            const std::uint8_t physical_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t logical_address,
            const std::uint8_t value,
            const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {physical_bank, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
        };

    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    auto checkpoint = output;
    checkpoint.accumulator = 0x28U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x02U, 0x54BFU, 0x54C1U, 33U, checkpoint);
    append_checkpoint(0x02U, 0x54C1U, 0x54C3U, 45U, checkpoint);
    add_write(0x02U, 0x54C1U, 0x202FU, 0x28U, 45U);
    staged[0x002FU] = static_cast<std::byte>(0x28U);

    append_checkpoint(0x02U, 0x54C3U, 0x54C5U, 57U, checkpoint);
    add_write(0x02U, 0x54C3U, 0x2030U, 0x00U, 57U);
    staged[0x0030U] = static_cast<std::byte>(0x00U);

    add_write(
        0x02U,
        0x54C5U,
        static_cast<std::uint16_t>(
            0x2100U + checkpoint.stack_pointer),
        0x54U,
        57U);
    add_write(
        0x02U,
        0x54C5U,
        static_cast<std::uint16_t>(
            0x2100U
            + static_cast<std::uint8_t>(
                checkpoint.stack_pointer - 1U)),
        0xC7U,
        57U);
    staged[0x0100U + checkpoint.stack_pointer] =
        static_cast<std::byte>(0x54U);
    staged[
        0x0100U
        + static_cast<std::uint8_t>(
            checkpoint.stack_pointer - 1U)] =
        static_cast<std::byte>(0xC7U);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(0x02U, 0x54C5U, 0xC02AU, 78U, checkpoint);
    append_checkpoint(0x01U, 0xC02AU, 0xF54FU, 90U, checkpoint);

    checkpoint.y =
        std::to_integer<std::uint8_t>(staged[0x0D38U]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0x00U, 0xF54FU, 0xF552U, 105U, checkpoint);
    checkpoint.y = static_cast<std::uint8_t>(checkpoint.y + 1U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0x00U, 0xF552U, 0xF553U, 111U, checkpoint);

    add_write(
        0x00U,
        0xF553U,
        static_cast<std::uint16_t>(
            0x2100U + checkpoint.stack_pointer),
        checkpoint.y,
        120U);
    staged[0x0100U + checkpoint.stack_pointer] =
        static_cast<std::byte>(checkpoint.y);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer - 1U);
    append_checkpoint(0x00U, 0xF553U, 0xF554U, 120U, checkpoint);

    add_write(
        0x00U,
        0xF554U,
        static_cast<std::uint16_t>(
            0x2100U + checkpoint.stack_pointer),
        0xF5U,
        120U);
    add_write(
        0x00U,
        0xF554U,
        static_cast<std::uint16_t>(
            0x2100U
            + static_cast<std::uint8_t>(
                checkpoint.stack_pointer - 1U)),
        0x56U,
        120U);
    staged[0x0100U + checkpoint.stack_pointer] =
        static_cast<std::byte>(0xF5U);
    staged[
        0x0100U
        + static_cast<std::uint8_t>(
            checkpoint.stack_pointer - 1U)] =
        static_cast<std::byte>(0x56U);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(0x00U, 0xF554U, 0xF55CU, 141U, checkpoint);

    RoutineEntry score_entry{
        .routine = {
            .physical_bank = 0x00U,
            .logical_pc = 0xF55CU,
        },
        .registers = checkpoint,
    };
    auto score = replace_score_accumulator(score_entry, staged);
    if (!score || score->checkpoints.empty()) {
        return std::nullopt;
    }

    constexpr std::uint32_t kScoreOffset = 141U;
    for (auto write : score->work_ram_writes) {
        write.master_cycle_offset += kScoreOffset;
        replacement.work_ram_writes.push_back(write);
    }
    for (auto child_checkpoint : score->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset +=
            kScoreOffset;
        child_checkpoint.master_cycle_offset += kScoreOffset;
        replacement.checkpoints.push_back(child_checkpoint);
    }

    checkpoint = score->output_registers;
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer + 2U);
    append_checkpoint(0x00U, 0xF5A8U, 0xF557U, 717U, checkpoint);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer + 1U);
    checkpoint.y = 0x01U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0x00U, 0xF557U, 0xF558U, 729U, checkpoint);
    checkpoint.y = 0x00U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0x00U, 0xF558U, 0xF559U, 735U, checkpoint);
    append_checkpoint(0x00U, 0xF559U, 0xF55BU, 741U, checkpoint);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer + 2U);
    append_checkpoint(0x00U, 0xF55BU, 0x54C8U, 759U, checkpoint);

    checkpoint.accumulator = 0x04U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x02U, 0x54C8U, 0x54CAU, 765U, checkpoint);
    append_checkpoint(0x02U, 0x54CAU, 0x54CDU, 786U, checkpoint);
    add_write(
        0x02U,
        0x54CAU,
        static_cast<std::uint16_t>(0x3A00U + gate.slot),
        0x04U,
        786U);
    staged[0x1A00U + gate.slot] = static_cast<std::byte>(0x04U);

    add_write(
        0x02U,
        0x54CDU,
        static_cast<std::uint16_t>(
            0x2100U + checkpoint.stack_pointer),
        0x54U,
        786U);
    add_write(
        0x02U,
        0x54CDU,
        static_cast<std::uint16_t>(
            0x2100U
            + static_cast<std::uint8_t>(
                checkpoint.stack_pointer - 1U)),
        0xCFU,
        786U);
    staged[0x0100U + checkpoint.stack_pointer] =
        static_cast<std::byte>(0x54U);
    staged[
        0x0100U
        + static_cast<std::uint8_t>(
            checkpoint.stack_pointer - 1U)] =
        static_cast<std::byte>(0xCFU);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(0x02U, 0x54CDU, 0xC018U, 807U, checkpoint);
    append_checkpoint(0x01U, 0xC018U, 0xC5C9U, 819U, checkpoint);

    RoutineEntry scheduler_entry{
        .routine = {
            .physical_bank = 0x01U,
            .logical_pc = 0xC5C9U,
        },
        .registers = checkpoint,
    };
    auto scheduler = replace_coroutine_scheduler(
        scheduler_entry,
        staged);
    if (!scheduler || scheduler->checkpoints.empty()) {
        return std::nullopt;
    }

    constexpr std::uint32_t kSchedulerOffset = 819U;
    for (auto write : scheduler->work_ram_writes) {
        write.master_cycle_offset += kSchedulerOffset;
        replacement.work_ram_writes.push_back(write);
    }
    for (auto child_checkpoint : scheduler->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset +=
            kSchedulerOffset;
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
replace_skeleton_insect_right_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x5551U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_skeleton_insect_right_gate gate{};
    if (!dc_classify_skeleton_insect_right_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &gate)) {
        throw std::runtime_error(
            "Native 5551 replacement rejected the work-RAM snapshot.");
    }
    auto output = entry.registers;
    output.accumulator = gate.trigger_latch;
    output.status = static_cast<std::uint8_t>(output.status & 0x7DU);
    if (output.accumulator == 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);
    } else if ((output.accumulator & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x80U);
    }
    output.program_counter = gate.path == DC_SKELETON_INSECT_RIGHT_GATE_IDLE
        ? 0x5556U
        : 0x5557U;

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x02U,
            .logical_pc = output.program_counter,
        },
        .output_registers = output,
        .body_master_cycles =
            gate.path == DC_SKELETON_INSECT_RIGHT_GATE_IDLE ? 21U : 27U,
    };
    const auto append_checkpoint =
        [&replacement](
            const std::uint8_t physical_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {physical_bank, instruction_pc},
                .registers = registers,
            });
        };

    append_checkpoint(0x02U, 0x5551U, 0x5554U, 15U, output);
    append_checkpoint(
        0x02U,
        0x5554U,
        output.program_counter,
        replacement.body_master_cycles,
        output);
    if (gate.path == DC_SKELETON_INSECT_RIGHT_GATE_IDLE) {
        return replacement;
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
    const auto add_write =
        [&replacement](
            const std::uint8_t physical_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t logical_address,
            const std::uint8_t value,
            const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {physical_bank, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
        };

    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    auto checkpoint = output;
    checkpoint.accumulator = 0x28U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x02U, 0x5557U, 0x5559U, 33U, checkpoint);
    append_checkpoint(0x02U, 0x5559U, 0x555BU, 45U, checkpoint);
    add_write(0x02U, 0x5559U, 0x202FU, 0x28U, 45U);
    staged[0x002FU] = static_cast<std::byte>(0x28U);

    append_checkpoint(0x02U, 0x555BU, 0x555DU, 57U, checkpoint);
    add_write(0x02U, 0x555BU, 0x2030U, 0x00U, 57U);
    staged[0x0030U] = static_cast<std::byte>(0x00U);

    add_write(
        0x02U,
        0x555DU,
        static_cast<std::uint16_t>(
            0x2100U + checkpoint.stack_pointer),
        0x55U,
        57U);
    add_write(
        0x02U,
        0x555DU,
        static_cast<std::uint16_t>(
            0x2100U
            + static_cast<std::uint8_t>(
                checkpoint.stack_pointer - 1U)),
        0x5FU,
        57U);
    staged[0x0100U + checkpoint.stack_pointer] =
        static_cast<std::byte>(0x55U);
    staged[
        0x0100U
        + static_cast<std::uint8_t>(
            checkpoint.stack_pointer - 1U)] =
        static_cast<std::byte>(0x5FU);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(0x02U, 0x555DU, 0xC02AU, 78U, checkpoint);
    append_checkpoint(0x01U, 0xC02AU, 0xF54FU, 90U, checkpoint);

    checkpoint.y =
        std::to_integer<std::uint8_t>(staged[0x0D38U]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0x00U, 0xF54FU, 0xF552U, 105U, checkpoint);
    checkpoint.y = static_cast<std::uint8_t>(checkpoint.y + 1U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0x00U, 0xF552U, 0xF553U, 111U, checkpoint);

    add_write(
        0x00U,
        0xF553U,
        static_cast<std::uint16_t>(
            0x2100U + checkpoint.stack_pointer),
        checkpoint.y,
        120U);
    staged[0x0100U + checkpoint.stack_pointer] =
        static_cast<std::byte>(checkpoint.y);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer - 1U);
    append_checkpoint(0x00U, 0xF553U, 0xF554U, 120U, checkpoint);

    add_write(
        0x00U,
        0xF554U,
        static_cast<std::uint16_t>(
            0x2100U + checkpoint.stack_pointer),
        0xF5U,
        120U);
    add_write(
        0x00U,
        0xF554U,
        static_cast<std::uint16_t>(
            0x2100U
            + static_cast<std::uint8_t>(
                checkpoint.stack_pointer - 1U)),
        0x56U,
        120U);
    staged[0x0100U + checkpoint.stack_pointer] =
        static_cast<std::byte>(0xF5U);
    staged[
        0x0100U
        + static_cast<std::uint8_t>(
            checkpoint.stack_pointer - 1U)] =
        static_cast<std::byte>(0x56U);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(0x00U, 0xF554U, 0xF55CU, 141U, checkpoint);

    RoutineEntry score_entry{
        .routine = {
            .physical_bank = 0x00U,
            .logical_pc = 0xF55CU,
        },
        .registers = checkpoint,
    };
    auto score = replace_score_accumulator(score_entry, staged);
    if (!score || score->checkpoints.empty()) {
        return std::nullopt;
    }

    constexpr std::uint32_t kScoreOffset = 141U;
    for (auto write : score->work_ram_writes) {
        write.master_cycle_offset += kScoreOffset;
        replacement.work_ram_writes.push_back(write);
    }
    for (auto child_checkpoint : score->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset +=
            kScoreOffset;
        child_checkpoint.master_cycle_offset += kScoreOffset;
        replacement.checkpoints.push_back(child_checkpoint);
    }

    checkpoint = score->output_registers;
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer + 2U);
    append_checkpoint(0x00U, 0xF5A8U, 0xF557U, 717U, checkpoint);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer + 1U);
    checkpoint.y = 0x01U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0x00U, 0xF557U, 0xF558U, 729U, checkpoint);
    checkpoint.y = 0x00U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0x00U, 0xF558U, 0xF559U, 735U, checkpoint);
    append_checkpoint(0x00U, 0xF559U, 0xF55BU, 741U, checkpoint);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer + 2U);
    append_checkpoint(0x00U, 0xF55BU, 0x5560U, 759U, checkpoint);

    checkpoint.accumulator = 0x04U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x02U, 0x5560U, 0x5562U, 765U, checkpoint);
    append_checkpoint(0x02U, 0x5562U, 0x5565U, 786U, checkpoint);
    add_write(
        0x02U,
        0x5562U,
        static_cast<std::uint16_t>(0x3A00U + gate.slot),
        0x04U,
        786U);
    staged[0x1A00U + gate.slot] = static_cast<std::byte>(0x04U);

    add_write(
        0x02U,
        0x5565U,
        static_cast<std::uint16_t>(
            0x2100U + checkpoint.stack_pointer),
        0x55U,
        786U);
    add_write(
        0x02U,
        0x5565U,
        static_cast<std::uint16_t>(
            0x2100U
            + static_cast<std::uint8_t>(
                checkpoint.stack_pointer - 1U)),
        0x67U,
        786U);
    staged[0x0100U + checkpoint.stack_pointer] =
        static_cast<std::byte>(0x55U);
    staged[
        0x0100U
        + static_cast<std::uint8_t>(
            checkpoint.stack_pointer - 1U)] =
        static_cast<std::byte>(0x67U);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(0x02U, 0x5565U, 0xC018U, 807U, checkpoint);
    append_checkpoint(0x01U, 0xC018U, 0xC5C9U, 819U, checkpoint);

    RoutineEntry scheduler_entry{
        .routine = {
            .physical_bank = 0x01U,
            .logical_pc = 0xC5C9U,
        },
        .registers = checkpoint,
    };
    auto scheduler = replace_coroutine_scheduler(
        scheduler_entry,
        staged);
    if (!scheduler || scheduler->checkpoints.empty()) {
        return std::nullopt;
    }

    constexpr std::uint32_t kSchedulerOffset = 819U;
    for (auto write : scheduler->work_ram_writes) {
        write.master_cycle_offset += kSchedulerOffset;
        replacement.work_ram_writes.push_back(write);
    }
    for (auto child_checkpoint : scheduler->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset +=
            kSchedulerOffset;
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
replace_skeleton_insect_center_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x55F9U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_skeleton_insect_center_gate gate{};
    if (!dc_classify_skeleton_insect_center_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &gate)) {
        throw std::runtime_error(
            "Native 55F9 replacement rejected the work-RAM snapshot.");
    }
    auto output = entry.registers;
    output.program_counter =
        gate.path == DC_SKELETON_INSECT_CENTER_GATE_IDLE
        ? 0x55FEU
        : 0x55FFU;
    output.accumulator = gate.trigger_latch;
    output.status = static_cast<std::uint8_t>(output.status & 0x7DU);
    if (output.accumulator == 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);
    } else if ((output.accumulator & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x80U);
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x02U,
            .logical_pc = output.program_counter,
        },
        .output_registers = output,
        .body_master_cycles =
            gate.path == DC_SKELETON_INSECT_CENTER_GATE_IDLE ? 21U : 27U,
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
                .instruction = {0x02U, instruction_pc},
                .registers = registers,
            });
        };

    append_checkpoint(0x55F9U, 0x55FCU, 15U, output);
    append_checkpoint(
        0x55FCU,
        output.program_counter,
        replacement.body_master_cycles,
        output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_skeleton_insect_center_continuation(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The trigger's shared D8D2 drawing helper intentionally remains
    // emulated. This boundary begins at its safe 5604 return continuation.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x5604U,
    };
    if (entry.routine != kRoutine
        || entry.registers.x != 0x32U
        || work_ram.size() <= 0x1FB2U
        || (entry.registers.status & 0x28U) != 0U) {
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

    RoutineReplacement replacement{};
    const auto append_checkpoint =
        [&replacement](
            const std::uint8_t physical_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {physical_bank, instruction_pc},
                .registers = registers,
            });
        };
    const auto add_write =
        [&replacement](
            const std::uint8_t physical_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t logical_address,
            const std::uint8_t value,
            const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {physical_bank, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
        };

    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    auto checkpoint = entry.registers;
    checkpoint.accumulator = 0x64U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x02U, 0x5604U, 0x5606U, 6U, checkpoint);

    append_checkpoint(0x02U, 0x5606U, 0x5608U, 18U, checkpoint);
    add_write(0x02U, 0x5606U, 0x202FU, 0x64U, 18U);
    staged[0x002FU] = static_cast<std::byte>(0x64U);

    append_checkpoint(0x02U, 0x5608U, 0x560AU, 30U, checkpoint);
    add_write(0x02U, 0x5608U, 0x2030U, 0x00U, 30U);
    staged[0x0030U] = static_cast<std::byte>(0x00U);

    const auto score_wrapper_stack_high = checkpoint.stack_pointer;
    const auto score_wrapper_stack_low = static_cast<std::uint8_t>(
        score_wrapper_stack_high - 1U);
    add_write(
        0x02U,
        0x560AU,
        static_cast<std::uint16_t>(0x2100U + score_wrapper_stack_high),
        0x56U,
        30U);
    add_write(
        0x02U,
        0x560AU,
        static_cast<std::uint16_t>(0x2100U + score_wrapper_stack_low),
        0x0CU,
        30U);
    staged[0x0100U + score_wrapper_stack_high] =
        static_cast<std::byte>(0x56U);
    staged[0x0100U + score_wrapper_stack_low] =
        static_cast<std::byte>(0x0CU);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(0x02U, 0x560AU, 0xC02AU, 51U, checkpoint);
    append_checkpoint(0x01U, 0xC02AU, 0xF54FU, 63U, checkpoint);

    checkpoint.y =
        std::to_integer<std::uint8_t>(staged[0x0D38U]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0x00U, 0xF54FU, 0xF552U, 78U, checkpoint);
    checkpoint.y = static_cast<std::uint8_t>(checkpoint.y + 1U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0x00U, 0xF552U, 0xF553U, 84U, checkpoint);

    add_write(
        0x00U,
        0xF553U,
        static_cast<std::uint16_t>(0x2100U + checkpoint.stack_pointer),
        checkpoint.y,
        93U);
    staged[0x0100U + checkpoint.stack_pointer] =
        static_cast<std::byte>(checkpoint.y);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer - 1U);
    append_checkpoint(0x00U, 0xF553U, 0xF554U, 93U, checkpoint);

    add_write(
        0x00U,
        0xF554U,
        static_cast<std::uint16_t>(0x2100U + checkpoint.stack_pointer),
        0xF5U,
        93U);
    add_write(
        0x00U,
        0xF554U,
        static_cast<std::uint16_t>(
            0x2100U
            + static_cast<std::uint8_t>(checkpoint.stack_pointer - 1U)),
        0x56U,
        93U);
    staged[0x0100U + checkpoint.stack_pointer] =
        static_cast<std::byte>(0xF5U);
    staged[
        0x0100U
        + static_cast<std::uint8_t>(checkpoint.stack_pointer - 1U)] =
        static_cast<std::byte>(0x56U);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(0x00U, 0xF554U, 0xF55CU, 114U, checkpoint);

    RoutineEntry score_entry{
        .routine = {
            .physical_bank = 0x00U,
            .logical_pc = 0xF55CU,
        },
        .registers = checkpoint,
    };
    auto score = replace_score_accumulator(score_entry, staged);
    if (!score || score->checkpoints.empty()) {
        return std::nullopt;
    }
    constexpr std::uint32_t kScoreOffset = 114U;
    for (auto write : score->work_ram_writes) {
        write.master_cycle_offset += kScoreOffset;
        replacement.work_ram_writes.push_back(write);
    }
    for (auto child_checkpoint : score->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset +=
            kScoreOffset;
        child_checkpoint.master_cycle_offset += kScoreOffset;
        replacement.checkpoints.push_back(child_checkpoint);
    }

    checkpoint = score->output_registers;
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer + 2U);
    append_checkpoint(0x00U, 0xF5A8U, 0xF557U, 690U, checkpoint);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer + 1U);
    checkpoint.y = std::to_integer<std::uint8_t>(
        staged[0x0100U + checkpoint.stack_pointer]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0x00U, 0xF557U, 0xF558U, 702U, checkpoint);
    checkpoint.y = static_cast<std::uint8_t>(checkpoint.y - 1U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0x00U, 0xF558U, 0xF559U, 708U, checkpoint);
    append_checkpoint(0x00U, 0xF559U, 0xF55BU, 714U, checkpoint);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer + 2U);
    append_checkpoint(0x00U, 0xF55BU, 0x560DU, 732U, checkpoint);

    const auto collision_offset = static_cast<std::size_t>(
        0x1080U + entry.registers.x);
    checkpoint.accumulator =
        std::to_integer<std::uint8_t>(staged[collision_offset]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x02U, 0x560DU, 0x5610U, 747U, checkpoint);
    checkpoint.accumulator =
        static_cast<std::uint8_t>(checkpoint.accumulator & 0xBFU);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x02U, 0x5610U, 0x5612U, 753U, checkpoint);
    append_checkpoint(0x02U, 0x5612U, 0x5615U, 774U, checkpoint);
    add_write(
        0x02U,
        0x5612U,
        static_cast<std::uint16_t>(0x3080U + entry.registers.x),
        checkpoint.accumulator,
        774U);
    staged[collision_offset] = static_cast<std::byte>(checkpoint.accumulator);

    checkpoint.accumulator = 0x10U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x02U, 0x5615U, 0x5617U, 780U, checkpoint);

    const auto delay_stack_high = checkpoint.stack_pointer;
    const auto delay_stack_low =
        static_cast<std::uint8_t>(delay_stack_high - 1U);
    add_write(
        0x02U,
        0x5617U,
        static_cast<std::uint16_t>(0x2100U + delay_stack_high),
        0x56U,
        780U);
    add_write(
        0x02U,
        0x5617U,
        static_cast<std::uint16_t>(0x2100U + delay_stack_low),
        0x19U,
        780U);
    staged[0x0100U + delay_stack_high] = static_cast<std::byte>(0x56U);
    staged[0x0100U + delay_stack_low] = static_cast<std::byte>(0x19U);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(0x02U, 0x5617U, 0xC021U, 801U, checkpoint);
    append_checkpoint(0x01U, 0xC021U, 0xC5EAU, 813U, checkpoint);

    const auto delay_offset = static_cast<std::size_t>(
        0x1D80U + entry.registers.x);
    append_checkpoint(0x01U, 0xC5EAU, 0xC5EDU, 828U, checkpoint);
    add_write(
        0x01U,
        0xC5EAU,
        static_cast<std::uint16_t>(0x3D80U + entry.registers.x),
        0x10U,
        828U);
    staged[delay_offset] = static_cast<std::byte>(0x10U);

    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer + 1U);
    checkpoint.accumulator =
        std::to_integer<std::uint8_t>(
            staged[0x0100U + checkpoint.stack_pointer]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x01U, 0xC5EDU, 0xC5EEU, 840U, checkpoint);
    append_checkpoint(0x01U, 0xC5EEU, 0xC5F0U, 852U, checkpoint);
    add_write(
        0x01U, 0xC5EEU, 0x202FU, checkpoint.accumulator, 852U);
    staged[0x002FU] = static_cast<std::byte>(checkpoint.accumulator);

    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer + 1U);
    checkpoint.accumulator =
        std::to_integer<std::uint8_t>(
            staged[0x0100U + checkpoint.stack_pointer]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x01U, 0xC5F0U, 0xC5F1U, 864U, checkpoint);
    append_checkpoint(0x01U, 0xC5F1U, 0xC5F3U, 876U, checkpoint);
    add_write(
        0x01U, 0xC5F1U, 0x2030U, checkpoint.accumulator, 876U);
    staged[0x0030U] = static_cast<std::byte>(checkpoint.accumulator);

    auto continuation_low = static_cast<std::uint8_t>(
        std::to_integer<std::uint8_t>(staged[0x002FU]) + 1U);
    staged[0x002FU] = static_cast<std::byte>(continuation_low);
    checkpoint.status = set_nz(checkpoint.status, continuation_low);
    append_checkpoint(0x01U, 0xC5F3U, 0xC5F5U, 894U, checkpoint);
    add_write(
        0x01U, 0xC5F3U, 0x202FU, continuation_low, 894U);
    if (continuation_low == 0U) {
        return std::nullopt;
    }
    append_checkpoint(0x01U, 0xC5F5U, 0xC5F9U, 906U, checkpoint);

    checkpoint.accumulator = continuation_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x01U, 0xC5F9U, 0xC5FBU, 915U, checkpoint);
    append_checkpoint(0x01U, 0xC5FBU, 0xC5FEU, 933U, checkpoint);
    add_write(
        0x01U,
        0xC5FBU,
        static_cast<std::uint16_t>(0x3F00U + entry.registers.x),
        checkpoint.accumulator,
        933U);
    staged[0x1F00U + entry.registers.x] =
        static_cast<std::byte>(checkpoint.accumulator);

    checkpoint.accumulator =
        std::to_integer<std::uint8_t>(staged[0x0030U]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x01U, 0xC5FEU, 0xC600U, 942U, checkpoint);
    append_checkpoint(0x01U, 0xC600U, 0xC603U, 960U, checkpoint);
    add_write(
        0x01U,
        0xC600U,
        static_cast<std::uint16_t>(0x3F80U + entry.registers.x),
        checkpoint.accumulator,
        960U);
    staged[0x1F80U + entry.registers.x] =
        static_cast<std::byte>(checkpoint.accumulator);
    append_checkpoint(0x01U, 0xC603U, 0x561AU, 981U, checkpoint);

    checkpoint.accumulator =
        std::to_integer<std::uint8_t>(staged[collision_offset]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x02U, 0x561AU, 0x561DU, 996U, checkpoint);
    checkpoint.accumulator =
        static_cast<std::uint8_t>(checkpoint.accumulator ^ 0x20U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x02U, 0x561DU, 0x561FU, 1002U, checkpoint);
    append_checkpoint(0x02U, 0x561FU, 0x5622U, 1017U, checkpoint);
    add_write(
        0x02U,
        0x561FU,
        static_cast<std::uint16_t>(0x3080U + entry.registers.x),
        checkpoint.accumulator,
        1017U);
    staged[collision_offset] = static_cast<std::byte>(checkpoint.accumulator);

    const auto scheduler_stack_high = checkpoint.stack_pointer;
    const auto scheduler_stack_low =
        static_cast<std::uint8_t>(scheduler_stack_high - 1U);
    add_write(
        0x02U,
        0x5622U,
        static_cast<std::uint16_t>(0x2100U + scheduler_stack_high),
        0x56U,
        1017U);
    add_write(
        0x02U,
        0x5622U,
        static_cast<std::uint16_t>(0x2100U + scheduler_stack_low),
        0x24U,
        1017U);
    staged[0x0100U + scheduler_stack_high] =
        static_cast<std::byte>(0x56U);
    staged[0x0100U + scheduler_stack_low] =
        static_cast<std::byte>(0x24U);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(0x02U, 0x5622U, 0xC018U, 1038U, checkpoint);
    append_checkpoint(0x01U, 0xC018U, 0xC5C9U, 1050U, checkpoint);

    RoutineEntry scheduler_entry{
        .routine = {
            .physical_bank = 0x01U,
            .logical_pc = 0xC5C9U,
        },
        .registers = checkpoint,
    };
    auto scheduler =
        replace_coroutine_scheduler(scheduler_entry, staged);
    if (!scheduler || scheduler->checkpoints.empty()) {
        return std::nullopt;
    }
    constexpr std::uint32_t kSchedulerOffset = 1050U;
    for (auto write : scheduler->work_ram_writes) {
        write.master_cycle_offset += kSchedulerOffset;
        replacement.work_ram_writes.push_back(write);
    }
    for (auto child_checkpoint : scheduler->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset +=
            kSchedulerOffset;
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
replace_devils_coffin_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x63F1U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_devils_coffin_gate gate{};
    if (!dc_classify_devils_coffin_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &gate)) {
        throw std::runtime_error(
            "Native 63F1 replacement rejected the work-RAM snapshot.");
    }

    const auto set_nz =
        [](std::uint8_t status, const std::uint8_t value) {
            status = static_cast<std::uint8_t>(status & 0x7DU);
            if (value == 0U) {
                status = static_cast<std::uint8_t>(status | 0x02U);
            } else if ((value & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x80U);
            }
            return status;
        };

    auto after_session_load = entry.registers;
    after_session_load.accumulator = gate.session_state;
    after_session_load.status =
        set_nz(after_session_load.status, after_session_load.accumulator);

    auto after_session_test = after_session_load;
    after_session_test.accumulator =
        static_cast<std::uint8_t>(gate.session_state & 0x08U);
    after_session_test.status =
        set_nz(after_session_test.status, after_session_test.accumulator);

    auto after_collision_load = after_session_test;
    after_collision_load.accumulator = gate.collision_state;
    after_collision_load.status =
        set_nz(after_collision_load.status, after_collision_load.accumulator);

    auto output = after_collision_load;
    output.program_counter = 0x6401U;

    const bool refresh_type =
        (gate.session_state & 0x08U) != 0U;
    const bool collision =
        gate.path == DC_DEVILS_COFFIN_GATE_COLLISION;
    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x6401U,
        },
        .output_registers = output,
        .body_master_cycles = refresh_type ? 66U : 51U,
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
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    append_checkpoint(0x63F1U, 0x63F3U, 12U, after_session_load);
    append_checkpoint(0x63F3U, 0x63F5U, 18U, after_session_test);
    if (!refresh_type) {
        append_checkpoint(0x63F5U, 0x63FCU, 30U, after_session_test);
    } else {
        auto after_type_load = after_session_test;
        after_type_load.accumulator = 0x83U;
        after_type_load.status =
            set_nz(after_type_load.status, after_type_load.accumulator);

        append_checkpoint(0x63F5U, 0x63F7U, 24U, after_session_test);
        append_checkpoint(0x63F7U, 0x63F9U, 30U, after_type_load);
        append_checkpoint(0x63F9U, 0x63FCU, 45U, after_type_load);
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x63F9U},
            .logical_address =
                static_cast<std::uint16_t>(0x3000U + gate.slot),
            .value = 0x83U,
            .master_cycle_offset = 45U,
        });
    }

    const std::uint32_t collision_load_cycle =
        refresh_type ? 60U : 45U;
    append_checkpoint(
        0x63FCU,
        0x63FFU,
        collision_load_cycle,
        after_collision_load);
    if (!collision) {
        append_checkpoint(
            0x63FFU,
            0x6401U,
            collision_load_cycle + 6U,
            output);
        return replacement;
    }

    const std::uint32_t collision_branch_cycle =
        collision_load_cycle + 12U;
    append_checkpoint(
        0x63FFU,
        0x6402U,
        collision_branch_cycle,
        after_collision_load);

    auto after_timer_load = after_collision_load;
    after_timer_load.accumulator = 0x01U;
    after_timer_load.status =
        set_nz(after_timer_load.status, after_timer_load.accumulator);
    append_checkpoint(
        0x6402U,
        0x6404U,
        collision_branch_cycle + 6U,
        after_timer_load);
    append_checkpoint(
        0x6404U,
        0x6407U,
        collision_branch_cycle + 21U,
        after_timer_load);

    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x6404U},
        .logical_address =
            static_cast<std::uint16_t>(0x3A00U + gate.slot),
        .value = 0x01U,
        .master_cycle_offset = collision_branch_cycle + 21U,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x6407U},
        .logical_address = static_cast<std::uint16_t>(
            0x2100U + entry.registers.stack_pointer),
        .value = 0x64U,
        .master_cycle_offset = collision_branch_cycle + 21U,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x6407U},
        .logical_address = static_cast<std::uint16_t>(
            0x2100U
            + static_cast<std::uint8_t>(
                entry.registers.stack_pointer - 1U)),
        .value = 0x09U,
        .master_cycle_offset = collision_branch_cycle + 21U,
    });

    auto scheduler_entry_registers = after_timer_load;
    scheduler_entry_registers.stack_pointer =
        static_cast<std::uint8_t>(
            scheduler_entry_registers.stack_pointer - 2U);
    append_checkpoint(
        0x6407U,
        0xC018U,
        collision_branch_cycle + 42U,
        scheduler_entry_registers);
    scheduler_entry_registers.program_counter = 0xC5C9U;
    const std::uint32_t scheduler_offset =
        collision_branch_cycle + 54U;
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset =
            scheduler_offset - 3U,
        .master_cycle_offset = scheduler_offset,
        .instruction = {0x01U, 0xC018U},
        .registers = scheduler_entry_registers,
    });

    auto staged = std::vector<std::byte>(
        work_ram.begin(),
        work_ram.end());
    staged[0x0100U + entry.registers.stack_pointer] =
        static_cast<std::byte>(0x64U);
    staged[
        0x0100U
        + static_cast<std::uint8_t>(
            entry.registers.stack_pointer - 1U)] =
        static_cast<std::byte>(0x09U);

    RoutineEntry scheduler_entry{
        .routine = {
            .physical_bank = 0x01U,
            .logical_pc = 0xC5C9U,
        },
        .registers = scheduler_entry_registers,
    };
    auto scheduler = replace_coroutine_scheduler(
        scheduler_entry,
        staged);
    if (!scheduler || scheduler->checkpoints.empty()) {
        return std::nullopt;
    }

    for (auto write : scheduler->work_ram_writes) {
        write.master_cycle_offset += scheduler_offset;
        replacement.work_ram_writes.push_back(write);
    }
    for (auto checkpoint : scheduler->checkpoints) {
        checkpoint.interrupt_sample_master_cycle_offset +=
            scheduler_offset;
        checkpoint.master_cycle_offset += scheduler_offset;
        replacement.checkpoints.push_back(checkpoint);
    }
    replacement.return_instruction = scheduler->return_instruction;
    replacement.output_registers = scheduler->output_registers;
    replacement.body_master_cycles =
        scheduler_offset + scheduler->body_master_cycles;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_devils_coffin_opening(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x640AU,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_devils_coffin_opening opening{};
    if (!dc_classify_devils_coffin_opening(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &opening)) {
        throw std::runtime_error(
            "Native 640A replacement rejected the work-RAM snapshot.");
    }
    const auto set_nz =
        [](std::uint8_t status, const std::uint8_t value) {
            status = static_cast<std::uint8_t>(status & 0x7DU);
            if (value == 0U) {
                status = static_cast<std::uint8_t>(status | 0x02U);
            } else if ((value & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x80U);
            }
            return status;
        };

    const auto set_compare =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right) {
            status = set_nz(
                static_cast<std::uint8_t>(status & 0xFEU),
                static_cast<std::uint8_t>(left - right));
            if (left >= right) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            return status;
        };

    auto after_decrement = entry.registers;
    after_decrement.status =
        set_nz(after_decrement.status, opening.decremented_timer);

    auto output = after_decrement;
    output.program_counter = 0x6426U;
    std::uint32_t body_master_cycles = 33U;
    if (opening.path != DC_DEVILS_COFFIN_OPENING_WAITING) {
        output.accumulator = opening.advanced_appearance;
        output.status = set_compare(
            output.status,
            opening.advanced_appearance,
            0x03U);
        body_master_cycles = 132U;
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x6426U,
        },
        .output_registers = output,
        .body_master_cycles = body_master_cycles,
    };
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x640AU},
        .logical_address = static_cast<std::uint16_t>(
            0x3A00U + opening.slot),
        .value = opening.decremented_timer,
        .master_cycle_offset = 21U,
    });
    if (opening.path != DC_DEVILS_COFFIN_OPENING_WAITING) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x6411U},
            .logical_address = static_cast<std::uint16_t>(
                0x3A00U + opening.slot),
            .value = 0x04U,
            .master_cycle_offset = 48U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x6414U},
            .logical_address = static_cast<std::uint16_t>(
                0x3980U + opening.slot),
            .value = opening.advanced_appearance,
            .master_cycle_offset = 69U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x641CU},
            .logical_address = static_cast<std::uint16_t>(
                0x3800U + opening.slot),
            .value = static_cast<std::uint8_t>(
                opening.collision_state | 0x40U),
            .master_cycle_offset = 105U,
        });
    }
    if (opening.path == DC_DEVILS_COFFIN_OPENING_HANDOFF) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x642CU},
            .logical_address = static_cast<std::uint16_t>(
                0x3800U + opening.slot),
            .value = static_cast<std::uint8_t>(
                (opening.collision_state | 0x40U) & 0x7FU),
            .master_cycle_offset = 174U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x642FU},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + entry.registers.stack_pointer),
            .value = 0x64U,
            .master_cycle_offset = 174U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x642FU},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U
                + static_cast<std::uint8_t>(
                    entry.registers.stack_pointer - 1U)),
            .value = 0x31U,
            .master_cycle_offset = 174U,
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
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    append_checkpoint(0x640AU, 0x640DU, 21U, after_decrement);
    if (opening.path == DC_DEVILS_COFFIN_OPENING_WAITING) {
        append_checkpoint(0x640DU, 0x6426U, 33U, output);
        return replacement;
    }

    append_checkpoint(0x640DU, 0x640FU, 27U, after_decrement);

    auto after_timer_load = after_decrement;
    after_timer_load.accumulator = 0x04U;
    after_timer_load.status = set_nz(after_timer_load.status, 0x04U);
    append_checkpoint(0x640FU, 0x6411U, 33U, after_timer_load);
    append_checkpoint(0x6411U, 0x6414U, 48U, after_timer_load);

    auto after_appearance_increment = after_timer_load;
    after_appearance_increment.status = set_nz(
        after_appearance_increment.status,
        opening.advanced_appearance);
    append_checkpoint(
        0x6414U,
        0x6417U,
        69U,
        after_appearance_increment);

    auto after_collision_load = after_appearance_increment;
    after_collision_load.accumulator = opening.collision_state;
    after_collision_load.status = set_nz(
        after_collision_load.status,
        opening.collision_state);
    append_checkpoint(
        0x6417U,
        0x641AU,
        84U,
        after_collision_load);

    auto after_collision_mark = after_collision_load;
    after_collision_mark.accumulator = static_cast<std::uint8_t>(
        opening.collision_state | 0x40U);
    after_collision_mark.status = set_nz(
        after_collision_mark.status,
        after_collision_mark.accumulator);
    append_checkpoint(
        0x641AU,
        0x641CU,
        90U,
        after_collision_mark);
    append_checkpoint(
        0x641CU,
        0x641FU,
        105U,
        after_collision_mark);

    auto after_appearance_load = after_collision_mark;
    after_appearance_load.accumulator = opening.advanced_appearance;
    after_appearance_load.status = set_nz(
        after_appearance_load.status,
        opening.advanced_appearance);
    append_checkpoint(
        0x641FU,
        0x6422U,
        120U,
        after_appearance_load);
    append_checkpoint(0x6422U, 0x6424U, 126U, output);
    if (opening.path == DC_DEVILS_COFFIN_OPENING_ADVANCE) {
        append_checkpoint(0x6424U, 0x6426U, 132U, output);
        return replacement;
    }

    append_checkpoint(0x6424U, 0x6427U, 138U, output);

    auto after_marked_collision_load = output;
    after_marked_collision_load.accumulator =
        static_cast<std::uint8_t>(opening.collision_state | 0x40U);
    after_marked_collision_load.status = set_nz(
        after_marked_collision_load.status,
        after_marked_collision_load.accumulator);
    append_checkpoint(
        0x6427U,
        0x642AU,
        153U,
        after_marked_collision_load);

    auto after_collision_clear = after_marked_collision_load;
    after_collision_clear.accumulator = static_cast<std::uint8_t>(
        after_marked_collision_load.accumulator & 0x7FU);
    after_collision_clear.status = set_nz(
        after_collision_clear.status,
        after_collision_clear.accumulator);
    append_checkpoint(
        0x642AU,
        0x642CU,
        159U,
        after_collision_clear);
    append_checkpoint(
        0x642CU,
        0x642FU,
        174U,
        after_collision_clear);

    auto scheduler_entry_registers = after_collision_clear;
    scheduler_entry_registers.stack_pointer =
        static_cast<std::uint8_t>(
            scheduler_entry_registers.stack_pointer - 2U);
    append_checkpoint(
        0x642FU,
        0xC018U,
        195U,
        scheduler_entry_registers);
    scheduler_entry_registers.program_counter = 0xC5C9U;
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 204U,
        .master_cycle_offset = 207U,
        .instruction = {0x01U, 0xC018U},
        .registers = scheduler_entry_registers,
    });

    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    staged[0x0100U + entry.registers.stack_pointer] =
        static_cast<std::byte>(0x64U);
    staged[
        0x0100U
        + static_cast<std::uint8_t>(
            entry.registers.stack_pointer - 1U)] =
        static_cast<std::byte>(0x31U);

    RoutineEntry scheduler_entry{
        .routine = {
            .physical_bank = 0x01U,
            .logical_pc = 0xC5C9U,
        },
        .registers = scheduler_entry_registers,
    };
    scheduler_entry.registers.program_counter = 0xC5C9U;
    auto scheduler = replace_coroutine_scheduler(
        scheduler_entry,
        staged);
    if (!scheduler || scheduler->checkpoints.empty()) {
        return std::nullopt;
    }

    constexpr std::uint32_t kSchedulerOffset = 207U;
    for (auto write : scheduler->work_ram_writes) {
        write.master_cycle_offset += kSchedulerOffset;
        replacement.work_ram_writes.push_back(write);
    }
    for (auto checkpoint : scheduler->checkpoints) {
        checkpoint.interrupt_sample_master_cycle_offset +=
            kSchedulerOffset;
        checkpoint.master_cycle_offset += kSchedulerOffset;
        replacement.checkpoints.push_back(checkpoint);
    }
    replacement.return_instruction = scheduler->return_instruction;
    replacement.output_registers = scheduler->output_registers;
    replacement.body_master_cycles =
        kSchedulerOffset + scheduler->body_master_cycles;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_devils_coffin_closing(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x6445U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_devils_coffin_closing closing{};
    if (!dc_classify_devils_coffin_closing(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &closing)) {
        throw std::runtime_error(
            "Native 6445 replacement rejected the work-RAM snapshot.");
    }
    const auto set_nz =
        [](std::uint8_t status, const std::uint8_t value) {
            status = static_cast<std::uint8_t>(status & 0x7DU);
            if (value == 0U) {
                status = static_cast<std::uint8_t>(status | 0x02U);
            } else if ((value & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x80U);
            }
            return status;
        };

    auto after_decrement = entry.registers;
    after_decrement.status =
        set_nz(after_decrement.status, closing.decremented_timer);

    auto output = after_decrement;
    output.program_counter = 0x645CU;
    std::uint32_t body_master_cycles = 33U;
    if (closing.path != DC_DEVILS_COFFIN_CLOSING_WAITING) {
        output.accumulator = static_cast<std::uint8_t>(
            closing.collision_state | 0x40U);
        output.status =
            set_nz(output.status, closing.decremented_appearance);
        body_master_cycles = 111U;
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x645CU,
        },
        .output_registers = output,
        .body_master_cycles = body_master_cycles,
    };
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x6445U},
        .logical_address = static_cast<std::uint16_t>(
            0x3A00U + closing.slot),
        .value = closing.decremented_timer,
        .master_cycle_offset = 21U,
    });
    if (closing.path != DC_DEVILS_COFFIN_CLOSING_WAITING) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x644CU},
            .logical_address = static_cast<std::uint16_t>(
                0x3A00U + closing.slot),
            .value = 0x04U,
            .master_cycle_offset = 48U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x6454U},
            .logical_address = static_cast<std::uint16_t>(
                0x3800U + closing.slot),
            .value = static_cast<std::uint8_t>(
                closing.collision_state | 0x40U),
            .master_cycle_offset = 84U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x6457U},
            .logical_address = static_cast<std::uint16_t>(
                0x3980U + closing.slot),
            .value = closing.decremented_appearance,
            .master_cycle_offset = 105U,
        });
    }
    if (closing.path == DC_DEVILS_COFFIN_CLOSING_COMPLETE) {
        const auto marked_collision = static_cast<std::uint8_t>(
            closing.collision_state | 0x40U);
        const auto initialized_collision = static_cast<std::uint8_t>(
            (marked_collision & 0x7FU) | 0x40U);
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x6461U},
            .logical_address = 0x2053U,
            .value = closing.closed_session_state,
            .master_cycle_offset = 147U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x6465U},
            .logical_address = static_cast<std::uint16_t>(
                0x3000U + closing.slot),
            .value = 0x82U,
            .master_cycle_offset = 168U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x63E8U},
            .logical_address = static_cast<std::uint16_t>(
                0x3800U + closing.slot),
            .value = initialized_collision,
            .master_cycle_offset = 222U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x63EBU},
            .logical_address = static_cast<std::uint16_t>(
                0x3980U + closing.slot),
            .value = 0x00U,
            .master_cycle_offset = 237U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x63EEU},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + entry.registers.stack_pointer),
            .value = 0x63U,
            .master_cycle_offset = 237U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x63EEU},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U
                + static_cast<std::uint8_t>(
                    entry.registers.stack_pointer - 1U)),
            .value = 0xF0U,
            .master_cycle_offset = 237U,
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
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    append_checkpoint(0x6445U, 0x6448U, 21U, after_decrement);
    if (closing.path == DC_DEVILS_COFFIN_CLOSING_WAITING) {
        append_checkpoint(0x6448U, 0x645CU, 33U, output);
        return replacement;
    }

    append_checkpoint(0x6448U, 0x644AU, 27U, after_decrement);

    auto after_timer_load = after_decrement;
    after_timer_load.accumulator = 0x04U;
    after_timer_load.status = set_nz(after_timer_load.status, 0x04U);
    append_checkpoint(0x644AU, 0x644CU, 33U, after_timer_load);
    append_checkpoint(0x644CU, 0x644FU, 48U, after_timer_load);

    auto after_collision_load = after_timer_load;
    after_collision_load.accumulator = closing.collision_state;
    after_collision_load.status = set_nz(
        after_collision_load.status,
        closing.collision_state);
    append_checkpoint(
        0x644FU,
        0x6452U,
        63U,
        after_collision_load);

    auto after_collision_mark = after_collision_load;
    after_collision_mark.accumulator = static_cast<std::uint8_t>(
        closing.collision_state | 0x40U);
    after_collision_mark.status = set_nz(
        after_collision_mark.status,
        after_collision_mark.accumulator);
    append_checkpoint(
        0x6452U,
        0x6454U,
        69U,
        after_collision_mark);
    append_checkpoint(
        0x6454U,
        0x6457U,
        84U,
        after_collision_mark);

    auto after_appearance_decrement = after_collision_mark;
    after_appearance_decrement.status = set_nz(
        after_appearance_decrement.status,
        closing.decremented_appearance);
    append_checkpoint(
        0x6457U,
        0x645AU,
        105U,
        after_appearance_decrement);
    if (closing.path == DC_DEVILS_COFFIN_CLOSING_ADVANCE) {
        append_checkpoint(0x645AU, 0x645CU, 111U, output);
        return replacement;
    }

    append_checkpoint(
        0x645AU,
        0x645DU,
        117U,
        after_appearance_decrement);

    auto after_session_load = after_appearance_decrement;
    after_session_load.accumulator = closing.session_state;
    after_session_load.status = set_nz(
        after_session_load.status,
        after_session_load.accumulator);
    append_checkpoint(
        0x645DU,
        0x645FU,
        129U,
        after_session_load);

    auto after_session_close = after_session_load;
    after_session_close.accumulator = closing.closed_session_state;
    after_session_close.status = set_nz(
        after_session_close.status,
        after_session_close.accumulator);
    append_checkpoint(
        0x645FU,
        0x6461U,
        135U,
        after_session_close);
    append_checkpoint(
        0x6461U,
        0x6463U,
        147U,
        after_session_close);

    auto after_type_load = after_session_close;
    after_type_load.accumulator = 0x82U;
    after_type_load.status = set_nz(
        after_type_load.status,
        after_type_load.accumulator);
    append_checkpoint(
        0x6463U,
        0x6465U,
        153U,
        after_type_load);
    append_checkpoint(
        0x6465U,
        0x6468U,
        168U,
        after_type_load);
    append_checkpoint(
        0x6468U,
        0x63E1U,
        180U,
        after_type_load);

    const auto marked_collision = static_cast<std::uint8_t>(
        closing.collision_state | 0x40U);
    auto after_initialized_collision_load = after_type_load;
    after_initialized_collision_load.accumulator = marked_collision;
    after_initialized_collision_load.status = set_nz(
        after_initialized_collision_load.status,
        marked_collision);
    append_checkpoint(
        0x63E1U,
        0x63E4U,
        195U,
        after_initialized_collision_load);

    auto after_collision_sign_clear = after_initialized_collision_load;
    after_collision_sign_clear.accumulator = static_cast<std::uint8_t>(
        marked_collision & 0x7FU);
    after_collision_sign_clear.status = set_nz(
        after_collision_sign_clear.status,
        after_collision_sign_clear.accumulator);
    append_checkpoint(
        0x63E4U,
        0x63E6U,
        201U,
        after_collision_sign_clear);

    auto after_collision_initialize = after_collision_sign_clear;
    after_collision_initialize.accumulator = static_cast<std::uint8_t>(
        after_collision_sign_clear.accumulator | 0x40U);
    after_collision_initialize.status = set_nz(
        after_collision_initialize.status,
        after_collision_initialize.accumulator);
    append_checkpoint(
        0x63E6U,
        0x63E8U,
        207U,
        after_collision_initialize);
    append_checkpoint(
        0x63E8U,
        0x63EBU,
        222U,
        after_collision_initialize);
    append_checkpoint(
        0x63EBU,
        0x63EEU,
        237U,
        after_collision_initialize);

    auto scheduler_entry_registers = after_collision_initialize;
    scheduler_entry_registers.stack_pointer =
        static_cast<std::uint8_t>(
            scheduler_entry_registers.stack_pointer - 2U);
    append_checkpoint(
        0x63EEU,
        0xC018U,
        258U,
        scheduler_entry_registers);
    scheduler_entry_registers.program_counter = 0xC5C9U;
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 267U,
        .master_cycle_offset = 270U,
        .instruction = {0x01U, 0xC018U},
        .registers = scheduler_entry_registers,
    });

    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    staged[0x0100U + entry.registers.stack_pointer] =
        static_cast<std::byte>(0x63U);
    staged[
        0x0100U
        + static_cast<std::uint8_t>(
            entry.registers.stack_pointer - 1U)] =
        static_cast<std::byte>(0xF0U);

    RoutineEntry scheduler_entry{
        .routine = {
            .physical_bank = 0x01U,
            .logical_pc = 0xC5C9U,
        },
        .registers = scheduler_entry_registers,
    };
    auto scheduler = replace_coroutine_scheduler(
        scheduler_entry,
        staged);
    if (!scheduler || scheduler->checkpoints.empty()) {
        return std::nullopt;
    }

    constexpr std::uint32_t kSchedulerOffset = 270U;
    for (auto write : scheduler->work_ram_writes) {
        write.master_cycle_offset += kSchedulerOffset;
        replacement.work_ram_writes.push_back(write);
    }
    for (auto checkpoint : scheduler->checkpoints) {
        checkpoint.interrupt_sample_master_cycle_offset +=
            kSchedulerOffset;
        checkpoint.master_cycle_offset += kSchedulerOffset;
        replacement.checkpoints.push_back(checkpoint);
    }
    replacement.return_instruction = scheduler->return_instruction;
    replacement.output_registers = scheduler->output_registers;
    replacement.body_master_cycles =
        kSchedulerOffset + scheduler->body_master_cycles;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_tower_guard_selector(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x60A6U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_tower_guard_selector selector{};
    if (!dc_classify_tower_guard_selector(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &selector)) {
        throw std::runtime_error(
            "Native 60A6 replacement rejected the work-RAM snapshot.");
    }
    if (selector.path != DC_TOWER_GUARD_SELECTOR_CLOSED) {
        return std::nullopt;
    }

    const auto set_nz =
        [](std::uint8_t status, const std::uint8_t value) {
            status = static_cast<std::uint8_t>(status & 0x7DU);
            if (value == 0U) {
                status = static_cast<std::uint8_t>(status | 0x02U);
            } else if ((value & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x80U);
            }
            return status;
        };

    auto after_load = entry.registers;
    after_load.accumulator = selector.selector;
    after_load.status = set_nz(after_load.status, after_load.accumulator);

    auto output = after_load;
    output.program_counter = 0x60B2U;
    output.y = selector.gate_counter;
    output.status = set_nz(output.status, output.y);

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x60B2U,
        },
        .output_registers = output,
        .body_master_cycles = 48U,
        .work_ram_writes = {{
            .instruction = {0x03U, 0x60AFU},
            .logical_address = static_cast<std::uint16_t>(
                0x3D00U + selector.slot),
            .value = selector.selector,
            .master_cycle_offset = 48U,
        }},
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
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    append_checkpoint(0x60A6U, 0x60A8U, 6U, after_load);
    append_checkpoint(0x60A8U, 0x60ABU, 21U, output);
    append_checkpoint(0x60ABU, 0x60AFU, 33U, output);
    append_checkpoint(0x60AFU, 0x60B2U, 48U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_fixed_scheduler_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x7B27U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_fixed_scheduler_gate gate{};
    if (!dc_classify_fixed_scheduler_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &gate)) {
        throw std::runtime_error(
            "Native 7B27 replacement rejected the work-RAM snapshot.");
    }
    if (gate.path == DC_FIXED_SCHEDULER_GATE_INACTIVE) {
        return std::nullopt;
    }

    const auto set_nz =
        [](std::uint8_t status, const std::uint8_t value) {
            status = static_cast<std::uint8_t>(status & 0x7DU);
            if (value == 0U) {
                status = static_cast<std::uint8_t>(status | 0x02U);
            } else if ((value & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x80U);
            }
            return status;
        };

    auto after_scene_load = entry.registers;
    after_scene_load.accumulator = gate.scene_state;
    after_scene_load.status =
        set_nz(after_scene_load.status, after_scene_load.accumulator);

    auto after_gate_load = after_scene_load;
    after_gate_load.accumulator = gate.presentation_gate;
    after_gate_load.status =
        set_nz(after_gate_load.status, after_gate_load.accumulator);

    const bool suppressed =
        gate.path == DC_FIXED_SCHEDULER_GATE_SUPPRESSED;
    const bool active = gate.path == DC_FIXED_SCHEDULER_GATE_ACTIVE;
    auto output = after_gate_load;
    output.program_counter =
        suppressed ? 0x7B34U : (active ? 0x7B4BU : 0x7B6CU);
    if (!suppressed) {
        output.status = set_nz(output.status, gate.decremented_timer);
    }
    if (active) {
        output.accumulator = 0x08U;
        output.status = set_nz(output.status, output.accumulator);
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc =
                suppressed ? 0x7B34U : (active ? 0x7B4BU : 0x7B6CU),
        },
        .output_registers = output,
        .body_master_cycles =
            suppressed ? 48U : (active ? 156U : 87U),
    };
    if (!suppressed) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x7B35U},
            .logical_address = static_cast<std::uint16_t>(
                0x3A00U + gate.slot),
            .value = gate.decremented_timer,
            .master_cycle_offset = 75U,
        });
    }
    if (active) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x7B3CU},
            .logical_address = static_cast<std::uint16_t>(
                0x3A00U + gate.slot),
            .value = 0x08U,
            .master_cycle_offset = 102U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x7B41U},
            .logical_address = 0x202AU,
            .value = 0x02U,
            .master_cycle_offset = 120U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x7B45U},
            .logical_address = 0x202FU,
            .value = 0x30U,
            .master_cycle_offset = 138U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x7B49U},
            .logical_address = 0x2030U,
            .value = 0x08U,
            .master_cycle_offset = 156U,
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
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    append_checkpoint(0x7B27U, 0x7B2AU, 15U, after_scene_load);
    append_checkpoint(0x7B2AU, 0x7B2FU, 27U, after_scene_load);
    append_checkpoint(0x7B2FU, 0x7B32U, 42U, after_gate_load);
    append_checkpoint(
        0x7B32U,
        suppressed ? 0x7B34U : 0x7B35U,
        suppressed ? 48U : 54U,
        after_gate_load);
    if (!suppressed) {
        auto after_decrement = after_gate_load;
        after_decrement.status =
            set_nz(after_decrement.status, gate.decremented_timer);
        append_checkpoint(0x7B35U, 0x7B38U, 75U, after_decrement);
        if (active) {
            append_checkpoint(
                0x7B38U, 0x7B3AU, 81U, after_decrement);
            auto checkpoint = after_decrement;
            checkpoint.accumulator = 0x08U;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(0x7B3AU, 0x7B3CU, 87U, checkpoint);
            append_checkpoint(0x7B3CU, 0x7B3FU, 102U, checkpoint);
            checkpoint.accumulator = 0x02U;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(0x7B3FU, 0x7B41U, 108U, checkpoint);
            append_checkpoint(0x7B41U, 0x7B43U, 120U, checkpoint);
            checkpoint.accumulator = 0x30U;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(0x7B43U, 0x7B45U, 126U, checkpoint);
            append_checkpoint(0x7B45U, 0x7B47U, 138U, checkpoint);
            checkpoint.accumulator = 0x08U;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(0x7B47U, 0x7B49U, 144U, checkpoint);
            append_checkpoint(0x7B49U, 0x7B4BU, 156U, checkpoint);
        } else {
            append_checkpoint(
                0x7B38U, 0x7B6CU, 87U, after_decrement);
        }
    }
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_fixed_scheduler_submission(
    const RoutineEntry& entry)
{
    if (entry.routine.physical_bank != 0x03U
        || entry.registers.x != 0x5CU) {
        return std::nullopt;
    }

    std::uint8_t selector = 0U;
    std::uint16_t return_pc = 0U;
    if (entry.routine.logical_pc == 0x7B4EU) {
        selector = 0x03U;
        return_pc = 0x7B5AU;
    } else if (entry.routine.logical_pc == 0x7B5DU) {
        selector = 0x07U;
        return_pc = 0x7B69U;
    } else {
        return std::nullopt;
    }

    const auto set_nz =
        [](std::uint8_t status, const std::uint8_t value) {
            status = static_cast<std::uint8_t>(status & 0x7DU);
            if (value == 0U) {
                status = static_cast<std::uint8_t>(status | 0x02U);
            } else if ((value & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x80U);
            }
            return status;
        };

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = return_pc,
        },
        .output_registers = entry.registers,
        .body_master_cycles = 54U,
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
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    auto checkpoint = entry.registers;
    checkpoint.accumulator = selector;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        entry.routine.logical_pc,
        static_cast<std::uint16_t>(entry.routine.logical_pc + 2U),
        6U,
        checkpoint);
    append_checkpoint(
        static_cast<std::uint16_t>(entry.routine.logical_pc + 2U),
        static_cast<std::uint16_t>(entry.routine.logical_pc + 4U),
        18U,
        checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {
            0x03U,
            static_cast<std::uint16_t>(entry.routine.logical_pc + 2U),
        },
        .logical_address = 0x202AU,
        .value = selector,
        .master_cycle_offset = 18U,
    });

    checkpoint.accumulator = 0x30U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        static_cast<std::uint16_t>(entry.routine.logical_pc + 4U),
        static_cast<std::uint16_t>(entry.routine.logical_pc + 6U),
        24U,
        checkpoint);
    append_checkpoint(
        static_cast<std::uint16_t>(entry.routine.logical_pc + 6U),
        static_cast<std::uint16_t>(entry.routine.logical_pc + 8U),
        36U,
        checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {
            0x03U,
            static_cast<std::uint16_t>(entry.routine.logical_pc + 6U),
        },
        .logical_address = 0x202FU,
        .value = 0x30U,
        .master_cycle_offset = 36U,
    });

    checkpoint.accumulator = 0x08U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        static_cast<std::uint16_t>(entry.routine.logical_pc + 8U),
        static_cast<std::uint16_t>(entry.routine.logical_pc + 10U),
        42U,
        checkpoint);
    append_checkpoint(
        static_cast<std::uint16_t>(entry.routine.logical_pc + 10U),
        return_pc,
        54U,
        checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {
            0x03U,
            static_cast<std::uint16_t>(entry.routine.logical_pc + 10U),
        },
        .logical_address = 0x2030U,
        .value = 0x08U,
        .master_cycle_offset = 54U,
    });
    replacement.output_registers = checkpoint;
    replacement.output_registers.program_counter = return_pc;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_fixed_marker_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x7B7AU,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_fixed_scheduler_gate gate{};
    if (!dc_classify_fixed_scheduler_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &gate)) {
        throw std::runtime_error(
            "Native 7B7A replacement rejected the work-RAM snapshot.");
    }
    if (gate.path == DC_FIXED_SCHEDULER_GATE_INACTIVE) {
        return std::nullopt;
    }

    const auto set_nz =
        [](std::uint8_t status, const std::uint8_t value) {
            status = static_cast<std::uint8_t>(status & 0x7DU);
            if (value == 0U) {
                status = static_cast<std::uint8_t>(status | 0x02U);
            } else if ((value & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x80U);
            }
            return status;
        };

    auto after_scene_load = entry.registers;
    after_scene_load.accumulator = gate.scene_state;
    after_scene_load.status =
        set_nz(after_scene_load.status, after_scene_load.accumulator);

    auto after_gate_load = after_scene_load;
    after_gate_load.accumulator = gate.presentation_gate;
    after_gate_load.status =
        set_nz(after_gate_load.status, after_gate_load.accumulator);

    const bool suppressed =
        gate.path == DC_FIXED_SCHEDULER_GATE_SUPPRESSED;
    const bool active = gate.path == DC_FIXED_SCHEDULER_GATE_ACTIVE;
    const auto phase_offset =
        static_cast<std::size_t>(0x1A80U + gate.slot);
    if (active && work_ram.size() <= phase_offset) {
        throw std::runtime_error(
            "Native 7B7A replacement rejected the marker phase snapshot.");
    }
    const auto prior_phase =
        active
            ? std::to_integer<std::uint8_t>(work_ram[phase_offset])
            : 0U;
    const auto incremented_phase =
        static_cast<std::uint8_t>(prior_phase + 1U);
    const bool phase_wrap = active && incremented_phase >= 0x08U;
    const std::uint8_t output_phase =
        phase_wrap ? 0U : incremented_phase;
    const std::uint8_t marker_mode =
        active
            ? std::to_integer<std::uint8_t>(work_ram[0x0055U])
            : 0U;
    const bool marker_mode_branch = active && marker_mode != 0U;
    const std::uint32_t active_body_cycles =
        231U + (phase_wrap ? 9U : 0U)
        + (marker_mode_branch ? 6U : 0U);
    auto output = after_gate_load;
    output.program_counter =
        suppressed ? 0x7B87U : (active ? 0x7BB1U : 0x7BDFU);
    if (!suppressed) {
        output.status = set_nz(output.status, gate.decremented_timer);
    }
    if (active) {
        output.accumulator = 0x1EU;
        output.y = output_phase;
        output.status = set_nz(output.status, output.accumulator);
        if (phase_wrap) {
            output.status =
                static_cast<std::uint8_t>(output.status | 0x01U);
        } else {
            output.status =
                static_cast<std::uint8_t>(output.status & 0xFEU);
        }
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc =
                suppressed ? 0x7B87U : (active ? 0x7BB1U : 0x7BDFU),
        },
        .output_registers = output,
        .body_master_cycles =
            suppressed ? 48U : (active ? active_body_cycles : 93U),
    };
    if (!suppressed) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x7B88U},
            .logical_address = static_cast<std::uint16_t>(
                0x3A00U + gate.slot),
            .value = gate.decremented_timer,
            .master_cycle_offset = 75U,
        });
    }
    if (active) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x7B92U},
            .logical_address = static_cast<std::uint16_t>(
                0x3A00U + gate.slot),
            .value = 0x08U,
            .master_cycle_offset = 108U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x7B95U},
            .logical_address = static_cast<std::uint16_t>(
                0x3A80U + gate.slot),
            .value = incremented_phase,
            .master_cycle_offset = 129U,
        });
        if (phase_wrap) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x03U, 0x7B9FU},
                .logical_address = static_cast<std::uint16_t>(
                    0x3A80U + gate.slot),
                .value = 0x00U,
                .master_cycle_offset = 171U,
            });
        }
        const std::uint32_t descriptor_low_cycle =
            213U + (phase_wrap ? 9U : 0U)
            + (marker_mode_branch ? 6U : 0U);
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x7BABU},
            .logical_address = 0x202FU,
            .value = 0x16U,
            .master_cycle_offset = descriptor_low_cycle,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x7BAFU},
            .logical_address = 0x2030U,
            .value = 0x1EU,
            .master_cycle_offset = descriptor_low_cycle + 18U,
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
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    append_checkpoint(0x7B7AU, 0x7B7DU, 15U, after_scene_load);
    append_checkpoint(0x7B7DU, 0x7B82U, 27U, after_scene_load);
    append_checkpoint(0x7B82U, 0x7B85U, 42U, after_gate_load);
    append_checkpoint(
        0x7B85U,
        suppressed ? 0x7B87U : 0x7B88U,
        suppressed ? 48U : 54U,
        after_gate_load);
    if (!suppressed) {
        auto after_decrement = after_gate_load;
        after_decrement.status =
            set_nz(after_decrement.status, gate.decremented_timer);
        append_checkpoint(0x7B88U, 0x7B8BU, 75U, after_decrement);
        if (!active) {
            append_checkpoint(0x7B8BU, 0x7B8DU, 81U, after_decrement);
            append_checkpoint(0x7B8DU, 0x7BDFU, 93U, after_decrement);
        } else {
            append_checkpoint(0x7B8BU, 0x7B90U, 87U, after_decrement);
            auto checkpoint = after_decrement;
            checkpoint.accumulator = 0x08U;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(0x7B90U, 0x7B92U, 93U, checkpoint);
            append_checkpoint(0x7B92U, 0x7B95U, 108U, checkpoint);
            checkpoint.status =
                set_nz(checkpoint.status, incremented_phase);
            append_checkpoint(0x7B95U, 0x7B98U, 129U, checkpoint);
            checkpoint.accumulator = incremented_phase;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(0x7B98U, 0x7B9BU, 144U, checkpoint);
            const auto compare_result =
                static_cast<std::uint8_t>(incremented_phase - 0x08U);
            checkpoint.status =
                set_nz(checkpoint.status, compare_result);
            if (incremented_phase >= 0x08U) {
                checkpoint.status =
                    static_cast<std::uint8_t>(checkpoint.status | 0x01U);
            } else {
                checkpoint.status =
                    static_cast<std::uint8_t>(checkpoint.status & 0xFEU);
            }
            append_checkpoint(0x7B9BU, 0x7B9DU, 150U, checkpoint);
            if (phase_wrap) {
                append_checkpoint(0x7B9DU, 0x7B9FU, 156U, checkpoint);
                append_checkpoint(0x7B9FU, 0x7BA2U, 171U, checkpoint);
            } else {
                append_checkpoint(0x7B9DU, 0x7BA2U, 162U, checkpoint);
            }
            checkpoint.y = output_phase;
            checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
            const std::uint32_t phase_selected_cycle =
                phase_wrap ? 186U : 177U;
            append_checkpoint(
                0x7BA2U, 0x7BA5U, phase_selected_cycle, checkpoint);
            checkpoint.accumulator = marker_mode;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            const std::uint32_t mode_loaded_cycle =
                phase_selected_cycle + 12U;
            append_checkpoint(
                0x7BA5U, 0x7BA7U, mode_loaded_cycle, checkpoint);
            const std::uint32_t mode_branch_cycle =
                mode_loaded_cycle + (marker_mode_branch ? 12U : 6U);
            append_checkpoint(
                0x7BA7U, 0x7BA9U, mode_branch_cycle, checkpoint);
            checkpoint.accumulator = 0x16U;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x7BA9U, 0x7BABU, mode_branch_cycle + 6U, checkpoint);
            append_checkpoint(
                0x7BABU, 0x7BADU, mode_branch_cycle + 18U, checkpoint);
            checkpoint.accumulator = 0x1EU;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x7BADU, 0x7BAFU, mode_branch_cycle + 24U, checkpoint);
            append_checkpoint(
                0x7BAFU, 0x7BB1U, mode_branch_cycle + 36U, checkpoint);
        }
    }
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_fixed_marker_submission(
    const RoutineEntry& entry)
{
    if (entry.routine.physical_bank != 0x03U
        || entry.registers.x != 0x5DU) {
        return std::nullopt;
    }

    std::uint8_t descriptor_low = 0U;
    std::uint16_t return_pc = 0U;
    if (entry.routine.logical_pc == 0x7BBBU) {
        descriptor_low = 0x18U;
        return_pc = 0x7BC3U;
    } else if (entry.routine.logical_pc == 0x7BCDU) {
        descriptor_low = 0x1AU;
        return_pc = 0x7BD5U;
    } else {
        return std::nullopt;
    }

    const auto set_nz =
        [](std::uint8_t status, const std::uint8_t value) {
            status = static_cast<std::uint8_t>(status & 0x7DU);
            if (value == 0U) {
                status = static_cast<std::uint8_t>(status | 0x02U);
            } else if ((value & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x80U);
            }
            return status;
        };

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = return_pc,
        },
        .output_registers = entry.registers,
        .body_master_cycles = 36U,
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
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    auto checkpoint = entry.registers;
    checkpoint.accumulator = descriptor_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        entry.routine.logical_pc,
        static_cast<std::uint16_t>(entry.routine.logical_pc + 2U),
        6U,
        checkpoint);
    append_checkpoint(
        static_cast<std::uint16_t>(entry.routine.logical_pc + 2U),
        static_cast<std::uint16_t>(entry.routine.logical_pc + 4U),
        18U,
        checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {
            0x03U,
            static_cast<std::uint16_t>(entry.routine.logical_pc + 2U),
        },
        .logical_address = 0x202FU,
        .value = descriptor_low,
        .master_cycle_offset = 18U,
    });

    checkpoint.accumulator = 0x1EU;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        static_cast<std::uint16_t>(entry.routine.logical_pc + 4U),
        static_cast<std::uint16_t>(entry.routine.logical_pc + 6U),
        24U,
        checkpoint);
    append_checkpoint(
        static_cast<std::uint16_t>(entry.routine.logical_pc + 6U),
        return_pc,
        36U,
        checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {
            0x03U,
            static_cast<std::uint16_t>(entry.routine.logical_pc + 6U),
        },
        .logical_address = 0x2030U,
        .value = 0x1EU,
        .master_cycle_offset = 36U,
    });
    replacement.output_registers = checkpoint;
    replacement.output_registers.program_counter = return_pc;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_fixed_marker_helper_segment(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    if (entry.routine.physical_bank != 0x03U
        || work_ram.size() < 0x0200U) {
        return std::nullopt;
    }

    const auto set_nz =
        [](std::uint8_t status, const std::uint8_t value) {
            status = static_cast<std::uint8_t>(status & 0x7DU);
            if (value == 0U) {
                status = static_cast<std::uint8_t>(status | 0x02U);
            } else if ((value & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x80U);
            }
            return status;
        };
    const auto stack_offset = [](const std::uint8_t stack_pointer) {
        return static_cast<std::size_t>(0x0100U + stack_pointer);
    };

    RoutineReplacement replacement{};
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
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    auto checkpoint = entry.registers;
    if (entry.routine.logical_pc == 0x7BE0U) {
        static constexpr std::array<std::uint8_t, 8U> kPhaseWave{
            0U, 1U, 2U, 3U, 4U, 3U, 2U, 1U,
        };
        if (checkpoint.y >= kPhaseWave.size()
            || checkpoint.stack_pointer < 0x01U) {
            return std::nullopt;
        }

        const auto entry_stack_pointer = checkpoint.stack_pointer;
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x7BE0U},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + entry_stack_pointer),
            .value = checkpoint.y,
            .master_cycle_offset = 9U,
        });
        checkpoint.stack_pointer = static_cast<std::uint8_t>(
            checkpoint.stack_pointer - 1U);
        append_checkpoint(0x7BE0U, 0x7BE1U, 9U, checkpoint);

        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x7BE1U},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + checkpoint.stack_pointer),
            .value = checkpoint.accumulator,
            .master_cycle_offset = 18U,
        });
        checkpoint.stack_pointer = static_cast<std::uint8_t>(
            checkpoint.stack_pointer - 1U);
        append_checkpoint(0x7BE1U, 0x7BE2U, 18U, checkpoint);

        checkpoint.accumulator = kPhaseWave[checkpoint.y];
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0x7BE2U, 0x7BE5U, 33U, checkpoint);

        replacement.return_instruction = {0x03U, 0x7BE5U};
        replacement.output_registers = checkpoint;
        replacement.output_registers.program_counter = 0x7BE5U;
        replacement.body_master_cycles = 33U;
        return replacement;
    }

    if (entry.routine.logical_pc == 0x7BE8U) {
        if (checkpoint.stack_pointer == 0xFFU) {
            return std::nullopt;
        }
        checkpoint.stack_pointer = static_cast<std::uint8_t>(
            checkpoint.stack_pointer + 1U);
        checkpoint.accumulator = std::to_integer<std::uint8_t>(
            work_ram[stack_offset(checkpoint.stack_pointer)]);
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0x7BE8U, 0x7BE9U, 12U, checkpoint);

        replacement.return_instruction = {0x03U, 0x7BE9U};
        replacement.output_registers = checkpoint;
        replacement.output_registers.program_counter = 0x7BE9U;
        replacement.body_master_cycles = 12U;
        return replacement;
    }

    if (entry.routine.logical_pc == 0x7BECU) {
        if (checkpoint.stack_pointer == 0xFFU) {
            return std::nullopt;
        }
        checkpoint.stack_pointer = static_cast<std::uint8_t>(
            checkpoint.stack_pointer + 1U);
        checkpoint.y = std::to_integer<std::uint8_t>(
            work_ram[stack_offset(checkpoint.stack_pointer)]);
        checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
        append_checkpoint(0x7BECU, 0x7BEDU, 12U, checkpoint);

        replacement.return_instruction = {0x03U, 0x7BEDU};
        replacement.output_registers = checkpoint;
        replacement.output_registers.program_counter = 0x7BEDU;
        replacement.body_master_cycles = 12U;
        return replacement;
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_scaled_word_add(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x5955U,
    };
    if (entry.routine != kRoutine || (entry.registers.status & 0x08U) != 0U) {
        return std::nullopt;
    }

    dc_scaled_word_add addition{};
    if (!dc_add_scaled_byte_to_word(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.accumulator,
            &addition)) {
        throw std::runtime_error(
            "Native 5955 replacement rejected the work-RAM snapshot.");
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x02U,
            .logical_pc = 0x5971U,
        },
        .output_registers = entry.registers,
        .body_master_cycles = 198U,
    };
    replacement.work_ram_writes.reserve(8U);
    replacement.checkpoints.reserve(15U);

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
            status = static_cast<std::uint8_t>(status & 0xFEU);
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
            status = static_cast<std::uint8_t>(status & 0x3EU);
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
                .instruction = {0x02U, instruction_pc},
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
                .instruction = {0x02U, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
        };

    auto checkpoint = entry.registers;
    std::uint32_t cycle = 12U;
    std::uint8_t low = addition.input;
    std::uint8_t high = 0U;
    append_write(0x5955U, 0x202EU, high, cycle);
    append_checkpoint(0x5955U, 0x5957U, cycle, checkpoint);

    static constexpr std::array<std::uint16_t, 5U> kLowShiftPcs{
        0x5957U, 0x595AU, 0x595DU, 0x5960U, 0x5963U,
    };
    static constexpr std::array<std::uint16_t, 5U> kHighShiftPcs{
        0x5958U, 0x595BU, 0x595EU, 0x5961U, 0x5964U,
    };
    for (std::size_t index = 0U;
         index < DC_SCALED_WORD_SHIFT_COUNT;
         ++index) {
        const auto low_before = low;
        low = addition.shifted_low[index];
        cycle += 6U;
        checkpoint.accumulator = low;
        checkpoint.status =
            shift_status(checkpoint.status, low_before, low);
        append_checkpoint(
            kLowShiftPcs[index],
            kHighShiftPcs[index],
            cycle,
            checkpoint);

        const auto high_before = high;
        high = addition.shifted_high[index];
        cycle += 18U;
        checkpoint.status =
            shift_status(checkpoint.status, high_before, high);
        append_write(
            kHighShiftPcs[index],
            0x202EU,
            high,
            cycle);
        append_checkpoint(
            kHighShiftPcs[index],
            static_cast<std::uint16_t>(
                index + 1U < DC_SCALED_WORD_SHIFT_COUNT
                    ? kLowShiftPcs[index + 1U]
                    : 0x5966U),
            cycle,
            checkpoint);
    }

    cycle += 6U;
    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status & 0xFEU);
    append_checkpoint(0x5966U, 0x5967U, cycle, checkpoint);

    const auto low_before_add = checkpoint.accumulator;
    cycle += 12U;
    checkpoint.accumulator = addition.result_low;
    checkpoint.status = adc_status(
        checkpoint.status,
        low_before_add,
        addition.base_low,
        false,
        checkpoint.accumulator);
    append_checkpoint(0x5967U, 0x5969U, cycle, checkpoint);

    cycle += 12U;
    append_write(
        0x5969U,
        0x202FU,
        addition.result_low,
        cycle);
    append_checkpoint(0x5969U, 0x596BU, cycle, checkpoint);

    cycle += 12U;
    checkpoint.accumulator = high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x596BU, 0x596DU, cycle, checkpoint);

    const auto high_before_add = checkpoint.accumulator;
    const bool carry = (checkpoint.status & 0x01U) != 0U;
    cycle += 12U;
    checkpoint.accumulator = addition.result_high;
    checkpoint.status = adc_status(
        checkpoint.status,
        high_before_add,
        addition.base_high,
        carry,
        checkpoint.accumulator);
    append_checkpoint(0x596DU, 0x596FU, cycle, checkpoint);

    cycle += 12U;
    append_write(
        0x596FU,
        0x2030U,
        addition.result_high,
        cycle);
    append_checkpoint(0x596FU, 0x5971U, cycle, checkpoint);

    if (cycle != replacement.body_master_cycles
        || low != addition.shifted_low[DC_SCALED_WORD_SHIFT_COUNT - 1U]
        || high != addition.shifted_high[DC_SCALED_WORD_SHIFT_COUNT - 1U]) {
        throw std::runtime_error(
            "Native 5955 transcript diverged from its state model.");
    }
    checkpoint.program_counter = 0x5971U;
    replacement.output_registers = checkpoint;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_fixed_response_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x7CAFU,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_fixed_response_gate gate{};
    if (!dc_classify_fixed_response_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &gate)) {
        throw std::runtime_error(
            "Native 7CAF replacement rejected the work-RAM snapshot.");
    }
    if (gate.path != DC_FIXED_RESPONSE_GATE_DORMANT) {
        return std::nullopt;
    }

    const auto set_nz =
        [](std::uint8_t status, const std::uint8_t value) {
            status = static_cast<std::uint8_t>(status & 0x7DU);
            if (value == 0U) {
                status = static_cast<std::uint8_t>(status | 0x02U);
            } else if ((value & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x80U);
            }
            return status;
        };

    auto after_scene_load = entry.registers;
    after_scene_load.accumulator = gate.scene_state;
    after_scene_load.status =
        set_nz(after_scene_load.status, after_scene_load.accumulator);

    auto output = after_scene_load;
    output.program_counter = 0x7CBBU;
    output.accumulator = gate.collision_scratch;
    output.status = set_nz(output.status, output.accumulator);

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x7CBBU,
        },
        .output_registers = output,
        .body_master_cycles = 45U,
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
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    append_checkpoint(0x7CAFU, 0x7CB2U, 15U, after_scene_load);
    append_checkpoint(0x7CB2U, 0x7CB7U, 27U, after_scene_load);
    append_checkpoint(0x7CB7U, 0x7CB9U, 39U, output);
    append_checkpoint(0x7CB9U, 0x7CBBU, 45U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_dragon_egg_hit_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x4345U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_dragon_egg_hit_gate gate{};
    if (!dc_classify_dragon_egg_hit_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &gate)) {
        throw std::runtime_error(
            "Native 4345 replacement rejected the work-RAM snapshot.");
    }
    const auto set_nz =
        [](std::uint8_t status, const std::uint8_t value) {
            status = static_cast<std::uint8_t>(status & 0x7DU);
            if (value == 0U) {
                status = static_cast<std::uint8_t>(status | 0x02U);
            } else if ((value & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x80U);
            }
            return status;
        };

    auto after_collision_load = entry.registers;
    after_collision_load.accumulator = gate.collision_state;
    after_collision_load.status =
        set_nz(after_collision_load.status, after_collision_load.accumulator);

    auto output = after_collision_load;
    output.program_counter = 0x434AU;

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x02U,
            .logical_pc = 0x434AU,
        },
        .output_registers = output,
        .body_master_cycles = 21U,
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
                .instruction = {0x02U, instruction_pc},
                .registers = registers,
            });
        };

    append_checkpoint(0x4345U, 0x4348U, 15U, after_collision_load);
    if (gate.path == DC_DRAGON_EGG_HIT_GATE_IDLE) {
        append_checkpoint(0x4348U, 0x434AU, 21U, output);
        return replacement;
    }

    append_checkpoint(0x4348U, 0x434BU, 27U, after_collision_load);
    output.accumulator = 0x0AU;
    output.status = set_nz(output.status, output.accumulator);
    append_checkpoint(0x434BU, 0x434DU, 33U, output);
    append_checkpoint(0x434DU, 0x434FU, 45U, output);
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x434DU},
        .logical_address = 0x202FU,
        .value = 0x0AU,
        .master_cycle_offset = 45U,
    });
    append_checkpoint(0x434FU, 0x4351U, 57U, output);
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x434FU},
        .logical_address = 0x2030U,
        .value = 0x00U,
        .master_cycle_offset = 57U,
    });
    replacement.return_instruction.logical_pc = 0x4351U;
    replacement.output_registers = output;
    replacement.output_registers.program_counter = 0x4351U;
    replacement.body_master_cycles = 57U;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_dragon_egg_hit_continuation(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // Shared score, burst allocation, and hatchling allocation stay emulated.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x4357U,
    };
    if (entry.routine != kRoutine
        || entry.registers.x < 0x20U
        || entry.registers.x > 0x25U
        || (entry.registers.status & 0x20U) != 0U) {
        return std::nullopt;
    }

    const auto actor_state_offset =
        static_cast<std::size_t>(0x1080U + entry.registers.x);
    if (work_ram.size() <= actor_state_offset
        || work_ram.size() <= 0x0D6EU
        || work_ram.size() <= 0x002CU) {
        return std::nullopt;
    }

    const auto set_nz =
        [](std::uint8_t status, const std::uint8_t value) {
            status = static_cast<std::uint8_t>(status & 0x7DU);
            if (value == 0U) {
                status = static_cast<std::uint8_t>(status | 0x02U);
            } else if ((value & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x80U);
            }
            return status;
        };
    const auto append_checkpoint =
        [](RoutineReplacement& replacement,
           const std::uint16_t instruction_pc,
           const std::uint16_t resume_pc,
           const std::uint32_t master_cycle,
           auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x02U, instruction_pc},
                .registers = registers,
            });
        };

    auto checkpoint = entry.registers;
    checkpoint.accumulator =
        std::to_integer<std::uint8_t>(work_ram[actor_state_offset]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x02U,
            .logical_pc = 0x436CU,
        },
        .output_registers = checkpoint,
        .body_master_cycles = 93U,
    };
    append_checkpoint(replacement, 0x4357U, 0x435AU, 15U, checkpoint);

    checkpoint.accumulator =
        static_cast<std::uint8_t>(checkpoint.accumulator | 0x40U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x435AU, 0x435CU, 21U, checkpoint);
    append_checkpoint(replacement, 0x435CU, 0x435FU, 36U, checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x435CU},
        .logical_address = static_cast<std::uint16_t>(
            0x3080U + entry.registers.x),
        .value = checkpoint.accumulator,
        .master_cycle_offset = 36U,
    });

    checkpoint.accumulator = 0x39U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x435FU, 0x4361U, 42U, checkpoint);
    append_checkpoint(replacement, 0x4361U, 0x4364U, 57U, checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x4361U},
        .logical_address = 0x2D6EU,
        .value = 0x39U,
        .master_cycle_offset = 57U,
    });

    checkpoint.accumulator = 0xF8U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x4364U, 0x4366U, 63U, checkpoint);
    append_checkpoint(replacement, 0x4366U, 0x4368U, 75U, checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x4366U},
        .logical_address = 0x202BU,
        .value = 0xF8U,
        .master_cycle_offset = 75U,
    });
    append_checkpoint(replacement, 0x4368U, 0x436AU, 81U, checkpoint);
    append_checkpoint(replacement, 0x436AU, 0x436CU, 93U, checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x436AU},
        .logical_address = 0x202CU,
        .value = 0xF8U,
        .master_cycle_offset = 93U,
    });
    replacement.output_registers = checkpoint;
    replacement.output_registers.program_counter = 0x436CU;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_dragon_egg_post_allocation(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x4371U,
    };
    if (entry.routine != kRoutine
        || entry.registers.x < 0x20U
        || entry.registers.x > 0x25U) {
        return std::nullopt;
    }
    const auto activation_offset =
        static_cast<std::size_t>(0x1980U + entry.registers.x);
    if (work_ram.size() <= activation_offset) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.accumulator = 0x01U;
    output.status = static_cast<std::uint8_t>(output.status & 0x7DU);
    output.program_counter = 0x4376U;

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x02U,
            .logical_pc = 0x4376U,
        },
        .output_registers = output,
        .body_master_cycles = 21U,
    };
    auto checkpoint = output;
    checkpoint.program_counter = 0x4373U;
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 3U,
        .master_cycle_offset = 6U,
        .instruction = {0x02U, 0x4371U},
        .registers = checkpoint,
    });
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 18U,
        .master_cycle_offset = 21U,
        .instruction = {0x02U, 0x4373U},
        .registers = output,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x4373U},
        .logical_address = static_cast<std::uint16_t>(
            0x3980U + entry.registers.x),
        .value = 0x01U,
        .master_cycle_offset = 21U,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_giant_door_hit_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x62B8U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_giant_door_hit_gate gate{};
    if (!dc_classify_giant_door_hit_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &gate)) {
        throw std::runtime_error(
            "Native 62B8 replacement rejected the work-RAM snapshot.");
    }
    if (gate.path != DC_GIANT_DOOR_HIT_GATE_IDLE) {
        return std::nullopt;
    }

    const auto set_nz =
        [](std::uint8_t status, const std::uint8_t value) {
            status = static_cast<std::uint8_t>(status & 0x7DU);
            if (value == 0U) {
                status = static_cast<std::uint8_t>(status | 0x02U);
            } else if ((value & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x80U);
            }
            return status;
        };

    auto output = entry.registers;
    output.program_counter = 0x62D3U;
    output.accumulator = gate.collision_state;
    output.status = set_nz(output.status, output.accumulator);

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x62D3U,
        },
        .output_registers = output,
        .body_master_cycles = 27U,
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
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    append_checkpoint(0x62B8U, 0x62BBU, 15U, output);
    append_checkpoint(0x62BBU, 0x62D3U, 27U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_giant_face_request_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x74D8U,
    };
    if (entry.routine != kRoutine || (entry.registers.status & 0x20U) != 0U) {
        return std::nullopt;
    }

    dc_giant_face_request_gate gate{};
    if (!dc_classify_giant_face_request_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &gate)) {
        throw std::runtime_error(
            "Native 74D8 replacement rejected the work-RAM snapshot.");
    }
    if (gate.path != DC_GIANT_FACE_REQUEST_GATE_IDLE) {
        return std::nullopt;
    }

    const auto set_nz =
        [](std::uint8_t status, const std::uint8_t value) {
            status = static_cast<std::uint8_t>(status & 0x7DU);
            if (value == 0U) {
                status = static_cast<std::uint8_t>(status | 0x02U);
            } else if ((value & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x80U);
            }
            return status;
        };

    auto checkpoint = entry.registers;
    checkpoint.accumulator = gate.request_flags;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);

    auto output = checkpoint;
    output.program_counter = 0x74DFU;
    output.accumulator = static_cast<std::uint8_t>(
        output.accumulator & 0x01U);
    output.status = set_nz(output.status, output.accumulator);

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x74DFU,
        },
        .output_registers = output,
        .body_master_cycles = 27U,
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
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    append_checkpoint(0x74D8U, 0x74DBU, 15U, checkpoint);
    append_checkpoint(0x74DBU, 0x74DDU, 21U, output);
    append_checkpoint(0x74DDU, 0x74DFU, 27U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_giant_face_animator_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x767EU,
    };
    if (entry.routine != kRoutine || (entry.registers.status & 0x20U) != 0U) {
        return std::nullopt;
    }

    dc_giant_face_animator_gate gate{};
    if (!dc_classify_giant_face_animator_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &gate)) {
        throw std::runtime_error(
            "Native 767E replacement rejected the work-RAM snapshot.");
    }
    if (gate.path != DC_GIANT_FACE_ANIMATOR_GATE_IDLE) {
        return std::nullopt;
    }

    const auto set_nz =
        [](std::uint8_t status, const std::uint8_t value) {
            status = static_cast<std::uint8_t>(status & 0x7DU);
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
            status = static_cast<std::uint8_t>(status & 0x7CU);
            status = set_nz(
                status,
                static_cast<std::uint8_t>(left - right));
            if (left >= right) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            return status;
        };

    auto checkpoint = entry.registers;
    checkpoint.accumulator = gate.animation_mode;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);

    auto output = checkpoint;
    output.program_counter = 0x7685U;
    output.status = compare_status(
        output.status,
        output.accumulator,
        0x01U);

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x7685U,
        },
        .output_registers = output,
        .body_master_cycles = 27U,
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
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    append_checkpoint(0x767EU, 0x7681U, 15U, checkpoint);
    append_checkpoint(0x7681U, 0x7683U, 21U, output);
    append_checkpoint(0x7683U, 0x7685U, 27U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_giant_face_crown_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x76FAU,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_giant_face_crown_gate gate{};
    if (!dc_classify_giant_face_crown_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &gate)) {
        throw std::runtime_error(
            "Native 76FA replacement rejected the work-RAM snapshot.");
    }
    if (gate.path != DC_GIANT_FACE_CROWN_GATE_IDLE) {
        return std::nullopt;
    }

    const auto set_nz =
        [](std::uint8_t status, const std::uint8_t value) {
            status = static_cast<std::uint8_t>(status & 0x7DU);
            if (value == 0U) {
                status = static_cast<std::uint8_t>(status | 0x02U);
            } else if ((value & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x80U);
            }
            return status;
        };

    auto output = entry.registers;
    output.program_counter = 0x7717U;
    output.accumulator = gate.collision_state;
    output.status = set_nz(output.status, output.accumulator);

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x7717U,
        },
        .output_registers = output,
        .body_master_cycles = 27U,
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
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    append_checkpoint(0x76FAU, 0x76FDU, 15U, output);
    append_checkpoint(0x76FDU, 0x7717U, 27U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_giant_face_mouth_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x7726U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_giant_face_mouth_gate gate{};
    if (!dc_classify_giant_face_mouth_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &gate)) {
        throw std::runtime_error(
            "Native 7726 replacement rejected the work-RAM snapshot.");
    }
    if (gate.path != DC_GIANT_FACE_MOUTH_GATE_IDLE) {
        return std::nullopt;
    }

    const auto set_nz =
        [](std::uint8_t status, const std::uint8_t value) {
            status = static_cast<std::uint8_t>(status & 0x7DU);
            if (value == 0U) {
                status = static_cast<std::uint8_t>(status | 0x02U);
            } else if ((value & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x80U);
            }
            return status;
        };

    auto mouth_state_registers = entry.registers;
    mouth_state_registers.accumulator = gate.mouth_state;
    mouth_state_registers.status = set_nz(
        mouth_state_registers.status, mouth_state_registers.accumulator);

    auto output = entry.registers;
    output.program_counter = 0x7748U;
    output.accumulator = gate.collision_state;
    output.status = set_nz(output.status, output.accumulator);

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x7748U,
        },
        .output_registers = output,
        .body_master_cycles = 48U,
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
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    append_checkpoint(0x7726U, 0x7729U, 15U, mouth_state_registers);
    append_checkpoint(0x7729U, 0x772BU, 21U, mouth_state_registers);
    append_checkpoint(0x772BU, 0x772EU, 36U, output);
    append_checkpoint(0x772EU, 0x7748U, 48U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_giant_face_helmet_controller(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x4C91U,
    };
    constexpr RoutineKey kWallPocketRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x4A93U,
    };
    if (entry.routine != kRoutine && entry.routine != kWallPocketRoutine) {
        return std::nullopt;
    }

    dc_giant_face_helmet_controller controller{};
    const auto classified = entry.routine == kWallPocketRoutine
        ? dc_classify_four_star_wall_pocket_controller(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &controller)
        : dc_classify_giant_face_helmet_controller(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &controller);
    if (!classified) {
        throw std::runtime_error(
            "Native four-tick controller replacement rejected the work-RAM snapshot.");
    }
    const auto set_nz =
        [](std::uint8_t status, const std::uint8_t value) {
            status = static_cast<std::uint8_t>(status & 0x7DU);
            if (value == 0U) {
                status = static_cast<std::uint8_t>(status | 0x02U);
            } else if ((value & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x80U);
            }
            return status;
        };
    const auto set_compare =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right) {
            status = set_nz(
                static_cast<std::uint8_t>(status & 0xFEU),
                static_cast<std::uint8_t>(left - right));
            if (left >= right) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            return status;
        };

    auto after_collision_load = entry.registers;
    after_collision_load.accumulator = controller.collision_state;
    after_collision_load.status = set_nz(
        after_collision_load.status,
        after_collision_load.accumulator);

    // The Wall-Pocket collision gate has a small, self-contained native
    // prefix before the access-specific score/scene logic.  Preserve its
    // LDA $3800,X / BMI $4A9B transcript, then leave the normal or special
    // access decision and its interrupt-bearing descendants in assembly.
    // Giant-Face helmet collisions still have no independently captured
    // continuation and therefore retain their prior assembly fallback.
    if (controller.path == DC_GIANT_FACE_HELMET_CONTROLLER_COLLISION) {
        if (entry.routine != kWallPocketRoutine) {
            return std::nullopt;
        }

        // The focused collision routes are the lower-left pocket's ordinary
        // and matching-arrow access decisions.  Their shared
        // selector/rotation gate is a self-contained, no-write prefix, so
        // move the native boundary through the first original normal or
        // special-path instruction.  The matching path also owns its first
        // self-contained latch clear and audio-command load; the audio helper
        // at $4ABB/$4ACF and every score/animation descendant after them stay
        // in assembly until they
        // have their own transcripts.
        constexpr std::size_t kRotationLowOffset = 0x0D00U;
        constexpr std::size_t kRotationHighOffset = 0x0D01U;
        constexpr std::size_t kDirectionOffset = 0x0D18U;
        constexpr std::size_t kSelectorOffset = 0x1780U;
        constexpr std::uint8_t kLowerLeftSlot = 0x3EU;
        constexpr std::uint8_t kLowerLeftSelector = 0x05U;
        const auto read = [&work_ram](const std::size_t offset) {
            return std::to_integer<std::uint8_t>(work_ram[offset]);
        };
        const auto selector_offset =
            kSelectorOffset + static_cast<std::size_t>(entry.registers.x);
        const auto lower_left_access_gate =
            entry.registers.x == kLowerLeftSlot
            && work_ram.size() > selector_offset
            && work_ram.size() > kDirectionOffset
            && read(selector_offset) == kLowerLeftSelector
            && read(kRotationLowOffset) == 0U
            && read(kRotationHighOffset) == 0U;
        if (!lower_left_access_gate) {
            RoutineReplacement replacement{
                .return_instruction = {
                    .physical_bank = 0x02U,
                    .logical_pc = 0x4A9BU,
                },
                .output_registers = after_collision_load,
                .body_master_cycles = 27U,
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
                        .instruction = {0x02U, instruction_pc},
                        .registers = registers,
                    });
                };
            append_checkpoint(0x4A93U, 0x4A96U, 15U, after_collision_load);
            append_checkpoint(0x4A96U, 0x4A9BU, 27U, after_collision_load);
            replacement.output_registers.program_counter = 0x4A9BU;
            return replacement;
        }

        const auto matching_lower_left_access =
            read(kDirectionOffset) == kLowerLeftSelector;
        const std::uint16_t access_decision_pc = matching_lower_left_access
            ? 0x4ACAU
            : 0x4ABBU;
        const std::uint16_t special_latch_resume_pc = 0x4ACDU;
        const std::uint16_t access_resume_pc = matching_lower_left_access
            ? 0x4ACFU
            : access_decision_pc;
        RoutineReplacement replacement{
            .return_instruction = {
                .physical_bank = 0x02U,
                .logical_pc = access_resume_pc,
            },
            .output_registers = after_collision_load,
            .body_master_cycles = matching_lower_left_access ? 171U : 144U,
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
                    .instruction = {0x02U, instruction_pc},
                    .registers = registers,
                });
        };
        append_checkpoint(0x4A93U, 0x4A96U, 15U, after_collision_load);
        append_checkpoint(0x4A96U, 0x4A9BU, 27U, after_collision_load);

        auto after_selector_load = after_collision_load;
        after_selector_load.accumulator = kLowerLeftSelector;
        after_selector_load.status = set_nz(
            after_selector_load.status,
            after_selector_load.accumulator);
        append_checkpoint(0x4A9BU, 0x4A9EU, 42U, after_selector_load);

        auto after_compare_three = after_selector_load;
        after_compare_three.status = set_compare(
            after_compare_three.status,
            after_compare_three.accumulator,
            0x03U);
        append_checkpoint(0x4A9EU, 0x4AA0U, 48U, after_compare_three);
        append_checkpoint(0x4AA0U, 0x4AA2U, 54U, after_compare_three);

        auto after_compare_five = after_compare_three;
        after_compare_five.status = set_compare(
            after_compare_five.status,
            after_compare_five.accumulator,
            kLowerLeftSelector);
        append_checkpoint(0x4AA2U, 0x4AA4U, 60U, after_compare_five);
        append_checkpoint(0x4AA4U, 0x4AABU, 72U, after_compare_five);

        auto after_rotation_low_load = after_compare_five;
        after_rotation_low_load.accumulator = 0U;
        after_rotation_low_load.status = set_nz(
            after_rotation_low_load.status,
            after_rotation_low_load.accumulator);
        append_checkpoint(
            0x4AABU,
            0x4AAEU,
            87U,
            after_rotation_low_load);

        auto after_rotation = after_rotation_low_load;
        after_rotation.status = set_nz(
            after_rotation.status,
            after_rotation.accumulator);
        append_checkpoint(0x4AAEU, 0x4AB1U, 102U, after_rotation);
        append_checkpoint(0x4AB1U, 0x4AB3U, 108U, after_rotation);

        auto after_direction_load = after_rotation;
        after_direction_load.accumulator = read(kDirectionOffset);
        after_direction_load.status = set_nz(
            after_direction_load.status,
            after_direction_load.accumulator);
        append_checkpoint(
            0x4AB3U,
            0x4AB6U,
            123U,
            after_direction_load);

        auto after_selector_compare = after_direction_load;
        after_selector_compare.status = set_compare(
            after_selector_compare.status,
            after_selector_compare.accumulator,
            kLowerLeftSelector);
        append_checkpoint(
            0x4AB6U,
            0x4AB9U,
            138U,
            after_selector_compare);
        append_checkpoint(
            0x4AB9U,
            access_decision_pc,
            matching_lower_left_access ? 150U : 144U,
            after_selector_compare);
        if (matching_lower_left_access) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x02U, 0x4ACAU},
                .logical_address = 0x2CFAU,
                .value = 0x00U,
                .master_cycle_offset = 165U,
            });
            append_checkpoint(
                0x4ACAU,
                special_latch_resume_pc,
                165U,
                after_selector_compare);
            auto after_audio_command_load = after_selector_compare;
            after_audio_command_load.accumulator = 0x41U;
            after_audio_command_load.status = set_nz(
                after_audio_command_load.status,
                after_audio_command_load.accumulator);
            append_checkpoint(
                0x4ACDU,
                access_resume_pc,
                171U,
                after_audio_command_load);
            replacement.output_registers = after_audio_command_load;
        } else {
            replacement.output_registers = after_selector_compare;
        }
        replacement.output_registers.program_counter = access_resume_pc;
        return replacement;
    }

    auto after_decrement = after_collision_load;
    after_decrement.status = set_nz(
        after_decrement.status,
        controller.decremented_timer);

    auto output = after_decrement;
    std::uint32_t body_master_cycles = 66U;
    if (controller.path != DC_GIANT_FACE_HELMET_CONTROLLER_WAITING) {
        output.accumulator = controller.advanced_appearance;
        output.status = set_compare(
            output.status,
            output.accumulator,
            0x03U);
        body_master_cycles =
            controller.path == DC_GIANT_FACE_HELMET_CONTROLLER_ADVANCE
            ? 135U
            : 144U;
    }
    output.program_counter = 0x4CF1U;

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x02U,
            .logical_pc = 0x4CF1U,
        },
        .output_registers = output,
        .body_master_cycles = body_master_cycles,
    };
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x4CDAU},
        .logical_address = static_cast<std::uint16_t>(
            0x3A00U + controller.slot),
        .value = controller.decremented_timer,
        .master_cycle_offset = 54U,
    });
    if (controller.path != DC_GIANT_FACE_HELMET_CONTROLLER_WAITING) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x02U, 0x4CE1U},
            .logical_address = static_cast<std::uint16_t>(
                0x3A00U + controller.slot),
            .value = 0x04U,
            .master_cycle_offset = 81U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x02U, 0x4CE4U},
            .logical_address = static_cast<std::uint16_t>(
                0x3980U + controller.slot),
            .value = controller.advanced_appearance,
            .master_cycle_offset = 102U,
        });
    }
    if (controller.path == DC_GIANT_FACE_HELMET_CONTROLLER_RESET) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x02U, 0x4CEEU},
            .logical_address = static_cast<std::uint16_t>(
                0x3980U + controller.slot),
            .value = 0x00U,
            .master_cycle_offset = 144U,
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
                .instruction = {0x02U, instruction_pc},
                .registers = registers,
            });
        };

    const auto entry_pc = entry.routine.logical_pc;
    append_checkpoint(entry_pc, static_cast<std::uint16_t>(entry_pc + 3U), 15U, after_collision_load);
    append_checkpoint(static_cast<std::uint16_t>(entry_pc + 3U), static_cast<std::uint16_t>(entry_pc + 5U), 21U, after_collision_load);
    append_checkpoint(static_cast<std::uint16_t>(entry_pc + 5U), 0x4CDAU, 33U, after_collision_load);
    append_checkpoint(0x4CDAU, 0x4CDDU, 54U, after_decrement);
    if (controller.path == DC_GIANT_FACE_HELMET_CONTROLLER_WAITING) {
        append_checkpoint(0x4CDDU, 0x4CF1U, 66U, output);
        return replacement;
    }

    append_checkpoint(0x4CDDU, 0x4CDFU, 60U, after_decrement);
    auto after_timer_load = after_decrement;
    after_timer_load.accumulator = 0x04U;
    after_timer_load.status = set_nz(after_timer_load.status, 0x04U);
    append_checkpoint(0x4CDFU, 0x4CE1U, 66U, after_timer_load);
    append_checkpoint(0x4CE1U, 0x4CE4U, 81U, after_timer_load);

    auto after_appearance_increment = after_timer_load;
    after_appearance_increment.status = set_nz(
        after_appearance_increment.status,
        controller.advanced_appearance);
    append_checkpoint(
        0x4CE4U,
        0x4CE7U,
        102U,
        after_appearance_increment);

    auto after_appearance_load = after_appearance_increment;
    after_appearance_load.accumulator = controller.advanced_appearance;
    after_appearance_load.status = set_nz(
        after_appearance_load.status,
        after_appearance_load.accumulator);
    append_checkpoint(
        0x4CE7U,
        0x4CEAU,
        117U,
        after_appearance_load);

    auto after_compare = after_appearance_load;
    after_compare.status = set_compare(
        after_compare.status,
        after_compare.accumulator,
        0x03U);
    append_checkpoint(0x4CEAU, 0x4CECU, 123U, after_compare);
    if (controller.path == DC_GIANT_FACE_HELMET_CONTROLLER_ADVANCE) {
        append_checkpoint(0x4CECU, 0x4CF1U, 135U, output);
        return replacement;
    }

    append_checkpoint(0x4CECU, 0x4CEEU, 129U, after_compare);
    append_checkpoint(0x4CEEU, 0x4CF1U, 144U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_four_star_wall_pocket_post_audio_score(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The matching-arrow path has returned from its shared C087 audio thunk.
    // Award the fixed 100 units and start the four-tick opening animation;
    // the short appearance helper at $4AE0 stays assembly-owned and returns
    // to the separately native $4AE5 continuation.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x4AD2U,
    };
    constexpr std::uint8_t kLowerLeftSlot = 0x3EU;
    constexpr std::size_t kTimerOffset = 0x1A3EU;
    if (entry.routine != kRoutine || entry.registers.x != kLowerLeftSlot
        || (entry.registers.status & 0x28U) != 0U
        || work_ram.size() <= kTimerOffset) {
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
    RoutineReplacement replacement{};
    const auto append_checkpoint =
        [&replacement](
            const std::uint8_t physical_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {physical_bank, instruction_pc},
                .registers = registers,
            });
        };
    const auto add_write =
        [&replacement](
            const std::uint8_t physical_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t logical_address,
            const std::uint8_t value,
            const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {physical_bank, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
        };

    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    auto checkpoint = entry.registers;
    checkpoint.accumulator = 0x64U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x02U, 0x4AD2U, 0x4AD4U, 6U, checkpoint);

    add_write(0x02U, 0x4AD4U, 0x202FU, 0x64U, 18U);
    staged[0x002FU] = static_cast<std::byte>(0x64U);
    append_checkpoint(0x02U, 0x4AD4U, 0x4AD6U, 18U, checkpoint);

    add_write(0x02U, 0x4AD6U, 0x2030U, 0x00U, 30U);
    staged[0x0030U] = static_cast<std::byte>(0x00U);
    append_checkpoint(0x02U, 0x4AD6U, 0x4AD8U, 30U, checkpoint);

    const auto score_stack_high = checkpoint.stack_pointer;
    const auto score_stack_low = static_cast<std::uint8_t>(
        score_stack_high - 1U);
    if (staged.size() <= 0x0100U + score_stack_high
        || staged.size() <= 0x0100U + score_stack_low) {
        return std::nullopt;
    }
    add_write(
        0x02U,
        0x4AD8U,
        static_cast<std::uint16_t>(0x2100U + score_stack_high),
        0x4AU,
        30U);
    add_write(
        0x02U,
        0x4AD8U,
        static_cast<std::uint16_t>(0x2100U + score_stack_low),
        0xDAU,
        30U);
    staged[0x0100U + score_stack_high] = static_cast<std::byte>(0x4AU);
    staged[0x0100U + score_stack_low] = static_cast<std::byte>(0xDAU);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 2U);
    append_checkpoint(0x02U, 0x4AD8U, 0xC02AU, 51U, checkpoint);
    append_checkpoint(0x01U, 0xC02AU, 0xF54FU, 63U, checkpoint);

    checkpoint.y = std::to_integer<std::uint8_t>(staged[0x0D38U]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0x00U, 0xF54FU, 0xF552U, 78U, checkpoint);
    checkpoint.y = static_cast<std::uint8_t>(checkpoint.y + 1U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0x00U, 0xF552U, 0xF553U, 84U, checkpoint);

    add_write(
        0x00U,
        0xF553U,
        static_cast<std::uint16_t>(0x2100U + checkpoint.stack_pointer),
        checkpoint.y,
        93U);
    staged[0x0100U + checkpoint.stack_pointer] =
        static_cast<std::byte>(checkpoint.y);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 1U);
    append_checkpoint(0x00U, 0xF553U, 0xF554U, 93U, checkpoint);

    add_write(
        0x00U,
        0xF554U,
        static_cast<std::uint16_t>(0x2100U + checkpoint.stack_pointer),
        0xF5U,
        93U);
    add_write(
        0x00U,
        0xF554U,
        static_cast<std::uint16_t>(
            0x2100U + static_cast<std::uint8_t>(checkpoint.stack_pointer - 1U)),
        0x56U,
        93U);
    staged[0x0100U + checkpoint.stack_pointer] = static_cast<std::byte>(0xF5U);
    staged[0x0100U + static_cast<std::uint8_t>(checkpoint.stack_pointer - 1U)] =
        static_cast<std::byte>(0x56U);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 2U);
    append_checkpoint(0x00U, 0xF554U, 0xF55CU, 114U, checkpoint);

    RoutineEntry score_entry{
        .routine = {.physical_bank = 0x00U, .logical_pc = 0xF55CU},
        .registers = checkpoint,
    };
    auto score = replace_score_accumulator(score_entry, staged);
    if (!score || score->checkpoints.empty()) {
        return std::nullopt;
    }
    constexpr std::uint32_t kScoreOffset = 114U;
    for (auto write : score->work_ram_writes) {
        write.master_cycle_offset += kScoreOffset;
        replacement.work_ram_writes.push_back(write);
    }
    for (auto child_checkpoint : score->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset += kScoreOffset;
        child_checkpoint.master_cycle_offset += kScoreOffset;
        replacement.checkpoints.push_back(child_checkpoint);
    }

    checkpoint = score->output_registers;
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 2U);
    append_checkpoint(0x00U, 0xF5A8U, 0xF557U, 690U, checkpoint);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 1U);
    checkpoint.y = 0x01U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0x00U, 0xF557U, 0xF558U, 702U, checkpoint);
    checkpoint.y = 0x00U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0x00U, 0xF558U, 0xF559U, 708U, checkpoint);
    append_checkpoint(0x00U, 0xF559U, 0xF55BU, 714U, checkpoint);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 2U);
    append_checkpoint(0x00U, 0xF55BU, 0x4ADBU, 732U, checkpoint);

    checkpoint.accumulator = 0x04U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x02U, 0x4ADBU, 0x4ADDU, 738U, checkpoint);
    add_write(0x02U, 0x4ADDU, 0x3A3EU, 0x04U, 759U);
    staged[kTimerOffset] = static_cast<std::byte>(0x04U);
    append_checkpoint(0x02U, 0x4ADDU, 0x4AE0U, 759U, checkpoint);

    replacement.return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4AE0U};
    checkpoint.program_counter = 0x4AE0U;
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = 759U;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_four_star_wall_pocket_post_appearance(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The assembly-owned $406F appearance helper leaves A=$04 and resumes
    // here.  Store that phase and compose the shared scheduler, which records
    // the following $4AEB dispatch in the actor's live callback.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x4AE5U,
    };
    constexpr std::uint8_t kLowerLeftSlot = 0x3EU;
    constexpr std::size_t kAppearanceOffset = 0x1980U;
    if (entry.routine != kRoutine || entry.registers.x != kLowerLeftSlot
        || entry.registers.accumulator != 0x04U
        || (entry.registers.status & 0x28U) != 0U
        || work_ram.size() <= kAppearanceOffset) {
        return std::nullopt;
    }

    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    auto checkpoint = entry.registers;
    RoutineReplacement replacement{};
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x4AE5U},
        .logical_address = 0x3980U,
        .value = 0x04U,
        .master_cycle_offset = 15U,
    });
    staged[kAppearanceOffset] = static_cast<std::byte>(0x04U);
    const auto append_checkpoint =
        [&replacement](
            const std::uint8_t physical_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {physical_bank, instruction_pc},
                .registers = registers,
            });
        };
    append_checkpoint(0x02U, 0x4AE5U, 0x4AE8U, 15U, checkpoint);

    const auto stack_high = checkpoint.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
    if (staged.size() <= 0x0100U + stack_high
        || staged.size() <= 0x0100U + stack_low) {
        return std::nullopt;
    }
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x4AE8U},
        .logical_address = static_cast<std::uint16_t>(0x2100U + stack_high),
        .value = 0x4AU,
        .master_cycle_offset = 15U,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x4AE8U},
        .logical_address = static_cast<std::uint16_t>(0x2100U + stack_low),
        .value = 0xEAU,
        .master_cycle_offset = 15U,
    });
    staged[0x0100U + stack_high] = static_cast<std::byte>(0x4AU);
    staged[0x0100U + stack_low] = static_cast<std::byte>(0xEAU);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 2U);
    append_checkpoint(0x02U, 0x4AE8U, 0xC018U, 36U, checkpoint);
    append_checkpoint(0x01U, 0xC018U, 0xC5C9U, 48U, checkpoint);

    RoutineEntry scheduler_entry{
        .routine = {.physical_bank = 0x01U, .logical_pc = 0xC5C9U},
        .registers = checkpoint,
    };
    auto scheduler = replace_coroutine_scheduler(scheduler_entry, staged);
    if (!scheduler || scheduler->checkpoints.empty()) {
        return std::nullopt;
    }
    constexpr std::uint32_t kSchedulerOffset = 48U;
    for (auto write : scheduler->work_ram_writes) {
        write.master_cycle_offset += kSchedulerOffset;
        replacement.work_ram_writes.push_back(write);
    }
    for (auto child_checkpoint : scheduler->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset += kSchedulerOffset;
        child_checkpoint.master_cycle_offset += kSchedulerOffset;
        replacement.checkpoints.push_back(child_checkpoint);
    }
    replacement.return_instruction = scheduler->return_instruction;
    replacement.output_registers = scheduler->output_registers;
    replacement.body_master_cycles = kSchedulerOffset + scheduler->body_master_cycles;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_upper_wall_pocket_hand_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x5972U,
    };
    if (entry.routine != kRoutine || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_upper_wall_pocket_hand_initializer initializer{};
    if (!dc_prepare_upper_wall_pocket_hand_initializer(
            entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    // C018 transfers directly to the shared C5C9 coroutine scheduler. The
    // JSR return address is saved as the next actor dispatch (5975), then the
    // scheduler returns to the original actor caller rather than continuing
    // through the delayed state body.
    RoutineReplacement replacement{};
    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    auto checkpoint = entry.registers;
    const auto coroutine_return = static_cast<std::uint16_t>(
        initializer.live_dispatch - 1U);
    const auto return_high = static_cast<std::uint8_t>(coroutine_return >> 8U);
    const auto return_low = static_cast<std::uint8_t>(coroutine_return);
    const auto stack_high = checkpoint.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
    if (staged.size() <= 0x0100U + stack_high
        || staged.size() <= 0x0100U + stack_low) {
        return std::nullopt;
    }
    replacement.work_ram_writes = {
        {
            .instruction = {0x02U, 0x5972U},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_high),
            .value = return_high,
            .master_cycle_offset = 0U,
        },
        {
            .instruction = {0x02U, 0x5972U},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_low),
            .value = return_low,
            .master_cycle_offset = 0U,
        },
    };
    staged[0x0100U + stack_high] = static_cast<std::byte>(return_high);
    staged[0x0100U + stack_low] = static_cast<std::byte>(return_low);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);

    const auto append_checkpoint =
        [&replacement](
            const std::uint8_t physical_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {physical_bank, instruction_pc},
                .registers = registers,
            });
        };
    append_checkpoint(0x02U, 0x5972U, 0xC018U, 21U, checkpoint);
    append_checkpoint(0x01U, 0xC018U, 0xC5C9U, 33U, checkpoint);

    RoutineEntry scheduler_entry{
        .routine = {
            .physical_bank = 0x01U,
            .logical_pc = 0xC5C9U,
        },
        .registers = checkpoint,
    };
    auto scheduler = replace_coroutine_scheduler(scheduler_entry, staged);
    if (!scheduler || scheduler->checkpoints.empty()) {
        return std::nullopt;
    }

    constexpr std::uint32_t kSchedulerOffset = 33U;
    for (auto write : scheduler->work_ram_writes) {
        write.master_cycle_offset += kSchedulerOffset;
        replacement.work_ram_writes.push_back(write);
    }
    for (auto child_checkpoint : scheduler->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset +=
            kSchedulerOffset;
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
replace_upper_wall_pocket_hand_live_setup(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x5975U,
    };
    if (entry.routine != kRoutine || (entry.registers.status & 0x28U) != 0U) {
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
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if (((~(left ^ right) & (left ^ result)) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x40U);
            }
            return status;
        };
    const auto append_checkpoint =
        [](RoutineReplacement& replacement,
           const std::uint8_t physical_bank,
           const std::uint16_t instruction_pc,
           const std::uint16_t resume_pc,
           const std::uint32_t master_cycle,
           auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {physical_bank, instruction_pc},
                .registers = registers,
            });
        };
    const auto stage_write =
        [](std::vector<std::byte>& staged,
           const std::uint16_t logical_address,
           const std::uint8_t value) {
            const auto offset = static_cast<std::size_t>(
                logical_address & 0x1FFFU);
            if (offset >= staged.size()) {
                return false;
            }
            staged[offset] = static_cast<std::byte>(value);
            return true;
        };

    RoutineReplacement replacement{};
    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    auto checkpoint = entry.registers;
    const auto collision_flags_offset = static_cast<std::uint16_t>(
        0x3080U + entry.registers.x);
    if ((collision_flags_offset & 0x1FFFU) >= staged.size()) {
        return std::nullopt;
    }
    const auto collision_flags = std::to_integer<std::uint8_t>(
        staged[collision_flags_offset & 0x1FFFU]);
    checkpoint.accumulator = collision_flags;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x5975U, 0x5978U, 15U, checkpoint);

    checkpoint.accumulator = static_cast<std::uint8_t>(
        checkpoint.accumulator | 0x40U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x5978U, 0x597AU, 21U, checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x597AU},
        .logical_address = collision_flags_offset,
        .value = checkpoint.accumulator,
        .master_cycle_offset = 36U,
    });
    if (!stage_write(staged, collision_flags_offset, checkpoint.accumulator)) {
        return std::nullopt;
    }
    append_checkpoint(replacement, 0x02U, 0x597AU, 0x597DU, 36U, checkpoint);

    const auto rng_stack_high = checkpoint.stack_pointer;
    const auto rng_stack_low = static_cast<std::uint8_t>(rng_stack_high - 1U);
    if (!stage_write(staged, static_cast<std::uint16_t>(0x2100U + rng_stack_high), 0x59U)
        || !stage_write(staged, static_cast<std::uint16_t>(0x2100U + rng_stack_low), 0x7FU)) {
        return std::nullopt;
    }
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x597DU},
        .logical_address = static_cast<std::uint16_t>(0x2100U + rng_stack_high),
        .value = 0x59U,
        .master_cycle_offset = 36U,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x597DU},
        .logical_address = static_cast<std::uint16_t>(0x2100U + rng_stack_low),
        .value = 0x7FU,
        .master_cycle_offset = 36U,
    });
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 2U);
    append_checkpoint(replacement, 0x02U, 0x597DU, 0xC081U, 57U, checkpoint);
    append_checkpoint(replacement, 0x00U, 0xC081U, 0xE8B3U, 69U, checkpoint);

    RoutineEntry rng_entry{
        .routine = {
            .physical_bank = 0x00U,
            .logical_pc = 0xE8B3U,
        },
        .registers = checkpoint,
    };
    auto rng = replace_rng_update(rng_entry, staged);
    if (!rng || rng->checkpoints.empty()) {
        return std::nullopt;
    }
    for (auto write : rng->work_ram_writes) {
        write.master_cycle_offset += 69U;
        replacement.work_ram_writes.push_back(write);
        if (!stage_write(staged, write.logical_address, write.value)) {
            return std::nullopt;
        }
    }
    for (auto child_checkpoint : rng->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset += 69U;
        child_checkpoint.master_cycle_offset += 69U;
        replacement.checkpoints.push_back(child_checkpoint);
    }

    dc_upper_wall_pocket_hand_live_setup setup{};
    if (!dc_prepare_upper_wall_pocket_hand_live_setup(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            rng->output_registers.accumulator,
            &setup)) {
        return std::nullopt;
    }

    checkpoint = rng->output_registers;
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 2U);
    append_checkpoint(replacement, 0x00U, 0xE8E6U, 0x5980U, 369U, checkpoint);

    checkpoint.accumulator = setup.masked_rng;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x5980U, 0x5982U, 375U, checkpoint);
    checkpoint.status = static_cast<std::uint8_t>(checkpoint.status & ~0x01U);
    append_checkpoint(replacement, 0x02U, 0x5982U, 0x5983U, 384U, checkpoint);
    // The one routed timer IRQ becomes pending at the end of CLC. Sampling
    // this boundary at its instruction-complete cycle preserves the original
    // $5983 return address and lets the handler re-enter through ADC.
    replacement.checkpoints.back().interrupt_sample_master_cycle_offset = 381U;
    checkpoint.accumulator = setup.timer_low;
    checkpoint.status = adc_status(
        checkpoint.status,
        setup.masked_rng,
        setup.timer_base,
        checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x5983U, 0x5986U, 402U, checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x5986U},
        .logical_address = static_cast<std::uint16_t>(0x3B00U + setup.slot),
        .value = setup.timer_low,
        .master_cycle_offset = 417U,
    });
    if (!stage_write(
            staged,
            static_cast<std::uint16_t>(0x3B00U + setup.slot),
            setup.timer_low)) {
        return std::nullopt;
    }
    append_checkpoint(replacement, 0x02U, 0x5986U, 0x5989U, 417U, checkpoint);
    checkpoint.accumulator = setup.timer_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x5989U, 0x598BU, 423U, checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x598BU},
        .logical_address = static_cast<std::uint16_t>(0x3B80U + setup.slot),
        .value = setup.timer_high,
        .master_cycle_offset = 438U,
    });
    if (!stage_write(
            staged,
            static_cast<std::uint16_t>(0x3B80U + setup.slot),
            setup.timer_high)) {
        return std::nullopt;
    }
    append_checkpoint(replacement, 0x02U, 0x598BU, 0x598EU, 438U, checkpoint);

    const auto scheduler_stack_high = checkpoint.stack_pointer;
    const auto scheduler_stack_low = static_cast<std::uint8_t>(
        scheduler_stack_high - 1U);
    const auto scheduler_return = static_cast<std::uint16_t>(
        setup.countdown_dispatch - 1U);
    if (!stage_write(
            staged,
            static_cast<std::uint16_t>(0x2100U + scheduler_stack_high),
            static_cast<std::uint8_t>(scheduler_return >> 8U))
        || !stage_write(
            staged,
            static_cast<std::uint16_t>(0x2100U + scheduler_stack_low),
            static_cast<std::uint8_t>(scheduler_return))) {
        return std::nullopt;
    }
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x598EU},
        .logical_address = static_cast<std::uint16_t>(0x2100U + scheduler_stack_high),
        .value = static_cast<std::uint8_t>(scheduler_return >> 8U),
        .master_cycle_offset = 438U,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x598EU},
        .logical_address = static_cast<std::uint16_t>(0x2100U + scheduler_stack_low),
        .value = static_cast<std::uint8_t>(scheduler_return),
        .master_cycle_offset = 438U,
    });
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 2U);
    append_checkpoint(replacement, 0x02U, 0x598EU, 0xC018U, 459U, checkpoint);
    append_checkpoint(replacement, 0x01U, 0xC018U, 0xC5C9U, 471U, checkpoint);

    RoutineEntry scheduler_entry{
        .routine = {
            .physical_bank = 0x01U,
            .logical_pc = 0xC5C9U,
        },
        .registers = checkpoint,
    };
    auto scheduler = replace_coroutine_scheduler(scheduler_entry, staged);
    if (!scheduler || scheduler->checkpoints.empty()) {
        return std::nullopt;
    }
    for (auto write : scheduler->work_ram_writes) {
        write.master_cycle_offset += 471U;
        replacement.work_ram_writes.push_back(write);
    }
    for (auto child_checkpoint : scheduler->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset += 471U;
        child_checkpoint.master_cycle_offset += 471U;
        replacement.checkpoints.push_back(child_checkpoint);
    }
    replacement.return_instruction = scheduler->return_instruction;
    replacement.output_registers = scheduler->output_registers;
    replacement.body_master_cycles = 471U + scheduler->body_master_cycles;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_upper_wall_pocket_hand_opening(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x59A5U,
    };
    constexpr std::size_t kSceneLatchOffset = 0x0D33U;
    constexpr std::size_t kCollisionOffset = 0x10C0U;
    constexpr std::size_t kActorStateOffset = 0x1840U;
    constexpr std::size_t kDelayOffset = 0x1DC0U;
    constexpr std::size_t kContinuationLowOffset = 0x1F40U;
    constexpr std::size_t kContinuationHighOffset = 0x1FC0U;
    if (entry.routine != kRoutine || entry.registers.x != 0x40U
        || (entry.registers.status & 0x28U) != 0U
        || work_ram.size() <= kContinuationHighOffset) {
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
    const auto append_checkpoint =
        [](RoutineReplacement& replacement,
           const std::uint8_t physical_bank,
           const std::uint16_t instruction_pc,
           const std::uint16_t resume_pc,
           const std::uint32_t master_cycle,
           auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {physical_bank, instruction_pc},
                .registers = registers,
            });
        };
    const auto add_write =
        [](RoutineReplacement& replacement,
           const std::uint8_t physical_bank,
           const std::uint16_t instruction_pc,
           const std::uint16_t logical_address,
           const std::uint8_t value,
           const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {physical_bank, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
        };

    const auto scene_latch =
        std::to_integer<std::uint8_t>(work_ram[kSceneLatchOffset]);
    // A nonzero shared region latch leaves the upper hand dormant.  The ROM
    // performs only LDA $2D33 / BNE $59F4, so preserve this frequent branch
    // as an exact 27-cycle, no-write transcript.
    if (scene_latch != 0U) {
        RoutineReplacement replacement{};
        auto checkpoint = entry.registers;
        checkpoint.accumulator = scene_latch;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x02U, 0x59A5U, 0x59A8U, 15U, checkpoint);
        append_checkpoint(replacement, 0x02U, 0x59A8U, 0x59F4U, 27U, checkpoint);
        replacement.return_instruction = {0x02U, 0x59F4U};
        checkpoint.program_counter = 0x59F4U;
        replacement.output_registers = checkpoint;
        replacement.body_master_cycles = 27U;
        return replacement;
    }

    RoutineReplacement replacement{};
    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    auto checkpoint = entry.registers;
    checkpoint.accumulator = 0U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x59A5U, 0x59A8U, 15U, checkpoint);
    append_checkpoint(replacement, 0x02U, 0x59A8U, 0x59AAU, 21U, checkpoint);

    checkpoint.accumulator =
        std::to_integer<std::uint8_t>(work_ram[kCollisionOffset]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x59AAU, 0x59ADU, 36U, checkpoint);
    checkpoint.accumulator = static_cast<std::uint8_t>(
        checkpoint.accumulator & 0xBFU);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x59ADU, 0x59AFU, 42U, checkpoint);
    add_write(replacement, 0x02U, 0x59AFU, 0x30C0U, checkpoint.accumulator, 57U);
    staged[kCollisionOffset] = static_cast<std::byte>(checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x59AFU, 0x59B2U, 57U, checkpoint);

    checkpoint.accumulator =
        std::to_integer<std::uint8_t>(work_ram[kActorStateOffset]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x59B2U, 0x59B5U, 72U, checkpoint);
    checkpoint.accumulator = static_cast<std::uint8_t>(
        checkpoint.accumulator & 0x7FU);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x59B5U, 0x59B7U, 78U, checkpoint);
    add_write(replacement, 0x02U, 0x59B7U, 0x3840U, checkpoint.accumulator, 93U);
    staged[kActorStateOffset] = static_cast<std::byte>(checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x59B7U, 0x59BAU, 93U, checkpoint);

    checkpoint.accumulator = 0x83U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x59BAU, 0x59BCU, 99U, checkpoint);
    add_write(replacement, 0x02U, 0x59BCU, 0x3040U, checkpoint.accumulator, 114U);
    append_checkpoint(replacement, 0x02U, 0x59BCU, 0x59BFU, 114U, checkpoint);
    checkpoint.accumulator = 0x04U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x59BFU, 0x59C1U, 120U, checkpoint);

    const auto delay_stack_high = checkpoint.stack_pointer;
    const auto delay_stack_low = static_cast<std::uint8_t>(delay_stack_high - 1U);
    add_write(replacement, 0x02U, 0x59C1U,
        static_cast<std::uint16_t>(0x2100U + delay_stack_high), 0x59U, 120U);
    add_write(replacement, 0x02U, 0x59C1U,
        static_cast<std::uint16_t>(0x2100U + delay_stack_low), 0xC3U, 120U);
    staged[0x0100U + delay_stack_high] = static_cast<std::byte>(0x59U);
    staged[0x0100U + delay_stack_low] = static_cast<std::byte>(0xC3U);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(replacement, 0x02U, 0x59C1U, 0xC021U, 141U, checkpoint);
    append_checkpoint(replacement, 0x01U, 0xC021U, 0xC5EAU, 153U, checkpoint);

    add_write(replacement, 0x01U, 0xC5EAU, 0x3DC0U, checkpoint.accumulator, 168U);
    staged[kDelayOffset] = static_cast<std::byte>(checkpoint.accumulator);
    append_checkpoint(replacement, 0x01U, 0xC5EAU, 0xC5EDU, 168U, checkpoint);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer + 1U);
    checkpoint.accumulator = std::to_integer<std::uint8_t>(
        staged[0x0100U + checkpoint.stack_pointer]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x01U, 0xC5EDU, 0xC5EEU, 180U, checkpoint);
    add_write(replacement, 0x01U, 0xC5EEU, 0x202FU, checkpoint.accumulator, 192U);
    staged[0x002FU] = static_cast<std::byte>(checkpoint.accumulator);
    append_checkpoint(replacement, 0x01U, 0xC5EEU, 0xC5F0U, 192U, checkpoint);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer + 1U);
    checkpoint.accumulator = std::to_integer<std::uint8_t>(
        staged[0x0100U + checkpoint.stack_pointer]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x01U, 0xC5F0U, 0xC5F1U, 204U, checkpoint);
    add_write(replacement, 0x01U, 0xC5F1U, 0x2030U, checkpoint.accumulator, 216U);
    staged[0x0030U] = static_cast<std::byte>(checkpoint.accumulator);
    append_checkpoint(replacement, 0x01U, 0xC5F1U, 0xC5F3U, 216U, checkpoint);
    const auto continuation_low = static_cast<std::uint8_t>(
        std::to_integer<std::uint8_t>(staged[0x002FU]) + 1U);
    checkpoint.status = set_nz(checkpoint.status, continuation_low);
    add_write(replacement, 0x01U, 0xC5F3U, 0x202FU, continuation_low, 234U);
    staged[0x002FU] = static_cast<std::byte>(continuation_low);
    append_checkpoint(replacement, 0x01U, 0xC5F3U, 0xC5F5U, 234U, checkpoint);
    append_checkpoint(replacement, 0x01U, 0xC5F5U, 0xC5F9U, 246U, checkpoint);
    checkpoint.accumulator = continuation_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x01U, 0xC5F9U, 0xC5FBU, 255U, checkpoint);
    add_write(replacement, 0x01U, 0xC5FBU, 0x3F40U, checkpoint.accumulator, 273U);
    staged[kContinuationLowOffset] = static_cast<std::byte>(checkpoint.accumulator);
    append_checkpoint(replacement, 0x01U, 0xC5FBU, 0xC5FEU, 273U, checkpoint);
    checkpoint.accumulator = std::to_integer<std::uint8_t>(staged[0x0030U]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x01U, 0xC5FEU, 0xC600U, 282U, checkpoint);
    add_write(replacement, 0x01U, 0xC600U, 0x3FC0U, checkpoint.accumulator, 300U);
    staged[kContinuationHighOffset] = static_cast<std::byte>(checkpoint.accumulator);
    append_checkpoint(replacement, 0x01U, 0xC600U, 0xC603U, 300U, checkpoint);
    append_checkpoint(replacement, 0x01U, 0xC603U, 0x59C4U, 321U, checkpoint);

    const auto scheduler_stack_high = checkpoint.stack_pointer;
    const auto scheduler_stack_low = static_cast<std::uint8_t>(scheduler_stack_high - 1U);
    add_write(replacement, 0x02U, 0x59C4U,
        static_cast<std::uint16_t>(0x2100U + scheduler_stack_high), 0x59U, 321U);
    add_write(replacement, 0x02U, 0x59C4U,
        static_cast<std::uint16_t>(0x2100U + scheduler_stack_low), 0xC6U, 321U);
    staged[0x0100U + scheduler_stack_high] = static_cast<std::byte>(0x59U);
    staged[0x0100U + scheduler_stack_low] = static_cast<std::byte>(0xC6U);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(replacement, 0x02U, 0x59C4U, 0xC018U, 342U, checkpoint);
    append_checkpoint(replacement, 0x01U, 0xC018U, 0xC5C9U, 354U, checkpoint);

    RoutineEntry scheduler_entry{
        .routine = {.physical_bank = 0x01U, .logical_pc = 0xC5C9U},
        .registers = checkpoint,
    };
    auto scheduler = replace_coroutine_scheduler(scheduler_entry, staged);
    if (!scheduler || scheduler->checkpoints.empty()) {
        return std::nullopt;
    }
    for (auto write : scheduler->work_ram_writes) {
        write.master_cycle_offset += 354U;
        replacement.work_ram_writes.push_back(write);
    }
    for (auto child_checkpoint : scheduler->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset += 354U;
        child_checkpoint.master_cycle_offset += 354U;
        replacement.checkpoints.push_back(child_checkpoint);
    }
    replacement.return_instruction = scheduler->return_instruction;
    replacement.output_registers = scheduler->output_registers;
    replacement.body_master_cycles = 354U + scheduler->body_master_cycles;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_upper_wall_pocket_hand_opening_followup(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x59C7U,
    };
    constexpr std::size_t kActorStateOffset = 0x1840U;
    constexpr std::size_t kTimerOffset = 0x1A40U;
    if (entry.routine != kRoutine || entry.registers.x != 0x40U
        || (entry.registers.status & 0x28U) != 0U
        || work_ram.size() <= kTimerOffset) {
        return std::nullopt;
    }

    const auto actor_state =
        std::to_integer<std::uint8_t>(work_ram[kActorStateOffset]);
    // The negative branch clears the close-state flag, then returns through
    // the opening loop at $59BF to schedule a four-tick delay before this
    // entry is dispatched again.  Its scheduler may receive a timer IRQ, so
    // compose the existing instruction-checkpointed scheduler rather than
    // shortcutting the transfer.
    if ((actor_state & 0x80U) != 0U) {
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
            [](RoutineReplacement& replacement,
               const std::uint8_t physical_bank,
               const std::uint16_t instruction_pc,
               const std::uint16_t resume_pc,
               const std::uint32_t master_cycle,
               auto registers) {
                registers.program_counter = resume_pc;
                replacement.checkpoints.push_back({
                    .interrupt_sample_master_cycle_offset = master_cycle - 3U,
                    .master_cycle_offset = master_cycle,
                    .instruction = {physical_bank, instruction_pc},
                    .registers = registers,
                });
            };
        const auto add_write =
            [](RoutineReplacement& replacement,
               const std::uint8_t physical_bank,
               const std::uint16_t instruction_pc,
               const std::uint16_t logical_address,
               const std::uint8_t value,
               const std::uint32_t master_cycle) {
                replacement.work_ram_writes.push_back({
                    .instruction = {physical_bank, instruction_pc},
                    .logical_address = logical_address,
                    .value = value,
                    .master_cycle_offset = master_cycle,
                });
            };

        RoutineReplacement replacement{};
        auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
        auto checkpoint = entry.registers;
        checkpoint.accumulator = actor_state;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x02U, 0x59C7U, 0x59CAU, 15U, checkpoint);
        append_checkpoint(replacement, 0x02U, 0x59CAU, 0x59CCU, 21U, checkpoint);
        checkpoint.accumulator = static_cast<std::uint8_t>(
            checkpoint.accumulator & 0x7FU);
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x02U, 0x59CCU, 0x59CEU, 27U, checkpoint);
        add_write(replacement, 0x02U, 0x59CEU, 0x3840U, checkpoint.accumulator, 42U);
        staged[kActorStateOffset] = static_cast<std::byte>(checkpoint.accumulator);
        append_checkpoint(replacement, 0x02U, 0x59CEU, 0x59D1U, 42U, checkpoint);
        append_checkpoint(replacement, 0x02U, 0x59D1U, 0x59BFU, 54U, checkpoint);

        checkpoint.accumulator = 0x04U;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x02U, 0x59BFU, 0x59C1U, 60U, checkpoint);
        const auto delay_stack_high = checkpoint.stack_pointer;
        const auto delay_stack_low = static_cast<std::uint8_t>(delay_stack_high - 1U);
        add_write(replacement, 0x02U, 0x59C1U,
            static_cast<std::uint16_t>(0x2100U + delay_stack_high), 0x59U, 60U);
        add_write(replacement, 0x02U, 0x59C1U,
            static_cast<std::uint16_t>(0x2100U + delay_stack_low), 0xC3U, 60U);
        staged[0x0100U + delay_stack_high] = static_cast<std::byte>(0x59U);
        staged[0x0100U + delay_stack_low] = static_cast<std::byte>(0xC3U);
        checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
        append_checkpoint(replacement, 0x02U, 0x59C1U, 0xC021U, 81U, checkpoint);
        append_checkpoint(replacement, 0x01U, 0xC021U, 0xC5EAU, 93U, checkpoint);

        add_write(replacement, 0x01U, 0xC5EAU, 0x3DC0U, checkpoint.accumulator, 108U);
        staged[0x1DC0U] = static_cast<std::byte>(checkpoint.accumulator);
        append_checkpoint(replacement, 0x01U, 0xC5EAU, 0xC5EDU, 108U, checkpoint);
        checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer + 1U);
        checkpoint.accumulator = std::to_integer<std::uint8_t>(
            staged[0x0100U + checkpoint.stack_pointer]);
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x01U, 0xC5EDU, 0xC5EEU, 120U, checkpoint);
        add_write(replacement, 0x01U, 0xC5EEU, 0x202FU, checkpoint.accumulator, 132U);
        staged[0x002FU] = static_cast<std::byte>(checkpoint.accumulator);
        append_checkpoint(replacement, 0x01U, 0xC5EEU, 0xC5F0U, 132U, checkpoint);
        checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer + 1U);
        checkpoint.accumulator = std::to_integer<std::uint8_t>(
            staged[0x0100U + checkpoint.stack_pointer]);
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x01U, 0xC5F0U, 0xC5F1U, 144U, checkpoint);
        add_write(replacement, 0x01U, 0xC5F1U, 0x2030U, checkpoint.accumulator, 156U);
        staged[0x0030U] = static_cast<std::byte>(checkpoint.accumulator);
        append_checkpoint(replacement, 0x01U, 0xC5F1U, 0xC5F3U, 156U, checkpoint);
        const auto continuation_low = static_cast<std::uint8_t>(
            std::to_integer<std::uint8_t>(staged[0x002FU]) + 1U);
        checkpoint.status = set_nz(checkpoint.status, continuation_low);
        add_write(replacement, 0x01U, 0xC5F3U, 0x202FU, continuation_low, 174U);
        staged[0x002FU] = static_cast<std::byte>(continuation_low);
        append_checkpoint(replacement, 0x01U, 0xC5F3U, 0xC5F5U, 174U, checkpoint);
        append_checkpoint(replacement, 0x01U, 0xC5F5U, 0xC5F9U, 186U, checkpoint);
        checkpoint.accumulator = continuation_low;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x01U, 0xC5F9U, 0xC5FBU, 195U, checkpoint);
        add_write(replacement, 0x01U, 0xC5FBU, 0x3F40U, checkpoint.accumulator, 213U);
        staged[0x1F40U] = static_cast<std::byte>(checkpoint.accumulator);
        append_checkpoint(replacement, 0x01U, 0xC5FBU, 0xC5FEU, 213U, checkpoint);
        checkpoint.accumulator = std::to_integer<std::uint8_t>(staged[0x0030U]);
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x01U, 0xC5FEU, 0xC600U, 222U, checkpoint);
        add_write(replacement, 0x01U, 0xC600U, 0x3FC0U, checkpoint.accumulator, 240U);
        staged[0x1FC0U] = static_cast<std::byte>(checkpoint.accumulator);
        append_checkpoint(replacement, 0x01U, 0xC600U, 0xC603U, 240U, checkpoint);
        append_checkpoint(replacement, 0x01U, 0xC603U, 0x59C4U, 261U, checkpoint);

        const auto scheduler_stack_high = checkpoint.stack_pointer;
        const auto scheduler_stack_low = static_cast<std::uint8_t>(scheduler_stack_high - 1U);
        add_write(replacement, 0x02U, 0x59C4U,
            static_cast<std::uint16_t>(0x2100U + scheduler_stack_high), 0x59U, 261U);
        add_write(replacement, 0x02U, 0x59C4U,
            static_cast<std::uint16_t>(0x2100U + scheduler_stack_low), 0xC6U, 261U);
        staged[0x0100U + scheduler_stack_high] = static_cast<std::byte>(0x59U);
        staged[0x0100U + scheduler_stack_low] = static_cast<std::byte>(0xC6U);
        checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
        append_checkpoint(replacement, 0x02U, 0x59C4U, 0xC018U, 282U, checkpoint);
        append_checkpoint(replacement, 0x01U, 0xC018U, 0xC5C9U, 294U, checkpoint);

        RoutineEntry scheduler_entry{
            .routine = {.physical_bank = 0x01U, .logical_pc = 0xC5C9U},
            .registers = checkpoint,
        };
        auto scheduler = replace_coroutine_scheduler(scheduler_entry, staged);
        if (!scheduler || scheduler->checkpoints.empty()) {
            return std::nullopt;
        }
        for (auto write : scheduler->work_ram_writes) {
            write.master_cycle_offset += 294U;
            replacement.work_ram_writes.push_back(write);
        }
        for (auto child_checkpoint : scheduler->checkpoints) {
            child_checkpoint.interrupt_sample_master_cycle_offset += 294U;
            child_checkpoint.master_cycle_offset += 294U;
            replacement.checkpoints.push_back(child_checkpoint);
        }
        replacement.return_instruction = scheduler->return_instruction;
        replacement.output_registers = scheduler->output_registers;
        replacement.body_master_cycles = 294U + scheduler->body_master_cycles;
        return replacement;
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
        [](RoutineReplacement& replacement,
           const std::uint8_t physical_bank,
           const std::uint16_t instruction_pc,
           const std::uint16_t resume_pc,
           const std::uint32_t master_cycle,
           auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {physical_bank, instruction_pc},
                .registers = registers,
            });
        };
    const auto add_write =
        [](RoutineReplacement& replacement,
           const std::uint8_t physical_bank,
           const std::uint16_t instruction_pc,
           const std::uint16_t logical_address,
           const std::uint8_t value,
           const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {physical_bank, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
        };

    RoutineReplacement replacement{};
    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    auto checkpoint = entry.registers;
    checkpoint.accumulator = actor_state;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x59C7U, 0x59CAU, 15U, checkpoint);
    append_checkpoint(replacement, 0x02U, 0x59CAU, 0x59D3U, 27U, checkpoint);

    checkpoint.accumulator = 0x82U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x59D3U, 0x59D5U, 33U, checkpoint);
    add_write(replacement, 0x02U, 0x59D5U, 0x3040U, checkpoint.accumulator, 48U);
    append_checkpoint(replacement, 0x02U, 0x59D5U, 0x59D8U, 48U, checkpoint);

    checkpoint.accumulator = 0x01U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x59D8U, 0x59DAU, 54U, checkpoint);
    add_write(replacement, 0x02U, 0x59DAU, 0x3A40U, checkpoint.accumulator, 69U);
    staged[kTimerOffset] = static_cast<std::byte>(checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x59DAU, 0x59DDU, 69U, checkpoint);

    const auto stack_high = checkpoint.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
    if (staged.size() <= 0x0100U + stack_high
        || staged.size() <= 0x0100U + stack_low) {
        return std::nullopt;
    }
    add_write(replacement, 0x02U, 0x59DDU,
        static_cast<std::uint16_t>(0x2100U + stack_high), 0x59U, 69U);
    add_write(replacement, 0x02U, 0x59DDU,
        static_cast<std::uint16_t>(0x2100U + stack_low), 0xDFU, 69U);
    staged[0x0100U + stack_high] = static_cast<std::byte>(0x59U);
    staged[0x0100U + stack_low] = static_cast<std::byte>(0xDFU);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(replacement, 0x02U, 0x59DDU, 0xC018U, 90U, checkpoint);
    append_checkpoint(replacement, 0x01U, 0xC018U, 0xC5C9U, 102U, checkpoint);

    RoutineEntry scheduler_entry{
        .routine = {.physical_bank = 0x01U, .logical_pc = 0xC5C9U},
        .registers = checkpoint,
    };
    auto scheduler = replace_coroutine_scheduler(scheduler_entry, staged);
    if (!scheduler || scheduler->checkpoints.empty()) {
        return std::nullopt;
    }
    for (auto write : scheduler->work_ram_writes) {
        write.master_cycle_offset += 102U;
        replacement.work_ram_writes.push_back(write);
    }
    for (auto child_checkpoint : scheduler->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset += 102U;
        child_checkpoint.master_cycle_offset += 102U;
        replacement.checkpoints.push_back(child_checkpoint);
    }
    replacement.return_instruction = scheduler->return_instruction;
    replacement.output_registers = scheduler->output_registers;
    replacement.body_master_cycles = 102U + scheduler->body_master_cycles;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_upper_wall_pocket_hand_opening_expiry(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x59E0U,
    };
    constexpr std::size_t kTimerOffset = 0x1A40U;
    constexpr std::size_t kAppearanceOffset = 0x19C0U;
    constexpr std::size_t kSceneLatchOffset = 0x0D33U;
    constexpr std::size_t kDelayOffset = 0x1DC0U;
    constexpr std::size_t kContinuationLowOffset = 0x1F40U;
    constexpr std::size_t kContinuationHighOffset = 0x1FC0U;
    if (entry.routine != kRoutine || entry.registers.x != 0x40U
        || (entry.registers.status & 0x28U) != 0U
        || work_ram.size() <= kContinuationHighOffset) {
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
    const auto set_compare =
        [&set_nz](std::uint8_t status,
                  const std::uint8_t left,
                  const std::uint8_t right) {
            status = set_nz(
                static_cast<std::uint8_t>(status & 0xFEU),
                static_cast<std::uint8_t>(left - right));
            if (left >= right) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            return status;
        };
    const auto append_checkpoint =
        [](RoutineReplacement& replacement,
           const std::uint8_t physical_bank,
           const std::uint16_t instruction_pc,
           const std::uint16_t resume_pc,
           const std::uint32_t master_cycle,
           auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {physical_bank, instruction_pc},
                .registers = registers,
            });
        };
    const auto add_write =
        [](RoutineReplacement& replacement,
           const std::uint8_t physical_bank,
           const std::uint16_t instruction_pc,
           const std::uint16_t logical_address,
           const std::uint8_t value,
           const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {physical_bank, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
        };
    const auto stage_write =
        [](std::vector<std::byte>& staged,
           const std::uint16_t logical_address,
           const std::uint8_t value) {
            const auto offset = static_cast<std::size_t>(
                logical_address & 0x1FFFU);
            if (offset >= staged.size()) {
                return false;
            }
            staged[offset] = static_cast<std::byte>(value);
            return true;
        };

    RoutineReplacement replacement{};
    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    auto checkpoint = entry.registers;
    const auto decremented_timer = static_cast<std::uint8_t>(
        std::to_integer<std::uint8_t>(staged[kTimerOffset]) - 1U);
    add_write(replacement, 0x02U, 0x59E0U, 0x3A40U, decremented_timer, 21U);
    staged[kTimerOffset] = static_cast<std::byte>(decremented_timer);
    checkpoint.status = set_nz(checkpoint.status, decremented_timer);
    append_checkpoint(replacement, 0x02U, 0x59E0U, 0x59E3U, 21U, checkpoint);
    if (decremented_timer != 0U) {
        append_checkpoint(replacement, 0x02U, 0x59E3U, 0x59F4U, 33U, checkpoint);
        checkpoint.program_counter = 0x59F4U;
        replacement.return_instruction = {0x02U, 0x59F4U};
        replacement.output_registers = checkpoint;
        replacement.body_master_cycles = 33U;
        return replacement;
    }

    append_checkpoint(replacement, 0x02U, 0x59E3U, 0x59E5U, 27U, checkpoint);
    checkpoint.accumulator = 0x04U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x59E5U, 0x59E7U, 33U, checkpoint);
    add_write(replacement, 0x02U, 0x59E7U, 0x3A40U, checkpoint.accumulator, 48U);
    staged[kTimerOffset] = static_cast<std::byte>(checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x59E7U, 0x59EAU, 48U, checkpoint);
    const auto appearance = static_cast<std::uint8_t>(
        std::to_integer<std::uint8_t>(staged[kAppearanceOffset]) + 1U);
    add_write(replacement, 0x02U, 0x59EAU, 0x39C0U, appearance, 69U);
    staged[kAppearanceOffset] = static_cast<std::byte>(appearance);
    checkpoint.status = set_nz(checkpoint.status, appearance);
    append_checkpoint(replacement, 0x02U, 0x59EAU, 0x59EDU, 69U, checkpoint);
    checkpoint.accumulator = appearance;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x59EDU, 0x59F0U, 84U, checkpoint);
    checkpoint.status = set_compare(checkpoint.status, checkpoint.accumulator, 0x02U);
    append_checkpoint(replacement, 0x02U, 0x59F0U, 0x59F2U, 90U, checkpoint);
    if (appearance != 0x02U) {
        append_checkpoint(replacement, 0x02U, 0x59F2U, 0x59F4U, 96U, checkpoint);
        checkpoint.program_counter = 0x59F4U;
        replacement.return_instruction = {0x02U, 0x59F4U};
        replacement.output_registers = checkpoint;
        replacement.body_master_cycles = 96U;
        return replacement;
    }
    if (std::to_integer<std::uint8_t>(staged[kSceneLatchOffset]) != 0U) {
        return std::nullopt;
    }

    const auto rng_stack_high = checkpoint.stack_pointer;
    const auto rng_stack_low = static_cast<std::uint8_t>(rng_stack_high - 1U);
    if (!stage_write(staged, static_cast<std::uint16_t>(0x2100U + rng_stack_high), 0x59U)
        || !stage_write(staged, static_cast<std::uint16_t>(0x2100U + rng_stack_low), 0xF7U)) {
        return std::nullopt;
    }
    add_write(replacement, 0x02U, 0x59F5U,
        static_cast<std::uint16_t>(0x2100U + rng_stack_high), 0x59U, 102U);
    add_write(replacement, 0x02U, 0x59F5U,
        static_cast<std::uint16_t>(0x2100U + rng_stack_low), 0xF7U, 102U);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(replacement, 0x02U, 0x59F2U, 0x59F5U, 102U, checkpoint);
    append_checkpoint(replacement, 0x02U, 0x59F5U, 0xC081U, 123U, checkpoint);
    append_checkpoint(replacement, 0x00U, 0xC081U, 0xE8B3U, 135U, checkpoint);
    RoutineEntry rng_entry{
        .routine = {.physical_bank = 0x00U, .logical_pc = 0xE8B3U},
        .registers = checkpoint,
    };
    auto rng = replace_rng_update(rng_entry, staged);
    if (!rng || rng->checkpoints.empty()) {
        return std::nullopt;
    }
    for (auto write : rng->work_ram_writes) {
        write.master_cycle_offset += 135U;
        replacement.work_ram_writes.push_back(write);
        if (!stage_write(staged, write.logical_address, write.value)) {
            return std::nullopt;
        }
    }
    for (auto child_checkpoint : rng->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset += 135U;
        child_checkpoint.master_cycle_offset += 135U;
        replacement.checkpoints.push_back(child_checkpoint);
    }
    checkpoint = rng->output_registers;
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer + 2U);
    append_checkpoint(replacement, 0x00U, 0xE8E6U, 0x59F8U, 435U, checkpoint);
    checkpoint.accumulator = static_cast<std::uint8_t>(checkpoint.accumulator & 0x3FU);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x59F8U, 0x59FAU, 441U, checkpoint);
    checkpoint.status = static_cast<std::uint8_t>(checkpoint.status & ~0x01U);
    append_checkpoint(replacement, 0x02U, 0x59FAU, 0x59FBU, 450U, checkpoint);
    checkpoint.accumulator = static_cast<std::uint8_t>(checkpoint.accumulator + 0xB0U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x59FBU, 0x59FDU, 456U, checkpoint);

    const auto delay_stack_high = checkpoint.stack_pointer;
    const auto delay_stack_low = static_cast<std::uint8_t>(delay_stack_high - 1U);
    add_write(replacement, 0x02U, 0x59FDU,
        static_cast<std::uint16_t>(0x2100U + delay_stack_high), 0x59U, 456U);
    add_write(replacement, 0x02U, 0x59FDU,
        static_cast<std::uint16_t>(0x2100U + delay_stack_low), 0xFFU, 456U);
    staged[0x0100U + delay_stack_high] = static_cast<std::byte>(0x59U);
    staged[0x0100U + delay_stack_low] = static_cast<std::byte>(0xFFU);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(replacement, 0x02U, 0x59FDU, 0xC021U, 477U, checkpoint);
    append_checkpoint(replacement, 0x01U, 0xC021U, 0xC5EAU, 489U, checkpoint);
    add_write(replacement, 0x01U, 0xC5EAU, 0x3DC0U, checkpoint.accumulator, 504U);
    staged[kDelayOffset] = static_cast<std::byte>(checkpoint.accumulator);
    append_checkpoint(replacement, 0x01U, 0xC5EAU, 0xC5EDU, 504U, checkpoint);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer + 1U);
    checkpoint.accumulator = std::to_integer<std::uint8_t>(staged[0x0100U + checkpoint.stack_pointer]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x01U, 0xC5EDU, 0xC5EEU, 516U, checkpoint);
    add_write(replacement, 0x01U, 0xC5EEU, 0x202FU, checkpoint.accumulator, 528U);
    staged[0x002FU] = static_cast<std::byte>(checkpoint.accumulator);
    append_checkpoint(replacement, 0x01U, 0xC5EEU, 0xC5F0U, 528U, checkpoint);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer + 1U);
    checkpoint.accumulator = std::to_integer<std::uint8_t>(staged[0x0100U + checkpoint.stack_pointer]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x01U, 0xC5F0U, 0xC5F1U, 540U, checkpoint);
    add_write(replacement, 0x01U, 0xC5F1U, 0x2030U, checkpoint.accumulator, 552U);
    staged[0x0030U] = static_cast<std::byte>(checkpoint.accumulator);
    append_checkpoint(replacement, 0x01U, 0xC5F1U, 0xC5F3U, 552U, checkpoint);
    const auto continuation_low = static_cast<std::uint8_t>(
        std::to_integer<std::uint8_t>(staged[0x002FU]) + 1U);
    checkpoint.status = set_nz(checkpoint.status, continuation_low);
    add_write(replacement, 0x01U, 0xC5F3U, 0x202FU, continuation_low, 570U);
    staged[0x002FU] = static_cast<std::byte>(continuation_low);
    append_checkpoint(replacement, 0x01U, 0xC5F3U, 0xC5F5U, 570U, checkpoint);
    auto continuation_high = std::to_integer<std::uint8_t>(staged[0x0030U]);
    if (continuation_low == 0U) {
        continuation_high = static_cast<std::uint8_t>(continuation_high + 1U);
        add_write(replacement, 0x01U, 0xC5F7U, 0x2030U, continuation_high, 594U);
        staged[0x0030U] = static_cast<std::byte>(continuation_high);
        append_checkpoint(replacement, 0x01U, 0xC5F5U, 0xC5F7U, 582U, checkpoint);
        append_checkpoint(replacement, 0x01U, 0xC5F7U, 0xC5F9U, 594U, checkpoint);
        append_checkpoint(replacement, 0x01U, 0xC5F9U, 0xC5FBU, 603U, checkpoint);
    } else {
        append_checkpoint(replacement, 0x01U, 0xC5F5U, 0xC5F9U, 582U, checkpoint);
        append_checkpoint(replacement, 0x01U, 0xC5F9U, 0xC5FBU, 591U, checkpoint);
    }
    checkpoint.accumulator = continuation_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    const auto continuation_offset = continuation_low == 0U ? 12U : 0U;
    add_write(replacement, 0x01U, 0xC5FBU, 0x3F40U, checkpoint.accumulator, 609U + continuation_offset);
    staged[kContinuationLowOffset] = static_cast<std::byte>(checkpoint.accumulator);
    append_checkpoint(replacement, 0x01U, 0xC5FBU, 0xC5FEU, 609U + continuation_offset, checkpoint);
    checkpoint.accumulator = std::to_integer<std::uint8_t>(staged[0x0030U]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x01U, 0xC5FEU, 0xC600U, 618U + continuation_offset, checkpoint);
    add_write(replacement, 0x01U, 0xC600U, 0x3FC0U, checkpoint.accumulator, 636U + continuation_offset);
    staged[kContinuationHighOffset] = static_cast<std::byte>(checkpoint.accumulator);
    append_checkpoint(replacement, 0x01U, 0xC600U, 0xC603U, 636U + continuation_offset, checkpoint);
    append_checkpoint(replacement, 0x01U, 0xC603U, 0x5A00U, 657U + continuation_offset, checkpoint);
    checkpoint.accumulator = std::to_integer<std::uint8_t>(staged[kSceneLatchOffset]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x5A00U, 0x5A03U, 675U, checkpoint);
    append_checkpoint(replacement, 0x02U, 0x5A03U, 0x5A05U, 690U, checkpoint);

    const auto scheduler_stack_high = checkpoint.stack_pointer;
    const auto scheduler_stack_low = static_cast<std::uint8_t>(scheduler_stack_high - 1U);
    add_write(replacement, 0x02U, 0x5A05U,
        static_cast<std::uint16_t>(0x2100U + scheduler_stack_high), 0x5AU, 690U);
    add_write(replacement, 0x02U, 0x5A05U,
        static_cast<std::uint16_t>(0x2100U + scheduler_stack_low), 0x07U, 690U);
    staged[0x0100U + scheduler_stack_high] = static_cast<std::byte>(0x5AU);
    staged[0x0100U + scheduler_stack_low] = static_cast<std::byte>(0x07U);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(replacement, 0x02U, 0x5A05U, 0xC018U, 711U, checkpoint);
    append_checkpoint(replacement, 0x01U, 0xC018U, 0xC5C9U, 723U, checkpoint);
    RoutineEntry scheduler_entry{
        .routine = {.physical_bank = 0x01U, .logical_pc = 0xC5C9U},
        .registers = checkpoint,
    };
    auto scheduler = replace_coroutine_scheduler(scheduler_entry, staged);
    if (!scheduler || scheduler->checkpoints.empty()) {
        return std::nullopt;
    }
    for (auto write : scheduler->work_ram_writes) {
        write.master_cycle_offset += 723U;
        replacement.work_ram_writes.push_back(write);
    }
    for (auto child_checkpoint : scheduler->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset += 723U;
        child_checkpoint.master_cycle_offset += 723U;
        replacement.checkpoints.push_back(child_checkpoint);
    }
    replacement.return_instruction = scheduler->return_instruction;
    replacement.output_registers = scheduler->output_registers;
    replacement.body_master_cycles = 723U + scheduler->body_master_cycles;
    // The routed zero-latch schedule preserves the clear overflow flag from
    // the original delay handoff; the shared scheduler itself only updates
    // the N/Z state of its decremented delay.
    replacement.output_registers.status = static_cast<std::uint8_t>(
        replacement.output_registers.status & ~0x40U);
    replacement.checkpoints.back().registers = replacement.output_registers;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_upper_wall_pocket_hand_post_opening_schedule(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x5A08U,
    };
    if (entry.routine != kRoutine || entry.registers.x != 0x40U
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    // This is the actual post-opening scheduler entry. The preceding $5A05
    // JSR stores $5A07 on the stack, so the scheduler dispatches here at
    // $5A08. Its JSR to C018 records $5A0A, which becomes the subsequent
    // actor dispatch at $5A0B after the shared scheduler increments it.
    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    auto checkpoint = entry.registers;
    const auto stack_high = checkpoint.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
    if (staged.size() <= 0x0100U + stack_high
        || staged.size() <= 0x0100U + stack_low) {
        return std::nullopt;
    }

    RoutineReplacement replacement{};
    replacement.work_ram_writes = {
        {
            .instruction = {0x02U, 0x5A08U},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_high),
            .value = 0x5AU,
            .master_cycle_offset = 0U,
        },
        {
            .instruction = {0x02U, 0x5A08U},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_low),
            .value = 0x0AU,
            .master_cycle_offset = 0U,
        },
    };
    staged[0x0100U + stack_high] = static_cast<std::byte>(0x5AU);
    staged[0x0100U + stack_low] = static_cast<std::byte>(0x0AU);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);

    const auto append_checkpoint =
        [&replacement](
            const std::uint8_t physical_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {physical_bank, instruction_pc},
                .registers = registers,
            });
        };
    append_checkpoint(0x02U, 0x5A08U, 0xC018U, 21U, checkpoint);
    append_checkpoint(0x01U, 0xC018U, 0xC5C9U, 33U, checkpoint);

    RoutineEntry scheduler_entry{
        .routine = {.physical_bank = 0x01U, .logical_pc = 0xC5C9U},
        .registers = checkpoint,
    };
    auto scheduler = replace_coroutine_scheduler(scheduler_entry, staged);
    if (!scheduler || scheduler->checkpoints.empty()) {
        return std::nullopt;
    }
    constexpr std::uint32_t kSchedulerOffset = 33U;
    for (auto write : scheduler->work_ram_writes) {
        write.master_cycle_offset += kSchedulerOffset;
        replacement.work_ram_writes.push_back(write);
    }
    for (auto child_checkpoint : scheduler->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset +=
            kSchedulerOffset;
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
replace_upper_wall_pocket_hand_post_opening_state(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x5A0BU,
    };
    constexpr std::size_t kTimerOffset = 0x1A40U;
    constexpr std::size_t kCollisionOffset = 0x10C0U;
    if (entry.routine != kRoutine || entry.registers.x != 0x40U
        || (entry.registers.status & 0x28U) != 0U
        || work_ram.size() <= kTimerOffset
        || work_ram.size() <= kCollisionOffset) {
        return std::nullopt;
    }

    // The prior scheduler installs $5A0B from the JSR return at $5A0A.
    // This entry starts the one-tick post-opening state, marks the hand
    // collision byte, and schedules the $5A1B countdown continuation. The
    // natural route receives a timer IRQ while C5C9 is active, so retain the
    // complete instruction-boundary transcript for the shared scheduler.
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
    const auto append_checkpoint =
        [](RoutineReplacement& replacement,
           const std::uint8_t physical_bank,
           const std::uint16_t instruction_pc,
           const std::uint16_t resume_pc,
           const std::uint32_t master_cycle,
           auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {physical_bank, instruction_pc},
                .registers = registers,
            });
        };
    const auto add_write =
        [](RoutineReplacement& replacement,
           const std::uint16_t instruction_pc,
           const std::uint16_t logical_address,
           const std::uint8_t value,
           const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x02U, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
        };

    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    RoutineReplacement replacement{};
    auto checkpoint = entry.registers;
    checkpoint.accumulator = 0x01U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x5A0BU, 0x5A0DU, 6U, checkpoint);

    add_write(replacement, 0x5A0DU, 0x3A40U, 0x01U, 21U);
    staged[kTimerOffset] = static_cast<std::byte>(0x01U);
    append_checkpoint(replacement, 0x02U, 0x5A0DU, 0x5A10U, 21U, checkpoint);

    checkpoint.accumulator =
        std::to_integer<std::uint8_t>(staged[kCollisionOffset]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x5A10U, 0x5A13U, 36U, checkpoint);

    checkpoint.accumulator = static_cast<std::uint8_t>(
        checkpoint.accumulator | 0x40U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x5A13U, 0x5A15U, 42U, checkpoint);

    add_write(
        replacement,
        0x5A15U,
        static_cast<std::uint16_t>(0x3080U + entry.registers.x),
        checkpoint.accumulator,
        57U);
    staged[kCollisionOffset] = static_cast<std::byte>(checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x5A15U, 0x5A18U, 57U, checkpoint);

    const auto stack_high = checkpoint.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
    if (staged.size() <= 0x0100U + stack_high
        || staged.size() <= 0x0100U + stack_low) {
        return std::nullopt;
    }
    add_write(
        replacement,
        0x5A18U,
        static_cast<std::uint16_t>(0x2100U + stack_high),
        0x5AU,
        57U);
    add_write(
        replacement,
        0x5A18U,
        static_cast<std::uint16_t>(0x2100U + stack_low),
        0x1AU,
        57U);
    staged[0x0100U + stack_high] = static_cast<std::byte>(0x5AU);
    staged[0x0100U + stack_low] = static_cast<std::byte>(0x1AU);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 2U);
    append_checkpoint(replacement, 0x02U, 0x5A18U, 0xC018U, 78U, checkpoint);
    append_checkpoint(replacement, 0x01U, 0xC018U, 0xC5C9U, 90U, checkpoint);

    RoutineEntry scheduler_entry{
        .routine = {.physical_bank = 0x01U, .logical_pc = 0xC5C9U},
        .registers = checkpoint,
    };
    auto scheduler = replace_coroutine_scheduler(scheduler_entry, staged);
    if (!scheduler || scheduler->checkpoints.empty()) {
        return std::nullopt;
    }
    constexpr std::uint32_t kSchedulerOffset = 90U;
    for (auto write : scheduler->work_ram_writes) {
        write.master_cycle_offset += kSchedulerOffset;
        replacement.work_ram_writes.push_back(write);
    }
    for (auto child_checkpoint : scheduler->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset +=
            kSchedulerOffset;
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
replace_upper_wall_pocket_hand_post_opening_countdown(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x5A1BU,
    };
    constexpr std::size_t kTimerOffset = 0x1A40U;
    constexpr std::size_t kCollisionOffset = 0x19C0U;
    if (entry.routine != kRoutine || entry.registers.x != 0x40U
        || (entry.registers.status & 0x28U) != 0U
        || work_ram.size() <= kTimerOffset
        || work_ram.size() <= kCollisionOffset) {
        return std::nullopt;
    }

    // $5A0B's scheduler installs this dispatch after the hand has opened.
    // Every visit decrements the one-byte cadence timer.  A nonzero result
    // returns through the local RTS.  On expiry, the original reloads four
    // ticks and consumes the live collision counter; the final zero counter
    // resumes the existing $5975 live/RNG setup in assembly, whose timing
    // still owns the interrupt-bearing tail.
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
    const auto append_checkpoint =
        [](RoutineReplacement& replacement,
           const std::uint16_t instruction_pc,
           const std::uint16_t resume_pc,
           const std::uint32_t master_cycle,
           auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x02U, instruction_pc},
                .registers = registers,
            });
        };
    const auto add_write =
        [](RoutineReplacement& replacement,
           const std::uint16_t instruction_pc,
           const std::uint16_t logical_address,
           const std::uint8_t value,
           const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x02U, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
        };

    const auto timer = std::to_integer<std::uint8_t>(work_ram[kTimerOffset]);
    const auto decremented_timer = static_cast<std::uint8_t>(timer - 1U);
    RoutineReplacement replacement{};
    auto checkpoint = entry.registers;
    checkpoint.status = set_nz(checkpoint.status, decremented_timer);
    add_write(replacement, 0x5A1BU, 0x3A40U, decremented_timer, 21U);
    append_checkpoint(replacement, 0x5A1BU, 0x5A1EU, 21U, checkpoint);

    if (decremented_timer != 0U) {
        append_checkpoint(replacement, 0x5A1EU, 0x5A30U, 33U, checkpoint);
        replacement.return_instruction = {0x02U, 0x5A30U};
        checkpoint.program_counter = 0x5A30U;
        replacement.output_registers = checkpoint;
        replacement.body_master_cycles = 33U;
        return replacement;
    }

    append_checkpoint(replacement, 0x5A1EU, 0x5A20U, 27U, checkpoint);
    checkpoint.accumulator = 0x04U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x5A20U, 0x5A22U, 33U, checkpoint);
    add_write(replacement, 0x5A22U, 0x3A40U, 0x04U, 48U);
    append_checkpoint(replacement, 0x5A22U, 0x5A25U, 48U, checkpoint);

    const auto collision =
        std::to_integer<std::uint8_t>(work_ram[kCollisionOffset]);
    checkpoint.accumulator = collision;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x5A25U, 0x5A28U, 63U, checkpoint);
    if (collision == 0U) {
        append_checkpoint(replacement, 0x5A28U, 0x5A2AU, 69U, checkpoint);
        append_checkpoint(replacement, 0x5A2AU, 0x5975U, 81U, checkpoint);
        replacement.return_instruction = {0x02U, 0x5975U};
        checkpoint.program_counter = 0x5975U;
        replacement.output_registers = checkpoint;
        replacement.body_master_cycles = 81U;
        return replacement;
    }

    append_checkpoint(replacement, 0x5A28U, 0x5A2DU, 69U, checkpoint);
    const auto decremented_collision =
        static_cast<std::uint8_t>(collision - 1U);
    checkpoint.status = set_nz(checkpoint.status, decremented_collision);
    add_write(
        replacement,
        0x5A2DU,
        static_cast<std::uint16_t>(0x3980U + entry.registers.x),
        decremented_collision,
        96U);
    append_checkpoint(replacement, 0x5A2DU, 0x5A30U, 96U, checkpoint);
    replacement.return_instruction = {0x02U, 0x5A30U};
    checkpoint.program_counter = 0x5A30U;
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = 96U;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_upper_wall_pocket_hand_countdown(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x5991U,
    };
    if (entry.routine != kRoutine || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_upper_wall_pocket_hand_countdown countdown{};
    if (!dc_classify_upper_wall_pocket_hand_countdown(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &countdown)) {
        return std::nullopt;
    }

    // The traced $0001 expiry for the routed left hand transfers through the
    // shared scheduler and installs $59A5.  The subsequent, independently
    // timed close-state expiries remain assembly-backed until their distinct
    // interrupt transcripts are captured.
    if (countdown.path == DC_UPPER_WALL_POCKET_HAND_COUNTDOWN_HANDOFF) {
        if (countdown.slot != 0x40U) {
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
        const auto append_checkpoint =
            [](RoutineReplacement& replacement,
               const std::uint8_t physical_bank,
               const std::uint16_t instruction_pc,
               const std::uint16_t resume_pc,
               const std::uint32_t master_cycle,
               auto registers) {
                registers.program_counter = resume_pc;
                replacement.checkpoints.push_back({
                    .interrupt_sample_master_cycle_offset = master_cycle - 3U,
                    .master_cycle_offset = master_cycle,
                    .instruction = {physical_bank, instruction_pc},
                    .registers = registers,
                });
            };

        RoutineReplacement replacement{};
        auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
        auto checkpoint = entry.registers;
        checkpoint.accumulator = countdown.timer_low;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x02U, 0x5991U, 0x5994U, 15U, checkpoint);
        append_checkpoint(replacement, 0x02U, 0x5994U, 0x5999U, 27U, checkpoint);

        checkpoint.accumulator = countdown.decremented_low;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x02U, 0x5999U, 0x599AU, 33U, checkpoint);
        replacement.work_ram_writes.push_back({
            .instruction = {0x02U, 0x599AU},
            .logical_address = static_cast<std::uint16_t>(
                0x3B00U + countdown.slot),
            .value = countdown.decremented_low,
            .master_cycle_offset = 48U,
        });
        staged[0x1B00U + countdown.slot] =
            static_cast<std::byte>(countdown.decremented_low);
        append_checkpoint(replacement, 0x02U, 0x599AU, 0x599DU, 48U, checkpoint);

        checkpoint.accumulator = countdown.remaining_timer;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x02U, 0x599DU, 0x59A0U, 63U, checkpoint);
        append_checkpoint(replacement, 0x02U, 0x59A0U, 0x59A2U, 69U, checkpoint);

        const auto stack_high = checkpoint.stack_pointer;
        const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
        if (staged.size() <= 0x0100U + stack_high
            || staged.size() <= 0x0100U + stack_low) {
            return std::nullopt;
        }
        replacement.work_ram_writes.push_back({
            .instruction = {0x02U, 0x59A2U},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_high),
            .value = 0x59U,
            .master_cycle_offset = 69U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x02U, 0x59A2U},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_low),
            .value = 0xA4U,
            .master_cycle_offset = 69U,
        });
        staged[0x0100U + stack_high] = static_cast<std::byte>(0x59U);
        staged[0x0100U + stack_low] = static_cast<std::byte>(0xA4U);
        checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
        append_checkpoint(replacement, 0x02U, 0x59A2U, 0xC018U, 90U, checkpoint);
        append_checkpoint(replacement, 0x01U, 0xC018U, 0xC5C9U, 102U, checkpoint);

        RoutineEntry scheduler_entry{
            .routine = {
                .physical_bank = 0x01U,
                .logical_pc = 0xC5C9U,
            },
            .registers = checkpoint,
        };
        auto scheduler = replace_coroutine_scheduler(scheduler_entry, staged);
        if (!scheduler || scheduler->checkpoints.empty()) {
            return std::nullopt;
        }
        for (auto write : scheduler->work_ram_writes) {
            write.master_cycle_offset += 102U;
            replacement.work_ram_writes.push_back(write);
        }
        for (auto child_checkpoint : scheduler->checkpoints) {
            child_checkpoint.interrupt_sample_master_cycle_offset += 102U;
            child_checkpoint.master_cycle_offset += 102U;
            replacement.checkpoints.push_back(child_checkpoint);
        }
        replacement.return_instruction = scheduler->return_instruction;
        replacement.output_registers = scheduler->output_registers;
        replacement.body_master_cycles = 102U + scheduler->body_master_cycles;
        return replacement;
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
    auto after_load = entry.registers;
    after_load.accumulator = countdown.timer_low;
    after_load.status = set_nz(after_load.status, after_load.accumulator);
    auto after_high_decrement = after_load;
    after_high_decrement.status = set_nz(
        after_high_decrement.status,
        countdown.decremented_high);
    auto after_low_decrement = after_load;
    after_low_decrement.accumulator = countdown.decremented_low;
    after_low_decrement.status = set_nz(
        after_low_decrement.status,
        after_low_decrement.accumulator);
    auto output = after_low_decrement;
    output.status = set_nz(output.status, countdown.remaining_timer);
    output.program_counter = 0x59F4U;

    const auto borrowed = countdown.timer_low == 0U;
    const auto body_master_cycles = borrowed ? 90U : 75U;
    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x02U,
            .logical_pc = 0x59F4U,
        },
        .output_registers = output,
        .body_master_cycles = body_master_cycles,
    };
    if (borrowed) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x02U, 0x5996U},
            .logical_address = static_cast<std::uint16_t>(
                0x3B80U + countdown.slot),
            .value = countdown.decremented_high,
            .master_cycle_offset = 42U,
        });
    }
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x599AU},
        .logical_address = static_cast<std::uint16_t>(
            0x3B00U + countdown.slot),
        .value = countdown.decremented_low,
        .master_cycle_offset = borrowed ? 63U : 48U,
    });

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
                .instruction = {0x02U, instruction_pc},
                .registers = registers,
            });
        };
    append_checkpoint(0x5991U, 0x5994U, 15U, after_load);
    if (borrowed) {
        append_checkpoint(0x5994U, 0x5996U, 21U, after_load);
        append_checkpoint(0x5996U, 0x5999U, 42U, after_high_decrement);
        append_checkpoint(0x5999U, 0x599AU, 48U, after_low_decrement);
        append_checkpoint(0x599AU, 0x599DU, 63U, after_low_decrement);
        append_checkpoint(0x599DU, 0x59A0U, 78U, output);
        append_checkpoint(0x59A0U, 0x59F4U, 90U, output);
    } else {
        append_checkpoint(0x5994U, 0x5999U, 27U, after_load);
        append_checkpoint(0x5999U, 0x599AU, 33U, after_low_decrement);
        append_checkpoint(0x599AU, 0x599DU, 48U, after_low_decrement);
        append_checkpoint(0x599DU, 0x59A0U, 63U, output);
        append_checkpoint(0x59A0U, 0x59F4U, 75U, output);
    }
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_three_blobs_controller(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x65C2U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    if ((entry.registers.status & 0x20U) != 0U) {
        return std::nullopt;
    }

    dc_three_blobs_controller controller{};
    if (!dc_classify_three_blobs_controller(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &controller)) {
        throw std::runtime_error(
            "Native 65C2 replacement rejected the work-RAM snapshot.");
    }
    if (controller.path == DC_THREE_BLOBS_EQUAL) {
        return std::nullopt;
    }

    const auto set_nz =
        [](std::uint8_t status, const std::uint8_t value) {
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

    const auto comparison =
        controller.path == DC_THREE_BLOBS_FIRST_PAIR_MISMATCH
        ? controller.second_state
        : controller.third_state;
    auto output = entry.registers;
    output.program_counter = 0x65E9U;
    output.accumulator = controller.first_state;
    output.status = compare_status(
        output.status,
        output.accumulator,
        comparison);

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x65E9U,
        },
        .output_registers = output,
        .body_master_cycles =
            controller.path == DC_THREE_BLOBS_FIRST_PAIR_MISMATCH
            ? 42U
            : 63U,
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
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    auto checkpoint = entry.registers;
    checkpoint.accumulator = controller.first_state;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x65C2U, 0x65C5U, 15U, checkpoint);

    checkpoint.status = compare_status(
        checkpoint.status,
        checkpoint.accumulator,
        controller.second_state);
    append_checkpoint(0x65C5U, 0x65C8U, 30U, checkpoint);

    if (controller.path == DC_THREE_BLOBS_FIRST_PAIR_MISMATCH) {
        append_checkpoint(0x65C8U, 0x65E9U, 42U, output);
        return replacement;
    }

    append_checkpoint(0x65C8U, 0x65CAU, 36U, checkpoint);
    checkpoint.status = compare_status(
        checkpoint.status,
        checkpoint.accumulator,
        controller.third_state);
    append_checkpoint(0x65CAU, 0x65CDU, 51U, checkpoint);
    append_checkpoint(0x65CDU, 0x65E9U, 63U, output);
    return replacement;
}

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
