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
        for (const auto pc : {0xC693U, 0xC696U}) {
            RoutineEntry entry{};
            entry.routine = {1U, static_cast<std::uint16_t>(pc)};
            auto& r = entry.registers;
            r.program_counter = static_cast<std::uint16_t>(pc);
            r.mpr[1] = 0xF8U; r.mpr[6] = 1U;
            r.accumulator = 0xA5U; r.x = 0xFFU; r.y = 0x37U;
            entry.master_cycles_until_user_event = 100U;
            entry.master_cycles_until_timer_interrupt = 100U;
            for (unsigned sp = 0U; sp < 256U; ++sp) {
                for (unsigned status = 0U; status < 256U; ++status) {
                    r.stack_pointer = static_cast<std::uint8_t>(sp);
                    r.status = static_cast<std::uint8_t>(status);
                    const auto result = devils_crush::app::detail::replace_coroutine_call(entry, ram);
                    require(result.has_value() == ((status & 0x20U) == 0U), "Call transfer-mode guard changed");
                    if (!result) { continue; }
                    const auto& out = result->output_registers;
                    require(out.program_counter == (pc == 0xC693U ? 0xC5EAU : 0xC5C9U)
                        && out.stack_pointer == static_cast<std::uint8_t>(sp - 2U)
                        && out.status == status && out.accumulator == r.accumulator
                        && out.x == r.x && out.y == r.y && out.mpr == r.mpr,
                        "Call changed registers or destination");
                    require(result->work_ram_writes.size() == 2U, "Call frame size changed");
                    const auto& high = result->work_ram_writes[0];
                    const auto& low = result->work_ram_writes[1];
                    require(high.logical_address == 0x2100U + sp && high.value == 0xC6U
                        && low.logical_address == 0x2100U + static_cast<std::uint8_t>(sp - 1U)
                        && low.value == static_cast<std::uint8_t>(pc + 2U)
                        && high.master_cycle_offset == 0U && low.master_cycle_offset == 0U
                        && high.instruction == entry.routine && low.instruction == entry.routine,
                        "Call frame order, wrap, identity or timing changed");
                    require(result->body_master_cycles == 21U && result->absorbed_returns.empty()
                        && result->checkpoints.size() == 1U
                        && result->checkpoints[0].interrupt_sample_master_cycle_offset == 18U
                        && result->checkpoints[0].master_cycle_offset == 21U,
                        "Call checkpoint timing changed");
                }
            }
            r.status = 0U;
            for (unsigned horizon = 0U; horizon <= 22U; ++horizon) {
                entry.master_cycles_until_user_event = horizon;
                require(devils_crush::app::detail::replace_coroutine_call(entry, ram).has_value()
                    == (horizon > 21U), "Call user horizon guard changed");
                entry.master_cycles_until_user_event = 100U;
                entry.master_cycles_until_timer_interrupt = horizon;
                require(devils_crush::app::detail::replace_coroutine_call(entry, ram).has_value()
                    == (horizon > 21U), "Call timer horizon guard changed");
                entry.master_cycles_until_timer_interrupt = 100U;
            }
            require(!devils_crush::app::detail::replace_coroutine_call(entry, std::span(ram).first(0x1FFF)), "Short call RAM admitted");
            r.mpr[1] = 0U;
            require(!devils_crush::app::detail::replace_coroutine_call(entry, ram), "Foreign call stack admitted");
            r.mpr[1] = 0xF8U; r.mpr[6] = 0U;
            require(!devils_crush::app::detail::replace_coroutine_call(entry, ram), "Foreign call code admitted");
            r.mpr[6] = 1U; ++r.program_counter;
            require(!devils_crush::app::detail::replace_coroutine_call(entry, ram), "Wrong call PC admitted");
        }
        std::puts("Verified both scheduler calls across all stack pointers and status values.");
        for (const auto routine : {RoutineKey{0x08U, 0x5976U}, RoutineKey{0x01U, 0xC4C8U},
                 RoutineKey{0x08U, 0x5A69U}}) {
            RoutineEntry entry{};
            entry.routine = routine;
            auto& r = entry.registers;
            r.program_counter = routine.logical_pc;
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
            r.mpr[1] = 0xF8U; r.mpr[routine.logical_pc >> 13U] = 0U;
            require(!replace_upper_scheduler_return(entry, ram), "Foreign code mapping admitted");
        }
        std::puts("Verified 589,824 return target/status/stack cases and both horizon guards.");
        for (unsigned sp : {0U, 254U, 255U}) {
            for (unsigned word = 0U; word < 65536U; ++word) {
                RoutineEntry entry{};
                entry.routine = {0x01U, 0xC5EAU};
                auto& r = entry.registers;
                r.program_counter = 0xC5EAU;
                r.mpr = {0xFFU, 0xF8U, 8U, 9U, 10U, 11U, 1U, 0U};
                r.stack_pointer = static_cast<std::uint8_t>(sp);
                r.x = static_cast<std::uint8_t>(word & 0x7FU);
                r.y = 0xA5U; r.accumulator = static_cast<std::uint8_t>(word);
                r.status = static_cast<std::uint8_t>(word & ~0x20U);
                ram[0x100U + static_cast<std::uint8_t>(sp + 1U)] = static_cast<std::byte>(word & 255U);
                ram[0x100U + static_cast<std::uint8_t>(sp + 2U)] = static_cast<std::byte>(word >> 8U);
                const auto result = devils_crush::app::detail::replace_coroutine_delay(entry, ram);
                require(result.has_value(), "Scheduler delay declined valid entry");
                const auto target = static_cast<std::uint16_t>(word + 1U);
                const auto high = static_cast<std::uint8_t>(target >> 8U);
                const auto& out = result->output_registers;
                require(out.program_counter == target && out.stack_pointer == static_cast<std::uint8_t>(sp + 2U)
                    && out.accumulator == high && out.x == r.x && out.y == r.y && out.mpr == r.mpr,
                    "Scheduler continuation or registers changed");
                require(out.status == static_cast<std::uint8_t>((r.status & ~0x82U)
                    | (high == 0U ? 2U : high & 0x80U)), "Scheduler flags changed");
                require(result->body_master_cycles == ((word & 255U) == 255U ? 180U : 168U)
                    && result->absorbed_returns.empty(), "Scheduler JMP timing or observer changed");
                auto staged = ram;
                for (const auto& write : result->work_ram_writes) {
                    staged[write.logical_address - 0x2000U] = static_cast<std::byte>(write.value);
                }
                require(staged[0x1D80U + r.x] == static_cast<std::byte>(r.accumulator)
                    && staged[0x2FU] == static_cast<std::byte>(target & 255U)
                    && staged[0x30U] == static_cast<std::byte>(high)
                    && staged[0x1F00U + r.x] == staged[0x2FU]
                    && staged[0x1F80U + r.x] == staged[0x30U], "Scheduler RAM changed");
                r.x = 0x80U;
                require(!devils_crush::app::detail::replace_coroutine_delay(entry, ram), "Foreign actor mapping admitted");
            }
        }
        std::puts("Verified 196,608 scheduler continuation/carry/stack cases across all RAM actor slots.");
        RoutineEntry delay_entry{};
        delay_entry.routine = {0x01U, 0xC5EAU};
        delay_entry.registers.program_counter = 0xC5EAU;
        delay_entry.registers.mpr[1] = 0xF8U;
        delay_entry.registers.mpr[6] = 0x01U;
        const auto rejects_delay = [&] {
            return !devils_crush::app::detail::replace_coroutine_delay(delay_entry, ram);
        };
        require(!devils_crush::app::detail::replace_coroutine_delay(delay_entry,
            std::span(ram).first(0x1FFF)), "Short scheduler RAM admitted");
        delay_entry.registers.status = 0x20U;
        require(rejects_delay(), "Transfer mode admitted");
        delay_entry.registers.status = 0U;
        delay_entry.registers.mpr[1] = 0U;
        require(rejects_delay(), "Foreign scheduler RAM mapping admitted");
        delay_entry.registers.mpr[1] = 0xF8U;
        delay_entry.registers.mpr[6] = 0U;
        require(rejects_delay(), "Foreign scheduler code mapping admitted");
        delay_entry.registers.mpr[6] = 1U;
        delay_entry.registers.program_counter = 0xC5EDU;
        require(rejects_delay(), "Wrong scheduler PC admitted");
        return 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
