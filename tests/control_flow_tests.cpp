// SPDX-License-Identifier: GPL-3.0-or-later
#include <devils_crush/cpu/huc6280_control_flow.hpp>
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

struct Event {
    unsigned kind, address, value, pc, status, stack, time;
    bool operator==(const Event&) const = default;
};
struct Machine {
    std::uint32_t pc;
    std::uint8_t status, stack, x;
    unsigned scale, divisor, stall, horizon;
    unsigned time = 0, remainder = 0, samples = 0, latch = 0;
    std::array<std::uint8_t, 65536> ram{};
    std::vector<Event> events;
    void event(unsigned kind, unsigned address, unsigned value) {
        events.push_back({kind,address,value,pc,status,stack,time});
    }
    void advance(unsigned n) {
        event(0,0,n);
        const auto clocks = remainder + n * scale;
        time += clocks / divisor;
        remainder = clocks % divisor;
    }
    void sample() { event(1,0,time >= horizon); latch = time >= horizon; ++samples; }
    std::uint8_t read(unsigned address, unsigned kind = 3) {
        const auto value = ram[address & 65535];
        event(kind,address,value); time += stall; return value;
    }
    std::uint8_t operand(unsigned address) { return read(address,2); }
    void write(unsigned address, std::uint8_t value) {
        event(4,address,value); time += stall; ram[address & 65535] = value;
    }
    void push(std::uint8_t value) { write(0x2100 | stack,value); --stack; }
    std::uint8_t pop() { ++stack; return read(0x2100 | stack); }
};

// Independent expansion of the six pinned opcode bodies, GetAB and JR.
void original(Machine& m, unsigned opcode)
{
    ++m.pc;
    switch (opcode) {
    case 0x60:
        m.pc = m.pop(); m.pc |= m.pop() << 8; ++m.pc;
        m.advance(6); m.sample(); m.advance(1); break;
    case 0x20: {
        const std::uint8_t npc = m.operand(m.pc); ++m.pc;
        m.push(static_cast<std::uint8_t>(m.pc >> 8));
        m.push(static_cast<std::uint8_t>(m.pc));
        m.pc = m.operand(m.pc) << 8; m.pc |= npc;
        m.advance(6); m.sample(); m.advance(1); break;
    }
    case 0x44: {
        m.push(static_cast<std::uint8_t>(m.pc >> 8));
        m.push(static_cast<std::uint8_t>(m.pc)); m.advance(4);
        const std::int32_t tmp = static_cast<std::int8_t>(m.operand(m.pc));
        ++m.pc; m.advance(3); m.pc += static_cast<std::uint32_t>(tmp);
        m.sample(); m.advance(1); break;
    }
    case 0x4c: {
        std::uint16_t ptmp = static_cast<std::uint16_t>(m.pc);
        unsigned npc = m.operand(ptmp); ++ptmp;
        npc |= m.operand(ptmp) << 8; m.pc = npc;
        m.advance(3); m.sample(); m.advance(1); break;
    }
    case 0x6c:
    case 0x7c: {
        unsigned tmp = m.operand(m.pc); ++m.pc;
        tmp |= m.operand(m.pc) << 8; ++m.pc;
        if (opcode == 0x7c) { tmp += m.x; }
        m.pc = m.read(tmp); m.pc |= m.read(tmp + 1) << 8;
        m.advance(6); m.sample(); m.advance(1); break;
    }
    }
    m.status &= static_cast<std::uint8_t>(~0x20u);
}
int main()
{
    std::uint64_t cases = 0;
    for (unsigned opcode = 0; opcode < 256; ++opcode) {
        const bool supported = opcode == 32 || opcode == 68 || opcode == 96
            || opcode == 76 || opcode == 108 || opcode == 124;
        if (devils_crush::cpu::is_huc6280_control_flow(static_cast<std::uint8_t>(opcode)) != supported) { return 1; }
        if (!supported) { continue; }
        for (unsigned s = 0; s < 256; ++s) {
            for (unsigned value = 0; value < 256; ++value) {
                for (unsigned scenario = 0; scenario < 4; ++scenario) {
                    // Stack aliases exercise operands overwritten by JSR/BSR.
                    const std::array<unsigned,4> pcs{0xfffe,0xffff,0x21fe,0x2100u | s};
                    Machine a{pcs[scenario], static_cast<std::uint8_t>(value),
                        static_cast<std::uint8_t>(s), static_cast<std::uint8_t>(value),
                        scenario & 1 ? 12u : 3u, s & 1 ? 5u : 1u,
                        s & 2 ? 7u : 0u, value % 100};
                    a.ram.fill(static_cast<std::uint8_t>(value));
                    a.ram[(a.pc + 1) & 65535] = static_cast<std::uint8_t>(value);
                    a.ram[(a.pc + 2) & 65535] = 0xff;
                    auto b = a;
                    original(a,opcode);
                    devils_crush::cpu::run_huc6280_control_flow(static_cast<std::uint8_t>(opcode),
                        b.pc,b.status,b.stack,b.x,
                        [&](unsigned n) { b.advance(n); },
                        [&](unsigned p) { return b.operand(p); },
                        [&](unsigned p) { return b.read(p); },
                        [&](unsigned p, std::uint8_t v) { b.write(p,v); },
                        [&]() { b.sample(); });
                    if (a.events != b.events || a.ram != b.ram || a.pc != b.pc
                        || a.status != b.status || a.stack != b.stack || a.x != b.x
                        || a.time != b.time || a.remainder != b.remainder
                        || a.latch != b.latch || a.samples != 1 || b.samples != 1) {
                        std::cerr << "Control-flow mismatch: " << opcode << ' ' << s << ' '
                            << value << ' ' << scenario << '\n'; return 1;
                    }
                    ++cases;
                }
            }
        }
    }
    std::cout << cases << " control-flow differential cases passed\n";
}
