// SPDX-License-Identifier: GPL-3.0-or-later

#include "native_replacements_internal.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace devils_crush::app::detail {
namespace {

using emulation::CpuRegisters;
using emulation::NativeBbr3Instruction;
using emulation::RoutineEntry;
using emulation::RoutineKey;
using emulation::RoutineReplacement;

// 01:C6C2-01:C71D and 01:C721-01:C770, the ball's initial and resumed substep
// loops. They consume the substep split at 01:DC95. Every routine they call is
// already in the registry, so what was left running interpreted was the
// control flow between them.
//
// `$57` is the substep count 01:DC95 wrote for this frame and `$56` counts the
// substeps taken against it. `$59` is a cap of sixteen on the number of times
// round, and the two times it ran out on this route the loop left through
// 01:C8C2, which zeroes both motion components and the accumulated step. The
// far end runs the path-length accumulator at 01:C8F3, the damping at 01:DD09
// and the drain test at 01:DD6E, then tail-jumps back into 01:DC95 to split the
// next frame's motion.
//
// Twenty entry points. The initial pass contributes nine: C6C2 crosses the
// bank-call wrapper; C6C5/C6D9/C6E9/C6F2/C6F5/C708 continue crossed calls;
// and C6E6/C6EB are the two arms after the core executes the compound BBR3.
// The resumed loop contributes the eleven below, every one an address a call
// returns to except the head:
//
//   01:C721  the head. It is not reached by a `JSR` at all -- 01:C711 writes
//            $21/$C7 into `$3F00,X`/`$3F80,X`, which is where the coroutine
//            scheduler at 01:C5C9 keeps this actor's resume address, so the
//            machine arrives here from the scheduler rather than from a caller.
//            That costs this walk nothing: it absorbs no return and pushes
//            nothing except the two bytes of each `JSR` it crosses, so the
//            stack it hands back is the stack it was given.
//   01:C72E  after `JSR $E219`, the bank-call wrapper.
//   01:C742  after `JSR $F1B4`, the position integrator.
//   01:C74C  after `JSR $CB2A`, the collision sweep.
//   01:C759  after the `BBR` below falls through -- one crossed `JSR $F241`.
//   01:C75C  after `JSR $F241`, the cached-position restore.
//   01:C75E  the `BBR`'s taken arm, and one crossed `JSR $C969`.
//   01:C761  after `JSR $C969`, the range-squared helper.
//   01:C768  after `JSR $C8F3` -- one crossed `JSR $DD09`.
//   01:C76B  after `JSR $DD09` -- one crossed `JSR $DD6E`.
//   01:C76E  after `JSR $DD6E`, the drain test, and the loop's exit.
//
// All fifteen calls across the two loops are crossed rather than stopped in
// front of, and all fifteen land on a registered entry, so the machine leaves
// each transcript already standing on the next one.
//
// Shared state was measured on a pure-assembly write trace of every write to
// `$56`, `$57`, `$59` and `$3C80-$3CFF` across the whole route. `$56` is
// written by 01:C500, by this loop's own `INC` at 01:C761 and by the
// near-identical pre-loop at 01:C6EB; `$57` only by 01:DC95's five stores;
// `$59` only by this loop, the pre-loop and the startup `TII` at 01:C100; and
// `$3C80,X` by bank 01, 02, 03 and 08 game code plus 00:F1D7 and 00:F23D, which
// are inside two of the callees above. Nothing in the VDC handler, the timer
// driver or the music sequencer writes any of it, so no interrupt taken inside
// one of these windows can change what the walk read. The callee writers are
// the reason 01:C742 and 01:C74C are separate entries rather than one long
// walk: each re-reads `$3C80,X` after a call that may have written it, and a
// transcript stages work RAM once at its entry.
constexpr std::uint8_t kBank = 0x01U;

constexpr std::uint8_t kCarry = 0x01U;
constexpr std::uint8_t kZero = 0x02U;
constexpr std::uint8_t kDecimal = 0x08U;
constexpr std::uint8_t kMemory = 0x20U;
constexpr std::uint8_t kNegative = 0x80U;

// Work-RAM window. Zero page is $2000-$20FF and the stack page is $2100-$21FF.
constexpr std::uint16_t kWorkRamBase = 0x2000U;
constexpr std::uint16_t kWorkRamEnd = 0x4000U;
constexpr std::uint16_t kStackBase = 0x2100U;

// Master cycles per instruction: three times the core's ADDCYC(n) plus the
// LastCycle it always charges.
constexpr std::uint32_t kImmediate = 6U;
constexpr std::uint32_t kZeroPage = 12U;
constexpr std::uint32_t kAbsoluteIndexed = 15U;
constexpr std::uint32_t kZeroPageReadModifyWrite = 18U;
constexpr std::uint32_t kBranchTaken = 12U;
constexpr std::uint32_t kBranchNotTaken = 6U;
// BRA is a branch whose condition is a constant true, and it costs what any
// taken branch costs. An absolute `JMP` costs the same four CPU cycles.
constexpr std::uint32_t kBranchAlways = 12U;
constexpr std::uint32_t kJump = 12U;
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
    return logical_pc == 0xC6C2U || logical_pc == 0xC6C5U
        || logical_pc == 0xC6D9U || logical_pc == 0xC6E6U
        || logical_pc == 0xC6E9U || logical_pc == 0xC6EBU
        || logical_pc == 0xC6F2U || logical_pc == 0xC6F5U
        || logical_pc == 0xC708U
        || logical_pc == 0xC721U || logical_pc == 0xC72EU
        || logical_pc == 0xC742U || logical_pc == 0xC74CU
        || logical_pc == 0xC759U || logical_pc == 0xC75CU
        || logical_pc == 0xC75EU || logical_pc == 0xC761U
        || logical_pc == 0xC768U || logical_pc == 0xC76BU
        || logical_pc == 0xC76EU;
}

