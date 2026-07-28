// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/core/backup_ram.hpp"
#include "devils_crush/core/rom_image.hpp"
#include "devils_crush/core/save_state.hpp"
#include "devils_crush/emulation/pce_runtime.hpp"
#include "devils_crush/game_logic/physics.h"
#include "devils_crush/render/pce_background.hpp"
#include "devils_crush/render/sharp_pixel_scaler.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace {

int failures = 0;

void check(const bool condition, const std::string_view description)
{
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

void check_rect(
    const devils_crush::render::IntRect actual,
    const devils_crush::render::IntRect expected,
    const std::string_view description)
{
    check(
        actual.x == expected.x && actual.y == expected.y
            && actual.width == expected.width
            && actual.height == expected.height,
        description);
}

void test_hash_vectors()
{
    constexpr std::array<std::byte, 0> empty{};
    const auto empty_digests =
        devils_crush::rom::compute_digests(empty);
    check(
        empty_digests.md5 == "d41d8cd98f00b204e9800998ecf8427e",
        "MD5 empty vector");
    check(
        empty_digests.sha256
            == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991"
               "b7852b855",
        "SHA-256 empty vector");

    constexpr std::array bytes{
        std::byte{'a'},
        std::byte{'b'},
        std::byte{'c'},
    };
    const auto digests = devils_crush::rom::compute_digests(bytes);
    check(
        digests.md5 == "900150983cd24fb0d6963f7d28e17f72",
        "MD5 known vector");
    check(
        digests.sha256
            == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61"
               "f20015ad",
        "SHA-256 known vector");
}

void test_native_cached_position_restore()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    constexpr std::array<std::uint8_t, DC_CACHED_POSITION_BYTE_COUNT>
        cached{0x12U, 0x34U, 0x56U, 0x78U, 0x9AU, 0xBCU};
    std::copy(
        cached.begin(),
        cached.end(),
        work_ram.begin() + 0x85);

    dc_cached_position_restore restore{};
    check(
        dc_restore_cached_position(
            work_ram.data(),
            work_ram.size(),
            3U,
            &restore)
            != 0,
        "native F241 accepts a complete work-RAM snapshot");
    constexpr std::array<std::uint16_t, DC_CACHED_POSITION_BYTE_COUNT>
        expected_offsets{
            0x1103U,
            0x1183U,
            0x1203U,
            0x1283U,
            0x1303U,
            0x1383U,
        };
    check(
        std::ranges::equal(
            restore.work_ram_offsets,
            expected_offsets),
        "native F241 indexes all six position arrays");
    check(
        std::ranges::equal(restore.values, cached),
        "native F241 returns all six cached bytes in store order");
    check(
        dc_restore_cached_position(
            work_ram.data(),
            work_ram.size() - 1U,
            0U,
            &restore)
            == 0,
        "native F241 rejects a truncated work-RAM snapshot");
}

void test_supported_rom_metadata()
{
    const devils_crush::rom::Digests expected{
        .md5 = std::string(devils_crush::rom::kSupportedMd5),
        .sha256 = std::string(devils_crush::rom::kSupportedSha256),
    };
    check(
        devils_crush::rom::matches_supported_rom(
            devils_crush::rom::kSupportedRomSize,
            expected),
        "supported ROM metadata is accepted");

    auto changed = expected;
    changed.sha256.front() = '0';
    check(
        !devils_crush::rom::matches_supported_rom(
            devils_crush::rom::kSupportedRomSize,
            changed),
        "changed ROM hash is rejected");
    check(
        !devils_crush::rom::matches_supported_rom(
            devils_crush::rom::kSupportedRomSize + 1U,
            expected),
        "changed ROM size is rejected");
}

