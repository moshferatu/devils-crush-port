// SPDX-License-Identifier: GPL-3.0-or-later
#include <devils_crush/cpu/huc6280_register.hpp>
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
    std::array<std::uint8_t,9> mapping{};
    std::vector<Event> events;
    std::uint8_t get_mapping(unsigned i) { event(5,i,mapping[i]); return mapping[i]; }
    void set_mapping(unsigned i, std::uint8_t v) { event(6,i,v); mapping[i] = v; }
    void publish_mask() { mask = status & 4 ? 0u : 1u; event(7,0,mask); }
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

 // Independent oracle: verbatim pinned register macros, mapping macros and
 // flag/clear cases, using observable timing, mapping and interrupt adapters.
#define A m.accumulator
#define X m.x
#define Y m.y
#define S m.stack
#define P m.status
#define ADDCYC(n) m.advance(n)
#define LastCycle() do { m.sample(); m.advance(1); } while(false)
#define T_FLAG 0x20
#define C_FLAG 1
#define D_FLAG 8
#define I_FLAG 4
#define V_FLAG 0x40
#define REDOPIMCACHE() m.publish_mask()
#define SetMPR(i,v) m.set_mapping(i,v)
#define X_ZN(v) m.status = static_cast<std::uint8_t>((m.status & ~0x82u) | ((v) & 0x80u) | ((v) == 0 ? 2u : 0u))
using uint8 = std::uint8_t;
#define SAX	{ uint8 tmp = X; X = A; A = tmp; ADDCYC(2); LastCycle(); }
#define SAY	{ uint8 tmp = Y; Y = A; A = tmp; ADDCYC(2); LastCycle(); }
#define SXY	{ uint8 tmp = X; X = Y; Y = tmp; ADDCYC(2); LastCycle(); }

#define TAX	{ X = A; X_ZN(A); ADDCYC(1); LastCycle(); }
#define TXA	{ A = X; X_ZN(A); ADDCYC(1); LastCycle(); }
#define TAY	{ Y = A; X_ZN(A); ADDCYC(1); LastCycle(); }
#define TYA	{ A = Y; X_ZN(A); ADDCYC(1); LastCycle(); }
#define TSX	{ X = S; X_ZN(X); ADDCYC(1); LastCycle(); }
#define TXS	{ S = X;	  ADDCYC(1); LastCycle(); }

#define TAM \
    for(int i = 0; i < 8; i ++) {               \
		if(x & (1 << i))        \
		{	\
			SetMPR(i, A);	\
		}	\
	} SetMPR(8, m.get_mapping(0));	\
	ADDCYC(4);	\
	LastCycle();

#define TMA	\
	for(int i = 0; i < 8; i ++) {		\
		if(x & (1 << i))	\
			A = m.get_mapping(i);	\
	}		\
	ADDCYC(3);	\
	LastCycle();

void original(Machine& m, unsigned opcode) {
    ++m.pc;
    switch (opcode) {
    case 0xaa: TAX; break;
    case 0x8a: TXA; break;
    case 0xa8: TAY; break;
    case 0x98: TYA; break;
    case 0xba: TSX; break;
    case 0x9a: TXS; break;
    case 0x02: SXY; break;
    case 0x22: SAX; break;
    case 0x42: SAY; break;
    case 0x43: { uint8 x = m.operand(m.pc); m.pc++; TMA; break; }
    case 0x53: { uint8 x = m.operand(m.pc); m.pc++; TAM; break; }
#define OP_CLEARR(r)	{ ADDCYC(1); LastCycle(); r = 0; break; }

case 0x62: OP_CLEARR(A); // CLA
case 0x82: OP_CLEARR(X); // CLX
case 0xC2: OP_CLEARR(Y); // CLY

// The optional argument(s) will run at the end, immediately before the break.
#define OP_CLEARF(f, ...)	{ ADDCYC(1); LastCycle(); P &= ~f; __VA_ARGS__ break; }
#define OP_SETF(f, ...)	{ ADDCYC(1); LastCycle(); P |= f; __VA_ARGS__ break; }

case 0x18: /* CLC */
	   OP_CLEARF(C_FLAG);

case 0xD8: /* CLD */
	   OP_CLEARF(D_FLAG);

case 0x58: /* CLI */
	   OP_CLEARF(I_FLAG, REDOPIMCACHE(););

case 0xB8: /* CLV */
	   OP_CLEARF(V_FLAG);

case 0x38: /* SEC */
	   OP_SETF(C_FLAG);

case 0xF8: /* SED */
	   OP_SETF(D_FLAG);

case 0x78: /* SEI */
	   OP_SETF(I_FLAG, REDOPIMCACHE(););

case 0xF4: /* SET */
	   ADDCYC(1);
	   LastCycle();
	   P |= T_FLAG;

	   goto skip_T_flag_clear;

	   break;


case 0xEA: /* NOP */
	   ADDCYC(1);
	   LastCycle();
           break;

    }
    P &= static_cast<std::uint8_t>(~T_FLAG);
skip_T_flag_clear:;
}
#undef A
#undef X
#undef Y
#undef S
#undef P
int main()
{
    std::uint64_t count = 0;
    constexpr std::array<unsigned,22> opcodes{
        0xaa,0x8a,0xa8,0x98,0xba,0x9a,0x02,0x22,0x42,0x62,0x82,0xc2,0x18,0x38,0x58,0xb8,0xd8,0xf8,0xf4,0xea,0x43,0x53};
    for (unsigned op = 0; op < 256; ++op) {
        bool supported = false;
        for (auto expected : opcodes) { supported |= op == expected; }
        if (devils_crush::cpu::is_huc6280_register(static_cast<std::uint8_t>(op)) != supported) { return 1; }
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
                    for (unsigned i = 0; i < 9; ++i) {
                        a.mapping[i] = static_cast<std::uint8_t>(value + i * 29 + p);
                    }
                    auto b = a;
                    original(a,op);
                    devils_crush::cpu::run_huc6280_register(static_cast<std::uint8_t>(op),
                        b.pc,b.status,b.stack,b.accumulator,b.x,b.y,
                        [&](unsigned n) { b.advance(n); },
                        [&](unsigned address) { return b.operand(address); },
                        [&](unsigned index) { return b.get_mapping(index); },
                        [&](unsigned index, std::uint8_t data) { b.set_mapping(index,data); },
                        [&]() { b.sample(); },
                        [&]() { b.publish_mask(); });
                    if (a.events != b.events || a.mapping != b.mapping || a.ram != b.ram || a.pc != b.pc
                        || a.status != b.status || a.stack != b.stack || a.accumulator != b.accumulator
                        || a.x != b.x || a.y != b.y || a.mask != b.mask || a.sampled_i != b.sampled_i
                        || a.time != b.time || a.remainder != b.remainder
                        || a.latch != b.latch || a.samples != 1 || b.samples != 1) {
                        std::cerr << "Register/mapping mismatch: " << op << ' ' << p << ' '
                            << value << ' ' << scenario << '\n'; return 1;
                    }
                    ++count;
                }
            }
        }
    }
    std::cout << count << " register/mapping differential cases passed\n";
}