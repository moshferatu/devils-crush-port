// SPDX-License-Identifier: GPL-3.0-or-later
void verify_eyeball_countdown_instructions()
{
    using namespace devils_crush::app::detail;
    std::array<std::byte, 0x2000> ram{};
    for (const auto pc : {0x4E24U, 0x4E29U, 0x4E2DU, 0x431BU, 0x4324U, 0x4327U}) {
        RoutineEntry entry{};
        entry.routine = {2U, static_cast<std::uint16_t>(pc)};
        auto& r = entry.registers;
        r.program_counter = static_cast<std::uint16_t>(pc);
        r.mpr[1] = 0xF8U; r.mpr[2] = 2U;
        r.y = 0xA5U; r.stack_pointer = 0xFFU;
        const auto run = [&] { return replace_eyeball_countdown_instruction(entry, ram); };
        for (unsigned x = 0; x < 256U; ++x) {
            r.x = static_cast<std::uint8_t>(x);
            ram[0x53U] = ram[0x1800U + x] = ram[0x1B00U + x] = ram[0x1B80U + x]
                = static_cast<std::byte>(255U - x);
            for (unsigned a = 0; a < 256U; ++a) {
                r.accumulator = static_cast<std::uint8_t>(a);
                for (unsigned p : {0U, 1U, 2U, 8U, 0x80U, 0xDFU, 0xFFU}) {
                    r.status = static_cast<std::uint8_t>(p);
                    const auto result = run();
                    require(result.has_value() == ((p & 0x20U) == 0U), "Actor instruction transfer guard");
                    if (!result) { continue; }
                    auto expected = r;
                    unsigned cycles = pc == 0x4E29U ? 12U : 15U;
                    expected.program_counter = static_cast<std::uint16_t>(pc + (pc == 0x4E29U ? 2U : 3U));
                    if (pc == 0x4E2DU) {
                        expected.program_counter = (p & 2U) ? 0x4E32U : 0x4E2FU;
                        cycles = (p & 2U) ? 12U : 6U;
                    } else if (pc != 0x4324U) {
                        expected.accumulator = static_cast<std::uint8_t>(pc == 0x4327U ? a | (255U - x) : 255U - x);
                        expected.status = static_cast<std::uint8_t>((p & 0x7DU)
                            | (expected.accumulator == 0U ? 2U : expected.accumulator & 0x80U));
                    }
                    require(result->output_registers == expected
                        && result->return_instruction == RoutineKey{2U, expected.program_counter}, "Actor live registers");
                    require(result->body_master_cycles == cycles && result->checkpoints.size() == 1U
                        && result->checkpoints[0].registers == expected
                        && result->checkpoints[0].instruction == entry.routine
                        && result->checkpoints[0].master_cycle_offset == cycles
                        && result->checkpoints[0].interrupt_sample_master_cycle_offset == cycles - 3U,
                        "Actor instruction timing");
                    require(result->work_ram_writes.size() == (pc == 0x4324U ? 1U : 0U), "Actor write count");
                    if (pc == 0x4324U) {
                        const auto& w = result->work_ram_writes[0];
                        require(w.logical_address == 0x3B00U + x && w.value == a
                            && w.master_cycle_offset == 15U && w.instruction == entry.routine, "Countdown live store");
                    }
                }
            }
        }
        r.status = 0U;
        for (unsigned h = 0; h <= 22U; ++h) {
            entry.master_cycles_until_user_event = h; entry.master_cycles_until_timer_interrupt = 100U;
            require(run().has_value(), "Actor user horizon");
            entry.master_cycles_until_user_event = 100U; entry.master_cycles_until_timer_interrupt = h;
            require(run().has_value(), "Actor timer horizon");
        }
        require(!replace_eyeball_countdown_instruction(entry, std::span(ram).first(8191)), "Actor short RAM");
        r.mpr[1] = 0U; require(!run(), "Actor RAM mapping"); r.mpr[1] = 0xF8U;
        r.mpr[2] = 3U; require(!run(), "Actor code mapping"); r.mpr[2] = 2U;
        ++r.program_counter; require(!run(), "Actor PC");
    }
    std::puts("Verified 2,752,512 Eyeball/countdown operand, accumulator and status cases.");
    for (const auto sei_pc : {0x49E3U, 0x485DU}) {
        RoutineEntry entry{};
        entry.routine = {0x45U, static_cast<std::uint16_t>(sei_pc)};
        auto& r = entry.registers;
        r.program_counter = static_cast<std::uint16_t>(sei_pc); r.mpr[2] = 0x45U;
        r.accumulator = 0xA5U; r.x = 0xFFU; r.y = 0x69U; r.stack_pointer = 0xFEU;
        for (unsigned p = 0; p < 256U; ++p) {
            r.status = static_cast<std::uint8_t>(p);
            for (unsigned h = 0; h <= 7U; ++h) {
                entry.master_cycles_until_user_event = h;
                entry.master_cycles_until_timer_interrupt = 7U - h;
                const auto result = replace_music_commit_sei(entry);
                require(result.has_value() == ((p & 0x20U) == 0U), "SEI transfer guard");
                if (!result) { continue; }
                auto expected = r; expected.status |= 4U; expected.program_counter++;
                require(result->output_registers == expected && result->body_master_cycles == 6U
                    && result->return_instruction == RoutineKey{0x45U, static_cast<std::uint16_t>(sei_pc + 1U)}
                    && result->work_ram_writes.empty() && result->checkpoints.size() == 1U, "SEI retirement");
                const auto& c = result->checkpoints[0];
                require(c.registers == expected && c.instruction == entry.routine
                    && c.interrupt_sample_master_cycle_offset == 3U && c.master_cycle_offset == 6U
                    && c.samples_interrupt_flag_clear == ((p & 4U) == 0U), "SEI old-mask sample");
            }
        }
        r.status = 0U; r.mpr[2] = 0U; require(!replace_music_commit_sei(entry), "SEI code mapping");
        r.mpr[2] = 0x45U; ++r.program_counter; require(!replace_music_commit_sei(entry), "SEI PC");
        std::puts("Verified 2,048 SEI status/horizon cases including the old-mask sample.");
        }
}
