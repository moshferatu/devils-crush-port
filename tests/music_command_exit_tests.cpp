// SPDX-License-Identifier: GPL-3.0-or-later

#include "../src/app/native_replacements_internal.hpp"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {
using namespace devils_crush::emulation;
using devils_crush::app::detail::replace_music_sequencer;
void require(bool condition, const char* message)
{
    if (!condition) { throw std::runtime_error(message); }
}
void verify_psg_horizon()
{
    std::array<std::byte, 0x2000> ram{};
    for (unsigned horizon = 0; horizon <= 46U; ++horizon) {
        for (unsigned sp = 0; sp < 256U; ++sp) {
            RoutineEntry entry{};
            entry.routine = {0x45U, 0x4142U};
            entry.registers.program_counter = 0x4142U;
            entry.registers.mpr = {0xFFU, 0xF8U, 0x45U, 0U, 0U, 0U, 0U, 0U};
            entry.registers.status = 0xC1U;
            entry.registers.x = 0xFFU;
            entry.registers.stack_pointer = static_cast<std::uint8_t>(sp);
            entry.master_cycles_until_timer_interrupt = horizon;
            ram[0xF5] = std::byte{1};
            const auto result = replace_music_sequencer(entry, ram, {});
            require(result.has_value(), "PSG prefix declined near timer horizon");
            if (horizon <= 45U) {
                require(result->return_instruction.logical_pc == 0x4148U
                    && result->body_master_cycles == 33U
                    && result->checkpoints.size() == 5U,
                    "PSG prefix did not stop before shared-state read");
                require(result->output_registers.accumulator == 0U
                    && result->output_registers.x == 0U
                    && result->output_registers.status == 0x42U,
                    "PSG prefix flags or registers changed");
                const auto& writes = result->work_ram_writes;
                require(writes.size() == 1U && writes[0].logical_address == 0x2100U + sp
                    && writes[0].value == 0xD1U && writes[0].master_cycle_offset == 15U
                    && result->output_registers.stack_pointer == static_cast<std::uint8_t>(sp - 1U),
                    "PSG PHP stack write or wrapping changed");
                for (const auto& c : result->checkpoints) {
                    require(c.interrupt_sample_master_cycle_offset + 3U == c.master_cycle_offset,
                        "PSG prefix interrupt sampling changed");
                }
            } else {
                require(result->return_instruction.logical_pc == 0x4153U,
                    "PSG read with sufficient horizon stopped early");
            }
        }
    }
    std::puts("Verified 12,032 PSG timer-horizon/stack combinations.");
}
void verify_gate()
{
    std::array<std::byte, 0x2000> ram{};
    for (unsigned step = 0; step < 256U; ++step) {
        for (unsigned accumulator = 0; accumulator < 256U; ++accumulator) {
            for (unsigned arm = 0; arm < 3U; ++arm) {
                ram.fill(std::byte{0});
                const unsigned x = accumulator & 7U;
                ram[0xEBE + x] = std::byte{0xFF};
                ram[0xE78 + x] = static_cast<std::byte>(step);
                ram[0xEDC + x] = static_cast<std::byte>(accumulator);
                ram[0xEB4 + x] = static_cast<std::byte>(arm == 0U ? 0U : 4U);
                ram[0xF18 + x] = static_cast<std::byte>(arm == 2U ? 0x80U : 0U);
                RoutineEntry entry{};
                entry.routine = {0x45U, 0x404FU};
                entry.registers.program_counter = 0x404FU;
                entry.registers.mpr = {0xFFU, 0xF8U, 0x45U, 0U, 0U, 0U, 0U, 0U};
                entry.registers.accumulator = 0xA5U;
                entry.registers.x = static_cast<std::uint8_t>(x);
                entry.registers.status = 0x41U;
                const auto before = ram;
                const auto result = replace_music_sequencer(entry, ram, {});
                require(result.has_value(), "Standalone gate declined");
                const bool skip = step != 0U && step + accumulator < 256U;
                const auto target = skip ? 0x408FU : arm == 2U ? 0x4072U : 0x4081U;
                const unsigned cycles = skip ? 111U
                    : (step == 0U ? 69U : 105U) + (arm == 0U ? 33U : arm == 1U ? 54U : 48U);
                require(result->return_instruction.logical_pc == target
                    && result->body_master_cycles == cycles, "Gate branch or timing changed");
                require(result->output_registers.y == 0xA5U
                    && result->output_registers.x == x && ram == before,
                    "Gate registers or input RAM changed");
                const auto sum = static_cast<std::uint8_t>(step + accumulator);
                const unsigned carry = step == 0U ? 1U : (step + accumulator > 255U ? 1U : 0U);
                const unsigned overflow = step == 0U ? 0x40U
                    : ((~(step ^ accumulator) & (step ^ sum) & 0x80U) != 0U ? 0x40U : 0U);
                const unsigned final_a = skip ? sum : arm == 2U ? 0x80U : 0U;
                require(result->output_registers.accumulator == final_a
                    && result->output_registers.status == (carry | overflow
                        | (final_a & 0x80U) | (final_a == 0U ? 2U : 0U)),
                    "Gate arithmetic flags changed");
                const auto& writes = result->work_ram_writes;
                require(writes.size() == (step == 0U ? 1U : 2U)
                    && writes[0].logical_address == 0x2EBEU + x
                    && writes[0].value == 0x7FU && writes[0].master_cycle_offset == 42U,
                    "Gate retrigger write changed");
                if (step != 0U) {
                    require(writes[1].logical_address == 0x2EDCU + x
                        && writes[1].value == static_cast<std::uint8_t>(step + accumulator)
                        && writes[1].master_cycle_offset == 99U,
                        "Gate accumulator write changed");
                }
                for (const auto& c : result->checkpoints) {
                    require(c.interrupt_sample_master_cycle_offset + 3U == c.master_cycle_offset,
                        "Gate interrupt checkpoint changed");
                }
            }
        }
    }
    std::puts("Verified 196,608 gate arithmetic/branch combinations.");
}
void verify()
{
    std::array<std::byte, 0x2000> ram{};
    std::vector<std::byte> rom(0x60000);
    // Synthetic command table, with the actual two ROM handler targets.
    // MPR2=45 maps to physical ROM page 25; MPR3=46 maps to page 26.
    rom[0x4A2BE] = std::byte{0x55}; rom[0x4A2BF] = std::byte{0x43};
    rom[0x4A2C6] = std::byte{0x4F}; rom[0x4A2C7] = std::byte{0x43};
    rom[0x4A2DE] = std::byte{0x55}; rom[0x4A2DF] = std::byte{0x43};
    unsigned count = 0;
    for (const auto command : {0x82U, 0x86U, 0x92U}) {
        rom[0x4C000] = static_cast<std::byte>(command);
        for (unsigned sp = 0; sp < 256U; ++sp) {
            for (unsigned x = 0; x < 256U; ++x) {
                ram.fill(std::byte{0});
                for (unsigned i = 0; i < 256U; ++i) {
                    ram[0x100U + i] = static_cast<std::byte>(i ^ 0xA5U);
                }
                ram[0xE34] = static_cast<std::byte>(x);
                ram[0xE8C + x] = std::byte{0};
                ram[0xE96 + x] = std::byte{0x60};
                ram[0xEBE + x] = std::byte{0xFF};
                ram[0xF0E + x] = std::byte{0x5A};
                RoutineEntry entry{};
                entry.routine = {0x45U, 0x457AU};
                auto& r = entry.registers;
                r.program_counter = 0x457AU;
                r.mpr = {0xFFU, 0xF8U, 0x45U, 0x46U, 0U, 0U, 0U, 0U};
                r.stack_pointer = static_cast<std::uint8_t>(sp);
                r.x = static_cast<std::uint8_t>(x);
                r.status = static_cast<std::uint8_t>((sp & 1U) ? 0xD7U : 0U);
                const auto before = ram;
                const auto result = replace_music_sequencer(entry, ram, rom);
                require(result.has_value(), "Sequence-end command declined");
                require(ram == before, "Model mutated input RAM");
                const bool publish = command == 0x86U;
                const auto& out = result->output_registers;
                const auto final_sp = static_cast<std::uint8_t>(sp + 2U);
                const auto final_a = static_cast<std::uint8_t>(final_sp ^ 0xA5U);
                require(out.program_counter == 0x40EEU
                    && result->return_instruction.logical_pc == 0x40EEU,
                    "Nonlocal exit did not stop on tick continuation");
                require(result->absorbed_returns.empty(), "Nonlocal jump was declared an RTS");
                require(out.stack_pointer == final_sp && out.accumulator == final_a,
                    "Live caller stack or wrapping pops changed");
                require(out.x == x && out.y == 1U && out.mpr == r.mpr,
                    "Exit clobbered X/Y or mapper");
                // ASL of the command sets carry; PLA changes only N/Z.
                const auto expected_status = static_cast<std::uint8_t>(
                    (r.status & 0x54U) | 1U | (final_a & 0x80U)
                    | (final_a == 0U ? 2U : 0U));
                require(out.status == expected_status, "PLA status changed");
                require(result->body_master_cycles == (publish ? 390U : 360U),
                    "Wrong sequence-end timing");
                require(result->checkpoints.size() == (publish ? 31U : 29U),
                    "Missing instruction checkpoint");
                const auto& writes = result->work_ram_writes;
                require(writes.size() == (publish ? 9U : 8U), "Wrong write count");
                require(writes.back().logical_address == 0x2E46U + x
                    && writes.back().value == 0U
                    && writes.back().master_cycle_offset == (publish ? 330U : 300U),
                    "Voice deactivation write changed");
                if (publish) {
                    const auto& w = writes[writes.size() - 2U];
                    require(w.logical_address == 0x2E00U && w.value == 0x5AU
                        && w.master_cycle_offset == 315U, "Published voice value changed");
                }
                // Real JSR writes happen before its cycles, not at its end.
                require(writes[3].logical_address == 0x2100U + sp
                    && writes[3].value == 0x45U && writes[3].master_cycle_offset == 141U,
                    "Dispatch JSR high-byte timing changed");
                require(writes[4].logical_address == 0x2100U + static_cast<std::uint8_t>(sp - 1U)
                    && writes[4].value == 0x9AU && writes[4].master_cycle_offset == 141U,
                    "Dispatch JSR low byte changed");
                const auto base = result->checkpoints.size() - 5U;
                for (unsigned i = 0; i < 4U; ++i) {
                    const auto& c = result->checkpoints[base + i];
                    const auto pop_sp = static_cast<std::uint8_t>(sp - 1U + i);
                    const auto value = i == 0U ? 0x9AU : i == 1U ? 0x45U
                        : static_cast<unsigned>(pop_sp ^ 0xA5U);
                    require(c.instruction.logical_pc == 0x4359U + i
                        && c.registers.stack_pointer == pop_sp
                        && c.registers.accumulator == value
                        && c.master_cycle_offset == (publish ? 342U : 312U) + 12U * i
                        && c.registers.status == ((r.status & 0x54U) | 1U
                            | (value & 0x80U) | (value == 0U ? 2U : 0U)),
                        "Intermediate PLA checkpoint changed");
                }
                for (const auto& c : result->checkpoints) {
                    require(c.interrupt_sample_master_cycle_offset + 3U == c.master_cycle_offset,
                        "Wrong interrupt sampling boundary");
                }
                if (sp == 0U && x == 0U) {
                    r.mpr[1] = 0xFEU;
                    require(!replace_music_sequencer(entry, ram, rom), "Wrong stack mapper admitted");
                    r.mpr[1] = 0xF8U;
                    require(!replace_music_sequencer(entry, std::span(ram).first(0x1FFF), rom),
                        "Short RAM admitted");
                    for (const auto flags : {8U, 32U, 40U}) {
                        r.status = static_cast<std::uint8_t>(flags);
                        require(!replace_music_sequencer(entry, ram, rom), "D/T flags admitted");
                    }
                }
                ++count;
            }
        }
    }
    std::printf("Verified %u sequence-end stack/voice combinations.\n", count);
}
}
int main()
{
    try { verify_gate(); verify_psg_horizon(); verify(); return 0; }
    catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
