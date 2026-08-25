// SPDX-License-Identifier: GPL-3.0-or-later

#include "native_replacements_internal.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace devils_crush::app::detail {
namespace {

using emulation::CpuRegisters;
using emulation::RoutineEntry;
using emulation::RoutineReplacement;

constexpr std::uint8_t kBank = 0x05U;
constexpr std::uint16_t kEntry = 0x6D27U;
constexpr std::uint16_t kReturn = 0x6D7EU;
constexpr std::uint8_t kCarry = 0x01U;
constexpr std::uint8_t kZero = 0x02U;
constexpr std::uint8_t kDecimal = 0x08U;
constexpr std::uint8_t kMemory = 0x20U;
constexpr std::uint8_t kOverflow = 0x40U;
constexpr std::uint8_t kNegative = 0x80U;
constexpr std::uint16_t kRamBase = 0x2000U;
constexpr std::uint16_t kRamEnd = 0x4000U;

[[nodiscard]] std::uint8_t set_nz(
    const std::uint8_t status, const std::uint8_t value)
{
    auto result = static_cast<std::uint8_t>(status & ~(kZero | kNegative));
    if (value == 0U) {
        result = static_cast<std::uint8_t>(result | kZero);
    }
    return static_cast<std::uint8_t>(result | (value & kNegative));
}

void add_with_carry(CpuRegisters& registers, const std::uint8_t right)
{
    const auto left = registers.accumulator;
    const auto wide = static_cast<std::uint16_t>(left)
        + static_cast<std::uint16_t>(right)
        + ((registers.status & kCarry) != 0U ? 1U : 0U);
    const auto value = static_cast<std::uint8_t>(wide);
    auto status = static_cast<std::uint8_t>(
        registers.status & ~(kCarry | kOverflow));
    if ((wide & 0x100U) != 0U) {
        status = static_cast<std::uint8_t>(status | kCarry);
    }
    if (((~(left ^ right) & (left ^ value)) & kNegative) != 0U) {
        status = static_cast<std::uint8_t>(status | kOverflow);
    }
    registers.accumulator = value;
    registers.status = set_nz(status, value);
}

} // namespace

