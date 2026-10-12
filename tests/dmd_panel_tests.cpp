// SPDX-License-Identifier: GPL-3.0-or-later
#include "devils_crush/cabinet/dmd_panel.hpp"
#include "devils_crush/core/rom_image.hpp"
#include "devils_crush/emulation/pce_runtime.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
int failures = 0;
void check(bool ok, const char* message)
{
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
// Optional review artifacts are lossless RGB, independent of SDL/display DPI.
void save(const std::filesystem::path& path,
    const devils_crush::render::ArgbImage& image)
{
    std::ofstream output(path, std::ios::binary);
    output << "P6\n" << image.width << ' ' << image.height << "\n255\n";
    for (auto color : image.pixels) {
        output.put(static_cast<char>(color >> 16U));
        output.put(static_cast<char>(color >> 8U));
        output.put(static_cast<char>(color));
    }
    check(static_cast<bool>(output), "review image writes");
}
}

int main(int argc, char** argv)
{
    using namespace devils_crush::cabinet;
    if (argc < 2) return 1;
    check(devils_crush::rom::validate_file(argv[1]).supported(), "supported ROM");
    std::vector<std::byte> rom(devils_crush::rom::kSupportedRomSize);
    std::ifstream input(argv[1], std::ios::binary);
    input.read(reinterpret_cast<char*>(rom.data()), static_cast<std::streamsize>(rom.size()));
    const auto font = DmdFont::decode(rom);
    check(font.has_value(), "original labels and HUD digits decode");
    check(!DmdFont::decode({}), "truncated font is rejected");
    if (!font) return 1;
    for (unsigned a = 0; a < 10U; ++a) {
        for (unsigned b = a + 1U; b < 10U; ++b) {
            check(font->score_digit(a) != font->score_digit(b), "all ten digits differ");
        }
    }
    check(std::any_of(font->score_digit(0).begin(), font->score_digit(0).end(),
        [](auto c) { return ((c >> 16U) & 255U) > ((c >> 8U) & 255U); }),
        "score digits retain red shading");
    check(font->score_digit(0) != font->score_digit(0, true),
        "special award uses its own original palette");

    std::vector<std::byte> ram(8192U);
    ram[0xD52U] = std::byte{3};
    ram[0xD50U] = std::byte{37};
    // 9,999,999 stored units -> the maximum 999,999,900-point score.
    ram[0x205U] = std::byte{0x7F};
    ram[0x20CU] = std::byte{0x96};
    ram[0x213U] = std::byte{0x98};
    DmdPanel panel;
    const auto review = [&](const char* name) {
        auto image = panel.compose(*font, nullptr, 0);
        check(image.valid() && image.width == 320 && image.height == 112, "panel dimensions");
        // Every scene reserves margins even at the maximum score.
        for (int y = 0; y < image.height; ++y) {
            check(image.pixels[static_cast<std::size_t>(y * image.width)] == 0xFF000000U
                && image.pixels[static_cast<std::size_t>((y + 1) * image.width - 1)] == 0xFF000000U,
                "scene does not clip horizontally");
        }
        if (argc >= 3) save(std::filesystem::path(argv[2]) / name, image);
        return image;
    };
    panel.observe(ram);
    check(panel.scene() == DmdScene::score && !panel.holds_playfield(), "live score scene");
    review("score.ppm");
    ram[0x53U] = std::byte{4}; ram[0xCAU] = std::byte{2};
    auto before = ram;
    panel.observe(ram);
    check(ram == before, "presentation never changes the game");
    check(panel.scene() == DmdScene::special_points && !panel.holds_playfield(), "special announcement keeps live table");
    review("special.ppm");
    ram[0x53U] = std::byte{0}; ram[0x54U] = std::byte{2};
    ram[0xD69U] = std::byte{0x70}; ram[0xD6AU] = std::byte{0x17};
    ram[0xD4FU] = std::byte{15}; ram[0xD51U] = std::byte{4};
    panel.observe(ram);
    check(panel.scene() == DmdScene::bonus_tally && panel.holds_playfield(), "bonus tally retains playfield");
    review("tally.ppm");
    ram[0xD71U] = std::byte{1}; ram[0x5DU] = std::byte{7};
    panel.observe(ram);
    check(panel.scene() == DmdScene::match, "match distinguished from bonus tally");
    review("match.ppm");
    ram[0x54U] = std::byte{0}; ram[0x53U] = std::byte{0x12};
    ram[0xDB7U] = std::byte{0x10}; ram[0xDB8U] = std::byte{0x27};
    panel.observe(ram);
    check(panel.scene() == DmdScene::score && !panel.holds_playfield(),
        "stage entry flag is not a result announcement");
    panel.begin_bonus_result();
    panel.observe(ram);
    check(panel.scene() == DmdScene::bonus_result && panel.holds_playfield(), "bonus result retains bonus room");
    review("result.ppm");
    ram[0xDB7U] = ram[0xDB8U] = std::byte{0};
    panel.observe(ram);
    const auto success_zero = review("result-zero.ppm");
    ram[0x53U] = std::byte{0}; panel.observe(ram);
    check(!panel.holds_playfield(), "leaving result releases playfield");
    ram[0x53U] = std::byte{0x12}; panel.begin_bonus_result(); panel.observe(ram);
    check(review("lost.ppm").pixels != success_zero.pixels,
        "a completed payout does not become a lost bonus");
    ram[0x53U] = std::byte{0}; ram[0x54U] = std::byte{4};
    // The live page is held over the screen's first frames, while the game
    // behind it sorts the table.
    for (int frame = 0; frame < 16; ++frame) panel.observe(ram);
    check(panel.attract() && !panel.holds_playfield(), "high score menu after live game");
    devils_crush::core::HighScoreTable scores{};
    scores.bytes[0] = 0x7F; scores.bytes[5] = 0x96; scores.bytes[10] = 0x98;
    scores.bytes[15] = scores.bytes[20] = scores.bytes[25] = 'A';
    if (argc >= 3) save(std::filesystem::path(argv[2]) / "high-score.ppm", panel.compose(*font, &scores, 0));

    // The attract carousel opens on the ending's head and then turns through
    // the pages it had before.
    using devils_crush::render::EndingHeadAnimation;
    using devils_crush::render::kEndingHeadFrames;
    const auto ending = EndingHeadAnimation::decode(rom);
    check(ending.has_value(), "the ending's head decodes");
    check(!EndingHeadAnimation::decode({}), "a truncated ROM has no ending head");
    if (!ending) return 1;
    const auto first_page = panel.compose(*font, &scores, 0);
    const auto turn = kDmdEndingHeadMilliseconds + 5U * 3'000U;
    check(panel.compose(*font, &scores, kDmdEndingHeadMilliseconds, &*ending).pixels == first_page.pixels
        && panel.compose(*font, &scores, kDmdEndingHeadMilliseconds - 1U, &*ending).pixels != first_page.pixels,
        "the Grand Champion page follows the head");
    check(panel.compose(*font, &scores, turn - 1U, &*ending).pixels
            == panel.compose(*font, &scores, 5U * 3'000U - 1U).pixels,
        "the last page is the last of the turn");
    const auto lit = [](const devils_crush::render::ArgbImage& image, int left, int top, int right, int bottom) {
        unsigned count = 0;
        for (int y = top; y < bottom; ++y) {
            for (int x = left; x < right; ++x) {
                count += (image.pixels[static_cast<std::size_t>(y * image.width + x)] & 0xFFFFFFU) != 0U;
            }
        }
        return count;
    };
    // The game cuts from its last frame to black; the panel fades the face
    // out and stays dark for the rest of a second.
    const auto at_frame = [&](unsigned frame) {
        return panel.compose(*font, &scores, (frame * 1'000U + 59U) / 60U, &*ending);
    };
    const auto whole = lit(at_frame(189U), 132, 20, 188, 92);
    const auto going = lit(at_frame(210U), 132, 20, 188, 92);
    check(going != 0U && going < whole && lit(at_frame(210U), 0, 0, 320, 112) == going,
        "the face alone fades out after the game's last frame");
    check(lit(at_frame(230U), 0, 0, 320, 112) == 0U && lit(at_frame(249U), 0, 0, 320, 112) == 0U,
        "and the panel is dark before the scores");
    for (const unsigned frame : {0U, 30U, 70U, 110U, 160U, 189U, 200U, 220U}) {
        const auto at = (frame * 1'000U + 59U) / 60U;
        const auto image = panel.compose(*font, &scores, at, &*ending);
        check(image.pixels == panel.compose(*font, &scores, at + turn, &*ending).pixels,
            "the carousel comes round to the head again");
        // The face is the middle 56 by 72 of the panel; nothing else is lit
        // until the balls come, and it is black until the fade begins.
        const auto face = lit(image, 132, 20, 188, 92);
        check((frame == 0U) == (face == 0U), "the face fades in from black");
        check((lit(image, 0, 0, 320, 112) != face) == (frame == 110U || frame == 160U),
            "balls come in from outside the face, and bursts spill past it");
        if (argc >= 3) save(std::filesystem::path(argv[2]) / ("ending-" + std::to_string(frame) + ".ppm"), image);
    }

    if (argc >= 4) {
        std::ifstream capture(argv[3], std::ios::binary);
        DmdPanel recorded;
        auto previous = DmdScene::score;
        unsigned frame = 0;
        while (capture.read(reinterpret_cast<char*>(ram.data()), 8192)) {
            ++frame; recorded.observe(ram);
            if (recorded.scene() != previous) {
                std::cout << "frame=" << frame << " scene=" << static_cast<int>(recorded.scene()) << '\n';
                if (argc >= 3) save(std::filesystem::path(argv[2]) / ("frame-" + std::to_string(frame) + ".ppm"),
                    recorded.compose(*font, nullptr, 0));
                previous = recorded.scene();
            }
        }
        check(frame != 0U && capture.eof(), "recorded RAM frames load");
    }
    // The ending's head against the cartridge's own. Score saturation raises
    // `$2CC0` bit 7; the game then ends the ball in play, blows the table up
    // and puts screen 5 up, and the sequence is the 190 frames from that one.
    {
        using namespace devils_crush::emulation;
        auto runtime = create_pce_runtime();
        check(runtime->load_rom(argv[1]).loaded, "ending fixture ROM loads");
        // What both sides agree a colour is: its three bits a channel.
        const auto levels = [](std::uint32_t argb) {
            const auto level = [argb](unsigned shift) { return (((argb >> shift) & 0xFFU) * 7U + 127U) / 255U; };
            return (level(16U) << 6U) | (level(8U) << 3U) | level(0U);
        };
        unsigned first = 0;
        unsigned compared = 0;
        unsigned wrong_frames = 0;
        for (unsigned frame = 1; frame <= 3000U && compared < kEndingHeadFrames; ++frame) {
            if (frame == 1300U) {
                auto patch = runtime->export_work_ram();
                patch[0xCC0U] = std::byte{0x80};
                check(runtime->import_work_ram(patch), "ending fixture patch applies");
            }
            InputFrame controls;
            const auto pair = [frame](unsigned start) { return frame == start || frame == start + 1U; };
            const auto press = [&controls](Button button) { controls.player_one |= static_cast<std::uint16_t>(button); };
            if (pair(180) || pair(300)) press(Button::run);
            if (pair(420) || pair(700) || (frame >= 850U && frame <= 880U)) press(Button::one);
            if (frame >= 1000U && frame <= 1701U && pair(frame / 100U * 100U)) {
                press(Button::one);
                press(Button::left);
            }
            if (pair(1050) || pair(1250) || pair(1450) || pair(1650)) press(Button::two);
            const auto output = runtime->run_frame(controls);
            if (first == 0U && std::to_integer<unsigned>(runtime->export_work_ram()[0x54U]) == 5U) first = frame;
            if (first == 0U) continue;
            const auto& screen = output.framebuffer;
            devils_crush::render::ArgbImage native{screen.width, screen.height,
                std::vector<std::uint32_t>(static_cast<std::size_t>(screen.width * screen.height), 0xFF000000U)};
            ending->draw(native, frame - first, 0, 0);
            unsigned wrong = 0;
            for (int y = 0; y < screen.height; ++y) {
                for (int x = 0; x < screen.width; ++x) {
                    wrong += levels(native.pixels[static_cast<std::size_t>(y * screen.width + x)])
                        != levels(screen.pixels[static_cast<std::size_t>(y * screen.pitch_pixels + x)]);
                }
            }
            if (wrong != 0U && wrong_frames++ < 12U) {
                std::cerr << "ending frame " << frame - first << ": " << wrong << " pixels differ\n";
            }
            ++compared;
        }
        std::cout << "ending first=" << first << " compared=" << compared << " wrong=" << wrong_frames << '\n';
        check(first != 0U && compared == kEndingHeadFrames, "the cartridge reaches its ending");
        check(wrong_frames == 0U, "every frame of the head is the cartridge's own");
    }
    // Run the existing forced BS-1 result fixture through the real cartridge.
    // The observer must neither trigger on stage entry nor alter CPU state.
    using namespace devils_crush::emulation;
    std::vector<std::byte> baseline;
    baseline.reserve(3600U * 8192U);
    for (unsigned route = 0; route < 4U; ++route) {
        auto runtime = create_pce_runtime();
        check(runtime->load_rom(argv[1]).loaded, "result fixture ROM loads");
        DmdPanel live;
        unsigned result_entry = 0;
        if (route != 0U) {
            check(runtime->install_execution_hook({
                .routine = {.physical_bank = 4U, .logical_pc = 0x456CU},
                .mode = HookMode::observe,
                .callback = [&live, &result_entry](const RoutineEntry& entry) {
                    result_entry = static_cast<unsigned>(entry.frame_number);
                    live.begin_bonus_result();
                },
            }), "read-only bonus result observer installs");
        }
        unsigned first_result = 0;
        unsigned result_frames = 0;
        for (unsigned frame = 1; frame <= 3600U; ++frame) {
            if (frame == 1300U || frame == 1459U) {
                auto patch = runtime->export_work_ram();
                if (frame == 1300U) {
                    patch[0x55U] = std::byte{1}; patch[0x53U] = std::byte{0x10};
                } else {
                    if (route != 3U) patch[0xDB7U] = std::byte{1};
                    if (route <= 1U) patch[0xDBAU] = std::byte{1};
                }
                check(runtime->import_work_ram(patch), "result fixture patch applies");
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
            if (pair(1450) || pair(2250) || pair(3050)) press(Button::two);
            const auto output = runtime->run_frame(controls);
            const auto snapshot = runtime->export_work_ram();
            if (route == 0U) baseline.insert(baseline.end(), snapshot.begin(), snapshot.end());
            if (route == 1U) check(std::equal(snapshot.begin(), snapshot.end(),
                baseline.begin() + static_cast<std::ptrdiff_t>((frame - 1U) * 8192U)),
                "result observer preserves every recorded RAM byte");
            live.observe(snapshot);
            if (live.scene() == DmdScene::bonus_result) {
                if (first_result == 0U) first_result = frame;
                ++result_frames;
            }
            if (route != 0U && argc >= 3 && (frame == 1459U || frame == 1700U || frame == 2300U)) {
                save(std::filesystem::path(argv[2]) / ("route-" + std::to_string(route)
                    + "-" + std::to_string(frame) + ".ppm"), live.compose(*font, nullptr, 0));
            }
            check(output.frame_number != 0U, "result fixture produces frames");
        }
        if (route != 0U) {
            check(first_result == 1403U && result_entry != 0U,
                "result begins at the real controller, after stage initialization");
            check(result_frames > 200U && !live.holds_playfield(),
                "result completes and releases the playfield");
            std::cout << "route=" << route << " result_frames=" << result_frames << '\n';
        }
        runtime->clear_execution_hooks();
    }
    return failures == 0 ? 0 : 1;
}
