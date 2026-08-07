// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/core/rom_image.hpp"
#include "devils_crush/emulation/pce_runtime.hpp"
#include "devils_crush/render/pce_background.hpp"
#include "devils_crush/render/pce_sprite_overlay.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using devils_crush::emulation::Button;
using devils_crush::emulation::InputFrame;

int failures = 0;

void check(const bool condition, const std::string_view description)
{
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

[[nodiscard]] InputFrame input_for_frame(const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)) {
        press(Button::one);
    }
    if (frame_number >= 1000 && frame_number <= 1180) {
        press(Button::one);
    }
    if (frame_number >= 1250) {
        const int phase = (frame_number - 1250) % 30;
        if (phase <= 1) {
            press(Button::left);
        }
        if (phase == 15 || phase == 16) {
            press(Button::one);
        }
    }
    if (is_pair(1450) || is_pair(2250) || is_pair(3050)) {
        press(Button::two);
    }
    return input;
}

template<typename T>
[[nodiscard]] std::span<const std::byte> as_bytes(
    const std::span<const T> values)
{
    return std::as_bytes(values);
}

struct BackgroundRowEntry {
    std::int16_t camera_y = 0;
    std::int16_t anchor_y = 0;
    std::uint16_t destination_word = 0;
    std::uint16_t tile_source = 0;
    std::uint16_t attribute_source = 0;
    std::uint8_t step_pixels = 0;
    std::uint8_t source_mpr = 0;
    std::uint8_t attribute_mpr = 0;
};

struct VramStreamEntry {
    std::uint64_t frame_number = 0;
    std::uint64_t absolute_master_cycle = 0;
    std::uint16_t destination_word = 0;
    std::uint16_t source_address = 0;
    std::uint8_t source_mpr = 0;
    std::uint16_t caller_pc = 0;
    std::uint8_t actor_slot = 0;
    std::uint8_t actor_appearance = 0;
    std::uint8_t actor_phase = 0;
    std::uint16_t loader_descriptor = 0;
    std::uint16_t pattern_base = 0;
    std::array<std::uint8_t, 8> mpr{};
};

struct SpriteRegionStream {
    std::uint64_t frame_number = 0;
    std::uint16_t destination_word = 0;
    std::uint8_t source_bank = 0;
    std::uint16_t source_address = 0;
    std::uint16_t caller_pc = 0;
    std::uint8_t actor_slot = 0;
    std::uint8_t actor_appearance = 0;
    std::uint8_t actor_phase = 0;
    std::uint16_t loader_descriptor = 0;
    std::uint16_t pattern_base = 0;

    [[nodiscard]] bool operator==(
        const SpriteRegionStream&) const = default;
};

struct ResidentPatternLoad {
    std::uint64_t frame_number = 0;
    std::uint8_t selector = 0;
    std::uint8_t actor_slot = 0;
    std::uint8_t actor_appearance = 0;
    std::uint8_t actor_phase = 0;

    [[nodiscard]] bool operator==(
        const ResidentPatternLoad&) const = default;
};

struct CompositeBackgroundComparison {
    std::size_t matching_pixels = 0;
    std::size_t sprite_or_overlay_pixels = 0;
};

struct FixedActorSatComparison {
    std::size_t matched_groups = 0;
    std::size_t eligible_groups = 0;
};

struct FixedActorAtlasComparison {
    std::size_t matched_groups = 0;
    std::size_t eligible_groups = 0;
};

struct ResidentFixedActorClassification {
    std::size_t actor_groups = 0;
    std::size_t covered_groups = 0;
    std::size_t disjoint_groups = 0;
    std::set<std::uint8_t> appearances;
    std::set<std::uint8_t> disjoint_appearances;
    std::set<std::uint8_t> overlapping_appearances;
};

struct FixedActorSeedClassification {
    std::size_t actor_groups = 0;
    std::size_t stream_owned_groups = 0;
    std::size_t upload_disjoint_groups = 0;
    std::size_t unresolved_groups = 0;
};

