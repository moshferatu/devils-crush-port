// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/app/native_replacements_internal.hpp"
#include <cstdio>
#include <stdexcept>

using namespace devils_crush::emulation;
static void require(bool ok) { if (!ok) { throw std::runtime_error("Three-Blobs branch mismatch"); } }
int main()
{
    try {
        for (unsigned status = 0; status < 256; ++status) {
            for (unsigned horizon = 0; horizon <= 301; ++horizon) {
                RoutineEntry entry{};
                entry.routine = {3U, 0x66A6U};
                auto& r = entry.registers;
                r.program_counter = 0x66A6U; r.mpr[3] = 3U;
                r.accumulator = 0xA5U; r.x = 0xFFU; r.y = 0x37U;
                r.stack_pointer = 0xFEU; r.status = static_cast<std::uint8_t>(status);
                entry.master_cycles_until_timer_interrupt = horizon;
                entry.master_cycles_until_user_event = horizon;
                const auto result = devils_crush::app::detail::replace_three_blobs_timer_branch(entry);
                require(result.has_value() == ((status & 0x20U) == 0U));
                if (!result) { continue; }
                const bool taken = (status & 2U) == 0U;
                const unsigned cycles = taken ? 12U : 6U;
                const unsigned target = taken ? 0x66B5U : 0x66A8U;
                const auto& out = result->output_registers;
                require(out.program_counter == target && out.status == status);
                require(out.accumulator == r.accumulator && out.x == r.x && out.y == r.y);
                require(out.stack_pointer == r.stack_pointer && out.mpr == r.mpr);
                require(result->return_instruction == RoutineKey{3U, static_cast<std::uint16_t>(target)});
                require(result->body_master_cycles == cycles && result->work_ram_writes.empty());
                require(result->checkpoints.size() == 1U);
                const auto& checkpoint = result->checkpoints.front();
                require(checkpoint.instruction == entry.routine);
                require(checkpoint.interrupt_sample_master_cycle_offset == cycles - 3U);
                require(checkpoint.master_cycle_offset == cycles);
                require(checkpoint.registers.program_counter == target && checkpoint.registers.status == status);
                r.program_counter = 0x66A3U;
                require(!devils_crush::app::detail::replace_three_blobs_timer_branch(entry));
                r.program_counter = 0x66A6U; r.mpr[3] = 4U;
                require(!devils_crush::app::detail::replace_three_blobs_timer_branch(entry));
                r.mpr[3] = 3U; entry.routine.physical_bank = 4U;
                require(!devils_crush::app::detail::replace_three_blobs_timer_branch(entry));
            }
        }
        std::puts("Verified 77,312 Three-Blobs status/horizon cases.");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what()); return 1;
    }
}
