// SPDX-License-Identifier: GPL-3.0-or-later
#include <devils_crush/cpu/huc6280_arithmetic.hpp>
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
    void speed(unsigned fast) { event(5,0,fast); scale = fast ? 3u : 12u; }
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
#define PC oracle.pc
#define X oracle.x
#define Y oracle.y
#define A oracle.accumulator
#define RdOp(a) oracle.operand(a)
#define RdMem(a) oracle.read(a)
#define WrMem(a,v) oracle.write(a,v)
#define ADDCYC(n) oracle.advance(n)
#define LastCycle() do { oracle.sample(); oracle.advance(1); } while(false)
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

#define X_ZN(v) oracle.status = static_cast<std::uint8_t>((oracle.status & ~0x82u) | ((v) & 0x80u) | ((v) == 0 ? 2u : 0u))
#define P oracle.status
#define T_FLAG 0x20
#define TPREFIX { uint8 Abackup = A; if(P & T_FLAG) { ADDCYC(1); A = RdMem(0x2000 + X); }
#define TPOSTFIX if(P & T_FLAG) { ADDCYC(1); WrMem(0x2000 + X, A); LastCycle(); A = Abackup; } }
using uint32 = std::uint32_t;
#define D_FLAG 8
#define Z_FLAG 2
#define N_FLAG 128
#define C_FLAG 1
#define V_FLAG 64
#define X_ZNT(v) P |= ((v) & 0x80) | ((v) == 0 ? 2 : 0)
#define ADC  \
TPREFIX; {					\
	if(P & D_FLAG)			\
	{						\
		uint32 tmp;			\
		tmp = (A & 0x0F) + (x & 0x0F) + (P & 1);	\
		if(tmp >= 0x0A)		\
			tmp += 0x06;	\
		tmp += (A & 0xF0) + (x & 0xF0);		\
		if(tmp >= 0xA0)		\
			tmp += 0x60;	\
		P &= ~(Z_FLAG | N_FLAG | C_FLAG);	\
		if(tmp & 0xFF00)	\
			P |= C_FLAG;	\
		A = tmp;			\
		X_ZNT(A);			\
		LastCycle();		\
	}	\
	else	\
	{	\
		uint32 l=A+x+(P&1);	\
		P&=~(Z_FLAG|C_FLAG|N_FLAG|V_FLAG);	\
		P|=((((A^x)&0x80)^0x80) & ((A^l)&0x80))>>1;	\
		P|=(l>>8)&C_FLAG;	\
		A=l;	\
		X_ZNT(A);	\
	}	\
} TPOSTFIX;

#define SBC  \
	if(P & D_FLAG)						\
	{								\
		const uint8 m = (A & 0xF) - (x & 0xF) - ((P & 1) ^ 1);	\
		const uint8 n = (A >> 4) - (x >> 4) - ((m >> 4) & 1);	\
		uint8 res = (n << 4) | (m & 0xF);				\
		P &= ~(Z_FLAG | N_FLAG | C_FLAG);				\
		if(m & 0x10)						\
			res -= 0x06;						\
		if(n & 0x10)						\
			res -= 0x60;						\
		A = res;							\
		P |= ((n >> 4) & 0x1) ^ 1;				\
		X_ZNT(A);							\
		LastCycle();      						\
	} else {	\
		uint32 l=A-x-((P&1)^1);	\
		P&=~(Z_FLAG|C_FLAG|N_FLAG|V_FLAG);	\
		P|=((A^l)&(A^x)&0x80)>>1;	\
		P|=((l>>8)&C_FLAG)^C_FLAG;	\
		A=l;	\
		X_ZNT(A);	\
	}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4244)
