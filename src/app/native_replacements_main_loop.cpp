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

// 01:C154-01:C1B5, the main session loop: wait for the frame, run the frame's
// nine subsystems in order, then ask the session state whether to run another.
//
//   C154  JSR $E321               wait for vblank
//   C157  JSR $C4DE               the actor scan, through its preamble
//   C15A  JSR $F261               the actor motion pass
//   C15D  STZ <$2F / LDA #$08 / STA <$30 / JSR $E241     request $08:$00
//   C166  JSR $C3CB               the control-piece input gate
//   C169  JSR $C49E
//   C16C  JSR $CA84               the ball/table gate
//   C16F  LDA #$03 / ... / JSR $E241                     request $07:$03
//   C17A  LDA #$09 / ... / JSR $E241                     request $08:$09
//   C185  LDA #$15 / ... / JSR $E241                     request $08:$15
//   C190  LDA $2CC0 / BPL $C198                          ending requested?
//   C198  LDA <$53 / four `BIT #` tests / BEQ $C154       another frame?
//
// Nine calls, and eight of them go straight into a registered transcript. The
// ninth is 00:E321, the frame-sync wait, which is not a porting target and
// never will be: the machine is meant to sit there. It is crossed all the same
// -- the `JSR` itself is 21 master cycles of work that the transcript can do,
// and what happens on the far side of it is the interpreter's business either
// way.
//
// **This is a chain of crossings rather than one walk.** A crossing ends a
// transcript, so the loop is ten transcripts, one per call, each entered at the
// address the previous callee returns to. Composition -- splicing the callee's
// own transcript in and walking its `RTS` back -- is what the collision sweep
// does one level down, and it is not available here: these callees are the
// frame's entire actor scan and motion pass, tens of thousands of cycles and
// hundreds of writes each, past every interrupt horizon and both channel caps.
//
// Ten entry points, all of them return addresses:
//
//   C157 C15A C15D C166 C169 C16C C16F C17A C185 C190
//
// 01:C154 is deliberately not one. Control reaches it twice: from the walk
// entered at 01:C190, which crosses it, and once at start-up by falling through
// from 01:C151, which the interpreter runs.
//
// Five arms leave the loop and none of them executes on the recorded route:
//
//   C195  JMP $C239   the maximum-score ending, requested by $2CC0.7
//   C1A2  JMP $C330   \
//   C1A9  JMP $C20D    | four session requests, in priority order, out of
//   C1B0  JMP $C223    | the four `BIT #` tests on the session flags at <$53
//   C1B7  JMP $C377   /
//   C1BA               the <$53.4 arm, which reads $2CC7 and leaves as well
//
// All six are handed back. The route takes the back edge on all 2,649
// iterations, so transcribing an arm would be claiming a path this port has
// never watched execute -- and 01:C195 in particular is live in real play, as
// the score-saturation helper sets $2CC0.7 and the ending runs through it.
//
// **`BIT #` sets N and V on this core.** A 65C02 leaves both alone for the
// immediate form; the core routes every addressing mode through one macro,
//
//   P &= ~(Z|V|N); P |= ZNTable[x & A] & Z; P |= x & (V|N);
//
// so `BIT #$80` at 01:C1A5 sets N and `BIT #$40` at 01:C1B3 sets V, from the
// operand rather than from the result. Nothing in the loop reads either -- all
// four tests branch on Z -- but the status register the transcript hands back
// is the emulator's, not the processor's, so it carries them.
constexpr std::uint8_t kBank = 0x01U;

constexpr std::uint8_t kZero = 0x02U;
constexpr std::uint8_t kDecimal = 0x08U;
constexpr std::uint8_t kMemory = 0x20U;
constexpr std::uint8_t kOverflow = 0x40U;
constexpr std::uint8_t kNegative = 0x80U;

// Work-RAM window. Zero page is $2000-$20FF and the stack page is $2100-$21FF.
constexpr std::uint16_t kWorkRamBase = 0x2000U;
constexpr std::uint16_t kWorkRamEnd = 0x4000U;
constexpr std::uint16_t kStackBase = 0x2100U;

