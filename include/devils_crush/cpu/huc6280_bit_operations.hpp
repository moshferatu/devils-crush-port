// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace devils_crush::cpu {
constexpr bool is_huc6280_bit_operations(std::uint8_t opcode)
{
    if ((opcode & 0x0f) == 7) { return true; }
    switch (opcode) {
    case 0x89: case 0x24: case 0x34: case 0x2c: case 0x3c:
    case 0x83: case 0xa3: case 0x93: case 0xb3:
    case 0x04: case 0x0c: case 0x14: case 0x1c: return true;
    default: return false;
    }
}

// Preserve operand reads, clock batches, and the distinct read/modify/write
// interrupt boundary. HuC6280 TSB/TRB test the modified value for zero.
template<class Advance, class ReadOp, class Read, class Write, class Sample>
void run_huc6280_bit_operations(std::uint8_t opcode, std::uint32_t& pc,
    std::uint8_t& status, std::uint8_t a, std::uint8_t x,
    Advance advance, ReadOp read_op, Read read, Write write, Sample sample)
{
    ++pc;
    const bool bit_update = (opcode & 0x0f) == 7;
    const bool modify = bit_update || opcode < 0x20;
    const bool test = (opcode & 0x0f) == 3;
    const bool immediate = opcode == 0x89;
    std::uint8_t mask = a;
    if (test) { mask = read_op(pc); ++pc; advance(3); }
    std::uint32_t address = read_op(pc); ++pc;
    const bool absolute = test ? (opcode & 0x10) != 0 : (opcode & 0x0f) == 0x0c;
    if (!immediate) {
        if (absolute) { address |= static_cast<std::uint32_t>(read_op(pc)) << 8; ++pc; }
        const bool indexed = test ? (opcode & 0x20) != 0 : !modify && (opcode & 0x10) != 0;
        if (indexed) { address += x; }
        if (!absolute) { address = 0x2000 | (address & 0xff); }
    }
    advance(immediate ? 1 : (absolute ? 4 : 3) + (modify ? 2 : 0));
    if (!modify) { sample(); advance(1); }
    auto value = immediate ? static_cast<std::uint8_t>(address) : read(address);
    if (bit_update) {
        advance(1);
        const auto bit = 1u << ((opcode >> 4) & 7);
        value = static_cast<std::uint8_t>(opcode & 0x80 ? value | bit : value & ~bit);
    } else {
        const auto result = modify ? ((opcode & 0x10) ? value & ~a : value | a) : value & mask;
        status = static_cast<std::uint8_t>((status & ~0xc2u) | (value & 0xc0u) | (result == 0 ? 2u : 0u));
        if (modify) { value = static_cast<std::uint8_t>(result); }
    }
    if (modify) { sample(); advance(1); write(address, value); }
    status &= static_cast<std::uint8_t>(~0x20u);
}
} // namespace devils_crush::cpu
