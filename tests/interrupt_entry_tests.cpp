// SPDX-License-Identifier: GPL-3.0-or-later
#include "devils_crush/cpu/huc6280_interrupt_entry.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>

namespace {
void require(bool value) { if (!value) throw std::runtime_error("IRQ entry trace mismatch"); }
void exercise(std::uint8_t initial_s, std::uint8_t initial_p,
              std::uint32_t initial_pc, std::uint32_t vector, unsigned scale)
{
    auto pc = initial_pc;
    auto stack = initial_s;
    auto status = initial_p;
    unsigned cycles = 0, reads = 0, writes = 0, samples = 0, publishes = 0;
    unsigned timestamp = 0;
    const std::array<std::uint32_t, 4> addresses{initial_pc, initial_pc + 1, vector, vector + 1};
    const std::array<unsigned, 4> read_cycles{1, 2, 6, 7};
    devils_crush::cpu::enter_huc6280_interrupt(pc, stack, status, vector,
        [&] {
            ++cycles;
            timestamp += scale;
            require(cycles <= 8);
            if (cycles <= 5) require(status == initial_p);
            else require(status == ((initial_p | 4u) & ~0x28u));
            if (cycles <= 6) require(pc == initial_pc);
            if (cycles == 7) require(pc == 0x34);
            if (cycles == 8) require(pc == 0x9234 && samples == 1);
        },
        [&](std::uint32_t address) -> std::uint8_t {
            require(reads < 4 && address == addresses[reads] && cycles == read_cycles[reads]);
            ++reads;
            // Inject a device stall after every read: the sequence must still
            // issue eight single-cycle advances and sample after the last read.
            timestamp += 5;
            return reads == 3 ? 0x34 : 0x92;
        },
        [&](std::uint32_t address, std::uint8_t value) {
            require(writes < 3 && cycles == writes + 3);
            require(stack == static_cast<std::uint8_t>(initial_s - writes));
            require(address == (0x2100u | stack));
            const auto expected = writes == 0 ? initial_pc >> 8
                : writes == 1 ? initial_pc : initial_p & ~0x10u;
            require(value == static_cast<std::uint8_t>(expected));
            require(status == initial_p && pc == initial_pc);
            ++writes;
            timestamp += 7;
        },
        [&] {
            require(cycles == 5 && writes == 3 && status == (initial_p | 4u));
            ++publishes;
        },
        [&] {
            require(cycles == 7 && reads == 4 && writes == 3 && publishes == 1);
            require(pc == 0x9234 && status == ((initial_p | 4u) & ~0x28u));
            require(timestamp == 7 * scale + 41);
            ++samples;
        });
    require(cycles == 8 && reads == 4 && writes == 3 && samples == 1 && publishes == 1);
    require(stack == static_cast<std::uint8_t>(initial_s - 3));
    require(timestamp == 8 * scale + 41);
}
}
int main()
{
    try {
        for (unsigned s = 0; s < 256; ++s)
            for (unsigned p = 0; p < 256; ++p)
                for (auto vector : {0xfff6u, 0xfff8u, 0xfffau})
                    for (auto pc : {0x0000u, 0xffffu})
                        for (auto scale : {3u, 12u})
                            exercise(static_cast<std::uint8_t>(s), static_cast<std::uint8_t>(p), pc, vector, scale);
        std::puts("786432 IRQ entries preserve bus order, live registers, stack wrap, flags and final sampling.");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
