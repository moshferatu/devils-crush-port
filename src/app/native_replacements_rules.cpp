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

using emulation::CpuRegisters;
using emulation::ReplacementMemoryWrite;
using emulation::RoutineEntry;
using emulation::RoutineKey;
using emulation::RoutineReplacement;

// Every one-shot initializer below reproduces a `JSR` whose return is pushed
// through the stack page, so the cells it writes follow the stack pointer the
// transcript was recorded at. That depth is part of the proven shape, not
// incidental. Re-entering one level shallower -- which is exactly what
// re-initializing the table after a drained ball does -- would put the return
// bytes in cells the real push never touched and leave the scheduler at
// `01:C5C9` popping whatever was already there. A lost return address does not
// fail quietly: the processor resumes inside data and writes through work RAM
// until the frame ends. Decline any depth but the proven one.
struct ProvenStackReturn {
    std::uint16_t high = 0;
    std::uint16_t low = 0;
};

[[nodiscard]] std::optional<ProvenStackReturn> proven_stack_return(
    const RoutineEntry& entry,
    const std::uint8_t proven_stack_pointer)
{
    if (entry.registers.stack_pointer != proven_stack_pointer) {
        return std::nullopt;
    }
    const auto high = static_cast<std::uint16_t>(
        0x2100U + entry.registers.stack_pointer);
    return ProvenStackReturn{
        .high = high, .low = static_cast<std::uint16_t>(high - 1U)};
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_giant_skeleton_eyeball_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x4DC8U,
    };
    constexpr std::uint32_t kBodyCycles = 258U;
    // The sole routed setup call is a no-interrupt transcript at frame 872.
    // Its caller enters the same fixed slot/register state every game, while
    // the frontend's conservative horizon lies inside this one-shot script.
    //
    // Re-initializing the table after a drained ball reaches this routine a
    // second time, one level shallower at S=$F2, with slot and status
    // matching. See proven_stack_return for why that has to be declined.
    if (entry.routine != kRoutine || entry.registers.x != 0x2FU
        || entry.registers.status != 0x02U) {
        return std::nullopt;
    }
    const auto stack_return = proven_stack_return(entry, 0xF3U);
    if (!stack_return) {
        return std::nullopt;
    }

    dc_giant_skeleton_eyeball_initializer initializer{};
    if (!dc_classify_giant_skeleton_eyeball_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x4DU;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    add_write(0x02U, 0x4DCDU, 0x30AFU, initializer.flags_after, 36U);
    add_write(0x02U, 0x4DD0U, 0x2D20U, 0x00U, 51U);
    add_write(0x02U, 0x4DD3U, 0x3E2FU, 0x00U, 66U);
    add_write(0x02U, 0x4DD6U, stack_return->high, 0x4DU, 66U);
    add_write(0x02U, 0x4DD6U, stack_return->low, 0xD8U, 66U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0xD8U, 123U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x4DU, 147U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0xD9U, 192U);
    add_write(0x01U, 0xC5E1U, 0x3F2FU, 0xD9U, 231U);
    add_write(0x01U, 0xC5E6U, 0x3FAFU, 0x4DU, 258U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 255U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {0x01U, 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

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
replace_giant_skeleton_eyeball_opening_wait(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x4DF7U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_giant_skeleton_eyeball_opening_wait wait{};
    if (!dc_classify_giant_skeleton_eyeball_opening_wait(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &wait)) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };

    auto output = entry.registers;
    output.status = set_nz(output.status, wait.decremented_timer);
    output.program_counter = 0x4E13U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4E13U},
        .output_registers = output,
        .body_master_cycles = 33U,
    };
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x4DF7U},
        .logical_address = 0x3A2FU,
        .value = wait.decremented_timer,
        .master_cycle_offset = 21U,
    });
    auto checkpoint = output;
    checkpoint.program_counter = 0x4DFAU;
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 18U,
        .master_cycle_offset = 21U,
        .instruction = {0x02U, 0x4DF7U},
        .registers = checkpoint,
    });
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 30U,
        .master_cycle_offset = 33U,
        .instruction = {0x02U, 0x4DFAU},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_giant_skeleton_eyeball_steady_idle(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x4E24U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    // The compact local return itself is deterministic, but an interrupt can
    // occur between its individual load/branch instructions. Leave that
    // interrupt-bearing cadence in assembly until it has a distinct complete
    // transcript.
    if (entry.master_cycles_until_user_event <= 51U
        || entry.master_cycles_until_timer_interrupt <= 51U) {
        return std::nullopt;
    }

    dc_giant_skeleton_eyeball_steady_idle idle{};
    if (!dc_classify_giant_skeleton_eyeball_steady_idle(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &idle)) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
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
            .instruction = {0x02U, instruction_pc},
            .registers = registers,
        });
    };

    auto output = entry.registers;
    output.accumulator = 0U;
    output.status = set_nz(output.status, output.accumulator);
    output.program_counter = 0x4E32U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4E32U},
        .output_registers = output,
        .body_master_cycles = 51U,
    };

    auto checkpoint = entry.registers;
    checkpoint.accumulator = idle.collision_state;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x4E24U, 0x4E27U, 15U, checkpoint);
    append_checkpoint(replacement, 0x4E27U, 0x4E29U, 21U, checkpoint);
    checkpoint.accumulator = idle.session_flags;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x4E29U, 0x4E2BU, 30U, checkpoint);
    checkpoint.accumulator = 0U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x4E2BU, 0x4E2DU, 36U, checkpoint);
    append_checkpoint(replacement, 0x4E2DU, 0x4E32U, 51U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_giant_skeleton_eyeball_final_wait(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x4F3DU,
    };
    if (entry.routine != kRoutine || entry.master_cycles_until_user_event <= 102U
        || entry.master_cycles_until_timer_interrupt <= 102U) {
        return std::nullopt;
    }

    dc_giant_skeleton_eyeball_final_wait wait{};
    if (!dc_classify_giant_skeleton_eyeball_final_wait(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &wait)) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
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
            .instruction = {0x02U, instruction_pc},
            .registers = registers,
        });
    };

    auto output = entry.registers;
    output.accumulator = wait.final_accumulator;
    output.status = set_nz(output.status, output.accumulator);
    output.program_counter = 0x4F75U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4F75U},
        .output_registers = output,
        .body_master_cycles = 102U,
    };
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x4F3DU},
        .logical_address = 0x3A2FU,
        .value = wait.decremented_timer,
        .master_cycle_offset = 21U,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x4F6DU},
        .logical_address = 0x3B2FU,
        .value = wait.decremented_phase,
        .master_cycle_offset = 81U,
    });

    auto checkpoint = entry.registers;
    checkpoint.status = set_nz(checkpoint.status, wait.decremented_timer);
    append_checkpoint(replacement, 0x4F3DU, 0x4F40U, 21U, checkpoint);
    append_checkpoint(replacement, 0x4F40U, 0x4F64U, 33U, checkpoint);
    checkpoint.accumulator = wait.phase;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x4F64U, 0x4F67U, 48U, checkpoint);
    append_checkpoint(replacement, 0x4F67U, 0x4F6CU, 60U, checkpoint);
    checkpoint.accumulator = wait.decremented_phase;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x4F6CU, 0x4F6DU, 66U, checkpoint);
    append_checkpoint(replacement, 0x4F6DU, 0x4F70U, 81U, checkpoint);
    checkpoint.accumulator = wait.final_accumulator;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x4F70U, 0x4F73U, 96U, checkpoint);
    append_checkpoint(replacement, 0x4F73U, 0x4F75U, 102U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_gray_blob_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x6876U,
    };
    constexpr std::uint32_t kBodyCycles = 372U;
    if (entry.routine != kRoutine || (entry.registers.status & 0x20U) != 0U) {
        return std::nullopt;
    }
    const auto stack_return = proven_stack_return(entry, 0xF3U);
    if (!stack_return) {
        return std::nullopt;
    }

    dc_gray_blob_initializer initializer{};
    if (!dc_classify_gray_blob_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x68U;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    const auto slot_address = [slot = initializer.slot](const std::uint16_t base) {
        return static_cast<std::uint16_t>(base + slot);
    };
    add_write(0x03U, 0x6878U, slot_address(0x3D00U), 0x05U, 21U);
    add_write(0x03U, 0x687DU, slot_address(0x3800U), 0x13U, 42U);
    add_write(0x03U, 0x6883U, slot_address(0x3E00U), initializer.position_low, 72U);
    add_write(0x03U, 0x6889U, slot_address(0x3E80U), initializer.position_high, 102U);
    add_write(0x03U, 0x688EU, slot_address(0x3680U), 0x20U, 123U);
    add_write(0x03U, 0x6891U, slot_address(0x3400U), 0x00U, 138U);
    add_write(0x03U, 0x6896U, slot_address(0x3480U), 0x01U, 159U);
    add_write(0x03U, 0x689BU, slot_address(0x3A00U), 0x01U, 180U);
    add_write(0x03U, 0x689EU, stack_return->high, 0x68U, 180U);
    add_write(0x03U, 0x689EU, stack_return->low, 0xA0U, 180U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0xA0U, 237U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x68U, 261U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0xA1U, 306U);
    add_write(0x01U, 0xC5E1U, slot_address(0x3F00U), 0xA1U, 345U);
    add_write(0x01U, 0xC5E6U, slot_address(0x3F80U), 0x68U, 372U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 369U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_gray_blob_first_live_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x03U, .logical_pc = 0x68A1U};
    if (entry.routine != kRoutine || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_gray_blob_first_live first_live{};
    if (!dc_classify_gray_blob_first_live(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &first_live)) {
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
    const auto adc_status =
        [&set_nz](std::uint8_t status,
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
           const std::uint8_t bank,
           const std::uint16_t instruction_pc,
           const std::uint16_t resume_pc,
           const std::uint32_t master_cycle,
           auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {bank, instruction_pc},
                .registers = registers,
            });
        };
    const auto add_write =
        [](RoutineReplacement& replacement,
           const std::uint8_t bank,
           const std::uint16_t instruction,
           const std::uint16_t address,
           const std::uint8_t value,
           const std::uint32_t cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {bank, instruction},
                .logical_address = address,
                .value = value,
                .master_cycle_offset = cycle,
            });
        };
    const auto stage_write =
        [](std::vector<std::byte>& staged,
           const std::uint16_t logical_address,
           const std::uint8_t value) {
            const auto offset = static_cast<std::size_t>(logical_address & 0x1FFFU);
            if (offset >= staged.size()) {
                return false;
            }
            staged[offset] = static_cast<std::byte>(value);
            return true;
        };

    RoutineReplacement replacement{};
    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    auto checkpoint = entry.registers;
    checkpoint.accumulator = first_live.flags_before;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x03U, 0x68A1U, 0x68A4U, 15U, checkpoint);

    checkpoint.accumulator = first_live.flags_after;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x03U, 0x68A4U, 0x68A6U, 21U, checkpoint);
    add_write(replacement, 0x03U, 0x68A6U,
              static_cast<std::uint16_t>(0x3080U + first_live.slot),
              checkpoint.accumulator, 36U);
    if (!stage_write(staged, static_cast<std::uint16_t>(0x3080U + first_live.slot),
                     checkpoint.accumulator)) {
        return std::nullopt;
    }
    append_checkpoint(replacement, 0x03U, 0x68A6U, 0x68A9U, 36U, checkpoint);

    checkpoint.accumulator = first_live.collision_before;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x03U, 0x68A9U, 0x68ACU, 51U, checkpoint);
    checkpoint.accumulator = first_live.collision_after;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x03U, 0x68ACU, 0x68AEU, 57U, checkpoint);
    add_write(replacement, 0x03U, 0x68AEU,
              static_cast<std::uint16_t>(0x3800U + first_live.slot),
              checkpoint.accumulator, 72U);
    if (!stage_write(staged, static_cast<std::uint16_t>(0x3800U + first_live.slot),
                     checkpoint.accumulator)) {
        return std::nullopt;
    }
    append_checkpoint(replacement, 0x03U, 0x68AEU, 0x68B1U, 72U, checkpoint);

    const auto rng_stack_high = checkpoint.stack_pointer;
    const auto rng_stack_low = static_cast<std::uint8_t>(rng_stack_high - 1U);
    add_write(replacement, 0x03U, 0x68B1U,
              static_cast<std::uint16_t>(0x2100U + rng_stack_high), 0x68U, 72U);
    add_write(replacement, 0x03U, 0x68B1U,
              static_cast<std::uint16_t>(0x2100U + rng_stack_low), 0xB3U, 72U);
    if (!stage_write(staged, static_cast<std::uint16_t>(0x2100U + rng_stack_high), 0x68U)
        || !stage_write(staged, static_cast<std::uint16_t>(0x2100U + rng_stack_low), 0xB3U)) {
        return std::nullopt;
    }
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(replacement, 0x03U, 0x68B1U, 0xC081U, 93U, checkpoint);
    append_checkpoint(replacement, 0x00U, 0xC081U, 0xE8B3U, 105U, checkpoint);

    RoutineEntry rng_entry{
        .routine = {.physical_bank = 0x00U, .logical_pc = 0xE8B3U},
        .registers = checkpoint,
    };
    auto rng = replace_rng_update(rng_entry, staged);
    if (!rng || rng->checkpoints.empty()) {
        return std::nullopt;
    }
    for (auto write : rng->work_ram_writes) {
        write.master_cycle_offset += 105U;
        replacement.work_ram_writes.push_back(write);
        if (!stage_write(staged, write.logical_address, write.value)) {
            return std::nullopt;
        }
    }
    for (auto child_checkpoint : rng->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset += 105U;
        child_checkpoint.master_cycle_offset += 105U;
        replacement.checkpoints.push_back(child_checkpoint);
    }

    checkpoint = rng->output_registers;
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer + 2U);
    append_checkpoint(replacement, 0x00U, 0xE8E6U, 0x68B4U, 405U, checkpoint);
    checkpoint.accumulator = static_cast<std::uint8_t>(checkpoint.accumulator & 0x1FU);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x03U, 0x68B4U, 0x68B6U, 411U, checkpoint);
    checkpoint.status = static_cast<std::uint8_t>(checkpoint.status & ~0x01U);
    append_checkpoint(replacement, 0x03U, 0x68B6U, 0x68B7U, 417U, checkpoint);
    const auto timer = static_cast<std::uint8_t>(checkpoint.accumulator + 0x40U);
    checkpoint.status = adc_status(
        checkpoint.status, checkpoint.accumulator, 0x40U, timer);
    checkpoint.accumulator = timer;
    append_checkpoint(replacement, 0x03U, 0x68B7U, 0x68B9U, 426U, checkpoint);
    add_write(replacement, 0x03U, 0x68B9U,
              static_cast<std::uint16_t>(0x3B80U + first_live.slot), timer, 441U);
    if (!stage_write(staged, static_cast<std::uint16_t>(0x3B80U + first_live.slot), timer)) {
        return std::nullopt;
    }
    append_checkpoint(replacement, 0x03U, 0x68B9U, 0x68BCU, 441U, checkpoint);

    const auto scheduler_stack_high = checkpoint.stack_pointer;
    const auto scheduler_stack_low = static_cast<std::uint8_t>(scheduler_stack_high - 1U);
    add_write(replacement, 0x03U, 0x68BCU,
              static_cast<std::uint16_t>(0x2100U + scheduler_stack_high), 0x68U, 441U);
    add_write(replacement, 0x03U, 0x68BCU,
              static_cast<std::uint16_t>(0x2100U + scheduler_stack_low), 0xBEU, 441U);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    checkpoint.program_counter = 0xC018U;
    // The C018 trampoline is entered after the caller's JSR frame; the
    // coroutine dispatcher owns the remaining scheduler tail.
    append_checkpoint(replacement, 0x03U, 0x68BCU, 0xC018U, 462U, checkpoint);
    replacement.return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC018U};
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = 462U;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_mercy_star_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x4F9FU,
    };
    constexpr std::uint32_t kBodyCycles = 318U;
    if (entry.routine != kRoutine || entry.registers.status != 0x02U
        || entry.registers.accumulator != 0U || entry.registers.y != 0x0BU) {
        return std::nullopt;
    }
    const auto stack_return = proven_stack_return(entry, 0xF3U);
    if (!stack_return) {
        return std::nullopt;
    }

    dc_mercy_star_initializer initializer{};
    if (!dc_classify_mercy_star_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x4FU;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    const auto slot_address = [slot = initializer.slot](const std::uint16_t base) {
        return static_cast<std::uint16_t>(base + slot);
    };
    add_write(0x02U, 0x4FA9U, slot_address(0x3C80U), 0x16U, 48U);
    add_write(0x02U, 0x4FAEU, slot_address(0x3700U), 0x01U, 69U);
    add_write(0x02U, 0x4FB3U, slot_address(0x3780U), 0x05U, 90U);
    add_write(0x02U, 0x4FBBU, slot_address(0x3080U), 0xE0U, 126U);
    add_write(0x02U, 0x4FBEU, stack_return->high, 0x4FU, 126U);
    add_write(0x02U, 0x4FBEU, stack_return->low, 0xC0U, 126U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0xC0U, 183U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x4FU, 207U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0xC1U, 252U);
    add_write(0x01U, 0xC5E1U, slot_address(0x3F00U), 0xC1U, 291U);
    add_write(0x01U, 0xC5E6U, slot_address(0x3F80U), 0x4FU, 318U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 315U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E6U},
        .registers = output,
    });
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
replace_dragon_egg_controller_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x42EBU};
    constexpr std::uint32_t kBodyCycles = 228U;
    if (entry.routine != kRoutine || entry.registers.status != 0x02U) {
        return std::nullopt;
    }

    dc_dragon_egg_controller_initializer initializer{};
    if (!dc_classify_dragon_egg_controller_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x42U;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    const auto stack_address = [stack = entry.registers.stack_pointer](
                                   const std::uint8_t adjustment) {
        return static_cast<std::uint16_t>(
            0x2100U + static_cast<std::uint8_t>(stack - adjustment));
    };
    add_write(0x02U, 0x42F0U, 0x2CF7U, 0x00U, 36U);
    add_write(0x02U, 0x42F3U, stack_address(0U), 0x42U, 36U);
    add_write(0x02U, 0x42F3U, stack_address(1U), 0xF5U, 36U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0xF5U, 93U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x42U, 117U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0xF6U, 162U);
    add_write(0x01U, 0xC5E1U, static_cast<std::uint16_t>(0x3F00U + initializer.slot),
              0xF6U, 201U);
    add_write(0x01U, 0xC5E6U, static_cast<std::uint16_t>(0x3F80U + initializer.slot),
              0x42U, 228U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = kBodyCycles - 3U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {0x01U, 0xC5E6U},
        .registers = output,
    });
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
replace_big_star_pending_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x570AU,
    };
    if (entry.routine != kRoutine
        || (entry.registers.status & 0x20U) != 0U) {
        return std::nullopt;
    }

    dc_big_star_pending_gate gate{};
    if (!dc_classify_big_star_pending_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &gate)) {
        throw std::runtime_error(
            "Native 570A replacement rejected the work-RAM snapshot.");
    }
    if (gate.path != DC_BIG_STAR_PENDING_GATE_IDLE) {
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

    auto checkpoint = entry.registers;
    checkpoint.accumulator = gate.pending_feature_bits;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    auto output = checkpoint;
    output.program_counter = 0x5721U;

    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x5721U},
        .output_registers = output,
        .body_master_cycles = 81U,
    };
    append_checkpoint(replacement, 0x570AU, 0x570DU, 15U, checkpoint);
    checkpoint.accumulator = static_cast<std::uint8_t>(
        gate.pending_feature_bits & 0x01U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x570DU, 0x570FU, 21U, checkpoint);
    append_checkpoint(replacement, 0x570FU, 0x5714U, 30U, checkpoint);
    checkpoint.accumulator = gate.pending_feature_bits;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x5714U, 0x5717U, 45U, checkpoint);
    checkpoint.accumulator = static_cast<std::uint8_t>(
        gate.pending_feature_bits & 0x02U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x5717U, 0x5719U, 51U, checkpoint);
    append_checkpoint(replacement, 0x5719U, 0x571EU, 60U, checkpoint);
    output = checkpoint;
    output.program_counter = 0x5721U;
    replacement.output_registers = output;
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x571EU},
        .logical_address = 0x2D04U,
        .value = 0x00U,
        .master_cycle_offset = 81U,
    });
    append_checkpoint(replacement, 0x571EU, 0x5721U, 81U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_big_star_pending_helper(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x5722U,
    };
    constexpr std::uint32_t kBodyCycles = 240U;
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x01U
        || entry.registers.y != 0x03U || entry.registers.stack_pointer != 0xF7U
        || entry.registers.status != 0U) {
        return std::nullopt;
    }

    dc_big_star_pending_helper helper{};
    if (!dc_classify_big_star_pending_helper(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &helper)) {
        throw std::runtime_error(
            "Native 5722 replacement rejected the work-RAM snapshot.");
    }
    const bool lower_phase_reset = helper.phase < 3U;
    if (!lower_phase_reset && helper.phase != 3U && helper.phase != 4U
        && helper.phase != 5U && helper.phase != 6U) {
        return std::nullopt;
    }
    const bool phase_six_saturates = helper.phase == 6U;
    constexpr std::size_t kSceneCounterOffset = 0x0D18U;
    if (lower_phase_reset && work_ram.size() <= kSceneCounterOffset) {
        return std::nullopt;
    }
    const std::uint8_t horizontal_velocity = lower_phase_reset
        ? 0U
        : (helper.phase == 3U
        ? 0xF0U
        : (helper.phase == 4U ? 0xE0U : 0xC0U));
    const std::uint8_t cadence = lower_phase_reset
        ? 0U
        : (helper.phase == 3U
        ? 0x08U
        : (helper.phase == 4U ? 0x06U : 0x03U));

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto set_compare = [](std::uint8_t status,
                                const std::uint8_t left,
                                const std::uint8_t right) {
        const auto result = static_cast<std::uint8_t>(left - right);
        status = static_cast<std::uint8_t>(status & 0x7CU);
        if (left >= right) {
            status = static_cast<std::uint8_t>(status | 0x01U);
        }
        if (result == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        }
        if ((result & 0x80U) != 0U) {
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
            .instruction = {0x02U, instruction_pc},
            .registers = registers,
        });
    };
    const auto add_write = [](RoutineReplacement& replacement,
                              const std::uint16_t instruction_pc,
                              const std::uint16_t address,
                              const std::uint8_t value,
                              const std::uint32_t master_cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x02U, instruction_pc},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = master_cycle,
        });
    };

    if (lower_phase_reset) {
        auto checkpoint = entry.registers;
        checkpoint.accumulator = helper.phase;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        RoutineReplacement replacement{
            .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x5778U},
            .output_registers = checkpoint,
            .body_master_cycles = 252U,
        };
        append_checkpoint(replacement, 0x5722U, 0x5725U, 15U, checkpoint);
        checkpoint.status = set_compare(checkpoint.status, helper.phase, 3U);
        append_checkpoint(replacement, 0x5725U, 0x5727U, 21U, checkpoint);
        append_checkpoint(replacement, 0x5727U, 0x5729U, 27U, checkpoint);
        checkpoint.accumulator = 3U;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x5729U, 0x572BU, 33U, checkpoint);
        append_checkpoint(replacement, 0x572BU, 0x5749U, 45U, checkpoint);
        add_write(replacement, 0x5749U, 0x3B00U + helper.slot, helper.next_phase, 60U);
        append_checkpoint(replacement, 0x5749U, 0x574CU, 60U, checkpoint);
        checkpoint.accumulator = static_cast<std::uint8_t>(helper.next_phase << 1U);
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x574CU, 0x574DU, 66U, checkpoint);
        checkpoint.y = checkpoint.accumulator;
        append_checkpoint(replacement, 0x574DU, 0x574EU, 72U, checkpoint);
        checkpoint.accumulator = horizontal_velocity;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x574EU, 0x5751U, 87U, checkpoint);
        add_write(replacement, 0x5751U, 0x2D00U, horizontal_velocity, 102U);
        append_checkpoint(replacement, 0x5751U, 0x5754U, 102U, checkpoint);
        checkpoint.accumulator = 0U;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x5754U, 0x5757U, 117U, checkpoint);
        add_write(replacement, 0x5757U, 0x2D01U, 0U, 132U);
        append_checkpoint(replacement, 0x5757U, 0x575AU, 132U, checkpoint);
        checkpoint.y = helper.next_phase;
        append_checkpoint(replacement, 0x575AU, 0x575DU, 147U, checkpoint);
        checkpoint.accumulator = cadence;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x575DU, 0x5760U, 162U, checkpoint);
        add_write(replacement, 0x5760U, 0x3A00U + helper.slot, cadence, 177U);
        append_checkpoint(replacement, 0x5760U, 0x5763U, 177U, checkpoint);
        add_write(replacement, 0x5763U, 0x2D05U, cadence, 192U);
        append_checkpoint(replacement, 0x5763U, 0x5766U, 192U, checkpoint);
        append_checkpoint(replacement, 0x5766U, 0x576CU, 204U, checkpoint);
        checkpoint.accumulator = std::to_integer<std::uint8_t>(work_ram[kSceneCounterOffset]);
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x576CU, 0x576FU, 219U, checkpoint);
        checkpoint.status = set_compare(checkpoint.status, checkpoint.accumulator, 4U);
        append_checkpoint(replacement, 0x576FU, 0x5771U, 225U, checkpoint);
        if (checkpoint.accumulator == 4U) {
            append_checkpoint(replacement, 0x5771U, 0x5768U, 237U, checkpoint);
            add_write(replacement, 0x5768U, 0x2CFAU, 0U, 252U);
            checkpoint.program_counter = 0x576BU;
            replacement.return_instruction.logical_pc = 0x576BU;
        } else {
            append_checkpoint(replacement, 0x5771U, 0x5773U, 231U, checkpoint);
            checkpoint.accumulator = 1U;
            checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(replacement, 0x5773U, 0x5775U, 237U, checkpoint);
            add_write(replacement, 0x5775U, 0x2CFAU, 1U, 252U);
            checkpoint.program_counter = 0x5778U;
        }
        replacement.output_registers = checkpoint;
        append_checkpoint(replacement, replacement.return_instruction.logical_pc == 0x576BU ? 0x5768U : 0x5775U,
                          replacement.return_instruction.logical_pc, 252U, checkpoint);
        return replacement;
    }

    auto checkpoint = entry.registers;
    checkpoint.accumulator = helper.phase;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x576BU},
        .output_registers = checkpoint,
        .body_master_cycles = phase_six_saturates ? 252U : kBodyCycles,
    };
    append_checkpoint(replacement, 0x5722U, 0x5725U, 15U, checkpoint);
    checkpoint.status = set_compare(checkpoint.status, helper.phase, 3U);
    append_checkpoint(replacement, 0x5725U, 0x5727U, 21U, checkpoint);
    append_checkpoint(replacement, 0x5727U, 0x572DU, 33U, checkpoint);
    checkpoint.accumulator = helper.phase;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x572DU, 0x5730U, 48U, checkpoint);
    checkpoint.accumulator = helper.next_phase;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x5730U, 0x5731U, 54U, checkpoint);
    checkpoint.status = set_compare(checkpoint.status, helper.next_phase, 7U);
    append_checkpoint(replacement, 0x5731U, 0x5733U, 60U, checkpoint);
    const std::uint32_t tail_cycle_offset = phase_six_saturates ? 12U : 0U;
    if (phase_six_saturates) {
        append_checkpoint(replacement, 0x5733U, 0x5735U, 66U, checkpoint);
        checkpoint.accumulator = helper.next_phase;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x5735U, 0x5736U, 72U, checkpoint);
        append_checkpoint(replacement, 0x5736U, 0x5749U, 84U, checkpoint);
    } else {
        append_checkpoint(replacement, 0x5733U, 0x5749U, 72U, checkpoint);
    }
    add_write(replacement, 0x5749U,
              static_cast<std::uint16_t>(0x3B00U + helper.slot),
              helper.next_phase, 87U + tail_cycle_offset);
    append_checkpoint(replacement, 0x5749U, 0x574CU, 87U + tail_cycle_offset, checkpoint);
    checkpoint.accumulator = static_cast<std::uint8_t>(helper.next_phase << 1U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x574CU, 0x574DU, 93U + tail_cycle_offset, checkpoint);
    checkpoint.y = checkpoint.accumulator;
    append_checkpoint(replacement, 0x574DU, 0x574EU, 99U + tail_cycle_offset, checkpoint);
    checkpoint.accumulator = horizontal_velocity;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x574EU, 0x5751U, 114U + tail_cycle_offset, checkpoint);
    add_write(replacement, 0x5751U, 0x2D00U, horizontal_velocity, 129U + tail_cycle_offset);
    append_checkpoint(replacement, 0x5751U, 0x5754U, 129U + tail_cycle_offset, checkpoint);
    checkpoint.accumulator = 0xFFU;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x5754U, 0x5757U, 144U + tail_cycle_offset, checkpoint);
    add_write(replacement, 0x5757U, 0x2D01U, 0xFFU, 159U + tail_cycle_offset);
    append_checkpoint(replacement, 0x5757U, 0x575AU, 159U + tail_cycle_offset, checkpoint);
    checkpoint.y = helper.next_phase;
    append_checkpoint(replacement, 0x575AU, 0x575DU, 174U + tail_cycle_offset, checkpoint);
    checkpoint.accumulator = cadence;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x575DU, 0x5760U, 189U + tail_cycle_offset, checkpoint);
    add_write(replacement, 0x5760U,
              static_cast<std::uint16_t>(0x3A00U + helper.slot),
              cadence, 204U + tail_cycle_offset);
    append_checkpoint(replacement, 0x5760U, 0x5763U, 204U + tail_cycle_offset, checkpoint);
    add_write(replacement, 0x5763U, 0x2D05U, cadence, 219U + tail_cycle_offset);
    append_checkpoint(replacement, 0x5763U, 0x5766U, 219U + tail_cycle_offset, checkpoint);
    append_checkpoint(replacement, 0x5766U, 0x5768U, 225U + tail_cycle_offset, checkpoint);
    add_write(replacement, 0x5768U, 0x2CFAU, 0U, 240U + tail_cycle_offset);
    checkpoint.program_counter = 0x576BU;
    replacement.output_registers = checkpoint;
    append_checkpoint(replacement, 0x5768U, 0x576BU, 240U + tail_cycle_offset, checkpoint);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_big_star_pending_right_helper(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x5738U,
    };
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x02U
        || entry.registers.y != 0x03U || entry.registers.stack_pointer != 0xF7U
        || entry.registers.status != 0U) {
        return std::nullopt;
    }

    dc_big_star_pending_right_helper helper{};
    if (!dc_classify_big_star_pending_right_helper(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &helper)) {
        throw std::runtime_error(
            "Native 5738 replacement rejected the work-RAM snapshot.");
    }
    if (helper.phase > 4U) {
        return std::nullopt;
    }
    const bool phase_four_reset = helper.phase == 4U;
    constexpr std::size_t kSceneCounterOffset = 0x0D18U;
    if (phase_four_reset && work_ram.size() <= kSceneCounterOffset) {
        return std::nullopt;
    }

    const std::uint8_t horizontal_velocity = phase_four_reset
        ? 0U
        : (helper.phase == 3U
        ? 0x10U
        : (helper.phase == 2U ? 0x20U : 0x40U));
    const std::uint8_t cadence = phase_four_reset
        ? 0U
        : (helper.phase == 3U
        ? 0x08U
        : (helper.phase == 2U ? 0x06U : 0x03U));

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto set_compare = [](std::uint8_t status,
                                const std::uint8_t left,
                                const std::uint8_t right) {
        const auto result = static_cast<std::uint8_t>(left - right);
        status = static_cast<std::uint8_t>(status & 0x7CU);
        if (left >= right) {
            status = static_cast<std::uint8_t>(status | 0x01U);
        }
        if (result == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        }
        if ((result & 0x80U) != 0U) {
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
            .instruction = {0x02U, instruction_pc},
            .registers = registers,
        });
    };
    const auto add_write = [](RoutineReplacement& replacement,
                              const std::uint16_t instruction_pc,
                              const std::uint16_t address,
                              const std::uint8_t value,
                              const std::uint32_t master_cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x02U, instruction_pc},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = master_cycle,
        });
    };

    auto checkpoint = entry.registers;
    checkpoint.accumulator = helper.phase;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U,
                               .logical_pc = phase_four_reset ? 0x5778U : 0x576BU},
        .output_registers = checkpoint,
        .body_master_cycles = phase_four_reset ? 252U : 228U,
    };
    append_checkpoint(replacement, 0x5738U, 0x573BU, 15U, checkpoint);
    checkpoint.status = set_compare(checkpoint.status, helper.phase, 4U);
    append_checkpoint(replacement, 0x573BU, 0x573DU, 21U, checkpoint);
    if (phase_four_reset) {
        append_checkpoint(replacement, 0x573DU, 0x573FU, 27U, checkpoint);
        checkpoint.accumulator = 3U;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x573FU, 0x5741U, 33U, checkpoint);
        append_checkpoint(replacement, 0x5741U, 0x5749U, 45U, checkpoint);
        add_write(replacement, 0x5749U,
                  static_cast<std::uint16_t>(0x3B00U + helper.slot),
                  helper.next_phase, 60U);
        append_checkpoint(replacement, 0x5749U, 0x574CU, 60U, checkpoint);
        checkpoint.accumulator = static_cast<std::uint8_t>(helper.next_phase << 1U);
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x574CU, 0x574DU, 66U, checkpoint);
        checkpoint.y = checkpoint.accumulator;
        append_checkpoint(replacement, 0x574DU, 0x574EU, 72U, checkpoint);
        checkpoint.accumulator = horizontal_velocity;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x574EU, 0x5751U, 87U, checkpoint);
        add_write(replacement, 0x5751U, 0x2D00U, horizontal_velocity, 102U);
        append_checkpoint(replacement, 0x5751U, 0x5754U, 102U, checkpoint);
        checkpoint.accumulator = 0U;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x5754U, 0x5757U, 117U, checkpoint);
        add_write(replacement, 0x5757U, 0x2D01U, 0U, 132U);
        append_checkpoint(replacement, 0x5757U, 0x575AU, 132U, checkpoint);
        checkpoint.y = helper.next_phase;
        append_checkpoint(replacement, 0x575AU, 0x575DU, 147U, checkpoint);
        checkpoint.accumulator = cadence;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x575DU, 0x5760U, 162U, checkpoint);
        add_write(replacement, 0x5760U,
                  static_cast<std::uint16_t>(0x3A00U + helper.slot),
                  cadence, 177U);
        append_checkpoint(replacement, 0x5760U, 0x5763U, 177U, checkpoint);
        add_write(replacement, 0x5763U, 0x2D05U, cadence, 192U);
        append_checkpoint(replacement, 0x5763U, 0x5766U, 192U, checkpoint);
        append_checkpoint(replacement, 0x5766U, 0x576CU, 204U, checkpoint);
        checkpoint.accumulator = std::to_integer<std::uint8_t>(work_ram[kSceneCounterOffset]);
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x576CU, 0x576FU, 219U, checkpoint);
        checkpoint.status = set_compare(checkpoint.status, checkpoint.accumulator, 4U);
        append_checkpoint(replacement, 0x576FU, 0x5771U, 225U, checkpoint);
        const bool scene_counter_equals_four = checkpoint.accumulator == 4U;
        if (scene_counter_equals_four) {
            append_checkpoint(replacement, 0x5771U, 0x5768U, 237U, checkpoint);
            add_write(replacement, 0x5768U, 0x2CFAU, 0U, 252U);
            checkpoint.program_counter = 0x576BU;
            replacement.return_instruction.logical_pc = 0x576BU;
            replacement.output_registers = checkpoint;
            append_checkpoint(replacement, 0x5768U, 0x576BU, 252U, checkpoint);
            return replacement;
        }
        append_checkpoint(replacement, 0x5771U, 0x5773U, 231U, checkpoint);
        checkpoint.accumulator = 1U;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x5773U, 0x5775U, 237U, checkpoint);
        add_write(replacement, 0x5775U, 0x2CFAU, 1U, 252U);
        checkpoint.program_counter = 0x5778U;
        replacement.output_registers = checkpoint;
        append_checkpoint(replacement, 0x5775U, 0x5778U, 252U, checkpoint);
        return replacement;
    }
    append_checkpoint(replacement, 0x573DU, 0x5743U, 33U, checkpoint);
    checkpoint.accumulator = helper.phase;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x5743U, 0x5746U, 48U, checkpoint);
    append_checkpoint(replacement, 0x5746U, 0x5748U, 54U, checkpoint);
    checkpoint.accumulator = helper.next_phase;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x5748U, 0x5749U, 60U, checkpoint);
    add_write(replacement, 0x5749U,
              static_cast<std::uint16_t>(0x3B00U + helper.slot),
              helper.next_phase, 75U);
    append_checkpoint(replacement, 0x5749U, 0x574CU, 75U, checkpoint);
    checkpoint.accumulator = static_cast<std::uint8_t>(helper.next_phase << 1U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x574CU, 0x574DU, 81U, checkpoint);
    checkpoint.y = checkpoint.accumulator;
    append_checkpoint(replacement, 0x574DU, 0x574EU, 87U, checkpoint);
    checkpoint.accumulator = horizontal_velocity;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x574EU, 0x5751U, 102U, checkpoint);
    add_write(replacement, 0x5751U, 0x2D00U, horizontal_velocity, 117U);
    append_checkpoint(replacement, 0x5751U, 0x5754U, 117U, checkpoint);
    checkpoint.accumulator = 0x00U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x5754U, 0x5757U, 132U, checkpoint);
    add_write(replacement, 0x5757U, 0x2D01U, 0x00U, 147U);
    append_checkpoint(replacement, 0x5757U, 0x575AU, 147U, checkpoint);
    checkpoint.y = helper.next_phase;
    append_checkpoint(replacement, 0x575AU, 0x575DU, 162U, checkpoint);
    checkpoint.accumulator = cadence;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x575DU, 0x5760U, 177U, checkpoint);
    add_write(replacement, 0x5760U,
              static_cast<std::uint16_t>(0x3A00U + helper.slot),
              cadence, 192U);
    append_checkpoint(replacement, 0x5760U, 0x5763U, 192U, checkpoint);
    add_write(replacement, 0x5763U, 0x2D05U, cadence, 207U);
    append_checkpoint(replacement, 0x5763U, 0x5766U, 207U, checkpoint);
    append_checkpoint(replacement, 0x5766U, 0x5768U, 213U, checkpoint);
    add_write(replacement, 0x5768U, 0x2CFAU, 0U, 228U);
    checkpoint.program_counter = 0x576BU;
    replacement.output_registers = checkpoint;
    append_checkpoint(replacement, 0x5768U, 0x576BU, 228U, checkpoint);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_big_star_rotation_wait(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x5674U,
    };
    constexpr std::uint32_t kBodyCycles = 222U;
    if (entry.routine != kRoutine || (entry.registers.status & 0x20U) != 0U
        || entry.master_cycles_until_user_event <= kBodyCycles
        || entry.master_cycles_until_timer_interrupt <= kBodyCycles) {
        return std::nullopt;
    }
    const auto stack_return = proven_stack_return(entry, 0xFBU);
    if (!stack_return) {
        return std::nullopt;
    }

    dc_big_star_rotation_wait rotation{};
    if (!dc_classify_big_star_rotation_wait(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &rotation)
        || rotation.path != DC_BIG_STAR_ROTATION_WAIT_STABLE) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & ~0x82U);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };

    auto output = entry.registers;
    output.program_counter = 0x56ABU;
    output.accumulator = rotation.cleared_session_flags;
    output.status = set_nz(output.status, output.accumulator);

    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x56ABU},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    replacement.work_ram_writes = {
        {
            .instruction = {0x02U, 0x5674U},
            .logical_address = stack_return->high,
            .value = 0x56U,
            .master_cycle_offset = 0U,
        },
        {
            .instruction = {0x02U, 0x5674U},
            .logical_address = stack_return->low,
            .value = 0x76U,
            .master_cycle_offset = 0U,
        },
        {
            .instruction = {0x02U, 0x571EU},
            .logical_address = 0x2D04U,
            .value = 0U,
            .master_cycle_offset = 102U,
        },
        {
            .instruction = {0x02U, 0x567FU},
            .logical_address = static_cast<std::uint16_t>(
                0x3A00U + rotation.slot),
            .value = rotation.decremented_timer,
            .master_cycle_offset = 180U,
        },
        {
            .instruction = {0x02U, 0x56A9U},
            .logical_address = 0x20CBU,
            .value = rotation.cleared_session_flags,
            .master_cycle_offset = 222U,
        },
    };
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_devil_shooter_companion_dormant(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kLeftRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x53DDU,
    };
    constexpr RoutineKey kRightRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x5456U,
    };
    if ((entry.routine != kLeftRoutine && entry.routine != kRightRoutine)
        || (entry.registers.status & 0x20U) != 0U
        || entry.master_cycles_until_user_event <= 24U
        || entry.master_cycles_until_timer_interrupt <= 24U) {
        return std::nullopt;
    }

    const auto slot = static_cast<std::uint8_t>(
        entry.routine == kLeftRoutine ? 0x0AU : 0x0DU);
    dc_devil_shooter_companion_dormant companion{};
    if (!dc_classify_devil_shooter_companion_dormant(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            slot,
            &companion)) {
        throw std::runtime_error(
            "Native Devil-Shooter companion replacement rejected the "
            "work-RAM snapshot.");
    }
    if (companion.path != DC_DEVIL_SHOOTER_COMPANION_DORMANT_IDLE) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto return_pc = static_cast<std::uint16_t>(
        entry.routine == kLeftRoutine ? 0x53E3U : 0x545CU);

    auto checkpoint = entry.registers;
    checkpoint.accumulator = companion.session_flags;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    auto output = checkpoint;
    output.accumulator = companion.active_session_bits;
    output.status = set_nz(output.status, output.accumulator);
    output.program_counter = return_pc;

    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = return_pc},
        .output_registers = output,
        .body_master_cycles = 24U,
    };
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_devil_shooter_companion_position(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x5397U,
    };
    if (entry.routine != kRoutine || (entry.registers.status & 0x20U) != 0U
        || entry.master_cycles_until_user_event <= 75U
        || entry.master_cycles_until_timer_interrupt <= 75U) {
        return std::nullopt;
    }

    dc_devil_shooter_companion_position position{};
    if (!dc_classify_devil_shooter_companion_position(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &position)) {
        throw std::runtime_error(
            "Native Devil-Shooter charge-position replacement rejected the "
            "work-RAM snapshot.");
    }

    const auto adc_status = [](
                                std::uint8_t status,
                                const std::uint8_t left,
                                const std::uint8_t right,
                                const bool carry_in,
                                const std::uint8_t result) {
        const auto sum = static_cast<std::uint16_t>(left)
            + static_cast<std::uint16_t>(right)
            + static_cast<std::uint16_t>(carry_in);
        status = static_cast<std::uint8_t>(status & ~0xC3U);
        if (result == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((result & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        if (sum > 0xFFU) {
            status = static_cast<std::uint8_t>(status | 0x01U);
        }
        if (((~(left ^ right) & (left ^ result)) & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x40U);
        }
        return status;
    };

    const auto output_status = adc_status(
        entry.registers.status,
        0U,
        0x02U,
        position.low_add_carry != 0U,
        position.vertical_high);
    auto output = entry.registers;
    output.accumulator = position.vertical_high;
    output.status = output_status;
    output.program_counter = 0x53A7U;

    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x53A7U},
        .output_registers = output,
        .body_master_cycles = 75U,
    };
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x539EU},
        .logical_address = static_cast<std::uint16_t>(0x3300U + position.slot),
        .value = position.vertical_low,
        .master_cycle_offset = 48U,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x53A4U},
        .logical_address = static_cast<std::uint16_t>(0x3380U + position.slot),
        .value = position.vertical_high,
        .master_cycle_offset = 75U,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_devil_shooter_charge_position(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x52E3U,
    };
    if (entry.routine != kRoutine || (entry.registers.status & 0x20U) != 0U) {
        return std::nullopt;
    }

    dc_devil_shooter_charge_position position{};
    if (!dc_classify_devil_shooter_charge_position(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &position)) {
        return std::nullopt;
    }

    const std::uint32_t body_cycles = position.appearance < 3U
        ? 243U + static_cast<std::uint32_t>(position.appearance) * 18U
        : 297U;
    if (entry.master_cycles_until_user_event <= body_cycles
        || entry.master_cycles_until_timer_interrupt <= body_cycles) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.accumulator = position.appearance;
    output.y = position.appearance;
    output.status = static_cast<std::uint8_t>(entry.registers.status & ~0xC3U);
    if (position.subtract_overflow != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x40U);
    }
    if (position.appearance == 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);
    }
    if (position.appearance == 4U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x01U);
    }
    output.program_counter = 0x533AU;

    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x533AU},
        .output_registers = output,
        .body_master_cycles = body_cycles,
    };
    replacement.work_ram_writes = {
        {
            .instruction = {0x02U, 0x52E9U},
            .logical_address = static_cast<std::uint16_t>(0x3280U + position.slot),
            .value = position.vertical_low,
            .master_cycle_offset = 42U,
        },
        {
            .instruction = {0x02U, 0x52F1U},
            .logical_address = static_cast<std::uint16_t>(0x3300U + position.slot),
            .value = position.vertical_middle,
            .master_cycle_offset = 78U,
        },
        {
            .instruction = {0x02U, 0x52F8U},
            .logical_address = static_cast<std::uint16_t>(0x3380U + position.slot),
            .value = position.vertical_high,
            .master_cycle_offset = 114U,
        },
        {
            .instruction = {0x02U, 0x5302U},
            .logical_address = 0x2D15U,
            .value = position.charge,
            .master_cycle_offset = 165U,
        },
        {
            .instruction = {0x02U, 0x5337U},
            .logical_address = static_cast<std::uint16_t>(0x3980U + position.slot),
            .value = position.appearance,
            .master_cycle_offset = body_cycles,
        },
    };
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_devil_shooter_companion_appearance(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x5408U,
    };
    if (entry.routine != kRoutine || (entry.registers.status & 0x20U) != 0U) {
        return std::nullopt;
    }

    dc_devil_shooter_companion_appearance appearance{};
    if (!dc_classify_devil_shooter_companion_appearance(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &appearance)) {
        return std::nullopt;
    }

    const auto cmp_status = [](
                                std::uint8_t status,
                                const std::uint8_t left,
                                const std::uint8_t right) {
        const auto difference = static_cast<std::uint8_t>(left - right);
        status = static_cast<std::uint8_t>(status & ~0x83U);
        if (difference == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        }
        if ((difference & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        if (left >= right) {
            status = static_cast<std::uint8_t>(status | 0x01U);
        }
        return status;
    };

    const bool is_idle = appearance.path
        == DC_DEVIL_SHOOTER_COMPANION_APPEARANCE_IDLE;
    const bool is_reset = appearance.path
        == DC_DEVIL_SHOOTER_COMPANION_APPEARANCE_RESET;
    const std::uint32_t body_cycles = is_idle ? 42U : is_reset ? 114U : 105U;
    if (entry.master_cycles_until_user_event <= body_cycles
        || entry.master_cycles_until_timer_interrupt <= body_cycles) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0x5420U;
    if (is_idle) {
        output.accumulator = appearance.charge;
        output.status = cmp_status(
            entry.registers.status,
            appearance.charge,
            appearance.cached_charge);
    } else {
        output.accumulator = appearance.advanced_appearance;
        output.status = cmp_status(
            entry.registers.status,
            appearance.advanced_appearance,
            0x03U);
    }

    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x5420U},
        .output_registers = output,
        .body_master_cycles = body_cycles,
    };
    if (!is_idle) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x02U, 0x5410U},
            .logical_address = static_cast<std::uint16_t>(0x3E00U + appearance.slot),
            .value = appearance.charge,
            .master_cycle_offset = 48U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x02U, 0x5413U},
            .logical_address = static_cast<std::uint16_t>(0x3980U + appearance.slot),
            .value = appearance.advanced_appearance,
            .master_cycle_offset = 75U,
        });
        if (is_reset) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x02U, 0x541DU},
                .logical_address = static_cast<std::uint16_t>(0x3980U + appearance.slot),
                .value = 0U,
                .master_cycle_offset = 114U,
            });
        }
    }
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_devil_shooter_companion_active_timer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kLeftRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x53E4U,
    };
    constexpr RoutineKey kRightRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x545DU,
    };
    if ((entry.routine != kLeftRoutine && entry.routine != kRightRoutine)
        || (entry.registers.status & 0x20U) != 0U) {
        return std::nullopt;
    }

    const auto expected_slot = static_cast<std::uint8_t>(
        entry.routine == kLeftRoutine ? 0x0AU : 0x0DU);
    if (entry.registers.x != expected_slot) {
        return std::nullopt;
    }

    dc_devil_shooter_companion_active_timer timer{};
    if (!dc_classify_devil_shooter_companion_active_timer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &timer)) {
        return std::nullopt;
    }

    const bool is_wait = timer.path
        == DC_DEVIL_SHOOTER_COMPANION_ACTIVE_TIMER_WAIT;
    const bool is_reset = timer.path
        == DC_DEVIL_SHOOTER_COMPANION_ACTIVE_TIMER_RESET;
    const std::uint32_t body_cycles = is_wait ? 33U : is_reset ? 111U : 102U;
    if (entry.master_cycles_until_user_event <= body_cycles
        || entry.master_cycles_until_timer_interrupt <= body_cycles) {
        return std::nullopt;
    }

    const auto nz_status = [](
                               std::uint8_t status,
                               const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & ~0x82U);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto cmp_status = [](
                                std::uint8_t status,
                                const std::uint8_t left,
                                const std::uint8_t right) {
        const auto difference = static_cast<std::uint8_t>(left - right);
        status = static_cast<std::uint8_t>(status & ~0x83U);
        if (difference == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        }
        if ((difference & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        if (left >= right) {
            status = static_cast<std::uint8_t>(status | 0x01U);
        }
        return status;
    };

    auto output = entry.registers;
    const auto return_pc = static_cast<std::uint16_t>(
        entry.routine == kLeftRoutine ? 0x53FBU : 0x5474U);
    const auto terminal_appearance = static_cast<std::uint8_t>(
        entry.routine == kLeftRoutine ? 0x04U : 0x05U);
    output.program_counter = return_pc;
    output.accumulator = is_wait ? entry.registers.accumulator
        : timer.advanced_appearance;
    output.status = is_wait
        ? nz_status(entry.registers.status, timer.decremented_timer)
        : cmp_status(
            entry.registers.status,
            timer.advanced_appearance,
            terminal_appearance);

    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = return_pc},
        .output_registers = output,
        .body_master_cycles = body_cycles,
    };
    replacement.work_ram_writes.push_back({
        .instruction = {
            0x02U,
            entry.routine == kLeftRoutine ? 0x53E4U : 0x545DU,
        },
        .logical_address = static_cast<std::uint16_t>(0x3B00U + timer.slot),
        .value = timer.decremented_timer,
        .master_cycle_offset = 21U,
    });
    if (!is_wait) {
        replacement.work_ram_writes.push_back({
            .instruction = {
                0x02U,
                entry.routine == kLeftRoutine ? 0x53EBU : 0x5464U,
            },
            .logical_address = static_cast<std::uint16_t>(0x3B00U + timer.slot),
            .value = 0x08U,
            .master_cycle_offset = 48U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {
                0x02U,
                entry.routine == kLeftRoutine ? 0x53EEU : 0x5467U,
            },
            .logical_address = static_cast<std::uint16_t>(0x3980U + timer.slot),
            .value = timer.advanced_appearance,
            .master_cycle_offset = 69U,
        });
        if (is_reset) {
            replacement.work_ram_writes.push_back({
                .instruction = {
                    0x02U,
                    entry.routine == kLeftRoutine ? 0x53F8U : 0x5471U,
                },
                .logical_address = static_cast<std::uint16_t>(0x3980U + timer.slot),
                .value = 0U,
                .master_cycle_offset = 111U,
            });
        }
    }
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_devil_shooter_companion_active_controller(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kLeftRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x53C8U,
    };
    constexpr RoutineKey kRightRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x5441U,
    };
    constexpr std::uint32_t kPreludeAndTailCycles = 66U;
    if ((entry.routine != kLeftRoutine && entry.routine != kRightRoutine)
        || entry.registers.x != (entry.routine == kLeftRoutine ? 0x0AU : 0x0DU)
        || (entry.registers.status & 0x20U) != 0U || work_ram.size() <= 0x53U
        || (std::to_integer<std::uint8_t>(work_ram[0x53U]) & 0x09U) == 0U) {
        return std::nullopt;
    }

    // These parents are the normal active paths: JSR timer, then the $53 flag
    // remains nonzero and immediately returns.  Keep its child and RTS
    // entirely inside the same no-interrupt window.
    auto child_entry = entry;
    const auto is_left = entry.routine == kLeftRoutine;
    const auto child_pc = static_cast<std::uint16_t>(is_left ? 0x53E4U : 0x545DU);
    child_entry.routine = {.physical_bank = 0x02U, .logical_pc = child_pc};
    child_entry.registers.program_counter = child_pc;
    child_entry.registers.stack_pointer = static_cast<std::uint8_t>(
        entry.registers.stack_pointer - 2U);
    const auto child = replace_devil_shooter_companion_active_timer(
        child_entry,
        work_ram);
    if (!child.has_value()) {
        return std::nullopt;
    }

    const auto body_cycles = child->body_master_cycles + kPreludeAndTailCycles;
    if (entry.master_cycles_until_user_event <= body_cycles
        || entry.master_cycles_until_timer_interrupt <= body_cycles) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & ~0x82U);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };

    auto output = child->output_registers;
    const auto return_pc = static_cast<std::uint16_t>(
        is_left ? 0x53D1U : 0x544AU);
    output.program_counter = return_pc;
    output.stack_pointer = entry.registers.stack_pointer;
    output.accumulator = static_cast<std::uint8_t>(
        std::to_integer<std::uint8_t>(work_ram[0x53U]) & 0x09U);
    output.status = set_nz(output.status, output.accumulator);

    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = return_pc},
        .output_registers = output,
        .body_master_cycles = body_cycles,
    };
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, entry.routine.logical_pc},
        .logical_address = static_cast<std::uint16_t>(
            0x2100U + entry.registers.stack_pointer),
        .value = is_left ? 0x53U : 0x54U,
        .master_cycle_offset = 6U,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, entry.routine.logical_pc},
        .logical_address = static_cast<std::uint16_t>(
            0x2100U + static_cast<std::uint8_t>(
                          entry.registers.stack_pointer - 1U)),
        .value = is_left ? 0xCAU : 0x43U,
        .master_cycle_offset = 6U,
    });
    for (auto write : child->work_ram_writes) {
        write.master_cycle_offset += 21U;
        replacement.work_ram_writes.push_back(write);
    }
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
replace_walled_star_post_destruction_idle(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // Once its wall is gone, the Walled Star waits for either access latch.
    // Keep the active acknowledgement, animation, and bonus routing in
    // assembly; this prefix only covers the two-latch idle return.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x638FU,
    };
    constexpr std::uint8_t kWalledStarSlot = 0x5BU;
    constexpr std::size_t kAccessLatchOffset = 0x0D53U;
    constexpr std::size_t kFallbackAccessLatchOffset = 0x0D32U;
    if (entry.routine != kRoutine || entry.registers.x != kWalledStarSlot
        || work_ram.size() <= kAccessLatchOffset
        || work_ram.size() <= kFallbackAccessLatchOffset
        || std::to_integer<std::uint8_t>(work_ram[kAccessLatchOffset]) != 0U
        || std::to_integer<std::uint8_t>(
               work_ram[kFallbackAccessLatchOffset])
            != 0U) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };

    auto output = entry.registers;
    output.accumulator = 0U;
    output.status = set_nz(output.status, output.accumulator);
    output.program_counter = 0x6399U;

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x6399U,
        },
        .output_registers = output,
        .body_master_cycles = 42U,
    };
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
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };
    append_checkpoint(0x638FU, 0x6392U, 15U, output);
    append_checkpoint(0x6392U, 0x6394U, 21U, output);
    append_checkpoint(0x6394U, 0x6397U, 36U, output);
    append_checkpoint(0x6397U, 0x6399U, 42U, output);
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
replace_tower_bonus_terminal(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // This is the armed final pass through the Tower's local controller. The
    // 612B scheduler remains assembly-owned. Its installed 6136 continuation
    // has its own no-hit adapter so that its independent IRQ boundary remains
    // explicit; its sign-bit prefix stops at the shared C021 dispatcher.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x60F9U,
    };
    if (entry.routine != kRoutine || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_tower_bonus_terminal terminal{};
    if (!dc_classify_tower_bonus_terminal(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &terminal)) {
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
                                       const std::uint8_t bank,
                                       const std::uint16_t instruction_pc,
                                       const std::uint16_t resume_pc,
                                       const std::uint32_t master_cycle,
                                       auto registers) {
        registers.program_counter = resume_pc;
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = master_cycle - 3U,
            .master_cycle_offset = master_cycle,
            .instruction = {bank, instruction_pc},
            .registers = registers,
        });
    };

    RoutineReplacement replacement{};
    auto checkpoint = entry.registers;
    checkpoint.accumulator = terminal.qualifying_latch;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x03U, 0x60F9U, 0x60FCU, 15U, checkpoint);
    append_checkpoint(replacement, 0x03U, 0x60FCU, 0x60FEU, 21U, checkpoint);

    const auto decremented_gate =
        static_cast<std::uint8_t>(terminal.gate_counter - 1U);
    checkpoint.status = set_nz(checkpoint.status, decremented_gate);
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x60FEU},
        .logical_address = 0x2D4DU,
        .value = decremented_gate,
        .master_cycle_offset = 42U,
    });
    append_checkpoint(replacement, 0x03U, 0x60FEU, 0x6101U, 42U, checkpoint);
    append_checkpoint(replacement, 0x03U, 0x6101U, 0x612BU, 54U, checkpoint);
    replacement.return_instruction = {.physical_bank = 0x03U, .logical_pc = 0x612BU};
    checkpoint.program_counter = 0x612BU;
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = 54U;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_tower_bonus_continuation(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x6136U,
    };
    if (entry.routine != kRoutine || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_tower_bonus_continuation continuation{};
    if (!dc_classify_tower_bonus_continuation(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &continuation)) {
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
                                       const std::uint32_t master_cycle,
                                       auto registers) {
        registers.program_counter = resume_pc;
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = master_cycle - 3U,
            .master_cycle_offset = master_cycle,
            .instruction = {0x03U, instruction_pc},
            .registers = registers,
        });
    };

    if ((continuation.collision_state & 0x80U) != 0U) {
        // A genuine Tower collision reaches this stage-selection prefix. The
        // shared C021 dispatcher remains emulated, including its timer,
        // audio, and scene-transition work.
        RoutineReplacement replacement{
            .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC021U},
            .body_master_cycles = 75U,
        };
        auto checkpoint = entry.registers;
        checkpoint.accumulator = continuation.collision_state;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x6136U, 0x6139U, 15U, checkpoint);
        append_checkpoint(replacement, 0x6139U, 0x613CU, 27U, checkpoint);

        checkpoint.accumulator = 0x01U;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x613CU, 0x613EU, 33U, checkpoint);
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x613EU},
            .logical_address = 0x3C80U,
            .value = checkpoint.accumulator,
            .master_cycle_offset = 48U,
        });
        append_checkpoint(replacement, 0x613EU, 0x6141U, 48U, checkpoint);

        checkpoint.accumulator = 0x10U;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x6141U, 0x6143U, 54U, checkpoint);
        const auto stack_high = checkpoint.stack_pointer;
        const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x6143U},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_high),
            .value = 0x61U,
            .master_cycle_offset = 54U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x6143U},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_low),
            .value = 0x45U,
            .master_cycle_offset = 54U,
        });
        checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
        checkpoint.program_counter = 0xC021U;
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = 72U,
            .master_cycle_offset = 75U,
            .instruction = {0x03U, 0x6143U},
            .registers = checkpoint,
        });
        replacement.output_registers = checkpoint;
        return replacement;
    }

    auto output = entry.registers;
    output.program_counter = 0x613BU;
    output.accumulator = continuation.collision_state;
    output.status = set_nz(output.status, output.accumulator);
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x03U, .logical_pc = 0x613BU},
        .output_registers = output,
        .body_master_cycles = 21U,
    };
    // The frame-3413 timer IRQ saves PC=6139, immediately after the indexed
    // load, so this checkpoint must precede the branch checkpoint.
    append_checkpoint(replacement, 0x6136U, 0x6139U, 15U, output);
    append_checkpoint(replacement, 0x6139U, 0x613BU, 21U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_hit_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x73D8U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_knight_hit_gate gate{};
    if (!dc_classify_knight_hit_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &gate)) {
        throw std::runtime_error(
            "Native 73D8 replacement rejected the work-RAM snapshot.");
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

    if (gate.path == DC_KNIGHT_HIT_GATE_TRIGGERED) {
        // The triggered path's bounded prefix clears the contact latch and
        // decrements the four-hit counter.  Its following 7BF6 helper owns
        // collision response, audio, score, and any intervening IRQs, so
        // preserve that larger transaction in assembly.
        const auto counter_offset = static_cast<std::size_t>(
            0x1C00U + gate.slot);
        if (work_ram.size() <= counter_offset) {
            return std::nullopt;
        }
        const auto counter = std::to_integer<std::uint8_t>(
            work_ram[counter_offset]);
        const auto decremented = static_cast<std::uint8_t>(counter - 1U);
        // A zero counter is not a valid live Knight.  The terminal fourth
        // hit itself has a bounded prefix, however: it clears the contact
        // latch, decrements to zero, then returns carry to the movement
        // parent.  The parent deliberately does not compose this carry-set
        // child, leaving the larger destruction state machine in assembly.
        if (counter == 0U) {
            return std::nullopt;
        }

        auto checkpoint = entry.registers;
        checkpoint.accumulator = gate.collision_state;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);

        RoutineReplacement replacement{};
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
                    .instruction = {0x03U, instruction_pc},
                    .registers = registers,
                });
            };
        append_checkpoint(0x73D8U, 0x73DBU, 15U, checkpoint);
        append_checkpoint(0x73DBU, 0x73DDU, 21U, checkpoint);

        checkpoint.accumulator = static_cast<std::uint8_t>(
            checkpoint.accumulator & 0x7FU);
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0x73DDU, 0x73DFU, 27U, checkpoint);

        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x73DFU},
            .logical_address = static_cast<std::uint16_t>(0x3800U + gate.slot),
            .value = checkpoint.accumulator,
            .master_cycle_offset = 42U,
        });
        append_checkpoint(0x73DFU, 0x73E2U, 42U, checkpoint);

        checkpoint.status = set_nz(checkpoint.status, decremented);
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x73E2U},
            .logical_address = static_cast<std::uint16_t>(0x3C00U + gate.slot),
            .value = decremented,
            .master_cycle_offset = 63U,
        });
        append_checkpoint(0x73E2U, 0x73E5U, 63U, checkpoint);
        append_checkpoint(0x73E5U, 0x73E9U, 72U, checkpoint);

        if (decremented == 0U) {
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status | 0x01U);
            append_checkpoint(0x73E7U, 0x73E8U, 75U, checkpoint);
            checkpoint.program_counter = 0x73E8U;
            replacement.return_instruction = {
                .physical_bank = 0x03U,
                .logical_pc = 0x73E8U,
            };
            replacement.output_registers = checkpoint;
            replacement.body_master_cycles = 75U;
            return replacement;
        }

        const auto stack_high = checkpoint.stack_pointer;
        const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x73E9U},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_high),
            .value = 0x73U,
            .master_cycle_offset = 75U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x73E9U},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_low),
            .value = 0xEBU,
            .master_cycle_offset = 75U,
        });
        checkpoint.stack_pointer = static_cast<std::uint8_t>(
            checkpoint.stack_pointer - 2U);
        append_checkpoint(0x73E9U, 0x7BF6U, 96U, checkpoint);

        checkpoint.program_counter = 0x7BF6U;
        replacement.return_instruction = {.physical_bank = 0x03U, .logical_pc = 0x7BF6U};
        replacement.output_registers = checkpoint;
        replacement.body_master_cycles = 96U;
        return replacement;
    }
    if (gate.path != DC_KNIGHT_HIT_GATE_IDLE) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.accumulator = gate.collision_state;
    output.status = set_nz(output.status, output.accumulator);
    auto return_output = output;
    return_output.status = static_cast<std::uint8_t>(
        return_output.status & 0xFEU);
    return_output.program_counter = 0x73FDU;

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x73FDU,
        },
        .output_registers = return_output,
        .body_master_cycles = 33U,
    };
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
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };
    append_checkpoint(0x73D8U, 0x73DBU, 15U, output);
    append_checkpoint(0x73DBU, 0x73FCU, 27U, output);
    append_checkpoint(0x73FCU, 0x73FDU, 33U, return_output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_score_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // This is the carry-set successor of the terminal fourth hit. It only
    // prepares the 100-unit award and enters the existing banked score helper;
    // score accumulation and the following release/acknowledgement state stay
    // assembly-owned.
    constexpr RoutineKey kRoutine{.physical_bank = 0x03U, .logical_pc = 0x73FEU};
    if (entry.routine != kRoutine || entry.registers.x < 0x3AU
        || entry.registers.x > 0x3DU || entry.registers.accumulator != 0U
        || work_ram.size() < 0x0200U || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
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
            .instruction = {0x03U, instruction_pc},
            .registers = registers,
        });
    };

    const auto stack_high = entry.registers.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
    auto checkpoint = entry.registers;
    checkpoint.accumulator = 0x64U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC02AU},
        .body_master_cycles = 51U,
    };
    append_checkpoint(replacement, 0x73FEU, 0x7400U, 6U, checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x7400U},
        .logical_address = 0x202FU,
        .value = 0x64U,
        .master_cycle_offset = 18U,
    });
    append_checkpoint(replacement, 0x7400U, 0x7402U, 18U, checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x7402U},
        .logical_address = 0x2030U,
        .value = 0x00U,
        .master_cycle_offset = 30U,
    });
    append_checkpoint(replacement, 0x7402U, 0x7404U, 30U, checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x7404U},
        .logical_address = static_cast<std::uint16_t>(0x2100U + stack_high),
        .value = 0x74U,
        .master_cycle_offset = 30U,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x7404U},
        .logical_address = static_cast<std::uint16_t>(0x2100U + stack_low),
        .value = 0x06U,
        .master_cycle_offset = 30U,
    });
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    checkpoint.program_counter = 0xC02AU;
    replacement.output_registers = checkpoint;
    append_checkpoint(replacement, 0x7404U, 0xC02AU, 51U, checkpoint);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_acknowledgement_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The common release helper leaves the terminal Knight here. A four-byte
    // acknowledgement prefix sets bit one in its queue channel before the
    // original release helper begins; retain that helper in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x03U, .logical_pc = 0x740AU};
    constexpr std::size_t kQueueIndexOffset = 0x1B00U;
    constexpr std::size_t kChannelOffset = 0x0D06U;
    if (entry.routine != kRoutine || entry.registers.x < 0x3AU
        || entry.registers.x > 0x3DU || entry.registers.accumulator != 0x01U
        || work_ram.size() <= kQueueIndexOffset + entry.registers.x) {
        return std::nullopt;
    }

    const auto queue_index = std::to_integer<std::uint8_t>(
        work_ram[kQueueIndexOffset + entry.registers.x]);
    if (queue_index > 0x03U || work_ram.size() <= kChannelOffset + queue_index) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
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
            .instruction = {0x03U, instruction_pc},
            .registers = registers,
        });
    };

    auto checkpoint = entry.registers;
    checkpoint.y = queue_index;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x03U, .logical_pc = 0x7415U},
        .body_master_cycles = 51U,
    };
    append_checkpoint(replacement, 0x740AU, 0x740DU, 15U, checkpoint);
    checkpoint.accumulator = std::to_integer<std::uint8_t>(
        work_ram[kChannelOffset + checkpoint.y]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x740DU, 0x7410U, 30U, checkpoint);
    checkpoint.accumulator = static_cast<std::uint8_t>(checkpoint.accumulator | 0x02U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x7410U, 0x7412U, 36U, checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x7412U},
        .logical_address = static_cast<std::uint16_t>(0x2D06U + checkpoint.y),
        .value = checkpoint.accumulator,
        .master_cycle_offset = 51U,
    });
    checkpoint.program_counter = 0x7415U;
    replacement.output_registers = checkpoint;
    append_checkpoint(replacement, 0x7412U, 0x7415U, 51U, checkpoint);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The terminal Knight enters the broad shared release helper through a
    // small unconditional scratch-clear prologue. Keep the helper's later
    // actor, coroutine, and interrupt-bearing work in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x41A0U};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x02U
        || entry.registers.x != 0x3AU || entry.registers.y != 0U
        || entry.registers.stack_pointer != 0xF9U || entry.registers.status != 0U
        || work_ram.size() <= 0x0038U) {
        return std::nullopt;
    }

    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x41A8U},
        .output_registers = entry.registers,
        .body_master_cycles = 48U,
    };
    const auto append_checkpoint = [&replacement](const std::uint16_t instruction_pc,
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
    struct ClearWrite {
        std::uint16_t instruction;
        std::uint16_t offset;
        std::uint32_t cycle;
    };
    constexpr std::array<ClearWrite, 4U> kClearWrites{{
        {0x41A0U, 0x0035U, 12U},
        {0x41A2U, 0x0036U, 24U},
        {0x41A4U, 0x0037U, 36U},
        {0x41A6U, 0x0038U, 48U},
    }};
    for (const auto [instruction, offset, cycle] : kClearWrites) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x02U, instruction},
            .logical_address = static_cast<std::uint16_t>(0x2000U + offset),
            .value = 0U,
            .master_cycle_offset = cycle,
        });
        append_checkpoint(instruction, static_cast<std::uint16_t>(instruction + 2U),
                          cycle, entry.registers);
    }
    replacement.output_registers.program_counter = 0x41A8U;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_setup_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // Continue the terminal-only release-helper decomposition with the
    // unconditional command setup before the shared helper's stack and table
    // work begins.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x41A8U};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x02U
        || entry.registers.x != 0x3AU || entry.registers.y != 0U
        || entry.registers.stack_pointer != 0xF9U || entry.registers.status != 0U
        || work_ram.size() <= 0x0D6EU) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    auto output = entry.registers;
    output.accumulator = 0x2DU;
    output.status = set_nz(output.status, output.accumulator);
    output.program_counter = 0x41ADU;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x41ADU},
        .output_registers = output,
        .body_master_cycles = 21U,
        .work_ram_writes = {{
            .instruction = {0x02U, 0x41AAU},
            .logical_address = 0x2D6EU,
            .value = 0x2DU,
            .master_cycle_offset = 21U,
        }},
    };
    auto checkpoint = output;
    checkpoint.program_counter = 0x41AAU;
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 3U,
        .master_cycle_offset = 6U,
        .instruction = {0x02U, 0x41A8U},
        .registers = checkpoint,
    });
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 18U,
        .master_cycle_offset = 21U,
        .instruction = {0x02U, 0x41AAU},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_table_setup_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The terminal helper next saves its zero Y index and turns the cleared
    // $35:$36 scratch pair into the table coordinates consumed by $41F5.
    // Keep that shared helper and every later actor/coroutine path in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x41ADU};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x2DU
        || entry.registers.x != 0x3AU || entry.registers.y != 0U
        || entry.registers.stack_pointer != 0xF9U || entry.registers.status != 0U
        || work_ram.size() <= 0x0036U || work_ram[0x0035U] != std::byte{0}
        || work_ram[0x0036U] != std::byte{0}) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    auto checkpoint = entry.registers;
    checkpoint.status = set_nz(checkpoint.status, 0U);
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x41C2U},
        .output_registers = checkpoint,
        .body_master_cycles = 123U,
    };
    const auto append_checkpoint = [&replacement](const std::uint16_t instruction_pc,
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
    append_checkpoint(0x41ADU, 0x41AEU, 6U, checkpoint);
    checkpoint.stack_pointer = 0xF8U;
    append_checkpoint(0x41AEU, 0x41AFU, 15U, checkpoint);
    checkpoint.accumulator = 0U;
    append_checkpoint(0x41AFU, 0x41B0U, 21U, checkpoint);
    append_checkpoint(0x41B0U, 0x41B1U, 27U, checkpoint);
    append_checkpoint(0x41B1U, 0x41B2U, 33U, checkpoint);
    append_checkpoint(0x41B2U, 0x41B3U, 39U, checkpoint);
    append_checkpoint(0x41B3U, 0x41B5U, 48U, checkpoint);
    append_checkpoint(0x41B5U, 0x41B8U, 69U, checkpoint);
    append_checkpoint(0x41B8U, 0x41BAU, 78U, checkpoint);
    append_checkpoint(0x41BAU, 0x41BBU, 84U, checkpoint);
    append_checkpoint(0x41BBU, 0x41BDU, 93U, checkpoint);
    append_checkpoint(0x41BDU, 0x41C0U, 114U, checkpoint);
    append_checkpoint(0x41C0U, 0x41C2U, 123U, checkpoint);
    replacement.work_ram_writes = {{
        {
            .instruction = {0x02U, 0x41AEU},
            .logical_address = 0x21F9U,
            .value = 0U,
            .master_cycle_offset = 15U,
        },
        {
            .instruction = {0x02U, 0x41B8U},
            .logical_address = 0x202BU,
            .value = 0U,
            .master_cycle_offset = 78U,
        },
        {
            .instruction = {0x02U, 0x41C0U},
            .logical_address = 0x202CU,
            .value = 0U,
            .master_cycle_offset = 123U,
        },
    }};
    checkpoint.program_counter = 0x41C2U;
    replacement.output_registers = checkpoint;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_call_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The terminal-only coordinate setup reaches the broad shared $41F5
    // helper through one fixed JSR frame. Keep the child and its interrupt-
    // bearing actor/coroutine work in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x41C2U};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x3AU || entry.registers.y != 0U
        || entry.registers.stack_pointer != 0xF8U || entry.registers.status != 0x02U
        || work_ram.size() <= 0x002CU) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.stack_pointer = 0xF6U;
    output.program_counter = 0x41F5U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x41F5U},
        .output_registers = output,
        .body_master_cycles = 21U,
        .work_ram_writes = {{
            {
                .instruction = {0x02U, 0x41C2U},
                .logical_address = 0x21F8U,
                .value = 0x41U,
                .master_cycle_offset = 0U,
            },
            {
                .instruction = {0x02U, 0x41C2U},
                .logical_address = 0x21F7U,
                .value = 0xC4U,
                .master_cycle_offset = 0U,
            },
        }},
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 18U,
        .master_cycle_offset = 21U,
        .instruction = {0x02U, 0x41C2U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_pointer_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // The terminal-only child entry starts by copying the fixed table pointer
    // through $2D:$2E. Keep its shared call and later actor/coroutine work in
    // assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x41F5U};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x3AU || entry.registers.y != 0U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x02U) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    // This terminal context reaches the fixed $42E1 table. The HuC6280
    // direct-page source is not represented by this work-RAM view, so retain
    // its observed terminal values rather than reading an unrelated mirror.
    constexpr std::uint8_t pointer_low = 0xE1U;
    constexpr std::uint8_t pointer_high = 0x42U;
    auto checkpoint = entry.registers;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x420BU},
        .output_registers = checkpoint,
        .body_master_cycles = 48U,
    };
    const auto append_checkpoint = [&replacement](const std::uint16_t instruction_pc,
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
    checkpoint.accumulator = pointer_low;
    checkpoint.status = set_nz(checkpoint.status, pointer_low);
    append_checkpoint(0x41F5U, 0x41F7U, 9U, checkpoint);
    append_checkpoint(0x41F7U, 0x41F9U, 18U, checkpoint);
    checkpoint.accumulator = pointer_high;
    checkpoint.status = set_nz(checkpoint.status, pointer_high);
    append_checkpoint(0x41F9U, 0x41FBU, 27U, checkpoint);
    append_checkpoint(0x41FBU, 0x41FDU, 36U, checkpoint);
    append_checkpoint(0x41FDU, 0x420BU, 48U, checkpoint);
    replacement.work_ram_writes = {{
        {
            .instruction = {0x02U, 0x41F7U},
            .logical_address = 0x202DU,
            .value = pointer_low,
            .master_cycle_offset = 18U,
        },
        {
            .instruction = {0x02U, 0x41FBU},
            .logical_address = 0x202EU,
            .value = pointer_high,
            .master_cycle_offset = 36U,
        },
    }};
    checkpoint.program_counter = 0x420BU;
    replacement.output_registers = checkpoint;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_shared_call_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The terminal-only pointer copy enters the broad shared helper through
    // one fixed JSR frame. Keep the callee's actor, drawing, and coroutine
    // work in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x420BU};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x42U
        || entry.registers.x != 0x3AU || entry.registers.y != 0U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0U
        || work_ram.size() <= 0x01F6U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.stack_pointer = 0xF4U;
    output.program_counter = 0xC024U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC024U},
        .output_registers = output,
        .body_master_cycles = 21U,
        .work_ram_writes = {{
            {
                .instruction = {0x02U, 0x420BU},
                .logical_address = 0x21F6U,
                .value = 0x42U,
                .master_cycle_offset = 0U,
            },
            {
                .instruction = {0x02U, 0x420BU},
                .logical_address = 0x21F5U,
                .value = 0x0DU,
                .master_cycle_offset = 0U,
            },
        }},
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 18U,
        .master_cycle_offset = 21U,
        .instruction = {0x02U, 0x420BU},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_mode_branch_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // The terminal allocator returns with a nonnegative cleared mode byte.
    // Preserve only the exact BMI fall-through before the caller's broad
    // actor setup resumes in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x420EU};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x02U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0x4210U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4210U},
        .output_registers = output,
        .body_master_cycles = 6U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 3U,
        .master_cycle_offset = 6U,
        .instruction = {0x02U, 0x420EU},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_pointer_low_load_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The terminal caller-side setup reloads the fixed pointer low byte for
    // its first actor field. Keep the indexed store and later setup in
    // assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4210U};
    constexpr std::size_t kPointerLowOffset = 0x002DU;
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x02U
        || work_ram.size() <= kPointerLowOffset
        || std::to_integer<std::uint8_t>(work_ram[kPointerLowOffset]) != 0xE1U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.accumulator = std::to_integer<std::uint8_t>(work_ram[kPointerLowOffset]);
    output.status = static_cast<std::uint8_t>((output.status & 0x7DU) | 0x80U);
    output.program_counter = 0x4212U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4212U},
        .output_registers = output,
        .body_master_cycles = 12U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 9U,
        .master_cycle_offset = 12U,
        .instruction = {0x02U, 0x4210U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_pointer_low_store_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // Store the terminal pointer low byte into its actor slot, then leave the
    // remaining field initialization and later coroutine work in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4212U};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0xE1U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x80U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0x4215U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4215U},
        .output_registers = output,
        .body_master_cycles = 15U,
    };
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x4212U},
        .logical_address = static_cast<std::uint16_t>(0x3E00U + output.y),
        .value = output.accumulator,
        .master_cycle_offset = 15U,
    });
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 12U,
        .master_cycle_offset = 15U,
        .instruction = {0x02U, 0x4212U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_pointer_high_load_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // Reload the terminal pointer high byte for the paired actor field while
    // leaving the indexed store and later initialization in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4215U};
    constexpr std::size_t kPointerHighOffset = 0x002EU;
    if (entry.routine != kRoutine || entry.registers.accumulator != 0xE1U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x80U
        || work_ram.size() <= kPointerHighOffset
        || std::to_integer<std::uint8_t>(work_ram[kPointerHighOffset]) != 0x42U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.accumulator = std::to_integer<std::uint8_t>(work_ram[kPointerHighOffset]);
    output.status = static_cast<std::uint8_t>(output.status & 0x7DU);
    output.program_counter = 0x4217U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4217U},
        .output_registers = output,
        .body_master_cycles = 12U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 9U,
        .master_cycle_offset = 12U,
        .instruction = {0x02U, 0x4215U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_pointer_high_store_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // Store the terminal pointer high byte into the paired actor field, then
    // leave the remaining initialization and coroutine work in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4217U};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x42U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0x421AU;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x421AU},
        .output_registers = output,
        .body_master_cycles = 15U,
    };
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x4217U},
        .logical_address = static_cast<std::uint16_t>(0x3E80U + output.y),
        .value = output.accumulator,
        .master_cycle_offset = 15U,
    });
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 12U,
        .master_cycle_offset = 15U,
        .instruction = {0x02U, 0x4217U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_scratch_clear_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // Clear the terminal setup's scratch byte, then retain the remaining
    // position-field initialization and coroutine work in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x421AU};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x42U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0x421CU;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x421CU},
        .output_registers = output,
        .body_master_cycles = 12U,
    };
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x421AU},
        .logical_address = 0x2026U,
        .value = 0U,
        .master_cycle_offset = 12U,
    });
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 9U,
        .master_cycle_offset = 12U,
        .instruction = {0x02U, 0x421AU},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_mode_load_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // Reload the terminal mode byte and leave its signed branch plus the
    // following position-field initialization in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x421CU};
    constexpr std::size_t kModeOffset = 0x002BU;
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x42U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0U
        || work_ram.size() <= kModeOffset) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.accumulator = std::to_integer<std::uint8_t>(work_ram[kModeOffset]);
    output.status = static_cast<std::uint8_t>(output.status & 0x7DU);
    if (output.accumulator == 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);
    }
    if ((output.accumulator & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x80U);
    }
    output.program_counter = 0x421EU;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x421EU},
        .output_registers = output,
        .body_master_cycles = 12U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 9U,
        .master_cycle_offset = 12U,
        .instruction = {0x02U, 0x421CU},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_mode_signed_branch_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // The terminal mode is nonnegative; take the signed branch while keeping
    // the alternate decrement and all later actor initialization in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x421EU};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x02U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0x4222U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4222U},
        .output_registers = output,
        .body_master_cycles = 12U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 9U,
        .master_cycle_offset = 12U,
        .instruction = {0x02U, 0x421EU},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_carry_clear_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // Clear carry before the terminal position arithmetic; retain the indexed
    // loads, add, and actor-field stores in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4222U};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x02U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0x4223U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4223U},
        .output_registers = output,
        .body_master_cycles = 6U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 3U,
        .master_cycle_offset = 6U,
        .instruction = {0x02U, 0x4222U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_base_load_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // Reload the terminal actor's base position before the original mode-byte
    // add and resulting field store continue in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4223U};
    constexpr std::size_t kBasePositionOffset = 0x1180U;
    const auto source_offset = kBasePositionOffset + entry.registers.x;
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x02U
        || work_ram.size() <= source_offset) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.accumulator = std::to_integer<std::uint8_t>(work_ram[source_offset]);
    output.status = static_cast<std::uint8_t>(output.status & 0x7DU);
    if (output.accumulator == 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);
    }
    if ((output.accumulator & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x80U);
    }
    output.program_counter = 0x4226U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4226U},
        .output_registers = output,
        .body_master_cycles = 15U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 12U,
        .master_cycle_offset = 15U,
        .instruction = {0x02U, 0x4223U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_mode_add_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // Add the terminal mode byte to its base position, preserving the exact
    // binary ADC flags before assembly stores the resulting actor position.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4226U};
    constexpr std::size_t kModeOffset = 0x002BU;
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x20U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0U
        || work_ram.size() <= kModeOffset) {
        return std::nullopt;
    }

    const auto left = entry.registers.accumulator;
    const auto right = std::to_integer<std::uint8_t>(work_ram[kModeOffset]);
    const auto carry_in = static_cast<std::uint16_t>(entry.registers.status & 0x01U);
    const auto sum = static_cast<std::uint16_t>(left)
        + static_cast<std::uint16_t>(right) + carry_in;
    auto output = entry.registers;
    output.accumulator = static_cast<std::uint8_t>(sum);
    output.status = static_cast<std::uint8_t>(output.status & ~0xC3U);
    if (output.accumulator == 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);
    }
    if ((output.accumulator & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x80U);
    }
    if (sum > 0xFFU) {
        output.status = static_cast<std::uint8_t>(output.status | 0x01U);
    }
    if (((~(left ^ right) & (left ^ output.accumulator)) & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x40U);
    }
    output.program_counter = 0x4228U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4228U},
        .output_registers = output,
        .body_master_cycles = 12U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 9U,
        .master_cycle_offset = 12U,
        .instruction = {0x02U, 0x4226U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_store_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // Store the terminal's resolved position into its actor field; retain the
    // remaining position setup and coroutine selection in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4228U};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x20U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0x422BU;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x422BU},
        .output_registers = output,
        .body_master_cycles = 15U,
    };
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x4228U},
        .logical_address = static_cast<std::uint16_t>(0x3180U + output.y),
        .value = output.accumulator,
        .master_cycle_offset = 15U,
    });
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 12U,
        .master_cycle_offset = 15U,
        .instruction = {0x02U, 0x4228U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_high_load_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // Reload the terminal actor's high position byte before the original
    // scratch-byte add and high-field store continue in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x422BU};
    constexpr std::size_t kHighPositionOffset = 0x1200U;
    const auto source_offset = kHighPositionOffset + entry.registers.x;
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x20U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0U
        || work_ram.size() <= source_offset) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.accumulator = std::to_integer<std::uint8_t>(work_ram[source_offset]);
    output.status = static_cast<std::uint8_t>(output.status & 0x7DU);
    if (output.accumulator == 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);
    }
    if ((output.accumulator & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x80U);
    }
    output.program_counter = 0x422EU;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x422EU},
        .output_registers = output,
        .body_master_cycles = 15U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 12U,
        .master_cycle_offset = 15U,
        .instruction = {0x02U, 0x422BU},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_high_add_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // The terminal's cleared scratch byte leaves the high position unchanged;
    // retain the following indexed store and later setup in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x422EU};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x02U) {
        return std::nullopt;
    }
    auto output = entry.registers;
    output.program_counter = 0x4230U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4230U},
        .output_registers = output,
        .body_master_cycles = 12U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 9U,
        .master_cycle_offset = 12U,
        .instruction = {0x02U, 0x422EU},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_high_store_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4230U};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x02U) {
        return std::nullopt;
    }
    auto output = entry.registers;
    output.program_counter = 0x4233U;
    RoutineReplacement replacement{{.physical_bank = 0x02U, .logical_pc = 0x4233U}, output, 15U};
    replacement.work_ram_writes.push_back({{0x02U, 0x4230U}, static_cast<std::uint16_t>(0x3200U + output.y), output.accumulator, 15U});
    replacement.checkpoints.push_back({12U, 15U, {0x02U, 0x4230U}, output});
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_high_scratch_clear_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // Clear the terminal setup's carry scratch before the original high-byte
    // mode load and remaining actor initialization.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4233U};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x02U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0x4235U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4235U},
        .output_registers = output,
        .body_master_cycles = 12U,
    };
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x4233U},
        .logical_address = 0x2026U,
        .value = 0U,
        .master_cycle_offset = 12U,
    });
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 9U,
        .master_cycle_offset = 12U,
        .instruction = {0x02U, 0x4233U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_high_mode_load_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // Reload the terminal high-byte mode and leave its signed branch plus the
    // remaining actor initialization in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4235U};
    constexpr std::size_t kHighModeOffset = 0x002CU;
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x02U
        || work_ram.size() <= kHighModeOffset) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.accumulator = std::to_integer<std::uint8_t>(work_ram[kHighModeOffset]);
    output.status = static_cast<std::uint8_t>(output.status & 0x7DU);
    if (output.accumulator == 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);
    }
    if ((output.accumulator & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x80U);
    }
    output.program_counter = 0x4237U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4237U},
        .output_registers = output,
        .body_master_cycles = 12U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 9U,
        .master_cycle_offset = 12U,
        .instruction = {0x02U, 0x4235U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_high_mode_signed_branch_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // The terminal high-byte mode is nonnegative; take the signed branch and
    // retain the alternate scratch decrement plus later setup in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4237U};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x02U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0x423BU;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x423BU},
        .output_registers = output,
        .body_master_cycles = 12U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 9U,
        .master_cycle_offset = 12U,
        .instruction = {0x02U, 0x4237U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_high_carry_clear_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // Clear carry before the terminal high-position base load, preserving the
    // original add and remaining actor initialization in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x423BU};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x02U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0x423CU;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x423CU},
        .output_registers = output,
        .body_master_cycles = 6U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 3U,
        .master_cycle_offset = 6U,
        .instruction = {0x02U, 0x423BU},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_high_base_load_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // Load the terminal high-position base before the original high-byte mode
    // add and remaining actor initialization.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x423CU};
    constexpr std::size_t kHighPositionBaseOffset = 0x1300U;
    const auto source_offset = kHighPositionBaseOffset + entry.registers.x;
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x02U
        || work_ram.size() <= source_offset) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.accumulator = std::to_integer<std::uint8_t>(work_ram[source_offset]);
    output.status = static_cast<std::uint8_t>(output.status & 0x7DU);
    if (output.accumulator == 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);
    }
    if ((output.accumulator & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x80U);
    }
    output.program_counter = 0x423FU;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x423FU},
        .output_registers = output,
        .body_master_cycles = 15U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 12U,
        .master_cycle_offset = 15U,
        .instruction = {0x02U, 0x423CU},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_high_scratch_add_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // Add the terminal high-position carry scratch, preserving binary ADC
    // flags before the original actor-field store.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x423FU};
    constexpr std::size_t kScratchOffset = 0x0026U;
    if (entry.routine != kRoutine || entry.registers.accumulator != 0xC7U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x80U
        || work_ram.size() <= kScratchOffset) {
        return std::nullopt;
    }

    const auto left = entry.registers.accumulator;
    const auto right = std::to_integer<std::uint8_t>(work_ram[kScratchOffset]);
    const auto carry_in = static_cast<std::uint16_t>(entry.registers.status & 0x01U);
    const auto sum = static_cast<std::uint16_t>(left)
        + static_cast<std::uint16_t>(right) + carry_in;
    auto output = entry.registers;
    output.accumulator = static_cast<std::uint8_t>(sum);
    output.status = static_cast<std::uint8_t>(output.status & ~0xC3U);
    if (output.accumulator == 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);
    }
    if ((output.accumulator & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x80U);
    }
    if (sum > 0xFFU) {
        output.status = static_cast<std::uint8_t>(output.status | 0x01U);
    }
    if (((~(left ^ right) & (left ^ output.accumulator)) & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x40U);
    }
    output.program_counter = 0x4241U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4241U},
        .output_registers = output,
        .body_master_cycles = 12U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 9U,
        .master_cycle_offset = 12U,
        .instruction = {0x02U, 0x423FU},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_high_actor_store_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // Store the terminal resolved high position, retaining the remaining
    // actor-field setup in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4241U};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0xC7U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x80U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0x4244U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4244U},
        .output_registers = output,
        .body_master_cycles = 15U,
    };
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x4241U},
        .logical_address = static_cast<std::uint16_t>(0x3300U + output.y),
        .value = output.accumulator,
        .master_cycle_offset = 15U,
    });
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 12U,
        .master_cycle_offset = 15U,
        .instruction = {0x02U, 0x4241U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_field_load_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // Load the terminal companion actor field before the original scratch add
    // and remaining actor initialization.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4244U};
    constexpr std::size_t kActorFieldOffset = 0x1380U;
    const auto source_offset = kActorFieldOffset + entry.registers.x;
    if (entry.routine != kRoutine || entry.registers.accumulator != 0xC7U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x80U
        || work_ram.size() <= source_offset) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.accumulator = std::to_integer<std::uint8_t>(work_ram[source_offset]);
    output.status = static_cast<std::uint8_t>(output.status & 0x7DU);
    if (output.accumulator == 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);
    }
    if ((output.accumulator & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x80U);
    }
    output.program_counter = 0x4247U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4247U},
        .output_registers = output,
        .body_master_cycles = 15U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 12U,
        .master_cycle_offset = 15U,
        .instruction = {0x02U, 0x4244U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_field_scratch_add_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // Add the terminal companion actor-field carry scratch before its original
    // indexed store and remaining actor initialization.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4247U};
    constexpr std::size_t kScratchOffset = 0x0026U;
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x02U
        || work_ram.size() <= kScratchOffset) {
        return std::nullopt;
    }

    const auto left = entry.registers.accumulator;
    const auto right = std::to_integer<std::uint8_t>(work_ram[kScratchOffset]);
    const auto carry_in = static_cast<std::uint16_t>(entry.registers.status & 0x01U);
    const auto sum = static_cast<std::uint16_t>(left)
        + static_cast<std::uint16_t>(right) + carry_in;
    auto output = entry.registers;
    output.accumulator = static_cast<std::uint8_t>(sum);
    output.status = static_cast<std::uint8_t>(output.status & ~0xC3U);
    if (output.accumulator == 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);
    }
    if ((output.accumulator & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x80U);
    }
    if (sum > 0xFFU) {
        output.status = static_cast<std::uint8_t>(output.status | 0x01U);
    }
    if (((~(left ^ right) & (left ^ output.accumulator)) & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x40U);
    }
    output.program_counter = 0x4249U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4249U},
        .output_registers = output,
        .body_master_cycles = 12U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 9U,
        .master_cycle_offset = 12U,
        .instruction = {0x02U, 0x4247U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_field_store_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // Store the terminal resolved companion actor field, retaining the
    // remaining actor initialization in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4249U};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x02U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0x424CU;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x424CU},
        .output_registers = output,
        .body_master_cycles = 15U,
    };
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x4249U},
        .logical_address = static_cast<std::uint16_t>(0x3380U + output.y),
        .value = output.accumulator,
        .master_cycle_offset = 15U,
    });
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 12U,
        .master_cycle_offset = 15U,
        .instruction = {0x02U, 0x4249U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_field_clear_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // Clear the terminal companion actor-field accumulator before the original
    // zero stores and remaining actor initialization.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x424CU};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x02U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.accumulator = 0U;
    output.status = static_cast<std::uint8_t>((output.status & 0x7DU) | 0x02U);
    output.program_counter = 0x424DU;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x424DU},
        .output_registers = output,
        .body_master_cycles = 6U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 3U,
        .master_cycle_offset = 6U,
        .instruction = {0x02U, 0x424CU},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_field_zero_store_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // Store the terminal cleared companion field, retaining the remaining
    // actor initialization in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x424DU};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x02U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0x4250U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4250U},
        .output_registers = output,
        .body_master_cycles = 15U,
    };
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x424DU},
        .logical_address = static_cast<std::uint16_t>(0x3800U + output.y),
        .value = output.accumulator,
        .master_cycle_offset = 15U,
    });
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 12U,
        .master_cycle_offset = 15U,
        .instruction = {0x02U, 0x424DU},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_field_second_zero_store_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // Store the second terminal cleared companion field, retaining the
    // remaining actor initialization in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4250U};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x02U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0x4253U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4253U},
        .output_registers = output,
        .body_master_cycles = 15U,
    };
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x4250U},
        .logical_address = static_cast<std::uint16_t>(0x3980U + output.y),
        .value = output.accumulator,
        .master_cycle_offset = 15U,
    });
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 12U,
        .master_cycle_offset = 15U,
        .instruction = {0x02U, 0x4250U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_attribute_load_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // Load the terminal actor attribute before the original mask, mode bits,
    // and remaining actor initialization.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4253U};
    constexpr std::size_t kActorAttributeOffset = 0x1000U;
    const auto source_offset = kActorAttributeOffset + entry.registers.y;
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x02U
        || work_ram.size() <= source_offset) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.accumulator = std::to_integer<std::uint8_t>(work_ram[source_offset]);
    output.status = static_cast<std::uint8_t>(output.status & 0x7DU);
    if (output.accumulator == 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);
    }
    if ((output.accumulator & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x80U);
    }
    output.program_counter = 0x4256U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4256U},
        .output_registers = output,
        .body_master_cycles = 15U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 12U,
        .master_cycle_offset = 15U,
        .instruction = {0x02U, 0x4253U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_attribute_mask_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // Mask the terminal actor attribute before the original mode-bit merge and
    // remaining actor initialization.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4256U};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x80U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x80U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.accumulator = static_cast<std::uint8_t>(output.accumulator & 0xF0U);
    output.status = static_cast<std::uint8_t>(output.status & 0x7DU);
    if (output.accumulator == 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);
    }
    if ((output.accumulator & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x80U);
    }
    output.program_counter = 0x4258U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4258U},
        .output_registers = output,
        .body_master_cycles = 6U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 3U,
        .master_cycle_offset = 6U,
        .instruction = {0x02U, 0x4256U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_attribute_mode_merge_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // Merge the terminal actor mode bits before the original attribute store
    // and remaining initialization.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4258U};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x80U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x80U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.accumulator = static_cast<std::uint8_t>(output.accumulator | 0x0FU);
    output.status = static_cast<std::uint8_t>(output.status & 0x7DU);
    if (output.accumulator == 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);
    }
    if ((output.accumulator & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x80U);
    }
    output.program_counter = 0x425AU;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x425AU},
        .output_registers = output,
        .body_master_cycles = 6U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 3U,
        .master_cycle_offset = 6U,
        .instruction = {0x02U, 0x4258U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_attribute_store_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // Store the terminal merged actor attribute, retaining the remaining
    // actor initialization in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x425AU};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x8FU
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x80U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0x425DU;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x425DU},
        .output_registers = output,
        .body_master_cycles = 15U,
    };
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x425AU},
        .logical_address = static_cast<std::uint16_t>(0x3000U + output.y),
        .value = output.accumulator,
        .master_cycle_offset = 15U,
    });
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 12U,
        .master_cycle_offset = 15U,
        .instruction = {0x02U, 0x425AU},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_field_constant_load_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x425DU};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x8FU
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x80U) {
        return std::nullopt;
    }
    auto output = entry.registers;
    output.accumulator = 0x90U;
    output.status = static_cast<std::uint8_t>((output.status & 0x7DU) | 0x80U);
    output.program_counter = 0x425FU;
    RoutineReplacement replacement{{.physical_bank = 0x02U, .logical_pc = 0x425FU}, output, 6U};
    replacement.checkpoints.push_back({.interrupt_sample_master_cycle_offset = 3U, .master_cycle_offset = 6U, .instruction = {0x02U, 0x425DU}, .registers = output});
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_field_constant_store_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x425FU};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x90U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x80U) return std::nullopt;
    auto output = entry.registers;
    output.program_counter = 0x4262U;
    RoutineReplacement replacement{{.physical_bank = 0x02U, .logical_pc = 0x4262U}, output, 15U};
    replacement.work_ram_writes.push_back({.instruction = {0x02U, 0x425FU}, .logical_address = static_cast<std::uint16_t>(0x3F00U + output.y), .value = output.accumulator, .master_cycle_offset = 15U});
    replacement.checkpoints.push_back({.interrupt_sample_master_cycle_offset = 12U, .master_cycle_offset = 15U, .instruction = {0x02U, 0x425FU}, .registers = output});
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_field_second_constant_load_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4262U};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x90U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0x80U) return std::nullopt;
    auto output = entry.registers;
    output.accumulator = 0x42U;
    output.status = static_cast<std::uint8_t>(output.status & 0x7DU);
    output.program_counter = 0x4264U;
    RoutineReplacement replacement{{.physical_bank = 0x02U, .logical_pc = 0x4264U}, output, 6U};
    replacement.checkpoints.push_back({.interrupt_sample_master_cycle_offset = 3U, .master_cycle_offset = 6U, .instruction = {0x02U, 0x4262U}, .registers = output});
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_field_second_constant_store_prefix(
    const RoutineEntry& entry, const std::span<const std::byte>)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4264U};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x42U || entry.registers.x != 0x3AU || entry.registers.y != 0x70U || entry.registers.stack_pointer != 0xF6U || entry.registers.status != 0U) return std::nullopt;
    auto output = entry.registers; output.program_counter = 0x4267U;
    RoutineReplacement replacement{{.physical_bank = 0x02U, .logical_pc = 0x4267U}, output, 15U};
    replacement.work_ram_writes.push_back({.instruction = {0x02U, 0x4264U}, .logical_address = static_cast<std::uint16_t>(0x3F80U + output.y), .value = output.accumulator, .master_cycle_offset = 15U});
    replacement.checkpoints.push_back({.interrupt_sample_master_cycle_offset = 12U, .master_cycle_offset = 15U, .instruction = {0x02U, 0x4264U}, .registers = output});
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_shared_jump_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // The terminal-only shared entry immediately jumps into the broad C5AB
    // worker. Keep the worker's timer, drawing, and coroutine work in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x01U, .logical_pc = 0xC024U};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x42U
        || entry.registers.x != 0x3AU || entry.registers.y != 0U
        || entry.registers.stack_pointer != 0xF4U || entry.registers.status != 0U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5ABU;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5ABU},
        .output_registers = output,
        .body_master_cycles = 12U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 9U,
        .master_cycle_offset = 12U,
        .instruction = {0x01U, 0xC024U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_shared_scan_initializer_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // The terminal-only shared worker begins by initializing its allocation
    // scan index. Keep the RAM scan and every allocation/drawing path in
    // assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x01U, .logical_pc = 0xC5ABU};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x42U
        || entry.registers.x != 0x3AU || entry.registers.y != 0U
        || entry.registers.stack_pointer != 0xF4U || entry.registers.status != 0U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.y = 0x04U;
    output.program_counter = 0xC5ADU;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5ADU},
        .output_registers = output,
        .body_master_cycles = 6U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 3U,
        .master_cycle_offset = 6U,
        .instruction = {0x01U, 0xC5ABU},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_shared_scan_read_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The terminal helper's first allocator iteration reads its slot-four
    // occupancy byte. Retain the branch, scan loop, reservation, drawing,
    // and coroutine paths in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x01U, .logical_pc = 0xC5ADU};
    constexpr std::size_t kAllocatorArrayOffset = 0x1000U;
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x42U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x04U
        || entry.registers.stack_pointer != 0xF4U || entry.registers.status != 0U
        || work_ram.size() <= kAllocatorArrayOffset + entry.registers.y) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.accumulator = std::to_integer<std::uint8_t>(
        work_ram[kAllocatorArrayOffset + entry.registers.y]);
    output.status = static_cast<std::uint8_t>(output.status & 0x7DU);
    if (output.accumulator == 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);
    } else if ((output.accumulator & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x80U);
    }
    output.program_counter = 0xC5B0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5B0U},
        .output_registers = output,
        .body_master_cycles = 15U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 12U,
        .master_cycle_offset = 15U,
        .instruction = {0x01U, 0xC5ADU},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_shared_scan_nonzero_branch_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // The terminal helper's first allocator slot is occupied. Preserve the
    // exact not-taken BEQ and leave the increment, loop, reservation, drawing,
    // and coroutine work in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x01U, .logical_pc = 0xC5B0U};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x80U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x04U
        || entry.registers.stack_pointer != 0xF4U || entry.registers.status != 0x80U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5B2U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5B2U},
        .output_registers = output,
        .body_master_cycles = 6U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 3U,
        .master_cycle_offset = 6U,
        .instruction = {0x01U, 0xC5B0U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_shared_scan_increment_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // The terminal helper advances from its occupied slot-four entry. Retain
    // the range comparison, loop, reservation, drawing, and coroutine work
    // in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x01U, .logical_pc = 0xC5B2U};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x80U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x04U
        || entry.registers.stack_pointer != 0xF4U || entry.registers.status != 0x80U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.y = static_cast<std::uint8_t>(output.y + 1U);
    output.status = static_cast<std::uint8_t>(output.status & 0x7DU);
    if (output.y == 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);
    } else if ((output.y & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x80U);
    }
    output.program_counter = 0xC5B3U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5B3U},
        .output_registers = output,
        .body_master_cycles = 6U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 3U,
        .master_cycle_offset = 6U,
        .instruction = {0x01U, 0xC5B2U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_shared_scan_range_compare_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // The terminal helper compares the post-increment slot-five index against
    // the allocator limit. Retain the loop branch, reservation, drawing, and
    // coroutine work in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x01U, .logical_pc = 0xC5B3U};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x80U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x05U
        || entry.registers.stack_pointer != 0xF4U || entry.registers.status != 0U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.status = 0x80U;
    output.program_counter = 0xC5B5U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5B5U},
        .output_registers = output,
        .body_master_cycles = 6U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 3U,
        .master_cycle_offset = 6U,
        .instruction = {0x01U, 0xC5B3U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_shared_scan_loop_branch_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // The terminal helper's post-increment slot-five comparison is nonzero,
    // so its allocator loop returns to the shared read. Retain later scan
    // iterations, reservation, drawing, and coroutine work in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x01U, .logical_pc = 0xC5B5U};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x80U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x05U
        || entry.registers.stack_pointer != 0xF4U || entry.registers.status != 0x80U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5ADU;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5ADU},
        .output_registers = output,
        .body_master_cycles = 12U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 9U,
        .master_cycle_offset = 12U,
        .instruction = {0x01U, 0xC5B5U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_allocator_load_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // The terminal helper found its first free allocator slot. Supply only
    // the reservation-byte load and leave the indexed store, actor setup,
    // drawing, and coroutine work in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x01U, .logical_pc = 0xC5B9U};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF4U || entry.registers.status != 0x02U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.accumulator = 0x80U;
    output.status = static_cast<std::uint8_t>((output.status & 0x7DU) | 0x80U);
    output.program_counter = 0xC5BBU;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5BBU},
        .output_registers = output,
        .body_master_cycles = 6U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 3U,
        .master_cycle_offset = 6U,
        .instruction = {0x01U, 0xC5B9U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_allocator_store_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // Reserve the terminal helper's first free allocator slot, then resume
    // the original actor setup, drawing, and coroutine body.
    constexpr RoutineKey kRoutine{.physical_bank = 0x01U, .logical_pc = 0xC5BBU};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x80U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF4U || entry.registers.status != 0x80U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5BEU;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5BEU},
        .output_registers = output,
        .body_master_cycles = 15U,
    };
    replacement.work_ram_writes.push_back({
        .instruction = {0x01U, 0xC5BBU},
        .logical_address = static_cast<std::uint16_t>(0x3000U + output.y),
        .value = output.accumulator,
        .master_cycle_offset = 15U,
    });
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 12U,
        .master_cycle_offset = 15U,
        .instruction = {0x01U, 0xC5BBU},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_allocator_mode_load_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The terminal reservation path reads its cleared mode byte before the
    // original branch and return. Keep those control-flow instructions and
    // all later actor/coroutine work in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x01U, .logical_pc = 0xC5BEU};
    constexpr std::size_t kModeByteOffset = 0x0055U;
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x80U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF4U || entry.registers.status != 0x80U
        || work_ram.size() <= kModeByteOffset
        || std::to_integer<std::uint8_t>(work_ram[kModeByteOffset]) != 0U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.accumulator = std::to_integer<std::uint8_t>(work_ram[kModeByteOffset]);
    output.status = static_cast<std::uint8_t>(output.status & 0x7DU);
    if (output.accumulator == 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);
    } else if ((output.accumulator & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x80U);
    }
    output.program_counter = 0xC5C0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5C0U},
        .output_registers = output,
        .body_master_cycles = 12U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 9U,
        .master_cycle_offset = 12U,
        .instruction = {0x01U, 0xC5BEU},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_allocator_mode_branch_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // The cleared terminal mode byte takes the branch to the allocator's
    // carry-clear return. Keep that return and all later helper work in
    // assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x01U, .logical_pc = 0xC5C0U};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF4U || entry.registers.status != 0x02U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5C7U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5C7U},
        .output_registers = output,
        .body_master_cycles = 12U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 9U,
        .master_cycle_offset = 12U,
        .instruction = {0x01U, 0xC5C0U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_release_helper_allocator_return_carry_clear_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte>)
{
    // The cleared terminal mode returns through CLC/RTS. Supply only the
    // carry-clear instruction so the shared return and caller continuation
    // remain assembly-backed.
    constexpr RoutineKey kRoutine{.physical_bank = 0x01U, .logical_pc = 0xC5C7U};
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x3AU || entry.registers.y != 0x70U
        || entry.registers.stack_pointer != 0xF4U || entry.registers.status != 0x02U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.status = static_cast<std::uint8_t>(output.status & 0xFEU);
    output.program_counter = 0xC5C8U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5C8U},
        .output_registers = output,
        .body_master_cycles = 6U,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 3U,
        .master_cycle_offset = 6U,
        .instruction = {0x01U, 0xC5C7U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_terminal_scheduler(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The release prefix reaches a compact banked scheduler wrapper. Compose
    // its JSR/C018 frame with the existing C5C9 scheduler rather than widening
    // the replacement through the unrelated continuation at C5E9.
    constexpr RoutineKey kRoutine{.physical_bank = 0x03U, .logical_pc = 0x7420U};
    if (entry.routine != kRoutine || entry.registers.x < 0x3AU
        || entry.registers.x > 0x3DU || entry.registers.accumulator != 0xE0U
        || entry.registers.y != 0x03U || entry.registers.stack_pointer != 0xFBU
        || entry.registers.status != 0x81U || work_ram.size() < 0x0200U) {
        return std::nullopt;
    }

    const auto stack_high = entry.registers.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    staged[0x0100U + stack_high] = std::byte{0x74U};
    staged[0x0100U + stack_low] = std::byte{0x22U};

    RoutineReplacement replacement{
        .work_ram_writes = {
            {
                .instruction = {0x03U, 0x7420U},
                .logical_address = static_cast<std::uint16_t>(0x2100U + stack_high),
                .value = 0x74U,
                .master_cycle_offset = 0U,
            },
            {
                .instruction = {0x03U, 0x7420U},
                .logical_address = static_cast<std::uint16_t>(0x2100U + stack_low),
                .value = 0x22U,
                .master_cycle_offset = 0U,
            },
        },
    };
    const auto append_checkpoint = [&replacement](const std::uint8_t bank,
                                                   const std::uint16_t instruction_pc,
                                                   const std::uint16_t resume_pc,
                                                   const std::uint32_t master_cycle,
                                                   auto registers) {
        registers.program_counter = resume_pc;
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = master_cycle - 3U,
            .master_cycle_offset = master_cycle,
            .instruction = {bank, instruction_pc},
            .registers = registers,
        });
    };

    auto checkpoint = entry.registers;
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(0x03U, 0x7420U, 0xC018U, 21U, checkpoint);
    append_checkpoint(0x01U, 0xC018U, 0xC5C9U, 33U, checkpoint);

    RoutineEntry scheduler_entry{
        .routine = {.physical_bank = 0x01U, .logical_pc = 0xC5C9U},
        .registers = checkpoint,
    };
    auto scheduler = replace_coroutine_scheduler(scheduler_entry, staged);
    if (!scheduler || scheduler->body_master_cycles != 159U
        || scheduler->checkpoints.empty()) {
        return std::nullopt;
    }

    constexpr std::uint32_t kSchedulerOffset = 33U;
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
replace_knight_terminal_release_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // After the terminal score and shared release helper have completed, this
    // prefix marks the Knight surface as released before the original
    // scheduler call. Keep that scheduler transaction in assembly.
    constexpr RoutineKey kRoutine{.physical_bank = 0x03U, .logical_pc = 0x7418U};
    constexpr std::size_t kSurfaceOffset = 0x1080U;
    if (entry.routine != kRoutine || entry.registers.x < 0x3AU
        || entry.registers.x > 0x3DU || entry.registers.accumulator != 0U
        || entry.registers.y != 0x03U || entry.registers.status != 0x03U
        || work_ram.size() <= kSurfaceOffset + entry.registers.x) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
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
            .instruction = {0x03U, instruction_pc},
            .registers = registers,
        });
    };

    auto checkpoint = entry.registers;
    checkpoint.accumulator = std::to_integer<std::uint8_t>(
        work_ram[kSurfaceOffset + entry.registers.x]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x03U, .logical_pc = 0x7420U},
        .body_master_cycles = 36U,
    };
    append_checkpoint(replacement, 0x7418U, 0x741BU, 15U, checkpoint);
    checkpoint.accumulator = static_cast<std::uint8_t>(checkpoint.accumulator | 0x60U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x741BU, 0x741DU, 21U, checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x741DU},
        .logical_address = static_cast<std::uint16_t>(0x3080U + entry.registers.x),
        .value = checkpoint.accumulator,
        .master_cycle_offset = 36U,
    });
    checkpoint.program_counter = 0x7420U;
    replacement.output_registers = checkpoint;
    append_checkpoint(replacement, 0x741DU, 0x7420U, 36U, checkpoint);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_initializers(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kFirstRoutine{.physical_bank = 0x03U, .logical_pc = 0x734EU};
    constexpr RoutineKey kSecondRoutine{.physical_bank = 0x03U, .logical_pc = 0x7353U};
    constexpr RoutineKey kThirdRoutine{.physical_bank = 0x03U, .logical_pc = 0x735AU};
    constexpr RoutineKey kFourthRoutine{.physical_bank = 0x03U, .logical_pc = 0x7361U};
    if ((entry.routine != kFirstRoutine && entry.routine != kSecondRoutine
            && entry.routine != kThirdRoutine && entry.routine != kFourthRoutine)
        || entry.registers.status != 0x02U || entry.registers.y != 0x0BU) {
        return std::nullopt;
    }
    const auto stack_return = proven_stack_return(entry, 0xF3U);
    if (!stack_return) {
        return std::nullopt;
    }

    const auto expected_slot = static_cast<std::uint8_t>(
        entry.routine == kFirstRoutine ? 0x3AU
        : entry.routine == kSecondRoutine ? 0x3BU
        : entry.routine == kThirdRoutine ? 0x3CU
        : 0x3DU);
    if (entry.registers.x != expected_slot) {
        return std::nullopt;
    }

    dc_knight_initializer initializer{};
    if (!dc_classify_knight_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    const auto first_instruction = static_cast<std::uint16_t>(
        entry.routine.logical_pc + (entry.routine == kFirstRoutine ? 0U : 2U));
    const auto first_cycle = static_cast<std::uint32_t>(
        entry.routine == kFirstRoutine ? 15U : 21U);
    const auto common_offset = static_cast<std::uint32_t>(
        entry.routine == kFirstRoutine ? 33U
        : entry.routine == kFourthRoutine ? 21U
        : 33U);
    const auto body_cycles = static_cast<std::uint32_t>(
        entry.routine == kFirstRoutine ? 282U
        : entry.routine == kFourthRoutine ? 276U
        : 288U);

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x73U;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = body_cycles,
    };
    const auto slot_address = [slot = initializer.slot](const std::uint16_t base) {
        return static_cast<std::uint16_t>(base + slot);
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    add_write(0x03U, first_instruction, slot_address(0x3B00U),
        initializer.queue_index, first_cycle);
    add_write(0x03U, 0x7368U, slot_address(0x3D00U), 0x8BU,
        first_cycle + common_offset);
    add_write(0x03U, 0x736DU, slot_address(0x3800U), 0x10U,
        first_cycle + common_offset + 21U);
    add_write(0x03U, 0x7372U, slot_address(0x3C00U), 0x04U,
        first_cycle + common_offset + 42U);
    add_write(0x03U, 0x7375U, stack_return->high, 0x73U,
        first_cycle + common_offset + 42U);
    add_write(0x03U, 0x7375U, stack_return->low, 0x77U,
        first_cycle + common_offset + 42U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0x77U,
        first_cycle + common_offset + 99U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x73U,
        first_cycle + common_offset + 123U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0x78U,
        first_cycle + common_offset + 168U);
    add_write(0x01U, 0xC5E1U, slot_address(0x3F00U), 0x78U,
        first_cycle + common_offset + 207U);
    add_write(0x01U, 0xC5E6U, slot_address(0x3F80U), 0x73U,
        body_cycles);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = body_cycles - 3U,
        .master_cycle_offset = body_cycles,
        .instruction = {0x01U, 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_side_spider_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x03U, .logical_pc = 0x64FAU};
    if (entry.routine != kRoutine || entry.registers.y != 0x0BU
        || (entry.registers.status & 0x20U) != 0U) {
        return std::nullopt;
    }
    const auto stack_return = proven_stack_return(entry, 0xF3U);
    if (!stack_return) {
        return std::nullopt;
    }

    dc_side_spider_initializer initializer{};
    if (!dc_classify_side_spider_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, entry.registers.accumulator,
            &initializer)) {
        return std::nullopt;
    }

    constexpr std::uint32_t kBodyCycles = 342U;
    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x65U;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto slot_address = [slot = initializer.slot](const std::uint16_t base) {
        return static_cast<std::uint16_t>(base + slot);
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    add_write(0x03U, 0x64FAU, slot_address(0x3C80U),
        initializer.side_parameter, 15U);
    add_write(0x03U, 0x64FFU, slot_address(0x3800U), 0x10U, 36U);
    add_write(0x03U, 0x6504U, slot_address(0x3D00U), 0x8CU, 57U);
    add_write(0x03U, 0x6509U, slot_address(0x3880U), 0x80U, 78U);
    add_write(0x03U, 0x650EU, slot_address(0x3A00U), 0x01U, 99U);
    add_write(0x03U, 0x6516U, slot_address(0x3800U), 0x10U, 135U);
    add_write(0x03U, 0x6519U, slot_address(0x3980U), 0x00U, 150U);
    add_write(0x03U, 0x651CU, stack_return->high, 0x65U, 150U);
    add_write(0x03U, 0x651CU, stack_return->low, 0x1EU, 150U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0x1EU, 207U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x65U, 231U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0x1FU, 276U);
    add_write(0x01U, 0xC5E1U, slot_address(0x3F00U), 0x1FU, 315U);
    add_write(0x01U, 0xC5E6U, slot_address(0x3F80U), 0x65U, kBodyCycles);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 339U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {0x01U, 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_front_spider_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x03U, .logical_pc = 0x646BU};
    if (entry.routine != kRoutine || entry.registers.y != 0x0BU
        || (entry.registers.status & 0x20U) != 0U) {
        return std::nullopt;
    }
    const auto stack_return = proven_stack_return(entry, 0xF3U);
    if (!stack_return) {
        return std::nullopt;
    }

    dc_front_spider_initializer initializer{};
    if (!dc_classify_front_spider_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, entry.registers.accumulator,
            &initializer)) {
        return std::nullopt;
    }

    constexpr std::uint32_t kBodyCycles = 342U;
    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x64U;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto slot_address = [slot = initializer.slot](const std::uint16_t base) {
        return static_cast<std::uint16_t>(base + slot);
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    add_write(0x03U, 0x646BU, slot_address(0x3C80U),
        initializer.side_parameter, 15U);
    add_write(0x03U, 0x6470U, slot_address(0x3800U), 0x10U, 36U);
    add_write(0x03U, 0x6475U, slot_address(0x3D00U), 0x8CU, 57U);
    add_write(0x03U, 0x647AU, slot_address(0x3880U), 0x80U, 78U);
    add_write(0x03U, 0x647FU, slot_address(0x3A00U), 0x01U, 99U);
    add_write(0x03U, 0x6482U, slot_address(0x3980U), 0x00U, 114U);
    add_write(0x03U, 0x648AU, slot_address(0x3800U), 0x10U, 150U);
    add_write(0x03U, 0x648DU, stack_return->high, 0x64U, 150U);
    add_write(0x03U, 0x648DU, stack_return->low, 0x8FU, 150U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0x8FU, 207U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x64U, 231U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0x90U, 276U);
    add_write(0x01U, 0xC5E1U, slot_address(0x3F00U), 0x90U, 315U);
    add_write(0x01U, 0xC5E6U, slot_address(0x3F80U), 0x64U, kBodyCycles);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = kBodyCycles - 3U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {0x01U, 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_waiting_parent(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x7378U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_knight_waiting_parent parent{};
    if (!dc_classify_knight_waiting_parent(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &parent)) {
        throw std::runtime_error(
            "Native 7378 replacement rejected the work-RAM snapshot.");
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

    if (parent.path == DC_KNIGHT_WAITING_PARENT_ACTIVATING) {
        // A released Giant-Door channel clears its request, disables the
        // waiting collision surface, initializes the movement counters, and
        // installs 03:73A6 through the common coroutine scheduler.
        if (work_ram.size() <= 0x1FCCU) {
            return std::nullopt;
        }
        auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
        auto checkpoint = entry.registers;
        checkpoint.y = parent.queue_index;
        const auto stack_high = checkpoint.stack_pointer;
        const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
        if (staged.size() <= 0x0100U + stack_high
            || staged.size() <= 0x0100U + stack_low) {
            return std::nullopt;
        }

        RoutineReplacement replacement{};
        replacement.work_ram_writes = {
            {
                .instruction = {0x03U, 0x7390U},
                .logical_address = static_cast<std::uint16_t>(
                    0x2D06U + parent.queue_index),
                .value = static_cast<std::uint8_t>(parent.door_handshake & 0xFEU),
                .master_cycle_offset = 99U,
            },
            {
                .instruction = {0x03U, 0x7395U},
                .logical_address = static_cast<std::uint16_t>(0x3880U + entry.registers.x),
                .value = 0x80U,
                .master_cycle_offset = 120U,
            },
            {
                .instruction = {0x03U, 0x739AU},
                .logical_address = static_cast<std::uint16_t>(0x3A00U + entry.registers.x),
                .value = 0x04U,
                .master_cycle_offset = 141U,
            },
            {
                .instruction = {0x03U, 0x739DU},
                .logical_address = static_cast<std::uint16_t>(0x3E00U + entry.registers.x),
                .value = 0x00U,
                .master_cycle_offset = 156U,
            },
            {
                .instruction = {0x03U, 0x73A0U},
                .logical_address = static_cast<std::uint16_t>(0x3E80U + entry.registers.x),
                .value = 0x00U,
                .master_cycle_offset = 171U,
            },
            {
                .instruction = {0x03U, 0x73A3U},
                .logical_address = static_cast<std::uint16_t>(0x2100U + stack_high),
                .value = 0x73U,
                .master_cycle_offset = 171U,
            },
            {
                .instruction = {0x03U, 0x73A3U},
                .logical_address = static_cast<std::uint16_t>(0x2100U + stack_low),
                .value = 0xA5U,
                .master_cycle_offset = 171U,
            },
        };
        staged[0x0D06U + parent.queue_index] =
            static_cast<std::byte>(parent.door_handshake & 0xFEU);
        staged[0x1880U + entry.registers.x] = std::byte{0x80};
        staged[0x1A00U + entry.registers.x] = std::byte{0x04};
        staged[0x1E00U + entry.registers.x] = std::byte{0x00};
        staged[0x1E80U + entry.registers.x] = std::byte{0x00};
        staged[0x0100U + stack_high] = std::byte{0x73};
        staged[0x0100U + stack_low] = std::byte{0xA5};

        const auto append_checkpoint =
            [&replacement](const std::uint8_t physical_bank,
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

        checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
        append_checkpoint(0x03U, 0x7378U, 0x737BU, 15U, checkpoint);
        checkpoint.accumulator = parent.door_handshake;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0x03U, 0x737BU, 0x737EU, 30U, checkpoint);
        checkpoint.accumulator = static_cast<std::uint8_t>(checkpoint.accumulator & 0x01U);
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0x03U, 0x737EU, 0x7380U, 36U, checkpoint);
        append_checkpoint(0x03U, 0x7380U, 0x7388U, 48U, checkpoint);
        append_checkpoint(0x03U, 0x7388U, 0x738BU, 63U, checkpoint);
        checkpoint.accumulator = parent.door_handshake;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0x03U, 0x738BU, 0x738EU, 78U, checkpoint);
        checkpoint.accumulator = static_cast<std::uint8_t>(checkpoint.accumulator & 0xFEU);
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0x03U, 0x738EU, 0x7390U, 84U, checkpoint);
        append_checkpoint(0x03U, 0x7390U, 0x7393U, 99U, checkpoint);
        checkpoint.accumulator = 0x80U;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0x03U, 0x7393U, 0x7395U, 105U, checkpoint);
        append_checkpoint(0x03U, 0x7395U, 0x7398U, 120U, checkpoint);
        checkpoint.accumulator = 0x04U;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0x03U, 0x7398U, 0x739AU, 126U, checkpoint);
        append_checkpoint(0x03U, 0x739AU, 0x739DU, 141U, checkpoint);
        append_checkpoint(0x03U, 0x739DU, 0x73A0U, 156U, checkpoint);
        append_checkpoint(0x03U, 0x73A0U, 0x73A3U, 171U, checkpoint);
        checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
        append_checkpoint(0x03U, 0x73A3U, 0xC018U, 192U, checkpoint);
        append_checkpoint(0x01U, 0xC018U, 0xC5C9U, 204U, checkpoint);

        RoutineEntry scheduler_entry{
            .routine = {.physical_bank = 0x01U, .logical_pc = 0xC5C9U},
            .registers = checkpoint,
        };
        auto scheduler = replace_coroutine_scheduler(scheduler_entry, staged);
        if (!scheduler || scheduler->checkpoints.empty()) {
            return std::nullopt;
        }
        constexpr std::uint32_t kSchedulerOffset = 204U;
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
    if (parent.path != DC_KNIGHT_WAITING_PARENT_WAITING) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.y = parent.queue_index;
    output.accumulator = parent.collision_state;
    output.status = set_nz(output.status, output.accumulator);
    output.status = static_cast<std::uint8_t>(output.status & 0xFEU);
    output.program_counter = 0x7387U;

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x7387U,
        },
        .output_registers = output,
        .body_master_cycles = 123U,
    };
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
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    auto checkpoint = entry.registers;
    checkpoint.y = parent.queue_index;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0x7378U, 0x737BU, 15U, checkpoint);

    checkpoint.accumulator = parent.door_handshake;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x737BU, 0x737EU, 30U, checkpoint);

    checkpoint.accumulator =
        static_cast<std::uint8_t>(checkpoint.accumulator & 0x01U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x737EU, 0x7380U, 36U, checkpoint);
    append_checkpoint(0x7380U, 0x7382U, 42U, checkpoint);

    const auto stack_high = checkpoint.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x7382U},
        .logical_address = static_cast<std::uint16_t>(0x2100U + stack_high),
        .value = 0x73U,
        .master_cycle_offset = 42U,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x7382U},
        .logical_address = static_cast<std::uint16_t>(0x2100U + stack_low),
        .value = 0x84U,
        .master_cycle_offset = 42U,
    });
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 2U);
    append_checkpoint(0x7382U, 0x73D8U, 63U, checkpoint);

    checkpoint.accumulator = parent.collision_state;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x73D8U, 0x73DBU, 78U, checkpoint);
    append_checkpoint(0x73DBU, 0x73FCU, 90U, checkpoint);
    checkpoint.status = static_cast<std::uint8_t>(checkpoint.status & 0xFEU);
    append_checkpoint(0x73FCU, 0x73FDU, 96U, checkpoint);

    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 2U);
    append_checkpoint(0x73FDU, 0x7385U, 117U, checkpoint);
    append_checkpoint(0x7385U, 0x7387U, 123U, output);
    // Absorb the return at $7387. Only this arm reaches it: the activating arm
    // tail-calls into the coroutine scheduler and ends wherever that ends, so
    // it never stands on a return of its own. This walk stages its child's
    // `JSR` frame at S and S-1 and already absorbs the child's own return at
    // $73FD, which is the interior case the declaration does not cover -- but
    // nothing observes 03:73D8 while this routine is replaced.
    absorb_return(
        replacement,
        work_ram,
        entry.registers.stack_pointer,
        {0x03U, 0x7387U},
        123U,
        output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_knight_movement_parent(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x03U, .logical_pc = 0x73A6U};
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_knight_movement_parent parent{};
    if (!dc_classify_knight_movement_parent(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &parent)) {
        return std::nullopt;
    }
    const auto selector_offset = static_cast<std::size_t>(
        0x03U * 0x2000U + (0x7424U & 0x1FFFU) + parent.queue_index);
    if (selector_offset >= rom.size() || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    const auto stage_write = [&staged](
                                 const std::uint16_t logical_address,
                                 const std::uint8_t value) {
        const auto offset = static_cast<std::size_t>(logical_address & 0x1FFFU);
        if (offset >= staged.size()) {
            return false;
        }
        staged[offset] = static_cast<std::byte>(value);
        return true;
    };

    RoutineReplacement replacement{};
    const auto append_checkpoint = [&replacement](
                                       const std::uint16_t instruction_pc,
                                       const std::uint16_t resume_pc,
                                       const std::uint32_t master_cycle,
                                       auto registers) {
        registers.program_counter = resume_pc;
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = master_cycle - 3U,
            .master_cycle_offset = master_cycle,
            .instruction = {0x03U, instruction_pc},
            .registers = registers,
        });
    };
    const auto append_child = [&replacement](
                                  RoutineReplacement child,
                                  const std::uint32_t offset) {
        for (auto write : child.work_ram_writes) {
            write.master_cycle_offset += offset;
            replacement.work_ram_writes.push_back(write);
        }
        for (auto checkpoint : child.checkpoints) {
            checkpoint.interrupt_sample_master_cycle_offset += offset;
            checkpoint.master_cycle_offset += offset;
            replacement.checkpoints.push_back(checkpoint);
        }
    };

    auto checkpoint = entry.registers;
    checkpoint.y = parent.queue_index;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0x73A6U, 0x73A9U, 15U, checkpoint);
    checkpoint.accumulator = std::to_integer<std::uint8_t>(rom[selector_offset]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x73A9U, 0x73ACU, 30U, checkpoint);

    const auto first_stack_high = checkpoint.stack_pointer;
    const auto first_stack_low = static_cast<std::uint8_t>(first_stack_high - 1U);
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x73ACU},
        .logical_address = static_cast<std::uint16_t>(0x2100U + first_stack_high),
        .value = 0x73U,
        .master_cycle_offset = 30U,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x73ACU},
        .logical_address = static_cast<std::uint16_t>(0x2100U + first_stack_low),
        .value = 0xAEU,
        .master_cycle_offset = 30U,
    });
    if (!stage_write(0x2100U + first_stack_high, 0x73U)
        || !stage_write(0x2100U + first_stack_low, 0xAEU)) {
        return std::nullopt;
    }
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(0x73ACU, 0x6966U, 51U, checkpoint);

    RoutineEntry collision_entry{
        .routine = {.physical_bank = 0x03U, .logical_pc = 0x6966U},
        .registers = checkpoint,
    };
    auto collision = replace_collision_table_interpreter(collision_entry, staged, rom);
    // This used to require the child to be exactly the 165-cycle one-pass
    // collision-free transcript, on the grounds that a longer record could take
    // an IRQ at the interpreter return boundary. That was true of an atomic
    // child. The interpreter is an instruction-boundary transcript now: it
    // checkpoints every instruction it walks, so a longer child brings more
    // boundaries across rather than fewer, and there is no stretch of this
    // transcript an interrupt cannot land in. What the length gate was really
    // standing in for is the two things below -- the child's own return
    // instruction, which the 165-cycle transcript always reached at $6988 and a
    // longer one need not, and the transaction caps.
    constexpr std::size_t kMaximumCheckpoints = 2048U;
    constexpr std::size_t kMaximumWrites = 512U;
    if (!collision || collision->checkpoints.empty()
        || collision->return_instruction.physical_bank != 0x03U
        || replacement.checkpoints.size() + collision->checkpoints.size()
            >= kMaximumCheckpoints
        || replacement.work_ram_writes.size()
                + collision->work_ram_writes.size()
            >= kMaximumWrites) {
        return std::nullopt;
    }
    for (const auto& write : collision->work_ram_writes) {
        if (!stage_write(write.logical_address, write.value)) {
            return std::nullopt;
        }
    }
    constexpr std::uint32_t kCollisionOffset = 51U;
    append_child(*collision, kCollisionOffset);
    if ((collision->output_registers.status & 0x01U) != 0U) {
        return std::nullopt;
    }

    checkpoint = collision->output_registers;
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer + 2U);
    const auto collision_end = kCollisionOffset + collision->body_master_cycles;
    // The collision child stops in front of its own RTS, which is this walk's
    // to transcribe. The interpreter has four of them -- $6988, $6994, $69A1
    // and $6A37 -- so take the address from the child rather than assuming the
    // one the 165-cycle transcript always reached. Its 21 master cycles expose
    // caller PC 73AF before the parent's carry branch; checkpoint both
    // boundaries so an IRQ stacks the same return PC as ROM.
    append_checkpoint(collision->return_instruction.logical_pc, 0x73AFU,
        collision_end + 21U, checkpoint);
    append_checkpoint(0x73AFU, 0x73B1U, collision_end + 27U, checkpoint);

    const auto timer = static_cast<std::uint8_t>(parent.timer - 1U);
    checkpoint.status = set_nz(checkpoint.status, timer);
    constexpr std::uint32_t kTimerWriteOffset = 99U;
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x73B1U},
        .logical_address = static_cast<std::uint16_t>(0x3A00U + parent.slot),
        .value = timer,
        .master_cycle_offset = collision->body_master_cycles + kTimerWriteOffset,
    });
    if (!stage_write(static_cast<std::uint16_t>(0x3A00U + parent.slot), timer)) {
        return std::nullopt;
    }
    append_checkpoint(0x73B1U, 0x73B4U,
        collision->body_master_cycles + kTimerWriteOffset, checkpoint);

    std::uint32_t hit_gate_offset = 0U;
    if (timer != 0U) {
        append_checkpoint(0x73B4U, 0x73C8U,
            collision->body_master_cycles + 111U, checkpoint);
        hit_gate_offset = collision->body_master_cycles + 132U;
    } else {
        append_checkpoint(0x73B4U, 0x73B6U,
            collision->body_master_cycles + 111U, checkpoint);
        checkpoint.accumulator = 0x04U;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0x73B6U, 0x73B8U,
            collision->body_master_cycles + 117U, checkpoint);
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x73B8U},
            .logical_address = static_cast<std::uint16_t>(0x3A00U + parent.slot),
            .value = 0x04U,
            .master_cycle_offset = collision->body_master_cycles + 126U,
        });
        if (!stage_write(static_cast<std::uint16_t>(0x3A00U + parent.slot), 0x04U)) {
            return std::nullopt;
        }
        append_checkpoint(0x73B8U, 0x73BBU,
            collision->body_master_cycles + 126U, checkpoint);

        const auto next_phase = static_cast<std::uint8_t>(parent.phase + 1U);
        checkpoint.status = set_nz(checkpoint.status, next_phase);
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x73BBU},
            .logical_address = static_cast<std::uint16_t>(0x3980U + parent.slot),
            .value = next_phase,
            .master_cycle_offset = collision->body_master_cycles + 147U,
        });
        if (!stage_write(static_cast<std::uint16_t>(0x3980U + parent.slot), next_phase)) {
            return std::nullopt;
        }
        append_checkpoint(0x73BBU, 0x73BEU,
            collision->body_master_cycles + 147U, checkpoint);
        checkpoint.accumulator = next_phase;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0x73BEU, 0x73C1U,
            collision->body_master_cycles + 168U, checkpoint);
        checkpoint.status = set_nz(
            static_cast<std::uint8_t>(checkpoint.status & 0xFEU),
            static_cast<std::uint8_t>(checkpoint.accumulator - 0x04U));
        if (checkpoint.accumulator >= 0x04U) {
            checkpoint.status = static_cast<std::uint8_t>(checkpoint.status | 0x01U);
        }
        append_checkpoint(0x73C1U, 0x73C3U,
            collision->body_master_cycles + 174U, checkpoint);
        if (next_phase < 0x04U) {
            append_checkpoint(0x73C3U, 0x73C8U,
                collision->body_master_cycles + 180U, checkpoint);
            hit_gate_offset = collision->body_master_cycles + 201U;
        } else {
            append_checkpoint(0x73C3U, 0x73C5U,
                collision->body_master_cycles + 180U, checkpoint);
            replacement.work_ram_writes.push_back({
                .instruction = {0x03U, 0x73C5U},
                .logical_address = static_cast<std::uint16_t>(0x3980U + parent.slot),
                .value = 0x00U,
                .master_cycle_offset = collision->body_master_cycles + 189U,
            });
            if (!stage_write(static_cast<std::uint16_t>(0x3980U + parent.slot), 0x00U)) {
                return std::nullopt;
            }
            append_checkpoint(0x73C5U, 0x73C8U,
                collision->body_master_cycles + 189U, checkpoint);
            hit_gate_offset = collision->body_master_cycles + 210U;
        }
    }

    const auto hit_stack_high = checkpoint.stack_pointer;
    const auto hit_stack_low = static_cast<std::uint8_t>(hit_stack_high - 1U);
    const auto hit_stack_write = hit_gate_offset - 21U;
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x73C8U},
        .logical_address = static_cast<std::uint16_t>(0x2100U + hit_stack_high),
        .value = 0x73U,
        .master_cycle_offset = hit_stack_write,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x73C8U},
        .logical_address = static_cast<std::uint16_t>(0x2100U + hit_stack_low),
        .value = 0xC9U,
        .master_cycle_offset = hit_stack_write,
    });
    if (!stage_write(0x2100U + hit_stack_high, 0x73U)
        || !stage_write(0x2100U + hit_stack_low, 0xC9U)) {
        return std::nullopt;
    }
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    // The JSR's interrupt sample trails its nominal return-address write by
    // one HuC6280 cycle. This preserves an IRQ that vectors at the first
    // instruction of 73D8 (the handler stacks 73D8, not 73DB).
    append_checkpoint(0x73C8U, 0x73D8U, hit_gate_offset + 3U, checkpoint);

    RoutineEntry hit_entry{
        .routine = {.physical_bank = 0x03U, .logical_pc = 0x73D8U},
        .registers = checkpoint,
    };
    auto hit_gate = replace_knight_hit_gate(hit_entry, staged);
    if (!hit_gate || hit_gate->checkpoints.empty()
        || hit_gate->return_instruction
            != RoutineKey{.physical_bank = 0x03U, .logical_pc = 0x73FDU}) {
        return std::nullopt;
    }
    append_child(*hit_gate, hit_gate_offset);
    if ((hit_gate->output_registers.status & 0x01U) != 0U) {
        return std::nullopt;
    }

    checkpoint = hit_gate->output_registers;
    const auto hit_end = hit_gate_offset + hit_gate->body_master_cycles;
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer + 2U);
    append_checkpoint(0x73CAU, 0x73CCU, hit_end + 12U, checkpoint);
    // The boundary the hit gate's own `RTS` lands on. Nothing reached it while
    // only the 165-cycle child was admitted, so its absence was invisible; an
    // interrupt that arrives here has to stack 73CA, and without a checkpoint
    // for it the core defers to the one after the branch and stacks 73CC.
    append_checkpoint(0x73FDU, 0x73CAU, hit_end + 24U, checkpoint);
    checkpoint.program_counter = 0x73CCU;
    // An IRQ after the carry-clear branch resumes at the outer RTS, but the
    // original handler attributes its stacked return to the branch at 73CA.
    append_checkpoint(0x73CAU, 0x73CCU, hit_end + 30U, checkpoint);
    replacement.return_instruction = {.physical_bank = 0x03U, .logical_pc = 0x73CCU};
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = hit_end + 30U;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_spider_appearance(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x65A4U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_spider_appearance appearance{};
    if (!dc_classify_spider_appearance(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &appearance)) {
        throw std::runtime_error(
            "Native 65A4 replacement rejected the work-RAM snapshot.");
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
                .instruction = {0x03U, instruction_pc},
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
                .instruction = {0x03U, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
        };

    auto output = entry.registers;
    output.status = set_nz(output.status, appearance.decremented_timer);
    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x65BBU,
        },
    };
    add_write(
        replacement,
        0x65A4U,
        static_cast<std::uint16_t>(0x3A00U + appearance.slot),
        appearance.decremented_timer,
        21U);
    append_checkpoint(replacement, 0x65A4U, 0x65A7U, 21U, output);

    // All three arms below meet at the `RTS` at $65BB and absorb it. The
    // routine is a leaf -- it decrements a timer, advances an appearance index
    // and wraps it -- so the boundary the walk ends on is the caller's, and the
    // only thing that differs between the arms is where in the transcript the
    // return falls.
    const auto absorb =
        [&entry, work_ram](
            RoutineReplacement& replacement,
            const std::uint32_t return_offset,
            CpuRegisters at_return) {
            at_return.program_counter = 0x65BBU;
            absorb_return(
                replacement,
                work_ram,
                entry.registers.stack_pointer,
                {0x03U, 0x65BBU},
                return_offset,
                at_return);
        };

    if (appearance.path == DC_SPIDER_APPEARANCE_WAITING) {
        append_checkpoint(replacement, 0x65A7U, 0x65BBU, 33U, output);
        absorb(replacement, 33U, output);
        return replacement;
    }

    append_checkpoint(replacement, 0x65A7U, 0x65A9U, 27U, output);
    output.accumulator = 0x08U;
    output.status = set_nz(output.status, output.accumulator);
    append_checkpoint(replacement, 0x65A9U, 0x65ABU, 33U, output);
    add_write(
        replacement,
        0x65ABU,
        static_cast<std::uint16_t>(0x3A00U + appearance.slot),
        output.accumulator,
        48U);
    append_checkpoint(replacement, 0x65ABU, 0x65AEU, 48U, output);
    output.status = set_nz(output.status, appearance.advanced_appearance);
    add_write(
        replacement,
        0x65AEU,
        static_cast<std::uint16_t>(0x3980U + appearance.slot),
        appearance.advanced_appearance,
        69U);
    append_checkpoint(replacement, 0x65AEU, 0x65B1U, 69U, output);
    output.accumulator = appearance.advanced_appearance;
    output.status = set_nz(output.status, output.accumulator);
    append_checkpoint(replacement, 0x65B1U, 0x65B4U, 84U, output);
    output.status = set_compare(output.status, output.accumulator, 0x04U);
    append_checkpoint(replacement, 0x65B4U, 0x65B6U, 90U, output);

    if (appearance.path == DC_SPIDER_APPEARANCE_ADVANCE) {
        append_checkpoint(replacement, 0x65B6U, 0x65BBU, 102U, output);
        absorb(replacement, 102U, output);
        return replacement;
    }

    add_write(
        replacement,
        0x65B8U,
        static_cast<std::uint16_t>(0x3980U + appearance.slot),
        0x00U,
        111U);
    append_checkpoint(replacement, 0x65B6U, 0x65B8U, 96U, output);
    append_checkpoint(replacement, 0x65B8U, 0x65BBU, 111U, output);
    absorb(replacement, 111U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_tower_insect_animation(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // This is the direct BSR target inside the dynamic Tower-insect
    // controller. It owns only the frame-table selection and phase timer;
    // collision, scoring, and child lifetime stay with the assembly parent.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x6054U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_tower_insect_animation animation{};
    if (!dc_classify_tower_insect_animation(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &animation)) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto set_asl = [&set_nz](
                             std::uint8_t status,
                             const std::uint8_t value,
                             const std::uint8_t source) {
        status = set_nz(
            static_cast<std::uint8_t>(status & 0xFEU), value);
        if ((source & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x01U);
        }
        return status;
    };
    const auto set_adc = [](std::uint8_t status,
                            const std::uint8_t left,
                            const std::uint8_t right) {
        const auto sum = static_cast<std::uint16_t>(left)
            + static_cast<std::uint16_t>(right);
        const auto result = static_cast<std::uint8_t>(sum);
        status = static_cast<std::uint8_t>(status & 0x3CU);
        if (result == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((result & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        if ((sum & 0x100U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x01U);
        }
        if (((~(left ^ right) & (left ^ result)) & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x40U);
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
                .instruction = {0x03U, instruction_pc},
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
                .instruction = {0x03U, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
        };

    const auto masked_cadence = static_cast<std::uint8_t>(
        animation.cadence & 0xFEU);
    const auto doubled_cadence = static_cast<std::uint8_t>(
        masked_cadence << 1U);
    const auto masked_phase = static_cast<std::uint8_t>(
        animation.phase & 0x03U);
    const auto lookup_index = static_cast<std::uint8_t>(
        masked_phase + doubled_cadence);

    auto checkpoint = entry.registers;
    checkpoint.accumulator = animation.cadence;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);

    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x03U, .logical_pc = 0x6078U},
    };
    append_checkpoint(replacement, 0x6054U, 0x6057U, 15U, checkpoint);

    checkpoint.accumulator = masked_cadence;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x6057U, 0x6059U, 21U, checkpoint);

    checkpoint.accumulator = doubled_cadence;
    checkpoint.status = set_asl(
        checkpoint.status, checkpoint.accumulator, masked_cadence);
    append_checkpoint(replacement, 0x6059U, 0x605AU, 27U, checkpoint);
    add_write(replacement, 0x605AU, 0x2026U, doubled_cadence, 39U);
    append_checkpoint(replacement, 0x605AU, 0x605CU, 39U, checkpoint);

    checkpoint.accumulator = animation.phase;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x605CU, 0x605FU, 54U, checkpoint);

    checkpoint.accumulator = masked_phase;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x605FU, 0x6061U, 60U, checkpoint);

    checkpoint.status = static_cast<std::uint8_t>(checkpoint.status & 0xFEU);
    append_checkpoint(replacement, 0x6061U, 0x6062U, 66U, checkpoint);

    // `ADC $26` is zero page and costs 12, the same as the `STA $26` at 605A
    // that this walk already charges 12. It was charged 9, which put every
    // boundary from here to 606B three master cycles early -- including both
    // remaining writes -- and the taken branch at 606E was charged 15 to
    // absorb it. The two errors cancel in the body total, so the routine ended
    // at the right cycle and only the writes inside it were misplaced.
    checkpoint.accumulator = lookup_index;
    checkpoint.status = set_adc(
        checkpoint.status, masked_phase, doubled_cadence);
    append_checkpoint(replacement, 0x6062U, 0x6064U, 78U, checkpoint);

    checkpoint.y = lookup_index;
    append_checkpoint(replacement, 0x6064U, 0x6065U, 84U, checkpoint);

    checkpoint.accumulator = animation.frame;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x6065U, 0x6068U, 99U, checkpoint);
    add_write(
        replacement,
        0x6068U,
        static_cast<std::uint16_t>(0x3980U + animation.slot),
        animation.frame,
        114U);
    append_checkpoint(replacement, 0x6068U, 0x606BU, 114U, checkpoint);

    checkpoint.status = set_nz(
        checkpoint.status, animation.decremented_timer);
    add_write(
        replacement,
        0x606BU,
        static_cast<std::uint16_t>(0x3A00U + animation.slot),
        animation.decremented_timer,
        135U);
    append_checkpoint(replacement, 0x606BU, 0x606EU, 135U, checkpoint);

    // Both arms meet at the `RTS` at $6078 and absorb it. This is a `BSR`
    // target rather than a `JSR` one, which changes nothing here: `BSR` pushes
    // the address of its own last byte the same way, so the return reads the
    // same two bytes off the same two stack slots.
    const auto absorb =
        [&entry, work_ram](
            RoutineReplacement& replacement,
            const std::uint32_t return_offset,
            CpuRegisters at_return) {
            at_return.program_counter = 0x6078U;
            absorb_return(
                replacement,
                work_ram,
                entry.registers.stack_pointer,
                {0x03U, 0x6078U},
                return_offset,
                at_return);
        };

    if (animation.path == DC_TOWER_INSECT_ANIMATION_WAITING) {
        append_checkpoint(replacement, 0x606EU, 0x6078U, 147U, checkpoint);
        absorb(replacement, 147U, checkpoint);
        return replacement;
    }

    append_checkpoint(replacement, 0x606EU, 0x6070U, 141U, checkpoint);
    checkpoint.accumulator = 0x06U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x6070U, 0x6072U, 147U, checkpoint);
    add_write(
        replacement,
        0x6072U,
        static_cast<std::uint16_t>(0x3A00U + animation.slot),
        checkpoint.accumulator,
        162U);
    append_checkpoint(replacement, 0x6072U, 0x6075U, 162U, checkpoint);

    checkpoint.status = set_nz(checkpoint.status, animation.advanced_phase);
    add_write(
        replacement,
        0x6075U,
        static_cast<std::uint16_t>(0x3C80U + animation.slot),
        animation.advanced_phase,
        183U);
    append_checkpoint(replacement, 0x6075U, 0x6078U, 183U, checkpoint);
    absorb(replacement, 183U, checkpoint);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_tower_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x60B3U,
    };
    constexpr std::uint32_t kBodyCycles = 249U;
    if (entry.routine != kRoutine || entry.registers.x != 0x2DU
        || entry.registers.accumulator != 0U || entry.registers.y != 0x0BU
        || entry.registers.stack_pointer != 0xF3U
        || entry.registers.status != 0x02U) {
        return std::nullopt;
    }

    dc_tower_initializer initializer{};
    if (!dc_classify_tower_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x60U;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    add_write(0x03U, 0x60B3U, 0x2D3FU, 0x00U, 15U);
    add_write(0x03U, 0x60B8U,
              static_cast<std::uint16_t>(0x3B00U + initializer.slot),
              0x03U, 36U);
    add_write(0x03U, 0x60BDU, 0x2D4DU, 0x05U, 57U);
    add_write(0x03U, 0x60C0U, 0x21F3U, 0x60U, 57U);
    add_write(0x03U, 0x60C0U, 0x21F2U, 0xC2U, 57U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0xC2U, 114U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x60U, 138U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0xC3U, 183U);
    add_write(0x01U, 0xC5E1U,
              static_cast<std::uint16_t>(0x3F00U + initializer.slot),
              0xC3U, 222U);
    add_write(0x01U, 0xC5E6U,
              static_cast<std::uint16_t>(0x3F80U + initializer.slot),
              0x60U, 249U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 246U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_bumper_initializers(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kLeftRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x789AU,
    };
    constexpr RoutineKey kRightRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x78A4U,
    };
    const bool left = entry.routine == kLeftRoutine;
    const bool right = entry.routine == kRightRoutine;
    if (!left && !right) {
        return std::nullopt;
    }

    const auto expected_slot = static_cast<std::uint8_t>(left ? 0x14U : 0x15U);
    const auto expected_side = static_cast<std::uint8_t>(left ? 0U : 1U);
    const auto expected_status = static_cast<std::uint8_t>(left ? 0x02U : 0U);
    const auto expected_side_pc = static_cast<std::uint16_t>(
        left ? 0x789AU : 0x78A6U);
    const auto expected_type_pc = static_cast<std::uint16_t>(
        left ? 0x789FU : 0x78ABU);
    const auto body_cycles = static_cast<std::uint32_t>(left ? 342U : 336U);
    const auto side_write_cycle = static_cast<std::uint32_t>(left ? 15U : 21U);
    const auto type_write_cycle = static_cast<std::uint32_t>(left ? 36U : 42U);
    const auto first_collision_write_cycle =
        static_cast<std::uint32_t>(left ? 69U : 63U);
    const auto second_collision_write_cycle =
        static_cast<std::uint32_t>(left ? 105U : 99U);
    const auto scheduler_call_cycle = static_cast<std::uint32_t>(
        left ? 150U : 144U);
    if (entry.registers.x != expected_slot
        || entry.registers.accumulator != expected_side
        || entry.registers.y != 0x0BU
        || entry.registers.stack_pointer != 0xF3U
        || entry.registers.status != expected_status) {
        return std::nullopt;
    }

    dc_bumper_initializer initializer{};
    if (!dc_classify_bumper_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)
        || initializer.side != expected_side) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x78U;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = body_cycles,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    add_write(0x03U, expected_side_pc,
              static_cast<std::uint16_t>(0x3E00U + initializer.slot),
              initializer.side, side_write_cycle);
    add_write(0x03U, expected_type_pc,
              static_cast<std::uint16_t>(0x3D00U + initializer.slot),
              static_cast<std::uint8_t>(0x86U + initializer.side),
              type_write_cycle);
    add_write(0x03U, 0x78B0U,
              static_cast<std::uint16_t>(0x3800U + initializer.slot),
              0x50U, first_collision_write_cycle);
    add_write(0x03U, 0x78B8U,
              static_cast<std::uint16_t>(0x3800U + initializer.slot),
              0x50U, second_collision_write_cycle);
    add_write(0x03U, 0x78BBU,
              static_cast<std::uint16_t>(0x3E80U + initializer.slot),
              0U, scheduler_call_cycle - 30U);
    add_write(0x03U, 0x78BEU,
              static_cast<std::uint16_t>(0x3B00U + initializer.slot),
              0U, scheduler_call_cycle - 15U);
    add_write(0x03U, 0x78C1U,
              static_cast<std::uint16_t>(0x3B80U + initializer.slot),
              0U, scheduler_call_cycle);
    add_write(0x03U, 0x78C4U, 0x21F3U, 0x78U, scheduler_call_cycle);
    add_write(0x03U, 0x78C4U, 0x21F2U, 0xC6U, scheduler_call_cycle);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0xC6U,
              scheduler_call_cycle + 57U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x78U,
              scheduler_call_cycle + 81U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0xC7U,
              scheduler_call_cycle + 126U);
    add_write(0x01U, 0xC5E1U,
              static_cast<std::uint16_t>(0x3F00U + initializer.slot),
              0xC7U, scheduler_call_cycle + 165U);
    add_write(0x01U, 0xC5E6U,
              static_cast<std::uint16_t>(0x3F80U + initializer.slot),
              0x78U, scheduler_call_cycle + 192U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = body_cycles - 3U,
        .master_cycle_offset = body_cycles,
        .instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_bumper_child_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x79DEU,
    };
    constexpr std::uint32_t kBodyCycles = 402U;
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x15U || entry.registers.y < 0x71U
        || entry.registers.y > 0x76U || entry.registers.stack_pointer != 0xF7U
        || entry.registers.status != 0x02U) {
        return std::nullopt;
    }

    dc_bumper_child_initializer initializer{};
    if (!dc_classify_bumper_child_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, entry.registers.y,
            &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0x7A38U;
    output.accumulator = 0x01U;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x03U, .logical_pc = 0x7A38U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto add_write = [&replacement](
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = 0x03U, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    const auto child = initializer.child_slot;
    add_write(0x79E0U, static_cast<std::uint16_t>(0x3F00U + child), 0x71U, 21U);
    add_write(0x79E5U, static_cast<std::uint16_t>(0x3F80U + child), 0x7AU, 42U);
    add_write(0x79EAU, static_cast<std::uint16_t>(0x3000U + child), 0x83U, 63U);
    add_write(0x79F0U, static_cast<std::uint16_t>(0x3180U + child),
              initializer.parent_horizontal_low, 93U);
    add_write(0x79F6U, static_cast<std::uint16_t>(0x3200U + child),
              initializer.parent_horizontal_high, 123U);
    add_write(0x79FCU, static_cast<std::uint16_t>(0x3300U + child),
              initializer.parent_vertical_low, 153U);
    add_write(0x7A02U, static_cast<std::uint16_t>(0x3380U + child),
              initializer.parent_vertical_high, 183U);
    add_write(0x7A08U, static_cast<std::uint16_t>(0x3C80U + child),
              initializer.parent_appearance, 213U);
    add_write(0x7A0DU, static_cast<std::uint16_t>(0x3900U + child), 0x1BU, 234U);
    add_write(0x7A12U, static_cast<std::uint16_t>(0x3800U + child), 0x10U, 255U);
    add_write(0x7A17U, static_cast<std::uint16_t>(0x3D00U + child), 0x8CU, 276U);
    add_write(0x7A1CU, static_cast<std::uint16_t>(0x3980U + child), 0U, 297U);
    add_write(0x7A21U, static_cast<std::uint16_t>(0x3080U + child), 0x84U, 318U);
    add_write(0x7A26U, static_cast<std::uint16_t>(0x3880U + child), 0x80U, 339U);
    add_write(0x7A2BU, static_cast<std::uint16_t>(0x3A00U + child), 0x04U, 360U);
    add_write(0x7A30U, static_cast<std::uint16_t>(0x3B00U + child), 0x50U, 381U);
    add_write(0x7A35U, static_cast<std::uint16_t>(0x3C00U + child), 0x01U, 402U);
    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            return static_cast<std::uint8_t>(status | 0x02U);
        }
        if ((value & 0x80U) != 0U) {
            return static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto add_checkpoint = [&replacement](
                                    const std::uint16_t instruction,
                                    const std::uint16_t resume,
                                    const std::uint32_t cycle,
                                    auto registers,
                                    const std::uint32_t interrupt_sample = 0U) {
        registers.program_counter = resume;
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = interrupt_sample != 0U
                ? interrupt_sample
                : cycle - 3U,
            .master_cycle_offset = cycle,
            .instruction = {.physical_bank = 0x03U, .logical_pc = instruction},
            .registers = registers,
        });
    };
    auto checkpoint = entry.registers;
    checkpoint.accumulator = 0x71U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    add_checkpoint(0x79DEU, 0x79E0U, 15U, checkpoint);
    add_checkpoint(0x79E0U, 0x79E3U, 21U, checkpoint);
    checkpoint.accumulator = 0x7AU;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    add_checkpoint(0x79E3U, 0x79E5U, 36U, checkpoint);
    add_checkpoint(0x79E5U, 0x79E8U, 42U, checkpoint);
    checkpoint.accumulator = 0x83U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    add_checkpoint(0x79E8U, 0x79EAU, 57U, checkpoint);
    add_checkpoint(0x79EAU, 0x79EDU, 63U, checkpoint);
    checkpoint.accumulator = initializer.parent_horizontal_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    add_checkpoint(0x79EDU, 0x79F0U, 84U, checkpoint);
    add_checkpoint(0x79F0U, 0x79F3U, 93U, checkpoint);
    checkpoint.accumulator = initializer.parent_horizontal_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    add_checkpoint(0x79F3U, 0x79F6U, 114U, checkpoint);
    add_checkpoint(0x79F6U, 0x79F9U, 123U, checkpoint);
    checkpoint.accumulator = initializer.parent_vertical_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    add_checkpoint(0x79F9U, 0x79FCU, 144U, checkpoint);
    add_checkpoint(0x79FCU, 0x79FFU, 153U, checkpoint);
    checkpoint.accumulator = initializer.parent_vertical_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    // The frame interrupt samples during the indexed load.  Its architectural
    // state is visible six master cycles before the following store's normal
    // instruction boundary, so preserve that sampled point explicitly.
    add_checkpoint(0x79FFU, 0x7A02U, 168U, checkpoint, 165U);
    add_checkpoint(0x7A02U, 0x7A05U, 183U, checkpoint);
    checkpoint.accumulator = initializer.parent_appearance;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    add_checkpoint(0x7A05U, 0x7A08U, 204U, checkpoint);
    add_checkpoint(0x7A08U, 0x7A0BU, 213U, checkpoint);
    checkpoint.accumulator = 0x1BU;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    add_checkpoint(0x7A0BU, 0x7A0DU, 228U, checkpoint);
    add_checkpoint(0x7A0DU, 0x7A10U, 234U, checkpoint);
    checkpoint.accumulator = 0x10U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    add_checkpoint(0x7A10U, 0x7A12U, 249U, checkpoint);
    add_checkpoint(0x7A12U, 0x7A15U, 255U, checkpoint);
    checkpoint.accumulator = 0x8CU;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    add_checkpoint(0x7A15U, 0x7A17U, 270U, checkpoint);
    add_checkpoint(0x7A17U, 0x7A1AU, 276U, checkpoint);
    checkpoint.accumulator = 0U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    add_checkpoint(0x7A1AU, 0x7A1CU, 291U, checkpoint);
    add_checkpoint(0x7A1CU, 0x7A1FU, 297U, checkpoint);
    checkpoint.accumulator = 0x84U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    add_checkpoint(0x7A1FU, 0x7A21U, 312U, checkpoint);
    add_checkpoint(0x7A21U, 0x7A24U, 318U, checkpoint);
    checkpoint.accumulator = 0x80U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    add_checkpoint(0x7A24U, 0x7A26U, 333U, checkpoint);
    add_checkpoint(0x7A26U, 0x7A29U, 339U, checkpoint);
    checkpoint.accumulator = 0x04U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    add_checkpoint(0x7A29U, 0x7A2BU, 354U, checkpoint);
    add_checkpoint(0x7A2BU, 0x7A2EU, 360U, checkpoint);
    checkpoint.accumulator = 0x50U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    add_checkpoint(0x7A2EU, 0x7A30U, 375U, checkpoint);
    add_checkpoint(0x7A30U, 0x7A33U, 381U, checkpoint);
    checkpoint.accumulator = 0x01U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    add_checkpoint(0x7A33U, 0x7A35U, 396U, checkpoint);
    add_checkpoint(0x7A35U, 0x7A38U, kBodyCycles, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_tower_insect_source_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x5F22U,
    };
    constexpr std::uint32_t kBodyCycles = 192U;
    if (entry.routine != kRoutine || entry.registers.x != 0x2CU
        || entry.registers.accumulator != 0U || entry.registers.y != 0x0BU
        || entry.registers.stack_pointer != 0xF3U
        || entry.registers.status != 0x02U) {
        return std::nullopt;
    }

    dc_tower_insect_source_initializer initializer{};
    if (!dc_classify_tower_insect_source_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x5FU;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    add_write(0x02U, 0x5F22U, 0x21F3U, 0x5FU, 0U);
    add_write(0x02U, 0x5F22U, 0x21F2U, 0x24U, 0U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0x24U, 57U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x5FU, 81U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0x25U, 126U);
    add_write(0x01U, 0xC5E1U,
              static_cast<std::uint16_t>(0x3F00U + initializer.slot),
              0x25U, 165U);
    add_write(0x01U, 0xC5E6U,
              static_cast<std::uint16_t>(0x3F80U + initializer.slot),
              0x5FU, 192U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 189U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_tower_insect_source_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The direct source-controller entry only owns the child-count gate. The
    // zero-count group-clear score, audio, and release path stay in assembly.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x5F5FU,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_tower_insect_source_gate gate{};
    if (!dc_classify_tower_insect_source_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &gate)) {
        return std::nullopt;
    }
    if (gate.path != DC_TOWER_INSECT_SOURCE_GATE_WAITING) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };

    auto output = entry.registers;
    output.accumulator = gate.child_count;
    output.status = set_nz(output.status, output.accumulator);
    output.program_counter = 0x5F64U;

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x02U,
            .logical_pc = 0x5F64U,
        },
        .output_registers = output,
        .body_master_cycles = 21U,
    };
    const auto append_checkpoint =
        [&replacement](const std::uint16_t instruction_pc,
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
    append_checkpoint(0x5F5FU, 0x5F62U, 15U, output);
    append_checkpoint(0x5F62U, 0x5F64U, 21U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_tower_insect_source_countdown_wait(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x5F4AU,
    };
    constexpr std::uint32_t kBodyCycles = 33U;
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_tower_insect_source_countdown_wait wait{};
    if (!dc_classify_tower_insect_source_countdown_wait(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &wait)
        || wait.timer <= 1U) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };

    auto checkpoint = entry.registers;
    checkpoint.status = set_nz(checkpoint.status, wait.decremented_timer);
    auto output = checkpoint;
    output.program_counter = 0x5F5BU;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x5F5BU},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x5F4AU},
        .logical_address = static_cast<std::uint16_t>(0x3A00U + wait.slot),
        .value = wait.decremented_timer,
        .master_cycle_offset = 21U,
    });
    checkpoint.program_counter = 0x5F4DU;
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 18U,
        .master_cycle_offset = 21U,
        .instruction = {0x02U, 0x5F4AU},
        .registers = checkpoint,
    });
    checkpoint.program_counter = 0x5F5BU;
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 30U,
        .master_cycle_offset = 33U,
        .instruction = {0x02U, 0x5F4DU},
        .registers = checkpoint,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_giant_face_animator_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x766CU,
    };
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x37U || entry.registers.y != 0x0BU
        || entry.registers.stack_pointer != 0xF3U
        || entry.registers.status != 0x02U) {
        return std::nullopt;
    }

    dc_giant_face_animator_initializer initializer{};
    if (!dc_classify_giant_face_animator_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x76U;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = 255U,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    add_write(0x03U, 0x766EU, 0x3E37U, 1U, 21U);
    add_write(0x03U, 0x7673U, 0x3B80U + initializer.slot, 8U, 42U);
    add_write(0x03U, 0x7678U, 0x3A37U, 1U, 63U);
    add_write(0x03U, 0x767BU, 0x21F3U, 0x76U, 63U);
    add_write(0x03U, 0x767BU, 0x21F2U, 0x7DU, 63U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0x7DU, 120U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x76U, 144U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0x7EU, 189U);
    add_write(0x01U, 0xC5E1U, 0x3F37U, 0x7EU, 228U);
    add_write(0x01U, 0xC5E6U, 0x3FB7U, 0x76U, 255U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 252U,
        .master_cycle_offset = 255U,
        .instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_giant_face_crown_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x76F7U,
    };
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x39U || entry.registers.y != 0x0BU
        || entry.registers.stack_pointer != 0xF3U
        || entry.registers.status != 0x02U) {
        return std::nullopt;
    }

    dc_giant_face_crown_initializer initializer{};
    if (!dc_classify_giant_face_crown_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x76U;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = 192U,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    add_write(0x03U, 0x76F7U, 0x21F3U, 0x76U, 0U);
    add_write(0x03U, 0x76F7U, 0x21F2U, 0xF9U, 0U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0xF9U, 57U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x76U, 81U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0xFAU, 126U);
    add_write(0x01U, 0xC5E1U, 0x3F00U + initializer.slot, 0xFAU, 165U);
    add_write(0x01U, 0xC5E6U, 0x3F80U + initializer.slot, 0x76U, 192U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 189U,
        .master_cycle_offset = 192U,
        .instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_giant_face_mouth_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x7718U,
    };
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x38U || entry.registers.y != 0x0BU
        || entry.registers.stack_pointer != 0xF3U
        || entry.registers.status != 0x02U) {
        return std::nullopt;
    }

    dc_giant_face_mouth_initializer initializer{};
    if (!dc_classify_giant_face_mouth_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x77U;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = 243U,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    add_write(0x03U, 0x771DU, 0x3838U, 0U, 36U);
    add_write(0x03U, 0x7720U, 0x3980U + initializer.slot, 0U, 51U);
    add_write(0x03U, 0x7723U, 0x21F3U, 0x77U, 51U);
    add_write(0x03U, 0x7723U, 0x21F2U, 0x25U, 51U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0x25U, 108U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x77U, 132U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0x26U, 177U);
    add_write(0x01U, 0xC5E1U, 0x3F00U + initializer.slot, 0x26U, 216U);
    add_write(0x01U, 0xC5E6U, 0x3F80U + initializer.slot, 0x77U, 243U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 240U,
        .master_cycle_offset = 243U,
        .instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_giant_face_controller_initializers(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kFirstStage{
        .physical_bank = 0x03U,
        .logical_pc = 0x7428U,
    };
    constexpr RoutineKey kSecondStage{
        .physical_bank = 0x03U,
        .logical_pc = 0x742BU,
    };
    constexpr RoutineKey kFinalStage{
        .physical_bank = 0x03U,
        .logical_pc = 0x742EU,
    };
    const auto stage = entry.routine == kFirstStage ? 0U
        : entry.routine == kSecondStage ? 1U
        : entry.routine == kFinalStage ? 2U
        : 0xFFU;
    if (stage == 0xFFU) {
        return std::nullopt;
    }

    const auto expected_a = stage == 0U ? 0U : 0x74U;
    const auto expected_y = static_cast<std::uint8_t>(
        stage == 0U ? 0x0BU : stage);
    const auto expected_sp = static_cast<std::uint8_t>(0xF3U + stage * 4U);
    const auto expected_status = stage == 0U ? 0x02U
        : stage == 1U ? 0x10U : 0U;
    if (entry.registers.accumulator != expected_a || entry.registers.x != 0x36U
        || entry.registers.y != expected_y
        || entry.registers.stack_pointer != expected_sp
        || entry.registers.status != expected_status) {
        return std::nullopt;
    }

    dc_giant_face_controller_initializer initializer{};
    if (!dc_classify_giant_face_controller_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x,
            static_cast<std::uint8_t>(stage), &initializer)) {
        return std::nullopt;
    }

    const auto body_cycles = static_cast<std::uint32_t>(
        stage == 2U ? 306U : 192U);
    const auto scheduler_call_cycle = static_cast<std::uint32_t>(
        stage == 2U ? 114U : 0U);
    const auto scheduler_return_low = static_cast<std::uint8_t>(
        stage == 0U ? 0x2AU : stage == 1U ? 0x2DU : 0xD7U);

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x74U;
    output.status = static_cast<std::uint8_t>(
        stage == 0U ? 0U : expected_status);
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = body_cycles,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    if (stage == 2U) {
        add_write(0x03U, 0x74C6U, 0x2258U, 0U, 54U);
        add_write(0x03U, 0x74C9U, 0x2D0DU, 0U, 69U);
        add_write(0x03U, 0x74CCU, 0x2D0CU, 0U, 84U);
        add_write(0x03U, 0x74CFU, 0x2D0EU, 0U, 99U);
        add_write(0x03U, 0x74D2U, 0x3E36U, 0U, 114U);
    }
    const auto scheduler_call_pc = static_cast<std::uint16_t>(
        stage == 0U ? 0x7428U : stage == 1U ? 0x742BU : 0x74D5U);
    add_write(0x03U, scheduler_call_pc,
              static_cast<std::uint16_t>(0x2100U + expected_sp), 0x74U,
              scheduler_call_cycle);
    add_write(0x03U, scheduler_call_pc,
              static_cast<std::uint16_t>(0x2100U + expected_sp - 1U),
              scheduler_return_low, scheduler_call_cycle);
    add_write(0x01U, 0xC5CAU, 0x2026U, scheduler_return_low,
              scheduler_call_cycle + 57U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x74U,
              scheduler_call_cycle + 81U);
    add_write(0x01U, 0xC5D9U, 0x2026U,
              static_cast<std::uint8_t>(scheduler_return_low + 1U),
              scheduler_call_cycle + 126U);
    add_write(0x01U, 0xC5E1U, 0x3F36U,
              static_cast<std::uint8_t>(scheduler_return_low + 1U),
              scheduler_call_cycle + 165U);
    add_write(0x01U, 0xC5E6U, 0x3FB6U, 0x74U,
              scheduler_call_cycle + 192U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = body_cycles - 3U,
        .master_cycle_offset = body_cycles,
        .instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_small_soldier_source_initializers(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kLeftRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x7D4DU,
    };
    constexpr RoutineKey kRightRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x7DE9U,
    };
    const bool left = entry.routine == kLeftRoutine;
    const bool right = entry.routine == kRightRoutine;
    if (!left && !right) {
        return std::nullopt;
    }

    const auto expected_slot = static_cast<std::uint8_t>(left ? 0x34U : 0x35U);
    const auto expected_status = static_cast<std::uint8_t>(0x02U);
    if (entry.registers.x != expected_slot || entry.registers.accumulator != 0U
        || entry.registers.y != 0x0BU
        || entry.registers.stack_pointer != 0xF3U
        || entry.registers.status != expected_status) {
        return std::nullopt;
    }

    dc_small_soldier_source_initializer initializer{};
    if (!dc_classify_small_soldier_source_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    const auto side = initializer.side;
    const auto first_collision_id = static_cast<std::uint8_t>(
        left ? 0x0AU : 0x0CU);
    const auto second_collision_id = static_cast<std::uint8_t>(
        left ? 0x0BU : 0x0DU);
    const auto routine = static_cast<std::uint16_t>(left ? 0x7D4DU : 0x7DE9U);
    const auto scheduler_call_pc = static_cast<std::uint16_t>(
        left ? 0x7D6CU : 0x7E0AU);
    const auto scheduler_return_low = static_cast<std::uint8_t>(
        left ? 0x6EU : 0x0CU);
    const auto scheduler_return_high = static_cast<std::uint8_t>(
        left ? 0x7DU : 0x7EU);
    const auto body_cycles = static_cast<std::uint32_t>(left ? 327U : 333U);

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = scheduler_return_high;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = body_cycles,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    add_write(0x03U, static_cast<std::uint16_t>(routine + 2U),
              static_cast<std::uint16_t>(0x3700U + initializer.slot),
              first_collision_id, 21U);
    add_write(0x03U, static_cast<std::uint16_t>(routine + 7U),
              static_cast<std::uint16_t>(0x3780U + initializer.slot),
              second_collision_id, 42U);
    add_write(0x03U, static_cast<std::uint16_t>(routine + (left ? 10U : 12U)),
              static_cast<std::uint16_t>(0x3480U + initializer.slot), side,
              left ? 57U : 63U);
    add_write(0x03U, static_cast<std::uint16_t>(routine + (left ? 13U : 15U)),
              static_cast<std::uint16_t>(0x2D16U + side), 0U,
              left ? 72U : 78U);
    if (left) {
        add_write(0x03U, static_cast<std::uint16_t>(routine + 18U),
                  static_cast<std::uint16_t>(0x3A00U + initializer.slot),
                  1U, 93U);
        add_write(0x03U, static_cast<std::uint16_t>(routine + 23U),
                  static_cast<std::uint16_t>(0x3D00U + initializer.slot),
                  static_cast<std::uint8_t>(0x88U + side), 114U);
    } else {
        add_write(0x03U, static_cast<std::uint16_t>(routine + 20U),
                  static_cast<std::uint16_t>(0x3D00U + initializer.slot),
                  static_cast<std::uint8_t>(0x88U + side), 99U);
        add_write(0x03U, static_cast<std::uint16_t>(routine + 25U),
                  static_cast<std::uint16_t>(0x3A00U + initializer.slot),
                  1U, 120U);
    }
    const auto scheduler_call_cycle = static_cast<std::uint32_t>(
        left ? 135U : 141U);
    add_write(0x03U, static_cast<std::uint16_t>(routine + (left ? 28U : 30U)),
              static_cast<std::uint16_t>(0x3B00U + initializer.slot), 5U,
              scheduler_call_cycle);
    add_write(0x03U, scheduler_call_pc, 0x21F3U, scheduler_return_high,
              scheduler_call_cycle);
    add_write(0x03U, scheduler_call_pc, 0x21F2U, scheduler_return_low,
              scheduler_call_cycle);
    add_write(0x01U, 0xC5CAU, 0x2026U, scheduler_return_low,
              scheduler_call_cycle + 57U);
    add_write(0x01U, 0xC5CDU, 0x2027U, scheduler_return_high,
              scheduler_call_cycle + 81U);
    add_write(0x01U, 0xC5D9U, 0x2026U,
              static_cast<std::uint8_t>(scheduler_return_low + 1U),
              scheduler_call_cycle + 126U);
    add_write(0x01U, 0xC5E1U,
              static_cast<std::uint16_t>(0x3F00U + initializer.slot),
              static_cast<std::uint8_t>(scheduler_return_low + 1U),
              scheduler_call_cycle + 165U);
    add_write(0x01U, 0xC5E6U,
              static_cast<std::uint16_t>(0x3F80U + initializer.slot),
              scheduler_return_high, scheduler_call_cycle + 192U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = body_cycles - 3U,
        .master_cycle_offset = body_cycles,
        .instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_small_soldier_delayed_spawn_gates(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kLeftRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x7D6FU,
    };
    constexpr RoutineKey kRightRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x7E0DU,
    };
    const bool left = entry.routine == kLeftRoutine;
    const bool right = entry.routine == kRightRoutine;
    if (!left && !right) {
        return std::nullopt;
    }
    if (entry.master_cycles_until_user_event <= 33U
        || entry.master_cycles_until_timer_interrupt <= 33U) {
        return std::nullopt;
    }

    const auto expected_slot = static_cast<std::uint8_t>(left ? 0x34U : 0x35U);
    dc_small_soldier_delayed_spawn_gate gate{};
    if (!dc_classify_small_soldier_delayed_spawn_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &gate)) {
        throw std::runtime_error(
            "Native Small-Soldier delayed-spawn replacement rejected the "
            "work-RAM snapshot.");
    }
    if (gate.slot != expected_slot
        || gate.path != DC_SMALL_SOLDIER_DELAYED_SPAWN_GATE_WAITING) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto return_pc = static_cast<std::uint16_t>(left ? 0x7D81U : 0x7E1FU);
    auto output = entry.registers;
    output.status = set_nz(output.status, gate.timer_after);
    output.program_counter = return_pc;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x03U, .logical_pc = return_pc},
        .output_registers = output,
        .body_master_cycles = 33U,
    };
    replacement.work_ram_writes.push_back({
        .instruction = entry.routine,
        .logical_address = static_cast<std::uint16_t>(0x3A00U + gate.slot),
        .value = gate.timer_after,
        .master_cycle_offset = 21U,
    });
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 30U,
        .master_cycle_offset = 33U,
        .instruction = {.physical_bank = 0x03U, .logical_pc = return_pc},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_small_soldier_source_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kLeftRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x7D85U,
    };
    constexpr RoutineKey kRightRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x7E23U,
    };
    if (entry.routine != kLeftRoutine && entry.routine != kRightRoutine) {
        return std::nullopt;
    }

    dc_small_soldier_source_gate gate{};
    if (!dc_classify_small_soldier_source_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &gate)) {
        throw std::runtime_error(
            "Native Small-Soldier source replacement rejected the work-RAM "
            "snapshot.");
    }
    const bool is_left = entry.routine == kLeftRoutine;
    if (gate.path != DC_SMALL_SOLDIER_SOURCE_GATE_WAITING
        || (is_left && gate.slot != 0x34U)
        || (!is_left && gate.slot != 0x35U)) {
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
    const std::uint16_t load_pc = is_left ? 0x7D85U : 0x7E23U;
    const std::uint16_t branch_pc = is_left ? 0x7D88U : 0x7E26U;
    const std::uint16_t return_pc = is_left ? 0x7D8AU : 0x7E28U;

    auto output = entry.registers;
    output.accumulator = gate.group_count;
    output.status = set_nz(output.status, output.accumulator);
    output.program_counter = return_pc;

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = return_pc,
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
                .interrupt_sample_master_cycle_offset = master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
    };
    append_checkpoint(load_pc, branch_pc, 15U, output);
    append_checkpoint(branch_pc, return_pc, 21U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_sorcerer_color_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x57E9U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_sorcerer_color_gate gate{};
    if (!dc_classify_sorcerer_color_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &gate)) {
        throw std::runtime_error(
            "Native Sorcerer color-gate replacement rejected the work-RAM "
            "snapshot.");
    }
    if (gate.path != DC_SORCERER_COLOR_GATE_WAITING) {
        return std::nullopt;
    }

    const auto set_compare =
        [](std::uint8_t status,
           const std::uint8_t left,
           const std::uint8_t right) {
            const std::uint8_t difference =
                static_cast<std::uint8_t>(left - right);
            status = static_cast<std::uint8_t>(status & 0x7CU);
            if (left >= right) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if (difference == 0U) {
                status = static_cast<std::uint8_t>(status | 0x02U);
            } else if ((difference & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x80U);
            }
            return status;
        };

    auto output = entry.registers;
    output.accumulator = gate.shared_color;
    output.status = set_compare(
        output.status, gate.shared_color, gate.cached_color);
    output.program_counter = 0x57F7U;

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x02U,
            .logical_pc = 0x57F7U,
        },
        .output_registers = output,
        .body_master_cycles = 42U,
    };
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
    auto checkpoint = entry.registers;
    checkpoint.accumulator = gate.shared_color;
    // `LDA $2D40` sets N and Z. Publishing the caller's flags here instead put
    // the wrong status on the stack for any interrupt that landed on this
    // boundary -- and this routine writes nothing, so a write trace could
    // never have shown it.
    checkpoint.status = static_cast<std::uint8_t>(checkpoint.status & 0x7DU);
    if (gate.shared_color == 0U) {
        checkpoint.status = static_cast<std::uint8_t>(checkpoint.status | 0x02U);
    } else if ((gate.shared_color & 0x80U) != 0U) {
        checkpoint.status = static_cast<std::uint8_t>(checkpoint.status | 0x80U);
    }
    append_checkpoint(0x57E9U, 0x57ECU, 15U, checkpoint);
    checkpoint.status = output.status;
    // `CMP $3980,X` is absolute-indexed and costs 15, not 18. The extra three
    // cycles were taken back off the taken branch below, which charged 9
    // instead of 12, so the 42-cycle body was right and only this boundary
    // moved.
    append_checkpoint(0x57ECU, 0x57EFU, 30U, checkpoint);
    append_checkpoint(0x57EFU, 0x57F7U, 42U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_sorcerer_staging_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x57F8U};
    constexpr std::uint32_t kBodyCycles = 249U;
    if (entry.routine != kRoutine || entry.registers.y != 0x0BU
        || entry.registers.stack_pointer != 0xF3U) {
        return std::nullopt;
    }

    dc_sorcerer_staging_initializer initializer{};
    if (!dc_classify_sorcerer_staging_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, entry.registers.accumulator,
            &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.accumulator = 0x58U;
    output.status = 0U;
    output.program_counter = 0xC5E9U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto slot_address = [slot = initializer.slot](const std::uint16_t base) {
        return static_cast<std::uint16_t>(base + slot);
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    add_write(0x02U, 0x57F8U, slot_address(0x3700U), initializer.route_index, 15U);
    add_write(0x02U, 0x57FDU, slot_address(0x3800U), 0x10U, 36U);
    add_write(0x02U, 0x5802U, slot_address(0x3D00U), 0x8DU, 57U);
    add_write(0x02U, 0x5805U, 0x21F3U, 0x58U, 57U);
    add_write(0x02U, 0x5805U, 0x21F2U, 0x07U, 57U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0x07U, 114U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x58U, 138U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0x08U, 183U);
    add_write(0x01U, 0xC5E1U, slot_address(0x3F00U), 0x08U, 222U);
    add_write(0x01U, 0xC5E6U, slot_address(0x3F80U), 0x58U, kBodyCycles);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = kBodyCycles - 3U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {0x01U, 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_sorcerer_deployment_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x5808U};
    constexpr std::uint32_t kBodyCycles = 642U;
    static constexpr std::array<std::uint8_t, 8> kInitialVelocityLow{
        0x00U, 0x2CU, 0x58U, 0x84U, 0xB0U, 0xDCU, 0x08U, 0x34U,
    };
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x58U
        || entry.registers.stack_pointer != 0xF7U || entry.registers.status != 0U) {
        return std::nullopt;
    }

    dc_sorcerer_deployment_initializer initializer{};
    if (!dc_classify_sorcerer_deployment_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.y = 0x80U;
    output.status = 0x01U;
    output.program_counter = 0xC5E9U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto slot_address = [slot = initializer.slot](const std::uint16_t base) {
        return static_cast<std::uint16_t>(base + slot);
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    add_write(0x02U, 0x580AU, slot_address(0x3180U), 0xF6U, 21U);
    add_write(0x02U, 0x580FU, slot_address(0x3200U), 0x00U, 42U);
    add_write(0x02U, 0x5814U, slot_address(0x3300U), 0xC4U, 63U);
    add_write(0x02U, 0x5819U, slot_address(0x3380U), 0x00U, 84U);
    add_write(0x02U, 0x5821U, slot_address(0x3080U), initializer.flags_after, 120U);
    add_write(0x02U, 0x5827U, slot_address(0x3780U), initializer.collision_selector, 150U);
    add_write(0x02U, 0x5837U, slot_address(0x3880U), 0x80U, 225U);
    add_write(0x02U, 0x583CU, slot_address(0x3980U), 0x0AU, 246U);
    add_write(0x02U, 0x583FU, slot_address(0x3E00U), 0x00U, 261U);
    add_write(0x02U, 0x5842U, slot_address(0x3E80U), 0x00U, 276U);
    add_write(0x02U, 0x5845U, 0x2D0FU, initializer.live_count_after, 297U);
    add_write(0x02U, 0x584EU, 0x202EU, 0x00U, 339U);
    add_write(0x02U, 0x5858U, 0x202EU, initializer.initial_delay_high, 390U);
    add_write(0x02U, 0x5864U, slot_address(0x3B00U),
              kInitialVelocityLow[initializer.route_index], 423U);
    add_write(0x02U, 0x5869U, slot_address(0x3B80U), initializer.initial_delay_high, 450U);
    add_write(0x02U, 0x586CU, 0x21F7U, 0x58U, 450U);
    add_write(0x02U, 0x586CU, 0x21F6U, 0x6EU, 450U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0x6EU, 507U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x58U, 531U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0x6FU, 576U);
    add_write(0x01U, 0xC5E1U, slot_address(0x3F00U), 0x6FU, 615U);
    add_write(0x01U, 0xC5E6U, slot_address(0x3F80U), 0x58U, kBodyCycles);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = kBodyCycles - 3U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {0x01U, 0xC5E6U},
        .registers = output,
    });
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
        // Only the idle arm returns. Every other path branches into a phase
        // handler and ends there, so $78E6 is this arm's own `RTS` and nothing
        // else's.
        append_checkpoint(0x78CAU, 0x78E6U, 27U, output);
        absorb_return(
            replacement,
            work_ram,
            entry.registers.stack_pointer,
            {0x03U, 0x78E6U},
            27U,
            output);
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
replace_big_star_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x565CU};
    constexpr std::uint32_t kBodyCycles = 285U;
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.y != 0x0BU) {
        return std::nullopt;
    }
    const auto stack_return = proven_stack_return(entry, 0xF3U);
    if (!stack_return) {
        return std::nullopt;
    }

    dc_big_star_initializer initializer{};
    if (!dc_classify_big_star_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x56U;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    add_write(0x02U, 0x565EU, 0x2D04U, 0x01U, 21U);
    add_write(0x02U, 0x5663U, 0x2D18U, 0x80U, 42U);
    add_write(0x02U, 0x5666U, 0x2D00U, 0x00U, 57U);
    add_write(0x02U, 0x5669U, 0x2D01U, 0x00U, 72U);
    add_write(0x02U, 0x566EU, 0x3B4BU, 0x03U, 93U);
    add_write(0x02U, 0x5671U, stack_return->high, 0x56U, 93U);
    add_write(0x02U, 0x5671U, stack_return->low, 0x73U, 93U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0x73U, 150U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x56U, 174U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0x74U, 219U);
    add_write(0x01U, 0xC5E1U, 0x3F4BU, 0x74U, 258U);
    add_write(0x01U, 0xC5E6U, 0x3FCBU, 0x56U, 285U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = kBodyCycles - 3U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {0x01U, 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_undead_head_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x03U, .logical_pc = 0x61A0U};
    constexpr std::uint32_t kBodyCycles = 192U;
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.y != 0x0BU) {
        return std::nullopt;
    }
    const auto stack_return = proven_stack_return(entry, 0xF3U);
    if (!stack_return) {
        return std::nullopt;
    }

    dc_undead_head_initializer initializer{};
    if (!dc_classify_undead_head_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x61U;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    add_write(0x03U, 0x61A0U, stack_return->high, 0x61U, 0U);
    add_write(0x03U, 0x61A0U, stack_return->low, 0xA2U, 0U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0xA2U, 57U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x61U, 81U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0xA3U, 126U);
    add_write(0x01U, 0xC5E1U, static_cast<std::uint16_t>(0x3F00U + initializer.slot),
              0xA3U, 165U);
    add_write(0x01U, 0xC5E6U, static_cast<std::uint16_t>(0x3F80U + initializer.slot),
              0x61U, 192U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = kBodyCycles - 3U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {0x01U, 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_undead_head_selector_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x03U, .logical_pc = 0x61ABU};
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_undead_head_selector_gate gate{};
    if (!dc_classify_undead_head_selector_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &gate)) {
        throw std::runtime_error(
            "Native Undead-Head selector-gate replacement rejected the "
            "work-RAM snapshot.");
    }
    if (gate.path != DC_UNDEAD_HEAD_SELECTOR_GATE_WAITING) {
        return std::nullopt;
    }

    const auto set_compare_zero = [](std::uint8_t status,
                                     const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7CU);
        status = static_cast<std::uint8_t>(status | 0x01U);
        if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };

    auto output = entry.registers;
    output.accumulator = gate.selector;
    output.status = set_compare_zero(output.status, output.accumulator);
    output.program_counter = 0x61B2U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x03U, .logical_pc = 0x61B2U},
        .output_registers = output,
        .body_master_cycles = 27U,
    };
    const auto append_checkpoint =
        [&replacement](const std::uint16_t instruction_pc,
                        const std::uint16_t resume_pc,
                        const std::uint32_t master_cycle,
                        auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };
    auto checkpoint = entry.registers;
    checkpoint.accumulator = gate.selector;
    append_checkpoint(0x61ABU, 0x61AEU, 15U, checkpoint);
    checkpoint.status = output.status;
    append_checkpoint(0x61AEU, 0x61B0U, 21U, checkpoint);
    append_checkpoint(0x61B0U, 0x61B2U, 27U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_undead_head_waiting_parent(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x03U, .logical_pc = 0x61A3U};
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_undead_head_waiting_parent parent{};
    if (!dc_classify_undead_head_waiting_parent(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &parent)) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto set_compare_zero = [](std::uint8_t status,
                                     const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7CU);
        status = static_cast<std::uint8_t>(status | 0x01U);
        if ((value & 0x80U) != 0U) {
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
            .instruction = {0x03U, instruction_pc},
            .registers = registers,
        });
    };

    auto checkpoint = entry.registers;
    checkpoint.accumulator = 0U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    auto output = checkpoint;
    output.accumulator = parent.selector;
    output.status = set_compare_zero(output.status, output.accumulator);
    output.program_counter = 0x61B2U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x03U, .logical_pc = 0x61B2U},
        .output_registers = output,
        .body_master_cycles = 63U,
    };
    append_checkpoint(replacement, 0x61A3U, 0x61A6U, 15U, checkpoint);
    append_checkpoint(replacement, 0x61A6U, 0x61A9U, 30U, checkpoint);
    append_checkpoint(replacement, 0x61A9U, 0x61ABU, 36U, checkpoint);
    checkpoint.accumulator = parent.selector;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x61ABU, 0x61AEU, 51U, checkpoint);
    checkpoint.status = output.status;
    append_checkpoint(replacement, 0x61AEU, 0x61B0U, 57U, checkpoint);
    append_checkpoint(replacement, 0x61B0U, 0x61B2U, 63U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_side_worm_initializers(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kLeftRoutine{.physical_bank = 0x02U, .logical_pc = 0x5C4BU};
    constexpr RoutineKey kRightRoutine{.physical_bank = 0x02U, .logical_pc = 0x5C7CU};
    if ((entry.routine != kLeftRoutine && entry.routine != kRightRoutine)
        || entry.registers.accumulator != 0U || entry.registers.y != 0x0BU) {
        return std::nullopt;
    }

    dc_side_worm_initializer initializer{};
    if (!dc_classify_side_worm_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    const bool is_left = entry.routine == kLeftRoutine;
    const auto terminal_instruction = static_cast<std::uint16_t>(
        is_left ? 0x5C7BU : 0x5CAEU);
    const auto body_cycles = static_cast<std::uint32_t>(
        is_left ? 204U : 210U);
    const auto first_write_cycle = static_cast<std::uint32_t>(
        is_left ? 15U : 21U);
    const auto first_instruction = static_cast<std::uint16_t>(
        is_left ? 0x5C4BU : 0x5C7EU);

    auto output = entry.registers;
    output.program_counter = terminal_instruction;
    output.accumulator = 0x5CU;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = terminal_instruction},
        .output_registers = output,
        .body_master_cycles = body_cycles,
    };
    const auto slot_address = [slot = initializer.slot](const std::uint16_t base) {
        return static_cast<std::uint16_t>(base + slot);
    };
    const auto add_write = [&replacement](
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = 0x02U, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    const auto write_cycle = [first_write_cycle](const std::uint32_t index) {
        return first_write_cycle + 21U * index;
    };
    add_write(first_instruction, slot_address(0x3500U), initializer.initial_state, write_cycle(0U));
    add_write(static_cast<std::uint16_t>(first_instruction + 5U), slot_address(0x3700U),
              is_left ? 0x02U : 0x03U, write_cycle(1U));
    add_write(static_cast<std::uint16_t>(first_instruction + 10U), slot_address(0x3780U),
              initializer.selector, write_cycle(2U));
    add_write(static_cast<std::uint16_t>(first_instruction + 15U), slot_address(0x3C80U),
              is_left ? 0x05U : 0x0BU, write_cycle(3U));
    add_write(static_cast<std::uint16_t>(first_instruction + 20U), slot_address(0x3E00U), 0xAFU,
              write_cycle(4U));
    add_write(static_cast<std::uint16_t>(first_instruction + 25U), slot_address(0x3E80U), 0x5CU,
              write_cycle(5U));
    add_write(static_cast<std::uint16_t>(first_instruction + 30U), slot_address(0x3400U),
              is_left ? 0x08U : 0x0AU, write_cycle(6U));
    add_write(static_cast<std::uint16_t>(first_instruction + 35U), slot_address(0x3480U), 0x0EU,
              write_cycle(7U));
    add_write(static_cast<std::uint16_t>(first_instruction + 40U), slot_address(0x3F00U), 0xC5U,
              write_cycle(8U));
    add_write(static_cast<std::uint16_t>(first_instruction + 45U), slot_address(0x3F80U), 0x5CU,
              write_cycle(9U));
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = body_cycles - 3U,
        .master_cycle_offset = body_cycles,
        .instruction = {
            0x02U,
            static_cast<std::uint16_t>(terminal_instruction - 3U),
        },
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_side_worm_selector_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x5CCDU};
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_side_worm_selector_gate gate{};
    if (!dc_classify_side_worm_selector_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &gate)) {
        throw std::runtime_error(
            "Native Side-Worm selector-gate replacement rejected the "
            "work-RAM snapshot.");
    }
    if (gate.path != DC_SIDE_WORM_SELECTOR_GATE_WAITING) {
        return std::nullopt;
    }

    const auto set_compare_status = [](std::uint8_t status,
                                       const std::uint8_t left,
                                       const std::uint8_t right) {
        const auto result = static_cast<std::uint8_t>(left - right);
        status = static_cast<std::uint8_t>(status & 0x7CU);
        if (left >= right) {
            status = static_cast<std::uint8_t>(status | 0x01U);
        }
        if (result == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        }
        if ((result & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto set_load_status = [](std::uint8_t status,
                                    const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        }
        if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };

    auto output = entry.registers;
    output.accumulator = gate.selector;
    output.status = set_compare_status(
        output.status, gate.selector, gate.expected_selector);
    output.program_counter = 0x5CD5U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x5CD5U},
        .output_registers = output,
        .body_master_cycles = 36U,
    };
    const auto append_checkpoint =
        [&replacement](const std::uint16_t instruction_pc,
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
    auto checkpoint = entry.registers;
    checkpoint.accumulator = gate.selector;
    checkpoint.status = set_load_status(checkpoint.status, gate.selector);
    append_checkpoint(0x5CCDU, 0x5CD0U, 15U, checkpoint);
    checkpoint.status = output.status;
    append_checkpoint(0x5CD0U, 0x5CD3U, 30U, checkpoint);
    append_checkpoint(0x5CD3U, 0x5CD5U, 36U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_side_worm_waiting_parent(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x5CC5U};
    if (entry.routine != kRoutine) return std::nullopt;
    dc_side_worm_waiting_parent parent{};
    if (!dc_classify_side_worm_waiting_parent(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()), work_ram.size(),
            entry.registers.x, &parent)) return std::nullopt;
    const auto set_nz = [](std::uint8_t status, std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) status = static_cast<std::uint8_t>(status | 0x02U);
        else if ((value & 0x80U) != 0U) status = static_cast<std::uint8_t>(status | 0x80U);
        return status;
    };
    const auto set_cmp = [](std::uint8_t status, std::uint8_t left, std::uint8_t right) {
        const auto result = static_cast<std::uint8_t>(left - right);
        status = static_cast<std::uint8_t>(status & 0x7CU);
        if (left >= right) status = static_cast<std::uint8_t>(status | 0x01U);
        if ((result & 0x80U) != 0U) status = static_cast<std::uint8_t>(status | 0x80U);
        return status;
    };
    auto checkpoint = entry.registers;
    checkpoint.accumulator = 0U;
    checkpoint.status = set_nz(checkpoint.status, 0U);
    auto output = checkpoint;
    output.accumulator = parent.selector;
    output.status = set_cmp(output.status, parent.selector, parent.expected_selector);
    output.program_counter = 0x5CD5U;
    RoutineReplacement replacement{{.physical_bank = 0x02U, .logical_pc = 0x5CD5U}, output, 72U};
    const auto add = [&replacement](std::uint16_t pc, std::uint16_t resume, std::uint32_t cycle, auto registers) {
        registers.program_counter = resume;
        replacement.checkpoints.push_back({cycle - 3U, cycle, {0x02U, pc}, registers});
    };
    add(0x5CC5U, 0x5CC8U, 15U, checkpoint);
    add(0x5CC8U, 0x5CCBU, 30U, checkpoint);
    add(0x5CCBU, 0x5CCDU, 36U, checkpoint);
    checkpoint.accumulator = parent.selector;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    add(0x5CCDU, 0x5CD0U, 51U, checkpoint);
    checkpoint.status = output.status;
    add(0x5CD0U, 0x5CD3U, 66U, checkpoint);
    add(0x5CD3U, 0x5CD5U, 72U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_side_worm_active_timer_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x5D4AU};
    constexpr std::uint8_t kLeftSlot = 0x4EU;
    constexpr std::uint16_t kContinuation = 0x5D4DU;
    if (entry.routine != kRoutine || entry.registers.x != kLeftSlot
        || entry.registers.accumulator != 0x01U || entry.registers.y != 0U
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    // C018 transfers directly to C5C9. Preserve the JSR frame so the shared
    // scheduler installs $5D4D before the mixed $5DFC helper begins.
    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    auto checkpoint = entry.registers;
    const auto coroutine_return = static_cast<std::uint16_t>(kContinuation - 1U);
    const auto return_high = static_cast<std::uint8_t>(coroutine_return >> 8U);
    const auto return_low = static_cast<std::uint8_t>(coroutine_return);
    const auto stack_high = checkpoint.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
    if (staged.size() <= 0x0100U + stack_high
        || staged.size() <= 0x0100U + stack_low) {
        return std::nullopt;
    }

    RoutineReplacement replacement{};
    replacement.work_ram_writes = {
        {{0x02U, 0x5D4AU}, static_cast<std::uint16_t>(0x2100U + stack_high), return_high, 0U},
        {{0x02U, 0x5D4AU}, static_cast<std::uint16_t>(0x2100U + stack_low), return_low, 0U},
    };
    staged[0x0100U + stack_high] = static_cast<std::byte>(return_high);
    staged[0x0100U + stack_low] = static_cast<std::byte>(return_low);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);

    const auto append_checkpoint =
        [&replacement](const std::uint8_t bank, const std::uint16_t instruction,
            const std::uint16_t resume, const std::uint32_t cycle, auto registers) {
            registers.program_counter = resume;
            replacement.checkpoints.push_back({cycle - 3U, cycle, {bank, instruction}, registers});
        };
    append_checkpoint(0x02U, 0x5D4AU, 0xC018U, 21U, checkpoint);
    append_checkpoint(0x01U, 0xC018U, 0xC5C9U, 33U, checkpoint);

    RoutineEntry scheduler_entry{
        .routine = {.physical_bank = 0x01U, .logical_pc = 0xC5C9U},
        .registers = checkpoint,
    };
    auto scheduler = replace_coroutine_scheduler(scheduler_entry, staged);
    if (!scheduler || scheduler->checkpoints.empty()) return std::nullopt;

    constexpr std::uint32_t kSchedulerOffset = 33U;
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
replace_little_skull_hidden_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4455U};
    constexpr std::uint32_t kBodyCycles = 207U;
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.y != 0x0BU) {
        return std::nullopt;
    }
    const auto stack_return = proven_stack_return(entry, 0xF3U);
    if (!stack_return) {
        return std::nullopt;
    }

    dc_little_skull_hidden_initializer initializer{};
    if (!dc_classify_little_skull_hidden_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x44U;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    add_write(0x02U, 0x4455U, 0x2CFAU, 0U, 15U);
    add_write(0x02U, 0x4458U, stack_return->high, 0x44U, 15U);
    add_write(0x02U, 0x4458U, stack_return->low, 0x5AU, 15U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0x5AU, 72U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x44U, 96U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0x5BU, 141U);
    add_write(0x01U, 0xC5E1U, 0x3F44U, 0x5BU, 180U);
    add_write(0x01U, 0xC5E6U, 0x3FC4U, 0x44U, 207U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = kBodyCycles - 3U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {0x01U, 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_little_skull_pair_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4485U};
    constexpr std::uint32_t kBodyCycles = 285U;
    if (entry.routine != kRoutine || entry.registers.y != 0x0BU) {
        return std::nullopt;
    }
    const auto stack_return = proven_stack_return(entry, 0xF3U);
    if (!stack_return) {
        return std::nullopt;
    }

    dc_little_skull_pair_initializer initializer{};
    if (!dc_classify_little_skull_pair_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, entry.registers.accumulator,
            &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x44U;
    output.y = initializer.pair;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    const auto slot_address = [slot = initializer.slot](const std::uint16_t base) {
        return static_cast<std::uint16_t>(base + slot);
    };
    add_write(0x02U, 0x4485U, slot_address(0x3E00U), initializer.pair, 15U);
    add_write(0x02U, 0x448AU,
        static_cast<std::uint16_t>(0x2CF5U + initializer.pair), 0U, 42U);
    add_write(0x02U, 0x4495U,
        static_cast<std::uint16_t>(0x2CF8U + initializer.pair), 0U, 93U);
    add_write(0x02U, 0x4498U, stack_return->high, 0x44U, 93U);
    add_write(0x02U, 0x4498U, stack_return->low, 0x9AU, 93U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0x9AU, 150U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x44U, 174U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0x9BU, 219U);
    add_write(0x01U, 0xC5E1U, slot_address(0x3F00U), 0x9BU, 258U);
    add_write(0x01U, 0xC5E6U, slot_address(0x3F80U), 0x44U, 285U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = kBodyCycles - 3U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {0x01U, 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_little_skull_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x44E4U};
    constexpr std::uint32_t kBodyCycles = 390U;
    if (entry.routine != kRoutine || entry.registers.y != 0x0BU) {
        return std::nullopt;
    }
    const auto stack_return = proven_stack_return(entry, 0xF3U);
    if (!stack_return) {
        return std::nullopt;
    }

    dc_little_skull_initializer initializer{};
    if (!dc_classify_little_skull_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, entry.registers.accumulator,
            &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x45U;
    output.y = initializer.pair;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    const auto slot_address = [slot = initializer.slot](const std::uint16_t base) {
        return static_cast<std::uint16_t>(base + slot);
    };
    add_write(0x02U, 0x44E4U, slot_address(0x3E00U), initializer.pair, 15U);
    add_write(0x02U, 0x44E9U, slot_address(0x3800U), 0x10U, 36U);
    add_write(0x02U, 0x44EEU, slot_address(0x3D00U), 0x03U, 57U);
    add_write(0x02U, 0x44F3U, slot_address(0x3300U), 0x34U, 78U);
    add_write(0x02U, 0x44F8U, slot_address(0x3380U), 0x00U, 99U);
    add_write(0x02U, 0x44FDU, slot_address(0x3B00U), 0x06U, 120U);
    add_write(0x02U, 0x4507U,
        static_cast<std::uint16_t>(0x2CF5U + initializer.pair),
        initializer.live_count_after, 171U);
    add_write(0x02U, 0x4512U, stack_return->high, 0x45U, 198U);
    add_write(0x02U, 0x4512U, stack_return->low, 0x14U, 198U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0x14U, 255U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x45U, 279U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0x15U, 324U);
    add_write(0x01U, 0xC5E1U, slot_address(0x3F00U), 0x15U, 363U);
    add_write(0x01U, 0xC5E6U, slot_address(0x3F80U), 0x45U, 390U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = kBodyCycles - 3U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {0x01U, 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_little_skull_gray_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The targeted gray-release route enters this setup only for left-pair
    // slot $46. It installs the gray selector, restores the layout Y
    // position, and schedules the existing 45A4 RNG continuation.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x458AU};
    constexpr std::uint32_t kBodyCycles = 291U;
    constexpr std::uint8_t kSlot = 0x46U;
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x45U
        || entry.registers.x != kSlot || entry.registers.y != 0x03U
        || entry.registers.stack_pointer != 0xFBU || entry.registers.status != 0U
        || work_ram.size() <= 0x1FC6U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    const auto slot_address = [](const std::uint16_t base) {
        return static_cast<std::uint16_t>(base + kSlot);
    };
    add_write(0x02U, 0x458CU, slot_address(0x3D00U), 0x0BU, 21U);
    add_write(0x02U, 0x4594U, slot_address(0x3080U), 0xC2U, 57U);
    add_write(0x02U, 0x4599U, slot_address(0x3300U), 0x38U, 78U);
    add_write(0x02U, 0x459EU, slot_address(0x3380U), 0x00U, 99U);
    add_write(0x02U, 0x45A1U, 0x21FBU, 0x45U, 99U);
    add_write(0x02U, 0x45A1U, 0x21FAU, 0xA3U, 99U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0xA3U, 156U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x45U, 180U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0xA4U, 225U);
    add_write(0x01U, 0xC5E1U, slot_address(0x3F00U), 0xA4U, 264U);
    add_write(0x01U, 0xC5E6U, slot_address(0x3F80U), 0x45U, kBodyCycles);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = kBodyCycles - 3U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_little_skull_gray_release(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The gray-release continuation first restores slot $46's normal flags
    // and collision state. Its observed odd RNG result selects pending value
    // two, then the original coroutine scheduler installs 4515.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x45A4U};
    constexpr std::uint8_t kSlot = 0x46U;
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x45U
        || entry.registers.x != kSlot || entry.registers.y != 0x03U
        || entry.registers.stack_pointer != 0xFBU || entry.registers.status != 0U
        || work_ram.size() <= 0x1FC6U
        || std::to_integer<std::uint8_t>(work_ram[0x0CFAU]) != 0U) {
        return std::nullopt;
    }
    const auto flags_before = std::to_integer<std::uint8_t>(work_ram[0x10C6U]);
    const auto flags_after = static_cast<std::uint8_t>(flags_before & 0xBFU);
    const auto collision_before = std::to_integer<std::uint8_t>(work_ram[0x1846U]);
    const auto collision_after = static_cast<std::uint8_t>(collision_before & 0x7FU);
    if (flags_after != 0x82U || collision_after != 0x10U) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
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
           const std::uint8_t bank,
           const std::uint16_t instruction_pc,
           const std::uint16_t resume_pc,
           const std::uint32_t master_cycle,
           auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {bank, instruction_pc},
                .registers = registers,
            });
        };
    const auto add_write =
        [](RoutineReplacement& replacement,
           const std::uint8_t bank,
           const std::uint16_t instruction_pc,
           const std::uint16_t address,
           const std::uint8_t value,
           const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {bank, instruction_pc},
                .logical_address = address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
        };
    const auto stage_write =
        [](std::vector<std::byte>& staged,
           const std::uint16_t address,
           const std::uint8_t value) {
            const auto offset = static_cast<std::size_t>(address & 0x1FFFU);
            if (offset >= staged.size()) {
                return false;
            }
            staged[offset] = static_cast<std::byte>(value);
            return true;
        };

    RoutineReplacement replacement{};
    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    auto checkpoint = entry.registers;
    checkpoint.accumulator = 0U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x45A4U, 0x45A7U, 15U, checkpoint);
    append_checkpoint(replacement, 0x02U, 0x45A7U, 0x45A9U, 27U, checkpoint);

    checkpoint.accumulator = flags_before;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x45A9U, 0x45ACU, 42U, checkpoint);
    checkpoint.accumulator = flags_after;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x45ACU, 0x45AEU, 48U, checkpoint);
    add_write(replacement, 0x02U, 0x45AFU, 0x30C6U, flags_after, 63U);
    if (!stage_write(staged, 0x30C6U, flags_after)) {
        return std::nullopt;
    }
    append_checkpoint(replacement, 0x02U, 0x45AFU, 0x45B2U, 63U, checkpoint);

    checkpoint.accumulator = collision_before;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x45B2U, 0x45B5U, 78U, checkpoint);
    checkpoint.accumulator = collision_after;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x45B5U, 0x45B7U, 84U, checkpoint);
    add_write(replacement, 0x02U, 0x45B7U, 0x3846U, collision_after, 99U);
    if (!stage_write(staged, 0x3846U, collision_after)) {
        return std::nullopt;
    }
    append_checkpoint(replacement, 0x02U, 0x45B7U, 0x45BAU, 99U, checkpoint);

    checkpoint.accumulator = 0x03U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x45BAU, 0x45BCU, 105U, checkpoint);
    add_write(replacement, 0x02U, 0x45BCU, 0x3D46U, 0x03U, 120U);
    if (!stage_write(staged, 0x3D46U, 0x03U)) {
        return std::nullopt;
    }
    append_checkpoint(replacement, 0x02U, 0x45BCU, 0x45BFU, 120U, checkpoint);

    checkpoint.accumulator = 0x04U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x45BFU, 0x45C1U, 126U, checkpoint);
    add_write(replacement, 0x02U, 0x45C1U, 0x3B46U, 0x04U, 141U);
    if (!stage_write(staged, 0x3B46U, 0x04U)) {
        return std::nullopt;
    }
    append_checkpoint(replacement, 0x02U, 0x45C1U, 0x45C4U, 141U, checkpoint);

    checkpoint.accumulator = 0x01U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x45C4U, 0x45C6U, 147U, checkpoint);
    add_write(replacement, 0x02U, 0x45C6U, 0x2D04U, 0x01U, 162U);
    add_write(replacement, 0x02U, 0x45C9U, 0x21FBU, 0x45U, 162U);
    add_write(replacement, 0x02U, 0x45C9U, 0x21FAU, 0xCBU, 162U);
    if (!stage_write(staged, 0x2D04U, 0x01U)
        || !stage_write(staged, 0x21FBU, 0x45U)
        || !stage_write(staged, 0x21FAU, 0xCBU)) {
        return std::nullopt;
    }
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(replacement, 0x02U, 0x45C6U, 0x45C9U, 162U, checkpoint);
    append_checkpoint(replacement, 0x02U, 0x45C9U, 0xC081U, 183U, checkpoint);
    append_checkpoint(replacement, 0x00U, 0xC081U, 0xE8B3U, 195U, checkpoint);

    RoutineEntry rng_entry{
        .routine = {.physical_bank = 0x00U, .logical_pc = 0xE8B3U},
        .registers = checkpoint,
    };
    auto rng = replace_rng_update(rng_entry, staged);
    if (!rng || rng->checkpoints.empty() || (rng->output_registers.accumulator & 0x01U) == 0U) {
        return std::nullopt;
    }
    for (auto write : rng->work_ram_writes) {
        write.master_cycle_offset += 195U;
        replacement.work_ram_writes.push_back(write);
        if (!stage_write(staged, write.logical_address, write.value)) {
            return std::nullopt;
        }
    }
    for (auto child_checkpoint : rng->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset += 195U;
        child_checkpoint.master_cycle_offset += 195U;
        replacement.checkpoints.push_back(child_checkpoint);
    }

    checkpoint = rng->output_registers;
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer + 2U);
    append_checkpoint(replacement, 0x00U, 0xE8E6U, 0x45CCU, 498U, checkpoint);
    checkpoint.accumulator = static_cast<std::uint8_t>(checkpoint.accumulator & 0x01U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x45CCU, 0x45CEU, 504U, checkpoint);
    append_checkpoint(replacement, 0x02U, 0x45CEU, 0x45D3U, 516U, checkpoint);
    checkpoint.accumulator = 0x02U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x45D3U, 0x45D5U, 522U, checkpoint);
    add_write(replacement, 0x02U, 0x45D5U, 0x2D04U, 0x02U, 537U);
    if (!stage_write(staged, 0x2D04U, 0x02U)) {
        return std::nullopt;
    }
    append_checkpoint(replacement, 0x02U, 0x45D5U, 0x45D8U, 537U, checkpoint);
    append_checkpoint(replacement, 0x02U, 0x45D8U, 0x450AU, 549U, checkpoint);

    checkpoint.accumulator = 0U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x450AU, 0x450DU, 564U, checkpoint);
    append_checkpoint(replacement, 0x02U, 0x450DU, 0x4512U, 576U, checkpoint);
    add_write(replacement, 0x02U, 0x4512U, 0x21FBU, 0x45U, 576U);
    add_write(replacement, 0x02U, 0x4512U, 0x21FAU, 0x14U, 576U);
    if (!stage_write(staged, 0x21FBU, 0x45U)
        || !stage_write(staged, 0x21FAU, 0x14U)) {
        return std::nullopt;
    }
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(replacement, 0x02U, 0x4512U, 0xC018U, 597U, checkpoint);
    append_checkpoint(replacement, 0x01U, 0xC018U, 0xC5C9U, 609U, checkpoint);

    RoutineEntry scheduler_entry{
        .routine = {.physical_bank = 0x01U, .logical_pc = 0xC5C9U},
        .registers = checkpoint,
    };
    auto scheduler = replace_coroutine_scheduler(scheduler_entry, staged);
    if (!scheduler || scheduler->checkpoints.empty()) {
        return std::nullopt;
    }
    for (auto write : scheduler->work_ram_writes) {
        write.master_cycle_offset += 609U;
        replacement.work_ram_writes.push_back(write);
    }
    for (auto child_checkpoint : scheduler->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset += 609U;
        child_checkpoint.master_cycle_offset += 609U;
        replacement.checkpoints.push_back(child_checkpoint);
    }
    replacement.return_instruction = scheduler->return_instruction;
    replacement.output_registers = scheduler->output_registers;
    replacement.body_master_cycles = 609U + scheduler->body_master_cycles;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_little_skull_live_continuation(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The ordinary Little-Skull continuation advances its horizontal
    // position, copies the current appearance byte, and counts down to the
    // same coroutine dispatch.  The natural gray-release route exercises the
    // four live slots and both its waiting and expiry branches. Slot $49 has
    // one periodic timer-IRQ passage between the position and appearance
    // stores; the instruction-boundary transcript below preserves that
    // original handler passage.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4515U};
    const auto is_live_slot = [](const std::uint8_t slot) {
        return slot == 0x46U || slot == 0x47U || slot == 0x49U || slot == 0x4AU;
    };
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x45U
        || !is_live_slot(entry.registers.x) || entry.registers.stack_pointer != 0xFBU
        || (entry.registers.status & 0xEFU) != 0U || work_ram.size() <= 0x1FC6U) {
        return std::nullopt;
    }

    const auto slot = entry.registers.x;
    const auto position_offset = static_cast<std::size_t>(0x1300U + slot);
    const auto appearance_offset = static_cast<std::size_t>(0x1380U + slot);
    const auto timer_offset = static_cast<std::size_t>(0x1B00U + slot);
    const auto timer_before = std::to_integer<std::uint8_t>(work_ram[timer_offset]);
    if (timer_before == 0U) {
        return std::nullopt;
    }
    const auto position_after = static_cast<std::uint8_t>(
        std::to_integer<std::uint8_t>(work_ram[position_offset]) + 2U);
    const auto appearance = std::to_integer<std::uint8_t>(work_ram[appearance_offset]);
    const auto timer_after = static_cast<std::uint8_t>(timer_before - 1U);

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto append_checkpoint = [](RoutineReplacement& replacement,
                                      const std::uint8_t bank,
                                      const std::uint16_t instruction_pc,
                                      const std::uint16_t resume_pc,
                                      const std::uint32_t master_cycle,
                                      auto registers) {
        registers.program_counter = resume_pc;
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = master_cycle - 3U,
            .master_cycle_offset = master_cycle,
            .instruction = {bank, instruction_pc},
            .registers = registers,
        });
    };
    const auto add_write = [](RoutineReplacement& replacement,
                              const std::uint8_t bank,
                              const std::uint16_t instruction_pc,
                              const std::uint16_t address,
                              const std::uint8_t value,
                              const std::uint32_t master_cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {bank, instruction_pc},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = master_cycle,
        });
    };
    const auto stage_write = [](std::vector<std::byte>& staged,
                                const std::uint16_t address,
                                const std::uint8_t value) {
        const auto offset = static_cast<std::size_t>(address & 0x1FFFU);
        if (offset >= staged.size()) {
            return false;
        }
        staged[offset] = static_cast<std::byte>(value);
        return true;
    };

    RoutineReplacement replacement{};
    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    auto checkpoint = entry.registers;
    append_checkpoint(replacement, 0x02U, 0x4515U, 0x4516U, 6U, checkpoint);
    checkpoint.accumulator = std::to_integer<std::uint8_t>(work_ram[position_offset]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x4516U, 0x4519U, 21U, checkpoint);
    checkpoint.accumulator = position_after;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x4519U, 0x451BU, 27U, checkpoint);
    add_write(replacement, 0x02U, 0x451BU, static_cast<std::uint16_t>(0x3300U + slot), position_after, 42U);
    if (!stage_write(staged, static_cast<std::uint16_t>(0x3300U + slot), position_after)) {
        return std::nullopt;
    }
    append_checkpoint(replacement, 0x02U, 0x451BU, 0x451EU, 42U, checkpoint);
    checkpoint.accumulator = 0U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x451EU, 0x451FU, 48U, checkpoint);
    checkpoint.accumulator = appearance;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x02U, 0x451FU, 0x4522U, 63U, checkpoint);
    add_write(replacement, 0x02U, 0x4522U, static_cast<std::uint16_t>(0x3380U + slot), appearance, 78U);
    append_checkpoint(replacement, 0x02U, 0x4522U, 0x4525U, 78U, checkpoint);
    checkpoint.status = set_nz(checkpoint.status, timer_after);
    add_write(replacement, 0x02U, 0x4525U, static_cast<std::uint16_t>(0x3B00U + slot), timer_after, 99U);
    if (!stage_write(staged, static_cast<std::uint16_t>(0x3B00U + slot), timer_after)) {
        return std::nullopt;
    }
    append_checkpoint(replacement, 0x02U, 0x4525U, 0x4528U, 99U, checkpoint);

    if (timer_after != 0U) {
        append_checkpoint(replacement, 0x02U, 0x4528U, 0x452AU, 105U, checkpoint);
        // The hook resumes at the original RTS.  Keep its accumulator and
        // flags from the final DEC/branch sequence rather than the caller's
        // entry snapshot; the observer's outer-return record is captured
        // after that RTS has unwound through C51D.
        auto output = checkpoint;
        output.program_counter = 0x452AU;
        replacement.return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x452AU};
        replacement.output_registers = output;
        replacement.body_master_cycles = 105U;
        return replacement;
    }

    append_checkpoint(replacement, 0x02U, 0x4528U, 0x452BU, 111U, checkpoint);
    add_write(replacement, 0x02U, 0x452BU, 0x21FBU, 0x45U, 111U);
    add_write(replacement, 0x02U, 0x452BU, 0x21FAU, 0x2DU, 111U);
    if (!stage_write(staged, 0x21FBU, 0x45U) || !stage_write(staged, 0x21FAU, 0x2DU)) {
        return std::nullopt;
    }
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(replacement, 0x02U, 0x452BU, 0xC018U, 132U, checkpoint);
    append_checkpoint(replacement, 0x01U, 0xC018U, 0xC5C9U, 144U, checkpoint);

    RoutineEntry scheduler_entry{
        .routine = {.physical_bank = 0x01U, .logical_pc = 0xC5C9U},
        .registers = checkpoint,
    };
    auto scheduler = replace_coroutine_scheduler(scheduler_entry, staged);
    if (!scheduler || scheduler->checkpoints.empty()) {
        return std::nullopt;
    }
    for (auto write : scheduler->work_ram_writes) {
        write.master_cycle_offset += 144U;
        replacement.work_ram_writes.push_back(write);
    }
    for (auto child_checkpoint : scheduler->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset += 144U;
        child_checkpoint.master_cycle_offset += 144U;
        replacement.checkpoints.push_back(child_checkpoint);
    }
    replacement.return_instruction = scheduler->return_instruction;
    replacement.output_registers = scheduler->output_registers;
    replacement.body_master_cycles = 144U + scheduler->body_master_cycles;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_devil_shooter_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x50EEU,
    };
    constexpr std::uint32_t kBodyCycles = 351U;
    if (entry.routine != kRoutine || entry.registers.status != 0x02U
        || entry.registers.accumulator != 0U || entry.registers.y != 0x0BU) {
        return std::nullopt;
    }
    const auto stack_return = proven_stack_return(entry, 0xF3U);
    if (!stack_return) {
        return std::nullopt;
    }

    dc_devil_shooter_initializer initializer{};
    if (!dc_classify_devil_shooter_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x51U;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    const auto slot_address = [slot = initializer.slot](const std::uint16_t base) {
        return static_cast<std::uint16_t>(base + slot);
    };
    add_write(0x02U, 0x50F0U, slot_address(0x3A00U), 0x05U, 21U);
    add_write(0x02U, 0x50F5U, slot_address(0x3300U), 0x9FU, 42U);
    add_write(0x02U, 0x50FAU, slot_address(0x3380U), 0x02U, 63U);
    add_write(0x02U, 0x50FFU, slot_address(0x3E00U), 0x78U, 84U);
    add_write(0x02U, 0x5104U, slot_address(0x3E80U), 0x02U, 105U);
    add_write(0x02U, 0x5109U, slot_address(0x3980U), 0x04U, 126U);
    add_write(0x02U, 0x510EU, 0x2D15U, 0x27U, 147U);
    add_write(0x02U, 0x5111U, 0x20BDU, 0x00U, 159U);
    add_write(0x02U, 0x5113U, stack_return->high, 0x51U, 159U);
    add_write(0x02U, 0x5113U, stack_return->low, 0x15U, 159U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0x15U, 216U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x51U, 240U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0x16U, 285U);
    add_write(0x01U, 0xC5E1U, slot_address(0x3F00U), 0x16U, 324U);
    add_write(0x01U, 0xC5E6U, slot_address(0x3F80U), 0x51U, 351U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 348U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_dragon_head_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x5A31U,
    };
    constexpr std::uint32_t kBodyCycles = 291U;
    if (entry.routine != kRoutine || entry.registers.x != 0x13U
        || entry.registers.accumulator != 0U || entry.registers.y != 0x0BU
        || entry.registers.status != 0x02U) {
        return std::nullopt;
    }
    const auto stack_return = proven_stack_return(entry, 0xF3U);
    if (!stack_return) {
        return std::nullopt;
    }

    dc_dragon_head_initializer initializer{};
    if (!dc_classify_dragon_head_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x5AU;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    const auto slot_address = [slot = initializer.slot](const std::uint16_t base) {
        return static_cast<std::uint16_t>(base + slot);
    };
    add_write(0x02U, 0x5A36U, slot_address(0x3080U), initializer.flags_after, 36U);
    add_write(0x02U, 0x5A3BU, slot_address(0x3800U), 0x10U, 57U);
    add_write(0x02U, 0x5A40U, slot_address(0x3D00U), 0x82U, 78U);
    add_write(0x02U, 0x5A45U, slot_address(0x3C00U), 0x08U, 99U);
    add_write(0x02U, 0x5A48U, stack_return->high, 0x5AU, 99U);
    add_write(0x02U, 0x5A48U, stack_return->low, 0x4AU, 99U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0x4AU, 156U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x5AU, 180U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0x4BU, 225U);
    add_write(0x01U, 0xC5E1U, slot_address(0x3F00U), 0x4BU, 264U);
    add_write(0x01U, 0xC5E6U, slot_address(0x3F80U), 0x5AU, 291U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 288U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_giant_skeleton_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x5EE4U,
    };
    constexpr std::uint32_t kBodyCycles = 213U;
    if (entry.routine != kRoutine || entry.registers.x != 0x30U
        || entry.registers.accumulator != 0U || entry.registers.y != 0x0BU
        || entry.registers.status != 0x02U) {
        return std::nullopt;
    }
    const auto stack_return = proven_stack_return(entry, 0xF3U);
    if (!stack_return) {
        return std::nullopt;
    }

    dc_giant_skeleton_initializer initializer{};
    if (!dc_classify_giant_skeleton_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x5EU;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    const auto slot_address = [slot = initializer.slot](const std::uint16_t base) {
        return static_cast<std::uint16_t>(base + slot);
    };
    add_write(0x02U, 0x5EE6U, slot_address(0x3980U), 0x03U, 21U);
    add_write(0x02U, 0x5EE9U, stack_return->high, 0x5EU, 21U);
    add_write(0x02U, 0x5EE9U, stack_return->low, 0xEBU, 21U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0xEBU, 78U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x5EU, 102U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0xECU, 147U);
    add_write(0x01U, 0xC5E1U, slot_address(0x3F00U), 0xECU, 186U);
    add_write(0x01U, 0xC5E6U, slot_address(0x3F80U), 0x5EU, 213U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 210U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_three_blobs_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x664AU,
    };
    constexpr std::uint32_t kBodyCycles = 396U;
    if (entry.routine != kRoutine || entry.registers.y != 0x0BU) {
        return std::nullopt;
    }
    const auto stack_return = proven_stack_return(entry, 0xF3U);
    if (!stack_return) {
        return std::nullopt;
    }

    dc_three_blobs_initializer initializer{};
    if (!dc_classify_three_blobs_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, entry.registers.accumulator,
            &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x66U;
    output.y = initializer.initial_state;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    const auto slot_address = [slot = initializer.slot](const std::uint16_t base) {
        return static_cast<std::uint16_t>(base + slot);
    };
    add_write(0x03U, 0x664AU, slot_address(0x3A80U), initializer.initial_state, 15U);
    add_write(0x03U, 0x664EU, static_cast<std::uint16_t>(0x2D39U + initializer.initial_state), initializer.initial_state, 36U);
    add_write(0x03U, 0x6654U, slot_address(0x3D00U), initializer.appearance, 66U);
    add_write(0x03U, 0x6659U, slot_address(0x3800U), 0x13U, 87U);
    add_write(0x03U, 0x665FU, slot_address(0x3E00U), 0xFEU, 117U);
    add_write(0x03U, 0x6665U, slot_address(0x3E80U), 0x00U, 147U);
    add_write(0x03U, 0x666AU, slot_address(0x3680U), 0x20U, 168U);
    add_write(0x03U, 0x666DU, slot_address(0x3400U), 0x00U, 183U);
    add_write(0x03U, 0x6672U, slot_address(0x3480U), 0x01U, 204U);
    add_write(0x03U, 0x6675U, stack_return->high, 0x66U, 204U);
    add_write(0x03U, 0x6675U, stack_return->low, 0x77U, 204U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0x77U, 261U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x66U, 285U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0x78U, 330U);
    add_write(0x01U, 0xC5E1U, slot_address(0x3F00U), 0x78U, 369U);
    add_write(0x01U, 0xC5E6U, slot_address(0x3F80U), 0x66U, 396U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 393U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_three_blobs_controller_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x65BCU,
    };
    constexpr std::uint32_t kBodyCycles = 207U;
    if (entry.routine != kRoutine || entry.registers.x != 0x57U
        || entry.registers.accumulator != 0x02U || entry.registers.y != 0x0BU
        || entry.registers.status != 0U) {
        return std::nullopt;
    }
    const auto stack_return = proven_stack_return(entry, 0xF3U);
    if (!stack_return) {
        return std::nullopt;
    }

    dc_three_blobs_controller_initializer initializer{};
    if (!dc_classify_three_blobs_controller_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x65U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    const auto slot_address = [slot = initializer.slot](const std::uint16_t base) {
        return static_cast<std::uint16_t>(base + slot);
    };
    add_write(0x03U, 0x65BCU, 0x2CFDU, 0x00U, 15U);
    add_write(0x03U, 0x65BFU, stack_return->high, 0x65U, 15U);
    add_write(0x03U, 0x65BFU, stack_return->low, 0xC1U, 15U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0xC1U, 72U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x65U, 96U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0xC2U, 141U);
    add_write(0x01U, 0xC5E1U, slot_address(0x3F00U), 0xC2U, 180U);
    add_write(0x01U, 0xC5E6U, slot_address(0x3F80U), 0x65U, 207U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 204U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_walled_star_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x6319U,
    };
    constexpr std::uint32_t kBodyCycles = 321U;
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x5BU || entry.registers.y != 0x0BU
        || entry.registers.status != 0x02U) {
        return std::nullopt;
    }
    const auto stack_return = proven_stack_return(entry, 0xF3U);
    if (!stack_return) {
        return std::nullopt;
    }

    dc_walled_star_initializer initializer{};
    if (!dc_classify_walled_star_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x63U;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    const auto slot_address = [slot = initializer.slot](const std::uint16_t base) {
        return static_cast<std::uint16_t>(base + slot);
    };
    add_write(0x03U, 0x6319U, 0x39DBU, 0x00U, 15U);
    add_write(0x03U, 0x6321U, 0x30DBU, 0x89U, 51U);
    add_write(0x03U, 0x6326U, slot_address(0x3800U), 0x10U, 72U);
    add_write(0x03U, 0x632BU, slot_address(0x3D00U), 0x84U, 93U);
    add_write(0x03U, 0x6333U, slot_address(0x3800U), 0x50U, 129U);
    add_write(0x03U, 0x6336U, stack_return->high, 0x63U, 129U);
    add_write(0x03U, 0x6336U, stack_return->low, 0x38U, 129U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0x38U, 186U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x63U, 210U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0x39U, 255U);
    add_write(0x01U, 0xC5E1U, slot_address(0x3F00U), 0x39U, 294U);
    add_write(0x01U, 0xC5E6U, slot_address(0x3F80U), 0x63U, kBodyCycles);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 318U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_walled_star_access_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x4A44U,
    };
    constexpr std::uint32_t kBodyCycles = 273U;
    if (entry.routine != kRoutine || entry.registers.accumulator != 0U
        || entry.registers.x != 0x11U || entry.registers.y != 0x0BU
        || entry.registers.status != 0x02U) {
        return std::nullopt;
    }

    dc_walled_star_access_initializer initializer{};
    if (!dc_classify_walled_star_access_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0x4CF1U;
    output.accumulator = 0x01U;
    output.status = 0x80U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4CF1U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto add_write = [&replacement](
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = 0x02U, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    const auto slot_address = [slot = initializer.slot](const std::uint16_t base) {
        return static_cast<std::uint16_t>(base + slot);
    };
    add_write(0x4A46U, slot_address(0x3C80U), 0x04U, 21U);
    add_write(0x4B28U, slot_address(0x3A00U), 0x01U, 54U);
    add_write(0x4B2DU, slot_address(0x3E00U), 0x26U, 75U);
    add_write(0x4B32U, slot_address(0x3E80U), 0x4BU, 96U);
    add_write(0x4B37U, slot_address(0x3F00U), 0x3FU, 117U);
    add_write(0x4B3CU, slot_address(0x3F80U), 0x4BU, 138U);
    add_write(0x4CDAU, slot_address(0x3A00U), 0x00U, 192U);
    add_write(0x4CE1U, slot_address(0x3A00U), 0x04U, 219U);
    add_write(0x4CE4U, slot_address(0x3980U), 0x01U, 240U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 270U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {.physical_bank = 0x02U, .logical_pc = 0x4CE4U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_side_star_initializers(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kLowerRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x4A54U,
    };
    constexpr RoutineKey kUpperRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x4A4CU,
    };
    constexpr std::uint32_t kBodyCycles = 273U;
    if ((entry.routine != kLowerRoutine && entry.routine != kUpperRoutine)
        || entry.registers.accumulator != 0U || entry.registers.y != 0x0BU
        || entry.registers.status != 0x02U
        || (entry.routine == kLowerRoutine && entry.registers.x != 0x10U)
        || (entry.routine == kUpperRoutine && entry.registers.x != 0x12U)) {
        return std::nullopt;
    }

    dc_side_star_initializer initializer{};
    if (!dc_classify_side_star_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    const auto first_instruction = static_cast<std::uint16_t>(
        entry.routine == kLowerRoutine ? 0x4A56U : 0x4A4EU);
    auto output = entry.registers;
    output.program_counter = 0x4CF1U;
    output.accumulator = 0x01U;
    output.status = 0x80U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4CF1U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto add_write = [&replacement](
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = 0x02U, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    const auto slot_address = [slot = initializer.slot](const std::uint16_t base) {
        return static_cast<std::uint16_t>(base + slot);
    };
    const auto first_dispatch_low =
        static_cast<std::uint8_t>(initializer.first_dispatch);
    const auto first_dispatch_high =
        static_cast<std::uint8_t>(initializer.first_dispatch >> 8U);
    const auto steady_dispatch_low =
        static_cast<std::uint8_t>(initializer.steady_dispatch);
    const auto steady_dispatch_high =
        static_cast<std::uint8_t>(initializer.steady_dispatch >> 8U);
    const auto first_dispatch_instruction = static_cast<std::uint16_t>(
        entry.routine == kLowerRoutine ? 0x4BBEU : 0x4BF3U);
    add_write(first_instruction, slot_address(0x3C80U), initializer.appearance, 21U);
    add_write(first_dispatch_instruction, slot_address(0x3A00U), 0x01U, 54U);
    add_write(
        static_cast<std::uint16_t>(first_dispatch_instruction + 5U),
        slot_address(0x3E00U), first_dispatch_low, 75U);
    add_write(
        static_cast<std::uint16_t>(first_dispatch_instruction + 10U),
        slot_address(0x3E80U), first_dispatch_high, 96U);
    add_write(
        static_cast<std::uint16_t>(first_dispatch_instruction + 15U),
        slot_address(0x3F00U), steady_dispatch_low, 117U);
    add_write(
        static_cast<std::uint16_t>(first_dispatch_instruction + 20U),
        slot_address(0x3F80U), steady_dispatch_high, 138U);
    add_write(0x4CDAU, slot_address(0x3A00U), 0x00U, 192U);
    add_write(0x4CE1U, slot_address(0x3A00U), 0x04U, 219U);
    add_write(0x4CE4U, slot_address(0x3980U), 0x01U, 240U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 270U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {.physical_bank = 0x02U, .logical_pc = 0x4CE4U},
        .registers = output,
    });
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
replace_devil_shooter_release_delay_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x5159U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_devil_shooter_release_delay_gate gate{};
    if (!dc_classify_devil_shooter_release_delay_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &gate)) {
        throw std::runtime_error(
            "Native 5159 replacement rejected the work-RAM snapshot.");
    }
    if (gate.returns_idle == 0U) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };

    auto after_table_mode = entry.registers;
    after_table_mode.accumulator = gate.table_mode;
    after_table_mode.status = set_nz(
        after_table_mode.status,
        after_table_mode.accumulator);

    auto after_player = after_table_mode;
    after_player.y = gate.active_player;

    auto after_button = after_player;
    after_button.accumulator = gate.button_input;
    after_button.status = set_nz(after_button.status, after_button.accumulator);

    auto after_button_mask = after_button;
    after_button_mask.accumulator = static_cast<std::uint8_t>(
        gate.button_input & 0x01U);
    after_button_mask.status = set_nz(
        after_button_mask.status,
        after_button_mask.accumulator);

    auto output = after_button_mask;
    output.accumulator = gate.collision_state;
    output.status = set_nz(output.status, output.accumulator);
    output.program_counter = 0x5178U;

    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x5178U},
        .output_registers = output,
        .body_master_cycles = 96U,
    };
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

    append_checkpoint(0x5159U, 0x515CU, 15U, after_table_mode);
    append_checkpoint(0x515CU, 0x5161U, 27U, after_table_mode);
    append_checkpoint(0x5161U, 0x5164U, 42U, after_player);
    append_checkpoint(0x5164U, 0x5167U, 57U, after_button);
    append_checkpoint(0x5167U, 0x5169U, 63U, after_button_mask);
    append_checkpoint(0x5169U, 0x516BU, 69U, after_button_mask);
    append_checkpoint(0x516BU, 0x516EU, 84U, output);
    append_checkpoint(0x516EU, 0x5178U, 96U, output);
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
replace_devil_shooter_charge_retraction(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x533BU,
    };
    if (entry.routine != kRoutine || (entry.registers.status & 0x20U) != 0U) {
        return std::nullopt;
    }

    dc_devil_shooter_charge_retraction retraction{};
    if (!dc_classify_devil_shooter_charge_retraction(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &retraction)) {
        return std::nullopt;
    }

    const std::uint32_t body_cycles = retraction.has_appearance_write != 0U
        ? 273U + static_cast<std::uint32_t>(
                     std::min<std::uint8_t>(retraction.appearance, 3U)) * 18U
        : 234U;
    if (entry.master_cycles_until_user_event <= body_cycles
        || entry.master_cycles_until_timer_interrupt <= body_cycles) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.accumulator = retraction.has_appearance_write != 0U
        ? retraction.appearance
        : 0U;
    if (retraction.has_appearance_write != 0U) {
        output.y = retraction.appearance;
    }
    output.status = static_cast<std::uint8_t>(entry.registers.status & ~0xC3U);
    if (retraction.high_subtract_overflow != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x40U);
    }
    if (retraction.has_appearance_write == 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x03U);
    } else if (retraction.appearance == 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);
    } else if (retraction.appearance == 4U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x01U);
    }
    output.program_counter = 0x5393U;

    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x5393U},
        .output_registers = output,
        .body_master_cycles = body_cycles,
    };
    replacement.work_ram_writes = {
        {
            .instruction = {0x02U, 0x5341U},
            .logical_address = static_cast<std::uint16_t>(0x3280U + retraction.slot),
            .value = retraction.vertical_low,
            .master_cycle_offset = 42U,
        },
        {
            .instruction = {0x02U, 0x5349U},
            .logical_address = static_cast<std::uint16_t>(0x3300U + retraction.slot),
            .value = retraction.vertical_middle,
            .master_cycle_offset = 78U,
        },
        {
            .instruction = {0x02U, 0x5351U},
            .logical_address = static_cast<std::uint16_t>(0x3380U + retraction.slot),
            .value = retraction.vertical_high,
            .master_cycle_offset = 114U,
        },
        {
            .instruction = {0x02U, 0x535BU},
            .logical_address = 0x2D15U,
            .value = retraction.charge,
            .master_cycle_offset = 165U,
        },
    };
    if (retraction.has_appearance_write != 0U) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x02U, 0x5390U},
            .logical_address = static_cast<std::uint16_t>(0x3980U + retraction.slot),
            .value = retraction.appearance,
            .master_cycle_offset = body_cycles,
        });
    }
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
replace_devils_coffin_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x03U, .logical_pc = 0x63E1U};
    constexpr std::uint32_t kBodyCycles = 249U;
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_devils_coffin_initializer initializer{};
    if (!dc_classify_devils_coffin_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x63U;
    output.status = static_cast<std::uint8_t>(entry.registers.status & 0x40U);
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    const auto stack_address = [stack = entry.registers.stack_pointer](
                                   const std::uint8_t adjustment) {
        return static_cast<std::uint16_t>(
            0x2100U + static_cast<std::uint8_t>(stack - adjustment));
    };
    add_write(0x03U, 0x63E8U, 0x380FU, 0x40U, 42U);
    add_write(0x03U, 0x63EBU, 0x398FU, 0x00U, 57U);
    add_write(0x03U, 0x63EEU, stack_address(0U), 0x63U, 57U);
    add_write(0x03U, 0x63EEU, stack_address(1U), 0xF0U, 57U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0xF0U, 114U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x63U, 138U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0xF1U, 183U);
    add_write(0x01U, 0xC5E1U, static_cast<std::uint16_t>(0x3F00U + initializer.slot),
              0xF1U, 222U);
    add_write(0x01U, 0xC5E6U, static_cast<std::uint16_t>(0x3F80U + initializer.slot),
              0x63U, 249U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = kBodyCycles - 3U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {0x01U, 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_dragon_egg_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x433FU};
    constexpr std::uint32_t kBodyCycles = 213U;
    if (entry.routine != kRoutine || entry.registers.status != 0x02U) {
        return std::nullopt;
    }

    dc_dragon_egg_initializer initializer{};
    if (!dc_classify_dragon_egg_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x43U;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    const auto stack_address = [stack = entry.registers.stack_pointer](
                                   const std::uint8_t adjustment) {
        return static_cast<std::uint16_t>(
            0x2100U + static_cast<std::uint8_t>(stack - adjustment));
    };
    add_write(0x02U, 0x433FU, 0x2CF4U, initializer.live_egg_count_after, 21U);
    add_write(0x02U, 0x4342U, stack_address(0U), 0x43U, 21U);
    add_write(0x02U, 0x4342U, stack_address(1U), 0x44U, 21U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0x44U, 78U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x43U, 102U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0x45U, 147U);
    add_write(0x01U, 0xC5E1U, static_cast<std::uint16_t>(0x3F00U + initializer.slot),
              0x45U, 186U);
    add_write(0x01U, 0xC5E6U, static_cast<std::uint16_t>(0x3F80U + initializer.slot),
              0x43U, 213U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = kBodyCycles - 3U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {0x01U, 0xC5E6U},
        .registers = output,
    });
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
    absorb_return(
        replacement,
        work_ram,
        entry.registers.stack_pointer,
        {0x03U, 0x60B2U},
        48U,
        output);
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
replace_fixed_scheduler_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x03U, .logical_pc = 0x7B1FU};
    constexpr std::uint32_t kBodyCycles = 213U;
    if (entry.routine != kRoutine || entry.registers.status != 0x02U) {
        return std::nullopt;
    }

    dc_fixed_scheduler_initializer initializer{};
    if (!dc_classify_fixed_scheduler_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x7BU;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    const auto stack_address = [stack = entry.registers.stack_pointer](
                                   const std::uint8_t adjustment) {
        return static_cast<std::uint16_t>(
            0x2100U + static_cast<std::uint8_t>(stack - adjustment));
    };
    add_write(0x03U, 0x7B21U, static_cast<std::uint16_t>(0x3A00U + initializer.slot),
              0x01U, 21U);
    add_write(0x03U, 0x7B24U, stack_address(0U), 0x7BU, 21U);
    add_write(0x03U, 0x7B24U, stack_address(1U), 0x26U, 21U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0x26U, 78U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x7BU, 102U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0x27U, 147U);
    add_write(0x01U, 0xC5E1U, static_cast<std::uint16_t>(0x3F00U + initializer.slot),
              0x27U, 186U);
    add_write(0x01U, 0xC5E6U, static_cast<std::uint16_t>(0x3F80U + initializer.slot),
              0x7BU, 213U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = kBodyCycles - 3U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {0x01U, 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_fixed_marker_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x03U, .logical_pc = 0x7B6DU};
    constexpr std::uint32_t kBodyCycles = 234U;
    if (entry.routine != kRoutine || entry.registers.status != 0x02U) {
        return std::nullopt;
    }

    dc_fixed_marker_initializer initializer{};
    if (!dc_classify_fixed_marker_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x7BU;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    const auto stack_address = [stack = entry.registers.stack_pointer](
                                   const std::uint8_t adjustment) {
        return static_cast<std::uint16_t>(
            0x2100U + static_cast<std::uint8_t>(stack - adjustment));
    };
    add_write(0x03U, 0x7B6FU, static_cast<std::uint16_t>(0x3E00U + initializer.slot),
              0x04U, 21U);
    add_write(0x03U, 0x7B74U, static_cast<std::uint16_t>(0x3A00U + initializer.slot),
              0x01U, 42U);
    add_write(0x03U, 0x7B77U, stack_address(0U), 0x7BU, 42U);
    add_write(0x03U, 0x7B77U, stack_address(1U), 0x79U, 42U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0x79U, 99U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x7BU, 123U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0x7AU, 168U);
    add_write(0x01U, 0xC5E1U, static_cast<std::uint16_t>(0x3F00U + initializer.slot),
              0x7AU, 207U);
    add_write(0x01U, 0xC5E6U, static_cast<std::uint16_t>(0x3F80U + initializer.slot),
              0x7BU, 234U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = kBodyCycles - 3U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {0x01U, 0xC5E6U},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_fixed_response_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x03U, .logical_pc = 0x7CA7U};
    constexpr std::uint32_t kBodyCycles = 219U;
    if (entry.routine != kRoutine || entry.registers.status != 0U) {
        return std::nullopt;
    }

    dc_fixed_response_initializer initializer{};
    if (!dc_classify_fixed_response_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x7CU;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    const auto stack_address = [stack = entry.registers.stack_pointer](
                                   const std::uint8_t adjustment) {
        return static_cast<std::uint16_t>(
            0x2100U + static_cast<std::uint8_t>(stack - adjustment));
    };
    add_write(0x03U, 0x7CA7U, 0x2D38U, 0x00U, 15U);
    add_write(0x03U, 0x7CAAU, 0x20CDU, 0x00U, 27U);
    add_write(0x03U, 0x7CACU, stack_address(0U), 0x7CU, 27U);
    add_write(0x03U, 0x7CACU, stack_address(1U), 0xAEU, 27U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0xAEU, 84U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x7CU, 108U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0xAFU, 153U);
    add_write(0x01U, 0xC5E1U, static_cast<std::uint16_t>(0x3F00U + initializer.slot),
              0xAFU, 192U);
    add_write(0x01U, 0xC5E6U, static_cast<std::uint16_t>(0x3F80U + initializer.slot),
              0x7CU, 219U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = kBodyCycles - 3U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {0x01U, 0xC5E6U},
        .registers = output,
    });
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
    // The dormant arm is the only one that reaches a return. The suppressed arm
    // stops at $7B87 and the active arm at $7BB1, both of which are ordinary
    // instructions the core goes on to execute, not this routine's `RTS`.
    if (!suppressed && !active) {
        absorb_return(
            replacement,
            work_ram,
            entry.registers.stack_pointer,
            {0x03U, 0x7BDFU},
            93U,
            output);
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
    absorb_return(
        replacement,
        work_ram,
        entry.registers.stack_pointer,
        {0x03U, 0x7CBBU},
        45U,
        output);
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
replace_giant_door_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x6294U,
    };
    constexpr std::uint32_t kBodyCycles = 345U;
    if (entry.routine != kRoutine || entry.registers.x != 0x4CU
        || entry.registers.accumulator != 0U || entry.registers.y != 0x0BU
        || entry.registers.status != 0x02U) {
        return std::nullopt;
    }
    const auto stack_return = proven_stack_return(entry, 0xF3U);
    if (!stack_return) {
        return std::nullopt;
    }

    dc_giant_door_initializer initializer{};
    if (!dc_classify_giant_door_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &initializer)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.accumulator = 0x62U;
    output.status = 0U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E9U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto add_write = [&replacement](
                               const std::uint8_t bank,
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    const auto slot_address = [slot = initializer.slot](const std::uint16_t base) {
        return static_cast<std::uint16_t>(base + slot);
    };
    add_write(0x03U, 0x6294U, 0x2D0AU, 0x00U, 15U);
    add_write(0x03U, 0x6297U, 0x2D06U, 0x00U, 30U);
    add_write(0x03U, 0x629AU, 0x2D07U, 0x00U, 45U);
    add_write(0x03U, 0x629DU, 0x2D08U, 0x00U, 60U);
    add_write(0x03U, 0x62A0U, 0x2D09U, 0x00U, 75U);
    add_write(0x03U, 0x62A5U, slot_address(0x3B00U), 0x03U, 96U);
    add_write(0x03U, 0x62ADU, slot_address(0x3800U), 0x10U, 132U);
    add_write(0x03U, 0x62B2U, slot_address(0x3D00U), 0x81U, 153U);
    add_write(0x03U, 0x62B5U, stack_return->high, 0x62U, 153U);
    add_write(0x03U, 0x62B5U, stack_return->low, 0xB7U, 153U);
    add_write(0x01U, 0xC5CAU, 0x2026U, 0xB7U, 210U);
    add_write(0x01U, 0xC5CDU, 0x2027U, 0x62U, 234U);
    add_write(0x01U, 0xC5D9U, 0x2026U, 0xB8U, 279U);
    add_write(0x01U, 0xC5E1U, slot_address(0x3F00U), 0xB8U, 318U);
    add_write(0x01U, 0xC5E6U, slot_address(0x3F80U), 0x62U, 345U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 342U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {.physical_bank = 0x01U, .logical_pc = 0xC5E6U},
        .registers = output,
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
    if (gate.path == DC_GIANT_DOOR_HIT_GATE_TRIGGERED) {
        const auto collision_state =
            static_cast<std::uint8_t>(gate.collision_state & 0x7FU);
        const auto score_status = set_nz(entry.registers.status, 0x32U);
        output.program_counter = 0x62C4U;
        output.accumulator = 0x32U;
        output.status = score_status;

        RoutineReplacement replacement{
            .return_instruction = {
                .physical_bank = 0x03U,
                .logical_pc = 0x62C4U,
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
        const auto add_write = [&replacement](
                                   const std::uint16_t instruction,
                                   const std::uint16_t address,
                                   const std::uint8_t value,
                                   const std::uint32_t cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {.physical_bank = 0x03U, .logical_pc = instruction},
                .logical_address = address,
                .value = value,
                .master_cycle_offset = cycle,
            });
        };

        auto checkpoint = entry.registers;
        checkpoint.accumulator = gate.collision_state;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0x62B8U, 0x62BBU, 15U, checkpoint);
        append_checkpoint(0x62BBU, 0x62BDU, 21U, checkpoint);

        checkpoint.accumulator = collision_state;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0x62BDU, 0x62BFU, 27U, checkpoint);
        append_checkpoint(0x62BFU, 0x62C2U, 42U, checkpoint);

        checkpoint.accumulator = 0x32U;
        checkpoint.status = score_status;
        append_checkpoint(0x62C2U, 0x62C4U, 48U, checkpoint);

        add_write(0x62BFU, static_cast<std::uint16_t>(0x3800U + gate.slot),
                  collision_state, 42U);
        return replacement;
    }

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
replace_giant_door_hit_continuation(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // After the shared score helper, each Giant Door hit decrements its
    // three-hit counter and selects the original impact or completion path.
    // Keep both helper bodies in assembly.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x62CBU,
    };
    constexpr std::uint8_t kGiantDoorSlot = 0x4CU;
    constexpr std::size_t kCounterOffset = 0x1B00U + kGiantDoorSlot;
    if (entry.routine != kRoutine || entry.registers.x != kGiantDoorSlot
        || work_ram.size() <= kCounterOffset
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    const auto counter = std::to_integer<std::uint8_t>(work_ram[kCounterOffset]);
    if (counter == 0U || counter > 3U) {
        return std::nullopt;
    }
    const auto decremented = static_cast<std::uint8_t>(counter - 1U);
    const bool terminal = decremented == 0U;
    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };

    auto output = entry.registers;
    output.program_counter = terminal ? 0x62D4U : 0x62D0U;
    output.status = set_nz(output.status, decremented);

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = output.program_counter,
        },
        .output_registers = output,
        .body_master_cycles = terminal ? 33U : 27U,
    };
    auto after_decrement = output;
    after_decrement.program_counter = 0x62CEU;
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 15U,
        .master_cycle_offset = 18U,
        .instruction = {0x03U, 0x62CBU},
        .registers = after_decrement,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x62CBU},
        .logical_address = static_cast<std::uint16_t>(0x3B00U + kGiantDoorSlot),
        .value = decremented,
        .master_cycle_offset = 21U,
    });
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = terminal ? 30U : 24U,
        .master_cycle_offset = terminal ? 33U : 27U,
        .instruction = {0x03U, 0x62CEU},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_giant_door_completion_score_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x62D4U,
    };
    constexpr std::uint8_t kGiantDoorSlot = 0x4CU;
    if (entry.routine != kRoutine || entry.registers.x != kGiantDoorSlot
        || work_ram.size() < 0x0200U
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
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
            .instruction = {0x03U, instruction_pc},
            .registers = registers,
        });
    };

    constexpr std::uint8_t kAwardLow = 0xF4U;
    constexpr std::uint8_t kAwardHigh = 0x01U;
    const auto stack_high = entry.registers.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);

    auto output = entry.registers;
    output.accumulator = kAwardHigh;
    output.status = set_nz(output.status, output.accumulator);
    output.stack_pointer = static_cast<std::uint8_t>(output.stack_pointer - 2U);
    output.program_counter = 0xC02AU;

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xC02AU,
        },
        .output_registers = output,
        .body_master_cycles = 57U,
    };
    replacement.work_ram_writes = {
        {
            .instruction = {0x03U, 0x62D6U},
            .logical_address = 0x202FU,
            .value = kAwardLow,
            .master_cycle_offset = 18U,
        },
        {
            .instruction = {0x03U, 0x62DAU},
            .logical_address = 0x2030U,
            .value = kAwardHigh,
            .master_cycle_offset = 36U,
        },
        {
            .instruction = {0x03U, 0x62DCU},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_high),
            .value = 0x62U,
            .master_cycle_offset = 36U,
        },
        {
            .instruction = {0x03U, 0x62DCU},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_low),
            .value = 0xDEU,
            .master_cycle_offset = 36U,
        },
    };

    auto checkpoint = entry.registers;
    checkpoint.accumulator = kAwardLow;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x62D4U, 0x62D6U, 6U, checkpoint);
    append_checkpoint(replacement, 0x62D6U, 0x62D8U, 18U, checkpoint);
    checkpoint.accumulator = kAwardHigh;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x62D8U, 0x62DAU, 24U, checkpoint);
    append_checkpoint(replacement, 0x62DAU, 0x62DCU, 36U, checkpoint);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 2U);
    append_checkpoint(replacement, 0x62DCU, 0xC02AU, 57U, checkpoint);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_giant_door_completion_scheduler_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x62E2U,
    };
    constexpr std::uint8_t kGiantDoorSlot = 0x4CU;
    constexpr std::size_t kCompletionStateOffset = 0x10CCU;
    constexpr std::size_t kHitCounterOffset = 0x1B4CU;
    if (entry.routine != kRoutine || entry.registers.x != kGiantDoorSlot
        || work_ram.size() <= 0x1FCCU
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto append_checkpoint = [](RoutineReplacement& replacement,
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

    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    const auto completed_state = static_cast<std::uint8_t>(
        static_cast<std::uint8_t>(staged[kCompletionStateOffset]) | 0x60U);
    auto checkpoint = entry.registers;
    checkpoint.accumulator = completed_state;
    checkpoint.status = set_nz(checkpoint.status, completed_state);

    const auto stack_high = checkpoint.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
    if (staged.size() <= 0x0100U + stack_high
        || staged.size() <= 0x0100U + stack_low) {
        return std::nullopt;
    }

    RoutineReplacement replacement{};
    replacement.work_ram_writes = {
        {
            .instruction = {0x03U, 0x62E7U},
            .logical_address = 0x30CCU,
            .value = completed_state,
            .master_cycle_offset = 36U,
        },
        {
            .instruction = {0x03U, 0x62EAU},
            .logical_address = 0x3B4CU,
            .value = 0x00U,
            .master_cycle_offset = 51U,
        },
        {
            .instruction = {0x03U, 0x62EDU},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_high),
            .value = 0x62U,
            .master_cycle_offset = 51U,
        },
        {
            .instruction = {0x03U, 0x62EDU},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_low),
            .value = 0xEFU,
            .master_cycle_offset = 51U,
        },
    };
    staged[kCompletionStateOffset] = static_cast<std::byte>(completed_state);
    staged[kHitCounterOffset] = std::byte{0};
    staged[0x0100U + stack_high] = std::byte{0x62};
    staged[0x0100U + stack_low] = std::byte{0xEF};
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);

    append_checkpoint(replacement, 0x03U, 0x62E2U, 0x62E5U, 15U, checkpoint);
    append_checkpoint(replacement, 0x03U, 0x62E5U, 0x62E7U, 21U, checkpoint);
    append_checkpoint(replacement, 0x03U, 0x62E7U, 0x62EAU, 36U, checkpoint);
    append_checkpoint(replacement, 0x03U, 0x62EAU, 0x62EDU, 51U, checkpoint);
    append_checkpoint(replacement, 0x03U, 0x62EDU, 0xC018U, 72U, checkpoint);
    append_checkpoint(replacement, 0x01U, 0xC018U, 0xC5C9U, 84U, checkpoint);

    RoutineEntry scheduler_entry{
        .routine = {.physical_bank = 0x01U, .logical_pc = 0xC5C9U},
        .registers = checkpoint,
    };
    auto scheduler = replace_coroutine_scheduler(scheduler_entry, staged);
    if (!scheduler || scheduler->checkpoints.empty()) {
        return std::nullopt;
    }

    constexpr std::uint32_t kSchedulerOffset = 84U;
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
replace_giant_door_completion_request_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x62F0U,
    };
    constexpr std::uint8_t kGiantDoorSlot = 0x4CU;
    constexpr std::size_t kHitCounterOffset = 0x1B4CU;
    constexpr std::size_t kRequestBaseOffset = 0x0D06U;
    if (entry.routine != kRoutine || entry.registers.x != kGiantDoorSlot
        || work_ram.size() <= 0x1FCCU
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto append_checkpoint = [](RoutineReplacement& replacement,
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

    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    const auto channel = static_cast<std::uint8_t>(staged[kHitCounterOffset]);
    const auto request_offset = kRequestBaseOffset + channel;
    if (request_offset >= staged.size()) {
        return std::nullopt;
    }
    const auto requested = static_cast<std::uint8_t>(
        static_cast<std::uint8_t>(staged[request_offset]) | 0x01U);
    auto checkpoint = entry.registers;
    checkpoint.y = channel;
    checkpoint.accumulator = requested;
    checkpoint.status = set_nz(checkpoint.status, requested);

    const auto stack_high = checkpoint.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
    if (staged.size() <= 0x0100U + stack_high
        || staged.size() <= 0x0100U + stack_low) {
        return std::nullopt;
    }

    RoutineReplacement replacement{};
    replacement.work_ram_writes = {
        {
            .instruction = {0x03U, 0x62F8U},
            .logical_address = static_cast<std::uint16_t>(0x2D06U + channel),
            .value = requested,
            .master_cycle_offset = 51U,
        },
        {
            .instruction = {0x03U, 0x62FBU},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_high),
            .value = 0x62U,
            .master_cycle_offset = 51U,
        },
        {
            .instruction = {0x03U, 0x62FBU},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_low),
            .value = 0xFDU,
            .master_cycle_offset = 51U,
        },
    };
    staged[request_offset] = static_cast<std::byte>(requested);
    staged[0x0100U + stack_high] = std::byte{0x62};
    staged[0x0100U + stack_low] = std::byte{0xFDU};
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);

    append_checkpoint(replacement, 0x03U, 0x62F0U, 0x62F3U, 15U, checkpoint);
    append_checkpoint(replacement, 0x03U, 0x62F3U, 0x62F6U, 30U, checkpoint);
    append_checkpoint(replacement, 0x03U, 0x62F6U, 0x62F8U, 36U, checkpoint);
    append_checkpoint(replacement, 0x03U, 0x62F8U, 0x62FBU, 51U, checkpoint);
    append_checkpoint(replacement, 0x03U, 0x62FBU, 0xC018U, 72U, checkpoint);
    append_checkpoint(replacement, 0x01U, 0xC018U, 0xC5C9U, 84U, checkpoint);

    RoutineEntry scheduler_entry{
        .routine = {.physical_bank = 0x01U, .logical_pc = 0xC5C9U},
        .registers = checkpoint,
    };
    auto scheduler = replace_coroutine_scheduler(scheduler_entry, staged);
    if (!scheduler || scheduler->checkpoints.empty()) {
        return std::nullopt;
    }

    constexpr std::uint32_t kSchedulerOffset = 84U;
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
replace_giant_door_completion_wait_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The completion coroutine polls bit one of its assigned channel.  A set
    // bit enters the unprofiled Knight-release body; the clear path is a
    // timing-sensitive wait leaf and is safe to replace directly.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x62FEU,
    };
    constexpr std::uint8_t kGiantDoorSlot = 0x4CU;
    constexpr std::size_t kChannelOffset = 0x1B4CU;
    constexpr std::size_t kRequestBaseOffset = 0x0D06U;
    if (entry.routine != kRoutine || entry.registers.x != kGiantDoorSlot
        || work_ram.size() <= kChannelOffset
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    const auto channel =
        std::to_integer<std::uint8_t>(work_ram[kChannelOffset]);
    const auto request_offset = kRequestBaseOffset + channel;
    if (request_offset >= work_ram.size()) {
        return std::nullopt;
    }
    const auto channel_flags =
        std::to_integer<std::uint8_t>(work_ram[request_offset]);
    if ((channel_flags & 0x02U) != 0U) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
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
            .instruction = {0x03U, instruction_pc},
            .registers = registers,
        });
    };

    auto after_load_channel = entry.registers;
    after_load_channel.y = channel;
    auto output = after_load_channel;
    output.program_counter = 0x6314U;
    output.accumulator = 0x00U;
    output.status = set_nz(output.status, output.accumulator);
    auto after_load_channel_with_and_status = after_load_channel;
    after_load_channel_with_and_status.status = output.status;

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x6314U,
        },
        .output_registers = output,
        .body_master_cycles = 48U,
    };
    append_checkpoint(replacement, 0x62FEU, 0x6301U, 15U,
                      after_load_channel_with_and_status);
    // Timer delivery can occur while the core advances the preceding LDA
    // checkpoint, with the AND's zero flag already visible at that boundary.
    append_checkpoint(replacement, 0x6301U, 0x6304U, 30U, output);
    replacement.checkpoints.back().interrupt_sample_master_cycle_offset = 15U;
    append_checkpoint(replacement, 0x6304U, 0x6306U, 36U, output);
    append_checkpoint(replacement, 0x6306U, 0x6314U, 48U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_giant_door_completion_channel_advance(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // A release request advances through the four completion channels and
    // reschedules this coroutine.  The terminal channel hands the scheduler
    // the post-release return address at 03:6318 instead of 03:62F0.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x6308U,
    };
    constexpr std::uint8_t kGiantDoorSlot = 0x4CU;
    constexpr std::size_t kChannelOffset = 0x1B4CU;
    if (entry.routine != kRoutine || entry.registers.x != kGiantDoorSlot
        || work_ram.size() <= 0x1FCCU
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    const auto channel =
        std::to_integer<std::uint8_t>(work_ram[kChannelOffset]);
    if (channel >= 4U) {
        return std::nullopt;
    }
    const auto advanced_channel = static_cast<std::uint8_t>(channel + 1U);
    const auto terminal_channel = advanced_channel == 4U;
    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto append_checkpoint = [](RoutineReplacement& replacement,
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

    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    auto checkpoint = entry.registers;
    checkpoint.accumulator = advanced_channel;
    checkpoint.status = set_nz(checkpoint.status, advanced_channel);
    const auto stack_high = checkpoint.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
    if (staged.size() <= 0x0100U + stack_high
        || staged.size() <= 0x0100U + stack_low) {
        return std::nullopt;
    }

    RoutineReplacement replacement{};
    const auto scheduler_return_high =
        static_cast<std::uint8_t>(terminal_channel ? 0x63U : 0x62U);
    const auto scheduler_return_low =
        static_cast<std::uint8_t>(terminal_channel ? 0x17U : 0xEFU);
    const auto scheduler_call_instruction =
        static_cast<std::uint16_t>(terminal_channel ? 0x6315U : 0x62EDU);
    const auto scheduler_call_offset =
        static_cast<std::uint32_t>(terminal_channel ? 54U : 60U);
    replacement.work_ram_writes = {
        {
            .instruction = {0x03U, 0x6308U},
            .logical_address = static_cast<std::uint16_t>(0x3B00U + kGiantDoorSlot),
            .value = advanced_channel,
            .master_cycle_offset = 21U,
        },
        {
            .instruction = {0x03U, scheduler_call_instruction},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_high),
            .value = scheduler_return_high,
            .master_cycle_offset = scheduler_call_offset,
        },
        {
            .instruction = {0x03U, scheduler_call_instruction},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_low),
            .value = scheduler_return_low,
            .master_cycle_offset = scheduler_call_offset,
        },
    };
    staged[kChannelOffset] = static_cast<std::byte>(advanced_channel);
    staged[0x0100U + stack_high] = static_cast<std::byte>(scheduler_return_high);
    staged[0x0100U + stack_low] = static_cast<std::byte>(scheduler_return_low);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);

    append_checkpoint(replacement, 0x03U, 0x6308U, 0x630BU, 21U, checkpoint);
    append_checkpoint(replacement, 0x03U, 0x630BU, 0x630EU, 36U, checkpoint);
    checkpoint.status = static_cast<std::uint8_t>(checkpoint.status & 0x7CU);
    if (!terminal_channel) {
        checkpoint.status = static_cast<std::uint8_t>(checkpoint.status | 0x80U);
    } else {
        checkpoint.status = static_cast<std::uint8_t>(checkpoint.status | 0x03U);
    }
    append_checkpoint(replacement, 0x03U, 0x630EU, 0x6310U, 42U, checkpoint);
    const auto scheduler_offset =
        static_cast<std::uint32_t>(terminal_channel ? 87U : 93U);
    if (terminal_channel) {
        append_checkpoint(replacement, 0x03U, 0x6310U, 0x6315U, 54U, checkpoint);
        append_checkpoint(replacement, 0x03U, 0x6315U, 0xC018U, 75U, checkpoint);
    } else {
        append_checkpoint(replacement, 0x03U, 0x6310U, 0x6312U, 48U, checkpoint);
        append_checkpoint(replacement, 0x03U, 0x6312U, 0x62EDU, 60U, checkpoint);
        append_checkpoint(replacement, 0x03U, 0x62EDU, 0xC018U, 81U, checkpoint);
    }
    append_checkpoint(replacement, 0x01U, 0xC018U, 0xC5C9U, scheduler_offset,
                      checkpoint);

    RoutineEntry scheduler_entry{
        .routine = {.physical_bank = 0x01U, .logical_pc = 0xC5C9U},
        .registers = checkpoint,
    };
    auto scheduler = replace_coroutine_scheduler(scheduler_entry, staged);
    if (!scheduler || scheduler->checkpoints.empty()) {
        return std::nullopt;
    }

    for (auto write : scheduler->work_ram_writes) {
        write.master_cycle_offset += scheduler_offset;
        replacement.work_ram_writes.push_back(write);
    }
    for (auto child_checkpoint : scheduler->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset +=
            scheduler_offset;
        child_checkpoint.master_cycle_offset += scheduler_offset;
        replacement.checkpoints.push_back(child_checkpoint);
    }
    replacement.return_instruction = scheduler->return_instruction;
    replacement.output_registers = scheduler->output_registers;
    replacement.body_master_cycles =
        scheduler_offset + scheduler->body_master_cycles;
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
replace_giant_face_animator_active_wait(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x76C5U,
    };
    if (entry.routine != kRoutine || entry.master_cycles_until_user_event <= 27U
        || entry.master_cycles_until_timer_interrupt <= 27U) {
        return std::nullopt;
    }

    dc_giant_face_animator_active_wait wait{};
    if (!dc_classify_giant_face_animator_active_wait(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &wait)) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };

    auto output = entry.registers;
    output.status = set_nz(output.status, wait.decremented_timer);
    output.program_counter = 0x76CAU;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x03U, .logical_pc = 0x76CAU},
        .output_registers = output,
        .body_master_cycles = 27U,
    };
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x76C5U},
        .logical_address = 0x3A37U,
        .value = wait.decremented_timer,
        .master_cycle_offset = 21U,
    });
    auto checkpoint = entry.registers;
    checkpoint.status = output.status;
    checkpoint.program_counter = 0x76C8U;
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 18U,
        .master_cycle_offset = 21U,
        .instruction = {0x03U, 0x76C5U},
        .registers = checkpoint,
    });
    checkpoint.program_counter = 0x76CAU;
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 24U,
        .master_cycle_offset = 27U,
        .instruction = {0x03U, 0x76C8U},
        .registers = checkpoint,
    });
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
    constexpr RoutineKey kWalledStarAccessRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x4B3FU,
    };
    constexpr RoutineKey kLowerLeftSideStarAccessRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x4BD5U,
    };
    constexpr RoutineKey kUpperRightSideStarAccessRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x4C0AU,
    };
    if (entry.routine != kRoutine && entry.routine != kWallPocketRoutine
        && entry.routine != kWalledStarAccessRoutine
        && entry.routine != kLowerLeftSideStarAccessRoutine
        && entry.routine != kUpperRightSideStarAccessRoutine) {
        return std::nullopt;
    }

    dc_giant_face_helmet_controller controller{};
    const auto classified = entry.routine == kWallPocketRoutine
        ? dc_classify_four_star_wall_pocket_controller(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &controller)
        : entry.routine == kWalledStarAccessRoutine
        ? dc_classify_walled_star_access_controller(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &controller)
        : entry.routine == kLowerLeftSideStarAccessRoutine
                || entry.routine == kUpperRightSideStarAccessRoutine
        ? dc_classify_side_star_access_controller(
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
replace_giant_face_helmet_hit_continuation(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4CFBU};
    constexpr std::uint8_t kLeftHelmetSlot = 0x2AU;
    constexpr std::uint8_t kRightHelmetSlot = 0x2BU;
    constexpr std::uint32_t kSchedulerOffset = 237U;
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x01U
        || (entry.registers.x != kLeftHelmetSlot
            && entry.registers.x != kRightHelmetSlot)
        || entry.registers.y != 0U
        || entry.registers.stack_pointer != 0xFBU || entry.registers.status != 0U
        || work_ram.size() <= 0x1FAAU
        || std::to_integer<std::uint8_t>(work_ram[0x0D0CU]) != 0x01U) {
        return std::nullopt;
    }

    // The preceding collision path owns score/audio and the Giant-Face wake
    // request. Once it jumps here, this continuation only restores the ball,
    // advances the helmet phase, and installs $4D0B through C018.
    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    const auto stage = [&staged](const std::uint16_t logical_address,
                                 const std::uint8_t value) {
        staged[static_cast<std::size_t>(logical_address - 0x2000U)] =
            static_cast<std::byte>(value);
    };
    const auto stack_high = entry.registers.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
    const auto stack_high_address = static_cast<std::uint16_t>(0x2100U + stack_high);
    const auto stack_low_address = static_cast<std::uint16_t>(0x2100U + stack_low);
    const auto helmet_slot = entry.registers.x;
    const auto ball_x_low = static_cast<std::uint8_t>(
        helmet_slot == kLeftHelmetSlot ? 0x86U : 0x9EU);
    const auto helmet_timer_address = static_cast<std::uint16_t>(0x3A00U + helmet_slot);
    stage(helmet_timer_address, 0x04U);
    stage(stack_high_address, 0x4DU);
    stage(stack_low_address, 0x02U);
    stage(0x3180U, ball_x_low);
    stage(0x3200U, 0x00U);
    stage(0x3300U, 0x24U);
    stage(0x3380U, 0x01U);
    stage(0x3980U, 0x04U);
    stage(stack_high_address, 0x4DU);
    stage(stack_low_address, 0x0AU);

    RoutineReplacement replacement{};
    const auto add_write = [&replacement](const std::uint8_t bank,
                                           const std::uint16_t instruction,
                                           const std::uint16_t address,
                                           const std::uint8_t value,
                                           const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = bank, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    add_write(0x02U, 0x4CFDU, helmet_timer_address, 0x04U, 21U);
    add_write(0x02U, 0x4D00U, stack_high_address, 0x4DU, 21U);
    add_write(0x02U, 0x4D00U, stack_low_address, 0x02U, 21U);
    add_write(0x02U, 0x4072U, 0x3180U, ball_x_low, 72U);
    add_write(0x02U, 0x4078U, 0x3200U, 0x00U, 102U);
    add_write(0x02U, 0x407EU, 0x3300U, 0x24U, 132U);
    add_write(0x02U, 0x4084U, 0x3380U, 0x01U, 162U);
    add_write(0x02U, 0x4D05U, 0x3980U, 0x04U, 204U);
    add_write(0x02U, 0x4D08U, stack_high_address, 0x4DU, 204U);
    add_write(0x02U, 0x4D08U, stack_low_address, 0x0AU, 204U);

    auto scheduler_registers = entry.registers;
    scheduler_registers.accumulator = 0x04U;
    scheduler_registers.program_counter = 0xC5C9U;
    scheduler_registers.stack_pointer = static_cast<std::uint8_t>(
        scheduler_registers.stack_pointer - 2U);
    RoutineEntry scheduler_entry{
        .routine = {.physical_bank = 0x01U, .logical_pc = 0xC5C9U},
        .registers = scheduler_registers,
    };
    auto scheduler = replace_coroutine_scheduler(scheduler_entry, staged);
    if (!scheduler || scheduler->checkpoints.empty()) {
        return std::nullopt;
    }
    for (auto write : scheduler->work_ram_writes) {
        write.master_cycle_offset += kSchedulerOffset;
        replacement.work_ram_writes.push_back(write);
    }
    for (auto checkpoint : scheduler->checkpoints) {
        checkpoint.interrupt_sample_master_cycle_offset += kSchedulerOffset;
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
replace_giant_face_helmet_followup_timer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4D0BU};
    constexpr std::uint8_t kLeftHelmetSlot = 0x2AU;
    constexpr std::uint8_t kRightHelmetSlot = 0x2BU;
    constexpr std::size_t kAppearanceOffset = 0x1980U;
    if (entry.routine != kRoutine || entry.registers.accumulator != 0x4DU
        || (entry.registers.x != kLeftHelmetSlot
            && entry.registers.x != kRightHelmetSlot)
        || entry.registers.stack_pointer != 0xFBU
        || (entry.registers.status & 0x28U) != 0U
        || work_ram.size() <= kAppearanceOffset) {
        return std::nullopt;
    }

    const auto timer_offset = static_cast<std::size_t>(0x1A00U + entry.registers.x);
    if (work_ram.size() <= timer_offset) {
        return std::nullopt;
    }
    const auto appearance = std::to_integer<std::uint8_t>(work_ram[kAppearanceOffset]);
    if (appearance == 0x06U) {
        return std::nullopt;
    }
    const auto timer = std::to_integer<std::uint8_t>(work_ram[timer_offset]);
    const auto decremented_timer = static_cast<std::uint8_t>(timer - 1U);
    const auto advances = decremented_timer == 0U;

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto set_compare = [&set_nz](
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

    auto after_appearance_load = entry.registers;
    after_appearance_load.accumulator = appearance;
    after_appearance_load.status = set_nz(
        after_appearance_load.status, appearance);
    auto after_compare = after_appearance_load;
    after_compare.status = set_compare(after_compare.status, appearance, 0x06U);
    auto after_decrement = after_compare;
    after_decrement.status = set_nz(after_decrement.status, decremented_timer);

    auto output = after_decrement;
    std::uint32_t body_master_cycles = 60U;
    if (advances) {
        output.accumulator = 0x04U;
        output.status = set_nz(
            output.status,
            static_cast<std::uint8_t>(appearance + 1U));
        body_master_cycles = 96U;
    }
    output.program_counter = 0x4D1FU;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4D1FU},
        .output_registers = output,
        .body_master_cycles = body_master_cycles,
    };
    const auto timer_address = static_cast<std::uint16_t>(0x3A00U + entry.registers.x);
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x4D12U},
        .logical_address = timer_address,
        .value = decremented_timer,
        .master_cycle_offset = 48U,
    });
    if (advances) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x02U, 0x4D19U},
            .logical_address = timer_address,
            .value = 0x04U,
            .master_cycle_offset = 75U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x02U, 0x4D1CU},
            .logical_address = 0x3980U,
            .value = static_cast<std::uint8_t>(appearance + 1U),
            .master_cycle_offset = 96U,
        });
    }
    const auto append_checkpoint = [&replacement](
                                       const std::uint16_t instruction_pc,
                                       const std::uint16_t resume_pc,
                                       const std::uint32_t master_cycle,
                                       auto registers,
                                       const std::uint32_t interrupt_sample_master_cycle = 0U) {
        registers.program_counter = resume_pc;
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = interrupt_sample_master_cycle == 0U
                ? master_cycle - 3U
                : interrupt_sample_master_cycle,
            .master_cycle_offset = master_cycle,
            .instruction = {0x02U, instruction_pc},
            .registers = registers,
        });
    };
    append_checkpoint(0x4D0BU, 0x4D0EU, 15U, after_appearance_load);
    append_checkpoint(0x4D0EU, 0x4D10U, 21U, after_compare);
    append_checkpoint(0x4D12U, 0x4D15U, 48U, after_decrement, 42U);
    if (!advances) {
        append_checkpoint(0x4D15U, 0x4D1FU, 60U, output);
        return replacement;
    }

    append_checkpoint(0x4D15U, 0x4D17U, 60U, after_decrement);
    auto after_timer_load = after_decrement;
    after_timer_load.accumulator = 0x04U;
    after_timer_load.status = set_nz(after_timer_load.status, 0x04U);
    append_checkpoint(0x4D17U, 0x4D19U, 66U, after_timer_load);
    append_checkpoint(0x4D19U, 0x4D1CU, 75U, after_timer_load);
    append_checkpoint(0x4D1CU, 0x4D1FU, 96U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_giant_face_helmet_initializer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4C78U};
    if (entry.routine != kRoutine || entry.registers.y != 0x0BU) {
        return std::nullopt;
    }

    dc_giant_face_helmet_initializer initializer{};
    if (!dc_classify_giant_face_helmet_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, entry.registers.accumulator,
            &initializer)) {
        return std::nullopt;
    }

    constexpr std::uint32_t kBodyCycles = 240U;
    auto output = entry.registers;
    output.program_counter = 0x4CF1U;
    output.accumulator = 0x01U;
    output.status = 0x80U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4CF1U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto slot_address = [slot = initializer.slot](const std::uint16_t base) {
        return static_cast<std::uint16_t>(base + slot);
    };
    const auto add_write = [&replacement](
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = 0x02U, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    add_write(0x4C7AU, slot_address(0x3A00U), 0x01U, 21U);
    add_write(0x4C7FU, slot_address(0x3E00U), 0x78U, 42U);
    add_write(0x4C84U, slot_address(0x3E80U), 0x4CU, 63U);
    add_write(0x4C89U, slot_address(0x3F00U), 0x91U, 84U);
    add_write(0x4C8EU, slot_address(0x3F80U), 0x4CU, 105U);
    add_write(0x4CDAU, slot_address(0x3A00U), 0x00U, 159U);
    add_write(0x4CE1U, slot_address(0x3A00U), 0x04U, 186U);
    add_write(0x4CE4U, slot_address(0x3980U), 0x01U, 207U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = kBodyCycles - 3U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {0x02U, 0x4CECU},
        .registers = output,
    });
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_giant_face_helmet_outer_initializers(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kLeftRoutine{.physical_bank = 0x02U, .logical_pc = 0x4A5CU};
    constexpr RoutineKey kRightRoutine{.physical_bank = 0x02U, .logical_pc = 0x4A64U};
    if ((entry.routine != kLeftRoutine && entry.routine != kRightRoutine)
        || entry.registers.y != 0x0BU) {
        return std::nullopt;
    }

    const auto selector = static_cast<std::uint8_t>(
        entry.routine == kLeftRoutine ? 0x13U : 0x1DU);
    dc_giant_face_helmet_initializer initializer{};
    if (!dc_classify_giant_face_helmet_initializer(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, selector, &initializer)) {
        return std::nullopt;
    }

    // $4A5C/$4A64 load their fixed selector, store it at $3C80,X, then jump
    // into the shared $4C78 setup.  The wrapper shifts every child event by
    // 33 master cycles and returns through the same $4CF1 RTS.
    constexpr std::uint32_t kBodyCycles = 273U;
    constexpr std::uint32_t kSharedCycleOffset = 33U;
    const auto entry_store_instruction = static_cast<std::uint16_t>(
        entry.routine == kLeftRoutine ? 0x4A5EU : 0x4A66U);
    auto output = entry.registers;
    output.program_counter = 0x4CF1U;
    output.accumulator = 0x01U;
    output.status = 0x80U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4CF1U},
        .output_registers = output,
        .body_master_cycles = kBodyCycles,
    };
    const auto slot_address = [slot = initializer.slot](const std::uint16_t base) {
        return static_cast<std::uint16_t>(base + slot);
    };
    const auto add_write = [&replacement](
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value,
                               const std::uint32_t cycle) {
        replacement.work_ram_writes.push_back({
            .instruction = {.physical_bank = 0x02U, .logical_pc = instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };
    add_write(entry_store_instruction, slot_address(0x3C80U), selector, 21U);
    add_write(0x4C7AU, slot_address(0x3A00U), 0x01U, 21U + kSharedCycleOffset);
    add_write(0x4C7FU, slot_address(0x3E00U), 0x78U, 42U + kSharedCycleOffset);
    add_write(0x4C84U, slot_address(0x3E80U), 0x4CU, 63U + kSharedCycleOffset);
    add_write(0x4C89U, slot_address(0x3F00U), 0x91U, 84U + kSharedCycleOffset);
    add_write(0x4C8EU, slot_address(0x3F80U), 0x4CU, 105U + kSharedCycleOffset);
    add_write(0x4CDAU, slot_address(0x3A00U), 0x00U, 159U + kSharedCycleOffset);
    add_write(0x4CE1U, slot_address(0x3A00U), 0x04U, 186U + kSharedCycleOffset);
    add_write(0x4CE4U, slot_address(0x3980U), 0x01U, 207U + kSharedCycleOffset);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = kBodyCycles - 3U,
        .master_cycle_offset = kBodyCycles,
        .instruction = {0x02U, 0x4CECU},
        .registers = output,
    });
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
    // The score core reaches F5A8 at cycle 672.  Its banked return bridge
    // consumes the original 21-cycle boundary before the stack-restoring
    // epilogue at F5A8, rather than the usual 18-cycle same-bank return.
    append_checkpoint(0x00U, 0xF5A8U, 0xF557U, 693U, checkpoint);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 1U);
    checkpoint.y = 0x01U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0x00U, 0xF557U, 0xF558U, 705U, checkpoint);
    checkpoint.y = 0x00U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0x00U, 0xF558U, 0xF559U, 711U, checkpoint);
    append_checkpoint(0x00U, 0xF559U, 0xF55BU, 717U, checkpoint);
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
replace_four_star_wall_pocket_animation(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The matching lower-left pocket schedules this continuation after its
    // opening helper. Appearance six transfers to the measured 942-cycle,
    // 22-write scene-transition body at $4B00, which remains assembly-owned.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x4AEBU,
    };
    constexpr std::uint8_t kLowerLeftSlot = 0x3EU;
    if (entry.routine != kRoutine || entry.registers.x != kLowerLeftSlot
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_four_star_wall_pocket_animation animation{};
    if (!dc_classify_four_star_wall_pocket_animation(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &animation)
        || animation.path == DC_FOUR_STAR_WALL_POCKET_ANIMATION_TRANSITION) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto set_compare = [&set_nz](
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

    auto after_appearance_load = entry.registers;
    after_appearance_load.accumulator = animation.appearance;
    after_appearance_load.status = set_nz(
        after_appearance_load.status, after_appearance_load.accumulator);
    auto after_compare = after_appearance_load;
    after_compare.status = set_compare(
        after_compare.status, after_compare.accumulator, 0x06U);
    auto after_decrement = after_compare;
    after_decrement.status = set_nz(
        after_decrement.status, animation.decremented_timer);

    auto output = after_decrement;
    std::uint32_t body_master_cycles = 60U;
    if (animation.path == DC_FOUR_STAR_WALL_POCKET_ANIMATION_ADVANCE) {
        output.accumulator = 0x04U;
        output.status = set_nz(output.status, animation.advanced_appearance);
        body_master_cycles = 96U;
    }
    output.program_counter = 0x4AFFU;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4AFFU},
        .output_registers = output,
        .body_master_cycles = body_master_cycles,
    };
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x4AF2U},
        .logical_address = static_cast<std::uint16_t>(0x3A00U + animation.slot),
        .value = animation.decremented_timer,
        .master_cycle_offset = 48U,
    });
    if (animation.path == DC_FOUR_STAR_WALL_POCKET_ANIMATION_ADVANCE) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x02U, 0x4AF9U},
            .logical_address = static_cast<std::uint16_t>(0x3A00U + animation.slot),
            .value = 0x04U,
            .master_cycle_offset = 75U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x02U, 0x4AFCU},
            .logical_address = 0x3980U,
            .value = animation.advanced_appearance,
            .master_cycle_offset = 96U,
        });
    }
    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers,
            const std::uint32_t interrupt_sample_master_cycle = 0U) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    interrupt_sample_master_cycle == 0U
                    ? master_cycle - 3U
                    : interrupt_sample_master_cycle,
                .master_cycle_offset = master_cycle,
                .instruction = {0x02U, instruction_pc},
                .registers = registers,
            });
    };
    append_checkpoint(0x4AEBU, 0x4AEEU, 15U, after_appearance_load);
    append_checkpoint(0x4AEEU, 0x4AF0U, 21U, after_compare);
    // BEQ is not an interrupt boundary on the covered non-six path.  Fold
    // it into the following DEC checkpoint so the IRQ sample observes the
    // completed decrement, exactly as the HuC6280 does at that boundary.
    append_checkpoint(0x4AF2U, 0x4AF5U, 48U, after_decrement, 42U);
    if (animation.path == DC_FOUR_STAR_WALL_POCKET_ANIMATION_WAITING) {
        append_checkpoint(0x4AF5U, 0x4AFFU, 60U, output);
        return replacement;
    }

    append_checkpoint(0x4AF5U, 0x4AF7U, 60U, after_decrement);
    auto after_timer_load = after_decrement;
    after_timer_load.accumulator = 0x04U;
    after_timer_load.status = set_nz(after_timer_load.status, 0x04U);
    append_checkpoint(0x4AF7U, 0x4AF9U, 66U, after_timer_load);
    append_checkpoint(0x4AF9U, 0x4AFCU, 81U, after_timer_load);
    append_checkpoint(0x4AFCU, 0x4AFFU, 96U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_side_star_bonus_entrance_animation(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The upper-right Side-Star's form-zero collision schedules this compact
    // six-step entrance animation. Appearance six enters a distinct
    // scene-session transition, which remains assembly-owned.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x02U,
        .logical_pc = 0x4C33U,
    };
    constexpr std::uint8_t kUpperRightSlot = 0x12U;
    if (entry.routine != kRoutine || entry.registers.x != kUpperRightSlot
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_side_star_bonus_entrance_animation animation{};
    if (!dc_classify_side_star_bonus_entrance_animation(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &animation)
        || animation.path == DC_FOUR_STAR_WALL_POCKET_ANIMATION_TRANSITION) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto set_compare = [&set_nz](
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

    auto after_appearance_load = entry.registers;
    after_appearance_load.accumulator = animation.appearance;
    after_appearance_load.status = set_nz(
        after_appearance_load.status, after_appearance_load.accumulator);
    auto after_compare = after_appearance_load;
    after_compare.status = set_compare(
        after_compare.status, after_compare.accumulator, 0x06U);
    auto after_decrement = after_compare;
    after_decrement.status = set_nz(
        after_decrement.status, animation.decremented_timer);

    auto output = after_decrement;
    std::uint32_t body_master_cycles = 60U;
    if (animation.path == DC_FOUR_STAR_WALL_POCKET_ANIMATION_ADVANCE) {
        output.accumulator = 0x04U;
        output.status = set_nz(output.status, animation.advanced_appearance);
        body_master_cycles = 96U;
    }
    output.program_counter = 0x4C47U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x4C47U},
        .output_registers = output,
        .body_master_cycles = body_master_cycles,
    };
    replacement.work_ram_writes.push_back({
        .instruction = {0x02U, 0x4C3AU},
        .logical_address = static_cast<std::uint16_t>(0x3A00U + animation.slot),
        .value = animation.decremented_timer,
        .master_cycle_offset = 48U,
    });
    if (animation.path == DC_FOUR_STAR_WALL_POCKET_ANIMATION_ADVANCE) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x02U, 0x4C41U},
            .logical_address = static_cast<std::uint16_t>(0x3A00U + animation.slot),
            .value = 0x04U,
            .master_cycle_offset = 75U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x02U, 0x4C44U},
            .logical_address = 0x3980U,
            .value = animation.advanced_appearance,
            .master_cycle_offset = 96U,
        });
    }
    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers,
            const std::uint32_t interrupt_sample_master_cycle = 0U) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    interrupt_sample_master_cycle == 0U
                    ? master_cycle - 3U
                    : interrupt_sample_master_cycle,
                .master_cycle_offset = master_cycle,
                .instruction = {0x02U, instruction_pc},
                .registers = registers,
            });
    };
    append_checkpoint(0x4C33U, 0x4C36U, 15U, after_appearance_load);
    append_checkpoint(0x4C36U, 0x4C38U, 21U, after_compare);
    append_checkpoint(0x4C3AU, 0x4C3DU, 48U, after_decrement, 42U);
    if (animation.path == DC_FOUR_STAR_WALL_POCKET_ANIMATION_WAITING) {
        append_checkpoint(0x4C3DU, 0x4C47U, 60U, output);
        return replacement;
    }

    append_checkpoint(0x4C3DU, 0x4C3FU, 60U, after_decrement);
    auto after_timer_load = after_decrement;
    after_timer_load.accumulator = 0x04U;
    after_timer_load.status = set_nz(after_timer_load.status, 0x04U);
    append_checkpoint(0x4C3FU, 0x4C41U, 66U, after_timer_load);
    append_checkpoint(0x4C41U, 0x4C44U, 81U, after_timer_load);
    append_checkpoint(0x4C44U, 0x4C47U, 96U, output);
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
replace_three_blobs_appearance_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x67B3U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_three_blobs_appearance_gate gate{};
    if (!dc_classify_three_blobs_appearance_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &gate)
        ) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
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
                .interrupt_sample_master_cycle_offset = master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    auto checkpoint = entry.registers;
    checkpoint.y = gate.phase_index;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);

    RoutineReplacement replacement{};
    append_checkpoint(replacement, 0x67B3U, 0x67B6U, 15U, checkpoint);

    checkpoint.accumulator = gate.global_state;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x67B6U, 0x67B9U, 30U, checkpoint);

    checkpoint.y = checkpoint.accumulator;
    append_checkpoint(replacement, 0x67B9U, 0x67BAU, 36U, checkpoint);

    checkpoint.accumulator = gate.appearance;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x67BAU, 0x67BDU, 51U, checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x67BDU},
        .logical_address = static_cast<std::uint16_t>(0x3D00U + gate.slot),
        .value = gate.appearance,
        .master_cycle_offset = 66U,
    });
    append_checkpoint(replacement, 0x67BDU, 0x67C0U, 66U, checkpoint);

    // A collision-active Blob continues through the original collision
    // dispatcher at 67C0.  The phase/color lookup and appearance store are
    // independent of that path, so retain the original dispatcher after the
    // exact 66-cycle prefix rather than attempting to compose its scoring,
    // audio, and state-transition behavior here.
    if (gate.path == DC_THREE_BLOBS_APPEARANCE_GATE_COLLISION) {
        checkpoint.program_counter = 0x67C0U;
        replacement.return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x67C0U,
        };
        replacement.output_registers = checkpoint;
        replacement.body_master_cycles = 66U;
        return replacement;
    }

    checkpoint.accumulator = gate.collision_state;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0x67C0U, 0x67C3U, 81U, checkpoint);

    checkpoint.program_counter = 0x67E5U;
    replacement.return_instruction = {.physical_bank = 0x03U, .logical_pc = 0x67E5U};
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = 93U;
    append_checkpoint(replacement, 0x67C3U, 0x67E5U, 93U, checkpoint);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_three_blobs_collision_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x03U, .logical_pc = 0x67C0U};
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_three_blobs_collision_gate gate{};
    if (!dc_classify_three_blobs_collision_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &gate)) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
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
                .interrupt_sample_master_cycle_offset = master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    auto output = entry.registers;
    output.accumulator = gate.collision_state;
    output.status = set_nz(output.status, output.accumulator);
    if ((gate.collision_state & 0x80U) != 0U) {
        // Keep the original audio driver and post-command state update in
        // assembly.  The latch-clear prefix, command load, and JSR frame are
        // independent and return at the original bank-one trampoline.
        RoutineReplacement replacement{};
        append_checkpoint(replacement, 0x67C0U, 0x67C3U, 15U, output);
        append_checkpoint(replacement, 0x67C3U, 0x67C5U, 21U, output);

        output.accumulator = static_cast<std::uint8_t>(output.accumulator & 0x7FU);
        output.status = set_nz(output.status, output.accumulator);
        append_checkpoint(replacement, 0x67C5U, 0x67C7U, 27U, output);
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x67C7U},
            .logical_address = static_cast<std::uint16_t>(0x3800U + gate.slot),
            .value = output.accumulator,
            .master_cycle_offset = 42U,
        });
        append_checkpoint(replacement, 0x67C7U, 0x67CAU, 42U, output);

        output.accumulator = 0x44U;
        output.status = set_nz(output.status, output.accumulator);
        append_checkpoint(replacement, 0x67CAU, 0x67CCU, 48U, output);
        const auto stack_high = output.stack_pointer;
        const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x67CCU},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_high),
            .value = 0x67U,
            .master_cycle_offset = 48U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x67CCU},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_low),
            .value = 0xCEU,
            .master_cycle_offset = 48U,
        });
        output.stack_pointer = static_cast<std::uint8_t>(output.stack_pointer - 2U);
        output.program_counter = 0xC087U;
        append_checkpoint(replacement, 0x67CCU, 0xC087U, 69U, output);
        replacement.return_instruction = {.physical_bank = 0x01U, .logical_pc = 0xC087U};
        replacement.output_registers = output;
        replacement.body_master_cycles = 69U;
        return replacement;
    }

    output.program_counter = 0x67E5U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x03U, .logical_pc = 0x67E5U},
        .output_registers = output,
        .body_master_cycles = 27U,
    };
    append_checkpoint(replacement, 0x67C0U, 0x67C3U, 15U, output);
    append_checkpoint(replacement, 0x67C3U, 0x67E5U, 27U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_three_blobs_direct_continuation(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // 66DE is the compact first-movement continuation: it calls the shared
    // scene-clear leaf at 676B, then leaves its own RTS to assembly.  Compose
    // the already classified idle child with the caller's JSR stack frame and
    // the child's return, without taking ownership of movement or collisions.
    constexpr RoutineKey kRoutine{.physical_bank = 0x03U, .logical_pc = 0x66DEU};
    constexpr std::size_t kGroupLatchOffset = 0x0CFDU;
    constexpr std::size_t kSceneLatchOffset = 0x0D3CU;
    if (entry.routine != kRoutine || work_ram.size() <= kSceneLatchOffset
        || std::to_integer<std::uint8_t>(work_ram[kGroupLatchOffset]) != 0U
        || std::to_integer<std::uint8_t>(work_ram[kSceneLatchOffset]) != 0U) {
        return std::nullopt;
    }

    dc_three_blobs_appearance_gate gate{};
    if (!dc_classify_three_blobs_appearance_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &gate)
        || gate.path != DC_THREE_BLOBS_APPEARANCE_GATE_IDLE) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
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
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    auto checkpoint = entry.registers;
    const auto stack_high = checkpoint.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
    replacement.work_ram_writes = {
        {
            .instruction = {0x03U, 0x66DEU},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_high),
            .value = 0x66U,
            .master_cycle_offset = 0U,
        },
        {
            .instruction = {0x03U, 0x66DEU},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_low),
            .value = 0xE0U,
            .master_cycle_offset = 0U,
        },
    };
    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(0x66DEU, 0x676BU, 21U, checkpoint);

    checkpoint.accumulator = 0U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x676BU, 0x676EU, 36U, checkpoint);
    append_checkpoint(0x676EU, 0x6773U, 48U, checkpoint);
    append_checkpoint(0x6773U, 0x6776U, 63U, checkpoint);
    append_checkpoint(0x6776U, 0x67B3U, 75U, checkpoint);

    checkpoint.y = gate.phase_index;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0x67B3U, 0x67B6U, 90U, checkpoint);
    checkpoint.accumulator = gate.global_state;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x67B6U, 0x67B9U, 105U, checkpoint);
    checkpoint.y = checkpoint.accumulator;
    append_checkpoint(0x67B9U, 0x67BAU, 111U, checkpoint);
    checkpoint.accumulator = gate.appearance;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x67BAU, 0x67BDU, 126U, checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x67BDU},
        .logical_address = static_cast<std::uint16_t>(0x3D00U + gate.slot),
        .value = gate.appearance,
        .master_cycle_offset = 141U,
    });
    append_checkpoint(0x67BDU, 0x67C0U, 141U, checkpoint);
    checkpoint.accumulator = gate.collision_state;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x67C0U, 0x67C3U, 156U, checkpoint);
    append_checkpoint(0x67C3U, 0x67E5U, 168U, checkpoint);

    checkpoint.stack_pointer = static_cast<std::uint8_t>(checkpoint.stack_pointer + 2U);
    checkpoint.program_counter = 0x66E1U;
    replacement.return_instruction = {.physical_bank = 0x03U, .logical_pc = 0x66E1U};
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = 189U;
    append_checkpoint(0x67E5U, 0x66E1U, 189U, checkpoint);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_sorcerer_live_controller_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // While one or more Sorcerers are live, the hidden controller only loads
    // its shared live count and branches directly to the existing color gate.
    // The zero-live delay/RNG state machine remains assembly-owned.
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x57ADU};
    constexpr std::size_t kLiveCountOffset = 0x0D0FU;
    if (entry.routine != kRoutine || work_ram.size() <= kLiveCountOffset
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }
    const auto live_count = std::to_integer<std::uint8_t>(work_ram[kLiveCountOffset]);
    if (live_count == 0U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.accumulator = live_count;
    output.status = static_cast<std::uint8_t>(output.status & ~0xA2U);
    if ((live_count & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x80U);
    }
    output.program_counter = 0x57E9U;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x02U, .logical_pc = 0x57E9U},
        .output_registers = output,
        .body_master_cycles = 27U,
    };
    replacement.checkpoints = {
        {
            .interrupt_sample_master_cycle_offset = 12U,
            .master_cycle_offset = 15U,
            .instruction = {0x02U, 0x57ADU},
            .registers = output,
        },
        {
            .interrupt_sample_master_cycle_offset = 24U,
            .master_cycle_offset = 27U,
            .instruction = {0x02U, 0x57B0U},
            .registers = output,
        },
    };
    replacement.checkpoints[0].registers.program_counter = 0x57B0U;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_three_blobs_shared_motion_idle(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // Both Three-Blobs movement phases fall through here after their own
    // position work.  A clear group latch and scene latch immediately enter
    // the same appearance/no-hit leaf at 67B3; all suspension and motion
    // states deliberately retain their original assembly paths.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x676BU,
    };
    constexpr std::size_t kGroupLatchOffset = 0x0CFDU;
    constexpr std::size_t kSceneLatchOffset = 0x0D3CU;
    if (entry.routine != kRoutine || work_ram.size() <= kSceneLatchOffset
        || std::to_integer<std::uint8_t>(work_ram[kGroupLatchOffset]) != 0U
        || std::to_integer<std::uint8_t>(work_ram[kSceneLatchOffset]) != 0U) {
        return std::nullopt;
    }

    dc_three_blobs_appearance_gate gate{};
    if (!dc_classify_three_blobs_appearance_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x, &gate)
        || gate.path != DC_THREE_BLOBS_APPEARANCE_GATE_IDLE) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    auto checkpoint = entry.registers;
    checkpoint.accumulator = 0U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    RoutineReplacement replacement{};
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
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    // The clear group/scene latches take the two branch boundaries below
    // before entering the already mapped appearance gate at 67B3.  Keeping
    // that prefix instruction-timed lets timer IRQs retain their original
    // placement rather than falling back at the conservative entry horizon.
    append_checkpoint(0x676BU, 0x676EU, 15U, checkpoint);
    append_checkpoint(0x676EU, 0x6773U, 27U, checkpoint);
    append_checkpoint(0x6773U, 0x6776U, 42U, checkpoint);
    append_checkpoint(0x6776U, 0x67B3U, 54U, checkpoint);

    checkpoint.y = gate.phase_index;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0x67B3U, 0x67B6U, 69U, checkpoint);
    checkpoint.accumulator = gate.global_state;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x67B6U, 0x67B9U, 84U, checkpoint);
    checkpoint.y = checkpoint.accumulator;
    append_checkpoint(0x67B9U, 0x67BAU, 90U, checkpoint);
    checkpoint.accumulator = gate.appearance;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x67BAU, 0x67BDU, 105U, checkpoint);
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x67BDU},
        .logical_address = static_cast<std::uint16_t>(0x3D00U + gate.slot),
        .value = gate.appearance,
        .master_cycle_offset = 120U,
    });
    append_checkpoint(0x67BDU, 0x67C0U, 120U, checkpoint);
    checkpoint.accumulator = gate.collision_state;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x67C0U, 0x67C3U, 135U, checkpoint);
    checkpoint.program_counter = 0x67E5U;
    replacement.return_instruction = {.physical_bank = 0x03U, .logical_pc = 0x67E5U};
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = 147U;
    append_checkpoint(0x67C3U, 0x67E5U, 147U, checkpoint);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_three_blobs_first_movement_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x03U, .logical_pc = 0x66A3U};
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_three_blobs_first_movement_prefix prefix{};
    if (!dc_classify_three_blobs_first_movement_prefix(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(), entry.registers.x, &prefix)) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    auto after_timer = entry.registers;
    after_timer.status = set_nz(after_timer.status, prefix.decremented_timer);
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
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
    };

    if (prefix.path == DC_THREE_BLOBS_FIRST_MOVEMENT_PREFIX_TIMER_EXPIRY) {
        RoutineReplacement replacement{};
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x66A3U},
            .logical_address = static_cast<std::uint16_t>(0x3A00U + prefix.slot),
            .value = 0x00U,
            .master_cycle_offset = 21U,
        });
        append_checkpoint(replacement, 0x66A3U, 0x66A6U, 21U, after_timer);
        append_checkpoint(replacement, 0x66A6U, 0x66A8U, 30U, after_timer);

        auto checkpoint = after_timer;
        checkpoint.accumulator = 0x04U;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x66A8U, 0x66AAU, 36U, checkpoint);
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x66AAU},
            .logical_address = static_cast<std::uint16_t>(0x3A00U + prefix.slot),
            .value = 0x04U,
            .master_cycle_offset = 48U,
        });
        append_checkpoint(replacement, 0x66AAU, 0x66ADU, 48U, checkpoint);

        checkpoint.accumulator = static_cast<std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(work_ram.data())[0x1980U + prefix.slot]);
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x66ADU, 0x66B0U, 63U, checkpoint);
        checkpoint.accumulator = static_cast<std::uint8_t>(checkpoint.accumulator ^ 0x01U);
        checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(replacement, 0x66B0U, 0x66B2U, 69U, checkpoint);
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x66B2U},
            .logical_address = static_cast<std::uint16_t>(0x3980U + prefix.slot),
            .value = checkpoint.accumulator,
            .master_cycle_offset = 84U,
        });
        checkpoint.program_counter = 0x66B5U;
        append_checkpoint(replacement, 0x66B2U, 0x66B5U, 84U, checkpoint);
        replacement.return_instruction = {.physical_bank = 0x03U, .logical_pc = 0x66B5U};
        replacement.output_registers = checkpoint;
        replacement.body_master_cycles = 84U;
        return replacement;
    }

    auto after_x = after_timer;
    after_x.status = set_nz(after_x.status, prefix.decremented_x);
    if (prefix.path == DC_THREE_BLOBS_FIRST_MOVEMENT_PREFIX_X_EXPIRY) {
        after_x.program_counter = 0x66E2U;
        RoutineReplacement replacement{
            .return_instruction = {.physical_bank = 0x03U, .logical_pc = 0x66E2U},
            .output_registers = after_x,
            .body_master_cycles = 66U,
        };
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x66A3U},
            .logical_address = static_cast<std::uint16_t>(0x3A00U + prefix.slot),
            .value = prefix.decremented_timer,
            .master_cycle_offset = 21U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x66B5U},
            .logical_address = static_cast<std::uint16_t>(0x3B00U + prefix.slot),
            .value = 0x00U,
            .master_cycle_offset = 54U,
        });
        append_checkpoint(replacement, 0x66A3U, 0x66A6U, 21U, after_timer);
        append_checkpoint(replacement, 0x66A6U, 0x66B5U, 30U, after_timer);
        append_checkpoint(replacement, 0x66B5U, 0x66B8U, 54U, after_x);
        append_checkpoint(replacement, 0x66B8U, 0x66E2U, 66U, after_x);
        return replacement;
    }
    auto handoff = after_x;
    handoff.status = set_nz(handoff.status, prefix.decremented_y);
    if (prefix.path == DC_THREE_BLOBS_FIRST_MOVEMENT_PREFIX_Y_EXPIRY) {
        handoff.program_counter = 0x66BFU;
        RoutineReplacement replacement{
            .return_instruction = {.physical_bank = 0x03U, .logical_pc = 0x66BFU},
            .output_registers = handoff,
            .body_master_cycles = 87U,
        };
        replacement.work_ram_writes = {
            {
                .instruction = {0x03U, 0x66A3U},
                .logical_address = static_cast<std::uint16_t>(0x3A00U + prefix.slot),
                .value = prefix.decremented_timer,
                .master_cycle_offset = 21U,
            },
            {
                .instruction = {0x03U, 0x66B5U},
                .logical_address = static_cast<std::uint16_t>(0x3B00U + prefix.slot),
                .value = prefix.decremented_x,
                .master_cycle_offset = 54U,
            },
            {
                .instruction = {0x03U, 0x66BAU},
                .logical_address = static_cast<std::uint16_t>(0x3B80U + prefix.slot),
                .value = 0x00U,
                .master_cycle_offset = 81U,
            },
        };
        append_checkpoint(replacement, 0x66A3U, 0x66A6U, 21U, after_timer);
        append_checkpoint(replacement, 0x66A6U, 0x66B5U, 30U, after_timer);
        append_checkpoint(replacement, 0x66B5U, 0x66B8U, 54U, after_x);
        append_checkpoint(replacement, 0x66B8U, 0x66BAU, 63U, after_x);
        append_checkpoint(replacement, 0x66BAU, 0x66BDU, 81U, handoff);
        append_checkpoint(replacement, 0x66BDU, 0x66BFU, 87U, handoff);
        return replacement;
    }
    handoff.program_counter = 0x66DEU;
    RoutineReplacement replacement{
        .return_instruction = {.physical_bank = 0x03U, .logical_pc = 0x66DEU},
        .output_registers = handoff,
        .body_master_cycles = 93U,
    };
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x66A3U},
        .logical_address = static_cast<std::uint16_t>(0x3A00U + prefix.slot),
        .value = prefix.decremented_timer,
        .master_cycle_offset = 21U,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x66B5U},
        .logical_address = static_cast<std::uint16_t>(0x3B00U + prefix.slot),
        .value = prefix.decremented_x,
        .master_cycle_offset = 54U,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x66BAU},
        .logical_address = static_cast<std::uint16_t>(0x3B80U + prefix.slot),
        .value = prefix.decremented_y,
        .master_cycle_offset = 81U,
    });
    append_checkpoint(replacement, 0x66A3U, 0x66A6U, 21U, after_timer);
    append_checkpoint(replacement, 0x66A6U, 0x66B5U, 33U, after_timer);
    append_checkpoint(replacement, 0x66B5U, 0x66B8U, 54U, after_x);
    append_checkpoint(replacement, 0x66B8U, 0x66BAU, 60U, after_x);
    append_checkpoint(replacement, 0x66BAU, 0x66BDU, 81U, handoff);
    append_checkpoint(replacement, 0x66BDU, 0x66DEU, 93U, handoff);
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
