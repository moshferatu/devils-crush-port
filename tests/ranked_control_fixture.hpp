// SPDX-License-Identifier: GPL-3.0-or-later
void verify_ranked_control_instructions()
{
    using devils_crush::app::detail::replace_ranked_control_instruction;

    std::array<std::byte, 8192> ram{};
    struct Case { RoutineKey key; unsigned base, cycles, length; };
    const Case cases[] = {
        {{0U, 0xEB1AU}, 0U, 12U, 3U}, {{0U, 0xE32FU}, 0x202CU, 18U, 2U},
        {{3U, 0x7D6FU}, 0x3A00U, 21U, 3U}, {{0x45U, 0x4B36U}, 0x2E45U, 15U, 3U},
        {{3U, 0x7E0DU}, 0x3A00U, 21U, 3U}
    };
    for (const auto& test : cases) {
        RoutineEntry entry{};
        entry.routine = test.key;
        auto& r = entry.registers;
        r.program_counter = test.key.logical_pc;
        r.mpr[1] = 0xF8U; r.mpr[r.program_counter >> 13U] = test.key.physical_bank;
        for (unsigned x = 0; x < 256U; ++x) {
            r.x = static_cast<std::uint8_t>(x); r.y = static_cast<std::uint8_t>(255U - x);
            r.stack_pointer = r.y; r.accumulator = r.x;
            const auto address = test.base + (test.base >= 0x2E45U ? x : 0U);
            for (unsigned value = 0; value < 256U; ++value) {
                if (address) { ram[address - 0x2000U] = static_cast<std::byte>(value); }
                for (const unsigned p : {0U, 2U, 0x85U, 0xDFU, 0x20U, 0x28U, 0xFFU}) {
                    r.status = static_cast<std::uint8_t>(p);
                    const auto result = replace_ranked_control_instruction(entry, ram);
                    require(result.has_value() == ((p & 0x20U) == 0U), "Control T guard");
                    if (!result) { continue; }
                    auto expected = r;
                    expected.program_counter = address ? static_cast<std::uint16_t>(r.program_counter + test.length) : 0xEAB0U;
                    const auto output = static_cast<std::uint8_t>(value - 1U);
                    if (test.base == 0x202CU || test.base == 0x3A00U) {
                        expected.status = static_cast<std::uint8_t>((p & 0x7DU) | (output == 0U ? 2U : output & 0x80U));
                    }
                    require(result->output_registers == expected && result->return_instruction
                        == RoutineKey{r.mpr[expected.program_counter >> 13U], expected.program_counter}, "Control live state");
                    require(result->body_master_cycles == test.cycles && result->checkpoints.size() == 1U, "Control duration");
                    const auto& cp = result->checkpoints[0];
                    require(cp.master_cycle_offset == test.cycles && cp.interrupt_sample_master_cycle_offset == test.cycles - 3U
                        && cp.instruction == test.key && cp.registers == expected, "Control sample and retirement");
                    require(result->work_ram_writes.size() == (address ? 1U : 0U), "Control write count");
                    if (address) {
                        const auto& w = result->work_ram_writes[0];
                        require(w.instruction == test.key && w.logical_address == address && w.master_cycle_offset == test.cycles
                            && w.value == (test.base == 0x2E45U ? 0U : output), "Control timed write");
                    }
                }
            }
        }
        r.status = 0xDFU;
        for (unsigned h = 0; h <= test.cycles + 1U; ++h) {
            entry.master_cycles_until_user_event = h; entry.master_cycles_until_timer_interrupt = 100U;
            require(replace_ranked_control_instruction(entry, ram).has_value(), "Control user horizon");
            entry.master_cycles_until_user_event = 100U; entry.master_cycles_until_timer_interrupt = h;
            require(replace_ranked_control_instruction(entry, ram).has_value(), "Control timer horizon");
        }
        require(replace_ranked_control_instruction(entry, std::span(ram).first(8191U)).has_value() == (test.base == 0U), "Control short RAM");
        r.mpr[1] = 0U;
        require(replace_ranked_control_instruction(entry, ram).has_value() == (test.base == 0U), "Control RAM mapping");
        r.mpr[1] = 0xF8U; r.mpr[r.program_counter >> 13U] ^= 1U;
        require(!replace_ranked_control_instruction(entry, ram), "Control code mapping");
        r.mpr[r.program_counter >> 13U] ^= 1U; ++r.program_counter;
        require(!replace_ranked_control_instruction(entry, ram), "Control PC");
    }
    std::puts("Verified 2,293,760 control cases, mappings and both horizons.");
}
