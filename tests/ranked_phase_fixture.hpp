// SPDX-License-Identifier: GPL-3.0-or-later
void verify_ranked_phase_instructions()
{
    using devils_crush::app::detail::replace_ranked_phase_instruction;
    enum Op { constant, compare, clear, load, toggle, call };
    struct Case { unsigned bank; std::uint16_t pc, operand; Op op; unsigned cycles, length; };
    const Case cases[] = {
        {3, 0x7AE0, 4, constant, 6, 2}, {3, 0x7AEB, 4, compare, 6, 2},
        {3, 0x7AEF, 0x3980, clear, 15, 3}, {0x45, 0x4148, 0xF5, load, 12, 2},
        {1, 0xD7B3, 0xD950, call, 21, 3}, {3, 0x66A8, 4, constant, 6, 2},
        {3, 0x66B0, 1, toggle, 6, 2}, {3, 0x7ACA, 0xC018, call, 21, 3},
        {3, 0x7AD5, 0xC018, call, 21, 3}
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
            r.accumulator = static_cast<std::uint8_t>(value);
            r.x = r.stack_pointer = static_cast<std::uint8_t>(value);
            r.y = static_cast<std::uint8_t>(255 - value);
            ram.fill(std::byte{0xA5});
            ram[(0xF5 + value) % 256] = static_cast<std::byte>(255 - value);
            for (unsigned p = 0; p < 256; ++p) {
                r.status = static_cast<std::uint8_t>(p);
                const auto result = replace_ranked_phase_instruction(entry, ram);
                require(result.has_value() == ((p & 0x20) == 0), "Phase T guard and decimal acceptance");
                if (!result) { continue; }
                auto expected = r;
                expected.program_counter = static_cast<std::uint16_t>(test.pc + test.length);
                if (test.op == constant || test.op == load || test.op == toggle) {
                    const auto a = test.op == constant ? 4U : test.op == load ? 255U - value : value ^ 1U;
                    expected.accumulator = static_cast<std::uint8_t>(a);
                    expected.status = static_cast<std::uint8_t>((p & 0x7D) | (a == 0 ? 2 : a & 0x80));
                } else if (test.op == compare) {
                    const int difference = static_cast<int>(value) - 4;
                    expected.status = static_cast<std::uint8_t>((p & 0x7C)
                        | (difference >= 0 ? 1 : 0) | (difference == 0 ? 2 : 0)
                        | ((difference < 0 || difference >= 128) ? 0x80 : 0));
                } else if (test.op == call) {
                    expected.program_counter = test.operand;
                    expected.stack_pointer = static_cast<std::uint8_t>((value + 254) % 256);
                }
                require(result->output_registers == expected, "Phase live registers, flags and stack wrap");
                require(result->return_instruction == RoutineKey{expected.mpr[expected.program_counter >> 13], expected.program_counter}, "Phase live return mapping");
                require(result->body_master_cycles == test.cycles && result->checkpoints.size() == 1, "Phase cycle count");
                const auto& cp = result->checkpoints[0];
                require(cp.registers == expected && cp.instruction == entry.routine
                    && cp.interrupt_sample_master_cycle_offset == test.cycles - 3 && cp.master_cycle_offset == test.cycles
                    && !cp.samples_interrupt_flag_clear, "Phase interrupt checkpoint");
                require(result->work_ram_writes.size() == (test.op == call ? 2U : test.op == clear ? 1U : 0U), "Phase write count");
                for (unsigned w = 0; w < result->work_ram_writes.size(); ++w) {
                    const auto& actual = result->work_ram_writes[w];
                    const auto address = test.op == clear ? 0x3980 + value : 0x2100 + ((value + 256 - w) % 256);
                    const auto byte = test.op == clear ? 0 : w == 0 ? (test.pc + 2) >> 8 : (test.pc + 2) & 255;
                    require(actual.instruction == entry.routine && actual.logical_address == address
                        && actual.value == byte && actual.master_cycle_offset == (test.op == clear ? 15U : 0U), "Phase write timing");
                }
            }
        }
        r.status = 0xDF;
        for (unsigned h = 0; h < 23; ++h) {
            entry.master_cycles_until_user_event = h; entry.master_cycles_until_timer_interrupt = 100;
            require(replace_ranked_phase_instruction(entry, ram).has_value(), "Phase user horizon");
            entry.master_cycles_until_user_event = 100; entry.master_cycles_until_timer_interrupt = h;
            require(replace_ranked_phase_instruction(entry, ram).has_value(), "Phase timer horizon");
        }
        require(!replace_ranked_phase_instruction(entry, std::span(ram).first(8191)), "Phase short RAM");
        r.mpr[1] = 0; require(!replace_ranked_phase_instruction(entry, ram), "Phase RAM mapping");
        r.mpr[1] = 0xF8; r.mpr[test.pc >> 13] = 0;
        require(!replace_ranked_phase_instruction(entry, ram), "Phase code mapping");
        r.mpr[test.pc >> 13] = static_cast<std::uint8_t>(test.bank); ++r.program_counter;
        require(!replace_ranked_phase_instruction(entry, ram), "Phase PC guard");
        r.program_counter = test.pc; entry.routine.physical_bank = 0;
        require(!replace_ranked_phase_instruction(entry, ram), "Phase bank guard");
    }
    std::puts("Verified 589,824 phase cases, all status values, indexed and stack wrap, and both horizons.");
}
