// SPDX-License-Identifier: GPL-3.0-or-later
#include "devils_crush/emulation/pce_runtime.hpp"
#include "devils_crush/cabinet/music_transition.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>

int main(int argc, char** argv)
{
    using namespace devils_crush::emulation;
    if (argc < 2) return 1;
    {
        devils_crush::cabinet::MusicTransition transition;
        std::vector<std::byte> ram(8192U);
        RoutineEntry entry;
        entry.routine = {3U, 0x7CF6U};
        ram[0xCCU] = std::byte{1};
        if (transition.finish_star(entry, ram)) return 1; // ordinary star
        transition.table_return(ram);
        const auto replacement = transition.finish_star(entry, ram);
        if (!replacement || replacement->output_registers.program_counter != 0x7CF9U
            || !replacement->work_ram_writes.empty()) return 1;
        if (transition.finish_star(entry, ram)) return 1; // one-shot
        transition.table_return(ram);
        ram[0x55U] = std::byte{6};
        if (transition.finish_star(entry, ram)) return 1; // another stage
        RoutineEntry jingle;
        jingle.routine = {3U, 0x7CC1U};
        ram[0x55U] = std::byte{0};
        transition.perfect_result();
        if (transition.star_jingle(jingle, ram)) return 1; // setting off
        devils_crush::cabinet::MusicTransition skipping(true);
        if (skipping.star_jingle(jingle, ram)) return 1; // ordinary star
        skipping.perfect_result();
        const auto skipped = skipping.star_jingle(jingle, ram);
        if (!skipped || skipped->output_registers.program_counter != 0x7CC4U
            || skipped->return_instruction != RoutineKey{3U, 0x7CC4U}) return 1;
        if (skipping.star_jingle(jingle, ram)) return 1; // one-shot
        jingle.routine = {5U, 0xA984U};
        skipping.perfect_result();
        const auto other = skipping.star_jingle(jingle, ram);
        if (!other || other->output_registers.program_counter != 0xA987U) return 1;
    }
    std::ifstream file(argv[1], std::ios::binary);
    std::vector<std::byte> rom(393216U);
    file.read(reinterpret_cast<char*>(rom.data()), static_cast<std::streamsize>(rom.size()));
    if (!file) return 1;
    std::vector<std::byte> baseline;
    std::vector<ObservedBusWrite> writes;
    std::vector<float> game_over_audio;
    std::vector<float> bonus_six_audio;
    std::size_t bonus_six_sample = 0;
    unsigned bonus_six_differences = 0;
    std::size_t game_over_sample = 0;
    unsigned game_over_differences = 0;
    // Pass 0: original. Pass 1: overlay. Pass 2: transition, jingle kept.
    // Pass 3: transition, jingle skipped.
    for (unsigned pass = 0; pass < 4; ++pass) {
        auto runtime = create_pce_runtime();
        if (!runtime->set_native_psg(true)
            || (pass && !runtime->set_music_voice_overlay(true, rom))
            || !runtime->load_rom(argv[1]).loaded) return 1;
        std::size_t write_index = 0;
        bool mismatch = false;
        unsigned table_starts = 0U;
        unsigned jingles = 0U;
        unsigned early_star_frames = 0U;
        if (!runtime->set_bus_write_observer([&](const ObservedBusWrite& w) {
            if (w.physical_bank == 0xFFU && w.logical_address >= 0x800U
                && w.logical_address <= 0x809U) {
                if (pass == 0) writes.push_back(w);
                else if (pass == 1 && (write_index >= writes.size() || writes[write_index++] != w)) mismatch = true;
            }
            if (pass == 0 && w.instruction == RoutineKey{0x45U, 0x4BCCU} && w.value != 40U) {
                std::cout << "command frame=" << w.frame_number << " value="
                    << static_cast<unsigned>(w.value) << '\n';
            }
            if (w.instruction == RoutineKey{0x45U, 0x4BCCU}
                && w.frame_number > 2400U && w.frame_number < 3600U && w.value == 1U) ++table_starts;
            if (w.instruction == RoutineKey{0x45U, 0x4BCCU} && w.value == 8U) ++jingles;
        })) return 1;
        devils_crush::cabinet::MusicTransition transition(pass == 3);
        if (pass >= 2 && !transition.install(*runtime)) return 1;
        if (!runtime->install_execution_hook({
            .routine = {0U, 0xECFDU}, .mode = HookMode::observe,
            .callback = [&](const RoutineEntry& entry) {
                if (pass == 0 && entry.registers.accumulator == 1U) {
                    const auto ram = runtime->export_work_ram();
                    const auto sp = entry.registers.stack_pointer;
                    const auto lo = std::to_integer<unsigned>(ram[0x100U + static_cast<std::uint8_t>(sp + 1U)]);
                    const auto hi = std::to_integer<unsigned>(ram[0x100U + static_cast<std::uint8_t>(sp + 2U)]);
                    std::cout << "table command frame=" << entry.frame_number
                        << " caller=" << std::hex << ((hi << 8U) | lo) << std::dec << '\n';
                }
            },
        })) return 1;
        for (unsigned frame = 1; frame <= 6000U; ++frame) {
            if (frame == 3601U || frame == 4801U) {
                // Exercise a full bonus-6 music phrase, then the game-over
                // command, through the cartridge's real command queue.
                auto ram = runtime->export_work_ram();
                ram[0xE02U] = frame == 3601U ? std::byte{11} : std::byte{2};
                if (!runtime->import_work_ram(ram)) return 1;
            }
            if (frame == 1300U || frame == 1459U) {
                auto ram = runtime->export_work_ram();
                if (frame == 1300U) {
                    ram[0x55U] = std::byte{1}; ram[0x53U] = std::byte{0x10};
                } else {
                    ram[0xDB7U] = std::byte{1}; ram[0xDBAU] = std::byte{1};
                }
                if (!runtime->import_work_ram(ram)) return 1;
            }
            InputFrame controls;
            const auto pair = [frame](unsigned f) { return frame == f || frame == f + 1U; };
            const auto press = [&](Button b) { controls.player_one |= static_cast<std::uint16_t>(b); };
            if (pair(180) || pair(300)) press(Button::run);
            if (pair(420) || pair(700) || (frame >= 850U && frame <= 880U)
                || (frame >= 1000U && frame <= 1180U)) press(Button::one);
            if (frame >= 1250U && frame < 3600U) {
                const auto phase = (frame - 1250U) % 30U;
                if (phase <= 1U) press(Button::left);
                if (phase == 15U || phase == 16U) press(Button::one);
            }
            if (pair(1450) || pair(2250) || pair(3050)) press(Button::two);
            const auto output = runtime->run_frame(controls);
            if (!output.frame_number) return 1;
            if (frame >= 3620U && frame <= 4780U) {
                for (const auto sample : output.audio.interleaved_stereo) {
                    if (pass == 0) bonus_six_audio.push_back(sample);
                    if (pass == 1 && (bonus_six_sample >= bonus_six_audio.size()
                        || sample != bonus_six_audio[bonus_six_sample++])) ++bonus_six_differences;
                }
            }
            if (frame >= 4820U) {
                for (const auto sample : output.audio.interleaved_stereo) {
                    if (pass == 0) game_over_audio.push_back(sample);
                    if (pass == 1 && (game_over_sample >= game_over_audio.size()
                        || sample != game_over_audio[game_over_sample++])) ++game_over_differences;
                }
            }
            const auto ram = runtime->export_work_ram();
            if (pass == 0) baseline.insert(baseline.end(), ram.begin(), ram.end());
            else if (pass == 1 && !std::equal(ram.begin(), ram.end(),
                baseline.begin() + static_cast<std::ptrdiff_t>((frame - 1U) * 8192U))) return 1;
            if (pass >= 2) {
                for (const auto offset : {0x53U, 0x55U, 0xCCU, 0xD52U,
                    0x205U, 0x20CU, 0x213U, 0x1180U, 0x1200U, 0x1300U, 0x1380U}) {
                    if (ram[offset] != baseline[(frame - 1U) * 8192U + offset]) {
                        // Without the jingle's driver work the star's wait
                        // ends a few frames sooner; nothing else may move.
                        if (pass == 3 && offset == 0xCCU && ram[offset] == std::byte{0}
                            && ++early_star_frames <= 4U) continue;
                        std::cerr << "Transition changed gameplay at frame " << frame
                            << " (pass " << pass << ", $" << std::hex << offset << std::dec << ")\n";
                        return 1;
                    }
                }
            }
        }
        if (mismatch || writes.empty() || (pass == 1 && write_index != writes.size())) return 1;
        if (table_starts != (pass >= 2 ? 1U : 2U)) {
            std::cerr << "Unexpected table starts: " << table_starts << '\n'; return 1;
        }
        if (jingles != (pass == 3 ? 0U : 1U)) {
            std::cerr << "Unexpected perfect jingles: " << jingles << '\n'; return 1;
        }
    }
    std::cout << "Overlay preserves 6000 RAM frames and " << writes.size()
        << " original PSG writes. Game-over sample differences: " << game_over_differences
        << "; bonus-6 sample differences: " << bonus_six_differences << '\n';
    if (game_over_audio.empty() || game_over_sample != game_over_audio.size()
        || game_over_differences != 0U || bonus_six_audio.empty()
        || bonus_six_sample != bonus_six_audio.size() || bonus_six_differences != 0U) return 1;
}
