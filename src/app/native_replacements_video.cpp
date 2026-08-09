// SPDX-License-Identifier: GPL-3.0-or-later

#include "native_replacements_internal.hpp"
#include "devils_crush/game_logic/sprite.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

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

    replacement.output_registers = registers;
    replacement.body_master_cycles = cycle;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_sat_tail_clear(
    const RoutineEntry& entry)
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
    //   40DE RTS            (stays emulated)
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

    if (entry.routine != kRoutine) {
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
    return replacement;
}

} // namespace devils_crush::app::detail
