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

// The timer interrupt handler is the game's sound driver. It is a flat state
// machine: no subroutine calls, three PSG channels walked at X = 4, 2, 0,
// each streaming five-bit DDA samples out of a bank of sequence data mapped
// into MPR6. Control only ever leaves through the two RTI instructions or
// the two JMP $FC03 chain exits, so the whole driver is one transcript with
// four possible return instructions.
// Coverage note: the walk implements every address in the handler, taken from
// the disassembly, but the 3,600-frame census route only ever executes 168 of
// them. The paths it never reaches are the track-advance tail (FDF4-FE48, which
// reloads a channel from $2E10,X), the portamento branch at FCB6-FCD9, the hold
// path at FCDC-FD05, and the V-flag arms at FC51-FC55 and FD09-FD0D. Those are
// transcribed but not proven; the route silences an expiring channel at FDE1
// rather than advancing it. Anything reaching an address with no case declines
// and runs emulated, so a wrong guess degrades to the machine rather than
// corrupting state -- but the unexercised arms have not been census-checked.
constexpr std::uint8_t kBank = 0x00U;
constexpr std::uint16_t kEntryPc = 0xFC00U;
// The handler ends in one of two byte-identical epilogues -- FC7C for the
// channel walk, FD70 for the track-advance tail -- and both unwind the same
// way before reading $DA, the rerun request a preempting handler leaves:
//
//   PLA / TAM #$40 / PLY / PLX  |  SEI |  STZ $D9 / LDA $DA / BEQ / PLA
//
// That read is why the walk cannot simply run on to the RTI. A transcript is
// computed once, from the snapshot taken on entry, and $DA is written by
// 00:E12A inside the video handler -- which preempts this one, because FC32's
// CLI opens the window on purpose. Deciding the BEQ from the entry snapshot
// would take the wrong arm on every arrival where the request landed
// mid-body: 380 of 251,531 over the census route, each one a JMP $FC03 chain
// into the sequencer that the transcript would have skipped. So the epilogue
// is two transcripts, and the second is entered late enough for its snapshot
// to contain the write.
//
// The split falls on the SEI because that instruction is the one a checkpoint
// cannot describe. A checkpoint carries a single status byte, and the core
// derives two different things from it: the machine's P, which an interrupt
// taken at that boundary pushes, and IFlagSample, which decides whether the
// timer may interrupt there at all. Every other instruction wants the same
// answer from both. SEI does not -- P has I set the moment it retires, but
// its LastCycle sampled the mask beforehand, so the boundary is still open.
// Publishing the open mask corrupts the pushed P; publishing the true P
// closes a window the hardware leaves open. Neither is the machine, so the
// SEI stays emulated, and the two transcripts meet on either side of it. It
// costs 1,526,046 master cycles of the 26.7M the epilogue is worth.
constexpr std::uint16_t kFirstMaskPc = 0xFC81U;
constexpr std::uint16_t kSecondMaskPc = 0xFD75U;
constexpr std::uint16_t kFirstResumePc = 0xFC82U;
constexpr std::uint16_t kSecondResumePc = 0xFD76U;
// Both halves stop in front of an instruction the driver does not own: the
// RTI that ends the interrupt, or the JMP that chains into the sequencer. A
// timer interrupt arriving while the handler is already running takes neither
// -- it bails out of FC2C through FC8C without touching a channel, and joins
// the same RTI.
constexpr std::uint16_t kFirstReturnPc = 0xFC8DU;
constexpr std::uint16_t kSecondReturnPc = 0xFD81U;
constexpr std::uint16_t kFirstChainPc = 0xFC89U;
constexpr std::uint16_t kSecondChainPc = 0xFD7DU;

constexpr std::uint8_t kCarry = 0x01U;
constexpr std::uint8_t kZero = 0x02U;
constexpr std::uint8_t kInterrupt = 0x04U;
constexpr std::uint8_t kDecimal = 0x08U;
constexpr std::uint8_t kMemory = 0x20U;
constexpr std::uint8_t kOverflow = 0x40U;
constexpr std::uint8_t kNegative = 0x80U;

// Zero page layout, all indexed by the channel selector in X.
constexpr std::uint16_t kSequenceLow = 0x00DDU;
constexpr std::uint16_t kSequenceHigh = 0x00DEU;
constexpr std::uint16_t kSampleBits = 0x00E3U;
constexpr std::uint16_t kChannelState = 0x00E4U;
constexpr std::uint16_t kHoldLow = 0x00E9U;
constexpr std::uint16_t kHoldHigh = 0x00EAU;
constexpr std::uint16_t kRateDelta = 0x00EFU;
constexpr std::uint16_t kRateAccumulator = 0x00F0U;
constexpr std::uint16_t kChannelBank = 0x00F5U;
constexpr std::uint16_t kChannelFlags = 0x00F6U;
constexpr std::uint16_t kHandlerBusy = 0x00D9U;
constexpr std::uint16_t kHandlerRerun = 0x00DAU;
constexpr std::uint16_t kCursorLow = 0x00DBU;
constexpr std::uint16_t kCursorHigh = 0x00DCU;
constexpr std::uint16_t kTrackPointer = 0x0E10U;

constexpr std::uint16_t kPsgChannelSelect = 0x0800U;
constexpr std::uint16_t kPsgBalance = 0x0805U;
constexpr std::uint16_t kPsgWaveform = 0x0806U;
constexpr std::uint16_t kTimerControl = 0x0C01U;
constexpr std::uint16_t kInterruptAcknowledge = 0x1403U;

// The bank table the driver indexes for a track's data bank lives in ROM at
// $FC06, immediately ahead of the handler's first real instruction.
constexpr std::uint16_t kBankTable = 0xFC06U;

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

}  // namespace

