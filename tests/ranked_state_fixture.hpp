// SPDX-License-Identifier: GPL-3.0-or-later
void verify_ranked_state_instructions()
{
    using devils_crush::app::detail::replace_ranked_state_instruction;
    enum Op { lda, sta, ora, adc, inc, phx, plx, jsr };
    struct Case { RoutineKey key; Op op; unsigned address, cycles, length; bool indexed; };
    const Case cases[] = {
        {{0U, 0xE5BAU}, plx, 0U, 12U, 1U, false},
        {{0U, 0xE59FU}, phx, 0U, 9U, 1U, false},
        {{2U, 0x4D34U}, lda, 0x3E80U, 15U, 3U, true},
        {{2U, 0x5677U}, lda, 0x2D00U, 15U, 3U, false},
        {{3U, 0x76EFU}, jsr, 0x5955U, 21U, 3U, false},
        {{2U, 0x4D3BU}, lda, 0x3D80U, 15U, 3U, true},
        {{3U, 0x76D0U}, inc, 0x3A80U, 21U, 3U, true},
        {{2U, 0x567AU}, ora, 0x2D01U, 15U, 3U, false},
        {{3U, 0x76E9U}, jsr, 0xC042U, 21U, 3U, false},
        {{1U, 0xC9C1U}, sta, 0x202EU, 12U, 2U, false},
        {{1U, 0xC9BDU}, lda, 0x2038U, 12U, 2U, false},
        {{1U, 0xC9CFU}, adc, 0x202CU, 12U, 2U, false}
    };
    std::array<std::byte, 8192> ram{};
    std::uint64_t checked = 0U;
    for (const auto& test : cases) {
        RoutineEntry entry{};
        entry.routine = test.key;
        auto& r = entry.registers;
        r.program_counter = test.key.logical_pc;
        r.mpr[1] = 0xF8U; r.mpr[2] = 2U; r.mpr[6] = 1U;
        r.mpr[r.program_counter >> 13U] = test.key.physical_bank;
        for (unsigned index = 0; index < 256; ++index) {
            r.x = r.accumulator = r.stack_pointer = static_cast<std::uint8_t>(index);
            r.y = static_cast<std::uint8_t>(index ^ 0xA5U);
            const auto address = test.address + (test.indexed ? index : 0U);
            for (unsigned value = 0; value < 256; ++value) {
                if (test.op != jsr && address) { ram[address - 0x2000U] = static_cast<std::byte>(value); }
                ram[0x100U + ((index + 1U) & 255U)] = static_cast<std::byte>(value);
                for (unsigned p : {0U, 1U, 2U, 3U, 0x44U, 0x45U, 0x84U, 0x85U, 0xDFU, 0x20U, 0x28U, 0xFFU}) {
                    ++checked;
                    r.status = static_cast<std::uint8_t>(p);
                    auto result = replace_ranked_state_instruction(entry, ram);
                    require(result.has_value() == ((p & 0x20U) == 0U && (test.op != adc || (p & 8U) == 0U)), "Ranked state T/decimal guard");
                    if (!result) { continue; }
                    auto expected = r;
                    expected.program_counter += static_cast<std::uint16_t>(test.length);
                    unsigned writes = 0U;
                    unsigned written = index;
                    if (test.op == jsr) {
                        expected.program_counter = static_cast<std::uint16_t>(address);
                        expected.stack_pointer = static_cast<std::uint8_t>(index - 2U);
                        writes = 2U;
                    } else if (test.op == phx) {
                        --expected.stack_pointer; writes = 1U;
                    } else if (test.op == sta) { writes = 1U; }
                    else {
                        unsigned v = value;
                        if (test.op == inc) { v = (value + 1U) & 255U; written = v; writes = 1U; }
                        if (test.op == ora) { v = index | value; }
                        if (test.op == adc) {
                            const auto sum = index + value + (p & 1U);
                            const int a = index < 128U ? static_cast<int>(index) : static_cast<int>(index) - 256;
                            const int b = value < 128U ? static_cast<int>(value) : static_cast<int>(value) - 256;
                            const int signed_sum = a + b + static_cast<int>(p & 1U);
                            expected.status = static_cast<std::uint8_t>((p & 0xBEU)
                                | (sum > 255U ? 1U : 0U) | (signed_sum < -128 || signed_sum > 127 ? 0x40U : 0U));
                            v = sum & 255U;
                        }
                        if (test.op == plx) { ++expected.stack_pointer; expected.x = static_cast<std::uint8_t>(v); }
                        else if (test.op != inc) { expected.accumulator = static_cast<std::uint8_t>(v); }
                        expected.status = static_cast<std::uint8_t>((expected.status & 0x7DU) | (v == 0U ? 2U : v & 0x80U));
                    }
                    require(result->output_registers == expected, "Ranked state registers and flags");
                    require(result->return_instruction == RoutineKey{r.mpr[expected.program_counter >> 13U], expected.program_counter}, "Ranked state live target mapping");
                    require(result->body_master_cycles == test.cycles && result->checkpoints.size() == 1U, "Ranked state cycles");
                    const auto& cp = result->checkpoints[0];
                    require(cp.registers == expected && cp.instruction == test.key && cp.master_cycle_offset == test.cycles
                        && cp.interrupt_sample_master_cycle_offset == test.cycles - 3U, "Ranked state interrupt sample");
                    require(result->work_ram_writes.size() == writes, "Ranked state write count");
                    for (unsigned w = 0U; w < writes; ++w) {
                        const auto& actual = result->work_ram_writes[w];
                        const auto saved = test.key.logical_pc + 2U;
                        require(actual.instruction == test.key
                            && actual.logical_address == (test.op == jsr ? 0x2100U + ((index - w) & 255U) : test.op == phx ? 0x2100U + index : address)
                            && actual.value == (test.op == jsr ? (w == 0U ? saved >> 8U : saved & 255U) : written)
                            && actual.master_cycle_offset == (test.op == jsr ? 0U : test.cycles), "Ranked state write value/address/timing");
                    }
                }
            }
        }
        r.status = 0x85U;
        for (unsigned h = 0U; h <= 22U; ++h) {
            entry.master_cycles_until_user_event = h; entry.master_cycles_until_timer_interrupt = 100U;
            require(replace_ranked_state_instruction(entry, ram).has_value(), "Ranked state user horizon");
            entry.master_cycles_until_user_event = 100U; entry.master_cycles_until_timer_interrupt = h;
            require(replace_ranked_state_instruction(entry, ram).has_value(), "Ranked state timer horizon");
        }
        require(!replace_ranked_state_instruction(entry, std::span(ram).first(8191U)), "Ranked state short RAM");
        r.mpr[1] = 0U; require(!replace_ranked_state_instruction(entry, ram), "Ranked state RAM mapping");
        r.mpr[1] = 0xF8U; r.mpr[r.program_counter >> 13U] ^= 1U;
        require(!replace_ranked_state_instruction(entry, ram), "Ranked state code mapping");
        r.mpr[r.program_counter >> 13U] ^= 1U; ++r.program_counter;
        require(!replace_ranked_state_instruction(entry, ram), "Ranked state PC guard");
    }
    require(checked == 9437184U, "Ranked state fixture coverage");
    std::puts("Verified 9,437,184 ranked state cases, all ADC/ORA pairs, wrapping stacks and both horizons.");
}
