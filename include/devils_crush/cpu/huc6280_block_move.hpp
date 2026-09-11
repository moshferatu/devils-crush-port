// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

namespace devils_crush::cpu {

// These values are the existing savestate ABI, including its suspended-byte
// convention: addresses have advanced, but length has not yet decremented.
constexpr std::uint32_t huc6280_block_mode(std::uint8_t opcode)
{
    switch (opcode) {
    case 0xe3: return 1; // TIA: alternate destination
    case 0xf3: return 2; // TAI: alternate source
    case 0xc3: return 3; // TDD: decrement both
    case 0x73: return 4; // TII: increment both
    case 0xd3: return 5; // TIN: fixed destination
    default: return 0;
    }
}

// opcode == 0 resumes an existing transfer. Other inputs must be block opcodes.
// Returns false at a scheduler yield, true after retirement. Memory accesses
// remain live, including overlapping copies, stack aliases and device stalls.
template<class Advance, class ReadOp, class Read, class Write, class Running,
         class Sample>
bool run_huc6280_block_move(
    std::uint8_t opcode, std::uint32_t& pc,
    std::uint8_t& a, std::uint8_t& x, std::uint8_t& y,
    std::uint8_t& stack, std::uint8_t& status,
    std::uint32_t& mode, std::uint16_t& source, std::uint16_t& destination,
    std::uint16_t& length, std::uint32_t& alternate,
    Advance advance, ReadOp read_op, Read read, Write write,
    Running running, Sample sample)
{
    const auto push = [&](std::uint8_t value) {
        write(0x2100u | stack, value);
        --stack;
    };
    const auto pop = [&]() {
        ++stack;
        return read(0x2100u | stack);
    };
    const auto operand = [&](std::uint16_t& value) {
        value = read_op(pc);
        ++pc;
        value |= static_cast<std::uint16_t>(read_op(pc) << 8);
        ++pc;
    };

    bool transfer = true;
    if (opcode != 0) {
        ++pc;
        push(y);
        push(a);
        push(x);
        operand(source);
        operand(destination);
        operand(length);
        advance(14);
        mode = huc6280_block_mode(opcode);
        if (mode == 1 || mode == 2) { alternate = 0; }
    } else {
        // A yield occurs after the write even on the last byte. Do not repeat
        // that byte or skip the final pop/sample sequence on resumption.
        --length;
        transfer = length != 0;
    }

    while (transfer) {
        advance(6);
        // Keep the pre-bus sum wide, just as the mapped bus expects at FFFF+1.
        const auto from = static_cast<std::uint32_t>(source)
            + (mode == 2 ? alternate : 0);
        const auto to = static_cast<std::uint32_t>(destination)
            + (mode == 1 ? alternate : 0);
        write(to, read(from));
        switch (mode) {
        case 1: ++source; alternate ^= 1; break;
        case 2: ++destination; alternate ^= 1; break;
        case 3: --source; --destination; break;
        case 4: ++source; ++destination; break;
        case 5: ++source; break;
        }
        if (!running()) { return false; }
        --length;
        transfer = length != 0; // Encoded zero transfers all 65,536 bytes.
    }
    mode = 0;
    x = pop();
    a = pop();
    y = pop();
    advance(2);
    sample();
    advance(1);
    status &= static_cast<std::uint8_t>(~0x20u);
    return true;
}

} // namespace devils_crush::cpu
