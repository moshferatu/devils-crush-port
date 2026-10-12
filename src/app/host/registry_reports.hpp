// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"
#include "cabinet_table.hpp"
#include "host_files.hpp"
#include "native_registry_install.hpp"

namespace devils_crush::app::host {

void print_native_registry_summary(
    const NativeRegistryDeployment& deployment,
    const ReplacementNesting nesting,
    const devils_crush::emulation::PceRuntime& runtime,
    const bool detailed = true,
    const bool cabinet_flipper_hook = false);

// Per-frame census of which table tiles' VRAM pattern words change, split by
// whether the tile is inside the camera window.
//
// The cabinet decodes the whole 32x96 table from live VRAM every frame, but
// the PC Engine only holds valid pattern data for the region the camera is
// in. An off-camera tile whose words change is therefore a tile the cabinet
// will draw differently for no in-game reason — the table appearing to
// re-render as the ball travels.
class TableVramCensus {
public:
    static constexpr std::size_t kWidthTiles = 32U;
    static constexpr std::size_t kHeightTiles = 96U;
    static constexpr std::size_t kTileWords = 16U;
    static constexpr std::size_t kTilePixels = 8U;
    static constexpr std::size_t kTileCount = kWidthTiles * kHeightTiles;
    static constexpr std::size_t kRomOffset = 0x14U * 0x2000U + 0x000AU;
    // The camera window is the playfield band of the 240-line presentation.
    static constexpr int kCameraWindowRows = 208;
    // decode_cabinet_table maps the camera through this origin to reach the
    // decoded source's row space.
    static constexpr int kViewportOrigin = 480;

    [[nodiscard]] bool begin(const std::span<const std::byte> rom)
    {
        if (rom.size() < kRomOffset + kTileCount * 2U) {
            return false;
        }
        tile_numbers_.resize(kTileCount);
        for (std::size_t index = 0U; index < kTileCount; ++index) {
            const auto low = std::to_integer<std::uint16_t>(
                rom[kRomOffset + index * 2U]);
            const auto high = static_cast<std::uint16_t>(
                std::to_integer<std::uint8_t>(
                    rom[kRomOffset + index * 2U + 1U])
                & 0x0FU);
            tile_numbers_[index] =
                static_cast<std::uint16_t>(low | (high << 8U));
        }
        return true;
    }

    void observe(
        const int frame,
        const std::span<const std::byte> work_ram,
        const std::span<const std::uint16_t> vram_words)
    {
        if (tile_numbers_.empty() || work_ram.size() <= 0x0CU) {
            return;
        }

        const auto camera_y = static_cast<std::int16_t>(
            static_cast<std::uint16_t>(
                std::to_integer<std::uint8_t>(work_ram[0x0BU]))
            | (static_cast<std::uint16_t>(
                   std::to_integer<std::uint8_t>(work_ram[0x0CU]))
               << 8U));
        const auto window_top = kViewportOrigin + static_cast<int>(camera_y);
        const auto window_bottom = window_top + kCameraWindowRows;

        std::vector<std::uint64_t> hashes(kTileCount);
        for (std::size_t index = 0U; index < kTileCount; ++index) {
            const std::size_t offset =
                static_cast<std::size_t>(tile_numbers_[index]) * kTileWords;
            std::uint64_t hash = 1469598103934665603ULL;
            for (std::size_t word = 0U; word < kTileWords; ++word) {
                const auto value = offset + word < vram_words.size()
                    ? vram_words[offset + word]
                    : std::uint16_t{0};
                hash = (hash ^ value) * 1099511628211ULL;
            }
            hashes[index] = hash;
        }

        // The same retention the cabinet applies, measured against the live
        // plane so the suppressed off-camera churn is visible.
        std::uint64_t suppressed = 0U;
        std::size_t learned = 0U;
        constexpr std::size_t kAttributeOffset = 0x0259U;
        if (work_ram.size()
            >= kAttributeOffset
                + devils_crush::render::TablePaletteCache::kByteCount) {
            const auto live = work_ram.subspan(
                kAttributeOffset,
                devils_crush::render::TablePaletteCache::kByteCount);
            const auto retained = palette_cache_.update(
                live,
                window_top,
                kCameraWindowRows);
            for (std::size_t byte = 0U; byte < retained.size(); ++byte) {
                if (retained[byte] != live[byte]) {
                    ++suppressed;
                }
            }
            learned = palette_cache_.learned_rows();
            total_suppressed_ += suppressed;
        }

        if (!previous_.empty()) {
            std::uint64_t on_camera = 0U;
            std::uint64_t off_camera = 0U;
            for (std::size_t index = 0U; index < kTileCount; ++index) {
                if (hashes[index] == previous_[index]) {
                    continue;
                }
                const auto tile_row =
                    static_cast<int>(index / kWidthTiles);
                const auto row_top =
                    tile_row * static_cast<int>(kTilePixels);
                const auto row_bottom =
                    row_top + static_cast<int>(kTilePixels);
                if (row_bottom > window_top && row_top < window_bottom) {
                    ++on_camera;
                } else {
                    ++off_camera;
                }
            }
            rows_ << frame << ',' << window_top << ','
                  << on_camera + off_camera << ',' << on_camera << ','
                  << off_camera << ',' << suppressed << ',' << learned
                  << '\n';
            total_on_camera_ += on_camera;
            total_off_camera_ += off_camera;
            if (off_camera != 0U) {
                ++frames_with_off_camera_change_;
            }
            ++compared_frames_;
        }
        previous_ = std::move(hashes);
    }

