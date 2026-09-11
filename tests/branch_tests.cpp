// SPDX-License-Identifier: GPL-3.0-or-later
#include <devils_crush/cpu/huc6280_branch.hpp>
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

struct Event {
    unsigned kind, address, value, pc, status, stack, accumulator, mask, time;
    bool operator==(const Event&) const = default;
};
struct Machine {
    std::uint32_t pc;
    std::uint8_t status, stack, accumulator;
    unsigned scale, divisor, stall, horizon, mask;
    unsigned time = 0, remainder = 0, samples = 0, latch = 0, sampled_i = 0;
    std::array<std::uint8_t, 65536> ram{};
    std::vector<Event> events;
    void event(unsigned kind, unsigned address, unsigned value) {
        events.push_back({kind,address,value,pc,status,stack,accumulator,mask,time});
    }
    void advance(unsigned n) {
        event(0,0,n);
        const auto clocks = remainder + n * scale;
        time += clocks / divisor;
        remainder = clocks % divisor;
    }
    void sample() {
        event(1,0,time >= horizon);
        latch = (time >= horizon) & mask;
        sampled_i = status & 4;
        ++samples;
    }
    void publish() { mask = (status & 4) ? 0u : 1u; event(4,0,mask); }
    std::uint8_t read(unsigned address, unsigned kind = 3) {
        const auto value = ram[address & 65535];
        event(kind,address,value); time += stall; return value;
    }
    std::uint8_t operand(unsigned address) { return read(address,2); }
    std::uint8_t pop() { ++stack; return read(0x2100 | stack); }
};

// Independent expansion of the eight opcode conditions and pinned JR helper.
void original(Machine& m, unsigned opcode)
{
    bool cond = false;
    switch (opcode) {
    case 0x10: cond = !(m.status & 0x80); break;
    case 0x30: cond = (m.status & 0x80) != 0; break;
    case 0x50: cond = !(m.status & 0x40); break;
    case 0x70: cond = (m.status & 0x40) != 0; break;
    case 0x90: cond = !(m.status & 0x01); break;
    case 0xb0: cond = (m.status & 0x01) != 0; break;
    case 0xd0: cond = !(m.status & 0x02); break;
    case 0xf0: cond = (m.status & 0x02) != 0; break;
    }
    ++m.pc;
    if (cond) {
        const std::int32_t disp = static_cast<std::int8_t>(m.operand(m.pc));
        ++m.pc;
        m.advance(3);
        m.pc += static_cast<std::uint32_t>(disp);
    } else {
        m.advance(1);
        ++m.pc;
    }
    m.sample(); m.advance(1);
    m.status &= static_cast<std::uint8_t>(~0x20u);
}
int main()
{
    std::uint64_t cases = 0;
    for (unsigned opcode = 0; opcode < 256; ++opcode) {
        const bool supported = opcode == 16 || opcode == 48 || opcode == 80 || opcode == 112 || opcode == 144 || opcode == 176 || opcode == 208 || opcode == 240;
        if (devils_crush::cpu::is_huc6280_branch(static_cast<std::uint8_t>(opcode)) != supported) { return 1; }
        if (!supported) { continue; }
        for (unsigned p = 0; p < 256; ++p) {
            for (unsigned value = 0; value < 256; ++value) {
                for (unsigned scenario = 0; scenario < 12; ++scenario) {
                    const std::array<unsigned,6> pcs{0xfffe,0xffff,0x0000,0xfffffffe,0x21fe,0x4000};
                    Machine a{pcs[scenario / 2], static_cast<std::uint8_t>(p),
                        static_cast<std::uint8_t>(value), static_cast<std::uint8_t>(~value),
                        scenario & 1 ? 12u : 3u, scenario & 2 ? 5u : 1u,
                        scenario & 4 ? 7u : 0u, p % 100, p & 4 ? 0u : 1u};
                    a.ram.fill(static_cast<std::uint8_t>(value));
                    a.ram[(a.pc + 1) & 65535] = static_cast<std::uint8_t>(value);
                    a.ram[(a.pc + 2) & 65535] = 0xff;
                    auto b = a;
                    original(a,opcode);
                    devils_crush::cpu::run_huc6280_branch(static_cast<std::uint8_t>(opcode),
                        b.pc,b.status,
                        [&](unsigned n) { b.advance(n); },
                        [&](unsigned addr) { return b.operand(addr); },
                        [&]() { b.sample(); });
                    if (a.events != b.events || a.ram != b.ram || a.pc != b.pc
                        || a.status != b.status || a.stack != b.stack || a.accumulator != b.accumulator
                        || a.mask != b.mask || a.sampled_i != b.sampled_i
                        || a.time != b.time || a.remainder != b.remainder
                        || a.latch != b.latch || a.samples != 1 || b.samples != 1) {
                        std::cerr << "Branch mismatch: " << opcode << ' ' << p << ' '
                            << value << ' ' << scenario << '\n'; return 1;
                    }
                    ++cases;
                }
            }
        }
    }
    std::cout << cases << " branch differential cases passed\n";
}
