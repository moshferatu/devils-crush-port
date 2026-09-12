// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace devils_crush::cpu {
constexpr bool is_huc6280_arithmetic(std::uint8_t opcode)
{
    if (opcode == 0x54 || opcode == 0xd4) { return true; }
    if ((opcode & 0xe0) != 0x60 && (opcode & 0xe0) != 0xe0) { return false; }
    switch (opcode & 0x1f) {
    case 0x09: case 0x05: case 0x15: case 0x0d: case 0x1d:
    case 0x19: case 0x12: case 0x01: case 0x11: return true;
    default: return false;
    }
}

// Operand reads precede the optional transfer-mode read/modify/write.
// ADC alone supports transfer mode; decimal arithmetic adds another sample.
// Speed changes take effect before the final cycle.
template<class Advance, class ReadOp, class Read, class Write, class Sample, class Speed>
void run_huc6280_arithmetic(std::uint8_t opcode, std::uint32_t& pc,
    std::uint8_t& status, std::uint8_t& a, std::uint8_t x,
    std::uint8_t y, Advance advance, ReadOp read_op, Read read,
    Write write, Sample sample, Speed speed)
{
    ++pc;
    if (opcode == 0x54 || opcode == 0xd4) {
        advance(2);
        speed(opcode == 0xd4 ? 1u : 0u);
        sample();
        advance(1);
        status &= static_cast<std::uint8_t>(~0x20u);
        return;
    }
    const auto mode = opcode & 0x1f;
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
    const bool add = opcode < 0x80;
    const bool transfer = add && (status & 0x20);
    if (transfer) {
        advance(1);
        a = read(0x2000u + x);
    }
    if (status & 0x08) {
        if (add) {
            unsigned sum = (a & 15u) + (value & 15u) + (status & 1u);
            if (sum >= 10) { sum += 6; }
            sum += (a & 0xf0u) + (value & 0xf0u);
            if (sum >= 0xa0) { sum += 0x60; }
            status = static_cast<std::uint8_t>((status & ~0x83u) | ((sum & 0xff00) ? 1u : 0u));
            a = static_cast<std::uint8_t>(sum);
        } else {
            const auto low = static_cast<std::uint8_t>((a & 15) - (value & 15) - ((status & 1) ^ 1));
            const auto high = static_cast<std::uint8_t>((a >> 4) - (value >> 4) - ((low >> 4) & 1));
            auto result = static_cast<std::uint8_t>((high << 4) | (low & 15));
            if (low & 0x10) { result = static_cast<std::uint8_t>(result - 6); }
            if (high & 0x10) { result = static_cast<std::uint8_t>(result - 0x60); }
            status = static_cast<std::uint8_t>((status & ~0x83u) | (((high >> 4) & 1u) ^ 1u));
            a = result;
        }
        status = static_cast<std::uint8_t>(status | (a & 0x80u) | (a == 0 ? 2u : 0u));
        sample();
        advance(1);
    } else {
        const std::uint32_t result = add ? a + value + (status & 1u)
            : a - value - ((status & 1u) ^ 1u);
        const auto overflow = add ? ((~(a ^ value) & (a ^ result) & 0x80u) >> 1)
            : (((a ^ result) & (a ^ value) & 0x80u) >> 1);
        const auto carry = ((result >> 8) & 1u) ^ (add ? 0u : 1u);
        a = static_cast<std::uint8_t>(result);
        status = static_cast<std::uint8_t>((status & ~0xc3u) | overflow | carry
            | (a & 0x80u) | (a == 0 ? 2u : 0u));
    }
    if (transfer) {
        advance(1);
        write(0x2000u + x, a);
        sample();
        advance(1);
        a = backup;
    }
    status &= static_cast<std::uint8_t>(~0x20u);
}
} // namespace devils_crush::cpu