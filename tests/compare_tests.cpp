// SPDX-License-Identifier: GPL-3.0-or-later
#include <devils_crush/cpu/huc6280_compare.hpp>
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

// Independent oracle: addressing/comparison macros and opcode map copied verbatim
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

#define LD_IM(op)   {uint8 x; x=RdOp(PC); PC++; ADDCYC(1); LastCycle(); op; break;}
#define LD_ZP(op)   {unsigned int EA; uint8 x; GetZP(EA); ADDCYC(3); LastCycle(); x=RdMem(EA); op; break;}
#define LD_ZPX(op)  {unsigned int EA; uint8 x; GetZPI(EA, X); ADDCYC(3); LastCycle(); x=RdMem(EA); op; break;}
#define LD_ZPY(op)  {unsigned int EA; uint8 x; GetZPI(EA, Y); ADDCYC(3); LastCycle(); x=RdMem(EA); op; break;}
#define LD_AB(op)   {unsigned int EA; uint8 x; GetAB(EA); ADDCYC(4); LastCycle(); x=RdMem(EA); op; break; }
#define LD_ABI(reg,op)  {unsigned int EA; uint8 x; GetABI(EA,reg); ADDCYC(4); LastCycle(); x=RdMem(EA); op; break;}
#define LD_ABX(op)  LD_ABI(X, op)
#define LD_ABY(op)  LD_ABI(Y, op)

#define LD_IND(op)	{unsigned int EA; uint8 x; GetIND(EA); ADDCYC(6); LastCycle(); x=RdMem(EA); op; break;}
#define LD_IX(op)	{unsigned int EA; uint8 x; GetIX(EA); ADDCYC(6); LastCycle(); x=RdMem(EA); op; break;}
#define LD_IY(op)	{unsigned int EA; uint8 x; GetIY(EA); ADDCYC(6); LastCycle(); x=RdMem(EA); op; break;}

#define X_ZN(v) m.status = static_cast<std::uint8_t>((m.status & ~0x82u) | ((v) & 0x80u) | ((v) == 0 ? 2u : 0u))
using uint32 = std::uint32_t;
#define P m.status
#define C_FLAG 1
#define CMPL(a1,a2) { \
    uint32 t=a1-a2;	\
    X_ZN(t&0xFF);	\
    P&=~C_FLAG;	\
    P|=((t>>8)&C_FLAG)^C_FLAG;	\
}

#define CMP CMPL(A,x)
#define CPX CMPL(X,x)
#define CPY CMPL(Y,x)
void original(Machine& m, unsigned opcode) {
    ++m.pc;
    switch (opcode) {
case 0xC9: LD_IM(CMP);
case 0xC5: LD_ZP(CMP);
case 0xD5: LD_ZPX(CMP);
case 0xCD: LD_AB(CMP);
case 0xDD: LD_ABX(CMP);
case 0xD9: LD_ABY(CMP);
case 0xD2: LD_IND(CMP);
case 0xC1: LD_IX(CMP);
case 0xD1: LD_IY(CMP);
case 0xE0: LD_IM(CPX);
case 0xE4: LD_ZP(CPX);
case 0xEC: LD_AB(CPX);
case 0xC0: LD_IM(CPY);
case 0xC4: LD_ZP(CPY);
case 0xCC: LD_AB(CPY);
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
    constexpr std::array<unsigned,15> opcodes{
        0xc9,0xc5,0xd5,0xcd,0xdd,0xd9,0xd2,0xc1,0xd1,0xe0,0xe4,0xec,0xc0,0xc4,0xcc};
    for (unsigned op = 0; op < 256; ++op) {
        bool supported = false;
        for (auto expected : opcodes) { supported |= op == expected; }
        if (devils_crush::cpu::is_huc6280_compare(static_cast<std::uint8_t>(op)) != supported) { return 1; }
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
                    devils_crush::cpu::run_huc6280_compare(static_cast<std::uint8_t>(op),
                        b.pc,b.status,b.accumulator,b.x,b.y,
                        [&](unsigned n) { b.advance(n); },
                        [&](unsigned address) { return b.operand(address); },
                        [&](unsigned address) { return b.read(address); },
                        [&]() { b.sample(); });
                    if (a.events != b.events || a.ram != b.ram || a.pc != b.pc
                        || a.status != b.status || a.stack != b.stack || a.accumulator != b.accumulator
                        || a.x != b.x || a.y != b.y || a.mask != b.mask || a.sampled_i != b.sampled_i
                        || a.time != b.time || a.remainder != b.remainder
                        || a.latch != b.latch || a.samples != 1 || b.samples != 1) {
                        std::cerr << "Comparison mismatch: " << op << ' ' << p << ' '
                            << value << ' ' << scenario << '\n'; return 1;
                    }
                    ++count;
                }
            }
        }
    }
    std::cout << count << " comparison differential cases passed\n";
}