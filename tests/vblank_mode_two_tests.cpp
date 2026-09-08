// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/app/native_replacements_internal.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>

namespace {
using namespace devils_crush::emulation;
using devils_crush::app::detail::replace_vblank_handler;
void require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}
RoutineEntry entry_at(std::uint16_t pc)
{
    RoutineEntry entry{};
    entry.routine = {0, pc};
    entry.registers.program_counter = pc;
    entry.registers.mpr = {0xFF, 0xF8, 2, 3, 4, 5, 1, 0};
    entry.registers.x = 0xA7;
    entry.registers.y = 0x5C;
    return entry;
}
void test_mode_two()
{
    std::array<std::byte, 0x2000> ram{};
    for (unsigned displacement = 0; displacement < 256; ++displacement) {
        ram[0xD56] = static_cast<std::byte>(displacement);
        for (unsigned status = 0; status < 256; ++status) {
            auto entry = entry_at(0xE09E);
            entry.registers.status = static_cast<std::uint8_t>(status);
            entry.registers.stack_pointer = static_cast<std::uint8_t>(status);
            ram[2] = static_cast<std::byte>(status);
            const auto result = replace_vblank_handler(entry, ram, {});
            if ((status & 0x28U) != 0U) {
                require(!result, "Unsupported CPU modes must decline");
                continue;
            }
            require(result.has_value(), "Mode 2 must be admitted");
            require(result->body_master_cycles == 345, "VDC wait-cycle accounting");
            require(result->return_instruction == RoutineKey{0, 0xEB30}, "Palette handoff");
            require(result->checkpoints.size() == 27, "Instruction checkpoints");
            require(result->work_ram_writes.size() == 14, "Complete write sequence");
            const auto raster = static_cast<std::uint8_t>(94U - displacement);
            const auto negated = static_cast<std::uint8_t>(0U - displacement);
            const auto signed_negated = negated < 128 ? static_cast<int>(negated) : static_cast<int>(negated) - 256;
            const bool overflow = signed_negated + 94 > 127;
            const auto arithmetic_status = static_cast<std::uint8_t>((status & 0x3CU)
                | (static_cast<unsigned>(negated) + 94 > 255 ? 1U : 0U)
                | (raster == 0 ? 2U : 0U) | (overflow ? 0x40U : 0U) | (raster & 0x80U));
            require(result->checkpoints[16].registers.accumulator == raster, "Raster wrapping");
            require(result->checkpoints[16].registers.status == arithmetic_status, "ADC flags");
            const std::array<std::uint16_t, 12> addresses{0,2,3,0,2,3,0,2,3,0,0x2002,2};
            const std::array<std::uint8_t, 12> values{8,static_cast<std::uint8_t>(displacement),0,
                7,0,1,6,raster,0,5,static_cast<std::uint8_t>(status & 0xBFU),static_cast<std::uint8_t>(status & 0xBFU)};
            const std::array<unsigned, 12> offsets{24,57,75,99,117,141,165,222,240,276,306,324};
            for (std::size_t i = 0; i < addresses.size(); ++i) {
                const auto& write = result->work_ram_writes[i];
                require(write.logical_address == addresses[i] && write.value == values[i]
                    && write.master_cycle_offset == offsets[i], "Timestamped VDC/shadow writes");
            }
            const auto& high = result->work_ram_writes[12];
            const auto& low = result->work_ram_writes[13];
            require(high.logical_address == 0x2100U + status && high.value == 0xE1
                && high.master_cycle_offset == 324, "JSR high byte at instruction start");
            require(low.logical_address == 0x2100U + static_cast<std::uint8_t>(status - 1U)
                && low.value == 0x13 && low.master_cycle_offset == 324, "JSR low byte and stack wrapping");
            require(result->output_registers.x == entry.registers.x
                && result->output_registers.y == entry.registers.y
                && result->output_registers.stack_pointer == static_cast<std::uint8_t>(status - 2U), "Preserved registers");
            for (const auto& cp : result->checkpoints) {
                const bool port = cp.instruction.logical_pc == 0xE0A0 || cp.instruction.logical_pc == 0xE0A6
                    || cp.instruction.logical_pc == 0xE0A9 || cp.instruction.logical_pc == 0xE0AE
                    || cp.instruction.logical_pc == 0xE0B1 || cp.instruction.logical_pc == 0xE0B6
                    || cp.instruction.logical_pc == 0xE0BB || cp.instruction.logical_pc == 0xE0C7
                    || cp.instruction.logical_pc == 0xE0CA || cp.instruction.logical_pc == 0xE105
                    || cp.instruction.logical_pc == 0xE10E;
                require(cp.master_cycle_offset - cp.interrupt_sample_master_cycle_offset == (port ? 6U : 3U), "Interrupt sampling before VDC stolen cycle");
            }
        }
    }
    auto entry = entry_at(0xE09E);
    entry.registers.mpr[0] = 0;
    require(!replace_vblank_handler(entry, ram, {}), "Unmapped VDC guard");
    entry = entry_at(0xE09E);
    entry.registers.mpr[1] = 0;
    require(!replace_vblank_handler(entry, ram, {}), "Unmapped RAM guard");
    entry = entry_at(0xE09E);
    require(!replace_vblank_handler(entry, std::span<const std::byte>(ram).first(0x1FFF), {}), "RAM size guard");
    entry = entry_at(0xE05F);
    entry.registers.accumulator = 0x20;
    ram[0x54] = std::byte{2};
    const auto dispatch = replace_vblank_handler(entry, ram, {});
    require(dispatch && dispatch->return_instruction == RoutineKey{0, 0xE09E}, "Dispatcher mode-2 boundary");
}
}
int main()
{
    try { test_mode_two(); }
    catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
    std::puts("VBlank mode 2: 65,536 displacement/status cases and guards passed");
}
