// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace devils_crush::cpu {
constexpr bool is_huc6280_branch(std::uint8_t opcode)
{
    return (opcode & 0x1f) == 0x10;
}

// PC addresses the opcode. Untaken branches skip the operand without a bus
// read. Taken branches publish their target before the final interrupt sample.
template<class Advance, class ReadOp, class Sample>
void run_huc6280_branch(std::uint8_t opcode, std::uint32_t& pc,
    std::uint8_t& status, Advance advance, ReadOp read_op, Sample sample)
{
    constexpr std::uint8_t masks[]{0x80, 0x40, 0x01, 0x02};
    const bool taken = ((status & masks[opcode >> 6]) != 0) == ((opcode & 0x20) != 0);
    ++pc;
    if (taken) {
        const auto displacement = static_cast<std::int8_t>(read_op(pc));
        ++pc;
        advance(3);
        pc += static_cast<std::uint32_t>(displacement);
    } else {
        advance(1);
        ++pc;
    }
    sample();
    advance(1);
    status &= static_cast<std::uint8_t>(~0x20u);
}
} // namespace devils_crush::cpu