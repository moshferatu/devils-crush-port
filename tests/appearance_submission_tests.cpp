// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/app/native_replacements_internal.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>
using namespace devils_crush::emulation;
using namespace devils_crush::app::detail;
namespace {
void require(bool ok) { if (!ok) throw std::runtime_error("Appearance submission mismatch"); }
void verify()
{
    std::array<std::byte, 0x2000> ram{};
    RoutineEntry e{};
    e.registers.mpr = {0xFF, 0xF8, 2, 3, 0, 0, 1, 0};
    for (unsigned sp = 0; sp < 256; ++sp) {
        for (unsigned a = 0; a < 256; ++a) {
            e.routine = {2, 0x5475};
            e.registers.stack_pointer = static_cast<std::uint8_t>(sp);
            e.registers.accumulator = static_cast<std::uint8_t>(a);
            e.registers.x = static_cast<std::uint8_t>(a ^ sp);
            e.registers.y = static_cast<std::uint8_t>(sp + 19);
            e.registers.status = static_cast<std::uint8_t>((a ^ sp) & ~0x28U);
            const auto head = replace_devil_shooter_appearance_submission_prefix(e, ram);
            require(head && head->body_master_cycles == 66 && head->work_ram_writes.size() == 5);
            const auto& writes = head->work_ram_writes;
            require(writes[0].logical_address == 0x2100 + sp && writes[0].value == a
                && writes[0].master_cycle_offset == 9);
            require(writes[3].logical_address == 0x2100 + ((sp - 1) & 255)
                && writes[3].value == 0x54 && writes[3].master_cycle_offset == 45);
            require(writes[4].logical_address == 0x2100 + ((sp - 2) & 255)
                && writes[4].value == 0x80 && writes[4].master_cycle_offset == 45);
            require(head->output_registers.stack_pointer == ((sp - 3) & 255)
                && head->output_registers.x == e.registers.x
                && head->output_registers.y == e.registers.y
                && head->output_registers.program_counter == 0xC042);

            // Resume after the resolver's RTS, using live stack and pointer
            // values. Every appearance and stack position, including wrap.
            e.routine = {2, 0x5481};
            ram[0x100 + ((sp + 1) & 255)] = static_cast<std::byte>(a);
            const unsigned pointer = (sp * 257 + a * 31) & 65535;
            ram[0x2F] = static_cast<std::byte>(pointer & 255);
            ram[0x30] = static_cast<std::byte>(pointer >> 8);
            const auto tail = replace_devil_shooter_appearance_submission_tail(e, ram);
            require(tail && tail->body_master_cycles == 228 && tail->work_ram_writes.size() == 8);
            const unsigned sum = pointer + a * 32;
            const unsigned high = a >> 3;
            const unsigned pointer_high = pointer >> 8;
            const unsigned high_result = (sum >> 8) & 255;
            const bool overflow = ((high ^ high_result) & (pointer_high ^ high_result) & 128) != 0;
            const unsigned expected_status = (e.registers.status & 0x14U)
                | (sum > 65535 ? 1U : 0U) | (overflow ? 0x40U : 0U);
            require(tail->work_ram_writes[6].value == (sum & 255)
                && tail->work_ram_writes[7].value == ((sum >> 8) & 255));
            require(tail->output_registers.stack_pointer == ((sp + 1) & 255)
                && tail->output_registers.x == e.registers.x
                && tail->output_registers.y == e.registers.y
                && tail->output_registers.accumulator == 0x10
                && tail->output_registers.program_counter == 0xC05D
                && tail->output_registers.status == expected_status);
            for (unsigned shift = 0; shift < 5; ++shift) {
                require(tail->work_ram_writes[shift + 1].value == (a >> (7 - shift))
                    && tail->work_ram_writes[shift + 1].master_cycle_offset == 48 + shift * 24);
            }
        }
    }
    for (const auto pc : {0x5475U, 0x5481U}) {
        e.routine = {2, static_cast<std::uint16_t>(pc)};
        const auto fn = pc == 0x5475 ? replace_devil_shooter_appearance_submission_prefix
                                    : replace_devil_shooter_appearance_submission_tail;
        for (const auto bad : {8U, 32U, 40U}) {
            e.registers.status = static_cast<std::uint8_t>(bad); require(!fn(e, ram));
        }
        e.registers.status = 0;
        for (const auto mpr : {1U, 2U, 6U}) {
            const auto saved = e.registers.mpr[mpr];
            e.registers.mpr[mpr] = 0; require(!fn(e, ram)); e.registers.mpr[mpr] = saved;
        }
        require(!fn(e, std::span<const std::byte>(ram).first(0x1FFF)));
    }
}
}
void verify_countdown()
{
    std::array<std::byte, 0x2000> ram{};
    std::vector<std::byte> rom(0x60000);
    for (unsigned i = 0; i < 256; ++i) rom[0x51D6 + i] = static_cast<std::byte>(i ^ 0xA5);
    RoutineEntry e{};
    e.routine = {2, 0x51AF};
    e.registers.mpr[1] = 0xF8; e.registers.mpr[2] = 2;
    for (unsigned timer = 0; timer < 256; ++timer) {
        for (unsigned state = 0; state < 256; ++state) {
            const auto x = static_cast<std::uint8_t>(timer ^ state);
            e.registers.x = x;
            e.registers.stack_pointer = static_cast<std::uint8_t>(state);
            e.registers.status = static_cast<std::uint8_t>(state & ~0x28U);
            e.registers.accumulator = 0x93; e.registers.y = 0x77;
            ram[0x1A00 + x] = static_cast<std::byte>(timer);
            ram[0x1A80 + x] = static_cast<std::byte>(state);
            const auto r = replace_devil_shooter_release_countdown_tail(e, ram, rom);
            require(r.has_value());
            require(r->output_registers.stack_pointer == state && r->output_registers.x == x);
            const auto& w = r->work_ram_writes;
            require(w[0].logical_address == 0x3A00 + x && w[0].value == ((timer - 1) & 255)
                && w[0].master_cycle_offset == 21);
            if (timer != 1) {
                const unsigned value = (timer - 1) & 255;
                const unsigned flags = (e.registers.status & ~0x82U) | (value & 0x80U)
                    | (value == 0 ? 2U : 0U);
                require(r->body_master_cycles == 33 && w.size() == 1
                    && r->output_registers.program_counter == 0x51D5
                    && r->output_registers.status == flags
                    && r->output_registers.accumulator == 0x93 && r->output_registers.y == 0x77);
            } else {
                const unsigned next = (state + 1) & 255;
                const bool reset = next >= 12;
                const unsigned appearance = reset ? 8 : next;
                const unsigned index = (appearance - 8) & 255;
                const unsigned result = index ^ 0xA5;
                const unsigned flags = (e.registers.status & 0x3CU)
                    | (appearance >= 8 ? 1U : 0U) | (result & 0x80U)
                    | (result == 0 ? 2U : 0U);
                require(r->body_master_cycles == (reset ? 165U : 150U)
                    && r->output_registers.status == flags
                    && r->output_registers.program_counter == 0x51D2
                    && r->output_registers.y == index
                    && r->output_registers.accumulator == (index ^ 0xA5));
                require(w.size() == (reset ? 5U : 4U));
                require(w[1].value == 5 && w[1].master_cycle_offset == 48);
                require(w[2].logical_address == 0x3A80 + x && w[2].value == next
                    && w[2].master_cycle_offset == 69);
                require(w.back().logical_address == 0x3980 && w.back().value == appearance
                    && w.back().master_cycle_offset == (reset ? 132U : 117U));
            }
            for (const auto& cp : r->checkpoints)
                require(cp.interrupt_sample_master_cycle_offset + 3 == cp.master_cycle_offset);
        }
    }
    e.registers.status = 0x08; require(!replace_devil_shooter_release_countdown_tail(e, ram, rom));
    e.registers.status = 0x20; require(!replace_devil_shooter_release_countdown_tail(e, ram, rom));
    e.registers.status = 0;
    e.registers.mpr[1] = 0; require(!replace_devil_shooter_release_countdown_tail(e, ram, rom));
    e.registers.mpr[1] = 0xF8; e.registers.mpr[2] = 3;
    require(!replace_devil_shooter_release_countdown_tail(e, ram, rom)); e.registers.mpr[2] = 2;
    require(!replace_devil_shooter_release_countdown_tail(e, std::span<const std::byte>(ram).first(0x1FFF), rom));
    require(!replace_devil_shooter_release_countdown_tail(e, ram, std::span<const std::byte>(rom).first(0x52D5)));
}
int main() { try { verify(); verify_countdown(); std::puts("Verified 65536 appearance/stack and 65536 countdown/state cases plus guards."); }
    catch (const std::exception& e) { std::puts(e.what()); return 1; } }
