// SPDX-License-Identifier: GPL-3.0-or-later
#include "devils_crush/cabinet/dragon_heads.hpp"
#include "devils_crush/emulation/pce_runtime.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace {
int failures = 0;
void check(bool ok, const char* message)
{
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
// Optional review artifacts are lossless RGB, independent of SDL/display DPI.
void save(const std::filesystem::path& path,
    const devils_crush::emulation::FramebufferView& screen)
{
    std::ofstream output(path, std::ios::binary);
    output << "P6\n" << screen.width << ' ' << screen.height << "\n255\n";
    for (int y = 0; y < screen.height; ++y) {
        for (int x = 0; x < screen.width; ++x) {
            const auto color = screen.pixels[static_cast<std::size_t>(y * screen.pitch_pixels + x)];
            output.put(static_cast<char>(color >> 16U));
            output.put(static_cast<char>(color >> 8U));
            output.put(static_cast<char>(color));
        }
    }
    check(static_cast<bool>(output), "review image writes");
}
}

int main(int argc, char** argv)
{
    using namespace devils_crush::cabinet;
    using namespace devils_crush::emulation;
    if (argc < 2) return 1;

    // Bonus stage 1's five heads, in the order of their slots.
    constexpr std::array<std::size_t, 5U> kHeads{0x34U, 0x35U, 0x50U, 0x51U, 0x52U};
    const auto poses = [&kHeads](const std::vector<std::byte>& ram) {
        std::array<unsigned, 5U> result{};
        for (std::size_t head = 0; head < kHeads.size(); ++head) {
            result[head] = std::to_integer<unsigned>(ram[kDragonHeadActorPose + kHeads[head]]);
        }
        return result;
    };
    const auto is_head = [&kHeads](const std::size_t offset) {
        return std::ranges::any_of(kHeads, [offset](const std::size_t slot) {
            return offset == kDragonHeadActorPose + slot;
        });
    };

    // The forced bonus stage 1 entry, once as the cartridge has it and once
    // with the heads faced after every frame. The stage is laid out over
    // frames 1,341 and 1,342 and its routines next run at 1,403, when it has
    // faded in.
    constexpr unsigned kFirst = 1300U;
    constexpr unsigned kLast = 2400U;
    std::vector<std::vector<std::byte>> game_ram;
    std::vector<std::vector<std::uint32_t>> game_screens;
    for (unsigned route = 0; route < 2U; ++route) {
        auto runtime = create_pce_runtime();
        check(runtime->load_rom(argv[1]).loaded, "ROM loads");
        unsigned first_faced = 0;
        check(runtime->install_execution_hook({
            .routine = {.physical_bank = 4U, .logical_pc = kDragonHeadFacingRoutine},
            .mode = HookMode::observe,
            .callback = [&first_faced](const RoutineEntry& entry) {
                if (first_faced == 0U) first_faced = static_cast<unsigned>(entry.frame_number);
            },
        }), "facing routine observer installs");
        unsigned waiting_frames = 0;
        unsigned faced_frames = 0;
        unsigned pictures_changed = 0;
        std::set<std::size_t> other_bytes;
        std::array<unsigned, 5U> waited{};
        for (unsigned frame = 1; frame <= kLast; ++frame) {
            if (frame == kFirst) {
                auto patch = runtime->export_work_ram();
                patch[0x55U] = std::byte{1}; patch[0x53U] = std::byte{0x10};
                check(runtime->import_work_ram(patch), "bonus stage request applies");
            }
            InputFrame controls;
            const auto pair = [frame](unsigned first) { return frame == first || frame == first + 1U; };
            const auto press = [&controls](Button button) { controls.player_one |= static_cast<std::uint16_t>(button); };
            if (pair(180) || pair(300)) press(Button::run);
            if (pair(420) || pair(700) || (frame >= 850U && frame <= 880U)
                || (frame >= 1000U && frame <= 1180U)) press(Button::one);
            if (frame >= 1250U) {
                const auto phase = (frame - 1250U) % 30U;
                if (phase <= 1U) press(Button::left);
                if (phase == 15U || phase == 16U) press(Button::one);
            }
            const auto output = runtime->run_frame(controls);
            if (frame < kFirst) continue;
            auto ram = runtime->export_work_ram();
            const auto& screen = output.framebuffer;
            std::vector<std::uint32_t> pixels;
            for (int y = 0; y < screen.height; ++y) {
                for (int x = 0; x < screen.width; ++x) {
                    pixels.push_back(screen.pixels[static_cast<std::size_t>(y * screen.pitch_pixels + x)] & 0xFFFFFFU);
                }
            }
            const bool laid_out = std::to_integer<unsigned>(ram[kDragonHeadActorType + kHeads[0]]) == kDragonHeadType
                && frame >= 1341U;
            const bool waiting = laid_out && first_faced == 0U;
            if (argc >= 3 && (frame == 1370U || frame == 1401U || frame == 1404U)) {
                save(std::filesystem::path(argv[2]) / ((route == 0U ? "game-" : "faced-") + std::to_string(frame) + ".ppm"), screen);
            }
            if (route == 0U) {
                // The cartridge: facing right until the routine runs, which
                // then gives every head the pose the port's rule does.
                if (waiting) {
                    ++waiting_frames;
                    check(poses(ram) == std::array<unsigned, 5U>{}, "the game's heads wait facing right");
                }
                auto faced = ram;
                if (!waiting && face_waiting_dragon_heads(faced)) ++faced_frames;
                if (frame == first_faced) {
                    check(std::to_integer<int>(ram[kDragonHeadActorXLow]) == kDragonHeadsServeX
                        && std::to_integer<int>(ram[kDragonHeadActorYLow]) == kDragonHeadsServeY,
                        "the ball is still where it is served from when the heads first face it");
                    waited = poses(ram);
                }
                game_ram.push_back(std::move(ram));
                game_screens.push_back(std::move(pixels));
                continue;
            }
            // The port: the same poses from the stage's first frame. Building
            // different sprites takes a different time, which moves where
            // interrupts land in the frames after: scratch bytes of the zero
            // page and dead bytes of the stack page, below `$2200`, differ
            // at a frame's end for it. Nothing the game keeps does.
            const auto& game = game_ram[frame - kFirst];
            for (std::size_t offset = 0; offset < ram.size(); ++offset) {
                if (ram[offset] != game[offset] && !(waiting && is_head(offset))) other_bytes.insert(offset);
            }
            if (frame == first_faced) waited = poses(ram);
            if (pixels != game_screens[frame - kFirst]) {
                ++pictures_changed;
                // The game's sprites of frame 1,403 are still the ones it
                // built at 1,342.
                check(waiting || frame == first_faced, "the picture is the game's once the heads have faced the ball");
            }
            if (face_waiting_dragon_heads(ram)) {
                check(runtime->import_work_ram(ram), "faced heads import");
                ++faced_frames;
            }
            if (waiting) {
                ++waiting_frames;
                check(poses(ram) == poses(game_ram[1403U - kFirst]),
                    "the port's heads wait in the pose the game gives them next");
            }
        }
        std::cout << "route=" << route << " first_faced=" << first_faced
                  << " waiting_frames=" << waiting_frames
                  << " faced_frames=" << faced_frames
                  << " pictures_changed=" << pictures_changed << " other_bytes=" << other_bytes.size();
        for (const auto offset : other_bytes) std::cout << ' ' << std::hex << offset << std::dec;
        std::cout << " poses";
        for (const auto pose : waited) std::cout << ' ' << pose;
        std::cout << '\n';
        check(first_faced == 1403U && waiting_frames == 62U, "the heads wait a second for the stage to fade in");
        check(waited == std::array<unsigned, 5U>{2U, 2U, 2U, 3U, 3U}, "they first face down and down to the left");
        if (route == 0U) {
            check(faced_frames == 0U, "the rule is the routine's own wherever the routine is running");
        } else {
            check(faced_frames == 1U, "the heads are faced once, as they are laid out");
            check(pictures_changed > 50U, "the picture is different through the fade");
            check(std::ranges::all_of(other_bytes, [](const std::size_t offset) {
                return offset < 0x200U && offset != kDragonHeadsSessionFlags && offset != kDragonHeadsStage;
            }), "only the poses and scratch differ in work RAM");
        }
        runtime->clear_execution_hooks();
    }

    // The direction classifier, `01:DAF8`.
    check(dragon_head_direction(10, 0) == 0U && dragon_head_direction(0, 10) == 4U
        && dragon_head_direction(-10, 0) == 8U && dragon_head_direction(0, -10) == 12U,
        "right, down, left and up");
    check(dragon_head_direction(10, 10) == 2U && dragon_head_direction(-10, 10) == 6U
        && dragon_head_direction(-10, -10) == 10U && dragon_head_direction(10, -10) == 14U,
        "and the diagonals between them");
    check(dragon_head_direction(600, 100) == dragon_head_direction(255, 100)
        && dragon_head_direction(-256, 10) == dragon_head_direction(0, 10),
        "a distance is a byte");
    std::vector<std::byte> none(8192U);
    check(!face_waiting_dragon_heads(none) && !face_waiting_dragon_heads({}), "nothing is faced outside the stage");
    return failures == 0 ? 0 : 1;
}
