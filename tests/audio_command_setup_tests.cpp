// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/app/native_replacements_internal.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>
using namespace devils_crush::emulation;
using devils_crush::app::detail::replace_audio_command_setup;
void require(bool ok, const char* message) { if (!ok) { throw std::runtime_error(message); } }
int main()
{
    try {
        std::array<std::byte, 0x2000> ram{};
        RoutineEntry entry{};
        entry.routine = {0U, 0xECFDU};
        auto& r = entry.registers;
        r.program_counter = 0xECFDU;
        r.mpr = {0xFFU, 0xF8U, 8U, 9U, 10U, 11U, 1U, 0U};
        r.x = 0xFFU; r.y = 0x37U; r.stack_pointer = 0U;
        for (unsigned command = 0U; command < 256U; ++command) {
            for (unsigned status = 0U; status < 256U; ++status) {
                for (unsigned filter : {0U, 1U, 255U}) {
                    r.accumulator = static_cast<std::uint8_t>(command);
                    r.status = static_cast<std::uint8_t>(status);
                    ram[0xCC7] = static_cast<std::byte>(filter);
                    const auto before = ram;
                    const auto result = replace_audio_command_setup(entry, ram);
                    require(result.has_value() == ((status & 32U) == 0U), "Transfer guard failed");
                    if (!result) { continue; }
                    const bool accepted = command != 255U
                        && (filter == 0U || command == 0x31U || command == 0x27U || command == 0x47U);
                    const auto& out = result->output_registers;
                    require(out.program_counter == (accepted ? 0xED18U : command == 255U ? 0xED03U : 0xED17U),
                        "Filter command decision failed");
                    require(out.accumulator == (command != 255U && filter == 0U ? 0U : command)
                        && out.x == r.x && out.y == r.y && out.stack_pointer == r.stack_pointer
                        && out.mpr == r.mpr && ram == before, "Filter changed unrelated state");
                    const unsigned cycles = command == 255U ? 24U : filter == 0U ? 57U
                        : command == 0x31U ? 81U : command == 0x27U ? 93U
                        : command == 0x47U ? 105U : 99U;
                    require(result->body_master_cycles == cycles, "Filter timing failed");
                    const auto operand = command == 255U ? 255U : filter == 0U ? command
                        : command == 0x31U ? 0x31U : command == 0x27U ? 0x27U : 0x47U;
                    const auto diff = static_cast<std::uint8_t>(command - operand);
                    const auto nz = filter == 0U && command != 255U ? 2U : diff == 0U ? 2U : diff & 128U;
                    const auto carry = command >= (command != 255U && filter == 0U ? 255U : operand) ? 1U : 0U;
                    require(out.status == ((status & ~0x83U) | nz | carry), "Filter flags failed");
                    require(result->work_ram_writes.size() == 1U && result->absorbed_returns.empty(), "Filter effects failed");
                    const auto& write = result->work_ram_writes[0];
                    require(write.logical_address == 0x202AU && write.value == command
                        && write.master_cycle_offset == 12U && write.instruction == entry.routine, "Command latch failed");
                    unsigned previous = 0U;
                    for (const auto& checkpoint : result->checkpoints) {
                        require(checkpoint.master_cycle_offset > previous
                            && checkpoint.interrupt_sample_master_cycle_offset == checkpoint.master_cycle_offset - 3U,
                            "Instruction sampling failed");
                        previous = checkpoint.master_cycle_offset;
                    }
                    require(previous == cycles, "Final checkpoint missing");
                }
            }
        }
        for (unsigned pc : {0xED18U, 0xED23U, 0xED2CU}) {
            entry.routine.logical_pc = static_cast<std::uint16_t>(pc);
            r.program_counter = static_cast<std::uint16_t>(pc);
            for (unsigned status = 0U; status < 256U; ++status) {
                r.status = static_cast<std::uint8_t>(status);
                const auto result = replace_audio_command_setup(entry, ram);
                require(result.has_value() == ((status & 32U) == 0U), "Setup transfer guard failed");
                if (!result) { continue; }
                require(result->body_master_cycles == (pc == 0xED23U ? 42U : 48U)
                    && result->output_registers.program_counter == 0xED58U
                    && result->output_registers.accumulator == 0x45U
                    && result->output_registers.status == (status & ~0x82U)
                    && result->output_registers.mpr == r.mpr
                    && result->output_registers.stack_pointer == r.stack_pointer, "Mapper handoff failed");
                require(result->work_ram_writes.size() == 2U
                    && result->work_ram_writes[0].logical_address == 0x202FU
                    && result->work_ram_writes[0].value == (pc == 0xED18U ? 6U : pc == 0xED23U ? 0U : 9U)
                    && result->work_ram_writes[1].logical_address == 0x2030U
                    && result->work_ram_writes[1].value == 0x45U, "Wrapper arguments failed");
            }
        }
        r.status = 0U;
        require(!replace_audio_command_setup(entry, std::span(ram).first(8191)), "Short RAM admitted");
        r.mpr[1] = 0U; require(!replace_audio_command_setup(entry, ram), "Foreign RAM admitted"); r.mpr[1] = 0xF8U;
        r.mpr[7] = 1U; require(!replace_audio_command_setup(entry, ram), "Foreign code admitted"); r.mpr[7] = 0U;
        ++r.program_counter; require(!replace_audio_command_setup(entry, ram), "Wrong PC admitted");
        std::puts("Verified 196,608 filter cases and all setup status values, mapper guards and checkpoints.");
        return 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
