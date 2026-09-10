// SPDX-License-Identifier: GPL-3.0-or-later
void verify_ranked_coordinate_instructions()
{
    using devils_crush::app::detail::replace_ranked_coordinate_instruction;
    enum Op { lda, sta, adc };
    struct Case { std::uint16_t pc, address; Op op; };
    const Case cases[] = {
        {0xC9B3U, 0x2030U, lda}, {0xC9CBU, 0x202FU, sta},
        {0xC9D1U, 0x2030U, sta}, {0xC9AFU, 0x202FU, lda},
        {0xC9C7U, 0x202FU, lda}, {0xC9C9U, 0x202BU, adc},
        {0xC9CDU, 0x2030U, lda}, {0xC9B1U, 0x202BU, sta},
        {0xC9B5U, 0x202CU, sta}, {0xC9B9U, 0x202FU, sta},
        {0xC9B7U, 0x2037U, lda}, {0xC9BFU, 0x2030U, sta}
    };
    std::array<std::byte, 8192> ram{};
    std::uint64_t checked = 0U;
    for (const auto& test : cases) {
        RoutineEntry entry{};
        entry.routine = {1U, test.pc};
        auto& r = entry.registers;
        r.program_counter = test.pc; r.mpr[1] = 0xF8U; r.mpr[6] = 1U;
        for (unsigned a = 0U; a < 256U; ++a) {
            r.accumulator = r.x = r.stack_pointer = static_cast<std::uint8_t>(a);
            r.y = static_cast<std::uint8_t>(a ^ 0xA5U);
            for (unsigned value = 0U; value < 256U; ++value) {
                ram[test.address - 0x2000U] = static_cast<std::byte>(value);
                for (unsigned p : {0U, 1U, 2U, 3U, 0x44U, 0x45U, 0x84U, 0x85U, 0xDFU, 0x20U, 0x28U, 0xFFU}) {
                    ++checked;
                    r.status = static_cast<std::uint8_t>(p);
                    const auto result = replace_ranked_coordinate_instruction(entry, ram);
                    require(result.has_value() == ((p & 0x20U) == 0U && (test.op != adc || (p & 8U) == 0U)), "Coordinate T/decimal guard");
                    if (!result) { continue; }
                    auto expected = r;
                    expected.program_counter += 2U;
                    if (test.op != sta) {
                        auto next = value;
                        if (test.op == adc) {
                            const int sa = a < 128U ? static_cast<int>(a) : static_cast<int>(a) - 256;
                            const int sb = value < 128U ? static_cast<int>(value) : static_cast<int>(value) - 256;
                            const auto signed_sum = sa + sb + static_cast<int>(p & 1U);
                            const auto sum = a + value + (p & 1U);
                            next = sum & 255U;
                            expected.status = static_cast<std::uint8_t>((p & 0xBEU)
                                | (sum > 255U ? 1U : 0U) | (signed_sum < -128 || signed_sum > 127 ? 0x40U : 0U));
                        }
                        expected.accumulator = static_cast<std::uint8_t>(next);
                        expected.status = static_cast<std::uint8_t>((expected.status & 0x7DU) | (next == 0U ? 2U : next & 0x80U));
                    }
                    require(result->output_registers == expected, "Coordinate live registers and NZCV");
                    require(result->return_instruction == RoutineKey{1U, expected.program_counter}, "Coordinate continuation");
                    require(result->body_master_cycles == 12U && result->checkpoints.size() == 1U, "Coordinate cycles");
                    const auto& cp = result->checkpoints[0];
                    require(cp.registers == expected && cp.instruction == entry.routine
                        && cp.interrupt_sample_master_cycle_offset == 9U && cp.master_cycle_offset == 12U, "Coordinate interrupt timing");
                    require(result->work_ram_writes.size() == (test.op == sta ? 1U : 0U), "Coordinate write count");
                    if (test.op == sta) {
                        const auto& w = result->work_ram_writes[0];
                        require(w.instruction == entry.routine && w.logical_address == test.address
                            && w.value == a && w.master_cycle_offset == 12U, "Coordinate live write and timing");
                    }
                }
            }
        }
        r.status = 0x85U;
        for (unsigned h = 0U; h <= 13U; ++h) {
            entry.master_cycles_until_user_event = h; entry.master_cycles_until_timer_interrupt = 100U;
            require(replace_ranked_coordinate_instruction(entry, ram).has_value(), "Coordinate user horizon");
            entry.master_cycles_until_user_event = 100U; entry.master_cycles_until_timer_interrupt = h;
            require(replace_ranked_coordinate_instruction(entry, ram).has_value(), "Coordinate timer horizon");
        }
        require(!replace_ranked_coordinate_instruction(entry, std::span(ram).first(8191U)), "Coordinate short RAM");
        r.mpr[1] = 0U; require(!replace_ranked_coordinate_instruction(entry, ram), "Coordinate RAM mapping");
        r.mpr[1] = 0xF8U; r.mpr[6] = 2U;
        require(!replace_ranked_coordinate_instruction(entry, ram), "Coordinate code mapping");
        r.mpr[6] = 1U; ++r.program_counter;
        require(!replace_ranked_coordinate_instruction(entry, ram), "Coordinate PC guard");
        r.program_counter = test.pc; entry.routine.physical_bank = 2U;
        require(!replace_ranked_coordinate_instruction(entry, ram), "Coordinate bank guard");
        entry.routine = {1U, 0xC9BBU}; r.program_counter = 0xC9BBU;
        require(!replace_ranked_coordinate_instruction(entry, ram), "Coordinate unrelated instruction");
    }
    require(checked == 9437184U, "Coordinate fixture coverage");
    std::puts("Verified 9,437,184 coordinate cases, all ADC pairs and carries, guards and both horizons.");
}
