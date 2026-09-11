// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace devils_crush::cpu {
constexpr bool is_huc6280_store(std::uint8_t opcode)
{
    switch (opcode) {
    case 0x85: case 0x95: case 0x8d: case 0x9d: case 0x99:
    case 0x92: case 0x81: case 0x91:
    case 0x86: case 0x96: case 0x8e:
    case 0x84: case 0x94: case 0x8c:
    case 0x64: case 0x74: case 0x9c: case 0x9e: return true;
    default: return false;
    }
}

// PC addresses the opcode. Keep wide absolute indexing, zero-page pointer
// wrapping, and the pinned core's delayed PC increment for (zp),Y.
template<class Advance, class ReadOp, class Read, class Write, class Sample>
void run_huc6280_store(std::uint8_t opcode, std::uint32_t& pc,
    std::uint8_t& status, const std::uint8_t& a, const std::uint8_t& x,
    const std::uint8_t& y, Advance advance, ReadOp read_op, Read read,
    Write write, Sample sample)
{
    ++pc;
    std::uint32_t address = read_op(pc);
    if (opcode != 0x91) { ++pc; }
    unsigned cycles = 3;
    switch (opcode) {
    case 0x85: case 0x86: case 0x84: case 0x64:
        address |= 0x2000; break;
    case 0x95: case 0x94: case 0x74:
        address = 0x2000 | ((address + x) & 0xff); break;
    case 0x96:
        address = 0x2000 | ((address + y) & 0xff); break;
    case 0x81: case 0x92: case 0x91: {
        auto pointer = static_cast<std::uint8_t>(address + (opcode == 0x81 ? x : 0));
        address = read(0x2000u + pointer);
        ++pointer;
        address |= static_cast<std::uint32_t>(read(0x2000u + pointer)) << 8;
        if (opcode == 0x91) { address += y; ++pc; }
        cycles = 6;
        break;
    }
    default:
        address |= static_cast<std::uint32_t>(read_op(pc)) << 8;
        ++pc;
        if (opcode == 0x9d || opcode == 0x9e) { address += x; }
        if (opcode == 0x99) { address += y; }
        cycles = 4;
        break;
    }
    advance(cycles);
    sample();
    advance(1);
    switch (opcode) {
    case 0x86: case 0x96: case 0x8e: write(address, x); break;
    case 0x84: case 0x94: case 0x8c: write(address, y); break;
    case 0x64: case 0x74: case 0x9c: case 0x9e: write(address, std::uint8_t{0}); break;
    default: write(address, a); break;
    }
    status &= static_cast<std::uint8_t>(~0x20u);
}
} // namespace devils_crush::cpu
