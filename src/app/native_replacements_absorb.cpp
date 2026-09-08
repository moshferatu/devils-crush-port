// SPDX-License-Identifier: GPL-3.0-or-later

#include "native_replacements_internal.hpp"

#include <algorithm>
#include <cstdint>

namespace devils_crush::app::detail {

using emulation::CpuRegisters;
using emulation::RoutineEntry;
using emulation::RoutineKey;
using emulation::RoutineReplacement;

std::optional<RoutineReplacement> replace_bit_branch(
    const RoutineEntry& entry, const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kCollision{.physical_bank = 0x01U, .logical_pc = 0xCB68U};
    constexpr RoutineKey kTileUpload{.physical_bank = 0x00U, .logical_pc = 0xFA67U};
    if ((entry.routine != kCollision && entry.routine != kTileUpload)
        || entry.registers.program_counter != entry.routine.logical_pc
        || entry.registers.mpr[entry.routine.logical_pc >> 13U] != entry.routine.physical_bank
        || entry.registers.mpr[1] != 0xF8U || work_ram.size() < 0x2000U
        || (entry.registers.status & 0x20U) != 0U) {
        return std::nullopt;
    }
    const bool collision = entry.routine == kCollision;
    const auto value = std::to_integer<std::uint8_t>(work_ram[collision ? 0x77U : 0x26U]);
    const bool taken = collision ? (value & 0x02U) != 0U : (value & 0x01U) == 0U;
    const std::uint32_t cycles = taken ? 24U : 18U;
    if (std::min(entry.master_cycles_until_user_event,
                 entry.master_cycles_until_timer_interrupt) <= cycles) {
        return std::nullopt;
    }
    // LD_ZP samples at 9 and completes at 12; JR then samples at 15/21
    // and completes at 18/24. With no intervening event, RAM or mask change,
    // the final sample is equivalent. Do not expose an interruptible boundary
    // between the two halves of this single opcode. Near events, use the core.
    RoutineReplacement result{};
    auto r = entry.registers;
    r.program_counter = collision ? (taken ? 0xCB93U : 0xCB6BU)
                                 : (taken ? 0xFA6EU : 0xFA6AU);
    result.return_instruction = {entry.routine.physical_bank, r.program_counter};
    result.output_registers = r;
    result.body_master_cycles = cycles;
    result.checkpoints.push_back({.interrupt_sample_master_cycle_offset = cycles - 3U,
        .master_cycle_offset = cycles, .instruction = entry.routine, .registers = r});
    return result;
}

std::optional<RoutineReplacement> replace_unconditional_branch(const RoutineEntry& entry)
{
    constexpr RoutineKey kEpilogue{.physical_bank = 0x00U, .logical_pc = 0xFC03U};
    constexpr RoutineKey kSoundChain{.physical_bank = 0x00U, .logical_pc = 0xFC89U};
    constexpr RoutineKey kStreamLoop{.physical_bank = 0x00U, .logical_pc = 0xE479U};
    if ((entry.routine != kEpilogue && entry.routine != kSoundChain && entry.routine != kStreamLoop)
        || entry.registers.program_counter != entry.routine.logical_pc
        || entry.registers.mpr[7] != 0x00U || (entry.registers.status & 0x20U) != 0U
        || std::min(entry.master_cycles_until_user_event,
                    entry.master_cycles_until_timer_interrupt) <= 12U) {
        return std::nullopt;
    }
    // Keep each JMP/BRA separate: the core samples interrupts after
    // nine master cycles, then completes the instruction three cycles later.
    RoutineReplacement result{};
    auto r = entry.registers;
    r.program_counter = entry.routine == kStreamLoop ? 0xE438U
        : entry.routine == kSoundChain ? 0xFC03U : 0xE003U;
    result.return_instruction = {0x00U, r.program_counter};
    result.output_registers = r;
    result.body_master_cycles = 12U;
    result.checkpoints.push_back({.interrupt_sample_master_cycle_offset = 9U,
        .master_cycle_offset = 12U, .instruction = entry.routine, .registers = r});
    return result;
}

std::optional<RoutineReplacement> replace_coroutine_call(
    const RoutineEntry& entry, const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kDelayCall{.physical_bank = 0x01U, .logical_pc = 0xC693U};
    constexpr RoutineKey kSchedulerCall{.physical_bank = 0x01U, .logical_pc = 0xC696U};
    constexpr RoutineKey kSecondDelayCall{.physical_bank = 0x01U, .logical_pc = 0xC69BU};
    constexpr RoutineKey kToggleCall{.physical_bank = 0x01U, .logical_pc = 0xC6A6U};
    constexpr RoutineKey kResetCall{.physical_bank = 0x01U, .logical_pc = 0xC6BFU};
    if ((entry.routine != kDelayCall && entry.routine != kSchedulerCall
            && entry.routine != kSecondDelayCall && entry.routine != kToggleCall
            && entry.routine != kResetCall)
        || entry.registers.program_counter != entry.routine.logical_pc
        || entry.registers.mpr[1] != 0xF8U || entry.registers.mpr[6] != 0x01U
        || (entry.registers.status & 0x20U) != 0U || work_ram.size() < 0x2000U
        || !absorbed_return_fits_atomically(entry, 0U)) {
        return std::nullopt;
    }
    // The child consumes the real JSR frame to choose the continuation.
    // Preserve hardware write order and timing; leave the child as a new entry.
    RoutineReplacement result{};
    auto r = entry.registers;
    const auto saved = static_cast<std::uint16_t>(entry.routine.logical_pc + 2U);
    for (const auto value : {static_cast<std::uint8_t>(saved >> 8U),
                            static_cast<std::uint8_t>(saved)}) {
        result.work_ram_writes.push_back({.instruction = entry.routine,
            .logical_address = static_cast<std::uint16_t>(0x2100U + r.stack_pointer),
            .value = value, .master_cycle_offset = 0U});
        r.stack_pointer = static_cast<std::uint8_t>(r.stack_pointer - 1U);
    }
    r.program_counter = (entry.routine == kDelayCall || entry.routine == kSecondDelayCall)
        ? 0xC5EAU : 0xC5C9U;
    result.return_instruction = {0x01U, r.program_counter};
    result.output_registers = r;
    result.body_master_cycles = 21U;
    result.checkpoints.push_back({.interrupt_sample_master_cycle_offset = 18U,
        .master_cycle_offset = 21U, .instruction = entry.routine, .registers = r});
    return result;
}

std::optional<RoutineReplacement> replace_coroutine_delay(
    const RoutineEntry& entry, const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x01U, .logical_pc = 0xC5EAU};
    if (entry.routine != kRoutine || entry.registers.program_counter != 0xC5EAU
        || entry.registers.mpr[1] != 0xF8U || entry.registers.mpr[6] != 0x01U
        || entry.registers.x >= 0x80U || (entry.registers.status & 0x20U) != 0U
        || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    // STA $3D80,X; consume the JSR frame; install its successor in the
    // actor's continuation fields; JMP ($202F). This is not an RTS.
    RoutineReplacement result{};
    auto r = entry.registers;
    std::uint32_t cycles = 0U;
    const auto nz = [&r](const std::uint8_t value) {
        r.status = static_cast<std::uint8_t>((r.status & ~0x82U)
            | (value == 0U ? 0x02U : value & 0x80U));
    };
    const auto step = [&](const std::uint16_t pc, const std::uint16_t next,
                          const std::uint32_t cost) {
        cycles += cost;
        r.program_counter = next;
        result.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = cycles - 3U,
            .master_cycle_offset = cycles, .instruction = {0x01U, pc}, .registers = r});
    };
    const auto write = [&](const std::uint16_t pc, const std::uint16_t address,
                           const std::uint8_t value, const std::uint32_t cost) {
        result.work_ram_writes.push_back({.instruction = {0x01U, pc},
            .logical_address = address, .value = value, .master_cycle_offset = cycles + cost});
    };
    write(0xC5EAU, static_cast<std::uint16_t>(0x3D80U + r.x), r.accumulator, 15U);
    step(0xC5EAU, 0xC5EDU, 15U);
    r.stack_pointer = static_cast<std::uint8_t>(r.stack_pointer + 1U);
    auto low = std::to_integer<std::uint8_t>(work_ram[0x100U + r.stack_pointer]);
    r.accumulator = low; nz(low);
    step(0xC5EDU, 0xC5EEU, 12U);
    write(0xC5EEU, 0x202FU, low, 12U); step(0xC5EEU, 0xC5F0U, 12U);
    r.stack_pointer = static_cast<std::uint8_t>(r.stack_pointer + 1U);
    auto high = std::to_integer<std::uint8_t>(work_ram[0x100U + r.stack_pointer]);
    r.accumulator = high; nz(high);
    step(0xC5F0U, 0xC5F1U, 12U);
    write(0xC5F1U, 0x2030U, high, 12U); step(0xC5F1U, 0xC5F3U, 12U);
    low = static_cast<std::uint8_t>(low + 1U); nz(low);
    write(0xC5F3U, 0x202FU, low, 18U); step(0xC5F3U, 0xC5F5U, 18U);
    if (low == 0U) {
        step(0xC5F5U, 0xC5F7U, 6U);
        high = static_cast<std::uint8_t>(high + 1U); nz(high);
        write(0xC5F7U, 0x2030U, high, 18U); step(0xC5F7U, 0xC5F9U, 18U);
    } else {
        step(0xC5F5U, 0xC5F9U, 12U);
    }
    r.accumulator = low; nz(low); step(0xC5F9U, 0xC5FBU, 12U);
    write(0xC5FBU, static_cast<std::uint16_t>(0x3F00U + r.x), low, 15U);
    step(0xC5FBU, 0xC5FEU, 15U);
    r.accumulator = high; nz(high); step(0xC5FEU, 0xC600U, 12U);
    write(0xC600U, static_cast<std::uint16_t>(0x3F80U + r.x), high, 15U);
    step(0xC600U, 0xC603U, 15U);
    const auto target = static_cast<std::uint16_t>((high << 8U) | low);
    step(0xC603U, target, 21U);
    result.return_instruction = {r.mpr[target >> 13U], target};
    result.output_registers = r;
    result.body_master_cycles = cycles;
    return result;
}

