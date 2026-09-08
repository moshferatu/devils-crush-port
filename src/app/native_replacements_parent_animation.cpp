// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_replacements_internal.hpp"
#include <algorithm>
#include <array>

namespace devils_crush::app::detail {
using namespace emulation;

std::optional<RoutineReplacement> replace_upper_parent_animation(
    const RoutineEntry& entry, const std::span<const std::byte> work_ram)
{
    // Real child-return and coroutine-resumption boundaries, not an inlined
    // scheduler. C5C9 consumes the JSR frame; F08E/ECFD/590C return normally.
    constexpr std::array<std::uint16_t, 12> entries{
        0x705C, 0x705F, 0x7084, 0x7087, 0x7094, 0x709E, 0x70A7,
        0x7194, 0x7197, 0x71BC, 0x71BF, 0x71CD};
    if (entry.routine.physical_bank != 9U
        || std::find(entries.begin(), entries.end(), entry.routine.logical_pc) == entries.end()
        || work_ram.size() < 0x2000U || entry.registers.mpr[1] != 0xF8U
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }
    RoutineReplacement result{};
    auto r = entry.registers;
    // The sibling has byte-identical descending and ascending phases, but a
    // shorter hold phase. Relocate instruction identities AND saved JSR PCs;
    // child targets and the existing final scheduler owner remain live.
    const bool sibling = entry.routine.logical_pc >= 0x7194U;
    const auto relocated = [&](const std::uint16_t pc) -> std::uint16_t {
        if (sibling && pc >= 0x705CU && pc <= 0x7087U) {
            return static_cast<std::uint16_t>(pc + 0x138U);
        }
        if (sibling && pc >= 0x70A7U && pc <= 0x70C1U) {
            return static_cast<std::uint16_t>(pc + 0x126U);
        }
        return pc;
    };
    std::uint32_t cycle = 0;
    const auto nz = [&](const std::uint8_t value) {
        r.status = static_cast<std::uint8_t>((r.status & ~0x82U)
            | (value == 0U ? 2U : value & 0x80U));
    };
    const auto read = [&](const std::uint16_t address) {
        for (auto it = result.work_ram_writes.rbegin(); it != result.work_ram_writes.rend(); ++it) {
            if (it->logical_address == address) { return it->value; }
        }
        return std::to_integer<std::uint8_t>(work_ram[address - 0x2000U]);
    };
    const auto write = [&](const std::uint16_t pc, const std::uint16_t address,
                           const std::uint8_t value) {
        result.work_ram_writes.push_back({.instruction = {9U, relocated(pc)},
            .logical_address = address, .value = value, .master_cycle_offset = cycle});
    };
    const auto step = [&](const std::uint16_t pc, const std::uint16_t next,
                          const std::uint32_t cost) {
        cycle += cost;
        r.program_counter = relocated(next);
        result.checkpoints.push_back({.interrupt_sample_master_cycle_offset = cycle - 3U,
            .master_cycle_offset = cycle, .instruction = {9U, relocated(pc)}, .registers = r});
    };
    const auto load = [&](const std::uint16_t pc, const std::uint8_t value) {
        r.accumulator = value; nz(value); step(pc, static_cast<std::uint16_t>(pc + 2U), 6U);
    };
    const auto store = [&](const std::uint16_t pc, const std::uint16_t base) {
        cycle += 15U;
        write(pc, static_cast<std::uint16_t>(base + r.x), r.accumulator);
        step(pc, static_cast<std::uint16_t>(pc + 3U), 0U);
    };
    const auto change = [&](const std::uint16_t pc, const std::uint16_t base, const int delta) {
        const auto address = static_cast<std::uint16_t>(base + r.x);
        const auto value = static_cast<std::uint8_t>(read(address) + delta);
        nz(value); cycle += 21U; write(pc, address, value);
        step(pc, static_cast<std::uint16_t>(pc + 3U), 0U);
        return value;
    };
    const auto branch = [&](const std::uint16_t pc, const bool taken, const std::uint16_t target) {
        step(pc, taken ? target : static_cast<std::uint16_t>(pc + 2U), taken ? 12U : 6U);
    };
    const auto call = [&](const std::uint16_t pc, const std::uint16_t target) {
        const auto saved = static_cast<std::uint16_t>(relocated(pc) + 2U);
        // JSR writes precede its cycle charge, including wrapped stack slots.
        write(pc, static_cast<std::uint16_t>(0x2100U + r.stack_pointer),
            static_cast<std::uint8_t>(saved >> 8U));
        r.stack_pointer = static_cast<std::uint8_t>(r.stack_pointer - 1U);
        write(pc, static_cast<std::uint16_t>(0x2100U + r.stack_pointer),
            static_cast<std::uint8_t>(saved));
        r.stack_pointer = static_cast<std::uint8_t>(r.stack_pointer - 1U);
        step(pc, target, 21U);
    };
    const auto finish = [&]() {
        result.return_instruction = {r.mpr[r.program_counter >> 13U], r.program_counter};
        result.output_registers = r;
        result.body_master_cycles = cycle;
        return result;
    };
    auto pc = entry.routine.logical_pc;
    if (pc == 0x7194U) { pc = 0x705C; }
    if (pc == 0x7197U) { pc = 0x705F; }
    if (pc == 0x71BCU) { pc = 0x7084; }
    if (pc == 0x71CDU) { pc = 0x70A7; }
    if (pc == 0x71BFU) {
        const auto timer = change(0x71BF, 0x3B00, -1);
        branch(0x71C2, timer == 0U, 0x71C5);
        if (timer != 0U) { return finish(); }
        load(0x71C5, 0x81); store(0x71C7, 0x3D00);
        call(0x71CA, 0xC5C9); return finish();
    }
    if (pc == 0x705CU || pc == 0x7084U) {
        call(pc, 0xF08EU); return finish();
    }
    if (pc == 0x705FU) {
        change(0x705F, 0x3B00, -1);
        const auto timer = change(0x7062, 0x3A00, -1);
        branch(0x7065, timer != 0U, 0x7071);
        if (timer != 0U) { return finish(); }
        load(0x7067, 6); store(0x7069, 0x3A00);
        const auto frame = change(0x706C, 0x3980, -1);
        branch(0x706F, frame == 0U, 0x7072);
        if (frame != 0U) { return finish(); }
        load(0x7072, 0x81); store(0x7074, 0x3D00);
        load(0x7077, 8); store(0x7079, 0x3600);
        load(0x707C, 8); store(0x707E, 0x3680);
        call(0x7081, 0xC5C9); return finish();
    }
    if (pc == 0x7087U) {
        r.accumulator = read(static_cast<std::uint16_t>(0x3B00U + r.x)); nz(r.accumulator);
        step(0x7087, 0x708A, 15);
        const auto other = read(static_cast<std::uint16_t>(0x3B80U + r.x));
        nz(static_cast<std::uint8_t>(r.accumulator - other));
        r.status = static_cast<std::uint8_t>((r.status & ~1U) | (r.accumulator >= other ? 1U : 0U));
        step(0x708A, 0x708D, 15);
        branch(0x708D, r.accumulator != other, 0x7094);
        if (r.accumulator == other) {
            load(0x708F, 0x0C); call(0x7091, 0xECFD); return finish();
        }
        pc = 0x7094;
    }
    if (pc == 0x7094U) {
        const auto timer = change(0x7094, 0x3B00, -1);
        branch(0x7097, timer == 0U, 0x709A);
        if (timer != 0U) { return finish(); }
        // PHX writes after charging its cycles (unlike JSR). The later PLX must
        // read the live stack after 590C, not the original entry snapshot.
        cycle += 9U;
        write(0x709A, static_cast<std::uint16_t>(0x2100U + r.stack_pointer), r.x);
        r.stack_pointer = static_cast<std::uint8_t>(r.stack_pointer - 1U);
        step(0x709A, 0x709B, 0);
        call(0x709B, 0x590C); return finish();
    }
    if (pc == 0x709EU) {
        r.stack_pointer = static_cast<std::uint8_t>(r.stack_pointer + 1U);
        r.x = read(static_cast<std::uint16_t>(0x2100U + r.stack_pointer)); nz(r.x);
        step(0x709E, 0x709F, 12);
        load(0x709F, 0x81); store(0x70A1, 0x3D00);
        call(0x70A4, 0xC5C9); return finish();
    }
    if (pc == 0x70A7U) {
        const auto timer = change(0x70A7, 0x3A00, -1);
        branch(0x70AA, timer != 0U, 0x70BB);
        if (timer != 0U) { return finish(); }
        load(0x70AC, 6); store(0x70AE, 0x3A00);
        change(0x70B1, 0x3980, 1);
        r.accumulator = read(static_cast<std::uint16_t>(0x3980U + r.x)); nz(r.accumulator);
        step(0x70B4, 0x70B7, 15);
        nz(static_cast<std::uint8_t>(r.accumulator - 6U));
        r.status = static_cast<std::uint8_t>((r.status & ~1U) | (r.accumulator >= 6U ? 1U : 0U));
        step(0x70B7, 0x70B9, 6);
        branch(0x70B9, r.accumulator == 6U, 0x70BC);
        if (r.accumulator != 6U) { return finish(); }
        load(0x70BC, 0x42); call(0x70BE, 0xECFD); return finish();
    }
    return std::nullopt;
}
} // namespace devils_crush::app::detail
