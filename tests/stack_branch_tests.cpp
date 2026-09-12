// SPDX-License-Identifier: GPL-3.0-or-later
#include <devils_crush/cpu/huc6280_stack_branch.hpp>
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

struct Event {
    unsigned kind, address, value, pc, status, stack, accumulator, x, y, mask, time;
    bool operator==(const Event&) const = default;
};
struct Machine {
    std::uint32_t pc;
    std::uint8_t status, stack, accumulator;
    unsigned scale, divisor, stall, horizon, mask;
    std::uint8_t x = 0, y = 0;
    unsigned time = 0, remainder = 0, samples = 0, latch = 0, sampled_i = 0;
    std::array<std::uint8_t, 65536> ram{};
    std::vector<Event> events;
    void event(unsigned kind, unsigned address, unsigned value) {
        events.push_back({kind,address,value,pc,status,stack,accumulator,x,y,mask,time});
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
    void write(unsigned address, std::uint8_t value) { event(4,address,value); time += stall; ram[address & 65535] = value; }
    std::uint8_t read(unsigned address, unsigned kind = 3) {
        const auto value = ram[address & 65535];
        event(kind,address,value); time += stall; return value;
    }
    std::uint8_t operand(unsigned address) { return read(address,2); }

};

// Independent oracle: stack cases copied from the pinned huc6280_ops.inc;
// adapters reproduce its PUSH, POP, X_ZN and taken JR helpers.
#define A m.accumulator
#define X m.x
#define Y m.y
#define P m.status
#define ADDCYC(n) m.advance(n)
#define LastCycle() do { m.sample(); m.advance(1); } while(false)
#define T_FLAG 0x20
#define B_FLAG 0x10
#define PUSH(v) do { m.write(0x2100 | m.stack, v); --m.stack; } while(false)
#define POP() pop(m)
#define X_ZN(v) m.status = static_cast<std::uint8_t>((m.status & ~0x82u) | ((v) & 0x80u) | ((v) == 0 ? 2u : 0u))
std::uint8_t pop(Machine& m) { ++m.stack; return m.read(0x2100 | m.stack); }
void original(Machine& m, unsigned opcode) {
    ++m.pc;
    switch (opcode) {
case 0x48: /* PHA */
           ADDCYC(2);
           LastCycle();
           PUSH(A);
           break;

case 0x08: /* PHP */
           ADDCYC(2);
           LastCycle();
	   P &= ~T_FLAG;
           PUSH(P | B_FLAG);
           break;

case 0xDA: // PHX	65C02
           ADDCYC(2);
           LastCycle();
           PUSH(X);
	   break;

case 0x5A: // PHY	65C02
           ADDCYC(2);
           LastCycle();
	   PUSH(Y);
	   break;

case 0x68: /* PLA */
           ADDCYC(3);
           LastCycle();
           A = POP();
           X_ZN(A);
           break;

case 0xFA: // PLX	65C02
           ADDCYC(3);
           LastCycle();
	   X = POP();
	   X_ZN(X);
	   break;

case 0x7A: // PLY	65C02
           ADDCYC(3);
           LastCycle();
	   Y = POP();
	   X_ZN(Y);
	   break;

case 0x80: {
    std::int32_t disp = static_cast<std::int8_t>(m.operand(m.pc));
    m.pc++;
    ADDCYC(3);
    m.pc += disp;
    LastCycle();
    break;
}
    }
    P &= static_cast<std::uint8_t>(~T_FLAG);
}
#undef A
#undef X
#undef Y
#undef P
int main()
{
    std::uint64_t count = 0;
    constexpr std::array<unsigned,8> opcodes{
        0x08,0x48,0x5a,0xda,0x68,0x7a,0xfa,0x80};
    for (unsigned op = 0; op < 256; ++op) {
        bool supported = false;
        for (auto expected : opcodes) { supported |= op == expected; }
        if (devils_crush::cpu::is_huc6280_stack_branch(static_cast<std::uint8_t>(op)) != supported) { return 1; }
    }
    for (auto op : opcodes) {
        for (unsigned p = 0; p < 256; ++p) {
            for (unsigned value = 0; value < 256; ++value) {
                for (unsigned scenario = 0; scenario < 8; ++scenario) {
                    constexpr std::array<unsigned,4> pcs{0xffff,0xfffffffe,0x21fe,0x4000};
                    Machine a{pcs[scenario / 2], static_cast<std::uint8_t>(p),
                        static_cast<std::uint8_t>(value), static_cast<std::uint8_t>(p ^ value),
                        scenario & 1 ? 12u : 3u, scenario & 2 ? 5u : 1u,
                        scenario & 4 ? 7u : 0u, p % 100, p & 4 ? 0u : 1u};
                    a.x = static_cast<std::uint8_t>(value);
                    a.y = static_cast<std::uint8_t>(255 - value);
                    a.ram.fill(static_cast<std::uint8_t>(p));
                    a.ram[(a.pc + 1) & 65535] = static_cast<std::uint8_t>(scenario & 2 ? 255 : value);
                    a.ram[(a.pc + 2) & 65535] = static_cast<std::uint8_t>(scenario & 4 ? 255 : 0);
                    auto b = a;
                    original(a,op);
                    devils_crush::cpu::run_huc6280_stack_branch(static_cast<std::uint8_t>(op),
                        b.pc,b.status,b.stack,b.accumulator,b.x,b.y,
                        [&](unsigned n) { b.advance(n); },
                        [&](unsigned address) { return b.operand(address); },
                        [&](unsigned address) { return b.read(address); },
                        [&](unsigned address, std::uint8_t data) { b.write(address,data); },
                        [&]() { b.sample(); });
                    if (a.events != b.events || a.ram != b.ram || a.pc != b.pc
                        || a.status != b.status || a.stack != b.stack || a.accumulator != b.accumulator
                        || a.x != b.x || a.y != b.y || a.mask != b.mask || a.sampled_i != b.sampled_i
                        || a.time != b.time || a.remainder != b.remainder
                        || a.latch != b.latch || a.samples != 1 || b.samples != 1) {
                        std::cerr << "Stack/branch mismatch: " << op << ' ' << p << ' '
                            << value << ' ' << scenario << '\n'; return 1;
                    }
                    ++count;
                }
            }
        }
    }
    std::cout << count << " stack/branch differential cases passed\n";
}