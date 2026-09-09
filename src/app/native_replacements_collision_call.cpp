// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_replacements_internal.hpp"
#include <algorithm>

namespace devils_crush::app::detail {
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_shared_collision_table_call(
    const emulation::RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    using emulation::RoutineKey;
    constexpr RoutineKey kLoad{.physical_bank = 0x02U, .logical_pc = 0x441EU};
    constexpr RoutineKey kRoutine{.physical_bank = 0x02U, .logical_pc = 0x4421U};
    if ((entry.routine != kLoad && entry.routine != kRoutine)
        || entry.registers.program_counter != entry.routine.logical_pc
        || entry.registers.mpr[1] != 0xF8U || entry.registers.mpr[2] != 0x02U
        || (entry.registers.status & 0x20U) != 0U || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }

    auto r = entry.registers;
    emulation::RoutineReplacement result{};
    const bool load = entry.routine == kLoad;
    const std::uint32_t cycles = load ? 15U : 21U;
    if (load) {
        // LDA $3C80,X uses the live actor slot and changes only A, N and Z.
        r.accumulator = std::to_integer<std::uint8_t>(work_ram[0x1C80U + r.x]);
        r.status = static_cast<std::uint8_t>((r.status & 0x7DU)
            | (r.accumulator == 0U ? 2U : r.accumulator & 0x80U));
        r.program_counter = 0x4421U;
    } else {
        // The core pushes both JSR bytes before charging instruction cycles.
        // Keep this stack transaction atomic near timer and device events.
        if (std::min(entry.master_cycles_until_user_event,
                     entry.master_cycles_until_timer_interrupt) <= cycles) {
            return std::nullopt;
        }
        for (const auto value : {std::uint8_t{0x44U}, std::uint8_t{0x23U}}) {
            result.work_ram_writes.push_back({
                .instruction = kRoutine,
                .logical_address = static_cast<std::uint16_t>(0x2100U + r.stack_pointer),
                .value = value,
                .master_cycle_offset = 0U,
            });
            r.stack_pointer = static_cast<std::uint8_t>(r.stack_pointer - 1U);
        }
        r.program_counter = 0x6966U;
    }
    result.return_instruction = {r.mpr[r.program_counter >> 13U], r.program_counter};
    result.output_registers = r;
    result.body_master_cycles = cycles;
    result.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = cycles - 3U,
        .master_cycle_offset = cycles,
        .instruction = entry.routine,
        .registers = r,
    });
    return result;
}
}
