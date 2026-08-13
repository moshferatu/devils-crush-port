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

// The bank-switching call wrappers. Every cross-bank call in the game goes
// through one of these: the caller leaves a physical bank in $30 and an entry
// offset in $2F, and a wrapper maps the bank into an MPR slot, jumps through
// the callee's $40xx table, and unmaps it again on the way out. They are the
// game's linkage, so they are hot -- 4,849,479 of the census route's 1,292M
// master cycles across 52,911 calls -- and they carry no game logic at all.
//
// $1E, $1F, $20 and $21 are the zero-page shadows of MPR2, MPR3, MPR4 and
// MPR5. Every wrapper keeps the shadow and the register in step, which is what
// makes the idiom re-entrant: a wrapper that preempts another restores both
// from its own stack frame.
//
// Nine entry points share one walk. Four of them are heads; the rest are the
// addresses control comes back to, because a wrapper's whole purpose is to
// call something and the walk has to stop in front of that call:
//
//   00:E20D  map $30 into MPR4/MPR5
//   00:E200  the asset resolver's tail: read the $8000 table through the slot
//            00:E20D just mapped and leave the resolved reference in $2F/$30
//   00:E219  map the fixed bank pair for the current mode at $55
//   00:E241  save MPR2/MPR3, map $30 into them, stop at the JSR $E262
//   00:E255  store the callee's result in $2A and restore MPR2/MPR3
//   00:E262  the trampoline: build $40xx in $2CBE/$2CBF, stop at the JMP
//   00:ECC6  save MPR2/MPR3/MPR4, map banks $45-$47, stop at the JSR $ECF2
//   00:ECE2  restore MPR2/MPR3/MPR4
//   00:ECF2  point $47/$48 at $4003 and stop at the JMP that enters bank $45
//
// Two of those stops are indirect jumps -- 00:E26E and 00:ECFA -- and the walk
// stops in front of both rather than resolving them. The vector each one reads
// is a cell the same transcript just wrote, so leaving the jump to the core
// costs six instructions of coverage and removes the only place this module
// would have to compute a resume bank.
//
// Nothing here reads state an interrupt handler leaves changed. The VBlank
// handler's own wrapper at 00:ECC6 is the only interrupt-context writer of
// $1E-$21, and it restores all three from the stack before it returns; the
// timer driver never touches them. $2A, $2F, $30 and $55 are written from the
// main thread only.
//
// 00:ED5E is the fifth wrapper and is deliberately absent. It is entered with
// two of its four stack slots already pushed by its caller, so its entry
// contract is not the routine's own, and it is 23,000 master cycles across 72
// calls -- 0.4% of this family.
constexpr std::uint8_t kBank = 0x00U;

constexpr std::uint8_t kZero = 0x02U;
constexpr std::uint8_t kDecimal = 0x08U;
constexpr std::uint8_t kMemory = 0x20U;
constexpr std::uint8_t kNegative = 0x80U;

// Work-RAM window. Zero page is $2000-$20FF and the stack page is $2100-$21FF;
// both are ordinary work RAM reached through segment 1.
constexpr std::uint16_t kWorkRamBase = 0x2000U;
constexpr std::uint16_t kWorkRamEnd = 0x4000U;
constexpr std::uint16_t kStackBase = 0x2100U;

[[nodiscard]] std::uint8_t set_nz(
    const std::uint8_t status,
    const std::uint8_t value)
{
    auto result = static_cast<std::uint8_t>(
        status & static_cast<std::uint8_t>(~(kZero | kNegative)));
    if (value == 0U) {
        result = static_cast<std::uint8_t>(result | kZero);
    }
    result = static_cast<std::uint8_t>(
        result | static_cast<std::uint8_t>(value & kNegative));
    return result;
}

[[nodiscard]] constexpr bool is_owned_entry(const std::uint16_t logical_pc)
{
    return logical_pc == 0xE200U || logical_pc == 0xE20DU
        || logical_pc == 0xE219U || logical_pc == 0xE241U
        || logical_pc == 0xE255U || logical_pc == 0xE262U
        || logical_pc == 0xECC6U || logical_pc == 0xECE2U
        || logical_pc == 0xECF2U;
}

