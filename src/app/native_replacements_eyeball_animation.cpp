// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_replacements_internal.hpp"

namespace devils_crush::app::detail {
using namespace emulation;
std::optional<RoutineReplacement> replace_eyeball_animation_prefix(
    const RoutineEntry& entry, const std::span<const std::byte> ram,
    const std::span<const std::byte> rom)
{
    if (entry.routine != RoutineKey{2, 0x4F3D} || ram.size() < 0x2000U
        || entry.registers.mpr[1] != 0xF8U || entry.registers.mpr[2] != 2U
        || rom.size() != 0x60000U || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }
    RoutineReplacement result{};
    auto r = entry.registers;
    std::uint32_t cycle = 0;
    const auto nz = [&](std::uint8_t v) {
        r.status = static_cast<std::uint8_t>((r.status & ~0x82U) | (v == 0U ? 2U : v & 0x80U));
    };
    const auto read = [&](std::uint16_t base) {
        const auto address = static_cast<std::uint16_t>(base + r.x);
        for (auto it = result.work_ram_writes.rbegin(); it != result.work_ram_writes.rend(); ++it) {
            if (it->logical_address == address) { return it->value; }
        }
        return std::to_integer<std::uint8_t>(ram[address - 0x2000U]);
    };
    const auto step = [&](std::uint16_t pc, std::uint16_t next, std::uint32_t cost) {
        cycle += cost; r.program_counter = next;
        result.checkpoints.push_back({.interrupt_sample_master_cycle_offset = cycle - 3U,
            .master_cycle_offset = cycle, .instruction = {2, pc}, .registers = r});
    };
    const auto write = [&](std::uint16_t pc, std::uint16_t base, std::uint8_t value, std::uint32_t cost) {
        cycle += cost;
        result.work_ram_writes.push_back({.instruction = {2, pc},
            .logical_address = static_cast<std::uint16_t>(base + r.x),
            .value = value, .master_cycle_offset = cycle});
        step(pc, static_cast<std::uint16_t>(pc + 3U), 0);
    };
    const auto timer = static_cast<std::uint8_t>(read(0x3A00) - 1U);
    nz(timer); write(0x4F3D, 0x3A00, timer, 21);
    step(0x4F40, timer != 0U ? 0x4F64 : 0x4F42, timer != 0U ? 12U : 6U);
    if (timer == 0U) {
        r.accumulator = 8; nz(r.accumulator); step(0x4F42, 0x4F44, 6);
        write(0x4F44, 0x3A00, r.accumulator, 15);
        const auto frame = static_cast<std::uint8_t>(read(0x3A80) + 1U);
        nz(frame); write(0x4F47, 0x3A80, frame, 21);
        r.accumulator = frame; nz(frame); step(0x4F4A, 0x4F4D, 15);
        nz(static_cast<std::uint8_t>(frame - 4U));
        r.status = static_cast<std::uint8_t>((r.status & ~1U) | (frame >= 4U ? 1U : 0U));
        step(0x4F4D, 0x4F4F, 6);
        step(0x4F4F, frame < 4U ? 0x4F55 : 0x4F51, frame < 4U ? 12U : 6U);
        if (frame >= 4U) {
            r.accumulator = 0; // HuC6280 CLA preserves flags; TAY sets them below.
            step(0x4F51, 0x4F52, 6);
            write(0x4F52, 0x3A80, r.accumulator, 15);
        }
        r.y = r.accumulator; nz(r.y); step(0x4F55, 0x4F56, 6);
        r.accumulator = std::to_integer<std::uint8_t>(rom[0x4F9BU + r.y]);
        nz(r.accumulator); step(0x4F56, 0x4F59, 15);
        write(0x4F59, 0x3980, r.accumulator, 15);
        r.accumulator = read(0x3800); nz(r.accumulator); step(0x4F5C, 0x4F5F, 15);
        r.accumulator |= 0x40U; nz(r.accumulator); step(0x4F5F, 0x4F61, 6);
        write(0x4F61, 0x3800, r.accumulator, 15);
    }
    r.accumulator = read(0x3B00); nz(r.accumulator); step(0x4F64, 0x4F67, 15);
    step(0x4F67, r.accumulator != 0U ? 0x4F6C : 0x4F69, r.accumulator != 0U ? 12U : 6U);
    if (r.accumulator == 0U) {
        const auto high = static_cast<std::uint8_t>(read(0x3B80) - 1U);
        nz(high); write(0x4F69, 0x3B80, high, 21);
    }
    // Preserve the existing countdown/terminal owner and core RTS handling.
    result.return_instruction = {2, 0x4F6C};
    result.output_registers = r; result.body_master_cycles = cycle;
    return result;
}
} // namespace devils_crush::app::detail
