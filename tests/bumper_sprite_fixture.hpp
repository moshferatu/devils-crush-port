// SPDX-License-Identifier: GPL-3.0-or-later
#include <vector>

void verify_bumper_sprite_instructions()
{
    using devils_crush::app::detail::replace_bumper_sprite_instruction;
    std::array<std::byte, 0x2000> ram{};
    std::vector<std::byte> rom(0x60000U);
    for (std::size_t i = 0; i < rom.size(); ++i) {
        rom[i] = static_cast<std::byte>((i ^ (i >> 8U) ^ (i >> 13U)) & 255U);
    }
    for (const auto key : {RoutineKey{3U, 0x7A71U}, RoutineKey{3U, 0x7A77U},
             RoutineKey{3U, 0x7A7FU}, RoutineKey{3U, 0x7A82U}, RoutineKey{3U, 0x7ADBU},
             RoutineKey{3U, 0x7ADEU}, RoutineKey{3U, 0x76C5U}, RoutineKey{8U, 0x40DFU}}) {
        RoutineEntry entry{};
        entry.routine = key;
        auto& r = entry.registers;
        r.program_counter = key.logical_pc;
        r.mpr[1] = 0xF8U; r.mpr[r.program_counter >> 13U] = key.physical_bank;
        r.accumulator = 0xA5U; r.y = 0x5AU; r.stack_pointer = 0xFD;
        for (unsigned x = 0; x < 256U; ++x) {
            r.x = static_cast<std::uint8_t>(x);
            for (unsigned value = 0; value < 256U; ++value) {
                ram.fill(std::byte{0x69});
                ram[0x1700U + x] = ram[0x1800U + x] = ram[0x1900U + x]
                    = ram[0x1A00U + x] = static_cast<std::byte>(value);
                for (unsigned status : {0U, 1U, 2U, 8U, 0x80U, 0xDFU, 0xFFU}) {
                    r.status = static_cast<std::uint8_t>(status);
                    const auto result = replace_bumper_sprite_instruction(entry, ram, rom);
                    require(result.has_value() == ((status & 0x20U) == 0U), "Bumper transfer guard");
                    if (!result) { continue; }
                    auto expected = r;
                    unsigned cycles = 15U;
                    unsigned output = value;
                    expected.program_counter = static_cast<std::uint16_t>(key.logical_pc + 3U);
                    const bool branch = key.logical_pc == 0x7A77U || key.logical_pc == 0x7A82U
                        || key.logical_pc == 0x7ADEU;
                    const bool dec = key.logical_pc == 0x7ADBU || key.logical_pc == 0x76C5U;
                    if (branch) {
                        const bool taken = key.logical_pc == 0x7A77U ? !(status & 1U)
                            : key.logical_pc == 0x7A82U ? !(status & 0x80U) : !(status & 2U);
                        expected.program_counter = taken ? (key.logical_pc == 0x7A77U ? 0x7A7CU
                            : key.logical_pc == 0x7A82U ? 0x7A8EU : 0x7AF2U)
                            : static_cast<std::uint16_t>(key.logical_pc + 2U);
                        cycles = taken ? 12U : 6U;
                    } else {
                        if (dec) { output = (value - 1U) & 255U; cycles = 21U; }
                        else if (key.logical_pc == 0x40DFU) { expected.y = static_cast<std::uint8_t>(value); }
                        else { expected.accumulator = static_cast<std::uint8_t>(value); }
                        expected.status = static_cast<std::uint8_t>((status & 0x7DU)
                            | (output == 0U ? 2U : output & 0x80U));
                    }
                    require(result->output_registers == expected, "Bumper live registers/flags");
                    require(result->body_master_cycles == cycles && result->checkpoints.size() == 1U,
                        "Bumper instruction timing");
                    const auto& checkpoint = result->checkpoints.front();
                    require(checkpoint.registers == expected && checkpoint.instruction == key
                        && checkpoint.master_cycle_offset == cycles
                        && checkpoint.interrupt_sample_master_cycle_offset == cycles - 3U,
                        "Bumper interrupt checkpoint");
                    require(result->work_ram_writes.size() == (dec ? 1U : 0U), "Bumper write count");
                    if (dec) {
                        const auto& write = result->work_ram_writes.front();
                        require(write.logical_address == 0x3A00U + x && write.value == output
                            && write.master_cycle_offset == 21U && write.instruction == key,
                            "Timer decrement write address/value/timing");
                    }
                }
            }
        }
        r.status = 0;
        for (unsigned horizon = 0; horizon <= 22U; ++horizon) {
            entry.master_cycles_until_timer_interrupt = horizon;
            entry.master_cycles_until_user_event = horizon;
            require(replace_bumper_sprite_instruction(entry, ram, rom).has_value(), "Bumper horizon checkpoint");
        }
        require(!replace_bumper_sprite_instruction(entry, std::span(ram).first(8191), rom), "Bumper short RAM");
        r.program_counter ^= 1U;
        require(!replace_bumper_sprite_instruction(entry, ram, rom), "Bumper wrong PC");
        r.program_counter ^= 1U; r.mpr[1] = 0;
        require(!replace_bumper_sprite_instruction(entry, ram, rom), "Bumper RAM mapping");
        r.mpr[1] = 0xF8U; r.mpr[r.program_counter >> 13U] = 0;
        require(!replace_bumper_sprite_instruction(entry, ram, rom), "Bumper code mapping");
    }
    for (const auto pc : {0x40F1U, 0x40F6U, 0x40FBU}) {
        for (unsigned i = 0; i < ram.size(); ++i) { ram[i] = static_cast<std::byte>((i ^ (i >> 8U)) & 255U); }
        for (unsigned pointer = 0; pointer < 65536U; ++pointer) {
            for (const unsigned mapping : {0x03U, 0x25U, 0x45U, 0xF8U, 0xF7U, 0xFFU}) {
                RoutineEntry entry{};
                entry.routine = {8U, static_cast<std::uint16_t>(pc)};
                auto& r = entry.registers;
                r.program_counter = static_cast<std::uint16_t>(pc);
                r.mpr.fill(static_cast<std::uint8_t>(mapping));
                r.mpr[1] = 0xF8U; r.mpr[2] = 8U;
                r.y = static_cast<std::uint8_t>(pointer >> 8U);
                r.x = 0xA5; r.stack_pointer = 0xFF; r.accumulator = 0x69;
                r.status = static_cast<std::uint8_t>((pointer >> 3U) & 0xDFU);
                const unsigned zp = pc == 0x40FBU ? 0x2FU : 0x2DU;
                ram[zp] = static_cast<std::byte>(pointer & 255U);
                ram[zp + 1U] = static_cast<std::byte>(pointer >> 8U);
                const auto address = (pointer + r.y) & 65535U;
                const auto bank = r.mpr[address >> 13U];
                const auto result = replace_bumper_sprite_instruction(entry, ram, rom);
                require(result.has_value() == (bank < 0x80U || bank == 0xF8U), "Sprite device mapping guard");
                if (!result) { continue; }
                const auto physical = bank < 0x40U ? bank & 31U : (bank & 15U) + 32U;
                const auto value = std::to_integer<std::uint8_t>(bank == 0xF8U ? ram[address & 8191U]
                    : rom[physical * 8192U + (address & 8191U)]);
                auto expected = r;
                expected.accumulator = value;
                expected.status = static_cast<std::uint8_t>((r.status & 0x7DU) | (value ? value & 128U : 2U));
                expected.program_counter = static_cast<std::uint16_t>(pc + 2U);
                require(result->output_registers == expected && result->body_master_cycles == 21U
                    && result->work_ram_writes.empty(), "Sprite live pointer/mapping/registers");
                require(result->checkpoints.size() == 1U
                    && result->checkpoints.front().interrupt_sample_master_cycle_offset == 18U
                    && result->checkpoints.front().master_cycle_offset == 21U
                    && result->checkpoints.front().registers == expected, "Sprite read checkpoint");
            }
        }
    }
    std::puts("Bumper/sprite live instruction, pointer wrapping, mapping and timing fixtures passed");
}