std::optional<RoutineReplacement> replace_upper_scheduler_return(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x08U, .logical_pc = 0x5976U};
    constexpr RoutineKey kRunInputReturn{.physical_bank = 0x01U, .logical_pc = 0xC4C8U};
    constexpr RoutineKey kSelectionReturn{.physical_bank = 0x08U, .logical_pc = 0x5A69U};
    constexpr RoutineKey kStreamReturn{.physical_bank = 0x00U, .logical_pc = 0xE47CU};
    constexpr RoutineKey kCountdownReturn{.physical_bank = 0x02U, .logical_pc = 0x4F75U};
    if ((entry.routine != kRoutine && entry.routine != kRunInputReturn
            && entry.routine != kSelectionReturn && entry.routine != kStreamReturn
            && entry.routine != kCountdownReturn)
        || entry.registers.program_counter != entry.routine.logical_pc
        || entry.registers.mpr[1] != 0xF8U
        || entry.registers.mpr[entry.routine.logical_pc >> 13U] != entry.routine.physical_bank
        || work_ram.size() < 0x2000U
        || !absorbed_return_fits_atomically(entry, 0U)) {
        return std::nullopt;
    }
    // Enter on the RTS itself: the scheduler may have installed any caller
    // frame. Read that live frame without assuming which parent opened it.
    // Keep this single instruction atomic and strictly before both horizons.
    RoutineReplacement replacement{};
    absorb_return(replacement, work_ram, entry.registers.stack_pointer,
        entry.routine, 0U, entry.registers);
    return replacement;
}

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
