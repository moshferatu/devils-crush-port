// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/cabinet/cabinet_state.hpp"
#include "devils_crush/render/pce_background.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace devils_crush::render {

struct PceSpriteSnapshot {
    // HuC6270 sprite patterns occupy 64 words per 16-by-16 pattern.
    std::span<const std::uint16_t> vram_words;
    // Four words per sprite: Y, X, pattern, and attributes.
    std::span<const std::uint16_t> sat_words;
    // Sprite colors use VCE entries 256-511.
    std::span<const std::uint16_t> palette_entries;
    std::uint16_t memory_width_register = 0;
};

struct SpriteOverlayStats {
    std::size_t visible_sprites = 0;
    std::size_t opaque_pixel_writes = 0;
};

struct PersistentActorOverlayStats {
    std::size_t observed_fixed_actors = 0;
    std::size_t seeded_fixed_actors = 0;
    std::size_t cached_fixed_actors = 0;
    std::size_t cached_flipper_actors = 0;
    std::size_t rendered_fixed_actors = 0;
    std::size_t rendered_flipper_actors = 0;
    std::size_t rendered_retained_fixed_actors = 0;
    std::size_t rendered_seeded_fixed_actors = 0;
    std::size_t restored_phase_variants = 0;
    std::size_t restored_animated_phase_variants = 0;
    std::size_t restored_sorcerer_phase_variants = 0;
    std::size_t resolved_sorcerer_phase_variants = 0;
    std::size_t missing_phase_variants = 0;
    std::size_t static_phase_fallbacks = 0;
    // Off-camera actors that changed phase but could not be re-resolved.
    // Each of these keeps its previous artwork while its position keeps
    // moving, which presents as a frozen actor.
    std::size_t stale_no_descriptor = 0;
    std::size_t stale_no_metadata = 0;
    std::size_t stale_no_metadata_hidden = 0;
    std::size_t stale_unusable_patterns = 0;
    std::size_t stale_resolve_failed = 0;
    std::size_t observed_table_world_effects = 0;
    std::size_t seeded_table_world_effects = 0;
    std::size_t cached_table_world_effects = 0;
    std::size_t rendered_table_world_effects = 0;
    std::size_t rendered_retained_table_world_effects = 0;
    std::size_t flipper_opaque_pixel_writes = 0;
    std::size_t opaque_pixel_writes = 0;

    // Per-slot record of what was actually drawn this frame, so a caller can
    // ask the question that matters directly: did this actor's artwork change
    // while its phase advanced? Counting code paths cannot answer that -- a
    // path can report success and still emit the same pixels.
    // actor_visual_hash folds each resolved sprite's pattern, attributes and
    // offsets; it is zero when the actor drew nothing.
    std::array<std::uint32_t, devils_crush::cabinet::kActorSlotCount>
        actor_visual_hash{};
    // What the ROM descriptors ask the actor to look like right now,
    // independent of phase. If this moves while actor_visual_hash does not,
    // the drawn artwork is stale whatever field drove the change.
    std::array<std::uint32_t, devils_crush::cabinet::kActorSlotCount>
        actor_metadata_hash{};
    std::array<std::uint8_t, devils_crush::cabinet::kActorSlotCount>
        actor_phase{};
    std::array<std::uint8_t, devils_crush::cabinet::kActorSlotCount>
        actor_on_camera{};
    std::array<std::uint8_t, devils_crush::cabinet::kActorSlotCount>
        actor_occupied{};
};

struct FixedActorSprite {
    std::uint8_t actor_slot = 0;
    int world_x = 0;
    int world_y = 0;
    std::uint16_t pattern = 0;
    std::uint16_t flags = 0;

    [[nodiscard]] bool operator==(const FixedActorSprite&) const = default;
};

inline constexpr std::size_t kPcePatternLoaderAppearanceCount = 0x2AU;

enum class PceVramUploadEncoding : std::uint8_t {
    compressed_stream,
    raw_words,
};

struct PceVramUpload {
    std::uint8_t phase = 0;
    std::uint16_t destination_word = 0;
    std::uint16_t asset_reference = 0;
    std::uint8_t source_bank = 0;
    std::uint16_t source_address = 0;
    PceVramUploadEncoding encoding =
        PceVramUploadEncoding::compressed_stream;
    std::vector<std::uint16_t> words;
};

struct PcePatternLoaderDescriptor {
    std::uint16_t logical_address = 0;
    // Appearances $24-$29 point at the resident-pattern loader routine
    // rather than at descriptor data and therefore have no per-phase upload.
    bool resident = false;
    std::vector<PceVramUpload> uploads;
};

struct PcePatternAtlas {
    std::array<
        std::uint8_t,
        kPcePatternLoaderAppearanceCount>
        appearance_descriptor_indices{};
    std::vector<PcePatternLoaderDescriptor> descriptors;
    // F768 selects one of five complete scene-resident packs and uploads it
    // at $7C00. Normal-table scene initialization selects pack 0; fixed
    // actor appearances sharing the F768 sentinel reference other,
    // upload-disjoint pattern blocks.
    std::vector<PceVramUpload> resident_uploads;
};

// Decodes the game's bank-zero E41F raw/RLE stream into VDC words. Source
// bytes follow the supplied HuC6280 MPR mapping so a stream may cross an
// 8 KiB logical-page boundary without assuming contiguous physical banks.
[[nodiscard]] std::optional<std::vector<std::uint16_t>>
decode_pce_vram_stream(
    std::span<const std::byte> rom_bytes,
    const std::array<std::uint8_t, 8>& mpr,
    std::uint16_t source_address,
    std::size_t maximum_output_words = 32768U);