std::optional<RoutineReplacement> replace_sound_timer_driver(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    if (entry.routine.physical_bank != kBank
        || (entry.routine.logical_pc != kEntryPc
            && entry.routine.logical_pc != kFirstResumePc
            && entry.routine.logical_pc != kSecondResumePc)
        || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    // Nothing in the driver is written to run in decimal mode or under the
    // memory-operand T flag; both would change ADC/SBC and the loads.
    if ((entry.registers.status & (kDecimal | kMemory)) != 0U) {
        return std::nullopt;
    }

    // A private copy of work RAM lets the walk read back its own stores,
    // which the channel loop does constantly through the $DD,X cursor.
    std::uint8_t ram[0x2000];
    for (std::size_t index = 0U; index < 0x2000U; ++index) {
        ram[index] = std::to_integer<std::uint8_t>(work_ram[index]);
    }

    RoutineReplacement replacement{};
    replacement.work_ram_writes.reserve(32U);
    replacement.checkpoints.reserve(384U);

    auto registers = entry.registers;
    std::uint32_t cycle = 0U;
    bool overflow = false;

    // Reads the ROM byte a logical address currently maps to. The driver
    // only ever reads through MPR6, but resolving the mapping generally
    // keeps the walk honest if a cursor is left pointing elsewhere.
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

    // Every store in the driver lands on its instruction's final cycle, so
    // a write shares the offset and the instruction of its checkpoint. The
    // stores are recorded first and priced here, once the instruction's
    // cycle count is known.
    std::size_t committed_writes = 0U;
    // CLI clears the interrupt mask only after the core has already sampled
    // the interrupt for that instruction, so the first instruction after a
    // CLI still runs masked. The checkpoint for CLI therefore publishes the
    // old mask and the clear lands on the following checkpoint. The mirror
    // case, SEI, is not modelled here: see kFirstMaskPc above for why it is
    // left emulated instead.
    bool interrupt_enable_pending = false;
    const auto append_checkpoint =
        [&replacement,
         &registers,
         &cycle,
         &committed_writes,
         &interrupt_enable_pending](
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
            if (interrupt_enable_pending) {
                registers.status = static_cast<std::uint8_t>(
                    registers.status
                    & static_cast<std::uint8_t>(~kInterrupt));
                interrupt_enable_pending = false;
            }
        };

    const auto store =
        [&replacement, &ram](
            const std::uint16_t instruction_pc,
            const std::uint16_t address,
            const std::uint8_t value) {
            if (address >= 0x2000U && address < 0x4000U) {
                ram[address - 0x2000U] = value;
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

    const auto pull =
        [&registers, &ram]() -> std::uint8_t {
            registers.stack_pointer = static_cast<std::uint8_t>(
                registers.stack_pointer + 1U);
            return ram[0x0100U + registers.stack_pointer];
        };

    const auto channel = [&registers](const std::uint16_t base) {
        return static_cast<std::uint16_t>(
            base + static_cast<std::uint16_t>(registers.x));
    };
    const auto zp = [&ram](const std::uint16_t address) {
        return ram[address];
    };
    const auto logical = [](const std::uint16_t address) {
        return static_cast<std::uint16_t>(0x2000U + address);
    };

    // The 16-bit sequence cursor for the active channel.
    const auto cursor = [&channel, &zp]() {
        return static_cast<std::uint16_t>(
            zp(channel(kSequenceLow))
            | static_cast<std::uint16_t>(zp(channel(kSequenceHigh))) << 8U);
    };

    auto pc = entry.routine.logical_pc;
    for (;;) {
        if (overflow
            || replacement.checkpoints.size() > 2000U
            || replacement.work_ram_writes.size() > 480U) {
            return std::nullopt;
        }

        switch (pc) {
        case 0xFC00U:  // JMP $FC26
            append_checkpoint(0xFC00U, 0xFC26U, 12U);
            pc = 0xFC26U;
            break;

        case 0xFC0EU: {  // AND #$7F
            registers.accumulator = static_cast<std::uint8_t>(
                registers.accumulator & 0x7FU);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFC0EU, 0xFC10U, 6U);
            pc = 0xFC10U;
            break;
        }

        case 0xFC10U:  // TAM #$40
            registers.mpr[6] = registers.accumulator;
            append_checkpoint(0xFC10U, 0xFC12U, 15U);
            pc = 0xFC12U;
            break;

        case 0xFC12U:  // STA $F5,X
            store(
                0xFC12U,
                logical(channel(kChannelBank)),
                registers.accumulator);
            append_checkpoint(0xFC12U, 0xFC14U, 12U);
            pc = 0xFC14U;
            break;

        case 0xFC14U:  // STZ $E3,X
            store(0xFC14U, logical(channel(kSampleBits)), 0U);
            append_checkpoint(0xFC14U, 0xFC16U, 12U);
            pc = 0xFC16U;
            break;

        case 0xFC16U:  // LDA $E4,X
            registers.accumulator = zp(channel(kChannelState));
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFC16U, 0xFC18U, 12U);
            pc = 0xFC18U;
            break;

        case 0xFC18U: {  // BPL $FC43
            const bool taken = (registers.status & kNegative) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFC43U)
                : static_cast<std::uint16_t>(0xFC1AU);
            append_checkpoint(0xFC18U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFC1AU:  // LDA ($DD,X)
            registers.accumulator = read_mapped(cursor());
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFC1AU, 0xFC1CU, 21U);
            pc = 0xFC1CU;
            break;

        case 0xFC1CU:  // STA $E3,X
            store(
                0xFC1CU,
                logical(channel(kSampleBits)),
                registers.accumulator);
            append_checkpoint(0xFC1CU, 0xFC1EU, 12U);
            pc = 0xFC1EU;
            break;

        case 0xFC1EU: {  // INC $DD,X
            const auto value = static_cast<std::uint8_t>(
                zp(channel(kSequenceLow)) + 1U);
            registers.status = set_nz(registers.status, value);
            store(0xFC1EU, logical(channel(kSequenceLow)), value);
            append_checkpoint(0xFC1EU, 0xFC20U, 18U);
            pc = 0xFC20U;
            break;
        }

        case 0xFC20U: {  // BNE $FC43
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFC43U)
                : static_cast<std::uint16_t>(0xFC22U);
            append_checkpoint(0xFC20U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFC22U: {  // INC $DE,X
            const auto value = static_cast<std::uint8_t>(
                zp(channel(kSequenceHigh)) + 1U);
            registers.status = set_nz(registers.status, value);
            store(0xFC22U, logical(channel(kSequenceHigh)), value);
            append_checkpoint(0xFC22U, 0xFC24U, 18U);
            pc = 0xFC24U;
            break;
        }

        case 0xFC24U: {  // BNE $FC43
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFC43U)
                : static_cast<std::uint16_t>(0xFC26U);
            append_checkpoint(0xFC24U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFC26U:  // PHA
            push(0xFC26U, registers.accumulator);
            append_checkpoint(0xFC26U, 0xFC27U, 9U);
            pc = 0xFC27U;
            break;

        case 0xFC27U:  // STZ $1403
            store(0xFC27U, kInterruptAcknowledge, 0U);
            append_checkpoint(0xFC27U, 0xFC2AU, 15U);
            pc = 0xFC2AU;
            break;

        case 0xFC2AU:  // LDA $D9
            registers.accumulator = zp(kHandlerBusy);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFC2AU, 0xFC2CU, 12U);
            pc = 0xFC2CU;
            break;

        case 0xFC2CU: {  // BNE $FC8C
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFC8CU)
                : static_cast<std::uint16_t>(0xFC2EU);
            append_checkpoint(0xFC2CU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFC2EU: {  // INC $D9
            const auto value = static_cast<std::uint8_t>(
                zp(kHandlerBusy) + 1U);
            registers.status = set_nz(registers.status, value);
            store(0xFC2EU, logical(kHandlerBusy), value);
            append_checkpoint(0xFC2EU, 0xFC30U, 18U);
            pc = 0xFC30U;
            break;
        }

        case 0xFC30U:  // STZ $DA
            store(0xFC30U, logical(kHandlerRerun), 0U);
            append_checkpoint(0xFC30U, 0xFC32U, 12U);
            pc = 0xFC32U;
            break;

        case 0xFC32U:  // CLI
            interrupt_enable_pending = true;
            append_checkpoint(0xFC32U, 0xFC33U, 6U);
            pc = 0xFC33U;
            break;

        case 0xFC33U:  // PHX
            push(0xFC33U, registers.x);
            append_checkpoint(0xFC33U, 0xFC34U, 9U);
            pc = 0xFC34U;
            break;

        case 0xFC34U:  // PHY
            push(0xFC34U, registers.y);
            append_checkpoint(0xFC34U, 0xFC35U, 9U);
            pc = 0xFC35U;
            break;

        case 0xFC35U:  // NOP
            append_checkpoint(0xFC35U, 0xFC36U, 6U);
            pc = 0xFC36U;
            break;

        case 0xFC36U:  // TMA #$40
            registers.accumulator = registers.mpr[6];
            append_checkpoint(0xFC36U, 0xFC38U, 12U);
            pc = 0xFC38U;
            break;

        case 0xFC38U:  // PHA
            push(0xFC38U, registers.accumulator);
            append_checkpoint(0xFC38U, 0xFC39U, 9U);
            pc = 0xFC39U;
            break;

        case 0xFC39U:  // LDX #$04
            registers.x = 0x04U;
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0xFC39U, 0xFC3BU, 6U);
            pc = 0xFC3BU;
            break;

        case 0xFC3BU:  // LDA $F5,X
            registers.accumulator = zp(channel(kChannelBank));
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFC3BU, 0xFC3DU, 12U);
            pc = 0xFC3DU;
            break;

        case 0xFC3DU: {  // BEQ $FC78
            const bool taken = (registers.status & kZero) != 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFC78U)
                : static_cast<std::uint16_t>(0xFC3FU);
            append_checkpoint(0xFC3DU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFC3FU: {  // BMI $FC0E
            const bool taken = (registers.status & kNegative) != 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFC0EU)
                : static_cast<std::uint16_t>(0xFC41U);
            append_checkpoint(0xFC3FU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFC41U:  // TAM #$40
            registers.mpr[6] = registers.accumulator;
            append_checkpoint(0xFC41U, 0xFC43U, 15U);
            pc = 0xFC43U;
            break;

        case 0xFC43U:  // STX $0800
            store(0xFC43U, kPsgChannelSelect, registers.x);
            append_checkpoint(0xFC43U, 0xFC46U, 15U);
            pc = 0xFC46U;
            break;

        case 0xFC46U:  // LDA $E4,X
            registers.accumulator = zp(channel(kChannelState));
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFC46U, 0xFC48U, 12U);
            pc = 0xFC48U;
            break;

        case 0xFC48U: {  // BIT #$01
            constexpr std::uint8_t operand = 0x01U;
            registers.status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(
                    ~(kZero | kOverflow | kNegative)));
            if ((operand & registers.accumulator) == 0U) {
                registers.status = static_cast<std::uint8_t>(
                    registers.status | kZero);
            }
            registers.status = static_cast<std::uint8_t>(
                registers.status
                | static_cast<std::uint8_t>(
                    operand & (kOverflow | kNegative)));
            append_checkpoint(0xFC48U, 0xFC4AU, 6U);
            pc = 0xFC4AU;
            break;
        }

        case 0xFC4AU: {  // BEQ $FC4F
            const bool taken = (registers.status & kZero) != 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFC4FU)
                : static_cast<std::uint16_t>(0xFC4CU);
            append_checkpoint(0xFC4AU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFC4CU:  // JMP $FD07
            append_checkpoint(0xFC4CU, 0xFD07U, 12U);
            pc = 0xFD07U;
            break;

        case 0xFC4FU: {  // BVC $FC57
            const bool taken = (registers.status & kOverflow) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFC57U)
                : static_cast<std::uint16_t>(0xFC51U);
            append_checkpoint(0xFC4FU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFC51U:  // AND #$BF
            registers.accumulator = static_cast<std::uint8_t>(
                registers.accumulator & 0xBFU);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFC51U, 0xFC53U, 6U);
            pc = 0xFC53U;
            break;

        case 0xFC53U:  // TAY
            registers.y = registers.accumulator;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xFC53U, 0xFC54U, 6U);
            pc = 0xFC54U;
            break;

        case 0xFC54U:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xFC54U, 0xFC55U, 6U);
            pc = 0xFC55U;
            break;

        case 0xFC55U:  // BCC $FC6A
            append_checkpoint(0xFC55U, 0xFC6AU, 12U);
            pc = 0xFC6AU;
            break;

        case 0xFC57U:  // TAY
            registers.y = registers.accumulator;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xFC57U, 0xFC58U, 6U);
            pc = 0xFC58U;
            break;

        case 0xFC58U:  // LDA ($DD,X)
            registers.accumulator = read_mapped(cursor());
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFC58U, 0xFC5AU, 21U);
            pc = 0xFC5AU;
            break;

        case 0xFC5AU:  // AND #$F0
            registers.accumulator = static_cast<std::uint8_t>(
                registers.accumulator & 0xF0U);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFC5AU, 0xFC5CU, 6U);
            pc = 0xFC5CU;
            break;

        case 0xFC5CU:
        case 0xFC5DU:
        case 0xFC5EU: {  // LSR A
            const auto value = registers.accumulator;
            registers.status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(
                    ~(kCarry | kNegative | kZero)));
            registers.status = static_cast<std::uint8_t>(
                registers.status | static_cast<std::uint8_t>(value & 1U));
            registers.accumulator = static_cast<std::uint8_t>(value >> 1U);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            const auto next = static_cast<std::uint16_t>(pc + 1U);
            append_checkpoint(pc, next, 6U);
            pc = next;
            break;
        }

        case 0xFC5FU: {  // ASL $E3,X
            const auto value = zp(channel(kSampleBits));
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            registers.status = static_cast<std::uint8_t>(
                registers.status | static_cast<std::uint8_t>(value >> 7U));
            const auto shifted = static_cast<std::uint8_t>(value << 1U);
            registers.status = set_nz(registers.status, shifted);
            store(0xFC5FU, logical(channel(kSampleBits)), shifted);
            append_checkpoint(0xFC5FU, 0xFC61U, 18U);
            pc = 0xFC61U;
            break;
        }

        case 0xFC61U: {  // ADC #$00
            const auto sum = static_cast<std::uint16_t>(
                registers.accumulator
                + static_cast<std::uint16_t>(registers.status & kCarry));
            registers.status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(
                    ~(kZero | kCarry | kNegative | kOverflow)));
            if ((sum & 0xFF00U) != 0U) {
                registers.status = static_cast<std::uint8_t>(
                    registers.status | kCarry);
            }
            registers.accumulator = static_cast<std::uint8_t>(sum);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFC61U, 0xFC63U, 6U);
            pc = 0xFC63U;
            break;
        }

        case 0xFC63U:  // STA $0806
            store(0xFC63U, kPsgWaveform, registers.accumulator);
            append_checkpoint(0xFC63U, 0xFC66U, 15U);
            pc = 0xFC66U;
            break;

        case 0xFC66U: {  // BIT $F6,X
            const auto operand = zp(channel(kChannelFlags));
            registers.status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(
                    ~(kZero | kOverflow | kNegative)));
            if ((operand & registers.accumulator) == 0U) {
                registers.status = static_cast<std::uint8_t>(
                    registers.status | kZero);
            }
            registers.status = static_cast<std::uint8_t>(
                registers.status
                | static_cast<std::uint8_t>(
                    operand & (kOverflow | kNegative)));
            append_checkpoint(0xFC66U, 0xFC68U, 12U);
            pc = 0xFC68U;
            break;
        }

        case 0xFC68U: {  // BMI $FC95
            const bool taken = (registers.status & kNegative) != 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFC95U)
                : static_cast<std::uint16_t>(0xFC6AU);
            append_checkpoint(0xFC68U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFC6AU:  // LDA $EF,X
            registers.accumulator = zp(channel(kRateDelta));
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFC6AU, 0xFC6CU, 12U);
            pc = 0xFC6CU;
            break;

        case 0xFC6CU: {  // BEQ $FC74
            const bool taken = (registers.status & kZero) != 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFC74U)
                : static_cast<std::uint16_t>(0xFC6EU);
            append_checkpoint(0xFC6CU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFC6EU: {  // ADC $F0,X
            const auto operand = zp(channel(kRateAccumulator));
            const auto sum = static_cast<std::uint16_t>(
                registers.accumulator + operand
                + static_cast<std::uint16_t>(registers.status & kCarry));
            registers.status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(
                    ~(kZero | kCarry | kNegative | kOverflow)));
            const auto signed_overflow = static_cast<std::uint8_t>(
                ((((registers.accumulator ^ operand) & 0x80U) ^ 0x80U)
                 & ((registers.accumulator ^ sum) & 0x80U))
                >> 1U);
            registers.status = static_cast<std::uint8_t>(
                registers.status | signed_overflow);
            if ((sum & 0xFF00U) != 0U) {
                registers.status = static_cast<std::uint8_t>(
                    registers.status | kCarry);
            }
            registers.accumulator = static_cast<std::uint8_t>(sum);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFC6EU, 0xFC70U, 12U);
            pc = 0xFC70U;
            break;
        }

        case 0xFC70U:  // STA $F0,X
            store(
                0xFC70U,
                logical(channel(kRateAccumulator)),
                registers.accumulator);
            append_checkpoint(0xFC70U, 0xFC72U, 12U);
            pc = 0xFC72U;
            break;

        case 0xFC72U: {  // BCC $FC8E
            const bool taken = (registers.status & kCarry) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFC8EU)
                : static_cast<std::uint16_t>(0xFC74U);
            append_checkpoint(0xFC72U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFC74U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xFC74U, 0xFC75U, 6U);
            pc = 0xFC75U;
            break;

        case 0xFC75U:  // TYA
            registers.accumulator = registers.y;
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFC75U, 0xFC76U, 6U);
            pc = 0xFC76U;
            break;

        case 0xFC76U:  // STA $E4,X
            store(
                0xFC76U,
                logical(channel(kChannelState)),
                registers.accumulator);
            append_checkpoint(0xFC76U, 0xFC78U, 12U);
            pc = 0xFC78U;
            break;

        case 0xFC78U:
        case 0xFC79U: {  // DEX
            registers.x = static_cast<std::uint8_t>(registers.x - 1U);
            registers.status = set_nz(registers.status, registers.x);
            const auto next = static_cast<std::uint16_t>(pc + 1U);
            append_checkpoint(pc, next, 6U);
            pc = next;
            break;
        }

        case 0xFC7AU: {  // BPL $FC3B
            const bool taken = (registers.status & kNegative) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFC3BU)
                : static_cast<std::uint16_t>(0xFC7CU);
            append_checkpoint(0xFC7AU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFC7CU:  // PLA
            registers.accumulator = pull();
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFC7CU, 0xFC7DU, 12U);
            pc = 0xFC7DU;
            break;

        // Restores the caller's MPR6, the value FC36's TMA pushed on the way
        // in. output_registers carries the whole register file, mapping
        // included, so a transcript can hand the bank back the way the
        // assembly does.
        case 0xFC7DU:  // TAM #$40
            registers.mpr[6] = registers.accumulator;
            append_checkpoint(0xFC7DU, 0xFC7FU, 15U);
            pc = 0xFC7FU;
            break;

        case 0xFC7FU:  // PLY
            registers.y = pull();
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xFC7FU, 0xFC80U, 12U);
            pc = 0xFC80U;
            break;

        case 0xFC80U:  // PLX
            registers.x = pull();
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0xFC80U, kFirstMaskPc, 12U);
            pc = kFirstMaskPc;
            break;

        // The head of the second transcript. The first one stopped in front
        // of the SEI at FC81; the core emulates that, then re-enters here,
        // so everything below runs against a snapshot taken after any
        // preempting handler has finished writing $DA.
        case 0xFC82U:  // STZ $D9
            store(0xFC82U, logical(kHandlerBusy), 0U);
            append_checkpoint(0xFC82U, 0xFC84U, 12U);
            pc = 0xFC84U;
            break;

        case 0xFC84U:  // LDA $DA
            registers.accumulator = zp(kHandlerRerun);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFC84U, 0xFC86U, 12U);
            pc = 0xFC86U;
            break;

        case 0xFC86U: {  // BEQ $FC8C
            const bool taken = (registers.status & kZero) != 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFC8CU)
                : static_cast<std::uint16_t>(0xFC88U);
            append_checkpoint(0xFC86U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFC88U:  // PLA
            registers.accumulator = pull();
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFC88U, 0xFC89U, 12U);
            pc = 0xFC89U;
            break;

        case 0xFC8CU:  // PLA
            registers.accumulator = pull();
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFC8CU, 0xFC8DU, 12U);
            pc = 0xFC8DU;
            break;

        case 0xFC8EU:  // TYA
            registers.accumulator = registers.y;
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFC8EU, 0xFC8FU, 6U);
            pc = 0xFC8FU;
            break;

        case 0xFC8FU:  // ORA #$40
            registers.accumulator = static_cast<std::uint8_t>(
                registers.accumulator | 0x40U);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFC8FU, 0xFC91U, 6U);
            pc = 0xFC91U;
            break;

        case 0xFC91U:  // STA $E4,X
            store(
                0xFC91U,
                logical(channel(kChannelState)),
                registers.accumulator);
            append_checkpoint(0xFC91U, 0xFC93U, 12U);
            pc = 0xFC93U;
            break;

        case 0xFC93U: {  // BNE $FC78
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFC78U)
                : static_cast<std::uint16_t>(0xFC95U);
            append_checkpoint(0xFC93U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFC95U: {  // BVS $FCB6
            const bool taken = (registers.status & kOverflow) != 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFCB6U)
                : static_cast<std::uint16_t>(0xFC97U);
            append_checkpoint(0xFC95U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFC97U:  // LDA $F0,X
            registers.accumulator = zp(channel(kRateAccumulator));
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFC97U, 0xFC99U, 12U);
            pc = 0xFC99U;
            break;

        case 0xFC99U:
        case 0xFCB8U:
        case 0xFD8DU:
        case 0xFDC4U: {  // SBC $EF,X
            const auto operand = zp(channel(kRateDelta));
            const auto difference = static_cast<std::uint16_t>(
                registers.accumulator - operand
                - static_cast<std::uint16_t>(
                    (registers.status & kCarry) ^ kCarry));
            registers.status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(
                    ~(kZero | kCarry | kNegative | kOverflow)));
            const auto signed_overflow = static_cast<std::uint8_t>(
                ((registers.accumulator ^ difference)
                 & (registers.accumulator ^ operand) & 0x80U)
                >> 1U);
            registers.status = static_cast<std::uint8_t>(
                registers.status | signed_overflow);
            registers.status = static_cast<std::uint8_t>(
                registers.status
                | static_cast<std::uint8_t>(
                    (static_cast<std::uint8_t>(difference >> 8U) & kCarry)
                    ^ kCarry));
            registers.accumulator = static_cast<std::uint8_t>(difference);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            const auto next = static_cast<std::uint16_t>(pc + 2U);
            append_checkpoint(pc, next, 12U);
            pc = next;
            break;
        }

        case 0xFC9BU:  // STA $F0,X
            store(
                0xFC9BU,
                logical(channel(kRateAccumulator)),
                registers.accumulator);
            append_checkpoint(0xFC9BU, 0xFC9DU, 12U);
            pc = 0xFC9DU;
            break;

        case 0xFC9DU: {  // BCS $FC74
            const bool taken = (registers.status & kCarry) != 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFC74U)
                : static_cast<std::uint16_t>(0xFC9FU);
            append_checkpoint(0xFC9DU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFC9FU:  // TYA
            registers.accumulator = registers.y;
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFC9FU, 0xFCA0U, 6U);
            pc = 0xFCA0U;
            break;

        case 0xFCA0U:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xFCA0U, 0xFCA1U, 6U);
            pc = 0xFCA1U;
            break;

        case 0xFCA1U:
        case 0xFCBFU:
        case 0xFD97U:
        case 0xFDCBU: {  // ADC #imm
            const std::uint8_t operand =
                pc == 0xFCBFU ? 0x04U : (pc == 0xFDCBU ? 0x03U : 0x02U);
            const auto sum = static_cast<std::uint16_t>(
                registers.accumulator + operand
                + static_cast<std::uint16_t>(registers.status & kCarry));
            registers.status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(
                    ~(kZero | kCarry | kNegative | kOverflow)));
            const auto signed_overflow = static_cast<std::uint8_t>(
                ((((registers.accumulator ^ operand) & 0x80U) ^ 0x80U)
                 & ((registers.accumulator ^ sum) & 0x80U))
                >> 1U);
            registers.status = static_cast<std::uint8_t>(
                registers.status | signed_overflow);
            if ((sum & 0xFF00U) != 0U) {
                registers.status = static_cast<std::uint8_t>(
                    registers.status | kCarry);
            }
            registers.accumulator = static_cast<std::uint8_t>(sum);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            const auto next = static_cast<std::uint16_t>(pc + 2U);
            append_checkpoint(pc, next, 6U);
            pc = next;
            break;
        }

        case 0xFCA3U:
        case 0xFCC1U:
        case 0xFD2AU:
        case 0xFD99U:
        case 0xFDCDU: {  // BIT #$08
            constexpr std::uint8_t operand = 0x08U;
            registers.status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(
                    ~(kZero | kOverflow | kNegative)));
            if ((operand & registers.accumulator) == 0U) {
                registers.status = static_cast<std::uint8_t>(
                    registers.status | kZero);
            }
            const auto next = static_cast<std::uint16_t>(pc + 2U);
            append_checkpoint(pc, next, 6U);
            pc = next;
            break;
        }

        case 0xFCA5U: {  // BNE $FCB3
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFCB3U)
                : static_cast<std::uint16_t>(0xFCA7U);
            append_checkpoint(0xFCA5U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFCA7U:  // STA $E4,X
            store(
                0xFCA7U,
                logical(channel(kChannelState)),
                registers.accumulator);
            append_checkpoint(0xFCA7U, 0xFCA9U, 12U);
            pc = 0xFCA9U;
            break;

        case 0xFCA9U:
        case 0xFD14U:
        case 0xFD93U: {  // ASL $E3,X
            const auto value = zp(channel(kSampleBits));
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            registers.status = static_cast<std::uint8_t>(
                registers.status | static_cast<std::uint8_t>(value >> 7U));
            const auto shifted = static_cast<std::uint8_t>(value << 1U);
            registers.status = set_nz(registers.status, shifted);
            store(pc, logical(channel(kSampleBits)), shifted);
            const auto next = static_cast<std::uint16_t>(pc + 2U);
            append_checkpoint(pc, next, 18U);
            pc = next;
            break;
        }

        case 0xFCABU:
        case 0xFCEEU:
        case 0xFD30U:
        case 0xFD38U:
        case 0xFD4AU:
        case 0xFD59U:
        case 0xFD63U:
        case 0xFDAFU:
        case 0xFDBAU: {  // INC $DD,X
            const auto value = static_cast<std::uint8_t>(
                zp(channel(kSequenceLow)) + 1U);
            registers.status = set_nz(registers.status, value);
            store(pc, logical(channel(kSequenceLow)), value);
            const auto next = static_cast<std::uint16_t>(pc + 2U);
            append_checkpoint(pc, next, 18U);
            pc = next;
            break;
        }

        case 0xFCADU: {  // BNE $FC78
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFC78U)
                : static_cast<std::uint16_t>(0xFCAFU);
            append_checkpoint(0xFCADU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFCAFU:
        case 0xFCD7U:
        case 0xFCF2U:
        case 0xFD03U:
        case 0xFD34U:
        case 0xFD3CU:
        case 0xFD4EU:
        case 0xFD5DU:
        case 0xFD67U:
        case 0xFDB3U:
        case 0xFDBEU: {  // INC $DE,X
            const auto value = static_cast<std::uint8_t>(
                zp(channel(kSequenceHigh)) + 1U);
            registers.status = set_nz(registers.status, value);
            store(pc, logical(channel(kSequenceHigh)), value);
            const auto next = static_cast<std::uint16_t>(pc + 2U);
            append_checkpoint(pc, next, 18U);
            pc = next;
            break;
        }

        case 0xFCB1U: {  // BNE $FC78
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFC78U)
                : static_cast<std::uint16_t>(0xFCB3U);
            append_checkpoint(0xFCB1U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFCB3U:  // JMP $FD3E
            append_checkpoint(0xFCB3U, 0xFD3EU, 12U);
            pc = 0xFD3EU;
            break;

        case 0xFCB6U:
        case 0xFD8BU:
        case 0xFDC2U:  // LDA $F0,X
            registers.accumulator = zp(channel(kRateAccumulator));
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(
                pc, static_cast<std::uint16_t>(pc + 2U), 12U);
            pc = static_cast<std::uint16_t>(pc + 2U);
            break;

        case 0xFCBAU:
        case 0xFD8FU:
        case 0xFDC6U:  // STA $F0,X
            store(
                pc,
                logical(channel(kRateAccumulator)),
                registers.accumulator);
            append_checkpoint(
                pc, static_cast<std::uint16_t>(pc + 2U), 12U);
            pc = static_cast<std::uint16_t>(pc + 2U);
            break;

        case 0xFCBCU: {  // BCS $FC9F
            const bool taken = (registers.status & kCarry) != 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFC9FU)
                : static_cast<std::uint16_t>(0xFCBEU);
            append_checkpoint(0xFCBCU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFCBEU:
        case 0xFD29U:
        case 0xFD95U:
        case 0xFDCAU:  // TYA
            registers.accumulator = registers.y;
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(
                pc, static_cast<std::uint16_t>(pc + 1U), 6U);
            pc = static_cast<std::uint16_t>(pc + 1U);
            break;

        case 0xFCC3U: {  // BNE $FCDC
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFCDCU)
                : static_cast<std::uint16_t>(0xFCC5U);
            append_checkpoint(0xFCC3U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFCC5U:
        case 0xFCEAU:
        case 0xFD2EU:
        case 0xFD48U:
        case 0xFD55U:
        case 0xFD85U:
        case 0xFD9DU:
        case 0xFDABU:
        case 0xFDD1U:  // STA $E4,X
            store(
                pc,
                logical(channel(kChannelState)),
                registers.accumulator);
            append_checkpoint(
                pc, static_cast<std::uint16_t>(pc + 2U), 12U);
            pc = static_cast<std::uint16_t>(pc + 2U);
            break;

        case 0xFCC7U:
        case 0xFDD3U:  // LDA $E3,X
            registers.accumulator = zp(channel(kSampleBits));
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(
                pc,
                pc == 0xFCC7U
                    ? static_cast<std::uint16_t>(0xFCC9U)
                    : static_cast<std::uint16_t>(0xFDD5U),
                12U);
            pc = pc == 0xFCC7U
                ? static_cast<std::uint16_t>(0xFCC9U)
                : static_cast<std::uint16_t>(0xFDD5U);
            break;

        case 0xFCC9U:
        case 0xFCCAU:
        case 0xFCCBU:
        case 0xFCF6U:
        case 0xFCF7U:
        case 0xFDB7U: {  // ASL A
            const auto value = registers.accumulator;
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            registers.status = static_cast<std::uint8_t>(
                registers.status | static_cast<std::uint8_t>(value >> 7U));
            registers.accumulator = static_cast<std::uint8_t>(value << 1U);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            const auto next = static_cast<std::uint16_t>(pc + 1U);
            append_checkpoint(pc, next, 6U);
            pc = next;
            break;
        }

        case 0xFCCCU:
        case 0xFCF8U:
        case 0xFD61U:
        case 0xFDB8U:  // STA $E3,X
            store(
                pc,
                logical(channel(kSampleBits)),
                registers.accumulator);
            append_checkpoint(
                pc, static_cast<std::uint16_t>(pc + 2U), 12U);
            pc = static_cast<std::uint16_t>(pc + 2U);
            break;

        case 0xFCCEU:
        case 0xFCFBU:  // LDA $DD,X
            registers.accumulator = zp(channel(kSequenceLow));
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(
                pc, static_cast<std::uint16_t>(pc + 2U), 12U);
            pc = static_cast<std::uint16_t>(pc + 2U);
            break;

        case 0xFCD0U:
        case 0xFCFAU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(
                pc, static_cast<std::uint16_t>(pc + 1U), 6U);
            pc = static_cast<std::uint16_t>(pc + 1U);
            break;

        case 0xFCD1U:
        case 0xFCFDU: {  // ADC #$02
            constexpr std::uint8_t operand = 0x02U;
            const auto sum = static_cast<std::uint16_t>(
                registers.accumulator + operand
                + static_cast<std::uint16_t>(registers.status & kCarry));
            registers.status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(
                    ~(kZero | kCarry | kNegative | kOverflow)));
            const auto signed_overflow = static_cast<std::uint8_t>(
                ((((registers.accumulator ^ operand) & 0x80U) ^ 0x80U)
                 & ((registers.accumulator ^ sum) & 0x80U))
                >> 1U);
            registers.status = static_cast<std::uint8_t>(
                registers.status | signed_overflow);
            if ((sum & 0xFF00U) != 0U) {
                registers.status = static_cast<std::uint8_t>(
                    registers.status | kCarry);
            }
            registers.accumulator = static_cast<std::uint8_t>(sum);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            const auto next = static_cast<std::uint16_t>(pc + 2U);
            append_checkpoint(pc, next, 6U);
            pc = next;
            break;
        }

        case 0xFCD3U:
        case 0xFCFFU:  // STA $DD,X
            store(
                pc,
                logical(channel(kSequenceLow)),
                registers.accumulator);
            append_checkpoint(
                pc, static_cast<std::uint16_t>(pc + 2U), 12U);
            pc = static_cast<std::uint16_t>(pc + 2U);
            break;

        case 0xFCD5U: {  // BCC $FC78
            const bool taken = (registers.status & kCarry) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFC78U)
                : static_cast<std::uint16_t>(0xFCD7U);
            append_checkpoint(0xFCD5U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFCD9U:  // JMP $FC78
            append_checkpoint(0xFCD9U, 0xFC78U, 12U);
            pc = 0xFC78U;
            break;

        case 0xFCDCU: {  // BIT #$07
            constexpr std::uint8_t operand = 0x07U;
            registers.status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(
                    ~(kZero | kOverflow | kNegative)));
            if ((operand & registers.accumulator) == 0U) {
                registers.status = static_cast<std::uint8_t>(
                    registers.status | kZero);
            }
            append_checkpoint(0xFCDCU, 0xFCDEU, 6U);
            pc = 0xFCDEU;
            break;
        }

        case 0xFCDEU: {  // BEQ $FD38
            const bool taken = (registers.status & kZero) != 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFD38U)
                : static_cast<std::uint16_t>(0xFCE0U);
            append_checkpoint(0xFCDEU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFCE0U:
        case 0xFD3EU:
        case 0xFDA1U: {  // INC $E9,X
            const auto value = static_cast<std::uint8_t>(
                zp(channel(kHoldLow)) + 1U);
            registers.status = set_nz(registers.status, value);
            store(pc, logical(channel(kHoldLow)), value);
            const auto next = static_cast<std::uint16_t>(pc + 2U);
            append_checkpoint(pc, next, 18U);
            pc = next;
            break;
        }

        case 0xFCE2U: {  // BNE $FCE8
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFCE8U)
                : static_cast<std::uint16_t>(0xFCE4U);
            append_checkpoint(0xFCE2U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFCE4U:
        case 0xFD42U:
        case 0xFDA5U: {  // INC $EA,X
            const auto value = static_cast<std::uint8_t>(
                zp(channel(kHoldHigh)) + 1U);
            registers.status = set_nz(registers.status, value);
            store(pc, logical(channel(kHoldHigh)), value);
            const auto next = static_cast<std::uint16_t>(pc + 2U);
            append_checkpoint(pc, next, 18U);
            pc = next;
            break;
        }

        case 0xFCE6U: {  // BEQ $FD46
            const bool taken = (registers.status & kZero) != 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFD46U)
                : static_cast<std::uint16_t>(0xFCE8U);
            append_checkpoint(0xFCE6U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFCE8U:
        case 0xFD53U:
        case 0xFDA9U:  // AND #$F7
            registers.accumulator = static_cast<std::uint8_t>(
                registers.accumulator & 0xF7U);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(
                pc, static_cast<std::uint16_t>(pc + 2U), 6U);
            pc = static_cast<std::uint16_t>(pc + 2U);
            break;

        case 0xFCECU: {  // BPL $FCFB
            const bool taken = (registers.status & kNegative) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFCFBU)
                : static_cast<std::uint16_t>(0xFCEEU);
            append_checkpoint(0xFCECU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFCF0U: {  // BNE $FCF4
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFCF4U)
                : static_cast<std::uint16_t>(0xFCF2U);
            append_checkpoint(0xFCF0U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFCF4U:
        case 0xFD5FU:
        case 0xFDB5U:  // LDA ($DD,X)
            registers.accumulator = read_mapped(cursor());
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(
                pc, static_cast<std::uint16_t>(pc + 2U), 21U);
            pc = static_cast<std::uint16_t>(pc + 2U);
            break;

        case 0xFD01U: {  // BCC $FD69
            const bool taken = (registers.status & kCarry) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFD69U)
                : static_cast<std::uint16_t>(0xFD03U);
            append_checkpoint(0xFD01U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFD05U: {  // BCS $FD69
            const bool taken = (registers.status & kCarry) != 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFD69U)
                : static_cast<std::uint16_t>(0xFD07U);
            append_checkpoint(0xFD05U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFD07U: {  // BVC $FD0F
            const bool taken = (registers.status & kOverflow) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFD0FU)
                : static_cast<std::uint16_t>(0xFD09U);
            append_checkpoint(0xFD07U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFD09U:  // AND #$BF
            registers.accumulator = static_cast<std::uint8_t>(
                registers.accumulator & 0xBFU);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFD09U, 0xFD0BU, 6U);
            pc = 0xFD0BU;
            break;

        case 0xFD0BU:
        case 0xFD0FU:  // TAY
            registers.y = registers.accumulator;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(
                pc, static_cast<std::uint16_t>(pc + 1U), 6U);
            pc = static_cast<std::uint16_t>(pc + 1U);
            break;

        case 0xFD0CU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xFD0CU, 0xFD0DU, 6U);
            pc = 0xFD0DU;
            break;

        case 0xFD0DU:  // BCC $FD1E
            append_checkpoint(0xFD0DU, 0xFD1EU, 12U);
            pc = 0xFD1EU;
            break;

        case 0xFD10U:  // LDA ($DD,X)
            registers.accumulator = read_mapped(cursor());
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFD10U, 0xFD12U, 21U);
            pc = 0xFD12U;
            break;

        case 0xFD12U:  // AND #$0F
            registers.accumulator = static_cast<std::uint8_t>(
                registers.accumulator & 0x0FU);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFD12U, 0xFD14U, 6U);
            pc = 0xFD14U;
            break;

        case 0xFD16U: {  // ROL A
            const auto value = registers.accumulator;
            const auto carry_out = static_cast<std::uint8_t>(value >> 7U);
            auto rotated = static_cast<std::uint8_t>(value << 1U);
            rotated = static_cast<std::uint8_t>(
                rotated | static_cast<std::uint8_t>(
                    registers.status & kCarry));
            registers.status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(
                    ~(kZero | kNegative | kCarry)));
            registers.status = static_cast<std::uint8_t>(
                registers.status | carry_out);
            registers.accumulator = rotated;
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFD16U, 0xFD17U, 6U);
            pc = 0xFD17U;
            break;
        }

        case 0xFD17U:  // STA $0806
            store(0xFD17U, kPsgWaveform, registers.accumulator);
            append_checkpoint(0xFD17U, 0xFD1AU, 15U);
            pc = 0xFD1AU;
            break;

        case 0xFD1AU: {  // BIT $F6,X
            const auto operand = zp(channel(kChannelFlags));
            registers.status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(
                    ~(kZero | kOverflow | kNegative)));
            if ((operand & registers.accumulator) == 0U) {
                registers.status = static_cast<std::uint8_t>(
                    registers.status | kZero);
            }
            registers.status = static_cast<std::uint8_t>(
                registers.status
                | static_cast<std::uint8_t>(
                    operand & (kOverflow | kNegative)));
            append_checkpoint(0xFD1AU, 0xFD1CU, 12U);
            pc = 0xFD1CU;
            break;
        }

        case 0xFD1CU: {  // BMI $FD89
            const bool taken = (registers.status & kNegative) != 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFD89U)
                : static_cast<std::uint16_t>(0xFD1EU);
            append_checkpoint(0xFD1CU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFD1EU:  // LDA $EF,X
            registers.accumulator = zp(channel(kRateDelta));
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFD1EU, 0xFD20U, 12U);
            pc = 0xFD20U;
            break;

        case 0xFD20U: {  // BEQ $FD28
            const bool taken = (registers.status & kZero) != 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFD28U)
                : static_cast<std::uint16_t>(0xFD22U);
            append_checkpoint(0xFD20U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFD22U: {  // ADC $F0,X
            const auto operand = zp(channel(kRateAccumulator));
            const auto sum = static_cast<std::uint16_t>(
                registers.accumulator + operand
                + static_cast<std::uint16_t>(registers.status & kCarry));
            registers.status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(
                    ~(kZero | kCarry | kNegative | kOverflow)));
            const auto signed_overflow = static_cast<std::uint8_t>(
                ((((registers.accumulator ^ operand) & 0x80U) ^ 0x80U)
                 & ((registers.accumulator ^ sum) & 0x80U))
                >> 1U);
            registers.status = static_cast<std::uint8_t>(
                registers.status | signed_overflow);
            if ((sum & 0xFF00U) != 0U) {
                registers.status = static_cast<std::uint8_t>(
                    registers.status | kCarry);
            }
            registers.accumulator = static_cast<std::uint8_t>(sum);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFD22U, 0xFD24U, 12U);
            pc = 0xFD24U;
            break;
        }

        case 0xFD24U:  // STA $F0,X
            store(
                0xFD24U,
                logical(channel(kRateAccumulator)),
                registers.accumulator);
            append_checkpoint(0xFD24U, 0xFD26U, 12U);
            pc = 0xFD26U;
            break;

        case 0xFD26U: {  // BCC $FD82
            const bool taken = (registers.status & kCarry) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFD82U)
                : static_cast<std::uint16_t>(0xFD28U);
            append_checkpoint(0xFD26U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFD28U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xFD28U, 0xFD29U, 6U);
            pc = 0xFD29U;
            break;

        case 0xFD2CU: {  // BNE $FD3E
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFD3EU)
                : static_cast<std::uint16_t>(0xFD2EU);
            append_checkpoint(0xFD2CU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFD32U: {  // BNE $FD69
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFD69U)
                : static_cast<std::uint16_t>(0xFD34U);
            append_checkpoint(0xFD32U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFD36U: {  // BNE $FD69
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFD69U)
                : static_cast<std::uint16_t>(0xFD38U);
            append_checkpoint(0xFD36U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFD3AU: {  // BNE $FD3E
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFD3EU)
                : static_cast<std::uint16_t>(0xFD3CU);
            append_checkpoint(0xFD3AU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFD40U: {  // BNE $FD53
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFD53U)
                : static_cast<std::uint16_t>(0xFD42U);
            append_checkpoint(0xFD40U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFD44U: {  // BNE $FD53
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFD53U)
                : static_cast<std::uint16_t>(0xFD46U);
            append_checkpoint(0xFD44U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFD46U:  // AND #$F0
            registers.accumulator = static_cast<std::uint8_t>(
                registers.accumulator & 0xF0U);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFD46U, 0xFD48U, 6U);
            pc = 0xFD48U;
            break;

        case 0xFD4CU: {  // BNE $FD50
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFD50U)
                : static_cast<std::uint16_t>(0xFD4EU);
            append_checkpoint(0xFD4CU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFD50U:  // JMP $FDDB
            append_checkpoint(0xFD50U, 0xFDDBU, 12U);
            pc = 0xFDDBU;
            break;

        case 0xFD57U: {  // BPL $FD63
            const bool taken = (registers.status & kNegative) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFD63U)
                : static_cast<std::uint16_t>(0xFD59U);
            append_checkpoint(0xFD57U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFD5BU: {  // BNE $FD5F
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFD5FU)
                : static_cast<std::uint16_t>(0xFD5DU);
            append_checkpoint(0xFD5BU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFD65U: {  // BNE $FD69
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFD69U)
                : static_cast<std::uint16_t>(0xFD67U);
            append_checkpoint(0xFD65U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFD69U:
        case 0xFD6AU: {  // DEX
            registers.x = static_cast<std::uint8_t>(registers.x - 1U);
            registers.status = set_nz(registers.status, registers.x);
            const auto next = static_cast<std::uint16_t>(pc + 1U);
            append_checkpoint(pc, next, 6U);
            pc = next;
            break;
        }

        case 0xFD6BU: {  // BMI $FD70
            const bool taken = (registers.status & kNegative) != 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFD70U)
                : static_cast<std::uint16_t>(0xFD6DU);
            append_checkpoint(0xFD6BU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFD6DU:  // JMP $FC3B
            append_checkpoint(0xFD6DU, 0xFC3BU, 12U);
            pc = 0xFC3BU;
            break;

        // The track-advance tail's epilogue. It is the same twelve bytes as
        // FC7C's, assembled a second time rather than branched to, and it
        // splits at the same instruction for the same reason.
        case 0xFD70U:  // PLA
            registers.accumulator = pull();
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFD70U, 0xFD71U, 12U);
            pc = 0xFD71U;
            break;

        case 0xFD71U:  // TAM #$40
            registers.mpr[6] = registers.accumulator;
            append_checkpoint(0xFD71U, 0xFD73U, 15U);
            pc = 0xFD73U;
            break;

        case 0xFD73U:  // PLY
            registers.y = pull();
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xFD73U, 0xFD74U, 12U);
            pc = 0xFD74U;
            break;

        case 0xFD74U:  // PLX
            registers.x = pull();
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0xFD74U, kSecondMaskPc, 12U);
            pc = kSecondMaskPc;
            break;

        case 0xFD76U:  // STZ $D9
            store(0xFD76U, logical(kHandlerBusy), 0U);
            append_checkpoint(0xFD76U, 0xFD78U, 12U);
            pc = 0xFD78U;
            break;

        case 0xFD78U:  // LDA $DA
            registers.accumulator = zp(kHandlerRerun);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFD78U, 0xFD7AU, 12U);
            pc = 0xFD7AU;
            break;

        case 0xFD7AU: {  // BEQ $FD80
            const bool taken = (registers.status & kZero) != 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFD80U)
                : static_cast<std::uint16_t>(0xFD7CU);
            append_checkpoint(0xFD7AU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFD7CU:  // PLA
            registers.accumulator = pull();
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFD7CU, 0xFD7DU, 12U);
            pc = 0xFD7DU;
            break;

        case 0xFD80U:  // PLA
            registers.accumulator = pull();
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFD80U, 0xFD81U, 12U);
            pc = 0xFD81U;
            break;

        case 0xFD82U:  // TYA
            registers.accumulator = registers.y;
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFD82U, 0xFD83U, 6U);
            pc = 0xFD83U;
            break;

        case 0xFD83U:  // ORA #$40
            registers.accumulator = static_cast<std::uint8_t>(
                registers.accumulator | 0x40U);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFD83U, 0xFD85U, 6U);
            pc = 0xFD85U;
            break;

        case 0xFD87U: {  // BNE $FD69
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFD69U)
                : static_cast<std::uint16_t>(0xFD89U);
            append_checkpoint(0xFD87U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFD89U: {  // BVS $FDC2
            const bool taken = (registers.status & kOverflow) != 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFDC2U)
                : static_cast<std::uint16_t>(0xFD8BU);
            append_checkpoint(0xFD89U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFD91U: {  // BCS $FD28
            const bool taken = (registers.status & kCarry) != 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFD28U)
                : static_cast<std::uint16_t>(0xFD93U);
            append_checkpoint(0xFD91U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFD96U:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xFD96U, 0xFD97U, 6U);
            pc = 0xFD97U;
            break;

        case 0xFD9BU: {  // BNE $FDA1
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFDA1U)
                : static_cast<std::uint16_t>(0xFD9DU);
            append_checkpoint(0xFD9BU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFD9FU: {  // BEQ $FD63
            const bool taken = (registers.status & kZero) != 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFD63U)
                : static_cast<std::uint16_t>(0xFDA1U);
            append_checkpoint(0xFD9FU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFDA3U: {  // BNE $FDA9
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFDA9U)
                : static_cast<std::uint16_t>(0xFDA5U);
            append_checkpoint(0xFDA3U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFDA7U: {  // BEQ $FD46
            const bool taken = (registers.status & kZero) != 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFD46U)
                : static_cast<std::uint16_t>(0xFDA9U);
            append_checkpoint(0xFDA7U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFDADU: {  // BPL $FDBA
            const bool taken = (registers.status & kNegative) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFDBAU)
                : static_cast<std::uint16_t>(0xFDAFU);
            append_checkpoint(0xFDADU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFDB1U: {  // BNE $FDB5
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFDB5U)
                : static_cast<std::uint16_t>(0xFDB3U);
            append_checkpoint(0xFDB1U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFDBCU: {  // BNE $FD69
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFD69U)
                : static_cast<std::uint16_t>(0xFDBEU);
            append_checkpoint(0xFDBCU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFDC0U: {  // BNE $FD69
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFD69U)
                : static_cast<std::uint16_t>(0xFDC2U);
            append_checkpoint(0xFDC0U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFDC8U: {  // BCS $FD93
            const bool taken = (registers.status & kCarry) != 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFD93U)
                : static_cast<std::uint16_t>(0xFDCAU);
            append_checkpoint(0xFDC8U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFDCFU: {  // BNE $FDD8
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFDD8U)
                : static_cast<std::uint16_t>(0xFDD1U);
            append_checkpoint(0xFDCFU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFDD5U:  // JMP $FCCA
            append_checkpoint(0xFDD5U, 0xFCCAU, 12U);
            pc = 0xFCCAU;
            break;

        case 0xFDD8U:  // JMP $FCDC
            append_checkpoint(0xFDD8U, 0xFCDCU, 12U);
            pc = 0xFCDCU;
            break;

        case 0xFDDBU:  // LDA $E4,X
            registers.accumulator = zp(channel(kChannelState));
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFDDBU, 0xFDDDU, 12U);
            pc = 0xFDDDU;
            break;

        case 0xFDDDU: {  // BIT #$20
            constexpr std::uint8_t operand = 0x20U;
            registers.status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(
                    ~(kZero | kOverflow | kNegative)));
            if ((operand & registers.accumulator) == 0U) {
                registers.status = static_cast<std::uint8_t>(
                    registers.status | kZero);
            }
            append_checkpoint(0xFDDDU, 0xFDDFU, 6U);
            pc = 0xFDDFU;
            break;
        }

        case 0xFDDFU: {  // BEQ $FDF4
            const bool taken = (registers.status & kZero) != 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFDF4U)
                : static_cast<std::uint16_t>(0xFDE1U);
            append_checkpoint(0xFDDFU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFDE1U:  // STZ $0805
            store(0xFDE1U, kPsgBalance, 0U);
            append_checkpoint(0xFDE1U, 0xFDE4U, 15U);
            pc = 0xFDE4U;
            break;

        case 0xFDE4U:  // STZ $F5,X
            store(0xFDE4U, logical(channel(kChannelBank)), 0U);
            append_checkpoint(0xFDE4U, 0xFDE6U, 12U);
            pc = 0xFDE6U;
            break;

        case 0xFDE6U:  // LDA $F5
            registers.accumulator = zp(kChannelBank);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFDE6U, 0xFDE8U, 12U);
            pc = 0xFDE8U;
            break;

        case 0xFDE8U:
        case 0xFDEAU: {  // ORA $F7 / ORA $F9
            const auto operand = zp(
                pc == 0xFDE8U
                    ? static_cast<std::uint16_t>(0x00F7U)
                    : static_cast<std::uint16_t>(0x00F9U));
            registers.accumulator = static_cast<std::uint8_t>(
                registers.accumulator | operand);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            const auto next = static_cast<std::uint16_t>(pc + 2U);
            append_checkpoint(pc, next, 12U);
            pc = next;
            break;
        }

        case 0xFDECU: {  // BNE $FDF1
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFDF1U)
                : static_cast<std::uint16_t>(0xFDEEU);
            append_checkpoint(0xFDECU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFDEEU:  // STZ $0C01
            store(0xFDEEU, kTimerControl, 0U);
            append_checkpoint(0xFDEEU, 0xFDF1U, 15U);
            pc = 0xFDF1U;
            break;

        case 0xFDF1U:  // JMP $FD69
            append_checkpoint(0xFDF1U, 0xFD69U, 12U);
            pc = 0xFD69U;
            break;

        case 0xFDF4U:  // LDA $FC06
            registers.accumulator = read_mapped(kBankTable);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFDF4U, 0xFDF7U, 15U);
            pc = 0xFDF7U;
            break;

        case 0xFDF7U:  // TAM #$40
            registers.mpr[6] = registers.accumulator;
            append_checkpoint(0xFDF7U, 0xFDF9U, 15U);
            pc = 0xFDF9U;
            break;

        case 0xFDF9U:  // LDA $2E10,X
            registers.accumulator = ram[kTrackPointer + registers.x];
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFDF9U, 0xFDFCU, 15U);
            pc = 0xFDFCU;
            break;

        case 0xFDFCU:  // STA $DB
            store(0xFDFCU, logical(kCursorLow), registers.accumulator);
            append_checkpoint(0xFDFCU, 0xFDFEU, 12U);
            pc = 0xFDFEU;
            break;

        case 0xFDFEU:  // LDA $2E11,X
            registers.accumulator = ram[kTrackPointer + 1U + registers.x];
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFDFEU, 0xFE01U, 15U);
            pc = 0xFE01U;
            break;

        case 0xFE01U:  // STA $DC
            store(0xFE01U, logical(kCursorHigh), registers.accumulator);
            append_checkpoint(0xFE01U, 0xFE03U, 12U);
            pc = 0xFE03U;
            break;

        case 0xFE03U:  // LDY #$02
            registers.y = 0x02U;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xFE03U, 0xFE05U, 6U);
            pc = 0xFE05U;
            break;

        case 0xFE05U:
        case 0xFE17U: {  // LDA ($DB)
            const auto pointer = static_cast<std::uint16_t>(
                zp(kCursorLow)
                | static_cast<std::uint16_t>(zp(kCursorHigh)) << 8U);
            registers.accumulator = read_mapped(pointer);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            const auto next = static_cast<std::uint16_t>(pc + 2U);
            append_checkpoint(pc, next, 21U);
            pc = next;
            break;
        }

        case 0xFE07U: {  // BIT #$40
            constexpr std::uint8_t operand = 0x40U;
            registers.status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(
                    ~(kZero | kOverflow | kNegative)));
            if ((operand & registers.accumulator) == 0U) {
                registers.status = static_cast<std::uint8_t>(
                    registers.status | kZero);
            }
            registers.status = static_cast<std::uint8_t>(
                registers.status
                | static_cast<std::uint8_t>(
                    operand & (kOverflow | kNegative)));
            append_checkpoint(0xFE07U, 0xFE09U, 6U);
            pc = 0xFE09U;
            break;
        }

        case 0xFE09U: {  // BEQ $FE19
            const bool taken = (registers.status & kZero) != 0U;
            const auto next = taken
                ? static_cast<std::uint16_t>(0xFE19U)
                : static_cast<std::uint16_t>(0xFE0BU);
            append_checkpoint(0xFE09U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xFE0BU:
        case 0xFE0FU:
        case 0xFE1BU:
        case 0xFE20U:
        case 0xFE25U:
        case 0xFE2AU:
        case 0xFE30U: {  // LDA ($DB),Y
            const auto pointer = static_cast<std::uint16_t>(
                zp(kCursorLow)
                | static_cast<std::uint16_t>(zp(kCursorHigh)) << 8U);
            registers.accumulator = read_mapped(
                static_cast<std::uint16_t>(pointer + registers.y));
            registers.status = set_nz(
                registers.status, registers.accumulator);
            const auto next = static_cast<std::uint16_t>(pc + 2U);
            append_checkpoint(pc, next, 21U);
            pc = next;
            break;
        }

        case 0xFE0DU:  // PHA
            push(0xFE0DU, registers.accumulator);
            append_checkpoint(0xFE0DU, 0xFE0EU, 9U);
            pc = 0xFE0EU;
            break;

        case 0xFE0EU:
        case 0xFE1FU:
        case 0xFE24U:
        case 0xFE29U:
        case 0xFE2EU:
        case 0xFE2FU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(
                pc, static_cast<std::uint16_t>(pc + 1U), 6U);
            pc = static_cast<std::uint16_t>(pc + 1U);
            break;

        case 0xFE11U:  // STA $DC
            store(0xFE11U, logical(kCursorHigh), registers.accumulator);
            append_checkpoint(0xFE11U, 0xFE13U, 12U);
            pc = 0xFE13U;
            break;

        case 0xFE13U:  // PLA
            registers.accumulator = pull();
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFE13U, 0xFE14U, 12U);
            pc = 0xFE14U;
            break;

        case 0xFE14U:  // STA $DB
            store(0xFE14U, logical(kCursorLow), registers.accumulator);
            append_checkpoint(0xFE14U, 0xFE16U, 12U);
            pc = 0xFE16U;
            break;

        case 0xFE16U:  // DEY
            registers.y = static_cast<std::uint8_t>(registers.y - 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xFE16U, 0xFE17U, 6U);
            pc = 0xFE17U;
            break;

        case 0xFE19U:  // STA $E4,X
            store(
                0xFE19U,
                logical(channel(kChannelState)),
                registers.accumulator);
            append_checkpoint(0xFE19U, 0xFE1BU, 12U);
            pc = 0xFE1BU;
            break;

        case 0xFE1DU:  // STA $DD,X
            store(
                0xFE1DU,
                logical(channel(kSequenceLow)),
                registers.accumulator);
            append_checkpoint(0xFE1DU, 0xFE1FU, 12U);
            pc = 0xFE1FU;
            break;

        case 0xFE22U:  // STA $DE,X
            store(
                0xFE22U,
                logical(channel(kSequenceHigh)),
                registers.accumulator);
            append_checkpoint(0xFE22U, 0xFE24U, 12U);
            pc = 0xFE24U;
            break;

        case 0xFE27U:  // STA $E9,X
            store(
                0xFE27U,
                logical(channel(kHoldLow)),
                registers.accumulator);
            append_checkpoint(0xFE27U, 0xFE29U, 12U);
            pc = 0xFE29U;
            break;

        case 0xFE2CU:  // STA $EA,X
            store(
                0xFE2CU,
                logical(channel(kHoldHigh)),
                registers.accumulator);
            append_checkpoint(0xFE2CU, 0xFE2EU, 12U);
            pc = 0xFE2EU;
            break;

        case 0xFE32U:  // TAY
            registers.y = registers.accumulator;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xFE32U, 0xFE33U, 6U);
            pc = 0xFE33U;
            break;

        case 0xFE33U:  // LDA $FC06,Y
            registers.accumulator = read_mapped(
                static_cast<std::uint16_t>(kBankTable + registers.y));
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFE33U, 0xFE36U, 15U);
            pc = 0xFE36U;
            break;

        case 0xFE36U:  // ORA #$80
            registers.accumulator = static_cast<std::uint8_t>(
                registers.accumulator | 0x80U);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFE36U, 0xFE38U, 6U);
            pc = 0xFE38U;
            break;

        case 0xFE38U:  // STA $F5,X
            store(
                0xFE38U,
                logical(channel(kChannelBank)),
                registers.accumulator);
            append_checkpoint(0xFE38U, 0xFE3AU, 12U);
            pc = 0xFE3AU;
            break;

        case 0xFE3AU:  // LDA $DB
            registers.accumulator = zp(kCursorLow);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFE3AU, 0xFE3CU, 12U);
            pc = 0xFE3CU;
            break;

        case 0xFE3CU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xFE3CU, 0xFE3DU, 6U);
            pc = 0xFE3DU;
            break;

        case 0xFE3DU: {  // ADC #$08
            constexpr std::uint8_t operand = 0x08U;
            const auto sum = static_cast<std::uint16_t>(
                registers.accumulator + operand
                + static_cast<std::uint16_t>(registers.status & kCarry));
            registers.status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(
                    ~(kZero | kCarry | kNegative | kOverflow)));
            const auto signed_overflow = static_cast<std::uint8_t>(
                ((((registers.accumulator ^ operand) & 0x80U) ^ 0x80U)
                 & ((registers.accumulator ^ sum) & 0x80U))
                >> 1U);
            registers.status = static_cast<std::uint8_t>(
                registers.status | signed_overflow);
            if ((sum & 0xFF00U) != 0U) {
                registers.status = static_cast<std::uint8_t>(
                    registers.status | kCarry);
            }
            registers.accumulator = static_cast<std::uint8_t>(sum);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFE3DU, 0xFE3FU, 6U);
            pc = 0xFE3FU;
            break;
        }

        case 0xFE3FU:  // STA $2E10,X
            store(
                0xFE3FU,
                static_cast<std::uint16_t>(
                    0x2000U + kTrackPointer + registers.x),
                registers.accumulator);
            append_checkpoint(0xFE3FU, 0xFE42U, 15U);
            pc = 0xFE42U;
            break;

        case 0xFE42U:  // CLA
            registers.accumulator = 0U;
            append_checkpoint(0xFE42U, 0xFE43U, 6U);
            pc = 0xFE43U;
            break;

        case 0xFE43U: {  // ADC $DC
            const auto operand = zp(kCursorHigh);
            const auto sum = static_cast<std::uint16_t>(
                registers.accumulator + operand
                + static_cast<std::uint16_t>(registers.status & kCarry));
            registers.status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(
                    ~(kZero | kCarry | kNegative | kOverflow)));
            const auto signed_overflow = static_cast<std::uint8_t>(
                ((((registers.accumulator ^ operand) & 0x80U) ^ 0x80U)
                 & ((registers.accumulator ^ sum) & 0x80U))
                >> 1U);
            registers.status = static_cast<std::uint8_t>(
                registers.status | signed_overflow);
            if ((sum & 0xFF00U) != 0U) {
                registers.status = static_cast<std::uint8_t>(
                    registers.status | kCarry);
            }
            registers.accumulator = static_cast<std::uint8_t>(sum);
            registers.status = set_nz(
                registers.status, registers.accumulator);
            append_checkpoint(0xFE43U, 0xFE45U, 12U);
            pc = 0xFE45U;
            break;
        }

        case 0xFE45U:  // STA $2E11,X
            store(
                0xFE45U,
                static_cast<std::uint16_t>(
                    0x2000U + kTrackPointer + 1U + registers.x),
                registers.accumulator);
            append_checkpoint(0xFE45U, 0xFE48U, 15U);
            pc = 0xFE48U;
            break;

        case 0xFE48U:  // JMP $FD69
            append_checkpoint(0xFE48U, 0xFD69U, 12U);
            pc = 0xFD69U;
            break;

        default:
            return std::nullopt;
        }

        if (pc == kFirstReturnPc || pc == kSecondReturnPc
            || pc == kFirstChainPc || pc == kSecondChainPc
            || pc == kFirstMaskPc || pc == kSecondMaskPc) {
            break;
        }
    }

    replacement.return_instruction = {kBank, pc};
    replacement.output_registers = registers;
    replacement.body_master_cycles = cycle;
    if (replacement.checkpoints.empty()
        || replacement.checkpoints.back().registers != registers) {
        return std::nullopt;
    }
    return replacement;
}

}  // namespace devils_crush::app::detail
