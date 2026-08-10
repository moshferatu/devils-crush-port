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

// The graphics driver's three port-streaming loops. All of 00:E3C0-E47C is one
// idiom: point a VDC or VCE write pointer at a base address, then push words
// through the data ports until a counter runs out. Nothing in the region reads
// a device register, so unlike the interrupt dispatcher above it the whole
// region is expressible.
//
//   00:E3C0  clear 512 VRAM words from $7800 through the VDC data ports
//   00:E3FC  clear 1,024 VCE colour entries through $0404/$0405
//   00:E41F  the generic word-stream decoder: $2D/$2E is the destination VRAM
//            address and $2F/$30 an MPR-mapped source pointer. Command $00
//            terminates, $01-$7F copies that many literal little-endian words,
//            and $81-$FF repeats the following word (command & $7F) times --
//            $80 repeating it 256 times.
//
// The decoder is 6.9M of the region's 7.0M master cycles across 41 calls, which
// is 167,000 cycles a call: 3,400 port writes and 12,400 instructions against a
// channel of 512 writes and 2,048 checkpoints. No transcript can hold a call, so
// this module is the first one that chunks. Six entry points, but only two
// kinds: a routine's head, and the head of the loop its transcripts stop inside.
//
//   00:E3C0 / 00:E3D7   the VRAM clear and its loop
//   00:E3FC / 00:E403   the VCE clear and its loop
//   00:E41F / 00:E439   the decoder and its stream-record loop
//
// A chunk ends in front of the backward branch that closes the loop -- 00:E3E4,
// 00:E41C, or the 00:E479 that starts the next stream record. The core executes
// that branch itself, lands on the loop head, and calls this module again for
// the next chunk. The cost is the branch, plus the CLY at 00:E438 for the
// decoder: eighteen master cycles per chunk, about 350 chunks on the route.
//
// Where the decoder chunks is a budget decision rather than a fixed count.
// 00:E479 reads the next record's command byte before committing to it and
// stops if that record would not fit in what is left of either channel, so a
// transcript never has to abandon work it has already transcribed. A record is
// at most 127 words on this route -- 258 writes and 1,036 checkpoints -- so the
// first record of a chunk always fits and the walk always makes progress. A
// $80 or $FF command would not fit; none occurs, and if one did the hard caps
// below decline the transcript and the assembly runs the record instead.
//
// The decoder's zero page is shared -- $2F/$30 in particular is the game's
// general-purpose source pointer, written from 340 sites across eight banks --
// so the transcripts are only safe if nothing writes it under them. Measured on
// an assembly write trace, bracketing each of the 41 calls between its header's
// $06 store at 00:E422 and its last data-port write: 23,300 writes to
// $2D-$30/$37/$38 inside those windows, every one from the decoder itself. No
// interrupt-context writer touches any of them.
//
// The stores at $0000-$0003 and $0402-$0405 are the VDC and VCE register files.
// The core prices the cycle each device steals, so they are transcribed at
// eighteen master cycles and sample the interrupt two CPU cycles early. Writing
// the VDC's high data port additionally wait-states the CPU while the VDC
// reaches VRAM, which is why 00:E449 measures twenty cycles and 00:E443
// eighteen; that steal lands on the real timestamp inside the declared offset,
// exactly as it does in the assembly, and is not the transcript's to predict.
constexpr std::uint8_t kBank = 0x00U;

constexpr std::uint8_t kCarry = 0x01U;
constexpr std::uint8_t kZero = 0x02U;
constexpr std::uint8_t kDecimal = 0x08U;
constexpr std::uint8_t kOverflow = 0x40U;
constexpr std::uint8_t kMemory = 0x20U;
constexpr std::uint8_t kNegative = 0x80U;

// Work-RAM window. Zero page is $2000-$20FF and the stack page is $2100-$21FF;
// both are ordinary work RAM reached through segment 1.
constexpr std::uint16_t kWorkRamBase = 0x2000U;
constexpr std::uint16_t kWorkRamEnd = 0x4000U;
constexpr std::uint16_t kStackBase = 0x2100U;

