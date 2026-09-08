// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/app/native_replacements_internal.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>
using namespace devils_crush::emulation;
using devils_crush::app::detail::replace_actor_initialization;
void require(bool ok) { if (!ok) throw std::runtime_error("Actor initialization mismatch"); }
int main()
{
    std::array<std::byte,0x2000> ram{};
    RoutineEntry e{};
    e.registers.mpr[1]=0xF8; e.registers.mpr[6]=1;
    for (unsigned x=0;x<256;++x) for (unsigned v=0;v<256;++v) {
        e.registers.x=static_cast<std::uint8_t>(x);
        e.registers.stack_pointer=static_cast<std::uint8_t>(v);
        e.registers.y=0x75;
        e.registers.status=static_cast<std::uint8_t>(v & ~0x28U);
        ram[0x1080+x]=static_cast<std::byte>(v);
        ram[0x53]=static_cast<std::byte>(x);
        for (auto pc : {0xC655U,0xC699U,0xC69EU,0xC6A9U}) {
            e.routine={1,static_cast<std::uint16_t>(pc)};
            auto result=replace_actor_initialization(e,ram);
            require(result.has_value());
            const auto& r=result->output_registers;
            require(r.stack_pointer==v && r.y==0x75 && r.x==(pc==0xC655U?0:x));
            const unsigned expected=pc==0xC655U?0x3C:pc==0xC699U?0x30:pc==0xC69EU?(v^0x20U):(x|8U);
            require(r.accumulator==expected);
            require(r.status==((e.registers.status&0x7DU)|(expected==0?2U:expected&0x80U)));
            const auto& w=result->work_ram_writes;
            if(pc==0xC655U) {
                require(result->body_master_cycles==285 && w.size()==16 && r.program_counter==0xC693);
                require(result->checkpoints.size()==26);
                require(result->checkpoints[2].registers.x==0
                    && result->checkpoints[2].registers.status==e.registers.status);
                constexpr std::array<unsigned,16> addresses{0x20CC,0x2D32,0x3000,0x3180,0x3200,0x3300,0x3380,0x3080,0x3800,0x3D00,0x3900,0x20B9,0x20BA,0x20BB,0x20BC,0x3980};
                constexpr std::array<unsigned,16> values{0,0,0x80,0x0E,1,0x8F,2,0xA0,0x13,0x80,0,0,0,0,0,0};
                for(std::size_t i=0;i<w.size();++i) require(w[i].logical_address==addresses[i] && w[i].value==values[i]);
            } else if(pc==0xC699U) {
                require(result->body_master_cycles==6 && w.empty() && r.program_counter==0xC69B);
            } else if(pc==0xC69EU) {
                require(result->body_master_cycles==36 && w.size()==1 && r.program_counter==0xC6A6);
                require(w[0].logical_address==0x3080+x && w[0].value==(v^0x20U));
            } else {
                require(result->body_master_cycles==102 && w.size()==4 && r.program_counter==0xC6BF);
                require(w[0].logical_address==0x3080+x && w[0].value==(v&0xDFU));
                require(w[1].logical_address==0x3A80+x && w[1].value==0);
                require(w[2].logical_address==0x3A00+x && w[2].value==4);
                require(w[3].logical_address==0x2053 && w[3].value==(x|8U));
            }
            for(const auto& c:result->checkpoints) require(c.interrupt_sample_master_cycle_offset+3==c.master_cycle_offset);
            for(const auto& write:w) {
                bool found=false;
                for(const auto& c:result->checkpoints) if(c.instruction==write.instruction) {
                    require(c.master_cycle_offset==write.master_cycle_offset); found=true;
                }
                require(found);
            }
        }
    }
    for(unsigned p=0;p<256;++p) {
        e.registers.status=static_cast<std::uint8_t>(p);
        require(replace_actor_initialization(e,ram).has_value()==((p&0x28U)==0));
    }
    e.registers.status=0; e.registers.mpr[1]=0;
    require(!replace_actor_initialization(e,ram));
    e.registers.mpr[1]=0xF8; e.registers.mpr[6]=0;
    require(!replace_actor_initialization(e,ram));
    e.registers.mpr[6]=1;
    require(!replace_actor_initialization(e,std::span<const std::byte>(ram).first(0x1FFF)));
    e.routine={2,0xC6A9}; require(!replace_actor_initialization(e,ram));
    e.routine={1,0xC6A6}; require(!replace_actor_initialization(e,ram));
    std::puts("Verified 262144 actor initialization/continuation cases plus guards.");
}
