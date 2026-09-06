// SPDX-License-Identifier: GPL-3.0-or-later

// Real joypad-driven assembly/native replay. No input-memory or ROM patching:
// only the wide-glyph case changes the selected actor's layout at a frame seam.
#include "../src/app/native_replacements_internal.hpp"
#include "devils_crush/core/rom_image.hpp"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace devils_crush::emulation;
using devils_crush::app::detail::replace_selection_actor_controller;
constexpr std::array<std::uint16_t, 4> pcs{0x5A3E, 0x5A6A, 0x5A7A, 0x5A86};
void require(bool value, const char* text)
{
    if (!value) { throw std::runtime_error(text); }
}
struct Evidence {
    std::vector<std::string> frames;
    std::array<unsigned, 4> entered{};
    std::array<unsigned, 4> replaced{};
    std::array<unsigned, 5> paths{}; // neutral, increment, wrap, decrement, wrap
};
void verify_guards()
{
    std::array<std::byte, 0x2000> ram{};
    RoutineEntry entry{};
    entry.routine = {0x08U, 0x5A3EU};
    entry.registers.program_counter = 0x5A3EU;
    entry.registers.mpr = {0xFFU, 0xF8U, 0x08U, 9U, 0x1EU, 0x1FU, 1U, 0U};
    for (const auto pc : pcs) {
        entry.routine.logical_pc = pc;
        entry.registers.program_counter = pc;
        require(replace_selection_actor_controller(entry, ram).has_value(), "Valid guard declined");
        for (const auto flag : {0x08U, 0x20U, 0x28U}) {
            entry.registers.status = static_cast<std::uint8_t>(flag);
            require(!replace_selection_actor_controller(entry, ram), "D/T must decline");
        }
        entry.registers.status = 0U;
        for (const auto index : {1U, 2U}) {
            const auto old = entry.registers.mpr[index];
            entry.registers.mpr[index] = 0U;
            require(!replace_selection_actor_controller(entry, ram), "Wrong mapping admitted");
            entry.registers.mpr[index] = old;
        }
        require(!replace_selection_actor_controller(entry, std::span(ram).first(0x1FFF)),
            "Short RAM admitted");
    }
    // The JSR stack frame must wrap like the CPU stack, including SP=$00/$01.
    entry.routine.logical_pc = 0x5A3EU;
    entry.registers.program_counter = 0x5A3EU;
    ram[0x18] = std::byte{0x44};
    ram[0x1E00] = std::byte{3};
    for (unsigned sp = 0; sp < 256U; ++sp) {
        entry.registers.stack_pointer = static_cast<std::uint8_t>(sp);
        const auto result = replace_selection_actor_controller(entry, ram);
        require(result && result->return_instruction.logical_pc == 0x5A6AU,
            "Active input must hand off at glyph call");
        const auto n = result->work_ram_writes.size();
        require(n >= 2U, "Missing JSR frame");
        const auto& high = result->work_ram_writes[n - 2U];
        const auto& low = result->work_ram_writes[n - 1U];
        require(high.logical_address == 0x2100U + sp && high.value == 0x5AU,
            "Wrong JSR high byte");
        require(low.logical_address == 0x2100U + ((sp - 1U) & 255U) && low.value == 0x68U,
            "Wrong JSR low byte");
        require(high.master_cycle_offset == low.master_cycle_offset
            && high.master_cycle_offset + 21U == result->body_master_cycles,
            "Wrong JSR write timing");
        require(result->output_registers.stack_pointer == ((sp - 2U) & 255U),
            "Wrong JSR stack wrap");
    }
    // Every input byte, edge-valued choices/limits, and the full indexed RAM
    // range. In particular, DEC's BPL is signed and a zero limit is not one.
    unsigned cases = 0;
    for (const auto x : {0U, 0x10U, 0x7FU, 0xFFU}) {
        entry.registers.x = static_cast<std::uint8_t>(x);
        for (const auto limit : {0U, 1U, 2U, 0x7FU, 0x80U, 0xFFU}) {
            ram[0x1E00U + x] = static_cast<std::byte>(limit);
            for (const auto choice : {0U, 1U, 2U, 0x7FU, 0x80U, 0xFEU, 0xFFU}) {
                ram[0x5D] = static_cast<std::byte>(choice);
                for (unsigned input = 0; input < 256U; ++input) {
                    ram[0x18] = static_cast<std::byte>(input);
                    const auto result = replace_selection_actor_controller(entry, ram);
                    require(result.has_value(), "Input combination declined");
                    auto expected = choice;
                    if ((input & 0x44U) != 0U) {
                        expected = (choice + 1U) & 255U;
                        if (expected >= limit) { expected = 0U; }
                    } else if ((input & 0x10U) != 0U) {
                        expected = (choice - 1U) & 255U;
                        if ((expected & 128U) != 0U) { expected = (limit - 1U) & 255U; }
                    }
                    auto actual = choice;
                    for (const auto& w : result->work_ram_writes) {
                        if (w.logical_address == 0x205DU) { actual = w.value; }
                    }
                    require(actual == expected, "Selection wrap changed");
                    require(result->work_ram_writes.front().logical_address == 0x3A80U + x
                        && result->work_ram_writes.front().value == choice, "Old choice not saved");
                    require(result->return_instruction.logical_pc == ((input & 0x54U) ? 0x5A6AU : 0x5A69U),
                        "Wrong selection handoff");
                    require(ram[0x5D] == static_cast<std::byte>(choice), "Input snapshot mutated");
                    for (const auto& checkpoint : result->checkpoints) {
                        require(checkpoint.master_cycle_offset == checkpoint.interrupt_sample_master_cycle_offset + 3U,
                            "Wrong interrupt sample boundary");
                    }
                    ++cases;
                }
            }
        }
    }
    std::printf("Verified %u input/choice/limit/slot combinations and 256 stack positions.\n", cases);
}
Evidence replay(const char* rom, bool native, bool wide)
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
        if (w.logical_address > 3U) { return; }
        append(w.instruction.physical_bank); append(w.instruction.logical_pc);
        append(w.logical_address); append(w.value); append(w.absolute_master_cycle);
    }), "VDC observer failed");
    // Only outer child/return boundaries are comparable: a native segment
    // need not re-enter hooks for instructions inside its own transcript.
    for (const auto pc : {0x5A69U, 0x5B01U}) {
        require(runtime->install_execution_hook({
            .routine = {0x08U, static_cast<std::uint16_t>(pc)},
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
                .routine = {0x08U, pcs[i]}, .mode = HookMode::replace,
                .replacement_callback = [&, i](const RoutineEntry& e, std::span<const std::byte> ram) {
                    ++evidence.entered[i];
                    if (i == 0U) {
                        const auto input = std::to_integer<unsigned>(ram[0x18]);
                        const auto choice = std::to_integer<unsigned>(ram[0x5D]);
                        const auto limit = std::to_integer<unsigned>(ram[0x1E00U + e.registers.x]);
                        const auto next = (choice + 1U) & 255U;
                        const auto path = (input & 0x44U) ? (next >= limit ? 2U : 1U)
                            : (input & 0x10U) ? (choice == 0U ? 4U : 3U) : 0U;
                        ++evidence.paths[path];
                    }
                    auto result = replace_selection_actor_controller(e, ram);
                    if (result) { ++evidence.replaced[i]; }
                    return result;
                },
            }), "Replacement hook failed");
        }
    }
    for (unsigned frame = 0; frame < 800U; ++frame) {
        if (wide && frame == 310U) {
            auto ram = runtime->export_work_ram();
            ram[0x1E90] = std::byte{1}; // first selection actor X=$10
            ram[0x1310] = std::byte{0x7F}; // row carry during glyph address arithmetic
            ram[0x1190] = std::byte{0xF8}; // column carry
            require(runtime->import_work_ram(ram), "Layout setup failed");
        }
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
        require(argc == 2, "Usage: dc_selection_actor_controller_tests <ROM>");
        verify_guards();
        for (const bool wide : {false, true}) {
            const auto assembly = replay(argv[1], false, wide);
            const auto native = replay(argv[1], true, wide);
            std::string aggregate;
            for (const auto& hash : assembly.frames) { aggregate += hash; }
            const auto digest = devils_crush::rom::compute_digests(
                std::as_bytes(std::span(aggregate.data(), aggregate.size()))).sha256;
            std::printf("wide=%d assembly evidence SHA-256: %s\n", wide, digest.c_str());
            for (std::size_t f = 0; f < assembly.frames.size(); ++f) {
                if (assembly.frames[f] != native.frames[f]) {
                    std::printf("Mismatch: wide=%d frame=%zu assembly=%s native=%s\n",
                        wide, f, assembly.frames[f].c_str(), native.frames[f].c_str());
                    return 1;
                }
            }
            for (std::size_t i = 0; i < pcs.size(); ++i) {
                std::printf("wide=%d Native replacement 08:%04X@all entries: %u of %u\n",
                    wide, pcs[i], native.replaced[i], native.entered[i]);
                require(native.entered[i] != 0U && native.entered[i] == native.replaced[i],
                    "Incomplete controller admission");
            }
            for (std::size_t i = 0; i < native.paths.size(); ++i) {
                std::printf("path=%zu count=%u\n", i, native.paths[i]);
                require(native.paths[i] > 0U, "Missing input branch");
            }
        }
        std::puts("Both 800-frame joypad routes preserve frame/audio/RAM/video, ordered RAM/VDC writes and child boundaries.");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
