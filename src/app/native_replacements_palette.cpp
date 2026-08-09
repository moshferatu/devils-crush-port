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

// The palette shadow buffer at $27B9-$2BB8. The game never writes the VCE
// directly from gameplay code: it keeps thirty-two 32-byte blocks of packed
// nine-bit GRB here, marks the ones that changed in $2CCC-$2CEB, and lets
// 00:EB30 push the dirty blocks out to the VCE ports once a frame.
//
// Four entry points are ported here. Three are leaves in the strong sense --
// no JSR, no device port, no state another thread of control writes -- which
// is what makes them the right first step into the video paths:
//
//   00:E93C  the sixteen-bit compare ($2F/$30 against $2D/$2E) the fade
//            drivers use to decide whether a source pointer has run past its
//            limit
//   00:E9B6  the block interpolator: one step of a thirty-two colour
//            crossfade, unpacking both the target and the current colour,
//            comparing their distance against the step counter at $2CEC and
//            nudging one channel of each colour toward the target
//   00:EBA5  the block stager: copy thirty-two bytes of a source palette into
//            block N of the shadow buffer and mark it dirty
//
// The fourth is 00:EB30, the upload itself, and it is the first routine in the
// project to drive the VCE. It reads the flags, and for each dirty block
// points the colour-table address at $0402/$0403 and pushes sixteen colours
// through $0404/$0405. Only its whole-palette arm is out of reach: when $2BB9
// is set the routine uploads all 1,024 bytes with a single TIA, and a
// transcript for that arm would need 1,024 writes against the channel's cap of
// 512, so the walk declines in front of it. Twenty-nine of 3,599 calls take
// that arm.
//
// Together they are 18.2M of the census route's 1,292M master cycles. The two
// drivers above them -- the crossfade at 00:E94D and the fade-to-black at
// 00:EA77 -- are not portable in the hybrid: both call the frame-sync spin at
// 00:E321, so a transcript for either would have to span twenty-one frames.
// 00:EAA7, the fade-to-black stepper, is 205,012 master cycles per call, which
// is past the checkpoint cap however the transcript is arranged.
constexpr std::uint8_t kBank = 0x00U;

constexpr std::uint8_t kCarry = 0x01U;
constexpr std::uint8_t kZero = 0x02U;
constexpr std::uint8_t kDecimal = 0x08U;
constexpr std::uint8_t kMemory = 0x20U;
constexpr std::uint8_t kNegative = 0x80U;

// Work-RAM window. Zero page is $2000-$20FF; the shadow buffer and its dirty
// flags sit at $27B9-$2BB8 and $2CCB-$2CEB. The walk keeps a private copy of
// the whole 8KB so it can read back its own stores.
constexpr std::uint16_t kWorkRamBase = 0x2000U;
constexpr std::uint16_t kWorkRamEnd = 0x4000U;

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
    return logical_pc == 0xE93CU || logical_pc == 0xE9B6U
        || logical_pc == 0xEB30U || logical_pc == 0xEBA5U;
}

// The RTS each entry point walks to. The transcript stops in front of it and
// the core executes the return itself. 00:EB30 has two of them: the early
// return it takes when nothing is dirty, and the one after the block walk.
[[nodiscard]] constexpr bool is_return_instruction(
    const std::uint16_t logical_pc)
{
    return logical_pc == 0xE94CU || logical_pc == 0xEA76U
        || logical_pc == 0xEB3AU || logical_pc == 0xEBA4U
        || logical_pc == 0xEBD6U;
}

} // namespace