// Master cycles per instruction: three times the core's ADDCYC(n) plus the
// LastCycle it always charges.
constexpr std::uint32_t kImmediate = 6U;
constexpr std::uint32_t kZeroPage = 12U;
constexpr std::uint32_t kAbsolute = 15U;
constexpr std::uint32_t kBranchTaken = 12U;
constexpr std::uint32_t kBranchNotTaken = 6U;
// JSR is seven CPU cycles: both return bytes pushed, then ADDCYC(6) and the
// LastCycle. Both writes carry the instruction's start offset, since the core
// pushes before it charges anything.
constexpr std::uint32_t kCall = 21U;

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

[[nodiscard]] constexpr bool is_owned_entry(const std::uint16_t logical_pc)
{
    return logical_pc == 0xC157U || logical_pc == 0xC15AU
        || logical_pc == 0xC15DU || logical_pc == 0xC166U
        || logical_pc == 0xC169U || logical_pc == 0xC16CU
        || logical_pc == 0xC16FU || logical_pc == 0xC17AU
        || logical_pc == 0xC185U || logical_pc == 0xC190U;
}

// Where each walk stops: the five `JMP`s out of the loop and the one arm that
// leaves through a test of its own. No walk here reaches a return -- every one
// of them ends on a call it crossed or on an arm it declines -- so nothing is
// absorbed and the stack is exactly as it was found in both cases.
[[nodiscard]] constexpr bool is_stop_instruction(const std::uint16_t logical_pc)
{
    return logical_pc == 0xC195U || logical_pc == 0xC1A2U
        || logical_pc == 0xC1A9U || logical_pc == 0xC1B0U
        || logical_pc == 0xC1B7U || logical_pc == 0xC1BAU;
}

} // namespace

