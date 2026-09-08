// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/app/native_replacements_internal.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

using namespace devils_crush::emulation;
using devils_crush::app::detail::replace_resident_pattern_loader;

namespace {
void require(bool condition, const char* message)
{
    if (!condition) { throw std::runtime_error(message); }
}
}

int main()
{
    try {
        std::array<std::byte, 0x2000> ram{};
        std::vector<std::byte> rom(0x60000);
        for (unsigned i = 0; i < 257U; ++i) {
            rom[0x1783U + i] = static_cast<std::byte>((i ^ 0xA5U) & 255U);
        }
        RoutineEntry entry{};
        entry.routine = {0x00U, 0xF768U};
        auto& r = entry.registers;
        r.program_counter = 0xF768U;
        r.mpr = {0xFFU, 0xF8U, 0x02U, 0x03U, 0x1EU, 0x1FU, 0x01U, 0x00U};
        for (unsigned sp : {0U, 1U, 249U, 255U}) {
            r.stack_pointer = static_cast<std::uint8_t>(sp);
            for (unsigned selector = 0; selector < 256U; ++selector) {
                r.accumulator = static_cast<std::uint8_t>(selector);
                r.x = static_cast<std::uint8_t>(selector ^ 0x37U);
                r.y = static_cast<std::uint8_t>(selector ^ 0x05U);
                for (unsigned status = 0; status < 256U; ++status) {
                    if ((status & 0x28U) != 0U) { continue; }
                    r.status = static_cast<std::uint8_t>(status);
                    const auto result = replace_resident_pattern_loader(entry, ram, rom);
                    require(result.has_value(), "Pattern selector declined");
                    require(result->checkpoints.size() == 8U, "Lost an instruction boundary");
                    constexpr std::array<unsigned, 8> times{9, 15, 21, 36, 48, 63, 75, 96};
                    constexpr std::array<unsigned, 8> pcs{
                        0xF768, 0xF769, 0xF76A, 0xF76B, 0xF76E, 0xF770, 0xF773, 0xF775};
                    for (std::size_t i = 0; i < times.size(); ++i) {
                        const auto& cp = result->checkpoints[i];
                        require(cp.instruction.logical_pc == pcs[i]
                            && cp.master_cycle_offset == times[i]
                            && cp.interrupt_sample_master_cycle_offset == times[i] - 3U,
                            "Incorrect instruction or interrupt timing");
                    }
                    const auto index = static_cast<std::uint8_t>(selector * 2U);
                    const auto flags = static_cast<std::uint8_t>((status & 0x7CU)
                        | (selector >= 128U ? 1U : 0U)
                        | (index == 0U ? 2U : (index & 0x80U)));
                    const auto& shifted = result->checkpoints[1].registers;
                    require(shifted.accumulator == index && shifted.status == flags
                        && shifted.y == r.y, "ASL did not preserve Y or compute C/N/Z");
                    require(result->checkpoints[2].registers.y == index
                        && result->checkpoints[2].registers.status == flags,
                        "TAY did not select the requested descriptor");
                    const auto& writes = result->work_ram_writes;
                    require(writes.size() == 5U && writes[0].value == r.x
                        && writes[0].logical_address == 0x2100U + sp
                        && writes[0].master_cycle_offset == 9U,
                        "PHX did not save the actor index");
                    require(writes[1].logical_address == 0x202FU
                        && writes[1].value == static_cast<std::uint8_t>(index ^ 0xA5U)
                        && writes[2].logical_address == 0x2030U
                        && writes[2].value == static_cast<std::uint8_t>((index + 1U) ^ 0xA5U),
                        "Descriptor depends on incoming Y rather than doubled A");
                    require(writes[3].master_cycle_offset == 75U
                        && writes[4].master_cycle_offset == 75U
                        && writes[3].value == 0xF7U && writes[4].value == 0x77U
                        && writes[3].logical_address == 0x2100U + static_cast<std::uint8_t>(sp - 1U)
                        && writes[4].logical_address == 0x2100U + static_cast<std::uint8_t>(sp - 2U),
                        "JSR stack writes lost their entry timing or page wrap");
                    require(result->output_registers.x == r.x
                        && result->output_registers.y == index
                        && result->output_registers.stack_pointer == static_cast<std::uint8_t>(sp - 3U)
                        && result->return_instruction == RoutineKey{0x00U, 0xE1FDU}
                        && result->body_master_cycles == 96U, "Mapper handoff changed");
                }
            }
        }
        // The captured face request has A=1, X=$36, Y=5: PLX must restore $36.
        entry.routine = {0x00U, 0xF781U};
        r.program_counter = 0xF781U;
        r.stack_pointer = 0xF8U;
        r.status = 0U;
        ram[0x1F9U] = std::byte{0x36};
        ram[0x1FAU] = std::byte{0x6B};
        ram[0x1FBU] = std::byte{0x75};
        const auto tail = replace_resident_pattern_loader(entry, ram, rom);
        require(tail && tail->output_registers.x == 0x36U
            && tail->output_registers.stack_pointer == 0xFBU
            && tail->return_instruction == RoutineKey{0x03U, 0x756CU},
            "Return did not restore the face actor and its caller");
        std::puts("Verified 65,536 selector/status/stack cases and the captured face return.");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