std::optional<RoutineReplacement> replace_palette_shadow(
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
    // 00:EB30 addresses the VCE at $0400-$0405, which only reaches the device
    // while the hardware page is mapped at segment 0 -- the same condition the
    // core's write validator imposes before it will commit a port write.
    if (entry.routine.logical_pc == 0xEB30U
        && entry.registers.mpr[0] != 0xFFU) {
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

    // Reads the byte a logical address currently maps to. The palettes these
    // routines read from live in ROM, reached through whichever MPR slot the
    // caller left pointing at them, so the mapping is resolved generally
    // rather than assumed.
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
    // offset and the instruction of its checkpoint. They are recorded here and
    // priced by the next append_checkpoint, once the instruction's cycle count
    // is known. Nothing in this slice pushes, so unlike the sequencer's walk
    // there is no store that lands anywhere else.
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
    // which is where the core's LastCycle sits. A store to the VCE is the
    // exception: its last cycle is the one the VCE steals after the bus write,
    // so it samples two CPU cycles early and passes sample_back = 6.
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

    // Every store here targets work RAM: zero page, the shadow buffer or the
    // dirty flags. A store computed through a pointer that leaves work RAM is
    // not something this slice models, so it declines instead of guessing.
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

    // The VCE colour-table registers at $0402-$0405. These are the only stores
    // in this module that leave work RAM, and they are ordinary bus writes
    // that the core commits through the HuC6280 memory path; nothing here
    // needs to model the colour table, only to emit the writes in order.
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
    // ($2D),Y and ($2F),Y. The core adds Y without wrapping, so a sum past
    // $FFFF would index outside the logical address space; that declines
    // rather than guessing.
    const auto indirect_y =
        [&ram, &registers, &overflow](const std::uint8_t pointer) {
            const auto base = static_cast<std::uint32_t>(
                static_cast<std::uint16_t>(ram[pointer])
                | static_cast<std::uint16_t>(
                    ram[static_cast<std::uint8_t>(pointer + 1U)])
                    << 8U);
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

    // ASL A / LSR A / ROR A. The accumulator forms are six master cycles and
    // set carry from the bit shifted out.
    const auto shift_left_accumulator = [&registers]() {
        const auto carry = static_cast<std::uint8_t>(
            (registers.accumulator >> 7U) & kCarry);
        registers.accumulator = static_cast<std::uint8_t>(
            registers.accumulator << 1U);
        registers.status = set_nz(
            static_cast<std::uint8_t>(
                (registers.status & static_cast<std::uint8_t>(~kCarry))
                | carry),
            registers.accumulator);
    };
    const auto shift_right_accumulator = [&registers]() {
        const auto carry = static_cast<std::uint8_t>(
            registers.accumulator & kCarry);
        registers.accumulator = static_cast<std::uint8_t>(
            registers.accumulator >> 1U);
        registers.status = set_nz(
            static_cast<std::uint8_t>(
                (registers.status & static_cast<std::uint8_t>(~kCarry))
                | carry),
            registers.accumulator);
    };
    const auto rotate_right_accumulator = [&registers]() {
        const auto carry_in = static_cast<std::uint8_t>(
            (registers.status & kCarry) << 7U);
        const auto carry_out = static_cast<std::uint8_t>(
            registers.accumulator & kCarry);
        registers.accumulator = static_cast<std::uint8_t>(
            static_cast<std::uint8_t>(registers.accumulator >> 1U) | carry_in);
        registers.status = set_nz(
            static_cast<std::uint8_t>(
                (registers.status & static_cast<std::uint8_t>(~kCarry))
                | carry_out),
            registers.accumulator);
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
            || replacement.checkpoints.size() > 1800U
            || replacement.work_ram_writes.size() > 480U) {
            return std::nullopt;
        }

        // The transcript ends in front of the RTS, which the core then
        // executes itself.
        if (is_return_instruction(pc)) {
            registers.program_counter = pc;
            replacement.return_instruction = {kBank, pc};
            replacement.output_registers = registers;
            replacement.body_master_cycles = cycle;
            return replacement;
        }

        switch (pc) {
        // ---- 00:E93C  sixteen-bit compare -------------------------------
        // ($2F/$30) - ($2D/$2E). The low byte of the difference is left in
        // $42; on the non-negative arm the ORA folds it into the high byte so
        // Z answers "equal", and the SEC restores the borrow-free carry the
        // callers branch on.
        case 0xE93CU:  // SEC
            registers.status = static_cast<std::uint8_t>(
                registers.status | kCarry);
            append_checkpoint(0xE93CU, 0xE93DU, 6U);
            pc = 0xE93DU;
            break;

        case 0xE93DU:  // LDA $2F
            load_accumulator(read_ram(zp(0x2FU)));
            append_checkpoint(0xE93DU, 0xE93FU, 12U);
            pc = 0xE93FU;
            break;

        case 0xE93FU:  // SBC $2D
            subtract_with_carry(read_ram(zp(0x2DU)));
            append_checkpoint(0xE93FU, 0xE941U, 12U);
            pc = 0xE941U;
            break;

        case 0xE941U:  // STA $42
            store(0xE941U, zp(0x42U), registers.accumulator);
            append_checkpoint(0xE941U, 0xE943U, 12U);
            pc = 0xE943U;
            break;

        case 0xE943U:  // LDA $30
            load_accumulator(read_ram(zp(0x30U)));
            append_checkpoint(0xE943U, 0xE945U, 12U);
            pc = 0xE945U;
            break;

        case 0xE945U:  // SBC $2E
            subtract_with_carry(read_ram(zp(0x2EU)));
            append_checkpoint(0xE945U, 0xE947U, 12U);
            pc = 0xE947U;
            break;

        case 0xE947U:  // BMI $E94C
            pc = branch(
                0xE947U,
                0xE94CU,
                0xE949U,
                (registers.status & kNegative) != 0U);
            break;

        case 0xE949U:  // ORA $42
            load_accumulator(
                static_cast<std::uint8_t>(
                    registers.accumulator | read_ram(zp(0x42U))));
            append_checkpoint(0xE949U, 0xE94BU, 12U);
            pc = 0xE94BU;
            break;

        case 0xE94BU:  // SEC
            registers.status = static_cast<std::uint8_t>(
                registers.status | kCarry);
            append_checkpoint(0xE94BU, 0xE94CU, 6U);
            pc = 0xE94CU;
            break;

        // ---- 00:E9B6  block interpolator --------------------------------
        // Sixteen colours, two bytes each. $2F/$30 points at the target
        // palette in ROM and $2D/$2E at the block's shadow copy. Each colour
        // is nine bits, three per channel, packed low-byte first: bits 0-2 are
        // channel zero, bits 3-5 channel one, bits 6-8 channel two.
        //
        // The target's three channels are summed and compared against the
        // step counter at $2CEC. Colours whose total is below the counter are
        // skipped entirely, which is what staggers the fade; the rest have
        // exactly one channel nudged one step toward the target, chosen by the
        // largest per-channel difference.
        case 0xE9B6U:  // CLY -- no flags
            registers.y = 0x00U;
            append_checkpoint(0xE9B6U, 0xE9B7U, 6U);
            pc = 0xE9B7U;
            break;

        case 0xE9B7U:  // LDA ($2F),Y
            load_accumulator(read_mapped(indirect_y(0x2FU)));
            append_checkpoint(0xE9B7U, 0xE9B9U, 21U);
            pc = 0xE9B9U;
            break;

        case 0xE9B9U:  // AND #$07
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x07U));
            append_checkpoint(0xE9B9U, 0xE9BBU, 6U);
            pc = 0xE9BBU;
            break;

        case 0xE9BBU:  // STA $3C
            store(0xE9BBU, zp(0x3CU), registers.accumulator);
            append_checkpoint(0xE9BBU, 0xE9BDU, 12U);
            pc = 0xE9BDU;
            break;

        case 0xE9BDU:  // LDA ($2F),Y
            load_accumulator(read_mapped(indirect_y(0x2FU)));
            append_checkpoint(0xE9BDU, 0xE9BFU, 21U);
            pc = 0xE9BFU;
            break;

        case 0xE9BFU:  // LSR A
            shift_right_accumulator();
            append_checkpoint(0xE9BFU, 0xE9C0U, 6U);
            pc = 0xE9C0U;
            break;

        case 0xE9C0U:  // LSR A
            shift_right_accumulator();
            append_checkpoint(0xE9C0U, 0xE9C1U, 6U);
            pc = 0xE9C1U;
            break;

        case 0xE9C1U:  // LSR A
            shift_right_accumulator();
            append_checkpoint(0xE9C1U, 0xE9C2U, 6U);
            pc = 0xE9C2U;
            break;

        case 0xE9C2U:  // AND #$07
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x07U));
            append_checkpoint(0xE9C2U, 0xE9C4U, 6U);
            pc = 0xE9C4U;
            break;

        case 0xE9C4U:  // STA $3D
            store(0xE9C4U, zp(0x3DU), registers.accumulator);
            append_checkpoint(0xE9C4U, 0xE9C6U, 12U);
            pc = 0xE9C6U;
            break;

        case 0xE9C6U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE9C6U, 0xE9C7U, 6U);
            pc = 0xE9C7U;
            break;

        case 0xE9C7U:  // LDA ($2F),Y -- the colour's high byte
            load_accumulator(read_mapped(indirect_y(0x2FU)));
            append_checkpoint(0xE9C7U, 0xE9C9U, 21U);
            pc = 0xE9C9U;
            break;

        case 0xE9C9U:  // STA $3E
            store(0xE9C9U, zp(0x3EU), registers.accumulator);
            append_checkpoint(0xE9C9U, 0xE9CBU, 12U);
            pc = 0xE9CBU;
            break;

        case 0xE9CBU:  // DEY
            registers.y = static_cast<std::uint8_t>(registers.y - 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE9CBU, 0xE9CCU, 6U);
            pc = 0xE9CCU;
            break;

        case 0xE9CCU:  // LDA ($2F),Y
            load_accumulator(read_mapped(indirect_y(0x2FU)));
            append_checkpoint(0xE9CCU, 0xE9CEU, 21U);
            pc = 0xE9CEU;
            break;

        case 0xE9CEU: {  // LSR $3E -- carry becomes the colour's bit 8
            const auto operand = read_ram(zp(0x3EU));
            const auto shifted = static_cast<std::uint8_t>(operand >> 1U);
            store(0xE9CEU, zp(0x3EU), shifted);
            registers.status = set_nz(
                static_cast<std::uint8_t>(
                    (registers.status & static_cast<std::uint8_t>(~kCarry))
                    | static_cast<std::uint8_t>(operand & kCarry)),
                shifted);
            append_checkpoint(0xE9CEU, 0xE9D0U, 18U);
            pc = 0xE9D0U;
            break;
        }

        case 0xE9D0U:  // ROR A
            rotate_right_accumulator();
            append_checkpoint(0xE9D0U, 0xE9D1U, 6U);
            pc = 0xE9D1U;
            break;

        case 0xE9D1U:  // LSR A
            shift_right_accumulator();
            append_checkpoint(0xE9D1U, 0xE9D2U, 6U);
            pc = 0xE9D2U;
            break;

        case 0xE9D2U:  // LSR A
            shift_right_accumulator();
            append_checkpoint(0xE9D2U, 0xE9D3U, 6U);
            pc = 0xE9D3U;
            break;

        case 0xE9D3U:  // LSR A
            shift_right_accumulator();
            append_checkpoint(0xE9D3U, 0xE9D4U, 6U);
            pc = 0xE9D4U;
            break;

        case 0xE9D4U:  // LSR A
            shift_right_accumulator();
            append_checkpoint(0xE9D4U, 0xE9D5U, 6U);
            pc = 0xE9D5U;
            break;

        case 0xE9D5U:  // LSR A
            shift_right_accumulator();
            append_checkpoint(0xE9D5U, 0xE9D6U, 6U);
            pc = 0xE9D6U;
            break;

        case 0xE9D6U:  // AND #$07
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x07U));
            append_checkpoint(0xE9D6U, 0xE9D8U, 6U);
            pc = 0xE9D8U;
            break;

        case 0xE9D8U:  // STA $3E
            store(0xE9D8U, zp(0x3EU), registers.accumulator);
            append_checkpoint(0xE9D8U, 0xE9DAU, 12U);
            pc = 0xE9DAU;
            break;

        case 0xE9DAU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xE9DAU, 0xE9DBU, 6U);
            pc = 0xE9DBU;
            break;

        case 0xE9DBU:  // ADC $3C
            add_with_carry(read_ram(zp(0x3CU)));
            append_checkpoint(0xE9DBU, 0xE9DDU, 12U);
            pc = 0xE9DDU;
            break;

        case 0xE9DDU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xE9DDU, 0xE9DEU, 6U);
            pc = 0xE9DEU;
            break;

        case 0xE9DEU:  // ADC $3D
            add_with_carry(read_ram(zp(0x3DU)));
            append_checkpoint(0xE9DEU, 0xE9E0U, 12U);
            pc = 0xE9E0U;
            break;

        case 0xE9E0U:  // CMP $2CEC -- the fade's step counter
            compare(registers.accumulator, read_ram(0x2CECU));
            append_checkpoint(0xE9E0U, 0xE9E3U, 15U);
            pc = 0xE9E3U;
            break;

        case 0xE9E3U:  // BCS $E9E8
            pc = branch(
                0xE9E3U,
                0xE9E8U,
                0xE9E5U,
                (registers.status & kCarry) != 0U);
            break;

        case 0xE9E5U:  // JMP $EA6D -- this colour is not due yet
            append_checkpoint(0xE9E5U, 0xEA6DU, 12U);
            pc = 0xEA6DU;
            break;

        case 0xE9E8U:  // LDA ($2D),Y -- the same unpacking, on the shadow
            load_accumulator(read_mapped(indirect_y(0x2DU)));
            append_checkpoint(0xE9E8U, 0xE9EAU, 21U);
            pc = 0xE9EAU;
            break;

        case 0xE9EAU:  // AND #$07
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x07U));
            append_checkpoint(0xE9EAU, 0xE9ECU, 6U);
            pc = 0xE9ECU;
            break;

        case 0xE9ECU:  // STA $39
            store(0xE9ECU, zp(0x39U), registers.accumulator);
            append_checkpoint(0xE9ECU, 0xE9EEU, 12U);
            pc = 0xE9EEU;
            break;

        case 0xE9EEU:  // LDA ($2D),Y
            load_accumulator(read_mapped(indirect_y(0x2DU)));
            append_checkpoint(0xE9EEU, 0xE9F0U, 21U);
            pc = 0xE9F0U;
            break;

        case 0xE9F0U:  // LSR A
            shift_right_accumulator();
            append_checkpoint(0xE9F0U, 0xE9F1U, 6U);
            pc = 0xE9F1U;
            break;

        case 0xE9F1U:  // LSR A
            shift_right_accumulator();
            append_checkpoint(0xE9F1U, 0xE9F2U, 6U);
            pc = 0xE9F2U;
            break;

        case 0xE9F2U:  // LSR A
            shift_right_accumulator();
            append_checkpoint(0xE9F2U, 0xE9F3U, 6U);
            pc = 0xE9F3U;
            break;

        case 0xE9F3U:  // AND #$07
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x07U));
            append_checkpoint(0xE9F3U, 0xE9F5U, 6U);
            pc = 0xE9F5U;
            break;

        case 0xE9F5U:  // STA $3A
            store(0xE9F5U, zp(0x3AU), registers.accumulator);
            append_checkpoint(0xE9F5U, 0xE9F7U, 12U);
            pc = 0xE9F7U;
            break;

        case 0xE9F7U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE9F7U, 0xE9F8U, 6U);
            pc = 0xE9F8U;
            break;

        case 0xE9F8U:  // LDA ($2D),Y
            load_accumulator(read_mapped(indirect_y(0x2DU)));
            append_checkpoint(0xE9F8U, 0xE9FAU, 21U);
            pc = 0xE9FAU;
            break;

        case 0xE9FAU:  // STA $3B
            store(0xE9FAU, zp(0x3BU), registers.accumulator);
            append_checkpoint(0xE9FAU, 0xE9FCU, 12U);
            pc = 0xE9FCU;
            break;

        case 0xE9FCU:  // DEY
            registers.y = static_cast<std::uint8_t>(registers.y - 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE9FCU, 0xE9FDU, 6U);
            pc = 0xE9FDU;
            break;

        case 0xE9FDU:  // LDA ($2D),Y
            load_accumulator(read_mapped(indirect_y(0x2DU)));
            append_checkpoint(0xE9FDU, 0xE9FFU, 21U);
            pc = 0xE9FFU;
            break;

        case 0xE9FFU: {  // LSR $3B
            const auto operand = read_ram(zp(0x3BU));
            const auto shifted = static_cast<std::uint8_t>(operand >> 1U);
            store(0xE9FFU, zp(0x3BU), shifted);
            registers.status = set_nz(
                static_cast<std::uint8_t>(
                    (registers.status & static_cast<std::uint8_t>(~kCarry))
                    | static_cast<std::uint8_t>(operand & kCarry)),
                shifted);
            append_checkpoint(0xE9FFU, 0xEA01U, 18U);
            pc = 0xEA01U;
            break;
        }

        case 0xEA01U:  // ROR A
            rotate_right_accumulator();
            append_checkpoint(0xEA01U, 0xEA02U, 6U);
            pc = 0xEA02U;
            break;

        case 0xEA02U:  // LSR A
            shift_right_accumulator();
            append_checkpoint(0xEA02U, 0xEA03U, 6U);
            pc = 0xEA03U;
            break;

        case 0xEA03U:  // LSR A
            shift_right_accumulator();
            append_checkpoint(0xEA03U, 0xEA04U, 6U);
            pc = 0xEA04U;
            break;

        case 0xEA04U:  // LSR A
            shift_right_accumulator();
            append_checkpoint(0xEA04U, 0xEA05U, 6U);
            pc = 0xEA05U;
            break;

        case 0xEA05U:  // LSR A
            shift_right_accumulator();
            append_checkpoint(0xEA05U, 0xEA06U, 6U);
            pc = 0xEA06U;
            break;

        case 0xEA06U:  // LSR A
            shift_right_accumulator();
            append_checkpoint(0xEA06U, 0xEA07U, 6U);
            pc = 0xEA07U;
            break;

        case 0xEA07U:  // AND #$07
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x07U));
            append_checkpoint(0xEA07U, 0xEA09U, 6U);
            pc = 0xEA09U;
            break;

        case 0xEA09U:  // STA $3B
            store(0xEA09U, zp(0x3BU), registers.accumulator);
            append_checkpoint(0xEA09U, 0xEA0BU, 12U);
            pc = 0xEA0BU;
            break;

        // The three per-channel differences, target minus shadow, into
        // $3F/$40/$41. They are computed with SBC and no clamp, so a channel
        // the shadow has overshot leaves a large unsigned value here; the
        // comparisons below are unsigned and treat that as "furthest away",
        // which is what drives it back.
        case 0xEA0BU:  // SEC
            registers.status = static_cast<std::uint8_t>(
                registers.status | kCarry);
            append_checkpoint(0xEA0BU, 0xEA0CU, 6U);
            pc = 0xEA0CU;
            break;

        case 0xEA0CU:  // LDA $3D
            load_accumulator(read_ram(zp(0x3DU)));
            append_checkpoint(0xEA0CU, 0xEA0EU, 12U);
            pc = 0xEA0EU;
            break;

        case 0xEA0EU:  // SBC $3A
            subtract_with_carry(read_ram(zp(0x3AU)));
            append_checkpoint(0xEA0EU, 0xEA10U, 12U);
            pc = 0xEA10U;
            break;

        case 0xEA10U:  // STA $40
            store(0xEA10U, zp(0x40U), registers.accumulator);
            append_checkpoint(0xEA10U, 0xEA12U, 12U);
            pc = 0xEA12U;
            break;

        case 0xEA12U:  // SEC
            registers.status = static_cast<std::uint8_t>(
                registers.status | kCarry);
            append_checkpoint(0xEA12U, 0xEA13U, 6U);
            pc = 0xEA13U;
            break;

        case 0xEA13U:  // LDA $3E
            load_accumulator(read_ram(zp(0x3EU)));
            append_checkpoint(0xEA13U, 0xEA15U, 12U);
            pc = 0xEA15U;
            break;

        case 0xEA15U:  // SBC $3B
            subtract_with_carry(read_ram(zp(0x3BU)));
            append_checkpoint(0xEA15U, 0xEA17U, 12U);
            pc = 0xEA17U;
            break;

        case 0xEA17U:  // STA $41
            store(0xEA17U, zp(0x41U), registers.accumulator);
            append_checkpoint(0xEA17U, 0xEA19U, 12U);
            pc = 0xEA19U;
            break;

        case 0xEA19U:  // SEC
            registers.status = static_cast<std::uint8_t>(
                registers.status | kCarry);
            append_checkpoint(0xEA19U, 0xEA1AU, 6U);
            pc = 0xEA1AU;
            break;

        case 0xEA1AU:  // LDA $3C
            load_accumulator(read_ram(zp(0x3CU)));
            append_checkpoint(0xEA1AU, 0xEA1CU, 12U);
            pc = 0xEA1CU;
            break;

        case 0xEA1CU:  // SBC $39
            subtract_with_carry(read_ram(zp(0x39U)));
            append_checkpoint(0xEA1CU, 0xEA1EU, 12U);
            pc = 0xEA1EU;
            break;

        case 0xEA1EU:  // STA $3F
            store(0xEA1EU, zp(0x3FU), registers.accumulator);
            append_checkpoint(0xEA1EU, 0xEA20U, 12U);
            pc = 0xEA20U;
            break;

        case 0xEA20U:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xEA20U, 0xEA21U, 6U);
            pc = 0xEA21U;
            break;

        case 0xEA21U:  // ADC $40
            add_with_carry(read_ram(zp(0x40U)));
            append_checkpoint(0xEA21U, 0xEA23U, 12U);
            pc = 0xEA23U;
            break;

        // No CLC before this one: the carry out of $3F+$40 is deliberately
        // carried into the sum, so the zero test below is on the low byte of
        // all three differences added together.
        case 0xEA23U:  // ADC $41
            add_with_carry(read_ram(zp(0x41U)));
            append_checkpoint(0xEA23U, 0xEA25U, 12U);
            pc = 0xEA25U;
            break;

        case 0xEA25U:  // BNE $EA2A
            pc = branch(
                0xEA25U,
                0xEA2AU,
                0xEA27U,
                (registers.status & kZero) == 0U);
            break;

        case 0xEA27U:  // JMP $EA6D -- already at the target
            append_checkpoint(0xEA27U, 0xEA6DU, 12U);
            pc = 0xEA6DU;
            break;

        // Pick the channel to step. $40 against $41 first, then the winner
        // against $3F; whichever difference is largest gets its shadow channel
        // incremented by one.
        case 0xEA2AU:  // LDA $40
            load_accumulator(read_ram(zp(0x40U)));
            append_checkpoint(0xEA2AU, 0xEA2CU, 12U);
            pc = 0xEA2CU;
            break;

        case 0xEA2CU:  // CMP $41
            compare(registers.accumulator, read_ram(zp(0x41U)));
            append_checkpoint(0xEA2CU, 0xEA2EU, 12U);
            pc = 0xEA2EU;
            break;

        case 0xEA2EU:  // BCC $EA3A
            pc = branch(
                0xEA2EU,
                0xEA3AU,
                0xEA30U,
                (registers.status & kCarry) == 0U);
            break;

        case 0xEA30U:  // BEQ $EA44
            pc = branch(
                0xEA30U,
                0xEA44U,
                0xEA32U,
                (registers.status & kZero) != 0U);
            break;

        case 0xEA32U:  // CMP $3F
            compare(registers.accumulator, read_ram(zp(0x3FU)));
            append_checkpoint(0xEA32U, 0xEA34U, 12U);
            pc = 0xEA34U;
            break;

        case 0xEA34U:  // BCC $EA4C
            pc = branch(
                0xEA34U,
                0xEA4CU,
                0xEA36U,
                (registers.status & kCarry) == 0U);
            break;

        case 0xEA36U: {  // INC $3A
            const auto stepped = static_cast<std::uint8_t>(
                read_ram(zp(0x3AU)) + 1U);
            store(0xEA36U, zp(0x3AU), stepped);
            registers.status = set_nz(registers.status, stepped);
            append_checkpoint(0xEA36U, 0xEA38U, 18U);
            pc = 0xEA38U;
            break;
        }

        case 0xEA38U:  // BRA $EA4E
            append_checkpoint(0xEA38U, 0xEA4EU, 12U);
            pc = 0xEA4EU;
            break;

        case 0xEA3AU:  // LDA $41
            load_accumulator(read_ram(zp(0x41U)));
            append_checkpoint(0xEA3AU, 0xEA3CU, 12U);
            pc = 0xEA3CU;
            break;

        case 0xEA3CU:  // CMP $3F
            compare(registers.accumulator, read_ram(zp(0x3FU)));
            append_checkpoint(0xEA3CU, 0xEA3EU, 12U);
            pc = 0xEA3EU;
            break;

        case 0xEA3EU:  // BCC $EA4C
            pc = branch(
                0xEA3EU,
                0xEA4CU,
                0xEA40U,
                (registers.status & kCarry) == 0U);
            break;

        case 0xEA40U: {  // INC $3B
            const auto stepped = static_cast<std::uint8_t>(
                read_ram(zp(0x3BU)) + 1U);
            store(0xEA40U, zp(0x3BU), stepped);
            registers.status = set_nz(registers.status, stepped);
            append_checkpoint(0xEA40U, 0xEA42U, 18U);
            pc = 0xEA42U;
            break;
        }

        case 0xEA42U:  // BRA $EA4E
            append_checkpoint(0xEA42U, 0xEA4EU, 12U);
            pc = 0xEA4EU;
            break;

        case 0xEA44U:  // CMP $3F -- $40 and $41 tied
            compare(registers.accumulator, read_ram(zp(0x3FU)));
            append_checkpoint(0xEA44U, 0xEA46U, 12U);
            pc = 0xEA46U;
            break;

        case 0xEA46U:  // BCC $EA4C
            pc = branch(
                0xEA46U,
                0xEA4CU,
                0xEA48U,
                (registers.status & kCarry) == 0U);
            break;

        case 0xEA48U: {  // INC $3A
            const auto stepped = static_cast<std::uint8_t>(
                read_ram(zp(0x3AU)) + 1U);
            store(0xEA48U, zp(0x3AU), stepped);
            registers.status = set_nz(registers.status, stepped);
            append_checkpoint(0xEA48U, 0xEA4AU, 18U);
            pc = 0xEA4AU;
            break;
        }

        case 0xEA4AU:  // BRA $EA4E
            append_checkpoint(0xEA4AU, 0xEA4EU, 12U);
            pc = 0xEA4EU;
            break;

        case 0xEA4CU: {  // INC $39
            const auto stepped = static_cast<std::uint8_t>(
                read_ram(zp(0x39U)) + 1U);
            store(0xEA4CU, zp(0x39U), stepped);
            registers.status = set_nz(registers.status, stepped);
            append_checkpoint(0xEA4CU, 0xEA4EU, 18U);
            pc = 0xEA4EU;
            break;
        }

        // Repack the stepped shadow colour and store it back. Channel two is
        // shifted six places, so only the last of the six ASLs can carry, and
        // the single ROL $2B captures that bit as the colour's bit 8.
        case 0xEA4EU:  // LDA $3A
            load_accumulator(read_ram(zp(0x3AU)));
            append_checkpoint(0xEA4EU, 0xEA50U, 12U);
            pc = 0xEA50U;
            break;

        case 0xEA50U:  // ASL A
            shift_left_accumulator();
            append_checkpoint(0xEA50U, 0xEA51U, 6U);
            pc = 0xEA51U;
            break;

        case 0xEA51U:  // ASL A
            shift_left_accumulator();
            append_checkpoint(0xEA51U, 0xEA52U, 6U);
            pc = 0xEA52U;
            break;

        case 0xEA52U:  // ASL A
            shift_left_accumulator();
            append_checkpoint(0xEA52U, 0xEA53U, 6U);
            pc = 0xEA53U;
            break;

        case 0xEA53U:  // ORA $39
            load_accumulator(
                static_cast<std::uint8_t>(
                    registers.accumulator | read_ram(zp(0x39U))));
            append_checkpoint(0xEA53U, 0xEA55U, 12U);
            pc = 0xEA55U;
            break;

        case 0xEA55U:  // STA $39
            store(0xEA55U, zp(0x39U), registers.accumulator);
            append_checkpoint(0xEA55U, 0xEA57U, 12U);
            pc = 0xEA57U;
            break;

        case 0xEA57U:  // LDA $3B
            load_accumulator(read_ram(zp(0x3BU)));
            append_checkpoint(0xEA57U, 0xEA59U, 12U);
            pc = 0xEA59U;
            break;

        case 0xEA59U:  // STZ $2B
            store(0xEA59U, zp(0x2BU), 0x00U);
            append_checkpoint(0xEA59U, 0xEA5BU, 12U);
            pc = 0xEA5BU;
            break;

        case 0xEA5BU:  // ASL A
            shift_left_accumulator();
            append_checkpoint(0xEA5BU, 0xEA5CU, 6U);
            pc = 0xEA5CU;
            break;

        case 0xEA5CU:  // ASL A
            shift_left_accumulator();
            append_checkpoint(0xEA5CU, 0xEA5DU, 6U);
            pc = 0xEA5DU;
            break;

        case 0xEA5DU:  // ASL A
            shift_left_accumulator();
            append_checkpoint(0xEA5DU, 0xEA5EU, 6U);
            pc = 0xEA5EU;
            break;

        case 0xEA5EU:  // ASL A
            shift_left_accumulator();
            append_checkpoint(0xEA5EU, 0xEA5FU, 6U);
            pc = 0xEA5FU;
            break;

        case 0xEA5FU:  // ASL A
            shift_left_accumulator();
            append_checkpoint(0xEA5FU, 0xEA60U, 6U);
            pc = 0xEA60U;
            break;

        case 0xEA60U:  // ASL A
            shift_left_accumulator();
            append_checkpoint(0xEA60U, 0xEA61U, 6U);
            pc = 0xEA61U;
            break;

        case 0xEA61U: {  // ROL $2B
            const auto operand = read_ram(zp(0x2BU));
            const auto rotated = static_cast<std::uint8_t>(
                static_cast<std::uint8_t>(operand << 1U)
                | static_cast<std::uint8_t>(registers.status & kCarry));
            store(0xEA61U, zp(0x2BU), rotated);
            registers.status = set_nz(
                static_cast<std::uint8_t>(
                    (registers.status & static_cast<std::uint8_t>(~kCarry))
                    | static_cast<std::uint8_t>((operand >> 7U) & kCarry)),
                rotated);
            append_checkpoint(0xEA61U, 0xEA63U, 18U);
            pc = 0xEA63U;
            break;
        }

        case 0xEA63U:  // ORA $39
            load_accumulator(
                static_cast<std::uint8_t>(
                    registers.accumulator | read_ram(zp(0x39U))));
            append_checkpoint(0xEA63U, 0xEA65U, 12U);
            pc = 0xEA65U;
            break;

        case 0xEA65U:  // STA ($2D),Y
            store(0xEA65U, indirect_y(0x2DU), registers.accumulator);
            append_checkpoint(0xEA65U, 0xEA67U, 21U);
            pc = 0xEA67U;
            break;

        case 0xEA67U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xEA67U, 0xEA68U, 6U);
            pc = 0xEA68U;
            break;

        case 0xEA68U:  // LDA $2B
            load_accumulator(read_ram(zp(0x2BU)));
            append_checkpoint(0xEA68U, 0xEA6AU, 12U);
            pc = 0xEA6AU;
            break;

        case 0xEA6AU:  // STA ($2D),Y
            store(0xEA6AU, indirect_y(0x2DU), registers.accumulator);
            append_checkpoint(0xEA6AU, 0xEA6CU, 21U);
            pc = 0xEA6CU;
            break;

        case 0xEA6CU:  // DEY
            registers.y = static_cast<std::uint8_t>(registers.y - 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xEA6CU, 0xEA6DU, 6U);
            pc = 0xEA6DU;
            break;

        case 0xEA6DU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xEA6DU, 0xEA6EU, 6U);
            pc = 0xEA6EU;
            break;

        case 0xEA6EU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xEA6EU, 0xEA6FU, 6U);
            pc = 0xEA6FU;
            break;

        case 0xEA6FU:  // CPY #$20 -- sixteen colours, two bytes each
            compare(registers.y, 0x20U);
            append_checkpoint(0xEA6FU, 0xEA71U, 6U);
            pc = 0xEA71U;
            break;

        case 0xEA71U:  // BEQ $EA76
            pc = branch(
                0xEA71U,
                0xEA76U,
                0xEA73U,
                (registers.status & kZero) != 0U);
            break;

        case 0xEA73U:  // JMP $E9B7
            append_checkpoint(0xEA73U, 0xE9B7U, 12U);
            pc = 0xE9B7U;
            break;

        // ---- 00:EB30  VCE upload ----------------------------------------
        // Once a frame. $2BB9 asks for a whole-palette upload, $2CCB says at
        // least one block is dirty, and $2CCC,X says which. The walk keeps the
        // shadow-buffer read pointer in $47/$48 and the VCE colour-table
        // address in $49/$4A, advancing them by $20 and $10 per block.
        case 0xEB30U:  // LDA $2BB9
            load_accumulator(read_ram(0x2BB9U));
            append_checkpoint(0xEB30U, 0xEB33U, 15U);
            pc = 0xEB33U;
            break;

        case 0xEB33U:  // BMI $EB3A
            pc = branch(
                0xEB33U,
                0xEB3AU,
                0xEB35U,
                (registers.status & kNegative) != 0U);
            break;

        case 0xEB35U:  // LDA $2CCB
            load_accumulator(read_ram(0x2CCBU));
            append_checkpoint(0xEB35U, 0xEB38U, 15U);
            pc = 0xEB38U;
            break;

        case 0xEB38U:  // BNE $EB3B
            pc = branch(
                0xEB38U,
                0xEB3BU,
                0xEB3AU,
                (registers.status & kZero) == 0U);
            break;

        case 0xEB3BU:  // LDA $2BB9
            load_accumulator(read_ram(0x2BB9U));
            append_checkpoint(0xEB3BU, 0xEB3EU, 15U);
            pc = 0xEB3EU;
            break;

        case 0xEB3EU:  // BEQ $EB54
            pc = branch(
                0xEB3EU,
                0xEB54U,
                0xEB40U,
                (registers.status & kZero) != 0U);
            break;

        // The whole-palette arm. TIA $27B9, $0404, $0400 moves 1,024 bytes
        // alternating between the two data ports, which is 1,024 writes
        // against a channel that carries 512. There is no transcript for it,
        // so the walk hands the call back before its first store lands.
        case 0xEB40U:  // STZ $0402
            return std::nullopt;

        case 0xEB54U:  // CLX -- no flags
            registers.x = 0x00U;
            append_checkpoint(0xEB54U, 0xEB55U, 6U);
            pc = 0xEB55U;
            break;

        case 0xEB55U:  // LDA #$B9
            load_accumulator(0xB9U);
            append_checkpoint(0xEB55U, 0xEB57U, 6U);
            pc = 0xEB57U;
            break;

        case 0xEB57U:  // STA $47
            store(0xEB57U, zp(0x47U), registers.accumulator);
            append_checkpoint(0xEB57U, 0xEB59U, 12U);
            pc = 0xEB59U;
            break;

        case 0xEB59U:  // LDA #$27
            load_accumulator(0x27U);
            append_checkpoint(0xEB59U, 0xEB5BU, 6U);
            pc = 0xEB5BU;
            break;

        case 0xEB5BU:  // STA $48
            store(0xEB5BU, zp(0x48U), registers.accumulator);
            append_checkpoint(0xEB5BU, 0xEB5DU, 12U);
            pc = 0xEB5DU;
            break;

        case 0xEB5DU:  // STZ $49
            store(0xEB5DU, zp(0x49U), 0x00U);
            append_checkpoint(0xEB5DU, 0xEB5FU, 12U);
            pc = 0xEB5FU;
            break;

        case 0xEB5FU:  // STZ $4A
            store(0xEB5FU, zp(0x4AU), 0x00U);
            append_checkpoint(0xEB5FU, 0xEB61U, 12U);
            pc = 0xEB61U;
            break;

        case 0xEB61U:  // LDA $2CCC,X -- is this block dirty?
            load_accumulator(read_ram(indexed_x(0x2CCCU)));
            append_checkpoint(0xEB61U, 0xEB64U, 15U);
            pc = 0xEB64U;
            break;

        case 0xEB64U:  // BEQ $EB84
            pc = branch(
                0xEB64U,
                0xEB84U,
                0xEB66U,
                (registers.status & kZero) != 0U);
            break;

        case 0xEB66U:  // LDA $49
            load_accumulator(read_ram(zp(0x49U)));
            append_checkpoint(0xEB66U, 0xEB68U, 12U);
            pc = 0xEB68U;
            break;

        case 0xEB68U:  // STA $0402 -- colour-table address low
            store_port(0xEB68U, 0x0402U, registers.accumulator);
            append_checkpoint(0xEB68U, 0xEB6BU, 18U, 6U);
            pc = 0xEB6BU;
            break;

        case 0xEB6BU:  // LDA $4A
            load_accumulator(read_ram(zp(0x4AU)));
            append_checkpoint(0xEB6BU, 0xEB6DU, 12U);
            pc = 0xEB6DU;
            break;

        case 0xEB6DU:  // STA $0403 -- colour-table address high
            store_port(0xEB6DU, 0x0403U, registers.accumulator);
            append_checkpoint(0xEB6DU, 0xEB70U, 18U, 6U);
            pc = 0xEB70U;
            break;

        case 0xEB70U:  // CLY -- no flags
            registers.y = 0x00U;
            append_checkpoint(0xEB70U, 0xEB71U, 6U);
            pc = 0xEB71U;
            break;

        case 0xEB71U:  // LDA ($47),Y
            load_accumulator(read_mapped(indirect_y(0x47U)));
            append_checkpoint(0xEB71U, 0xEB73U, 21U);
            pc = 0xEB73U;
            break;

        case 0xEB73U:  // STA $0404 -- colour low
            store_port(0xEB73U, 0x0404U, registers.accumulator);
            append_checkpoint(0xEB73U, 0xEB76U, 18U, 6U);
            pc = 0xEB76U;
            break;

        case 0xEB76U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xEB76U, 0xEB77U, 6U);
            pc = 0xEB77U;
            break;

        case 0xEB77U:  // LDA ($47),Y
            load_accumulator(read_mapped(indirect_y(0x47U)));
            append_checkpoint(0xEB77U, 0xEB79U, 21U);
            pc = 0xEB79U;
            break;

        case 0xEB79U:  // STA $0405 -- colour high; the VCE steps its address
            store_port(0xEB79U, 0x0405U, registers.accumulator);
            append_checkpoint(0xEB79U, 0xEB7CU, 18U, 6U);
            pc = 0xEB7CU;
            break;

        case 0xEB7CU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xEB7CU, 0xEB7DU, 6U);
            pc = 0xEB7DU;
            break;

        case 0xEB7DU:  // CPY #$20
            compare(registers.y, 0x20U);
            append_checkpoint(0xEB7DU, 0xEB7FU, 6U);
            pc = 0xEB7FU;
            break;

        case 0xEB7FU:  // BNE $EB71
            pc = branch(
                0xEB7FU,
                0xEB71U,
                0xEB81U,
                (registers.status & kZero) == 0U);
            break;

        case 0xEB81U:  // STZ $2CCC,X -- the block is clean again
            store(0xEB81U, indexed_x(0x2CCCU), 0x00U);
            append_checkpoint(0xEB81U, 0xEB84U, 15U);
            pc = 0xEB84U;
            break;

        case 0xEB84U:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xEB84U, 0xEB85U, 6U);
            pc = 0xEB85U;
            break;

        case 0xEB85U:  // LDA $47
            load_accumulator(read_ram(zp(0x47U)));
            append_checkpoint(0xEB85U, 0xEB87U, 12U);
            pc = 0xEB87U;
            break;

        case 0xEB87U:  // ADC #$20 -- next block, thirty-two bytes on
            add_with_carry(0x20U);
            append_checkpoint(0xEB87U, 0xEB89U, 6U);
            pc = 0xEB89U;
            break;

        case 0xEB89U:  // STA $47
            store(0xEB89U, zp(0x47U), registers.accumulator);
            append_checkpoint(0xEB89U, 0xEB8BU, 12U);
            pc = 0xEB8BU;
            break;

        case 0xEB8BU:  // CLA -- no flags; the carry survives into the ADC
            registers.accumulator = 0x00U;
            append_checkpoint(0xEB8BU, 0xEB8CU, 6U);
            pc = 0xEB8CU;
            break;

        case 0xEB8CU:  // ADC $48
            add_with_carry(read_ram(zp(0x48U)));
            append_checkpoint(0xEB8CU, 0xEB8EU, 12U);
            pc = 0xEB8EU;
            break;

        case 0xEB8EU:  // STA $48
            store(0xEB8EU, zp(0x48U), registers.accumulator);
            append_checkpoint(0xEB8EU, 0xEB90U, 12U);
            pc = 0xEB90U;
            break;

        case 0xEB90U:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xEB90U, 0xEB91U, 6U);
            pc = 0xEB91U;
            break;

        case 0xEB91U:  // LDA $49
            load_accumulator(read_ram(zp(0x49U)));
            append_checkpoint(0xEB91U, 0xEB93U, 12U);
            pc = 0xEB93U;
            break;

        case 0xEB93U:  // ADC #$10 -- sixteen colours on
            add_with_carry(0x10U);
            append_checkpoint(0xEB93U, 0xEB95U, 6U);
            pc = 0xEB95U;
            break;

        case 0xEB95U:  // STA $49
            store(0xEB95U, zp(0x49U), registers.accumulator);
            append_checkpoint(0xEB95U, 0xEB97U, 12U);
            pc = 0xEB97U;
            break;

        case 0xEB97U:  // CLA -- no flags
            registers.accumulator = 0x00U;
            append_checkpoint(0xEB97U, 0xEB98U, 6U);
            pc = 0xEB98U;
            break;

        case 0xEB98U:  // ADC $4A
            add_with_carry(read_ram(zp(0x4AU)));
            append_checkpoint(0xEB98U, 0xEB9AU, 12U);
            pc = 0xEB9AU;
            break;

        case 0xEB9AU:  // STA $4A
            store(0xEB9AU, zp(0x4AU), registers.accumulator);
            append_checkpoint(0xEB9AU, 0xEB9CU, 12U);
            pc = 0xEB9CU;
            break;

        case 0xEB9CU:  // INX
            registers.x = static_cast<std::uint8_t>(registers.x + 1U);
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0xEB9CU, 0xEB9DU, 6U);
            pc = 0xEB9DU;
            break;

        case 0xEB9DU:  // CPX #$20
            compare(registers.x, 0x20U);
            append_checkpoint(0xEB9DU, 0xEB9FU, 6U);
            pc = 0xEB9FU;
            break;

        case 0xEB9FU:  // BNE $EB61
            pc = branch(
                0xEB9FU,
                0xEB61U,
                0xEBA1U,
                (registers.status & kZero) == 0U);
            break;

        case 0xEBA1U:  // STZ $2CCB -- nothing is dirty any more
            store(0xEBA1U, 0x2CCBU, 0x00U);
            append_checkpoint(0xEBA1U, 0xEBA4U, 15U);
            pc = 0xEBA4U;
            break;

        // ---- 00:EBA5  block stager --------------------------------------
        // A holds the block index. The index is multiplied by thirty-two into
        // $2D/$2E to address the block inside the shadow buffer at $27B9, the
        // block's dirty flag at $2CCC,Y is set, and thirty-two bytes are
        // copied in from the source palette $2F/$30 points at. $2CCB is the
        // "some block is dirty" flag 00:EB30 tests once a frame.
        case 0xEBA5U:  // TAY
            registers.y = registers.accumulator;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xEBA5U, 0xEBA6U, 6U);
            pc = 0xEBA6U;
            break;

        case 0xEBA6U:  // STZ $2E
            store(0xEBA6U, zp(0x2EU), 0x00U);
            append_checkpoint(0xEBA6U, 0xEBA8U, 12U);
            pc = 0xEBA8U;
            break;

        case 0xEBA8U:  // ASL A
            shift_left_accumulator();
            append_checkpoint(0xEBA8U, 0xEBA9U, 6U);
            pc = 0xEBA9U;
            break;

        case 0xEBA9U: {  // ROL $2E
            const auto operand = read_ram(zp(0x2EU));
            const auto rotated = static_cast<std::uint8_t>(
                static_cast<std::uint8_t>(operand << 1U)
                | static_cast<std::uint8_t>(registers.status & kCarry));
            store(0xEBA9U, zp(0x2EU), rotated);
            registers.status = set_nz(
                static_cast<std::uint8_t>(
                    (registers.status & static_cast<std::uint8_t>(~kCarry))
                    | static_cast<std::uint8_t>((operand >> 7U) & kCarry)),
                rotated);
            append_checkpoint(0xEBA9U, 0xEBABU, 18U);
            pc = 0xEBABU;
            break;
        }

        case 0xEBABU:  // ASL A
            shift_left_accumulator();
            append_checkpoint(0xEBABU, 0xEBACU, 6U);
            pc = 0xEBACU;
            break;

        case 0xEBACU: {  // ROL $2E
            const auto operand = read_ram(zp(0x2EU));
            const auto rotated = static_cast<std::uint8_t>(
                static_cast<std::uint8_t>(operand << 1U)
                | static_cast<std::uint8_t>(registers.status & kCarry));
            store(0xEBACU, zp(0x2EU), rotated);
            registers.status = set_nz(
                static_cast<std::uint8_t>(
                    (registers.status & static_cast<std::uint8_t>(~kCarry))
                    | static_cast<std::uint8_t>((operand >> 7U) & kCarry)),
                rotated);
            append_checkpoint(0xEBACU, 0xEBAEU, 18U);
            pc = 0xEBAEU;
            break;
        }

        case 0xEBAEU:  // ASL A
            shift_left_accumulator();
            append_checkpoint(0xEBAEU, 0xEBAFU, 6U);
            pc = 0xEBAFU;
            break;

        case 0xEBAFU: {  // ROL $2E
            const auto operand = read_ram(zp(0x2EU));
            const auto rotated = static_cast<std::uint8_t>(
                static_cast<std::uint8_t>(operand << 1U)
                | static_cast<std::uint8_t>(registers.status & kCarry));
            store(0xEBAFU, zp(0x2EU), rotated);
            registers.status = set_nz(
                static_cast<std::uint8_t>(
                    (registers.status & static_cast<std::uint8_t>(~kCarry))
                    | static_cast<std::uint8_t>((operand >> 7U) & kCarry)),
                rotated);
            append_checkpoint(0xEBAFU, 0xEBB1U, 18U);
            pc = 0xEBB1U;
            break;
        }

        case 0xEBB1U:  // ASL A
            shift_left_accumulator();
            append_checkpoint(0xEBB1U, 0xEBB2U, 6U);
            pc = 0xEBB2U;
            break;

        case 0xEBB2U: {  // ROL $2E
            const auto operand = read_ram(zp(0x2EU));
            const auto rotated = static_cast<std::uint8_t>(
                static_cast<std::uint8_t>(operand << 1U)
                | static_cast<std::uint8_t>(registers.status & kCarry));
            store(0xEBB2U, zp(0x2EU), rotated);
            registers.status = set_nz(
                static_cast<std::uint8_t>(
                    (registers.status & static_cast<std::uint8_t>(~kCarry))
                    | static_cast<std::uint8_t>((operand >> 7U) & kCarry)),
                rotated);
            append_checkpoint(0xEBB2U, 0xEBB4U, 18U);
            pc = 0xEBB4U;
            break;
        }

        case 0xEBB4U:  // ASL A
            shift_left_accumulator();
            append_checkpoint(0xEBB4U, 0xEBB5U, 6U);
            pc = 0xEBB5U;
            break;

        case 0xEBB5U: {  // ROL $2E
            const auto operand = read_ram(zp(0x2EU));
            const auto rotated = static_cast<std::uint8_t>(
                static_cast<std::uint8_t>(operand << 1U)
                | static_cast<std::uint8_t>(registers.status & kCarry));
            store(0xEBB5U, zp(0x2EU), rotated);
            registers.status = set_nz(
                static_cast<std::uint8_t>(
                    (registers.status & static_cast<std::uint8_t>(~kCarry))
                    | static_cast<std::uint8_t>((operand >> 7U) & kCarry)),
                rotated);
            append_checkpoint(0xEBB5U, 0xEBB7U, 18U);
            pc = 0xEBB7U;
            break;
        }

        case 0xEBB7U:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xEBB7U, 0xEBB8U, 6U);
            pc = 0xEBB8U;
            break;

        case 0xEBB8U:  // ADC #$B9
            add_with_carry(0xB9U);
            append_checkpoint(0xEBB8U, 0xEBBAU, 6U);
            pc = 0xEBBAU;
            break;

        case 0xEBBAU:  // STA $2D
            store(0xEBBAU, zp(0x2DU), registers.accumulator);
            append_checkpoint(0xEBBAU, 0xEBBCU, 12U);
            pc = 0xEBBCU;
            break;

        case 0xEBBCU:  // LDA $2E
            load_accumulator(read_ram(zp(0x2EU)));
            append_checkpoint(0xEBBCU, 0xEBBEU, 12U);
            pc = 0xEBBEU;
            break;

        case 0xEBBEU:  // ADC #$27
            add_with_carry(0x27U);
            append_checkpoint(0xEBBEU, 0xEBC0U, 6U);
            pc = 0xEBC0U;
            break;

        case 0xEBC0U:  // STA $2E
            store(0xEBC0U, zp(0x2EU), registers.accumulator);
            append_checkpoint(0xEBC0U, 0xEBC2U, 12U);
            pc = 0xEBC2U;
            break;

        case 0xEBC2U:  // LDA #$01
            load_accumulator(0x01U);
            append_checkpoint(0xEBC2U, 0xEBC4U, 6U);
            pc = 0xEBC4U;
            break;

        case 0xEBC4U:  // STA $2CCC,Y -- mark this block dirty
            store(0xEBC4U, indexed_y(0x2CCCU), registers.accumulator);
            append_checkpoint(0xEBC4U, 0xEBC7U, 15U);
            pc = 0xEBC7U;
            break;

        case 0xEBC7U:  // CLY -- no flags
            registers.y = 0x00U;
            append_checkpoint(0xEBC7U, 0xEBC8U, 6U);
            pc = 0xEBC8U;
            break;

        case 0xEBC8U:  // LDA ($2F),Y
            load_accumulator(read_mapped(indirect_y(0x2FU)));
            append_checkpoint(0xEBC8U, 0xEBCAU, 21U);
            pc = 0xEBCAU;
            break;

        case 0xEBCAU:  // STA ($2D),Y
            store(0xEBCAU, indirect_y(0x2DU), registers.accumulator);
            append_checkpoint(0xEBCAU, 0xEBCCU, 21U);
            pc = 0xEBCCU;
            break;

        case 0xEBCCU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xEBCCU, 0xEBCDU, 6U);
            pc = 0xEBCDU;
            break;

        case 0xEBCDU:  // CPY #$20
            compare(registers.y, 0x20U);
            append_checkpoint(0xEBCDU, 0xEBCFU, 6U);
            pc = 0xEBCFU;
            break;

        case 0xEBCFU:  // BNE $EBC8
            pc = branch(
                0xEBCFU,
                0xEBC8U,
                0xEBD1U,
                (registers.status & kZero) == 0U);
            break;

        case 0xEBD1U:  // LDA #$FF
            load_accumulator(0xFFU);
            append_checkpoint(0xEBD1U, 0xEBD3U, 6U);
            pc = 0xEBD3U;
            break;

        case 0xEBD3U:  // STA $2CCB -- "at least one block is dirty"
            store(0xEBD3U, 0x2CCBU, registers.accumulator);
            append_checkpoint(0xEBD3U, 0xEBD6U, 15U);
            pc = 0xEBD6U;
            break;

        default:
            return std::nullopt;
        }
    }
}

} // namespace devils_crush::app::detail
