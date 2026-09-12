// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace devils_crush::cpu {
constexpr bool is_huc6280_stack_branch(std::uint8_t opcode)
{
    switch (opcode) {
    case 0x08: case 0x48: case 0x5a: case 0xda:
    case 0x68: case 0x7a: case 0xfa: case 0x80: return true;
    default: return false;
    }
}

// PLP has its own native status engine. Preserve bus and interrupt ordering,
// including PHP's transfer-bit clear before the stack write.
template<class Advance, class ReadOp, class Read, class Write, class Sample>
void run_huc6280_stack_branch(std::uint8_t opcode, std::uint32_t& pc,
    std::uint8_t& status, std::uint8_t& stack, std::uint8_t& a,
    std::uint8_t& x, std::uint8_t& y, Advance advance, ReadOp read_op,
    Read read, Write write, Sample sample)
{
    ++pc;
    if (opcode == 0x80) {
        const auto displacement = static_cast<std::int8_t>(read_op(pc));
        ++pc;
        advance(3);
        pc += static_cast<std::uint32_t>(displacement);
        sample();
        advance(1);
    } else {
        const bool pull = opcode == 0x68 || opcode == 0x7a || opcode == 0xfa;
        advance(pull ? 3 : 2);
        sample();
        advance(1);
        if (pull) {
            ++stack;
            auto& target = opcode == 0x68 ? a : opcode == 0xfa ? x : y;
            target = read(0x2100u | stack);
            status = static_cast<std::uint8_t>((status & ~0x82u)
                | (target & 0x80u) | (target == 0 ? 2u : 0u));
        } else {
            if (opcode == 0x08) { status &= static_cast<std::uint8_t>(~0x20u); }
            const auto value = opcode == 0x08 ? static_cast<std::uint8_t>(status | 0x10u)
                : opcode == 0x48 ? a : opcode == 0xda ? x : y;
            write(0x2100u | stack, value);
            --stack;
        }
    }
    status &= static_cast<std::uint8_t>(~0x20u);
}
} // namespace devils_crush::cpu
