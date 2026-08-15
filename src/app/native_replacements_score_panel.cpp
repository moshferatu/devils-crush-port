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

// 09:7237-09:7268, the score panel's redraw poll: four gates in front of the
// panel body, and on this route it declines to redraw 2,616 times out of 2,649.
//
//   7237  LDA $2DBB / BEQ $723F     a redraw already in progress?
//   723C  JMP $726B                 ... then go straight to the body
//   723F  LDA $2D53 / BNE $725E     the session handshake, skip the countdown
//   7244  LDA $2D70 / BEQ $725E     the hold-off countdown, zero means ready
//   7249  ... DEC $2D70 ...         otherwise tick it down and leave
//   725E  LDA $2CC0 / BIT #$01      the panel dirty bit
//   7263  BNE $7266                 set: consume it and redraw
//   7265  RTS                       clear: nothing to do
//   7266  AND #$FE / STA $2CC0      consume the request
//   726B  ... the redraw itself
//
// **The addresses name it before the control flow does.** `$2CC0` is the flag
// `00:F55C`'s score accumulator sets, and bit 7 of it is already the ending
// request `01:C190` tests; bit 0 is the panel's. `ROUTINE_MAP.md` records the
// far side of this poll under the session-and-scoring closure route -- "`09:7266`,
// `09:72FD`: consumes the dirty bit and redraws the score and remaining-ball
// digits", with `01:C812` marking the panel dirty at frame 3,287. So this is
// the guard in front of a routine the port had already read, and what it mostly
// does is decide that the panel has not changed.
//
// **It is the largest thing left in bank 09 and the profile's grouping hid
// that.** `09:7237`-`09:7265` is 327,924 master cycles, forty-two per cent of
// the bank's remainder, in ten instructions. The candidate analyzer groups
// contiguous executed addresses, and this routine branches over the untaken
// countdown at 7249-725B, so it was ranked as two separate runs -- `09:71EA`-`7247`
// and `09:725E`-`72EA` -- neither of which is a routine. Control flow joins what
// address adjacency splits.
//
// Every gate is decided from work RAM, so the walk is state-generic and the
// three arms this route never takes are simply where it stops:
//
//   * `$2DBB` non-zero stops in front of the `JMP` at 723C.
//   * `$2D70` non-zero stops in front of the `DEC` at 7249.
//   * the dirty bit set walks 7266 and 7268 -- consuming the request is two
//     work RAM instructions -- and stops in front of the body at 726B. Thirty-
//     three calls on this route, and the body is the part that drives the VDC.
//
// The fourth arm, `$2D53` non-zero at 7242, needs nothing: it branches to 725E,
// which the walk models anyway for the 2,649 calls that arrive there.
//
// `BIT #$01` is the one instruction here worth checking against the core rather
// than the datasheet. A 65C02's immediate-mode `BIT` touches only `Z`; this
// core's `BIT` macro is shared across every addressing mode and clears `N` and
// `V` from the operand's top two bits, so `BIT #$01` clears both. Writing the
// documented behaviour instead would leave the transcript's flags disagreeing
// with the core's at the resume point, which is an abandonment rather than a
// wrong answer -- but an abandonment on 2,649 calls.
//
// The return is absorbed. The `RTS` at 7265 reads only the stack, and the
// caller is the bank-call wrapper at 00:E252 with MPR3 already holding this
// bank, which is what admission at `09:7237` states.
constexpr std::uint8_t kBank = 0x09U;

constexpr std::uint8_t kZero = 0x02U;
constexpr std::uint8_t kMemory = 0x20U;
constexpr std::uint8_t kOverflow = 0x40U;
constexpr std::uint8_t kNegative = 0x80U;

constexpr std::uint16_t kWorkRamBase = 0x2000U;

// The four gates, in the order the routine reads them. All four are absolute
// addresses written into the instructions, so unlike a walk that builds a
// pointer there is nothing here that can leave work RAM.
constexpr std::uint16_t kRedrawInProgress = 0x2DBBU;
constexpr std::uint16_t kSessionHandshake = 0x2D53U;
constexpr std::uint16_t kHoldOffCountdown = 0x2D70U;
constexpr std::uint16_t kPanelFlags = 0x2CC0U;
constexpr std::uint8_t kPanelDirtyBit = 0x01U;

// Master cycles, three per CPU cycle, confirmed against the profile's own
// per-row totals: 2,649 executions of `LDA $2DBB` is 40,095, of the taken
// `BEQ` at 723A is 32,076, of the untaken `BNE` at 7242 is 15,990, and 2,616
// of `RTS` is 55,464.
constexpr std::uint32_t kImmediate = 6U;
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

}  // namespace

