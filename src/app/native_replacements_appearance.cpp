// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_replacements_internal.hpp"
#include <array>
namespace devils_crush::app::detail {
using namespace emulation;
[[nodiscard]] std::optional<RoutineReplacement>
replace_devil_shooter_appearance_submission_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The Devil Shooter hands its appearance byte to a shared palette-block
    // submission helper. Keep the asset resolver as a separately observed
    // child: this prefix reproduces PHA and the pointer-reference setup, then
    // crosses the original JSR through bank 01's native jump-table row.
    constexpr RoutineKey kRoutine{0x02U, 0x5475U};
    constexpr std::uint16_t kStackBase = 0x2100U;
    // Neither the actor slot nor the caller's stack depth participates in
    // this helper. Preserve both, wrapping each stack access independently.
    if (entry.routine != kRoutine || entry.registers.mpr[1] != 0xF8U
        || entry.registers.mpr[2] != 0x02U
        || entry.registers.mpr[6] != 0x01U
        || work_ram.size() < 0x2000U
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };

    RoutineReplacement replacement{};
    auto checkpoint = entry.registers;
    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = cycle - 3U,
                .master_cycle_offset = cycle,
                .instruction = {0x02U, instruction_pc},
                .registers = registers,
            });
        };
    const auto add_write =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t address,
            const std::uint8_t value,
            const std::uint32_t cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x02U, instruction_pc},
                .logical_address = address,
                .value = value,
                .master_cycle_offset = cycle,
            });
        };

    add_write(
        0x5475U,
        static_cast<std::uint16_t>(kStackBase + checkpoint.stack_pointer),
        checkpoint.accumulator,
        9U);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 1U);
    append_checkpoint(0x5475U, 0x5476U, 9U, checkpoint);

    checkpoint.accumulator = 0x10U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x5476U, 0x5478U, 15U, checkpoint);
    add_write(0x5478U, 0x202FU, checkpoint.accumulator, 27U);
    append_checkpoint(0x5478U, 0x547AU, 27U, checkpoint);

    checkpoint.accumulator = 0x12U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x547AU, 0x547CU, 33U, checkpoint);
    add_write(0x547CU, 0x2030U, checkpoint.accumulator, 45U);
    append_checkpoint(0x547CU, 0x547EU, 45U, checkpoint);

    // JSR stores its return bytes before charging its 21 master cycles.
    add_write(
        0x547EU,
        static_cast<std::uint16_t>(kStackBase + checkpoint.stack_pointer),
        0x54U,
        45U);
    add_write(
        0x547EU,
        static_cast<std::uint16_t>(
            kStackBase
            + static_cast<std::uint8_t>(checkpoint.stack_pointer - 1U)),
        0x80U,
        45U);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 2U);
    append_checkpoint(0x547EU, 0xC042U, 66U, checkpoint);
    checkpoint.program_counter = 0xC042U;
    replacement.return_instruction = {0x01U, 0xC042U};
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = 66U;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_devil_shooter_appearance_submission_tail(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The resolver returns with the original appearance still beneath its JSR
    // frame. Pull it, multiply it by 32 into $2E:A, add that displacement to
    // the resolved $2F/$30 pointer, and cross the shared palette staging tail.
    constexpr RoutineKey kRoutine{0x02U, 0x5481U};
    constexpr std::uint16_t kStackBase = 0x2100U;
    if (entry.routine != kRoutine || entry.registers.mpr[1] != 0xF8U
        || entry.registers.mpr[2] != 0x02U
        || entry.registers.mpr[6] != 0x01U
        || work_ram.size() < 0x2000U
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto add_with_carry =
        [&set_nz](
            std::uint8_t& status,
            const std::uint8_t left,
            const std::uint8_t right) {
            const auto sum = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(left)
                + static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(status & 0x01U));
            const auto result = static_cast<std::uint8_t>(sum);
            const bool overflow =
                ((left ^ result) & (right ^ result) & 0x80U) != 0U;
            status = static_cast<std::uint8_t>(status & 0x3CU);
            if (sum > 0xFFU) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if (overflow) {
                status = static_cast<std::uint8_t>(status | 0x40U);
            }
            status = set_nz(status, result);
            return result;
        };

    RoutineReplacement replacement{};
    auto checkpoint = entry.registers;
    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = cycle - 3U,
                .master_cycle_offset = cycle,
                .instruction = {0x02U, instruction_pc},
                .registers = registers,
            });
        };
    const auto add_write =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t address,
            const std::uint8_t value,
            const std::uint32_t cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x02U, instruction_pc},
                .logical_address = address,
                .value = value,
                .master_cycle_offset = cycle,
            });
        };

    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 1U);
    checkpoint.accumulator = std::to_integer<std::uint8_t>(
        work_ram[(kStackBase + checkpoint.stack_pointer) & 0x1FFFU]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x5481U, 0x5482U, 12U, checkpoint);

    std::uint8_t high = 0U;
    add_write(0x5482U, 0x202EU, high, 24U);
    append_checkpoint(0x5482U, 0x5484U, 24U, checkpoint);

    constexpr std::array<std::uint16_t, 5U> kShiftPcs{
        0x5484U, 0x5487U, 0x548AU, 0x548DU, 0x5490U};
    constexpr std::array<std::uint16_t, 5U> kRotatePcs{
        0x5485U, 0x5488U, 0x548BU, 0x548EU, 0x5491U};
    constexpr std::array<std::uint16_t, 5U> kRotateResumePcs{
        0x5487U, 0x548AU, 0x548DU, 0x5490U, 0x5493U};
    std::uint32_t cycle = 24U;
    for (std::size_t index = 0U; index < kShiftPcs.size(); ++index) {
        const auto accumulator_carry = static_cast<std::uint8_t>(
            (checkpoint.accumulator >> 7U) & 0x01U);
        checkpoint.accumulator = static_cast<std::uint8_t>(
            checkpoint.accumulator << 1U);
        checkpoint.status = set_nz(
            static_cast<std::uint8_t>(
                (checkpoint.status & 0xFEU) | accumulator_carry),
            checkpoint.accumulator);
        cycle += 6U;
        append_checkpoint(
            kShiftPcs[index], kRotatePcs[index], cycle, checkpoint);

        const auto high_carry = static_cast<std::uint8_t>(
            (high >> 7U) & 0x01U);
        high = static_cast<std::uint8_t>(
            static_cast<std::uint8_t>(high << 1U)
            | static_cast<std::uint8_t>(checkpoint.status & 0x01U));
        checkpoint.status = set_nz(
            static_cast<std::uint8_t>(
                (checkpoint.status & 0xFEU) | high_carry),
            high);
        cycle += 18U;
        add_write(kRotatePcs[index], 0x202EU, high, cycle);
        append_checkpoint(
            kRotatePcs[index], kRotateResumePcs[index], cycle, checkpoint);
    }

    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status & 0xFEU);
    append_checkpoint(0x5493U, 0x5494U, 150U, checkpoint);
    const auto pointer_low = std::to_integer<std::uint8_t>(work_ram[0x2FU]);
    checkpoint.accumulator = add_with_carry(
        checkpoint.status, checkpoint.accumulator, pointer_low);
    append_checkpoint(0x5494U, 0x5496U, 162U, checkpoint);
    add_write(0x5496U, 0x202FU, checkpoint.accumulator, 174U);
    append_checkpoint(0x5496U, 0x5498U, 174U, checkpoint);

    checkpoint.accumulator = high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x5498U, 0x549AU, 186U, checkpoint);
    const auto pointer_high = std::to_integer<std::uint8_t>(work_ram[0x30U]);
    checkpoint.accumulator = add_with_carry(
        checkpoint.status, checkpoint.accumulator, pointer_high);
    append_checkpoint(0x549AU, 0x549CU, 198U, checkpoint);
    add_write(0x549CU, 0x2030U, checkpoint.accumulator, 210U);
    append_checkpoint(0x549CU, 0x549EU, 210U, checkpoint);

    checkpoint.accumulator = 0x10U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x549EU, 0x54A0U, 216U, checkpoint);
    append_checkpoint(0x54A0U, 0xC05DU, 228U, checkpoint);
    checkpoint.program_counter = 0xC05DU;
    replacement.return_instruction = {0x01U, 0xC05DU};
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = 228U;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_devil_shooter_release_countdown_tail(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    // This is the release delay reached after the Shooter's launch jump.  A
    // nonzero countdown simply returns.  At zero it refreshes the actor's
    // appearance, then keeps the original $5475 submission call as a child;
    // that child has a distinct JSR stack shape from the direct launch path.
    constexpr RoutineKey kRoutine{0x02U, 0x51AFU};
    if (entry.routine != kRoutine || entry.registers.mpr[2] != 0x02U
        || entry.registers.mpr[1] != 0xF8U
        || (entry.registers.status & 0x28U) != 0U
        || work_ram.size() < 0x2000U || rom.size() <= 0x52D5U) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & ~0x82U);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        }
        if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto compare = [&set_nz](std::uint8_t status,
                                   const std::uint8_t left,
                                   const std::uint8_t right) {
        const auto difference = static_cast<std::uint8_t>(left - right);
        status = set_nz(static_cast<std::uint8_t>(status & ~0x01U), difference);
        if (left >= right) {
            status = static_cast<std::uint8_t>(status | 0x01U);
        }
        return status;
    };
    const auto subtract = [&set_nz](std::uint8_t status,
                                    const std::uint8_t left,
                                    const std::uint8_t right) {
        const auto result = static_cast<std::uint8_t>(left - right);
        status = static_cast<std::uint8_t>(status & ~0x43U);
        if (left >= right) {
            status = static_cast<std::uint8_t>(status | 0x01U);
        }
        if (((left ^ right) & (left ^ result) & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x40U);
        }
        return std::pair{result, set_nz(status, result)};
    };

    const auto timer_address = static_cast<std::uint16_t>(
        0x3A00U + entry.registers.x);
    const auto state_address = static_cast<std::uint16_t>(
        0x3A80U + entry.registers.x);
    // 51C8 is absolute STA, not STA abs,X: this is the shared appearance.
    constexpr std::uint16_t appearance_address = 0x3980U;
    const auto read = [work_ram](const std::uint16_t logical_address) {
        return std::to_integer<std::uint8_t>(
            work_ram[logical_address & 0x1FFFU]);
    };

    RoutineReplacement replacement{};
    auto registers = entry.registers;
    std::uint32_t cycle = 0U;
    const auto checkpoint = [&replacement, &registers, &cycle](
                                const std::uint16_t instruction_pc,
                                const std::uint16_t resume_pc,
                                const std::uint32_t instruction_cycles) {
        cycle += instruction_cycles;
        registers.program_counter = resume_pc;
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = cycle - 3U,
            .master_cycle_offset = cycle,
            .instruction = {0x02U, instruction_pc},
            .registers = registers,
        });
    };
    const auto write = [&replacement, &cycle](
                           const std::uint16_t instruction_pc,
                           const std::uint16_t logical_address,
                           const std::uint8_t value) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x02U, instruction_pc},
            .logical_address = logical_address,
            .value = value,
            .master_cycle_offset = cycle,
        });
    };

    auto countdown = static_cast<std::uint8_t>(read(timer_address) - 1U);
    registers.status = set_nz(registers.status, countdown);
    checkpoint(0x51AFU, 0x51B2U, 21U);
    write(0x51AFU, timer_address, countdown);
    if (countdown != 0U) {
        checkpoint(0x51B2U, 0x51D5U, 12U);
        replacement.return_instruction = {0x02U, 0x51D5U};
        replacement.output_registers = registers;
        replacement.body_master_cycles = cycle;
        return replacement;
    }

    checkpoint(0x51B2U, 0x51B4U, 6U);
    registers.accumulator = 0x05U;
    registers.status = set_nz(registers.status, registers.accumulator);
    checkpoint(0x51B4U, 0x51B6U, 6U);
    checkpoint(0x51B6U, 0x51B9U, 15U);
    write(0x51B6U, timer_address, registers.accumulator);

    auto state = static_cast<std::uint8_t>(read(state_address) + 1U);
    registers.status = set_nz(registers.status, state);
    checkpoint(0x51B9U, 0x51BCU, 21U);
    write(0x51B9U, state_address, state);
    registers.accumulator = state;
    registers.status = set_nz(registers.status, registers.accumulator);
    checkpoint(0x51BCU, 0x51BFU, 15U);
    registers.status = compare(registers.status, registers.accumulator, 0x0CU);
    checkpoint(0x51BFU, 0x51C1U, 6U);
    if (registers.accumulator < 0x0CU) {
        checkpoint(0x51C1U, 0x51C8U, 12U);
    } else {
        checkpoint(0x51C1U, 0x51C3U, 6U);
        registers.accumulator = 0x08U;
        registers.status = set_nz(registers.status, registers.accumulator);
        checkpoint(0x51C3U, 0x51C5U, 6U);
        state = registers.accumulator;
        checkpoint(0x51C5U, 0x51C8U, 15U);
        write(0x51C5U, state_address, state);
    }

    checkpoint(0x51C8U, 0x51CBU, 15U);
    write(0x51C8U, appearance_address, registers.accumulator);
    registers.status = static_cast<std::uint8_t>(registers.status | 0x01U);
    checkpoint(0x51CBU, 0x51CCU, 6U);
    const auto [appearance_index, appearance_status] = subtract(
        registers.status, registers.accumulator, 0x08U);
    registers.accumulator = appearance_index;
    registers.status = appearance_status;
    checkpoint(0x51CCU, 0x51CEU, 6U);
    registers.y = registers.accumulator;
    registers.status = set_nz(registers.status, registers.y);
    checkpoint(0x51CEU, 0x51CFU, 6U);
    registers.accumulator = std::to_integer<std::uint8_t>(
        rom[0x51D6U + registers.y]);
    registers.status = set_nz(registers.status, registers.accumulator);
    checkpoint(0x51CFU, 0x51D2U, 15U);

    // Retain the JSR as a core boundary. The native submission child owns
    // its own entry and resume transcript; this body never absorbs it.
    replacement.return_instruction = {0x02U, 0x51D2U};
    replacement.output_registers = registers;
    replacement.body_master_cycles = cycle;
    return replacement;
}

} // namespace devils_crush::app::detail
