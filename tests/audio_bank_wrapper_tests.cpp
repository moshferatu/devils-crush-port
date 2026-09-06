// SPDX-License-Identifier: GPL-3.0-or-later

// Audio bank-wrapper assembly/native replay with live mapper/stack evidence.
#include "../src/app/native_replacements_internal.hpp"
#include "devils_crush/core/rom_image.hpp"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace devils_crush::emulation;
using devils_crush::app::detail::replace_bank_call_wrappers;
constexpr std::array<std::uint16_t, 2> pcs{0xED58, 0xED75};
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
    RoutineEntry entry{};
    entry.routine = {0U, 0xED58U};
    entry.registers.program_counter = 0xED58U;
    entry.registers.mpr = {0xFFU, 0xF8U, 8U, 9U, 0x1EU, 0x1FU, 1U, 0U};
    for (const auto pc : pcs) {
        entry.routine.logical_pc = pc;
        entry.registers.program_counter = pc;
        for (const auto flag : {8U, 32U, 40U}) {
            entry.registers.status = static_cast<std::uint8_t>(flag);
            require(!replace_bank_call_wrappers(entry, ram, {}), "D/T admitted");
        }
        entry.registers.status = 0U;
        for (const auto slot : {1U, 7U}) {
            const auto old = entry.registers.mpr[slot];
            entry.registers.mpr[slot] = 0xFEU;
            require(!replace_bank_call_wrappers(entry, ram, {}), "Wrong mapper admitted");
            entry.registers.mpr[slot] = old;
        }
        require(!replace_bank_call_wrappers(entry, std::span(ram).first(0x1FFF), {}),
            "Short RAM admitted");
    }
    unsigned cases = 0U;
    for (unsigned sp = 0U; sp < 256U; ++sp) {
        for (unsigned bank = 0U; bank < 256U; ++bank) {
            ram.fill(std::byte{0});
            entry.routine.logical_pc = 0xED58U;
            entry.registers.program_counter = 0xED58U;
            entry.registers.stack_pointer = static_cast<std::uint8_t>(sp);
            entry.registers.x = static_cast<std::uint8_t>(sp ^ bank);
            entry.registers.y = 0xA5U;
            entry.registers.status = 0x45U; // C/I/V must survive all loads/maps.
            ram[0x30] = static_cast<std::byte>(bank);
            ram[0x1E] = std::byte{0x81};
            ram[0x1F] = std::byte{0x00};
            ram[0x20] = std::byte{0xF8};
            const auto before = ram;
            const auto head = replace_bank_call_wrappers(entry, ram, {});
            require(head && head->return_instruction.logical_pc == 0xE262U,
                "Missing driver handoff");
            require(head->body_master_cycles == 198U && head->checkpoints.size() == 17U,
                "Wrong prologue timing");
            require(head->work_ram_writes.size() == 9U, "Wrong prologue writes");
            const std::array<unsigned, 4> values{entry.registers.x, 0x81U, 0U, 0xF8U};
            for (std::size_t i = 0; i < 4U; ++i) {
                const auto& w = head->work_ram_writes[i];
                require(w.logical_address == 0x2100U + ((sp - i) & 255U)
                    && w.value == values[i], "Wrong caller frame");
            }
            for (const auto& w : head->work_ram_writes) {
                ram[w.logical_address - 0x2000U] = static_cast<std::byte>(w.value);
            }
            require(head->work_ram_writes[7].value == 0xEDU
                && head->work_ram_writes[8].value == 0x74U
                && head->work_ram_writes[7].master_cycle_offset == 177U
                && head->work_ram_writes[8].master_cycle_offset == 177U,
                "Wrong JSR stack/timestamp");
            for (unsigned i = 0U; i < 3U; ++i) {
                require(head->output_registers.mpr[2U + i] == ((bank + i) & 255U)
                    && std::to_integer<unsigned>(ram[0x1EU + i]) == ((bank + i) & 255U),
                    "Mapper/shadow mismatch");
            }
            // The real child's RTS consumes only its two return bytes.
            auto tail_entry = entry;
            tail_entry.routine.logical_pc = 0xED75U;
            tail_entry.registers = head->output_registers;
            tail_entry.registers.program_counter = 0xED75U;
            tail_entry.registers.stack_pointer = static_cast<std::uint8_t>(sp - 4U);
            tail_entry.registers.x = 0xCCU; // child is allowed to clobber X
            const auto tail = replace_bank_call_wrappers(tail_entry, ram, {});
            require(tail && tail->return_instruction.logical_pc == 0xED85U
                && tail->body_master_cycles == 129U && tail->checkpoints.size() == 10U,
                "Wrong restoration timing");
            require(tail->output_registers.stack_pointer == sp
                && tail->output_registers.x == entry.registers.x
                && tail->output_registers.y == entry.registers.y
                && tail->output_registers.accumulator == 0x81U, "Caller state not restored");
            require(tail->output_registers.mpr[2] == 0x81U
                && tail->output_registers.mpr[3] == 0U
                && tail->output_registers.mpr[4] == 0xF8U, "Saved mappings not restored");
            const auto x = entry.registers.x;
            require(tail->output_registers.status == (0x45U | (x & 128U) | (x == 0 ? 2U : 0U)),
                "PLX flags differ");
            require(tail->work_ram_writes.size() == 3U, "Unexpected epilogue writes");
            for (const auto* result : {&*head, &*tail}) {
                for (const auto& c : result->checkpoints) {
                    require(c.interrupt_sample_master_cycle_offset + 3U == c.master_cycle_offset,
                        "Wrong interrupt sample");
                }
            }
            require(before[0x30] == ram[0x30], "Command bank changed");
            ++cases;
        }
    }
    std::printf("Verified %u stack/bank combinations.\n", cases);
}
Evidence replay(const char* rom, bool native, bool composed)
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
    for (const auto pc : {0xED85U}) {
        require(runtime->install_execution_hook({
            .routine = {0x00U, static_cast<std::uint16_t>(pc)},
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
                .routine = {0x00U, pcs[i]}, .mode = HookMode::replace,
                .replacement_callback = [&, i](const RoutineEntry& e, std::span<const std::byte> ram) {
                    ++evidence.entered[i];
                    auto result = replace_bank_call_wrappers(e, ram, {});
                    if (result) { ++evidence.replaced[i]; }
                    return result;
                },
            }), "Replacement hook failed");
        }
    }
    if (native && composed) {
        for (const auto pc : {0xE241U, 0xE255U, 0xE262U, 0xECC6U, 0xECE2U, 0xECF2U}) {
            require(runtime->install_execution_hook({
                .routine = {0U, static_cast<std::uint16_t>(pc)}, .mode = HookMode::replace,
                .replacement_callback = [](const RoutineEntry& e, std::span<const std::byte> ram) {
                    return replace_bank_call_wrappers(e, ram, {});
                },
            }), "Composed wrapper hook failed");
        }
    }
    for (unsigned frame = 0; frame < 800U; ++frame) {
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
        require(argc == 2, "Usage: dc_audio_bank_wrapper_tests <ROM>");
        verify_guards();
        const auto assembly = replay(argv[1], false, false);
        std::string aggregate;
        for (const auto& hash : assembly.frames) { aggregate += hash; }
        const auto digest = devils_crush::rom::compute_digests(
            std::as_bytes(std::span(aggregate.data(), aggregate.size()))).sha256;
        std::printf("Assembly evidence SHA-256: %s\n", digest.c_str());
        for (const bool composed : {false, true}) {
            const auto native = replay(argv[1], true, composed);
            for (std::size_t f = 0; f < assembly.frames.size(); ++f) {
                if (assembly.frames[f] != native.frames[f]) {
                    std::printf("Mismatch: composed=%d frame=%zu assembly=%s native=%s\n",
                        composed, f, assembly.frames[f].c_str(), native.frames[f].c_str());
                    return 1;
                }
            }
            for (std::size_t i = 0; i < pcs.size(); ++i) {
                std::printf("composed=%d Native replacement 00:%04X@all entries: %u of %u\n",
                    composed, pcs[i], native.replaced[i], native.entered[i]);
                require(native.entered[i] != 0U && native.entered[i] == native.replaced[i],
                    "Incomplete wrapper admission");
            }
        }
        std::puts("Both modes preserve every frame/audio/RAM/video, ordered writes and restored mapper boundary.");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