void test_scaler_examples()
{
    using devils_crush::render::IntRect;
    using devils_crush::render::calculate_scale_layout;

    const auto hd =
        calculate_scale_layout(1920, 1080, 256, 240, 4U, 3U);
    check_rect(hd.destination, IntRect{240, 0, 1440, 1080}, "1080p fit");
    check(
        hd.prescale_x == 5 && hd.prescale_y == 4
            && hd.stage_width == 1280 && hd.stage_height == 960,
        "1080p prescale");

    const auto qhd =
        calculate_scale_layout(2560, 1440, 256, 240, 4U, 3U);
    check_rect(qhd.destination, IntRect{320, 0, 1920, 1440}, "1440p fit");
    check(
        qhd.prescale_x == 7 && qhd.prescale_y == 6
            && qhd.stage_width == 1792 && qhd.stage_height == 1440,
        "1440p prescale");

    const auto uhd =
        calculate_scale_layout(3840, 2160, 256, 240, 4U, 3U);
    check_rect(uhd.destination, IntRect{480, 0, 2880, 2160}, "4K fit");
    check(
        uhd.prescale_x == 11 && uhd.prescale_y == 9
            && uhd.stage_width == 2816 && uhd.stage_height == 2160,
        "4K prescale");

    const auto five_four =
        calculate_scale_layout(1280, 1024, 256, 240, 4U, 3U);
    check_rect(
        five_four.destination,
        IntRect{0, 32, 1280, 960},
        "5:4 monitor fit");

    const auto odd =
        calculate_scale_layout(1366, 768, 256, 240, 4U, 3U);
    check_rect(
        odd.destination,
        IntRect{171, 0, 1024, 768},
        "odd-width monitor fit");
}

void test_scaler_edges()
{
    using devils_crush::render::calculate_scale_layout;

    check(
        !calculate_scale_layout(0, 1080, 256, 240, 4U, 3U).valid(),
        "zero output is invalid");
    check(
        !calculate_scale_layout(1920, 1080, 0, 240, 4U, 3U).valid(),
        "zero source is invalid");
    check(
        !calculate_scale_layout(1920, 1080, 256, 240, 0U, 3U).valid(),
        "zero aspect is invalid");

    const auto portrait =
        calculate_scale_layout(1080, 1920, 256, 240, 4U, 3U);
    check(portrait.valid(), "portrait layout is valid");
    check(
        portrait.destination.x >= 0 && portrait.destination.y >= 0
            && portrait.destination.x + portrait.destination.width <= 1080
            && portrait.destination.y + portrait.destination.height <= 1920,
        "portrait layout stays inside output");
    check(
        portrait.stage_width % 256 == 0
            && portrait.stage_height % 240 == 0,
        "stage uses exact source multiples");

    constexpr std::array output_sizes{
        std::pair{1, 1},
        std::pair{255, 239},
        std::pair{1001, 1000},
        std::pair{1920, 1200},
        std::pair{2160, 3840},
        std::pair{7680, 4320},
    };
    for (const auto [width, height] : output_sizes) {
        const auto layout =
            calculate_scale_layout(width, height, 256, 240, 4U, 3U);
        check(layout.valid(), "property layout is valid");
        check(
            layout.destination.x >= 0 && layout.destination.y >= 0
                && layout.destination.x + layout.destination.width <= width
                && layout.destination.y + layout.destination.height <= height,
            "property layout stays inside output");
        check(
            layout.destination.width == width
                || layout.destination.height == height,
            "property layout fills one output dimension");
        check(
            layout.stage_width % 256 == 0
                && layout.stage_height % 240 == 0,
            "property stage uses exact source multiples");
    }

    const auto four_three =
        calculate_scale_layout(1920, 1200, 256, 240, 4U, 3U);
    const auto eight_six =
        calculate_scale_layout(1920, 1200, 256, 240, 8U, 6U);
    check_rect(
        four_three.destination,
        eight_six.destination,
        "equivalent aspect ratios produce the same fit");

    const auto maximum = calculate_scale_layout(
        std::numeric_limits<int>::max(),
        std::numeric_limits<int>::max(),
        1,
        1,
        std::numeric_limits<std::uint32_t>::max(),
        1U);
    check(maximum.valid(), "maximum integer inputs remain valid");
}