std::optional<RoutineReplacement> replace_score_panel_poll(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = kBank,
        .logical_pc = 0x7237U,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    // The memory-operand T flag would take the left-hand side of the `AND` at
    // 7266 out of zero page instead of the accumulator. Decimal reaches nothing
    // here -- there is no arithmetic in the routine at all.
    if ((entry.registers.status & kMemory) != 0U) {
        return std::nullopt;
    }

    RoutineReplacement replacement;
    replacement.work_ram_writes.reserve(1U);
    replacement.checkpoints.reserve(11U);

    CpuRegisters registers = entry.registers;
    std::uint32_t cycle = 0U;

    const auto read_ram = [work_ram](const std::uint16_t address) {
        return std::to_integer<std::uint8_t>(
            work_ram[static_cast<std::size_t>(address - kWorkRamBase)]);
    };

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
    // which is where the core's LastCycle sits -- and a branch calls it on
    // both arms, so a taken branch samples at 9 and an untaken one at 3.
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

    const auto load_accumulator = [&registers](const std::uint8_t value) {
        registers.accumulator = value;
        registers.status = set_nz(registers.status, value);
    };

    // A transcript that ends in front of an instruction rather than past a
    // return: the core resumes inside this bank, on the arm the gates chose.
    const auto stop_before =
        [&replacement, &registers, &cycle](const std::uint16_t resume_pc) {
            registers.program_counter = resume_pc;
            replacement.return_instruction = {kBank, resume_pc};
            replacement.output_registers = registers;
            replacement.body_master_cycles = cycle;
            return replacement;
        };

    // ---- 7237  a redraw already in progress? -----------------------------
    load_accumulator(read_ram(kRedrawInProgress));
    append_checkpoint(0x7237U, 0x723AU, kAbsolute);

    if ((registers.status & kZero) == 0U) {
        // The body is already running somewhere above this call; 723C jumps
        // straight into it. Never taken on the census route, so the walk stops
        // in front of the `JMP` rather than modelling an arm nothing exercises.
        append_checkpoint(0x723AU, 0x723CU, kBranchNotTaken);
        return stop_before(0x723CU);
    }
    append_checkpoint(0x723AU, 0x723FU, kBranchTaken);

    // ---- 723F  the session handshake -------------------------------------
    // Non-zero skips the hold-off countdown entirely and goes to the dirty-bit
    // test, which is where this walk is going anyway.
    load_accumulator(read_ram(kSessionHandshake));
    append_checkpoint(0x723FU, 0x7242U, kAbsolute);

    if ((registers.status & kZero) == 0U) {
        append_checkpoint(0x7242U, 0x725EU, kBranchTaken);
    } else {
        append_checkpoint(0x7242U, 0x7244U, kBranchNotTaken);

        // ---- 7244  the hold-off countdown --------------------------------
        load_accumulator(read_ram(kHoldOffCountdown));
        append_checkpoint(0x7244U, 0x7247U, kAbsolute);

        if ((registers.status & kZero) == 0U) {
            // Still counting down. 7249 decrements it and takes a different
            // exit; the census route never gets here.
            append_checkpoint(0x7247U, 0x7249U, kBranchNotTaken);
            return stop_before(0x7249U);
        }
        append_checkpoint(0x7247U, 0x725EU, kBranchTaken);
    }

    // ---- 725E  the panel dirty bit ---------------------------------------
    load_accumulator(read_ram(kPanelFlags));
    append_checkpoint(0x725EU, 0x7261U, kAbsolute);

    // `BIT #$01` as this core implements it: Z from the masked accumulator, and
    // N and V taken from the operand, which for $01 clears both.
    registers.status = static_cast<std::uint8_t>(
        registers.status
        & static_cast<std::uint8_t>(~(kZero | kOverflow | kNegative)));
    if ((registers.accumulator & kPanelDirtyBit) == 0U) {
        registers.status = static_cast<std::uint8_t>(registers.status | kZero);
    }
    append_checkpoint(0x7261U, 0x7263U, kImmediate);

    if ((registers.status & kZero) == 0U) {
        // ---- 7266  consume the request -----------------------------------
        // Two work RAM instructions, and then the body, which converts the
        // score and drives the VDC through 09:72FD. The walk clears the bit
        // and hands the redraw itself back to the interpreter.
        append_checkpoint(0x7263U, 0x7266U, kBranchTaken);

        const auto cleared = static_cast<std::uint8_t>(
            registers.accumulator
            & static_cast<std::uint8_t>(~kPanelDirtyBit));
        load_accumulator(cleared);
        append_checkpoint(0x7266U, 0x7268U, kImmediate);

        replacement.work_ram_writes.push_back({
            .instruction = {kBank, 0x7268U},
            .logical_address = kPanelFlags,
            .value = registers.accumulator,
            .master_cycle_offset = 0U,
        });
        append_checkpoint(0x7268U, 0x726BU, kAbsolute);

        return stop_before(0x726BU);
    }
    append_checkpoint(0x7263U, 0x7265U, kBranchNotTaken);

    // ---- 7265  the return, absorbed --------------------------------------
    // The common answer, 2,616 of 2,649 calls: nothing about the panel has
    // changed. The routine pushes nothing, so the stack pointer is still the
    // one the call was entered on.
    auto at_return = registers;
    at_return.program_counter = 0x7265U;
    absorb_return(
        replacement,
        work_ram,
        entry.registers.stack_pointer,
        {kBank, 0x7265U},
        cycle,
        at_return);
    return replacement;
}

}  // namespace devils_crush::app::detail
