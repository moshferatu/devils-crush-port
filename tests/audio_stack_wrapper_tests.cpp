// SPDX-License-Identifier: GPL-3.0-or-later

// Audio stack-wrapper assembly/native replay with live caller-frame evidence.
#include "../src/app/native_replacements_internal.hpp"
#include "devils_crush/core/rom_image.hpp"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace devils_crush::emulation;
using devils_crush::app::detail::replace_audio_stack_wrapper;
constexpr std::array<std::uint16_t, 2> pcs{0x4B7B, 0x4B82};
void require(bool value, const char* text)
{
    if (!value) { throw std::runtime_error(text); }
}
struct Evidence {
    std::vector<std::string> frames;
    std::array<unsigned, 2> entered{};
    std::array<unsigned, 2> replaced{};
};
void verify_guards()
{
    std::array<std::byte, 0x2000> ram{};
    RoutineEntry e{};
    e.registers.mpr = {0xFF, 0xF8, 0x45, 9, 0x1E, 0x1F, 1, 0};
    unsigned cases = 0;
    for (unsigned sp = 0; sp < 256; ++sp) {
        for (unsigned p = 0; p < 256; ++p) {
            e.routine = {0x45, 0x4B7B}; e.registers.program_counter = 0x4B7B;
            e.registers.stack_pointer = static_cast<std::uint8_t>(sp);
            e.registers.status = static_cast<std::uint8_t>(p);
            e.registers.x = static_cast<std::uint8_t>(sp ^ p);
            e.registers.y = static_cast<std::uint8_t>(p + 1);
            const auto head = replace_audio_stack_wrapper(e, ram);
            if ((p & 0x28U) != 0U) {
                require(!head, "D/T admitted"); continue;
            }
            require(head && head->body_master_cycles == 54 && head->checkpoints.size() == 5
                && head->work_ram_writes.size() == 5
                && head->return_instruction.logical_pc == 0x4BCC, "Head shape");
            const std::array<unsigned, 5> values{p | 0x10U, e.registers.x, e.registers.y, 0x4B, 0x81};
            const std::array<unsigned, 5> times{9, 18, 27, 33, 33};
            for (unsigned i = 0; i < 5; ++i) {
                const auto& w = head->work_ram_writes[i];
                require(w.logical_address == 0x2100U + ((sp - i) & 255U)
                    && w.value == values[i] && w.master_cycle_offset == times[i], "Stack write");
                ram[w.logical_address - 0x2000U] = static_cast<std::byte>(w.value);
            }
            require(head->checkpoints[3].samples_interrupt_flag_clear == ((p & 4U) == 0),
                "SEI sample");
            auto tail_entry = e;
            tail_entry.routine.logical_pc = 0x4B82;
            tail_entry.registers = head->output_registers;
            tail_entry.registers.program_counter = 0x4B82;
            tail_entry.registers.stack_pointer = static_cast<std::uint8_t>(sp - 3U);
            const auto tail = replace_audio_stack_wrapper(tail_entry, ram);
            require(tail && tail->body_master_cycles == 24 && tail->checkpoints.size() == 2
                && tail->work_ram_writes.empty() && tail->return_instruction.logical_pc == 0x4B84,
                "Tail shape");
            require(tail->output_registers.x == e.registers.x && tail->output_registers.y == e.registers.y
                && tail->output_registers.stack_pointer == ((sp - 1U) & 255U), "Live frame restore");
            const auto x = e.registers.x;
            require(tail->output_registers.status == (((p | 4U) & ~0x82U)
                | (x == 0U ? 2U : x & 0x80U)), "PLX flags");
            for (const auto* result : {&*head, &*tail}) {
                for (const auto& c : result->checkpoints) {
                    require(c.interrupt_sample_master_cycle_offset + 3U == c.master_cycle_offset,
                        "Checkpoint sampling");
                }
            }
            ++cases;
        }
    }
    for (auto pc : pcs) {
        e.routine = {0x45, pc}; e.registers.status = 0;
        require(!replace_audio_stack_wrapper(e, std::span(ram).first(0x1FFF)), "Short RAM");
        for (auto slot : {1U, 2U}) {
            const auto old = e.registers.mpr[slot]; e.registers.mpr[slot] = 0xFE;
            require(!replace_audio_stack_wrapper(e, ram), "Wrong mapper");
            e.registers.mpr[slot] = old;
        }
    }
    std::printf("Verified %u stack/status combinations.\n", cases);
}
Evidence replay(const char* rom, bool native)
{
    Evidence evidence;
    auto runtime = create_pce_runtime();
    require(runtime->load_rom(rom).loaded, "ROM load failed");
    std::vector<std::byte> bytes;
    const auto append = [&bytes](const auto& value) {
        const auto data = std::as_bytes(std::span(&value, 1));
        bytes.insert(bytes.end(), data.begin(), data.end());
    };
    const auto append_span = [&bytes](const auto data) {
        const auto raw = std::as_bytes(data);
        bytes.insert(bytes.end(), raw.begin(), raw.end());
    };
    require(runtime->set_work_ram_write_observer([&](const ObservedMemoryWrite& w) {
        append(w.instruction.physical_bank); append(w.instruction.logical_pc);
        append(w.logical_address); append(w.value); append(w.absolute_master_cycle);
    }), "RAM observer failed");
    require(runtime->set_bus_write_observer([&](const ObservedBusWrite& w) {
        if (w.logical_address > 0x1FFFU) { return; }
        append(w.instruction.physical_bank); append(w.instruction.logical_pc);
        append(w.logical_address); append(w.value); append(w.absolute_master_cycle);
    }), "VDC observer failed");
    // Only outer child/return boundaries are comparable: a native segment
    // need not re-enter hooks for instructions inside its own transcript.
    for (const auto pc : {0x4BCCU, 0x4B85U}) {
        require(runtime->install_execution_hook({
            .routine = {0x45U, static_cast<std::uint16_t>(pc)},
            .mode = HookMode::observe,
            .callback = [&](const RoutineEntry& e) {
                append(e.routine.logical_pc); append(e.absolute_master_cycle);
                const auto& r = e.registers;
                append(r.accumulator); append(r.x); append(r.y);
                append(r.stack_pointer); append(r.status); append_span(std::span(r.mpr));
            },
        }), "Boundary hook failed");
    }
    if (native) {
        for (std::size_t i = 0; i < pcs.size(); ++i) {
            require(runtime->install_execution_hook({
                .routine = {0x45U, pcs[i]}, .mode = HookMode::replace,
                .replacement_callback = [&, i](const RoutineEntry& e, std::span<const std::byte> ram) {
                    ++evidence.entered[i];
                    auto result = replace_audio_stack_wrapper(e, ram);
                    if (result) { ++evidence.replaced[i]; }
                    return result;
                },
            }), "Replacement hook failed");
        }
    }
    for (unsigned frame = 0; frame < 1800U; ++frame) {
        InputFrame input{};
        if (frame == 180U || frame == 181U || frame == 300U || frame == 301U) {
            input.player_one = static_cast<std::uint16_t>(Button::run);
        }
        // Exercise both direction wraps repeatedly before confirming the page.
        if (frame >= 320U && frame < 680U && (frame % 8U) == 0U) {
            input.player_one = static_cast<std::uint16_t>(
                (frame % 64U) < 32U ? Button::up : Button::down);
        }
        if (frame == 700U || frame == 701U) {
            input.player_one = static_cast<std::uint16_t>(Button::one);
        }
        bytes.clear();
        const auto output = runtime->run_frame(input);
        require(!output.framebuffer.pixels.empty(), "Missing framebuffer");
        append(output.framebuffer.width); append(output.framebuffer.height);
        append(output.framebuffer.pitch_pixels); append_span(output.framebuffer.pixels);
        append(output.audio.sample_rate); append_span(output.audio.interleaved_stereo);
        const auto ram = runtime->export_work_ram();
        append_span(std::span(ram));
        const auto video = runtime->export_video_memory();
        require(video.has_value(), "Missing video memory");
        append_span(std::span(video->vram_words)); append_span(std::span(video->sat_words));
        append_span(std::span(video->palette_entries));
        append(video->memory_width_register); append(video->background_x_scroll);
        append(video->background_y_scroll);
        evidence.frames.push_back(devils_crush::rom::compute_digests(bytes).sha256);
    }
    return evidence;
}
} // namespace

int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Usage: dc_audio_stack_wrapper_tests <ROM>");
        verify_guards();
        const auto assembly = replay(argv[1], false);
        std::string aggregate;
        for (const auto& hash : assembly.frames) { aggregate += hash; }
        const auto digest = devils_crush::rom::compute_digests(
            std::as_bytes(std::span(aggregate.data(), aggregate.size()))).sha256;
        std::printf("Assembly evidence SHA-256: %s\n", digest.c_str());
        const auto native = replay(argv[1], true);
        for (std::size_t f = 0; f < assembly.frames.size(); ++f) {
            if (assembly.frames[f] != native.frames[f]) {
                std::printf("Mismatch: frame=%zu assembly=%s native=%s\n",
                    f, assembly.frames[f].c_str(), native.frames[f].c_str());
                return 1;
            }
        }
        for (std::size_t i = 0; i < pcs.size(); ++i) {
            std::printf("Native replacement 45:%04X@all entries: %u of %u\n",
                pcs[i], native.replaced[i], native.entered[i]);
            require(native.entered[i] != 0U && native.entered[i] == native.replaced[i],
                "Incomplete wrapper admission");
        }
        std::puts("Replay preserves every frame/audio/RAM/video, ordered writes and child/return boundaries.");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
