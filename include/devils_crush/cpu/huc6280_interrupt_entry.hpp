// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

namespace devils_crush::cpu {

// A maskable interrupt is indivisible at instruction boundaries, but each
// cycle and bus access remains observable by devices. Register references are
// live: callbacks see the same intermediate PC, S and P as the processor.
// The bus owns mapping and stalls; cycle() owns speed/overclock conversion.
template<class Cycle, class Read, class Write, class Flags, class Sample>
void enter_huc6280_interrupt(
    std::uint32_t& pc, std::uint8_t& stack, std::uint8_t& status,
    std::uint32_t vector, Cycle cycle, Read read, Write write,
    Flags publish_interrupt_mask, Sample sample)
{
    cycle();
    (void)read(pc);
    cycle();
    (void)read(pc + 1);

    const auto push = [&](std::uint8_t value) {
        write(0x2100u | stack, value);
        --stack;
    };
    cycle();
    push(static_cast<std::uint8_t>(pc >> 8));
    cycle();
    push(static_cast<std::uint8_t>(pc));
    cycle();
    push(static_cast<std::uint8_t>(status & ~0x10u));
    status |= 0x04;
    publish_interrupt_mask();
    status &= static_cast<std::uint8_t>(~0x28u);

    cycle();
    pc = read(vector);
    cycle();
    pc |= static_cast<std::uint32_t>(read(vector + 1)) << 8;
    sample();
    cycle();
}

} // namespace devils_crush::cpu