// Where each walk stops. Five are RTS instructions and four are the calls this
// family exists to make -- two JSRs and two indirect jumps -- which the core
// executes, re-entering whichever replacement owns the target.
[[nodiscard]] constexpr bool is_stop_instruction(const std::uint16_t logical_pc)
{
    return logical_pc == 0xE20CU || logical_pc == 0xE218U
        || logical_pc == 0xE232U || logical_pc == 0xE252U
        || logical_pc == 0xE261U || logical_pc == 0xE26EU
        || logical_pc == 0xECDFU || logical_pc == 0xECF1U
        || logical_pc == 0xECFAU;
}

// All five of those RTS instructions are returns, and every one of them can be
// run past. What differs from the rest of the port is where the frame is: two
// of these walks give the stack back before they return, so the pointer at the
// RTS is above the one the routine was entered with. This says by how much, and
// the walk is checked against it rather than trusted -- the pointer is derived
// from the entry's through the same push and pop the machine performs, so a
// disagreement here is a modelling error and not a fact about the route.
//
// Each stop is reachable from exactly one of the nine entries, so a single
// count per address is well defined: the walks are straight-line from the pop
// onwards, and the only branch in the family (00:E224) is upstream of the two
// that pop anything.
//
//   00:E20C  from 00:E200, which touches no stack
//   00:E218  from 00:E20D, which touches no stack
//   00:E232  from 00:E219, which touches no stack
//   00:E261  from 00:E255, which restores $1F and $1E
//   00:ECF1  from 00:ECE2, which restores $20, $1F and $1E
//
// The last two are the far side of this family's own linkage. 00:E241 pushes
// the two MPR shadows and stops in front of `JSR $E262`; the core makes the
// call, the callee returns to 00:E255, and it is that transcript -- a separate
// entry, entered by an RTS landing rather than by a JSR -- which pulls the
// shadows back off and returns to whoever called the wrapper. So the frame this
// absorbs belongs to a call that was opened two transcripts ago, which is what
// makes reading the pointer at the return rather than at the entry the whole
// of the correctness argument. 00:ECC6 and 00:ECE2, either side of the `JSR` at
// 00:ECDF, are the same shape one shadow wider.
[[nodiscard]] constexpr std::optional<std::uint8_t> absorbable_return_pops(
    const std::uint16_t logical_pc)
{
    switch (logical_pc) {
    case 0xE20CU:
    case 0xE218U:
    case 0xE232U:
        return static_cast<std::uint8_t>(0U);
    case 0xE261U:
        return static_cast<std::uint8_t>(2U);
    case 0xECF1U:
        return static_cast<std::uint8_t>(3U);
    default:
        return std::nullopt;
    }
}

} // namespace

