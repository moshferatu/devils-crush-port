// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/app/native_replacements_internal.hpp"
#include <array>
#include <vector>
#include <stdexcept>
#include <cstdio>
using namespace devils_crush::emulation;
using devils_crush::app::detail::replace_eyeball_animation_prefix;
void check(bool value) { if (!value) { throw std::runtime_error("Eyeball prefix mismatch"); } }
int main() {
    std::array<std::byte, 0x2000> ram{};
    std::vector<std::byte> rom(0x60000);
    for (unsigned i = 0; i < 4; ++i) { rom[0x4F9B+i] = static_cast<std::byte>(0x80U+i); }
    RoutineEntry e{}; e.routine = {2, 0x4F3D};
    e.registers.mpr = {0xFF, 0xF8, 2, 3, 0, 0, 1, 0};
    unsigned cases = 0;
    for (unsigned timer = 0; timer < 256; ++timer) {
        for (unsigned frame = 0; frame < 256; ++frame) {
            for (unsigned borrow = 0; borrow < 2; ++borrow) {
                const auto x = static_cast<std::uint8_t>(frame + timer);
                e.registers.x = x; e.registers.y = 0xA7;
                e.registers.status = static_cast<std::uint8_t>(frame & ~0x28U);
                ram.fill(std::byte{0}); ram[0x1A00+x] = static_cast<std::byte>(timer);
                ram[0x1A80+x] = static_cast<std::byte>(frame);
                ram[0x1B00+x] = static_cast<std::byte>(borrow ? 0 : 7);
                const auto before = ram;
                auto r = replace_eyeball_animation_prefix(e, ram, rom); check(r.has_value());
                check(ram == before && r->return_instruction == RoutineKey{2, 0x4F6C});
                check(r->output_registers.program_counter == 0x4F6C);
                check(r->output_registers.accumulator == (borrow ? 0 : 7));
                check(r->output_registers.x == x && r->output_registers.mpr == e.registers.mpr
                    && r->output_registers.stack_pointer == e.registers.stack_pointer);
                for (const auto& w : r->work_ram_writes) { ram[w.logical_address-0x2000] = static_cast<std::byte>(w.value); }
                check(ram[0x1A00+x] == static_cast<std::byte>(timer == 1 ? 8 : (timer-1)&255));
                const unsigned next = (frame+1)&255;
                const unsigned wrapped = next < 4 ? next : 0;
                check((r->output_registers.status & 1U) == (timer == 1 ? (next >= 4 ? 1U : 0U) : e.registers.status & 1U));
                check((r->output_registers.status & 0x7CU) == (e.registers.status & 0x7CU));
                check(r->output_registers.y == (timer == 1 ? wrapped : 0xA7));
                check(ram[0x1A80+x] == static_cast<std::byte>(timer == 1 ? wrapped : frame));
                if (timer == 1) {
                    check(ram[0x1980+x] == static_cast<std::byte>(0x80+wrapped));
                    check(ram[0x1800+x] == std::byte{0x40});
                }
                check(ram[0x1B80+x] == static_cast<std::byte>(borrow ? 255 : 0));
                check(r->body_master_cycles == (timer == 1 ? (next < 4 ? 201U : 216U) : 60U) + (borrow ? 15U : 0U));
                check((r->output_registers.status & 0x82U) == (borrow ? 0x80U : 0U));
                for (const auto& cp : r->checkpoints) {
                    check(cp.interrupt_sample_master_cycle_offset+3 == cp.master_cycle_offset);
                    if (cp.instruction.logical_pc == 0x4F51) {
                        check(cp.registers.accumulator == 0 && (cp.registers.status & 2U) == (next == 4 ? 2U : 0U));
                    }
                }
                ++cases;
            }
        }
    }
    e.registers.x = 0; e.registers.status = 0;
    for (unsigned high = 0; high < 256; ++high) {
        ram.fill(std::byte{0}); ram[0x1A00] = std::byte{2};
        ram[0x1B80] = static_cast<std::byte>(high);
        const auto r = replace_eyeball_animation_prefix(e, ram, rom); check(r.has_value());
        check(r->work_ram_writes.back().value == static_cast<std::uint8_t>(high-1U));
        check(r->work_ram_writes.back().master_cycle_offset == 75U);
        check(r->output_registers.accumulator == 0);
        ++cases;
    }
    e.registers.status = 0x20; check(!replace_eyeball_animation_prefix(e, ram, rom));
    e.registers.status = 8; check(!replace_eyeball_animation_prefix(e, ram, rom));
    e.registers.status = 0;
    check(!replace_eyeball_animation_prefix(e, std::span<const std::byte>(ram).first(0x1FFF), rom));
    check(!replace_eyeball_animation_prefix(e, ram, std::span<const std::byte>(rom).first(0x50000)));
    e.registers.mpr[1] = 0; check(!replace_eyeball_animation_prefix(e, ram, rom));
    e.registers.mpr[1] = 0xF8;
    e.registers.status = 0; e.registers.mpr[2] = 0xFF; check(!replace_eyeball_animation_prefix(e, ram, rom));
    e.routine = {2, 0x4F6C}; e.registers.mpr[2] = 2;
    for (unsigned low = 0; low < 256; ++low) {
        for (unsigned high = 0; high < 256; ++high) {
            e.registers.x = static_cast<std::uint8_t>(low + high);
            e.registers.accumulator = static_cast<std::uint8_t>(low);
            e.registers.status = 0x45;
            ram.fill(std::byte{0});
            ram[0x1B80U + e.registers.x] = static_cast<std::byte>(high);
            ram[0x1800U + e.registers.x] = std::byte{0x81};
            const auto r = devils_crush::app::detail::replace_bank2_actor_countdown_return(e, ram);
            check(r.has_value());
            const bool expiry = low == 1 && high == 0;
            check(r->body_master_cycles == (expiry ? 105U : 42U));
            check(r->output_registers.program_counter == (expiry ? 0x4F83 : 0x4F75));
            check(r->work_ram_writes.size() == (expiry ? 3U : 1U));
            check(r->work_ram_writes[0].value == static_cast<std::uint8_t>(low-1U));
            check(r->work_ram_writes[0].master_cycle_offset == 21);
            if (expiry) {
                check(r->work_ram_writes[1].value == 0 && r->work_ram_writes[1].master_cycle_offset == 63);
                check(r->work_ram_writes[2].value == 0xC1 && r->work_ram_writes[2].master_cycle_offset == 99);
                check(r->output_registers.accumulator == 4 && r->output_registers.status == 0x45);
            }
            for (const auto& c : r->checkpoints) { check(c.interrupt_sample_master_cycle_offset + 3 == c.master_cycle_offset); }
            ++cases;
        }
    }
    std::printf("Verified %u eyeball animation/countdown cases.\n", cases);
}
