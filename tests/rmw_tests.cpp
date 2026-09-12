// SPDX-License-Identifier: GPL-3.0-or-later
#include <devils_crush/cpu/huc6280_rmw.hpp>
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

struct Event {
    unsigned kind, address, value, pc, status, stack, accumulator, index_x, index_y, mask, time;
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


// Oracle operation and addressing macros copied from the pinned core.
#define PC m.pc
#define P m.status
#define X m.x
#define Y m.y
#define A m.accumulator
#define C_FLAG 1
#define Z_FLAG 2
#define N_FLAG 128
#define RdOp(a) m.operand(a)
#define RdMem(a) m.read(a)
#define WrMem(a,v) m.write(a,v)
#define ADDCYC(n) m.advance(n)
#define LastCycle() do { m.sample(); m.advance(1); } while(false)
#define X_ZNT(v) P |= ((v) & 0x80) | ((v) == 0 ? 2 : 0)
#define X_ZN(v) P &= ~0x82; X_ZNT(v)
using uint8 = std::uint8_t;
#define DEC   x--;X_ZN(x)
#define INC	  x++;X_ZN(x)

#define ASL   P&=~C_FLAG;P|=x>>7;x<<=1;X_ZN(x)
#define LSR	  P&=~(C_FLAG|N_FLAG|Z_FLAG);P|=x&1;x>>=1;X_ZNT(x)

#define ROL	{	\
	uint8 l=x>>7;	\
	x<<=1;	\
	x|=P&C_FLAG;	\
	P&=~(Z_FLAG|N_FLAG|C_FLAG);	\
	P|=l;	\
	X_ZNT(x);	\
}

#define ROR	{	\
	uint8 l=x&1;	\
	x>>=1;	\
	x|=(P&C_FLAG)<<7;	\
	P&=~(Z_FLAG|N_FLAG|C_FLAG);	\
	P|=l;	\
	X_ZNT(x);	\
}
		 
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

#define RMW_A(op)       { uint8 x = A; op; A = x; ADDCYC(1); LastCycle(); break; } /* Meh... */
#define RMW_AB(op)      { unsigned int EA; uint8 x; GetAB(EA); ADDCYC(6); x=RdMem(EA); op; LastCycle(); WrMem(EA,x); break; }
#define RMW_ABI(reg,op) { unsigned int EA; uint8 x; GetABI(EA,reg); ADDCYC(6); x=RdMem(EA); op; LastCycle(); WrMem(EA,x); break; }
#define RMW_ABX(op)	RMW_ABI(X,op)
#define RMW_ABY(op)	RMW_ABI(Y,op)
#define RMW_ZP(op)      { unsigned int EA; uint8 x; GetZP(EA); ADDCYC(5); x=RdMem(EA); op; LastCycle(); WrMem(EA,x); break; }
#define RMW_ZPX(op)     { unsigned int EA; uint8 x; GetZPI(EA, X); ADDCYC(5); x=RdMem(EA); op; LastCycle(); WrMem(EA,x); break;}

#define IMP(op) op; break;
#define DEX	{ X--; X_ZN(X);  ADDCYC(1); LastCycle(); }
#define DEY	{ Y--; X_ZN(Y);  ADDCYC(1); LastCycle(); }
#define INX	{ X++; X_ZN(X);  ADDCYC(1); LastCycle(); }
#define INY	{ Y++; X_ZN(Y);  ADDCYC(1); LastCycle(); }

void original(Machine& m, unsigned opcode) {
 ++PC;
 switch(opcode) {
case 0xCA: IMP(DEX);
case 0x88: IMP(DEY);
case 0xE8: IMP(INX);
case 0xC8: IMP(INY);
case 0x0A: RMW_A(ASL);
case 0x06: RMW_ZP(ASL);
case 0x16: RMW_ZPX(ASL);
case 0x0E: RMW_AB(ASL);
case 0x1E: RMW_ABX(ASL);
case 0x3A: RMW_A(DEC);
case 0xC6: RMW_ZP(DEC);
case 0xD6: RMW_ZPX(DEC);
case 0xCE: RMW_AB(DEC);
case 0xDE: RMW_ABX(DEC);
case 0x1A: RMW_A(INC);		// 65C02
case 0xE6: RMW_ZP(INC);
case 0xF6: RMW_ZPX(INC);
case 0xEE: RMW_AB(INC);
case 0xFE: RMW_ABX(INC);
case 0x4A: RMW_A(LSR);
case 0x46: RMW_ZP(LSR);
case 0x56: RMW_ZPX(LSR);
case 0x4E: RMW_AB(LSR);
case 0x5E: RMW_ABX(LSR);
case 0x2A: RMW_A(ROL);
case 0x26: RMW_ZP(ROL);
case 0x36: RMW_ZPX(ROL);
case 0x2E: RMW_AB(ROL);
case 0x3E: RMW_ABX(ROL);
case 0x6A: RMW_A(ROR);
case 0x66: RMW_ZP(ROR);
case 0x76: RMW_ZPX(ROR);
case 0x6E: RMW_AB(ROR);
case 0x7E: RMW_ABX(ROR);
 }
 P &= static_cast<std::uint8_t>(~0x20u);
}
#undef P
#undef PC
#undef X
#undef Y
#undef A
int main()
{
    std::uint64_t count = 0;
    constexpr std::array<unsigned,34> opcodes{0xCA,0x88,0xE8,0xC8,0x0A,0x06,0x16,0x0E,0x1E,0x3A,0xC6,0xD6,0xCE,0xDE,0x1A,0xE6,0xF6,0xEE,0xFE,0x4A,0x46,0x56,0x4E,0x5E,0x2A,0x26,0x36,0x2E,0x3E,0x6A,0x66,0x76,0x6E,0x7E};
    for (unsigned op = 0; op < 256; ++op) {
        bool supported = false;
        for (auto expected : opcodes) { supported |= op == expected; }
        if (devils_crush::cpu::is_huc6280_rmw(static_cast<std::uint8_t>(op)) != supported) { return 1; }
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
                    devils_crush::cpu::run_huc6280_rmw(static_cast<std::uint8_t>(op),
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
                        || a.latch != b.latch || a.samples != 1 || b.samples != a.samples) {
                        std::cerr << "RMW mismatch: " << op << ' ' << p << ' '
                            << value << ' ' << scenario << '\n'; return 1;
                    }
                    ++count;
                }
            }
        }
    }
    std::cout << count << " rmw differential cases passed\n";
}