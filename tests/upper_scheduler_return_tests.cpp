// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/app/native_replacements_internal.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>
using namespace devils_crush::emulation;
using devils_crush::app::detail::replace_upper_scheduler_return;
void require(bool ok, const char* message) { if (!ok) { throw std::runtime_error(message); } }
int main()
{
    try {
        std::array<std::byte, 0x2000> ram{};
        RoutineEntry entry{};
        entry.routine = {0x08U, 0x5976U};
        auto& r = entry.registers;
        r.program_counter = 0x5976U;
        r.mpr = {0xFFU, 0xF8U, 0x08U, 0x09U, 0x0AU, 0x0BU, 0x01U, 0x00U};
        r.accumulator = 0xA5U; r.x = 0x35U; r.y = 0xE7U;
        entry.master_cycles_until_user_event = 100U;
        entry.master_cycles_until_timer_interrupt = 100U;
        for (unsigned sp : {0U, 254U, 255U}) {
            r.stack_pointer = static_cast<std::uint8_t>(sp);
            for (unsigned word = 0; word < 65536U; ++word) {
                r.status = static_cast<std::uint8_t>(word);
                ram[0x100U + static_cast<std::uint8_t>(sp + 1U)] = static_cast<std::byte>(word & 255U);
                ram[0x100U + static_cast<std::uint8_t>(sp + 2U)] = static_cast<std::byte>(word >> 8U);
                const auto result = replace_upper_scheduler_return(entry, ram);
                require(result.has_value(), "Return declined with open horizons");
                const auto pc = static_cast<std::uint16_t>(word + 1U);
                const auto& out = result->output_registers;
                require(out.program_counter == pc && out.stack_pointer == static_cast<std::uint8_t>(sp + 2U)
                    && result->return_instruction == RoutineKey{r.mpr[pc >> 13U], pc}, "Live stack target or mapper changed");
                require(out.status == r.status && out.accumulator == r.accumulator
                    && out.x == r.x && out.y == r.y && out.mpr == r.mpr, "RTS clobbered registers");
                require(result->body_master_cycles == 21U && result->checkpoints.empty()
                    && result->work_ram_writes.empty() && !result->native_rts_instruction,
                    "Return was not a read-only atomic transcript");
                require(result->absorbed_returns.size() == 1U
                    && result->absorbed_returns[0].instruction == entry.routine
                    && result->absorbed_returns[0].master_cycle_offset == 0U
                    && result->absorbed_returns[0].registers.stack_pointer == sp,
                    "Return observer lost the live entry frame");
            }
        }
        for (unsigned horizon = 0; horizon <= 22U; ++horizon) {
            entry.master_cycles_until_timer_interrupt = horizon;
            require(replace_upper_scheduler_return(entry, ram).has_value() == (horizon > 21U), "Timer horizon guard changed");
            entry.master_cycles_until_timer_interrupt = 100U;
            entry.master_cycles_until_user_event = horizon;
            require(replace_upper_scheduler_return(entry, ram).has_value() == (horizon > 21U), "User horizon guard changed");
            entry.master_cycles_until_user_event = 100U;
        }
        require(!replace_upper_scheduler_return(entry, std::span(ram).first(0x1FFF)), "Short RAM admitted");
        r.mpr[1] = 0U;
        require(!replace_upper_scheduler_return(entry, ram), "Foreign stack mapping admitted");
        r.mpr[1] = 0xF8U; r.mpr[2] = 0U;
        require(!replace_upper_scheduler_return(entry, ram), "Foreign code mapping admitted");
        std::puts("Verified 196,608 return target/status/stack cases and both horizon guards.");
        return 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
