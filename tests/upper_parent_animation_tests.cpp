// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/app/native_replacements_internal.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>

namespace {
using namespace devils_crush::emulation;
using devils_crush::app::detail::replace_upper_parent_animation;
void require(bool value, const char* message) {
    if (!value) { throw std::runtime_error(message); }
}
void verify() {
    std::array<std::byte, 0x2000> ram{};
    RoutineEntry e{};
    e.registers.mpr = {0xFF, 0xF8, 2, 9, 0, 0, 1, 0};
    e.registers.y = 0x73;
    const auto flags = [](unsigned status, std::uint8_t value) {
        return (status & ~0x82U) | (value == 0U ? 2U : value & 0x80U);
    };
    const auto invoke = [&](std::uint16_t pc) {
        e.routine = {9, pc}; e.registers.program_counter = pc;
        const auto before = ram;
        auto result = replace_upper_parent_animation(e, ram);
        require(result.has_value(), "Supported controller declined");
        require(ram == before, "Input snapshot mutated");
        require(result->output_registers.y == e.registers.y, "Y changed");
        require(result->output_registers.mpr == e.registers.mpr, "MPR changed");
        require(result->checkpoints.size() < 32U && result->work_ram_writes.size() < 16U, "Unbounded segment");
        for (const auto& cp : result->checkpoints) {
            require(cp.interrupt_sample_master_cycle_offset + 3U == cp.master_cycle_offset,
                "Interrupt sample displaced");
        }
        return *result;
    };
    const auto field = [&](const RoutineReplacement& r, std::uint16_t address) {
        for (auto it = r.work_ram_writes.rbegin(); it != r.work_ram_writes.rend(); ++it) {
            if (it->logical_address == address) { return it->value; }
        }
        return std::to_integer<std::uint8_t>(ram[address - 0x2000U]);
    };
    const auto stack = [&](const RoutineReplacement& r, std::uint8_t sp,
                           std::uint16_t saved, std::uint32_t at) {
        const auto n = r.work_ram_writes.size();
        require(n >= 2U, "Missing call stack writes");
        const auto& hi = r.work_ram_writes[n-2U];
        const auto& lo = r.work_ram_writes[n-1U];
        require(hi.logical_address == 0x2100U + sp && hi.value == (saved >> 8U), "JSR high byte");
        require(lo.logical_address == 0x2100U + static_cast<std::uint8_t>(sp-1U)
            && lo.value == (saved & 255U), "JSR low byte/wrap");
        require(hi.master_cycle_offset == at && lo.master_cycle_offset == at, "JSR write time");
        require(r.output_registers.stack_pointer == static_cast<std::uint8_t>(sp-2U), "JSR SP");
    };
    unsigned cases = 0;
    for (unsigned timer = 0; timer < 256U; ++timer) {
        for (unsigned frame = 0; frame < 256U; ++frame) {
            ram.fill(std::byte{0});
            e.registers.x = static_cast<std::uint8_t>(timer + frame);
            e.registers.stack_pointer = static_cast<std::uint8_t>(frame);
            e.registers.status = static_cast<std::uint8_t>(frame & ~0x28U);
            e.registers.accumulator = 0xA5;
            const auto x = e.registers.x;
            ram[0x1A00U+x] = static_cast<std::byte>(timer);
            ram[0x1980U+x] = static_cast<std::byte>(frame);
            auto r = invoke(0x705F);
            require(field(r, static_cast<std::uint16_t>(0x3B00U+x)) == 255U, "Decay wrap");
            const auto rollover = timer == 1U;
            const auto end = rollover && frame == 1U;
            require(field(r, static_cast<std::uint16_t>(0x3A00U+x))
                == (rollover ? 6U : static_cast<std::uint8_t>(timer-1U)), "Decay timer");
            require(field(r, static_cast<std::uint16_t>(0x3980U+x))
                == (rollover ? static_cast<std::uint8_t>(frame-1U) : frame), "Decay frame");
            require(r.return_instruction.logical_pc == (end ? 0xC5C9U : 0x7071U), "Decay exit");
            require(r.body_master_cycles == (end ? 186U : rollover ? 96U : 54U), "Decay cycles");
            const auto decay_value = static_cast<std::uint8_t>(end ? 8U : rollover ? frame-1U : timer-1U);
            require(r.output_registers.status == flags(e.registers.status, decay_value), "Decay flags");
            require(r.output_registers.accumulator == (end ? 8U : rollover ? 6U : 0xA5U), "Decay A");
            if (end) {
                stack(r, e.registers.stack_pointer, 0x7083, 165);
                require(field(r, static_cast<std::uint16_t>(0x3D00U+x)) == 0x81U, "Phase flag");
                require(field(r, static_cast<std::uint16_t>(0x3600U+x)) == 8U
                    && field(r, static_cast<std::uint16_t>(0x3680U+x)) == 8U, "Phase counters");
            }
            r = invoke(0x70A7);
            const auto up_end = rollover && frame == 5U;
            require(r.return_instruction.logical_pc == (up_end ? 0xECFDU : 0x70BBU), "Ascend exit");
            require(r.body_master_cycles == (up_end ? 129U : rollover ? 96U : 33U), "Ascend cycles");
            const auto next_frame = static_cast<std::uint8_t>(frame+1U);
            const auto ascend_status = rollover
                ? flags((e.registers.status & ~1U) | (next_frame >= 6U ? 1U : 0U),
                    static_cast<std::uint8_t>(up_end ? 0x42U : next_frame-6U))
                : flags(e.registers.status, static_cast<std::uint8_t>(timer-1U));
            require(r.output_registers.status == ascend_status, "Ascend flags");
            require(r.output_registers.accumulator == (up_end ? 0x42U : rollover ? next_frame : 0xA5U), "Ascend A");
            if (up_end) { stack(r, e.registers.stack_pointer, 0x70C0, 108); }
            if (rollover) {
                require(field(r, static_cast<std::uint16_t>(0x3980U+x)) == static_cast<std::uint8_t>(frame+1U), "Ascend frame");
                require((r.output_registers.status & 1U) == (static_cast<std::uint8_t>(frame+1U) >= 6U ? 1U : 0U), "CMP carry");
            }
            cases += 2;
        }
    }
    for (unsigned sp = 0; sp < 256U; ++sp) {
        e.registers.stack_pointer = static_cast<std::uint8_t>(sp);
        e.registers.x = 0xFE;
        e.registers.status = 0xC5;
        for (const auto pc : {0x705CU, 0x7084U}) {
            const auto r = invoke(static_cast<std::uint16_t>(pc));
            stack(r, static_cast<std::uint8_t>(sp), static_cast<std::uint16_t>(pc+2U), 0);
            require(r.body_master_cycles == 21U && r.output_registers.status == 0xC5U, "Pure JSR state");
            require(r.return_instruction.physical_bank == 0U, "Live child bank");
            ++cases;
        }
        ram[0x1BFE] = std::byte{1};
        auto r = invoke(0x7094);
        require(r.return_instruction.logical_pc == 0x590CU && r.body_master_cycles == 63U, "Expiry call");
        require(r.work_ram_writes[1].logical_address == 0x2100U+sp
            && r.work_ram_writes[1].value == 0xFEU
            && r.work_ram_writes[1].master_cycle_offset == 42U, "PHX write");
        stack(r, static_cast<std::uint8_t>(sp-1U), 0x709D, 42);
        // The child may change X; PLX restores the live stacked value.
        ram[0x100U+static_cast<std::uint8_t>(sp+1U)] = static_cast<std::byte>(sp);
        r = invoke(0x709E);
        require(r.output_registers.x == sp && r.body_master_cycles == 54U, "PLX restore");
        require(field(r, static_cast<std::uint16_t>(0x3D00U+sp)) == 0x81U, "Restored slot store");
        stack(r, static_cast<std::uint8_t>(sp+1U), 0x70A6, 33);
        cases += 2;
    }
    for (unsigned a = 0; a < 256U; ++a) {
        for (unsigned b = 0; b < 256U; ++b) {
            e.registers.x = 0;
            ram[0x1B00] = static_cast<std::byte>(a);
            ram[0x1B80] = static_cast<std::byte>(b);
            const auto r = invoke(0x7087);
            require(r.return_instruction.logical_pc == (a == b ? 0xECFDU : a == 1U ? 0x590CU : 0x7099U), "Compare route");
            require(r.body_master_cycles == (a == b ? 63U : a == 1U ? 105U : 69U), "Compare cycles");
            require((r.output_registers.status & 1U) == (a >= b ? 1U : 0U), "Compare carry");
            require(r.output_registers.status == flags((e.registers.status & ~1U) | (a >= b ? 1U : 0U),
                static_cast<std::uint8_t>(a == b ? 0x0CU : a-1U)), "Hold flags");
            ++cases;
        }
    }
    for (unsigned timer = 0; timer < 256U; ++timer) {
        for (unsigned frame = 0; frame < 256U; ++frame) {
            ram.fill(std::byte{0});
            e.registers.x = static_cast<std::uint8_t>(timer + frame);
            e.registers.stack_pointer = static_cast<std::uint8_t>(frame);
            e.registers.status = static_cast<std::uint8_t>(frame & ~0x28U);
            ram[0x1A00U + e.registers.x] = static_cast<std::byte>(timer);
            ram[0x1980U + e.registers.x] = static_cast<std::byte>(frame);
            for (const auto ascending : {false, true}) {
                const auto original = invoke(ascending ? 0x70A7 : 0x705F);
                const auto sibling = invoke(ascending ? 0x71CD : 0x7197);
                const auto offset = ascending ? 0x126U : 0x138U;
                require(original.body_master_cycles == sibling.body_master_cycles, "Sibling cycles");
                require(original.output_registers.status == sibling.output_registers.status, "Sibling flags");
                require(original.output_registers.accumulator == sibling.output_registers.accumulator, "Sibling A");
                require(original.checkpoints.size() == sibling.checkpoints.size(), "Sibling checkpoints");
                for (std::size_t i = 0; i < original.checkpoints.size(); ++i) {
                    require(sibling.checkpoints[i].instruction.logical_pc == original.checkpoints[i].instruction.logical_pc + offset,
                        "Sibling checkpoint relocation");
                }
                require(field(original, static_cast<std::uint16_t>(0x3980U + e.registers.x))
                    == field(sibling, static_cast<std::uint16_t>(0x3980U + e.registers.x)), "Sibling frame");
                if (timer == 1U && frame == (ascending ? 5U : 1U)) {
                    stack(sibling, e.registers.stack_pointer, ascending ? 0x71E6 : 0x71BB, ascending ? 108U : 165U);
                }
                ++cases;
            }
        }
        e.registers.x = 0;
        e.registers.stack_pointer = static_cast<std::uint8_t>(timer);
        e.registers.status = 0x41;
        ram[0x1B00] = static_cast<std::byte>(timer);
        const auto hold = invoke(0x71BF);
        require(hold.body_master_cycles == (timer == 1U ? 75U : 27U), "Sibling hold cycles");
        require(hold.return_instruction.logical_pc == (timer == 1U ? 0xC5C9U : 0x71C4U), "Sibling hold route");
        if (timer == 1U) { stack(hold, e.registers.stack_pointer, 0x71CC, 54); }
        for (const auto pc : {0x7194U, 0x71BCU}) {
            const auto call = invoke(static_cast<std::uint16_t>(pc));
            stack(call, e.registers.stack_pointer, static_cast<std::uint16_t>(pc + 2U), 0);
            require(call.return_instruction.logical_pc == 0xF08EU, "Sibling child");
            ++cases;
        }
        ++cases;
    }
    for (const auto flag : {8U, 32U, 40U}) {
        e.registers.status = static_cast<std::uint8_t>(flag);
        require(!replace_upper_parent_animation(e, ram), "D/T guard");
    }
    e.registers.status = 0; e.registers.mpr[1] = 0;
    require(!replace_upper_parent_animation(e, ram), "RAM mapping guard");
    std::printf("Verified %u parent animation counter, comparison and stack cases.\n", cases);
}
}
int main() {
    try { verify(); return 0; }
    catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
