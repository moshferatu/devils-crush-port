// SPDX-License-Identifier: GPL-3.0-or-later

#include "native_replacements_internal.hpp"

#include <algorithm>
#include <cstdint>

namespace devils_crush::app::detail {

using emulation::CpuRegisters;
using emulation::RoutineEntry;
using emulation::RoutineKey;
using emulation::RoutineReplacement;

bool absorbed_return_fits_atomically(
    const RoutineEntry& entry,
    const std::uint32_t body_master_cycles)
{
    // The valve declines a body that reaches the horizon, so the absorbed
    // return has to land strictly before it.
    return body_master_cycles + 21U
        < std::min(
               entry.master_cycles_until_user_event,
               entry.master_cycles_until_timer_interrupt);
}

void absorb_return(
    RoutineReplacement& replacement,
    const std::span<const std::byte> work_ram,
    const std::uint8_t entry_stack_pointer,
    const RoutineKey return_instruction,
    const std::uint32_t return_master_cycle_offset,
    const CpuRegisters& at_return)
{
    // The stack is ordinary work RAM at $2100, so an `RTS` is expressible as a
    // transcript: it pops the low byte from $2100 + S + 1 and the high from
    // $2100 + S + 2, and lands on that word plus one, `JSR` having pushed the
    // address of its own last byte. Both indices wrap inside the page.
    //
    // The bytes come from the staged transcript rather than from entry work
    // RAM. Most walks never touch the stack page, but a composition stages its
    // children's `JSR` frames there, and a walk that stored over its own return
    // address would otherwise be a silent wrong answer instead of a loud one.
    const auto staged_byte = [&replacement, work_ram](
                                 const std::uint16_t address) {
        auto value = std::to_integer<std::uint8_t>(
            work_ram[static_cast<std::size_t>(address - 0x2000U)]);
        for (const auto& write : replacement.work_ram_writes) {
            if (write.logical_address == address) {
                value = write.value;
            }
        }
        return value;
    };
    const auto low = staged_byte(static_cast<std::uint16_t>(
        0x2100U + static_cast<std::uint8_t>(entry_stack_pointer + 1U)));
    const auto high = staged_byte(static_cast<std::uint16_t>(
        0x2100U + static_cast<std::uint8_t>(entry_stack_pointer + 2U)));
    const auto return_pc = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(high) << 8U)
        + static_cast<std::uint16_t>(low) + 1U);

    // Declared before it is taken. Absorbing a return deletes the instruction
    // boundary the routine-call observer completes a call on, so the transcript
    // has to say where that boundary would have been and what state it would
    // have carried -- with the stack pointer the call was entered on, which is
    // the half the observer matches. The host validates all of that.
    replacement.absorbed_returns.push_back({
        .instruction = return_instruction,
        .master_cycle_offset = return_master_cycle_offset,
        .registers = at_return,
    });

    auto output = at_return;
    output.program_counter = return_pc;
    output.stack_pointer =
        static_cast<std::uint8_t>(entry_stack_pointer + 2U);

    // The caller need not be in the routine's own segment, so the bank is
    // resolved through the MPRs the transcript is declaring. The host rejects a
    // resume whose bank disagrees with them.
    replacement.return_instruction = {
        output.mpr[return_pc >> 13U],
        return_pc,
    };
    replacement.output_registers = output;
    replacement.body_master_cycles =
        static_cast<std::uint32_t>(return_master_cycle_offset + 21U);

    // An `RTS` only reads, so this adds no write and moves no cycle: the same
    // twenty-one either side of the seam, counted on the other side of it. A
    // transcript with no checkpoints is atomic and wants none here either.
    if (!replacement.checkpoints.empty()) {
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset =
                static_cast<std::uint32_t>(return_master_cycle_offset + 18U),
            .master_cycle_offset = replacement.body_master_cycles,
            .instruction = return_instruction,
            .registers = output,
        });
    }
}

}  // namespace devils_crush::app::detail
