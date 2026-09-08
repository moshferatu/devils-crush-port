// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_replacements_internal.hpp"
#include <array>
namespace devils_crush::app::detail {
using namespace emulation;
std::optional<RoutineReplacement> replace_actor_initialization(
    const RoutineEntry& entry, const std::span<const std::byte> work_ram)
{
    const auto start = entry.routine.logical_pc;
    if (entry.routine.physical_bank != 1U
        || (start != 0xC655U && start != 0xC699U && start != 0xC69EU && start != 0xC6A9U)
        || entry.registers.mpr[1] != 0xF8U || entry.registers.mpr[6] != 1U
        || work_ram.size() < 0x2000U || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }
    RoutineReplacement result{};
    auto r = entry.registers;
    std::uint32_t cycle = 0;
    const auto nz = [&r](std::uint8_t value) {
        r.accumulator = value;
        r.status = static_cast<std::uint8_t>((r.status & 0x7DU)
            | (value == 0U ? 2U : value & 0x80U));
    };
    const auto cp = [&](std::uint16_t pc, std::uint16_t next, std::uint32_t cost) {
        cycle += cost;
        r.program_counter = next;
        result.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = cycle - 3U,
            .master_cycle_offset = cycle, .instruction = {1U, pc}, .registers = r});
    };
    const auto store = [&](std::uint16_t pc, std::uint16_t next,
                           std::uint16_t address, std::uint8_t value, std::uint32_t cost) {
        cp(pc, next, cost);
        result.work_ram_writes.push_back({.instruction = {1U, pc},
            .logical_address = address, .value = value, .master_cycle_offset = cycle});
    };
    const auto load = [&](std::uint16_t pc, std::uint16_t next, std::uint8_t value, std::uint32_t cost) {
        nz(value); cp(pc, next, cost);
    };
    const auto read = [&](std::uint16_t address) {
        return std::to_integer<std::uint8_t>(work_ram[address - 0x2000U]);
    };
    if (start == 0xC655U) {
        store(0xC655, 0xC657, 0x20CC, 0, 12);
        store(0xC657, 0xC65A, 0x2D32, 0, 15);
        r.x = 0; cp(0xC65A, 0xC65B, 6); // CLX does not change N/Z.
        constexpr std::array<std::uint8_t, 8> values{0x80,0x0E,1,0x8F,2,0xA0,0x13,0x80};
        constexpr std::array<std::uint16_t, 8> addresses{0x3000,0x3180,0x3200,0x3300,0x3380,0x3080,0x3800,0x3D00};
        for (std::size_t i = 0; i < values.size(); ++i) {
            const auto pc = static_cast<std::uint16_t>(0xC65BU + 5U * i);
            load(pc, static_cast<std::uint16_t>(pc+2U), values[i], 6);
            store(static_cast<std::uint16_t>(pc+2U), static_cast<std::uint16_t>(pc+5U), addresses[i], r.accumulator, 15);
        }
        store(0xC683,0xC686,0x3900,0,15);
        for (unsigned i=0; i<4; ++i) {
            store(static_cast<std::uint16_t>(0xC686U+2U*i), static_cast<std::uint16_t>(0xC688U+2U*i),
                static_cast<std::uint16_t>(0x20B9U+i),0,12);
        }
        store(0xC68E,0xC691,0x3980,0,15);
        load(0xC691,0xC693,0x3C,6);
    } else if (start == 0xC699U) {
        load(0xC699,0xC69B,0x30,6);
    } else {
        const auto address = static_cast<std::uint16_t>(0x3080U+r.x);
        if (start == 0xC69EU) {
            load(0xC69E,0xC6A1,read(address),15);
            load(0xC6A1,0xC6A3,static_cast<std::uint8_t>(r.accumulator ^ 0x20U),6);
            store(0xC6A3,0xC6A6,address,r.accumulator,15);
        } else {
            load(0xC6A9,0xC6AC,read(address),15);
            load(0xC6AC,0xC6AE,static_cast<std::uint8_t>(r.accumulator & 0xDFU),6);
            store(0xC6AE,0xC6B1,address,r.accumulator,15);
            store(0xC6B1,0xC6B4,static_cast<std::uint16_t>(0x3A80U+r.x),0,15);
            load(0xC6B4,0xC6B6,4,6);
            store(0xC6B6,0xC6B9,static_cast<std::uint16_t>(0x3A00U+r.x),4,15);
            load(0xC6B9,0xC6BB,read(0x2053),12);
            load(0xC6BB,0xC6BD,static_cast<std::uint8_t>(r.accumulator | 8U),6);
            store(0xC6BD,0xC6BF,0x2053,r.accumulator,12);
        }
    }
    // C5EA/C5C9 suspend and resume their caller. Leave each original JSR
    // outside this body; never synthesize a return across the scheduler.
    result.return_instruction = {1U, r.program_counter};
    result.output_registers = r;
    result.body_master_cycles = cycle;
    return result;
}
}
