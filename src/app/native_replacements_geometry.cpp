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

// Three arithmetic leaves in physical bank 01. They are not one routine and
// they do not call each other; what they have in common is the only thing that
// matters to a transcript. Each is straight-line integer arithmetic over work
// RAM and zero page, each ends in an `RTS`, and none of them calls anything,
// reads a device register or leaves the bank. After the Sorcerer tick they were
// the three largest addresses on the census route with no native entry anywhere
// inside them.
//
//   01:C8F3  the path-length accumulator, 2,340 calls and 1.53M master cycles.
//            `$3400,X`/`$3480,X` and `$3500,X`/`$3580,X` are the actor's two
//            signed sixteen-bit per-frame velocity components. It takes the
//            absolute value of each, adds them, shifts the sum left four times
//            into a twenty-four-bit intermediate, and adds that to the running
//            total held little-endian across `$3E00,X`, `$3A00,X` and
//            `$3980,X`. The top byte saturates: at 4 or more it is zeroed.
//
//   01:C969  the head of the range-squared helper, 2,993 calls. Its body never
//            runs on this route -- the sixteen-bit population counter at
//            `$2D00`/`$2D01` is zero on all 2,993 entries, so every call is the
//            three-instruction guard and its `RTS`. The walk transcribes the
//            guard and hands the taken arm at 01:C972 back to the core rather
//            than transcribing a path it has never observed.
//
//   01:DAF8  the direction-octant classifier, 2,139 calls and 1.51M master
//            cycles. It subtracts actor X's position from actor Y's on both
//            axes -- `$3180`/`$3200` horizontal, `$3300`/`$3380` vertical --
//            saturating each signed difference to a byte in `$43` and `$42`
//            and recording the two signs as bits 1 and 0 of `$45`. It then
//            orders the pair so `$43` holds the larger, notes the swap as bit
//            2, and splits the octant three ways: 3x the smaller against 2x the
//            larger, then 12x the smaller against the same, folded in as bits 4
//            and 3. The five bits index the thirty-two-entry table at `$DB9E`
//            in this bank's own ROM page. Two exits, 01:DB81 when the first
//            comparison already settles it and 01:DB9D when the second is
//            needed.
//
// 01:DAF8 was screened twice before and rejected both times, on the grounds
// that its cycle, write and result populations were "mixed rather than exposing
// a stable state-derived return". That was the right answer for the method in
// use then, which had to find a return value a replacement could predict. An
// instruction-boundary transcript does not predict anything -- it executes the
// branches the data selects -- so the mixture is not an obstacle to it. Both
// exits are transcribed here and the population divides 1,459 to 680 between
// them.
//
// Shared state was measured on exact windows from observers on all three heads.
// Of the 358 instructions that write any address these three read, exactly four
// ever write inside a window: 01:C94E, 01:C956, 01:C95E and 01:C965, which are
// 01:C8F3's own three stores and its saturating `STZ`. There is no foreign
// writer at all, and no callee to account for either -- these are leaves.
constexpr std::uint8_t kBank = 0x01U;

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
constexpr std::uint32_t kZeroPage = 12U;
constexpr std::uint32_t kZeroPageReadModifyWrite = 18U;
constexpr std::uint32_t kAbsolute = 15U;
constexpr std::uint32_t kAbsoluteIndexed = 15U;
constexpr std::uint32_t kBranchTaken = 12U;
constexpr std::uint32_t kBranchNotTaken = 6U;
// BRA is a branch whose condition is a constant true, and it costs what any
// taken branch costs.
constexpr std::uint32_t kBranchAlways = 12U;

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
    return logical_pc == 0xC8F3U || logical_pc == 0xC969U
        || logical_pc == 0xDAF8U;
}

// Where each walk stops: the accumulator's `RTS` at 01:C968, the guard's at
// 01:C971, the classifier's two at 01:DB81 and 01:DB9D, and 01:C972 -- the arm
// of the guard's `BNE` that this route never takes. Handing that arm back costs
// nothing and keeps the walk from claiming a path it has never seen.
[[nodiscard]] constexpr bool is_stop_instruction(const std::uint16_t logical_pc)
{
    return logical_pc == 0xC968U || logical_pc == 0xC971U
        || logical_pc == 0xC972U || logical_pc == 0xDB81U
        || logical_pc == 0xDB9DU;
}

} // namespace