#endif
void original(Machine& oracle, unsigned opcode) {
    ++oracle.pc;
    switch (opcode) {
case 0x69: LD_IM(ADC);
case 0x65: LD_ZP(ADC);
case 0x75: LD_ZPX(ADC);
case 0x6D: LD_AB(ADC);
case 0x7D: LD_ABX(ADC);
case 0x79: LD_ABY(ADC);
case 0x72: LD_IND(ADC);
case 0x61: LD_IX(ADC);
case 0x71: LD_IY(ADC);
case 0xE9: LD_IM(SBC);
case 0xE5: LD_ZP(SBC);
case 0xF5: LD_ZPX(SBC);
case 0xED: LD_AB(SBC);
case 0xFD: LD_ABX(SBC);
case 0xF9: LD_ABY(SBC);
case 0xF2: LD_IND(SBC);
case 0xE1: LD_IX(SBC);
case 0xF1: LD_IY(SBC);
case 0x54: oracle.advance(2); oracle.speed(0); LastCycle(); break;
case 0xD4: oracle.advance(2); oracle.speed(1); LastCycle(); break;
    }
    oracle.status &= static_cast<std::uint8_t>(~0x20u);
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#undef P
#undef PC
#undef X
#undef Y
#undef A
int main()
{
    std::uint64_t count = 0;
    constexpr std::array<unsigned,20> opcodes{
        0x69,0x65,0x75,0x6D,0x7D,0x79,0x72,0x61,0x71,
        0xE9,0xE5,0xF5,0xED,0xFD,0xF9,0xF2,0xE1,0xF1,0x54,0xD4};
    for (unsigned op = 0; op < 256; ++op) {
        bool supported = false;
        for (auto expected : opcodes) { supported |= op == expected; }
        if (devils_crush::cpu::is_huc6280_arithmetic(static_cast<std::uint8_t>(op)) != supported) { return 1; }
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
                    devils_crush::cpu::run_huc6280_arithmetic(static_cast<std::uint8_t>(op),
                        b.pc,b.status,b.accumulator,b.x,b.y,
                        [&](unsigned n) { b.advance(n); },
                        [&](unsigned address) { return b.operand(address); },
                        [&](unsigned address) { return b.read(address); },
                        [&](unsigned address, std::uint8_t v) { b.write(address,v); },
                        [&]() { b.sample(); }, [&](unsigned fast) { b.speed(fast); });
                    if (a.events != b.events || a.ram != b.ram || a.pc != b.pc
                        || a.status != b.status || a.stack != b.stack || a.accumulator != b.accumulator
                        || a.x != b.x || a.y != b.y || a.mask != b.mask || a.sampled_i != b.sampled_i
                        || a.time != b.time || a.remainder != b.remainder || a.scale != b.scale
                        || a.latch != b.latch || a.samples != ((op == 0x54 || op == 0xd4) ? 1u : (1u + ((p & 8) ? 1u : 0u) + ((op < 0x80 && (p & 0x20)) ? 1u : 0u))) || b.samples != a.samples) {
                        std::cerr << "Arithmetic mismatch: " << op << ' ' << p << ' '
                            << value << ' ' << scenario << '\n'; return 1;
                    }
                    ++count;
                }
            }
        }
    }
    // Exhaust every accumulator/operand pair with independent C/D/V/T flags,
    // including invalid BCD digits. Reuse memory to keep this matrix inexpensive.
    Machine a{0x4000,0,0,0,3,1,0,4,1}, b = a;
    for (unsigned op : {0x69u,0xe9u}) {
        for (unsigned flags = 0; flags < 16; ++flags) {
            const auto p = static_cast<std::uint8_t>((flags & 1)
                | ((flags & 2) << 2) | ((flags & 4) << 3) | ((flags & 8) << 3));
            for (unsigned left = 0; left < 256; ++left) {
                for (unsigned right = 0; right < 256; ++right) {
                    for (auto* m : {&a,&b}) {
                        m->pc = 0x4000; m->status = p;
                        m->accumulator = static_cast<std::uint8_t>(left);
                        m->ram[0x4001] = static_cast<std::uint8_t>(right);
                        m->ram[0x2000] = static_cast<std::uint8_t>(255-left);
                        m->time = m->remainder = m->samples = m->latch = m->sampled_i = 0;
                        m->events.clear();
                    }
                    original(a,op);
                    devils_crush::cpu::run_huc6280_arithmetic(static_cast<std::uint8_t>(op),
                        b.pc,b.status,b.accumulator,b.x,b.y,
                        [&](unsigned n) { b.advance(n); },
                        [&](unsigned address) { return b.operand(address); },
                        [&](unsigned address) { return b.read(address); },
                        [&](unsigned address, std::uint8_t v) { b.write(address,v); },
                        [&]() { b.sample(); }, [&](unsigned fast) { b.speed(fast); });
                    if (a.events != b.events || a.status != b.status || a.accumulator != b.accumulator
                        || a.ram[0x2000] != b.ram[0x2000] || a.pc != b.pc || a.time != b.time
                        || a.samples != b.samples || a.latch != b.latch || a.sampled_i != b.sampled_i) {
                        std::cerr << "Arithmetic operand matrix mismatch: " << op << ' '
                            << flags << ' ' << left << ' ' << right << '\n'; return 1;
                    }
                    ++count;
                }
            }
        }
    }
    std::cout << count << " arithmetic differential cases passed\n";
}