// SPDX-License-Identifier: GPL-3.0-or-later
#include <devils_crush/cpu/huc6280_logical.hpp>
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

// Independent oracle: addressing/load macros and opcode map copied verbatim
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
#define P m.status
#define T_FLAG 0x20
#define TPREFIX { uint8 Abackup = A; if(P & T_FLAG) { ADDCYC(1); A = RdMem(0x2000 + X); }
#define TPOSTFIX if(P & T_FLAG) { ADDCYC(1); WrMem(0x2000 + X, A); LastCycle(); A = Abackup; } }
#define AND TPREFIX; A&=x;X_ZN(A); TPOSTFIX;
#define EOR TPREFIX; A^=x;X_ZN(A); TPOSTFIX;
#define ORA TPREFIX; A|=x;X_ZN(A); TPOSTFIX;
void original(Machine& m, unsigned opcode) {
    ++m.pc;
    switch (opcode) {
case 0x29: LD_IM(AND);
case 0x25: LD_ZP(AND);
case 0x35: LD_ZPX(AND);
case 0x2D: LD_AB(AND);
case 0x3D: LD_ABX(AND);
case 0x39: LD_ABY(AND);
case 0x32: LD_IND(AND);
case 0x21: LD_IX(AND);
case 0x31: LD_IY(AND);
case 0x49: LD_IM(EOR);
case 0x45: LD_ZP(EOR);
case 0x55: LD_ZPX(EOR);
case 0x4D: LD_AB(EOR);
case 0x5D: LD_ABX(EOR);
case 0x59: LD_ABY(EOR);
case 0x52: LD_IND(EOR);
case 0x41: LD_IX(EOR);
case 0x51: LD_IY(EOR);
case 0x09: LD_IM(ORA);
case 0x05: LD_ZP(ORA);
case 0x15: LD_ZPX(ORA);
case 0x0D: LD_AB(ORA);
case 0x1D: LD_ABX(ORA);
case 0x19: LD_ABY(ORA);
case 0x12: LD_IND(ORA);
case 0x01: LD_IX(ORA);
case 0x11: LD_IY(ORA);
    }
    m.status &= static_cast<std::uint8_t>(~0x20u);
}
#undef P
#undef PC
#undef X
#undef Y
#undef A
int main()
{
    std::uint64_t count = 0;
    constexpr std::array<unsigned,27> opcodes{
        0x29,0x25,0x35,0x2D,0x3D,0x39,0x32,0x21,0x31,0x49,0x45,0x55,0x4D,0x5D,0x59,0x52,0x41,0x51,0x09,0x05,0x15,0x0D,0x1D,0x19,0x12,0x01,0x11};
    for (unsigned op = 0; op < 256; ++op) {
        bool supported = false;
        for (auto expected : opcodes) { supported |= op == expected; }
        if (devils_crush::cpu::is_huc6280_logical(static_cast<std::uint8_t>(op)) != supported) { return 1; }
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
                    a.ram[0x2000u + a.x] = static_cast<std::uint8_t>(value);
                    a.ram[(a.pc + 1) & 65535] = static_cast<std::uint8_t>(scenario & 2 ? 255 : value);
                    a.ram[(a.pc + 2) & 65535] = static_cast<std::uint8_t>(scenario & 4 ? 255 : 0);
                    auto b = a;
                    original(a,op);
                    devils_crush::cpu::run_huc6280_logical(static_cast<std::uint8_t>(op),
                        b.pc,b.status,b.accumulator,b.x,b.y,
                        [&](unsigned n) { b.advance(n); },
                        [&](unsigned address) { return b.operand(address); },
                        [&](unsigned address) { return b.read(address); },
                        [&](unsigned address, std::uint8_t v) { b.write(address,v); },
                        [&]() { b.sample(); });
                    if (a.events != b.events || a.ram != b.ram || a.pc != b.pc
                        || a.status != b.status || a.stack != b.stack || a.accumulator != b.accumulator
                        || a.x != b.x || a.y != b.y || a.mask != b.mask || a.sampled_i != b.sampled_i
                        || a.time != b.time || a.remainder != b.remainder
                        || a.latch != b.latch || a.samples != (p & 0x20 ? 2u : 1u) || b.samples != a.samples) {
                        std::cerr << "Logical mismatch: " << op << ' ' << p << ' '
                            << value << ' ' << scenario << '\n'; return 1;
                    }
                    ++count;
                }
            }
        }
    }
    std::cout << count << " logical differential cases passed\n";
}