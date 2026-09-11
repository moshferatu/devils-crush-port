// SPDX-License-Identifier: GPL-3.0-or-later
#include <devils_crush/cpu/huc6280_store.hpp>
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
    std::uint8_t x = 0, y = 0;
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
    void write(unsigned address, std::uint8_t value) { event(4,address,value); time += stall; ram[address & 65535] = value; }
    std::uint8_t read(unsigned address, unsigned kind = 3) {
        const auto value = ram[address & 65535];
        event(kind,address,value); time += stall; return value;
    }
    std::uint8_t operand(unsigned address) { return read(address,2); }

};

// Independent oracle: addressing/store macros and opcode map copied verbatim
// from the pinned core. Adapter macros only bind their live bus and clock.
using uint8 = std::uint8_t;
#define PC m.pc
#define X m.x
#define Y m.y
#define A m.accumulator
#define RdOp(a) m.operand(a)
#define RdMem(a) m.read(a)
#define WrMem(a,v) m.write(a,v)
#define ADDCYC(n) m.advance(n)
#define LastCycle() do { m.sample(); m.advance(1); } while(false)
#define GetAB(target) {	\
	target=RdOp(PC);	\
	PC++;	\
	target|=RdOp(PC)<<8;	\
	PC++;	\
}

/* Absolute Indexed(for reads) */
#define GetABI(target, i) {	\
	unsigned int tmp;	\
	GetAB(tmp);	\
	target=tmp;	\
	target+=i;	\
}

/* Zero Page */
#define GetZP(target) {	\
	target=0x2000 | RdOp(PC); 	\
	PC++;	\
}

/* Zero Page Indexed */
#define GetZPI(target,i) {	\
	target=0x2000 | ((i+RdOp(PC)) & 0xFF);	\
	PC++;	\
}

/* Indirect */
#define GetIND(target) {       \
	uint8 tmp;     \
	tmp=RdOp(PC);        \
	PC++; \
	target=RdMem(0x2000 + tmp);	\
	tmp++;         \
	target|=RdMem(0x2000 + tmp)<<8;       \
}


/* Indexed Indirect */
#define GetIX(target) {	\
	uint8 tmp;	\
	tmp=RdOp(PC);	\
	PC++;	\
	tmp+=X;	\
	target=RdMem(0x2000 + tmp);	\
	tmp++;		\
	target|=RdMem(0x2000 + tmp) <<8;	\
}

/* Indirect Indexed(for reads) */
#define GetIY(target) {	\
	unsigned int rt;	\
	uint8 tmp;	\
	tmp=RdOp(PC);	\
	rt=RdMem(0x2000 + tmp);	\
	tmp++;	\
	rt|=RdMem(0x2000 + tmp)<<8;	\
	target = (rt + Y);	\
	PC++;	\
}

#define ST_ZP(r)	{unsigned int EA; GetZP(EA); ADDCYC(3); LastCycle(); WrMem(EA, r); break;}
#define ST_ZPX(r)	{unsigned int EA; GetZPI(EA,X); ADDCYC(3); LastCycle(); WrMem(EA, r); break;}
#define ST_ZPY(r)	{unsigned int EA; GetZPI(EA,Y); ADDCYC(3); LastCycle(); WrMem(EA, r); break;}
#define ST_AB(r)	{unsigned int EA; GetAB(EA); ADDCYC(4); LastCycle(); WrMem(EA, r); break;}
#define ST_ABI(reg,r)	{unsigned int EA; GetABI(EA,reg); ADDCYC(4); LastCycle(); WrMem(EA,r); break; }
#define ST_ABX(r)	ST_ABI(X, r)
#define ST_ABY(r)	ST_ABI(Y, r)

#define ST_IND(r)	{unsigned int EA; GetIND(EA); ADDCYC(6); LastCycle(); WrMem(EA,r); break; }
#define ST_IX(r)	{unsigned int EA; GetIX(EA); ADDCYC(6); LastCycle(); WrMem(EA,r); break; }
#define ST_IY(r)	{unsigned int EA; GetIY(EA); ADDCYC(6); LastCycle(); WrMem(EA,r); break; }


void original(Machine& m, unsigned opcode) {
    ++m.pc;
    switch (opcode) {
case 0x85: ST_ZP(A);
case 0x95: ST_ZPX(A);
case 0x8D: ST_AB(A);
case 0x9D: ST_ABX(A);
case 0x99: ST_ABY(A);
case 0x92: ST_IND(A);
case 0x81: ST_IX(A);
case 0x91: ST_IY(A);
case 0x86: ST_ZP(X);
case 0x96: ST_ZPY(X);
case 0x8E: ST_AB(X);
case 0x84: ST_ZP(Y);
case 0x94: ST_ZPX(Y);
case 0x8C: ST_AB(Y);
case 0x64: ST_ZP(0);
case 0x74: ST_ZPX(0);
case 0x9C: ST_AB(0);
case 0x9E: ST_ABX(0);
    }
    m.status &= static_cast<std::uint8_t>(~0x20u);
}
#undef PC
#undef X
#undef Y
#undef A
int main()
{
    std::uint64_t count = 0;
    constexpr std::array<unsigned,18> opcodes{
        0x85,0x95,0x8d,0x9d,0x99,0x92,0x81,0x91,0x86,0x96,0x8e,0x84,0x94,0x8c,0x64,0x74,0x9c,0x9e};
    for (unsigned op = 0; op < 256; ++op) {
        bool supported = false;
        for (auto expected : opcodes) { supported |= op == expected; }
        if (devils_crush::cpu::is_huc6280_store(static_cast<std::uint8_t>(op)) != supported) { return 1; }
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
                    a.ram.fill(0xff);
                    a.ram[(a.pc + 1) & 65535] = static_cast<std::uint8_t>(scenario & 2 ? 255 : value);
                    a.ram[(a.pc + 2) & 65535] = static_cast<std::uint8_t>(scenario & 4 ? 255 : 0);
                    auto b = a;
                    original(a,op);
                    devils_crush::cpu::run_huc6280_store(static_cast<std::uint8_t>(op),
                        b.pc,b.status,b.accumulator,b.x,b.y,
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
                        std::cerr << "Store mismatch: " << op << ' ' << p << ' '
                            << value << ' ' << scenario << '\n'; return 1;
                    }
                    ++count;
                }
            }
        }
    }
    std::cout << count << " store differential cases passed\n";
}