std::optional<RoutineReplacement> replace_main_session_loop(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    if (entry.routine.physical_bank != kBank
        || !is_owned_entry(entry.routine.logical_pc)
        || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    // Neither flag can change anything here: there is no arithmetic for decimal
    // to alter, and no instruction in the loop takes the memory operand the T
    // flag redirects. Both are refused anyway, so that every walk in this port
    // is admitted under the same conditions rather than under a per-family
    // argument about which ones happen not to matter.
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

    // Zero page is the first 256 bytes of work RAM, which the core hardwires
    // rather than reading out of MPR1.
    const auto zp = [](const std::uint8_t address) {
        return static_cast<std::uint16_t>(kWorkRamBase + address);
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

    const auto push =
        [&registers, &store](
            const std::uint16_t instruction_pc,
            const std::uint8_t value) {
            store(
                instruction_pc,
                static_cast<std::uint16_t>(kStackBase + registers.stack_pointer),
                value);
            registers.stack_pointer =
                static_cast<std::uint8_t>(registers.stack_pointer - 1U);
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

    // The core's one BIT, reached by every addressing mode: Z from the test,
    // and N and V copied out of the operand rather than out of the result.
    const auto bit_immediate = [&registers](const std::uint8_t value) {
        auto status = static_cast<std::uint8_t>(
            registers.status
            & static_cast<std::uint8_t>(~(kZero | kOverflow | kNegative)));
        if ((registers.accumulator & value) == 0U) {
            status = static_cast<std::uint8_t>(status | kZero);
        }
        registers.status = static_cast<std::uint8_t>(
            status | static_cast<std::uint8_t>(value & (kOverflow | kNegative)));
    };

    // A call the walk makes rather than hands back. The two return bytes are
    // pushed before the instruction charges a cycle, so both writes carry its
    // start offset, and the transcript ends on the callee's first instruction
    // with whatever runs there picking the body up.
    std::optional<RoutineKey> crossed;
    const auto call =
        [&registers, &cycle, &push, &commit_pending_writes, &append_checkpoint,
         &crossed](
            const std::uint16_t instruction_pc,
            const std::uint16_t target_pc) {
            const auto pushed = static_cast<std::uint16_t>(instruction_pc + 2U);
            push(instruction_pc, static_cast<std::uint8_t>(pushed >> 8U));
            push(instruction_pc, static_cast<std::uint8_t>(pushed & 0x00FFU));
            commit_pending_writes(cycle);
            append_checkpoint(instruction_pc, target_pc, kCall);
            crossed = RoutineKey{
                .physical_bank = registers.mpr[target_pc >> 13U],
                .logical_pc = target_pc,
            };
        };

    auto pc = entry.routine.logical_pc;
    for (;;) {
        // The core's channels are 512 writes and 2,048 checkpoints. The longest
        // walk here is the tail at fourteen instructions, and the widest is one
        // of the request setups at two stores and a crossed call's two pushes,
        // so exceeding either means the walk has gone somewhere it was not
        // written for and the assembly should run.
        if (overflow
            || replacement.checkpoints.size() > 32U
            || replacement.work_ram_writes.size() > 16U) {
            return std::nullopt;
        }

        if (is_stop_instruction(pc)) {
            registers.program_counter = pc;
            replacement.return_instruction = {kBank, pc};
            replacement.output_registers = registers;
            replacement.body_master_cycles = cycle;
            return replacement;
        }

        switch (pc) {
        // ---- the actor scan and the motion pass -----------------------------
        case 0xC157U:  // JSR $C4DE -- crossed into the scan's preamble
            call(0xC157U, 0xC4DEU);
            break;

        case 0xC15AU:  // JSR $F261 -- crossed into the motion pass
            call(0xC15AU, 0xF261U);
            break;

        // ---- request $08:$00 ------------------------------------------------
        case 0xC15DU:  // STZ <$2F
            store(0xC15DU, zp(0x2FU), 0x00U);
            append_checkpoint(0xC15DU, 0xC15FU, kZeroPage);
            pc = 0xC15FU;
            break;

        case 0xC15FU:  // LDA #$08
            load_accumulator(0x08U);
            append_checkpoint(0xC15FU, 0xC161U, kImmediate);
            pc = 0xC161U;
            break;

        case 0xC161U:  // STA <$30
            store(0xC161U, zp(0x30U), registers.accumulator);
            append_checkpoint(0xC161U, 0xC163U, kZeroPage);
            pc = 0xC163U;
            break;

        case 0xC163U:  // JSR $E241
            call(0xC163U, 0xE241U);
            break;

        // ---- the control gates ----------------------------------------------
        case 0xC166U:  // JSR $C3CB -- the control-piece input gate
            call(0xC166U, 0xC3CBU);
            break;

        case 0xC169U:  // JSR $C49E
            call(0xC169U, 0xC49EU);
            break;

        case 0xC16CU:  // JSR $CA84 -- the ball/table gate
            call(0xC16CU, 0xCA84U);
            break;

        // ---- request $07:$03 ------------------------------------------------
        case 0xC16FU:  // LDA #$03
            load_accumulator(0x03U);
            append_checkpoint(0xC16FU, 0xC171U, kImmediate);
            pc = 0xC171U;
            break;

        case 0xC171U:  // STA <$2F
            store(0xC171U, zp(0x2FU), registers.accumulator);
            append_checkpoint(0xC171U, 0xC173U, kZeroPage);
            pc = 0xC173U;
            break;

        case 0xC173U:  // LDA #$07
            load_accumulator(0x07U);
            append_checkpoint(0xC173U, 0xC175U, kImmediate);
            pc = 0xC175U;
            break;

        case 0xC175U:  // STA <$30
            store(0xC175U, zp(0x30U), registers.accumulator);
            append_checkpoint(0xC175U, 0xC177U, kZeroPage);
            pc = 0xC177U;
            break;

        case 0xC177U:  // JSR $E241
            call(0xC177U, 0xE241U);
            break;

        // ---- request $08:$09 ------------------------------------------------
        case 0xC17AU:  // LDA #$09
            load_accumulator(0x09U);
            append_checkpoint(0xC17AU, 0xC17CU, kImmediate);
            pc = 0xC17CU;
            break;

        case 0xC17CU:  // STA <$2F
            store(0xC17CU, zp(0x2FU), registers.accumulator);
            append_checkpoint(0xC17CU, 0xC17EU, kZeroPage);
            pc = 0xC17EU;
            break;

        case 0xC17EU:  // LDA #$08
            load_accumulator(0x08U);
            append_checkpoint(0xC17EU, 0xC180U, kImmediate);
            pc = 0xC180U;
            break;

        case 0xC180U:  // STA <$30
            store(0xC180U, zp(0x30U), registers.accumulator);
            append_checkpoint(0xC180U, 0xC182U, kZeroPage);
            pc = 0xC182U;
            break;

        case 0xC182U:  // JSR $E241
            call(0xC182U, 0xE241U);
            break;

        // ---- request $08:$15 ------------------------------------------------
        case 0xC185U:  // LDA #$15
            load_accumulator(0x15U);
            append_checkpoint(0xC185U, 0xC187U, kImmediate);
            pc = 0xC187U;
            break;

        case 0xC187U:  // STA <$2F
            store(0xC187U, zp(0x2FU), registers.accumulator);
            append_checkpoint(0xC187U, 0xC189U, kZeroPage);
            pc = 0xC189U;
            break;

        case 0xC189U:  // LDA #$08
            load_accumulator(0x08U);
            append_checkpoint(0xC189U, 0xC18BU, kImmediate);
            pc = 0xC18BU;
            break;

        case 0xC18BU:  // STA <$30
            store(0xC18BU, zp(0x30U), registers.accumulator);
            append_checkpoint(0xC18BU, 0xC18DU, kZeroPage);
            pc = 0xC18DU;
            break;

        case 0xC18DU:  // JSR $E241
            call(0xC18DU, 0xE241U);
            break;

        // ---- has the session been asked to end? ------------------------------
        case 0xC190U:  // LDA $2CC0
            load_accumulator(read_ram(0x2CC0U));
            append_checkpoint(0xC190U, 0xC193U, kAbsolute);
            pc = 0xC193U;
            break;

        case 0xC193U:  // BPL $C198 -- $2CC0.7 is the ending request
            pc = branch(
                0xC193U, 0xC198U, 0xC195U,
                (registers.status & kNegative) == 0U);
            break;

        // ---- ... or to leave the loop some other way -------------------------
        case 0xC198U:  // LDA <$53 -- the session flags
            load_accumulator(read_ram(zp(0x53U)));
            append_checkpoint(0xC198U, 0xC19AU, kZeroPage);
            pc = 0xC19AU;
            break;

        case 0xC19AU:  // BIT #$10
            bit_immediate(0x10U);
            append_checkpoint(0xC19AU, 0xC19CU, kImmediate);
            pc = 0xC19CU;
            break;

        case 0xC19CU:  // BNE $C1BA
            pc = branch(
                0xC19CU, 0xC1BAU, 0xC19EU,
                (registers.status & kZero) == 0U);
            break;

        case 0xC19EU:  // BIT #$20
            bit_immediate(0x20U);
            append_checkpoint(0xC19EU, 0xC1A0U, kImmediate);
            pc = 0xC1A0U;
            break;

        case 0xC1A0U:  // BEQ $C1A5
            pc = branch(
                0xC1A0U, 0xC1A5U, 0xC1A2U,
                (registers.status & kZero) != 0U);
            break;

        case 0xC1A5U:  // BIT #$80 -- sets N from the operand
            bit_immediate(0x80U);
            append_checkpoint(0xC1A5U, 0xC1A7U, kImmediate);
            pc = 0xC1A7U;
            break;

        case 0xC1A7U:  // BEQ $C1AC
            pc = branch(
                0xC1A7U, 0xC1ACU, 0xC1A9U,
                (registers.status & kZero) != 0U);
            break;

        case 0xC1ACU:  // BIT #$04
            bit_immediate(0x04U);
            append_checkpoint(0xC1ACU, 0xC1AEU, kImmediate);
            pc = 0xC1AEU;
            break;

        case 0xC1AEU:  // BEQ $C1B3
            pc = branch(
                0xC1AEU, 0xC1B3U, 0xC1B0U,
                (registers.status & kZero) != 0U);
            break;

        case 0xC1B3U:  // BIT #$40 -- sets V from the operand
            bit_immediate(0x40U);
            append_checkpoint(0xC1B3U, 0xC1B5U, kImmediate);
            pc = 0xC1B5U;
            break;

        case 0xC1B5U:  // BEQ $C154 -- the back edge: another frame
            pc = branch(
                0xC1B5U, 0xC154U, 0xC1B7U,
                (registers.status & kZero) != 0U);
            break;

        // ---- the frame-sync wait, crossed into rather than handed back -------
        case 0xC154U:  // JSR $E321
            call(0xC154U, 0xE321U);
            break;

        default:
            return std::nullopt;
        }

        // A call ends the walk, but it is made rather than left standing: the
        // transcript hands the machine the callee's first instruction, and
        // whatever owns that address picks the body up from there. Nothing is
        // absorbed here -- the return the walk would take belongs to the callee
        // and has not been executed yet.
        if (crossed.has_value()) {
            if (overflow) {
                return std::nullopt;
            }
            replacement.return_instruction = *crossed;
            replacement.output_registers = registers;
            replacement.body_master_cycles = cycle;
            return replacement;
        }
    }
}

} // namespace devils_crush::app::detail
