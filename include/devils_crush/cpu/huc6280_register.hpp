// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <utility>

namespace devils_crush::cpu {
constexpr bool is_huc6280_register(std::uint8_t opcode)
{
    switch (opcode) {
    case 0xaa: case 0x8a: case 0xa8: case 0x98: case 0xba: case 0x9a:
    case 0x02: case 0x22: case 0x42: case 0x62: case 0x82: case 0xc2:
    case 0x18: case 0x38: case 0x58: case 0xb8: case 0xd8: case 0xf8:
    case 0xf4: case 0xea: case 0x43: case 0x53: return true;
    default: return false;
    }
}

// Mapping callbacks retain the core's cache updates, including MPR[8].
// CLI publishes its new mask only after the old mask's interrupt sample.
template<class Advance, class ReadOp, class GetMapping, class SetMapping,
         class Sample, class PublishMask>
void run_huc6280_register(std::uint8_t opcode, std::uint32_t& pc,
    std::uint8_t& status, std::uint8_t& stack, std::uint8_t& a,
    std::uint8_t& x, std::uint8_t& y, Advance advance, ReadOp read_op,
    GetMapping get_mapping, SetMapping set_mapping, Sample sample, PublishMask publish_mask)
{
    ++pc;
    unsigned cycles = 1;
    const auto nz = [&](std::uint8_t value) {
        status = static_cast<std::uint8_t>((status & ~0x82u)
            | (value & 0x80u) | (value == 0 ? 2u : 0u));
    };
    switch (opcode) {
    case 0xaa: x = a; nz(x); break;
    case 0x8a: a = x; nz(a); break;
    case 0xa8: y = a; nz(y); break;
    case 0x98: a = y; nz(a); break;
    case 0xba: x = stack; nz(x); break;
    case 0x9a: stack = x; break;
    case 0x02: std::swap(x,y); cycles = 2; break;
    case 0x22: std::swap(a,x); cycles = 2; break;
    case 0x42: std::swap(a,y); cycles = 2; break;
    case 0x43: case 0x53: {
        const auto mask = read_op(pc);
        ++pc;
        for (unsigned i = 0; i < 8; ++i) {
            if (mask & (1u << i)) {
                if (opcode == 0x53) { set_mapping(i,a); }
                else { a = get_mapping(i); }
            }
        }
        if (opcode == 0x53) { set_mapping(8,get_mapping(0)); }
        cycles = opcode == 0x53 ? 4 : 3;
        break;
    }
    }
    advance(cycles);
    sample();
    advance(1);
    switch (opcode) {
    case 0x62: a = 0; break;
    case 0x82: x = 0; break;
    case 0xc2: y = 0; break;
    case 0x18: status &= static_cast<std::uint8_t>(~1u); break;
    case 0x38: status |= 1; break;
    case 0x58: status &= static_cast<std::uint8_t>(~4u); publish_mask(); break;
    case 0xb8: status &= static_cast<std::uint8_t>(~0x40u); break;
    case 0xd8: status &= static_cast<std::uint8_t>(~8u); break;
    case 0xf8: status |= 8; break;
    case 0xf4: status |= 0x20; return;
    }
    status &= static_cast<std::uint8_t>(~0x20u);
}
} // namespace devils_crush::cpu
