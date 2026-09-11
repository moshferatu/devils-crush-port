// SPDX-License-Identifier: GPL-3.0-or-later
#include "devils_crush/cpu/huc6280_block_move.hpp"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {
void require(bool value, const char* message)
{
    if (!value) { throw std::runtime_error(message); }
}
struct Registers {
    std::uint32_t pc = 0xfffc, mode = 0, alternate = 9;
    std::uint16_t source = 0, destination = 0, length = 0;
    std::uint8_t a = 0x73, x = 0xa1, y = 0x29, s = 0, p = 0;
    bool operator==(const Registers&) const = default;
};
struct Event {
    char kind;
    std::uint32_t address, value;
    std::uint64_t clock;
    Registers registers;
    bool operator==(const Event&) const = default;
};
struct Machine {
    Registers r;
    std::array<std::uint8_t, 65536> memory{};
    std::vector<Event> events;
    std::uint64_t clock = 0;
    unsigned scale = 3, fraction = 0, divisor = 1, writes = 0, stop = 0;
    bool stalls = false;
    void event(char kind, std::uint32_t address = 0, std::uint32_t value = 0)
    {
        events.push_back({kind, address, value, clock, r});
    }
    void advance(unsigned cycles)
    {
        event('C', 0, cycles);
        const unsigned divided = fraction + cycles * scale;
        clock += divided / divisor;
        fraction = divided % divisor;
    }
    std::uint8_t read(std::uint32_t address, bool operand = false)
    {
        const auto value = memory[address & 65535];
        event(operand ? 'O' : 'R', address, value);
        if (stalls && !operand) { clock += 5; }
        return value;
    }
    void write(std::uint32_t address, std::uint8_t value)
    {
        event('W', address, value);
        memory[address & 65535] = value;
        if (stalls) { clock += 7; }
        ++writes;
    }
    bool running() const { return stop == 0 || writes < stop; }
    bool native(std::uint8_t opcode)
    {
        return devils_crush::cpu::run_huc6280_block_move(
            opcode, r.pc, r.a, r.x, r.y, r.s, r.p, r.mode,
            r.source, r.destination, r.length, r.alternate,
            [&](unsigned n) { advance(n); },
            [&](std::uint32_t address) { return read(address, true); },
            [&](std::uint32_t address) { return read(address); },
            [&](std::uint32_t address, std::uint8_t value) { write(address, value); },
            [&]() { return running(); }, [&]() { event('S'); });
    }
    // Literal pre-port LD_BMT / BMT_* / PUSH / POP ordering from the pinned
    // core, with its continuation label expressed as a decrement on resume.
    bool original(std::uint8_t opcode)
    {
        if (opcode) {
            ++r.pc;
            write(0x2100 | r.s, r.y); --r.s;
            write(0x2100 | r.s, r.a); --r.s;
            write(0x2100 | r.s, r.x); --r.s;
            r.source = read(r.pc, true); ++r.pc;
            r.source |= static_cast<std::uint16_t>(read(r.pc, true) << 8); ++r.pc;
            r.destination = read(r.pc, true); ++r.pc;
            r.destination |= static_cast<std::uint16_t>(read(r.pc, true) << 8); ++r.pc;
            r.length = read(r.pc, true); ++r.pc;
            r.length |= static_cast<std::uint16_t>(read(r.pc, true) << 8); ++r.pc;
            advance(14);
            switch (opcode) {
            case 0xe3: r.mode = 1; r.alternate = 0; break;
            case 0xf3: r.mode = 2; r.alternate = 0; break;
            case 0xc3: r.mode = 3; break;
            case 0x73: r.mode = 4; break;
            case 0xd3: r.mode = 5; break;
            }
        } else {
            --r.length;
            if (!r.length) { return retire(); }
        }
        do {
            advance(6);
            switch (r.mode) {
            case 1:
                write(r.destination + r.alternate, read(r.source));
                ++r.source; r.alternate ^= 1; break;
            case 2:
                write(r.destination, read(r.source + r.alternate));
                ++r.destination; r.alternate ^= 1; break;
            case 3:
                write(r.destination, read(r.source));
                --r.source; --r.destination; break;
            case 4:
                write(r.destination, read(r.source));
                ++r.source; ++r.destination; break;
            case 5:
                write(r.destination, read(r.source));
                ++r.source; break;
            }
            if (!running()) { return false; }
            --r.length;
        } while (r.length);
        return retire();
    }
    bool retire()
    {
        r.mode = 0;
        ++r.s; r.x = read(0x2100 | r.s);
        ++r.s; r.a = read(0x2100 | r.s);
        ++r.s; r.y = read(0x2100 | r.s);
        advance(2); event('S'); advance(1);
        r.p &= 0xdf;
        return true;
    }
};
void compare(const Machine& native, const Machine& original)
{
    require(native.r == original.r, "Live transfer registers differ");
    require(native.memory == original.memory, "Transfer memory differs");
    require(native.clock == original.clock && native.fraction == original.fraction,
            "Transfer divided clock differs");
    require(native.events == original.events, "Transfer bus/cycle/sample ordering differs");
}
void exercise(std::uint8_t opcode, unsigned status, unsigned stack, unsigned length,
              unsigned source, unsigned destination, unsigned scale, unsigned divisor,
              unsigned yield_byte, bool stalls)
{
    Machine native;
    for (unsigned i = 0; i < native.memory.size(); ++i) {
        native.memory[i] = static_cast<std::uint8_t>((i * 31) ^ (i >> 8));
    }
    native.r.s = static_cast<std::uint8_t>(stack);
    native.r.p = static_cast<std::uint8_t>(status);
    native.scale = scale; native.divisor = divisor; native.stalls = stalls;
    unsigned operand = (native.r.pc + 1) & 65535;
    for (auto word : {source, destination, length}) {
        native.memory[operand] = static_cast<std::uint8_t>(word);
        operand = (operand + 1) & 65535;
        native.memory[operand] = static_cast<std::uint8_t>(word >> 8);
        operand = (operand + 1) & 65535;
    }
    native.stop = yield_byte ? yield_byte + 3 : 0; // Three prefix pushes.
    Machine original = native;
    bool done = native.native(opcode);
    require(done == original.original(opcode), "Yield boundary differs");
    compare(native, original);
    if (!done) {
        const auto snapshot = native.r;
        native.r = {}; // Only the existing serialized registers are needed.
        native.r = snapshot;
        native.stop = original.stop = 0;
        require(native.native(0) && original.original(0), "Resume did not retire");
        compare(native, original);
    }
    require(native.r.mode == 0 && native.r.length == 0, "Transfer did not finish");
    require(native.r.s == stack && native.r.p == (status & 0xdf), "Stack/flags differ");
    unsigned samples = 0;
    for (const auto& event : native.events) { samples += event.kind == 'S'; }
    require(samples == 1, "Transfer sampled an interrupt before retirement");
}
} // namespace

