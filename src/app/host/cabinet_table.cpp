// SPDX-License-Identifier: GPL-3.0-or-later
#include "cabinet_table.hpp"

#include "command_line.hpp"

namespace devils_crush::app::host {

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
    devils_crush::render::PersistentActorOverlayStats* const out_stats,
    devils_crush::render::TableMotionFrame* const motion_frame)
{
    constexpr std::size_t table_width_tiles = 32U;
    constexpr std::size_t table_height_tiles = 96U;
    constexpr std::size_t table_bat_entries =
        table_width_tiles * table_height_tiles;
    constexpr std::size_t table_rom_offset =
        0x14U * 0x2000U + 0x000AU;
    constexpr std::size_t table_tile_bytes =
        table_bat_entries * 2U;
    constexpr std::size_t table_attribute_offset = 0x0259U;
    constexpr std::size_t table_attribute_bytes =
        table_bat_entries / 2U;
    if (rom_bytes.size() < table_rom_offset
        || rom_bytes.size() - table_rom_offset < table_tile_bytes
        || work_ram.size() < table_attribute_offset
        || work_ram.size() - table_attribute_offset
            < table_attribute_bytes) {
        return std::nullopt;
    }

    // The validation above already guarantees work RAM reaches well past the
    // camera pair at $0B:$0C.
    const auto camera_y = static_cast<std::int16_t>(
        static_cast<std::uint16_t>(
            std::to_integer<std::uint8_t>(work_ram[0x0BU]))
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(work_ram[0x0CU]))
           << 8U));
    const auto viewport_top_row = 480 + static_cast<int>(camera_y);

    // The copied SAT is latched at vblank, in the same time base as the
    // VDC's background scroll register, while work RAM is exported after
    // the frame's logic has run. Translating SAT screen coordinates with
    // the post-frame camera displaces every live sprite relative to the
    // retained actors and the fixed table by one frame of camera motion.
    // On the video-snapshot route that skew is zero on 2,279 of 2,727
    // frames -- every frame the camera rests -- and reaches 13 pixels on
    // the 448 frames it moves. Recover the camera the SAT was built
    // against: the scroll register settles exactly 32 rows below the
    // table-world viewport, and work RAM supplies the absolute region that
    // the register's 256-line wrap cannot.
    constexpr int scroll_register_world_bias = 32;
    constexpr int maximum_camera_skew = 32;
    auto sat_viewport_top_row = viewport_top_row;
    {
        auto skew =
            (static_cast<int>(video.background_y_scroll)
             - scroll_register_world_bias - viewport_top_row)
            % 256;
        if (skew > 127) {
            skew -= 256;
        } else if (skew < -128) {
            skew += 256;
        }
        if (skew >= -maximum_camera_skew
            && skew <= maximum_camera_skew) {
            sat_viewport_top_row += skew;
        }
    }
    if (seeded_actors != nullptr) {
        seeded_actors->observe(work_ram, viewport_top_row, 208);
    }

    // The attribute plane is region-owned, so the table is decoded against
    // the rows retained while the camera covered them rather than against
    // whichever region the game happens to be streaming this frame.
    const auto retained_palette_nibbles = table_palette_cache.update(
        work_ram.subspan(table_attribute_offset, table_attribute_bytes),
        viewport_top_row,
        208);

    const auto source =
        devils_crush::render::decode_packed_pce_background_layer({
            .tile_words =
                rom_bytes.subspan(table_rom_offset, table_tile_bytes),
            .packed_palette_nibbles = retained_palette_nibbles,
            .vram_words = video.vram_words,
            .palette_entries = video.palette_entries,
            .width_tiles = static_cast<int>(table_width_tiles),
            .height_tiles = static_cast<int>(table_height_tiles),
        });
    if (!source) {
        return std::nullopt;
    }
    auto table = devils_crush::render::crop_background_layer(
        *source,
        0,
        devils_crush::cabinet::kTableWorldTop,
        devils_crush::cabinet::kTableWorldWidth,
        devils_crush::cabinet::kTableWorldHeight);
    if (!table) {
        return table;
    }

    if (motion_frame != nullptr) {
        motion_frame->background = *table;
        motion_frame->sprites.clear();
    }
    if (layers == CabinetLayers::all
        || layers == CabinetLayers::retained_only) {
    const auto overlay_stats =
        persistent_actor_sprites.update_and_composite(
            table->image,
            {
                .vram_words = video.vram_words,
                .sat_words = video.sat_words,
                .palette_entries = video.palette_entries,
                .memory_width_register =
                    video.memory_width_register,
            },
            work_ram,
            viewport_top_row,
            devils_crush::cabinet::kTableWorldTop,
            30,
            208,
            table->opaque_pixels,
            rom_bytes,
            motion_frame);
    // A retained actor whose current phase has no ROM-owned artwork either
    // freezes on its last learned image or drops out entirely. Both read as
    // an actor that stops animating off-camera and then snaps when the live
    // SAT takes authority, so the counts are worth surfacing.
    if (out_stats != nullptr && overlay_stats) {
        *out_stats = *overlay_stats;
    }
    if (report_overlay_stats && overlay_stats) {
        std::cout << "Overlay: observed " << overlay_stats->observed_fixed_actors
                  << ", cached " << overlay_stats->cached_fixed_actors
                  << ", seeded " << overlay_stats->seeded_fixed_actors
                  << " | rendered "
                  << overlay_stats->rendered_fixed_actors << " ("
                  << overlay_stats->rendered_retained_fixed_actors
                  << " retained + "
                  << overlay_stats->rendered_seeded_fixed_actors
                  << " seeded) | frozen "
                  << overlay_stats->static_phase_fallbacks << ", missing "
                  << overlay_stats->missing_phase_variants << ", restored "
                  << overlay_stats->restored_phase_variants << '/'
                  << overlay_stats->restored_animated_phase_variants
                  << " phases/animated\n";
        std::cout << "  stale off-camera: no-descriptor "
                  << overlay_stats->stale_no_descriptor
                  << ", no-metadata " << overlay_stats->stale_no_metadata
                  << ", unusable-patterns "
                  << overlay_stats->stale_unusable_patterns
                  << ", resolve-failed "
                  << overlay_stats->stale_resolve_failed << '\n';
    }
    }
    // The live SAT is no longer part of the render. Measured against the
    // emulator's own framebuffer over the census route, compositing it on top
    // of the native layer disagreed on 3.14% of pixels while the native layer
    // alone disagreed on 2.40%: hardware's copy is latched at vblank and
    // placed through the scroll register, so it lands in a different time base
    // from the work-RAM actors beside it and the two disagree wherever they
    // overlap. It stays reachable as sat_only, purely as a diagnostic.
    if (layers == CabinetLayers::sat_only) {
    static_cast<void>(
        devils_crush::render::composite_pce_sprites(
            table->image,
            {
                .vram_words = video.vram_words,
                .sat_words = video.sat_words,
                .palette_entries = video.palette_entries,
                .memory_width_register =
                    video.memory_width_register,
            },
            sat_viewport_top_row,
            devils_crush::cabinet::kTableWorldTop,
            30,
            208,
            table->opaque_pixels));
    }
    return table;
}

} // namespace devils_crush::app::host
