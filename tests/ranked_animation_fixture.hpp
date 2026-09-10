// SPDX-License-Identifier: GPL-3.0-or-later
void verify_ranked_animation_instructions()
{
    using devils_crush::app::detail::replace_ranked_animation_instruction;
    enum Op { lda, sta, ora, cmp, bit_and, beq, bne, jsr };
    struct Case { RoutineKey key; Op op; unsigned address; };
    const Case cases[] = {
        {{2U, 0x4D40U}, bne, 0x4D53U}, {{3U, 0x788EU}, jsr, 0xC018U},
        {{3U, 0x76C8U}, beq, 0x76CBU}, {{3U, 0x76D3U}, lda, 0x3A80U},
        {{3U, 0x76CDU}, sta, 0x3A00U}, {{3U, 0x76D6U}, cmp, 0x3E80U},
        {{3U, 0x76ECU}, lda, 0x3A80U}, {{1U, 0xC8DDU}, bit_and, 1U},
        {{3U, 0x7DCCU}, lda, 0x3B00U}, {{1U, 0xC8DFU}, beq, 0xC8E2U},
        {{3U, 0x7DD5U}, sta, 0x3B00U}, {{3U, 0x7DD8U}, ora, 0x3B80U}
    };
    std::array<std::byte, 8192> ram{};
    std::uint64_t checked = 0U;
    for (const auto& test : cases) {
        RoutineEntry entry{};
        entry.routine = test.key;
        auto& r = entry.registers;
        r.program_counter = test.key.logical_pc;
        r.mpr[1] = 0xF8U; r.mpr[6] = 0x31U;
        r.mpr[r.program_counter >> 13U] = test.key.physical_bank;
        for (unsigned index = 0; index < 256; ++index) {
            r.x = r.accumulator = r.stack_pointer = static_cast<std::uint8_t>(index);
            r.y = static_cast<std::uint8_t>(index ^ 0xA5U);
            for (unsigned value = 0; value < 256; ++value) {
                const bool memory = test.op == lda || test.op == sta || test.op == ora || test.op == cmp;
                const auto address = test.address + index;
                if (memory) { ram[address - 0x2000U] = static_cast<std::byte>(value); }
                for (unsigned p : {0U, 1U, 2U, 3U, 0x44U, 0x45U, 0x84U, 0x85U, 0xDFU, 0x20U, 0x28U, 0xFFU}) {
                    ++checked; r.status = static_cast<std::uint8_t>(p);
                    auto result = replace_ranked_animation_instruction(entry, ram);
                    require(result.has_value() == ((p & 0x20U) == 0U), "Ranked animation T guard and decimal acceptance");
                    if (!result) { continue; }
                    auto expected = r;
                    unsigned cycles = memory ? 15U : 6U;
                    expected.program_counter += static_cast<std::uint16_t>(memory || test.op == jsr ? 3U : 2U);
                    unsigned writes = test.op == sta ? 1U : test.op == jsr ? 2U : 0U;
                    if (test.op == jsr) {
                        expected.program_counter = static_cast<std::uint16_t>(test.address);
                        expected.stack_pointer = static_cast<std::uint8_t>(index - 2U); cycles = 21U;
                    } else if (test.op == beq || test.op == bne) {
                        const bool taken = test.op == beq ? (p & 2U) != 0U : (p & 2U) == 0U;
                        if (taken) { expected.program_counter = static_cast<std::uint16_t>(test.address); cycles = 12U; }
                    } else if (test.op != sta) {
                        unsigned v = value;
                        if (test.op == ora) { v = index | value; }
                        if (test.op == bit_and) { v = index % 2U; }
                        if (test.op == cmp) {
                            const int difference = static_cast<int>(index) - static_cast<int>(value);
                            v = static_cast<unsigned>(difference + 256) % 256U;
                            expected.status = static_cast<std::uint8_t>((p & 0xFEU) | (difference >= 0 ? 1U : 0U));
                        } else { expected.accumulator = static_cast<std::uint8_t>(v); }
                        expected.status = static_cast<std::uint8_t>((expected.status & 0x7DU) | (v == 0U ? 2U : v & 0x80U));
                    }
                    require(result->output_registers == expected, "Ranked animation registers and flags");
                    require(result->return_instruction == RoutineKey{r.mpr[expected.program_counter >> 13U], expected.program_counter}, "Ranked animation live target mapping");
                    require(result->body_master_cycles == cycles && result->checkpoints.size() == 1U, "Ranked animation cycles");
                    const auto& cp = result->checkpoints[0];
                    require(cp.registers == expected && cp.instruction == test.key && cp.master_cycle_offset == cycles
                        && cp.interrupt_sample_master_cycle_offset == cycles - 3U, "Ranked animation interrupt sample");
                    require(result->work_ram_writes.size() == writes, "Ranked animation write count");
                    for (unsigned w = 0U; w < writes; ++w) {
                        const auto& actual = result->work_ram_writes[w];
                        require(actual.instruction == test.key
                            && actual.logical_address == (test.op == jsr ? 0x2100U + ((index - w) & 255U) : address)
                            && actual.value == (test.op == jsr ? (w == 0U ? 0x78U : 0x90U) : index)
                            && actual.master_cycle_offset == (test.op == jsr ? 0U : cycles), "Ranked animation write value/address/timing");
                    }
                }
            }
        }
        r.status = 0xDFU;
        for (unsigned h = 0U; h <= 22U; ++h) {
            entry.master_cycles_until_user_event = h; entry.master_cycles_until_timer_interrupt = 100U;
            require(replace_ranked_animation_instruction(entry, ram).has_value(), "Ranked animation user horizon");
            entry.master_cycles_until_user_event = 100U; entry.master_cycles_until_timer_interrupt = h;
            require(replace_ranked_animation_instruction(entry, ram).has_value(), "Ranked animation timer horizon");
        }
        require(!replace_ranked_animation_instruction(entry, std::span(ram).first(8191U)), "Ranked animation short RAM");
        r.mpr[1] = 0U; require(!replace_ranked_animation_instruction(entry, ram), "Ranked animation RAM mapping");
        r.mpr[1] = 0xF8U; r.mpr[r.program_counter >> 13U] ^= 1U;
        require(!replace_ranked_animation_instruction(entry, ram), "Ranked animation code mapping");
        r.mpr[r.program_counter >> 13U] ^= 1U; ++r.program_counter;
        require(!replace_ranked_animation_instruction(entry, ram), "Ranked animation PC guard");
    }
    require(checked == 9437184U, "Ranked animation fixture coverage");
    std::puts("Verified 9,437,184 ranked animation cases, all CMP/ORA pairs, wrapping stacks and both horizons.");
}