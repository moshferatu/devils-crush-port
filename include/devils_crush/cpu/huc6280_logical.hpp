// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace devils_crush::cpu {
constexpr bool is_huc6280_logical(std::uint8_t opcode)
{
    if (opcode >= 0x60) { return false; }
    switch (opcode & 0x1f) {
    case 0x09: case 0x05: case 0x15: case 0x0d: case 0x1d:
    case 0x19: case 0x12: case 0x01: case 0x11: return true;
    default: return false;
    }
}

// Operand reads precede the optional transfer-mode read/modify/write.
// Preserve both interrupt samples, including the transient accumulator.
template<class Advance, class ReadOp, class Read, class Write, class Sample>
void run_huc6280_logical(std::uint8_t opcode, std::uint32_t& pc,
    std::uint8_t& status, std::uint8_t& a, std::uint8_t x,
    std::uint8_t y, Advance advance, ReadOp read_op, Read read,
    Write write, Sample sample)
{
    const auto mode = opcode & 0x1f;
    ++pc;
    std::uint32_t address = read_op(pc);
    if (mode != 0x11) { ++pc; }
    unsigned cycles = 3;
    switch (mode) {
    case 0x09: cycles = 1; break;
    case 0x05: address |= 0x2000; break;
    case 0x15: address = 0x2000 | ((address + x) & 0xff); break;
    case 0x01: case 0x12: case 0x11: {
        auto pointer = static_cast<std::uint8_t>(address + (mode == 0x01 ? x : 0));
        address = read(0x2000u + pointer);
        ++pointer;
        address |= static_cast<std::uint32_t>(read(0x2000u + pointer)) << 8;
        if (mode == 0x11) { address += y; ++pc; }
        cycles = 6;
        break;
    }
    default:
        address |= static_cast<std::uint32_t>(read_op(pc)) << 8;
        ++pc;
        if (mode == 0x1d) { address += x; }
        if (mode == 0x19) { address += y; }
        cycles = 4;
        break;
    }
    advance(cycles);
    sample();
    advance(1);
    const auto value = mode == 0x09 ? static_cast<std::uint8_t>(address) : read(address);
    const auto backup = a;
    if (status & 0x20) {
        advance(1);
        a = read(0x2000u + x);
    }
    switch (opcode >> 5) {
    case 0: a |= value; break;
    case 1: a &= value; break;
    default: a ^= value; break;
    }
    status = static_cast<std::uint8_t>((status & ~0x82u)
        | (a & 0x80u) | (a == 0 ? 2u : 0u));
    if (status & 0x20) {
        advance(1);
        write(0x2000u + x, a);
        sample();
        advance(1);
        a = backup;
    }
    status &= static_cast<std::uint8_t>(~0x20u);
}
} // namespace devils_crush::cpu
