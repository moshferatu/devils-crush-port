// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/app/native_replacements_internal.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>
using namespace devils_crush::emulation;
using devils_crush::app::detail::replace_actor_scan_preamble;
void require(bool ok, const char* message) { if (!ok) { throw std::runtime_error(message); } }
int main()
{
    try {
        std::array<std::byte, 0x2000> ram{};
        ram.fill(std::byte{0xA5});
        RoutineEntry entry{};
        entry.routine = {1U, 0xC500U};
        auto& r = entry.registers;
        r.program_counter = 0xC500U;
        r.mpr = {0xFFU, 0xF8U, 8U, 9U, 10U, 11U, 1U, 0U};
        r.y = 0x37U; r.stack_pointer = 0xFEU;
        constexpr std::array<unsigned, 7> pcs{0xC500, 0xC502, 0xC505, 0xC508, 0xC50A, 0xC50D, 0xC50E};
        constexpr std::array<unsigned, 7> cycles{12, 27, 42, 48, 63, 69, 81};
        constexpr std::array<unsigned, 5> addresses{0x2056, 0x2CBA, 0x2CBC, 0x2CBB, 0x204D};
        constexpr std::array<unsigned, 5> write_cycles{12, 27, 42, 63, 81};
        for (unsigned status = 0; status < 256; ++status) {
            for (unsigned x = 0; x < 256; ++x) {
                r.status = static_cast<std::uint8_t>(status);
                r.x = static_cast<std::uint8_t>(x);
                r.accumulator = static_cast<std::uint8_t>(255 - x);
                const auto before = ram;
                const auto result = replace_actor_scan_preamble(entry, ram);
                require(result.has_value() == ((status & 0x28U) == 0), "Status guards");
                if (!result) { continue; }
                const auto& out = result->output_registers;
                require(out.program_counter == 0xC510 && out.accumulator == 0x7F && out.x == 0
                    && out.y == r.y && out.stack_pointer == r.stack_pointer && out.mpr == r.mpr
                    && out.status == (status & ~0x82U), "Cursor output registers");
                require(result->body_master_cycles == 81 && result->checkpoints.size() == 7
                    && result->work_ram_writes.size() == 5 && result->absorbed_returns.empty()
                    && ram == before, "Cursor effects");
                for (std::size_t i = 0; i < pcs.size(); ++i) {
                    const auto& c = result->checkpoints[i];
                    require(c.instruction == RoutineKey{1U, static_cast<std::uint16_t>(pcs[i])}
                        && c.master_cycle_offset == cycles[i]
                        && c.interrupt_sample_master_cycle_offset == cycles[i] - 3, "Cursor checkpoint");
                }
                for (std::size_t i = 0; i < addresses.size(); ++i) {
                    const auto& w = result->work_ram_writes[i];
                    require(w.logical_address == addresses[i] && w.value == (i == 3 ? 0x7F : 0)
                        && w.master_cycle_offset == write_cycles[i], "Cursor write");
                }
            }
        }
        r.status = 0;
        r.program_counter = 0xC502;
        require(!replace_actor_scan_preamble(entry, ram), "Live PC guard");
        r.program_counter = 0xC500;
        r.mpr[1] = 0;
        require(!replace_actor_scan_preamble(entry, ram), "RAM mapping guard");
        r.mpr[1] = 0xF8;
        r.mpr[6] = 2;
        require(!replace_actor_scan_preamble(entry, ram), "Code mapping guard");
        r.mpr[6] = 1;
        require(!replace_actor_scan_preamble(entry, std::span<const std::byte>(ram).first(0x1FFF)), "RAM size guard");
        std::puts("Actor scan cursor tests passed: 65536 register/status cases");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}