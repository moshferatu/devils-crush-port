// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/core/rom_image.hpp"
#include "devils_crush/emulation/pce_runtime.hpp"
#include "devils_crush/render/pce_background.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <span>
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

} // namespace

int main(const int argc, char** const argv)
{
    if (argc < 2 || argc > 3
        || (argc == 3
            && std::string_view(argv[2]) != "--map-row-writer")) {
        std::cerr
            << "Usage: dc_video_snapshot_tests <rom-path>"
               " [--map-row-writer]\n";
        return EXIT_FAILURE;
    }
    const bool map_row_writer = argc == 3;

    const auto runtime = devils_crush::emulation::create_pce_runtime();
    const auto load = runtime->load_rom(std::filesystem::path(argv[1]));
    check(load.loaded, "supported ROM loads for video snapshot");
    if (!load.loaded) {
        std::cerr << load.message << '\n';
        return EXIT_FAILURE;
    }

    std::map<std::pair<std::uint8_t, std::uint16_t>, std::size_t>
        vdc_write_sites;
    std::vector<devils_crush::emulation::RoutineCall>
        background_row_calls;
    if (map_row_writer) {
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
    }

    constexpr std::array capture_frames{
        900,
        1200,
        1500,
        1800,
        2100,
        2400,
        2700,
        3000,
        3300,
        3600,
    };
    std::size_t capture_index = 0;
    devils_crush::emulation::EmulatedFrame frame;
    for (int frame_number = 1; frame_number <= capture_frames.back();
        ++frame_number) {
        frame = runtime->run_frame(input_for_frame(frame_number));
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
        check(
            snapshot->vram_words.size() == 32768U,
            "video snapshot copies all base VDC VRAM words");
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
        if (frame_number == 900) {
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
        }
        std::cout << "frame=" << frame_number
                  << " mwr=" << snapshot->memory_width_register
                  << " bxr=" << snapshot->background_x_scroll
                  << " byr=" << snapshot->background_y_scroll
                  << " camera_y=" << camera_y
                  << " bat=" << snapshot->bat_width_tiles() << 'x'
                  << snapshot->bat_height_tiles()
                  << " vram_sha256=" << vram_digest.sha256
                  << " palette_sha256=" << palette_digest.sha256
                  << " background_sha256="
                  << background_digest.sha256 << '\n';
    }
    check(
        frame.frame_number
            == static_cast<std::uint64_t>(capture_frames.back()),
        "table snapshot route reaches its final frame");
    check(
        capture_index == capture_frames.size(),
        "table snapshot route captures every checkpoint");
    if (!map_row_writer) {
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

    if (failures == 0) {
        std::cout << "Video row-writer tests passed.\n";
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}
