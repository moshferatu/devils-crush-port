// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace devils_crush::cpu {
constexpr bool is_huc6280_rmw(std::uint8_t opcode)
{
    switch (opcode) {
    case 0x0a: case 0x06: case 0x16: case 0x0e: case 0x1e:
    case 0x2a: case 0x26: case 0x36: case 0x2e: case 0x3e:
    case 0x4a: case 0x46: case 0x56: case 0x4e: case 0x5e:
    case 0x6a: case 0x66: case 0x76: case 0x6e: case 0x7e:
    case 0x3a: case 0xc6: case 0xd6: case 0xce: case 0xde:
    case 0x1a: case 0xe6: case 0xf6: case 0xee: case 0xfe:
    case 0xca: case 0x88: case 0xe8: case 0xc8: return true;
    default: return false;
    }
}

// Memory operations publish flags before sampling interrupts and write after
// the final cycle. Register operations publish both value and flags first.
template<class Advance, class ReadOp, class Read, class Write, class Sample>
void run_huc6280_rmw(std::uint8_t opcode, std::uint32_t& pc,
    std::uint8_t& status, std::uint8_t& a, std::uint8_t& x,
    std::uint8_t& y, Advance advance, ReadOp read_op, Read read,
    Write write, Sample sample)
{
    ++pc;
    auto zn = [&](std::uint8_t value) {
        status = static_cast<std::uint8_t>((status & ~0x82u)
            | (value & 0x80u) | (value == 0 ? 2u : 0u));
    };
    const bool index = opcode == 0xca || opcode == 0x88 || opcode == 0xe8 || opcode == 0xc8;
    const bool accumulator = (opcode & 0x0f) == 0x0a && !index;
    std::uint32_t address = 0;
    std::uint8_t value = 0;
    if (index) {
        auto& reg = opcode == 0xca || opcode == 0xe8 ? x : y;
        reg = static_cast<std::uint8_t>(reg + (opcode == 0xca || opcode == 0x88 ? -1 : 1));
        zn(reg);
    } else {
        if (accumulator) { value = a; }
        else {
            address = read_op(pc); ++pc;
            if ((opcode & 0x0f) == 6) {
                address = 0x2000 | ((address + ((opcode & 0x10) ? x : 0)) & 0xff);
                advance(5);
            } else {
                address |= static_cast<std::uint32_t>(read_op(pc)) << 8; ++pc;
                if (opcode & 0x10) { address += x; }
                advance(6);
            }
            value = read(address);
        }
        if (opcode == 0x3a || opcode >= 0xc0) {
            value = static_cast<std::uint8_t>(value + (opcode == 0x3a || opcode < 0xe0 ? -1 : 1));
        } else if (opcode == 0x1a) { ++value; }
        else {
            const auto carry = status & 1;
            const bool right = (opcode & 0x40) != 0;
            status = static_cast<std::uint8_t>((status & ~1u) | (right ? value & 1 : value >> 7));
            value = static_cast<std::uint8_t>(right ? value >> 1 : value << 1);
            if (opcode & 0x20) { value |= static_cast<std::uint8_t>(right ? carry << 7 : carry); }
        }
        zn(value);
        if (accumulator) { a = value; }
    }
    if (index || accumulator) { advance(1); }
    sample(); advance(1);
    if (!index && !accumulator) { write(address, value); }
    status &= static_cast<std::uint8_t>(~0x20u);
}
} // namespace devils_crush::cpu
