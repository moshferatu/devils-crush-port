// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/app/native_replacements_internal.hpp"
#include "devils_crush/core/rom_image.hpp"
#include "devils_crush/game_logic/controls.h"
#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace devils_crush::emulation;
using devils_crush::app::detail::replace_nudge_input_gate;
void require(bool value, const char* message) {
    if (!value) { throw std::runtime_error(message); }
}
void verify() {
    std::array<std::byte, 0x2000> ram{};
    RoutineEntry entry{};
    entry.routine = {1U, 0xC3CBU};
    entry.registers.program_counter = 0xC3CBU;
    entry.registers.mpr = {0xFFU, 0xF8U, 0U, 0U, 0U, 0U, 1U, 0U};
    unsigned locks = 0;
    for (unsigned flags = 0; flags < 256U; ++flags) {
        for (unsigned tilt = 0; tilt < 256U; ++tilt) {
            if ((flags & 2U) == 0U && tilt == 0U) { continue; }
            ram.fill(std::byte{0});
            ram[0x53] = static_cast<std::byte>(flags);
            ram[0xCC7] = static_cast<std::byte>(tilt);
            entry.registers.status = static_cast<std::uint8_t>(tilt);
            entry.registers.stack_pointer = static_cast<std::uint8_t>(flags + tilt);
            entry.registers.x = static_cast<std::uint8_t>(flags);
            entry.registers.y = static_cast<std::uint8_t>(tilt);
            const auto sp = entry.registers.stack_pointer;
            ram[0x100U + static_cast<std::uint8_t>(sp + 1U)] = std::byte{0xFF};
            ram[0x100U + static_cast<std::uint8_t>(sp + 2U)] = std::byte{0xC1};
            const auto before = ram;
            const auto result = replace_nudge_input_gate(entry, ram);
            require(result.has_value(), "Lock return declined");
            const auto a = (flags & 2U) != 0U ? 2U : tilt;
            const auto body = (flags & 2U) != 0U ? 30U : 51U;
            require(result->work_ram_writes.empty() && ram == before, "Lock changed RAM");
            require(result->body_master_cycles == body + 21U, "Lock cycle mismatch");
            const auto& out = result->output_registers;
            require(out.accumulator == a && out.status == ((tilt & ~0xA2U) | (a & 0x80U)),
                "Lock flags or branch priority mismatch");
            require(out.program_counter == 0xC200U
                && out.stack_pointer == static_cast<std::uint8_t>(sp + 2U)
                && out.x == entry.registers.x && out.y == entry.registers.y
                && out.mpr == entry.registers.mpr, "Lock live return mismatch");
            require(result->absorbed_returns.size() == 1U
                && result->absorbed_returns[0].master_cycle_offset == body,
                "Lock return declaration mismatch");
            require(result->checkpoints.size() == ((flags & 2U) ? 4U : 6U),
                "Lock instruction checkpoints missing");
            for (const auto& point : result->checkpoints) {
                require(point.interrupt_sample_master_cycle_offset + 3U == point.master_cycle_offset,
                    "Lock interrupt sample mismatch");
            }
            ++locks;
        }
    }
    unsigned active = 0;
    for (const auto phase : {0U, 1U}) {
        for (unsigned counter = 0; counter < 256U; ++counter) {
            ram.fill(std::byte{0});
            ram[0x18] = std::byte{2}; ram[0x1000] = std::byte{1};
            ram[0xCC4] = static_cast<std::byte>(phase);
            ram[0xCC5] = static_cast<std::byte>(counter);
            ram[0xCC6] = std::byte{1};
            ram[0xCC2] = std::byte{0xFF}; ram[0xCC3] = std::byte{0x80};
            ram[0x1500] = std::byte{0xFF}; ram[0x1580] = std::byte{0x7F};
            entry.registers.status = 0xD7U;
            entry.registers.stack_pointer = static_cast<std::uint8_t>(counter);
            const auto before = ram;
            const auto result = replace_nudge_input_gate(entry, ram);
            require(result.has_value() && ram == before, "Active reload declined or mutated RAM");
            require(result->body_master_cycles == (phase == 0U ? 792U : 552U), "Reload body cycles");
            const auto& writes = result->work_ram_writes;
            require(writes[0].value == 0U && writes[0].master_cycle_offset == 96U
                && writes[1].logical_address == 0x2CC6U && writes[1].value == 0x78U
                && writes[1].master_cycle_offset == 123U
                && writes[2].logical_address == 0x2CC5U && writes[2].value == 0U
                && writes[2].master_cycle_offset == 138U, "Reload ordered writes");
            if (phase == 0U) {
                require(writes[3].value == 1U && writes[3].master_cycle_offset == 252U,
                    "Active start used stale counter after reset");
            }
            ram[0xCC6] = std::byte{2}; ram[0xCC5] = std::byte{0};
            const auto ordinary = replace_nudge_input_gate(entry, ram);
            require(ordinary.has_value() && result->output_registers == ordinary->output_registers,
                "Reload changed active motion result");
            require(result->checkpoints.size() == ordinary->checkpoints.size() + 3U,
                "Reload checkpoint count");
            for (std::size_t i = 11U; i < ordinary->checkpoints.size(); ++i) {
                const auto& a = ordinary->checkpoints[i];
                const auto& b = result->checkpoints[i + 3U];
                require(a.instruction == b.instruction && a.registers == b.registers
                    && a.master_cycle_offset + 30U == b.master_cycle_offset
                    && a.interrupt_sample_master_cycle_offset + 30U == b.interrupt_sample_master_cycle_offset,
                    "Reload active continuation checkpoint mismatch");
            }
            for (std::size_t i = 1U; i < result->checkpoints.size(); ++i) {
                const auto& p = result->checkpoints[i];
                if (p.instruction.logical_pc == 0xC472U) {
                    require(p.registers.x == 0U
                        && p.registers.status == result->checkpoints[i - 1U].registers.status,
                        "CLX modified flags");
                }
                if (p.instruction.logical_pc == 0xC426U || p.instruction.logical_pc == 0xC44FU) {
                    require((p.registers.status & 4U) != 0U
                        && (result->checkpoints[i + 1U].registers.status & 4U) == 0U,
                        "CLI mask was not deferred to the following instruction");
                }
            }
            for (const auto status : {8U, 32U, 40U}) {
                entry.registers.status = static_cast<std::uint8_t>(status);
                require(!replace_nudge_input_gate(entry, ram), "Rejected active path became idle return");
            }
            ++active;
        }
    }
    std::printf("Verified %u lock combinations and %u active reload cases.\n", locks, active);
}
struct Evidence {
    std::vector<std::string> frames;
    unsigned starts = 0, finishes = 0, locks = 0;
    unsigned admitted_starts = 0, admitted_finishes = 0, admitted_locks = 0;
};
Evidence replay(const char* rom, bool native) {
    Evidence evidence;
    auto runtime = create_pce_runtime();
    require(runtime->load_rom(rom).loaded, "ROM load failed");
    std::vector<std::byte> bytes;
    const auto append = [&bytes](const auto& value) {
        const auto raw = std::as_bytes(std::span(&value, 1));
        bytes.insert(bytes.end(), raw.begin(), raw.end());
    };
    const auto append_span = [&bytes](const auto values) {
        const auto raw = std::as_bytes(values);
        bytes.insert(bytes.end(), raw.begin(), raw.end());
    };
    require(runtime->set_work_ram_write_observer([&](const ObservedMemoryWrite& w) {
        append(w.instruction.physical_bank); append(w.instruction.logical_pc);
        append(w.logical_address); append(w.value); append(w.absolute_master_cycle);
    }), "RAM observer failed");
    require(runtime->set_bus_write_observer([&](const ObservedBusWrite& w) {
        append(w.instruction.physical_bank); append(w.instruction.logical_pc);
        append(w.logical_address); append(w.value); append(w.absolute_master_cycle);
    }), "Bus observer failed");
    ExecutionHook hook{
        .routine = {1U, 0xC3CBU}, .mode = native ? HookMode::replace : HookMode::observe,
        .callback = [&](const RoutineEntry&) {
            const auto ram = runtime->export_work_ram();
            dc_nudge_input_gate gate{};
            require(dc_evaluate_nudge_input_gate(
                reinterpret_cast<const std::uint8_t*>(ram.data()), ram.size(), &gate) != 0,
                "Gate classification failed");
            if (gate.timer_after == 0U) {
                evidence.starts += gate.branch == DC_NUDGE_GATE_ACTIVE_START;
                evidence.finishes += gate.branch == DC_NUDGE_GATE_ACTIVE_FINISH;
            }
            evidence.locks += gate.branch == DC_NUDGE_GATE_LOCK_RETURN && gate.tilt_lock != 0U;
        },
        .replacement_callback = [&](const RoutineEntry& e, std::span<const std::byte> ram) {
            auto result = replace_nudge_input_gate(e, ram);
            dc_nudge_input_gate gate{};
            require(dc_evaluate_nudge_input_gate(
                reinterpret_cast<const std::uint8_t*>(ram.data()), ram.size(), &gate) != 0,
                "Replacement classification failed");
            if (result) {
                evidence.admitted_starts += gate.timer_after == 0U && gate.branch == DC_NUDGE_GATE_ACTIVE_START;
                evidence.admitted_finishes += gate.timer_after == 0U && gate.branch == DC_NUDGE_GATE_ACTIVE_FINISH;
                evidence.admitted_locks += gate.branch == DC_NUDGE_GATE_LOCK_RETURN && gate.tilt_lock != 0U;
            }
            return result;
        },
    };
    if (!native) { hook.replacement_callback = {}; }
    require(runtime->install_execution_hook(std::move(hook)), "Gate observer/replacement failed");
    for (unsigned frame = 0; frame < 1800U; ++frame) {
        // Controlled rare-state fixture, applied equally at frame boundaries.
        // Real code, interrupts, caller stack and I/O still run in the core.
        if (frame >= 1000U && frame < 1600U) {
            auto ram = runtime->export_work_ram();
            ram[0x53] = std::byte{0}; ram[0x55] = std::byte{0};
            ram[0xCC7] = frame < 1400U ? std::byte{0} : std::byte{0x80};
            ram[0xCC6] = std::byte{1}; ram[0xCC5] = std::byte{0xFF};
            ram[0xCC4] = static_cast<std::byte>((frame / 100U) & 1U);
            ram[0x1000] = std::byte{1}; ram[0x1C80] = std::byte{0};
            require(runtime->import_work_ram(ram), "Rare-state setup failed");
        }
        InputFrame input{};
        if (frame == 180U || frame == 181U || frame == 300U || frame == 301U) {
            input.player_one = static_cast<std::uint16_t>(Button::run);
        }
        if (frame == 420U || frame == 421U || frame == 700U || frame == 701U
            || (frame >= 850U && frame <= 880U)) {
            input.player_one = static_cast<std::uint16_t>(Button::one);
        }
        if (frame >= 1000U) { input.player_one = static_cast<std::uint16_t>(Button::two); }
        bytes.clear();
        const auto out = runtime->run_frame(input);
        require(!out.framebuffer.pixels.empty(), "Missing framebuffer");
        append(out.framebuffer.width); append(out.framebuffer.height);
        append(out.framebuffer.pitch_pixels); append_span(out.framebuffer.pixels);
        append(out.audio.sample_rate); append_span(out.audio.interleaved_stereo);
        const auto ram = runtime->export_work_ram(); append_span(std::span(ram));
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
}
int main(int argc, char** argv) {
    try {
        verify();
        if (argc == 2) {
            const auto assembly = replay(argv[1], false);
            const auto native = replay(argv[1], true);
            std::printf("Rare-state coverage: %u reload starts, %u reload finishes, %u tilt locks.\n",
                assembly.starts, assembly.finishes, assembly.locks);
            require(assembly.starts == 1U && assembly.finishes == 200U && assembly.locks == 400U,
                "Rare paths not exercised");
            require(assembly.starts == native.starts && assembly.finishes == native.finishes
                && assembly.locks == native.locks, "Rare-path count drift");
            require(native.admitted_starts == native.starts
                && native.admitted_finishes == native.finishes && native.admitted_locks == native.locks,
                "Rare-state equality depended on an assembly fallback");
            for (std::size_t f = 0; f < assembly.frames.size(); ++f) {
                if (assembly.frames[f] != native.frames[f]) {
                    std::printf("First rare-state mismatch at frame %zu\n", f);
                    throw std::runtime_error("Rare-state RAM/bus/video/audio mismatch");
                }
            }
            std::puts("All 1800 rare-state frames and ordered RAM/bus writes match assembly.");
        }
        return 0;
    }
    catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