// Where each walk stops. None of them is an `RTS`: this loop has no return of
// its own, and it leaves through a jump on every arm, so nothing here is
// absorbed and the `absorb_return` contract never comes up.
//
// 01:C756 is the `BBR3 <$77` and is handed back on purpose. Every other
// instruction here charges its cycles in one ADDCYC and one LastCycle, which is
// the shape a checkpoint expresses; `BBR` is `LD_ZP` wrapping `JR`, so the
// machine crosses it in four clock steps where a checkpoint crosses it in two,
// and those steps are where the core tests `next_event`. Both of its arms,
// 01:C759 and 01:C75E, are entry points precisely because the core resolves the
// `BBR` itself. It is the only opcode in this range with a low nibble of `F`,
// and it costs about 64,000 master cycles to leave interpreted.
//
// 01:C728, 01:C753 and 01:C773 are branch arms this route never takes: the
// table-inactive exit through 01:C8D7 twice over, and the drain test reporting
// a drained ball. Handing them back costs nothing -- they did not execute once
// in 3,600 frames -- and keeps the walk from claiming a path it has never seen.
//
// 01:C8C2 and 01:DC95 are the other two ends. They are not instructions this
// walk transcribes; they are where its two transcribed `JMP`s put the machine,
// and the walk stops standing on them. 01:DC95 is a registered entry, so that
// arm hands straight over to the substep split; 01:C8C2 is not, and converting
// the `JMP` in front of it is worth its twelve cycles either way.
[[nodiscard]] constexpr bool is_stop_instruction(const std::uint16_t logical_pc)
{
    return logical_pc == 0xC6E3U || logical_pc == 0xC70AU
        || logical_pc == 0xC728U || logical_pc == 0xC753U
        || logical_pc == 0xC756U || logical_pc == 0xC773U
        || logical_pc == 0xC8C2U || logical_pc == 0xDC95U;
}

} // namespace

