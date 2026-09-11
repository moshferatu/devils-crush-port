// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace devils_crush::cpu {
constexpr bool is_huc6280_control_flow(std::uint8_t opcode)
{
    return opcode == 0x20 || opcode == 0x44 || opcode == 0x60
        || opcode == 0x4c || opcode == 0x6c || opcode == 0x7c;
}

// PC initially addresses the opcode. Bus operations and clock batches use
// live state: notably JSR reads its high operand after both stack writes.
template<class Advance, class ReadOp, class Read, class Write, class Sample>
void run_huc6280_control_flow(std::uint8_t opcode, std::uint32_t& pc,
    std::uint8_t& status, std::uint8_t& stack, std::uint8_t x,
    Advance advance, ReadOp read_op, Read read, Write write, Sample sample)
{
    const auto push = [&](std::uint8_t value) { write(0x2100u | stack, value); --stack; };
    const auto pop = [&]() { ++stack; return read(0x2100u | stack); };
    ++pc;
    if (opcode == 0x60) {
        pc = pop();
        pc |= static_cast<std::uint32_t>(pop()) << 8;
        ++pc;
        advance(6);
    } else if (opcode == 0x20) {
        const auto low = read_op(pc);
        ++pc;
        push(static_cast<std::uint8_t>(pc >> 8));
        push(static_cast<std::uint8_t>(pc));
        pc = static_cast<std::uint32_t>(read_op(pc)) << 8;
        pc |= low;
        advance(6);
    } else if (opcode == 0x44) {
        push(static_cast<std::uint8_t>(pc >> 8));
        push(static_cast<std::uint8_t>(pc));
        advance(4);
        const auto displacement = static_cast<std::int8_t>(read_op(pc));
        ++pc;
        advance(3);
        pc += static_cast<std::uint32_t>(displacement);
    } else if (opcode == 0x4c) {
        // Absolute JMP alone wraps its operand cursor to sixteen bits.
        auto cursor = static_cast<std::uint16_t>(pc);
        std::uint32_t target = read_op(cursor);
        ++cursor;
        target |= static_cast<std::uint32_t>(read_op(cursor)) << 8;
        pc = target;
        advance(3);
    } else {
        std::uint32_t pointer = read_op(pc);
        ++pc;
        pointer |= static_cast<std::uint32_t>(read_op(pc)) << 8;
        ++pc;
        if (opcode == 0x7c) { pointer += x; }
        pc = read(pointer);
        pc |= static_cast<std::uint32_t>(read(pointer + 1)) << 8;
        advance(6);
    }
    sample();
    advance(1);
    status &= static_cast<std::uint8_t>(~0x20u);
}
} // namespace devils_crush::cpu
