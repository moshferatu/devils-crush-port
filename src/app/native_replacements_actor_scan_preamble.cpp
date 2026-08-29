// SPDX-License-Identifier: GPL-3.0-or-later

#include "native_replacements_internal.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace devils_crush::app::detail {
namespace {

using emulation::CpuRegisters;
using emulation::RoutineEntry;
using emulation::RoutineKey;
using emulation::RoutineReplacement;

// 01:C4DE-01:C50E, the actor scan's preamble: settle this frame's actor enable
// mask, reset the three cursors the scan allocates its list entries from, clear
// the scan index, and fall into the scan itself.
//
//   C4DE  LDA $2D35 / BNE $C4FC    frozen, and nothing is enabled at all
//   C4E3  LDX $2251 / LDY <$13,X   otherwise the current side's own mask
//   C4E8  LDA $2254 / BEQ $C4F1    ... or the one staged in <$B7 instead
//   C4F1  LDA $2CC7 / BNE $C4FC    ... and $2CC7 or bit 3 of <$53 clears it
//   C4F6  LDA <$53 / AND #$08 / BEQ $C4FD
//   C4FC  CLY
//   C4FD  STY $2CEE                the mask 01:D466 tests each actor against
//   C500  STZ <$56
//   C502  STZ $2CBA                ascending cursor into the list at $2BBA
//   C505  STZ $2CBC                ascending cursor into the list at $2C3A
//   C508  LDA #$7F / STA $2CBB     descending cursor into $2BBA, from the top
//   C50D  CLX / STX <$4D           the scan index 01:C510 immediately reads
//
// `$2CEE` is a mask rather than an index: 01:D466 loads it and ANDs it against
// each actor's `$3B80,X` attribute byte, so clearing it turns every actor off
// for the frame. The two cursors into `$2BBA` walk one list from both ends,
// which is why one starts at zero and the other at $7F.
//
// One entry point, twenty instructions, and no call of any kind -- no `JSR`,
// no `JMP`, not one ROM read. Every operand is in work RAM, and every branch
// target but one is inside the range.
//
// The host restores cabinet flipper input immediately after `STY $2CEE`. The
// C4DE entry therefore hands back at 01:C500, where the host-aware C500 entry
// restores the input and then uses this same walker for the cursor suffix.
// The suffix falls into the registered actor scan at 01:C510. This keeps the
// handoff at its original instruction boundary without leaving its six simple
// instructions in the interpreter.
//
// 3,492 arrivals on the recorded route: 2,649 from 01:C157, immediately after
// the frame-sync wait at 01:C154, and 843 from five `JSR`s in bank 08 plus a
// single arrival from 00:F92C at start-up. The bank-01 jump table's row for it,
// 01:C00F `JMP $C4DE`, never runs.
constexpr std::uint8_t kBank = 0x01U;

constexpr std::uint8_t kZero = 0x02U;
constexpr std::uint8_t kDecimal = 0x08U;
constexpr std::uint8_t kMemory = 0x20U;
constexpr std::uint8_t kNegative = 0x80U;

// Work-RAM window. Zero page is $2000-$20FF, which the core hardwires rather
// than reading out of MPR1.
constexpr std::uint16_t kWorkRamBase = 0x2000U;
constexpr std::uint16_t kWorkRamEnd = 0x4000U;

// Master cycles per instruction: three times the core's ADDCYC(n) plus the
// LastCycle it always charges.
constexpr std::uint32_t kImplied = 6U;
constexpr std::uint32_t kImmediate = 6U;
constexpr std::uint32_t kZeroPage = 12U;
constexpr std::uint32_t kZeroPageIndexed = 12U;
constexpr std::uint32_t kAbsolute = 15U;
constexpr std::uint32_t kBranchTaken = 12U;
constexpr std::uint32_t kBranchNotTaken = 6U;

[[nodiscard]] std::uint8_t set_nz(
    const std::uint8_t status,
    const std::uint8_t value)
{
    auto result = static_cast<std::uint8_t>(
        status & static_cast<std::uint8_t>(~(kZero | kNegative)));
    if (value == 0U) {
        result = static_cast<std::uint8_t>(result | kZero);
    }
    return static_cast<std::uint8_t>(
        result | static_cast<std::uint8_t>(value & kNegative));
}

// Where each walk stops.
//
// 01:C500 is the host-visible input hand-off. Its host-aware native replacement
// resumes this walk for the cursor suffix, which returns at the registered
// 01:C510 scan.
//
// 01:C4ED is the arm this route has never taken: `$2254` was zero on all 3,492
// arrivals, so the mask staged in <$B7 has never been the one chosen and
// 01:C4ED and 01:C4EF have not executed once in 3,600 frames. Handing the pair
// back costs nothing measurable and leaves them to the core, which is right
// about them by construction rather than by transcription.
//
// The two branches to 01:C4FC are a different case and are transcribed. Neither
// has been taken either, but 01:C4FC is live -- the fall-through from 01:C4FA
// reaches it 322 times -- so following a branch there lands on an instruction
// this walk already owns, and the only untranscribed thing about the arm is
// which way the flag went.
[[nodiscard]] constexpr bool is_stop_instruction(
    const std::uint16_t logical_pc,
    const bool hand_off_input)
{
    return (hand_off_input && logical_pc == 0xC500U)
        || logical_pc == 0xC4EDU
        || logical_pc == 0xC510U;
}

} // namespace

