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

// Bank 45 is the music sequencer. It is a separate component from the timer
// sound driver at 00:FC00: the driver streams DDA samples, the sequencer
// decides what they are, programming the PSG register file at $0801-$0809 and
// maintaining the per-voice records in $2E00-$2FFF.
//
// Unlike the driver it is not flat. Its per-frame tick at 45:4015 walks the
// voices and calls sixteen subroutines, so the port is incremental: each entry
// below is a subroutine registered on its own, and the walk inlines any
// nested call it makes. The tick itself, and the eight subroutines it reaches
// that are not listed here, stay emulated for now.
//
// This slice ports the sample/envelope group -- the routines the voice walk
// calls once per voice per frame, which is where the bank's time actually
// goes:
//
//   45:48B4  sequence-data fetch, the shared command decoder
//   45:4883  pitch/period update           (inlines 48B4)
//   45:48F6  sweep accumulator update      (inlines 48B4)
//   45:49A2  vibrato accumulator update
//   45:49B6  voice-latch clear
//   45:49F9  voice commit, which programs $0800/$0804
//   45:4211  the two pending-event gates
//
// Together they are 22.9M of the bank's 46.1M master cycles on the census
// route.
constexpr std::uint8_t kBank = 0x45U;

constexpr std::uint8_t kCarry = 0x01U;
constexpr std::uint8_t kZero = 0x02U;
constexpr std::uint8_t kInterruptDisable = 0x04U;
constexpr std::uint8_t kDecimal = 0x08U;
constexpr std::uint8_t kMemory = 0x20U;
constexpr std::uint8_t kNegative = 0x80U;

// Work-RAM window. Zero page is $2000-$20FF and the sequencer's voice records
// occupy $2E00-$2FFF; the walk keeps a private copy of the whole 8KB so it can
// read back its own stores.
constexpr std::uint16_t kWorkRamBase = 0x2000U;
constexpr std::uint16_t kWorkRamEnd = 0x4000U;

// PSG ports the voice commit drives directly.
constexpr std::uint16_t kPsgChannelSelect = 0x0800U;
constexpr std::uint16_t kPsgAmplitude = 0x0804U;

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

// The entry points this module owns. Anything else declines before the walk
// starts, so an unrelated routine in the bank is never mistaken for one of
// these.
[[nodiscard]] constexpr bool is_owned_entry(const std::uint16_t logical_pc)
{
    return logical_pc == 0x4211U
        || logical_pc == 0x4883U
        || logical_pc == 0x48B4U
        || logical_pc == 0x48F6U
        || logical_pc == 0x49A2U
        || logical_pc == 0x49B6U
        || logical_pc == 0x49F9U;
}

// Every RTS in the ported set. At call depth zero the walk stops in front of
// one of these and hands the return itself back to the core, which is the
// established shape for a leaf transcript; deeper than that the walk executes
// it, because the matching JSR was one of its own.
[[nodiscard]] constexpr bool is_return_instruction(const std::uint16_t pc)
{
    return pc == 0x4227U
        || pc == 0x48ACU || pc == 0x48B3U
        || pc == 0x48B8U || pc == 0x48C0U || pc == 0x48CDU || pc == 0x48D6U
        || pc == 0x48E5U || pc == 0x48F5U
        || pc == 0x4935U
        || pc == 0x49B5U
        || pc == 0x49D1U
        || pc == 0x49F8U
        || pc == 0x4A28U
        || pc == 0x4A9BU;
}

}  // namespace

