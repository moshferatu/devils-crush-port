// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/app/native_replacements_internal.hpp"
#include <stdexcept>
using namespace devils_crush::emulation;
int main() {
    RoutineEntry e{};
    e.routine = {1, 0xCBF9};
    e.registers.program_counter = 0xCBF9;
    e.registers.accumulator = 0xA5;
    e.registers.x = 0x2F;
    e.registers.y = 0x04;
    e.registers.mpr = {0xFF, 0xF8, 2, 3, 0, 0, 1, 0};
    // No speculative stack reads or D/T normalization: the original opcode
    // owns all status combinations, stack wrap and live target resolution.
    for (unsigned status = 0; status < 256; ++status) {
        for (unsigned sp = 0; sp < 256; ++sp) {
            e.registers.status = static_cast<std::uint8_t>(status);
            e.registers.stack_pointer = static_cast<std::uint8_t>(sp);
            const auto r = devils_crush::app::detail::replace_flipper_owner_rts(e);
            if (!r || !r->native_rts_instruction || r->body_master_cycles != 21
                || !r->checkpoints.empty() || !r->work_ram_writes.empty()
                || r->return_instruction != e.routine
                || r->output_registers != e.registers) {
                throw std::runtime_error("Collision vector descriptor changed live RTS state");
            }
        }
    }
    e.routine = {2, 0xCBF9};
    if (devils_crush::app::detail::replace_flipper_owner_rts(e)) {
        throw std::runtime_error("Collision vector descriptor accepted wrong bank");
    }
}
