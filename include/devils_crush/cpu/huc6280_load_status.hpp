// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace devils_crush::cpu {
constexpr bool is_huc6280_load_status(std::uint8_t opcode)
{
    return opcode == 0xad || opcode == 0x28 || opcode == 0x78;
}

// PC addresses the opcode. Sampling observes the old interrupt mask; PLP
// publishes its restored mask only after the final cycle and stack read.
template<class Advance, class ReadOp, class Read, class Sample, class PublishMask>
void run_huc6280_load_status(std::uint8_t opcode, std::uint32_t& pc,
    std::uint8_t& status, std::uint8_t& stack, std::uint8_t& accumulator,
    Advance advance, ReadOp read_op, Read read, Sample sample, PublishMask publish_mask)
{
    ++pc;
    if (opcode == 0xad) {
        std::uint32_t address = read_op(pc);
        ++pc;
        address |= static_cast<std::uint32_t>(read_op(pc)) << 8;
        ++pc;
        advance(4);
        sample();
        advance(1);
        accumulator = read(address);
        status = static_cast<std::uint8_t>((status & ~0x82u)
            | (accumulator & 0x80u) | (accumulator == 0 ? 2u : 0u));
    } else if (opcode == 0x28) {
        advance(3);
        sample();
        advance(1);
        ++stack;
        status = read(0x2100u | stack);
        publish_mask();
        return; // Restored T survives PLP.
    } else {
        advance(1);
        sample();
        advance(1);
        status |= 0x04;
        publish_mask();
    }
    status &= static_cast<std::uint8_t>(~0x20u);
}
} // namespace devils_crush::cpu