[[nodiscard]] bool upload_covers_fixed_actor_sprite(
    const devils_crush::render::PceVramUpload& upload,
    const devils_crush::render::FixedActorSprite& sprite)
{
    const auto upload_begin =
        static_cast<std::size_t>(upload.destination_word);
    const auto upload_end = upload_begin + upload.words.size();
    if (upload_end < upload_begin || upload_end > 32768U) {
        return false;
    }
    const int width = (sprite.flags & 0x0100U) != 0U ? 32 : 16;
    constexpr std::array heights{16, 32, 64, 64};
    const int height =
        heights[static_cast<std::size_t>(
            (sprite.flags >> 12U) & 0x03U)];
    auto first_pattern = static_cast<std::uint16_t>(
        (sprite.pattern >> 1U) & 0x03FFU);
    if (height == 32) {
        first_pattern =
            static_cast<std::uint16_t>(first_pattern & ~2U);
    } else if (height == 64) {
        first_pattern =
            static_cast<std::uint16_t>(first_pattern & ~6U);
    }
    if (width == 32) {
        first_pattern =
            static_cast<std::uint16_t>(first_pattern & ~1U);
    }
    for (int y = 0; y < height / 16; ++y) {
        for (int x = 0; x < width / 16; ++x) {
            const auto pattern = static_cast<std::size_t>(
                first_pattern
                | static_cast<std::uint16_t>(y * 2 + x));
            const auto pattern_begin = pattern * 64U;
            if (pattern_begin < upload_begin
                || pattern_begin > upload_end
                || upload_end - pattern_begin < 64U) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] bool upload_overlaps_fixed_actor_sprite(
    const devils_crush::render::PceVramUpload& upload,
    const devils_crush::render::FixedActorSprite& sprite)
{
    const auto upload_begin =
        static_cast<std::size_t>(upload.destination_word);
    const auto upload_end = upload_begin + upload.words.size();
    if (upload_end < upload_begin || upload_end > 32768U) {
        return true;
    }
    const int width = (sprite.flags & 0x0100U) != 0U ? 32 : 16;
    constexpr std::array heights{16, 32, 64, 64};
    const int height =
        heights[static_cast<std::size_t>(
            (sprite.flags >> 12U) & 0x03U)];
    auto first_pattern = static_cast<std::uint16_t>(
        (sprite.pattern >> 1U) & 0x03FFU);
    if (height == 32) {
        first_pattern =
            static_cast<std::uint16_t>(first_pattern & ~2U);
    } else if (height == 64) {
        first_pattern =
            static_cast<std::uint16_t>(first_pattern & ~6U);
    }
    if (width == 32) {
        first_pattern =
            static_cast<std::uint16_t>(first_pattern & ~1U);
    }
    for (int y = 0; y < height / 16; ++y) {
        for (int x = 0; x < width / 16; ++x) {
            const auto pattern = static_cast<std::size_t>(
                first_pattern
                | static_cast<std::uint16_t>(y * 2 + x));
            const auto pattern_begin = pattern * 64U;
            const auto pattern_end = pattern_begin + 64U;
            if (pattern_begin < upload_end
                && pattern_end > upload_begin) {
                return true;
            }
        }
    }
    return false;
}

[[nodiscard]] std::optional<ResidentFixedActorClassification>
classify_resident_fixed_actors(
    const devils_crush::render::PcePatternAtlas& atlas,
    const std::span<const devils_crush::render::FixedActorSprite> sprites,
    const std::span<const std::byte> work_ram)
{
    const auto actors =
        devils_crush::cabinet::decode_table_actor_state(work_ram);
    if (!actors) {
        return std::nullopt;
    }
    std::map<
        std::uint8_t,
        std::vector<devils_crush::render::FixedActorSprite>>
        sprites_by_actor;
    for (const auto& sprite : sprites) {
        sprites_by_actor[sprite.actor_slot].push_back(sprite);
    }

    ResidentFixedActorClassification result;
    for (const auto& [slot, actor_sprites] : sprites_by_actor) {
        const auto appearance = (*actors)[slot].appearance;
        if (appearance
            < devils_crush::render::
                  kPcePatternLoaderAppearanceCount - 6U) {
            continue;
        }
        ++result.actor_groups;
        result.appearances.insert(appearance);
        const auto& resident_upload = atlas.resident_uploads.front();
        if (std::ranges::all_of(
                actor_sprites,
                [&resident_upload](
                    const devils_crush::render::FixedActorSprite&
                        sprite) {
                    return upload_covers_fixed_actor_sprite(
                        resident_upload,
                        sprite);
                })) {
            ++result.covered_groups;
        }
        const auto overlaps_actor =
            [&actor_sprites](
                const devils_crush::render::PceVramUpload& upload) {
                return std::ranges::any_of(
                    actor_sprites,
                    [&upload](
                        const devils_crush::render::
                            FixedActorSprite& sprite) {
                        return upload_overlaps_fixed_actor_sprite(
                            upload,
                            sprite);
                    });
            };
        const bool overlaps_known_upload =
            std::ranges::any_of(
                atlas.resident_uploads,
                overlaps_actor)
            || std::ranges::any_of(
                atlas.descriptors,
                [&overlaps_actor](
                    const devils_crush::render::
                        PcePatternLoaderDescriptor& descriptor) {
                    return std::ranges::any_of(
                        descriptor.uploads,
                        overlaps_actor);
                });
        if (overlaps_known_upload) {
            result.overlapping_appearances.insert(appearance);
        } else {
            ++result.disjoint_groups;
            result.disjoint_appearances.insert(appearance);
        }
    }
    return result;
}

[[nodiscard]] std::optional<FixedActorSeedClassification>
classify_fixed_actor_seeds(
    const devils_crush::render::PcePatternAtlas& atlas,
    const std::span<const devils_crush::render::FixedActorSprite> sprites,
    const std::span<const std::byte> work_ram)
{
    const auto actors =
        devils_crush::cabinet::decode_table_actor_state(work_ram);
    if (!actors
        || work_ram.size()
            <= 0x1980U
                + devils_crush::cabinet::kLastFixedActorSlot) {
        return std::nullopt;
    }
    std::map<
        std::uint8_t,
        std::vector<devils_crush::render::FixedActorSprite>>
        sprites_by_actor;
    for (const auto& sprite : sprites) {
        sprites_by_actor[sprite.actor_slot].push_back(sprite);
    }

    FixedActorSeedClassification result;
    for (const auto& [slot, actor_sprites] : sprites_by_actor) {
        const auto& actor = (*actors)[slot];
        ++result.actor_groups;
        const devils_crush::render::PceVramUpload* selected_upload =
            nullptr;
        if (actor.appearance
            < devils_crush::render::
                  kPcePatternLoaderAppearanceCount) {
            const auto descriptor_index =
                atlas.appearance_descriptor_indices[
                    actor.appearance];
            if (descriptor_index >= atlas.descriptors.size()) {
                return std::nullopt;
            }
            const auto& descriptor =
                atlas.descriptors[descriptor_index];
            if (!descriptor.resident) {
                const auto phase =
                    std::to_integer<std::uint8_t>(
                        work_ram[0x1980U + slot]);
                const auto selected = std::ranges::find(
                    descriptor.uploads,
                    phase,
                    &devils_crush::render::PceVramUpload::phase);
                if (selected != descriptor.uploads.end()) {
                    selected_upload = &*selected;
                }
            }
        }
        if (selected_upload
            && std::ranges::all_of(
                actor_sprites,
                [selected_upload](
                    const devils_crush::render::
                        FixedActorSprite& sprite) {
                    return upload_covers_fixed_actor_sprite(
                        *selected_upload,
                        sprite);
                })) {
            ++result.stream_owned_groups;
            continue;
        }
        const auto overlaps_actor =
            [&actor_sprites](
                const devils_crush::render::PceVramUpload& upload) {
                return std::ranges::any_of(
                    actor_sprites,
                    [&upload](
                        const devils_crush::render::
                            FixedActorSprite& sprite) {
                        return upload_overlaps_fixed_actor_sprite(
                            upload,
                            sprite);
                    });
            };
        const bool overlaps_known_upload =
            std::ranges::any_of(
                atlas.resident_uploads,
                overlaps_actor)
            || std::ranges::any_of(
                atlas.descriptors,
                [&overlaps_actor](
                    const devils_crush::render::
                        PcePatternLoaderDescriptor& descriptor) {
                    return std::ranges::any_of(
                        descriptor.uploads,
                        overlaps_actor);
                });
        if (overlaps_known_upload) {
            ++result.unresolved_groups;
        } else {
            ++result.upload_disjoint_groups;
        }
    }
    return result;
}

[[nodiscard]] std::optional<FixedActorAtlasComparison>
compare_fixed_actor_atlas_to_vram(
    const devils_crush::render::PcePatternAtlas& atlas,
    const std::span<const std::byte> work_ram,
    const std::span<const std::uint16_t> vram_words)
{
    const auto actors =
        devils_crush::cabinet::decode_table_actor_state(work_ram);
    if (!actors || work_ram.size() <= 0x1980U
            + devils_crush::cabinet::kLastFixedActorSlot
        || vram_words.size() < 32768U) {
        return std::nullopt;
    }

    FixedActorAtlasComparison result;
    for (std::size_t slot =
             devils_crush::cabinet::kFirstFixedActorSlot;
         slot <= devils_crush::cabinet::kLastFixedActorSlot;
         ++slot) {
        const auto& actor = (*actors)[slot];
        if (!actor.occupied() || !actor.camera_active()
            || actor.appearance
                >= devils_crush::render::
                       kPcePatternLoaderAppearanceCount) {
            continue;
        }
        const auto descriptor_index =
            atlas.appearance_descriptor_indices[actor.appearance];
        if (descriptor_index >= atlas.descriptors.size()) {
            return std::nullopt;
        }
        const auto& descriptor =
            atlas.descriptors[descriptor_index];
        if (descriptor.resident) {
            continue;
        }
        const auto phase =
            std::to_integer<std::uint8_t>(
                work_ram[0x1980U + slot]);
        const auto upload = std::ranges::find(
            descriptor.uploads,
            phase,
            &devils_crush::render::PceVramUpload::phase);
        if (upload == descriptor.uploads.end()
            || upload->destination_word >= vram_words.size()
            || upload->words.size()
                > vram_words.size() - upload->destination_word) {
            continue;
        }
        ++result.eligible_groups;
        if (std::equal(
                upload->words.begin(),
                upload->words.end(),
                vram_words.begin()
                    + static_cast<std::ptrdiff_t>(
                        upload->destination_word))) {
            ++result.matched_groups;
        }
    }
    return result;
}

[[nodiscard]] std::optional<FixedActorSatComparison>
compare_fixed_actors_to_sat(
    const std::span<const devils_crush::render::FixedActorSprite> sprites,
    const std::span<const std::byte> work_ram,
    const std::span<const std::uint16_t> sat_words,
    const int viewport_world_y)
{
    const auto actors =
        devils_crush::cabinet::decode_table_actor_state(work_ram);
    if (!actors || sat_words.size() < 256U) {
        return std::nullopt;
    }

    FixedActorSatComparison result;
    for (std::size_t slot =
             devils_crush::cabinet::kFirstFixedActorSlot;
         slot <= devils_crush::cabinet::kLastFixedActorSlot;
         ++slot) {
        const auto& actor = (*actors)[slot];
        if (!actor.occupied() || !actor.camera_active()
            || (actor.flags & 0x20U) != 0U) {
            continue;
        }
        std::vector<std::uint16_t> expected_sat;
        for (const auto& sprite : sprites) {
            if (sprite.actor_slot != slot) {
                continue;
            }
            expected_sat.push_back(
                static_cast<std::uint16_t>(
                    sprite.world_y - viewport_world_y + 0x40));
            expected_sat.push_back(
                static_cast<std::uint16_t>(
                    sprite.world_x + 0x20));
            expected_sat.push_back(sprite.pattern);
            expected_sat.push_back(sprite.flags);
        }
        if (expected_sat.empty()) {
            continue;
        }
        ++result.eligible_groups;
        if (std::search(
                sat_words.begin(),
                sat_words.end(),
                expected_sat.begin(),
                expected_sat.end())
            != sat_words.end()) {
            ++result.matched_groups;
        }
    }
    return result;
}

[[nodiscard]] std::optional<FixedActorSatComparison>
compare_ball_and_flippers_to_sat(
    const std::span<const devils_crush::render::FixedActorSprite> sprites,
    const std::span<const std::byte> work_ram,
    const std::span<const std::uint16_t> sat_words,
    const int viewport_world_y)
{
    const auto actors =
        devils_crush::cabinet::decode_table_actor_state(work_ram);
    if (!actors || sat_words.size() < 256U) {
        return std::nullopt;
    }

    FixedActorSatComparison result;
    for (std::size_t slot = 0U;
         slot < devils_crush::cabinet::kFirstFixedActorSlot;
         ++slot) {
        const auto& actor = (*actors)[slot];
        if (!actor.occupied() || !actor.camera_active()
            || (actor.flags & 0x20U) != 0U) {
            continue;
        }
        std::vector<std::uint16_t> expected_sat;
        for (const auto& sprite : sprites) {
            if (sprite.actor_slot != slot) {
                continue;
            }
            expected_sat.push_back(
                static_cast<std::uint16_t>(
                    sprite.world_y - viewport_world_y
                    + 0x40));
            expected_sat.push_back(
                static_cast<std::uint16_t>(
                    sprite.world_x + 0x20));
            expected_sat.push_back(sprite.pattern);
            expected_sat.push_back(sprite.flags);
        }
        if (expected_sat.empty()) {
            continue;
        }
        ++result.eligible_groups;
        if (std::search(
                sat_words.begin(),
                sat_words.end(),
                expected_sat.begin(),
                expected_sat.end())
            != sat_words.end()) {
            ++result.matched_groups;
        }
    }
    return result;
}

[[nodiscard]] std::optional<FixedActorSatComparison>
compare_dynamic_actors_to_sat(
    const std::span<const devils_crush::render::FixedActorSprite> sprites,
    const std::span<const std::byte> work_ram,
    const std::span<const std::uint16_t> sat_words,
    const int viewport_world_y)
{
    const auto actors =
        devils_crush::cabinet::decode_table_actor_state(work_ram);
    if (!actors || sat_words.size() < 256U) {
        return std::nullopt;
    }

    FixedActorSatComparison result;
    for (std::size_t slot =
             devils_crush::cabinet::kFirstDynamicActorSlot;
         slot <= devils_crush::cabinet::kLastDynamicActorSlot;
         ++slot) {
        const auto& actor = (*actors)[slot];
        if (!actor.occupied() || !actor.camera_active()
            || (actor.flags & 0x20U) != 0U) {
            continue;
        }
        std::vector<std::uint16_t> expected_sat;
        for (const auto& sprite : sprites) {
            if (sprite.actor_slot != slot) {
                continue;
            }
            expected_sat.push_back(
                static_cast<std::uint16_t>(
                    sprite.world_y - viewport_world_y + 0x40));
            expected_sat.push_back(
                static_cast<std::uint16_t>(
                    sprite.world_x + 0x20));
            expected_sat.push_back(sprite.pattern);
            expected_sat.push_back(sprite.flags);
        }
        if (expected_sat.empty()) {
            continue;
        }
        ++result.eligible_groups;
        if (std::search(
                sat_words.begin(),
                sat_words.end(),
                expected_sat.begin(),
                expected_sat.end())
            != sat_words.end()) {
            ++result.matched_groups;
        }
    }
    return result;
}

[[nodiscard]] constexpr bool retained_dynamic_enemy(
    const devils_crush::cabinet::TableActorState& actor) noexcept
{
    return actor.type == 0x82U
        && (actor.appearance == 0x1AU
            || actor.appearance == 0x39U);
}

struct DynamicEnemyCensus {
    std::size_t small_soldiers = 0;
    std::size_t tower_insects = 0;
};

struct TableWorldEffectCensus {
    std::size_t impact_bursts = 0;
    std::size_t camera_active_impact_bursts = 0;
    std::set<std::uint8_t> slots;
    std::set<std::uint8_t> appearances;
    std::set<std::uint8_t> phases;
    std::set<std::uint16_t> dispatches;
};

[[nodiscard]] std::optional<DynamicEnemyCensus>
census_dynamic_enemies(
    const std::span<const std::byte> work_ram)
{
    const auto actors =
        devils_crush::cabinet::decode_table_actor_state(work_ram);
    if (!actors) {
        return std::nullopt;
    }

    DynamicEnemyCensus result;
    for (std::size_t slot =
             devils_crush::cabinet::kFirstDynamicActorSlot;
         slot <= devils_crush::cabinet::kLastDynamicActorSlot;
         ++slot) {
        const auto& actor = (*actors)[slot];
        if (!actor.occupied() || actor.type != 0x82U) {
            continue;
        }
        if (actor.appearance == 0x1AU) {
            ++result.small_soldiers;
        } else if (actor.appearance == 0x39U) {
            ++result.tower_insects;
        }
    }
    return result;
}

[[nodiscard]] std::optional<TableWorldEffectCensus>
census_table_world_effects(
    const std::span<const std::byte> work_ram)
{
    const auto actors =
        devils_crush::cabinet::decode_table_actor_state(work_ram);
    if (!actors) {
        return std::nullopt;
    }

    TableWorldEffectCensus result;
    for (std::size_t slot =
             devils_crush::cabinet::kFirstDynamicActorSlot;
         slot <= devils_crush::cabinet::kLastDynamicActorSlot;
         ++slot) {
        const auto& actor = (*actors)[slot];
        if (actor.type != 0x8FU
            || actor.appearance != 0x10U
            || (actor.dispatch != 0x4290U
                && actor.dispatch != 0x42B0U)) {
            continue;
        }
        ++result.impact_bursts;
        if (actor.camera_active()) {
            ++result.camera_active_impact_bursts;
        }
        result.slots.insert(static_cast<std::uint8_t>(slot));
        result.appearances.insert(actor.appearance);
        result.phases.insert(std::to_integer<std::uint8_t>(
            work_ram[0x1980U + slot]));
        result.dispatches.insert(actor.dispatch);
    }
    return result;
}

[[nodiscard]] std::optional<std::vector<
    devils_crush::render::FixedActorSprite>>
retained_dynamic_enemy_sprites(
    const std::span<
        const devils_crush::render::FixedActorSprite> sprites,
    const std::span<const std::byte> work_ram)
{
    const auto actors =
        devils_crush::cabinet::decode_table_actor_state(work_ram);
    if (!actors) {
        return std::nullopt;
    }

    std::vector<devils_crush::render::FixedActorSprite> result;
    std::ranges::copy_if(
        sprites,
        std::back_inserter(result),
        [&actors](const devils_crush::render::FixedActorSprite& sprite) {
            return retained_dynamic_enemy(
                (*actors)[sprite.actor_slot]);
        });
    return result;
}

[[nodiscard]] constexpr std::uint32_t quantize_argb_to_rgb565(
    const std::uint32_t pixel) noexcept
{
    const auto red = (pixel >> 16U) & 0xFFU;
    const auto green = (pixel >> 8U) & 0xFFU;
    const auto blue = pixel & 0xFFU;
    const auto red_five = red >> 3U;
    const auto green_six = green >> 2U;
    const auto blue_five = blue >> 3U;
    const auto red_eight = (red_five << 3U) | (red_five >> 2U);
    const auto green_eight =
        (green_six << 2U) | (green_six >> 4U);
    const auto blue_eight = (blue_five << 3U) | (blue_five >> 2U);
    return 0xFF000000U | (red_eight << 16U) | (green_eight << 8U)
        | blue_eight;
}

[[nodiscard]] std::optional<CompositeBackgroundComparison>
compare_table_to_composite(
    const devils_crush::render::ArgbImage& table,
    const int table_y,
    const devils_crush::emulation::FramebufferView& framebuffer)
{
    constexpr int playfield_width = 256;
    constexpr int playfield_height = 208;
    // Beetle crops the VCE surface at scanline 14. In the resulting
    // 240-line frame, the game's active 208-line playfield begins at y=30.
    constexpr int framebuffer_y = 30;
    if (!table.valid() || table.width != playfield_width || table_y < 0
        || table_y > table.height - playfield_height
        || framebuffer.width != playfield_width
        || framebuffer.height < framebuffer_y + playfield_height
        || framebuffer.pitch_pixels < framebuffer.width) {
        return std::nullopt;
    }

    CompositeBackgroundComparison result;
    for (int y = 0; y < playfield_height; ++y) {
        const auto table_offset =
            static_cast<std::size_t>(table_y + y)
            * static_cast<std::size_t>(table.width);
        const auto framebuffer_offset =
            static_cast<std::size_t>(framebuffer_y + y)
            * static_cast<std::size_t>(framebuffer.pitch_pixels);
        for (int x = 0; x < playfield_width; ++x) {
            const auto expected = quantize_argb_to_rgb565(
                table.pixels[
                    table_offset + static_cast<std::size_t>(x)]);
            const auto actual =
                framebuffer.pixels[
                    framebuffer_offset + static_cast<std::size_t>(x)];
            if (expected == actual) {
                ++result.matching_pixels;
            } else {
                ++result.sprite_or_overlay_pixels;
            }
        }
    }
    return result;
}

[[nodiscard]] bool force_ball_y(
    devils_crush::emulation::PceRuntime& runtime,
    const std::uint16_t ball_y)
{
    auto work_ram = runtime.export_work_ram();
    if (work_ram.size() <= 0x1380U) {
        return false;
    }
    work_ram[0x1300U] =
        static_cast<std::byte>(ball_y & 0xFFU);
    work_ram[0x1380U] =
        static_cast<std::byte>((ball_y >> 8U) & 0xFFU);
    return runtime.import_work_ram(work_ram);
}

[[nodiscard]] bool write_bmp(
    const std::filesystem::path& path,
    const devils_crush::render::ArgbImage& image)
{
    if (!image.valid()) {
        return false;
    }

    const auto pixel_bytes = static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(image.width)
        * static_cast<std::uint64_t>(image.height) * 4U);
    constexpr std::uint32_t pixel_offset = 14U + 40U;
    const auto file_bytes = pixel_offset + pixel_bytes;
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        return false;
    }
    const auto write_u16 = [&output](const std::uint16_t value) {
        const std::array bytes{
            static_cast<char>(value & 0xFFU),
            static_cast<char>((value >> 8U) & 0xFFU),
        };
        output.write(bytes.data(), bytes.size());
    };
    const auto write_u32 = [&output](const std::uint32_t value) {
        const std::array bytes{
            static_cast<char>(value & 0xFFU),
            static_cast<char>((value >> 8U) & 0xFFU),
            static_cast<char>((value >> 16U) & 0xFFU),
            static_cast<char>((value >> 24U) & 0xFFU),
        };
        output.write(bytes.data(), bytes.size());
    };

    output.write("BM", 2);
    write_u32(file_bytes);
    write_u16(0U);
    write_u16(0U);
    write_u32(pixel_offset);
    write_u32(40U);
    write_u32(static_cast<std::uint32_t>(image.width));
    write_u32(static_cast<std::uint32_t>(image.height));
    write_u16(1U);
    write_u16(32U);
    write_u32(0U);
    write_u32(pixel_bytes);
    write_u32(0U);
    write_u32(0U);
    write_u32(0U);
    write_u32(0U);

    for (int y = image.height - 1; y >= 0; --y) {
        const auto row_offset =
            static_cast<std::size_t>(y)
            * static_cast<std::size_t>(image.width);
        for (int x = 0; x < image.width; ++x) {
            const auto pixel =
                image.pixels[
                    row_offset + static_cast<std::size_t>(x)];
            const std::array bytes{
                static_cast<char>(pixel & 0xFFU),
                static_cast<char>((pixel >> 8U) & 0xFFU),
                static_cast<char>((pixel >> 16U) & 0xFFU),
                static_cast<char>(0xFFU),
            };
            output.write(bytes.data(), bytes.size());
        }
    }
    return static_cast<bool>(output);
}

[[nodiscard]] std::filesystem::path diagnostic_path(
    const std::filesystem::path& base,
    const std::string_view suffix)
{
    auto path = base;
    path.replace_filename(
        path.stem().string() + std::string(suffix)
        + path.extension().string());
    return path;
}

[[nodiscard]] std::optional<devils_crush::render::ArgbImage>
copy_framebuffer(
    const devils_crush::emulation::FramebufferView& framebuffer)
{
    devils_crush::render::ArgbImage image{
        .width = framebuffer.width,
        .height = framebuffer.height,
    };
    if (image.width <= 0 || image.height <= 0
        || framebuffer.pitch_pixels < image.width) {
        return std::nullopt;
    }
    image.pixels.resize(
        static_cast<std::size_t>(image.width)
        * static_cast<std::size_t>(image.height));
    for (int y = 0; y < image.height; ++y) {
        std::copy_n(
            framebuffer.pixels.begin()
                + static_cast<std::ptrdiff_t>(y)
                    * framebuffer.pitch_pixels,
            image.width,
            image.pixels.begin()
                + static_cast<std::ptrdiff_t>(y) * image.width);
    }
    return image;
}

