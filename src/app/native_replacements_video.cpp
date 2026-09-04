// SPDX-License-Identifier: GPL-3.0-or-later

#include "native_replacements_internal.hpp"
#include "devils_crush/game_logic/sprite.h"
#include "devils_crush/game_logic/video.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace devils_crush::app::detail {

using emulation::RoutineEntry;
using emulation::RoutineKey;
using emulation::RoutineReplacement;

namespace {

constexpr std::uint8_t kCarryFlag = 0x01U;
constexpr std::uint8_t kZeroFlag = 0x02U;
constexpr std::uint8_t kOverflowFlag = 0x40U;
constexpr std::uint8_t kNegativeFlag = 0x80U;

// Decimal mode changes every ADC and SBC in the routine; the T flag turns
// them into memory operands against $2000+X.
constexpr std::uint8_t kUnsupportedFlags = 0x28U;

[[nodiscard]] std::uint8_t set_nz(
    std::uint8_t status,
    const std::uint8_t value) noexcept
{
    status = static_cast<std::uint8_t>(
        status & static_cast<std::uint8_t>(~(kNegativeFlag | kZeroFlag)));
    if (value == 0U) {
        return static_cast<std::uint8_t>(status | kZeroFlag);
    }
    return static_cast<std::uint8_t>(
        status | (value & kNegativeFlag));
}

[[nodiscard]] std::uint8_t add_with_carry(
    std::uint8_t& status,
    const std::uint8_t accumulator,
    const std::uint8_t operand) noexcept
{
    const auto sum = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(accumulator)
        + static_cast<std::uint16_t>(operand)
        + static_cast<std::uint16_t>(status & kCarryFlag));
    const auto result = static_cast<std::uint8_t>(sum);
    status = static_cast<std::uint8_t>(
        status
        & static_cast<std::uint8_t>(~(kCarryFlag | kOverflowFlag)));
    if (sum > 0xFFU) {
        status = static_cast<std::uint8_t>(status | kCarryFlag);
    }
    if (((accumulator ^ result) & (operand ^ result) & 0x80U) != 0U) {
        status = static_cast<std::uint8_t>(status | kOverflowFlag);
    }
    status = set_nz(status, result);
    return result;
}

// ASL A. Carry takes bit seven before the shift; N and Z follow the result.
[[nodiscard]] std::uint8_t shift_left(
    std::uint8_t& status,
    const std::uint8_t value) noexcept
{
    status = static_cast<std::uint8_t>(
        status & static_cast<std::uint8_t>(~kCarryFlag));
    status = static_cast<std::uint8_t>(status | (value >> 7U));
    const auto result = static_cast<std::uint8_t>(value << 1U);
    status = set_nz(status, result);
    return result;
}

[[nodiscard]] std::uint8_t subtract_with_borrow(
    std::uint8_t& status,
    const std::uint8_t accumulator,
    const std::uint8_t operand) noexcept
{
    const auto difference = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(accumulator)
        - static_cast<std::uint16_t>(operand)
        - static_cast<std::uint16_t>((status & kCarryFlag) ^ kCarryFlag));
    const auto result = static_cast<std::uint8_t>(difference);
    status = static_cast<std::uint8_t>(
        status
        & static_cast<std::uint8_t>(~(kCarryFlag | kOverflowFlag)));
    if (difference <= 0xFFU) {
        status = static_cast<std::uint8_t>(status | kCarryFlag);
    }
    if (((accumulator ^ result) & (accumulator ^ operand) & 0x80U) != 0U) {
        status = static_cast<std::uint8_t>(status | kOverflowFlag);
    }
    status = set_nz(status, result);
    return result;
}

[[nodiscard]] std::uint8_t compare(
    std::uint8_t status,
    const std::uint8_t accumulator,
    const std::uint8_t operand) noexcept
{
    status = static_cast<std::uint8_t>(
        status & static_cast<std::uint8_t>(~kCarryFlag));
    if (accumulator >= operand) {
        status = static_cast<std::uint8_t>(status | kCarryFlag);
    }
    return set_nz(
        status,
        static_cast<std::uint8_t>(accumulator - operand));
}

// Beetle's BIT clears N and V and then takes them from the operand, in
// immediate mode as well as the addressing modes where the 65C02 does. Both
// of this routine's tests use an immediate whose top two bits are clear, so
// each one leaves N and V clear. That is why the routine returns with V
// clear even when the pattern high-byte add overflowed.
[[nodiscard]] std::uint8_t bit_immediate(
    std::uint8_t status,
    const std::uint8_t accumulator,
    const std::uint8_t operand) noexcept
{
    status = static_cast<std::uint8_t>(
        status
        & static_cast<std::uint8_t>(
            ~(kZeroFlag | kOverflowFlag | kNegativeFlag)));
    if ((accumulator & operand) == 0U) {
        status = static_cast<std::uint8_t>(status | kZeroFlag);
    }
    return static_cast<std::uint8_t>(
        status | (operand & (kOverflowFlag | kNegativeFlag)));
}

} // namespace

