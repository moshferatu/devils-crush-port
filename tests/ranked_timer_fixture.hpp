// SPDX-License-Identifier: GPL-3.0-or-later
void verify_ranked_timer_instructions()
{
    using namespace devils_crush::app::detail;
    // Table describes the ROM operands independently of the replacement's dispatch.
    struct Case { RoutineKey key; unsigned address; char operation; };
    const Case cases[] = {
        {{3U, 0x66A3U}, 0x3A00U, 'D'}, {{2U, 0x567FU}, 0x3A00U, 'D'},
        {{3U, 0x66AAU}, 0x3A00U, 'S'}, {{3U, 0x66ADU}, 0x3980U, 'L'},
        {{3U, 0x66B2U}, 0x3980U, 'S'}, {{3U, 0x7E6AU}, 0x3B00U, 'L'},
        {{3U, 0x7E73U}, 0x3B00U, 'S'}, {{3U, 0x7E76U}, 0x3B80U, 'O'},
        {{0U, 0xE438U}, 0U, 'C'}
    };
    std::array<std::byte, 0x2000> ram{};
    for (const auto& test : cases) {
        RoutineEntry entry{};
        entry.routine = test.key;
        auto& r = entry.registers;
        r.program_counter = test.key.logical_pc;
        r.mpr[1] = 0xF8U; r.mpr[r.program_counter >> 13U] = test.key.physical_bank;
        r.stack_pointer = 0xFFU; r.y = 0xA5U;
        for (unsigned x = 0U; x < 256U; ++x) {
            r.x = static_cast<std::uint8_t>(x);
            for (unsigned value = 0U; value < 256U; ++value) {
                if (test.address) { ram[test.address + x - 0x2000U] = static_cast<std::byte>(value); }
                r.accumulator = static_cast<std::uint8_t>(255U - value);
                for (unsigned p : {0U, 1U, 2U, 8U, 0x80U, 0xDFU, 0xFFU}) {
                    r.status = static_cast<std::uint8_t>(p);
                    const auto result = replace_ranked_timer_instruction(entry, ram);
                    require(result.has_value() == ((p & 0x20U) == 0U), "Ranked timer T guard");
                    if (!result) { continue; }
                    auto expected = r;
                    expected.program_counter += test.operation == 'C' ? 1U : 3U;
                    unsigned data = r.accumulator;
                    if (test.operation == 'C') { expected.y = 0U; }
                    else if (test.operation != 'S') {
                        data = test.operation == 'D' ? (value + 255U) & 255U
                            : test.operation == 'O' ? value | r.accumulator : value;
                        if (test.operation != 'D') { expected.accumulator = static_cast<std::uint8_t>(data); }
                        expected.status = static_cast<std::uint8_t>((p & 0x7DU) | (data == 0U ? 2U : data & 0x80U));
                    }
                    const unsigned cycles = test.operation == 'C' ? 6U : test.operation == 'D' ? 21U : 15U;
                    require(result->output_registers == expected
                        && result->return_instruction == RoutineKey{test.key.physical_bank, expected.program_counter}, "Ranked timer registers");
                    require(result->body_master_cycles == cycles && result->checkpoints.size() == 1U,
                        "Ranked timer checkpoint count");
                    const auto& cp = result->checkpoints[0];
                    require(cp.registers == expected && cp.instruction == test.key
                        && cp.master_cycle_offset == cycles && cp.interrupt_sample_master_cycle_offset == cycles - 3U,
                        "Ranked timer checkpoint timing");
                    const bool writes = test.operation == 'S' || test.operation == 'D';
                    require(result->work_ram_writes.size() == (writes ? 1U : 0U), "Ranked timer writes");
                    if (writes) {
                        const auto& w = result->work_ram_writes[0];
                        require(w.instruction == test.key && w.value == data && w.logical_address == test.address + x
                            && w.master_cycle_offset == cycles, "Ranked timer write timing");
                    }
                }
            }
        }
        r.status = 0U;
        for (unsigned horizon = 0; horizon <= 22U; ++horizon) {
            entry.master_cycles_until_user_event = horizon; entry.master_cycles_until_timer_interrupt = 100U;
            require(replace_ranked_timer_instruction(entry, ram).has_value(), "Ranked user horizon");
            entry.master_cycles_until_user_event = 100U; entry.master_cycles_until_timer_interrupt = horizon;
            require(replace_ranked_timer_instruction(entry, ram).has_value(), "Ranked timer horizon");
        }
        if (test.address) {
            require(!replace_ranked_timer_instruction(entry, std::span(ram).first(8191U)), "Ranked short RAM");
            r.mpr[1] = 0U; require(!replace_ranked_timer_instruction(entry, ram), "Ranked RAM mapping"); r.mpr[1] = 0xF8U;
        }
        r.mpr[r.program_counter >> 13U] ^= 1U;
        require(!replace_ranked_timer_instruction(entry, ram), "Ranked code mapping");
        r.mpr[r.program_counter >> 13U] ^= 1U; ++r.program_counter;
        require(!replace_ranked_timer_instruction(entry, ram), "Ranked PC guard");
    }
    RoutineEntry or_entry{};
    or_entry.routine = {3U, 0x7E76U};
    or_entry.registers.program_counter = 0x7E76U;
    or_entry.registers.mpr[1] = 0xF8U; or_entry.registers.mpr[3] = 3U;
    or_entry.registers.x = 0xFFU;
    for (unsigned a = 0; a < 256U; ++a) {
        for (unsigned high = 0; high < 256U; ++high) {
            or_entry.registers.accumulator = static_cast<std::uint8_t>(a);
            or_entry.registers.status = 0xDFU;
            ram[0x1C7FU] = static_cast<std::byte>(high);
            const auto result = replace_ranked_timer_instruction(or_entry, ram);
            const auto value = a | high;
            require(result && result->output_registers.accumulator == value
                && result->output_registers.status == (0x5DU | (value == 0U ? 2U : value & 0x80U)),
                "Countdown OR independent accumulator and high byte");
        }
    }
    std::puts("Verified 4,128,768 ranked timer/CLY cases and 65,536 independent countdown OR pairs.");
}
