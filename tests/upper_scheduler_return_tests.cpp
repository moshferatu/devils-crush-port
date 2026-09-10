// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/app/native_replacements_internal.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>
using namespace devils_crush::emulation;
using devils_crush::app::detail::replace_upper_scheduler_return;
void require(bool ok, const char* message) { if (!ok) { throw std::runtime_error(message); } }
#include "bumper_sprite_fixture.hpp"
#include "eyeball_countdown_fixture.hpp"
#include "ranked_timer_fixture.hpp"
#include "math_transfer_fixture.hpp"
#include "ranked_control_fixture.hpp"
int main()
{
    try {
        verify_bumper_sprite_instructions();
        verify_eyeball_countdown_instructions();
        verify_ranked_timer_instructions();
        verify_math_transfer_instructions();
        verify_ranked_control_instructions();
        std::array<std::byte, 0x2000> ram{};
        for (const auto routine : {RoutineKey{5U, 0x7376U}, RoutineKey{5U, 0x7379U},
                 RoutineKey{5U, 0x7387U}, RoutineKey{5U, 0x738AU}, RoutineKey{5U, 0x738FU},
                 RoutineKey{5U, 0x7390U}, RoutineKey{5U, 0x7393U}, RoutineKey{5U, 0x7396U},
                 RoutineKey{2U, 0x4FC1U}}) {
            RoutineEntry entry{};
            entry.routine = routine;
            auto& r = entry.registers;
            r.program_counter = routine.logical_pc;
            r.mpr[1] = 0xF8U; r.mpr[r.program_counter >> 13U] = routine.physical_bank;
            r.y = 0x5AU; r.stack_pointer = 0xA5U;
            const auto run = [&] { return devils_crush::app::detail::replace_bonus_countdown_instruction(entry, ram); };
            for (unsigned index = 0; index < 256U; ++index) {
                r.x = static_cast<std::uint8_t>(index);
                const auto operand = static_cast<std::uint8_t>(255U - index);
                ram.fill(std::byte{0x69});
                ram[0x1800U + index] = ram[0x1B00U + index] = ram[0x1B80U + index]
                    = ram[0x254U] = static_cast<std::byte>(operand);
                for (unsigned input = 0; input < 256U; ++input) {
                    r.accumulator = static_cast<std::uint8_t>(input);
                    for (const unsigned status : {0U, 2U, 8U, 0x20U, 0xDFU, 0xFFU}) {
                        r.status = static_cast<std::uint8_t>(status);
                        const auto result = run();
                        require(result.has_value() == ((status & 0x20U) == 0U), "Bonus T guard");
                        if (!result) { continue; }
                        auto expected = r;
                        unsigned cycles = 15U;
                        bool nz = true;
                        switch (routine.logical_pc) {
                        case 0x7379U:
                            expected.program_counter = (status & 0x80U) ? 0x737BU : 0x7387U;
                            cycles = (status & 0x80U) ? 6U : 12U; nz = false; break;
                        case 0x738AU:
                            expected.program_counter = (status & 2U) ? 0x738CU : 0x738FU;
                            cycles = (status & 2U) ? 6U : 12U; nz = false; break;
                        case 0x7396U:
                            expected.program_counter = (status & 2U) ? 0x7399U : 0x7398U;
                            cycles = (status & 2U) ? 12U : 6U; nz = false; break;
                        case 0x738FU:
                            expected.accumulator = static_cast<std::uint8_t>(input - 1U);
                            expected.program_counter = 0x7390U; cycles = 6U; break;
                        case 0x7390U:
                            expected.program_counter = 0x7393U; nz = false; break;
                        default:
                            expected.accumulator = routine.logical_pc == 0x7393U
                                ? static_cast<std::uint8_t>(input | operand) : operand;
                            expected.program_counter += 3U; break;
                        }
                        if (nz) {
                            expected.status = static_cast<std::uint8_t>((status & 0x7DU)
                                | (expected.accumulator == 0U ? 2U : expected.accumulator & 0x80U));
                        }
                        require(result->output_registers == expected
                            && result->return_instruction == RoutineKey{routine.physical_bank, expected.program_counter}, "Bonus live state");
                        require(result->checkpoints.size() == 1U && result->body_master_cycles == cycles
                            && result->checkpoints[0].registers == expected
                            && result->checkpoints[0].instruction == routine
                            && result->checkpoints[0].interrupt_sample_master_cycle_offset == cycles - 3U
                            && result->checkpoints[0].master_cycle_offset == cycles
                            && result->absorbed_returns.empty(), "Bonus instruction checkpoint");
                        require(result->work_ram_writes.size() == (routine.logical_pc == 0x7390U ? 1U : 0U), "Bonus write count");
                        if (!result->work_ram_writes.empty()) {
                            const auto& write = result->work_ram_writes[0];
                            require(write.instruction == routine && write.logical_address == 0x3B00U + index
                                && write.value == input && write.master_cycle_offset == 15U, "Bonus live STA timing");
                        }
                    }
                }
            }
            r.status = 0;
            for (unsigned horizon = 0; horizon <= 16; ++horizon) {
                entry.master_cycles_until_user_event = horizon;
                entry.master_cycles_until_timer_interrupt = horizon;
                require(run().has_value(), "Bonus checkpoint horizon");
            }
            ++r.program_counter; require(!run(), "Bonus PC guard"); --r.program_counter;
            r.mpr[1] = 0; require(!run(), "Bonus RAM mapping"); r.mpr[1] = 0xF8U;
            r.mpr[r.program_counter >> 13U] ^= 1U; require(!run(), "Bonus code mapping");
            r.mpr[r.program_counter >> 13U] ^= 1U;
            require(!devils_crush::app::detail::replace_bonus_countdown_instruction(entry, std::span(ram).first(0x1FFF)), "Bonus RAM size");
        }
        std::puts("Verified 3,538,944 bonus/Mercy instruction operand/status cases and guards.");
        for (const auto routine : {RoutineKey{2U, 0x5CC5U}, RoutineKey{2U, 0x5CC8U},
                 RoutineKey{2U, 0x5CCBU}, RoutineKey{1U, 0xDA3EU}}) {
            RoutineEntry entry{};
            entry.routine = routine;
            auto& r = entry.registers;
            r.program_counter = routine.logical_pc;
            r.mpr[1] = 0xF8U; r.mpr[r.program_counter >> 13U] = routine.physical_bank;
            r.x = 0xA5U; r.y = 0x37U;
            const auto run = [&] { return devils_crush::app::detail::replace_ranked_gate_instruction(entry, ram); };
            for (unsigned operand = 0; operand < 256U; ++operand) {
                for (unsigned input = 0; input < 256U; ++input) {
                    r.accumulator = static_cast<std::uint8_t>(input);
                    r.stack_pointer = static_cast<std::uint8_t>(input);
                    ram[0xD00] = ram[0xD01] = ram[0x100U + static_cast<std::uint8_t>(input + 1U)] = static_cast<std::byte>(operand);
                    for (const unsigned status : {0U, 2U, 8U, 0x20U, 0xDFU, 0xFFU}) {
                        r.status = static_cast<std::uint8_t>(status);
                        const auto result = run();
                        require(result.has_value() == ((status & 0x20U) == 0), "Gate T guard");
                        if (!result) { continue; }
                        auto expected = r;
                        unsigned cycles = 15;
                        if (routine.logical_pc == 0x5CCBU) {
                            expected.program_counter = (status & 2U) ? 0x5CCDU : 0x5CD5U;
                            cycles = (status & 2U) ? 6U : 12U;
                        } else {
                            unsigned value = operand;
                            if (routine.logical_pc == 0xDA3EU) {
                                expected.y = static_cast<std::uint8_t>(operand);
                                expected.stack_pointer = static_cast<std::uint8_t>(input + 1U);
                                expected.program_counter = 0xDA3FU;
                                cycles = 12;
                            } else {
                                if (routine.logical_pc == 0x5CC8U) { value |= input; }
                                expected.accumulator = static_cast<std::uint8_t>(value);
                                expected.program_counter += 3U;
                            }
                            expected.status = static_cast<std::uint8_t>((status & 0x7DU) | (value == 0U ? 2U : value & 0x80U));
                        }
                        require(result->output_registers == expected && result->return_instruction == RoutineKey{routine.physical_bank, expected.program_counter}, "Gate live operand or stack state");
                        require(result->body_master_cycles == cycles && result->checkpoints.size() == 1
                            && result->work_ram_writes.empty() && result->absorbed_returns.empty()
                            && result->checkpoints[0].instruction == routine
                            && result->checkpoints[0].registers == expected
                            && result->checkpoints[0].master_cycle_offset == cycles
                            && result->checkpoints[0].interrupt_sample_master_cycle_offset == cycles - 3U, "Gate instruction timing");
                    }
                }
            }
            r.status = 0;
            for (unsigned horizon = 0; horizon <= 16; ++horizon) {
                entry.master_cycles_until_user_event = horizon;
                entry.master_cycles_until_timer_interrupt = horizon;
                require(run().has_value(), "Gate retains checkpoint at every horizon");
            }
            ++r.program_counter; require(!run(), "Gate PC guard"); --r.program_counter;
            r.mpr[1] = 0; require(!run(), "Gate RAM mapping guard"); r.mpr[1] = 0xF8U;
            r.mpr[r.program_counter >> 13U] ^= 1U; require(!run(), "Gate code mapping guard");
            r.mpr[r.program_counter >> 13U] ^= 1U;
            require(!devils_crush::app::detail::replace_ranked_gate_instruction(entry, std::span(ram).first(0x1FFF)), "Gate RAM size guard");
        }
        std::puts("Verified 1,572,864 gate/stack operand/status cases and mapping guards.");
        for (const unsigned pc : {0x45A4U, 0x4567U, 0x456AU}) {
            RoutineEntry entry{};
            entry.routine = {2U, static_cast<std::uint16_t>(pc)};
            auto& r = entry.registers;
            r.program_counter = static_cast<std::uint16_t>(pc);
            r.mpr[1] = 0xF8U; r.mpr[2] = 2U; r.mpr[3] = 3U;
            r.accumulator = 0xA5U; r.stack_pointer = 0xFFU;
            const auto load = [&] { return devils_crush::app::detail::replace_little_skull_gate_load(entry, ram); };
            for (unsigned index = 0; index < 256; ++index) {
                r.x = static_cast<std::uint8_t>(index);
                r.y = static_cast<std::uint8_t>(255U - index);
                const auto address = pc == 0x45A4U ? 0xCFAU : pc == 0x4567U ? 0x1E00U + r.x : 0xCF8U + r.y;
                for (unsigned value = 0; value < 256; ++value) {
                    ram[address] = static_cast<std::byte>(value);
                    for (const unsigned status : {0U, 2U, 8U, 0x20U, 0xDFU, 0xFFU}) {
                        r.status = static_cast<std::uint8_t>(status);
                        const auto result = load();
                        require(result.has_value() == ((status & 0x20U) == 0U), "Skull load transfer guard");
                        if (!result) { continue; }
                        auto expected = r;
                        if (pc == 0x4567U) { expected.y = static_cast<std::uint8_t>(value); }
                        else { expected.accumulator = static_cast<std::uint8_t>(value); }
                        expected.status = static_cast<std::uint8_t>((status & 0x7DU) | (value == 0 ? 2U : value & 0x80U));
                        expected.program_counter += 3U;
                        require(result->output_registers == expected && result->return_instruction == RoutineKey{2U, expected.program_counter}, "Skull live load state");
                        require(result->body_master_cycles == 15U && result->checkpoints.size() == 1U
                            && result->work_ram_writes.empty() && result->absorbed_returns.empty()
                            && result->checkpoints[0].interrupt_sample_master_cycle_offset == 12U
                            && result->checkpoints[0].master_cycle_offset == 15U
                            && result->checkpoints[0].registers == expected
                            && result->checkpoints[0].instruction == entry.routine, "Skull load timing");
                    }
                }
            }
            r.status = 0;
            for (unsigned horizon = 0; horizon <= 16; ++horizon) {
                entry.master_cycles_until_user_event = horizon;
                entry.master_cycles_until_timer_interrupt = horizon;
                require(load().has_value(), "Single load must retain its event checkpoint");
            }
            ++r.program_counter; require(!load(), "Skull PC guard"); --r.program_counter;
            r.mpr[1] = 0; require(!load(), "Skull RAM mapping guard"); r.mpr[1] = 0xF8U;
            r.mpr[2] = 0; require(!load(), "Skull code mapping guard"); r.mpr[2] = 2U; r.mpr[3] = 3U;
            require(!devils_crush::app::detail::replace_little_skull_gate_load(entry, std::span(ram).first(0x1FFF)), "Skull RAM size guard");
        }
        std::puts("Verified 1,179,648 skull load index/value/status cases and mapping guards.");
        for (unsigned attributes = 0; attributes < 256; ++attributes) {
            for (unsigned status = 0; status < 256; ++status) {
                for (unsigned slot : {0U, 127U, 255U}) {
                    RoutineEntry entry{};
                    entry.routine = {1U, 0xC627U};
                    auto& r = entry.registers;
                    r.program_counter = 0xC627U; r.mpr[1] = 0xF8U; r.mpr[6] = 1U;
                    r.accumulator = 0xA5U; r.x = static_cast<std::uint8_t>(slot);
                    r.y = 0x37U; r.stack_pointer = 0xFEU; r.status = static_cast<std::uint8_t>(status);
                    ram[0x4D] = static_cast<std::byte>(slot ^ 0x7FU);
                    ram[0x1080U + slot] = static_cast<std::byte>(attributes);
                    const auto before = ram;
                    const auto result = devils_crush::app::detail::replace_actor_queue_reload(entry, ram);
                    require(result.has_value() == ((status & 0x20U) == 0), "Queue reload transfer guard");
                    if (!result) { continue; }
                    const bool suppressed = (attributes & 0x20U) != 0;
                    const bool first = (attributes & 4U) != 0;
                    const auto pc = suppressed ? 0xC654U : first ? 0xC632U : 0xC63EU;
                    const auto cost = suppressed ? 33U : first ? 39U : 45U;
                    const auto flags = (status & ~0xC2U) | (!suppressed && !first ? 2U : 0U);
                    const auto& out = result->output_registers;
                    require(out.program_counter == pc && out.status == flags && out.accumulator == attributes
                        && out.x == r.x && out.y == r.y && out.stack_pointer == r.stack_pointer
                        && out.mpr == r.mpr && ram == before, "Queue reload live state");
                    require(result->body_master_cycles == cost && result->checkpoints.size() == (suppressed ? 3U : 5U)
                        && result->work_ram_writes.empty() && result->absorbed_returns.empty(), "Queue reload effects");
                    constexpr std::array<unsigned, 5> pcs{0xC627, 0xC62A, 0xC62C, 0xC62E, 0xC630};
                    for (std::size_t i = 0; i < result->checkpoints.size(); ++i) {
                        const auto& c = result->checkpoints[i];
                        const auto cycles = i == 0 ? 15U : i == 1 ? 21U : i == 2 ? (suppressed ? 33U : 27U)
                            : i == 3 ? 33U : cost;
                        require(c.instruction == RoutineKey{1U, static_cast<std::uint16_t>(pcs[i])}
                            && c.master_cycle_offset == cycles && c.interrupt_sample_master_cycle_offset == cycles - 3,
                            "Queue reload instruction timing");
                    }
                }
            }
        }
        {
            RoutineEntry entry{};
            entry.routine = {1U, 0xC627U};
            entry.registers.program_counter = 0xC627U;
            entry.registers.mpr[1] = 0xF8U; entry.registers.mpr[6] = 1U;
            const auto reload = [&] { return devils_crush::app::detail::replace_actor_queue_reload(entry, ram); };
            entry.registers.program_counter = 0xC624U;
            require(!reload(), "Queue live PC guard");
            entry.registers.program_counter = 0xC627U;
            entry.registers.mpr[1] = 0;
            require(!reload(), "Queue RAM mapping guard");
            entry.registers.mpr[1] = 0xF8U; entry.registers.mpr[6] = 2U;
            require(!reload(), "Queue code mapping guard");
            entry.registers.mpr[6] = 1U;
            require(!devils_crush::app::detail::replace_actor_queue_reload(entry,
                std::span<const std::byte>(ram).first(0x1FFFU)), "Queue RAM size guard");
        }
        std::puts("Verified 196,608 live queue attribute/status/slot cases.");
        for (const auto pc : {0x4095U, 0x4097U}) {
            RoutineEntry entry{};
            entry.routine = {0x45U, static_cast<std::uint16_t>(pc)};
            auto& r = entry.registers;
            r.program_counter = static_cast<std::uint16_t>(pc);
            r.mpr[2] = 0x45U; r.x = 0xFFU; r.y = 0x37U; r.stack_pointer = 0xFEU;
            entry.master_cycles_until_user_event = 100U;
            entry.master_cycles_until_timer_interrupt = 100U;
            const auto gate = [&] { return devils_crush::app::detail::replace_music_voice_gate(entry); };
            for (unsigned a = 0U; a < 256U; ++a) {
                for (unsigned status = 0U; status < 256U; ++status) {
                    r.accumulator = static_cast<std::uint8_t>(a);
                    r.status = static_cast<std::uint8_t>(status);
                    const bool mask = pc == 0x4095U;
                    const bool taken = !mask && (status & 2U) != 0U;
                    const unsigned cycles = taken ? 12U : 6U;
                    const auto result = gate();
                    require(result.has_value() == ((status & 0x20U) == 0U), "Music gate transfer guard changed");
                    if (!result) { continue; }
                    const auto& out = result->output_registers;
                    require(out.accumulator == (mask ? a % 2U : a)
                        && out.status == (mask ? ((status & 0x7DU) | (a % 2U == 0U ? 2U : 0U)) : status)
                        && out.program_counter == (mask ? 0x4097U : taken ? 0x409CU : 0x4099U)
                        && out.x == r.x && out.y == r.y && out.stack_pointer == r.stack_pointer && out.mpr == r.mpr
                        && result->return_instruction == RoutineKey{0x45U, out.program_counter},
                        "Music gate output changed");
                    require(result->body_master_cycles == cycles && result->checkpoints.size() == 1U
                        && result->checkpoints[0].interrupt_sample_master_cycle_offset == cycles - 3U
                        && result->checkpoints[0].master_cycle_offset == cycles
                        && result->checkpoints[0].instruction == entry.routine
                        && result->checkpoints[0].registers.accumulator == out.accumulator
                        && result->checkpoints[0].registers.status == out.status
                        && result->checkpoints[0].registers.program_counter == out.program_counter
                        && result->work_ram_writes.empty() && result->absorbed_returns.empty(),
                        "Music gate timing or side effects changed");
                    for (unsigned horizon = 0U; horizon <= 13U; ++horizon) {
                        entry.master_cycles_until_user_event = horizon;
                        require(gate().has_value() == (horizon > cycles), "Music gate user horizon changed");
                        entry.master_cycles_until_user_event = 100U;
                        entry.master_cycles_until_timer_interrupt = horizon;
                        require(gate().has_value() == (horizon > cycles), "Music gate timer horizon changed");
                        entry.master_cycles_until_timer_interrupt = 100U;
                    }
                }
            }
            r.status = 0U;
            r.mpr[2] = 0U; require(!gate(), "Foreign music gate code admitted"); r.mpr[2] = 0x45U;
            r.program_counter++; require(!gate(), "Wrong music gate PC admitted"); r.program_counter--;
            entry.routine.logical_pc++; r.program_counter++;
            require(!gate(), "Unsupported music gate key admitted");
        }
        std::puts("Verified 131,072 music gate accumulator/status cases and both event horizons.");
        for (const auto pc : {0xCB68U, 0xFA67U}) {
            const bool collision = pc == 0xCB68U;
            RoutineEntry entry{};
            entry.routine = {static_cast<std::uint8_t>(collision ? 1U : 0U),
                             static_cast<std::uint16_t>(pc)};
            auto& r = entry.registers;
            r.program_counter = static_cast<std::uint16_t>(pc);
            r.mpr[1] = 0xF8U; r.mpr[6] = 1U;
            r.accumulator = 0xA5U; r.x = 0xFFU; r.y = 0x37U; r.stack_pointer = 0xFEU;
            entry.master_cycles_until_user_event = 100U;
            entry.master_cycles_until_timer_interrupt = 100U;
            const auto branch = [&] { return devils_crush::app::detail::replace_bit_branch(entry, ram); };
            for (unsigned value = 0U; value < 256U; ++value) {
                ram[collision ? 0x77U : 0x26U] = static_cast<std::byte>(value);
                const bool taken = collision ? (value & 2U) != 0U : (value & 1U) == 0U;
                const unsigned cycles = taken ? 24U : 18U;
                for (unsigned status = 0U; status < 256U; ++status) {
                    r.status = static_cast<std::uint8_t>(status);
                    const auto result = branch();
                    require(result.has_value() == ((status & 0x20U) == 0U), "Bit branch transfer guard changed");
                    if (!result) { continue; }
                    const auto& out = result->output_registers;
                    require(out.program_counter == (collision ? (taken ? 0xCB93U : 0xCB6BU)
                                                             : (taken ? 0xFA6EU : 0xFA6AU))
                        && out.accumulator == r.accumulator && out.x == r.x && out.y == r.y
                        && out.stack_pointer == r.stack_pointer && out.status == status && out.mpr == r.mpr
                        && result->return_instruction == RoutineKey{entry.routine.physical_bank, out.program_counter},
                        "Bit branch arm or preserved registers changed");
                    require(result->body_master_cycles == cycles && result->checkpoints.size() == 1U
                        && result->checkpoints[0].interrupt_sample_master_cycle_offset == cycles - 3U
                        && result->checkpoints[0].master_cycle_offset == cycles
                        && result->checkpoints[0].instruction == entry.routine
                        && result->checkpoints[0].registers.program_counter == out.program_counter
                        && result->work_ram_writes.empty() && result->absorbed_returns.empty()
                        && !result->native_bbr0_instruction && !result->native_bbs2_instruction,
                        "Bit branch timing, boundaries or side effects changed");
                }
                r.status = 0U;
                const unsigned event_limit = collision ? cycles : 12U;
                for (unsigned horizon = 0U; horizon <= 25U; ++horizon) {
                    entry.master_cycles_until_user_event = horizon;
                    require(branch().has_value() == (horizon > event_limit), "Bit branch user horizon changed");
                    entry.master_cycles_until_user_event = 100U;
                    entry.master_cycles_until_timer_interrupt = horizon;
                    require(branch().has_value() == (horizon > event_limit), "Bit branch timer horizon changed");
                    entry.master_cycles_until_timer_interrupt = 100U;
                }
            }
            r.mpr[1] = 0U; require(!branch(), "Foreign bit branch RAM mapping admitted"); r.mpr[1] = 0xF8U;
            r.mpr[pc >> 13U] ^= 1U; require(!branch(), "Foreign bit branch code mapping admitted");
            r.mpr[pc >> 13U] ^= 1U;
            r.program_counter++; require(!branch(), "Wrong bit branch PC admitted"); r.program_counter--;
            require(!devils_crush::app::detail::replace_bit_branch(entry, std::span<const std::byte>(ram).first(0x1FFFU)),
                    "Short bit branch RAM admitted");
            entry.routine.logical_pc++; r.program_counter++;
            require(!branch(), "Unsupported bit branch key admitted");
        }
        std::puts("Verified 131,072 bit-branch operand/status cases and both horizons for every operand.");
        for (const auto pc : {0xFC03U, 0xFC89U, 0xE479U, 0xE003U}) {
            RoutineEntry entry{};
            entry.routine = {0U, static_cast<std::uint16_t>(pc)};
            auto& r = entry.registers;
            r.program_counter = static_cast<std::uint16_t>(pc);
            r.accumulator = 0xA5U; r.x = 0xFFU; r.y = 0x37U;
            entry.master_cycles_until_user_event = 100U;
            entry.master_cycles_until_timer_interrupt = 100U;
            const auto jump = [&] { return devils_crush::app::detail::replace_unconditional_branch(entry); };
            for (unsigned sp = 0U; sp < 256U; ++sp) {
                for (unsigned status = 0U; status < 256U; ++status) {
                    r.stack_pointer = static_cast<std::uint8_t>(sp);
                    r.status = static_cast<std::uint8_t>(status);
                    const auto result = jump();
                    require(result.has_value() == ((status & 0x20U) == 0U), "Jump transfer-mode guard changed");
                    if (!result) { continue; }
                    const auto& out = result->output_registers;
                    require(out.program_counter == (pc == 0xE479U ? 0xE438U : pc == 0xE003U ? 0xE179U : pc == 0xFC03U ? 0xE003U : 0xFC03U)
                        && out.stack_pointer == sp && out.status == status
                        && out.accumulator == r.accumulator && out.x == r.x && out.y == r.y
                        && out.mpr == r.mpr && result->return_instruction == RoutineKey{0U, out.program_counter},
                        "Jump destination or preserved registers changed");
                    require(result->body_master_cycles == 12U && result->checkpoints.size() == 1U
                        && result->checkpoints[0].interrupt_sample_master_cycle_offset == 9U
                        && result->checkpoints[0].master_cycle_offset == 12U
                        && result->checkpoints[0].instruction == entry.routine
                        && result->checkpoints[0].registers.program_counter == out.program_counter
                        && result->work_ram_writes.empty() && result->absorbed_returns.empty(),
                        "Jump timing or side effects changed");
                }
            }
            r.status = 0U;
            for (unsigned horizon = 0U; horizon <= 13U; ++horizon) {
                entry.master_cycles_until_user_event = horizon;
                require(jump().has_value() == (horizon > 12U), "Jump user horizon guard changed");
                entry.master_cycles_until_user_event = 100U;
                entry.master_cycles_until_timer_interrupt = horizon;
                require(jump().has_value() == (horizon > 12U), "Jump timer horizon guard changed");
                entry.master_cycles_until_timer_interrupt = 100U;
            }
            r.mpr[7] = 1U; require(!jump(), "Foreign jump code mapping admitted");
            r.mpr[7] = 0U; r.program_counter++;
            require(!jump(), "Wrong jump PC admitted");
            r.program_counter = 0xFC04U; entry.routine.logical_pc = r.program_counter;
            require(!jump(), "Unsupported jump key admitted");
        }
        std::puts("Verified 262,144 unconditional-branch stack/status cases and both horizon guards.");
        for (const auto pc : {0xC693U, 0xC696U, 0xC69BU, 0xC6A6U, 0xC6BFU, 0x51D2U, 0xC624U, 0x5F73U, 0x5674U, 0xE5AFU, 0xE5ECU, 0xE5F1U, 0xE5F8U, 0x7C34U, 0x7643U, 0xC9C3U, 0xC9D3U, 0xE32CU}) {
            RoutineEntry entry{};
            entry.routine = {static_cast<std::uint8_t>((pc == 0x7C34U || pc == 0x7643U) ? 3U : pc >= 0xE000U ? 0U : (pc == 0x51D2U || pc == 0x5F73U || pc == 0x5674U) ? 2U : 1U), static_cast<std::uint16_t>(pc)};
            auto& r = entry.registers;
            r.program_counter = static_cast<std::uint16_t>(pc);
            r.mpr[1] = 0xF8U; r.mpr[6] = 1U; r.mpr[2] = 2U; r.mpr[3] = 3U;
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
                    require(out.program_counter == ((pc == 0xC693U || pc == 0xC69BU) ? 0xC5EAU : pc == 0x51D2U ? 0x5475U : pc == 0xC624U ? 0xF605U : (pc == 0x5F73U || (pc == 0x7C34U || pc == 0x7643U)) ? 0xC018U : pc == 0x5674U ? 0x570AU : pc == 0xE32CU ? 0xE321U : pc == 0xC9C3U ? 0xE5ECU : pc == 0xC9D3U ? 0xE85DU : pc == 0xE5AFU ? 0xE5BCU : pc == 0xE5ECU ? 0xE813U : pc == 0xE5F1U ? 0xE825U : pc == 0xE5F8U ? 0xE59FU : 0xC5C9U)
                        && out.stack_pointer == static_cast<std::uint8_t>(sp - 2U)
                        && out.status == status && out.accumulator == r.accumulator
                        && out.x == r.x && out.y == r.y && out.mpr == r.mpr,
                        "Call changed registers or destination");
                    require(result->return_instruction == RoutineKey{r.mpr[out.program_counter >> 13U], out.program_counter},
                        "Call returned the wrong physical destination");
                    require(result->work_ram_writes.size() == 2U, "Call frame size changed");
                    const auto& high = result->work_ram_writes[0];
                    const auto& low = result->work_ram_writes[1];
                    require(high.logical_address == 0x2100U + sp && high.value == static_cast<std::uint8_t>((pc + 2U) >> 8U)
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
            r.mpr[1] = 0xF8U; r.mpr[pc >> 13U] = static_cast<std::uint8_t>(entry.routine.physical_bank ^ 1U);
            require(!devils_crush::app::detail::replace_coroutine_call(entry, ram), "Foreign call code admitted");
            r.mpr[pc >> 13U] = entry.routine.physical_bank; ++r.program_counter;
            require(!devils_crush::app::detail::replace_coroutine_call(entry, ram), "Wrong call PC admitted");
        }
        std::puts("Verified eighteen scheduler/submission/asset/math calls across all stack pointers and status values.");
        for (const auto routine : {RoutineKey{0x08U, 0x5976U}, RoutineKey{0x01U, 0xC4C8U},
                 RoutineKey{0x08U, 0x5A69U}, RoutineKey{0x00U, 0xE47CU},
                 RoutineKey{0x02U, 0x4F75U}, RoutineKey{0x02U, 0x51D5U}, RoutineKey{0x45U, 0x4B85U}, RoutineKey{0x00U, 0xED85U}, RoutineKey{0x02U, 0x4F92U},
                 RoutineKey{0x09U, 0x704EU}, RoutineKey{0x09U, 0x7186U}, RoutineKey{0x09U, 0x7099U}, RoutineKey{0x09U, 0x71C4U}, RoutineKey{0x09U, 0x7071U}, RoutineKey{0x09U, 0x71A9U}, RoutineKey{0x03U, 0x7D81U}, RoutineKey{0x03U, 0x7E1FU}, RoutineKey{0x09U, 0x703CU}, RoutineKey{0x09U, 0x7174U}, RoutineKey{0x00U, 0xE8E6U}, RoutineKey{0x01U, 0xDA3FU}, RoutineKey{0x00U, 0xE333U}, RoutineKey{0x01U, 0xC8E1U}, RoutineKey{0x09U, 0x70BBU}, RoutineKey{0x09U, 0x71E1U}, RoutineKey{2U, 0x45A9U}, RoutineKey{2U, 0x4571U}, RoutineKey{2U, 0x59F4U}, RoutineKey{2U, 0x4484U}, RoutineKey{2U, 0x56ABU}, RoutineKey{0U, 0xE60AU}, RoutineKey{3U, 0x61B2U}, RoutineKey{2U, 0x5721U}, RoutineKey{0U, 0xE824U}, RoutineKey{0U, 0xE5BBU}, RoutineKey{0U, 0xE5EBU}, RoutineKey{0U, 0xE836U}, RoutineKey{5U, 0x7398U}, RoutineKey{2U, 0x4FE1U}, RoutineKey{4U, 0x5197U}, RoutineKey{8U, 0x4106U}, RoutineKey{2U, 0x4E32U}, RoutineKey{2U, 0x432CU}, RoutineKey{3U, 0x7DE0U}, RoutineKey{3U, 0x7E7EU}, RoutineKey{0U, 0xE8A3U}, RoutineKey{1U, 0xD62DU}, RoutineKey{1U, 0xD68DU}, RoutineKey{0U, 0xF55BU}}) {
            RoutineEntry entry{};
            entry.routine = routine;
            auto& r = entry.registers;
            r.program_counter = routine.logical_pc;
            r.mpr = {0xFFU, 0xF8U, 0x08U, 0x09U, 0x0AU, 0x0BU, 0x01U, 0x00U};
            r.mpr[routine.logical_pc >> 13U] = routine.physical_bank;
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
                    require(result->body_master_cycles == 21U && result->checkpoints.size() == (routine.physical_bank == 0x45U ? 1U : 0U)
                        && result->work_ram_writes.empty() && !result->native_rts_instruction,
                        "Return was not a read-only atomic transcript");
                    if (routine.physical_bank == 0x45U) {
                        const auto& sample = result->checkpoints.front();
                        require(sample.interrupt_sample_master_cycle_offset == 18U
                            && sample.master_cycle_offset == 21U && sample.instruction == routine
                            && sample.registers == out && !sample.samples_interrupt_flag_clear,
                            "Audio RTS lost its restored-status interrupt sample");
                    }
                    require(result->absorbed_returns.size() == 1U
                        && result->absorbed_returns[0].instruction == entry.routine
                        && result->absorbed_returns[0].master_cycle_offset == 0U
                        && result->absorbed_returns[0].registers.stack_pointer == sp,
                        "Return observer lost the live entry frame");
                }
            }
            for (unsigned horizon = 0; horizon <= 22U; ++horizon) {
                entry.master_cycles_until_timer_interrupt = horizon;
                require(replace_upper_scheduler_return(entry, ram).has_value() == (routine.physical_bank == 0x45U || horizon > 21U), "Timer horizon guard changed");
                entry.master_cycles_until_timer_interrupt = 100U;
                entry.master_cycles_until_user_event = horizon;
                require(replace_upper_scheduler_return(entry, ram).has_value() == (routine.physical_bank == 0x45U || horizon > 21U), "User horizon guard changed");
                entry.master_cycles_until_user_event = 100U;
            }
            require(!replace_upper_scheduler_return(entry, std::span(ram).first(0x1FFF)), "Short RAM admitted");
            ++r.program_counter;
            require(!replace_upper_scheduler_return(entry, ram), "Wrong return PC admitted");
            r.program_counter = routine.logical_pc;
            r.mpr[1] = 0U;
            require(!replace_upper_scheduler_return(entry, ram), "Foreign stack mapping admitted");
            r.mpr[1] = 0xF8U;
            r.mpr[routine.logical_pc >> 13U] = static_cast<std::uint8_t>(routine.physical_bank ^ 1U);
            require(!replace_upper_scheduler_return(entry, ram), "Foreign code mapping admitted");
        }
        std::puts("Verified 9,633,792 return target/status/stack cases and both horizon guards.");
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
