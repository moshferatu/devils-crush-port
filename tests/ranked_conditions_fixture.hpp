// SPDX-License-Identifier: GPL-3.0-or-later
void verify_ranked_condition_instructions()
{
    using devils_crush::app::detail::replace_ranked_condition_instruction;
    enum Op { call, ret, branch, load, store };
    struct Case { unsigned bank; std::uint16_t pc, operand; Op op; unsigned cycles, length; };
    const Case cases[] = {
        {2, 0x5A05, 0xC018, call, 21, 3}, {3, 0x61A9, 0x61B2, branch, 12, 2},
        {1, 0xC9BB, 0x202D, store, 12, 2}, {2, 0x5A00, 0x2D33, load, 15, 3},
        {2, 0x4D32, 0x4D53, branch, 12, 2}, {2, 0x56A5, 0x20CB, load, 12, 2},
        {2, 0x56A9, 0x20CB, store, 12, 2}, {2, 0x5682, 0x56A5, branch, 12, 2},
        {1, 0xD902, 0xD950, call, 21, 3}, {1, 0xD915, 0, ret, 21, 1},
        {1, 0xD92F, 0, ret, 21, 1}, {1, 0xD907, 0xD950, call, 21, 3}
    };
    std::array<std::byte, 8192> ram{};
    for (const auto& test : cases) {
        RoutineEntry entry{};
        entry.routine = {static_cast<std::uint8_t>(test.bank), test.pc};
        auto& r = entry.registers;
        r.program_counter = test.pc;
        for (unsigned page = 0; page < 8; ++page) { r.mpr[page] = static_cast<std::uint8_t>(page + 8); }
        r.mpr[test.pc >> 13] = static_cast<std::uint8_t>(test.bank); r.mpr[1] = 0xF8;
        for (unsigned value = 0; value < 256; ++value) {
            r.accumulator = static_cast<std::uint8_t>(value ^ 0xA5);
            r.x = r.stack_pointer = static_cast<std::uint8_t>(value);
            r.y = static_cast<std::uint8_t>(255 - value);
            if (test.op == load || test.op == store) { ram[test.operand - 0x2000] = static_cast<std::byte>(value); }
            const auto low_address = 0x100U + ((value + 1U) % 256U);
            const auto high_address = 0x100U + ((value + 2U) % 256U);
            ram[low_address] = static_cast<std::byte>(value);
            ram[high_address] = static_cast<std::byte>(value);
            for (unsigned p = 0; p < 256; ++p) {
                r.status = static_cast<std::uint8_t>(p);
                const auto result = replace_ranked_condition_instruction(entry, ram);
                require(result.has_value() == ((p & 0x20) == 0), "Conditions T guard and decimal acceptance");
                if (!result) { continue; }
                auto expected = r;
                expected.program_counter = static_cast<std::uint16_t>(test.pc + test.length);
                auto cycles = test.cycles;
                if (test.op == branch) {
                    if ((p & 2) == 0) { expected.program_counter = test.operand; }
                    else { cycles = 6; }
                } else if (test.op == load) {
                    expected.accumulator = static_cast<std::uint8_t>(value);
                    expected.status = static_cast<std::uint8_t>((p & 0x7D) | (value == 0 ? 2 : value & 0x80));
                } else if (test.op == call) {
                    expected.program_counter = test.operand;
                    expected.stack_pointer = static_cast<std::uint8_t>((value + 254) % 256);
                } else if (test.op == ret) {
                    expected.program_counter = static_cast<std::uint16_t>(value * 257 + 1);
                    expected.stack_pointer = static_cast<std::uint8_t>((value + 2) % 256);
                }
                require(result->output_registers == expected, "Conditions live registers, flags and stack wrap");
                require(result->return_instruction == RoutineKey{expected.mpr[expected.program_counter >> 13], expected.program_counter}, "Conditions live return mapping");
                require(result->body_master_cycles == cycles && result->checkpoints.size() == 1, "Conditions cycle count");
                const auto& cp = result->checkpoints[0];
                require(cp.registers == expected && cp.instruction == entry.routine
                    && cp.interrupt_sample_master_cycle_offset == cycles - 3 && cp.master_cycle_offset == cycles
                    && !cp.samples_interrupt_flag_clear, "Conditions interrupt checkpoint");
                require(result->work_ram_writes.size() == (test.op == call ? 2U : test.op == store ? 1U : 0U), "Conditions write count");
                for (unsigned w = 0; w < result->work_ram_writes.size(); ++w) {
                    const auto& actual = result->work_ram_writes[w];
                    const auto address = test.op == store ? test.operand : 0x2100 + ((value + 256 - w) % 256);
                    const auto byte = test.op == store ? r.accumulator : w == 0 ? (test.pc + 2) >> 8 : (test.pc + 2) & 255;
                    require(actual.instruction == entry.routine && actual.logical_address == address
                        && actual.value == byte && actual.master_cycle_offset == (test.op == store ? 12U : 0U), "Conditions live write and timing");
                }
            }
        }
        r.status = 0xDF;
        for (unsigned h = 0; h < 23; ++h) {
            entry.master_cycles_until_user_event = h; entry.master_cycles_until_timer_interrupt = 100;
            require(replace_ranked_condition_instruction(entry, ram).has_value(), "Conditions user horizon");
            entry.master_cycles_until_user_event = 100; entry.master_cycles_until_timer_interrupt = h;
            require(replace_ranked_condition_instruction(entry, ram).has_value(), "Conditions timer horizon");
        }
        if (test.op == ret) {
            // Every possible saved PC, including FFFF -> 0000 and stack FF -> 00.
            r.stack_pointer = 0xFF;
            for (unsigned saved = 0; saved < 65536; ++saved) {
                ram[0x100] = static_cast<std::byte>(saved & 255);
                ram[0x101] = static_cast<std::byte>(saved >> 8);
                const auto result = replace_ranked_condition_instruction(entry, ram);
                const auto pc = static_cast<std::uint16_t>(saved + 1);
                require(result && result->output_registers.program_counter == pc
                    && result->output_registers.stack_pointer == 1
                    && result->return_instruction == RoutineKey{r.mpr[pc >> 13], pc}, "Conditions every saved return and bank");
            }
        }
        require(!replace_ranked_condition_instruction(entry, std::span(ram).first(8191)), "Conditions short RAM");
        r.mpr[1] = 0; require(!replace_ranked_condition_instruction(entry, ram), "Conditions RAM mapping");
        r.mpr[1] = 0xF8; r.mpr[test.pc >> 13] = 0;
        require(!replace_ranked_condition_instruction(entry, ram), "Conditions code mapping");
        r.mpr[test.pc >> 13] = static_cast<std::uint8_t>(test.bank); ++r.program_counter;
        require(!replace_ranked_condition_instruction(entry, ram), "Conditions PC guard");
        r.program_counter = test.pc; entry.routine.physical_bank = 0;
        require(!replace_ranked_condition_instruction(entry, ram), "Conditions bank guard");
    }
    std::puts("Verified 786,432 condition cases, 131,072 saved returns, all flags and stack slots, and both horizons.");
}