// Decodes one F606 loader descriptor. byte_count bounds the descriptor data
// independently of the ROM. Mode 0 performs the original E365 raw-word
// upload; modes 1 and 2 resolve and decode E41F streams.
[[nodiscard]] std::optional<PcePatternLoaderDescriptor>
decode_pce_pattern_loader_descriptor(
    std::span<const std::byte> rom_bytes,
    std::uint16_t logical_address,
    std::size_t byte_count,
    std::uint16_t pattern_base,
    std::size_t maximum_output_words = 32768U);

// Decodes all 42 appearance selectors at F6A6/F6D0 into a local, ROM-derived
// atlas. Duplicate pointers share one descriptor. The six no-per-phase
// selectors share an F768 sentinel, while its five scene-resident packs are
// decoded separately into resident_uploads.
[[nodiscard]] std::optional<PcePatternAtlas>
decode_pce_pattern_atlas(
    std::span<const std::byte> rom_bytes,
    std::uint16_t pattern_base,
    std::size_t maximum_output_words = 32768U);

// Decodes bank $08's original actor-to-SAT descriptor tables for fixed
// normal-table slots. Coordinates are returned in table-world space. Actors
// hidden through flag bit 5 intentionally produce no sprites.
[[nodiscard]] std::optional<std::vector<FixedActorSprite>>
decode_fixed_actor_sprites(
    std::span<const std::byte> rom_bytes,
    std::span<const std::byte> work_ram);

// Decodes the ball and six flipper/control slots through the same bank-$08
// descriptor tables. Their actor coordinates are already in table-world
// space; the ball remains live-only while flippers may be retained.
[[nodiscard]] std::optional<std::vector<FixedActorSprite>>
decode_ball_and_flipper_sprites(
    std::span<const std::byte> rom_bytes,
    std::span<const std::byte> work_ram);

// Decodes the dynamically allocated normal-table pool through the same
// presentation tables. Callers must still distinguish persistent enemies,
// table-world effects, and camera-only transients and invalidate reused
// slots.
[[nodiscard]] std::optional<std::vector<FixedActorSprite>>
decode_dynamic_actor_sprites(
    std::span<const std::byte> rom_bytes,
    std::span<const std::byte> work_ram);

// Resolves fixed mechanisms, flippers, persistent enemies, and classified
// table-world effects from their exact ROM actor descriptors. The copied SAT
// remains authoritative for actors observed in the original camera; the
// resolved cache fills only actors omitted there, including sprites which
// overlap that camera band. Work RAM remains authoritative for occupancy,
// position, appearance, phase, dispatch, and effect lifetime, so off-camera
// actors continue to move and animate without advancing gameplay here. The
// moving ball and unclassified dynamic effects remain live-SAT-only, and
// reused slots are cleared immediately, so neither can leave a retained
// trail. Callers should composite this layer before the live SAT.
class PersistentActorSpriteCache {
public:
    void reset() noexcept;

    [[nodiscard]] std::optional<PersistentActorOverlayStats>
    update_and_composite(
        ArgbImage& destination,
        const PceSpriteSnapshot& snapshot,
        std::span<const std::byte> work_ram,
        int viewport_world_y,
        int destination_world_y,
        int framebuffer_playfield_y = 30,
        int framebuffer_playfield_height = 208,
        std::span<const std::uint8_t> background_opaque = {},
        std::span<const std::byte> rom_bytes = {});

private:
    struct CachedSprite {
        std::int16_t offset_x = 0;
        std::int16_t offset_y = 0;
        std::uint16_t pattern = 0;
        std::uint16_t flags = 0;
        std::uint8_t sat_index = 0;
        int width = 0;
        int height = 0;
        // Pattern pixels are resolved at observation time so later
        // camera-region VRAM cannot mutate a retained mechanism. Palette
        // indices remain live so actors learned during fade-in acquire the
        // settled VCE colors instead of freezing dark silhouettes.
        std::vector<std::uint16_t> palette_indices;
    };

    struct ActorVisual {
        bool learned = false;
        bool seeded = false;
        std::uint8_t type = 0;
        std::uint8_t appearance = 0;
        std::uint8_t phase = 0;
        std::uint16_t dispatch = 0;
        std::vector<CachedSprite> sprites;
    };

    std::array<
        ActorVisual,
        devils_crush::cabinet::kActorSlotCount>
        actor_visuals_{};
    std::array<
        std::vector<ActorVisual>,
        devils_crush::cabinet::kActorSlotCount>
        actor_visual_variants_{};
    std::optional<PcePatternAtlas> pattern_atlas_;
    std::uint16_t pattern_atlas_base_ = 0;
    bool left_initial_lower_viewport_ = false;
};

// Composites the copied post-DMA SAT in hardware priority order. SAT
// coordinates are viewport-relative; viewport_world_y locates the game's
// active playfield top in the decoded table source, while destination_world_y
// locates row zero of destination in that same coordinate system.
//
// Transparent sprite color zero is skipped. When background_opaque contains
// one byte per destination pixel, sprites without the HuC6270 foreground
// priority bit remain behind nonzero background pixels.
[[nodiscard]] std::optional<SpriteOverlayStats> composite_pce_sprites(
    ArgbImage& destination,
    const PceSpriteSnapshot& snapshot,
    int viewport_world_y,
    int destination_world_y,
    int framebuffer_playfield_y = 30,
    int framebuffer_playfield_height = 208,
    std::span<const std::uint8_t> background_opaque = {});

} // namespace devils_crush::render