[[nodiscard]] std::optional<RoutineReplacement>
replace_sprite_emit_run(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    // The satellite-attribute-table emitter. One call walks an actor's sprite
    // descriptor for the current animation frame and pushes four 16-bit words
    // per entry — vertical, horizontal, pattern, attribute — through the VDC
    // data ports, advancing the table cursor in $4E as it goes.
    //
    // This is a full instruction-boundary transcript rather than an atomic
    // body, and it has to be. The mean body is 1,328 master cycles against a
    // ~5,068-cycle timer period, but the routine runs inside the video window
    // where the user-event horizon is far shorter; priced atomically the
    // valve admitted 535 of 73,818 calls, turning 7.8% of all master cycles
    // into 0.06%. A checkpointed transcript crosses those horizons, because
    // the core samples and delivers interrupts at the original instruction
    // boundaries the transcript declares.
    //
    // The transcript is also what makes the VDC stores cost what they really
    // cost. Applying a port write through the HuC6280 bus path charges the
    // VDC wait cycle, and only the checkpointed path in the core advances to
    // the pre-write boundary and lets that charge land inside the declared
    // offset. An atomic body paid it twice: eight port writes per entry,
    // three master cycles each, so every call ran 24 cycles per entry longer
    // than it declared and shifted every later interrupt.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x08U,
        .logical_pc = 0x40DFU,
    };
    constexpr std::uint16_t kReturnPc = 0x41ABU;
    constexpr std::uint16_t kVdcDataLow = 0x0002U;
    constexpr std::uint16_t kVdcDataHigh = 0x0003U;

    // Master cycles per instruction, at the high CPU speed this routine runs
    // at: three master cycles to the CPU cycle. A store to a VDC data port
    // costs the ordinary absolute five plus the VDC's stolen cycle.
    constexpr std::uint32_t kImplied = 6U;
    constexpr std::uint32_t kZeroPage = 12U;
    constexpr std::uint32_t kAbsolute = 15U;
    constexpr std::uint32_t kIndirectIndexed = 21U;
    constexpr std::uint32_t kPortStore = 18U;
    constexpr std::uint32_t kReadModifyWrite = 18U;
    constexpr std::uint32_t kJump = 12U;
    constexpr std::uint32_t kBranchTaken = 12U;
    constexpr std::uint32_t kBranchNotTaken = 6U;

    // Twenty-one prologue instructions and at most sixty-one per entry, so
    // this many entries fit the core's 2,048-checkpoint transaction. The
    // 512-write cap is looser: five prologue writes plus fourteen per entry.
    // The largest call on any observed route emits five entries.
    constexpr std::uint8_t kMaximumEntries = 33U;

    if (entry.routine != kRoutine || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    if ((entry.registers.status & kUnsupportedFlags) != 0U) {
        return std::nullopt;
    }
    // The VDC data ports are only addressable while the hardware page is
    // mapped at segment 0, which is also what the core's write validator
    // requires before it will accept them.
    if (entry.registers.mpr[0] != 0xFFU) {
        return std::nullopt;
    }

    // The routine's own bank supplies every descriptor and pointer it follows.
    const auto sprite_bank = entry.registers.mpr[2];
    std::size_t mapped_bank = sprite_bank;
    if (rom.size() == 0x60000U && sprite_bank < 0x80U) {
        mapped_bank = sprite_bank < 0x40U
            ? static_cast<std::size_t>(sprite_bank & 0x1FU)
            : static_cast<std::size_t>((sprite_bank & 0x0FU) + 0x20U);
    }
    const auto rom_offset = mapped_bank * 0x2000U;
    if (rom_offset + DC_SPRITE_EMIT_ROM_SIZE > rom.size()) {
        return std::nullopt;
    }
    const auto sprite_rom = rom.subspan(rom_offset, DC_SPRITE_EMIT_ROM_SIZE);

    dc_sprite_emit_run run{};
    if (!dc_classify_sprite_emit_run(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            reinterpret_cast<const std::uint8_t*>(sprite_rom.data()),
            sprite_rom.size(),
            entry.registers.x,
            &run)) {
        return std::nullopt;
    }
    if (run.count > kMaximumEntries) {
        return std::nullopt;
    }

    // The same indexed actor state the routine reads, at the same addresses.
    const auto read = [&work_ram](const std::size_t offset) {
        return std::to_integer<std::uint8_t>(work_ram[offset]);
    };
    const auto slot = static_cast<std::size_t>(entry.registers.x);
    const auto horizontal_low = read(0x1180U + slot);
    const auto horizontal_high = read(0x1200U + slot);
    const auto vertical_low = read(0x1300U + slot);
    const auto vertical_high = read(0x1380U + slot);
    const auto attributes = read(0x1800U + slot);
    const auto merge_low = read(0x1D00U + slot);
    const auto camera_low = read(0x00A9U);
    const auto camera_high = read(0x00AAU);
    const auto pattern_bias_low = read(0x0051U);
    const auto pattern_bias_high = read(0x0052U);
    const auto cursor_before = read(0x004EU);

    RoutineReplacement replacement{
        .return_instruction =
            {
                .physical_bank = kRoutine.physical_bank,
                .logical_pc = kReturnPc,
            },
        .output_registers = entry.registers,
    };
    replacement.work_ram_writes.reserve(
        5U + static_cast<std::size_t>(run.count) * 14U);
    replacement.checkpoints.reserve(
        21U + static_cast<std::size_t>(run.count) * 61U);

    auto registers = entry.registers;
    std::uint32_t cycle = 0U;

    // One checkpoint per original instruction: the state it leaves behind, at
    // the master cycle it ends on, with the interrupt sampled a CPU cycle
    // earlier the way the hardware samples it.
    // Every instruction here samples the interrupt one CPU cycle before its
    // own last, which is where the core's LastCycle sits. A VDC port store is
    // the exception: its last cycle is the wait the VDC steals after the bus
    // write, so it samples two CPU cycles early.
    const auto boundary =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t sample_back) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = cycle - sample_back,
                .master_cycle_offset = cycle,
                .instruction = {kRoutine.physical_bank, instruction_pc},
                .registers = registers,
            });
        };
    const auto step =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t cycles) {
            cycle += cycles;
            boundary(instruction_pc, resume_pc, 3U);
        };
    const auto store =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t cycles,
            const std::uint16_t address,
            const std::uint8_t value) {
            cycle += cycles;
            replacement.work_ram_writes.push_back({
                .instruction = {kRoutine.physical_bank, instruction_pc},
                .logical_address = address,
                .value = value,
                .master_cycle_offset = cycle,
            });
            boundary(
                instruction_pc,
                resume_pc,
                address <= kVdcDataHigh ? 6U : 3U);
        };
    const auto load = [&](const std::uint8_t value) {
        registers.accumulator = value;
        registers.status = set_nz(registers.status, value);
    };
    const auto increment_y = [&]() {
        registers.y = static_cast<std::uint8_t>(registers.y + 1U);
        registers.status = set_nz(registers.status, registers.y);
    };

    const auto descriptor_low =
        static_cast<std::uint8_t>(run.descriptor_pointer);
    const auto descriptor_high =
        static_cast<std::uint8_t>(run.descriptor_pointer >> 8U);
    const auto entry_low = static_cast<std::uint8_t>(run.entry_pointer);
    const auto entry_high =
        static_cast<std::uint8_t>(run.entry_pointer >> 8U);

    // Prologue: resolve the descriptor for this actor's animation frame, take
    // the entry count, and reject a cursor overflow.
    registers.y = run.descriptor_id;
    registers.status = set_nz(registers.status, registers.y);
    step(0x40DFU, 0x40E2U, kAbsolute);
    load(descriptor_low);
    step(0x40E2U, 0x40E5U, kAbsolute);
    store(0x40E5U, 0x40E7U, kZeroPage, 0x202DU, descriptor_low);
    load(descriptor_high);
    step(0x40E7U, 0x40EAU, kAbsolute);
    store(0x40EAU, 0x40ECU, kZeroPage, 0x202EU, descriptor_high);
    load(run.frame_index);
    step(0x40ECU, 0x40EFU, kAbsolute);
    {
        const bool shifted_out = (registers.accumulator & 0x80U) != 0U;
        registers.accumulator =
            static_cast<std::uint8_t>(registers.accumulator << 1U);
        registers.status = static_cast<std::uint8_t>(
            registers.status & static_cast<std::uint8_t>(~kCarryFlag));
        if (shifted_out) {
            registers.status =
                static_cast<std::uint8_t>(registers.status | kCarryFlag);
        }
        registers.status = set_nz(registers.status, registers.accumulator);
    }
    step(0x40EFU, 0x40F0U, kImplied);
    registers.y = registers.accumulator;
    registers.status = set_nz(registers.status, registers.y);
    step(0x40F0U, 0x40F1U, kImplied);
    load(entry_low);
    step(0x40F1U, 0x40F3U, kIndirectIndexed);
    store(0x40F3U, 0x40F5U, kZeroPage, 0x202FU, entry_low);
    increment_y();
    step(0x40F5U, 0x40F6U, kImplied);
    load(entry_high);
    step(0x40F6U, 0x40F8U, kIndirectIndexed);
    store(0x40F8U, 0x40FAU, kZeroPage, 0x2030U, entry_high);
    registers.y = 0U;
    step(0x40FAU, 0x40FBU, kImplied);
    load(run.count);
    step(0x40FBU, 0x40FDU, kIndirectIndexed);
    store(0x40FDU, 0x40FFU, kZeroPage, 0x202CU, run.count);
    registers.status = static_cast<std::uint8_t>(
        registers.status & static_cast<std::uint8_t>(~kCarryFlag));
    step(0x40FFU, 0x4100U, kImplied);
    registers.accumulator = add_with_carry(
        registers.status, registers.accumulator, cursor_before);
    step(0x4100U, 0x4102U, kZeroPage);
    registers.status =
        compare(registers.status, registers.accumulator, 0x40U);
    // The classifier declines a cursor overflow, so this branch is taken.
    step(0x4102U, 0x4104U, kImplied);
    step(0x4104U, 0x4107U, kBranchTaken);
    increment_y();
    step(0x4107U, 0x4108U, kImplied);

    auto cursor = cursor_before;
    for (std::uint8_t index = 0U; index < run.count; ++index) {
        const auto& emitted = run.entries[index];
        const bool last = index + 1U == run.count;

        // The signed vertical offset widens into $35:$36 against the actor's
        // own coordinate.
        load(emitted.descriptor[0]);
        step(0x4108U, 0x410AU, kIndirectIndexed);
        if (emitted.vertical_negative != 0U) {
            step(0x410AU, 0x411BU, kBranchTaken);
            registers.status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(~kCarryFlag));
            step(0x411BU, 0x411CU, kImplied);
            registers.accumulator = add_with_carry(
                registers.status, registers.accumulator, vertical_low);
            step(0x411CU, 0x411FU, kAbsolute);
            store(0x411FU, 0x4121U, kZeroPage, 0x2035U, emitted.widen_low);
            load(vertical_high);
            step(0x4121U, 0x4124U, kAbsolute);
            registers.accumulator = add_with_carry(
                registers.status, registers.accumulator, 0xFFU);
            step(0x4124U, 0x4126U, kImplied);
            store(0x4126U, 0x4128U, kZeroPage, 0x2036U, emitted.widen_high);
        } else {
            step(0x410AU, 0x410CU, kBranchNotTaken);
            registers.status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(~kCarryFlag));
            step(0x410CU, 0x410DU, kImplied);
            registers.accumulator = add_with_carry(
                registers.status, registers.accumulator, vertical_low);
            step(0x410DU, 0x4110U, kAbsolute);
            store(0x4110U, 0x4112U, kZeroPage, 0x2035U, emitted.widen_low);
            registers.accumulator = 0U;
            step(0x4112U, 0x4113U, kImplied);
            registers.accumulator = add_with_carry(
                registers.status, registers.accumulator, vertical_high);
            step(0x4113U, 0x4116U, kAbsolute);
            store(0x4116U, 0x4118U, kZeroPage, 0x2036U, emitted.widen_high);
            step(0x4118U, 0x4128U, kJump);
        }

        // The camera origin subtracts back out through the same pair, then
        // the table's own vertical origin adds in on the way to the port.
        registers.status =
            static_cast<std::uint8_t>(registers.status | kCarryFlag);
        step(0x4128U, 0x4129U, kImplied);
        load(emitted.widen_low);
        step(0x4129U, 0x412BU, kZeroPage);
        registers.accumulator = subtract_with_borrow(
            registers.status, registers.accumulator, camera_low);
        step(0x412BU, 0x412DU, kZeroPage);
        store(0x412DU, 0x412FU, kZeroPage, 0x2035U, emitted.camera_low);
        load(emitted.widen_high);
        step(0x412FU, 0x4131U, kZeroPage);
        registers.accumulator = subtract_with_borrow(
            registers.status, registers.accumulator, camera_high);
        step(0x4131U, 0x4133U, kZeroPage);
        store(0x4133U, 0x4135U, kZeroPage, 0x2036U, emitted.camera_high);
        registers.status = static_cast<std::uint8_t>(
            registers.status & static_cast<std::uint8_t>(~kCarryFlag));
        step(0x4135U, 0x4136U, kImplied);
        load(emitted.camera_low);
        step(0x4136U, 0x4138U, kZeroPage);
        registers.accumulator = add_with_carry(
            registers.status, registers.accumulator, 0x40U);
        step(0x4138U, 0x413AU, kImplied);
        store(
            0x413AU, 0x413DU, kPortStore, kVdcDataLow, emitted.vertical_low);
        load(emitted.camera_high);
        step(0x413DU, 0x413FU, kZeroPage);
        registers.accumulator = add_with_carry(
            registers.status, registers.accumulator, 0x00U);
        step(0x413FU, 0x4141U, kImplied);
        store(
            0x4141U,
            0x4144U,
            kPortStore,
            kVdcDataHigh,
            emitted.vertical_high);
        increment_y();
        step(0x4144U, 0x4145U, kImplied);

        // The horizontal offset widens the same way, straight to the port.
        load(emitted.descriptor[1]);
        step(0x4145U, 0x4147U, kIndirectIndexed);
        if (emitted.horizontal_negative != 0U) {
            step(0x4147U, 0x4159U, kBranchTaken);
            registers.status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(~kCarryFlag));
            step(0x4159U, 0x415AU, kImplied);
            registers.accumulator = add_with_carry(
                registers.status, registers.accumulator, horizontal_low);
            step(0x415AU, 0x415DU, kAbsolute);
            store(
                0x415DU,
                0x4160U,
                kPortStore,
                kVdcDataLow,
                emitted.horizontal_low);
            load(horizontal_high);
            step(0x4160U, 0x4163U, kAbsolute);
            registers.accumulator = add_with_carry(
                registers.status, registers.accumulator, 0xFFU);
            step(0x4163U, 0x4165U, kImplied);
            store(
                0x4165U,
                0x4168U,
                kPortStore,
                kVdcDataHigh,
                emitted.horizontal_high);
        } else {
            step(0x4147U, 0x4149U, kBranchNotTaken);
            registers.status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(~kCarryFlag));
            step(0x4149U, 0x414AU, kImplied);
            registers.accumulator = add_with_carry(
                registers.status, registers.accumulator, horizontal_low);
            step(0x414AU, 0x414DU, kAbsolute);
            store(
                0x414DU,
                0x4150U,
                kPortStore,
                kVdcDataLow,
                emitted.horizontal_low);
            registers.accumulator = 0U;
            step(0x4150U, 0x4151U, kImplied);
            registers.accumulator = add_with_carry(
                registers.status, registers.accumulator, horizontal_high);
            step(0x4151U, 0x4154U, kAbsolute);
            store(
                0x4154U,
                0x4157U,
                kPortStore,
                kVdcDataHigh,
                emitted.horizontal_high);
            step(0x4157U, 0x4168U, kJump);
        }

        // The pattern index biases by $51:$52.
        increment_y();
        step(0x4168U, 0x4169U, kImplied);
        load(emitted.descriptor[2]);
        step(0x4169U, 0x416BU, kIndirectIndexed);
        increment_y();
        step(0x416BU, 0x416CU, kImplied);
        registers.status = static_cast<std::uint8_t>(
            registers.status & static_cast<std::uint8_t>(~kCarryFlag));
        step(0x416CU, 0x416DU, kImplied);
        registers.accumulator = add_with_carry(
            registers.status, registers.accumulator, pattern_bias_low);
        step(0x416DU, 0x416FU, kZeroPage);
        store(0x416FU, 0x4172U, kPortStore, kVdcDataLow, emitted.pattern_low);
        load(emitted.descriptor[3]);
        step(0x4172U, 0x4174U, kIndirectIndexed);
        registers.accumulator = add_with_carry(
            registers.status, registers.accumulator, pattern_bias_high);
        step(0x4174U, 0x4176U, kZeroPage);
        store(
            0x4176U, 0x4179U, kPortStore, kVdcDataHigh, emitted.pattern_high);
        increment_y();
        step(0x4179U, 0x417AU, kImplied);

        // A bit-four actor merges $3D00,X into the attribute low byte.
        load(attributes);
        step(0x417AU, 0x417DU, kAbsolute);
        registers.status =
            bit_immediate(registers.status, registers.accumulator, 0x10U);
        step(0x417DU, 0x417FU, kImplied);
        if (emitted.attribute_low_merged != 0U) {
            step(0x417FU, 0x4181U, kBranchNotTaken);
            load(emitted.descriptor[4]);
            step(0x4181U, 0x4183U, kIndirectIndexed);
            registers.accumulator = static_cast<std::uint8_t>(
                registers.accumulator | merge_low);
            registers.status =
                set_nz(registers.status, registers.accumulator);
            step(0x4183U, 0x4186U, kAbsolute);
            step(0x4186U, 0x418AU, kJump);
        } else {
            step(0x417FU, 0x4188U, kBranchTaken);
            load(emitted.descriptor[4]);
            step(0x4188U, 0x418AU, kIndirectIndexed);
        }
        store(
            0x418AU,
            0x418DU,
            kPortStore,
            kVdcDataLow,
            emitted.attribute_low);

        // The classifier declines a bit-five actor, so this branch is taken.
        increment_y();
        step(0x418DU, 0x418EU, kImplied);
        load(attributes);
        step(0x418EU, 0x4191U, kAbsolute);
        registers.status =
            bit_immediate(registers.status, registers.accumulator, 0x20U);
        step(0x4191U, 0x4193U, kImplied);
        step(0x4193U, 0x419CU, kBranchTaken);
        load(emitted.descriptor[5]);
        step(0x419CU, 0x419EU, kIndirectIndexed);
        store(
            0x419EU,
            0x41A1U,
            kPortStore,
            kVdcDataHigh,
            emitted.attribute_high);

        // Advance the table cursor and the remaining count.
        increment_y();
        step(0x41A1U, 0x41A2U, kImplied);
        cursor = static_cast<std::uint8_t>(cursor + 1U);
        registers.status = set_nz(registers.status, cursor);
        store(0x41A2U, 0x41A4U, kReadModifyWrite, 0x204EU, cursor);
        const auto remaining =
            static_cast<std::uint8_t>(run.count - index - 1U);
        registers.status = set_nz(registers.status, remaining);
        store(0x41A4U, 0x41A6U, kReadModifyWrite, 0x202CU, remaining);
        if (last) {
            step(0x41A6U, kReturnPc, kBranchTaken);
        } else {
            step(0x41A6U, 0x41A8U, kBranchNotTaken);
            step(0x41A8U, 0x4108U, kJump);
        }
    }

    // The transcript and the classifier must agree about where the routine
    // ends up; a disagreement is a modelling error, not a state to run.
    if (registers.accumulator != run.accumulator
        || registers.y != run.final_y
        || cursor != run.cursor
        || registers.program_counter != kReturnPc) {
        return std::nullopt;
    }

    // And then past the return itself. Every one of the 73,818 calls ends on
    // this single `RTS`, which made it the largest emulated instruction left
    // anywhere on the route -- 1,560,450 master cycles, more than the whole of
    // bank `03`'s rollout put together. The routine pushes nothing, so the
    // frame the return reads is the one the caller's `JSR` left.
    absorb_return(
        replacement,
        work_ram,
        entry.registers.stack_pointer,
        {kRoutine.physical_bank, kReturnPc},
        cycle,
        registers);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_text_tile_write(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // 08:5B01 writes one character to the background tilemap. Six bounded
    // VDC stores point MAWR at $2D:$2E, select VWR, and emit the translated
    // tile word. A checkpoint per original instruction keeps timer IRQs at
    // the same boundaries and lets the hardware bus charge each VDC wait.
    constexpr RoutineKey kRoutine{0x08U, 0x5B01U};
    constexpr std::uint16_t kReturnPc = 0x5B43U;
    constexpr std::uint8_t kCarry = 0x01U;
    constexpr std::uint32_t kImplied = 6U;
    constexpr std::uint32_t kImmediate = 6U;
    constexpr std::uint32_t kZeroPage = 12U;
    constexpr std::uint32_t kZeroPageRmw = 18U;
    constexpr std::uint32_t kPortStore = 18U;
    constexpr std::uint32_t kBranchTaken = 12U;
    constexpr std::uint32_t kBranchNotTaken = 6U;

    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || (entry.registers.status & kUnsupportedFlags) != 0U
        || entry.registers.mpr[0] != 0xFFU
        || entry.registers.mpr[1] != 0xF8U) {
        return std::nullopt;
    }

    const auto read = [&work_ram](const std::uint8_t address) {
        return std::to_integer<std::uint8_t>(work_ram[address]);
    };
    dc_text_tile_write tile{};
    const auto address = static_cast<std::uint16_t>(
        read(0x2DU) | (static_cast<std::uint16_t>(read(0x2EU)) << 8U));
    if (!dc_prepare_text_tile_write(
            entry.registers.accumulator,
            address,
            &tile)) {
        return std::nullopt;
    }

    auto registers = entry.registers;
    std::uint32_t cycle = 0U;
    RoutineReplacement replacement{};
    replacement.checkpoints.reserve(32U);
    replacement.work_ram_writes.reserve(11U);

    const auto boundary =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t sample_back) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = cycle - sample_back,
                .master_cycle_offset = cycle,
                .instruction = {kRoutine.physical_bank, instruction_pc},
                .registers = registers,
            });
        };
    const auto step =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t cycles) {
            cycle += cycles;
            boundary(instruction_pc, resume_pc, 3U);
        };
    const auto write =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t cycles,
            const std::uint16_t logical_address,
            const std::uint8_t value) {
            cycle += cycles;
            replacement.work_ram_writes.push_back({
                .instruction = {kRoutine.physical_bank, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = cycle,
            });
            boundary(
                instruction_pc,
                resume_pc,
                logical_address <= 0x0003U ? 6U : 3U);
        };
    const auto load =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint8_t value,
            const std::uint32_t cycles) {
            registers.accumulator = value;
            registers.status = set_nz(registers.status, value);
            step(instruction_pc, resume_pc, cycles);
        };

    write(0x5B01U, 0x5B03U, kZeroPage, 0x202BU, registers.accumulator);
    registers.status = compare(registers.status, registers.accumulator, 0x20U);
    step(0x5B03U, 0x5B05U, kImmediate);
    if (registers.accumulator == 0x20U) {
        step(0x5B05U, 0x5B07U, kBranchNotTaken);
        load(0x5B07U, 0x5B09U, tile.character, kImmediate);
        write(0x5B09U, 0x5B0BU, kZeroPage, 0x202BU, registers.accumulator);
    } else {
        step(0x5B05U, 0x5B0BU, kBranchTaken);
    }

    load(0x5B0BU, 0x5B0DU, 0x00U, kImmediate);
    write(0x5B0DU, 0x5B0FU, kZeroPage, 0x2006U, registers.accumulator);
    write(0x5B0FU, 0x5B12U, kPortStore, 0x0000U, registers.accumulator);
    load(0x5B12U, 0x5B14U, read(0x2DU), kZeroPage);
    write(0x5B14U, 0x5B17U, kPortStore, 0x0002U, registers.accumulator);
    load(0x5B17U, 0x5B19U, read(0x2EU), kZeroPage);
    write(0x5B19U, 0x5B1CU, kPortStore, 0x0003U, registers.accumulator);
    load(0x5B1CU, 0x5B1EU, 0x02U, kImmediate);
    write(0x5B1EU, 0x5B20U, kZeroPage, 0x2006U, registers.accumulator);
    write(0x5B20U, 0x5B23U, kPortStore, 0x0000U, registers.accumulator);
    load(0x5B23U, 0x5B25U, tile.character, kZeroPage);
    registers.status = compare(registers.status, registers.accumulator, 0x41U);
    step(0x5B25U, 0x5B27U, kImmediate);
    if (registers.accumulator >= 0x41U) {
        step(0x5B27U, 0x5B2EU, kBranchTaken);
        registers.status = static_cast<std::uint8_t>(registers.status | kCarry);
        step(0x5B2EU, 0x5B2FU, kImplied);
        registers.accumulator = subtract_with_borrow(
            registers.status, registers.accumulator, 0x37U);
        step(0x5B2FU, 0x5B31U, kImmediate);
    } else {
        step(0x5B27U, 0x5B29U, kBranchNotTaken);
        registers.status = static_cast<std::uint8_t>(registers.status | kCarry);
        step(0x5B29U, 0x5B2AU, kImplied);
        registers.accumulator = subtract_with_borrow(
            registers.status, registers.accumulator, 0x30U);
        step(0x5B2AU, 0x5B2CU, kImmediate);
        step(0x5B2CU, 0x5B31U, kBranchTaken);
    }
    registers.status = static_cast<std::uint8_t>(registers.status & ~kCarry);
    step(0x5B31U, 0x5B32U, kImplied);
    registers.accumulator = add_with_carry(
        registers.status, registers.accumulator, 0x80U);
    step(0x5B32U, 0x5B34U, kImmediate);
    write(0x5B34U, 0x5B37U, kPortStore, 0x0002U, registers.accumulator);
    registers.accumulator = 0U; // CLA leaves flags unchanged on this core.
    step(0x5B37U, 0x5B38U, kImplied);
    registers.accumulator = add_with_carry(
        registers.status, registers.accumulator, 0x02U);
    step(0x5B38U, 0x5B3AU, kImmediate);
    write(0x5B3AU, 0x5B3DU, kPortStore, 0x0003U, registers.accumulator);

    const auto next_low = static_cast<std::uint8_t>(read(0x2DU) + 1U);
    auto next_high = read(0x2EU);
    registers.status = set_nz(registers.status, next_low);
    write(0x5B3DU, 0x5B3FU, kZeroPageRmw, 0x202DU, next_low);
    if (next_low != 0U) {
        step(0x5B3FU, kReturnPc, kBranchTaken);
    } else {
        step(0x5B3FU, 0x5B41U, kBranchNotTaken);
        next_high = static_cast<std::uint8_t>(next_high + 1U);
        registers.status = set_nz(registers.status, next_high);
        write(0x5B41U, kReturnPc, kZeroPageRmw, 0x202EU, next_high);
    }

    if (registers.accumulator != tile.tile_high
        || next_low != static_cast<std::uint8_t>(tile.next_address)
        || next_high != static_cast<std::uint8_t>(tile.next_address >> 8U)) {
        return std::nullopt;
    }
    auto at_return = registers;
    at_return.program_counter = kReturnPc;
    absorb_return(
        replacement,
        work_ram,
        entry.registers.stack_pointer,
        {kRoutine.physical_bank, kReturnPc},
        cycle,
        at_return);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_upper_table_actor_tile_schedule(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // 08:5A2E is the one-shot upper-table actor initializer reached from the
    // scene setup arms at 56E9, 571F and 575E.  It gives the selected actor a
    // fixed $80/$80 position, renders the first glyph through 5A7A -> 5B01,
    // then gives the actor to C5C9.  The last call is a coroutine handoff:
    // C5C9 consumes 5A3D and returns through C5E9, so this is a bounded
    // transaction despite not ending at a local RTS.
    constexpr RoutineKey kRoutine{0x08U, 0x5A2EU};
    constexpr RoutineKey kTileWriter{0x08U, 0x5B01U};
    constexpr RoutineKey kScheduler{0x01U, 0xC5C9U};
    constexpr RoutineKey kTileReturn{0x08U, 0x5A3BU};
    constexpr std::uint16_t kWorkRamBase = 0x2000U;
    constexpr std::uint32_t kImmediate = 6U;
    constexpr std::uint32_t kZeroPage = 12U;
    constexpr std::uint32_t kZeroPageRmw = 18U;
    constexpr std::uint32_t kAbsoluteIndexed = 15U;
    constexpr std::uint32_t kBranchTaken = 12U;
    constexpr std::uint32_t kBranchNotTaken = 6U;
    constexpr std::uint32_t kJumpToSubroutine = 21U;
    constexpr std::uint32_t kJump = 12U;

    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || (entry.registers.status & kUnsupportedFlags) != 0U
        || entry.registers.mpr[0] != 0xFFU
        || entry.registers.mpr[1] != 0xF8U
        || entry.registers.mpr[2] != 0x08U) {
        return std::nullopt;
    }

    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    RoutineReplacement replacement{};
    replacement.checkpoints.reserve(64U);
    replacement.work_ram_writes.reserve(32U);
    auto registers = entry.registers;
    std::uint32_t cycle = 0U;

    const auto read = [&staged](const std::size_t offset) {
        return std::to_integer<std::uint8_t>(staged[offset]);
    };
    const auto stage_write = [&staged](const std::uint16_t address,
                                       const std::uint8_t value) {
        if (address < kWorkRamBase
            || static_cast<std::size_t>(address - kWorkRamBase)
                >= staged.size()) {
            return false;
        }
        staged[static_cast<std::size_t>(address - kWorkRamBase)] =
            static_cast<std::byte>(value);
        return true;
    };
    const auto boundary = [&replacement, &registers, &cycle](
                              const std::uint8_t bank,
                              const std::uint16_t instruction_pc,
                              const std::uint16_t resume_pc) {
        registers.program_counter = resume_pc;
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = cycle - 3U,
            .master_cycle_offset = cycle,
            .instruction = {bank, instruction_pc},
            .registers = registers,
        });
    };
    const auto step = [&boundary, &cycle](const std::uint16_t instruction_pc,
                                           const std::uint16_t resume_pc,
                                           const std::uint32_t cycles) {
        cycle += cycles;
        boundary(0x08U, instruction_pc, resume_pc);
    };
    const auto write = [&replacement, &stage_write, &boundary, &cycle](
                           const std::uint16_t instruction_pc,
                           const std::uint16_t resume_pc,
                           const std::uint32_t cycles,
                           const std::uint16_t address,
                           const std::uint8_t value) {
        cycle += cycles;
        if (!stage_write(address, value)) {
            return false;
        }
        replacement.work_ram_writes.push_back({
            .instruction = {0x08U, instruction_pc},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
        boundary(0x08U, instruction_pc, resume_pc);
        return true;
    };
    const auto append_child = [&replacement, &staged, &registers, &cycle](
                                  const RoutineReplacement& child) {
        for (auto child_write : child.work_ram_writes) {
            if (child_write.logical_address >= kWorkRamBase
                && child_write.logical_address < 0x4000U) {
                staged[static_cast<std::size_t>(
                    child_write.logical_address - kWorkRamBase)] =
                    static_cast<std::byte>(child_write.value);
            }
            child_write.master_cycle_offset += cycle;
            replacement.work_ram_writes.push_back(child_write);
        }
        for (auto child_checkpoint : child.checkpoints) {
            child_checkpoint.interrupt_sample_master_cycle_offset += cycle;
            child_checkpoint.master_cycle_offset += cycle;
            replacement.checkpoints.push_back(child_checkpoint);
        }
        cycle += child.body_master_cycles;
        registers = child.output_registers;
    };
    const auto rotate_left = [](std::uint8_t& status,
                                const std::uint8_t value) {
        const auto carry_in = static_cast<std::uint8_t>(status & kCarryFlag);
        status = static_cast<std::uint8_t>(status & ~kCarryFlag);
        status = static_cast<std::uint8_t>(status | (value >> 7U));
        const auto result = static_cast<std::uint8_t>((value << 1U) | carry_in);
        status = set_nz(status, result);
        return result;
    };

    registers.accumulator = 0x80U;
    registers.status = set_nz(registers.status, registers.accumulator);
    step(0x5A2EU, 0x5A30U, kImmediate);
    if (!write(
            0x5A30U,
            0x5A33U,
            kAbsoluteIndexed,
            static_cast<std::uint16_t>(0x3000U + registers.x),
            registers.accumulator)) {
        return std::nullopt;
    }
    registers.accumulator = 0x80U;
    registers.status = set_nz(registers.status, registers.accumulator);
    step(0x5A33U, 0x5A35U, kImmediate);
    if (!write(
            0x5A35U,
            0x5A38U,
            kAbsoluteIndexed,
            static_cast<std::uint16_t>(0x3080U + registers.x),
            registers.accumulator)) {
        return std::nullopt;
    }

    // JSR $5A7A. Its tail JMP enters the independently proven 5B01 tile
    // writer, whose absorbed RTS returns to 5A3B.
    const auto tile_stack_high = registers.stack_pointer;
    const auto tile_stack_low = static_cast<std::uint8_t>(tile_stack_high - 1U);
    if (!stage_write(static_cast<std::uint16_t>(0x2100U + tile_stack_high), 0x5AU)
        || !stage_write(static_cast<std::uint16_t>(0x2100U + tile_stack_low), 0x3AU)) {
        return std::nullopt;
    }
    replacement.work_ram_writes.push_back({
        .instruction = {0x08U, 0x5A38U},
        .logical_address = static_cast<std::uint16_t>(0x2100U + tile_stack_high),
        .value = 0x5AU,
        .master_cycle_offset = cycle,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x08U, 0x5A38U},
        .logical_address = static_cast<std::uint16_t>(0x2100U + tile_stack_low),
        .value = 0x3AU,
        .master_cycle_offset = cycle,
    });
    registers.stack_pointer = static_cast<std::uint8_t>(registers.stack_pointer - 2U);
    step(0x5A38U, 0x5A7AU, kJumpToSubroutine);

    registers.accumulator = 0x65U;
    registers.status = set_nz(registers.status, registers.accumulator);
    step(0x5A7AU, 0x5A7CU, kImmediate);
    if (!write(0x5A7CU, 0x5A7EU, kZeroPage, 0x2026U, registers.accumulator)) {
        return std::nullopt;
    }
    registers.accumulator = read(0x5DU);
    registers.status = set_nz(registers.status, registers.accumulator);
    step(0x5A7EU, 0x5A80U, kZeroPage);
    registers.y = read(static_cast<std::size_t>(0x1E80U + registers.x));
    registers.status = set_nz(registers.status, registers.y);
    step(0x5A80U, 0x5A83U, kAbsoluteIndexed);
    if (registers.y == 0U) {
        step(0x5A83U, 0x5A86U, kBranchTaken);
    } else {
        step(0x5A83U, 0x5A85U, kBranchNotTaken);
        registers.accumulator = shift_left(registers.status, registers.accumulator);
        step(0x5A85U, 0x5A86U, kImmediate);
    }
    registers.status = static_cast<std::uint8_t>(registers.status & ~kCarryFlag);
    step(0x5A86U, 0x5A87U, kImmediate);
    registers.accumulator = add_with_carry(
        registers.status,
        registers.accumulator,
        read(static_cast<std::size_t>(0x1300U + registers.x)));
    step(0x5A87U, 0x5A8AU, kAbsoluteIndexed);
    if (!write(0x5A8AU, 0x5A8CU, kZeroPage, 0x202EU, 0x00U)) {
        return std::nullopt;
    }
    for (const auto instruction_pc : std::array<std::uint16_t, 5U>{
             0x5A8CU, 0x5A8FU, 0x5A92U, 0x5A95U, 0x5A98U}) {
        registers.accumulator = shift_left(registers.status, registers.accumulator);
        step(instruction_pc, static_cast<std::uint16_t>(instruction_pc + 1U), kImmediate);
        const auto rotated = rotate_left(registers.status, read(0x2EU));
        if (!write(
                static_cast<std::uint16_t>(instruction_pc + 1U),
                static_cast<std::uint16_t>(instruction_pc + 3U),
                kZeroPageRmw,
                0x202EU,
                rotated)) {
            return std::nullopt;
        }
    }
    registers.status = static_cast<std::uint8_t>(registers.status & ~kCarryFlag);
    step(0x5A9BU, 0x5A9CU, kImmediate);
    registers.accumulator = add_with_carry(
        registers.status,
        registers.accumulator,
        read(static_cast<std::size_t>(0x1180U + registers.x)));
    step(0x5A9CU, 0x5A9FU, kAbsoluteIndexed);
    if (!write(0x5A9FU, 0x5AA1U, kZeroPage, 0x202DU, registers.accumulator)) {
        return std::nullopt;
    }
    registers.accumulator = 0U; // CLA leaves flags unchanged on this core.
    step(0x5AA1U, 0x5AA2U, kImmediate);
    registers.accumulator = add_with_carry(registers.status, registers.accumulator, read(0x2EU));
    step(0x5AA2U, 0x5AA4U, kZeroPage);
    if (!write(0x5AA4U, 0x5AA6U, kZeroPage, 0x202EU, registers.accumulator)) {
        return std::nullopt;
    }
    registers.accumulator = read(0x26U);
    registers.status = set_nz(registers.status, registers.accumulator);
    step(0x5AA6U, 0x5AA8U, kZeroPage);
    step(0x5AA8U, 0x5B01U, kJump);

    RoutineEntry tile_entry{.routine = kTileWriter, .registers = registers};
    const auto tile = replace_text_tile_write(tile_entry, staged);
    if (!tile || tile->checkpoints.empty()
        || tile->return_instruction != kTileReturn) {
        return std::nullopt;
    }
    append_child(*tile);

    // JSR $C5C9.  It installs the 5A3E continuation into the actor slot and
    // returns directly to the actor pass through C5E9.
    const auto scheduler_stack_high = registers.stack_pointer;
    const auto scheduler_stack_low = static_cast<std::uint8_t>(scheduler_stack_high - 1U);
    if (!stage_write(static_cast<std::uint16_t>(0x2100U + scheduler_stack_high), 0x5AU)
        || !stage_write(static_cast<std::uint16_t>(0x2100U + scheduler_stack_low), 0x3DU)) {
        return std::nullopt;
    }
    replacement.work_ram_writes.push_back({
        .instruction = {0x08U, 0x5A3BU},
        .logical_address = static_cast<std::uint16_t>(0x2100U + scheduler_stack_high),
        .value = 0x5AU,
        .master_cycle_offset = cycle,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x08U, 0x5A3BU},
        .logical_address = static_cast<std::uint16_t>(0x2100U + scheduler_stack_low),
        .value = 0x3DU,
        .master_cycle_offset = cycle,
    });
    registers.stack_pointer = static_cast<std::uint8_t>(registers.stack_pointer - 2U);
    registers.program_counter = 0xC5C9U;
    cycle += kJumpToSubroutine;
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = cycle - 3U,
        .master_cycle_offset = cycle,
        .instruction = {0x08U, 0x5A3BU},
        .registers = registers,
    });

    RoutineEntry scheduler_entry{.routine = kScheduler, .registers = registers};
    const auto scheduler = replace_coroutine_scheduler(scheduler_entry, staged);
    if (!scheduler || scheduler->checkpoints.empty()) {
        return std::nullopt;
    }
    append_child(*scheduler);
    replacement.return_instruction = scheduler->return_instruction;
    replacement.output_registers = scheduler->output_registers;
    replacement.body_master_cycles = cycle;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_upper_table_string_record(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    // 08:5ABF renders one upper-table text record.  A record starts with a
    // tile-address selector at cursor+1 and then contains characters from
    // cursor+2 through an FF terminator.  FE advances the cursor by three
    // bytes and tail-jumps back to 5ABF for the next record.  Keeping that
    // jump as the transaction boundary is intentional: the long scene labels
    // otherwise exceed the runtime's bounded transcript budget, while 5ABF
    // itself is an ordinary instruction boundary at which the registry can
    // safely re-enter.
    constexpr RoutineKey kRoutine{0x08U, 0x5ABFU};
    constexpr RoutineKey kTileWriter{0x08U, 0x5B01U};
    constexpr std::uint16_t kTileReturn = 0x5AEEU;
    constexpr std::uint16_t kReturnPc = 0x5AF1U;
    constexpr std::uint16_t kWorkRamBase = 0x2000U;
    constexpr std::uint32_t kImplied = 6U;
    constexpr std::uint32_t kImmediate = 6U;
    constexpr std::uint32_t kZeroPage = 12U;
    constexpr std::uint32_t kZeroPageRmw = 18U;
    constexpr std::uint32_t kIndirectIndexed = 21U;
    constexpr std::uint32_t kJumpToSubroutine = 21U;
    constexpr std::uint32_t kJump = 12U;
    constexpr std::uint32_t kBranchTaken = 12U;
    constexpr std::uint32_t kBranchNotTaken = 6U;
    constexpr std::size_t kMaximumCharacters = 32U;

    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || (entry.registers.status & kUnsupportedFlags) != 0U
        || entry.registers.mpr[0] != 0xFFU
        || entry.registers.mpr[1] != 0xF8U
        || entry.registers.mpr[2] != 0x08U) {
        return std::nullopt;
    }

    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    auto registers = entry.registers;
    RoutineReplacement replacement{};
    replacement.work_ram_writes.reserve(kMaximumCharacters * 16U + 8U);
    replacement.checkpoints.reserve(kMaximumCharacters * 40U + 32U);
    std::uint32_t cycle = 0U;

    const auto read = [&staged](const std::size_t offset) {
        return std::to_integer<std::uint8_t>(staged[offset]);
    };
    const auto read_indirect = [&read, &registers, rom](const std::uint16_t address)
        -> std::optional<std::uint8_t> {
        const auto bank = registers.mpr[address >> 13U];
        if (bank == 0xF8U) {
            return read(static_cast<std::size_t>(address & 0x1FFFU));
        }
        if (bank >= 0x80U || rom.size() != 0x60000U) {
            return std::nullopt;
        }
        const auto mapped_bank = bank < 0x40U
            ? static_cast<std::size_t>(bank & 0x1FU)
            : static_cast<std::size_t>((bank & 0x0FU) + 0x20U);
        const auto offset = mapped_bank * 0x2000U + (address & 0x1FFFU);
        if (offset >= rom.size()) {
            return std::nullopt;
        }
        return std::to_integer<std::uint8_t>(rom[offset]);
    };
    const auto stage_write = [&staged](const std::uint16_t address,
                                       const std::uint8_t value) {
        if (address < kWorkRamBase
            || static_cast<std::size_t>(address - kWorkRamBase) >= staged.size()) {
            return false;
        }
        staged[static_cast<std::size_t>(address - kWorkRamBase)] =
            static_cast<std::byte>(value);
        return true;
    };
    const auto boundary = [&replacement, &registers, &cycle](
                              const std::uint8_t bank,
                              const std::uint16_t instruction_pc,
                              const std::uint16_t resume_pc,
                              const std::uint32_t sample_back = 3U) {
        registers.program_counter = resume_pc;
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = cycle - sample_back,
            .master_cycle_offset = cycle,
            .instruction = {bank, instruction_pc},
            .registers = registers,
        });
    };
    const auto step = [&boundary, &cycle](const std::uint16_t instruction_pc,
                                           const std::uint16_t resume_pc,
                                           const std::uint32_t cycles) {
        cycle += cycles;
        boundary(0x08U, instruction_pc, resume_pc);
    };
    const auto write = [&replacement, &stage_write, &boundary, &cycle](
                           const std::uint16_t instruction_pc,
                           const std::uint16_t resume_pc,
                           const std::uint32_t cycles,
                           const std::uint16_t address,
                           const std::uint8_t value) {
        cycle += cycles;
        if (!stage_write(address, value)) {
            return false;
        }
        replacement.work_ram_writes.push_back({
            .instruction = {0x08U, instruction_pc},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = cycle,
        });
        boundary(0x08U, instruction_pc, resume_pc);
        return true;
    };
    const auto append_child = [&replacement, &staged, &registers, &cycle](
                                  const RoutineReplacement& child) {
        for (auto child_write : child.work_ram_writes) {
            if (child_write.logical_address >= kWorkRamBase
                && child_write.logical_address < 0x4000U) {
                staged[static_cast<std::size_t>(
                    child_write.logical_address - kWorkRamBase)] =
                    static_cast<std::byte>(child_write.value);
            }
            child_write.master_cycle_offset += cycle;
            replacement.work_ram_writes.push_back(child_write);
        }
        for (auto child_checkpoint : child.checkpoints) {
            child_checkpoint.interrupt_sample_master_cycle_offset += cycle;
            child_checkpoint.master_cycle_offset += cycle;
            replacement.checkpoints.push_back(child_checkpoint);
        }
        cycle += child.body_master_cycles;
        registers = child.output_registers;
    };
    const auto rotate_left = [](std::uint8_t& status,
                                const std::uint8_t value) {
        const auto carry_in = static_cast<std::uint8_t>(status & kCarryFlag);
        status = static_cast<std::uint8_t>(status & ~kCarryFlag);
        status = static_cast<std::uint8_t>(status | (value >> 7U));
        const auto result = static_cast<std::uint8_t>((value << 1U) | carry_in);
        status = set_nz(status, result);
        return result;
    };

    const auto cursor = [&read]() {
        return static_cast<std::uint16_t>(
            read(0x2FU) | (static_cast<std::uint16_t>(read(0x30U)) << 8U));
    };
    const auto cursor_byte = [&cursor, &read_indirect](const std::uint8_t index)
        -> std::optional<std::uint8_t> {
        return read_indirect(static_cast<std::uint16_t>(cursor() + index));
    };

    registers.y = 0x01U;
    registers.status = set_nz(registers.status, registers.y);
    step(0x5ABFU, 0x5AC1U, kImmediate);
    const auto selector = cursor_byte(registers.y);
    if (!selector) {
        return std::nullopt;
    }
    registers.accumulator = *selector;
    registers.status = set_nz(registers.status, registers.accumulator);
    step(0x5AC1U, 0x5AC3U, kIndirectIndexed);
    if (!write(0x5AC3U, 0x5AC5U, kZeroPage, 0x202EU, 0U)) {
        return std::nullopt;
    }
    for (const auto instruction_pc : std::array<std::uint16_t, 5U>{
             0x5AC5U, 0x5AC8U, 0x5ACBU, 0x5ACEU, 0x5AD1U}) {
        registers.accumulator = shift_left(registers.status, registers.accumulator);
        step(instruction_pc, static_cast<std::uint16_t>(instruction_pc + 1U), kImplied);
        const auto rotated = rotate_left(registers.status, read(0x2EU));
        if (!write(
                static_cast<std::uint16_t>(instruction_pc + 1U),
                static_cast<std::uint16_t>(instruction_pc + 3U),
                kZeroPageRmw,
                0x202EU,
                rotated)) {
            return std::nullopt;
        }
    }
    registers.y = 0U; // CLY leaves flags unchanged on this core.
    step(0x5AD4U, 0x5AD5U, kImplied);
    registers.status = static_cast<std::uint8_t>(registers.status & ~kCarryFlag);
    step(0x5AD5U, 0x5AD6U, kImplied);
    const auto low_addend = cursor_byte(registers.y);
    if (!low_addend) {
        return std::nullopt;
    }
    registers.accumulator = add_with_carry(
        registers.status, registers.accumulator, *low_addend);
    step(0x5AD6U, 0x5AD8U, kIndirectIndexed);
    if (!write(0x5AD8U, 0x5ADAU, kZeroPage, 0x202DU, registers.accumulator)) {
        return std::nullopt;
    }
    registers.accumulator = 0U; // CLA leaves flags unchanged on this core.
    step(0x5ADAU, 0x5ADBU, kImplied);
    registers.accumulator = add_with_carry(registers.status, registers.accumulator, read(0x2EU));
    step(0x5ADBU, 0x5ADDU, kZeroPage);
    if (!write(0x5ADDU, 0x5ADFU, kZeroPage, 0x202EU, registers.accumulator)) {
        return std::nullopt;
    }

    registers.y = 0x02U;
    registers.status = set_nz(registers.status, registers.y);
    step(0x5ADFU, 0x5AE1U, kImmediate);
    for (std::size_t character_count = 0U;
         character_count < kMaximumCharacters;
         ++character_count) {
        const auto character = cursor_byte(registers.y);
        if (!character) {
            return std::nullopt;
        }
        registers.accumulator = *character;
        registers.status = set_nz(registers.status, registers.accumulator);
        step(0x5AE1U, 0x5AE3U, kIndirectIndexed);
        registers.status = compare(registers.status, registers.accumulator, 0xFFU);
        step(0x5AE3U, 0x5AE5U, kImmediate);
        if (registers.accumulator == 0xFFU) {
            step(0x5AE5U, kReturnPc, kBranchTaken);
            auto at_return = registers;
            at_return.program_counter = kReturnPc;
            absorb_return(
                replacement,
                work_ram,
                entry.registers.stack_pointer,
                {kRoutine.physical_bank, kReturnPc},
                cycle,
                at_return);
            return replacement;
        }
        step(0x5AE5U, 0x5AE7U, kBranchNotTaken);
        registers.status = compare(registers.status, registers.accumulator, 0xFEU);
        step(0x5AE7U, 0x5AE9U, kImmediate);
        if (registers.accumulator == 0xFEU) {
            step(0x5AE9U, 0x5AF2U, kBranchTaken);
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            step(0x5AF2U, 0x5AF3U, kImplied);
            registers.accumulator = registers.y;
            registers.status = set_nz(registers.status, registers.accumulator);
            step(0x5AF3U, 0x5AF4U, kImplied);
            registers.status = static_cast<std::uint8_t>(registers.status & ~kCarryFlag);
            step(0x5AF4U, 0x5AF5U, kImplied);
            registers.accumulator = add_with_carry(registers.status, registers.accumulator, read(0x2FU));
            step(0x5AF5U, 0x5AF7U, kZeroPage);
            if (!write(0x5AF7U, 0x5AF9U, kZeroPage, 0x202FU, registers.accumulator)) {
                return std::nullopt;
            }
            registers.accumulator = 0U; // CLA leaves flags unchanged on this core.
            step(0x5AF9U, 0x5AFAU, kImplied);
            registers.accumulator = add_with_carry(registers.status, registers.accumulator, read(0x30U));
            step(0x5AFAU, 0x5AFCU, kZeroPage);
            if (!write(0x5AFCU, 0x5AFEU, kZeroPage, 0x2030U, registers.accumulator)) {
                return std::nullopt;
            }
            step(0x5AFEU, 0x5ABFU, kJump);
            replacement.return_instruction = kRoutine;
            replacement.output_registers = registers;
            replacement.body_master_cycles = cycle;
            return replacement;
        }
        step(0x5AE9U, 0x5AEBU, kBranchNotTaken);

        // JSR $5B01.  The child absorbs its RTS from this staged 5AED frame
        // and resumes at INY $5AEE, preserving every VDC wait checkpoint.
        const auto stack_high = registers.stack_pointer;
        const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
        if (!stage_write(static_cast<std::uint16_t>(0x2100U + stack_high), 0x5AU)
            || !stage_write(static_cast<std::uint16_t>(0x2100U + stack_low), 0xEDU)) {
            return std::nullopt;
        }
        replacement.work_ram_writes.push_back({
            .instruction = {0x08U, 0x5AEBU},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_high),
            .value = 0x5AU,
            .master_cycle_offset = cycle,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x08U, 0x5AEBU},
            .logical_address = static_cast<std::uint16_t>(0x2100U + stack_low),
            .value = 0xEDU,
            .master_cycle_offset = cycle,
        });
        registers.stack_pointer = static_cast<std::uint8_t>(registers.stack_pointer - 2U);
        step(0x5AEBU, 0x5B01U, kJumpToSubroutine);
        RoutineEntry tile_entry{.routine = kTileWriter, .registers = registers};
        const auto tile = replace_text_tile_write(tile_entry, staged);
        if (!tile || tile->checkpoints.empty()
            || tile->return_instruction != RoutineKey{0x08U, kTileReturn}) {
            return std::nullopt;
        }
        append_child(*tile);

        registers.y = static_cast<std::uint8_t>(registers.y + 1U);
        registers.status = set_nz(registers.status, registers.y);
        step(0x5AEEU, 0x5AEFU, kImplied);
        step(0x5AEFU, 0x5AE1U, kJump);
    }

    // A record longer than the proved transcript budget is not a failure of
    // the game path: decline it so the original CPU continues from 5ABF.
    return std::nullopt;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_video_update_idle_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // 08:5E42 is the head of a deferred video-update routine. The canonical
    // route enters it once per active frame, and all 2,649 calls find both
    // gate bytes clear and return after four instructions. If either byte is
    // active, the routine has state-changing work beyond this prefix; leave
    // that unobserved path to the original code.
    constexpr RoutineKey kRoutine{0x08U, 0x5E42U};
    constexpr std::uint16_t kReturnPc = 0x5E4CU;
    constexpr std::uint32_t kAbsolute = 15U;
    constexpr std::uint32_t kBranchNotTaken = 6U;

    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || (entry.registers.status & kUnsupportedFlags) != 0U
        || entry.registers.mpr[1] != 0xF8U) {
        return std::nullopt;
    }

    const auto video_busy =
        std::to_integer<std::uint8_t>(work_ram[0x0254U]);
    const auto deferred_update =
        std::to_integer<std::uint8_t>(work_ram[0x0D53U]);
    if (!dc_video_update_gate_is_idle(video_busy, deferred_update)) {
        return std::nullopt;
    }

    auto registers = entry.registers;
    std::uint32_t cycle = 0U;
    RoutineReplacement replacement{};
    replacement.checkpoints.reserve(5U);

    const auto step =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t cycles) {
            cycle += cycles;
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = cycle - 3U,
                .master_cycle_offset = cycle,
                .instruction = {kRoutine.physical_bank, instruction_pc},
                .registers = registers,
            });
        };
    const auto load =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint8_t value) {
            registers.accumulator = value;
            registers.status = set_nz(registers.status, value);
            step(instruction_pc, resume_pc, kAbsolute);
        };

    load(0x5E42U, 0x5E45U, video_busy);
    step(0x5E45U, 0x5E47U, kBranchNotTaken);
    load(0x5E47U, 0x5E4AU, deferred_update);
    step(0x5E4AU, kReturnPc, kBranchNotTaken);

    auto at_return = registers;
    at_return.program_counter = kReturnPc;
    absorb_return(
        replacement,
        work_ram,
        entry.registers.stack_pointer,
        {kRoutine.physical_bank, kReturnPc},
        cycle,
        at_return);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_boot_work_ram_clear_seam(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // 08:584C initializes a fresh gameplay work area. Its middle instruction,
    // `TII $3000,$3001,$0FFF`, performs 4,095 ordered writes without an
    // instruction boundary inside it, so it cannot fit the replacement
    // channel and cannot be chunked. The code around it is still four bounded
    // transactions: cross both calls, stop immediately before the TII, and
    // resume immediately after it for the VDC setup and final return.
    constexpr std::uint8_t kBank = 0x08U;
    constexpr std::uint16_t kFirstCallPc = 0x584CU;
    constexpr std::uint16_t kSecondCallPc = 0x584FU;
    constexpr std::uint16_t kClearPrefixPc = 0x5852U;
    constexpr std::uint16_t kTransferPc = 0x585AU;
    constexpr std::uint16_t kVdcTailPc = 0x5861U;
    constexpr std::uint16_t kReturnPc = 0x5870U;
    constexpr std::uint32_t kImmediate = 6U;
    constexpr std::uint32_t kZeroPage = 12U;
    constexpr std::uint32_t kAbsolute = 15U;
    constexpr std::uint32_t kPortStore = 18U;
    constexpr std::uint32_t kSubroutineCall = 21U;

    const auto pc = entry.routine.logical_pc;
    if (entry.routine.physical_bank != kBank
        || (pc != kFirstCallPc && pc != kSecondCallPc
            && pc != kClearPrefixPc && pc != kVdcTailPc)
        || work_ram.size() < 0x2000U
        || (entry.registers.status & kUnsupportedFlags) != 0U
        || entry.registers.mpr[1] != 0xF8U) {
        return std::nullopt;
    }
    if ((pc == kFirstCallPc || pc == kSecondCallPc)
        && entry.registers.mpr[7] != 0x00U) {
        return std::nullopt;
    }
    if (pc == kVdcTailPc && entry.registers.mpr[0] != 0xFFU) {
        return std::nullopt;
    }

    auto registers = entry.registers;
    std::uint32_t cycle = 0U;
    RoutineReplacement replacement{};
    replacement.checkpoints.reserve(7U);
    replacement.work_ram_writes.reserve(3U);

    const auto boundary =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t sample_back) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = cycle - sample_back,
                .master_cycle_offset = cycle,
                .instruction = {kBank, instruction_pc},
                .registers = registers,
            });
        };
    const auto step =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t cycles) {
            cycle += cycles;
            boundary(instruction_pc, resume_pc, 3U);
        };
    const auto write =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t cycles,
            const std::uint16_t logical_address,
            const std::uint8_t value) {
            cycle += cycles;
            replacement.work_ram_writes.push_back({
                .instruction = {kBank, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = cycle,
            });
            boundary(
                instruction_pc,
                resume_pc,
                logical_address <= 0x0003U ? 6U : 3U);
        };

    if (pc == kFirstCallPc || pc == kSecondCallPc) {
        const auto target = static_cast<std::uint16_t>(
            pc == kFirstCallPc ? 0xED4DU : 0xEA77U);
        const auto pushed = static_cast<std::uint16_t>(pc + 2U);
        replacement.work_ram_writes.push_back({
            .instruction = {kBank, pc},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + registers.stack_pointer),
            .value = static_cast<std::uint8_t>(pushed >> 8U),
            .master_cycle_offset = cycle,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {kBank, pc},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U
                + static_cast<std::uint8_t>(
                    registers.stack_pointer - 1U)),
            .value = static_cast<std::uint8_t>(pushed),
            .master_cycle_offset = cycle,
        });
        registers.stack_pointer =
            static_cast<std::uint8_t>(registers.stack_pointer - 2U);
        cycle += kSubroutineCall;
        boundary(pc, target, 3U);
        replacement.return_instruction = {0x00U, target};
        replacement.output_registers = registers;
        replacement.body_master_cycles = cycle;
        return replacement;
    }

    if (pc == kClearPrefixPc) {
        write(0x5852U, 0x5854U, kZeroPage, 0x2054U, 0x00U);
        write(0x5854U, 0x5857U, kAbsolute, 0x2D53U, 0x00U);
        write(0x5857U, kTransferPc, kAbsolute, 0x3000U, 0x00U);
        replacement.return_instruction = {kBank, kTransferPc};
        replacement.output_registers = registers;
        replacement.body_master_cycles = cycle;
        return replacement;
    }

    registers.accumulator = 0x09U;
    registers.status = set_nz(registers.status, registers.accumulator);
    step(0x5861U, 0x5863U, kImmediate);
    write(0x5863U, 0x5865U, kZeroPage, 0x2006U, registers.accumulator);
    write(0x5865U, 0x5868U, kPortStore, 0x0000U, registers.accumulator);
    registers.accumulator = 0x10U;
    registers.status = set_nz(registers.status, registers.accumulator);
    step(0x5868U, 0x586AU, kImmediate);
    write(0x586AU, 0x586DU, kPortStore, 0x0002U, registers.accumulator);
    write(0x586DU, kReturnPc, kPortStore, 0x0003U, 0x00U);

    auto at_return = registers;
    at_return.program_counter = kReturnPc;
    absorb_return(
        replacement,
        work_ram,
        entry.registers.stack_pointer,
        {kBank, kReturnPc},
        cycle,
        at_return);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_sat_submission_step(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The driver that surrounds `08:40DF` and `08:40C3`. It points the VDC at
    // the satellite attribute table, drains the three queues `01:C606` fills,
    // emits the ball's own slot, and falls into the tail clear.
    //
    // It cannot be replaced as one routine: a single transaction would have to
    // subsume every nested `08:40DF` call, twenty-one per frame at ~944 cycles
    // each. So the transcript is a walk that starts at any of the five
    // addresses control can arrive at with no replacement active -- the
    // routine's head, and the four returns from a `JSR $40DF` -- and runs
    // until it has made the next call, ending on the emitter's own first
    // instruction so that the machine takes the body from there. The walk is
    // shared because the stopping points chain: draining one queue empty falls
    // straight into the next, so an arrival at `08:4063` can end anywhere from
    // `08:40DF` to `08:40C3`.
    //
    // The four `JSR`s used to be where each walk stopped rather than what it
    // ended on, which left the call itself emulated: 73,818 of them, and the
    // largest block of assembly still standing on the route. Subsuming the
    // callee is what a transcript cannot afford here; crossing the call is
    // three stack bytes and a jump, and the address it lands on is a
    // registered entry that picks the body up unchanged. A transcript may end
    // anywhere, including on another transcript's front door.
    //
    //   4036  point MAWR at $7800 + ($58 << 2), select VWR
    //   4056  drain $2C3A/$2CBC downward   -> JSR $40DF at 4060
    //   4068  drain $2BBA/$2CBA downward   -> JSR $40DF at 4072
    //   407A  emit slot 0 when on camera and not hidden -> JSR at 4085
    //   4088  drain $2BBA from $7F down to $2CBB -> JSR $40DF at 4094
    //   409B  re-point MAWR past the emitted entries -> falls into 40C3
    constexpr std::uint8_t kBank = 0x08U;
    constexpr std::uint16_t kEntryPc = 0x4036U;
    constexpr std::uint16_t kFirstReturnPc = 0x4063U;
    constexpr std::uint16_t kSecondReturnPc = 0x4075U;
    constexpr std::uint16_t kBallReturnPc = 0x4088U;
    constexpr std::uint16_t kThirdReturnPc = 0x4097U;
    constexpr std::uint16_t kEmitterPc = 0x40DFU;
    constexpr std::uint16_t kTailClearPc = 0x40C3U;
    constexpr std::uint16_t kSpriteCountGuardPc = 0x40A6U;

    constexpr std::uint16_t kVdcAddress = 0x0000U;
    constexpr std::uint16_t kVdcDataLow = 0x0002U;
    constexpr std::uint16_t kVdcDataHigh = 0x0003U;
    constexpr std::uint16_t kPortShadowAddress = 0x2006U;
    constexpr std::uint16_t kSpriteCountAddress = 0x204EU;
    constexpr std::uint16_t kSpriteBaseAddress = 0x2058U;
    constexpr std::uint16_t kFirstCursorAddress = 0x2CBCU;
    constexpr std::uint16_t kSecondCursorAddress = 0x2CBAU;
    constexpr std::uint16_t kThirdCursorAddress = 0x2CBBU;
    constexpr std::uint16_t kFirstQueueBase = 0x2C3AU;
    constexpr std::uint16_t kSharedQueueBase = 0x2BBAU;
    constexpr std::uint16_t kAttributesBase = 0x3080U;

    constexpr std::uint32_t kImplied = 6U;
    constexpr std::uint32_t kImmediate = 6U;
    constexpr std::uint32_t kZeroPage = 12U;
    constexpr std::uint32_t kAbsolute = 15U;
    constexpr std::uint32_t kAbsoluteIndexed = 15U;
    constexpr std::uint32_t kAbsoluteReadModifyWrite = 21U;
    constexpr std::uint32_t kPortStore = 18U;
    constexpr std::uint32_t kPush = 9U;
    constexpr std::uint32_t kPull = 12U;
    constexpr std::uint32_t kSubroutineCall = 21U;
    constexpr std::uint32_t kBranchTaken = 12U;
    constexpr std::uint32_t kBranchNotTaken = 6U;

    if (entry.routine.physical_bank != kBank
        || (entry.routine.logical_pc != kEntryPc
            && entry.routine.logical_pc != kFirstReturnPc
            && entry.routine.logical_pc != kSecondReturnPc
            && entry.routine.logical_pc != kBallReturnPc
            && entry.routine.logical_pc != kThirdReturnPc)) {
        return std::nullopt;
    }
    if (work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    if ((entry.registers.status & kUnsupportedFlags) != 0U) {
        return std::nullopt;
    }
    // The VDC ports are only addressable while the hardware page is mapped at
    // segment 0, which is also what the core's write validator requires.
    if (entry.registers.mpr[0] != 0xFFU) {
        return std::nullopt;
    }
    // Three of the walk's queue drains push and pull around the call, and all
    // four of its calls push a return address, out of the same private copy of
    // work RAM the queues are read from. That copy is only the stack while
    // $2000 is the page MPR1 selects.
    if (entry.registers.mpr[1] != 0xF8U) {
        return std::nullopt;
    }

    // The walk re-reads two of the bytes it writes: each queue's cursor is
    // decremented and then loaded again at the top of its loop. Shadow them
    // rather than reading stale RAM.
    auto first_cursor = std::to_integer<std::uint8_t>(
        work_ram[kFirstCursorAddress - 0x2000U]);
    auto second_cursor = std::to_integer<std::uint8_t>(
        work_ram[kSecondCursorAddress - 0x2000U]);
    const auto third_cursor = std::to_integer<std::uint8_t>(
        work_ram[kThirdCursorAddress - 0x2000U]);
    auto sprite_count = std::to_integer<std::uint8_t>(
        work_ram[kSpriteCountAddress - 0x2000U]);

    auto registers = entry.registers;
    std::uint32_t cycle = 0U;
    RoutineReplacement replacement{};
    // The longest walk is an entry that finds all three queues empty: about
    // thirty instructions and a dozen writes.
    replacement.checkpoints.reserve(40U);
    replacement.work_ram_writes.reserve(16U);

    const auto boundary =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t sample_back) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = cycle - sample_back,
                .master_cycle_offset = cycle,
                .instruction = {kBank, instruction_pc},
                .registers = registers,
            });
        };
    const auto step =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t cycles) {
            cycle += cycles;
            boundary(instruction_pc, resume_pc, 3U);
        };
    const auto write =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t cycles,
            const std::uint16_t address,
            const std::uint8_t value) {
            cycle += cycles;
            replacement.work_ram_writes.push_back({
                .instruction = {kBank, instruction_pc},
                .logical_address = address,
                .value = value,
                .master_cycle_offset = cycle,
            });
            // A store to a VDC port samples two CPU cycles early: its last
            // cycle is the wait the VDC steals after the bus write, not the
            // LastCycle every other instruction ends on.
            boundary(
                instruction_pc,
                resume_pc,
                address <= kVdcDataHigh ? 6U : 3U);
        };
    // Cross a `JSR $40DF`. All four sites are the same instruction with the
    // same target, so the only thing that distinguishes them is the address
    // pushed: the last byte of the call, which is what an `RTS` adds one to.
    //
    // `JSR` is the one instruction in this walk that pushes before it charges
    // a cycle -- Mednafen's case $20 is PUSH, PUSH, ADDCYC -- so both bytes
    // carry the instruction's start offset, which is also the offset the
    // checkpoint in front of it carries. The core holds a write back at a
    // checkpoint that names a different instruction, so the pair lands with
    // the call rather than with whatever preceded it, and an interrupt taken
    // at that earlier boundary finds a stack the call has not touched.
    const auto call =
        [&](const std::uint16_t instruction_pc) {
            const auto pushed =
                static_cast<std::uint16_t>(instruction_pc + 2U);
            replacement.work_ram_writes.push_back({
                .instruction = {kBank, instruction_pc},
                .logical_address = static_cast<std::uint16_t>(
                    0x2100U + registers.stack_pointer),
                .value = static_cast<std::uint8_t>(pushed >> 8U),
                .master_cycle_offset = cycle,
            });
            replacement.work_ram_writes.push_back({
                .instruction = {kBank, instruction_pc},
                .logical_address = static_cast<std::uint16_t>(
                    0x2100U
                    + static_cast<std::uint8_t>(
                        registers.stack_pointer - 1U)),
                .value = static_cast<std::uint8_t>(pushed & 0x00FFU),
                .master_cycle_offset = cycle,
            });
            registers.stack_pointer =
                static_cast<std::uint8_t>(registers.stack_pointer - 2U);
            cycle += kSubroutineCall;
            boundary(instruction_pc, kEmitterPc, 3U);
        };
    // The two identical MAWR setups at 4041 and 40A8: A << 2 becomes the low
    // half of the word address and $78 the high half.
    const auto point_write_address =
        [&](const std::uint16_t base_pc) {
            const auto shift_low = static_cast<std::uint16_t>(base_pc);
            const auto shift_high = static_cast<std::uint16_t>(base_pc + 1U);
            const auto clear_carry = static_cast<std::uint16_t>(base_pc + 2U);
            const auto add_low = static_cast<std::uint16_t>(base_pc + 3U);
            const auto store_low = static_cast<std::uint16_t>(base_pc + 5U);
            const auto clear_accumulator =
                static_cast<std::uint16_t>(base_pc + 8U);
            const auto add_high = static_cast<std::uint16_t>(base_pc + 9U);
            const auto store_high =
                static_cast<std::uint16_t>(base_pc + 11U);
            const auto select_data =
                static_cast<std::uint16_t>(base_pc + 14U);
            const auto store_shadow =
                static_cast<std::uint16_t>(base_pc + 16U);
            const auto store_register =
                static_cast<std::uint16_t>(base_pc + 18U);
            const auto after =
                static_cast<std::uint16_t>(base_pc + 21U);

            registers.accumulator = shift_left(
                registers.status,
                registers.accumulator);
            step(shift_low, shift_high, kImplied);
            registers.accumulator = shift_left(
                registers.status,
                registers.accumulator);
            step(shift_high, clear_carry, kImplied);
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarryFlag));
            step(clear_carry, add_low, kImplied);
            registers.accumulator = add_with_carry(
                registers.status,
                registers.accumulator,
                0x00U);
            step(add_low, store_low, kImmediate);
            write(
                store_low,
                clear_accumulator,
                kPortStore,
                kVdcDataLow,
                registers.accumulator);
            registers.accumulator = 0U;
            step(clear_accumulator, add_high, kImplied);
            registers.accumulator = add_with_carry(
                registers.status,
                registers.accumulator,
                0x78U);
            step(add_high, store_high, kImmediate);
            write(
                store_high,
                select_data,
                kPortStore,
                kVdcDataHigh,
                registers.accumulator);
            registers.accumulator = 0x02U;
            registers.status = set_nz(registers.status, registers.accumulator);
            step(select_data, store_shadow, kImmediate);
            write(
                store_shadow,
                store_register,
                kZeroPage,
                kPortShadowAddress,
                registers.accumulator);
            write(
                store_register,
                after,
                kPortStore,
                kVdcAddress,
                registers.accumulator);
        };

    auto pc = entry.routine.logical_pc;
    for (;;) {
        switch (pc) {
        case 0x4036U:  // LDA #$00 : STA $06 : STA $0000
            registers.accumulator = 0U;
            registers.status = set_nz(registers.status, 0U);
            step(0x4036U, 0x4038U, kImmediate);
            write(0x4038U, 0x403AU, kZeroPage, kPortShadowAddress, 0U);
            write(0x403AU, 0x403DU, kPortStore, kVdcAddress, 0U);
            // LDA $58 : STA $4E
            registers.accumulator = std::to_integer<std::uint8_t>(
                work_ram[kSpriteBaseAddress - 0x2000U]);
            registers.status = set_nz(registers.status, registers.accumulator);
            step(0x403DU, 0x403FU, kZeroPage);
            sprite_count = registers.accumulator;
            write(
                0x403FU,
                0x4041U,
                kZeroPage,
                kSpriteCountAddress,
                sprite_count);
            point_write_address(0x4041U);
            pc = 0x4056U;
            break;

        case 0x4056U:  // LDY $2CBC : BEQ $4068
            registers.y = first_cursor;
            registers.status = set_nz(registers.status, registers.y);
            step(0x4056U, 0x4059U, kAbsolute);
            if (registers.y == 0U) {
                step(0x4059U, 0x4068U, kBranchTaken);
                pc = 0x4068U;
                break;
            }
            step(0x4059U, 0x405BU, kBranchNotTaken);
            // DEY : LDA $2C3A,Y : TAX
            registers.y = static_cast<std::uint8_t>(registers.y - 1U);
            registers.status = set_nz(registers.status, registers.y);
            step(0x405BU, 0x405CU, kImplied);
            registers.accumulator = std::to_integer<std::uint8_t>(
                work_ram[kFirstQueueBase - 0x2000U + registers.y]);
            registers.status = set_nz(registers.status, registers.accumulator);
            step(0x405CU, 0x405FU, kAbsoluteIndexed);
            registers.x = registers.accumulator;
            registers.status = set_nz(registers.status, registers.x);
            step(0x405FU, 0x4060U, kImplied);
            pc = 0x4060U;  // JSR $40DF
            break;

        case 0x4063U:  // DEC $2CBC : BRA $4056
            first_cursor = static_cast<std::uint8_t>(first_cursor - 1U);
            registers.status = set_nz(registers.status, first_cursor);
            write(
                0x4063U,
                0x4066U,
                kAbsoluteReadModifyWrite,
                kFirstCursorAddress,
                first_cursor);
            step(0x4066U, 0x4056U, kBranchTaken);
            pc = 0x4056U;
            break;

        case 0x4068U:  // LDY $2CBA : BEQ $407A
            registers.y = second_cursor;
            registers.status = set_nz(registers.status, registers.y);
            step(0x4068U, 0x406BU, kAbsolute);
            if (registers.y == 0U) {
                step(0x406BU, 0x407AU, kBranchTaken);
                pc = 0x407AU;
                break;
            }
            step(0x406BU, 0x406DU, kBranchNotTaken);
            // DEY : LDA $2BBA,Y : TAX
            registers.y = static_cast<std::uint8_t>(registers.y - 1U);
            registers.status = set_nz(registers.status, registers.y);
            step(0x406DU, 0x406EU, kImplied);
            registers.accumulator = std::to_integer<std::uint8_t>(
                work_ram[kSharedQueueBase - 0x2000U + registers.y]);
            registers.status = set_nz(registers.status, registers.accumulator);
            step(0x406EU, 0x4071U, kAbsoluteIndexed);
            registers.x = registers.accumulator;
            registers.status = set_nz(registers.status, registers.x);
            step(0x4071U, 0x4072U, kImplied);
            pc = 0x4072U;  // JSR $40DF
            break;

        case 0x4075U:  // DEC $2CBA : BRA $4068
            second_cursor = static_cast<std::uint8_t>(second_cursor - 1U);
            registers.status = set_nz(registers.status, second_cursor);
            write(
                0x4075U,
                0x4078U,
                kAbsoluteReadModifyWrite,
                kSecondCursorAddress,
                second_cursor);
            step(0x4078U, 0x4068U, kBranchTaken);
            pc = 0x4068U;
            break;

        case 0x407AU: {  // LDX #$00 : LDA $3080,X : BPL/AND #$20/BNE
            registers.x = 0U;
            registers.status = set_nz(registers.status, 0U);
            step(0x407AU, 0x407CU, kImmediate);
            const auto attributes = std::to_integer<std::uint8_t>(
                work_ram[kAttributesBase - 0x2000U]);
            registers.accumulator = attributes;
            registers.status = set_nz(registers.status, attributes);
            step(0x407CU, 0x407FU, kAbsoluteIndexed);
            if ((attributes & 0x80U) == 0U) {
                step(0x407FU, 0x4088U, kBranchTaken);
                pc = 0x4088U;
                break;
            }
            step(0x407FU, 0x4081U, kBranchNotTaken);
            registers.accumulator =
                static_cast<std::uint8_t>(registers.accumulator & 0x20U);
            registers.status = set_nz(registers.status, registers.accumulator);
            step(0x4081U, 0x4083U, kImmediate);
            if (registers.accumulator != 0U) {
                step(0x4083U, 0x4088U, kBranchTaken);
                pc = 0x4088U;
                break;
            }
            step(0x4083U, 0x4085U, kBranchNotTaken);
            pc = 0x4085U;  // JSR $40DF
            break;
        }

        case 0x4088U:  // LDY #$7F
            registers.y = 0x7FU;
            registers.status = set_nz(registers.status, registers.y);
            step(0x4088U, 0x408AU, kImmediate);
            pc = 0x408AU;
            break;

        case 0x408AU:  // CPY $2CBB : BEQ $409B
            registers.status =
                compare(registers.status, registers.y, third_cursor);
            step(0x408AU, 0x408DU, kAbsolute);
            if (registers.y == third_cursor) {
                step(0x408DU, 0x409BU, kBranchTaken);
                pc = 0x409BU;
                break;
            }
            step(0x408DU, 0x408FU, kBranchNotTaken);
            // LDA $2BBA,Y : TAX : PHY
            registers.accumulator = std::to_integer<std::uint8_t>(
                work_ram[kSharedQueueBase - 0x2000U + registers.y]);
            registers.status = set_nz(registers.status, registers.accumulator);
            step(0x408FU, 0x4092U, kAbsoluteIndexed);
            registers.x = registers.accumulator;
            registers.status = set_nz(registers.status, registers.x);
            step(0x4092U, 0x4093U, kImplied);
            // PHY writes at $2100|S and then drops S; unlike JSR it charges
            // its cycles first, so the write lands at the instruction's end.
            {
                const auto slot = static_cast<std::uint16_t>(
                    0x2100U + registers.stack_pointer);
                registers.stack_pointer = static_cast<std::uint8_t>(
                    registers.stack_pointer - 1U);
                write(0x4093U, 0x4094U, kPush, slot, registers.y);
            }
            pc = 0x4094U;  // JSR $40DF
            break;

        case 0x4097U:  // PLY : DEY : BRA $408A
            registers.stack_pointer =
                static_cast<std::uint8_t>(registers.stack_pointer + 1U);
            registers.y = std::to_integer<std::uint8_t>(
                work_ram[0x0100U + registers.stack_pointer]);
            registers.status = set_nz(registers.status, registers.y);
            step(0x4097U, 0x4098U, kPull);
            registers.y = static_cast<std::uint8_t>(registers.y - 1U);
            registers.status = set_nz(registers.status, registers.y);
            step(0x4098U, 0x4099U, kImplied);
            step(0x4099U, 0x408AU, kBranchTaken);
            pc = 0x408AU;
            break;

        case 0x409BU:  // LDA #$00 : STA $06 : STA $0000 : LDA $4E : CMP #$40
            registers.accumulator = 0U;
            registers.status = set_nz(registers.status, 0U);
            step(0x409BU, 0x409DU, kImmediate);
            write(0x409DU, 0x409FU, kZeroPage, kPortShadowAddress, 0U);
            write(0x409FU, 0x40A2U, kPortStore, kVdcAddress, 0U);
            registers.accumulator = sprite_count;
            registers.status = set_nz(registers.status, sprite_count);
            step(0x40A2U, 0x40A4U, kZeroPage);
            registers.status =
                compare(registers.status, registers.accumulator, 0x40U);
            step(0x40A4U, kSpriteCountGuardPc, kImmediate);
            // A count of $40 or more skips the tail clear entirely. The route
            // never produces one, so the branch and everything past it stay
            // emulated rather than run an unproven transcript.
            if (sprite_count >= 0x40U) {
                pc = kSpriteCountGuardPc;
                break;
            }
            step(kSpriteCountGuardPc, 0x40A8U, kBranchNotTaken);
            point_write_address(0x40A8U);
            // SEC : LDA #$40 : SBC $4E : TAY
            registers.status =
                static_cast<std::uint8_t>(registers.status | kCarryFlag);
            step(0x40BDU, 0x40BEU, kImplied);
            registers.accumulator = 0x40U;
            registers.status = set_nz(registers.status, registers.accumulator);
            step(0x40BEU, 0x40C0U, kImmediate);
            registers.accumulator = subtract_with_borrow(
                registers.status,
                registers.accumulator,
                sprite_count);
            step(0x40C0U, 0x40C2U, kZeroPage);
            registers.y = registers.accumulator;
            registers.status = set_nz(registers.status, registers.y);
            step(0x40C2U, kTailClearPc, kImplied);
            pc = kTailClearPc;
            break;

        default:
            // Unreachable: every pc the walk assigns is a case above.
            return std::nullopt;
        }

        // A `JSR` ends the walk, but it is made rather than left standing: the
        // transcript pushes the return and hands the machine the emitter's
        // first instruction, where the emitter's own transcript is waiting.
        if (pc == 0x4060U || pc == 0x4072U || pc == 0x4085U || pc == 0x4094U) {
            call(pc);
            pc = kEmitterPc;
            break;
        }
        if (pc == kTailClearPc || pc == kSpriteCountGuardPc) {
            break;
        }
    }

    if (replacement.checkpoints.empty()
        || registers.program_counter != pc) {
        return std::nullopt;
    }

    replacement.return_instruction = {
        .physical_bank = kBank,
        .logical_pc = pc,
    };
    replacement.output_registers = registers;
    replacement.body_master_cycles = cycle;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_sat_tail_clear(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The tail of the same SAT submission `08:40DF` fills the head of. Once
    // every queued actor has been emitted, `$4E` holds the sprite count and
    // the sweep zeroes the rest of the 64-entry table, four words per
    // iteration, straight through the VDC data ports. Y is the entry count
    // left to clear, already computed as $40 - $4E by the caller.
    //
    //   40C3 STZ $0002 18   40C6 STZ $0003 18
    //   40C9 STZ $0002 18   40CC STZ $0003 18
    //   40CF STZ $0002 18   40D2 STZ $0003 18
    //   40D5 STZ $0002 18   40D8 STZ $0003 18
    //   40DB DEY        6   40DC BNE $40C3 6/12
    //   40DE RTS       21
    //
    // There is no snapshot to classify: the loop reads no memory, writes the
    // same constant every time, and is driven entirely by Y. What it does
    // need is a budget, because eight writes an iteration reach the core's
    // 512-write transaction cap before Y does. A run that spends its budget
    // resumes at the loop head with Y standing, which is a real instruction
    // boundary, so the next arrival simply continues.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x08U,
        .logical_pc = 0x40C3U,
    };
    constexpr std::uint16_t kReturnPc = 0x40DEU;
    constexpr std::uint16_t kDecrementPc = 0x40DBU;
    constexpr std::uint16_t kLoopBranchPc = 0x40DCU;
    constexpr std::uint16_t kVdcDataLow = 0x0002U;
    constexpr std::uint16_t kVdcDataHigh = 0x0003U;

    constexpr std::uint32_t kImplied = 6U;
    constexpr std::uint32_t kPortStore = 18U;
    constexpr std::uint32_t kBranchTaken = 12U;
    constexpr std::uint32_t kBranchNotTaken = 6U;

    // Eight writes and ten boundaries an iteration, against the core's 512
    // and 2,048. Forty-eight leaves margin in both and covers every observed
    // call in one transaction: Y is $40 - $4E and averages 33.
    constexpr std::uint16_t kMaximumIterations = 48U;

    // The loop itself reads nothing, but the return it ends on reads the
    // caller's frame out of the stack page.
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    if ((entry.registers.status & kUnsupportedFlags) != 0U) {
        return std::nullopt;
    }
    // The VDC data ports are only addressable while the hardware page is
    // mapped at segment 0, which is also what the core's write validator
    // requires before it will accept them.
    if (entry.registers.mpr[0] != 0xFFU) {
        return std::nullopt;
    }

    // DEY wraps, so a zero Y at the loop head means a full 256 iterations.
    const std::uint16_t remaining =
        entry.registers.y == 0U ? 256U : entry.registers.y;
    const std::uint16_t iterations =
        remaining < kMaximumIterations ? remaining : kMaximumIterations;

    auto registers = entry.registers;
    std::uint32_t cycle = 0U;
    RoutineReplacement replacement{};
    replacement.work_ram_writes.reserve(
        static_cast<std::size_t>(iterations) * 8U);
    replacement.checkpoints.reserve(
        static_cast<std::size_t>(iterations) * 10U);

    const auto boundary =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t sample_back) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = cycle - sample_back,
                .master_cycle_offset = cycle,
                .instruction = {kRoutine.physical_bank, instruction_pc},
                .registers = registers,
            });
        };
    // A store to a VDC data port samples two CPU cycles early: its last cycle
    // is the wait the VDC steals after the bus write, not the LastCycle every
    // other instruction ends on.
    const auto store =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t address) {
            cycle += kPortStore;
            replacement.work_ram_writes.push_back({
                .instruction = {kRoutine.physical_bank, instruction_pc},
                .logical_address = address,
                .value = 0U,
                .master_cycle_offset = cycle,
            });
            boundary(instruction_pc, resume_pc, 6U);
        };

    for (std::uint16_t walked = 0U; walked < iterations; ++walked) {
        // STZ touches no flag, so the whole word quartet leaves the status
        // register exactly as the previous branch left it.
        for (std::uint16_t index = 0U; index < 4U; ++index) {
            const auto low_pc =
                static_cast<std::uint16_t>(0x40C3U + index * 6U);
            const auto high_pc = static_cast<std::uint16_t>(low_pc + 3U);
            store(low_pc, high_pc, kVdcDataLow);
            store(
                high_pc,
                index == 3U
                    ? kDecrementPc
                    : static_cast<std::uint16_t>(high_pc + 3U),
                kVdcDataHigh);
        }

        registers.y = static_cast<std::uint8_t>(registers.y - 1U);
        registers.status = set_nz(registers.status, registers.y);
        cycle += kImplied;
        boundary(kDecrementPc, kLoopBranchPc, 3U);

        const bool exhausted = registers.y == 0U;
        cycle += exhausted ? kBranchNotTaken : kBranchTaken;
        boundary(
            kLoopBranchPc,
            exhausted ? kReturnPc : kRoutine.logical_pc,
            3U);
        if (exhausted) {
            break;
        }
    }

    if (replacement.checkpoints.empty()) {
        return std::nullopt;
    }

    replacement.return_instruction = {
        .physical_bank = kRoutine.physical_bank,
        .logical_pc = registers.program_counter,
    };
    replacement.output_registers = registers;
    replacement.body_master_cycles = cycle;

    // Only the arm that ran Y down to zero ends on the return. The other one
    // spent its write budget and stops at the loop head, which is an ordinary
    // instruction boundary the next arrival continues from -- 4,334 arrivals
    // against 3,492 calls, and only the calls have a return to absorb.
    if (registers.program_counter == kReturnPc) {
        absorb_return(
            replacement,
            work_ram,
            entry.registers.stack_pointer,
            {kRoutine.physical_bank, kReturnPc},
            cycle,
            registers);
    }
    return replacement;
}

} // namespace devils_crush::app::detail
