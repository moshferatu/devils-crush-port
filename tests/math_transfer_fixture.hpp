// SPDX-License-Identifier: GPL-3.0-or-later
void verify_math_transfer_instructions()
{
    using devils_crush::app::detail::replace_math_transfer_instruction;
    struct Case { std::uint16_t pc; unsigned address; char operation; };
    const Case cases[] = {
        {0xE5ADU, 0x2030U, 'S'}, {0xE5B8U, 0x202FU, 'S'},
        {0xE5B2U, 0x2043U, 'L'}, {0xE5B4U, 0x2030U, 'S'},
        {0xE5EFU, 0x2046U, 'S'}, {0xE5B6U, 0x2042U, 'L'},
        {0xE5FBU, 0x2046U, 'L'}, {0xE5A0U, 0x2042U, 'Z'},
        {0xE5A2U, 0x2043U, 'Z'}
    };
    std::array<std::byte, 8192> ram{};
    for (const auto& test : cases) {
        RoutineEntry entry{};
        entry.routine = {0U, test.pc};
        auto& r = entry.registers;
        r.program_counter = test.pc; r.mpr[1] = 0xF8U;
        for (unsigned value = 0; value < 256U; ++value) {
            ram[test.address - 0x2000U] = static_cast<std::byte>(value);
            r.accumulator = static_cast<std::uint8_t>(255U - value);
            r.x = static_cast<std::uint8_t>(value); r.y = r.accumulator;
            r.stack_pointer = static_cast<std::uint8_t>(value ^ 0xA5U);
            for (unsigned p = 0; p < 256U; ++p) {
                r.status = static_cast<std::uint8_t>(p);
                const auto result = replace_math_transfer_instruction(entry, ram);
                require(result.has_value() == ((p & 0x20U) == 0U), "Math transfer T guard");
                if (!result) { continue; }
                auto expected = r;
                expected.program_counter += 2U;
                if (test.operation == 'L') {
                    expected.accumulator = static_cast<std::uint8_t>(value);
                    expected.status = static_cast<std::uint8_t>((p & 0x7DU)
                        | (value == 0U ? 2U : value & 0x80U));
                }
                require(result->output_registers == expected
                    && result->return_instruction == RoutineKey{0U, expected.program_counter},
                    "Math transfer live registers");
                require(result->body_master_cycles == 12U && result->checkpoints.size() == 1U,
                    "Math transfer checkpoint count");
                const auto& cp = result->checkpoints[0];
                require(cp.interrupt_sample_master_cycle_offset == 9U && cp.master_cycle_offset == 12U
                    && cp.instruction == entry.routine && cp.registers == expected,
                    "Math transfer sampling and retirement");
                require(result->work_ram_writes.size() == (test.operation == 'L' ? 0U : 1U),
                    "Math transfer write count");
                if (test.operation != 'L') {
                    const auto& w = result->work_ram_writes[0];
                    require(w.logical_address == test.address && w.instruction == entry.routine
                        && w.value == (test.operation == 'Z' ? 0U : r.accumulator)
                        && w.master_cycle_offset == 12U, "Math transfer write value and timing");
                }
            }
        }
        r.status = 0xDFU;
        for (unsigned horizon = 0; horizon <= 13U; ++horizon) {
            entry.master_cycles_until_user_event = horizon; entry.master_cycles_until_timer_interrupt = 100U;
            require(replace_math_transfer_instruction(entry, ram).has_value(), "Math transfer user horizon");
            entry.master_cycles_until_user_event = 100U; entry.master_cycles_until_timer_interrupt = horizon;
            require(replace_math_transfer_instruction(entry, ram).has_value(), "Math transfer timer horizon");
        }
        require(!replace_math_transfer_instruction(entry, std::span(ram).first(8191U)), "Math transfer short RAM");
        r.mpr[1] = 0U; require(!replace_math_transfer_instruction(entry, ram), "Math transfer RAM mapping");
        r.mpr[1] = 0xF8U; r.mpr[7] = 1U;
        require(!replace_math_transfer_instruction(entry, ram), "Math transfer code mapping");
        r.mpr[7] = 0U; ++r.program_counter;
        require(!replace_math_transfer_instruction(entry, ram), "Math transfer PC guard");
        r.program_counter = 0xE5A4U; entry.routine.logical_pc = r.program_counter;
        require(!replace_math_transfer_instruction(entry, ram), "Math transfer unsupported key");
        r.program_counter = test.pc; entry.routine = {1U, test.pc}; r.mpr[7] = 1U;
        require(!replace_math_transfer_instruction(entry, ram), "Math transfer foreign bank");
    }
    std::puts("Verified 589,824 math transfer value/status cases, sampling, stores and guards.");
}
