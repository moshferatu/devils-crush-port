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

// 03:6966, the collision-table interpreter, as an instruction-boundary
// transcript rather than a computed result.
//
// The routine is the busiest single entry on the census route: 34,259 entries
// and 14.4M native master cycles with the registry deployed, and 54,149 calls
// when nothing is replaced at all. It advances one actor along a scripted
// motion path. `A` selects a table from the sixteen-bit pointer array at
// `$6A38`; `$3E00,X` and `$3E80,X` are the actor's two cursors into that table
// and into the motion record it names; the record's bytes step the actor's
// sixteen-bit position at `$3180`/`$3200` horizontally and `$3300`/`$3380`
// vertically, one unit at a time.
//
// It was already replaced, and replaced correctly -- the semantic
// implementation in native_replacements_collision.cpp resolves the record and
// emits the writes, and it covers 34,254 of the 34,259 entries. What it could
// not do is produce an instruction-boundary transcript for every path. It emits
// checkpoints only for headerless one-step records with bounded axes, so every
// other path returns an atomic body, and an atomic body is refused whenever it
// would span a user event or a serviceable timer interrupt. That refusal costs
// 2,850 entries and 4.7M emulated master cycles, and it is also what stops the
// interpreter composing into its callers: both composition sites in the tree
// gate on `checkpoints` being non-empty, and one of them additionally pins the
// child to the single 165-cycle path.
//
// A transcript does not need to know what the routine means, so the shape of
// the problem is not the shape of the existing replacement. The routine is 0xD2
// bytes and about a hundred instructions. It reads work RAM, zero page and its
// own bank's ROM, and nothing else -- no device register, no call out of the
// bank, no bank change. The only control flow that leaves the straight line is
// its own: a `BRA` back to 03:6995 when a record ends in a `$C0` terminator, and
// a `JSR`/`RTS` loop at 03:6989 that runs ten times on the whole route.
//
// So this walk executes the routine, and the semantic implementation stays as
// the fallback for the calls the caps below refuse.
constexpr std::uint8_t kBank = 0x03U;

constexpr std::uint8_t kCarry = 0x01U;
constexpr std::uint8_t kZero = 0x02U;
constexpr std::uint8_t kDecimal = 0x08U;
constexpr std::uint8_t kBreak = 0x10U;
constexpr std::uint8_t kMemory = 0x20U;
constexpr std::uint8_t kOverflow = 0x40U;
constexpr std::uint8_t kNegative = 0x80U;

constexpr std::uint16_t kWorkRamBase = 0x2000U;
constexpr std::uint16_t kWorkRamEnd = 0x4000U;
constexpr std::uint16_t kStackBase = 0x2100U;

// Every cost here is (1 + the operation's own ADDCYC) CPU cycles times three,
// because HuC6280::LastCycle spends the cycle the interrupt is sampled on and
// every instruction reaches it exactly once. The bit-branch reaches it twice --
// once for the zero-page read and once for the branch -- which is why it is the
// only instruction in the routine that costs more than a subroutine call.
constexpr std::uint32_t kImplied = 6U;
constexpr std::uint32_t kImmediate = 6U;
constexpr std::uint32_t kAccumulator = 6U;
constexpr std::uint32_t kZeroPage = 12U;
constexpr std::uint32_t kAbsoluteIndexed = 15U;
constexpr std::uint32_t kAbsoluteIndexedReadModifyWrite = 21U;
constexpr std::uint32_t kSetMemoryBit = 21U;
constexpr std::uint32_t kIndirect = 21U;
constexpr std::uint32_t kIndirectIndexed = 21U;
constexpr std::uint32_t kPush = 9U;
constexpr std::uint32_t kPull = 12U;
constexpr std::uint32_t kJumpToSubroutine = 21U;
constexpr std::uint32_t kReturnFromSubroutine = 21U;
constexpr std::uint32_t kBranchTaken = 12U;
constexpr std::uint32_t kBranchNotTaken = 6U;
constexpr std::uint32_t kBranchAlways = 12U;
constexpr std::uint32_t kBitBranchTaken = 24U;
constexpr std::uint32_t kBitBranchNotTaken = 18U;

// The record loop at 03:69D1 has no bound of its own; it ends when the table
// runs out of `$C0` terminators. Three calls on the census route write more
// than the transaction cap allows, so the walk refuses them here and the
// semantic replacement keeps them.
constexpr std::size_t kMaximumCheckpoints = 1024U;
constexpr std::size_t kMaximumWrites = 480U;

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

