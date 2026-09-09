// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/app/native_replacements_internal.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>

using namespace devils_crush::emulation;
using devils_crush::app::detail::replace_shared_collision_table_call;
static void require(bool ok) { if (!ok) { throw std::runtime_error("Collision call mismatch"); } }
int main()
{
    try {
        std::array<std::byte, 0x2000> ram{};
        RoutineEntry entry{};
        auto& r = entry.registers;
        r.mpr[1] = 0xF8U; r.mpr[2] = 2U; r.mpr[3] = 9U;
        r.y = 0xA5U;
        for (unsigned status = 0; status < 256; ++status) {
            for (unsigned value = 0; value < 256; ++value) {
                for (unsigned slot : {0U, 0x58U, 0x7FU, 0xFFU}) {
                    entry.routine = {2U, 0x441EU}; r.program_counter = 0x441EU;
                    r.status = static_cast<std::uint8_t>(status);
                    r.x = static_cast<std::uint8_t>(slot); r.accumulator = 0x37U;
                    ram[0x1C80U + slot] = static_cast<std::byte>(value);
                    auto result = replace_shared_collision_table_call(entry, ram);
                    require(result.has_value() == ((status & 0x20U) == 0U));
                    if (!result) { continue; }
                    const auto& out = result->output_registers;
                    require(out.accumulator == value);
                    require(out.status == ((status & 0x7DU) | (value == 0 ? 2U : value & 0x80U)));
                    require(out.x == slot && out.y == r.y && out.stack_pointer == r.stack_pointer && out.mpr == r.mpr);
                    require(out.program_counter == 0x4421U && result->return_instruction == RoutineKey{2U, 0x4421U});
                    require(result->body_master_cycles == 15U && result->work_ram_writes.empty());
                    require(result->checkpoints.size() == 1U);
                    require(result->checkpoints[0].interrupt_sample_master_cycle_offset == 12U);
                    require(result->checkpoints[0].master_cycle_offset == 15U);
                }
                // Every stack pointer and status, with a destination bank
                // deliberately different from the caller's code bank.
                entry.routine = {2U, 0x4421U}; r.program_counter = 0x4421U;
                r.status = static_cast<std::uint8_t>(status); r.stack_pointer = static_cast<std::uint8_t>(value);
                auto result = replace_shared_collision_table_call(entry, ram);
                require(result.has_value() == ((status & 0x20U) == 0U));
                if (!result) { continue; }
                const auto& out = result->output_registers;
                require(out.status == status && out.accumulator == r.accumulator && out.x == r.x && out.y == r.y && out.mpr == r.mpr);
                require(out.stack_pointer == static_cast<std::uint8_t>(value - 2U));
                require(out.program_counter == 0x6966U && result->return_instruction == RoutineKey{9U, 0x6966U});
                require(result->body_master_cycles == 21U && result->work_ram_writes.size() == 2U);
                const auto& high = result->work_ram_writes[0]; const auto& low = result->work_ram_writes[1];
                require(high.logical_address == 0x2100U + value && high.value == 0x44U && high.master_cycle_offset == 0U);
                require(low.logical_address == 0x2100U + static_cast<std::uint8_t>(value - 1U) && low.value == 0x23U && low.master_cycle_offset == 0U);
                require(high.instruction == entry.routine && low.instruction == entry.routine);
                require(result->checkpoints.size() == 1U && result->checkpoints[0].interrupt_sample_master_cycle_offset == 18U);
                require(result->checkpoints[0].master_cycle_offset == 21U);
            }
        }
        r.status = 0;
        for (unsigned horizon = 0; horizon < 24; ++horizon) {
            entry.master_cycles_until_timer_interrupt = horizon;
            require(replace_shared_collision_table_call(entry, ram).has_value() == (horizon > 21));
            entry.master_cycles_until_timer_interrupt = 1000U;
            entry.master_cycles_until_user_event = horizon;
            require(replace_shared_collision_table_call(entry, ram).has_value() == (horizon > 21));
            entry.master_cycles_until_user_event = 1000U;
        }
        for (const auto pc : {std::uint16_t{0x441EU}, std::uint16_t{0x4421U}}) {
            entry.routine = {2U, pc}; r.program_counter = pc;
            require(!replace_shared_collision_table_call(entry, std::span(ram).first(0x1FFF)));
            r.mpr[1] = 0; require(!replace_shared_collision_table_call(entry, ram)); r.mpr[1] = 0xF8U;
            r.mpr[2] = 0; require(!replace_shared_collision_table_call(entry, ram)); r.mpr[2] = 2U;
            r.program_counter = 0; require(!replace_shared_collision_table_call(entry, ram)); r.program_counter = pc;
        }
        std::puts("Verified 262,144 selector-load and 65,536 call cases plus event and mapping guards.");
    } catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