std::optional<RoutineReplacement> replace_actor_scan_preamble(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kPreambleRoutine{
        .physical_bank = kBank,
        .logical_pc = 0xC4DEU,
    };
    constexpr RoutineKey kCursorRoutine{
        .physical_bank = kBank,
        .logical_pc = 0xC500U,
    };
    if ((entry.routine != kPreambleRoutine
            && entry.routine != kCursorRoutine)
        || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    // The memory-operand T flag would take the left-hand side of the `AND` at
    // 01:C4F8 out of zero page instead of the accumulator, which is the one
    // decision this routine actually makes. Decimal changes nothing here --
    // there is no arithmetic to change -- but it is refused with T so that
    // every walk in this port is admitted under the same conditions.
    if ((entry.registers.status & (kDecimal | kMemory)) != 0U) {
        return std::nullopt;
    }

    std::uint8_t ram[0x2000];
    for (std::size_t index = 0U; index < 0x2000U; ++index) {
        ram[index] = std::to_integer<std::uint8_t>(work_ram[index]);
    }

    RoutineReplacement replacement;
    CpuRegisters registers = entry.registers;
    std::uint32_t cycle = 0U;
    bool overflow = false;

    const auto read_ram =
        [&ram, &overflow](const std::uint16_t address) -> std::uint8_t {
            if (address < kWorkRamBase || address >= kWorkRamEnd) {
                overflow = true;
                return 0U;
            }
            return ram[address - kWorkRamBase];
        };

    const auto zp = [](const std::uint8_t address) {
        return static_cast<std::uint16_t>(kWorkRamBase + address);
    };

    // Zero-page indexing wraps inside the page: the core forms
    // $2000 | ((X + operand) & 0xFF), so an index of $FF walks backwards rather
    // than out of the page.
    const auto zp_indexed = [&registers](const std::uint8_t address) {
        return static_cast<std::uint16_t>(
            kWorkRamBase
            + static_cast<std::uint8_t>(address + registers.x));
    };

    // Stores land on their instruction's final cycle, so a write shares the
    // offset and the instruction of the checkpoint that follows it.
    std::size_t committed_writes = 0U;
    const auto commit_pending_writes =
        [&replacement, &committed_writes](const std::uint32_t at_cycle) {
            while (committed_writes < replacement.work_ram_writes.size()) {
                replacement.work_ram_writes[committed_writes]
                    .master_cycle_offset = at_cycle;
                ++committed_writes;
            }
        };

    // An instruction samples the interrupt one CPU cycle before its own last,
    // which is where the core's LastCycle sits.
    const auto append_checkpoint =
        [&replacement, &registers, &cycle, &commit_pending_writes](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t instruction_cycles) {
            cycle += instruction_cycles;
            commit_pending_writes(cycle);
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = cycle - 3U,
                .master_cycle_offset = cycle,
                .instruction = {kBank, instruction_pc},
                .registers = registers,
            });
        };

    const auto store =
        [&replacement, &ram, &overflow](
            const std::uint16_t instruction_pc,
            const std::uint16_t address,
            const std::uint8_t value) {
            if (address < kWorkRamBase || address >= kWorkRamEnd) {
                overflow = true;
                return;
            }
            ram[address - kWorkRamBase] = value;
            replacement.work_ram_writes.push_back({
                .instruction = {kBank, instruction_pc},
                .logical_address = address,
                .value = value,
                .master_cycle_offset = 0U,
            });
        };

    // A relative branch costs a cycle more when it is taken, and the address it
    // resumes at is the only thing that distinguishes the two arms.
    const auto branch =
        [&append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t taken_pc,
            const std::uint16_t fallthrough_pc,
            const bool condition) {
            const auto next = condition ? taken_pc : fallthrough_pc;
            append_checkpoint(
                instruction_pc, next,
                condition ? kBranchTaken : kBranchNotTaken);
            return next;
        };

    const auto load_accumulator = [&registers](const std::uint8_t value) {
        registers.accumulator = value;
        registers.status = set_nz(registers.status, value);
    };
    const auto load_x = [&registers](const std::uint8_t value) {
        registers.x = value;
        registers.status = set_nz(registers.status, value);
    };
    const auto load_y = [&registers](const std::uint8_t value) {
        registers.y = value;
        registers.status = set_nz(registers.status, value);
    };
    const auto and_accumulator = [&registers](const std::uint8_t value) {
        registers.accumulator =
            static_cast<std::uint8_t>(registers.accumulator & value);
        registers.status = set_nz(registers.status, registers.accumulator);
    };

    auto pc = entry.routine.logical_pc;
    for (;;) {
        // The core's channels are 512 writes and 2,048 checkpoints. The longest
        // walk here is twenty instructions and six writes, so exceeding either
        // means the walk has gone somewhere it was not written for and the
        // assembly should run.
        if (overflow
            || replacement.checkpoints.size() > 32U
            || replacement.work_ram_writes.size() > 16U) {
            return std::nullopt;
        }

        if (is_stop_instruction(pc, entry.routine == kPreambleRoutine)) {
            registers.program_counter = pc;
            replacement.return_instruction = {kBank, pc};
            replacement.output_registers = registers;
            replacement.body_master_cycles = cycle;
            // Nothing is absorbed. This walk never reaches a return: it ends
            // either on the host-visible input hand-off or on the arm it
            // declines, and the stack is exactly as it was found in both
            // cases.
            return replacement;
        }

        switch (pc) {
        // ---- the enable mask ------------------------------------------------
        case 0xC4DEU:  // LDA $2D35
            load_accumulator(read_ram(0x2D35U));
            append_checkpoint(0xC4DEU, 0xC4E1U, kAbsolute);
            pc = 0xC4E1U;
            break;

        case 0xC4E1U:  // BNE $C4FC -- frozen: no actor is enabled
            pc = branch(
                0xC4E1U, 0xC4FCU, 0xC4E3U,
                (registers.status & kZero) == 0U);
            break;

        case 0xC4E3U:  // LDX $2251 -- the current side
            load_x(read_ram(0x2251U));
            append_checkpoint(0xC4E3U, 0xC4E6U, kAbsolute);
            pc = 0xC4E6U;
            break;

        case 0xC4E6U:  // LDY <$13,X -- its mask
            load_y(read_ram(zp_indexed(0x13U)));
            append_checkpoint(0xC4E6U, 0xC4E8U, kZeroPageIndexed);
            pc = 0xC4E8U;
            break;

        case 0xC4E8U:  // LDA $2254
            load_accumulator(read_ram(0x2254U));
            append_checkpoint(0xC4E8U, 0xC4EBU, kAbsolute);
            pc = 0xC4EBU;
            break;

        case 0xC4EBU:  // BEQ $C4F1 -- clear on every arrival recorded
            pc = branch(
                0xC4EBU, 0xC4F1U, 0xC4EDU,
                (registers.status & kZero) != 0U);
            break;

        case 0xC4F1U:  // LDA $2CC7
            load_accumulator(read_ram(0x2CC7U));
            append_checkpoint(0xC4F1U, 0xC4F4U, kAbsolute);
            pc = 0xC4F4U;
            break;

        case 0xC4F4U:  // BNE $C4FC
            pc = branch(
                0xC4F4U, 0xC4FCU, 0xC4F6U,
                (registers.status & kZero) == 0U);
            break;

        case 0xC4F6U:  // LDA <$53
            load_accumulator(read_ram(zp(0x53U)));
            append_checkpoint(0xC4F6U, 0xC4F8U, kZeroPage);
            pc = 0xC4F8U;
            break;

        case 0xC4F8U:  // AND #$08
            and_accumulator(0x08U);
            append_checkpoint(0xC4F8U, 0xC4FAU, kImmediate);
            pc = 0xC4FAU;
            break;

        case 0xC4FAU:  // BEQ $C4FD -- the one decision this route ever varies
            pc = branch(
                0xC4FAU, 0xC4FDU, 0xC4FCU,
                (registers.status & kZero) != 0U);
            break;

        case 0xC4FCU:  // CLY -- no flags: the core's OP_CLEARR touches none
            registers.y = 0x00U;
            append_checkpoint(0xC4FCU, 0xC4FDU, kImplied);
            pc = 0xC4FDU;
            break;

        case 0xC4FDU:  // STY $2CEE
            store(0xC4FDU, 0x2CEEU, registers.y);
            append_checkpoint(0xC4FDU, 0xC500U, kAbsolute);
            pc = 0xC500U;
            break;

        // ---- the cursors ----------------------------------------------------
        case 0xC500U:  // STZ <$56
            store(0xC500U, zp(0x56U), 0x00U);
            append_checkpoint(0xC500U, 0xC502U, kZeroPage);
            pc = 0xC502U;
            break;

        case 0xC502U:  // STZ $2CBA -- $2BBA from the bottom
            store(0xC502U, 0x2CBAU, 0x00U);
            append_checkpoint(0xC502U, 0xC505U, kAbsolute);
            pc = 0xC505U;
            break;

        case 0xC505U:  // STZ $2CBC -- $2C3A from the bottom
            store(0xC505U, 0x2CBCU, 0x00U);
            append_checkpoint(0xC505U, 0xC508U, kAbsolute);
            pc = 0xC508U;
            break;

        case 0xC508U:  // LDA #$7F
            load_accumulator(0x7FU);
            append_checkpoint(0xC508U, 0xC50AU, kImmediate);
            pc = 0xC50AU;
            break;

        case 0xC50AU:  // STA $2CBB -- $2BBA from the top
            store(0xC50AU, 0x2CBBU, registers.accumulator);
            append_checkpoint(0xC50AU, 0xC50DU, kAbsolute);
            pc = 0xC50DU;
            break;

        // ---- the scan index -------------------------------------------------
        case 0xC50DU:  // CLX -- no flags
            registers.x = 0x00U;
            append_checkpoint(0xC50DU, 0xC50EU, kImplied);
            pc = 0xC50EU;
            break;

        case 0xC50EU:  // STX <$4D, and 01:C510 is the registered scan
            store(0xC50EU, zp(0x4DU), registers.x);
            append_checkpoint(0xC50EU, 0xC510U, kZeroPage);
            pc = 0xC510U;
            break;

        default:
            return std::nullopt;
        }
    }
}

} // namespace devils_crush::app::detail