// The four `RTS` instructions the routine can end on. Three of them are also
// reachable one frame deep, through the `JSR $6995` at 03:698A, so reaching one
// is not on its own a reason to stop -- the walk stops only when the return
// address on the stack is the caller's.
[[nodiscard]] constexpr bool is_return(const std::uint16_t logical_pc)
{
    return logical_pc == 0x6988U || logical_pc == 0x6994U
        || logical_pc == 0x69A1U || logical_pc == 0x6A37U;
}

} // namespace

std::optional<RoutineReplacement> replace_collision_table_walk(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = kBank,
        .logical_pc = 0x6966U,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    // Nothing in the routine is arithmetic that decimal mode would change, but
    // the memory-operand T flag would redirect every AND, ORA and EOR below
    // through zero page.
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
    std::uint32_t depth = 0U;
    bool overflow = false;

    const auto read_ram =
        [&ram, &overflow](const std::uint16_t address) -> std::uint8_t {
            if (address < kWorkRamBase || address >= kWorkRamEnd) {
                overflow = true;
                return 0U;
            }
            return ram[address - kWorkRamBase];
        };

    // Zero page and the stack are the first two pages of work RAM, because the
    // HuC6280 addresses both through MPR1 and every entry here arrives with F8
    // in it.
    const auto zero_page = [](const std::uint8_t address) {
        return static_cast<std::uint16_t>(kWorkRamBase + address);
    };

    // The two record tables live in this bank's own ROM page and the two
    // indirect loads resolve through zero-page pointers that could name any
    // page at all, so both go through the live MPRs. A hardware-page bank fails
    // the test, which makes a device read decline rather than fabricate.
    const auto read_mapped =
        [&registers, &ram, rom, &overflow](
            const std::uint16_t address) -> std::uint8_t {
            const auto bank = registers.mpr[address >> 13U];
            if (bank == 0xF8U) {
                return ram[address & 0x1FFFU];
            }
            if (bank >= 0x80U || rom.size() != 0x60000U) {
                overflow = true;
                return 0U;
            }
            const auto rom_bank = bank < 0x40U
                ? static_cast<std::size_t>(bank & 0x1FU)
                : static_cast<std::size_t>((bank & 0x0FU) + 0x20U);
            const auto offset = rom_bank * 0x2000U
                + static_cast<std::size_t>(address & 0x1FFFU);
            if (offset >= rom.size()) {
                overflow = true;
                return 0U;
            }
            return std::to_integer<std::uint8_t>(rom[offset]);
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

    // A push writes at the stack pointer and then moves it down; a pull moves it
    // up and then reads. Both stay inside the second page of work RAM, so they
    // are ordinary transaction writes and reads.
    const auto push =
        [&registers, &store](
            const std::uint16_t instruction_pc,
            const std::uint8_t value) {
            store(
                instruction_pc,
                static_cast<std::uint16_t>(
                    kStackBase + registers.stack_pointer),
                value);
            registers.stack_pointer =
                static_cast<std::uint8_t>(registers.stack_pointer - 1U);
        };
    const auto pull = [&registers, &read_ram]() {
        registers.stack_pointer =
            static_cast<std::uint8_t>(registers.stack_pointer + 1U);
        return read_ram(static_cast<std::uint16_t>(
            kStackBase + registers.stack_pointer));
    };

    const auto slot_x = [&registers](const std::uint16_t base) {
        return static_cast<std::uint16_t>(
            base + static_cast<std::uint16_t>(registers.x));
    };

    // The pointer an indirect load goes through is a zero-page pair, and the
    // high half wraps inside zero page rather than running into the stack.
    const auto indirect =
        [&read_ram, &zero_page](const std::uint8_t pointer) {
            const auto low = read_ram(zero_page(pointer));
            const auto high =
                read_ram(zero_page(static_cast<std::uint8_t>(pointer + 1U)));
            return static_cast<std::uint16_t>(
                low | static_cast<std::uint16_t>(high) << 8U);
        };

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
    const auto load_y = [&registers](const std::uint8_t value) {
        registers.y = value;
        registers.status = set_nz(registers.status, value);
    };
    const auto and_accumulator = [&registers](const std::uint8_t value) {
        registers.accumulator =
            static_cast<std::uint8_t>(registers.accumulator & value);
        registers.status = set_nz(registers.status, registers.accumulator);
    };
    const auto or_accumulator = [&registers](const std::uint8_t value) {
        registers.accumulator =
            static_cast<std::uint8_t>(registers.accumulator | value);
        registers.status = set_nz(registers.status, registers.accumulator);
    };
    const auto exclusive_or_accumulator =
        [&registers](const std::uint8_t value) {
            registers.accumulator =
                static_cast<std::uint8_t>(registers.accumulator ^ value);
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
    // The 6280's immediate BIT is not the 65C02's: it takes the negative and
    // overflow flags from the operand as well as the zero flag from the mask.
    const auto bit_immediate = [&registers](const std::uint8_t value) {
        auto status = static_cast<std::uint8_t>(
            registers.status
            & static_cast<std::uint8_t>(~(kZero | kOverflow | kNegative)));
        if ((registers.accumulator & value) == 0U) {
            status = static_cast<std::uint8_t>(status | kZero);
        }
        registers.status = static_cast<std::uint8_t>(
            status
            | static_cast<std::uint8_t>(value & (kOverflow | kNegative)));
    };
    const auto shift_accumulator_left = [&registers]() {
        const auto original = registers.accumulator;
        const auto result = static_cast<std::uint8_t>(original << 1U);
        auto status = static_cast<std::uint8_t>(
            registers.status & static_cast<std::uint8_t>(~kCarry));
        if ((original & kNegative) != 0U) {
            status = static_cast<std::uint8_t>(status | kCarry);
        }
        registers.accumulator = result;
        registers.status = set_nz(status, result);
    };
    const auto increment_memory =
        [&registers, &read_ram, &store](
            const std::uint16_t instruction_pc,
            const std::uint16_t address) {
            const auto result =
                static_cast<std::uint8_t>(read_ram(address) + 1U);
            store(instruction_pc, address, result);
            registers.status = set_nz(registers.status, result);
        };
    const auto decrement_memory =
        [&registers, &read_ram, &store](
            const std::uint16_t instruction_pc,
            const std::uint16_t address) {
            const auto result =
                static_cast<std::uint8_t>(read_ram(address) - 1U);
            store(instruction_pc, address, result);
            registers.status = set_nz(registers.status, result);
        };

    auto pc = entry.routine.logical_pc;
    while (!overflow
        && replacement.checkpoints.size() < kMaximumCheckpoints
        && replacement.work_ram_writes.size() < kMaximumWrites) {
        if (is_return(pc) && depth == 0U) {
            break;
        }
        switch (pc) {
        // --- the table selector and the pass flag ------------------------
        case 0x6966U:  // ASL A -- the selector indexes a pointer pair
            shift_accumulator_left();
            append_checkpoint(0x6966U, 0x6967U, kAccumulator);
            pc = 0x6967U;
            break;
        case 0x6967U:  // TAY
            load_y(registers.accumulator);
            append_checkpoint(0x6967U, 0x6968U, kImplied);
            pc = 0x6968U;
            break;
        case 0x6968U:  // LDA $6A38,Y -- the table's low byte
            load_accumulator(read_mapped(static_cast<std::uint16_t>(
                0x6A38U + static_cast<std::uint16_t>(registers.y))));
            append_checkpoint(0x6968U, 0x696BU, kAbsoluteIndexed);
            pc = 0x696BU;
            break;
        case 0x696BU:  // STA $2F
            store(0x696BU, zero_page(0x2FU), registers.accumulator);
            append_checkpoint(0x696BU, 0x696DU, kZeroPage);
            pc = 0x696DU;
            break;
        case 0x696DU:  // LDA $6A39,Y -- and its high byte
            load_accumulator(read_mapped(static_cast<std::uint16_t>(
                0x6A39U + static_cast<std::uint16_t>(registers.y))));
            append_checkpoint(0x696DU, 0x6970U, kAbsoluteIndexed);
            pc = 0x6970U;
            break;
        case 0x6970U:  // STA $30
            store(0x6970U, zero_page(0x30U), registers.accumulator);
            append_checkpoint(0x6970U, 0x6972U, kZeroPage);
            pc = 0x6972U;
            break;
        // $3880,X holds the actor's step budget with bit 7 set while the path
        // is running. A clear bit 7 means the budget is a repeat count, and the
        // loop below runs the record that many times.
        case 0x6972U:  // LDY $3880,X
            load_y(read_ram(slot_x(0x3880U)));
            append_checkpoint(0x6972U, 0x6975U, kAbsoluteIndexed);
            pc = 0x6975U;
            break;
        case 0x6975U:  // BPL $6989
            pc = branch(
                0x6975U, 0x6989U, 0x6977U,
                (registers.status & kNegative) == 0U);
            break;
        // The running case: the low seven bits are a phase that alternates on
        // every call, so a path advances on one call in two.
        case 0x6977U:  // TYA
            load_accumulator(registers.y);
            append_checkpoint(0x6977U, 0x6978U, kImplied);
            pc = 0x6978U;
            break;
        case 0x6978U:  // AND #$7F
            and_accumulator(0x7FU);
            append_checkpoint(0x6978U, 0x697AU, kImmediate);
            pc = 0x697AU;
            break;
        case 0x697AU:  // EOR #$01 -- toggle the phase, and record whether it
                       // came back to zero
            exclusive_or_accumulator(0x01U);
            append_checkpoint(0x697AU, 0x697CU, kImmediate);
            pc = 0x697CU;
            break;
        case 0x697CU:  // PHP -- the zero flag has to survive the store below
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kMemory));
            push(
                0x697CU,
                static_cast<std::uint8_t>(registers.status | kBreak));
            append_checkpoint(0x697CU, 0x697DU, kPush);
            pc = 0x697DU;
            break;
        case 0x697DU:  // ORA #$80
            or_accumulator(0x80U);
            append_checkpoint(0x697DU, 0x697FU, kImmediate);
            pc = 0x697FU;
            break;
        case 0x697FU:  // STA $3880,X
            store(0x697FU, slot_x(0x3880U), registers.accumulator);
            append_checkpoint(0x697FU, 0x6982U, kAbsoluteIndexed);
            pc = 0x6982U;
            break;
        case 0x6982U:  // PLP
            registers.status = pull();
            append_checkpoint(0x6982U, 0x6983U, kPull);
            pc = 0x6983U;
            break;
        case 0x6983U:  // BEQ $6987 -- the phase is odd, so this call is a rest
            pc = branch(
                0x6983U, 0x6987U, 0x6985U,
                (registers.status & kZero) != 0U);
            break;
        case 0x6985U:  // BRA $6995 -- and this one steps
            append_checkpoint(0x6985U, 0x6995U, kBranchAlways);
            pc = 0x6995U;
            break;
        case 0x6987U:  // CLC -- a rest reports no contact
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0x6987U, 0x6988U, kImplied);
            pc = 0x6988U;
            break;
        // --- the repeat loop, ten executions on the whole route -----------
        case 0x6989U:  // PHY
            push(0x6989U, registers.y);
            append_checkpoint(0x6989U, 0x698AU, kPush);
            pc = 0x698AU;
            break;
        case 0x698AU:  // JSR $6995 -- the pushed address is the last operand
                       // byte, and both halves land before the call is paid for
            push(0x698AU, 0x69U);
            push(0x698AU, 0x8CU);
            commit_pending_writes(cycle);
            ++depth;
            append_checkpoint(0x698AU, 0x6995U, kJumpToSubroutine);
            pc = 0x6995U;
            break;
        case 0x698DU:  // PLY
            load_y(pull());
            append_checkpoint(0x698DU, 0x698EU, kPull);
            pc = 0x698EU;
            break;
        case 0x698EU:  // BCS $6994 -- a contact ends the repeat early
            pc = branch(
                0x698EU, 0x6994U, 0x6990U,
                (registers.status & kCarry) != 0U);
            break;
        case 0x6990U:  // DEY
            load_y(static_cast<std::uint8_t>(registers.y - 1U));
            append_checkpoint(0x6990U, 0x6991U, kImplied);
            pc = 0x6991U;
            break;
        case 0x6991U:  // BNE $6989
            pc = branch(
                0x6991U, 0x6989U, 0x6993U,
                (registers.status & kZero) == 0U);
            break;
        case 0x6993U:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0x6993U, 0x6994U, kImplied);
            pc = 0x6994U;
            break;
        // --- one step of the path ----------------------------------------
        case 0x6995U:  // STZ $35 -- the header flag for this step
            store(0x6995U, zero_page(0x35U), 0U);
            append_checkpoint(0x6995U, 0x6997U, kZeroPage);
            pc = 0x6997U;
            break;
        case 0x6997U:  // LDY $3E00,X -- the cursor into the table
            load_y(read_ram(slot_x(0x3E00U)));
            append_checkpoint(0x6997U, 0x699AU, kAbsoluteIndexed);
            pc = 0x699AU;
            break;
        case 0x699AU:  // LDA ($2F),Y
            load_accumulator(read_mapped(static_cast<std::uint16_t>(
                indirect(0x2FU) + static_cast<std::uint16_t>(registers.y))));
            append_checkpoint(0x699AU, 0x699CU, kIndirectIndexed);
            pc = 0x699CU;
            break;
        case 0x699CU:  // CMP #$FF -- the table's terminator
            compare_accumulator(0xFFU);
            append_checkpoint(0x699CU, 0x699EU, kImmediate);
            pc = 0x699EU;
            break;
        case 0x699EU:  // BNE $69A2
            pc = branch(
                0x699EU, 0x69A2U, 0x69A0U,
                (registers.status & kZero) == 0U);
            break;
        case 0x69A0U:  // SEC -- a finished path reports contact
            registers.status =
                static_cast<std::uint8_t>(registers.status | kCarry);
            append_checkpoint(0x69A0U, 0x69A1U, kImplied);
            pc = 0x69A1U;
            break;
        case 0x69A2U:  // CMP #$FE -- the table's rewind marker
            compare_accumulator(0xFEU);
            append_checkpoint(0x69A2U, 0x69A4U, kImmediate);
            pc = 0x69A4U;
            break;
        case 0x69A4U:  // BNE $69AE
            pc = branch(
                0x69A4U, 0x69AEU, 0x69A6U,
                (registers.status & kZero) == 0U);
            break;
        case 0x69A6U:  // STZ $3E00,X -- rewind both cursors and re-read
            store(0x69A6U, slot_x(0x3E00U), 0U);
            append_checkpoint(0x69A6U, 0x69A9U, kAbsoluteIndexed);
            pc = 0x69A9U;
            break;
        case 0x69A9U:  // STZ $3E80,X
            store(0x69A9U, slot_x(0x3E80U), 0U);
            append_checkpoint(0x69A9U, 0x69ACU, kAbsoluteIndexed);
            pc = 0x69ACU;
            break;
        case 0x69ACU:  // LDA ($2F)
            load_accumulator(read_mapped(indirect(0x2FU)));
            append_checkpoint(0x69ACU, 0x69AEU, kIndirect);
            pc = 0x69AEU;
            break;
        // Bit 7 of a table entry marks the record as reversed; the flag is kept
        // in $35 for the direction fix-up further down.
        case 0x69AEU:  // BIT #$80
            bit_immediate(0x80U);
            append_checkpoint(0x69AEU, 0x69B0U, kImmediate);
            pc = 0x69B0U;
            break;
        case 0x69B0U:  // BEQ $69B6
            pc = branch(
                0x69B0U, 0x69B6U, 0x69B2U,
                (registers.status & kZero) != 0U);
            break;
        case 0x69B2U:  // SMB0 $35
            store(
                0x69B2U,
                zero_page(0x35U),
                static_cast<std::uint8_t>(read_ram(zero_page(0x35U)) | 0x01U));
            append_checkpoint(0x69B2U, 0x69B4U, kSetMemoryBit);
            pc = 0x69B4U;
            break;
        case 0x69B4U:  // AND #$7F
            and_accumulator(0x7FU);
            append_checkpoint(0x69B4U, 0x69B6U, kImmediate);
            pc = 0x69B6U;
            break;
        // --- the motion record the table entry names ----------------------
        case 0x69B6U:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0x69B6U, 0x69B7U, kAccumulator);
            pc = 0x69B7U;
            break;
        case 0x69B7U:  // TAY
            load_y(registers.accumulator);
            append_checkpoint(0x69B7U, 0x69B8U, kImplied);
            pc = 0x69B8U;
            break;
        case 0x69B8U:  // LDA $6C3A,Y
            load_accumulator(read_mapped(static_cast<std::uint16_t>(
                0x6C3AU + static_cast<std::uint16_t>(registers.y))));
            append_checkpoint(0x69B8U, 0x69BBU, kAbsoluteIndexed);
            pc = 0x69BBU;
            break;
        case 0x69BBU:  // STA $2D
            store(0x69BBU, zero_page(0x2DU), registers.accumulator);
            append_checkpoint(0x69BBU, 0x69BDU, kZeroPage);
            pc = 0x69BDU;
            break;
        case 0x69BDU:  // LDA $6C3B,Y
            load_accumulator(read_mapped(static_cast<std::uint16_t>(
                0x6C3BU + static_cast<std::uint16_t>(registers.y))));
            append_checkpoint(0x69BDU, 0x69C0U, kAbsoluteIndexed);
            pc = 0x69C0U;
            break;
        case 0x69C0U:  // STA $2E
            store(0x69C0U, zero_page(0x2EU), registers.accumulator);
            append_checkpoint(0x69C0U, 0x69C2U, kZeroPage);
            pc = 0x69C2U;
            break;
        case 0x69C2U:  // LDY $3E80,X -- the cursor into the record
            load_y(read_ram(slot_x(0x3E80U)));
            append_checkpoint(0x69C2U, 0x69C5U, kAbsoluteIndexed);
            pc = 0x69C5U;
            break;
        case 0x69C5U:  // LDA ($2D),Y
            load_accumulator(read_mapped(static_cast<std::uint16_t>(
                indirect(0x2DU) + static_cast<std::uint16_t>(registers.y))));
            append_checkpoint(0x69C5U, 0x69C7U, kIndirectIndexed);
            pc = 0x69C7U;
            break;
        case 0x69C7U:  // CMP #$C0 -- the record's terminator
            compare_accumulator(0xC0U);
            append_checkpoint(0x69C7U, 0x69C9U, kImmediate);
            pc = 0x69C9U;
            break;
        case 0x69C9U:  // BNE $69D3
            pc = branch(
                0x69C9U, 0x69D3U, 0x69CBU,
                (registers.status & kZero) == 0U);
            break;
        case 0x69CBU:  // INC $3E00,X -- next table entry, and start it over
            increment_memory(0x69CBU, slot_x(0x3E00U));
            append_checkpoint(
                0x69CBU, 0x69CEU, kAbsoluteIndexedReadModifyWrite);
            pc = 0x69CEU;
            break;
        case 0x69CEU:  // STZ $3E80,X
            store(0x69CEU, slot_x(0x3E80U), 0U);
            append_checkpoint(0x69CEU, 0x69D1U, kAbsoluteIndexed);
            pc = 0x69D1U;
            break;
        case 0x69D1U:  // BRA $6995
            append_checkpoint(0x69D1U, 0x6995U, kBranchAlways);
            pc = 0x6995U;
            break;
        // A record byte with bit 7 set carries a sprite frame in its low seven
        // bits, and the step itself is in the byte after it.
        case 0x69D3U:  // BIT #$80
            bit_immediate(0x80U);
            append_checkpoint(0x69D3U, 0x69D5U, kImmediate);
            pc = 0x69D5U;
            break;
        case 0x69D5U:  // BEQ $69F7
            pc = branch(
                0x69D5U, 0x69F7U, 0x69D7U,
                (registers.status & kZero) != 0U);
            break;
        case 0x69D7U:  // AND #$7F
            and_accumulator(0x7FU);
            append_checkpoint(0x69D7U, 0x69D9U, kImmediate);
            pc = 0x69D9U;
            break;
        case 0x69D9U:  // STA $3A80,X -- the actor's frame
            store(0x69D9U, slot_x(0x3A80U), registers.accumulator);
            append_checkpoint(0x69D9U, 0x69DCU, kAbsoluteIndexed);
            pc = 0x69DCU;
            break;
        // $3D00,X is the actor's horizontal mirror. A reversed table entry
        // clears it, an ordinary one sets it.
        case 0x69DCU:  // BBR0 $35,$69E9
            pc = (read_ram(zero_page(0x35U)) & 0x01U) == 0U ? 0x69E9U : 0x69DFU;
            append_checkpoint(
                0x69DCU, pc,
                pc == 0x69E9U ? kBitBranchTaken : kBitBranchNotTaken);
            break;
        case 0x69DFU:  // LDA $3D00,X
            load_accumulator(read_ram(slot_x(0x3D00U)));
            append_checkpoint(0x69DFU, 0x69E2U, kAbsoluteIndexed);
            pc = 0x69E2U;
            break;
        case 0x69E2U:  // AND #$7F
            and_accumulator(0x7FU);
            append_checkpoint(0x69E2U, 0x69E4U, kImmediate);
            pc = 0x69E4U;
            break;
        case 0x69E4U:  // STA $3D00,X
            store(0x69E4U, slot_x(0x3D00U), registers.accumulator);
            append_checkpoint(0x69E4U, 0x69E7U, kAbsoluteIndexed);
            pc = 0x69E7U;
            break;
        case 0x69E7U:  // BRA $69F1
            append_checkpoint(0x69E7U, 0x69F1U, kBranchAlways);
            pc = 0x69F1U;
            break;
        case 0x69E9U:  // LDA $3D00,X
            load_accumulator(read_ram(slot_x(0x3D00U)));
            append_checkpoint(0x69E9U, 0x69ECU, kAbsoluteIndexed);
            pc = 0x69ECU;
            break;
        case 0x69ECU:  // ORA #$80
            or_accumulator(0x80U);
            append_checkpoint(0x69ECU, 0x69EEU, kImmediate);
            pc = 0x69EEU;
            break;
        case 0x69EEU:  // STA $3D00,X
            store(0x69EEU, slot_x(0x3D00U), registers.accumulator);
            append_checkpoint(0x69EEU, 0x69F1U, kAbsoluteIndexed);
            pc = 0x69F1U;
            break;
        case 0x69F1U:  // INC $3E80,X -- the frame byte is consumed
            increment_memory(0x69F1U, slot_x(0x3E80U));
            append_checkpoint(
                0x69F1U, 0x69F4U, kAbsoluteIndexedReadModifyWrite);
            pc = 0x69F4U;
            break;
        case 0x69F4U:  // INY
            load_y(static_cast<std::uint8_t>(registers.y + 1U));
            append_checkpoint(0x69F4U, 0x69F5U, kImplied);
            pc = 0x69F5U;
            break;
        case 0x69F5U:  // LDA ($2D),Y
            load_accumulator(read_mapped(static_cast<std::uint16_t>(
                indirect(0x2DU) + static_cast<std::uint16_t>(registers.y))));
            append_checkpoint(0x69F5U, 0x69F7U, kIndirectIndexed);
            pc = 0x69F7U;
            break;
        // --- the step itself: two bits of horizontal, two of vertical ------
        case 0x69F7U:  // AND #$60
            and_accumulator(0x60U);
            append_checkpoint(0x69F7U, 0x69F9U, kImmediate);
            pc = 0x69F9U;
            break;
        case 0x69F9U:  // BEQ $6A14 -- no horizontal component
            pc = branch(
                0x69F9U, 0x6A14U, 0x69FBU,
                (registers.status & kZero) != 0U);
            break;
        case 0x69FBU:  // CMP #$20
            compare_accumulator(0x20U);
            append_checkpoint(0x69FBU, 0x69FDU, kImmediate);
            pc = 0x69FDU;
            break;
        case 0x69FDU:  // BNE $6A09
            pc = branch(
                0x69FDU, 0x6A09U, 0x69FFU,
                (registers.status & kZero) == 0U);
            break;
        case 0x69FFU:  // INC $3180,X -- one unit right
            increment_memory(0x69FFU, slot_x(0x3180U));
            append_checkpoint(
                0x69FFU, 0x6A02U, kAbsoluteIndexedReadModifyWrite);
            pc = 0x6A02U;
            break;
        case 0x6A02U:  // BNE $6A07
            pc = branch(
                0x6A02U, 0x6A07U, 0x6A04U,
                (registers.status & kZero) == 0U);
            break;
        case 0x6A04U:  // INC $3200,X -- the carry into the high byte
            increment_memory(0x6A04U, slot_x(0x3200U));
            append_checkpoint(
                0x6A04U, 0x6A07U, kAbsoluteIndexedReadModifyWrite);
            pc = 0x6A07U;
            break;
        case 0x6A07U:  // BRA $6A14
            append_checkpoint(0x6A07U, 0x6A14U, kBranchAlways);
            pc = 0x6A14U;
            break;
        case 0x6A09U:  // LDA $3180,X -- one unit left, borrow first
            load_accumulator(read_ram(slot_x(0x3180U)));
            append_checkpoint(0x6A09U, 0x6A0CU, kAbsoluteIndexed);
            pc = 0x6A0CU;
            break;
        case 0x6A0CU:  // BNE $6A11
            pc = branch(
                0x6A0CU, 0x6A11U, 0x6A0EU,
                (registers.status & kZero) == 0U);
            break;
        case 0x6A0EU:  // DEC $3200,X
            decrement_memory(0x6A0EU, slot_x(0x3200U));
            append_checkpoint(
                0x6A0EU, 0x6A11U, kAbsoluteIndexedReadModifyWrite);
            pc = 0x6A11U;
            break;
        case 0x6A11U:  // DEC $3180,X
            decrement_memory(0x6A11U, slot_x(0x3180U));
            append_checkpoint(
                0x6A11U, 0x6A14U, kAbsoluteIndexedReadModifyWrite);
            pc = 0x6A14U;
            break;
        case 0x6A14U:  // LDA ($2D),Y -- the same byte, read again for the
                       // vertical pair
            load_accumulator(read_mapped(static_cast<std::uint16_t>(
                indirect(0x2DU) + static_cast<std::uint16_t>(registers.y))));
            append_checkpoint(0x6A14U, 0x6A16U, kIndirectIndexed);
            pc = 0x6A16U;
            break;
        case 0x6A16U:  // AND #$18
            and_accumulator(0x18U);
            append_checkpoint(0x6A16U, 0x6A18U, kImmediate);
            pc = 0x6A18U;
            break;
        case 0x6A18U:  // BEQ $6A33 -- no vertical component
            pc = branch(
                0x6A18U, 0x6A33U, 0x6A1AU,
                (registers.status & kZero) != 0U);
            break;
        case 0x6A1AU:  // CMP #$08
            compare_accumulator(0x08U);
            append_checkpoint(0x6A1AU, 0x6A1CU, kImmediate);
            pc = 0x6A1CU;
            break;
        case 0x6A1CU:  // BNE $6A28
            pc = branch(
                0x6A1CU, 0x6A28U, 0x6A1EU,
                (registers.status & kZero) == 0U);
            break;
        case 0x6A1EU:  // INC $3300,X -- one unit down
            increment_memory(0x6A1EU, slot_x(0x3300U));
            append_checkpoint(
                0x6A1EU, 0x6A21U, kAbsoluteIndexedReadModifyWrite);
            pc = 0x6A21U;
            break;
        case 0x6A21U:  // BNE $6A26
            pc = branch(
                0x6A21U, 0x6A26U, 0x6A23U,
                (registers.status & kZero) == 0U);
            break;
        case 0x6A23U:  // INC $3380,X
            increment_memory(0x6A23U, slot_x(0x3380U));
            append_checkpoint(
                0x6A23U, 0x6A26U, kAbsoluteIndexedReadModifyWrite);
            pc = 0x6A26U;
            break;
        case 0x6A26U:  // BRA $6A33
            append_checkpoint(0x6A26U, 0x6A33U, kBranchAlways);
            pc = 0x6A33U;
            break;
        case 0x6A28U:  // LDA $3300,X -- one unit up, borrow first
            load_accumulator(read_ram(slot_x(0x3300U)));
            append_checkpoint(0x6A28U, 0x6A2BU, kAbsoluteIndexed);
            pc = 0x6A2BU;
            break;
        case 0x6A2BU:  // BNE $6A30
            pc = branch(
                0x6A2BU, 0x6A30U, 0x6A2DU,
                (registers.status & kZero) == 0U);
            break;
        case 0x6A2DU:  // DEC $3380,X
            decrement_memory(0x6A2DU, slot_x(0x3380U));
            append_checkpoint(
                0x6A2DU, 0x6A30U, kAbsoluteIndexedReadModifyWrite);
            pc = 0x6A30U;
            break;
        case 0x6A30U:  // DEC $3300,X
            decrement_memory(0x6A30U, slot_x(0x3300U));
            append_checkpoint(
                0x6A30U, 0x6A33U, kAbsoluteIndexedReadModifyWrite);
            pc = 0x6A33U;
            break;
        case 0x6A33U:  // INC $3E80,X -- the step byte is consumed
            increment_memory(0x6A33U, slot_x(0x3E80U));
            append_checkpoint(
                0x6A33U, 0x6A36U, kAbsoluteIndexedReadModifyWrite);
            pc = 0x6A36U;
            break;
        case 0x6A36U:  // CLC -- a step that landed reports no contact
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0x6A36U, 0x6A37U, kImplied);
            pc = 0x6A37U;
            break;
        // An inner return: the repeat loop's own frame, not the caller's.
        case 0x6988U:
        case 0x6994U:
        case 0x69A1U:
        case 0x6A37U: {
            const auto low = pull();
            const auto high = pull();
            const auto resume = static_cast<std::uint16_t>(
                (low | static_cast<std::uint16_t>(high) << 8U) + 1U);
            --depth;
            append_checkpoint(pc, resume, kReturnFromSubroutine);
            pc = resume;
            break;
        }

        default:
            return std::nullopt;
        }
    }

    if (overflow || !is_return(pc) || depth != 0U
        || replacement.checkpoints.empty()
        || replacement.checkpoints.size() >= kMaximumCheckpoints
        || replacement.work_ram_writes.size() >= kMaximumWrites) {
        return std::nullopt;
    }

    replacement.return_instruction = {
        .physical_bank = kBank,
        .logical_pc = pc,
    };
    registers.program_counter = pc;
    replacement.output_registers = registers;
    replacement.body_master_cycles = cycle;
    return replacement;
}

} // namespace devils_crush::app::detail
