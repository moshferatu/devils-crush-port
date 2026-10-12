// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"
#include "command_line.hpp"

namespace devils_crush::app::host {

// Tracks which actor slots the camera never claims. A slot whose anchor never
// enters the viewport is never observed, so the retained layer seeds it once
// from ROM and never refreshes it against its live phase — the shape of an
// actor that is drawn but never animates.
class SeededActorTracker {
public:
    void observe(
        const std::span<const std::byte> work_ram,
        const int viewport_top_row,
        const int viewport_rows)
    {
        const auto actors =
            devils_crush::cabinet::decode_table_actor_state(work_ram);
        if (!actors) {
            return;
        }
        ++frames_;
        camera_low_ = std::min(camera_low_, viewport_top_row);
        camera_high_ = std::max(camera_high_, viewport_top_row);
        for (const auto& actor : *actors) {
            if (!actor.occupied() || !actor.camera_active()) {
                continue;
            }
            auto& record = slots_[actor.slot];
            ++record.live_frames;
            record.low_y = std::min(record.low_y, actor.world_y);
            record.high_y = std::max(record.high_y, actor.world_y);
            // The playfield sits 30 lines below the camera's top line.
            const int anchor = static_cast<int>(actor.world_y) - 30;
            const bool observed =
                anchor >= viewport_top_row
                && anchor < viewport_top_row + viewport_rows;
            if (observed) {
                ++record.observed_frames;
            }
            // Does the ROM keep advancing this actor's animation while the
            // camera is elsewhere? If not, the renderer is faithful and the
            // freeze is original behaviour rather than a presentation bug.
            const auto phase_index = kActorPhaseBase + actor.slot;
            if (phase_index >= work_ram.size()) {
                continue;
            }
            const auto phase =
                static_cast<std::uint8_t>(work_ram[phase_index]);
            if (record.have_phase && phase != record.last_phase) {
                if (observed) {
                    ++record.phase_changes_on;
                } else {
                    ++record.phase_changes_off;
                }
            }
            record.last_phase = phase;
            record.have_phase = true;
        }
    }

    void report() const
    {
        if (frames_ == 0U) {
            return;
        }
        std::cout << "Seeded-actor census over " << frames_
                  << " table frames; camera top spanned " << camera_low_
                  << ".." << camera_high_ << '\n';
        std::cout << "  slot  live  observed  offCam  dPhaseOn  dPhaseOff\n";
        std::size_t animate_off_camera = 0U;
        std::size_t frozen_off_camera = 0U;
        for (std::size_t slot = 0U; slot < slots_.size(); ++slot) {
            const auto& record = slots_[slot];
            if (record.live_frames == 0U) {
                continue;
            }
            const auto off_camera_frames =
                record.live_frames - record.observed_frames;
            if (off_camera_frames == 0U) {
                continue;
            }
            if (record.phase_changes_off != 0U) {
                ++animate_off_camera;
            } else if (record.phase_changes_on != 0U) {
                // Animates only while the camera holds it.
                ++frozen_off_camera;
            }
            std::cout << "  " << std::hex << std::uppercase << std::setw(4)
                      << std::setfill('0') << slot << std::dec
                      << std::setfill(' ') << std::setw(6)
                      << record.live_frames << std::setw(10)
                      << record.observed_frames << std::setw(8)
                      << off_camera_frames << std::setw(10)
                      << record.phase_changes_on << std::setw(11)
                      << record.phase_changes_off << '\n';
        }
        std::cout << "  " << animate_off_camera
                  << " slots advanced phase off-camera; " << frozen_off_camera
                  << " advanced only while observed\n";
        std::cout << "  slot  live  observed  world_y range  fixed\n";
        std::size_t never = 0U;
        for (std::size_t slot = 0U; slot < slots_.size(); ++slot) {
            const auto& record = slots_[slot];
            if (record.live_frames == 0U) {
                continue;
            }
            if (record.observed_frames != 0U) {
                continue;
            }
            ++never;
            const bool fixed =
                slot >= devils_crush::cabinet::kFirstFixedActorSlot
                && slot <= devils_crush::cabinet::kLastFixedActorSlot;
            std::cout << "  " << std::hex << std::uppercase << std::setw(4)
                      << std::setfill('0') << slot << std::dec
                      << std::setfill(' ') << std::setw(6)
                      << record.live_frames << std::setw(10)
                      << record.observed_frames << std::setw(8)
                      << record.low_y << ".." << record.high_y
                      << (fixed ? "   yes" : "   no") << '\n';
        }
        std::cout << "  " << never
                  << " live slots were never observed in the camera\n";
    }

private:
    // Work RAM is $2000-based; the actor appearance-phase array is $3980.
    static constexpr std::size_t kActorPhaseBase = 0x1980U;

    struct Record {
        std::uint64_t live_frames = 0U;
        std::uint64_t observed_frames = 0U;
        std::uint64_t phase_changes_on = 0U;
        std::uint64_t phase_changes_off = 0U;
        std::int16_t low_y = std::numeric_limits<std::int16_t>::max();
        std::int16_t high_y = std::numeric_limits<std::int16_t>::min();
        std::uint8_t last_phase = 0U;
        bool have_phase = false;
    };
    std::array<Record, devils_crush::cabinet::kActorSlotCount> slots_{};
    std::uint64_t frames_ = 0U;
    int camera_low_ = std::numeric_limits<int>::max();
    int camera_high_ = std::numeric_limits<int>::min();
};

[[nodiscard]] std::optional<devils_crush::render::PceBackgroundLayer>
decode_cabinet_table(
    const std::span<const std::byte> rom_bytes,
    const std::span<const std::byte> work_ram,
    const devils_crush::emulation::VideoMemorySnapshot& video,
    devils_crush::render::PersistentActorSpriteCache&
        persistent_actor_sprites,
    devils_crush::render::TablePaletteCache& table_palette_cache,
    const CabinetLayers layers,
    const bool report_overlay_stats,
    SeededActorTracker* const seeded_actors,
    devils_crush::render::PersistentActorOverlayStats* const out_stats =
        nullptr,
    devils_crush::render::TableMotionFrame* const motion_frame = nullptr);

} // namespace devils_crush::app::host