// The live bonus-stage-4 orbit arm.  Its inactive branches join the outer
// controller at 05:6D7F, so they decline before staging any write.
std::optional<RoutineReplacement> replace_bonus_stage4_orbit(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    if (entry.routine.physical_bank != kBank
        || entry.routine.logical_pc != kEntry || work_ram.size() < 0x2000U
        || (entry.registers.status & (kDecimal | kMemory)) != 0U) {
        return std::nullopt;
    }

    std::uint8_t ram[0x2000];
    for (std::size_t i = 0U; i < 0x2000U; ++i) {
        ram[i] = std::to_integer<std::uint8_t>(work_ram[i]);
    }
    auto registers = entry.registers;
    const auto read = [&ram](const std::uint16_t address) {
        return ram[address - kRamBase];
    };
    const auto zp = [](const std::uint8_t address) {
        return static_cast<std::uint16_t>(kRamBase + address);
    };
    const auto slot = static_cast<std::uint16_t>(registers.x);
    const auto linked_slot = read(static_cast<std::uint16_t>(0x3E80U + slot));
    const auto linked_state = read(static_cast<std::uint16_t>(
        0x3C80U + static_cast<std::uint16_t>(linked_slot)));
    const auto attributes = read(static_cast<std::uint16_t>(0x3800U + slot));
    if (linked_state == 0U || (attributes & kNegative) != 0U) {
        return std::nullopt;
    }

    const auto read_table = [&registers, rom](const std::uint16_t address)
        -> std::optional<std::uint8_t> {
        const auto bank = registers.mpr[address >> 13U];
        if (bank >= 0x80U || rom.size() != 0x60000U) {
            return std::nullopt;
        }
        const auto rom_bank = bank < 0x40U
            ? static_cast<std::size_t>(bank & 0x1FU)
            : static_cast<std::size_t>((bank & 0x0FU) + 0x20U);
        const auto offset = rom_bank * 0x2000U
            + static_cast<std::size_t>(address & 0x1FFFU);
        if (offset >= rom.size()) {
            return std::nullopt;
        }
        return std::to_integer<std::uint8_t>(rom[offset]);
    };

    RoutineReplacement replacement;
    std::uint32_t cycle = 0U;
    std::size_t committed = 0U;
    const auto checkpoint = [&replacement, &registers, &cycle, &committed](
                                const std::uint16_t instruction,
                                const std::uint16_t resume,
                                const std::uint32_t cycles) {
        cycle += cycles;
        while (committed < replacement.work_ram_writes.size()) {
            replacement.work_ram_writes[committed].master_cycle_offset = cycle;
            ++committed;
        }
        registers.program_counter = resume;
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = cycle - 3U,
            .master_cycle_offset = cycle,
            .instruction = {kBank, instruction},
            .registers = registers,
        });
    };
    const auto store = [&replacement, &ram](const std::uint16_t instruction,
                                             const std::uint16_t address,
                                             const std::uint8_t value) {
        ram[address - kRamBase] = value;
        replacement.work_ram_writes.push_back({
            .instruction = {kBank, instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = 0U,
        });
    };
    const auto load_a = [&registers](const std::uint8_t value) {
        registers.accumulator = value;
        registers.status = set_nz(registers.status, value);
    };
    const auto load_y = [&registers](const std::uint8_t value) {
        registers.y = value;
        registers.status = set_nz(registers.status, value);
    };
    const auto asl_a = [&registers]() {
        const auto source = registers.accumulator;
        registers.accumulator = static_cast<std::uint8_t>(source << 1U);
        registers.status = set_nz(
            static_cast<std::uint8_t>(registers.status & ~kCarry),
            registers.accumulator);
        if ((source & kNegative) != 0U) {
            registers.status = static_cast<std::uint8_t>(
                registers.status | kCarry);
        }
    };

    // 6D27-6D56: validate the live pair, advance the phase, and cache the
    // linked actor's four coordinate bytes in zero page.
    load_y(linked_slot); checkpoint(0x6D27U, 0x6D2AU, 15U);
    load_a(linked_state); checkpoint(0x6D2AU, 0x6D2DU, 15U);
    checkpoint(0x6D2DU, 0x6D2FU, 6U);
    load_a(attributes); checkpoint(0x6D2FU, 0x6D32U, 15U);
    checkpoint(0x6D32U, 0x6D34U, 6U);
    const auto phase_address = static_cast<std::uint16_t>(0x3E00U + slot);
    auto phase = static_cast<std::uint8_t>(read(phase_address) + 1U);
    store(0x6D34U, phase_address, phase);
    registers.status = set_nz(registers.status, phase);
    checkpoint(0x6D34U, 0x6D37U, 21U);
    load_a(phase); checkpoint(0x6D37U, 0x6D3AU, 15U);
    registers.accumulator = static_cast<std::uint8_t>(registers.accumulator & 0x1FU);
    registers.status = set_nz(registers.status, registers.accumulator);
    checkpoint(0x6D3AU, 0x6D3CU, 6U);
    phase = registers.accumulator;
    store(0x6D3CU, phase_address, phase); checkpoint(0x6D3CU, 0x6D3FU, 15U);
    load_y(read(static_cast<std::uint16_t>(0x3E80U + slot)));
    checkpoint(0x6D3FU, 0x6D42U, 15U);
    load_a(read(static_cast<std::uint16_t>(0x3180U + registers.y)));
    checkpoint(0x6D42U, 0x6D45U, 15U);
    store(0x6D45U, zp(0x35U), registers.accumulator); checkpoint(0x6D45U, 0x6D47U, 12U);
    load_a(read(static_cast<std::uint16_t>(0x3200U + registers.y)));
    checkpoint(0x6D47U, 0x6D4AU, 15U);
    store(0x6D4AU, zp(0x36U), registers.accumulator); checkpoint(0x6D4AU, 0x6D4CU, 12U);
    load_a(read(static_cast<std::uint16_t>(0x3300U + registers.y)));
    checkpoint(0x6D4CU, 0x6D4FU, 15U);
    store(0x6D4FU, zp(0x37U), registers.accumulator); checkpoint(0x6D4FU, 0x6D51U, 12U);
    load_a(read(static_cast<std::uint16_t>(0x3380U + registers.y)));
    checkpoint(0x6D51U, 0x6D54U, 15U);
    store(0x6D54U, zp(0x38U), registers.accumulator); checkpoint(0x6D54U, 0x6D56U, 12U);

    load_a(phase); checkpoint(0x6D56U, 0x6D59U, 15U);
    asl_a(); checkpoint(0x6D59U, 0x6D5AU, 6U);
    asl_a(); checkpoint(0x6D5AU, 0x6D5BU, 6U);
    load_y(registers.accumulator); checkpoint(0x6D5BU, 0x6D5CU, 6U);
    const auto d0 = read_table(static_cast<std::uint16_t>(0x6D8EU + registers.y));
    const auto d1 = read_table(static_cast<std::uint16_t>(0x6D8FU + registers.y));
    const auto d2 = read_table(static_cast<std::uint16_t>(0x6D90U + registers.y));
    const auto d3 = read_table(static_cast<std::uint16_t>(0x6D91U + registers.y));
    if (!d0 || !d1 || !d2 || !d3) {
        return std::nullopt;
    }

    registers.status = static_cast<std::uint8_t>(registers.status & ~kCarry);
    checkpoint(0x6D5CU, 0x6D5DU, 6U);
    load_a(*d0); checkpoint(0x6D5DU, 0x6D60U, 15U);
    add_with_carry(registers, read(zp(0x35U))); checkpoint(0x6D60U, 0x6D62U, 12U);
    store(0x6D62U, static_cast<std::uint16_t>(0x3180U + slot), registers.accumulator);
    checkpoint(0x6D62U, 0x6D65U, 15U);
    load_a(*d1); checkpoint(0x6D65U, 0x6D68U, 15U);
    add_with_carry(registers, read(zp(0x36U))); checkpoint(0x6D68U, 0x6D6AU, 12U);
    store(0x6D6AU, static_cast<std::uint16_t>(0x3200U + slot), registers.accumulator);
    checkpoint(0x6D6AU, 0x6D6DU, 15U);
    registers.status = static_cast<std::uint8_t>(registers.status & ~kCarry);
    checkpoint(0x6D6DU, 0x6D6EU, 6U);
    load_a(*d2); checkpoint(0x6D6EU, 0x6D71U, 15U);
    add_with_carry(registers, read(zp(0x37U))); checkpoint(0x6D71U, 0x6D73U, 12U);
    store(0x6D73U, static_cast<std::uint16_t>(0x3300U + slot), registers.accumulator);
    checkpoint(0x6D73U, 0x6D76U, 15U);
    load_a(*d3); checkpoint(0x6D76U, 0x6D79U, 15U);
    add_with_carry(registers, read(zp(0x38U))); checkpoint(0x6D79U, 0x6D7BU, 12U);
    store(0x6D7BU, static_cast<std::uint16_t>(0x3380U + slot), registers.accumulator);
    checkpoint(0x6D7BU, kReturn, 15U);

    replacement.return_instruction = {kBank, kReturn};
    replacement.output_registers = registers;
    replacement.body_master_cycles = cycle;
    absorb_return(replacement, work_ram, entry.registers.stack_pointer,
                  {kBank, kReturn}, cycle, registers);
    return replacement;
}

} // namespace devils_crush::app::detail