std::optional<RoutineReplacement> replace_ball_substep_loop(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    if (entry.routine.physical_bank != kBank
        || !is_owned_entry(entry.routine.logical_pc)
        || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    // The memory-operand T flag would take the left-hand side of both `AND`s
    // out of zero page instead of the accumulator, and nothing in this loop is
    // written to run that way. Decimal changes none of these instructions --
    // there is no `ADC` or `SBC` here -- but it is refused with T because every
    // other walk in this bank refuses it and nothing needs it allowed.
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

    // Zero page is the first 256 bytes of work RAM, because MPR1 holds F8 on
    // every entry this loop is reached from.
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

    // PUSH writes $2100 + S and then decrements S. Deriving it from the live
    // stack pointer is what keeps a transcript valid at a depth other than the
    // one it was recorded at.
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
    const auto and_accumulator = [&registers](const std::uint8_t value) {
        registers.accumulator =
            static_cast<std::uint8_t>(registers.accumulator & value);
        registers.status = set_nz(registers.status, registers.accumulator);
    };
    const auto compare_accumulator = [&registers](const std::uint8_t value) {
        const auto result =
            static_cast<std::uint8_t>(registers.accumulator - value);
        auto status = static_cast<std::uint8_t>(
            registers.status & static_cast<std::uint8_t>(~kCarry));
        if (registers.accumulator >= value) {
            status = static_cast<std::uint8_t>(status | kCarry);
        }
        registers.status = set_nz(status, result);
    };
    // INC and DEC on memory report on the result and leave the carry alone.
    const auto add_to_memory =
        [&registers, &read_ram, &store](
            const std::uint16_t instruction_pc,
            const std::uint16_t address,
            const std::uint8_t addend) {
            const auto value =
                static_cast<std::uint8_t>(read_ram(address) + addend);
            store(instruction_pc, address, value);
            registers.status = set_nz(registers.status, value);
        };

    // A call the walk makes rather than hands back. The two return bytes are
    // pushed before the instruction charges a cycle, so both writes carry its
    // start offset, and the transcript ends on the callee's first instruction
    // with whatever runs there picking the body up. What it converts is the
    // `JSR` itself, so it is worth its twenty-one cycles whatever is on the far
    // side; here all eight far sides are registered routines.
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
        // transcript here is a dozen instructions and two pushes, so exceeding
        // either means the walk has gone somewhere it was not written for and
        // the assembly should run.
        if (overflow
            || replacement.checkpoints.size() > 64U
            || replacement.work_ram_writes.size() > 32U) {
            return std::nullopt;
        }

        if (is_stop_instruction(pc)) {
            if (replacement.checkpoints.empty()) {
                return std::nullopt;
            }
            registers.program_counter = pc;
            replacement.return_instruction = {kBank, pc};
            replacement.output_registers = registers;
            replacement.body_master_cycles = cycle;
            return replacement;
        }

        switch (pc) {
        // ---- 01:C6C2  initial pass before the scheduled loop -------------
        case 0xC6C2U:  // JSR $E219 -- crossed into the bank-call wrapper
            call(0xC6C2U, 0xE219U);
            break;

        case 0xC6C5U:  // LDA #$10 -- sixteen attempts at most
            load_accumulator(0x10U);
            append_checkpoint(0xC6C5U, 0xC6C7U, kImmediate);
            pc = 0xC6C7U;
            break;

        case 0xC6C7U:  // STA $59
            store(0xC6C7U, zp(0x59U), registers.accumulator);
            append_checkpoint(0xC6C7U, 0xC6C9U, kZeroPage);
            pc = 0xC6C9U;
            break;

        case 0xC6C9U:  // LDA $56
            load_accumulator(read_ram(zp(0x56U)));
            append_checkpoint(0xC6C9U, 0xC6CBU, kZeroPage);
            pc = 0xC6CBU;
            break;

        case 0xC6CBU:  // CMP $57
            compare_accumulator(read_ram(zp(0x57U)));
            append_checkpoint(0xC6CBU, 0xC6CDU, kZeroPage);
            pc = 0xC6CDU;
            break;

        case 0xC6CDU:  // BCS $C6EF -- every requested step is complete
            pc = branch(
                0xC6CDU, 0xC6EFU, 0xC6CFU,
                (registers.status & kCarry) != 0U);
            break;

        case 0xC6CFU:  // DEC $59
            add_to_memory(0xC6CFU, zp(0x59U), 0xFFU);
            append_checkpoint(0xC6CFU, 0xC6D1U, kZeroPageReadModifyWrite);
            pc = 0xC6D1U;
            break;

        case 0xC6D1U:  // BNE $C6D6
            pc = branch(
                0xC6D1U, 0xC6D6U, 0xC6D3U,
                (registers.status & kZero) == 0U);
            break;

        case 0xC6D3U:  // JMP $C8C2 -- attempt cap exhausted
            append_checkpoint(0xC6D3U, 0xC8C2U, kJump);
            pc = 0xC8C2U;
            break;

        case 0xC6D6U:  // JSR $F1B4 -- crossed into position integration
            call(0xC6D6U, 0xF1B4U);
            break;

        case 0xC6D9U:  // LDA $3C80,X
            load_accumulator(read_ram(
                static_cast<std::uint16_t>(0x3C80U + registers.x)));
            append_checkpoint(0xC6D9U, 0xC6DCU, kAbsoluteIndexed);
            pc = 0xC6DCU;
            break;

        case 0xC6DCU:  // AND #$02
            and_accumulator(0x02U);
            append_checkpoint(0xC6DCU, 0xC6DEU, kImmediate);
            pc = 0xC6DEU;
            break;

        case 0xC6DEU:  // BNE $C6EB
            pc = branch(
                0xC6DEU, 0xC6EBU, 0xC6E0U,
                (registers.status & kZero) == 0U);
            break;

        case 0xC6E0U:  // JSR $CB2A -- crossed into the collision sweep
            call(0xC6E0U, 0xCB2AU);
            break;

        // C6E3 is the compound-sampled BBR3 <$77. The core executes it and
        // re-enters this walk at either C6E6 or C6EB.
        case 0xC6E6U:  // JSR $F241 -- restore the cached position
            call(0xC6E6U, 0xF241U);
            break;

        case 0xC6E9U:  // BRA $C6C9 -- retry without consuming a substep
            append_checkpoint(0xC6E9U, 0xC6C9U, kBranchAlways);
            pc = 0xC6C9U;
            break;

        case 0xC6EBU:  // INC $56 -- one substep consumed
            add_to_memory(0xC6EBU, zp(0x56U), 0x01U);
            append_checkpoint(0xC6EBU, 0xC6EDU, kZeroPageReadModifyWrite);
            pc = 0xC6EDU;
            break;

        case 0xC6EDU:  // BRA $C6C9
            append_checkpoint(0xC6EDU, 0xC6C9U, kBranchAlways);
            pc = 0xC6C9U;
            break;

        case 0xC6EFU:  // JSR $DD09 -- crossed into motion damping
            call(0xC6EFU, 0xDD09U);
            break;

        case 0xC6F2U:  // JSR $DC95 -- split the damped motion
            call(0xC6F2U, 0xDC95U);
            break;

        case 0xC6F5U:  // LDA $3180,X -- stage the first integer position
            load_accumulator(read_ram(
                static_cast<std::uint16_t>(0x3180U + registers.x)));
            append_checkpoint(0xC6F5U, 0xC6F8U, kAbsoluteIndexed);
            pc = 0xC6F8U;
            break;

        case 0xC6F8U:  // STA $2F
            store(0xC6F8U, zp(0x2FU), registers.accumulator);
            append_checkpoint(0xC6F8U, 0xC6FAU, kZeroPage);
            pc = 0xC6FAU;
            break;

        case 0xC6FAU:  // LDA $3200,X
            load_accumulator(read_ram(
                static_cast<std::uint16_t>(0x3200U + registers.x)));
            append_checkpoint(0xC6FAU, 0xC6FDU, kAbsoluteIndexed);
            pc = 0xC6FDU;
            break;

        case 0xC6FDU:  // STA $30
            store(0xC6FDU, zp(0x30U), registers.accumulator);
            append_checkpoint(0xC6FDU, 0xC6FFU, kZeroPage);
            pc = 0xC6FFU;
            break;

        case 0xC6FFU:  // LDA #$F0
            load_accumulator(0xF0U);
            append_checkpoint(0xC6FFU, 0xC701U, kImmediate);
            pc = 0xC701U;
            break;

        case 0xC701U:  // STA $2D
            store(0xC701U, zp(0x2DU), registers.accumulator);
            append_checkpoint(0xC701U, 0xC703U, kZeroPage);
            pc = 0xC703U;
            break;

        case 0xC703U:  // STZ $2E
            store(0xC703U, zp(0x2EU), 0U);
            append_checkpoint(0xC703U, 0xC705U, kZeroPage);
            pc = 0xC705U;
            break;

        case 0xC705U:  // JSR $E93C -- crossed into the signed comparison
            call(0xC705U, 0xE93CU);
            break;

        case 0xC708U:  // BCC $C70B; carry-set returns through core at C70A
            pc = branch(
                0xC708U, 0xC70BU, 0xC70AU,
                (registers.status & kCarry) == 0U);
            break;

        case 0xC70BU:  // STZ $3900,X
            store(
                0xC70BU,
                static_cast<std::uint16_t>(0x3900U + registers.x), 0U);
            append_checkpoint(0xC70BU, 0xC70EU, kAbsoluteIndexed);
            pc = 0xC70EU;
            break;

        case 0xC70EU:  // STZ $3980,X
            store(
                0xC70EU,
                static_cast<std::uint16_t>(0x3980U + registers.x), 0U);
            append_checkpoint(0xC70EU, 0xC711U, kAbsoluteIndexed);
            pc = 0xC711U;
            break;

        case 0xC711U:  // LDA #$21 -- install the scheduled resume address
            load_accumulator(0x21U);
            append_checkpoint(0xC711U, 0xC713U, kImmediate);
            pc = 0xC713U;
            break;

        case 0xC713U:  // STA $3F00,X
            store(
                0xC713U,
                static_cast<std::uint16_t>(0x3F00U + registers.x),
                registers.accumulator);
            append_checkpoint(0xC713U, 0xC716U, kAbsoluteIndexed);
            pc = 0xC716U;
            break;

        case 0xC716U:  // LDA #$C7
            load_accumulator(0xC7U);
            append_checkpoint(0xC716U, 0xC718U, kImmediate);
            pc = 0xC718U;
            break;

        case 0xC718U:  // STA $3F80,X
            store(
                0xC718U,
                static_cast<std::uint16_t>(0x3F80U + registers.x),
                registers.accumulator);
            append_checkpoint(0xC718U, 0xC71BU, kAbsoluteIndexed);
            pc = 0xC71BU;
            break;

        case 0xC71BU:  // JMP $DC95 -- tail handoff to the substep split
            append_checkpoint(0xC71BU, 0xDC95U, kJump);
            pc = 0xDC95U;
            break;

        // ---- 01:C721  the head: is this actor still on the table? ---------
        // Bit 0 of `$3C80,X` is the actor's off-table flag. It was clear on all
        // 2,342 arrivals, so 01:C728 has never run.
        case 0xC721U:  // LDA $3C80,X
            load_accumulator(read_ram(
                static_cast<std::uint16_t>(0x3C80U + registers.x)));
            append_checkpoint(0xC721U, 0xC724U, kAbsoluteIndexed);
            pc = 0xC724U;
            break;

        case 0xC724U:  // AND #$01
            and_accumulator(0x01U);
            append_checkpoint(0xC724U, 0xC726U, kImmediate);
            pc = 0xC726U;
            break;

        case 0xC726U:  // BEQ $C72B
            pc = branch(
                0xC726U, 0xC72BU, 0xC728U,
                (registers.status & kZero) != 0U);
            break;

        case 0xC72BU:  // JSR $E219 -- crossed into a bank-call wrapper
            call(0xC72BU, 0xE219U);
            break;

        // ---- 01:C72E  arm the cap, then the loop head ---------------------
        case 0xC72EU:  // LDA #$10 -- sixteen times round at the most
            load_accumulator(0x10U);
            append_checkpoint(0xC72EU, 0xC730U, kImmediate);
            pc = 0xC730U;
            break;

        case 0xC730U:  // STA $59
            store(0xC730U, zp(0x59U), registers.accumulator);
            append_checkpoint(0xC730U, 0xC732U, kZeroPage);
            pc = 0xC732U;
            break;

        // `$56` is the substeps taken and `$57` the substeps 01:DC95 asked
        // for, so the loop runs while `$56` is below `$57`.
        case 0xC732U:  // LDA $56
            load_accumulator(read_ram(zp(0x56U)));
            append_checkpoint(0xC732U, 0xC734U, kZeroPage);
            pc = 0xC734U;
            break;

        case 0xC734U:  // CMP $57
            compare_accumulator(read_ram(zp(0x57U)));
            append_checkpoint(0xC734U, 0xC736U, kZeroPage);
            pc = 0xC736U;
            break;

        case 0xC736U:  // BCS $C765 -- every substep taken, go to the far end
            pc = branch(
                0xC736U, 0xC765U, 0xC738U,
                (registers.status & kCarry) != 0U);
            break;

        case 0xC738U:  // DEC $59
            add_to_memory(0xC738U, zp(0x59U), 0xFFU);
            append_checkpoint(0xC738U, 0xC73AU, kZeroPageReadModifyWrite);
            pc = 0xC73AU;
            break;

        case 0xC73AU:  // BNE $C73F
            pc = branch(
                0xC73AU, 0xC73FU, 0xC73CU,
                (registers.status & kZero) == 0U);
            break;

        // The cap ran out twice in 3,600 frames. 01:C8C2 zeroes both motion
        // components and the accumulated step, which is the loop giving up on
        // a substep it cannot finish rather than an error.
        case 0xC73CU:  // JMP $C8C2
            append_checkpoint(0xC73CU, 0xC8C2U, kJump);
            pc = 0xC8C2U;
            break;

        case 0xC73FU:  // JSR $F1B4 -- crossed into the position integrator
            call(0xC73FU, 0xF1B4U);
            break;

        // ---- 01:C742  what the integrator left behind ---------------------
        // Bit 1 of `$3C80,X` says the actor has already resolved a contact this
        // substep, in which case the collision sweep is skipped.
        case 0xC742U:  // LDA $3C80,X
            load_accumulator(read_ram(
                static_cast<std::uint16_t>(0x3C80U + registers.x)));
            append_checkpoint(0xC742U, 0xC745U, kAbsoluteIndexed);
            pc = 0xC745U;
            break;

        case 0xC745U:  // AND #$02
            and_accumulator(0x02U);
            append_checkpoint(0xC745U, 0xC747U, kImmediate);
            pc = 0xC747U;
            break;

        case 0xC747U:  // BNE $C75E
            pc = branch(
                0xC747U, 0xC75EU, 0xC749U,
                (registers.status & kZero) == 0U);
            break;

        case 0xC749U:  // JSR $CB2A -- crossed into the collision sweep
            call(0xC749U, 0xCB2AU);
            break;

        // ---- 01:C74C  what the sweep left behind --------------------------
        // The sweep can take the actor off the table, which is what bit 0 is
        // re-read for. It has not happened on this route, so 01:C753 has never
        // run either.
        case 0xC74CU:  // LDA $3C80,X
            load_accumulator(read_ram(
                static_cast<std::uint16_t>(0x3C80U + registers.x)));
            append_checkpoint(0xC74CU, 0xC74FU, kAbsoluteIndexed);
            pc = 0xC74FU;
            break;

        case 0xC74FU:  // AND #$01
            and_accumulator(0x01U);
            append_checkpoint(0xC74FU, 0xC751U, kImmediate);
            pc = 0xC751U;
            break;

        case 0xC751U:  // BEQ $C756 -- into the BBR, which the core resolves
            pc = branch(
                0xC751U, 0xC756U, 0xC753U,
                (registers.status & kZero) != 0U);
            break;

        // ---- 01:C759  the BBR's fall-through ------------------------------
        case 0xC759U:  // JSR $F241 -- crossed into the cached-position restore
            call(0xC759U, 0xF241U);
            break;

        case 0xC75CU:  // BRA $C732 -- round again without counting a substep
            append_checkpoint(0xC75CU, 0xC732U, kBranchAlways);
            pc = 0xC732U;
            break;

        // ---- 01:C75E  the BBR's taken arm ---------------------------------
        case 0xC75EU:  // JSR $C969 -- crossed into the range-squared helper
            call(0xC75EU, 0xC969U);
            break;

        case 0xC761U:  // INC $56 -- one substep done
            add_to_memory(0xC761U, zp(0x56U), 0x01U);
            append_checkpoint(0xC761U, 0xC763U, kZeroPageReadModifyWrite);
            pc = 0xC763U;
            break;

        case 0xC763U:  // BRA $C732
            append_checkpoint(0xC763U, 0xC732U, kBranchAlways);
            pc = 0xC732U;
            break;

        // ---- 01:C765  the far end, once every substep is spent ------------
        case 0xC765U:  // JSR $C8F3 -- crossed into the path-length accumulator
            call(0xC765U, 0xC8F3U);
            break;

        case 0xC768U:  // JSR $DD09 -- crossed into the motion damping
            call(0xC768U, 0xDD09U);
            break;

        case 0xC76BU:  // JSR $DD6E -- crossed into the drain test
            call(0xC76BU, 0xDD6EU);
            break;

        // The drain test reports a drained ball in the carry, and it has not
        // reported one on this route: 01:C773 has never run.
        case 0xC76EU:  // BCS $C773
            pc = branch(
                0xC76EU, 0xC773U, 0xC770U,
                (registers.status & kCarry) != 0U);
            break;

        // The tail jump into the substep split, which arms the next frame. It
        // is the site that gives 01:DC95 2,340 of its 2,895 arrivals, and
        // transcribing the `JMP` leaves the stack exactly as the `JMP` would --
        // the split still reads the frame this loop's own caller pushed.
        case 0xC770U:  // JMP $DC95
            append_checkpoint(0xC770U, 0xDC95U, kJump);
            pc = 0xDC95U;
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

std::optional<RoutineReplacement> replace_ball_substep_bbr3(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // BBR3 <$77, $C75E. The core owns the zero-page read and the branch so it
    // can sample an event after each one; this only records the arm it will
    // execute and asks the core to retain that exact opcode path.
    if (entry.routine != RoutineKey{kBank, 0xC756U}
        || entry.registers.mpr[1] != 0xF8U
        || work_ram.size() <= 0x77U) {
        return std::nullopt;
    }

    const auto value = std::to_integer<std::uint8_t>(work_ram[0x77U]);
    const bool clear = (value & 0x08U) == 0U;
    const auto resume_pc = static_cast<std::uint16_t>(
        clear ? 0xC75EU : 0xC759U);
    RoutineReplacement replacement{};
    auto registers = entry.registers;
    registers.program_counter = resume_pc;
    replacement.return_instruction = {
        registers.mpr[resume_pc >> 13U],
        resume_pc,
    };
    replacement.output_registers = registers;
    replacement.body_master_cycles = clear ? 21U : 15U;
    replacement.native_bbr3_instruction = NativeBbr3Instruction{};
    return replacement;
}

} // namespace devils_crush::app::detail
