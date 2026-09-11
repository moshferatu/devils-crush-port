// SPDX-License-Identifier: GPL-3.0-or-later
void verify_ranked_vdc_instructions()
{
    using devils_crush::app::detail::replace_ranked_vdc_instruction;
    enum Op { indirect, port, decrement, branch, ret, clear, store, store_x, compare };
    struct Case { unsigned bank; std::uint16_t pc, operand; Op op; unsigned cycles, sample, length; };
    const Case cases[] = {
        {9,0x62E0,0,indirect,21,18,2}, {9,0x62E6,0,indirect,21,18,2},
        {9,0x62E8,3,port,18,12,3}, {9,0x62E2,2,port,18,12,3},
        {9,0x60BA,0x2D55,decrement,21,18,3}, {9,0x62EE,0x62E0,branch,12,9,2},
        {9,0x60BF,0,ret,21,18,1}, {9,0x608D,0x2CBA,clear,15,12,3},
        {9,0x6090,0x2CBC,clear,15,12,3}, {9,0x6095,0x2CBB,store,15,12,3},
        {9,0x6099,0x204D,store_x,12,9,2}, {2,0x4D30,0x78,compare,6,3,2}
    };
    std::array<std::byte,8192> ram{};
    std::vector<std::byte> rom(0x60000);
    for (unsigned i=0; i<rom.size(); ++i) { rom[i]=static_cast<std::byte>((i / 8192 + i) & 255); }
    for (const auto& test : cases) {
        RoutineEntry entry{};
        entry.routine={static_cast<std::uint8_t>(test.bank),test.pc};
        auto& r=entry.registers;
        r.program_counter=test.pc; r.mpr.fill(0xF8); r.mpr[0]=0xFF;
        r.mpr[test.pc >> 13]=static_cast<std::uint8_t>(test.bank);
        for (unsigned value=0; value<256; ++value) {
            r.accumulator=r.stack_pointer=static_cast<std::uint8_t>(value);
            r.x=r.y=static_cast<std::uint8_t>(255-value);
            ram.fill(std::byte{0xA5}); ram[0x2F]=std::byte{0}; ram[0x30]=std::byte{0x28};
            ram[0x800+r.y]=static_cast<std::byte>(value); ram[0xD55]=static_cast<std::byte>(value);
            ram[0x100+((value+1)%256)]=std::byte{0xFF};
            ram[0x100+((value+2)%256)]=std::byte{0xFF};
            for (unsigned p=0; p<256; ++p) {
                r.status=static_cast<std::uint8_t>(p);
                const auto result=replace_ranked_vdc_instruction(entry,ram,rom);
                require(result.has_value()==((p&32)==0), "Ranked VDC T guard, decimal acceptance");
                if (!result) { continue; }
                auto expected=r;
                expected.program_counter=static_cast<std::uint16_t>(test.pc+test.length);
                unsigned cycles=test.cycles, sample=test.sample, byte=value;
                if (test.op==indirect || test.op==decrement) {
                    byte=test.op==decrement ? (value+255)%256 : value;
                    if (test.op==indirect) { expected.accumulator=static_cast<std::uint8_t>(byte); }
                    expected.status=static_cast<std::uint8_t>((p&0x7D)|(byte==0 ? 2 : byte&128));
                } else if (test.op==branch) {
                    if ((p&2)==0) { expected.program_counter=test.operand; }
                    else { cycles=6; sample=3; }
                } else if (test.op==ret) {
                    expected.stack_pointer=static_cast<std::uint8_t>((value+2)%256);
                    expected.program_counter=0;
                } else if (test.op==compare) {
                    const int difference=static_cast<int>(value)-120;
                    expected.status=static_cast<std::uint8_t>((p&0x7C)|(difference>=0 ? 1:0)
                        |(difference==0 ? 2:0)|((difference<0 || difference>=128) ? 128:0));
                } else if (test.op==clear) { byte=0; }
                else if (test.op==store_x) { byte=255-value; }
                require(result->output_registers==expected, "Ranked VDC registers and flags");
                require(result->return_instruction==RoutineKey{expected.mpr[expected.program_counter>>13],expected.program_counter}, "Ranked VDC return mapping");
                require(result->body_master_cycles==cycles && result->checkpoints.size()==1, "Ranked VDC cycles");
                const auto& cp=result->checkpoints[0];
                require(cp.registers==expected && cp.instruction==entry.routine
                    && cp.interrupt_sample_master_cycle_offset==sample && cp.master_cycle_offset==cycles
                    && !cp.samples_interrupt_flag_clear, "Ranked VDC sample before bus wait");
                const bool writes=test.op==port || test.op==decrement || test.op==clear || test.op==store || test.op==store_x;
                require(result->work_ram_writes.size()==(writes ? 1U:0U), "Ranked VDC write count");
                if (writes) {
                    const auto& w=result->work_ram_writes[0];
                    require(w.instruction==entry.routine && w.logical_address==test.operand
                        && w.value==byte && w.master_cycle_offset==cycles, "Ranked VDC timed write");
                }
            }
        }
        r.status=0xDF;
        for (unsigned horizon=0; horizon<23; ++horizon) {
            entry.master_cycles_until_user_event=horizon; entry.master_cycles_until_timer_interrupt=100;
            require(replace_ranked_vdc_instruction(entry,ram,rom).has_value(), "Ranked VDC user horizon");
            entry.master_cycles_until_user_event=100; entry.master_cycles_until_timer_interrupt=horizon;
            require(replace_ranked_vdc_instruction(entry,ram,rom).has_value(), "Ranked VDC timer horizon");
        }
        if (test.op==port) {
            r.mpr[0]=0xF8; require(!replace_ranked_vdc_instruction(entry,ram,rom), "Ranked VDC device mapping"); r.mpr[0]=0xFF;
        }
        require(!replace_ranked_vdc_instruction(entry,std::span(ram).first(8191),rom), "Ranked VDC RAM size");
        r.mpr[1]=0; require(!replace_ranked_vdc_instruction(entry,ram,rom), "Ranked VDC RAM mapping"); r.mpr[1]=0xF8;
        r.mpr[test.pc>>13]=0; require(!replace_ranked_vdc_instruction(entry,ram,rom), "Ranked VDC code mapping");
        r.mpr[test.pc>>13]=static_cast<std::uint8_t>(test.bank);
        ++r.program_counter; require(!replace_ranked_vdc_instruction(entry,ram,rom), "Ranked VDC PC guard");
    }
    // Every mapped bank, every Y, page crossing, and 16-bit address wrapping.
    for (const auto pc : {0x62E0U,0x62E6U}) {
        RoutineEntry entry{}; entry.routine={9,static_cast<std::uint16_t>(pc)};
        auto& r=entry.registers; r.program_counter=static_cast<std::uint16_t>(pc); r.status=0;
        for (const unsigned pointer : {0x9FF0U,0xFFF0U}) {
            ram[0x2F]=static_cast<std::byte>(pointer&255); ram[0x30]=static_cast<std::byte>(pointer>>8);
            for (unsigned bank=0; bank<256; ++bank) {
                for (unsigned y=0; y<256; ++y) {
                    r.mpr.fill(static_cast<std::uint8_t>(bank)); r.mpr[1]=0xF8; r.mpr[3]=9; r.y=static_cast<std::uint8_t>(y);
                    const unsigned address=(pointer+y)%65536;
                    const bool safe=bank<128 || bank==248;

                    const auto result=replace_ranked_vdc_instruction(entry,ram,rom);
                    require(result.has_value()==safe, "Ranked VDC indirect mapping guard");
                    if (result) {
                        const unsigned physical=bank<64 ? bank%32 : bank%16+32;
                        const unsigned byte=bank==248 ? std::to_integer<unsigned>(ram[address%8192]) : std::to_integer<unsigned>(rom[physical*8192+address%8192]);
                        require(result->output_registers.accumulator==byte, "Ranked VDC ROM mirror and pointer wrap");
                    }
                }
            }
        }
    }
    std::puts("Verified 786,432 ranked VDC status cases and 262,144 indirect mapping cases.");
}
