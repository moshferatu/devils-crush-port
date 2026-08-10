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

// The Sorcerer per-frame tick in physical bank 02: `02:586F`-`02:5903`, the
// largest block of unported gameplay left on the census route and the only one
// of its size with no native entry anywhere inside it. Its deployment and
// staging heads at 02:57F8 and 02:5808 are already native; this is the three
// states that follow, for the eight Sorcerers in slots $17-$1E.
//
// One routine in three pieces, split by the two calls it makes to the
// collision-table interpreter at 03:6966:
//
//   02:586F  the staggered deployment delay. `$3B00,X` and `$3B80,X` are a
//            sixteen-bit countdown; while either byte is set the tick
//            decrements it and returns, which is 1,240 of the entries on this
//            route and all but eight of them take that arm.
//
//   02:5886  the arrival state. It flips bit 5 of the actor's attribute byte at
//            `$3080,X` and asks the interpreter for record 6. A set carry --
//            the table reached its terminator -- runs the reset at 02:5896:
//            clear bits 5 and 6 of the attributes, seed the table cursors at
//            `$3E00,X`/`$3E80,X` with 5 or 12 depending on whether the actor
//            type in `$3780,X` is $28, set the pass count at `$3C00,X` to 2,
//            clear the high bit of `$3800,X`, and decrement the population
//            counter at `$2D10`.
//
//   02:58C2  orbit, entered 18,152 times and worth 3.0M master cycles on its
//            own -- the state every Sorcerer sits in once all eight have
//            arrived. When the live counter has reached zero it
//            derives a table index from the difficulty byte at `$2D05` --
//            eight less it, masked to three bits, halved -- and reads a
//            four-entry table at `$593C` in this bank's own ROM page into
//            `$3880,X`. Either way it loads the actor type and calls the
//            interpreter again; 02:58DD copies the event the interpreter
//            recorded in `$3A80,X` to `$3980,X` and tests `$3800,X`.
//
// Five entry points. Two are routine heads the core arrives at directly, and
// three -- 02:5893, 02:58C2 and 02:58DD -- are the addresses it arrives at
// after one of the two interpreter calls, because a transcript stops in front
// of a call. 02:5896 is hookable on its own and is not an entry here: the walk
// from 02:5893 takes its own branch into it, so registering it would leave a
// routine the deployed registry never enters.
//
// The family reads `$2D05`, `$2D10`, `$3780,X`, `$3800,X`, `$3A80,X` and the
// cooldown pair, and writes `$3080,X`, `$3880,X`, `$3980,X`, `$3C00,X`,
// `$3E00,X`, `$3E80,X` and `$2D10`. Shared state was measured on exact windows
// from observers on all six heads: of the ninety-nine instructions that write
// any of those addresses anywhere on the route, exactly one ever writes inside
// a window, and it is 03:69D9 -- the interpreter's own event store, which is
// this family's callee. Every other writer lands inside a window on 0.0% of
// its executions. Nothing in the timer driver, the music sequencer or either
// video bank touches any of it.
constexpr std::uint8_t kBank = 0x02U;

constexpr std::uint8_t kCarry = 0x01U;
constexpr std::uint8_t kZero = 0x02U;
constexpr std::uint8_t kDecimal = 0x08U;
constexpr std::uint8_t kMemory = 0x20U;
constexpr std::uint8_t kOverflow = 0x40U;
constexpr std::uint8_t kNegative = 0x80U;

constexpr std::uint16_t kWorkRamBase = 0x2000U;
constexpr std::uint16_t kWorkRamEnd = 0x4000U;

constexpr std::uint32_t kImplied = 6U;
constexpr std::uint32_t kImmediate = 6U;
constexpr std::uint32_t kAccumulator = 6U;
constexpr std::uint32_t kAbsolute = 15U;
constexpr std::uint32_t kAbsoluteIndexed = 15U;
// DEC and INC on an absolute operand, indexed or not, are the same seven-CPU-
// cycle read-modify-write.
constexpr std::uint32_t kAbsoluteReadModifyWrite = 21U;
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

[[nodiscard]] constexpr bool is_owned_entry(const std::uint16_t logical_pc)
{
    return logical_pc == 0x586FU || logical_pc == 0x5886U
        || logical_pc == 0x5893U || logical_pc == 0x58C2U
        || logical_pc == 0x58DDU;
}

