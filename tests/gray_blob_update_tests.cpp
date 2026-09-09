// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/app/native_replacements_internal.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>
using namespace devils_crush::emulation;
static void require(bool ok) { if (!ok) { throw std::runtime_error("Gray-Blob update mismatch"); } }
int main() {
    try {
        std::array<std::byte, 0x2000> ram{};
        for (unsigned slot : {0U, 0x50U, 0x56U, 255U}) {
            for (unsigned timer = 0; timer < 256; ++timer) {
                for (unsigned status = 0; status < 256; ++status) {
                    for (unsigned collision : {0U, 0x7FU, 0x80U, 0xFFU}) {
                        RoutineEntry entry{};
                        entry.routine = {3U, 0x68BFU};
                        auto& r = entry.registers;
                        r.program_counter = 0x68BFU; r.mpr[1] = 0xF8U; r.mpr[3] = 3U; r.mpr[6] = 1U;
                        r.x = static_cast<std::uint8_t>(slot); r.y = 0x37U; r.accumulator = 0xA5U;
                        r.status = static_cast<std::uint8_t>(status); r.stack_pointer = 0xFFU;
                        ram[0x100] = std::byte{0xC8}; ram[0x101] = std::byte{0xC5};
                        ram[0x1A00 + slot] = static_cast<std::byte>(timer);
                        ram[0x1B80 + slot] = static_cast<std::byte>(timer);
                        ram[0x1800 + slot] = static_cast<std::byte>(collision);
                        ram[0x1980 + slot] = std::byte{0xA5};
                        ram[0x1B00 + slot] = std::byte{1}; // Never decremented by the steady arm.
                        const auto result = devils_crush::app::detail::replace_gray_blob_live_wait_prefix(entry, ram);
                        require(result.has_value() == ((status & 0x20U) == 0));
                        if (!result) { continue; }
                        const bool hit = collision >= 128;
                        const bool expires = timer == 1;
                        const unsigned remaining = (timer - 1U) & 255U;
                        const unsigned value = hit ? collision : remaining;
                        const unsigned expected_flags = (status & 0x7DU) | (value == 0 ? 2U : value & 128U);
                        const auto& out = result->output_registers;
                        require(out.accumulator == collision && out.x == slot && out.y == r.y
                            && out.status == expected_flags && out.mpr == r.mpr);
                        require(out.program_counter == (hit ? 0x6906U : expires ? 0x68DEU : 0xC5C9U));
                        require(out.stack_pointer == (!hit && !expires ? 1U : 0xFFU));
                        require(result->body_master_cycles == (hit ? 66U : expires ? 87U : 114U) + (expires ? 51U : 0U));
                        const auto& writes = result->work_ram_writes;
                        require(writes.size() == 1U + (expires ? 2U : 0U) + (hit ? 0U : 1U));
                        require(writes[0].logical_address == 0x3A00U + slot && writes[0].value == remaining
                            && writes[0].master_cycle_offset == 21U);
                        if (expires) {
                            require(writes[1].logical_address == 0x3A00U + slot && writes[1].value == 4U
                                && writes[1].master_cycle_offset == 48U);
                            require(writes[2].logical_address == 0x3980U + slot && writes[2].value == 0xA4U
                                && writes[2].master_cycle_offset == 84U);
                        }
                        if (!hit) {
                            require(writes.back().logical_address == 0x3B80U + slot && writes.back().value == remaining
                                && writes.back().master_cycle_offset == 81U + (expires ? 51U : 0U));
                        }
                        for (const auto& checkpoint : result->checkpoints) {
                            require(checkpoint.interrupt_sample_master_cycle_offset + 3U == checkpoint.master_cycle_offset);
                        }
                        require(result->absorbed_returns.size() == (!hit && !expires ? 1U : 0U));
                        if (!result->absorbed_returns.empty()) {
                            require(result->absorbed_returns[0].instruction == RoutineKey{3U, 0x6905U});
                        }
                    }
                }
            }
        }
        RoutineEntry guard{}; guard.routine = {3U, 0x68BFU};
        guard.registers.program_counter = 0x68BFU;
        guard.registers.mpr[1] = 0xF8U; guard.registers.mpr[3] = 3U;
        const auto run = [&] { return devils_crush::app::detail::replace_gray_blob_live_wait_prefix(guard, ram); };
        guard.registers.program_counter = 0x68D9U; require(!run()); guard.registers.program_counter = 0x68BFU;
        guard.registers.mpr[1] = 0; require(!run()); guard.registers.mpr[1] = 0xF8U;
        guard.registers.mpr[3] = 2; require(!run()); guard.registers.mpr[3] = 3;
        require(!devils_crush::app::detail::replace_gray_blob_live_wait_prefix(guard, std::span<const std::byte>(ram).first(0x1FFF)));
        std::puts("Verified 1,048,576 Gray-Blob timer/status/collision/slot cases.");
    } catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}