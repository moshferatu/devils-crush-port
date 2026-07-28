// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/render/backglass_image.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

int failures = 0;

void check(const bool condition, const std::string_view label)
{
    if (!condition) {
        std::cerr << "FAIL: " << label << '\n';
        ++failures;
    }
}

[[nodiscard]] std::vector<std::byte> decode_base64(
    const std::string_view encoded)
{
    constexpr std::string_view alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<std::byte> bytes;
    std::uint32_t value = 0U;
    int bits = 0;
    for (const char character : encoded) {
        if (character == '=') {
            break;
        }
        const auto digit = alphabet.find(character);
        if (digit == std::string_view::npos) {
            return {};
        }
        value = (value << 6U) | static_cast<std::uint32_t>(digit);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            bytes.push_back(static_cast<std::byte>(
                (value >> static_cast<unsigned int>(bits)) & 0xFFU));
        }
    }
    return bytes;
}

void remove_file(const std::filesystem::path& path)
{
    std::error_code error;
    (void)std::filesystem::remove(path, error);
}

[[nodiscard]] bool write_fixture(
    const std::filesystem::path& path,
    const std::string_view encoded)
{
    const auto bytes = decode_base64(encoded);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    return !bytes.empty() && output.good();
}

} // namespace

int main()
{
    check(SDL_Init(0), "SDL initializes for image tests");

    const auto base = std::filesystem::current_path();
    const std::array paths{
        base / "dc-backglass-format-test.dat",
        base / "dc-backglass-format-test.jpg",
        base / "dc-backglass-format-test.webp",
    };
    for (const auto& path : paths) {
        remove_file(path);
    }
    const auto invalid_path =
        base / "dc-backglass-format-test.txt";
    remove_file(invalid_path);

    constexpr std::array fixtures{
        std::string_view{
            "iVBORw0KGgoAAAANSUhEUgAAAAMAAAACCAYAAACddGYaAAAAEUlEQVR4AWNQ"
            "SHjwH4YZkDkAr7UOO/CNoPMAAAAASUVORK5CYII="},
        std::string_view{
            "/9j/4AAQSkZJRgABAgEAYABgAAD//gAeQ3JlYXRlZCBieSBUaW55IEpQRUcg"
            "RW5jb2Rlcv/bAEMAAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEB"
            "AQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAf/bAEMBAQEBAQEBAQEB"
            "AQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEB"
            "AQEBAQEBAQEBAf/AABEIAAIAAwMBEQACEQEDEQH/xAAfAAABBQEBAQEBAQAA"
            "AAAAAAAAAQIDBAUGBwgJCgv/xAC1EAACAQMDAgQDBQUEBAAAAX0BAgMABBEF"
            "EiExQQYTUWEHInEUMoGRoQgjQrHBFVLR8CQzYnKCCQoWFxgZGiUmJygpKjQ1"
            "Njc4OTpDREVGR0hJSlNUVVZXWFlaY2RlZmdoaWpzdHV2d3h5eoOEhYaHiImK"
            "kpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna"
            "4eLj5OXm5+jp6vHy8/T19vf4+fr/xAAfAQADAQEBAQEBAQEBAAAAAAAAAQID"
            "BAUGBwgJCgv/xAC1EQACAQIEBAMEBwUEBAABAncAAQIDEQQFITEGEkFRB2Fx"
            "EyIygQgUQpGhscEJIzNS8BVictEKFiQ04SXxFxgZGiYnKCkqNTY3ODk6Q0RF"
            "RkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqCg4SFhoeIiYqSk5SVlpeY"
            "mZqio6Slpqeoqaqys7S1tre4ubrCw8TFxsfIycrS09TV1tfY2dri4+Tl5ufo"
            "6ery8/T19vf4+fr/2gAMAwEAAhEDEQA/APzbr/pWP8rA/9k="},
        std::string_view{
            "UklGRkAAAABXRUJQVlA4IDQAAADQAQCdASoDAAIAAMASJaACdLoB+AAD"
            "sAD+9hNf/2Gf9hn/YZ//XmX/3eb9z5+58/4QAAAA"},
    };
    check(
        write_fixture(paths[0], fixtures[0])
            && write_fixture(paths[1], fixtures[1])
            && write_fixture(paths[2], fixtures[2]),
        "embedded image fixtures are written");

    constexpr std::array labels{"PNG", "JPEG", "WebP"};
    for (std::size_t index = 0; index < paths.size(); ++index) {
        const auto loaded =
            devils_crush::render::load_backglass_image(paths[index]);
        check(
            loaded.loaded(),
            std::string(labels[index]) + " backglass image loads");
        check(
            loaded.surface != nullptr && loaded.surface->w == 3
                && loaded.surface->h == 2,
            std::string(labels[index])
                + " backglass dimensions are preserved");
    }

    {
        std::ofstream invalid(invalid_path, std::ios::binary);
        invalid << "not an image";
    }
    const auto unsupported =
        devils_crush::render::load_backglass_image(invalid_path);
    check(
        unsupported.status
            == devils_crush::render::BackglassLoadStatus::
                unsupported_format,
        "unsupported backglass content is rejected");
    const auto missing =
        devils_crush::render::load_backglass_image(
            base / "dc-backglass-intentionally-missing.png");
    check(
        missing.status
            == devils_crush::render::BackglassLoadStatus::io_error,
        "missing backglass image is rejected");

    for (const auto& path : paths) {
        remove_file(path);
    }
    remove_file(invalid_path);
    SDL_Quit();

    if (failures == 0) {
        std::cout << "All image tests passed.\n";
        return EXIT_SUCCESS;
    }
    std::cerr << failures << " image test(s) failed.\n";
    return EXIT_FAILURE;
}
