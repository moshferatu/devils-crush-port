// SPDX-License-Identifier: GPL-3.0-or-later

#include "native_replacements_internal.hpp"

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

} // namespace

std::optional<RoutineReplacement> replace_selection_actor_controller(
    const RoutineEntry& entry, const std::span<const std::byte> work_ram)
{
    // The scheduled input callback and its old/new glyph updates are separate
    // transactions. End at each original JSR or tail JMP: the VDC writer and
    // its possibly interrupted return retain their existing ownership.
    if (entry.routine.physical_bank != 0x08U || work_ram.size() < 0x2000U
        || entry.registers.mpr[1] != 0xF8U || entry.registers.mpr[2] != 0x08U
        || (entry.registers.status & kUnsupportedFlags) != 0U) {
        return std::nullopt;
    }
    RoutineReplacement replacement{};
    auto registers = entry.registers;
    auto pc = entry.routine.logical_pc;
    std::uint32_t cycle = 0U;
    bool finished = false;
    const auto read = [&](const std::uint16_t address) {
        for (auto it = replacement.work_ram_writes.rbegin();
             it != replacement.work_ram_writes.rend(); ++it) {
            if (it->logical_address == address) { return it->value; }
        }
        return std::to_integer<std::uint8_t>(work_ram[address - 0x2000U]);
    };
    const auto step = [&](const std::uint16_t next, const std::uint32_t cost) {
        cycle += cost;
        registers.program_counter = next;
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = cycle - 3U,
            .master_cycle_offset = cycle,
            .instruction = {0x08U, pc},
            .registers = registers,
        });
        pc = next;
    };
    const auto load = [&](const std::uint8_t value, const std::uint16_t next,
                          const std::uint32_t cost) {
        registers.accumulator = value;
        registers.status = set_nz(registers.status, value);
        step(next, cost);
    };
    const auto write = [&](const std::uint16_t address, const std::uint8_t value,
                           const std::uint32_t offset) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x08U, pc}, .logical_address = address,
            .value = value, .master_cycle_offset = offset,
        });
    };
    const auto store = [&](const std::uint16_t address, const std::uint8_t value,
                           const std::uint16_t next, const std::uint32_t cost) {
        write(address, value, cycle + cost);
        step(next, cost);
    };
    const auto indexed = [&](const std::uint16_t base) {
        return static_cast<std::uint16_t>(base + registers.x);
    };
    const auto branch = [&](const bool taken, const std::uint16_t target) {
        step(taken ? target : static_cast<std::uint16_t>(pc + 2U), taken ? 12U : 6U);
    };
    const auto call = [&](const std::uint16_t target) {
        const auto address = static_cast<std::uint16_t>(pc + 2U);
        write(static_cast<std::uint16_t>(0x2100U + registers.stack_pointer),
              static_cast<std::uint8_t>(address >> 8U), cycle);
        --registers.stack_pointer;
        write(static_cast<std::uint16_t>(0x2100U + registers.stack_pointer),
              static_cast<std::uint8_t>(address), cycle);
        --registers.stack_pointer;
        step(target, 21U);
        finished = true;
    };
    while (!finished && replacement.checkpoints.size() < 64U) {
        switch (pc) {
        case 0x5A3EU: load(read(0x205DU), 0x5A40U, 12U); break;
        case 0x5A40U: store(indexed(0x3A80U), registers.accumulator, 0x5A43U, 15U); break;
        case 0x5A43U: load(read(0x2018U), 0x5A45U, 12U); break;
        case 0x5A45U: load(registers.accumulator & 0x44U, 0x5A47U, 6U); break;
        case 0x5A47U: branch((registers.status & kZeroFlag) != 0U, 0x5A56U); break;
        case 0x5A49U: case 0x5A5CU: {
            const auto value = static_cast<std::uint8_t>(read(0x205DU)
                + (pc == 0x5A49U ? 1 : -1));
            registers.status = set_nz(registers.status, value);
            store(0x205DU, value, static_cast<std::uint16_t>(pc + 2U), 18U);
            break;
        }
        case 0x5A4BU: load(read(0x205DU), 0x5A4DU, 12U); break;
        case 0x5A4DU: {
            const auto operand = read(indexed(0x3E00U));
            registers.status = static_cast<std::uint8_t>(
                (registers.status & ~kCarryFlag)
                | (registers.accumulator >= operand ? kCarryFlag : 0U));
            registers.status = set_nz(registers.status,
                static_cast<std::uint8_t>(registers.accumulator - operand));
            step(0x5A50U, 15U); break;
        }
        case 0x5A50U: branch((registers.status & kCarryFlag) == 0U, 0x5A66U); break;
        case 0x5A52U: store(0x205DU, 0U, 0x5A54U, 12U); break;
        case 0x5A54U: step(0x5A66U, 12U); break;
        case 0x5A56U: load(read(0x2018U), 0x5A58U, 12U); break;
        case 0x5A58U: load(registers.accumulator & 0x10U, 0x5A5AU, 6U); break;
        case 0x5A5AU: branch((registers.status & kZeroFlag) != 0U, 0x5A69U); break;
        case 0x5A5EU: branch((registers.status & kNegativeFlag) == 0U, 0x5A66U); break;
        case 0x5A60U: load(read(indexed(0x3E00U)), 0x5A63U, 15U); break;
        case 0x5A63U: load(static_cast<std::uint8_t>(registers.accumulator - 1U), 0x5A64U, 6U); break;
        case 0x5A64U: store(0x205DU, registers.accumulator, 0x5A66U, 12U); break;
        case 0x5A66U: call(0x5A6AU); break;
        case 0x5A69U: finished = true; break; // Original RTS remains core-owned.
        case 0x5A6AU: load(0x5BU, 0x5A6CU, 6U); break;
        case 0x5A6CU: store(0x2026U, registers.accumulator, 0x5A6EU, 12U); break;
        case 0x5A6EU: load(read(indexed(0x3A80U)), 0x5A71U, 15U); break;
        case 0x5A71U: case 0x5A80U:
            registers.y = read(indexed(0x3E80U));
            registers.status = set_nz(registers.status, registers.y);
            step(static_cast<std::uint16_t>(pc + 3U), 15U); break;
        case 0x5A74U: branch((registers.status & kZeroFlag) != 0U, 0x5A77U); break;
        case 0x5A76U: case 0x5A85U:
            registers.accumulator = shift_left(registers.status, registers.accumulator);
            step(static_cast<std::uint16_t>(pc + 1U), 6U); break;
        case 0x5A77U: call(0x5A86U); break;
        case 0x5A7AU: load(0x65U, 0x5A7CU, 6U); break;
        case 0x5A7CU: store(0x2026U, registers.accumulator, 0x5A7EU, 12U); break;
        case 0x5A7EU: load(read(0x205DU), 0x5A80U, 12U); break;
        case 0x5A83U: branch((registers.status & kZeroFlag) != 0U, 0x5A86U); break;
        case 0x5A86U: case 0x5A9BU:
            registers.status &= static_cast<std::uint8_t>(~kCarryFlag);
            step(static_cast<std::uint16_t>(pc + 1U), 6U); break;
        case 0x5A87U: case 0x5A9CU: case 0x5AA2U: {
            const auto address = pc == 0x5A87U ? indexed(0x3300U)
                : pc == 0x5A9CU ? indexed(0x3180U) : std::uint16_t{0x202EU};
            registers.accumulator = add_with_carry(registers.status, registers.accumulator, read(address));
            step(static_cast<std::uint16_t>(pc + (pc == 0x5AA2U ? 2U : 3U)),
                 pc == 0x5AA2U ? 12U : 15U); break;
        }
        case 0x5A8AU: store(0x202EU, 0U, 0x5A8CU, 12U); break;
        case 0x5A8CU: case 0x5A8FU: case 0x5A92U: case 0x5A95U: case 0x5A98U:
            registers.accumulator = shift_left(registers.status, registers.accumulator);
            step(static_cast<std::uint16_t>(pc + 1U), 6U); break;
        case 0x5A8DU: case 0x5A90U: case 0x5A93U: case 0x5A96U: case 0x5A99U: {
            const auto old = read(0x202EU);
            const auto value = static_cast<std::uint8_t>((old << 1U) | (registers.status & kCarryFlag));
            registers.status = set_nz(static_cast<std::uint8_t>(
                (registers.status & ~kCarryFlag) | (old >> 7U)), value);
            store(0x202EU, value, static_cast<std::uint16_t>(pc + 2U), 18U); break;
        }
        case 0x5A9FU: store(0x202DU, registers.accumulator, 0x5AA1U, 12U); break;
        case 0x5AA1U: registers.accumulator = 0U; step(0x5AA2U, 6U); break; // CLA preserves N/Z.
        case 0x5AA4U: store(0x202EU, registers.accumulator, 0x5AA6U, 12U); break;
        case 0x5AA6U: load(read(0x2026U), 0x5AA8U, 12U); break;
        case 0x5AA8U: step(0x5B01U, 12U); finished = true; break;
        default: return std::nullopt;
        }
    }
    if (!finished || replacement.checkpoints.empty()) { return std::nullopt; }
    replacement.return_instruction = {0x08U, pc};
    replacement.output_registers = registers;
    replacement.body_master_cycles = cycle;
    return replacement;
}

} // namespace devils_crush::app::detail
