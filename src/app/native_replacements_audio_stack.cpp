// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_replacements_internal.hpp"

namespace devils_crush::app::detail {
using namespace emulation;
std::optional<RoutineReplacement> replace_audio_stack_wrapper(
    const RoutineEntry& entry, const std::span<const std::byte> ram)
{
    if ((entry.routine != RoutineKey{0x45, 0x4B7B}
            && entry.routine != RoutineKey{0x45, 0x4B82})
        || ram.size() < 0x2000U || entry.registers.mpr[1] != 0xF8U
        || entry.registers.mpr[2] != 0x45U || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }
    RoutineReplacement result{};
    auto r = entry.registers;
    std::uint32_t cycle = 0;
    const auto step = [&](std::uint16_t pc, std::uint16_t next, std::uint32_t cost) {
        cycle += cost; r.program_counter = next;
        result.checkpoints.push_back({.interrupt_sample_master_cycle_offset = cycle - 3U,
            .master_cycle_offset = cycle, .instruction = {0x45, pc}, .registers = r});
    };
    const auto push = [&](std::uint16_t pc, std::uint8_t value, std::uint32_t at) {
        result.work_ram_writes.push_back({.instruction = {0x45, pc},
            .logical_address = static_cast<std::uint16_t>(0x2100U + r.stack_pointer),
            .value = value, .master_cycle_offset = at});
        --r.stack_pointer;
    };
    const auto pop = [&]() {
        ++r.stack_pointer;
        return std::to_integer<std::uint8_t>(ram[0x100U + r.stack_pointer]);
    };
    const auto nz = [&](std::uint8_t v) {
        r.status = static_cast<std::uint8_t>((r.status & ~0x82U) | (v == 0U ? 2U : v & 0x80U));
    };
    if (entry.routine.logical_pc == 0x4B7B) {
        push(0x4B7B, static_cast<std::uint8_t>(r.status | 0x10U), 9);
        step(0x4B7B, 0x4B7C, 9);
        push(0x4B7C, r.x, 18); step(0x4B7C, 0x4B7D, 9);
        push(0x4B7D, r.y, 27); step(0x4B7D, 0x4B7E, 9);
        const bool old_i_clear = (r.status & 4U) == 0U;
        r.status |= 4U; step(0x4B7E, 0x4B7F, 6);
        result.checkpoints.back().samples_interrupt_flag_clear = old_i_clear;
        // JSR pushes PC+2 at instruction start; the child's stack remains live.
        push(0x4B7F, 0x4B, cycle); push(0x4B7F, 0x81, cycle);
        step(0x4B7F, 0x4BCC, 21);
    } else {
        r.y = pop(); nz(r.y); step(0x4B82, 0x4B83, 12);
        r.x = pop(); nz(r.x); step(0x4B83, 0x4B84, 12);
        // PLP samples the old interrupt mask before restoring P. Generic
        // checkpoints cannot describe set-to-clear sampling. Leave PLP in
        // the interpreter; the fixed native PLP capability currently
        // admits only the VBlank epilogues, not this audio address.
        // RTS at $4B85 is independently admitted using the restored live P.
    }
    result.return_instruction = {0x45, r.program_counter};
    result.output_registers = r; result.body_master_cycles = cycle;
    return result;
}
} // namespace devils_crush::app::detail