int main()
{
    try {
        unsigned cases = 0;
        for (const auto opcode : {0xe3, 0xf3, 0xc3, 0x73, 0xd3}) {
            for (unsigned p = 0; p < 256; ++p) {
                for (const unsigned scale : {3u, 12u}) {
                    // Every status/stack byte, address wrap, overlapping and
                    // stack-aliasing transfers; yield on first/middle/last byte.
                    for (unsigned stop = 0; stop <= 3; ++stop) {
                        const unsigned source = p & 1 ? 0xffff : 0x21ff;
                        const unsigned destination = p & 1 ? 0xfffe : 0x2100;
                        exercise(static_cast<std::uint8_t>(opcode), p, (p * 73) & 255,
                                 3, source, destination, scale, p & 1 ? 5 : 1,
                                 stop, (p & 2) != 0);
                        ++cases;
                    }
                }
            }
            for (const unsigned count : {1u, 2u, 255u, 256u, 65535u, 0u}) {
                for (const unsigned stop : {1u, count ? count : 65536u}) {
                    exercise(static_cast<std::uint8_t>(opcode), 0xff, 1, count,
                             0xfffe, 0xffff, 12, 7, stop, true);
                    ++cases;
                }
            }
        }
        std::printf("%u block transfers match the core bus and suspended-state oracle.\n", cases);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