// Where each walk stops: the two `RTS` instructions at 02:5882 and 02:5895,
// the final `RTS` at 02:5903, the three calls this family makes -- 02:5883 and
// 02:58BF to the bank-01 wrapper at $C018 and 02:5890 and 02:58DA to the
// interpreter -- and 02:58E8, the arm of the `BPL` at 02:58E6 that this route
// never takes. Handing that arm back costs nothing and keeps the walk honest
// about a path it has never seen.
[[nodiscard]] constexpr bool is_stop_instruction(const std::uint16_t logical_pc)
{
    return logical_pc == 0x5882U || logical_pc == 0x5883U
        || logical_pc == 0x5890U || logical_pc == 0x5895U
        || logical_pc == 0x58BFU || logical_pc == 0x58DAU
        || logical_pc == 0x58E8U || logical_pc == 0x5903U;
}

} // namespace

std::optional<RoutineReplacement> replace_sorcerer_tick(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    if (entry.routine.physical_bank != kBank
        || !is_owned_entry(entry.routine.logical_pc)
        || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    // The subtraction at 02:58CA is binary here, and the memory-operand T flag
    // would redirect the accumulator through zero page for it and for every
    // AND, ORA and EOR below.
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

    // The four-entry table at 02:593C sits in this bank's own ROM page, so it
    // is resolved from the live MPRs rather than assumed. The hardware page
    // fails the bank test, which is what makes a device read decline rather
    // than fabricate a value.
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

    // Every absolute operand this family touches is indexed by the actor slot.
    const auto slot = [&registers](const std::uint16_t base) {
        return static_cast<std::uint16_t>(
            base + static_cast<std::uint16_t>(registers.x));
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
    // SBC with the carry standing in for "no borrow", in binary only.
    const auto subtract_with_carry = [&registers](const std::uint8_t value) {
        const auto left = registers.accumulator;
        const auto borrow =
            static_cast<std::uint16_t>((registers.status & kCarry) != 0U ? 0U : 1U);
        const auto wide = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(left)
            - static_cast<std::uint16_t>(value) - borrow);
        const auto result = static_cast<std::uint8_t>(wide);
        auto status = static_cast<std::uint8_t>(
            registers.status
            & static_cast<std::uint8_t>(~(kCarry | kOverflow)));
        if (static_cast<std::uint16_t>(left)
            >= static_cast<std::uint16_t>(value) + borrow) {
            status = static_cast<std::uint8_t>(status | kCarry);
        }
        if ((static_cast<std::uint8_t>(left ^ result)
             & static_cast<std::uint8_t>(left ^ value) & kNegative)
            != 0U) {
            status = static_cast<std::uint8_t>(status | kOverflow);
        }
        registers.accumulator = result;
        registers.status = set_nz(status, result);
    };
    const auto compare_y = [&registers](const std::uint8_t value) {
        const auto result = static_cast<std::uint8_t>(registers.y - value);
        auto status = static_cast<std::uint8_t>(
            registers.status & static_cast<std::uint8_t>(~kCarry));
        if (registers.y >= value) {
            status = static_cast<std::uint8_t>(status | kCarry);
        }
        registers.status = set_nz(status, result);
    };
    const auto shift_accumulator_right = [&registers]() {
        const auto original = registers.accumulator;
        const auto result = static_cast<std::uint8_t>(original >> 1U);
        auto status = static_cast<std::uint8_t>(
            registers.status & static_cast<std::uint8_t>(~kCarry));
        if ((original & 0x01U) != 0U) {
            status = static_cast<std::uint8_t>(status | kCarry);
        }
        registers.accumulator = result;
        registers.status = set_nz(status, result);
    };
    // DEC on memory reports on the result and leaves the carry alone.
    const auto decrement_memory =
        [&registers, &read_ram, &store](
            const std::uint16_t instruction_pc,
            const std::uint16_t address) {
            const auto value =
                static_cast<std::uint8_t>(read_ram(address) - 1U);
            store(instruction_pc, address, value);
            registers.status = set_nz(registers.status, value);
        };

    auto pc = entry.routine.logical_pc;
    while (!is_stop_instruction(pc) && !overflow
        && replacement.checkpoints.size() < 64U) {
        switch (pc) {
        case 0x586FU:  // LDA $3B00,X
            load_accumulator(read_ram(slot(0x3B00U)));
            append_checkpoint(0x586FU, 0x5872U, kAbsoluteIndexed);
            pc = 0x5872U;
            break;
        case 0x5872U:  // ORA $3B80,X
            or_accumulator(read_ram(slot(0x3B80U)));
            append_checkpoint(0x5872U, 0x5875U, kAbsoluteIndexed);
            pc = 0x5875U;
            break;
        case 0x5875U:  // BEQ $5883
            pc = branch(
                0x5875U, 0x5883U, 0x5877U,
                (registers.status & kZero) != 0U);
            break;
        case 0x5877U:  // LDA $3B00,X
            load_accumulator(read_ram(slot(0x3B00U)));
            append_checkpoint(0x5877U, 0x587AU, kAbsoluteIndexed);
            pc = 0x587AU;
            break;
        case 0x587AU:  // BNE $587F
            pc = branch(
                0x587AU, 0x587FU, 0x587CU,
                (registers.status & kZero) == 0U);
            break;
        case 0x587CU:  // DEC $3B80,X -- the high byte borrows only when the
                       // low byte is already zero.
            decrement_memory(0x587CU, slot(0x3B80U));
            append_checkpoint(0x587CU, 0x587FU, kAbsoluteReadModifyWrite);
            pc = 0x587FU;
            break;
        case 0x587FU:  // DEC $3B00,X
            decrement_memory(0x587FU, slot(0x3B00U));
            append_checkpoint(0x587FU, 0x5882U, kAbsoluteReadModifyWrite);
            pc = 0x5882U;
            break;

        case 0x5886U:  // LDA $3080,X
            load_accumulator(read_ram(slot(0x3080U)));
            append_checkpoint(0x5886U, 0x5889U, kAbsoluteIndexed);
            pc = 0x5889U;
            break;
        case 0x5889U:  // EOR #$20
            exclusive_or_accumulator(0x20U);
            append_checkpoint(0x5889U, 0x588BU, kImmediate);
            pc = 0x588BU;
            break;
        case 0x588BU:  // STA $3080,X
            store(0x588BU, slot(0x3080U), registers.accumulator);
            append_checkpoint(0x588BU, 0x588EU, kAbsoluteIndexed);
            pc = 0x588EU;
            break;
        case 0x588EU:  // LDA #$06 -- the interpreter's record selector
            load_accumulator(0x06U);
            append_checkpoint(0x588EU, 0x5890U, kImmediate);
            pc = 0x5890U;
            break;

        case 0x5893U:  // BCS $5896 -- a set carry means the table terminated
            pc = branch(
                0x5893U, 0x5896U, 0x5895U,
                (registers.status & kCarry) != 0U);
            break;
        case 0x5896U:  // LDA $3080,X
            load_accumulator(read_ram(slot(0x3080U)));
            append_checkpoint(0x5896U, 0x5899U, kAbsoluteIndexed);
            pc = 0x5899U;
            break;
        case 0x5899U:  // AND #$9F -- clear the two dispatch bits
            and_accumulator(0x9FU);
            append_checkpoint(0x5899U, 0x589BU, kImmediate);
            pc = 0x589BU;
            break;
        case 0x589BU:  // STA $3080,X
            store(0x589BU, slot(0x3080U), registers.accumulator);
            append_checkpoint(0x589BU, 0x589EU, kAbsoluteIndexed);
            pc = 0x589EU;
            break;
        case 0x589EU:  // LDA #$05
            load_accumulator(0x05U);
            append_checkpoint(0x589EU, 0x58A0U, kImmediate);
            pc = 0x58A0U;
            break;
        case 0x58A0U:  // LDY $3780,X
            load_y(read_ram(slot(0x3780U)));
            append_checkpoint(0x58A0U, 0x58A3U, kAbsoluteIndexed);
            pc = 0x58A3U;
            break;
        case 0x58A3U:  // CPY #$28
            compare_y(0x28U);
            append_checkpoint(0x58A3U, 0x58A5U, kImmediate);
            pc = 0x58A5U;
            break;
        case 0x58A5U:  // BEQ $58A9
            pc = branch(
                0x58A5U, 0x58A9U, 0x58A7U,
                (registers.status & kZero) != 0U);
            break;
        case 0x58A7U:  // LDA #$0C
            load_accumulator(0x0CU);
            append_checkpoint(0x58A7U, 0x58A9U, kImmediate);
            pc = 0x58A9U;
            break;
        case 0x58A9U:  // STA $3E00,X -- seed the table cursor
            store(0x58A9U, slot(0x3E00U), registers.accumulator);
            append_checkpoint(0x58A9U, 0x58ACU, kAbsoluteIndexed);
            pc = 0x58ACU;
            break;
        case 0x58ACU:  // STZ $3E80,X
            store(0x58ACU, slot(0x3E80U), 0x00U);
            append_checkpoint(0x58ACU, 0x58AFU, kAbsoluteIndexed);
            pc = 0x58AFU;
            break;
        case 0x58AFU:  // LDA #$02
            load_accumulator(0x02U);
            append_checkpoint(0x58AFU, 0x58B1U, kImmediate);
            pc = 0x58B1U;
            break;
        case 0x58B1U:  // STA $3C00,X -- two passes
            store(0x58B1U, slot(0x3C00U), registers.accumulator);
            append_checkpoint(0x58B1U, 0x58B4U, kAbsoluteIndexed);
            pc = 0x58B4U;
            break;
        case 0x58B4U:  // LDA $3800,X
            load_accumulator(read_ram(slot(0x3800U)));
            append_checkpoint(0x58B4U, 0x58B7U, kAbsoluteIndexed);
            pc = 0x58B7U;
            break;
        case 0x58B7U:  // AND #$7F
            and_accumulator(0x7FU);
            append_checkpoint(0x58B7U, 0x58B9U, kImmediate);
            pc = 0x58B9U;
            break;
        case 0x58B9U:  // STA $3800,X
            store(0x58B9U, slot(0x3800U), registers.accumulator);
            append_checkpoint(0x58B9U, 0x58BCU, kAbsoluteIndexed);
            pc = 0x58BCU;
            break;
        case 0x58BCU:  // DEC $2D10 -- one fewer live actor
            decrement_memory(0x58BCU, 0x2D10U);
            append_checkpoint(0x58BCU, 0x58BFU, kAbsoluteReadModifyWrite);
            pc = 0x58BFU;
            break;

        case 0x58C2U:  // LDA $2D10
            load_accumulator(read_ram(0x2D10U));
            append_checkpoint(0x58C2U, 0x58C5U, kAbsolute);
            pc = 0x58C5U;
            break;
        case 0x58C5U:  // BNE $58D7 -- skip the reseed while actors remain
            pc = branch(
                0x58C5U, 0x58D7U, 0x58C7U,
                (registers.status & kZero) == 0U);
            break;
        case 0x58C7U:  // SEC
            registers.status =
                static_cast<std::uint8_t>(registers.status | kCarry);
            append_checkpoint(0x58C7U, 0x58C8U, kImplied);
            pc = 0x58C8U;
            break;
        case 0x58C8U:  // LDA #$08
            load_accumulator(0x08U);
            append_checkpoint(0x58C8U, 0x58CAU, kImmediate);
            pc = 0x58CAU;
            break;
        case 0x58CAU:  // SBC $2D05
            subtract_with_carry(read_ram(0x2D05U));
            append_checkpoint(0x58CAU, 0x58CDU, kAbsolute);
            pc = 0x58CDU;
            break;
        case 0x58CDU:  // AND #$07
            and_accumulator(0x07U);
            append_checkpoint(0x58CDU, 0x58CFU, kImmediate);
            pc = 0x58CFU;
            break;
        case 0x58CFU:  // LSR A -- four table entries, two difficulties each
            shift_accumulator_right();
            append_checkpoint(0x58CFU, 0x58D0U, kAccumulator);
            pc = 0x58D0U;
            break;
        case 0x58D0U:  // TAY
            load_y(registers.accumulator);
            append_checkpoint(0x58D0U, 0x58D1U, kImplied);
            pc = 0x58D1U;
            break;
        case 0x58D1U:  // LDA $593C,Y -- this bank's own ROM page
            load_accumulator(read_mapped(
                static_cast<std::uint16_t>(
                    0x593CU + static_cast<std::uint16_t>(registers.y))));
            append_checkpoint(0x58D1U, 0x58D4U, kAbsoluteIndexed);
            pc = 0x58D4U;
            break;
        case 0x58D4U:  // STA $3880,X
            store(0x58D4U, slot(0x3880U), registers.accumulator);
            append_checkpoint(0x58D4U, 0x58D7U, kAbsoluteIndexed);
            pc = 0x58D7U;
            break;
        case 0x58D7U:  // LDA $3780,X -- the actor type the interpreter wants
            load_accumulator(read_ram(slot(0x3780U)));
            append_checkpoint(0x58D7U, 0x58DAU, kAbsoluteIndexed);
            pc = 0x58DAU;
            break;

        case 0x58DDU:  // LDA $3A80,X -- the event the interpreter recorded
            load_accumulator(read_ram(slot(0x3A80U)));
            append_checkpoint(0x58DDU, 0x58E0U, kAbsoluteIndexed);
            pc = 0x58E0U;
            break;
        case 0x58E0U:  // STA $3980,X
            store(0x58E0U, slot(0x3980U), registers.accumulator);
            append_checkpoint(0x58E0U, 0x58E3U, kAbsoluteIndexed);
            pc = 0x58E3U;
            break;
        case 0x58E3U:  // LDA $3800,X
            load_accumulator(read_ram(slot(0x3800U)));
            append_checkpoint(0x58E3U, 0x58E6U, kAbsoluteIndexed);
            pc = 0x58E6U;
            break;
        case 0x58E6U:  // BPL $5903
            pc = branch(
                0x58E6U, 0x5903U, 0x58E8U,
                (registers.status & kNegative) == 0U);
            break;

        default:
            return std::nullopt;
        }
    }

    if (overflow || !is_stop_instruction(pc)
        || replacement.checkpoints.empty()) {
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