std::optional<RoutineReplacement> replace_music_sequencer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    if (entry.routine.physical_bank != kBank
        || !is_owned_entry(entry.routine.logical_pc)
        || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    // None of these routines is written to run in decimal mode or under the
    // memory-operand T flag; both would change ADC/SBC and the loads.
    if ((entry.registers.status & (kDecimal | kMemory)) != 0U) {
        return std::nullopt;
    }
    // The sequencer reaches its command tables through MPR2, which has to be
    // this bank for the walk's ROM reads to mean anything.
    if (entry.registers.mpr[2] != kBank) {
        return std::nullopt;
    }

    std::uint8_t ram[0x2000];
    for (std::size_t index = 0U; index < 0x2000U; ++index) {
        ram[index] = std::to_integer<std::uint8_t>(work_ram[index]);
    }

    RoutineReplacement replacement{};
    replacement.work_ram_writes.reserve(24U);
    replacement.checkpoints.reserve(96U);

    auto registers = entry.registers;
    std::uint32_t cycle = 0U;
    // Set by any read or write the walk cannot resolve. Checked at the top of
    // every step, so a bad address declines instead of guessing.
    bool overflow = false;
    // JSR/RTS nesting inside the transcript. Zero means the next RTS belongs
    // to the caller and terminates the walk.
    unsigned depth = 0U;

    const auto read_ram =
        [&ram, &overflow](const std::uint16_t address) -> std::uint8_t {
            if (address < kWorkRamBase || address >= kWorkRamEnd) {
                overflow = true;
                return 0U;
            }
            return ram[address - kWorkRamBase];
        };

    // Reads the byte a logical address currently maps to. The command tables
    // at $5B6E/$5C2F/$5D81 live in this bank, reached through MPR2, but the
    // pointer the tables install is data-driven, so the mapping is resolved
    // generally rather than assumed.
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
    // offset and the instruction of its checkpoint. They are recorded here
    // and priced by the next append_checkpoint, once the instruction's cycle
    // count is known.
    std::size_t committed_writes = 0U;
    // SEI and CLI both apply the flag after the core has already sampled the
    // interrupt for that instruction, so the checkpoint at a SEI still
    // publishes the old mask and the change lands on the following one.
    bool status_change_pending = false;
    std::uint8_t pending_status = 0U;

    const auto append_checkpoint =
        [&replacement,
         &registers,
         &cycle,
         &committed_writes,
         &status_change_pending,
         &pending_status](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t instruction_cycles) {
            cycle += instruction_cycles;
            while (committed_writes < replacement.work_ram_writes.size()) {
                replacement.work_ram_writes[committed_writes]
                    .master_cycle_offset = cycle;
                ++committed_writes;
            }
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = cycle - 3U,
                .master_cycle_offset = cycle,
                .instruction = {kBank, instruction_pc},
                .registers = registers,
            });
            if (status_change_pending) {
                registers.status = pending_status;
                status_change_pending = false;
            }
        };

    const auto store =
        [&replacement, &ram](
            const std::uint16_t instruction_pc,
            const std::uint16_t address,
            const std::uint8_t value) {
            if (address >= kWorkRamBase && address < kWorkRamEnd) {
                ram[address - kWorkRamBase] = value;
            }
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
                static_cast<std::uint16_t>(
                    0x2100U + registers.stack_pointer),
                value);
            registers.stack_pointer = static_cast<std::uint8_t>(
                registers.stack_pointer - 1U);
        };

    const auto pull = [&registers, &ram]() -> std::uint8_t {
        registers.stack_pointer = static_cast<std::uint8_t>(
            registers.stack_pointer + 1U);
        return ram[0x0100U + registers.stack_pointer];
    };

    // Zero page lives at $2000; an absolute address is used as written.
    const auto zp = [](const std::uint8_t address) {
        return static_cast<std::uint16_t>(kWorkRamBase + address);
    };
    const auto indexed_x = [&registers](const std::uint16_t base) {
        return static_cast<std::uint16_t>(
            base + static_cast<std::uint16_t>(registers.x));
    };
    const auto indexed_y = [&registers](const std::uint16_t base) {
        return static_cast<std::uint16_t>(
            base + static_cast<std::uint16_t>(registers.y));
    };
    // ($D2),Y: the 16-bit command pointer the tables install, plus Y. The
    // core adds Y without wrapping, so a sum past $FFFF would index outside
    // the logical address space; that declines rather than guessing.
    const auto command_pointer = [&ram, &registers, &overflow]() {
        const auto base = static_cast<std::uint32_t>(
            static_cast<std::uint16_t>(ram[0x00D2U])
            | static_cast<std::uint16_t>(ram[0x00D3U]) << 8U);
        const auto target = base + static_cast<std::uint32_t>(registers.y);
        if (target > 0xFFFFU) {
            overflow = true;
            return static_cast<std::uint16_t>(0U);
        }
        return static_cast<std::uint16_t>(target);
    };

    const auto load_accumulator = [&registers](const std::uint8_t value) {
        registers.accumulator = value;
        registers.status = set_nz(registers.status, value);
    };
    const auto compare =
        [&registers](const std::uint8_t lhs, const std::uint8_t rhs) {
            const auto difference = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(lhs)
                - static_cast<std::uint16_t>(rhs));
            registers.status = set_nz(
                registers.status,
                static_cast<std::uint8_t>(difference));
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            if (lhs >= rhs) {
                registers.status = static_cast<std::uint8_t>(
                    registers.status | kCarry);
            }
        };
    const auto add_with_carry = [&registers](const std::uint8_t operand) {
        const auto sum = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(registers.accumulator)
            + static_cast<std::uint16_t>(operand)
            + static_cast<std::uint16_t>(registers.status & kCarry));
        auto status = static_cast<std::uint8_t>(
            registers.status & static_cast<std::uint8_t>(~(0x43U | kNegative)));
        const auto overflow_bit = static_cast<std::uint8_t>(
            ((((registers.accumulator ^ operand) & 0x80U) ^ 0x80U)
             & ((registers.accumulator ^ sum) & 0x80U))
            >> 1U);
        status = static_cast<std::uint8_t>(status | overflow_bit);
        status = static_cast<std::uint8_t>(
            status | static_cast<std::uint8_t>((sum >> 8U) & kCarry));
        registers.accumulator = static_cast<std::uint8_t>(sum);
        registers.status = set_nz(status, registers.accumulator);
    };
    const auto subtract_with_carry = [&registers](const std::uint8_t operand) {
        const auto difference = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(registers.accumulator)
            - static_cast<std::uint16_t>(operand)
            - static_cast<std::uint16_t>((registers.status & kCarry) ^ kCarry));
        auto status = static_cast<std::uint8_t>(
            registers.status & static_cast<std::uint8_t>(~(0x43U | kNegative)));
        const auto overflow_bit = static_cast<std::uint8_t>(
            ((registers.accumulator ^ difference)
             & (registers.accumulator ^ operand) & 0x80U)
            >> 1U);
        status = static_cast<std::uint8_t>(status | overflow_bit);
        status = static_cast<std::uint8_t>(
            status
            | static_cast<std::uint8_t>(
                (static_cast<std::uint8_t>((difference >> 8U) & kCarry))
                ^ kCarry));
        registers.accumulator = static_cast<std::uint8_t>(difference);
        registers.status = set_nz(status, registers.accumulator);
    };

    const auto branch =
        [&append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t taken_pc,
            const std::uint16_t fallthrough_pc,
            const bool taken) {
            const auto next = taken ? taken_pc : fallthrough_pc;
            append_checkpoint(instruction_pc, next, taken ? 12U : 6U);
            return next;
        };

    auto pc = entry.routine.logical_pc;
    for (;;) {
        if (overflow
            || replacement.checkpoints.size() > 512U
            || replacement.work_ram_writes.size() > 128U) {
            return std::nullopt;
        }

        // A return at the outermost level ends the transcript in front of the
        // RTS, which the core then executes itself.
        if (depth == 0U && is_return_instruction(pc)) {
            registers.program_counter = pc;
            replacement.return_instruction = {kBank, pc};
            replacement.output_registers = registers;
            replacement.body_master_cycles = cycle;
            return replacement;
        }

        switch (pc) {
        // ---- 45:4211  pending-event gates -------------------------------
        case 0x4211U:  // LDA $2E02
            load_accumulator(read_ram(0x2E02U));
            append_checkpoint(0x4211U, 0x4214U, 15U);
            pc = 0x4214U;
            break;

        case 0x4214U:  // BEQ $421C
            pc = branch(
                0x4214U,
                0x421CU,
                0x4216U,
                (registers.status & kZero) != 0U);
            break;

        case 0x421CU:  // LDA $2E03
            load_accumulator(read_ram(0x2E03U));
            append_checkpoint(0x421CU, 0x421FU, 15U);
            pc = 0x421FU;
            break;

        case 0x421FU:  // BEQ $4227
            pc = branch(
                0x421FU,
                0x4227U,
                0x4221U,
                (registers.status & kZero) != 0U);
            break;

        // ---- 45:4883  pitch/period update -------------------------------
        case 0x4883U:  // LDA $2E5A,X
            load_accumulator(read_ram(indexed_x(0x2E5AU)));
            append_checkpoint(0x4883U, 0x4886U, 15U);
            pc = 0x4886U;
            break;

        case 0x4886U: {  // ASL A
            const auto shifted = static_cast<std::uint8_t>(
                registers.accumulator << 1U);
            auto status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            status = static_cast<std::uint8_t>(
                status
                | static_cast<std::uint8_t>(registers.accumulator >> 7U));
            registers.accumulator = shifted;
            registers.status = set_nz(status, shifted);
            append_checkpoint(0x4886U, 0x4887U, 6U);
            pc = 0x4887U;
            break;
        }

        case 0x4887U:  // BEQ $48AD
            pc = branch(
                0x4887U,
                0x48ADU,
                0x4889U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4889U:  // TAY
            registers.y = registers.accumulator;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4889U, 0x488AU, 6U);
            pc = 0x488AU;
            break;

        case 0x488AU:  // LDA $5C2F,Y
            load_accumulator(read_mapped(indexed_y(0x5C2FU)));
            append_checkpoint(0x488AU, 0x488DU, 15U);
            pc = 0x488DU;
            break;

        case 0x488DU:  // STA $D2
            store(0x488DU, zp(0xD2U), registers.accumulator);
            append_checkpoint(0x488DU, 0x488FU, 12U);
            pc = 0x488FU;
            break;

        case 0x488FU:  // LDA $5C30,Y
            load_accumulator(read_mapped(indexed_y(0x5C30U)));
            append_checkpoint(0x488FU, 0x4892U, 15U);
            pc = 0x4892U;
            break;

        case 0x4892U:  // STA $D3
            store(0x4892U, zp(0xD3U), registers.accumulator);
            append_checkpoint(0x4892U, 0x4894U, 12U);
            pc = 0x4894U;
            break;

        case 0x4894U:  // LDY $2F72,X
            registers.y = read_ram(indexed_x(0x2F72U));
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4894U, 0x4897U, 15U);
            pc = 0x4897U;
            break;

        case 0x4897U:  // JSR $48B4
            push(0x4897U, 0x48U);
            push(0x4897U, 0x99U);
            ++depth;
            append_checkpoint(0x4897U, 0x48B4U, 21U);
            pc = 0x48B4U;
            break;

        case 0x489AU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x489AU, 0x489BU, 6U);
            pc = 0x489BU;
            break;

        case 0x489BU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0x489BU, 0x489CU, 6U);
            pc = 0x489CU;
            break;

        case 0x489CU:  // ADC $2E50,X
            add_with_carry(read_ram(indexed_x(0x2E50U)));
            append_checkpoint(0x489CU, 0x489FU, 15U);
            pc = 0x489FU;
            break;

        case 0x489FU:  // SBC #$1E
            subtract_with_carry(0x1EU);
            append_checkpoint(0x489FU, 0x48A1U, 6U);
            pc = 0x48A1U;
            break;

        case 0x48A1U:  // BCS $48A5
            pc = branch(
                0x48A1U,
                0x48A5U,
                0x48A3U,
                (registers.status & kCarry) != 0U);
            break;

        case 0x48A3U:  // LDA #$00
            load_accumulator(0x00U);
            append_checkpoint(0x48A3U, 0x48A5U, 6U);
            pc = 0x48A5U;
            break;

        case 0x48A5U:  // STA $2E37
            store(0x48A5U, 0x2E37U, registers.accumulator);
            append_checkpoint(0x48A5U, 0x48A8U, 15U);
            pc = 0x48A8U;
            break;

        case 0x48A8U:  // TYA
            load_accumulator(registers.y);
            append_checkpoint(0x48A8U, 0x48A9U, 6U);
            pc = 0x48A9U;
            break;

        case 0x48A9U:  // STA $2F72,X
            store(0x48A9U, indexed_x(0x2F72U), registers.accumulator);
            append_checkpoint(0x48A9U, 0x48ACU, 15U);
            pc = 0x48ACU;
            break;

        case 0x48ADU:  // LDA $2E50,X
            load_accumulator(read_ram(indexed_x(0x2E50U)));
            append_checkpoint(0x48ADU, 0x48B0U, 15U);
            pc = 0x48B0U;
            break;

        case 0x48B0U:  // STA $2E37
            store(0x48B0U, 0x2E37U, registers.accumulator);
            append_checkpoint(0x48B0U, 0x48B3U, 15U);
            pc = 0x48B3U;
            break;

        // ---- 45:48B4  sequence-data fetch -------------------------------
        // The shared command decoder. Values below $80 are literal data; the
        // five commands $80-$84 re-read the stream relative to the cursor.
        case 0x48B4U:  // LDA ($D2),Y
            load_accumulator(read_mapped(command_pointer()));
            append_checkpoint(0x48B4U, 0x48B6U, 21U);
            pc = 0x48B6U;
            break;

        case 0x48B6U:  // BMI $48B9
            pc = branch(
                0x48B6U,
                0x48B9U,
                0x48B8U,
                (registers.status & kNegative) != 0U);
            break;

        case 0x48B9U:  // CMP #$80
            compare(registers.accumulator, 0x80U);
            append_checkpoint(0x48B9U, 0x48BBU, 6U);
            pc = 0x48BBU;
            break;

        case 0x48BBU:  // BNE $48C1
            pc = branch(
                0x48BBU,
                0x48C1U,
                0x48BDU,
                (registers.status & kZero) == 0U);
            break;

        case 0x48BDU:  // DEY
            registers.y = static_cast<std::uint8_t>(registers.y - 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x48BDU, 0x48BEU, 6U);
            pc = 0x48BEU;
            break;

        case 0x48BEU:  // LDA ($D2),Y
            load_accumulator(read_mapped(command_pointer()));
            append_checkpoint(0x48BEU, 0x48C0U, 21U);
            pc = 0x48C0U;
            break;

        case 0x48C1U:  // CMP #$81
            compare(registers.accumulator, 0x81U);
            append_checkpoint(0x48C1U, 0x48C3U, 6U);
            pc = 0x48C3U;
            break;

        case 0x48C3U:  // BNE $48CE
            pc = branch(
                0x48C3U,
                0x48CEU,
                0x48C5U,
                (registers.status & kZero) == 0U);
            break;

        case 0x48C5U:  // TYA
            load_accumulator(registers.y);
            append_checkpoint(0x48C5U, 0x48C6U, 6U);
            pc = 0x48C6U;
            break;

        case 0x48C6U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x48C6U, 0x48C7U, 6U);
            pc = 0x48C7U;
            break;

        case 0x48C7U:  // SEC
            registers.status = static_cast<std::uint8_t>(
                registers.status | kCarry);
            append_checkpoint(0x48C7U, 0x48C8U, 6U);
            pc = 0x48C8U;
            break;

        case 0x48C8U:  // SBC ($D2),Y
            subtract_with_carry(read_mapped(command_pointer()));
            append_checkpoint(0x48C8U, 0x48CAU, 21U);
            pc = 0x48CAU;
            break;

        case 0x48CAU:  // TAY
            registers.y = registers.accumulator;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x48CAU, 0x48CBU, 6U);
            pc = 0x48CBU;
            break;

        case 0x48CBU:  // LDA ($D2),Y
            load_accumulator(read_mapped(command_pointer()));
            append_checkpoint(0x48CBU, 0x48CDU, 21U);
            pc = 0x48CDU;
            break;

        case 0x48CEU:  // CMP #$82
            compare(registers.accumulator, 0x82U);
            append_checkpoint(0x48CEU, 0x48D0U, 6U);
            pc = 0x48D0U;
            break;

        case 0x48D0U:  // BNE $48D7
            pc = branch(
                0x48D0U,
                0x48D7U,
                0x48D2U,
                (registers.status & kZero) == 0U);
            break;

        case 0x48D2U:  // LDY #$00
            registers.y = 0x00U;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x48D2U, 0x48D4U, 6U);
            pc = 0x48D4U;
            break;

        case 0x48D4U:  // LDA ($D2),Y
            load_accumulator(read_mapped(command_pointer()));
            append_checkpoint(0x48D4U, 0x48D6U, 21U);
            pc = 0x48D6U;
            break;

        case 0x48D7U:  // CMP #$83
            compare(registers.accumulator, 0x83U);
            append_checkpoint(0x48D7U, 0x48D9U, 6U);
            pc = 0x48D9U;
            break;

        case 0x48D9U:  // BNE $48E6
            pc = branch(
                0x48D9U,
                0x48E6U,
                0x48DBU,
                (registers.status & kZero) == 0U);
            break;

        case 0x48DBU:  // LDA $2EB4,X
            load_accumulator(read_ram(indexed_x(0x2EB4U)));
            append_checkpoint(0x48DBU, 0x48DEU, 15U);
            pc = 0x48DEU;
            break;

        case 0x48DEU:  // AND #$04
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x04U));
            append_checkpoint(0x48DEU, 0x48E0U, 6U);
            pc = 0x48E0U;
            break;

        case 0x48E0U:  // BNE $48BD
            pc = branch(
                0x48E0U,
                0x48BDU,
                0x48E2U,
                (registers.status & kZero) == 0U);
            break;

        case 0x48E2U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x48E2U, 0x48E3U, 6U);
            pc = 0x48E3U;
            break;

        case 0x48E3U:  // LDA ($D2),Y
            load_accumulator(read_mapped(command_pointer()));
            append_checkpoint(0x48E3U, 0x48E5U, 21U);
            pc = 0x48E5U;
            break;

        case 0x48E6U:  // CMP #$84
            compare(registers.accumulator, 0x84U);
            append_checkpoint(0x48E6U, 0x48E8U, 6U);
            pc = 0x48E8U;
            break;

        case 0x48E8U:  // BNE $48E5
            pc = branch(
                0x48E8U,
                0x48E5U,
                0x48EAU,
                (registers.status & kZero) == 0U);
            break;

        case 0x48EAU:  // LDA $2EB4,X
            load_accumulator(read_ram(indexed_x(0x2EB4U)));
            append_checkpoint(0x48EAU, 0x48EDU, 15U);
            pc = 0x48EDU;
            break;

        case 0x48EDU:  // AND #$04
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x04U));
            append_checkpoint(0x48EDU, 0x48EFU, 6U);
            pc = 0x48EFU;
            break;

        case 0x48EFU:  // BNE $48C5
            pc = branch(
                0x48EFU,
                0x48C5U,
                0x48F1U,
                (registers.status & kZero) == 0U);
            break;

        case 0x48F1U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x48F1U, 0x48F2U, 6U);
            pc = 0x48F2U;
            break;

        case 0x48F2U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x48F2U, 0x48F3U, 6U);
            pc = 0x48F3U;
            break;

        case 0x48F3U:  // LDA ($D2),Y
            load_accumulator(read_mapped(command_pointer()));
            append_checkpoint(0x48F3U, 0x48F5U, 21U);
            pc = 0x48F5U;
            break;

        // ---- 45:48F6  sweep accumulator update --------------------------
        case 0x48F6U:  // LDA $2F22,X
            load_accumulator(read_ram(indexed_x(0x2F22U)));
            append_checkpoint(0x48F6U, 0x48F9U, 15U);
            pc = 0x48F9U;
            break;

        case 0x48F9U:  // ORA $2F2C,X
            load_accumulator(
                static_cast<std::uint8_t>(
                    registers.accumulator | read_ram(indexed_x(0x2F2CU))));
            append_checkpoint(0x48F9U, 0x48FCU, 15U);
            pc = 0x48FCU;
            break;

        case 0x48FCU:  // BEQ $4935
            pc = branch(
                0x48FCU,
                0x4935U,
                0x48FEU,
                (registers.status & kZero) != 0U);
            break;

        case 0x48FEU:  // LDA $2E64,X
            load_accumulator(read_ram(indexed_x(0x2E64U)));
            append_checkpoint(0x48FEU, 0x4901U, 15U);
            pc = 0x4901U;
            break;

        case 0x4901U:  // AND #$1F
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x1FU));
            append_checkpoint(0x4901U, 0x4903U, 6U);
            pc = 0x4903U;
            break;

        case 0x4903U: {  // ASL A
            const auto shifted = static_cast<std::uint8_t>(
                registers.accumulator << 1U);
            auto status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            status = static_cast<std::uint8_t>(
                status
                | static_cast<std::uint8_t>(registers.accumulator >> 7U));
            registers.accumulator = shifted;
            registers.status = set_nz(status, shifted);
            append_checkpoint(0x4903U, 0x4904U, 6U);
            pc = 0x4904U;
            break;
        }

        case 0x4904U:  // BEQ $4935
            pc = branch(
                0x4904U,
                0x4935U,
                0x4906U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4906U:  // TAY
            registers.y = registers.accumulator;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4906U, 0x4907U, 6U);
            pc = 0x4907U;
            break;

        case 0x4907U:  // LDA $5B6E,Y
            load_accumulator(read_mapped(indexed_y(0x5B6EU)));
            append_checkpoint(0x4907U, 0x490AU, 15U);
            pc = 0x490AU;
            break;

        case 0x490AU:  // STA $D2
            store(0x490AU, zp(0xD2U), registers.accumulator);
            append_checkpoint(0x490AU, 0x490CU, 12U);
            pc = 0x490CU;
            break;

        case 0x490CU:  // LDA $5B6F,Y
            load_accumulator(read_mapped(indexed_y(0x5B6FU)));
            append_checkpoint(0x490CU, 0x490FU, 15U);
            pc = 0x490FU;
            break;

        case 0x490FU:  // STA $D3
            store(0x490FU, zp(0xD3U), registers.accumulator);
            append_checkpoint(0x490FU, 0x4911U, 12U);
            pc = 0x4911U;
            break;

        case 0x4911U:  // LDY $2F7C,X
            registers.y = read_ram(indexed_x(0x2F7CU));
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4911U, 0x4914U, 15U);
            pc = 0x4914U;
            break;

        case 0x4914U:  // JSR $48B4
            push(0x4914U, 0x49U);
            push(0x4914U, 0x16U);
            ++depth;
            append_checkpoint(0x4914U, 0x48B4U, 21U);
            pc = 0x48B4U;
            break;

        case 0x4917U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4917U, 0x4918U, 6U);
            pc = 0x4918U;
            break;

        case 0x4918U:  // STA $D5
            store(0x4918U, zp(0xD5U), registers.accumulator);
            append_checkpoint(0x4918U, 0x491AU, 12U);
            pc = 0x491AU;
            break;

        case 0x491AU:  // TYA
            load_accumulator(registers.y);
            append_checkpoint(0x491AU, 0x491BU, 6U);
            pc = 0x491BU;
            break;

        case 0x491BU:  // STA $2F7C,X
            store(0x491BU, indexed_x(0x2F7CU), registers.accumulator);
            append_checkpoint(0x491BU, 0x491EU, 15U);
            pc = 0x491EU;
            break;

        case 0x491EU:  // LDY #$00
            registers.y = 0x00U;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x491EU, 0x4920U, 6U);
            pc = 0x4920U;
            break;

        case 0x4920U:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0x4920U, 0x4921U, 6U);
            pc = 0x4921U;
            break;

        case 0x4921U:  // LDA $D5
            load_accumulator(ram[0x00D5U]);
            append_checkpoint(0x4921U, 0x4923U, 12U);
            pc = 0x4923U;
            break;

        case 0x4923U:  // BEQ $4935
            pc = branch(
                0x4923U,
                0x4935U,
                0x4925U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4925U:  // BPL $4928
            pc = branch(
                0x4925U,
                0x4928U,
                0x4927U,
                (registers.status & kNegative) == 0U);
            break;

        case 0x4927U:  // DEY -- sign-extends the delta into the high byte
            registers.y = static_cast<std::uint8_t>(registers.y - 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4927U, 0x4928U, 6U);
            pc = 0x4928U;
            break;

        case 0x4928U:  // ADC $2F22,X
            add_with_carry(read_ram(indexed_x(0x2F22U)));
            append_checkpoint(0x4928U, 0x492BU, 15U);
            pc = 0x492BU;
            break;

        case 0x492BU:  // STA $2F22,X
            store(0x492BU, indexed_x(0x2F22U), registers.accumulator);
            append_checkpoint(0x492BU, 0x492EU, 15U);
            pc = 0x492EU;
            break;

        case 0x492EU:  // TYA
            load_accumulator(registers.y);
            append_checkpoint(0x492EU, 0x492FU, 6U);
            pc = 0x492FU;
            break;

        case 0x492FU:  // ADC $2F2C,X
            add_with_carry(read_ram(indexed_x(0x2F2CU)));
            append_checkpoint(0x492FU, 0x4932U, 15U);
            pc = 0x4932U;
            break;

        case 0x4932U:  // STA $2F2C,X
            store(0x4932U, indexed_x(0x2F2CU), registers.accumulator);
            append_checkpoint(0x4932U, 0x4935U, 15U);
            pc = 0x4935U;
            break;

        // ---- 45:49A2  vibrato accumulator update ------------------------
        case 0x49A2U:  // LDA $2E46,X
            load_accumulator(read_ram(indexed_x(0x2E46U)));
            append_checkpoint(0x49A2U, 0x49A5U, 15U);
            pc = 0x49A5U;
            break;

        case 0x49A5U:  // AND #$02
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x02U));
            append_checkpoint(0x49A5U, 0x49A7U, 6U);
            pc = 0x49A7U;
            break;

        case 0x49A7U:  // BEQ $49B5
            pc = branch(
                0x49A7U,
                0x49B5U,
                0x49A9U,
                (registers.status & kZero) != 0U);
            break;

        case 0x49A9U:  // LDA $2F5E,X
            load_accumulator(read_ram(indexed_x(0x2F5EU)));
            append_checkpoint(0x49A9U, 0x49ACU, 15U);
            pc = 0x49ACU;
            break;

        case 0x49ACU:  // BEQ $49B5
            pc = branch(
                0x49ACU,
                0x49B5U,
                0x49AEU,
                (registers.status & kZero) != 0U);
            break;

        case 0x49AEU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0x49AEU, 0x49AFU, 6U);
            pc = 0x49AFU;
            break;

        case 0x49AFU:  // ADC $2F68,X
            add_with_carry(read_ram(indexed_x(0x2F68U)));
            append_checkpoint(0x49AFU, 0x49B2U, 15U);
            pc = 0x49B2U;
            break;

        case 0x49B2U:  // STA $2F68,X
            store(0x49B2U, indexed_x(0x2F68U), registers.accumulator);
            append_checkpoint(0x49B2U, 0x49B5U, 15U);
            pc = 0x49B5U;
            break;

        // ---- 45:49B6  voice-latch clear ---------------------------------
        case 0x49B6U:  // STZ $2E22
            store(0x49B6U, 0x2E22U, 0x00U);
            append_checkpoint(0x49B6U, 0x49B9U, 15U);
            pc = 0x49B9U;
            break;

        case 0x49B9U:  // STZ $2E23
            store(0x49B9U, 0x2E23U, 0x00U);
            append_checkpoint(0x49B9U, 0x49BCU, 15U);
            pc = 0x49BCU;
            break;

        case 0x49BCU:  // STZ $2E24
            store(0x49BCU, 0x2E24U, 0x00U);
            append_checkpoint(0x49BCU, 0x49BFU, 15U);
            pc = 0x49BFU;
            break;

        case 0x49BFU:  // STZ $2E25
            store(0x49BFU, 0x2E25U, 0x00U);
            append_checkpoint(0x49BFU, 0x49C2U, 15U);
            pc = 0x49C2U;
            break;

        case 0x49C2U:  // STZ $2E26
            store(0x49C2U, 0x2E26U, 0x00U);
            append_checkpoint(0x49C2U, 0x49C5U, 15U);
            pc = 0x49C5U;
            break;

        case 0x49C5U:  // STZ $2E27
            store(0x49C5U, 0x2E27U, 0x00U);
            append_checkpoint(0x49C5U, 0x49C8U, 15U);
            pc = 0x49C8U;
            break;

        case 0x49C8U:  // STZ $2E0A
            store(0x49C8U, 0x2E0AU, 0x00U);
            append_checkpoint(0x49C8U, 0x49CBU, 15U);
            pc = 0x49CBU;
            break;

        case 0x49CBU:  // STZ $2E0C
            store(0x49CBU, 0x2E0CU, 0x00U);
            append_checkpoint(0x49CBU, 0x49CEU, 15U);
            pc = 0x49CEU;
            break;

        case 0x49CEU:  // STZ $2E0E
            store(0x49CEU, 0x2E0EU, 0x00U);
            append_checkpoint(0x49CEU, 0x49D1U, 15U);
            pc = 0x49D1U;
            break;

        // ---- 45:49D2  amplitude commit ----------------------------------
        // Reached from the voice commit below. Selects the PSG channel in Y,
        // writes the pending level to $0804, and marks the voice latched.
        case 0x49D2U:  // TYA
            load_accumulator(registers.y);
            append_checkpoint(0x49D2U, 0x49D3U, 6U);
            pc = 0x49D3U;
            break;

        case 0x49D3U: {  // LSR A
            auto status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(~(kCarry | kNegative | kZero)));
            status = static_cast<std::uint8_t>(
                status
                | static_cast<std::uint8_t>(registers.accumulator & 0x01U));
            registers.accumulator = static_cast<std::uint8_t>(
                registers.accumulator >> 1U);
            registers.status = set_nz(status, registers.accumulator);
            append_checkpoint(0x49D3U, 0x49D4U, 6U);
            pc = 0x49D4U;
            break;
        }

        case 0x49D4U:  // BCS $49D1
            pc = branch(
                0x49D4U,
                0x49D1U,
                0x49D6U,
                (registers.status & kCarry) != 0U);
            break;

        case 0x49D6U:  // LDA $20F5,Y
            load_accumulator(read_ram(indexed_y(0x20F5U)));
            append_checkpoint(0x49D6U, 0x49D9U, 15U);
            pc = 0x49D9U;
            break;

        case 0x49D9U:  // BEQ $49D1
            pc = branch(
                0x49D9U,
                0x49D1U,
                0x49DBU,
                (registers.status & kZero) != 0U);
            break;

        case 0x49DBU:  // LDA $2E05,Y
            load_accumulator(read_ram(indexed_y(0x2E05U)));
            append_checkpoint(0x49DBU, 0x49DEU, 15U);
            pc = 0x49DEU;
            break;

        case 0x49DEU:  // CMP $2F0E,X
            compare(registers.accumulator, read_ram(indexed_x(0x2F0EU)));
            append_checkpoint(0x49DEU, 0x49E1U, 15U);
            pc = 0x49E1U;
            break;

        case 0x49E1U:  // BNE $49D1
            pc = branch(
                0x49E1U,
                0x49D1U,
                0x49E3U,
                (registers.status & kZero) == 0U);
            break;

        case 0x49E3U:  // SEI
            // The mask is applied after the core has sampled the interrupt
            // for this instruction, so an interrupt already pending here is
            // still taken; the checkpoint publishes the old status.
            pending_status = static_cast<std::uint8_t>(
                registers.status | kInterruptDisable);
            status_change_pending = true;
            append_checkpoint(0x49E3U, 0x49E4U, 6U);
            pc = 0x49E4U;
            break;

        case 0x49E4U:  // STY $0800
            store(0x49E4U, kPsgChannelSelect, registers.y);
            append_checkpoint(0x49E4U, 0x49E7U, 15U);
            pc = 0x49E7U;
            break;

        case 0x49E7U:  // LDA $2E37
            load_accumulator(read_ram(0x2E37U));
            append_checkpoint(0x49E7U, 0x49EAU, 15U);
            pc = 0x49EAU;
            break;

        case 0x49EAU:  // ORA #$C0
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator | 0xC0U));
            append_checkpoint(0x49EAU, 0x49ECU, 6U);
            pc = 0x49ECU;
            break;

        case 0x49ECU:  // STA $0804
            store(0x49ECU, kPsgAmplitude, registers.accumulator);
            append_checkpoint(0x49ECU, 0x49EFU, 15U);
            pc = 0x49EFU;
            break;

        case 0x49EFU:  // STA $2E22,Y
            store(0x49EFU, indexed_y(0x2E22U), registers.accumulator);
            append_checkpoint(0x49EFU, 0x49F2U, 15U);
            pc = 0x49F2U;
            break;

        case 0x49F2U:  // CLI -- opens the window one instruction later
            pending_status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(~kInterruptDisable));
            status_change_pending = true;
            append_checkpoint(0x49F2U, 0x49F3U, 6U);
            pc = 0x49F3U;
            break;

        case 0x49F3U:  // LDA #$01
            load_accumulator(0x01U);
            append_checkpoint(0x49F3U, 0x49F5U, 6U);
            pc = 0x49F5U;
            break;

        case 0x49F5U:  // STA $2E0A,Y
            store(0x49F5U, indexed_y(0x2E0AU), registers.accumulator);
            append_checkpoint(0x49F5U, 0x49F8U, 15U);
            pc = 0x49F8U;
            break;

        // ---- 45:49F9  voice commit --------------------------------------
        case 0x49F9U:  // LDY $2E82,X
            registers.y = read_ram(indexed_x(0x2E82U));
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x49F9U, 0x49FCU, 15U);
            pc = 0x49FCU;
            break;

        case 0x49FCU:  // LDA $2E22,Y
            load_accumulator(read_ram(indexed_y(0x2E22U)));
            append_checkpoint(0x49FCU, 0x49FFU, 15U);
            pc = 0x49FFU;
            break;

        case 0x49FFU:  // BNE $49D1
            pc = branch(
                0x49FFU,
                0x49D1U,
                0x4A01U,
                (registers.status & kZero) == 0U);
            break;

        case 0x4A01U:  // LDA $2EB4,X
            load_accumulator(read_ram(indexed_x(0x2EB4U)));
            append_checkpoint(0x4A01U, 0x4A04U, 15U);
            pc = 0x4A04U;
            break;

        case 0x4A04U:  // AND #$08
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x08U));
            append_checkpoint(0x4A04U, 0x4A06U, 6U);
            pc = 0x4A06U;
            break;

        case 0x4A06U:  // BEQ $4A16
            pc = branch(
                0x4A06U,
                0x4A16U,
                0x4A08U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4A08U:  // LDA $2E37
            load_accumulator(read_ram(0x2E37U));
            append_checkpoint(0x4A08U, 0x4A0BU, 15U);
            pc = 0x4A0BU;
            break;

        case 0x4A0BU:  // ADC $2E44 -- no CLC: the carry is the caller's
            add_with_carry(read_ram(0x2E44U));
            append_checkpoint(0x4A0BU, 0x4A0EU, 15U);
            pc = 0x4A0EU;
            break;

        case 0x4A0EU:  // SBC #$1F
            subtract_with_carry(0x1FU);
            append_checkpoint(0x4A0EU, 0x4A10U, 6U);
            pc = 0x4A10U;
            break;

        case 0x4A10U:  // BCS $4A13
            pc = branch(
                0x4A10U,
                0x4A13U,
                0x4A12U,
                (registers.status & kCarry) != 0U);
            break;

        case 0x4A12U:  // CLA
            registers.accumulator = 0x00U;
            append_checkpoint(0x4A12U, 0x4A13U, 6U);
            pc = 0x4A13U;
            break;

        case 0x4A13U:  // STA $2E37
            store(0x4A13U, 0x2E37U, registers.accumulator);
            append_checkpoint(0x4A13U, 0x4A16U, 15U);
            pc = 0x4A16U;
            break;

        case 0x4A16U:  // LDA $2EBE,X
            load_accumulator(read_ram(indexed_x(0x2EBEU)));
            append_checkpoint(0x4A16U, 0x4A19U, 15U);
            pc = 0x4A19U;
            break;

        case 0x4A19U:  // AND #$02
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x02U));
            append_checkpoint(0x4A19U, 0x4A1BU, 6U);
            pc = 0x4A1BU;
            break;

        case 0x4A1BU:  // BNE $49D2
            pc = branch(
                0x4A1BU,
                0x49D2U,
                0x4A1DU,
                (registers.status & kZero) == 0U);
            break;

        case 0x4A1DU:  // LDA $2F22,X
            load_accumulator(read_ram(indexed_x(0x2F22U)));
            append_checkpoint(0x4A1DU, 0x4A20U, 15U);
            pc = 0x4A20U;
            break;

        case 0x4A20U:  // ORA $2F2C,X
            load_accumulator(
                static_cast<std::uint8_t>(
                    registers.accumulator | read_ram(indexed_x(0x2F2CU))));
            append_checkpoint(0x4A20U, 0x4A23U, 15U);
            pc = 0x4A23U;
            break;

        case 0x4A23U:  // BNE $4A29
            pc = branch(
                0x4A23U,
                0x4A29U,
                0x4A25U,
                (registers.status & kZero) == 0U);
            break;

        case 0x4A25U:  // STA $2E22,Y
            store(0x4A25U, indexed_y(0x2E22U), registers.accumulator);
            append_checkpoint(0x4A25U, 0x4A28U, 15U);
            pc = 0x4A28U;
            break;

        case 0x4A29U:  // LDA $2E46,X
            load_accumulator(read_ram(indexed_x(0x2E46U)));
            append_checkpoint(0x4A29U, 0x4A2CU, 15U);
            pc = 0x4A2CU;
            break;

        case 0x4A2CU:  // AND #$0A
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x0AU));
            append_checkpoint(0x4A2CU, 0x4A2EU, 6U);
            pc = 0x4A2EU;
            break;

        case 0x4A2EU:  // BEQ $4A57
            pc = branch(
                0x4A2EU,
                0x4A57U,
                0x4A30U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4A30U:  // CMP #$02
            compare(registers.accumulator, 0x02U);
            append_checkpoint(0x4A30U, 0x4A32U, 6U);
            pc = 0x4A32U;
            break;

        case 0x4A32U:  // BEQ $4A45
            pc = branch(
                0x4A32U,
                0x4A45U,
                0x4A34U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4A34U:  // LDA $2F68,X
            load_accumulator(read_ram(indexed_x(0x2F68U)));
            append_checkpoint(0x4A34U, 0x4A37U, 15U);
            pc = 0x4A37U;
            break;

        case 0x4A37U:  // AND #$1F
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x1FU));
            append_checkpoint(0x4A37U, 0x4A39U, 6U);
            pc = 0x4A39U;
            break;

        case 0x4A39U:  // ORA #$80
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator | 0x80U));
            append_checkpoint(0x4A39U, 0x4A3BU, 6U);
            pc = 0x4A3BU;
            break;

        case 0x4A3BU:  // STA $2E34,Y
            store(0x4A3BU, indexed_y(0x2E34U), registers.accumulator);
            append_checkpoint(0x4A3BU, 0x4A3EU, 15U);
            pc = 0x4A3EU;
            break;

        case 0x4A3EU:  // LDA #$FF
            load_accumulator(0xFFU);
            append_checkpoint(0x4A3EU, 0x4A40U, 6U);
            pc = 0x4A40U;
            break;

        case 0x4A40U:  // STA $2E2E,Y
            store(0x4A40U, indexed_y(0x2E2EU), registers.accumulator);
            append_checkpoint(0x4A40U, 0x4A43U, 15U);
            pc = 0x4A43U;
            break;

        case 0x4A43U:  // BNE $4A8F -- always taken, A is $FF
            pc = branch(
                0x4A43U,
                0x4A8FU,
                0x4A45U,
                (registers.status & kZero) == 0U);
            break;

        case 0x4A45U:  // CPY #$04
            compare(registers.y, 0x04U);
            append_checkpoint(0x4A45U, 0x4A47U, 6U);
            pc = 0x4A47U;
            break;

        case 0x4A47U:  // BCS $4A34
            pc = branch(
                0x4A47U,
                0x4A34U,
                0x4A49U,
                (registers.status & kCarry) != 0U);
            break;

        case 0x4A49U:  // LDY $2EAA,X
            registers.y = read_ram(indexed_x(0x2EAAU));
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4A49U, 0x4A4CU, 15U);
            pc = 0x4A4CU;
            break;

        case 0x4A4CU:  // LDA $2E22,Y
            load_accumulator(read_ram(indexed_y(0x2E22U)));
            append_checkpoint(0x4A4CU, 0x4A4FU, 15U);
            pc = 0x4A4FU;
            break;

        case 0x4A4FU:  // BNE $4A54
            pc = branch(
                0x4A4FU,
                0x4A54U,
                0x4A51U,
                (registers.status & kZero) == 0U);
            break;

        case 0x4A51U:  // JSR $4A34
            push(0x4A51U, 0x4AU);
            push(0x4A51U, 0x53U);
            ++depth;
            append_checkpoint(0x4A51U, 0x4A34U, 21U);
            pc = 0x4A34U;
            break;

        case 0x4A54U:  // LDY $2E82,X
            registers.y = read_ram(indexed_x(0x2E82U));
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4A54U, 0x4A57U, 15U);
            pc = 0x4A57U;
            break;

        case 0x4A57U:  // LDA $2E46,X
            load_accumulator(read_ram(indexed_x(0x2E46U)));
            append_checkpoint(0x4A57U, 0x4A5AU, 15U);
            pc = 0x4A5AU;
            break;

        case 0x4A5AU:  // AND #$01
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x01U));
            append_checkpoint(0x4A5AU, 0x4A5CU, 6U);
            pc = 0x4A5CU;
            break;

        case 0x4A5CU:  // BEQ $4A9B
            pc = branch(
                0x4A5CU,
                0x4A9BU,
                0x4A5EU,
                (registers.status & kZero) != 0U);
            break;

        case 0x4A5EU:  // LDA $2EB4,X
            load_accumulator(read_ram(indexed_x(0x2EB4U)));
            append_checkpoint(0x4A5EU, 0x4A61U, 15U);
            pc = 0x4A61U;
            break;

        case 0x4A61U:  // AND #$01
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x01U));
            append_checkpoint(0x4A61U, 0x4A63U, 6U);
            pc = 0x4A63U;
            break;

        case 0x4A63U:  // BEQ $4A6B
            // The not-taken arm at 4A65 calls back into 4A6B and then jumps
            // to $4A9C, outside this slice, so it is left to the core.
            if ((registers.status & kZero) == 0U) {
                return std::nullopt;
            }
            pc = branch(0x4A63U, 0x4A6BU, 0x4A65U, true);
            break;

        case 0x4A6BU:  // CPY #$04
            compare(registers.y, 0x04U);
            append_checkpoint(0x4A6BU, 0x4A6DU, 6U);
            pc = 0x4A6DU;
            break;

        case 0x4A6DU:  // BCC $4A73
            pc = branch(
                0x4A6DU,
                0x4A73U,
                0x4A6FU,
                (registers.status & kCarry) == 0U);
            break;

        case 0x4A6FU:  // CLA
            registers.accumulator = 0x00U;
            append_checkpoint(0x4A6FU, 0x4A70U, 6U);
            pc = 0x4A70U;
            break;

        case 0x4A70U:  // STA $2E34,Y
            store(0x4A70U, indexed_y(0x2E34U), registers.accumulator);
            append_checkpoint(0x4A70U, 0x4A73U, 15U);
            pc = 0x4A73U;
            break;

        case 0x4A73U:  // LDA $2F22,X
            load_accumulator(read_ram(indexed_x(0x2F22U)));
            append_checkpoint(0x4A73U, 0x4A76U, 15U);
            pc = 0x4A76U;
            break;

        case 0x4A76U:  // STA $2E16,Y
            store(0x4A76U, indexed_y(0x2E16U), registers.accumulator);
            append_checkpoint(0x4A76U, 0x4A79U, 15U);
            pc = 0x4A79U;
            break;

        case 0x4A79U:  // LDA $2F2C,X
            load_accumulator(read_ram(indexed_x(0x2F2CU)));
            append_checkpoint(0x4A79U, 0x4A7CU, 15U);
            pc = 0x4A7CU;
            break;

        case 0x4A7CU:  // STA $2E1C,Y
            store(0x4A7CU, indexed_y(0x2E1CU), registers.accumulator);
            append_checkpoint(0x4A7CU, 0x4A7FU, 15U);
            pc = 0x4A7FU;
            break;

        case 0x4A7FU:  // LDA $2E46,X
            load_accumulator(read_ram(indexed_x(0x2E46U)));
            append_checkpoint(0x4A7FU, 0x4A82U, 15U);
            pc = 0x4A82U;
            break;

        case 0x4A82U:  // BPL $4A89
            pc = branch(
                0x4A82U,
                0x4A89U,
                0x4A84U,
                (registers.status & kNegative) == 0U);
            break;

        case 0x4A84U:  // LDA $2FB8,X
            load_accumulator(read_ram(indexed_x(0x2FB8U)));
            append_checkpoint(0x4A84U, 0x4A87U, 15U);
            pc = 0x4A87U;
            break;

        case 0x4A87U:  // BRA $4A8C
            pc = branch(0x4A87U, 0x4A8CU, 0x4A89U, true);
            break;

        case 0x4A89U:  // LDA $2EA0,X
            load_accumulator(read_ram(indexed_x(0x2EA0U)));
            append_checkpoint(0x4A89U, 0x4A8CU, 15U);
            pc = 0x4A8CU;
            break;

        case 0x4A8CU:  // STA $2E2E,Y
            store(0x4A8CU, indexed_y(0x2E2EU), registers.accumulator);
            append_checkpoint(0x4A8CU, 0x4A8FU, 15U);
            pc = 0x4A8FU;
            break;

        case 0x4A8FU:  // LDA $2E37
            load_accumulator(read_ram(0x2E37U));
            append_checkpoint(0x4A8FU, 0x4A92U, 15U);
            pc = 0x4A92U;
            break;

        case 0x4A92U:  // STA $2E22,Y
            store(0x4A92U, indexed_y(0x2E22U), registers.accumulator);
            append_checkpoint(0x4A92U, 0x4A95U, 15U);
            pc = 0x4A95U;
            break;

        case 0x4A95U:  // LDA $2FAE,X
            load_accumulator(read_ram(indexed_x(0x2FAEU)));
            append_checkpoint(0x4A95U, 0x4A98U, 15U);
            pc = 0x4A98U;
            break;

        case 0x4A98U:  // STA $2E28,Y
            store(0x4A98U, indexed_y(0x2E28U), registers.accumulator);
            append_checkpoint(0x4A98U, 0x4A9BU, 15U);
            pc = 0x4A9BU;
            break;

        // ---- nested returns ---------------------------------------------
        // Only reached with depth > 0; the depth-zero case terminated the
        // walk above, in front of the RTS.
        case 0x4227U:
        case 0x48ACU:
        case 0x48B3U:
        case 0x48B8U:
        case 0x48C0U:
        case 0x48CDU:
        case 0x48D6U:
        case 0x48E5U:
        case 0x48F5U:
        case 0x4935U:
        case 0x49B5U:
        case 0x49D1U:
        case 0x49F8U:
        case 0x4A28U:
        case 0x4A9BU: {
            const auto low = pull();
            const auto high = pull();
            const auto return_pc = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(low)
                 | static_cast<std::uint16_t>(high) << 8U)
                + 1U);
            --depth;
            const auto rts_pc = pc;
            append_checkpoint(rts_pc, return_pc, 21U);
            pc = return_pc;
            break;
        }

        default:
            // Any path this slice does not transcribe -- including the two
            // pending-event arms at 4216/4221, which call out to $4B86 --
            // runs emulated instead.
            return std::nullopt;
        }
    }
}

}  // namespace devils_crush::app::detail
