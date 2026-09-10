// SPDX-License-Identifier: GPL-3.0-or-later
void verify_ranked_operand_instructions()
{
    using devils_crush::app::detail::replace_ranked_operand_instruction;
    enum Op { lda, sta, ora, eor, beq, bpl };
    struct Case { RoutineKey key; Op op; unsigned address, target, cycles; bool indexed; };
    // Properties taken from the pinned ROM; no actor classification supplies A or flags.
    const Case cases[] = {
        {{2U, 0x4D2DU}, lda, 0x3E00U, 0U, 15U, true},
        {{1U, 0xC8DAU}, lda, 0x3C80U, 0U, 15U, true},
        {{1U, 0xDA09U}, sta, 0x2028U, 0U, 12U, false},
        {{3U, 0x61A3U}, lda, 0x2D00U, 0U, 15U, false},
        {{3U, 0x61A6U}, ora, 0x2D01U, 0U, 15U, false},
        {{0U, 0xE5A4U}, lda, 0x2030U, 0U, 12U, false},
        {{0U, 0xE5ABU}, lda, 0x202FU, 0U, 12U, false},
        {{0U, 0xE5A6U}, beq, 0U, 0xE5ABU, 12U, false},
        {{0U, 0xE5F4U}, eor, 0x2046U, 0U, 12U, false},
        {{0U, 0xE5F6U}, sta, 0x2046U, 0U, 12U, false},
        {{0U, 0xE5FDU}, bpl, 0U, 0xE60AU, 12U, false}
    };
    std::array<std::byte, 8192> ram{};
    for (const auto& test : cases) {
        RoutineEntry entry{};
        entry.routine = test.key;
        auto& r = entry.registers;
        r.program_counter = test.key.logical_pc;
        r.mpr[1] = 0xF8U; r.mpr[r.program_counter >> 13U] = test.key.physical_bank;
        for (unsigned index = 0U; index < 256U; ++index) {
            r.x = static_cast<std::uint8_t>(index); r.y = static_cast<std::uint8_t>(index ^ 0xA5U);
            r.stack_pointer = r.y;
            const auto address = test.address + (test.indexed ? index : 0U);
            for (unsigned value = 0U; value < 256U; ++value) {
                // For ORA/EOR this covers every A/operand pair independently.
                r.accumulator = static_cast<std::uint8_t>(index);
                if (address) { ram[address - 0x2000U] = static_cast<std::byte>(value); }
                for (unsigned p : {0U, 1U, 2U, 3U, 0x84U, 0x85U, 0xDFU, 0x20U, 0x28U, 0xFFU}) {
                    r.status = static_cast<std::uint8_t>(p);
                    const auto result = replace_ranked_operand_instruction(entry, ram);
                    require(result.has_value() == ((p & 0x20U) == 0U), "Ranked operand T guard");
                    if (!result) { continue; }
                    auto expected = r;
                    unsigned cycles = test.cycles;
                    expected.program_counter += static_cast<std::uint16_t>(cycles == 15U ? 3U : 2U);
                    if (test.op == beq || test.op == bpl) {
                        const bool taken = test.op == beq ? (p & 2U) != 0U : (p & 0x80U) == 0U;
                        if (taken) { expected.program_counter = static_cast<std::uint16_t>(test.target); }
                        else { cycles = 6U; }
                    } else if (test.op != sta) {
                        const auto v = static_cast<std::uint8_t>(test.op == ora ? index | value
                            : test.op == eor ? index ^ value : value);
                        expected.accumulator = v;
                        expected.status = static_cast<std::uint8_t>((p & 0x7DU) | (v == 0U ? 2U : v & 0x80U));
                    }
                    require(result->output_registers == expected, "Ranked operand live registers");
                    require(result->return_instruction == RoutineKey{r.mpr[expected.program_counter >> 13U], expected.program_counter}, "Ranked operand target mapping");
                    require(result->body_master_cycles == cycles && result->checkpoints.size() == 1U, "Ranked operand cycles");
                    const auto& cp = result->checkpoints[0];
                    require(cp.instruction == test.key && cp.registers == expected && cp.master_cycle_offset == cycles
                        && cp.interrupt_sample_master_cycle_offset == cycles - 3U, "Ranked operand interrupt sample");
                    require(result->work_ram_writes.size() == (test.op == sta ? 1U : 0U), "Ranked operand writes");
                    if (test.op == sta) {
                        const auto& w = result->work_ram_writes[0];
                        require(w.instruction == test.key && w.logical_address == address && w.value == r.accumulator
                            && w.master_cycle_offset == 12U, "Ranked operand store timing");
                    }
                }
            }
        }
        r.status = 0xDFU;
        for (unsigned h = 0U; h <= 22U; ++h) {
            entry.master_cycles_until_user_event = h; entry.master_cycles_until_timer_interrupt = 100U;
            require(replace_ranked_operand_instruction(entry, ram).has_value(), "Ranked operand user horizon");
            entry.master_cycles_until_user_event = 100U; entry.master_cycles_until_timer_interrupt = h;
            require(replace_ranked_operand_instruction(entry, ram).has_value(), "Ranked operand timer horizon");
        }
        require(!replace_ranked_operand_instruction(entry, std::span(ram).first(8191U)), "Ranked operand short RAM");
        r.mpr[1] = 0U; require(!replace_ranked_operand_instruction(entry, ram), "Ranked operand RAM mapping");
        r.mpr[1] = 0xF8U; r.mpr[r.program_counter >> 13U] ^= 1U;
        require(!replace_ranked_operand_instruction(entry, ram), "Ranked operand code mapping");
        r.mpr[r.program_counter >> 13U] ^= 1U; ++r.program_counter;
        require(!replace_ranked_operand_instruction(entry, ram), "Ranked operand PC guard");
    }
    std::puts("Verified 7,208,960 ranked operand cases, all ORA/EOR pairs and both horizons.");
}
