// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/app/native_replacements_internal.hpp"
#include <array>
#include <stdexcept>
using namespace devils_crush::emulation;
using devils_crush::app::detail::replace_session_input_gate;
void check(bool b) { if (!b) throw std::runtime_error("Session gate mismatch"); }
int main() {
    std::array<std::byte, 0x2000> ram{};
    RoutineEntry e{}; e.routine = {1, 0xC49E};
    e.registers.mpr = {0xFF, 0xF8, 2, 3, 0, 0, 1, 0};
    for (unsigned flags = 0; flags < 256; ++flags) {
        for (unsigned buttons = 0; buttons < 256; ++buttons) {
            if (!(flags & 2U) && !(buttons & 8U)) continue;
            ram.fill(std::byte{});
            // Exercise indexed reads across a page and stack wrapping.
            const auto player = static_cast<std::uint8_t>(buttons + flags);
            ram[0x251] = static_cast<std::byte>(player);
            ram[0x18U + player] = static_cast<std::byte>(buttons);
            ram[0x53] = static_cast<std::byte>(flags);
            const auto input = std::to_integer<std::uint8_t>(ram[0x18U + player]);
            if (!(flags & 2U) && !(input & 8U)) continue;
            e.registers.y = 0xA5; e.registers.x = 0x89;
            e.registers.stack_pointer = static_cast<std::uint8_t>(buttons);
            e.registers.status = static_cast<std::uint8_t>(flags & ~0x28U);
            const auto before = ram;
            const auto r = replace_session_input_gate(e, ram); check(r.has_value());
            const bool blocked = (flags & 2U) != 0;
            check(ram == before);
            check(r->body_master_cycles == (blocked ? 30U : 87U));
            check(r->return_instruction == (blocked ? RoutineKey{1, 0xC4C8} : RoutineKey{0, 0xED37}));
            check(r->output_registers.accumulator == (blocked ? 2 : 8));
            check(r->output_registers.y == (blocked ? 0xA5 : player));
            check(r->output_registers.x == 0x89 && r->output_registers.mpr == e.registers.mpr);
            check(r->output_registers.status == (e.registers.status & ~0x82U));
            check(r->checkpoints.size() == (blocked ? 3U : 8U));
            check(r->work_ram_writes.size() == (blocked ? 0U : 2U));
            check(r->output_registers.stack_pointer == static_cast<std::uint8_t>(buttons - (blocked ? 0U : 2U)));
            if (!blocked) {
                for (unsigned i = 0; i < 2; ++i) {
                    const auto& w = r->work_ram_writes[i];
                    check(w.logical_address == 0x2100U + static_cast<std::uint8_t>(buttons-i));
                    check(w.value == (i == 0 ? 0xC4 : 0xB0));
                    check(w.master_cycle_offset == 66 && w.instruction == RoutineKey{1, 0xC4AE});
                }
            }
            for (const auto& cp : r->checkpoints) check(cp.interrupt_sample_master_cycle_offset + 3 == cp.master_cycle_offset);
        }
    }
    ram.fill(std::byte{}); ram[0x18] = std::byte{8};
    e.registers.status = 0;
    for (unsigned sp = 0; sp < 256; ++sp) {
        e.registers.stack_pointer = static_cast<std::uint8_t>(sp);
        const auto r = replace_session_input_gate(e, ram); check(r.has_value());
        check(r->work_ram_writes.size() == 2);
        check(r->work_ram_writes[0].logical_address == 0x2100U + sp);
        check(r->work_ram_writes[1].logical_address == 0x2100U + static_cast<std::uint8_t>(sp-1));
        check(r->output_registers.stack_pointer == static_cast<std::uint8_t>(sp-2));
    }
    ram[0x53] = std::byte{2};
    e.registers.status = 8; check(!replace_session_input_gate(e, ram));
    e.registers.status = 0x20; check(!replace_session_input_gate(e, ram));
    e.registers.status = 0; e.registers.mpr[7] = 1; check(!replace_session_input_gate(e, ram));
    e.registers.mpr[7] = 0; e.registers.mpr[6] = 0; check(!replace_session_input_gate(e, ram));
    e.registers.mpr[6] = 1; e.registers.mpr[1] = 0; check(!replace_session_input_gate(e, ram));
}