void test_pce_background_decode()
{
    std::array<std::uint16_t, 48> vram{};
    std::array<std::uint16_t, 256> palette{};

    // Two BAT cells select tiles 1 and 2. Tile 1 uses subpalette 3.
    vram[0] = 0x3001U;
    vram[1] = 0x0002U;

    // Tile 1, row 0: color 3 at the left edge and color 1 at the right.
    vram[16] = 0x8081U;
    // Tile 1, row 1: color 4 at the left edge.
    vram[25] = 0x0080U;
    palette[0] = 0x0007U;
    palette[0x31] = 0x0038U;
    palette[0x33] = 0x01C0U;
    palette[0x34] = 0x01FFU;

    const auto decoded =
        devils_crush::render::decode_pce_background({
            .vram_words = vram,
            .palette_entries = palette,
            .bat_width_tiles = 2,
            .bat_height_tiles = 1,
        });
    check(decoded.has_value(), "PCE background snapshot decodes");
    if (!decoded) {
        return;
    }

    check(
        decoded->width == 16 && decoded->height == 8
            && decoded->pixels.size() == 128U,
        "PCE BAT dimensions expand to 8x8 tiles");
    check(
        decoded->pixels[0] == 0xFF00FC00U,
        "PCE planar bit order and subpalette selection");
    check(
        decoded->pixels[7] == 0xFFFC0000U,
        "PCE tile right-edge bit order");
    check(
        decoded->pixels[16] == 0xFFFCFCFCU,
        "PCE upper bitplanes decode");
    check(
        decoded->pixels[1] == 0xFF0000FCU,
        "PCE background color zero uses shared palette entry zero");

    auto invalid_vram = vram;
    invalid_vram[1] = 0x0FFFU;
    const auto dormant_tile =
        devils_crush::render::decode_pce_background({
            .vram_words = invalid_vram,
            .palette_entries = palette,
            .bat_width_tiles = 2,
            .bat_height_tiles = 1,
        });
    check(
        dormant_tile && dormant_tile->pixels[8] == 0xFF0000FCU,
        "PCE decoder blanks a dormant out-of-cache tile");
    check(
        !devils_crush::render::decode_pce_background({
            .vram_words =
                std::span<const std::uint16_t>(invalid_vram).first(1),
            .palette_entries = palette,
            .bat_width_tiles = 2,
            .bat_height_tiles = 1,
        }),
        "PCE decoder rejects a truncated BAT");
}

void test_background_stitch_and_crop()
{
    using devils_crush::render::ArgbImage;
    using devils_crush::render::PlacedImage;

    const ArgbImage upper{
        .width = 2,
        .height = 2,
        .pixels = {
            0xFF100000U,
            0xFF200000U,
            0xFF300000U,
            0xFF400000U,
        },
    };
    const ArgbImage lower{
        .width = 2,
        .height = 2,
        .pixels = {
            0xFF001000U,
            0xFF002000U,
            0xFF003000U,
            0xFF004000U,
        },
    };
    const std::array regions{
        PlacedImage{.image = &upper, .world_x = -1, .world_y = -2},
        PlacedImage{.image = &lower, .world_x = -1, .world_y = 0},
    };
    const auto stitched = devils_crush::render::stitch_images(regions);
    check(stitched.has_value(), "table regions stitch");
    if (!stitched) {
        return;
    }
    check(
        stitched->world_x == -1 && stitched->world_y == -2
            && stitched->image.width == 2
            && stitched->image.height == 4,
        "stitched table retains its world origin and extent");
    check(
        stitched->image.pixels[3] == 0xFF400000U
            && stitched->image.pixels[4] == 0xFF001000U,
        "stitched table preserves the region seam");

    const auto crop = devils_crush::render::crop_image(
        stitched->image,
        0,
        1,
        2,
        2);
    check(
        crop && crop->pixels
                == std::vector<std::uint32_t>{
                    0xFF300000U,
                    0xFF400000U,
                    0xFF001000U,
                    0xFF002000U,
                },
        "camera-sized crop crosses a stitched seam exactly");
    check(
        !devils_crush::render::crop_image(
            stitched->image,
            1,
            3,
            2,
            2),
        "crop rejects out-of-range camera rectangles");
}

