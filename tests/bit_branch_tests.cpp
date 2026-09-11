// SPDX-License-Identifier: GPL-3.0-or-later
#include <devils_crush/cpu/huc6280_bit_branch.hpp>
#include <cstdint>
#include <iostream>
#include <vector>

struct Event {
    unsigned kind, value, pc, status, time;
    bool operator==(const Event&) const = default;
};
struct Machine {
    std::uint32_t pc;
    std::uint8_t status, value, displacement;
    unsigned scale, divisor, stall, horizon;
    unsigned time = 0, remainder = 0, samples = 0, latch = 0;
    std::vector<Event> events;
    void event(unsigned kind, unsigned value_) { events.push_back({kind,value_,pc,status,time}); }
    void advance(unsigned n) {
        event(0,n);
        const auto clocks = remainder + n * scale;
        time += clocks / divisor;
        remainder = clocks % divisor;
    }
    void sample() { event(1, time >= horizon); latch = time >= horizon; ++samples; }
    std::uint8_t operand(unsigned address) {
        event(2,address);
        time += stall;
        // The first operand is the zero-page index; the second is signed.
        unsigned count = 0;
        for (const auto& e : events) { if (e.kind == 2) { ++count; } }
        return count == 1 ? value : displacement;
    }
    std::uint8_t read(unsigned address) { event(3,address); time += stall; return value; }
};

// Independent expansion of the pinned LD_ZP, BBRi/BBSi and JR bodies.
void original(Machine& m, unsigned opcode)
{
    ++m.pc;
    const unsigned address = 0x2000 | m.operand(m.pc);
    ++m.pc;
    m.advance(3); m.sample(); m.advance(1);
    const unsigned value = m.read(address);
    const unsigned bit = (opcode >> 4) & 7;
    const bool condition = opcode < 0x80 ? !(value & (1u << bit)) : (value & (1u << bit)) != 0;
    if (condition) {
        const std::int32_t displacement = static_cast<std::int8_t>(m.operand(m.pc));
        ++m.pc;
        m.advance(3);
        m.pc += static_cast<std::uint32_t>(displacement);
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
        if (devils_crush::cpu::is_huc6280_bit_branch(static_cast<std::uint8_t>(opcode)) != ((opcode % 16) == 15)) { return 1; }
        if ((opcode & 15) != 15) { continue; }
        for (unsigned value = 0; value < 256; ++value) {
            for (unsigned displacement = 0; displacement < 256; ++displacement) {
                for (unsigned scale : {3u,12u}) {
                    Machine a{displacement & 1 ? 0xfffeu : 0u,
                        static_cast<std::uint8_t>(displacement),
                        static_cast<std::uint8_t>(value), static_cast<std::uint8_t>(displacement),
                        scale, value & 1 ? 5u : 1u, value & 2 ? 7u : 0u, displacement % 100};
                    auto b = a;
                    original(a,opcode);
                    devils_crush::cpu::run_huc6280_bit_branch(static_cast<std::uint8_t>(opcode), b.pc, b.status,
                        [&](unsigned n) { b.advance(n); }, [&](unsigned p) { return b.operand(p); },
                        [&](unsigned p) { return b.read(p); }, [&]() { b.sample(); });
                    if (a.pc != b.pc || a.status != b.status || a.time != b.time
                        || a.remainder != b.remainder || a.latch != b.latch
                        || a.events != b.events || b.samples != 2) {
                        std::cerr << "Bit-branch ordering mismatch\n"; return 1;
                    }
                    ++cases;
                }
            }
        }
    }
    std::cout << cases << " differential bit branches passed\n";
}