    [[nodiscard]] bool write(
        const std::filesystem::path& path,
        const std::string_view core_revision) const
    {
        std::error_code error;
        const auto directory = path.parent_path();
        if (!directory.empty()) {
            std::filesystem::create_directories(directory, error);
        }
        if (error) {
            std::cerr << "Could not create table VRAM census directory: "
                      << error.message() << '\n';
            return false;
        }

        auto temporary_path = path;
        temporary_path += ".tmp";
        std::ofstream output(
            temporary_path,
            std::ios::binary | std::ios::trunc);
        if (!output) {
            std::cerr << "Could not create table VRAM census: "
                      << temporary_path.string() << '\n';
            return false;
        }
        output << "# format=devils-crush-table-vram-census-v1\n"
               << "# rom_sha256=" << devils_crush::rom::kSupportedSha256
               << '\n'
               << "# core_revision=" << core_revision << '\n'
               << "# table_tiles=" << kTileCount << '\n'
               << "# compared_frames=" << compared_frames_ << '\n'
               << "# frames_with_off_camera_change="
               << frames_with_off_camera_change_ << '\n'
               << "# total_on_camera_tile_changes=" << total_on_camera_
               << '\n'
               << "# total_off_camera_tile_changes=" << total_off_camera_
               << '\n'
               << "# total_suppressed_palette_bytes=" << total_suppressed_
               << '\n'
               << "frame,camera_top_row,tiles_changed,on_camera,off_camera,"
                  "retained_palette_bytes_differing,learned_rows\n"
               << rows_.str();
        output.close();
        if (!output) {
            std::cerr << "Could not finalize table VRAM census.\n";
            return false;
        }
        if (!commit_artifact(temporary_path, path, "table VRAM census")) {
            return false;
        }

        std::cout << "Table VRAM census complete: " << compared_frames_
                  << " compared frames, " << frames_with_off_camera_change_
                  << " with off-camera tile changes ("
                  << total_off_camera_ << " off-camera vs "
                  << total_on_camera_ << " on-camera tile changes)\n"
                  << "Palette retention held back " << total_suppressed_
                  << " off-camera attribute bytes across the route\n"
                  << "Output: " << path.string() << '\n';
        return true;
    }

private:
    std::vector<std::uint16_t> tile_numbers_;
    std::vector<std::uint64_t> previous_;
    std::ostringstream rows_;
    std::uint64_t total_on_camera_ = 0U;
    std::uint64_t total_off_camera_ = 0U;
    std::uint64_t compared_frames_ = 0U;
    std::uint64_t frames_with_off_camera_change_ = 0U;
    std::uint64_t total_suppressed_ = 0U;
    devils_crush::render::TablePaletteCache palette_cache_;
};

} // namespace devils_crush::app::host
