// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace devils_crush::cpu {
constexpr bool is_huc6280_bit_branch(std::uint8_t opcode)
{
    return (opcode & 0x0f) == 0x0f;
}

// PC points at the opcode. Preserve both samples and the original clock
// batching: the first sample precedes the live zero-page read. An untaken
// branch does not fetch its displacement. PC stays wide until bus mapping.
template<class Advance, class ReadOp, class Read, class Sample>
void run_huc6280_bit_branch(std::uint8_t opcode, std::uint32_t& pc,
    std::uint8_t& status, Advance advance, ReadOp read_op, Read read, Sample sample)
{
    ++pc;
    const auto address = 0x2000u | read_op(pc);
    ++pc;
    advance(3);
    sample();
    advance(1);
    const auto value = read(address);
    const bool set = (value & (1u << ((opcode >> 4) & 7))) != 0;
    if (set == ((opcode & 0x80) != 0)) {
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