std::optional<RoutineReplacement> replace_actor_geometry_leaf(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    if (entry.routine.physical_bank != kBank
        || !is_owned_entry(entry.routine.logical_pc)
        || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    // Both routines run their arithmetic in binary, and the memory-operand T
    // flag would redirect the accumulator through zero page for every ADC, SBC,
    // EOR and ORA below.
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
    // every entry these routines are reached from.
    const auto zero_page = [](const std::uint8_t address) {
        return static_cast<std::uint16_t>(kWorkRamBase + address);
    };

    // The classifier's answer table sits in this bank's own ROM page, so it is
    // resolved from the live MPRs rather than assumed. The hardware page fails
    // the bank test, which is what makes a device read decline rather than
    // fabricate a value.
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

    const auto slot_x = [&registers](const std::uint16_t base) {
        return static_cast<std::uint16_t>(
            base + static_cast<std::uint16_t>(registers.x));
    };
    const auto slot_y = [&registers](const std::uint16_t base) {
        return static_cast<std::uint16_t>(
            base + static_cast<std::uint16_t>(registers.y));
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
    const auto add_with_carry = [&registers](const std::uint8_t value) {
        const auto left = registers.accumulator;
        const auto carry_in = static_cast<std::uint16_t>(
            (registers.status & kCarry) != 0U ? 1U : 0U);
        const auto wide = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(left)
            + static_cast<std::uint16_t>(value) + carry_in);
        const auto result = static_cast<std::uint8_t>(wide);
        auto status = static_cast<std::uint8_t>(
            registers.status
            & static_cast<std::uint8_t>(~(kCarry | kOverflow)));
        if (wide > 0xFFU) {
            status = static_cast<std::uint8_t>(status | kCarry);
        }
        if ((static_cast<std::uint8_t>(~(left ^ value))
             & static_cast<std::uint8_t>(left ^ result) & kNegative)
            != 0U) {
            status = static_cast<std::uint8_t>(status | kOverflow);
        }
        registers.accumulator = result;
        registers.status = set_nz(status, result);
    };
    // SBC with the carry standing in for "no borrow", in binary only.
    const auto subtract_with_carry = [&registers](const std::uint8_t value) {
        const auto left = registers.accumulator;
        const auto borrow = static_cast<std::uint16_t>(
            (registers.status & kCarry) != 0U ? 0U : 1U);
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
    const auto increment_accumulator = [&registers]() {
        registers.accumulator =
            static_cast<std::uint8_t>(registers.accumulator + 1U);
        registers.status = set_nz(registers.status, registers.accumulator);
    };
    const auto shift_memory_left =
        [&registers, &read_ram, &store](
            const std::uint16_t instruction_pc,
            const std::uint16_t address) {
            const auto original = read_ram(address);
            const auto result = static_cast<std::uint8_t>(original << 1U);
            auto status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            if ((original & kNegative) != 0U) {
                status = static_cast<std::uint8_t>(status | kCarry);
            }
            store(instruction_pc, address, result);
            registers.status = set_nz(status, result);
        };
    const auto rotate_memory_left =
        [&registers, &read_ram, &store](
            const std::uint16_t instruction_pc,
            const std::uint16_t address) {
            const auto original = read_ram(address);
            const auto carry_in = static_cast<std::uint8_t>(
                (registers.status & kCarry) != 0U ? 1U : 0U);
            const auto result =
                static_cast<std::uint8_t>((original << 1U) | carry_in);
            auto status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            if ((original & kNegative) != 0U) {
                status = static_cast<std::uint8_t>(status | kCarry);
            }
            store(instruction_pc, address, result);
            registers.status = set_nz(status, result);
        };
    // INC on memory reports on the result and leaves the carry alone.
    const auto increment_memory =
        [&registers, &read_ram, &store](
            const std::uint16_t instruction_pc,
            const std::uint16_t address) {
            const auto value =
                static_cast<std::uint8_t>(read_ram(address) + 1U);
            store(instruction_pc, address, value);
            registers.status = set_nz(registers.status, value);
        };

    auto pc = entry.routine.logical_pc;
    while (!is_stop_instruction(pc) && !overflow
        && replacement.checkpoints.size() < 128U) {
        switch (pc) {
        // --- 01:C8F3, the path-length accumulator ------------------------
        case 0xC8F3U:  // LDA $3400,X -- horizontal velocity, low byte
            load_accumulator(read_ram(slot_x(0x3400U)));
            append_checkpoint(0xC8F3U, 0xC8F6U, kAbsoluteIndexed);
            pc = 0xC8F6U;
            break;
        case 0xC8F6U:  // STA $2F
            store(0xC8F6U, zero_page(0x2FU), registers.accumulator);
            append_checkpoint(0xC8F6U, 0xC8F8U, kZeroPage);
            pc = 0xC8F8U;
            break;
        case 0xC8F8U:  // LDA $3480,X -- and its high byte, which carries the
                       // sign the negation below tests
            load_accumulator(read_ram(slot_x(0x3480U)));
            append_checkpoint(0xC8F8U, 0xC8FBU, kAbsoluteIndexed);
            pc = 0xC8FBU;
            break;
        case 0xC8FBU:  // STA $30
            store(0xC8FBU, zero_page(0x30U), registers.accumulator);
            append_checkpoint(0xC8FBU, 0xC8FDU, kZeroPage);
            pc = 0xC8FDU;
            break;
        case 0xC8FDU:  // BPL $C90A
            pc = branch(
                0xC8FDU, 0xC90AU, 0xC8FFU,
                (registers.status & kNegative) == 0U);
            break;
        case 0xC8FFU:  // SEC -- zero minus the value is the sixteen-bit negate
            registers.status =
                static_cast<std::uint8_t>(registers.status | kCarry);
            append_checkpoint(0xC8FFU, 0xC900U, kImplied);
            pc = 0xC900U;
            break;
        case 0xC900U:  // CLA -- clears the accumulator and no flag with it
            registers.accumulator = 0U;
            append_checkpoint(0xC900U, 0xC901U, kImplied);
            pc = 0xC901U;
            break;
        case 0xC901U:  // SBC $2F
            subtract_with_carry(read_ram(zero_page(0x2FU)));
            append_checkpoint(0xC901U, 0xC903U, kZeroPage);
            pc = 0xC903U;
            break;
        case 0xC903U:  // STA $2F
            store(0xC903U, zero_page(0x2FU), registers.accumulator);
            append_checkpoint(0xC903U, 0xC905U, kZeroPage);
            pc = 0xC905U;
            break;
        case 0xC905U:  // CLA
            registers.accumulator = 0U;
            append_checkpoint(0xC905U, 0xC906U, kImplied);
            pc = 0xC906U;
            break;
        case 0xC906U:  // SBC $30
            subtract_with_carry(read_ram(zero_page(0x30U)));
            append_checkpoint(0xC906U, 0xC908U, kZeroPage);
            pc = 0xC908U;
            break;
        case 0xC908U:  // STA $30
            store(0xC908U, zero_page(0x30U), registers.accumulator);
            append_checkpoint(0xC908U, 0xC90AU, kZeroPage);
            pc = 0xC90AU;
            break;
        case 0xC90AU:  // LDA $3500,X -- vertical velocity, low byte
            load_accumulator(read_ram(slot_x(0x3500U)));
            append_checkpoint(0xC90AU, 0xC90DU, kAbsoluteIndexed);
            pc = 0xC90DU;
            break;
        case 0xC90DU:  // STA $2D
            store(0xC90DU, zero_page(0x2DU), registers.accumulator);
            append_checkpoint(0xC90DU, 0xC90FU, kZeroPage);
            pc = 0xC90FU;
            break;
        case 0xC90FU:  // LDA $3580,X
            load_accumulator(read_ram(slot_x(0x3580U)));
            append_checkpoint(0xC90FU, 0xC912U, kAbsoluteIndexed);
            pc = 0xC912U;
            break;
        case 0xC912U:  // STA $2E
            store(0xC912U, zero_page(0x2EU), registers.accumulator);
            append_checkpoint(0xC912U, 0xC914U, kZeroPage);
            pc = 0xC914U;
            break;
        case 0xC914U:  // BPL $C921
            pc = branch(
                0xC914U, 0xC921U, 0xC916U,
                (registers.status & kNegative) == 0U);
            break;
        case 0xC916U:  // SEC
            registers.status =
                static_cast<std::uint8_t>(registers.status | kCarry);
            append_checkpoint(0xC916U, 0xC917U, kImplied);
            pc = 0xC917U;
            break;
        case 0xC917U:  // CLA
            registers.accumulator = 0U;
            append_checkpoint(0xC917U, 0xC918U, kImplied);
            pc = 0xC918U;
            break;
        case 0xC918U:  // SBC $2D
            subtract_with_carry(read_ram(zero_page(0x2DU)));
            append_checkpoint(0xC918U, 0xC91AU, kZeroPage);
            pc = 0xC91AU;
            break;
        case 0xC91AU:  // STA $2D
            store(0xC91AU, zero_page(0x2DU), registers.accumulator);
            append_checkpoint(0xC91AU, 0xC91CU, kZeroPage);
            pc = 0xC91CU;
            break;
        case 0xC91CU:  // CLA
            registers.accumulator = 0U;
            append_checkpoint(0xC91CU, 0xC91DU, kImplied);
            pc = 0xC91DU;
            break;
        case 0xC91DU:  // SBC $2E
            subtract_with_carry(read_ram(zero_page(0x2EU)));
            append_checkpoint(0xC91DU, 0xC91FU, kZeroPage);
            pc = 0xC91FU;
            break;
        case 0xC91FU:  // STA $2E
            store(0xC91FU, zero_page(0x2EU), registers.accumulator);
            append_checkpoint(0xC91FU, 0xC921U, kZeroPage);
            pc = 0xC921U;
            break;
        case 0xC921U:  // CLC -- |dx| + |dy|
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xC921U, 0xC922U, kImplied);
            pc = 0xC922U;
            break;
        case 0xC922U:  // LDA $2F
            load_accumulator(read_ram(zero_page(0x2FU)));
            append_checkpoint(0xC922U, 0xC924U, kZeroPage);
            pc = 0xC924U;
            break;
        case 0xC924U:  // ADC $2D
            add_with_carry(read_ram(zero_page(0x2DU)));
            append_checkpoint(0xC924U, 0xC926U, kZeroPage);
            pc = 0xC926U;
            break;
        case 0xC926U:  // STA $2F
            store(0xC926U, zero_page(0x2FU), registers.accumulator);
            append_checkpoint(0xC926U, 0xC928U, kZeroPage);
            pc = 0xC928U;
            break;
        case 0xC928U:  // LDA $30
            load_accumulator(read_ram(zero_page(0x30U)));
            append_checkpoint(0xC928U, 0xC92AU, kZeroPage);
            pc = 0xC92AU;
            break;
        case 0xC92AU:  // ADC $2E
            add_with_carry(read_ram(zero_page(0x2EU)));
            append_checkpoint(0xC92AU, 0xC92CU, kZeroPage);
            pc = 0xC92CU;
            break;
        case 0xC92CU:  // STA $30
            store(0xC92CU, zero_page(0x30U), registers.accumulator);
            append_checkpoint(0xC92CU, 0xC92EU, kZeroPage);
            pc = 0xC92EU;
            break;
        case 0xC92EU:  // STZ $2D -- $2D becomes the third byte of the shift
            store(0xC92EU, zero_page(0x2DU), 0x00U);
            append_checkpoint(0xC92EU, 0xC930U, kZeroPage);
            pc = 0xC930U;
            break;
        // Four passes of the same twenty-four-bit left shift: x16.
        case 0xC930U:  // ASL $2F
            shift_memory_left(0xC930U, zero_page(0x2FU));
            append_checkpoint(0xC930U, 0xC932U, kZeroPageReadModifyWrite);
            pc = 0xC932U;
            break;
        case 0xC932U:  // ROL $30
            rotate_memory_left(0xC932U, zero_page(0x30U));
            append_checkpoint(0xC932U, 0xC934U, kZeroPageReadModifyWrite);
            pc = 0xC934U;
            break;
        case 0xC934U:  // ROL $2D
            rotate_memory_left(0xC934U, zero_page(0x2DU));
            append_checkpoint(0xC934U, 0xC936U, kZeroPageReadModifyWrite);
            pc = 0xC936U;
            break;
        case 0xC936U:  // ASL $2F
            shift_memory_left(0xC936U, zero_page(0x2FU));
            append_checkpoint(0xC936U, 0xC938U, kZeroPageReadModifyWrite);
            pc = 0xC938U;
            break;
        case 0xC938U:  // ROL $30
            rotate_memory_left(0xC938U, zero_page(0x30U));
            append_checkpoint(0xC938U, 0xC93AU, kZeroPageReadModifyWrite);
            pc = 0xC93AU;
            break;
        case 0xC93AU:  // ROL $2D
            rotate_memory_left(0xC93AU, zero_page(0x2DU));
            append_checkpoint(0xC93AU, 0xC93CU, kZeroPageReadModifyWrite);
            pc = 0xC93CU;
            break;
        case 0xC93CU:  // ASL $2F
            shift_memory_left(0xC93CU, zero_page(0x2FU));
            append_checkpoint(0xC93CU, 0xC93EU, kZeroPageReadModifyWrite);
            pc = 0xC93EU;
            break;
        case 0xC93EU:  // ROL $30
            rotate_memory_left(0xC93EU, zero_page(0x30U));
            append_checkpoint(0xC93EU, 0xC940U, kZeroPageReadModifyWrite);
            pc = 0xC940U;
            break;
        case 0xC940U:  // ROL $2D
            rotate_memory_left(0xC940U, zero_page(0x2DU));
            append_checkpoint(0xC940U, 0xC942U, kZeroPageReadModifyWrite);
            pc = 0xC942U;
            break;
        case 0xC942U:  // ASL $2F
            shift_memory_left(0xC942U, zero_page(0x2FU));
            append_checkpoint(0xC942U, 0xC944U, kZeroPageReadModifyWrite);
            pc = 0xC944U;
            break;
        case 0xC944U:  // ROL $30
            rotate_memory_left(0xC944U, zero_page(0x30U));
            append_checkpoint(0xC944U, 0xC946U, kZeroPageReadModifyWrite);
            pc = 0xC946U;
            break;
        case 0xC946U:  // ROL $2D
            rotate_memory_left(0xC946U, zero_page(0x2DU));
            append_checkpoint(0xC946U, 0xC948U, kZeroPageReadModifyWrite);
            pc = 0xC948U;
            break;
        case 0xC948U:  // CLC -- add the scaled step into the running total
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xC948U, 0xC949U, kImplied);
            pc = 0xC949U;
            break;
        case 0xC949U:  // LDA $3E00,X
            load_accumulator(read_ram(slot_x(0x3E00U)));
            append_checkpoint(0xC949U, 0xC94CU, kAbsoluteIndexed);
            pc = 0xC94CU;
            break;
        case 0xC94CU:  // ADC $2F
            add_with_carry(read_ram(zero_page(0x2FU)));
            append_checkpoint(0xC94CU, 0xC94EU, kZeroPage);
            pc = 0xC94EU;
            break;
        case 0xC94EU:  // STA $3E00,X
            store(0xC94EU, slot_x(0x3E00U), registers.accumulator);
            append_checkpoint(0xC94EU, 0xC951U, kAbsoluteIndexed);
            pc = 0xC951U;
            break;
        case 0xC951U:  // LDA $3A00,X
            load_accumulator(read_ram(slot_x(0x3A00U)));
            append_checkpoint(0xC951U, 0xC954U, kAbsoluteIndexed);
            pc = 0xC954U;
            break;
        case 0xC954U:  // ADC $30
            add_with_carry(read_ram(zero_page(0x30U)));
            append_checkpoint(0xC954U, 0xC956U, kZeroPage);
            pc = 0xC956U;
            break;
        case 0xC956U:  // STA $3A00,X
            store(0xC956U, slot_x(0x3A00U), registers.accumulator);
            append_checkpoint(0xC956U, 0xC959U, kAbsoluteIndexed);
            pc = 0xC959U;
            break;
        case 0xC959U:  // LDA $3980,X
            load_accumulator(read_ram(slot_x(0x3980U)));
            append_checkpoint(0xC959U, 0xC95CU, kAbsoluteIndexed);
            pc = 0xC95CU;
            break;
        case 0xC95CU:  // ADC $2D
            add_with_carry(read_ram(zero_page(0x2DU)));
            append_checkpoint(0xC95CU, 0xC95EU, kZeroPage);
            pc = 0xC95EU;
            break;
        case 0xC95EU:  // STA $3980,X
            store(0xC95EU, slot_x(0x3980U), registers.accumulator);
            append_checkpoint(0xC95EU, 0xC961U, kAbsoluteIndexed);
            pc = 0xC961U;
            break;
        case 0xC961U:  // CMP #$04 -- the total saturates rather than wrapping
            compare_accumulator(0x04U);
            append_checkpoint(0xC961U, 0xC963U, kImmediate);
            pc = 0xC963U;
            break;
        case 0xC963U:  // BCC $C968
            pc = branch(
                0xC963U, 0xC968U, 0xC965U,
                (registers.status & kCarry) == 0U);
            break;
        case 0xC965U:  // STZ $3980,X
            store(0xC965U, slot_x(0x3980U), 0x00U);
            append_checkpoint(0xC965U, 0xC968U, kAbsoluteIndexed);
            pc = 0xC968U;
            break;

        // --- 01:C969, the range-squared guard ----------------------------
        case 0xC969U:  // LDA $2D00
            load_accumulator(read_ram(0x2D00U));
            append_checkpoint(0xC969U, 0xC96CU, kAbsolute);
            pc = 0xC96CU;
            break;
        case 0xC96CU:  // ORA $2D01
            or_accumulator(read_ram(0x2D01U));
            append_checkpoint(0xC96CU, 0xC96FU, kAbsolute);
            pc = 0xC96FU;
            break;
        case 0xC96FU:  // BNE $C972 -- never taken on this route
            pc = branch(
                0xC96FU, 0xC972U, 0xC971U,
                (registers.status & kZero) == 0U);
            break;

        // --- 01:DAF8, the direction-octant classifier --------------------
        case 0xDAF8U:  // CLY -- the reference actor is slot zero
            registers.y = 0U;
            append_checkpoint(0xDAF8U, 0xDAF9U, kImplied);
            pc = 0xDAF9U;
            break;
        case 0xDAF9U:  // STZ $45 -- the octant bits accumulate here
            store(0xDAF9U, zero_page(0x45U), 0x00U);
            append_checkpoint(0xDAF9U, 0xDAFBU, kZeroPage);
            pc = 0xDAFBU;
            break;
        case 0xDAFBU:  // LDA $3180,Y
            load_accumulator(read_ram(slot_y(0x3180U)));
            append_checkpoint(0xDAFBU, 0xDAFEU, kAbsoluteIndexed);
            pc = 0xDAFEU;
            break;
        case 0xDAFEU:  // SEC
            registers.status =
                static_cast<std::uint8_t>(registers.status | kCarry);
            append_checkpoint(0xDAFEU, 0xDAFFU, kImplied);
            pc = 0xDAFFU;
            break;
        case 0xDAFFU:  // SBC $3180,X -- horizontal difference, low byte
            subtract_with_carry(read_ram(slot_x(0x3180U)));
            append_checkpoint(0xDAFFU, 0xDB02U, kAbsoluteIndexed);
            pc = 0xDB02U;
            break;
        case 0xDB02U:  // STA $43
            store(0xDB02U, zero_page(0x43U), registers.accumulator);
            append_checkpoint(0xDB02U, 0xDB04U, kZeroPage);
            pc = 0xDB04U;
            break;
        case 0xDB04U:  // LDA $3200,Y
            load_accumulator(read_ram(slot_y(0x3200U)));
            append_checkpoint(0xDB04U, 0xDB07U, kAbsoluteIndexed);
            pc = 0xDB07U;
            break;
        case 0xDB07U:  // SBC $3200,X -- and its high byte
            subtract_with_carry(read_ram(slot_x(0x3200U)));
            append_checkpoint(0xDB07U, 0xDB0AU, kAbsoluteIndexed);
            pc = 0xDB0AU;
            break;
        case 0xDB0AU:  // BCS $DB1F -- a clear carry means the target is left
            pc = branch(
                0xDB0AU, 0xDB1FU, 0xDB0CU,
                (registers.status & kCarry) != 0U);
            break;
        case 0xDB0CU:  // INC $45 -- octant bit 0
            increment_memory(0xDB0CU, zero_page(0x45U));
            append_checkpoint(0xDB0CU, 0xDB0EU, kZeroPageReadModifyWrite);
            pc = 0xDB0EU;
            break;
        case 0xDB0EU:  // CMP #$FF -- only a difference of -1..-256 negates
            compare_accumulator(0xFFU);
            append_checkpoint(0xDB0EU, 0xDB10U, kImmediate);
            pc = 0xDB10U;
            break;
        case 0xDB10U:  // BNE $DB19
            pc = branch(
                0xDB10U, 0xDB19U, 0xDB12U,
                (registers.status & kZero) == 0U);
            break;
        case 0xDB12U:  // EOR $43 -- the accumulator is $FF here, so this and
                       // the INC below are a two's complement negate of $43
            exclusive_or_accumulator(read_ram(zero_page(0x43U)));
            append_checkpoint(0xDB12U, 0xDB14U, kZeroPage);
            pc = 0xDB14U;
            break;
        case 0xDB14U:  // INC A
            increment_accumulator();
            append_checkpoint(0xDB14U, 0xDB15U, kAccumulator);
            pc = 0xDB15U;
            break;
        case 0xDB15U:  // STA $43
            store(0xDB15U, zero_page(0x43U), registers.accumulator);
            append_checkpoint(0xDB15U, 0xDB17U, kZeroPage);
            pc = 0xDB17U;
            break;
        case 0xDB17U:  // BRA $DB21
            append_checkpoint(0xDB17U, 0xDB21U, kBranchAlways);
            pc = 0xDB21U;
            break;
        case 0xDB19U:  // LDA #$FF -- anything further away saturates
            load_accumulator(0xFFU);
            append_checkpoint(0xDB19U, 0xDB1BU, kImmediate);
            pc = 0xDB1BU;
            break;
        case 0xDB1BU:  // STA $43
            store(0xDB1BU, zero_page(0x43U), registers.accumulator);
            append_checkpoint(0xDB1BU, 0xDB1DU, kZeroPage);
            pc = 0xDB1DU;
            break;
        case 0xDB1DU:  // BRA $DB21
            append_checkpoint(0xDB1DU, 0xDB21U, kBranchAlways);
            pc = 0xDB21U;
            break;
        case 0xDB1FU:  // BNE $DB19 -- a nonzero high byte saturates too
            pc = branch(
                0xDB1FU, 0xDB19U, 0xDB21U,
                (registers.status & kZero) == 0U);
            break;
        case 0xDB21U:  // ASL $45 -- make room for the vertical bit
            shift_memory_left(0xDB21U, zero_page(0x45U));
            append_checkpoint(0xDB21U, 0xDB23U, kZeroPageReadModifyWrite);
            pc = 0xDB23U;
            break;
        case 0xDB23U:  // LDA $3300,Y
            load_accumulator(read_ram(slot_y(0x3300U)));
            append_checkpoint(0xDB23U, 0xDB26U, kAbsoluteIndexed);
            pc = 0xDB26U;
            break;
        case 0xDB26U:  // SEC
            registers.status =
                static_cast<std::uint8_t>(registers.status | kCarry);
            append_checkpoint(0xDB26U, 0xDB27U, kImplied);
            pc = 0xDB27U;
            break;
        case 0xDB27U:  // SBC $3300,X -- vertical difference, low byte
            subtract_with_carry(read_ram(slot_x(0x3300U)));
            append_checkpoint(0xDB27U, 0xDB2AU, kAbsoluteIndexed);
            pc = 0xDB2AU;
            break;
        case 0xDB2AU:  // STA $42
            store(0xDB2AU, zero_page(0x42U), registers.accumulator);
            append_checkpoint(0xDB2AU, 0xDB2CU, kZeroPage);
            pc = 0xDB2CU;
            break;
        case 0xDB2CU:  // LDA $3380,Y
            load_accumulator(read_ram(slot_y(0x3380U)));
            append_checkpoint(0xDB2CU, 0xDB2FU, kAbsoluteIndexed);
            pc = 0xDB2FU;
            break;
        case 0xDB2FU:  // SBC $3380,X
            subtract_with_carry(read_ram(slot_x(0x3380U)));
            append_checkpoint(0xDB2FU, 0xDB32U, kAbsoluteIndexed);
            pc = 0xDB32U;
            break;
        case 0xDB32U:  // BCS $DB47 -- a clear carry means the target is above
            pc = branch(
                0xDB32U, 0xDB47U, 0xDB34U,
                (registers.status & kCarry) != 0U);
            break;
        case 0xDB34U:  // INC $45 -- octant bit 1
            increment_memory(0xDB34U, zero_page(0x45U));
            append_checkpoint(0xDB34U, 0xDB36U, kZeroPageReadModifyWrite);
            pc = 0xDB36U;
            break;
        case 0xDB36U:  // CMP #$FF
            compare_accumulator(0xFFU);
            append_checkpoint(0xDB36U, 0xDB38U, kImmediate);
            pc = 0xDB38U;
            break;
        case 0xDB38U:  // BNE $DB41
            pc = branch(
                0xDB38U, 0xDB41U, 0xDB3AU,
                (registers.status & kZero) == 0U);
            break;
        case 0xDB3AU:  // EOR $42
            exclusive_or_accumulator(read_ram(zero_page(0x42U)));
            append_checkpoint(0xDB3AU, 0xDB3CU, kZeroPage);
            pc = 0xDB3CU;
            break;
        case 0xDB3CU:  // INC A
            increment_accumulator();
            append_checkpoint(0xDB3CU, 0xDB3DU, kAccumulator);
            pc = 0xDB3DU;
            break;
        case 0xDB3DU:  // STA $42
            store(0xDB3DU, zero_page(0x42U), registers.accumulator);
            append_checkpoint(0xDB3DU, 0xDB3FU, kZeroPage);
            pc = 0xDB3FU;
            break;
        case 0xDB3FU:  // BRA $DB49
            append_checkpoint(0xDB3FU, 0xDB49U, kBranchAlways);
            pc = 0xDB49U;
            break;
        case 0xDB41U:  // LDA #$FF
            load_accumulator(0xFFU);
            append_checkpoint(0xDB41U, 0xDB43U, kImmediate);
            pc = 0xDB43U;
            break;
        case 0xDB43U:  // STA $42
            store(0xDB43U, zero_page(0x42U), registers.accumulator);
            append_checkpoint(0xDB43U, 0xDB45U, kZeroPage);
            pc = 0xDB45U;
            break;
        case 0xDB45U:  // BRA $DB49
            append_checkpoint(0xDB45U, 0xDB49U, kBranchAlways);
            pc = 0xDB49U;
            break;
        case 0xDB47U:  // BNE $DB41
            pc = branch(
                0xDB47U, 0xDB41U, 0xDB49U,
                (registers.status & kZero) == 0U);
            break;
        case 0xDB49U:  // LDA $43 -- order the pair so $43 holds the larger
            load_accumulator(read_ram(zero_page(0x43U)));
            append_checkpoint(0xDB49U, 0xDB4BU, kZeroPage);
            pc = 0xDB4BU;
            break;
        case 0xDB4BU:  // CMP $42
            compare_accumulator(read_ram(zero_page(0x42U)));
            append_checkpoint(0xDB4BU, 0xDB4DU, kZeroPage);
            pc = 0xDB4DU;
            break;
        case 0xDB4DU:  // BCS $DB5B
            pc = branch(
                0xDB4DU, 0xDB5BU, 0xDB4FU,
                (registers.status & kCarry) != 0U);
            break;
        case 0xDB4FU:  // LDY $42 -- the swap, through Y
            load_y(read_ram(zero_page(0x42U)));
            append_checkpoint(0xDB4FU, 0xDB51U, kZeroPage);
            pc = 0xDB51U;
            break;
        case 0xDB51U:  // STA $42
            store(0xDB51U, zero_page(0x42U), registers.accumulator);
            append_checkpoint(0xDB51U, 0xDB53U, kZeroPage);
            pc = 0xDB53U;
            break;
        case 0xDB53U:  // STY $43
            store(0xDB53U, zero_page(0x43U), registers.y);
            append_checkpoint(0xDB53U, 0xDB55U, kZeroPage);
            pc = 0xDB55U;
            break;
        case 0xDB55U:  // LDA #$04
            load_accumulator(0x04U);
            append_checkpoint(0xDB55U, 0xDB57U, kImmediate);
            pc = 0xDB57U;
            break;
        case 0xDB57U:  // ORA $45 -- bit 2 records the swap
            or_accumulator(read_ram(zero_page(0x45U)));
            append_checkpoint(0xDB57U, 0xDB59U, kZeroPage);
            pc = 0xDB59U;
            break;
        case 0xDB59U:  // STA $45
            store(0xDB59U, zero_page(0x45U), registers.accumulator);
            append_checkpoint(0xDB59U, 0xDB5BU, kZeroPage);
            pc = 0xDB5BU;
            break;
        case 0xDB5BU:  // STZ $44 -- $44:A becomes 3x the smaller axis
            store(0xDB5BU, zero_page(0x44U), 0x00U);
            append_checkpoint(0xDB5BU, 0xDB5DU, kZeroPage);
            pc = 0xDB5DU;
            break;
        case 0xDB5DU:  // LDA $42
            load_accumulator(read_ram(zero_page(0x42U)));
            append_checkpoint(0xDB5DU, 0xDB5FU, kZeroPage);
            pc = 0xDB5FU;
            break;
        case 0xDB5FU:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xDB5FU, 0xDB60U, kAccumulator);
            pc = 0xDB60U;
            break;
        case 0xDB60U:  // ROL $44
            rotate_memory_left(0xDB60U, zero_page(0x44U));
            append_checkpoint(0xDB60U, 0xDB62U, kZeroPageReadModifyWrite);
            pc = 0xDB62U;
            break;
        case 0xDB62U:  // ADC $42
            add_with_carry(read_ram(zero_page(0x42U)));
            append_checkpoint(0xDB62U, 0xDB64U, kZeroPage);
            pc = 0xDB64U;
            break;
        case 0xDB64U:  // STA $42
            store(0xDB64U, zero_page(0x42U), registers.accumulator);
            append_checkpoint(0xDB64U, 0xDB66U, kZeroPage);
            pc = 0xDB66U;
            break;
        case 0xDB66U:  // LDY $44 -- Y carries the high byte from here
            load_y(read_ram(zero_page(0x44U)));
            append_checkpoint(0xDB66U, 0xDB68U, kZeroPage);
            pc = 0xDB68U;
            break;
        case 0xDB68U:  // BCC $DB6B
            pc = branch(
                0xDB68U, 0xDB6BU, 0xDB6AU,
                (registers.status & kCarry) == 0U);
            break;
        case 0xDB6AU:  // INY
            load_y(static_cast<std::uint8_t>(registers.y + 1U));
            append_checkpoint(0xDB6AU, 0xDB6BU, kImplied);
            pc = 0xDB6BU;
            break;
        case 0xDB6BU:  // STZ $44 -- $44:$43 becomes 2x the larger axis
            store(0xDB6BU, zero_page(0x44U), 0x00U);
            append_checkpoint(0xDB6BU, 0xDB6DU, kZeroPage);
            pc = 0xDB6DU;
            break;
        case 0xDB6DU:  // ASL $43
            shift_memory_left(0xDB6DU, zero_page(0x43U));
            append_checkpoint(0xDB6DU, 0xDB6FU, kZeroPageReadModifyWrite);
            pc = 0xDB6FU;
            break;
        case 0xDB6FU:  // ROL $44
            rotate_memory_left(0xDB6FU, zero_page(0x44U));
            append_checkpoint(0xDB6FU, 0xDB71U, kZeroPageReadModifyWrite);
            pc = 0xDB71U;
            break;
        case 0xDB71U:  // SEC -- 3x the smaller against 2x the larger
            registers.status =
                static_cast<std::uint8_t>(registers.status | kCarry);
            append_checkpoint(0xDB71U, 0xDB72U, kImplied);
            pc = 0xDB72U;
            break;
        case 0xDB72U:  // SBC $43
            subtract_with_carry(read_ram(zero_page(0x43U)));
            append_checkpoint(0xDB72U, 0xDB74U, kZeroPage);
            pc = 0xDB74U;
            break;
        case 0xDB74U:  // TYA
            load_accumulator(registers.y);
            append_checkpoint(0xDB74U, 0xDB75U, kImplied);
            pc = 0xDB75U;
            break;
        case 0xDB75U:  // SBC $44
            subtract_with_carry(read_ram(zero_page(0x44U)));
            append_checkpoint(0xDB75U, 0xDB77U, kZeroPage);
            pc = 0xDB77U;
            break;
        case 0xDB77U:  // BCC $DB82 -- a shallower angle needs the second test
            pc = branch(
                0xDB77U, 0xDB82U, 0xDB79U,
                (registers.status & kCarry) == 0U);
            break;
        case 0xDB79U:  // LDA $45 -- bit 4: the pair is within 3:2 of square
            load_accumulator(read_ram(zero_page(0x45U)));
            append_checkpoint(0xDB79U, 0xDB7BU, kZeroPage);
            pc = 0xDB7BU;
            break;
        case 0xDB7BU:  // ORA #$10
            or_accumulator(0x10U);
            append_checkpoint(0xDB7BU, 0xDB7DU, kImmediate);
            pc = 0xDB7DU;
            break;
        case 0xDB7DU:  // TAY
            load_y(registers.accumulator);
            append_checkpoint(0xDB7DU, 0xDB7EU, kImplied);
            pc = 0xDB7EU;
            break;
        case 0xDB7EU:  // LDA $DB9E,Y -- the answer table, in this bank's ROM
            load_accumulator(read_mapped(
                static_cast<std::uint16_t>(
                    0xDB9EU + static_cast<std::uint16_t>(registers.y))));
            append_checkpoint(0xDB7EU, 0xDB81U, kAbsoluteIndexed);
            pc = 0xDB81U;
            break;
        case 0xDB82U:  // LDA $42 -- 12x the smaller against the same 2x the larger
            load_accumulator(read_ram(zero_page(0x42U)));
            append_checkpoint(0xDB82U, 0xDB84U, kZeroPage);
            pc = 0xDB84U;
            break;
        case 0xDB84U:  // STY $42 -- Y is the high byte of 3x the smaller
            store(0xDB84U, zero_page(0x42U), registers.y);
            append_checkpoint(0xDB84U, 0xDB86U, kZeroPage);
            pc = 0xDB86U;
            break;
        case 0xDB86U:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xDB86U, 0xDB87U, kAccumulator);
            pc = 0xDB87U;
            break;
        case 0xDB87U:  // ROL $42
            rotate_memory_left(0xDB87U, zero_page(0x42U));
            append_checkpoint(0xDB87U, 0xDB89U, kZeroPageReadModifyWrite);
            pc = 0xDB89U;
            break;
        case 0xDB89U:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xDB89U, 0xDB8AU, kAccumulator);
            pc = 0xDB8AU;
            break;
        case 0xDB8AU:  // ROL $42
            rotate_memory_left(0xDB8AU, zero_page(0x42U));
            append_checkpoint(0xDB8AU, 0xDB8CU, kZeroPageReadModifyWrite);
            pc = 0xDB8CU;
            break;
        case 0xDB8CU:  // SEC
            registers.status =
                static_cast<std::uint8_t>(registers.status | kCarry);
            append_checkpoint(0xDB8CU, 0xDB8DU, kImplied);
            pc = 0xDB8DU;
            break;
        case 0xDB8DU:  // SBC $43
            subtract_with_carry(read_ram(zero_page(0x43U)));
            append_checkpoint(0xDB8DU, 0xDB8FU, kZeroPage);
            pc = 0xDB8FU;
            break;
        case 0xDB8FU:  // LDA $42 -- only the high byte's borrow decides
            load_accumulator(read_ram(zero_page(0x42U)));
            append_checkpoint(0xDB8FU, 0xDB91U, kZeroPage);
            pc = 0xDB91U;
            break;
        case 0xDB91U:  // SBC $44
            subtract_with_carry(read_ram(zero_page(0x44U)));
            append_checkpoint(0xDB91U, 0xDB93U, kZeroPage);
            pc = 0xDB93U;
            break;
        case 0xDB93U:  // LDA $45
            load_accumulator(read_ram(zero_page(0x45U)));
            append_checkpoint(0xDB93U, 0xDB95U, kZeroPage);
            pc = 0xDB95U;
            break;
        case 0xDB95U:  // BCC $DB99
            pc = branch(
                0xDB95U, 0xDB99U, 0xDB97U,
                (registers.status & kCarry) == 0U);
            break;
        case 0xDB97U:  // ORA #$08 -- bit 3: within 6:1, so not quite axis-aligned
            or_accumulator(0x08U);
            append_checkpoint(0xDB97U, 0xDB99U, kImmediate);
            pc = 0xDB99U;
            break;
        case 0xDB99U:  // TAY
            load_y(registers.accumulator);
            append_checkpoint(0xDB99U, 0xDB9AU, kImplied);
            pc = 0xDB9AU;
            break;
        case 0xDB9AU:  // LDA $DB9E,Y
            load_accumulator(read_mapped(
                static_cast<std::uint16_t>(
                    0xDB9EU + static_cast<std::uint16_t>(registers.y))));
            append_checkpoint(0xDB9AU, 0xDB9DU, kAbsoluteIndexed);
            pc = 0xDB9DU;
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
