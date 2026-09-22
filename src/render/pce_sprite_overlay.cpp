// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/render/pce_sprite_overlay.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <map>
#include <ranges>
#include <set>
#include <tuple>
#include <vector>

namespace devils_crush::render {
namespace {

constexpr std::size_t kVramWordCount = 32768U;
constexpr std::size_t kSatWordCount = 256U;
constexpr std::size_t kPaletteEntryCount = 512U;
constexpr int kSpriteCount = 64;
constexpr int kSpriteWords = 4;
constexpr int kSpriteXBias = 0x20;
constexpr int kSpriteYBias = 0x40;
constexpr std::uint16_t kHorizontalFlip = 0x0800U;
constexpr std::uint16_t kVerticalFlip = 0x8000U;
constexpr std::array<int, 4> kSpriteHeights{16, 32, 64, 64};
constexpr std::size_t kMaximumSpriteHalvesPerLine = 16U;
constexpr std::size_t kActorAppearancePhaseBase = 0x1980U;
constexpr std::size_t kActorDisplayFlagsBase = 0x1800U;
constexpr std::size_t kActorLowAttributeBase = 0x1D00U;
constexpr std::size_t kActorHighAttributeBase = 0x1E00U;
constexpr std::size_t kLoaderPatternBaseLow = 0x004FU;
constexpr std::size_t kLoaderPatternBaseHigh = 0x0050U;
constexpr std::size_t kSatPatternBaseLow = 0x0051U;
constexpr std::size_t kSatPatternBaseHigh = 0x0052U;
constexpr std::size_t kBankSize = 0x2000U;
constexpr std::size_t kSpriteDescriptorBank = 0x08U;
constexpr std::uint16_t kBankLogicalBase = 0x4000U;
constexpr std::uint16_t kBankLogicalEnd = 0x6000U;
constexpr std::uint16_t kAppearancePointerLow = 0x41ACU;
constexpr std::uint16_t kAppearancePointerHigh = 0x41F1U;
constexpr std::size_t kAppearanceCount = 0x45U;
constexpr std::size_t kSpriteDescriptorBytes = 6U;
constexpr std::size_t kMaximumSpritesPerActor = 64U;
constexpr std::uint16_t kPatternLoaderPointerLow = 0xF6A6U;
constexpr std::uint16_t kPatternLoaderPointerHigh = 0xF6D0U;
constexpr std::uint16_t kResidentPatternLoader = 0xF768U;
constexpr std::uint16_t kResidentPatternAssetTable = 0xF783U;
constexpr std::size_t kResidentPatternAssetCount = 5U;
constexpr std::uint16_t kResidentPatternDestination = 0x7C00U;

struct SpriteSlice {
    int sprite_index = 0;
    int horizontal_half = 0;
};

[[nodiscard]] constexpr int sprite_width(
    const std::uint16_t flags) noexcept
{
    return (flags & 0x0100U) != 0U ? 32 : 16;
}

[[nodiscard]] constexpr int sprite_height(
    const std::uint16_t flags) noexcept
{
    return kSpriteHeights[static_cast<std::size_t>(
        (flags >> 12U) & 0x03U)];
}

[[nodiscard]] bool valid_snapshot(
    const ArgbImage& destination,
    const PceSpriteSnapshot& snapshot,
    const int framebuffer_playfield_height,
    const std::span<const std::uint8_t> background_opaque) noexcept
{
    return destination.valid()
        && snapshot.vram_words.size() >= kVramWordCount
        && snapshot.sat_words.size() >= kSatWordCount
        && snapshot.palette_entries.size() >= kPaletteEntryCount
        && framebuffer_playfield_height > 0
        && (background_opaque.empty()
            || background_opaque.size()
                == destination.pixels.size());
}

[[nodiscard]] std::optional<std::size_t> descriptor_rom_offset(
    const std::span<const std::byte> rom_bytes,
    const std::uint16_t logical_address,
    const std::size_t required_bytes = 1U) noexcept
{
    if (logical_address < kBankLogicalBase
        || logical_address >= kBankLogicalEnd
        || required_bytes
            > static_cast<std::size_t>(
                kBankLogicalEnd - logical_address)) {
        return std::nullopt;
    }
    const auto offset =
        kSpriteDescriptorBank * kBankSize
        + static_cast<std::size_t>(
            logical_address - kBankLogicalBase);
    if (offset > rom_bytes.size()
        || required_bytes > rom_bytes.size() - offset) {
        return std::nullopt;
    }
    return offset;
}

[[nodiscard]] std::optional<std::size_t> bank_zero_rom_offset(
    const std::span<const std::byte> rom_bytes,
    const std::uint16_t logical_address,
    const std::size_t required_bytes = 1U) noexcept
{
    constexpr std::uint16_t logical_base = 0xE000U;
    if (logical_address < logical_base
        || required_bytes
            > static_cast<std::size_t>(
                std::numeric_limits<std::uint16_t>::max()
                - logical_address)
                + 1U) {
        return std::nullopt;
    }
    const auto offset =
        static_cast<std::size_t>(logical_address - logical_base);
    if (offset > rom_bytes.size()
        || required_bytes > rom_bytes.size() - offset
        || offset + required_bytes > kBankSize) {
        return std::nullopt;
    }
    return offset;
}

[[nodiscard]] constexpr std::uint8_t byte_value(
    const std::byte value) noexcept
{
    return std::to_integer<std::uint8_t>(value);
}

[[nodiscard]] constexpr std::uint16_t little_endian_word(
    const std::uint8_t low,
    const std::uint8_t high) noexcept
{
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(low)
        | (static_cast<std::uint16_t>(high) << 8U));
}

[[nodiscard]] constexpr std::uint16_t aligned_pattern(
    const std::uint16_t pattern,
    const int height) noexcept
{
    if (height == 32) {
        return static_cast<std::uint16_t>(pattern & ~2U);
    }
    if (height == 64) {
        return static_cast<std::uint16_t>(pattern & ~6U);
    }
    return pattern;
}

enum class DynamicPresentation {
    live_camera_only,
    persistent_enemy,
    table_world_effect,
};

[[nodiscard]] constexpr bool impact_burst_signature(
    const std::uint8_t type,
    const std::uint8_t appearance,
    const std::uint16_t dispatch) noexcept
{
    constexpr std::uint8_t impact_type = 0x8FU;
    constexpr std::uint8_t impact_appearance = 0x10U;
    constexpr std::uint16_t impact_activation = 0x4290U;
    constexpr std::uint16_t impact_animation = 0x42B0U;
    return type == impact_type
        && appearance == impact_appearance
        && (dispatch == impact_activation
            || dispatch == impact_animation);
}

[[nodiscard]] constexpr DynamicPresentation dynamic_presentation(
    const devils_crush::cabinet::TableActorState& actor) noexcept
{
    constexpr std::uint8_t enemy_type = 0x82U;
    constexpr std::uint8_t small_soldier_appearance = 0x1AU;
    constexpr std::uint8_t tower_insect_appearance = 0x39U;
    const bool persistent_enemy = actor.type == enemy_type
        && (actor.appearance == small_soldier_appearance
            || actor.appearance == tower_insect_appearance);
    if (persistent_enemy) {
        return DynamicPresentation::persistent_enemy;
    }

    // The shared impact burst copies its source's absolute table position,
    // advances phases 0-8 under the original actor scheduler, and clears its
    // slot after 35-36 frames. Both its one-shot activation and steady
    // animation therefore have a meaningful full-table lifetime.
    if (impact_burst_signature(
            actor.type,
            actor.appearance,
            actor.dispatch)) {
        return DynamicPresentation::table_world_effect;
    }
    return DynamicPresentation::live_camera_only;
}

[[nodiscard]] constexpr bool compatible_actor_dispatch(
    const std::uint8_t cached_type,
    const std::uint8_t cached_appearance,
    const std::uint16_t cached_dispatch,
    const devils_crush::cabinet::TableActorState& actor) noexcept
{
    return cached_dispatch == actor.dispatch
        || (impact_burst_signature(
                cached_type,
                cached_appearance,
                cached_dispatch)
            && impact_burst_signature(
                actor.type,
                actor.appearance,
                actor.dispatch));
}

[[nodiscard]] constexpr bool retained_dynamic_actor(
    const devils_crush::cabinet::TableActorState& actor) noexcept
{
    return dynamic_presentation(actor)
        != DynamicPresentation::live_camera_only;
}

struct ResolvedAsset {
    std::array<std::uint8_t, 8> mpr{};
    std::uint8_t physical_bank = 0;
    std::uint16_t source_address = 0;
};

[[nodiscard]] std::optional<ResolvedAsset> resolve_asset(
    const std::span<const std::byte> rom_bytes,
    const std::uint16_t asset_reference) noexcept
{
    const auto bank = static_cast<std::uint8_t>(
        asset_reference >> 8U);
    const auto directory_index = static_cast<std::size_t>(
        asset_reference & 0x00FFU);
    const auto directory_offset =
        static_cast<std::size_t>(bank) * kBankSize
        + directory_index;
    if (directory_offset >= rom_bytes.size()
        || rom_bytes.size() - directory_offset < 2U) {
        return std::nullopt;
    }

    ResolvedAsset result;
    result.physical_bank = bank;
    result.source_address = little_endian_word(
        byte_value(rom_bytes[directory_offset]),
        byte_value(rom_bytes[directory_offset + 1U]));
    result.mpr[4] = bank;
    result.mpr[5] = static_cast<std::uint8_t>(bank + 1U);
    return result;
}

[[nodiscard]] std::optional<std::uint8_t> mapped_rom_byte(
    const std::span<const std::byte> rom_bytes,
    const std::array<std::uint8_t, 8>& mpr,
    const std::uint16_t logical_address) noexcept
{
    const auto page =
        static_cast<std::size_t>(logical_address >> 13U);
    const auto offset =
        static_cast<std::size_t>(mpr[page]) * kBankSize
        + static_cast<std::size_t>(
            logical_address & (kBankSize - 1U));
    if (offset >= rom_bytes.size()) {
        return std::nullopt;
    }
    return byte_value(rom_bytes[offset]);
}

[[nodiscard]] std::optional<std::vector<std::uint16_t>>
decode_raw_vram_words(
    const std::span<const std::byte> rom_bytes,
    const std::array<std::uint8_t, 8>& mpr,
    std::uint16_t source_address,
    const std::size_t word_count,
    const std::size_t maximum_output_words)
{
    if (word_count == 0U
        || word_count > maximum_output_words) {
        return std::nullopt;
    }
    std::vector<std::uint16_t> words;
    words.reserve(word_count);
    for (std::size_t index = 0; index < word_count; ++index) {
        const auto low =
            mapped_rom_byte(rom_bytes, mpr, source_address);
        ++source_address;
        const auto high =
            mapped_rom_byte(rom_bytes, mpr, source_address);
        ++source_address;
        if (!low || !high) {
            return std::nullopt;
        }
        words.push_back(little_endian_word(*low, *high));
    }
    return words;
}

[[nodiscard]] std::uint8_t sprite_pixel(
    const PceSpriteSnapshot& snapshot,
    const std::uint16_t sat_pattern,
    const std::uint16_t flags,
    const int width,
    const int height,
    const int x,
    const int y) noexcept
{
    const bool horizontal_flip = (flags & kHorizontalFlip) != 0U;
    const bool vertical_flip = (flags & kVerticalFlip) != 0U;
    const int source_y = vertical_flip ? height - 1 - y : y;
    const int block = x / 16;
    const int pattern_block =
        horizontal_flip && width == 32 ? block ^ 1 : block;
    const int local_x = x & 15;
    const int bit = horizontal_flip ? local_x : 15 - local_x;

    auto pattern = static_cast<std::uint16_t>(
        (sat_pattern >> 1U) & 0x03FFU);
    pattern = aligned_pattern(pattern, height);
    pattern = static_cast<std::uint16_t>(
        pattern | static_cast<std::uint16_t>((source_y & 0x30) >> 3));
    if (width == 32) {
        pattern = static_cast<std::uint16_t>(pattern & ~1U);
        pattern = static_cast<std::uint16_t>(
            pattern | static_cast<std::uint16_t>(pattern_block));
    }

    const auto base = static_cast<std::size_t>(pattern) * 64U
        + static_cast<std::size_t>(source_y & 15);
    if (base + 48U >= snapshot.vram_words.size()) {
        return 0U;
    }

    std::array<std::uint16_t, 4> planes{
        snapshot.vram_words[base],
        snapshot.vram_words[base + 16U],
        snapshot.vram_words[base + 32U],
        snapshot.vram_words[base + 48U],
    };
    if ((snapshot.memory_width_register & 0x0CU) == 0x04U) {
        if ((sat_pattern & 1U) != 0U) {
            planes[0] = planes[2];
            planes[1] = planes[3];
        }
        planes[2] = 0U;
        planes[3] = 0U;
    }

    std::uint8_t pixel = 0U;
    for (std::size_t plane = 0; plane < planes.size(); ++plane) {
        pixel = static_cast<std::uint8_t>(
            pixel
            | static_cast<std::uint8_t>(
                ((planes[plane] >> bit) & 1U) << plane));
    }
    return pixel;
}

[[nodiscard]] bool upload_covers_sprite(
    const PceVramUpload& upload,
    const std::uint16_t sat_pattern,
    const std::uint16_t flags) noexcept
{
    const auto upload_begin =
        static_cast<std::size_t>(upload.destination_word);
    const auto upload_end = upload_begin + upload.words.size();
    if (upload_end < upload_begin
        || upload_end > kVramWordCount) {
        return false;
    }

    const int width = sprite_width(flags);
    const int height = sprite_height(flags);
    auto first_pattern = static_cast<std::uint16_t>(
        (sat_pattern >> 1U) & 0x03FFU);
    first_pattern = aligned_pattern(first_pattern, height);
    if (width == 32) {
        first_pattern =
            static_cast<std::uint16_t>(first_pattern & ~1U);
    }
    for (int vertical_block = 0;
         vertical_block < height / 16;
         ++vertical_block) {
        for (int horizontal_block = 0;
             horizontal_block < width / 16;
             ++horizontal_block) {
            const auto pattern = static_cast<std::size_t>(
                first_pattern
                | static_cast<std::uint16_t>(
                    vertical_block * 2 + horizontal_block));
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

[[nodiscard]] bool upload_overlaps_sprite(
    const PceVramUpload& upload,
    const std::uint16_t sat_pattern,
    const std::uint16_t flags) noexcept
{
    const auto upload_begin =
        static_cast<std::size_t>(upload.destination_word);
    const auto upload_end = upload_begin + upload.words.size();
    if (upload_end < upload_begin
        || upload_end > kVramWordCount) {
        return true;
    }

    const int width = sprite_width(flags);
    const int height = sprite_height(flags);
    auto first_pattern = static_cast<std::uint16_t>(
        (sat_pattern >> 1U) & 0x03FFU);
    first_pattern = aligned_pattern(first_pattern, height);
    if (width == 32) {
        first_pattern =
            static_cast<std::uint16_t>(first_pattern & ~1U);
    }
    for (int vertical_block = 0;
         vertical_block < height / 16;
         ++vertical_block) {
        for (int horizontal_block = 0;
             horizontal_block < width / 16;
             ++horizontal_block) {
            const auto pattern = static_cast<std::size_t>(
                first_pattern
                | static_cast<std::uint16_t>(
                    vertical_block * 2 + horizontal_block));
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

} // namespace

std::optional<std::vector<std::uint16_t>>
decode_pce_vram_stream(
    const std::span<const std::byte> rom_bytes,
    const std::array<std::uint8_t, 8>& mpr,
    std::uint16_t source_address,
    const std::size_t maximum_output_words)
{
    if (maximum_output_words == 0U) {
        return std::nullopt;
    }
    const auto read_source_byte =
        [&rom_bytes, &mpr, &source_address]()
        -> std::optional<std::uint8_t> {
        const auto page =
            static_cast<std::size_t>(source_address >> 13U);
        const auto physical_bank =
            static_cast<std::size_t>(mpr[page]);
        const auto offset =
            physical_bank * kBankSize
            + static_cast<std::size_t>(
                source_address & (kBankSize - 1U));
        if (offset >= rom_bytes.size()) {
            return std::nullopt;
        }
        const auto value = byte_value(rom_bytes[offset]);
        ++source_address;
        return value;
    };

    std::vector<std::uint16_t> output;
    while (true) {
        const auto command = read_source_byte();
        if (!command) {
            return std::nullopt;
        }
        if (*command == 0U) {
            return output;
        }

        if ((*command & 0x80U) == 0U) {
            const auto count = static_cast<std::size_t>(*command);
            if (count > maximum_output_words - output.size()) {
                return std::nullopt;
            }
            for (std::size_t index = 0; index < count; ++index) {
                const auto low = read_source_byte();
                const auto high = read_source_byte();
                if (!low || !high) {
                    return std::nullopt;
                }
                output.push_back(little_endian_word(*low, *high));
            }
            continue;
        }

        auto count =
            static_cast<std::size_t>(*command & 0x7FU);
        if (count == 0U) {
            count = 256U;
        }
        if (count > maximum_output_words - output.size()) {
            return std::nullopt;
        }
        const auto low = read_source_byte();
        const auto high = read_source_byte();
        if (!low || !high) {
            return std::nullopt;
        }
        output.insert(
            output.end(),
            count,
            little_endian_word(*low, *high));
    }
}

std::optional<PcePatternLoaderDescriptor>
decode_pce_pattern_loader_descriptor(
    const std::span<const std::byte> rom_bytes,
    const std::uint16_t logical_address,
    const std::size_t byte_count,
    const std::uint16_t pattern_base,
    const std::size_t maximum_output_words)
{
    const auto descriptor_offset =
        bank_zero_rom_offset(
            rom_bytes,
            logical_address,
            byte_count);
    if (!descriptor_offset || byte_count < 7U
        || maximum_output_words == 0U) {
        return std::nullopt;
    }
    const auto read_descriptor_word =
        [&rom_bytes, descriptor_offset](
            const std::size_t offset) {
            return little_endian_word(
                byte_value(rom_bytes[*descriptor_offset + offset]),
                byte_value(
                    rom_bytes[*descriptor_offset + offset + 1U]));
        };

    PcePatternLoaderDescriptor descriptor{
        .logical_address = logical_address,
    };
    const auto mode =
        byte_value(rom_bytes[*descriptor_offset]);
    if (mode == 0U) {
        if ((byte_count - 5U) % 4U != 0U) {
            return std::nullopt;
        }
        const auto asset_reference = read_descriptor_word(1U);
        const auto destination_word =
            static_cast<std::uint16_t>(
                pattern_base + read_descriptor_word(3U));
        const auto asset = resolve_asset(
            rom_bytes,
            asset_reference);
        if (!asset) {
            return std::nullopt;
        }
        const auto phase_count = (byte_count - 5U) / 4U;
        if (phase_count
            > static_cast<std::size_t>(
                std::numeric_limits<std::uint8_t>::max())
                + 1U) {
            return std::nullopt;
        }
        descriptor.uploads.reserve(phase_count);
        for (std::size_t phase = 0;
             phase < phase_count;
             ++phase) {
            const auto record_offset = 5U + phase * 4U;
            const auto source_address =
                static_cast<std::uint16_t>(
                    asset->source_address
                    + read_descriptor_word(record_offset));
            auto word_count = static_cast<std::size_t>(
                read_descriptor_word(record_offset + 2U));
            // E365's decrement-before-test loop treats zero as 65,536.
            if (word_count == 0U) {
                word_count = 65536U;
            }
            auto words = decode_raw_vram_words(
                rom_bytes,
                asset->mpr,
                source_address,
                word_count,
                maximum_output_words);
            if (!words) {
                return std::nullopt;
            }
            descriptor.uploads.push_back({
                .phase = static_cast<std::uint8_t>(phase),
                .destination_word = destination_word,
                .asset_reference = asset_reference,
                .source_bank = asset->physical_bank,
                .source_address = source_address,
                .encoding = PceVramUploadEncoding::raw_words,
                .words = std::move(*words),
            });
        }
        return descriptor;
    }

    if ((mode != 1U && mode != 2U)
        || (byte_count - 5U) % 2U != 0U) {
        return std::nullopt;
    }
    const auto phase_count = (byte_count - 5U) / 2U;
    if (phase_count == 0U
        || (mode == 2U && phase_count != 1U)
        || phase_count
            > static_cast<std::size_t>(
                std::numeric_limits<std::uint8_t>::max())
                + 1U) {
        return std::nullopt;
    }
    const auto destination_word =
        static_cast<std::uint16_t>(
            pattern_base + read_descriptor_word(1U));
    descriptor.uploads.reserve(phase_count);
    for (std::size_t phase = 0;
         phase < phase_count;
         ++phase) {
        const auto asset_reference =
            read_descriptor_word(5U + phase * 2U);
        const auto asset = resolve_asset(
            rom_bytes,
            asset_reference);
        if (!asset) {
            return std::nullopt;
        }
        auto words = decode_pce_vram_stream(
            rom_bytes,
            asset->mpr,
            asset->source_address,
            maximum_output_words);
        if (!words) {
            return std::nullopt;
        }
        descriptor.uploads.push_back({
            .phase = static_cast<std::uint8_t>(phase),
            .destination_word = destination_word,
            .asset_reference = asset_reference,
            .source_bank = asset->physical_bank,
            .source_address = asset->source_address,
            .encoding =
                PceVramUploadEncoding::compressed_stream,
            .words = std::move(*words),
        });
    }
    return descriptor;
}

std::optional<PcePatternAtlas>
decode_pce_pattern_atlas(
    const std::span<const std::byte> rom_bytes,
    const std::uint16_t pattern_base,
    const std::size_t maximum_output_words)
{
    const auto pointer_low_offset = bank_zero_rom_offset(
        rom_bytes,
        kPatternLoaderPointerLow,
        kPcePatternLoaderAppearanceCount);
    const auto pointer_high_offset = bank_zero_rom_offset(
        rom_bytes,
        kPatternLoaderPointerHigh,
        kPcePatternLoaderAppearanceCount);
    if (!pointer_low_offset || !pointer_high_offset
        || maximum_output_words == 0U) {
        return std::nullopt;
    }

    std::array<
        std::uint16_t,
        kPcePatternLoaderAppearanceCount>
        appearance_pointers{};
    std::set<std::uint16_t> sorted_pointers;
    for (std::size_t appearance = 0;
         appearance < appearance_pointers.size();
         ++appearance) {
        const auto pointer = little_endian_word(
            byte_value(
                rom_bytes[*pointer_low_offset + appearance]),
            byte_value(
                rom_bytes[*pointer_high_offset + appearance]));
        appearance_pointers[appearance] = pointer;
        sorted_pointers.insert(pointer);
    }

    PcePatternAtlas atlas;
    const auto resident_asset_table_offset = bank_zero_rom_offset(
        rom_bytes,
        kResidentPatternAssetTable,
        kResidentPatternAssetCount * 2U);
    if (!resident_asset_table_offset) {
        return std::nullopt;
    }
    atlas.resident_uploads.reserve(kResidentPatternAssetCount);
    for (std::size_t selector = 0;
         selector < kResidentPatternAssetCount;
         ++selector) {
        const auto asset_reference = little_endian_word(
            byte_value(
                rom_bytes[
                    *resident_asset_table_offset + selector * 2U]),
            byte_value(
                rom_bytes[
                    *resident_asset_table_offset + selector * 2U + 1U]));
        const auto asset = resolve_asset(rom_bytes, asset_reference);
        if (!asset) {
            return std::nullopt;
        }
        auto words = decode_pce_vram_stream(
            rom_bytes,
            asset->mpr,
            asset->source_address,
            maximum_output_words);
        if (!words
            || words->size()
                > kVramWordCount - kResidentPatternDestination) {
            return std::nullopt;
        }
        atlas.resident_uploads.push_back({
            .phase = static_cast<std::uint8_t>(selector),
            .destination_word = kResidentPatternDestination,
            .asset_reference = asset_reference,
            .source_bank = asset->physical_bank,
            .source_address = asset->source_address,
            .encoding = PceVramUploadEncoding::compressed_stream,
            .words = std::move(*words),
        });
    }
    std::map<std::uint16_t, std::uint8_t>
        descriptor_indices;
    for (std::size_t appearance = 0;
         appearance < appearance_pointers.size();
         ++appearance) {
        const auto pointer = appearance_pointers[appearance];
        const auto existing = descriptor_indices.find(pointer);
        if (existing != descriptor_indices.end()) {
            atlas.appearance_descriptor_indices[appearance] =
                existing->second;
            continue;
        }
        if (atlas.descriptors.size()
            > static_cast<std::size_t>(
                std::numeric_limits<std::uint8_t>::max())) {
            return std::nullopt;
        }
        const auto descriptor_index =
            static_cast<std::uint8_t>(atlas.descriptors.size());
        descriptor_indices.emplace(pointer, descriptor_index);
        atlas.appearance_descriptor_indices[appearance] =
            descriptor_index;

        if (pointer == kResidentPatternLoader) {
            atlas.descriptors.push_back({
                .logical_address = pointer,
                .resident = true,
            });
            continue;
        }
        const auto next = sorted_pointers.upper_bound(pointer);
        if (next == sorted_pointers.end()
            || *next <= pointer) {
            return std::nullopt;
        }
        const auto byte_count =
            static_cast<std::size_t>(*next - pointer);
        auto descriptor =
            decode_pce_pattern_loader_descriptor(
                rom_bytes,
                pointer,
                byte_count,
                pattern_base,
                maximum_output_words);
        if (!descriptor) {
            return std::nullopt;
        }
        atlas.descriptors.push_back(std::move(*descriptor));
    }
    return atlas;
}

namespace {

[[nodiscard]] std::optional<std::vector<FixedActorSprite>>
decode_actor_sprites(
    const std::span<const std::byte> rom_bytes,
    const std::span<const std::byte> work_ram,
    const std::size_t first_slot,
    const std::size_t last_slot,
    const int world_y_offset)
{
    const auto actors =
        devils_crush::cabinet::decode_table_actor_state(work_ram);
    const auto pointer_low_offset = descriptor_rom_offset(
        rom_bytes,
        kAppearancePointerLow,
        kAppearanceCount);
    const auto pointer_high_offset = descriptor_rom_offset(
        rom_bytes,
        kAppearancePointerHigh,
        kAppearanceCount);
    if (!actors || first_slot > last_slot
        || last_slot >= devils_crush::cabinet::kActorSlotCount
        || !pointer_low_offset || !pointer_high_offset
        || work_ram.size() <= kActorHighAttributeBase
                + last_slot
        || work_ram.size() <= kActorAppearancePhaseBase
                + last_slot
        || work_ram.size() <= kSatPatternBaseHigh) {
        return std::nullopt;
    }

    const auto pattern_base = little_endian_word(
        byte_value(work_ram[kSatPatternBaseLow]),
        byte_value(work_ram[kSatPatternBaseHigh]));
    std::vector<FixedActorSprite> sprites;
    for (std::size_t slot = first_slot;
         slot <= last_slot;
         ++slot) {
        const auto& actor = (*actors)[slot];
        if (!actor.occupied() || (actor.flags & 0x20U) != 0U) {
            continue;
        }
        if (actor.appearance >= kAppearanceCount) {
            return std::nullopt;
        }

        const auto animation_pointer = little_endian_word(
            byte_value(
                rom_bytes[*pointer_low_offset + actor.appearance]),
            byte_value(
                rom_bytes[*pointer_high_offset + actor.appearance]));
        const auto phase = static_cast<std::size_t>(
            byte_value(work_ram[kActorAppearancePhaseBase + slot]));
        if (phase
            > (std::numeric_limits<std::uint16_t>::max()
                - animation_pointer)
                / 2U) {
            return std::nullopt;
        }
        const auto phase_pointer = static_cast<std::uint16_t>(
            animation_pointer
            + static_cast<std::uint16_t>(phase * 2U));
        const auto phase_offset =
            descriptor_rom_offset(rom_bytes, phase_pointer, 2U);
        if (!phase_offset) {
            return std::nullopt;
        }
        const auto descriptor_pointer = little_endian_word(
            byte_value(rom_bytes[*phase_offset]),
            byte_value(rom_bytes[*phase_offset + 1U]));
        const auto descriptor_offset =
            descriptor_rom_offset(rom_bytes, descriptor_pointer);
        if (!descriptor_offset) {
            return std::nullopt;
        }
        const auto sprite_count = static_cast<std::size_t>(
            byte_value(rom_bytes[*descriptor_offset]));
        if (sprite_count == 0U
            || sprite_count > kMaximumSpritesPerActor
            || sprite_count
                > (std::numeric_limits<std::size_t>::max() - 1U)
                    / kSpriteDescriptorBytes) {
            return std::nullopt;
        }
        const auto descriptor_bytes =
            1U + sprite_count * kSpriteDescriptorBytes;
        if (!descriptor_rom_offset(
                rom_bytes,
                descriptor_pointer,
                descriptor_bytes)) {
            return std::nullopt;
        }

        const auto display_flags =
            byte_value(work_ram[kActorDisplayFlagsBase + slot]);
        for (std::size_t sprite_index = 0;
             sprite_index < sprite_count;
             ++sprite_index) {
            const auto offset =
                *descriptor_offset + 1U
                + sprite_index * kSpriteDescriptorBytes;
            const auto y_offset = static_cast<std::int8_t>(
                byte_value(rom_bytes[offset]));
            const auto x_offset = static_cast<std::int8_t>(
                byte_value(rom_bytes[offset + 1U]));
            auto pattern = little_endian_word(
                byte_value(rom_bytes[offset + 2U]),
                byte_value(rom_bytes[offset + 3U]));
            pattern = static_cast<std::uint16_t>(
                pattern + pattern_base);
            auto flags_low = byte_value(rom_bytes[offset + 4U]);
            auto flags_high = byte_value(rom_bytes[offset + 5U]);
            if ((display_flags & 0x10U) != 0U) {
                flags_low = static_cast<std::uint8_t>(
                    flags_low
                    | byte_value(
                        work_ram[kActorLowAttributeBase + slot]));
            }
            if ((display_flags & 0x20U) != 0U) {
                flags_high = static_cast<std::uint8_t>(
                    flags_high
                    | byte_value(
                        work_ram[kActorHighAttributeBase + slot]));
            }
            sprites.push_back({
                .actor_slot = static_cast<std::uint8_t>(slot),
                .world_x =
                    static_cast<int>(actor.world_x)
                    + static_cast<int>(x_offset) - kSpriteXBias,
                .world_y =
                    world_y_offset
                    + static_cast<int>(actor.world_y)
                    + static_cast<int>(y_offset),
                .pattern = pattern,
                .flags = little_endian_word(flags_low, flags_high),
            });
        }
    }
    return sprites;
}

} // namespace

std::optional<std::vector<FixedActorSprite>>
decode_fixed_actor_sprites(
    const std::span<const std::byte> rom_bytes,
    const std::span<const std::byte> work_ram)
{
    return decode_actor_sprites(
        rom_bytes,
        work_ram,
        devils_crush::cabinet::kFirstFixedActorSlot,
        devils_crush::cabinet::kLastFixedActorSlot,
        0);
}

std::optional<std::vector<FixedActorSprite>>
decode_ball_and_flipper_sprites(
    const std::span<const std::byte> rom_bytes,
    const std::span<const std::byte> work_ram)
{
    return decode_actor_sprites(
        rom_bytes,
        work_ram,
        0U,
        devils_crush::cabinet::kFirstFixedActorSlot - 1U,
        0);
}

std::optional<std::vector<FixedActorSprite>>
decode_dynamic_actor_sprites(
    const std::span<const std::byte> rom_bytes,
    const std::span<const std::byte> work_ram)
{
    return decode_actor_sprites(
        rom_bytes,
        work_ram,
        devils_crush::cabinet::kFirstDynamicActorSlot,
        devils_crush::cabinet::kLastDynamicActorSlot,
        0);
}

void PersistentActorSpriteCache::reset() noexcept
{
    actor_visuals_ = {};
    actor_visual_variants_ = {};
    left_initial_lower_viewport_ = false;
}

std::optional<PersistentActorOverlayStats>
PersistentActorSpriteCache::update_and_composite(
    ArgbImage& destination,
    const PceSpriteSnapshot& snapshot,
    const std::span<const std::byte> work_ram,
    const int viewport_world_y,
    const int destination_world_y,
    const int framebuffer_playfield_y,
    const int framebuffer_playfield_height,
    const std::span<const std::uint8_t> background_opaque,
    const std::span<const std::byte> rom_bytes,
    TableMotionFrame* const motion_frame)
{
    const auto actors =
        devils_crush::cabinet::decode_table_actor_state(work_ram);
    if (!actors
        || !valid_snapshot(
            destination,
            snapshot,
            framebuffer_playfield_height,
            background_opaque)) {
        return std::nullopt;
    }

    PersistentActorOverlayStats stats;
    constexpr std::size_t maximum_variants_per_actor = 32U;
    const auto remember_variant =
        [this, maximum_variants_per_actor](
            const std::size_t slot,
            const ActorVisual& visual) {
            if (!visual.learned || visual.sprites.empty()) {
                return;
            }
            auto& variants = actor_visual_variants_[slot];
            const auto existing = std::ranges::find_if(
                variants,
                [&visual](const ActorVisual& candidate) {
                    return candidate.type == visual.type
                        && candidate.appearance
                            == visual.appearance
                        && candidate.phase == visual.phase
                        && candidate.dispatch == visual.dispatch;
                });
            if (existing != variants.end()) {
                *existing = visual;
                return;
            }
            if (variants.size() >= maximum_variants_per_actor) {
                variants.erase(variants.begin());
            }
            variants.push_back(visual);
        };
    // The decode covers every slot, the ball included. A layer that omits
    // anything forces a second layer to cover for it, and the seam between
    // the two is what disagrees.
    constexpr std::size_t first_persistent =
        devils_crush::cabinet::kBallActorSlot;
    constexpr std::size_t last_persistent =
        devils_crush::cabinet::kLastDynamicActorSlot;
    constexpr int initial_lower_viewport_world_y = 480;
    if (viewport_world_y < initial_lower_viewport_world_y) {
        left_initial_lower_viewport_ = true;
    }
    const int viewport_end =
        viewport_world_y + framebuffer_playfield_height;

    std::optional<std::vector<FixedActorSprite>>
        metadata_sprites;
    if (!rom_bytes.empty()
        && work_ram.size() > kLoaderPatternBaseHigh) {
        const auto pattern_base = little_endian_word(
            byte_value(work_ram[kLoaderPatternBaseLow]),
            byte_value(work_ram[kLoaderPatternBaseHigh]));
        if (!pattern_atlas_
            || pattern_atlas_base_ != pattern_base) {
            pattern_atlas_ = decode_pce_pattern_atlas(
                rom_bytes,
                pattern_base);
            pattern_atlas_base_ = pattern_base;
        }
        if (pattern_atlas_) {
            metadata_sprites = decode_fixed_actor_sprites(
                rom_bytes,
                work_ram);
            const auto ball_and_flippers =
                decode_ball_and_flipper_sprites(
                    rom_bytes,
                    work_ram);
            if (!metadata_sprites || !ball_and_flippers) {
                metadata_sprites = std::nullopt;
            } else {
                std::ranges::copy_if(
                    *ball_and_flippers,
                    std::back_inserter(*metadata_sprites),
                    [](const FixedActorSprite&) { return true; });
                const auto dynamic_sprites =
                    decode_dynamic_actor_sprites(
                        rom_bytes,
                        work_ram);
                if (!dynamic_sprites) {
                    metadata_sprites = std::nullopt;
                } else {
                    std::ranges::copy(
                        *dynamic_sprites,
                        std::back_inserter(*metadata_sprites));
                }
            }
        }
    }
    std::array<
        std::vector<FixedActorSprite>,
        devils_crush::cabinet::kActorSlotCount>
        metadata_by_slot;
    if (metadata_sprites) {
        for (const auto& sprite : *metadata_sprites) {
            metadata_by_slot[sprite.actor_slot].push_back(sprite);
        }
    }

    std::array<bool, devils_crush::cabinet::kActorSlotCount>
        observed{};
    for (std::size_t slot = first_persistent;
         slot <= last_persistent;
         ++slot) {
        const auto& actor = (*actors)[slot];
        auto& visual = actor_visuals_[slot];
        if (!actor.occupied()) {
            visual = {};
            actor_visual_variants_[slot].clear();
            continue;
        }
        if ((actor.flags & 0x20U) != 0U) {
            // Bit 5 hides an actor. The ROM descriptor walk already omits
            // hidden actors, so without this the retained layer keeps
            // drawing whatever artwork the mechanism last had while live
            // work RAM continues to move it. Learned phase variants are
            // kept: hiding is a visibility state, not a family change.
            visual = {};
            continue;
        }

        const bool table_world_effect =
            slot
                    >= devils_crush::cabinet::
                           kFirstDynamicActorSlot
            && dynamic_presentation(actor)
                == DynamicPresentation::table_world_effect;

        const auto phase = byte_value(
            work_ram[kActorAppearancePhaseBase + slot]);
        const bool identity_matches =
            visual.type == actor.type
            && visual.appearance == actor.appearance
            && compatible_actor_dispatch(
                visual.type,
                visual.appearance,
                visual.dispatch,
                actor);
        const bool signature_matches =
            identity_matches && visual.phase == phase;
        const bool phase_animated_actor =
            (slot >= 0x17U && slot <= 0x1EU)
            || slot
                >= devils_crush::cabinet::
                       kFirstDynamicActorSlot;
        bool restored_phase_variant = false;
        std::optional<ActorVisual> static_phase_fallback;
        // Actor Y values are expressed in the VDC's 240-line presentation
        // space. The stitched table begins at the game's 208-line
        // playfield, below the 30-line score/status band.
        const int actor_table_y =
            static_cast<int>(actor.world_y)
            - framebuffer_playfield_y;
        // An actor straddling the camera window is drawn by the retained
        // layer from work-RAM coordinates while the live SAT is already
        // drawing it, producing two copies of one actor that disagree. The
        // obvious repair -- treating "any sprite reaches the window" as
        // observed -- is wrong: it predicts what the hardware *could* put in
        // the SAT, not what it does, because the ROM culls on its own terms
        // and the 64-sprite and 16-halves-per-line limits drop entries. On
        // the scripted route that geometric rule erased two enemies the SAT
        // was never drawing. Omission must therefore be driven by the SAT's
        // actual contents, not by geometry, so the anchor test stands until
        // that mapping exists.
        const bool anchor_in_viewport =
            actor.camera_active()
            && actor_table_y >= viewport_world_y
            && actor_table_y < viewport_end
            && actor.world_x >= -64
            && actor.world_x < destination.width + 64;
        const bool refresh_initial_seed =
            !left_initial_lower_viewport_
            && viewport_world_y == initial_lower_viewport_world_y
            && visual.seeded
            && !anchor_in_viewport;
        if (anchor_in_viewport) {
            observed[slot] = true;
            if (!metadata_sprites) {
                visual = {
                    .learned = true,
                    .seeded = false,
                    .type = actor.type,
                    .appearance = actor.appearance,
                    .phase = byte_value(
                        work_ram[
                            kActorAppearancePhaseBase + slot]),
                    .dispatch = actor.dispatch,
                };
            }
            ++stats.observed_fixed_actors;
            if (table_world_effect) {
                ++stats.observed_table_world_effects;
            }
        } else if (!signature_matches) {
            if (identity_matches && !phase_animated_actor
                && !visual.sprites.empty()) {
                static_phase_fallback = visual;
            }
            auto& variants = actor_visual_variants_[slot];
            const auto variant = std::ranges::find_if(
                variants,
                [&actor, phase](const ActorVisual& candidate) {
                    return candidate.type == actor.type
                        && candidate.appearance
                            == actor.appearance
                        && candidate.phase == phase
                        && compatible_actor_dispatch(
                            candidate.type,
                            candidate.appearance,
                            candidate.dispatch,
                            actor);
                });
            if (variant != variants.end()) {
                visual = *variant;
                restored_phase_variant = true;
                ++stats.restored_phase_variants;
                if (phase_animated_actor) {
                    ++stats.restored_animated_phase_variants;
                }
                if (slot >= 0x17U && slot <= 0x1EU) {
                    ++stats.restored_sorcerer_phase_variants;
                }
            } else {
                visual = {};
            }
        }

        // An off-camera actor whose phase moved must be re-resolved this
        // frame. Every path that gives up below leaves the previous artwork
        // in place while live position keeps moving it, so each is counted.
        const bool stale_off_camera =
            !anchor_in_viewport && !signature_matches;
        if (!pattern_atlas_ || !metadata_sprites) {
            continue;
        }
        if (!anchor_in_viewport && visual.learned
            && (signature_matches || restored_phase_variant)
            && !refresh_initial_seed) {
            // Position is applied from live actor RAM during composition.
            // Exact learned phase variants can therefore animate while
            // moving without sampling VRAM owned by another camera region.
            continue;
        }
        const PceVramUpload* upload = nullptr;
        if (actor.appearance < kPcePatternLoaderAppearanceCount) {
            const auto descriptor_index =
                pattern_atlas_->
                    appearance_descriptor_indices[actor.appearance];
            if (descriptor_index
                >= pattern_atlas_->descriptors.size()) {
                stats.stale_no_descriptor += stale_off_camera ? 1U : 0U;
                continue;
            }
            const auto& descriptor =
                pattern_atlas_->descriptors[descriptor_index];
            if (!descriptor.resident) {
                const auto selected = std::ranges::find(
                    descriptor.uploads,
                    phase,
                    &PceVramUpload::phase);
                if (selected != descriptor.uploads.end()) {
                    upload = &*selected;
                }
            }
        }
        const auto& actor_metadata = metadata_by_slot[slot];
        if (actor_metadata.empty()) {
            if (stale_off_camera) {
                ++stats.stale_no_metadata;
                if ((actor.flags & 0x20U) != 0U) {
                    ++stats.stale_no_metadata_hidden;
                }
            }
            continue;
        }

        const bool selected_upload_covers_actor =
            upload != nullptr
            && upload->destination_word < kVramWordCount
            && upload->words.size()
                <= kVramWordCount - upload->destination_word
            && std::ranges::all_of(
                actor_metadata,
                [&upload](const FixedActorSprite& sprite) {
                    return upload_covers_sprite(
                        *upload,
                        sprite.pattern,
                        sprite.flags);
                });
        // Patterns no decoded loader ever writes are resident: they sit at
        // the same VRAM addresses whatever region the camera is on, so live
        // VRAM is authoritative for them everywhere. Proving that here rather
        // than after the guard below is what lets a resident actor keep
        // animating off camera. Flippers and the lower-table enemies are
        // exactly this case, and holding their learned artwork instead froze
        // them the moment the ball left.
        const auto overlaps_actor =
            [&actor_metadata](const PceVramUpload& candidate) {
                return std::ranges::any_of(
                    actor_metadata,
                    [&candidate](const FixedActorSprite& sprite) {
                        return upload_overlaps_sprite(
                            candidate,
                            sprite.pattern,
                            sprite.flags);
                    });
            };
        // Only asked when it can change the outcome: an actor under the
        // camera, or one whose own upload already covers it, never needs it.
        const auto patterns_are_resident = [&] {
            return std::ranges::none_of(
                       pattern_atlas_->resident_uploads,
                       overlaps_actor)
                && std::ranges::all_of(
                       pattern_atlas_->descriptors,
                       [&overlaps_actor](
                           const PcePatternLoaderDescriptor& descriptor) {
                           return std::ranges::none_of(
                               descriptor.uploads,
                               overlaps_actor);
                       });
        };
        const bool resident_patterns =
            !anchor_in_viewport && !selected_upload_covers_actor
            && patterns_are_resident();
        if (!anchor_in_viewport
            && !selected_upload_covers_actor
            && !resident_patterns
            && !actor_visual_variants_[slot].empty()) {
            // A streamed phase that was never observed has no honest source:
            // live VRAM belongs to whichever region the camera is on. Hold the
            // learned artwork rather than corrupt the cache with it.
            if (static_phase_fallback) {
                visual = std::move(*static_phase_fallback);
                visual.phase = phase;
                visual.dispatch = actor.dispatch;
                ++stats.static_phase_fallbacks;
            } else {
                ++stats.missing_phase_variants;
            }
            continue;
        }
        // A sentinel or out-of-table appearance can use live VRAM before its
        // first camera visit, as can a stream-family actor whose selected
        // upload does not own its descriptor patterns.
        const bool use_live_static_patterns =
            anchor_in_viewport || resident_patterns;
        if (!selected_upload_covers_actor
            && !use_live_static_patterns) {
            stats.stale_unusable_patterns += stale_off_camera ? 1U : 0U;
            continue;
        }

        std::vector<std::uint16_t> atlas_vram;
        auto source_vram = snapshot.vram_words;
        if (!anchor_in_viewport
            && selected_upload_covers_actor) {
            atlas_vram.assign(kVramWordCount, 0U);
            std::copy(
                upload->words.begin(),
                upload->words.end(),
                atlas_vram.begin()
                    + static_cast<std::ptrdiff_t>(
                        upload->destination_word));
            source_vram = atlas_vram;
        }
        const PceSpriteSnapshot source_snapshot{
            .vram_words = source_vram,
            .sat_words = snapshot.sat_words,
            .palette_entries = snapshot.palette_entries,
            .memory_width_register =
                snapshot.memory_width_register,
        };
        ActorVisual resolved{
            .learned = true,
            .seeded = !anchor_in_viewport,
            .type = actor.type,
            .appearance = actor.appearance,
            .phase = phase,
            .dispatch = actor.dispatch,
        };
        bool resolved_valid = true;
        for (std::size_t sprite_index = 0;
             sprite_index < actor_metadata.size();
             ++sprite_index) {
            const auto& metadata = actor_metadata[sprite_index];
            const int width = sprite_width(metadata.flags);
            const int height = sprite_height(metadata.flags);
            const auto offset_x =
                metadata.world_x - static_cast<int>(actor.world_x);
            const auto offset_y =
                metadata.world_y
                - static_cast<int>(actor.world_y)
                - framebuffer_playfield_y;
            if (offset_x
                    < std::numeric_limits<std::int16_t>::min()
                || offset_x
                    > std::numeric_limits<std::int16_t>::max()
                || offset_y
                    < std::numeric_limits<std::int16_t>::min()
                || offset_y
                    > std::numeric_limits<std::int16_t>::max()
                || sprite_index
                    > static_cast<std::size_t>(
                        std::numeric_limits<std::uint8_t>::max())) {
                resolved_valid = false;
                break;
            }
            resolved.sprites.push_back({
                .offset_x =
                    static_cast<std::int16_t>(offset_x),
                .offset_y =
                    static_cast<std::int16_t>(offset_y),
                .pattern = metadata.pattern,
                .flags = metadata.flags,
                .sat_index =
                    static_cast<std::uint8_t>(sprite_index),
                .width = width,
                .height = height,
                .palette_indices =
                    std::vector<std::uint16_t>(
                        static_cast<std::size_t>(width)
                            * static_cast<std::size_t>(height)),
            });
            auto& cached = resolved.sprites.back();
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    const auto pixel = sprite_pixel(
                        source_snapshot,
                        metadata.pattern,
                        metadata.flags,
                        width,
                        height,
                        x,
                        y);
                    if (pixel == 0U) {
                        continue;
                    }
                    const auto palette_index =
                        256U
                        + static_cast<std::size_t>(
                              metadata.flags & 0x000FU)
                            * 16U
                        + static_cast<std::size_t>(pixel);
                    cached.palette_indices[
                        static_cast<std::size_t>(y)
                            * static_cast<std::size_t>(width)
                        + static_cast<std::size_t>(x)] =
                        static_cast<std::uint16_t>(palette_index);
                }
            }
        }
        if (resolved_valid && !resolved.sprites.empty()) {
            visual = std::move(resolved);
            if (slot >= 0x17U && slot <= 0x1EU) {
                ++stats.resolved_sorcerer_phase_variants;
            }
            if (anchor_in_viewport
                || selected_upload_covers_actor) {
                remember_variant(slot, visual);
            }
            if (!anchor_in_viewport) {
                ++stats.seeded_fixed_actors;
                if (table_world_effect) {
                    ++stats.seeded_table_world_effects;
                }
            }
        } else {
            stats.stale_resolve_failed += stale_off_camera ? 1U : 0U;
        }
    }

    // Record what each slot resolved to, so a caller can compare artwork
    // against phase directly instead of trusting a code path's own report.
    for (std::size_t slot = first_persistent;
         slot <= last_persistent;
         ++slot) {
        const auto& actor = (*actors)[slot];
        const auto& visual = actor_visuals_[slot];
        stats.actor_occupied[slot] = actor.occupied() ? 1U : 0U;
        stats.actor_on_camera[slot] = observed[slot] ? 1U : 0U;
        stats.actor_phase[slot] =
            byte_value(work_ram[kActorAppearancePhaseBase + slot]);
        std::uint32_t hash = 2166136261U;
        for (const auto& sprite : visual.sprites) {
            for (const std::uint32_t field : {
                     static_cast<std::uint32_t>(sprite.pattern),
                     static_cast<std::uint32_t>(sprite.flags),
                     static_cast<std::uint32_t>(
                         static_cast<std::uint16_t>(sprite.offset_x)),
                     static_cast<std::uint32_t>(
                         static_cast<std::uint16_t>(sprite.offset_y))}) {
                hash = (hash ^ field) * 16777619U;
            }
        }
        stats.actor_visual_hash[slot] =
            visual.sprites.empty() ? 0U : hash;
        std::uint32_t metadata_hash = 2166136261U;
        for (const auto& sprite : metadata_by_slot[slot]) {
            for (const std::uint32_t field : {
                     static_cast<std::uint32_t>(sprite.pattern),
                     static_cast<std::uint32_t>(sprite.flags),
                     static_cast<std::uint32_t>(
                         sprite.world_x - static_cast<int>(actor.world_x)),
                     static_cast<std::uint32_t>(
                         sprite.world_y
                         - static_cast<int>(actor.world_y))}) {
                metadata_hash = (metadata_hash ^ field) * 16777619U;
            }
        }
        stats.actor_metadata_hash[slot] =
            metadata_by_slot[slot].empty() ? 0U : metadata_hash;
    }

    // Without ROM descriptor metadata, retain the legacy live-SAT fallback
    // used by isolated callers and tests. Production cabinet composition
    // uses exact bank-$08 actor groups above; proximity cannot safely
    // associate clustered mechanisms such as sorcerers and pentagram parts.
    std::array<bool, kSpriteCount> selected_sprites{};
    const int framebuffer_end =
        framebuffer_playfield_y + framebuffer_playfield_height;
    for (int framebuffer_y = framebuffer_playfield_y;
         framebuffer_y < framebuffer_end;
         ++framebuffer_y) {
        std::size_t selected_halves = 0;
        for (int sprite_index = 0;
             sprite_index < kSpriteCount
             && selected_halves < kMaximumSpriteHalvesPerLine;
             ++sprite_index) {
            const auto sat_offset =
                static_cast<std::size_t>(
                    sprite_index * kSpriteWords);
            const auto raw_y = snapshot.sat_words[sat_offset];
            const auto flags = snapshot.sat_words[sat_offset + 3U];
            const int screen_y =
                static_cast<int>(raw_y & 0x03FFU) - kSpriteYBias;
            const int height = sprite_height(flags);
            if (framebuffer_y < screen_y
                || framebuffer_y >= screen_y + height) {
                continue;
            }
            const auto halves = static_cast<std::size_t>(
                sprite_width(flags) / 16);
            selected_halves = std::min(
                kMaximumSpriteHalvesPerLine,
                selected_halves + halves);
            selected_sprites[
                static_cast<std::size_t>(sprite_index)] = true;
        }
    }

    for (int sprite_index = 0;
         sprite_index < kSpriteCount;
         ++sprite_index) {
        if (metadata_sprites) {
            break;
        }
        if (!selected_sprites[
                static_cast<std::size_t>(sprite_index)]) {
            continue;
        }
        const auto sat_offset =
            static_cast<std::size_t>(sprite_index * kSpriteWords);
        const auto raw_y = snapshot.sat_words[sat_offset];
        const auto raw_x = snapshot.sat_words[sat_offset + 1U];
        const auto pattern = snapshot.sat_words[sat_offset + 2U];
        const auto flags = snapshot.sat_words[sat_offset + 3U];
        const int screen_x =
            static_cast<int>(raw_x & 0x03FFU) - kSpriteXBias;
        const int screen_y =
            static_cast<int>(raw_y & 0x03FFU) - kSpriteYBias;
        const int width = sprite_width(flags);
        const int height = sprite_height(flags);
        if (screen_x >= destination.width
            || screen_x + width <= 0
            || screen_y >= framebuffer_end
            || screen_y + height <= framebuffer_playfield_y) {
            continue;
        }

        const int sprite_world_y =
            viewport_world_y + screen_y - framebuffer_playfield_y;
        std::size_t best_slot =
            devils_crush::cabinet::kActorSlotCount;
        int best_distance = std::numeric_limits<int>::max();
        for (std::size_t slot = first_persistent;
             slot <= last_persistent;
             ++slot) {
            if (!observed[slot]) {
                continue;
            }
            const auto& actor = (*actors)[slot];
            const int distance_x =
                std::abs(screen_x - static_cast<int>(actor.world_x));
            const int distance_y =
                std::abs(
                    sprite_world_y
                    - static_cast<int>(actor.world_y));
            if (distance_x > 64 || distance_y > 64) {
                continue;
            }
            const int distance = distance_x + distance_y;
            if (distance < best_distance) {
                best_distance = distance;
                best_slot = slot;
            }
        }
        if (best_slot
            == devils_crush::cabinet::kActorSlotCount) {
            continue;
        }

        const auto& actor = (*actors)[best_slot];
        const int offset_x =
            screen_x - static_cast<int>(actor.world_x);
        const int offset_y =
            sprite_world_y - static_cast<int>(actor.world_y);
        if (offset_x < std::numeric_limits<std::int16_t>::min()
            || offset_x > std::numeric_limits<std::int16_t>::max()
            || offset_y < std::numeric_limits<std::int16_t>::min()
            || offset_y > std::numeric_limits<std::int16_t>::max()) {
            continue;
        }
        auto& cached_sprites =
            actor_visuals_[best_slot].sprites;
        cached_sprites.push_back({
            .offset_x = static_cast<std::int16_t>(offset_x),
            .offset_y = static_cast<std::int16_t>(offset_y),
            .pattern = pattern,
            .flags = flags,
            .sat_index = static_cast<std::uint8_t>(sprite_index),
            .width = width,
            .height = height,
            .palette_indices = std::vector<std::uint16_t>(
                static_cast<std::size_t>(width)
                    * static_cast<std::size_t>(height)),
        });
        auto& cached = cached_sprites.back();
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const auto pixel = sprite_pixel(
                    snapshot,
                    pattern,
                    flags,
                    width,
                    height,
                    x,
                    y);
                if (pixel == 0U) {
                    continue;
                }
                const auto palette_index =
                    256U
                    + static_cast<std::size_t>(flags & 0x000FU)
                        * 16U
                    + static_cast<std::size_t>(pixel);
                cached.palette_indices[
                    static_cast<std::size_t>(y)
                        * static_cast<std::size_t>(width)
                    + static_cast<std::size_t>(x)] =
                    static_cast<std::uint16_t>(palette_index);
            }
        }
    }

    if (!metadata_sprites) {
        for (std::size_t slot = first_persistent;
             slot <= last_persistent;
             ++slot) {
            if (observed[slot]) {
                remember_variant(slot, actor_visuals_[slot]);
            }
        }
    }

    struct WorldSprite {
        std::size_t actor_slot = 0;
        std::uint8_t sat_index = 0;
        int world_x = 0;
        int world_y = 0;
        std::uint16_t pattern = 0;
        std::uint16_t flags = 0;
        int width = 0;
        int height = 0;
        const std::vector<std::uint16_t>* palette_indices = nullptr;
    };
    std::vector<WorldSprite> world_sprites;
    std::array<bool, devils_crush::cabinet::kActorSlotCount>
        actor_rendered{};
    for (std::size_t slot = first_persistent;
         slot <= last_persistent;
         ++slot) {
        const auto& actor = (*actors)[slot];
        const auto& visual = actor_visuals_[slot];
        // 01:DD6E registers a main-table drain at Y >= $02D0. The
        // end-of-ball coroutine leaves the ball there while it waits, so
        // the top of its sprite can still peek over the playfield edge.
        // Suppress only its presentation; the next launch restores it as
        // soon as its position returns above the drain. Bonus stages use
        // their own boundaries and must not inherit this table cutoff.
        if (slot == devils_crush::cabinet::kBallActorSlot
            && byte_value(work_ram[0x0055U]) == 0U
            && (byte_value(work_ram[0x0053U]) & 0x02U) != 0U
            && actor.world_y >= 0x02D0) {
            continue;
        }
        if (!actor.occupied() || !visual.learned
            || visual.type != actor.type
            || visual.appearance != actor.appearance
            || visual.phase
                != byte_value(
                    work_ram[kActorAppearancePhaseBase + slot])
            || !compatible_actor_dispatch(
                visual.type,
                visual.appearance,
                visual.dispatch,
                actor)) {
            continue;
        }
        if (!visual.sprites.empty()) {
            ++stats.cached_fixed_actors;
            if (slot
                    >= devils_crush::cabinet::
                           kFirstFlipperControlSlot
                && slot
                    <= devils_crush::cabinet::
                           kLastFlipperControlSlot) {
                ++stats.cached_flipper_actors;
            }
            if (dynamic_presentation(actor)
                == DynamicPresentation::table_world_effect) {
                ++stats.cached_table_world_effects;
            }
        }
        // Every occupied actor is drawn from work RAM, on camera or off.
        // Handing observed actors to the live SAT is what split the table
        // into two layers with different time bases, and the seam between
        // them is what disagreed. `observed` still drives learning, because
        // that is the only moment an actor's artwork is in live VRAM.
        for (const auto& sprite : visual.sprites) {
            world_sprites.push_back({
                .actor_slot = slot,
                .sat_index = sprite.sat_index,
                .world_x =
                    static_cast<int>(actor.world_x)
                    + static_cast<int>(sprite.offset_x),
                .world_y =
                    static_cast<int>(actor.world_y)
                    + static_cast<int>(sprite.offset_y),
                .pattern = sprite.pattern,
                .flags = sprite.flags,
                .width = sprite.width,
                .height = sprite.height,
                .palette_indices = &sprite.palette_indices,
            });
        }
    }
    std::ranges::sort(
        world_sprites,
        {},
        [](const WorldSprite& sprite) {
            return std::tuple{
                sprite.actor_slot,
                sprite.sat_index,
            };
        });

    if (motion_frame != nullptr) {
        motion_frame->sprites.clear();
        for (auto it = world_sprites.rbegin(); it != world_sprites.rend(); ++it) {
            const auto& sprite = *it;
            if (sprite.palette_indices == nullptr) continue;
            TableMotionSprite draw;
            draw.actor = (*actors)[sprite.actor_slot];
            draw.x = sprite.world_x;
            draw.y = sprite.world_y - destination_world_y;
            draw.foreground = (sprite.flags & 0x0080U) != 0U;
            draw.image = {sprite.width, sprite.height, {}};
            draw.image.pixels.reserve(sprite.palette_indices->size());
            for (const auto index : *sprite.palette_indices) {
                draw.image.pixels.push_back(index != 0U
                    && index < snapshot.palette_entries.size()
                    ? decode_pce_color(snapshot.palette_entries[index]) : 0U);
            }
            motion_frame->sprites.push_back(std::move(draw));
        }
    }

    struct WorldSlice {
        std::size_t sprite_index = 0;
        int horizontal_half = 0;
    };
    std::vector<WorldSlice> scanline_sprites;
    for (int output_y = 0;
         output_y < destination.height;
         ++output_y) {
        const int world_y = destination_world_y + output_y;
        scanline_sprites.clear();
        for (std::size_t sprite_index = 0;
             sprite_index < world_sprites.size();
             ++sprite_index) {
            const auto& sprite = world_sprites[sprite_index];
            const int height = sprite.height;
            if (world_y < sprite.world_y
                || world_y >= sprite.world_y + height) {
                continue;
            }
            const int halves = sprite.width / 16;
            for (int half = 0; half < halves; ++half) {
                scanline_sprites.push_back({
                    .sprite_index = sprite_index,
                    .horizontal_half = half,
                });
            }
        }

        // The persistent layer expands actors retained from different
        // camera visits onto one table. They did not coexist in one copied
        // SAT, so applying a single 16-half hardware budget here makes
        // off-camera regions suppress one another. The live SAT composed
        // afterward still enforces the real per-camera scanline limit.
        for (auto slice = scanline_sprites.rbegin();
             slice != scanline_sprites.rend();
             ++slice) {
            const auto& sprite =
                world_sprites[slice->sprite_index];
            const int width = sprite.width;
            const int height = sprite.height;
            const int sprite_y = world_y - sprite.world_y;
            for (int half_x = 0; half_x < 16; ++half_x) {
                const int sprite_x =
                    slice->horizontal_half * 16 + half_x;
                const int output_x = sprite.world_x + sprite_x;
                if (output_x < 0
                    || output_x >= destination.width) {
                    continue;
                }
                if (sprite.palette_indices == nullptr
                    || sprite.palette_indices->size()
                        != static_cast<std::size_t>(width)
                            * static_cast<std::size_t>(height)) {
                    continue;
                }
                const auto palette_index =
                    (*sprite.palette_indices)[
                        static_cast<std::size_t>(sprite_y)
                            * static_cast<std::size_t>(width)
                        + static_cast<std::size_t>(sprite_x)];
                if (palette_index == 0U
                    || palette_index
                        >= snapshot.palette_entries.size()) {
                    continue;
                }
                const auto resolved_pixel = decode_pce_color(
                    snapshot.palette_entries[palette_index]);
                const auto output_offset =
                    static_cast<std::size_t>(output_y)
                        * static_cast<std::size_t>(destination.width)
                    + static_cast<std::size_t>(output_x);
                const bool foreground_priority =
                    (sprite.flags & 0x0080U) != 0U;
                if (!foreground_priority
                    && !background_opaque.empty()
                    && background_opaque[output_offset] != 0U) {
                    continue;
                }
                destination.pixels[output_offset] = resolved_pixel;
                actor_rendered[sprite.actor_slot] = true;
                if (sprite.actor_slot
                        >= devils_crush::cabinet::
                               kFirstFlipperControlSlot
                    && sprite.actor_slot
                        <= devils_crush::cabinet::
                               kLastFlipperControlSlot) {
                    ++stats.flipper_opaque_pixel_writes;
                }
                ++stats.opaque_pixel_writes;
            }
        }
    }
    for (std::size_t slot = 0;
         slot < actor_rendered.size();
         ++slot) {
        if (actor_rendered[slot]) {
            ++stats.rendered_fixed_actors;
            if (slot
                    >= devils_crush::cabinet::
                           kFirstFlipperControlSlot
                && slot
                    <= devils_crush::cabinet::
                           kLastFlipperControlSlot) {
                ++stats.rendered_flipper_actors;
            }
            const bool table_world_effect =
                dynamic_presentation((*actors)[slot])
                == DynamicPresentation::table_world_effect;
            if (table_world_effect) {
                ++stats.rendered_table_world_effects;
            }
            if (!observed[slot]) {
                ++stats.rendered_retained_fixed_actors;
                if (table_world_effect) {
                    ++stats.rendered_retained_table_world_effects;
                }
            }
            if (actor_visuals_[slot].seeded) {
                ++stats.rendered_seeded_fixed_actors;
            }
        }
    }
    return stats;
}

std::optional<SpriteOverlayStats> composite_pce_sprites(
    ArgbImage& destination,
    const PceSpriteSnapshot& snapshot,
    const int viewport_world_y,
    const int destination_world_y,
    const int framebuffer_playfield_y,
    const int framebuffer_playfield_height,
    const std::span<const std::uint8_t> background_opaque)
{
    if (!valid_snapshot(
            destination,
            snapshot,
            framebuffer_playfield_height,
            background_opaque)) {
        return std::nullopt;
    }

    SpriteOverlayStats stats;
    std::array<bool, kSpriteCount> visible_sprites{};
    std::vector<SpriteSlice> scanline_sprites;
    scanline_sprites.reserve(kMaximumSpriteHalvesPerLine);

    const int framebuffer_end =
        framebuffer_playfield_y + framebuffer_playfield_height;
    for (int framebuffer_y = framebuffer_playfield_y;
         framebuffer_y < framebuffer_end;
         ++framebuffer_y) {
        scanline_sprites.clear();
        // Hardware scans low SAT indices first and stops after 16
        // 16-pixel-wide sprite halves. A 32-pixel sprite consumes two slots.
        for (int sprite_index = 0;
             sprite_index < kSpriteCount
             && scanline_sprites.size()
                 < kMaximumSpriteHalvesPerLine;
             ++sprite_index) {
            const auto sat_offset =
                static_cast<std::size_t>(
                    sprite_index * kSpriteWords);
            const auto raw_y = snapshot.sat_words[sat_offset];
            const auto flags = snapshot.sat_words[sat_offset + 3U];
            const int screen_y =
                static_cast<int>(raw_y & 0x03FFU) - kSpriteYBias;
            const int height = sprite_height(flags);
            if (framebuffer_y < screen_y
                || framebuffer_y >= screen_y + height) {
                continue;
            }

            const int horizontal_halves =
                (flags & 0x0100U) != 0U ? 2 : 1;
            for (int half = 0;
                 half < horizontal_halves
                 && scanline_sprites.size()
                     < kMaximumSpriteHalvesPerLine;
                 ++half) {
                scanline_sprites.push_back({
                    .sprite_index = sprite_index,
                    .horizontal_half = half,
                });
            }
        }

        // Lower SAT indices own overlap pixels, so paint selected halves in
        // reverse selection order.
        for (auto slice = scanline_sprites.rbegin();
             slice != scanline_sprites.rend();
             ++slice) {
            const auto sat_offset =
                static_cast<std::size_t>(
                    slice->sprite_index * kSpriteWords);
            const auto raw_y = snapshot.sat_words[sat_offset];
            const auto raw_x = snapshot.sat_words[sat_offset + 1U];
            const auto pattern = snapshot.sat_words[sat_offset + 2U];
            const auto flags = snapshot.sat_words[sat_offset + 3U];
            const int screen_x =
                static_cast<int>(raw_x & 0x03FFU) - kSpriteXBias;
            const int screen_y =
                static_cast<int>(raw_y & 0x03FFU) - kSpriteYBias;
            const int width = sprite_width(flags);
            const int height = sprite_height(flags);
            const int sprite_y = framebuffer_y - screen_y;
            const int output_y = viewport_world_y
                + framebuffer_y - framebuffer_playfield_y
                - destination_world_y;
            if (output_y < 0 || output_y >= destination.height) {
                continue;
            }

            for (int half_x = 0; half_x < 16; ++half_x) {
                const int sprite_x =
                    slice->horizontal_half * 16 + half_x;
                const int framebuffer_x = screen_x + sprite_x;
                if (framebuffer_x < 0
                    || framebuffer_x >= destination.width) {
                    continue;
                }
                const auto pixel = sprite_pixel(
                    snapshot,
                    pattern,
                    flags,
                    width,
                    height,
                    sprite_x,
                    sprite_y);
                if (pixel == 0U) {
                    continue;
                }
                const auto palette_index =
                    256U
                    + static_cast<std::size_t>(flags & 0x000FU) * 16U
                    + static_cast<std::size_t>(pixel);
                const auto output_offset =
                    static_cast<std::size_t>(output_y)
                        * static_cast<std::size_t>(destination.width)
                    + static_cast<std::size_t>(framebuffer_x);
                const bool foreground_priority =
                    (flags & 0x0080U) != 0U;
                if (!foreground_priority
                    && !background_opaque.empty()
                    && background_opaque[output_offset] != 0U) {
                    continue;
                }
                destination.pixels[output_offset] =
                    decode_pce_color(
                        snapshot.palette_entries[palette_index]);
                ++stats.opaque_pixel_writes;
                visible_sprites[
                    static_cast<std::size_t>(
                        slice->sprite_index)] = true;
            }
        }
    }
    for (const bool visible : visible_sprites) {
        if (visible) {
            ++stats.visible_sprites;
        }
    }
    return stats;
}

} // namespace devils_crush::render