void test_save_state_container()
{
    const devils_crush::state::SaveStateContainer source{
        .format_version =
            devils_crush::state::kSaveStateFormatVersion,
        .frame_number = 0x1020304050607080ULL,
        .rom_sha256 =
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
        .core_revision = "ae99235c2139c176c1a8d0fde2957bf701d3cab0",
        .core_state = {
            std::byte{0x00},
            std::byte{0x7F},
            std::byte{0x80},
            std::byte{0xFF},
        },
    };

    const auto encoded = devils_crush::state::encode(source);
    const auto decoded = devils_crush::state::decode(encoded);
    check(decoded.decoded, "save-state container round trip decodes");
    check(
        decoded.container.format_version == source.format_version
            && decoded.container.frame_number == source.frame_number
            && decoded.container.rom_sha256 == source.rom_sha256
            && decoded.container.core_revision == source.core_revision
            && decoded.container.core_state == source.core_state,
        "save-state container round trip preserves fields");

    auto corrupted = encoded;
    corrupted.back() ^= std::byte{0x01};
    check(
        !devils_crush::state::decode(corrupted).decoded,
        "save-state payload corruption is rejected");

    auto unknown_version = encoded;
    unknown_version[8] = std::byte{0x02};
    check(
        !devils_crush::state::decode(unknown_version).decoded,
        "unknown save-state version is rejected");

    check(
        !devils_crush::state::decode(
             std::span<const std::byte>(encoded).first(32U))
             .decoded,
        "truncated save-state container is rejected");

    auto invalid_metadata = source;
    invalid_metadata.rom_sha256.front() = 'G';
    bool invalid_rejected = false;
    try {
        (void)devils_crush::state::encode(invalid_metadata);
    } catch (const std::invalid_argument&) {
        invalid_rejected = true;
    }
    check(invalid_rejected, "invalid save-state metadata is rejected");
}

void test_backup_ram_storage()
{
    const auto path =
        std::filesystem::current_path() / "dc-core-tests-backup.sav";
    auto temporary_path = path;
    temporary_path += ".tmp";
    std::error_code error;
    (void)std::filesystem::remove(path, error);
    error.clear();
    (void)std::filesystem::remove(temporary_path, error);

    const auto missing = devils_crush::backup::load_exact(path, 32U);
    check(
        missing.status == devils_crush::backup::LoadStatus::not_found,
        "missing backup RAM is reported distinctly");

    std::vector<std::byte> expected(32U);
    for (std::size_t index = 0; index < expected.size(); ++index) {
        expected[index] =
            static_cast<std::byte>((index * 13U + 7U) & 0xFFU);
    }
    check(
        devils_crush::backup::save_atomic(path, expected).saved,
        "backup RAM is atomically saved");

    const auto loaded =
        devils_crush::backup::load_exact(path, expected.size());
    check(loaded.loaded(), "backup RAM is loaded");
    check(loaded.bytes == expected, "backup RAM bytes round trip");
    check(
        devils_crush::backup::load_exact(path, expected.size() - 1U).status
            == devils_crush::backup::LoadStatus::invalid_size,
        "backup RAM with the wrong exact size is rejected");

    expected.front() ^= std::byte{0xFF};
    check(
        devils_crush::backup::save_atomic(path, expected).saved,
        "an existing backup RAM file is atomically replaced");
    check(
        devils_crush::backup::load_exact(path, expected.size()).bytes
            == expected,
        "replacement backup RAM bytes round trip");
    check(
        !std::filesystem::exists(temporary_path),
        "successful backup save leaves no temporary file");

    error.clear();
    check(
        std::filesystem::remove(path, error) && !error,
        "backup RAM test file is removed");
}

