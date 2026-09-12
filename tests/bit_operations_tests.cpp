// SPDX-License-Identifier: GPL-3.0-or-later
#include <devils_crush/cpu/huc6280_bit_operations.hpp>
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

#define V_FLAG 64
constexpr auto ZNTable = [] { std::array<unsigned,256> t{}; for(unsigned i=0;i<256;++i) t[i]=(i&128)|(i==0?2:0); return t; }();
#define BIT        P &= ~(Z_FLAG|V_FLAG|N_FLAG); P|=ZNTable[x&A]&Z_FLAG; P|=x&(V_FLAG|N_FLAG);
#define RMB(bitto)	x &= ~(1 << (bitto & 7))
#define SMB(bitto)	x |= 1 << (bitto & 7)
#define TSB   { P &= ~(Z_FLAG | V_FLAG | N_FLAG); P |= (x | A) ? 0 : Z_FLAG; P |= x & (N_FLAG | V_FLAG); x |= A; }
#define TRB   { P &= ~(Z_FLAG | V_FLAG | N_FLAG); P |= (x & ~A) ? 0 : Z_FLAG; P |= x & (N_FLAG | V_FLAG); x &= ~A; }
#define TST	  { P &= ~(Z_FLAG | V_FLAG | N_FLAG); P |= (x & zoomhack) ? 0: Z_FLAG; P |= x & (V_FLAG | N_FLAG); }
#define GetAB(target) {	\
	target=RdOp(PC);	\
	PC++;	\
	target|=RdOp(PC)<<8;	\
	PC++;	\
}
#define GetABI(target, i) {	\
	unsigned int tmp;	\
	GetAB(tmp);	\
	target=tmp;	\
	target+=i;	\
}
#define GetZP(target) {	\
	target=0x2000 | RdOp(PC); 	\
	PC++;	\
}
#define GetZPI(target,i) {	\
	target=0x2000 | ((i+RdOp(PC)) & 0xFF);	\
	PC++;	\
}
#define RMW_AB(op)      { unsigned int EA; uint8 x; GetAB(EA); ADDCYC(6); x=RdMem(EA); op; LastCycle(); WrMem(EA,x); break; }
#define RMW_ZP(op)      { unsigned int EA; uint8 x; GetZP(EA); ADDCYC(5); x=RdMem(EA); op; LastCycle(); WrMem(EA,x); break; }
#define RMW_ZP_B(op)    { unsigned int EA; uint8 x; GetZP(EA); ADDCYC(5); x=RdMem(EA); ADDCYC(1); op; LastCycle(); WrMem(EA,x); break; }
#define LD_IM(op)   {uint8 x; x=RdOp(PC); PC++; ADDCYC(1); LastCycle(); op; break;}
#define LD_ZP(op)   {unsigned int EA; uint8 x; GetZP(EA); ADDCYC(3); LastCycle(); x=RdMem(EA); op; break;}
#define LD_ZPX(op)  {unsigned int EA; uint8 x; GetZPI(EA, X); ADDCYC(3); LastCycle(); x=RdMem(EA); op; break;}
#define LD_AB(op)   {unsigned int EA; uint8 x; GetAB(EA); ADDCYC(4); LastCycle(); x=RdMem(EA); op; break; }
#define LD_ABI(reg,op)  {unsigned int EA; uint8 x; GetABI(EA,reg); ADDCYC(4); LastCycle(); x=RdMem(EA); op; break;}
#define LD_ABX(op)  LD_ABI(X, op)
#define LD_IM_TST(op, lt)       { uint8 lt = RdOp(PC); PC++; ADDCYC(3); op; }
#define LD_IM_ZP(op)	LD_IM_TST(LD_ZP(TST), zoomhack);
#define LD_IM_ZPX(op)	LD_IM_TST(LD_ZPX(TST), zoomhack);
#define LD_IM_AB(op)	LD_IM_TST(LD_AB(TST), zoomhack);
#define LD_IM_ABX(op)	LD_IM_TST(LD_ABX(TST), zoomhack);

void original(Machine& m, unsigned opcode) {
 ++PC;
 switch(opcode) {
case 0x89: LD_IM(BIT);
case 0x24: LD_ZP(BIT);
case 0x34: LD_ZPX(BIT);
case 0x2C: LD_AB(BIT);
case 0x3C: LD_ABX(BIT);
case 0x07: RMW_ZP_B(RMB(0));
case 0x17: RMW_ZP_B(RMB(1));
case 0x27: RMW_ZP_B(RMB(2));
case 0x37: RMW_ZP_B(RMB(3));
case 0x47: RMW_ZP_B(RMB(4));
case 0x57: RMW_ZP_B(RMB(5));
case 0x67: RMW_ZP_B(RMB(6));
case 0x77: RMW_ZP_B(RMB(7));
case 0x87: RMW_ZP_B(SMB(0));
case 0x97: RMW_ZP_B(SMB(1));
case 0xa7: RMW_ZP_B(SMB(2));
case 0xb7: RMW_ZP_B(SMB(3));
case 0xc7: RMW_ZP_B(SMB(4));
case 0xd7: RMW_ZP_B(SMB(5));
case 0xe7: RMW_ZP_B(SMB(6));
case 0xf7: RMW_ZP_B(SMB(7));
case 0x14: RMW_ZP(TRB);
case 0x1C: RMW_AB(TRB);
case 0x04: RMW_ZP(TSB);
case 0x0C: RMW_AB(TSB);
case 0x83: LD_IM_ZP(TST);
case 0xA3: LD_IM_ZPX(TST);
case 0x93: LD_IM_AB(TST);
case 0xB3: LD_IM_ABX(TST);
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
    constexpr std::array<unsigned,29> opcodes{0x89,0x24,0x34,0x2C,0x3C,0x07,0x17,0x27,0x37,0x47,0x57,0x67,0x77,0x87,0x97,0xa7,0xb7,0xc7,0xd7,0xe7,0xf7,0x14,0x1C,0x04,0x0C,0x83,0xA3,0x93,0xB3};
    for (unsigned op = 0; op < 256; ++op) {
        bool supported = false;
        for (auto expected : opcodes) { supported |= op == expected; }
        if (devils_crush::cpu::is_huc6280_bit_operations(static_cast<std::uint8_t>(op)) != supported) { return 1; }
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
                    devils_crush::cpu::run_huc6280_bit_operations(static_cast<std::uint8_t>(op),
                        b.pc,b.status,b.accumulator,b.x,
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
                        std::cerr << "Bit operations mismatch: " << op << ' ' << p << ' '
                            << value << ' ' << scenario << '\n'; return 1;
                    }
                    ++count;
                }
            }
        }
    }
    std::cout << count << " bit operations differential cases passed\n";
}