[[nodiscard]] std::optional<devils_crush::render::PceBackgroundLayer>
decode_stitched_table_layer(
    const std::span<const std::byte> rom_bytes,
    const std::span<const std::byte> work_ram,
    const devils_crush::emulation::VideoMemorySnapshot& video)
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
    return devils_crush::render::decode_packed_pce_background_layer({
        .tile_words =
            rom_bytes.subspan(table_rom_offset, table_tile_bytes),
        .packed_palette_nibbles =
            work_ram.subspan(
                table_attribute_offset,
                table_attribute_bytes),
        .vram_words = video.vram_words,
        .palette_entries = video.palette_entries,
        .width_tiles = static_cast<int>(table_width_tiles),
        .height_tiles = static_cast<int>(table_height_tiles),
    });
}

[[nodiscard]] std::optional<devils_crush::render::ArgbImage>
decode_stitched_table(
    const std::span<const std::byte> rom_bytes,
    const std::span<const std::byte> work_ram,
    const devils_crush::emulation::VideoMemorySnapshot& video)
{
    auto layer =
        decode_stitched_table_layer(rom_bytes, work_ram, video);
    if (!layer) {
        return std::nullopt;
    }
    return std::move(layer->image);
}

} // namespace

int main(const int argc, char** const argv)
{
    const bool map_row_writer =
        argc == 3
        && std::string_view(argv[2]) == "--map-row-writer";
    const bool write_table =
        argc == 4
        && std::string_view(argv[2]) == "--write-table-bmp";
    if (argc < 2 || (!map_row_writer && !write_table && argc != 2)) {
        std::cerr
            << "Usage: dc_video_snapshot_tests <rom-path>"
               " [--map-row-writer | --write-table-bmp <path>]\n";
        return EXIT_FAILURE;
    }
    const auto table_output =
        write_table
        ? std::optional<std::filesystem::path>{
              std::filesystem::path(argv[3])}
        : std::nullopt;

    auto runtime = devils_crush::emulation::create_pce_runtime();
    const auto load = runtime->load_rom(std::filesystem::path(argv[1]));
    check(load.loaded, "supported ROM loads for video snapshot");
    if (!load.loaded) {
        std::cerr << load.message << '\n';
        return EXIT_FAILURE;
    }
    std::vector<std::byte> rom_bytes(
        static_cast<std::size_t>(
            devils_crush::rom::kSupportedRomSize));
    std::ifstream rom_input(
        std::filesystem::path(argv[1]),
        std::ios::binary);
    if (rom_input) {
        rom_input.read(
            reinterpret_cast<char*>(rom_bytes.data()),
            static_cast<std::streamsize>(rom_bytes.size()));
    }
    check(
        rom_input
            && rom_input.gcount()
                == static_cast<std::streamsize>(rom_bytes.size()),
        "supported ROM bytes load for packed table decode");
    if (!rom_input) {
        return EXIT_FAILURE;
    }

    const auto pattern_atlas =
        devils_crush::render::decode_pce_pattern_atlas(
            rom_bytes,
            0x3800U);
    constexpr std::array<std::uint8_t, 42>
        expected_atlas_mapping{
            0, 0, 0, 0,
            1, 1, 1, 1,
            2,
            3, 3, 3, 3, 3, 3,
            4,
            5, 5,
            6, 6, 6, 6, 6, 6, 6, 6,
            7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
            8, 8, 8, 8, 8, 8,
        };
    constexpr std::array<std::uint16_t, 9>
        expected_descriptor_pointers{
            0xF6FAU,
            0xF709U,
            0xF716U,
            0xF721U,
            0xF72EU,
            0xF73BU,
            0xF748U,
            0xF753U,
            0xF768U,
        };
    constexpr std::array<std::size_t, 9>
        expected_upload_counts{
            5U, 4U, 3U, 4U, 4U, 4U, 3U, 8U, 0U,
        };
    constexpr std::array<std::uint16_t, 8>
        expected_upload_destinations{
            0x7000U,
            0x5500U,
            0x4F00U,
            0x7200U,
            0x7380U,
            0x5400U,
            0x6D00U,
            0x7500U,
        };
    constexpr std::array<std::uint16_t, 5>
        expected_resident_assets{
            0x1222U, 0x1224U, 0x1226U, 0x1228U, 0x122AU,
        };
    constexpr std::array<std::uint16_t, 5>
        expected_resident_sources{
            0x91EBU, 0x9919U, 0xA04FU, 0xA79DU, 0xAECCU,
        };
    bool atlas_descriptors_match =
        pattern_atlas
        && pattern_atlas->appearance_descriptor_indices
            == expected_atlas_mapping
        && pattern_atlas->descriptors.size()
            == expected_descriptor_pointers.size();
    std::size_t atlas_upload_count = 0;
    std::size_t atlas_word_count = 0;
    std::size_t resident_upload_count = 0;
    std::size_t resident_word_count = 0;
    if (pattern_atlas
        && pattern_atlas->descriptors.size()
            == expected_descriptor_pointers.size()) {
        for (std::size_t index = 0;
             index < pattern_atlas->descriptors.size();
             ++index) {
            const auto& descriptor =
                pattern_atlas->descriptors[index];
            atlas_descriptors_match =
                atlas_descriptors_match
                && descriptor.logical_address
                    == expected_descriptor_pointers[index]
                && descriptor.uploads.size()
                    == expected_upload_counts[index]
                && descriptor.resident == (index == 8U);
            if (index < expected_upload_destinations.size()) {
                atlas_descriptors_match =
                    atlas_descriptors_match
                    && std::ranges::all_of(
                        descriptor.uploads,
                        [index, &expected_upload_destinations](
                            const devils_crush::render::
                                PceVramUpload& upload) {
                            return upload.destination_word
                                == expected_upload_destinations[index]
                                && upload.encoding
                                    == devils_crush::render::
                                           PceVramUploadEncoding::
                                               compressed_stream
                                && !upload.words.empty();
                        });
            }
            atlas_upload_count += descriptor.uploads.size();
            for (const auto& upload : descriptor.uploads) {
                atlas_word_count += upload.words.size();
            }
        }
        resident_upload_count =
            pattern_atlas->resident_uploads.size();
        for (std::size_t index = 0;
             index < pattern_atlas->resident_uploads.size();
             ++index) {
            const auto& upload =
                pattern_atlas->resident_uploads[index];
            resident_word_count += upload.words.size();
            atlas_descriptors_match =
                atlas_descriptors_match
                && index < expected_resident_assets.size()
                && upload.phase == index
                && upload.destination_word == 0x7C00U
                && upload.asset_reference
                    == expected_resident_assets[index]
                && upload.source_bank == 0x12U
                && upload.source_address
                    == expected_resident_sources[index]
                && upload.encoding
                    == devils_crush::render::
                           PceVramUploadEncoding::compressed_stream
                && upload.words.size() == 1008U;
            std::cout << "Resident atlas selector="
                      << static_cast<unsigned>(upload.phase)
                      << " asset=" << std::hex << std::uppercase
                      << upload.asset_reference
                      << " source=" << static_cast<unsigned>(
                             upload.source_bank)
                      << ':' << upload.source_address
                      << " destination=" << upload.destination_word
                      << std::dec << " words=" << upload.words.size()
                      << '\n';
        }
    }
    check(
        atlas_descriptors_match
            && atlas_upload_count == 35U
            && atlas_word_count == 12544U
            && resident_upload_count == 5U
            && resident_word_count == 5040U,
        "F606 selectors and five F768 packs decode into the local pattern atlas");
    std::cout << "Pattern atlas: selectors=42 descriptors="
              << (pattern_atlas
                      ? pattern_atlas->descriptors.size()
                      : 0U)
              << " uploads=" << atlas_upload_count
              << " words=" << atlas_word_count
              << " resident_uploads=" << resident_upload_count
              << " resident_words=" << resident_word_count << '\n';

    std::map<std::pair<std::uint8_t, std::uint16_t>, std::size_t>
        vdc_write_sites;
    std::vector<devils_crush::emulation::RoutineCall>
        background_row_calls;
    std::vector<BackgroundRowEntry> background_row_entries;
    std::uint8_t selected_vdc_register = 0U;
    std::uint16_t vdc_memory_write_address = 0U;
    std::uint8_t vdc_memory_write_low = 0U;
    std::map<std::pair<std::uint8_t, std::uint16_t>, std::size_t>
        sat_writer_sites;
    std::map<std::pair<std::uint8_t, std::uint16_t>, std::size_t>
        sprite_region_writer_sites;
    std::vector<VramStreamEntry> vram_stream_entries;
    std::vector<ResidentPatternLoad> resident_pattern_loads;
    if (map_row_writer) {
        check(
            runtime->install_execution_hook({
                .routine = {
                    .physical_bank = 0x00U,
                    .logical_pc = 0xF400U,
                },
                .mode = devils_crush::emulation::HookMode::observe,
                .callback =
                    [&runtime, &background_row_entries](
                        const devils_crush::emulation::RoutineEntry& entry) {
                        const auto work_ram = runtime->export_work_ram();
                        const auto read_u8 =
                            [&work_ram](const std::size_t offset) {
                                return std::to_integer<std::uint8_t>(
                                    work_ram[offset]);
                            };
                        const auto read_i16 =
                            [&read_u8](
                                const std::size_t low_offset,
                                const std::size_t high_offset) {
                                return static_cast<std::int16_t>(
                                    static_cast<std::uint16_t>(
                                        read_u8(low_offset))
                                    | (static_cast<std::uint16_t>(
                                           read_u8(high_offset))
                                       << 8U));
                            };
                        background_row_entries.push_back({
                            .camera_y = read_i16(0x0BU, 0x0CU),
                            .anchor_y = read_i16(0xA9U, 0xAAU),
                            .destination_word =
                                static_cast<std::uint16_t>(
                                    read_u8(0x2DU)
                                    | (static_cast<std::uint16_t>(
                                           read_u8(0x2EU))
                                       << 8U)),
                            .tile_source =
                                static_cast<std::uint16_t>(
                                    read_u8(0x2FU)
                                    | (static_cast<std::uint16_t>(
                                           read_u8(0x30U))
                                       << 8U)),
                            .attribute_source =
                                static_cast<std::uint16_t>(
                                    read_u8(0x2BU)
                                    | (static_cast<std::uint16_t>(
                                           read_u8(0x2CU))
                                       << 8U)),
                            .step_pixels = read_u8(0x26U),
                            .source_mpr = entry.registers.mpr[4],
                            .attribute_mpr = entry.registers.mpr[1],
                        });
                    },
            }),
            "background-row entry hook installs");
        check(
            runtime->set_bus_write_observer(
                [&vdc_write_sites](
                    const devils_crush::emulation::ObservedBusWrite& write) {
                    if (write.physical_bank == 0xFFU
                        && write.logical_address <= 0x0003U
                        && write.instruction.physical_bank == 0x00U
                        && write.instruction.logical_pc >= 0xF400U
                        && write.instruction.logical_pc <= 0xF434U) {
                        const auto site = std::pair{
                            write.instruction.physical_bank,
                            write.instruction.logical_pc,
                        };
                        ++vdc_write_sites[site];
                    }
                }),
            "VDC bus-write observer installs");
        check(
            runtime->install_routine_call_observer({
                .routine = {
                    .physical_bank = 0x00U,
                    .logical_pc = 0xF400U,
                },
                .callback =
                    [&background_row_calls](
                        const devils_crush::emulation::RoutineCall& call) {
                        background_row_calls.push_back(call);
                    },
            }),
            "background-row call observer installs");
    } else {
        check(
            runtime->install_execution_hook({
                .routine = {
                    .physical_bank = 0x00U,
                    .logical_pc = 0xE41FU,
                },
                .mode = devils_crush::emulation::HookMode::observe,
                .callback =
                    [&runtime, &vram_stream_entries](
                        const devils_crush::emulation::RoutineEntry&
                            entry) {
                        const auto work_ram = runtime->export_work_ram();
                        const auto read_word =
                            [&work_ram](
                                const std::size_t low_offset) {
                                return static_cast<std::uint16_t>(
                                    std::to_integer<std::uint8_t>(
                                        work_ram[low_offset])
                                    | (static_cast<std::uint16_t>(
                                           std::to_integer<std::uint8_t>(
                                               work_ram[
                                                   low_offset + 1U]))
                                       << 8U));
                            };
                        const auto source = read_word(0x2FU);
                        vram_stream_entries.push_back({
                            .frame_number = entry.frame_number,
                            .absolute_master_cycle =
                                entry.absolute_master_cycle,
                            .destination_word = read_word(0x2DU),
                            .source_address = source,
                            .source_mpr =
                                entry.registers.mpr[
                                    static_cast<std::size_t>(
                                        source >> 13U)],
                            .actor_slot = entry.registers.x,
                            .actor_appearance =
                                std::to_integer<std::uint8_t>(
                                    work_ram[
                                        0x1900U
                                        + entry.registers.x]),
                            .actor_phase =
                                std::to_integer<std::uint8_t>(
                                    work_ram[
                                        0x1980U
                                        + entry.registers.x]),
                            .loader_descriptor = read_word(0x35U),
                            .pattern_base = read_word(0x4FU),
                            .mpr = entry.registers.mpr,
                        });
                    },
            }),
            "VRAM stream entry hook installs");
        check(
            runtime->install_routine_call_observer({
                .routine = {
                    .physical_bank = 0x00U,
                    .logical_pc = 0xE41FU,
                },
                .callback =
                    [&vram_stream_entries](
                        const devils_crush::emulation::RoutineCall&
                            call) {
                        const auto entry = std::ranges::find_if(
                            vram_stream_entries,
                            [&call](const VramStreamEntry& candidate) {
                                return candidate.absolute_master_cycle
                                    == call.entry.absolute_master_cycle;
                            });
                        if (entry != vram_stream_entries.end()) {
                            entry->caller_pc = call.caller_pc;
                        }
                    },
            }),
            "VRAM stream call observer installs");
        check(
            runtime->install_execution_hook({
                .routine = {
                    .physical_bank = 0x00U,
                    .logical_pc = 0xF768U,
                },
                .mode = devils_crush::emulation::HookMode::observe,
                .callback =
                    [&runtime, &resident_pattern_loads](
                        const devils_crush::emulation::RoutineEntry&
                            entry) {
                        const auto work_ram = runtime->export_work_ram();
                        const auto slot =
                            static_cast<std::size_t>(entry.registers.x);
                        resident_pattern_loads.push_back({
                            .frame_number = entry.frame_number,
                            .selector = entry.registers.accumulator,
                            .actor_slot = entry.registers.x,
                            .actor_appearance =
                                slot
                                        < devils_crush::cabinet::
                                              kActorSlotCount
                                    ? std::to_integer<std::uint8_t>(
                                          work_ram[0x1900U + slot])
                                    : std::uint8_t{0},
                            .actor_phase =
                                slot
                                        < devils_crush::cabinet::
                                              kActorSlotCount
                                    ? std::to_integer<std::uint8_t>(
                                          work_ram[0x1980U + slot])
                                    : std::uint8_t{0},
                        });
                    },
            }),
            "resident pattern-loader hook installs");
        check(
            runtime->set_bus_write_observer(
                [&selected_vdc_register,
                 &vdc_memory_write_address,
                 &vdc_memory_write_low,
                 &sat_writer_sites,
                 &sprite_region_writer_sites](
                    const devils_crush::emulation::ObservedBusWrite&
                        write) {
                    if (write.physical_bank != 0xFFU
                        || write.logical_address > 0x0003U) {
                        return;
                    }
                    if (write.logical_address == 0x0000U) {
                        selected_vdc_register =
                            static_cast<std::uint8_t>(
                                write.value & 0x1FU);
                    } else if (write.logical_address == 0x0002U) {
                        if (selected_vdc_register == 0U) {
                            vdc_memory_write_address =
                                static_cast<std::uint16_t>(
                                    (vdc_memory_write_address
                                     & 0xFF00U)
                                    | write.value);
                        } else if (selected_vdc_register == 2U) {
                            vdc_memory_write_low = write.value;
                        }
                    } else if (write.logical_address == 0x0003U) {
                        if (selected_vdc_register == 0U) {
                            vdc_memory_write_address =
                                static_cast<std::uint16_t>(
                                    (vdc_memory_write_address
                                     & 0x00FFU)
                                    | (static_cast<std::uint16_t>(
                                           write.value)
                                       << 8U));
                        } else if (selected_vdc_register == 2U) {
                            static_cast<void>(vdc_memory_write_low);
                            if (write.frame_number == 1200U
                                && vdc_memory_write_address
                                    >= 0x7800U
                                && vdc_memory_write_address
                                    <= 0x78FFU) {
                                ++sat_writer_sites[{
                                    write.instruction.physical_bank,
                                    write.instruction.logical_pc,
                                }];
                            }
                            if (write.frame_number > 1200U
                                && write.frame_number <= 1600U
                                && vdc_memory_write_address
                                    >= 0x4000U
                                && vdc_memory_write_address
                                    < 0x7800U) {
                                ++sprite_region_writer_sites[{
                                    write.instruction.physical_bank,
                                    write.instruction.logical_pc,
                                }];
                            }
                            ++vdc_memory_write_address;
                        }
                    }
                }),
            "SAT staging writer observer installs");
    }

    constexpr std::array capture_frames{
        900,
        1200,
        1500,
        1600,
        1800,
        2100,
        2400,
        2700,
        3000,
        3300,
        3600,
    };
    std::size_t capture_index = 0;
    devils_crush::render::PersistentActorSpriteCache
        persistent_actor_sprites;
    bool observed_fixed_actor_visual = false;
    bool rendered_off_camera_actor_visual = false;
    std::array<std::pair<int, int>, 8>
        frame_1600_sorcerer_positions{};
    bool captured_frame_1600_sorcerer_positions = false;
    std::vector<std::uint16_t> frame_1200_vram;
    std::vector<std::uint16_t> frame_1600_vram;
    std::vector<devils_crush::render::FixedActorSprite>
        frame_1200_flipper_sprites;
    std::vector<devils_crush::render::FixedActorSprite>
        frame_1600_flipper_sprites;
    std::vector<devils_crush::render::FixedActorSprite>
        frame_1800_flipper_sprites;
    std::vector<devils_crush::render::FixedActorSprite>
        frame_1200_dynamic_enemy_sprites;
    std::vector<devils_crush::render::FixedActorSprite>
        frame_1600_dynamic_enemy_sprites;
    std::vector<devils_crush::render::FixedActorSprite>
        frame_1800_dynamic_enemy_sprites;
    std::optional<DynamicEnemyCensus>
        frame_1200_dynamic_enemy_census;
    devils_crush::render::ArgbImage animation_learning_table{
        .width = devils_crush::cabinet::kTableWorldWidth,
        .height = devils_crush::cabinet::kTableWorldHeight,
        .pixels = std::vector<std::uint32_t>(
            static_cast<std::size_t>(
                devils_crush::cabinet::kTableWorldWidth)
                * static_cast<std::size_t>(
                    devils_crush::cabinet::kTableWorldHeight),
            0xFF000000U),
    };
    bool animation_cache_updates_valid = true;
    std::size_t restored_animation_phases = 0;
    std::size_t restored_animated_actor_phases = 0;
    std::size_t presented_sorcerer_animation_phases = 0;
    std::set<std::uint8_t> presented_sorcerer_phase_values;
    devils_crush::render::PersistentActorOverlayStats stale_totals{};
    std::uint32_t camera_skew_zero = 0;
    int camera_skew_maximum = 0;
    devils_crush::emulation::EmulatedFrame frame;
    for (int frame_number = 1; frame_number <= capture_frames.back();
        ++frame_number) {
        frame = runtime->run_frame(input_for_frame(frame_number));
        if (frame_number >= 874) {
            const auto learning_snapshot =
                runtime->export_video_memory();
            const auto learning_work_ram =
                runtime->export_work_ram();
            if (!learning_snapshot
                || learning_work_ram.size() <= 0x0CU) {
                animation_cache_updates_valid = false;
            } else {
                const auto learning_camera_y =
                    static_cast<std::int16_t>(
                        static_cast<std::uint16_t>(
                            std::to_integer<std::uint8_t>(
                                learning_work_ram[0x0BU]))
                        | (static_cast<std::uint16_t>(
                               std::to_integer<std::uint8_t>(
                                   learning_work_ram[0x0CU]))
                           << 8U));
                {
                    // The vblank-latched scroll register settles exactly 32
                    // rows below the post-frame work-RAM viewport. Any other
                    // residue is one frame of camera motion, which is the
                    // amount the live SAT is displaced from the retained
                    // actors unless it is translated in its own time base.
                    const int top = 480 + static_cast<int>(
                        learning_camera_y);
                    int skew = (static_cast<int>(
                        learning_snapshot->background_y_scroll)
                        - 32 - top) % 256;
                    if (skew > 127) {
                        skew -= 256;
                    }
                    if (skew < -128) {
                        skew += 256;
                    }
                    camera_skew_zero += skew == 0 ? 1U : 0U;
                    camera_skew_maximum = std::max(
                        camera_skew_maximum,
                        skew < 0 ? -skew : skew);
                }
                const auto learning_stats =
                    persistent_actor_sprites.
                        update_and_composite(
                            animation_learning_table,
                            {
                                .vram_words =
                                    learning_snapshot->vram_words,
                                .sat_words =
                                    learning_snapshot->sat_words,
                                .palette_entries =
                                    learning_snapshot->
                                        palette_entries,
                                .memory_width_register =
                                    learning_snapshot->
                                        memory_width_register,
                            },
                            learning_work_ram,
                            480
                                + static_cast<int>(
                                    learning_camera_y),
                            0,
                            30,
                            208,
                            {},
                            rom_bytes);
                animation_cache_updates_valid =
                    animation_cache_updates_valid
                    && learning_stats.has_value();
                if (learning_stats) {
                    stale_totals.stale_no_descriptor +=
                        learning_stats->stale_no_descriptor;
                    stale_totals.stale_no_metadata +=
                        learning_stats->stale_no_metadata;
                    stale_totals.stale_no_metadata_hidden +=
                        learning_stats->stale_no_metadata_hidden;
                    stale_totals.stale_unusable_patterns +=
                        learning_stats->stale_unusable_patterns;
                    stale_totals.stale_resolve_failed +=
                        learning_stats->stale_resolve_failed;
                    stale_totals.resolved_sorcerer_phase_variants +=
                        learning_stats->resolved_sorcerer_phase_variants;
                    stale_totals.missing_phase_variants +=
                        learning_stats->missing_phase_variants;
                    stale_totals.static_phase_fallbacks +=
                        learning_stats->static_phase_fallbacks;
                    restored_animation_phases +=
                        learning_stats->
                            restored_phase_variants;
                    restored_animated_actor_phases +=
                        learning_stats->
                            restored_animated_phase_variants;
                    const auto presented_sorcerers =
                        learning_stats->
                            restored_sorcerer_phase_variants
                        + learning_stats->
                            resolved_sorcerer_phase_variants;
                    if (presented_sorcerers != 0U) {
                        presented_sorcerer_animation_phases +=
                            presented_sorcerers;
                        for (std::size_t slot = 0x17U;
                             slot <= 0x1EU;
                             ++slot) {
                            presented_sorcerer_phase_values.insert(
                                std::to_integer<std::uint8_t>(
                                    learning_work_ram[
                                        0x1980U + slot]));
                        }
                    }
                }
            }
        }
        if (frame_number == capture_frames.back()) {
            // The scroll register must agree with the work-RAM camera
            // whenever the camera is at rest, which is what makes 32 the
            // real alignment constant rather than a fitted offset. It must
            // also disagree while the camera moves, because that residue is
            // exactly the displacement the host now corrects for.
            check(
                camera_skew_zero > 2000U,
                "scroll register agrees with the work-RAM camera at rest");
            check(
                camera_skew_maximum > 0 && camera_skew_maximum < 32,
                "scroll register lags the work-RAM camera by a bounded "
                "amount while scrolling");
            // An off-camera actor whose phase moved must either resolve new
            // artwork or be cleared. Every path that silently keeps the old
            // artwork leaves a frozen sprite riding live coordinates, which
            // is what hidden mechanisms did before they were suppressed.
            check(
                stale_totals.stale_no_descriptor == 0U
                    && stale_totals.stale_no_metadata == 0U
                    && stale_totals.stale_unusable_patterns == 0U
                    && stale_totals.stale_resolve_failed == 0U,
                "no off-camera actor keeps stale artwork after its phase "
                "moves");
        }
        if (capture_index >= capture_frames.size()
            || frame_number != capture_frames[capture_index]) {
            continue;
        }
        ++capture_index;

        const auto snapshot = runtime->export_video_memory();
        check(snapshot.has_value(), "live VDC/VCE snapshot is available");
        if (!snapshot) {
            return EXIT_FAILURE;
        }
        if (frame_number == 1200) {
            frame_1200_vram = snapshot->vram_words;
        } else if (frame_number == 1600) {
            frame_1600_vram = snapshot->vram_words;
        }
        check(
            snapshot->vram_words.size() == 32768U,
            "video snapshot copies all base VDC VRAM words");
        check(
            snapshot->sat_words.size() == 256U,
            "video snapshot copies the internal post-DMA SAT");
        check(
            snapshot->palette_entries.size() == 512U,
            "video snapshot copies all VCE palette entries");
        check(
            snapshot->bat_width_tiles() == 32
                || snapshot->bat_width_tiles() == 64
                || snapshot->bat_width_tiles() == 128,
            "VDC MWR selects a legal BAT width");
        check(
            snapshot->bat_height_tiles() == 32
                || snapshot->bat_height_tiles() == 64,
            "VDC MWR selects a legal BAT height");

        const auto background =
            devils_crush::render::decode_pce_background({
                .vram_words = snapshot->vram_words,
                .palette_entries = snapshot->palette_entries,
                .bat_width_tiles = snapshot->bat_width_tiles(),
                .bat_height_tiles = snapshot->bat_height_tiles(),
            });
        check(
            background && background->valid(),
            "live table BAT, tiles, and palette decode");

        const auto vram_digest = devils_crush::rom::compute_digests(
            as_bytes<std::uint16_t>(snapshot->vram_words));
        const auto sprite_region_digest =
            devils_crush::rom::compute_digests(
                as_bytes<std::uint16_t>(
                    std::span<const std::uint16_t>(
                        snapshot->vram_words)
                        .subspan(0x4000U, 0x3800U)));
        const auto sat_digest = devils_crush::rom::compute_digests(
            as_bytes<std::uint16_t>(snapshot->sat_words));
        const auto palette_digest = devils_crush::rom::compute_digests(
            as_bytes<std::uint16_t>(snapshot->palette_entries));
        const auto background_digest = background
            ? devils_crush::rom::compute_digests(
                  as_bytes<std::uint32_t>(background->pixels))
            : devils_crush::rom::Digests{};
        const auto work_ram = runtime->export_work_ram();
        const auto camera_low =
            std::to_integer<std::uint8_t>(work_ram[0x0BU]);
        const auto camera_high =
            std::to_integer<std::uint8_t>(work_ram[0x0CU]);
        const auto camera_y = static_cast<std::int16_t>(
            static_cast<std::uint16_t>(camera_low)
            | (static_cast<std::uint16_t>(camera_high) << 8U));
        const auto settled_layer = frame_number >= 1200
            ? decode_stitched_table_layer(
                  rom_bytes,
                  work_ram,
                  *snapshot)
            : std::nullopt;
        const auto settled_table = settled_layer
            ? std::optional<devils_crush::render::ArgbImage>{
                  settled_layer->image}
            : std::nullopt;
        const auto table_comparison = settled_table
            ? compare_table_to_composite(
                  *settled_table,
                  480 + static_cast<int>(camera_y),
                  frame.framebuffer)
            : std::nullopt;
        int best_background_y_delta = 0;
        std::size_t best_background_matches = 0;
        if (settled_table) {
            for (int delta = -32; delta <= 32; ++delta) {
                const auto comparison =
                    compare_table_to_composite(
                        *settled_table,
                        480 + static_cast<int>(camera_y) + delta,
                        frame.framebuffer);
                if (comparison
                    && comparison->matching_pixels
                        > best_background_matches) {
                    best_background_matches =
                        comparison->matching_pixels;
                    best_background_y_delta = delta;
                }
            }
        }
        auto sprite_table = settled_table;
        const auto sprite_stats = sprite_table
            ? devils_crush::render::composite_pce_sprites(
                  *sprite_table,
                  {
                      .vram_words = snapshot->vram_words,
                      .sat_words = snapshot->sat_words,
                      .palette_entries = snapshot->palette_entries,
                      .memory_width_register =
                          snapshot->memory_width_register,
                  },
                  480 + static_cast<int>(camera_y),
                  0,
                  30,
                  208,
                  settled_layer->opaque_pixels)
            : std::nullopt;
        const auto sprite_comparison = sprite_table
            ? compare_table_to_composite(
                  *sprite_table,
                  480 + static_cast<int>(camera_y),
                  frame.framebuffer)
            : std::nullopt;
        const auto fixed_actor_sprites =
            frame_number >= 1200
            ? devils_crush::render::decode_fixed_actor_sprites(
                  rom_bytes,
                  work_ram)
            : std::nullopt;
        const auto fixed_actor_comparison = fixed_actor_sprites
            ? compare_fixed_actors_to_sat(
                  *fixed_actor_sprites,
                  work_ram,
                  snapshot->sat_words,
                  480 + static_cast<int>(camera_y))
            : std::nullopt;
        const auto ball_and_flipper_sprites =
            frame_number >= 1200
            ? devils_crush::render::
                  decode_ball_and_flipper_sprites(
                      rom_bytes,
                      work_ram)
            : std::nullopt;
        const auto ball_and_flipper_comparison =
            ball_and_flipper_sprites
            ? compare_ball_and_flippers_to_sat(
                  *ball_and_flipper_sprites,
                  work_ram,
                  snapshot->sat_words,
                  480 + static_cast<int>(camera_y))
            : std::nullopt;
        const auto ball_and_flipper_seed_classification =
            frame_number == 1200 && ball_and_flipper_sprites
                && pattern_atlas
            ? classify_fixed_actor_seeds(
                  *pattern_atlas,
                  *ball_and_flipper_sprites,
                  work_ram)
            : std::nullopt;
        const auto dynamic_actor_sprites =
            frame_number >= 1200
            ? devils_crush::render::decode_dynamic_actor_sprites(
                  rom_bytes,
                  work_ram)
            : std::nullopt;
        const auto dynamic_actor_comparison =
            dynamic_actor_sprites
            ? compare_dynamic_actors_to_sat(
                  *dynamic_actor_sprites,
                  work_ram,
                  snapshot->sat_words,
                  480 + static_cast<int>(camera_y))
            : std::nullopt;
        const auto dynamic_actor_seed_classification =
            frame_number == 1200 && dynamic_actor_sprites
                && pattern_atlas
            ? classify_fixed_actor_seeds(
                  *pattern_atlas,
                  *dynamic_actor_sprites,
                  work_ram)
            : std::nullopt;
        const auto table_world_effect_census =
            frame_number == 2100
            ? census_table_world_effects(work_ram)
            : std::nullopt;
        std::vector<devils_crush::render::FixedActorSprite>
            table_world_effect_sprites;
        if (frame_number == 2100 && dynamic_actor_sprites
            && table_world_effect_census) {
            std::ranges::copy_if(
                *dynamic_actor_sprites,
                std::back_inserter(table_world_effect_sprites),
                [&table_world_effect_census](
                    const devils_crush::render::
                        FixedActorSprite& sprite) {
                    return table_world_effect_census->slots.contains(
                        sprite.actor_slot);
                });
        }
        const auto table_world_effect_seed_classification =
            frame_number == 2100
                && !table_world_effect_sprites.empty()
                && pattern_atlas
            ? classify_fixed_actor_seeds(
                  *pattern_atlas,
                  table_world_effect_sprites,
                  work_ram)
            : std::nullopt;
        if (frame_number >= 1200) {
            check(
                dynamic_actor_sprites
                    && dynamic_actor_comparison
                    && dynamic_actor_comparison->matched_groups
                        == dynamic_actor_comparison->eligible_groups,
                "every active dynamic descriptor group matches the copied SAT");
        }
        if (dynamic_actor_sprites
            && (frame_number == 1200 || frame_number == 1600
                || frame_number == 1800)) {
            const auto retained_sprites =
                retained_dynamic_enemy_sprites(
                    *dynamic_actor_sprites,
                    work_ram);
            check(
                retained_sprites.has_value(),
                "dynamic enemy families decode for animation tracking");
            if (retained_sprites) {
                auto* destination = frame_number == 1200
                    ? &frame_1200_dynamic_enemy_sprites
                    : frame_number == 1600
                        ? &frame_1600_dynamic_enemy_sprites
                        : &frame_1800_dynamic_enemy_sprites;
                *destination = *retained_sprites;
            }
        }
        if (frame_number == 1200) {
            frame_1200_dynamic_enemy_census =
                census_dynamic_enemies(work_ram);
        }
        if (ball_and_flipper_sprites
            && (frame_number == 1200 || frame_number == 1600
                || frame_number == 1800)) {
            auto* destination = frame_number == 1200
                ? &frame_1200_flipper_sprites
                : frame_number == 1600
                    ? &frame_1600_flipper_sprites
                    : &frame_1800_flipper_sprites;
            std::ranges::copy_if(
                *ball_and_flipper_sprites,
                std::back_inserter(*destination),
                [](const devils_crush::render::
                       FixedActorSprite& sprite) {
                    return sprite.actor_slot
                        >= devils_crush::cabinet::
                               kFirstFlipperControlSlot;
                });
        }
        const auto resident_fixed_actor_classification =
            frame_number == 1200 && fixed_actor_sprites
                && pattern_atlas
                && !pattern_atlas->resident_uploads.empty()
            ? classify_resident_fixed_actors(
                  *pattern_atlas,
                  *fixed_actor_sprites,
                  work_ram)
            : std::nullopt;
        const auto fixed_actor_seed_classification =
            frame_number == 1200 && fixed_actor_sprites
                && pattern_atlas
            ? classify_fixed_actor_seeds(
                  *pattern_atlas,
                  *fixed_actor_sprites,
                  work_ram)
            : std::nullopt;
        const auto fixed_actor_atlas_comparison = pattern_atlas
            ? compare_fixed_actor_atlas_to_vram(
                  *pattern_atlas,
                  work_ram,
                  snapshot->vram_words)
            : std::nullopt;
        auto persistent_table = settled_table;
        const auto persistent_stats = persistent_table
            ? persistent_actor_sprites.update_and_composite(
                  *persistent_table,
                  {
                      .vram_words = snapshot->vram_words,
                      .sat_words = snapshot->sat_words,
                      .palette_entries = snapshot->palette_entries,
                      .memory_width_register =
                          snapshot->memory_width_register,
                  },
                  work_ram,
                  480 + static_cast<int>(camera_y),
                  0,
                  30,
                  208,
                  settled_layer->opaque_pixels,
                  rom_bytes)
            : std::nullopt;
        if (persistent_table && persistent_stats) {
            static_cast<void>(
                devils_crush::render::composite_pce_sprites(
                    *persistent_table,
                    {
                        .vram_words = snapshot->vram_words,
                        .sat_words = snapshot->sat_words,
                        .palette_entries =
                            snapshot->palette_entries,
                        .memory_width_register =
                            snapshot->memory_width_register,
                    },
                    480 + static_cast<int>(camera_y),
                    0,
                    30,
                    208,
                    settled_layer->opaque_pixels));
        }
        std::optional<
            devils_crush::render::PersistentActorOverlayStats>
            off_camera_effect_stats;
        devils_crush::rom::Digests off_camera_effect_digest;
        if (frame_number == 2100 && settled_table
            && persistent_stats) {
            auto effect_probe_cache = persistent_actor_sprites;
            auto effect_probe_table = *settled_table;
            off_camera_effect_stats =
                effect_probe_cache.update_and_composite(
                    effect_probe_table,
                    {
                        .vram_words = snapshot->vram_words,
                        .sat_words = snapshot->sat_words,
                        .palette_entries =
                            snapshot->palette_entries,
                        .memory_width_register =
                            snapshot->memory_width_register,
                    },
                    work_ram,
                    devils_crush::cabinet::kTableWorldTop,
                    0,
                    30,
                    208,
                    {},
                    rom_bytes);
            const auto accepted_effect_probe =
                devils_crush::render::crop_image(
                    effect_probe_table,
                    0,
                    devils_crush::cabinet::kTableWorldTop,
                    devils_crush::cabinet::kTableWorldWidth,
                    devils_crush::cabinet::kTableWorldHeight);
            if (accepted_effect_probe) {
                off_camera_effect_digest =
                    devils_crush::rom::compute_digests(
                        as_bytes<std::uint32_t>(
                            accepted_effect_probe->pixels));
            }
        }
        const auto accepted_persistent_table = persistent_table
            ? devils_crush::render::crop_image(
                  *persistent_table,
                  0,
                  devils_crush::cabinet::kTableWorldTop,
                  devils_crush::cabinet::kTableWorldWidth,
                  devils_crush::cabinet::kTableWorldHeight)
            : std::nullopt;
        const auto persistent_digest = accepted_persistent_table
            ? devils_crush::rom::compute_digests(
                  as_bytes<std::uint32_t>(
                      accepted_persistent_table->pixels))
            : devils_crush::rom::Digests{};
        if (persistent_stats) {
            observed_fixed_actor_visual =
                observed_fixed_actor_visual
                || persistent_stats->cached_fixed_actors != 0U;
            rendered_off_camera_actor_visual =
                rendered_off_camera_actor_visual
                || persistent_stats->rendered_retained_fixed_actors
                    != 0U;
        }
        if (frame_number >= 1200) {
            check(
                settled_table && table_comparison
                    && fixed_actor_sprites
                    && fixed_actor_comparison
                    && fixed_actor_comparison->matched_groups
                        == fixed_actor_comparison->eligible_groups,
                "post-fade table and fixed actor visuals decode");
            check(
                ball_and_flipper_sprites
                    && ball_and_flipper_comparison
                    && ball_and_flipper_comparison->matched_groups
                        == ball_and_flipper_comparison->
                               eligible_groups,
                "ball and active flippers match copied SAT groups");
            check(
                persistent_stats
                    && persistent_stats->cached_flipper_actors == 6U,
                "all six flippers remain resolved across camera regions");
        }
        if (frame_number == 1600 || frame_number == 1800) {
            const auto actor_state =
                devils_crush::cabinet::decode_table_actor_state(
                    work_ram);
            std::array<std::pair<int, int>, 8>
                sorcerer_positions{};
            if (actor_state) {
                for (std::size_t index = 0;
                     index < sorcerer_positions.size();
                     ++index) {
                    const auto& actor =
                        (*actor_state)[0x17U + index];
                    sorcerer_positions[index] = {
                        actor.world_x,
                        actor.world_y,
                    };
                }
            }
            if (frame_number == 1600 && actor_state) {
                frame_1600_sorcerer_positions =
                    sorcerer_positions;
                captured_frame_1600_sorcerer_positions = true;
            } else if (frame_number == 1800) {
                check(
                    actor_state
                        && captured_frame_1600_sorcerer_positions
                        && sorcerer_positions
                            != frame_1600_sorcerer_positions,
                    "off-camera Sorcerer positions continue to advance");
            }
        }
        if (frame_number == 2100) {
            check(
                table_world_effect_census
                    && table_world_effect_census->impact_bursts == 1U
                    && table_world_effect_census->
                           camera_active_impact_bursts
                        == 1U
                    && table_world_effect_census->slots
                        == std::set<std::uint8_t>{0x71U}
                    && table_world_effect_census->appearances
                        == std::set<std::uint8_t>{0x10U}
                    && table_world_effect_census->dispatches
                        == std::set<std::uint16_t>{0x42B0U}
                    && table_world_effect_census->phases
                        == std::set<std::uint8_t>{5U}
                    && table_world_effect_sprites.size() == 2U
                    && table_world_effect_seed_classification
                    && table_world_effect_seed_classification->
                           actor_groups
                        == 1U
                    && table_world_effect_seed_classification->
                           stream_owned_groups
                        == 0U
                    && table_world_effect_seed_classification->
                           upload_disjoint_groups
                        == 1U
                    && table_world_effect_seed_classification->
                           unresolved_groups
                        == 0U,
                "impact burst has exact table-world metadata and a resolved sprite source");
            check(
                persistent_stats
                    && persistent_stats->
                           observed_table_world_effects
                        == 1U
                    && persistent_stats->
                           cached_table_world_effects
                        == 1U
                    && off_camera_effect_stats
                    && off_camera_effect_stats->
                           rendered_table_world_effects
                        == 1U
                    && off_camera_effect_stats->
                           rendered_retained_table_world_effects
                        == 1U
                    && off_camera_effect_stats->opaque_pixel_writes
                        == 25165U
                    && off_camera_effect_digest.sha256
                        == "f075afa29ad327d70535fcbc53123e7f"
                           "e7aae1b6be0322cda0ccf536d78c7c60",
                "impact burst remains visible when probed outside the copied camera");
        } else if (frame_number == 2400
                   || frame_number == 2700) {
            check(
                persistent_stats
                    && persistent_stats->
                           cached_table_world_effects
                        == 0U
                    && persistent_stats->
                           rendered_table_world_effects
                        == 0U,
                "released impact burst leaves no retained table trail");
        }
        if (table_output) {
            const auto live_background =
                background
                ? devils_crush::render::crop_image(
                      *background,
                      0,
                      0,
                      256,
                      240)
                : std::nullopt;
            const auto live_composite =
                copy_framebuffer(frame.framebuffer);
            const auto prefix =
                "-frame" + std::to_string(frame_number);
            check(
                live_background
                    && write_bmp(
                        diagnostic_path(
                            *table_output,
                            prefix + "-background"),
                        *live_background),
                "captured background BMP writes");
            check(
                live_composite
                    && write_bmp(
                        diagnostic_path(
                            *table_output,
                            prefix + "-composite"),
                        *live_composite),
                "captured composite BMP writes");
            if (settled_table) {
                check(
                    write_bmp(
                        diagnostic_path(
                            *table_output,
                            prefix + "-table"),
                        *settled_table),
                    "captured stitched table BMP writes");
            }
            if (accepted_persistent_table) {
                check(
                    write_bmp(
                        diagnostic_path(
                            *table_output,
                            prefix + "-persistent"),
                        *accepted_persistent_table),
                    "persistent full-table BMP writes");
            }
        }
        if (frame_number == 900) {
            const bool resident_pack_matches_live_vram =
                pattern_atlas
                && !pattern_atlas->resident_uploads.empty()
                && pattern_atlas->resident_uploads.front().
                       destination_word
                    + pattern_atlas->resident_uploads.front().
                          words.size()
                    <= snapshot->vram_words.size()
                && std::equal(
                    pattern_atlas->resident_uploads.front().
                        words.begin(),
                    pattern_atlas->resident_uploads.front().
                        words.end(),
                    snapshot->vram_words.begin()
                        + pattern_atlas->resident_uploads.front().
                              destination_word);
            check(
                resident_pack_matches_live_vram,
                "decoded F768 selector 0 matches live $7C00 VRAM");
            check(
                vram_digest.sha256
                    == "f452889188abdf446a4caf85f5aac51e3afa1cd8ccc1c86"
                       "ce6044562a0a7d900",
                "initial table VRAM snapshot matches its golden");
            check(
                palette_digest.sha256
                    == "be9d93e01ba3e5f26cfaaa6df5f6107bd05f41a8ec8f29"
                       "d6f8ff8b88e14f710f",
                "initial table palette snapshot matches its golden");
            check(
                background_digest.sha256
                    == "f812ec7fdba43453dbd1dc926fdb8adf2d4ef400d7631b8"
                       "ca7ebc49f18446c17",
                "initial decoded BAT snapshot matches its golden");

            const auto table = decode_stitched_table(
                rom_bytes,
                work_ram,
                *snapshot);
            check(
                table && table->width == 256 && table->height == 768,
                "three packed table regions decode to 256x768");
            if (table) {
                std::array<devils_crush::render::ArgbImage, 3>
                    table_regions;
                bool regions_valid = true;
                for (std::size_t region = 0;
                     region < table_regions.size();
                     ++region) {
                    const auto decoded_region =
                        devils_crush::render::crop_image(
                            *table,
                            0,
                            static_cast<int>(region * 256U),
                            256,
                            256);
                    if (!decoded_region) {
                        regions_valid = false;
                        break;
                    }
                    table_regions[region] = *decoded_region;
                }
                check(
                    regions_valid,
                    "upper, middle, and lower table regions split");
                if (regions_valid) {
                    const std::array placed_regions{
                        devils_crush::render::PlacedImage{
                            .image = &table_regions[0],
                            .world_x = 0,
                            .world_y = 0,
                        },
                        devils_crush::render::PlacedImage{
                            .image = &table_regions[1],
                            .world_x = 0,
                            .world_y = 256,
                        },
                        devils_crush::render::PlacedImage{
                            .image = &table_regions[2],
                            .world_x = 0,
                            .world_y = 512,
                        },
                    };
                    const auto stitched =
                        devils_crush::render::stitch_images(
                            placed_regions);
                    check(
                        stitched && stitched->world_x == 0
                            && stitched->world_y == 0
                            && stitched->image.pixels
                                == table->pixels,
                        "three table regions stitch at world origins");
                }

                const auto native_crop =
                    devils_crush::render::crop_image(
                        *table,
                        0,
                        480,
                        256,
                        208);
                const auto live_crop =
                    background
                    ? devils_crush::render::crop_image(
                          *background,
                          0,
                          0,
                          256,
                          208)
                    : std::nullopt;
                check(
                    native_crop && live_crop
                        && native_crop->pixels == live_crop->pixels,
                    "lower native playfield crop matches live BAT background");

                const auto native_with_panel =
                    devils_crush::render::crop_image(
                        *table,
                        0,
                        480,
                        256,
                        240);
                const auto live_with_panel =
                    background
                    ? devils_crush::render::crop_image(
                          *background,
                          0,
                          0,
                          256,
                          240)
                    : std::nullopt;
                if (native_with_panel && live_with_panel) {
                    std::size_t mismatch_count = 0;
                    int minimum_y = native_with_panel->height;
                    int maximum_y = -1;
                    for (int y = 0;
                         y < native_with_panel->height;
                         ++y) {
                        for (int x = 0;
                             x < native_with_panel->width;
                             ++x) {
                            const auto index =
                                static_cast<std::size_t>(y)
                                    * static_cast<std::size_t>(
                                        native_with_panel->width)
                                + static_cast<std::size_t>(x);
                            if (native_with_panel->pixels[index]
                                == live_with_panel->pixels[index]) {
                                continue;
                            }
                            ++mismatch_count;
                            minimum_y = std::min(minimum_y, y);
                            maximum_y = std::max(maximum_y, y);
                        }
                    }
                    check(
                        mismatch_count == 475U && minimum_y == 208
                            && maximum_y == 239,
                        "bottom 32 rows are the non-playfield score panel");
                }

                const auto table_digest =
                    devils_crush::rom::compute_digests(
                        as_bytes<std::uint32_t>(table->pixels));
                check(
                    table_digest.sha256
                        == "f0df872362bcc231674151eecdd402752d1bc18dd9e333e"
                           "5a95fb872dad54cee",
                    "stitched 256x768 table matches its golden");
                if (table_output) {
                    check(
                        write_bmp(*table_output, *table),
                        "stitched table BMP writes");
                }
                std::cout << "table=256x768"
                          << " table_sha256="
                          << table_digest.sha256;
                if (native_crop) {
                    const auto crop_digest =
                        devils_crush::rom::compute_digests(
                            as_bytes<std::uint32_t>(
                                native_crop->pixels));
                    check(
                        crop_digest.sha256
                            == "da841334896409730877e48a19d89cfa4bacfde153b3157"
                               "e31c2b126c03bbb2d",
                        "lower 256x208 playfield crop matches its golden");
                    std::cout << " lower_crop_sha256="
                              << crop_digest.sha256;
                }
                std::cout << '\n';
            }
        }
        std::cout << "frame=" << frame_number
                  << " mwr=" << snapshot->memory_width_register
                  << " bxr=" << snapshot->background_x_scroll
                  << " byr=" << snapshot->background_y_scroll
                  << " camera_y=" << camera_y
                  << " bat=" << snapshot->bat_width_tiles() << 'x'
                  << snapshot->bat_height_tiles()
                  << " vram_sha256=" << vram_digest.sha256
                  << " sprite_region_sha256="
                  << sprite_region_digest.sha256
                  << " sat_sha256=" << sat_digest.sha256
                  << " palette_sha256=" << palette_digest.sha256
                  << " background_sha256="
                  << background_digest.sha256;
        if (fixed_actor_atlas_comparison) {
            std::cout << " atlas_vram_groups="
                      << fixed_actor_atlas_comparison->matched_groups
                      << '/'
                      << fixed_actor_atlas_comparison->eligible_groups;
        }
        if (accepted_persistent_table && persistent_stats) {
            std::cout << " persistent_sha256="
                      << persistent_digest.sha256
                      << " persistent_cached="
                      << persistent_stats->cached_fixed_actors
                      << " persistent_seeded="
                      << persistent_stats->seeded_fixed_actors
                      << " persistent_rendered_seeded="
                      << persistent_stats->
                             rendered_seeded_fixed_actors
                      << " persistent_rendered_flippers="
                      << persistent_stats->
                             rendered_flipper_actors
                      << " persistent_flipper_writes="
                      << persistent_stats->
                             flipper_opaque_pixel_writes;
        }
        if (frame_number == 2100
            && table_world_effect_census
            && table_world_effect_seed_classification
            && off_camera_effect_stats) {
            std::cout
                << " impact_slots=";
            for (const auto slot :
                 table_world_effect_census->slots) {
                std::cout << std::hex << std::uppercase
                          << static_cast<unsigned>(slot) << ',';
            }
            std::cout << " impact_phases=";
            for (const auto phase :
                 table_world_effect_census->phases) {
                std::cout << static_cast<unsigned>(phase) << ',';
            }
            std::cout << std::dec
                      << " impact_sprites="
                      << table_world_effect_sprites.size()
                      << " impact_stream_owned="
                      << table_world_effect_seed_classification->
                             stream_owned_groups
                      << " impact_disjoint="
                      << table_world_effect_seed_classification->
                             upload_disjoint_groups
                      << " impact_off_camera_writes="
                      << off_camera_effect_stats->opaque_pixel_writes
                      << " impact_probe_sha256="
                      << off_camera_effect_digest.sha256;
        }
        if (settled_table && table_comparison) {
            const auto table_digest =
                devils_crush::rom::compute_digests(
                    as_bytes<std::uint32_t>(settled_table->pixels));
            check(
                table_comparison->matching_pixels
                        + table_comparison->sprite_or_overlay_pixels
                    == 256U * 208U,
                "composite comparison covers the full playfield viewport");
            if (frame_number == 1200 || frame_number == 1600
                || frame_number == 1800) {
                check(
                    best_background_y_delta == 0
                        && best_background_matches
                            == table_comparison->matching_pixels,
                    "stitched background retains its exact vertical origin across launch and camera transitions");
            }
            if (frame_number == 1200) {
                const auto accepted_table =
                    devils_crush::render::crop_image(
                        *settled_table,
                        0,
                        20,
                        256,
                        668);
                const auto accepted_digest = accepted_table
                    ? devils_crush::rom::compute_digests(
                          as_bytes<std::uint32_t>(
                              accepted_table->pixels))
                    : devils_crush::rom::Digests{};
                check(
                    table_digest.sha256
                            == "e11ec46a715e311d584c19c4ddb78750b9e18e6a607f22d"
                               "b7d70de8dddf74a6f"
                        && accepted_digest.sha256
                            == "3d5afa6f1f633e23b837c1f3ec3e0a19c8f00ca87abf2a"
                               "7ff841f600dadc7e1e"
                        && table_comparison->matching_pixels == 44901U,
                    "settled lower-region source matches emulator output");
                check(
                    sat_digest.sha256
                            == "26ad08bcac982e81b34e02f899e16d790f148562daaa17e"
                               "5651b92e5f6248276"
                        && sprite_region_digest.sha256
                            == "f84447542add674e39a660d57bde50277ef414c6ee860a"
                               "b0e8c5d707e889edd3"
                        && sprite_stats
                        && sprite_stats->visible_sprites == 46U
                        && sprite_stats->opaque_pixel_writes == 9175U
                        && sprite_comparison
                        && sprite_comparison->matching_pixels == 51778U,
                    "copied SAT overlay matches the settled live composite");
                check(
                    fixed_actor_atlas_comparison
                        && fixed_actor_atlas_comparison->matched_groups
                            == 16U
                        && fixed_actor_atlas_comparison->eligible_groups
                            == 17U
                        && persistent_digest.sha256
                            == "45901880df0caad856496f1a4d1a0063132f144a424587"
                               "cb2fc3936f17501785"
                        && persistent_stats
                        && persistent_stats->cached_fixed_actors
                            == 81U
                        && persistent_stats->seeded_fixed_actors
                            == 52U
                        && persistent_stats->
                               rendered_seeded_fixed_actors
                            == 52U
                        && persistent_stats->opaque_pixel_writes
                            == 14918U
                        && persistent_stats->rendered_flipper_actors
                            == 4U
                        && persistent_stats->flipper_opaque_pixel_writes
                            != 0U,
                    "lower full-table composite includes fixed actors, all flipper pairs, and persistent enemies");
                if (resident_fixed_actor_classification) {
                    constexpr std::array<std::uint8_t, 9>
                        expected_fixed_appearances{
                            0x24U, 0x25U, 0x29U,
                            0x2AU, 0x2BU, 0x2CU,
                            0x37U, 0x41U, 0x43U,
                        };
                    constexpr std::array<std::uint8_t, 9>
                        expected_disjoint_appearances{
                            0x24U, 0x25U, 0x29U,
                            0x2AU, 0x2BU, 0x2CU,
                            0x37U, 0x41U, 0x43U,
                        };
                    check(
                        resident_fixed_actor_classification->
                                actor_groups
                                == 24U
                            && resident_fixed_actor_classification->
                                   covered_groups
                                == 0U
                            && resident_fixed_actor_classification->
                                   disjoint_groups
                                == 24U
                            && resident_fixed_actor_classification->
                                   appearances
                                == std::set<std::uint8_t>{
                                    expected_fixed_appearances.begin(),
                                    expected_fixed_appearances.end()}
                            && resident_fixed_actor_classification->
                                   disjoint_appearances
                                == std::set<std::uint8_t>{
                                    expected_disjoint_appearances.
                                        begin(),
                                    expected_disjoint_appearances.
                                        end()}
                            && resident_fixed_actor_classification->
                                   overlapping_appearances
                                == std::set<std::uint8_t>{},
                        "fixed actors classify independently of known pattern uploads");
                    std::cout
                        << " resident_fixed_groups="
                        << resident_fixed_actor_classification->
                               actor_groups
                        << " resident_covered_groups="
                        << resident_fixed_actor_classification->
                               covered_groups
                        << " disjoint_fixed_groups="
                        << resident_fixed_actor_classification->
                               disjoint_groups
                        << " resident_appearances=";
                    for (const auto appearance :
                         resident_fixed_actor_classification->
                             appearances) {
                        std::cout << std::hex << std::uppercase
                                  << static_cast<unsigned>(appearance)
                                  << ',';
                    }
                    std::cout << std::dec;
                }
                if (fixed_actor_seed_classification) {
                    check(
                        fixed_actor_seed_classification->
                                actor_groups
                                == 61U
                            && fixed_actor_seed_classification->
                                   stream_owned_groups
                                == 8U
                            && fixed_actor_seed_classification->
                                   upload_disjoint_groups
                                == 53U
                            && fixed_actor_seed_classification->
                                   unresolved_groups
                                == 0U,
                        "all non-hidden fixed actors have an exact pattern source");
                    std::cout
                        << " seed_fixed_groups="
                        << fixed_actor_seed_classification->
                               actor_groups
                        << " stream_owned_groups="
                        << fixed_actor_seed_classification->
                               stream_owned_groups
                        << " upload_disjoint_groups="
                        << fixed_actor_seed_classification->
                               upload_disjoint_groups
                        << " unresolved_seed_groups="
                        << fixed_actor_seed_classification->
                               unresolved_groups;
                }
            } else if (frame_number == 1600) {
                check(
                    table_digest.sha256
                            == "750ba5022e7604636f97107a374df21262ede4a5aa2ac1e"
                               "7cc64cb9b9decf25d"
                        && sprite_region_digest.sha256
                            == "c5f344d3130e20d40a37e5d055a557022290be8c87d97"
                               "df397068850fbf7155d"
                        && table_comparison->matching_pixels == 47768U,
                    "settled upper-middle source matches emulator output");
                check(
                    persistent_digest.sha256
                            == "de214ff8a7195a1e1d8a099a00889dcef3a2ed5df4d068"
                               "85fb3da0d2e68447e8"
                        && persistent_stats
                        && persistent_stats->cached_fixed_actors
                            == 81U
                        && persistent_stats->
                               rendered_seeded_fixed_actors
                            == 31U
                        && persistent_stats->rendered_flipper_actors
                            == 4U
                        && persistent_stats->flipper_opaque_pixel_writes
                            != 0U,
                    "upper-middle persistent composite matches its golden");
            } else if (frame_number == 1800) {
                check(
                    table_digest.sha256
                            == "910a3372d4c516f6076bff49556dfd289915014c1cf652a"
                               "8c8df921fa538bb44"
                        && sprite_region_digest.sha256
                            == "c5f344d3130e20d40a37e5d055a557022290be8c87d97"
                               "df397068850fbf7155d"
                        && table_comparison->matching_pixels == 49432U,
                    "settled middle-region source matches emulator output");
                check(
                    persistent_digest.sha256
                            == "b8680ea899a0ea273118d9349d9d2b51006d497f47f5e"
                               "1fb630b04159a4fe890"
                        && persistent_stats
                        && persistent_stats->cached_fixed_actors
                            == 78U
                        && persistent_stats->
                               rendered_seeded_fixed_actors
                            == 31U
                        && persistent_stats->rendered_flipper_actors
                            == 4U
                        && persistent_stats->flipper_opaque_pixel_writes
                            != 0U,
                    "middle persistent composite matches its golden");
            }
            if (frame_number == 1200) {
                const auto sat_source = std::search(
                    snapshot->vram_words.begin(),
                    snapshot->vram_words.end(),
                    snapshot->sat_words.begin(),
                    snapshot->sat_words.end());
                check(
                    sat_source != snapshot->vram_words.end()
                        && std::distance(
                               snapshot->vram_words.begin(),
                               sat_source)
                            == 0x7800,
                    "copied SAT matches the VRAM $7800 staging table");
                std::cout << " sat_vram_word="
                          << (sat_source == snapshot->vram_words.end()
                                  ? -1
                                  : static_cast<int>(
                                        std::distance(
                                            snapshot->vram_words.begin(),
                                            sat_source)));
                std::cout << " fixed_actor_sprites="
                          << (fixed_actor_sprites
                                  ? fixed_actor_sprites->size()
                                  : 0U);
                check(
                    fixed_actor_sprites
                        && fixed_actor_sprites->size() == 100U
                        && fixed_actor_comparison
                        && fixed_actor_comparison->matched_groups
                            == 23U
                        && fixed_actor_comparison->eligible_groups
                            == 23U,
                    "native fixed-actor descriptors match every active SAT group");
                check(
                    ball_and_flipper_sprites
                        && ball_and_flipper_sprites->size() == 13U
                        && ball_and_flipper_comparison
                        && ball_and_flipper_comparison->
                               matched_groups
                            == 5U
                        && ball_and_flipper_comparison->
                               eligible_groups
                            == 5U
                        && ball_and_flipper_seed_classification
                        && ball_and_flipper_seed_classification->
                               actor_groups
                            == 7U
                        && ball_and_flipper_seed_classification->
                               stream_owned_groups
                            == 0U
                        && ball_and_flipper_seed_classification->
                               upload_disjoint_groups
                            == 7U
                        && ball_and_flipper_seed_classification->
                               unresolved_groups
                            == 0U,
                    "ball and flipper descriptors have exact live and retained sources");
                check(
                    dynamic_actor_sprites
                        && dynamic_actor_sprites->size() == 14U
                        && dynamic_actor_comparison
                        && dynamic_actor_comparison->matched_groups
                            == 4U
                        && dynamic_actor_comparison->eligible_groups
                            == 4U
                        && dynamic_actor_seed_classification
                        && dynamic_actor_seed_classification->
                               actor_groups
                            == 14U
                        && dynamic_actor_seed_classification->
                               stream_owned_groups
                            == 0U
                        && dynamic_actor_seed_classification->
                               upload_disjoint_groups
                            == 14U
                        && dynamic_actor_seed_classification->
                               unresolved_groups
                            == 0U
                        && frame_1200_dynamic_enemy_census
                        && frame_1200_dynamic_enemy_census->
                               small_soldiers
                            == 10U
                        && frame_1200_dynamic_enemy_census->
                               tower_insects
                            == 4U,
                    "persistent dynamic enemies have exact live and retained sources");
                std::cout << " fixed_actor_sat_groups="
                          << fixed_actor_comparison->matched_groups
                          << '/'
                          << fixed_actor_comparison->eligible_groups;
                if (ball_and_flipper_comparison) {
                    std::cout << " ball_flipper_sat_groups="
                              << ball_and_flipper_comparison->
                                     matched_groups
                              << '/'
                              << ball_and_flipper_comparison->
                                     eligible_groups;
                }
                if (ball_and_flipper_seed_classification) {
                    std::cout
                        << " ball_flipper_groups="
                        << ball_and_flipper_seed_classification->
                               actor_groups
                        << " ball_flipper_stream_owned="
                        << ball_and_flipper_seed_classification->
                               stream_owned_groups
                        << " ball_flipper_disjoint="
                        << ball_and_flipper_seed_classification->
                               upload_disjoint_groups
                        << " ball_flipper_unresolved="
                        << ball_and_flipper_seed_classification->
                               unresolved_groups;
                }
                if (dynamic_actor_comparison) {
                    std::cout
                        << " dynamic_sat_groups="
                        << dynamic_actor_comparison->matched_groups
                        << '/'
                        << dynamic_actor_comparison->
                               eligible_groups
                        << " dynamic_sprites="
                        << (dynamic_actor_sprites
                                ? dynamic_actor_sprites->size()
                                : 0U);
                }
                if (dynamic_actor_seed_classification) {
                    std::cout
                        << " dynamic_groups="
                        << dynamic_actor_seed_classification->
                               actor_groups
                        << " dynamic_stream_owned="
                        << dynamic_actor_seed_classification->
                               stream_owned_groups
                        << " dynamic_disjoint="
                        << dynamic_actor_seed_classification->
                               upload_disjoint_groups
                        << " dynamic_unresolved="
                        << dynamic_actor_seed_classification->
                               unresolved_groups;
                }
                if (fixed_actor_atlas_comparison) {
                    std::cout << " fixed_actor_atlas_groups="
                              << fixed_actor_atlas_comparison->
                                     matched_groups
                              << '/'
                              << fixed_actor_atlas_comparison->
                                     eligible_groups;
                }
                std::cout << " persistent_table_sha256="
                          << persistent_digest.sha256;
                if (persistent_stats) {
                    std::cout << " cached_actors="
                              << persistent_stats->cached_fixed_actors
                              << " seeded_actors="
                              << persistent_stats->seeded_fixed_actors
                              << " rendered_retained_actors="
                              << persistent_stats->
                                     rendered_retained_fixed_actors
                              << " rendered_seeded_actors="
                      << persistent_stats->
                             rendered_seeded_fixed_actors
                      << " retained_opaque_writes="
                      << persistent_stats->opaque_pixel_writes
                      << " rendered_flippers="
                      << persistent_stats->rendered_flipper_actors
                      << " flipper_writes="
                      << persistent_stats->flipper_opaque_pixel_writes;
                }
            }
            std::cout
                << " table_world_y="
                << 480 + static_cast<int>(camera_y)
                << " best_background_y_delta="
                << best_background_y_delta
                << " best_background_matches="
                << best_background_matches
                << " table_sha256=" << table_digest.sha256
                << " composite_background_matches="
                << table_comparison->matching_pixels
                << " composite_overlay_pixels="
                << table_comparison->sprite_or_overlay_pixels;
            if (sprite_stats && sprite_comparison) {
                std::cout
                    << " sat_visible_sprites="
                    << sprite_stats->visible_sprites
                    << " sat_opaque_writes="
                    << sprite_stats->opaque_pixel_writes
                    << " sprite_composite_matches="
                    << sprite_comparison->matching_pixels;
            }
        }
        std::cout << '\n';
        if (!map_row_writer && frame_number == 1600) {
            check(
                runtime->remove_execution_hook({
                    .physical_bank = 0x00U,
                    .logical_pc = 0xE41FU,
                }),
                "bounded VRAM stream entry hook removes");
            check(
                runtime->remove_routine_call_observer({
                    .physical_bank = 0x00U,
                    .logical_pc = 0xE41FU,
                }),
                "bounded VRAM stream call observer removes");
            runtime->clear_bus_write_observer();
        }
    }
    check(
        frame.frame_number
            == static_cast<std::uint64_t>(capture_frames.back()),
        "table snapshot route reaches its final frame");
    check(
        capture_index == capture_frames.size(),
        "table snapshot route captures every checkpoint");
    check(
        animation_cache_updates_valid
            && restored_animation_phases != 0U
            && restored_animated_actor_phases != 0U,
        "app-like cabinet updates restore learned animation phases");
    std::cout
        << "Animation cache: restored="
        << restored_animation_phases
        << " animated=" << restored_animated_actor_phases
        << " sorcerer=" << presented_sorcerer_animation_phases
        << " sorcerer_phases=";
    for (const auto phase : presented_sorcerer_phase_values) {
        std::cout << static_cast<unsigned>(phase) << ',';
    }
    std::cout << '\n';
    check(
        presented_sorcerer_animation_phases != 0U
            && presented_sorcerer_phase_values.size() > 1U,
        "off-camera Sorcerers present multiple exact walking phases");
    check(
        observed_fixed_actor_visual,
        "natural route learns a fixed mechanism from the copied SAT");
    check(
        rendered_off_camera_actor_visual,
        "natural route preserves a learned mechanism outside the live camera");
    check(
        frame_1200_flipper_sprites.size() == 12U
            && frame_1600_flipper_sprites.size() == 12U
            && frame_1800_flipper_sprites.size() == 12U
            && (frame_1200_flipper_sprites
                    != frame_1600_flipper_sprites
                || frame_1600_flipper_sprites
                    != frame_1800_flipper_sprites),
        "all six flippers retain live animation-phase metadata");
    check(
        frame_1200_dynamic_enemy_sprites.size() == 14U
            && frame_1600_dynamic_enemy_sprites.size() == 14U
            && frame_1800_dynamic_enemy_sprites.size() == 14U
            && (frame_1200_dynamic_enemy_sprites
                    != frame_1600_dynamic_enemy_sprites
                || frame_1600_dynamic_enemy_sprites
                    != frame_1800_dynamic_enemy_sprites),
        "all fourteen persistent enemies retain live animation metadata");
    if (!map_row_writer) {
        constexpr std::array expected_resident_pattern_loads{
            ResidentPatternLoad{
                .frame_number = 874U,
                .selector = 0U,
                .actor_slot = 0U,
                .actor_appearance = 0U,
                .actor_phase = 0U,
            },
        };
        check(
            std::ranges::equal(
                resident_pattern_loads,
                expected_resident_pattern_loads),
            "normal-table initialization selects resident asset zero");
        std::cout << "Resident pattern loads:";
        for (const auto& resident_load : resident_pattern_loads) {
            std::cout << " {" << resident_load.frame_number
                      << ",selector="
                      << static_cast<unsigned>(
                             resident_load.selector)
                      << ",x="
                      << static_cast<unsigned>(
                             resident_load.actor_slot)
                      << ",appearance="
                      << static_cast<unsigned>(
                             resident_load.actor_appearance)
                      << ",phase="
                      << static_cast<unsigned>(
                             resident_load.actor_phase)
                      << '}';
        }
        std::cout << '\n';
        using SatWriterSite =
            std::pair<std::uint8_t, std::uint16_t>;
        const std::map<
            SatWriterSite,
            std::size_t>
            expected_sat_writer_sites{
                {SatWriterSite{
                     std::uint8_t{0x08},
                     std::uint16_t{0x40C6}},
                 12U},
                {SatWriterSite{
                     std::uint8_t{0x08},
                     std::uint16_t{0x40CC}},
                 12U},
                {SatWriterSite{
                     std::uint8_t{0x08},
                     std::uint16_t{0x40D2}},
                 12U},
                {SatWriterSite{
                     std::uint8_t{0x08},
                     std::uint16_t{0x40D8}},
                 12U},
                {SatWriterSite{
                     std::uint8_t{0x08},
                     std::uint16_t{0x4141}},
                 52U},
                {SatWriterSite{
                     std::uint8_t{0x08},
                     std::uint16_t{0x4154}},
                 43U},
                {SatWriterSite{
                     std::uint8_t{0x08},
                     std::uint16_t{0x4165}},
                 9U},
                {SatWriterSite{
                     std::uint8_t{0x08},
                     std::uint16_t{0x4176}},
                 52U},
                {SatWriterSite{
                     std::uint8_t{0x08},
                     std::uint16_t{0x419E}},
                 52U},
            };
        check(
            sat_writer_sites == expected_sat_writer_sites,
            "bank-8 actor renderer writes the complete SAT staging table");
        const std::map<SatWriterSite, std::size_t>
            expected_sprite_region_writer_sites{
                {SatWriterSite{
                     std::uint8_t{0x00},
                     std::uint16_t{0xE449}},
                 1036U},
                {SatWriterSite{
                     std::uint8_t{0x00},
                     std::uint16_t{0xE467}},
                 2548U},
            };
        check(
            sprite_region_writer_sites
                == expected_sprite_region_writer_sites,
            "generic stream decoder owns every changed sprite-pattern word");
        std::vector<SpriteRegionStream> sprite_region_streams;
        auto reconstructed_vram = frame_1200_vram;
        bool streams_decode = reconstructed_vram.size() == 32768U;
        for (const auto& entry : vram_stream_entries) {
            if (entry.frame_number <= 1200U
                || entry.frame_number > 1600U) {
                continue;
            }
            const auto decoded =
                devils_crush::render::decode_pce_vram_stream(
                    rom_bytes,
                    entry.mpr,
                    entry.source_address);
            if (!decoded) {
                streams_decode = false;
                continue;
            }
            const auto stream_begin =
                static_cast<std::size_t>(entry.destination_word);
            const auto stream_end = stream_begin + decoded->size();
            if (stream_begin < 0x7800U && stream_end > 0x4000U) {
                sprite_region_streams.push_back({
                    .frame_number = entry.frame_number,
                    .destination_word = entry.destination_word,
                    .source_bank = entry.source_mpr,
                    .source_address = entry.source_address,
                    .caller_pc = entry.caller_pc,
                    .actor_slot = entry.actor_slot,
                    .actor_appearance = entry.actor_appearance,
                    .actor_phase = entry.actor_phase,
                    .loader_descriptor =
                        entry.loader_descriptor,
                    .pattern_base = entry.pattern_base,
                });
            }
            if (stream_end > reconstructed_vram.size()) {
                streams_decode = false;
                continue;
            }
            std::copy(
                decoded->begin(),
                decoded->end(),
                reconstructed_vram.begin()
                    + static_cast<std::ptrdiff_t>(stream_begin));
        }
        constexpr std::array expected_sprite_region_streams{
            SpriteRegionStream{
                1220U, 0x7000U, 0x12U, 0x8030U, 0xF661U,
                0x5BU, 0x03U, 0x00U, 0xF6FAU, 0x3800U},
            SpriteRegionStream{
                1227U, 0x5400U, 0x12U, 0x8AFDU, 0xF661U,
                0x0FU, 0x11U, 0x00U, 0xF73BU, 0x3800U},
            SpriteRegionStream{
                1245U, 0x5400U, 0x12U, 0x8B80U, 0xF661U,
                0x0FU, 0x11U, 0x01U, 0xF73BU, 0x3800U},
            SpriteRegionStream{
                1249U, 0x5400U, 0x12U, 0x8C5CU, 0xF661U,
                0x0FU, 0x11U, 0x02U, 0xF73BU, 0x3800U},
            SpriteRegionStream{
                1253U, 0x5400U, 0x12U, 0x8D52U, 0xF661U,
                0x0FU, 0x11U, 0x03U, 0xF73BU, 0x3800U},
            SpriteRegionStream{
                1255U, 0x5400U, 0x12U, 0x8C5CU, 0xF661U,
                0x0FU, 0x11U, 0x02U, 0xF73BU, 0x3800U},
            SpriteRegionStream{
                1259U, 0x5400U, 0x12U, 0x8B80U, 0xF661U,
                0x0FU, 0x11U, 0x01U, 0xF73BU, 0x3800U},
            SpriteRegionStream{
                1263U, 0x5400U, 0x12U, 0x8AFDU, 0xF661U,
                0x0FU, 0x11U, 0x00U, 0xF73BU, 0x3800U},
            SpriteRegionStream{
                1298U, 0x5400U, 0x12U, 0x8B80U, 0xF661U,
                0x0FU, 0x11U, 0x01U, 0xF73BU, 0x3800U},
            SpriteRegionStream{
                1302U, 0x5400U, 0x12U, 0x8C5CU, 0xF661U,
                0x0FU, 0x11U, 0x02U, 0xF73BU, 0x3800U},
            SpriteRegionStream{
                1306U, 0x5400U, 0x12U, 0x8D52U, 0xF661U,
                0x0FU, 0x11U, 0x03U, 0xF73BU, 0x3800U},
            SpriteRegionStream{
                1308U, 0x5400U, 0x12U, 0x8C5CU, 0xF661U,
                0x0FU, 0x11U, 0x02U, 0xF73BU, 0x3800U},
            SpriteRegionStream{
                1312U, 0x5400U, 0x12U, 0x8B80U, 0xF661U,
                0x0FU, 0x11U, 0x01U, 0xF73BU, 0x3800U},
            SpriteRegionStream{
                1316U, 0x5400U, 0x12U, 0x8AFDU, 0xF661U,
                0x0FU, 0x11U, 0x00U, 0xF73BU, 0x3800U},
        };
        const bool sprite_streams_match = std::ranges::equal(
            sprite_region_streams,
            expected_sprite_region_streams);
        check(
            sprite_streams_match,
            "natural route selects the expected sprite-pattern streams");
        if (!sprite_streams_match) {
            std::cerr << "Observed sprite-pattern streams:";
            for (const auto& stream : sprite_region_streams) {
                std::cerr << " {" << std::dec << stream.frame_number
                          << ',' << std::hex
                          << stream.destination_word << ','
                          << static_cast<unsigned>(stream.source_bank)
                          << ':' << stream.source_address << ','
                          << stream.caller_pc << '}';
            }
            std::cerr << std::dec << '\n';
        }
        check(
            streams_decode && frame_1600_vram.size() == 32768U,
            "native decoder reconstructs every routed VRAM stream");
        const auto reconstructed_sprite_region_digest =
            reconstructed_vram.size() == 32768U
            ? devils_crush::rom::compute_digests(
                  as_bytes<std::uint16_t>(
                      std::span<const std::uint16_t>(
                          reconstructed_vram)
                          .subspan(0x4000U, 0x3800U)))
            : devils_crush::rom::Digests{};
        check(
            reconstructed_vram.size() == frame_1600_vram.size()
                && std::equal(
                    reconstructed_vram.begin() + 0x4000,
                    reconstructed_vram.begin() + 0x7800,
                    frame_1600_vram.begin() + 0x4000),
            "native streams reproduce frame-1600 sprite-pattern VRAM exactly");
        check(
            reconstructed_sprite_region_digest.sha256
                == "c5f344d3130e20d40a37e5d055a557022290be8c87d97df3"
                   "97068850fbf7155d",
            "reconstructed sprite-pattern region matches its golden");
        std::cout
            << "Sprite-pattern streams: calls="
            << sprite_region_streams.size()
            << " raw_words=1036 repeated_words=2548"
            << " reconstructed_sha256="
            << reconstructed_sprite_region_digest.sha256 << '\n';
    }
    runtime.reset();
    if (!map_row_writer) {
        const auto upper_runtime =
            devils_crush::emulation::create_pce_runtime();
        const auto upper_load =
            upper_runtime->load_rom(std::filesystem::path(argv[1]));
        check(
            upper_load.loaded,
            "supported ROM loads for upper-table camera route");
        devils_crush::emulation::EmulatedFrame upper_frame;
        constexpr int upper_ball_first_frame = 1200;
        constexpr int upper_ball_last_frame = 3000;
        constexpr int upper_capture_frame = 3420;
        for (int frame_number = 1;
             upper_load.loaded
                 && frame_number <= upper_capture_frame;
             ++frame_number) {
            if (frame_number >= upper_ball_first_frame
                && frame_number <= upper_ball_last_frame
                && (frame_number - upper_ball_first_frame) % 10 == 0) {
                check(
                    force_ball_y(*upper_runtime, 64U),
                    "upper-table ball-Y state imports");
            }
            upper_frame =
                upper_runtime->run_frame(input_for_frame(frame_number));
        }
        const auto upper_video = upper_runtime->export_video_memory();
        const auto upper_work_ram = upper_runtime->export_work_ram();
        const auto upper_table = upper_video
            ? decode_stitched_table(
                  rom_bytes,
                  upper_work_ram,
                  *upper_video)
            : std::nullopt;
        const auto upper_comparison = upper_table
            ? compare_table_to_composite(
                  *upper_table,
                  20,
                  upper_frame.framebuffer)
            : std::nullopt;
        check(
            upper_frame.frame_number == upper_capture_frame,
            "upper-table camera route reaches its final frame");
        check(
            upper_video && upper_video->background_y_scroll == 52U,
            "upper-table camera route reaches signed -460 scroll");
        const auto read_upper_i16 =
            [&upper_work_ram](
                const std::size_t low,
                const std::size_t high) {
                return static_cast<std::int16_t>(
                    static_cast<std::uint16_t>(
                        std::to_integer<std::uint8_t>(
                            upper_work_ram[low]))
                    | (static_cast<std::uint16_t>(
                           std::to_integer<std::uint8_t>(
                               upper_work_ram[high]))
                       << 8U));
            };
        check(
            upper_work_ram.size() > 0xAAU
                && read_upper_i16(0x0BU, 0x0CU) == -460
                && read_upper_i16(0xA9U, 0xAAU) == 20,
            "upper camera clamp leaves world row 20 at the viewport top");
        check(
            upper_table && upper_comparison,
            "upper-table source crop aligns with emulator composite");
        if (upper_table && upper_comparison) {
            const auto upper_crop =
                devils_crush::render::crop_image(
                    *upper_table,
                    0,
                    20,
                    256,
                    208);
            const auto upper_digest = upper_crop
                ? devils_crush::rom::compute_digests(
                      as_bytes<std::uint32_t>(upper_crop->pixels))
                : devils_crush::rom::Digests{};
            check(
                upper_digest.sha256
                        == "f2f8fda9090a1f98f1cf07ece0e775122c43257d38caef"
                           "7f33f2d63c3ff81aec"
                    && upper_comparison->matching_pixels == 44247U
                    && upper_comparison->sprite_or_overlay_pixels
                        == 9001U,
                "settled upper-region source matches emulator output");
            std::cout
                << "upper_table_world_y=20"
                << " upper_crop_sha256=" << upper_digest.sha256
                << " upper_composite_background_matches="
                << upper_comparison->matching_pixels
                << " upper_composite_overlay_pixels="
                << upper_comparison->sprite_or_overlay_pixels << '\n';
            if (table_output) {
                const auto upper_composite =
                    copy_framebuffer(upper_frame.framebuffer);
                check(
                    upper_crop
                        && write_bmp(
                            diagnostic_path(
                                *table_output,
                                "-upper-background"),
                            *upper_crop),
                    "upper-table background BMP writes");
                check(
                    upper_composite
                        && write_bmp(
                            diagnostic_path(
                                *table_output,
                                "-upper-composite"),
                            *upper_composite),
                    "upper-table composite BMP writes");
            }
        }
        if (failures == 0) {
            std::cout << "Video snapshot tests passed.\n";
            return EXIT_SUCCESS;
        }
        return EXIT_FAILURE;
    }

    const std::map<std::pair<std::uint8_t, std::uint16_t>, std::size_t>
        expected_vdc_write_sites{
            {{std::uint8_t{0x00U}, std::uint16_t{0xF400U}}, 539U},
            {{std::uint8_t{0x00U}, std::uint16_t{0xF405U}}, 539U},
            {{std::uint8_t{0x00U}, std::uint16_t{0xF40AU}}, 539U},
            {{std::uint8_t{0x00U}, std::uint16_t{0xF411U}}, 539U},
            {{std::uint8_t{0x00U}, std::uint16_t{0xF417U}}, 17248U},
            {{std::uint8_t{0x00U}, std::uint16_t{0xF434U}}, 17248U},
        };
    check(
        vdc_write_sites == expected_vdc_write_sites,
        "F400 emits four setup writes and 32 BAT words per call");
    check(
        background_row_calls.size() == 539U,
        "background-row transfer call count matches");
    check(
        background_row_entries.size() == background_row_calls.size(),
        "background-row entry metadata aligns with completed calls");
    std::map<
        std::pair<std::uint16_t, std::uint16_t>,
        std::size_t>
        row_call_paths;
    for (const auto& call : background_row_calls) {
        ++row_call_paths[{
            call.caller_pc,
            call.return_instruction.logical_pc,
        }];
    }
    const std::map<
        std::pair<std::uint16_t, std::uint16_t>,
        std::size_t>
        expected_row_call_paths{
            {{std::uint16_t{0xF2FEU}, std::uint16_t{0xF43CU}}, 286U},
            {{std::uint16_t{0xF32DU}, std::uint16_t{0xF43CU}}, 5U},
            {{std::uint16_t{0xF3B2U}, std::uint16_t{0xF43CU}}, 232U},
            {{std::uint16_t{0xF3E1U}, std::uint16_t{0xF43CU}}, 16U},
        };
    check(
        row_call_paths == expected_row_call_paths,
        "F400 row-transfer caller paths match");
    check(
        !background_row_calls.empty()
            && background_row_calls.front().entry.frame_number == 1201U
            && background_row_calls.back().entry.frame_number == 3600U,
        "F400 row-transfer frame range matches");
    std::cout << "F400 calls=" << background_row_calls.size();
    if (!background_row_calls.empty()) {
        std::cout << " frames="
                  << background_row_calls.front().entry.frame_number
                  << '-' << background_row_calls.back().entry.frame_number;
    }
    std::cout << " paths:";
    for (const auto& [path, count] : row_call_paths) {
        std::cout << ' ' << std::hex << path.first << "->"
                  << path.second << std::dec << '=' << count;
    }
    std::cout << '\n';

    std::map<int, std::size_t> all_world_rows;
    bool row_coordinate_contract_matches = true;
    for (std::size_t index = 0;
         index < std::min(
             background_row_calls.size(),
             background_row_entries.size());
        ++index) {
        const auto caller = background_row_calls[index].caller_pc;
        const auto& entry = background_row_entries[index];
        const auto rounded_anchor =
            static_cast<int>(entry.anchor_y) & ~7;
        auto world_row = rounded_anchor;
        if (caller == 0xF2FEU) {
            world_row += 240;
        } else if (caller == 0xF32DU) {
            world_row += 248;
        } else if (caller == 0xF3B2U) {
            world_row -= entry.step_pixels;
        } else if (caller == 0xF3E1U) {
            world_row -= entry.step_pixels - 8;
        }
        ++all_world_rows[world_row];
        const auto destination_pixel =
            (world_row - 480) & 0x00FF;
        const auto expected_destination =
            static_cast<std::uint16_t>(
                (destination_pixel / 8) * 64);
        const auto expected_tile_source =
            static_cast<std::uint16_t>(
                0x800AU + world_row * 8);
        const auto expected_attribute_source =
            static_cast<std::uint16_t>(
                0x2259U + world_row * 2);
        row_coordinate_contract_matches =
            row_coordinate_contract_matches
            && entry.anchor_y - entry.camera_y == 480
            && entry.destination_word == expected_destination
            && entry.tile_source == expected_tile_source
            && entry.attribute_source == expected_attribute_source
            && entry.source_mpr == 0x14U
            && entry.attribute_mpr == 0xF8U;
    }
    check(
        row_coordinate_contract_matches,
        "streamed rows map world Y to ROM, RAM, and circular BAT");
    check(
        all_world_rows.size() == 70U
            && all_world_rows.begin()->first == 168
            && all_world_rows.rbegin()->first == 720,
        "state-census stream covers every row from world Y 168 through 720");

    if (failures == 0) {
        std::cout << "Video row-writer tests passed.\n";
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}
