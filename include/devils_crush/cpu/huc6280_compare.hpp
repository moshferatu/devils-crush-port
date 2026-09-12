// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace devils_crush::cpu {
constexpr bool is_huc6280_compare(std::uint8_t opcode)
{
    switch (opcode) {
    case 0xc9: case 0xc5: case 0xd5: case 0xcd: case 0xdd:
    case 0xd9: case 0xd2: case 0xc1: case 0xd1:
    case 0xe0: case 0xe4: case 0xec:
    case 0xc0: case 0xc4: case 0xcc: return true;
    default: return false;
    }
}

// Fetch and sample in the pinned core's order. Comparisons preserve registers
// and decimal/overflow flags; transfer mode does not redirect the operand.
template<class Advance, class ReadOp, class Read, class Sample>
void run_huc6280_compare(std::uint8_t opcode, std::uint32_t& pc,
    std::uint8_t& status, std::uint8_t a, std::uint8_t x,
    std::uint8_t y, Advance advance, ReadOp read_op, Read read, Sample sample)
{
    ++pc;
    std::uint32_t address = read_op(pc);
    if (opcode != 0xd1) { ++pc; }
    const bool immediate = opcode == 0xc9 || opcode == 0xe0 || opcode == 0xc0;
    unsigned cycles = 3;
    switch (opcode) {
    case 0xc9: case 0xe0: case 0xc0: cycles = 1; break;
    case 0xc5: case 0xe4: case 0xc4: address |= 0x2000; break;
    case 0xd5: address = 0x2000 | ((address + x) & 0xff); break;
    case 0xc1: case 0xd2: case 0xd1: {
        auto pointer = static_cast<std::uint8_t>(address + (opcode == 0xc1 ? x : 0));
        address = read(0x2000u + pointer);
        ++pointer;
        address |= static_cast<std::uint32_t>(read(0x2000u + pointer)) << 8;
        if (opcode == 0xd1) { address += y; ++pc; }
        cycles = 6;
        break;
    }
    default:
        address |= static_cast<std::uint32_t>(read_op(pc)) << 8;
        ++pc;
        if (opcode == 0xdd) { address += x; }
        if (opcode == 0xd9) { address += y; }
        cycles = 4;
        break;
    }
    advance(cycles);
    sample();
    advance(1);
    const auto value = immediate ? static_cast<std::uint8_t>(address) : read(address);
    const auto left = opcode == 0xe0 || opcode == 0xe4 || opcode == 0xec ? x
        : opcode == 0xc0 || opcode == 0xc4 || opcode == 0xcc ? y : a;
    const auto result = static_cast<std::uint8_t>(left - value);
    status = static_cast<std::uint8_t>((status & ~0xa3u)
        | (result & 0x80u) | (result == 0 ? 2u : 0u) | (left >= value ? 1u : 0u));
}
} // namespace devils_crush::cpu
