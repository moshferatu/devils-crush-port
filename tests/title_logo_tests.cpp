// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/core/rom_image.hpp"
#include "devils_crush/render/title_logo.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string_view>
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

[[nodiscard]] std::span<const std::byte> pixel_bytes(
    const devils_crush::render::ArgbImage& image)
{
    return std::as_bytes(std::span(image.pixels));
}

[[nodiscard]] bool is_black(const std::uint32_t pixel) noexcept
{
    return (pixel & 0x00FFFFFFU) == 0U;
}

} // namespace

int main(const int argc, char** const argv)
{
    if (argc != 2) {
        std::cerr << "Usage: dc_title_logo_tests <rom-path>\n";
        return 1;
    }

    const auto path = std::filesystem::path(argv[1]);
    const auto validation = devils_crush::rom::validate_file(path);
    check(validation.supported(), "supported ROM validates");

    std::vector<std::byte> rom_bytes(
        static_cast<std::size_t>(
            devils_crush::rom::kSupportedRomSize));
    std::ifstream input(path, std::ios::binary);
    input.read(
        reinterpret_cast<char*>(rom_bytes.data()),
        static_cast<std::streamsize>(rom_bytes.size()));
    check(
        input
            && input.gcount()
                == static_cast<std::streamsize>(rom_bytes.size()),
        "supported ROM bytes load");

    const auto animation =
        devils_crush::render::TitleLogoAnimation::decode(rom_bytes);
    check(animation.has_value(), "native title-logo assets decode");
    if (!animation) {
        return 1;
    }

    const auto frame0 = animation->render(0U);
    const auto frame2 = animation->render(2U);
    const auto frame24 = animation->render(24U);
    const auto frame32 = animation->render(32U);
    const auto repeated = animation->render(64U);
    check(
        frame0 && frame2 && frame24 && frame32 && repeated,
        "native title-logo animation frames render");
    if (frame0 && frame2 && frame24 && frame32 && repeated) {
        check(
            frame0->width
                    == devils_crush::render::kTitleLogoWidth
                && frame0->height
                    == devils_crush::render::kTitleLogoHeight,
            "native title-logo crop has the mapped dimensions");
        check(
            frame0->pixels == repeated->pixels,
            "native title-logo animation repeats after 64 frames");
        bool lower_lettering_present = false;
        for (int y = 144; y < 149; ++y) {
            for (int x = 0; x < frame0->width; ++x) {
                lower_lettering_present |= !is_black(
                    frame0->pixels[
                        static_cast<std::size_t>(y)
                            * static_cast<std::size_t>(frame0->width)
                        + static_cast<std::size_t>(x)]);
            }
        }
        check(
            lower_lettering_present,
            "complete Crush lettering extends into the corrected crop");
        bool clean_lower_margin = true;
        for (int y = frame0->height - 3; y < frame0->height; ++y) {
            for (int x = 0; x < frame0->width; ++x) {
                clean_lower_margin &= is_black(
                    frame0->pixels[
                        static_cast<std::size_t>(y)
                            * static_cast<std::size_t>(frame0->width)
                        + static_cast<std::size_t>(x)]);
            }
        }
        check(
            clean_lower_margin,
            "complete Crush lettering retains a clean lower margin");
        bool clean_right_margin = true;
        for (int y = 0; y < frame0->height; ++y) {
            for (int x = frame0->width - 3; x < frame0->width; ++x) {
                clean_right_margin &= is_black(
                    frame0->pixels[
                        static_cast<std::size_t>(y)
                            * static_cast<std::size_t>(frame0->width)
                        + static_cast<std::size_t>(x)]);
            }
        }
        check(
            clean_right_margin,
            "trademark glyphs remain outside the logo crop");

        const auto digest0 =
            devils_crush::rom::compute_digests(pixel_bytes(*frame0));
        const auto digest2 =
            devils_crush::rom::compute_digests(pixel_bytes(*frame2));
        const auto digest24 =
            devils_crush::rom::compute_digests(pixel_bytes(*frame24));
        const auto digest32 =
            devils_crush::rom::compute_digests(pixel_bytes(*frame32));
        check(
            digest0.sha256
                    == "fe1e50e6d2997196fa7f378b102b8635cb5908dab6751758e5d39"
                       "b8c894a1a01"
                && digest2.sha256
                    == "35b79d0c8e8fb2243ed53b9329defbe89ebaf951dbdc1fbebd6ba6"
                       "3f8a1d70bd"
                && digest24.sha256
                    == "df8fdf862cb58631fbf48a0dfe0585a27c6a38b6ec1e95c43e61e"
                       "962f794399d"
                && digest32.sha256
                    == "b55f1b0664d2114fc5c95cbbfc530afbe3af1428f5f8afe0af954"
                       "c60ce86f3a3",
            "mapped title palettes and both eye animations match goldens");
        std::cout
            << "frame0=" << digest0.sha256
            << "\nframe2=" << digest2.sha256
            << "\nframe24=" << digest24.sha256
            << "\nframe32=" << digest32.sha256 << '\n';
    }

    if (failures == 0) {
        std::cout << "Native title-logo tests passed.\n";
        return 0;
    }
    return 1;
}
