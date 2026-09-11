// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace devils_crush::cpu {
// Absolute LDA remains in the earlier load/status engine.
constexpr bool is_huc6280_load(std::uint8_t opcode)
{
    switch (opcode) {
    case 0xa9: case 0xa5: case 0xb5: case 0xbd: case 0xb9:
    case 0xb2: case 0xa1: case 0xb1:
    case 0xa2: case 0xa6: case 0xb6: case 0xae: case 0xbe:
    case 0xa0: case 0xa4: case 0xb4: case 0xac: case 0xbc: return true;
    default: return false;
    }
}

// Preserve operand fetch order, wide absolute indexing, pointer wrapping,
// and final-cycle sampling before the destination register and flags change.
template<class Advance, class ReadOp, class Read, class Sample>
void run_huc6280_load(std::uint8_t opcode, std::uint32_t& pc,
    std::uint8_t& status, std::uint8_t& a, std::uint8_t& x,
    std::uint8_t& y, Advance advance, ReadOp read_op, Read read, Sample sample)
{
    ++pc;
    std::uint32_t address = read_op(pc);
    if (opcode != 0xb1) { ++pc; }
    const bool immediate = opcode == 0xa9 || opcode == 0xa2 || opcode == 0xa0;
    unsigned cycles = 3;
    switch (opcode) {
    case 0xa9: case 0xa2: case 0xa0: cycles = 1; break;
    case 0xa5: case 0xa6: case 0xa4: address |= 0x2000; break;
    case 0xb5: case 0xb4: address = 0x2000 | ((address + x) & 0xff); break;
    case 0xb6: address = 0x2000 | ((address + y) & 0xff); break;
    case 0xa1: case 0xb2: case 0xb1: {
        auto pointer = static_cast<std::uint8_t>(address + (opcode == 0xa1 ? x : 0));
        address = read(0x2000u + pointer);
        ++pointer;
        address |= static_cast<std::uint32_t>(read(0x2000u + pointer)) << 8;
        if (opcode == 0xb1) { address += y; ++pc; }
        cycles = 6;
        break;
    }
    default:
        address |= static_cast<std::uint32_t>(read_op(pc)) << 8;
        ++pc;
        if (opcode == 0xbd || opcode == 0xbc) { address += x; }
        if (opcode == 0xb9 || opcode == 0xbe) { address += y; }
        cycles = 4;
        break;
    }
    advance(cycles);
    sample();
    advance(1);
    const auto value = immediate ? static_cast<std::uint8_t>(address) : read(address);
    switch (opcode) {
    case 0xa2: case 0xa6: case 0xb6: case 0xae: case 0xbe: x = value; break;
    case 0xa0: case 0xa4: case 0xb4: case 0xac: case 0xbc: y = value; break;
    default: a = value; break;
    }
    status = static_cast<std::uint8_t>((status & ~0xa2u)
        | (value & 0x80u) | (value == 0 ? 2u : 0u));
}
} // namespace devils_crush::cpu
