// SPDX-License-Identifier: GPL-3.0-or-later
void verify_actor_gate_phase_instructions()
{
    using devils_crush::app::detail::replace_actor_gate_phase_instruction;
    std::array<std::byte, 8192> ram{};
    // ROM-derived instruction properties; inputs deliberately separate A from RAM.
    enum Op { load, store, increment, compare, bmi, bne, bcc, jump };
    struct Case { RoutineKey key; Op op; unsigned address, target; bool indexed; };
    const Case cases[] = {
        {{2U, 0x5004U}, load, 0x3800U, 0U, true},
        {{2U, 0x5007U}, bmi, 0U, 0x5016U, false},
        {{2U, 0x5009U}, load, 0x2D4EU, 0U, false},
        {{2U, 0x500CU}, compare, 0U, 0U, false},
        {{2U, 0x500EU}, bne, 0U, 0x5013U, false},
        {{2U, 0x5013U}, jump, 0U, 0x4CDAU, false},
        {{3U, 0x7AE2U}, store, 0x3A00U, 0U, true},
        {{3U, 0x7AE5U}, increment, 0x3980U, 0U, true},
        {{3U, 0x7AE8U}, load, 0x3980U, 0U, true},
        {{3U, 0x7AEDU}, bcc, 0U, 0x7AF2U, false}
    };
    for (const auto& test : cases) {
        RoutineEntry entry{};
        entry.routine = test.key;
        auto& r = entry.registers;
        r.program_counter = test.key.logical_pc;
        r.mpr[1] = 0xF8U; r.mpr[r.program_counter >> 13U] = test.key.physical_bank;
        for (unsigned x = 0; x < 256U; ++x) {
            r.x = static_cast<std::uint8_t>(x); r.y = static_cast<std::uint8_t>(255U - x);
            r.stack_pointer = r.y;
            const auto address = test.address + (test.indexed ? x : 0U);
            for (unsigned value = 0; value < 256U; ++value) {
                r.accumulator = static_cast<std::uint8_t>(value ^ 0xA5U);
                if (address) { ram[address - 0x2000U] = static_cast<std::byte>(value); }
                for (const unsigned p : {0U, 1U, 2U, 3U, 0x84U, 0x85U, 0xDFU, 0x20U, 0x28U, 0xFFU}) {
                    r.status = static_cast<std::uint8_t>(p);
                    const auto result = replace_actor_gate_phase_instruction(entry, ram);
                    require(result.has_value() == ((p & 0x20U) == 0U), "Actor phase T guard");
                    if (!result) { continue; }
                    auto expected = r;
                    unsigned cycles = 15U;
                    expected.program_counter += 3U;
                    const auto inc = static_cast<std::uint8_t>(value + 1U);
                    if (test.op == load || test.op == increment || test.op == compare) {
                        const auto v = test.op == load ? static_cast<std::uint8_t>(value)
                            : test.op == increment ? inc : static_cast<std::uint8_t>(r.accumulator - 2U);
                        expected.status = static_cast<std::uint8_t>((p & 0x7DU) | (v == 0U ? 2U : v & 0x80U));
                        if (test.op == load) { expected.accumulator = v; }
                        if (test.op == increment) { cycles = 21U; }
                        if (test.op == compare) {
                            expected.status = static_cast<std::uint8_t>((expected.status & 0xFEU) | (r.accumulator >= 2U));
                            cycles = 6U; --expected.program_counter;
                        }
                    } else if (test.op == jump) {
                        cycles = 12U; expected.program_counter = static_cast<std::uint16_t>(test.target);
                    } else if (test.op != store) {
                        const bool taken = test.op == bmi ? (p & 0x80U) != 0U : test.op == bne ? (p & 2U) == 0U : (p & 1U) == 0U;
                        cycles = taken ? 12U : 6U;
                        expected.program_counter = static_cast<std::uint16_t>(taken ? test.target : r.program_counter + 2U);
                    }
                    require(result->output_registers == expected, "Actor phase live state");
                    require(result->return_instruction == RoutineKey{r.mpr[expected.program_counter >> 13U], expected.program_counter}, "Actor phase target mapping");
                    require(result->body_master_cycles == cycles && result->checkpoints.size() == 1U, "Actor phase cycles");
                    const auto& cp = result->checkpoints[0];
                    require(cp.instruction == test.key && cp.registers == expected && cp.master_cycle_offset == cycles
                        && cp.interrupt_sample_master_cycle_offset == cycles - 3U, "Actor phase sample");
                    const bool writes = test.op == store || test.op == increment;
                    require(result->work_ram_writes.size() == (writes ? 1U : 0U), "Actor phase write count");
                    if (writes) {
                        const auto& w = result->work_ram_writes[0];
                        require(w.instruction == test.key && w.logical_address == address && w.master_cycle_offset == cycles
                            && w.value == (test.op == store ? r.accumulator : inc), "Actor phase write timing and value");
                    }
                }
            }
        }
        r.status = 0xDFU;
        for (unsigned h = 0U; h <= 22U; ++h) {
            entry.master_cycles_until_user_event = h; entry.master_cycles_until_timer_interrupt = 100U;
            require(replace_actor_gate_phase_instruction(entry, ram).has_value(), "Actor phase user horizon");
            entry.master_cycles_until_user_event = 100U; entry.master_cycles_until_timer_interrupt = h;
            require(replace_actor_gate_phase_instruction(entry, ram).has_value(), "Actor phase timer horizon");
        }
        require(!replace_actor_gate_phase_instruction(entry, std::span(ram).first(8191U)), "Actor phase short RAM");
        r.mpr[1] = 0U; require(!replace_actor_gate_phase_instruction(entry, ram), "Actor phase RAM mapping");
        r.mpr[1] = 0xF8U; r.mpr[r.program_counter >> 13U] ^= 1U;
        require(!replace_actor_gate_phase_instruction(entry, ram), "Actor phase code mapping");
        r.mpr[r.program_counter >> 13U] ^= 1U; ++r.program_counter;
        require(!replace_actor_gate_phase_instruction(entry, ram), "Actor phase PC");
    }
    std::puts("Verified 6,553,600 actor gate/phase cases, mappings and both horizons.");
}