void test_runtime_boundary()
{
    const auto runtime = devils_crush::emulation::create_pce_runtime();
    const auto info = runtime->info();
    check(!info.name.empty(), "runtime has a diagnostic name");
    check(
        info.core_available == !info.upstream_revision.empty(),
        "runtime availability and revision agree");
    check(
        runtime->backup_ram_size() == 0U,
        "backup RAM is unavailable before ROM load");
    check(
        runtime->export_backup_ram().empty(),
        "backup RAM export is empty before ROM load");
    constexpr std::array invalid_backup{std::byte{0}};
    check(
        !runtime->import_backup_ram(invalid_backup),
        "backup RAM import is rejected before ROM load");
    check(
        runtime->work_ram_size() == 0U,
        "work RAM is unavailable before ROM load");
    check(
        runtime->export_work_ram().empty(),
        "work RAM export is empty before ROM load");
    check(
        !runtime->install_execution_hook({
            .routine = {
                .physical_bank = 0,
                .logical_pc = 0xE462,
            },
            .mode =
                devils_crush::emulation::HookMode::pass_through,
        }),
        "execution hook installation is rejected before ROM load");
    check(
        runtime->execution_hook_count() == 0U,
        "execution-hook registry is empty before ROM load");
    check(
        !runtime->remove_execution_hook({
            .physical_bank = 0,
            .logical_pc = 0xE462,
        }),
        "execution-hook removal is rejected before ROM load");
    runtime->clear_execution_hooks();
    check(
        !runtime->install_routine_call_observer({
            .routine = {
                .physical_bank = 0,
                .logical_pc = 0xE462,
            },
            .callback = [](const devils_crush::emulation::RoutineCall&) {},
        }),
        "routine-call observer installation is rejected before ROM load");
    check(
        runtime->routine_call_observer_count() == 0U,
        "routine-call observer registry is empty before ROM load");
    check(
        !runtime->remove_routine_call_observer({
            .physical_bank = 0,
            .logical_pc = 0xE462,
        }),
        "routine-call observer removal is rejected before ROM load");
    runtime->clear_routine_call_observers();
    check(
        !runtime->set_work_ram_write_observer(
            [](const devils_crush::emulation::ObservedMemoryWrite&) {}),
        "work-RAM write observer installation is rejected before ROM load");
    check(
        !runtime->has_work_ram_write_observer(),
        "work-RAM write observer is empty before ROM load");
    runtime->clear_work_ram_write_observer();
    check(
        !runtime->set_bus_write_observer(
            [](const devils_crush::emulation::ObservedBusWrite&) {}),
        "bus-write observer installation is rejected before ROM load");
    check(
        !runtime->has_bus_write_observer(),
        "bus-write observer is empty before ROM load");
    runtime->clear_bus_write_observer();
    check(
        !runtime->export_video_memory(),
        "video-memory export is empty before ROM load");
    check(
        runtime->serialize().core_state.empty(),
        "savestate serialization is empty before ROM load");
    check(
        !runtime->deserialize({}),
        "savestate restore is rejected before ROM load");

    bool second_runtime_rejected = false;
    try {
        const auto second =
            devils_crush::emulation::create_pce_runtime();
        (void)second;
        second_runtime_rejected = false;
    } catch (const std::logic_error&) {
        second_runtime_rejected = true;
    }
    check(
        second_runtime_rejected == info.core_available,
        "real runtime enforces the single-instance contract");
}

} // namespace

int main()
{
    test_hash_vectors();
    test_native_cached_position_restore();
    test_supported_rom_metadata();
    test_scaler_examples();
    test_scaler_edges();
    test_pce_background_decode();
    test_background_stitch_and_crop();
    test_save_state_container();
    test_backup_ram_storage();
    test_runtime_boundary();

    if (failures == 0) {
        std::cout << "All core tests passed.\n";
        return EXIT_SUCCESS;
    }

    std::cerr << failures << " test(s) failed.\n";
    return EXIT_FAILURE;
}