// Room left for one more stream record after a chunk decides to continue. The
// hard limits are the core's 512 writes and 2,048 checkpoints.
constexpr std::size_t kWriteBudget = 460U;
constexpr std::size_t kCheckpointBudget = 1980U;

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
    return logical_pc == 0xE3C0U || logical_pc == 0xE3D7U
        || logical_pc == 0xE3FCU || logical_pc == 0xE403U
        || logical_pc == 0xE41FU || logical_pc == 0xE439U;
}

// The three RTS instructions the core executes itself, as everywhere else in
// the port.
[[nodiscard]] constexpr bool is_return_instruction(
    const std::uint16_t logical_pc)
{
    return logical_pc == 0xE3E6U || logical_pc == 0xE41EU
        || logical_pc == 0xE47CU;
}

} // namespace

std::optional<RoutineReplacement> replace_vram_upload(
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
    // loops are written to run under; T would redirect the accumulator loads
    // through zero page and D would change the pointer advance at 00:E46F.
    if ((entry.registers.status & (kDecimal | kMemory)) != 0U) {
        return std::nullopt;
    }
    // The VDC and VCE ports only reach their devices while the hardware page is
    // mapped at segment 0 -- the same condition the core's write validator
    // imposes.
    if (entry.registers.mpr[0] != 0xFFU) {
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

    // Reads the byte a logical address currently maps to. The decoder's source
    // pointer is resolved through the live MPRs rather than assumed: the route
    // reaches it through MPR4 and MPR5 for the most part, but a stream that
    // runs past $BFFF continues through whatever the next slot holds.
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
    // offset and the instruction of the checkpoint that follows it. A push is
    // the same shape.
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
    // which is where the core's LastCycle sits. A store to a VDC or VCE port is
    // the exception: its last cycle is the one the device steals after the bus
    // write, so it samples two CPU cycles early and passes sample_back = 6.
    const auto append_checkpoint =
        [&replacement, &registers, &cycle, &commit_pending_writes](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t instruction_cycles,
            const std::uint32_t sample_back = 3U) {
            cycle += instruction_cycles;
            commit_pending_writes(cycle);
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = cycle - sample_back,
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

    // The VDC's register-select port at $0000 and its data ports at $0002 and
    // $0003, and the VCE's colour-table ports at $0402-$0405. Nothing here
    // models either device; the transcript only has to emit the writes in the
    // order and at the offsets the instructions produce them.
    const auto store_port =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t address,
            const std::uint8_t value) {
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
    // The decoder's source pointer, indexed by Y. Both halves come from the
    // snapshot, so a chunk that resumes mid-stream picks up whatever the
    // previous chunk stored.
    const auto source_byte = [&read_ram, &read_mapped, &registers, &zp]() {
        const auto pointer = static_cast<std::uint16_t>(
            read_ram(zp(0x2FU))
            | static_cast<std::uint16_t>(read_ram(zp(0x30U)) << 8U));
        return read_mapped(
            static_cast<std::uint16_t>(pointer + registers.y));
    };

    const auto load_accumulator = [&registers](const std::uint8_t value) {
        registers.accumulator = value;
        registers.status = set_nz(registers.status, value);
    };

    // ADC in binary mode, with the overflow bit the core computes. Nothing in
    // this region tests V, but a checkpoint that publishes the wrong one is
    // abandoned at the core's register comparison, so it has to be exact.
    const auto add_with_carry = [&registers](const std::uint8_t operand) {
        const auto sum = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(registers.accumulator)
            + static_cast<std::uint16_t>(operand)
            + static_cast<std::uint16_t>(registers.status & kCarry));
        const auto result = static_cast<std::uint8_t>(sum);
        const bool signed_overflow =
            ((registers.accumulator ^ result)
             & (operand ^ result) & kNegative) != 0U;
        registers.status = static_cast<std::uint8_t>(
            registers.status
            & static_cast<std::uint8_t>(~(kCarry | kOverflow)));
        if (sum > 0xFFU) {
            registers.status =
                static_cast<std::uint8_t>(registers.status | kCarry);
        }
        if (signed_overflow) {
            registers.status =
                static_cast<std::uint8_t>(registers.status | kOverflow);
        }
        registers.accumulator = result;
        registers.status = set_nz(registers.status, result);
    };

    auto pc = entry.routine.logical_pc;
    for (;;) {
        if (overflow
            || replacement.checkpoints.size() > 2000U
            || replacement.work_ram_writes.size() > 500U) {
            return std::nullopt;
        }

        const auto finish = [&](const std::uint16_t stop_pc) {
            registers.program_counter = stop_pc;
            replacement.return_instruction = {kBank, stop_pc};
            replacement.output_registers = registers;
            replacement.body_master_cycles = cycle;
        };

        if (is_return_instruction(pc)) {
            finish(pc);
            return replacement;
        }
        // A clear loop chunks whenever either channel is full. The core takes
        // the branch and re-enters at the loop head.
        if ((pc == 0xE3E4U || pc == 0xE41CU)
            && (replacement.work_ram_writes.size() >= kWriteBudget
                || replacement.checkpoints.size() >= kCheckpointBudget)) {
            finish(pc);
            return replacement;
        }
        // The decoder chunks between stream records, and only when the record
        // it would start next does not fit. Reading its command byte here is
        // the one lookahead in the walk.
        if (pc == 0xE479U && !replacement.work_ram_writes.empty()) {
            const auto pointer = static_cast<std::uint16_t>(
                read_ram(zp(0x2FU))
                | static_cast<std::uint16_t>(read_ram(zp(0x30U)) << 8U));
            const auto command = read_mapped(pointer);
            if (command != 0U) {
                const std::size_t words = (command & 0x80U) != 0U
                    ? ((command & 0x7FU) != 0U
                           ? static_cast<std::size_t>(command & 0x7FU)
                           : 256U)
                    : static_cast<std::size_t>(command);
                if (replacement.work_ram_writes.size() + 2U * words + 4U
                        > kWriteBudget
                    || replacement.checkpoints.size() + 8U * words + 24U
                        > kCheckpointBudget) {
                    finish(pc);
                    return replacement;
                }
            }
        }

        switch (pc) {
        // ---- 00:E3C0  clear 512 VRAM words from $7800 --------------------
        case 0xE3C0U:  // LDA #$00
            load_accumulator(0x00U);
            append_checkpoint(0xE3C0U, 0xE3C2U, 6U);
            pc = 0xE3C2U;
            break;

        case 0xE3C2U:  // STA $06
            store(0xE3C2U, zp(0x06U), registers.accumulator);
            append_checkpoint(0xE3C2U, 0xE3C4U, 12U);
            pc = 0xE3C4U;
            break;

        case 0xE3C4U:  // STA $0000 -- select MAWR
            store_port(0xE3C4U, 0x0000U, registers.accumulator);
            append_checkpoint(0xE3C4U, 0xE3C7U, 18U, 6U);
            pc = 0xE3C7U;
            break;

        case 0xE3C7U:  // STZ $0002
            store_port(0xE3C7U, 0x0002U, 0x00U);
            append_checkpoint(0xE3C7U, 0xE3CAU, 18U, 6U);
            pc = 0xE3CAU;
            break;

        case 0xE3CAU:  // LDA #$78
            load_accumulator(0x78U);
            append_checkpoint(0xE3CAU, 0xE3CCU, 6U);
            pc = 0xE3CCU;
            break;

        case 0xE3CCU:  // STA $0003 -- MAWR = $7800
            store_port(0xE3CCU, 0x0003U, registers.accumulator);
            append_checkpoint(0xE3CCU, 0xE3CFU, 18U, 6U);
            pc = 0xE3CFU;
            break;

        case 0xE3CFU:  // LDA #$02
            load_accumulator(0x02U);
            append_checkpoint(0xE3CFU, 0xE3D1U, 6U);
            pc = 0xE3D1U;
            break;

        case 0xE3D1U:  // STA $06
            store(0xE3D1U, zp(0x06U), registers.accumulator);
            append_checkpoint(0xE3D1U, 0xE3D3U, 12U);
            pc = 0xE3D3U;
            break;

        case 0xE3D3U:  // STA $0000 -- select VWR
            store_port(0xE3D3U, 0x0000U, registers.accumulator);
            append_checkpoint(0xE3D3U, 0xE3D6U, 18U, 6U);
            pc = 0xE3D6U;
            break;

        case 0xE3D6U:  // CLY -- 256 iterations of two words
            registers.y = 0U;
            append_checkpoint(0xE3D6U, 0xE3D7U, 6U);
            pc = 0xE3D7U;
            break;

        case 0xE3D7U:  // STZ $0002
            store_port(0xE3D7U, 0x0002U, 0x00U);
            append_checkpoint(0xE3D7U, 0xE3DAU, 18U, 6U);
            pc = 0xE3DAU;
            break;

        case 0xE3DAU:  // STZ $0003
            store_port(0xE3DAU, 0x0003U, 0x00U);
            append_checkpoint(0xE3DAU, 0xE3DDU, 18U, 6U);
            pc = 0xE3DDU;
            break;

        case 0xE3DDU:  // STZ $0002
            store_port(0xE3DDU, 0x0002U, 0x00U);
            append_checkpoint(0xE3DDU, 0xE3E0U, 18U, 6U);
            pc = 0xE3E0U;
            break;

        case 0xE3E0U:  // STZ $0003
            store_port(0xE3E0U, 0x0003U, 0x00U);
            append_checkpoint(0xE3E0U, 0xE3E3U, 18U, 6U);
            pc = 0xE3E3U;
            break;

        case 0xE3E3U:  // DEY
            registers.y = static_cast<std::uint8_t>(registers.y - 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE3E3U, 0xE3E4U, 6U);
            pc = 0xE3E4U;
            break;

        case 0xE3E4U: {  // BNE $E3D7
            const bool taken = (registers.status & kZero) == 0U;
            const std::uint16_t next = taken ? 0xE3D7U : 0xE3E6U;
            append_checkpoint(0xE3E4U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        // ---- 00:E3FC  clear the VCE colour table -------------------------
        case 0xE3FCU:  // STZ $0402 -- colour-table address low
            store_port(0xE3FCU, 0x0402U, 0x00U);
            append_checkpoint(0xE3FCU, 0xE3FFU, 18U, 6U);
            pc = 0xE3FFU;
            break;

        case 0xE3FFU:  // STZ $0403
            store_port(0xE3FFU, 0x0403U, 0x00U);
            append_checkpoint(0xE3FFU, 0xE402U, 18U, 6U);
            pc = 0xE402U;
            break;

        case 0xE402U:  // CLY -- 256 iterations of four entries
            registers.y = 0U;
            append_checkpoint(0xE402U, 0xE403U, 6U);
            pc = 0xE403U;
            break;

        case 0xE403U:  // STZ $0404
            store_port(0xE403U, 0x0404U, 0x00U);
            append_checkpoint(0xE403U, 0xE406U, 18U, 6U);
            pc = 0xE406U;
            break;

        case 0xE406U:  // STZ $0405
            store_port(0xE406U, 0x0405U, 0x00U);
            append_checkpoint(0xE406U, 0xE409U, 18U, 6U);
            pc = 0xE409U;
            break;

        case 0xE409U:  // STZ $0404
            store_port(0xE409U, 0x0404U, 0x00U);
            append_checkpoint(0xE409U, 0xE40CU, 18U, 6U);
            pc = 0xE40CU;
            break;

        case 0xE40CU:  // STZ $0405
            store_port(0xE40CU, 0x0405U, 0x00U);
            append_checkpoint(0xE40CU, 0xE40FU, 18U, 6U);
            pc = 0xE40FU;
            break;

        case 0xE40FU:  // STZ $0404
            store_port(0xE40FU, 0x0404U, 0x00U);
            append_checkpoint(0xE40FU, 0xE412U, 18U, 6U);
            pc = 0xE412U;
            break;

        case 0xE412U:  // STZ $0405
            store_port(0xE412U, 0x0405U, 0x00U);
            append_checkpoint(0xE412U, 0xE415U, 18U, 6U);
            pc = 0xE415U;
            break;

        case 0xE415U:  // STZ $0404
            store_port(0xE415U, 0x0404U, 0x00U);
            append_checkpoint(0xE415U, 0xE418U, 18U, 6U);
            pc = 0xE418U;
            break;

        case 0xE418U:  // STZ $0405
            store_port(0xE418U, 0x0405U, 0x00U);
            append_checkpoint(0xE418U, 0xE41BU, 18U, 6U);
            pc = 0xE41BU;
            break;

        case 0xE41BU:  // DEY
            registers.y = static_cast<std::uint8_t>(registers.y - 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE41BU, 0xE41CU, 6U);
            pc = 0xE41CU;
            break;

        case 0xE41CU: {  // BNE $E403
            const bool taken = (registers.status & kZero) == 0U;
            const std::uint16_t next = taken ? 0xE403U : 0xE41EU;
            append_checkpoint(0xE41CU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        // ---- 00:E41F  the generic word-stream decoder --------------------
        case 0xE41FU:  // PHY
            push(0xE41FU, registers.y);
            append_checkpoint(0xE41FU, 0xE420U, 9U);
            pc = 0xE420U;
            break;

        case 0xE420U:  // LDA #$00
            load_accumulator(0x00U);
            append_checkpoint(0xE420U, 0xE422U, 6U);
            pc = 0xE422U;
            break;

        case 0xE422U:  // STA $06
            store(0xE422U, zp(0x06U), registers.accumulator);
            append_checkpoint(0xE422U, 0xE424U, 12U);
            pc = 0xE424U;
            break;

        case 0xE424U:  // STA $0000 -- select MAWR
            store_port(0xE424U, 0x0000U, registers.accumulator);
            append_checkpoint(0xE424U, 0xE427U, 18U, 6U);
            pc = 0xE427U;
            break;

        case 0xE427U:  // LDA $2D
            load_accumulator(read_ram(zp(0x2DU)));
            append_checkpoint(0xE427U, 0xE429U, 12U);
            pc = 0xE429U;
            break;

        case 0xE429U:  // STA $0002
            store_port(0xE429U, 0x0002U, registers.accumulator);
            append_checkpoint(0xE429U, 0xE42CU, 18U, 6U);
            pc = 0xE42CU;
            break;

        case 0xE42CU:  // LDA $2E
            load_accumulator(read_ram(zp(0x2EU)));
            append_checkpoint(0xE42CU, 0xE42EU, 12U);
            pc = 0xE42EU;
            break;

        case 0xE42EU:  // STA $0003 -- MAWR = $2D:$2E
            store_port(0xE42EU, 0x0003U, registers.accumulator);
            append_checkpoint(0xE42EU, 0xE431U, 18U, 6U);
            pc = 0xE431U;
            break;

        case 0xE431U:  // LDA #$02
            load_accumulator(0x02U);
            append_checkpoint(0xE431U, 0xE433U, 6U);
            pc = 0xE433U;
            break;

        case 0xE433U:  // STA $06
            store(0xE433U, zp(0x06U), registers.accumulator);
            append_checkpoint(0xE433U, 0xE435U, 12U);
            pc = 0xE435U;
            break;

        case 0xE435U:  // STA $0000 -- select VWR
            store_port(0xE435U, 0x0000U, registers.accumulator);
            append_checkpoint(0xE435U, 0xE438U, 18U, 6U);
            pc = 0xE438U;
            break;

        case 0xE438U:  // CLY -- every record is indexed from its own head
            registers.y = 0U;
            append_checkpoint(0xE438U, 0xE439U, 6U);
            pc = 0xE439U;
            break;

        case 0xE439U:  // LDA ($2F),Y -- the record's command byte
            load_accumulator(source_byte());
            append_checkpoint(0xE439U, 0xE43BU, 21U);
            pc = 0xE43BU;
            break;

        case 0xE43BU: {  // BEQ $E47B -- command $00 terminates
            const bool taken = (registers.status & kZero) != 0U;
            const std::uint16_t next = taken ? 0xE47BU : 0xE43DU;
            append_checkpoint(0xE43BU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xE43DU: {  // BMI $E452 -- commands $80-$FF repeat
            const bool taken = (registers.status & kNegative) != 0U;
            const std::uint16_t next = taken ? 0xE452U : 0xE43FU;
            append_checkpoint(0xE43DU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xE43FU:  // TAX -- literal word count
            registers.x = registers.accumulator;
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0xE43FU, 0xE440U, 6U);
            pc = 0xE440U;
            break;

        case 0xE440U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE440U, 0xE441U, 6U);
            pc = 0xE441U;
            break;

        case 0xE441U:  // LDA ($2F),Y
            load_accumulator(source_byte());
            append_checkpoint(0xE441U, 0xE443U, 21U);
            pc = 0xE443U;
            break;

        case 0xE443U:  // STA $0002
            store_port(0xE443U, 0x0002U, registers.accumulator);
            append_checkpoint(0xE443U, 0xE446U, 18U, 6U);
            pc = 0xE446U;
            break;

        case 0xE446U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE446U, 0xE447U, 6U);
            pc = 0xE447U;
            break;

        case 0xE447U:  // LDA ($2F),Y
            load_accumulator(source_byte());
            append_checkpoint(0xE447U, 0xE449U, 21U);
            pc = 0xE449U;
            break;

        case 0xE449U:  // STA $0003 -- the word reaches VRAM here
            store_port(0xE449U, 0x0003U, registers.accumulator);
            append_checkpoint(0xE449U, 0xE44CU, 18U, 6U);
            pc = 0xE44CU;
            break;

        case 0xE44CU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE44CU, 0xE44DU, 6U);
            pc = 0xE44DU;
            break;

        case 0xE44DU:  // DEX
            registers.x = static_cast<std::uint8_t>(registers.x - 1U);
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0xE44DU, 0xE44EU, 6U);
            pc = 0xE44EU;
            break;

        case 0xE44EU: {  // BNE $E441
            const bool taken = (registers.status & kZero) == 0U;
            const std::uint16_t next = taken ? 0xE441U : 0xE450U;
            append_checkpoint(0xE44EU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xE450U:  // BRA $E46D
            append_checkpoint(0xE450U, 0xE46DU, 12U);
            pc = 0xE46DU;
            break;

        case 0xE452U:  // AND #$7F -- repeat count, zero meaning 256
            registers.accumulator =
                static_cast<std::uint8_t>(registers.accumulator & 0x7FU);
            registers.status = set_nz(registers.status, registers.accumulator);
            append_checkpoint(0xE452U, 0xE454U, 6U);
            pc = 0xE454U;
            break;

        case 0xE454U:  // TAX
            registers.x = registers.accumulator;
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0xE454U, 0xE455U, 6U);
            pc = 0xE455U;
            break;

        case 0xE455U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE455U, 0xE456U, 6U);
            pc = 0xE456U;
            break;

        case 0xE456U:  // LDA ($2F),Y
            load_accumulator(source_byte());
            append_checkpoint(0xE456U, 0xE458U, 21U);
            pc = 0xE458U;
            break;

        case 0xE458U:  // STA $37 -- the repeated word's low byte
            store(0xE458U, zp(0x37U), registers.accumulator);
            append_checkpoint(0xE458U, 0xE45AU, 12U);
            pc = 0xE45AU;
            break;

        case 0xE45AU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE45AU, 0xE45BU, 6U);
            pc = 0xE45BU;
            break;

        case 0xE45BU:  // LDA ($2F),Y
            load_accumulator(source_byte());
            append_checkpoint(0xE45BU, 0xE45DU, 21U);
            pc = 0xE45DU;
            break;

        case 0xE45DU:  // STA $38
            store(0xE45DU, zp(0x38U), registers.accumulator);
            append_checkpoint(0xE45DU, 0xE45FU, 12U);
            pc = 0xE45FU;
            break;

        case 0xE45FU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE45FU, 0xE460U, 6U);
            pc = 0xE460U;
            break;

        case 0xE460U:  // LDA $37
            load_accumulator(read_ram(zp(0x37U)));
            append_checkpoint(0xE460U, 0xE462U, 12U);
            pc = 0xE462U;
            break;

        case 0xE462U:  // STA $0002
            store_port(0xE462U, 0x0002U, registers.accumulator);
            append_checkpoint(0xE462U, 0xE465U, 18U, 6U);
            pc = 0xE465U;
            break;

        case 0xE465U:  // LDA $38
            load_accumulator(read_ram(zp(0x38U)));
            append_checkpoint(0xE465U, 0xE467U, 12U);
            pc = 0xE467U;
            break;

        case 0xE467U:  // STA $0003
            store_port(0xE467U, 0x0003U, registers.accumulator);
            append_checkpoint(0xE467U, 0xE46AU, 18U, 6U);
            pc = 0xE46AU;
            break;

        case 0xE46AU:  // DEX
            registers.x = static_cast<std::uint8_t>(registers.x - 1U);
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0xE46AU, 0xE46BU, 6U);
            pc = 0xE46BU;
            break;

        case 0xE46BU: {  // BNE $E460
            const bool taken = (registers.status & kZero) == 0U;
            const std::uint16_t next = taken ? 0xE460U : 0xE46DU;
            append_checkpoint(0xE46BU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xE46DU:  // TYA -- advance the source pointer past the record
            load_accumulator(registers.y);
            append_checkpoint(0xE46DU, 0xE46EU, 6U);
            pc = 0xE46EU;
            break;

        case 0xE46EU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xE46EU, 0xE46FU, 6U);
            pc = 0xE46FU;
            break;

        case 0xE46FU:  // ADC $2F
            add_with_carry(read_ram(zp(0x2FU)));
            append_checkpoint(0xE46FU, 0xE471U, 12U);
            pc = 0xE471U;
            break;

        case 0xE471U:  // STA $2F
            store(0xE471U, zp(0x2FU), registers.accumulator);
            append_checkpoint(0xE471U, 0xE473U, 12U);
            pc = 0xE473U;
            break;

        case 0xE473U:  // LDA $30
            load_accumulator(read_ram(zp(0x30U)));
            append_checkpoint(0xE473U, 0xE475U, 12U);
            pc = 0xE475U;
            break;

        case 0xE475U:  // ADC #$00
            add_with_carry(0x00U);
            append_checkpoint(0xE475U, 0xE477U, 6U);
            pc = 0xE477U;
            break;

        case 0xE477U:  // STA $30
            store(0xE477U, zp(0x30U), registers.accumulator);
            append_checkpoint(0xE477U, 0xE479U, 12U);
            pc = 0xE479U;
            break;

        case 0xE479U:  // BRA $E438
            append_checkpoint(0xE479U, 0xE438U, 12U);
            pc = 0xE438U;
            break;

        case 0xE47BU:  // PLY
            registers.y = pop();
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE47BU, 0xE47CU, 12U);
            pc = 0xE47CU;
            break;

        default:
            return std::nullopt;
        }
    }
}

} // namespace devils_crush::app::detail