std::optional<RoutineReplacement> replace_bank_call_wrappers(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    if (entry.routine.physical_bank != kBank
        || !is_owned_entry(entry.routine.logical_pc)
        || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    // Neither decimal mode nor the memory-operand T flag is something these
    // wrappers are written to run under; T in particular would redirect the
    // accumulator loads through zero page.
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

    // Reads the byte a logical address currently maps to. 00:E200 reads its
    // table through MPR4, which 00:E20D pointed at an arbitrary bank one call
    // earlier, so the mapping is resolved from the registers rather than
    // assumed.
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
    // offset and the instruction of the checkpoint that follows it. PHA is the
    // same shape -- ADDCYC(2); LastCycle(); PUSH(A) -- unlike JSR, whose two
    // return-address bytes land before any cycle is charged.
    std::size_t committed_writes = 0U;
    const auto commit_pending_writes =
        [&replacement, &committed_writes](const std::uint32_t at_cycle) {
            while (committed_writes < replacement.work_ram_writes.size()) {
                replacement.work_ram_writes[committed_writes]
                    .master_cycle_offset = at_cycle;
                ++committed_writes;
            }
        };

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

    // PUSH writes $2100 + S and then decrements S; POP increments S first.
    // Deriving both from the live stack pointer is what keeps a transcript
    // valid at a depth other than the one it was recorded at.
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
    const auto pop = [&registers, &read_ram]() {
        registers.stack_pointer =
            static_cast<std::uint8_t>(registers.stack_pointer + 1U);
        return read_ram(
            static_cast<std::uint16_t>(kStackBase + registers.stack_pointer));
    };

    // Zero page lives at $2000; an absolute address is used as written.
    const auto zp = [](const std::uint8_t address) {
        return static_cast<std::uint16_t>(kWorkRamBase + address);
    };
    const auto indexed_y = [&registers](const std::uint16_t base) {
        return static_cast<std::uint16_t>(
            base + static_cast<std::uint16_t>(registers.y));
    };

    const auto load_accumulator = [&registers](const std::uint8_t value) {
        registers.accumulator = value;
        registers.status = set_nz(registers.status, value);
    };
    const auto load_y = [&registers](const std::uint8_t value) {
        registers.y = value;
        registers.status = set_nz(registers.status, value);
    };
    // TAM maps the accumulator into every MPR slot the operand's bits name and
    // changes no flag. Only single-bit operands occur here.
    const auto transfer_accumulator_to_mpr =
        [&registers](const std::uint8_t mask) {
            for (std::size_t slot = 0U; slot < registers.mpr.size(); ++slot) {
                if ((mask & static_cast<std::uint8_t>(1U << slot)) != 0U) {
                    registers.mpr[slot] = registers.accumulator;
                }
            }
        };

    auto pc = entry.routine.logical_pc;
    for (;;) {
        if (overflow
            || replacement.checkpoints.size() > 1800U
            || replacement.work_ram_writes.size() > 480U) {
            return std::nullopt;
        }

        if (is_stop_instruction(pc)) {
            registers.program_counter = pc;
            replacement.return_instruction = {kBank, pc};
            replacement.output_registers = registers;
            replacement.body_master_cycles = cycle;

            // Nothing composes this walk -- the registry is its only caller --
            // so it can absorb its own return rather than needing an entry
            // wrapper to do it. The stack pointer is the one the walk arrived
            // at the RTS with, not the entry's, and it is required to be
            // exactly the entry's plus what this stop's walk popped. Every walk
            // here spends at least one instruction before it stops, so the
            // atomic question can never be the thing that refuses an absorption
            // -- it is asked because a transcript with no checkpoint is not
            // this file's rule to break.
            if (const auto pops = absorbable_return_pops(pc);
                pops.has_value()
                && registers.stack_pointer
                    == static_cast<std::uint8_t>(
                        entry.registers.stack_pointer + *pops)
                && (!replacement.checkpoints.empty()
                    || absorbed_return_fits_atomically(entry, cycle))) {
                absorb_return(
                    replacement,
                    work_ram,
                    registers.stack_pointer,
                    {kBank, pc},
                    cycle,
                    registers);
            }
            return replacement;
        }

        switch (pc) {
        // ---- 00:E200  asset resolver tail ---------------------------------
        // $2F indexes a table of two-byte references at the head of the bank
        // 00:E20D has just mapped at $8000. The resolved offset and bank
        // replace the reference in place.
        case 0xE200U:  // LDY $2F
            load_y(read_ram(zp(0x2FU)));
            append_checkpoint(0xE200U, 0xE202U, 12U);
            pc = 0xE202U;
            break;

        case 0xE202U:  // LDA $8000,Y
            load_accumulator(read_mapped(indexed_y(0x8000U)));
            append_checkpoint(0xE202U, 0xE205U, 15U);
            pc = 0xE205U;
            break;

        case 0xE205U:  // STA $2F
            store(0xE205U, zp(0x2FU), registers.accumulator);
            append_checkpoint(0xE205U, 0xE207U, 12U);
            pc = 0xE207U;
            break;

        case 0xE207U:  // LDA $8001,Y
            load_accumulator(read_mapped(indexed_y(0x8001U)));
            append_checkpoint(0xE207U, 0xE20AU, 15U);
            pc = 0xE20AU;
            break;

        case 0xE20AU:  // STA $30
            store(0xE20AU, zp(0x30U), registers.accumulator);
            append_checkpoint(0xE20AU, 0xE20CU, 12U);
            pc = 0xE20CU;
            break;

        // ---- 00:E20D  map $30 into MPR4/MPR5 ------------------------------
        case 0xE20DU:  // LDA $30
            load_accumulator(read_ram(zp(0x30U)));
            append_checkpoint(0xE20DU, 0xE20FU, 12U);
            pc = 0xE20FU;
            break;

        case 0xE20FU:  // STA $20
            store(0xE20FU, zp(0x20U), registers.accumulator);
            append_checkpoint(0xE20FU, 0xE211U, 12U);
            pc = 0xE211U;
            break;

        case 0xE211U:  // TAM #$10 -- MPR4
            transfer_accumulator_to_mpr(0x10U);
            append_checkpoint(0xE211U, 0xE213U, 15U);
            pc = 0xE213U;
            break;

        case 0xE213U:  // INC A
            registers.accumulator =
                static_cast<std::uint8_t>(registers.accumulator + 1U);
            registers.status = set_nz(registers.status, registers.accumulator);
            append_checkpoint(0xE213U, 0xE214U, 6U);
            pc = 0xE214U;
            break;

        case 0xE214U:  // STA $21
            store(0xE214U, zp(0x21U), registers.accumulator);
            append_checkpoint(0xE214U, 0xE216U, 12U);
            pc = 0xE216U;
            break;

        case 0xE216U:  // TAM #$20 -- MPR5
            transfer_accumulator_to_mpr(0x20U);
            append_checkpoint(0xE216U, 0xE218U, 15U);
            pc = 0xE218U;
            break;

        // ---- 00:E219  map the mode's fixed bank pair ----------------------
        // $2D6C selects between two tables of high banks; $55 indexes them.
        // The route only ever takes the $E233 table, so the $E23A arm is
        // transcribed from the disassembly and has not been census-checked.
        case 0xE219U:  // LDA #$42
            load_accumulator(0x42U);
            append_checkpoint(0xE219U, 0xE21BU, 6U);
            pc = 0xE21BU;
            break;

        case 0xE21BU:  // STA $20
            store(0xE21BU, zp(0x20U), registers.accumulator);
            append_checkpoint(0xE21BU, 0xE21DU, 12U);
            pc = 0xE21DU;
            break;

        case 0xE21DU:  // TAM #$10 -- MPR4
            transfer_accumulator_to_mpr(0x10U);
            append_checkpoint(0xE21DU, 0xE21FU, 15U);
            pc = 0xE21FU;
            break;

        case 0xE21FU:  // LDY $55
            load_y(read_ram(zp(0x55U)));
            append_checkpoint(0xE21FU, 0xE221U, 12U);
            pc = 0xE221U;
            break;

        case 0xE221U:  // LDA $2D6C
            load_accumulator(read_ram(0x2D6CU));
            append_checkpoint(0xE221U, 0xE224U, 15U);
            pc = 0xE224U;
            break;

        case 0xE224U: {  // BNE $E22B
            const bool taken = (registers.status & kZero) == 0U;
            const std::uint16_t next = taken ? 0xE22BU : 0xE226U;
            append_checkpoint(0xE224U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xE226U:  // LDA $E233,Y
            load_accumulator(read_mapped(indexed_y(0xE233U)));
            append_checkpoint(0xE226U, 0xE229U, 15U);
            pc = 0xE229U;
            break;

        case 0xE229U:  // BRA $E22E
            append_checkpoint(0xE229U, 0xE22EU, 12U);
            pc = 0xE22EU;
            break;

        case 0xE22BU:  // LDA $E23A,Y
            load_accumulator(read_mapped(indexed_y(0xE23AU)));
            append_checkpoint(0xE22BU, 0xE22EU, 15U);
            pc = 0xE22EU;
            break;

        case 0xE22EU:  // STA $21
            store(0xE22EU, zp(0x21U), registers.accumulator);
            append_checkpoint(0xE22EU, 0xE230U, 12U);
            pc = 0xE230U;
            break;

        case 0xE230U:  // TAM #$20 -- MPR5
            transfer_accumulator_to_mpr(0x20U);
            append_checkpoint(0xE230U, 0xE232U, 15U);
            pc = 0xE232U;
            break;

        // ---- 00:E241  banked call, prologue -------------------------------
        case 0xE241U:  // LDA $1E
            load_accumulator(read_ram(zp(0x1EU)));
            append_checkpoint(0xE241U, 0xE243U, 12U);
            pc = 0xE243U;
            break;

        case 0xE243U:  // PHA
            push(0xE243U, registers.accumulator);
            append_checkpoint(0xE243U, 0xE244U, 9U);
            pc = 0xE244U;
            break;

        case 0xE244U:  // LDA $1F
            load_accumulator(read_ram(zp(0x1FU)));
            append_checkpoint(0xE244U, 0xE246U, 12U);
            pc = 0xE246U;
            break;

        case 0xE246U:  // PHA
            push(0xE246U, registers.accumulator);
            append_checkpoint(0xE246U, 0xE247U, 9U);
            pc = 0xE247U;
            break;

        case 0xE247U:  // LDA $30
            load_accumulator(read_ram(zp(0x30U)));
            append_checkpoint(0xE247U, 0xE249U, 12U);
            pc = 0xE249U;
            break;

        case 0xE249U:  // STA $1E
            store(0xE249U, zp(0x1EU), registers.accumulator);
            append_checkpoint(0xE249U, 0xE24BU, 12U);
            pc = 0xE24BU;
            break;

        case 0xE24BU:  // TAM #$04 -- MPR2
            transfer_accumulator_to_mpr(0x04U);
            append_checkpoint(0xE24BU, 0xE24DU, 15U);
            pc = 0xE24DU;
            break;

        case 0xE24DU:  // INC A
            registers.accumulator =
                static_cast<std::uint8_t>(registers.accumulator + 1U);
            registers.status = set_nz(registers.status, registers.accumulator);
            append_checkpoint(0xE24DU, 0xE24EU, 6U);
            pc = 0xE24EU;
            break;

        case 0xE24EU:  // STA $1F
            store(0xE24EU, zp(0x1FU), registers.accumulator);
            append_checkpoint(0xE24EU, 0xE250U, 12U);
            pc = 0xE250U;
            break;

        case 0xE250U:  // TAM #$08 -- MPR3
            transfer_accumulator_to_mpr(0x08U);
            append_checkpoint(0xE250U, 0xE252U, 15U);
            pc = 0xE252U;
            break;

        // ---- 00:E255  banked call, epilogue -------------------------------
        case 0xE255U:  // STA $2A -- the callee's result
            store(0xE255U, zp(0x2AU), registers.accumulator);
            append_checkpoint(0xE255U, 0xE257U, 12U);
            pc = 0xE257U;
            break;

        case 0xE257U:  // PLA
            load_accumulator(pop());
            append_checkpoint(0xE257U, 0xE258U, 12U);
            pc = 0xE258U;
            break;

        case 0xE258U:  // STA $1F
            store(0xE258U, zp(0x1FU), registers.accumulator);
            append_checkpoint(0xE258U, 0xE25AU, 12U);
            pc = 0xE25AU;
            break;

        case 0xE25AU:  // TAM #$08 -- MPR3
            transfer_accumulator_to_mpr(0x08U);
            append_checkpoint(0xE25AU, 0xE25CU, 15U);
            pc = 0xE25CU;
            break;

        case 0xE25CU:  // PLA
            load_accumulator(pop());
            append_checkpoint(0xE25CU, 0xE25DU, 12U);
            pc = 0xE25DU;
            break;

        case 0xE25DU:  // STA $1E
            store(0xE25DU, zp(0x1EU), registers.accumulator);
            append_checkpoint(0xE25DU, 0xE25FU, 12U);
            pc = 0xE25FU;
            break;

        case 0xE25FU:  // TAM #$04 -- MPR2
            transfer_accumulator_to_mpr(0x04U);
            append_checkpoint(0xE25FU, 0xE261U, 15U);
            pc = 0xE261U;
            break;

        // ---- 00:E262  the trampoline --------------------------------------
        // $2CBE/$2CBF is the indirect vector, assembled here as $40xx from the
        // entry offset in $2F. The accumulator is loaded with $2A, which is
        // the argument every callee reads.
        case 0xE262U:  // LDA $2F
            load_accumulator(read_ram(zp(0x2FU)));
            append_checkpoint(0xE262U, 0xE264U, 12U);
            pc = 0xE264U;
            break;

        case 0xE264U:  // STA $2CBE
            store(0xE264U, 0x2CBEU, registers.accumulator);
            append_checkpoint(0xE264U, 0xE267U, 15U);
            pc = 0xE267U;
            break;

        case 0xE267U:  // LDA #$40
            load_accumulator(0x40U);
            append_checkpoint(0xE267U, 0xE269U, 6U);
            pc = 0xE269U;
            break;

        case 0xE269U:  // STA $2CBF
            store(0xE269U, 0x2CBFU, registers.accumulator);
            append_checkpoint(0xE269U, 0xE26CU, 15U);
            pc = 0xE26CU;
            break;

        case 0xE26CU:  // LDA $2A
            load_accumulator(read_ram(zp(0x2AU)));
            append_checkpoint(0xE26CU, 0xE26EU, 12U);
            pc = 0xE26EU;
            break;

        // ---- 00:ECC6  the VBlank handler's music call, prologue -----------
        // Banks $45-$47 are the music sequencer. This wrapper is the only
        // interrupt-context writer of $1E-$21 in the game.
        case 0xECC6U:  // LDA $1E
            load_accumulator(read_ram(zp(0x1EU)));
            append_checkpoint(0xECC6U, 0xECC8U, 12U);
            pc = 0xECC8U;
            break;

        case 0xECC8U:  // PHA
            push(0xECC8U, registers.accumulator);
            append_checkpoint(0xECC8U, 0xECC9U, 9U);
            pc = 0xECC9U;
            break;

        case 0xECC9U:  // LDA $1F
            load_accumulator(read_ram(zp(0x1FU)));
            append_checkpoint(0xECC9U, 0xECCBU, 12U);
            pc = 0xECCBU;
            break;

        case 0xECCBU:  // PHA
            push(0xECCBU, registers.accumulator);
            append_checkpoint(0xECCBU, 0xECCCU, 9U);
            pc = 0xECCCU;
            break;

        case 0xECCCU:  // LDA $20
            load_accumulator(read_ram(zp(0x20U)));
            append_checkpoint(0xECCCU, 0xECCEU, 12U);
            pc = 0xECCEU;
            break;

        case 0xECCEU:  // PHA
            push(0xECCEU, registers.accumulator);
            append_checkpoint(0xECCEU, 0xECCFU, 9U);
            pc = 0xECCFU;
            break;

        case 0xECCFU:  // LDA #$45
            load_accumulator(0x45U);
            append_checkpoint(0xECCFU, 0xECD1U, 6U);
            pc = 0xECD1U;
            break;

        case 0xECD1U:  // STA $1E
            store(0xECD1U, zp(0x1EU), registers.accumulator);
            append_checkpoint(0xECD1U, 0xECD3U, 12U);
            pc = 0xECD3U;
            break;

        case 0xECD3U:  // TAM #$04 -- MPR2
            transfer_accumulator_to_mpr(0x04U);
            append_checkpoint(0xECD3U, 0xECD5U, 15U);
            pc = 0xECD5U;
            break;

        case 0xECD5U:  // INC A
            registers.accumulator =
                static_cast<std::uint8_t>(registers.accumulator + 1U);
            registers.status = set_nz(registers.status, registers.accumulator);
            append_checkpoint(0xECD5U, 0xECD6U, 6U);
            pc = 0xECD6U;
            break;

        case 0xECD6U:  // STA $1F
            store(0xECD6U, zp(0x1FU), registers.accumulator);
            append_checkpoint(0xECD6U, 0xECD8U, 12U);
            pc = 0xECD8U;
            break;

        case 0xECD8U:  // TAM #$08 -- MPR3
            transfer_accumulator_to_mpr(0x08U);
            append_checkpoint(0xECD8U, 0xECDAU, 15U);
            pc = 0xECDAU;
            break;

        case 0xECDAU:  // INC A
            registers.accumulator =
                static_cast<std::uint8_t>(registers.accumulator + 1U);
            registers.status = set_nz(registers.status, registers.accumulator);
            append_checkpoint(0xECDAU, 0xECDBU, 6U);
            pc = 0xECDBU;
            break;

        case 0xECDBU:  // STA $20
            store(0xECDBU, zp(0x20U), registers.accumulator);
            append_checkpoint(0xECDBU, 0xECDDU, 12U);
            pc = 0xECDDU;
            break;

        case 0xECDDU:  // TAM #$10 -- MPR4
            transfer_accumulator_to_mpr(0x10U);
            append_checkpoint(0xECDDU, 0xECDFU, 15U);
            pc = 0xECDFU;
            break;

        // ---- 00:ECE2  the VBlank handler's music call, epilogue -----------
        case 0xECE2U:  // PLA
            load_accumulator(pop());
            append_checkpoint(0xECE2U, 0xECE3U, 12U);
            pc = 0xECE3U;
            break;

        case 0xECE3U:  // STA $20
            store(0xECE3U, zp(0x20U), registers.accumulator);
            append_checkpoint(0xECE3U, 0xECE5U, 12U);
            pc = 0xECE5U;
            break;

        case 0xECE5U:  // TAM #$10 -- MPR4
            transfer_accumulator_to_mpr(0x10U);
            append_checkpoint(0xECE5U, 0xECE7U, 15U);
            pc = 0xECE7U;
            break;

        case 0xECE7U:  // PLA
            load_accumulator(pop());
            append_checkpoint(0xECE7U, 0xECE8U, 12U);
            pc = 0xECE8U;
            break;

        case 0xECE8U:  // STA $1F
            store(0xECE8U, zp(0x1FU), registers.accumulator);
            append_checkpoint(0xECE8U, 0xECEAU, 12U);
            pc = 0xECEAU;
            break;

        case 0xECEAU:  // TAM #$08 -- MPR3
            transfer_accumulator_to_mpr(0x08U);
            append_checkpoint(0xECEAU, 0xECECU, 15U);
            pc = 0xECECU;
            break;

        case 0xECECU:  // PLA
            load_accumulator(pop());
            append_checkpoint(0xECECU, 0xECEDU, 12U);
            pc = 0xECEDU;
            break;

        case 0xECEDU:  // STA $1E
            store(0xECEDU, zp(0x1EU), registers.accumulator);
            append_checkpoint(0xECEDU, 0xECEFU, 12U);
            pc = 0xECEFU;
            break;

        case 0xECEFU:  // TAM #$04 -- MPR2
            transfer_accumulator_to_mpr(0x04U);
            append_checkpoint(0xECEFU, 0xECF1U, 15U);
            pc = 0xECF1U;
            break;

        // ---- 00:ECF2  enter the sequencer ---------------------------------
        // $47/$48 is the vector the JMP at ECFA reads, and this writes $4003
        // into it, so the jump lands on the sequencer's own entry in whichever
        // bank ECC6 mapped at segment 2.
        case 0xECF2U:  // LDA #$03
            load_accumulator(0x03U);
            append_checkpoint(0xECF2U, 0xECF4U, 6U);
            pc = 0xECF4U;
            break;

        case 0xECF4U:  // STA $47
            store(0xECF4U, zp(0x47U), registers.accumulator);
            append_checkpoint(0xECF4U, 0xECF6U, 12U);
            pc = 0xECF6U;
            break;

        case 0xECF6U:  // LDA #$40
            load_accumulator(0x40U);
            append_checkpoint(0xECF6U, 0xECF8U, 6U);
            pc = 0xECF8U;
            break;

        case 0xECF8U:  // STA $48
            store(0xECF8U, zp(0x48U), registers.accumulator);
            append_checkpoint(0xECF8U, 0xECFAU, 12U);
            pc = 0xECFAU;
            break;

        default:
            return std::nullopt;
        }
    }
}

} // namespace devils_crush::app::detail
