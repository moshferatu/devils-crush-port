// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/app/native_replacements_internal.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>
namespace {
using namespace devils_crush::emulation;
using devils_crush::app::detail::replace_selective_scene_layout;
void require(bool value, const char* message) {
    if (!value) { throw std::runtime_error(message); }
}
void verify() {
    std::array<std::byte, 0x2000> ram{};
    std::vector<std::byte> rom(0x60000);
    RoutineEntry entry{};
    entry.routine = {2U, 0x40BDU};
    entry.registers.mpr = {0xFFU, 0xF8U, 2U, 3U, 4U, 5U, 1U, 0U};
    entry.registers.x = 0xFEU; entry.registers.y = 0x81U;
    unsigned cases = 0;
    for (unsigned low = 0; low < 256U; ++low) {
        for (unsigned sp = 0; sp < 256U; ++sp) {
            for (unsigned path = 0; path < 3U; ++path) {
                ram[0x2F] = static_cast<std::byte>(low); ram[0x30] = std::byte{0x45};
                entry.registers.stack_pointer = static_cast<std::uint8_t>(sp);
                entry.registers.status = static_cast<std::uint8_t>(sp & ~0x28U);
                auto pointer = 0x4500U + low + 12U;
                rom[pointer] = path == 0 ? std::byte{1} : path == 1 ? std::byte{0xFF} : std::byte{2};
                rom[pointer + 12U] = std::byte{1};
                const auto before = ram;
                const auto model = replace_selective_scene_layout(entry, ram, rom);
                require(model.has_value(), "valid scan declined");
                const auto& out = model->output_registers;
                if (path == 2) { pointer += 12U; }
                require(ram == before, "input mutated");
                require(out.x == 0xFE && out.y == 0x81 && out.mpr == entry.registers.mpr, "preserved registers");
                require(out.stack_pointer == static_cast<std::uint8_t>(sp - 2U), "stack wrap");
                require(out.program_counter == (path == 1 ? 0xC08A : 0x40F6), "child target");
                require(model->return_instruction.physical_bank == (path == 1 ? 1 : 2), "child bank");
                require(model->body_master_cycles == (path == 0 ? 144U : path == 1 ? 138U : 273U), "scan timing");
                const auto n = model->work_ram_writes.size();
                require(n == (path == 2 ? 6U : 4U), "write count");
                const auto& lo = model->work_ram_writes[n - 4U];
                const auto& hi = model->work_ram_writes[n - 3U];
                require(lo.logical_address == 0x202F && lo.value == (pointer & 255U), "low pointer");
                require(hi.logical_address == 0x2030 && hi.value == (pointer >> 8U), "page carry");
                const auto& push_hi = model->work_ram_writes[n - 2U];
                const auto& push_lo = model->work_ram_writes[n - 1U];
                require(push_hi.logical_address == 0x2100U + sp && push_hi.value == 0x40, "JSR high push");
                require(push_lo.logical_address == 0x2100U + static_cast<std::uint8_t>(sp - 1U)
                    && push_lo.value == (path == 1 ? 0xCD : 0xBC), "JSR low push");
                require(push_hi.master_cycle_offset == model->body_master_cycles - 21U
                    && push_lo.master_cycle_offset == push_hi.master_cycle_offset, "JSR write timing");
                require(out.status == ((entry.registers.status & 0x14U) | 3U), "complete output status");
                std::uint8_t previous_status = entry.registers.status;
                for (const auto& c : model->checkpoints) {
                    require(c.interrupt_sample_master_cycle_offset + 3U == c.master_cycle_offset, "interrupt sample");
                    if (c.instruction.logical_pc == 0x40C4U) {
                        require(c.registers.status == previous_status, "CLA must preserve carry and overflow");
                    }
                    previous_status = c.registers.status;
                }
                ++cases;
            }
        }
    }
    entry.routine = {2U, 0x40A8U}; entry.registers.status = 0;
    rom[0x45DB] = std::byte{1};
    auto start = replace_selective_scene_layout(entry, ram, rom);
    require(start && start->body_master_cycles == 102 && start->work_ram_writes.size() == 4, "staged initial pointer");
    for (const auto flag : {8U, 32U, 40U}) {
        entry.registers.status = static_cast<std::uint8_t>(flag);
        require(!replace_selective_scene_layout(entry, ram, rom), "D/T admission");
    }
    entry.registers.status = 0;
    std::fill(rom.begin(), rom.end(), std::byte{0});
    rom[0x45DBU + 88U * 12U] = std::byte{0xFF};
    const auto full_scan = replace_selective_scene_layout(entry, ram, rom);
    require(full_scan && full_scan->checkpoints.size() < 2048U
        && full_scan->work_ram_writes.size() < 512U
        && full_scan->output_registers.program_counter == 0xC08A, "complete bounded scan");
    rom[0x45DBU + 88U * 12U] = std::byte{0};
    require(!replace_selective_scene_layout(entry, ram, rom), "unbounded list must decline");
    entry.registers.mpr[2] = 0xFFU;
    require(!replace_selective_scene_layout(entry, ram, rom), "device source must decline");
    entry.routine = {2U, 0x40BDU};
    entry.registers.mpr[2] = 2U;
    entry.registers.mpr[0] = 6U;
    ram[0x2F] = std::byte{0xFF}; ram[0x30] = std::byte{0xFF};
    rom[6U * 0x2000U + 11U] = std::byte{1};
    const auto wrap = replace_selective_scene_layout(entry, ram, rom);
    require(wrap && wrap->work_ram_writes[0].value == 11U
        && wrap->work_ram_writes[1].value == 0U, "16-bit wrap and live source mapper");
    std::printf("Verified %u selective scan cases, pointer carries and wrapping JSR stacks.\n", cases);
}
}
int main() {
    try { verify(); return 0; }
    catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
