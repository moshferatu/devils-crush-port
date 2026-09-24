// SPDX-License-Identifier: GPL-3.0-or-later

// The launcher's icon and heading, drawn by the port: the title screen's logo,
// DEVIL'S CRUSH on two lines with an eye in the D and in the C, as
// `TitleLogoAnimation` draws it for the backglass.
//
// Both are made from the player's ROM when the launcher is built, not kept in
// the repository, for the reason nothing else from the ROM is kept there. The
// logo is decoded from the ROM's own title graphics and palettes -- no
// emulator runs -- and drawn at animation frame 12, in the bright palette of
// its flicker. The ROM is checked against the supported
// dump first, so a different one is said to be different rather than drawn
// into something else.
//
// The logo sits on the title's black backdrop. The heading keeps it, since the
// launcher is drawn on black too. The icon does not: the backdrop that can be
// reached from the edge without crossing the logo is made transparent, so the
// black inside the drawing stays black and what is around it goes. The icon is
// square, the logo centred in it at its own shape, and every size is an
// average of the pixels it covers, taken in linear light, so the red does not
// darken as it shrinks. Every size is stored as a bitmap with its alpha,
// which every Windows since XP reads.
//
// Usage: dc_launcher_art <rom.pce> <out.ico> <out.h> [out.png]
//
// The header holds DC_LOGO_W, DC_LOGO_H and dc_logo[], ARGB, top row first.
// The PNG, if named, is the icon's 256 image, for looking at.

#include "devils_crush/core/rom_image.hpp"
#include "devils_crush/render/title_logo.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

constexpr std::uint64_t kLogoFrame = 12U;
constexpr std::array<int, 7> kSizes{16, 24, 32, 48, 64, 128, 256};

float to_linear(const std::uint8_t v)
{
    const float c = static_cast<float>(v) / 255.0F;
    return c <= 0.04045F ? c / 12.92F : std::pow((c + 0.055F) / 1.055F, 2.4F);
}

std::uint8_t to_srgb(float c)
{
    c = c <= 0.0031308F ? c * 12.92F : 1.055F * std::pow(c, 1.0F / 2.4F) - 0.055F;
    const int v = static_cast<int>(c * 255.0F + 0.5F);
    return static_cast<std::uint8_t>(v < 0 ? 0 : v > 255 ? 255 : v);
}

// The weight each of `sw` source samples gives each of `dw` destination
// samples: an average of what the destination sample covers.
std::vector<std::vector<std::pair<int, double>>> shrink_weights(const int sw, const int dw)
{
    std::vector<std::vector<std::pair<int, double>>> weights(static_cast<std::size_t>(dw));
    const double step = static_cast<double>(sw) / dw;
    for (int d = 0; d < dw; ++d) {
        const double a = d * step;
        const double b = a + step;
        for (int s = static_cast<int>(a); s < sw && s < b; ++s) {
            const double lo = s < a ? a : s;
            const double hi = s + 1 > b ? b : s + 1;
            weights[static_cast<std::size_t>(d)].emplace_back(s, (hi - lo) / step);
        }
    }
    return weights;
}

// The logo at `size` square, RGBA, top row first. Colour is averaged
// premultiplied by coverage, so a transparent pixel's black does not bleed into
// the edge of the drawing.
std::vector<std::uint8_t> render_size(
    const std::vector<std::array<float, 4>>& premultiplied,
    const int w,
    const int h,
    const int size)
{
    const int pw = w >= h ? size : (size * w + h / 2) / h;
    const int ph = h >= w ? size : (size * h + w / 2) / w;
    const int left = (size - pw) / 2;
    const int top = (size - ph) / 2;
    const auto across = shrink_weights(w, pw);
    const auto down = shrink_weights(h, ph);
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(size) * size * 4U, 0U);
    for (int y = 0; y < ph; ++y) {
        for (int x = 0; x < pw; ++x) {
            std::array<double, 4> sum{};
            for (const auto& [sy, wy] : down[static_cast<std::size_t>(y)]) {
                for (const auto& [sx, wx] : across[static_cast<std::size_t>(x)]) {
                    const auto& p = premultiplied[static_cast<std::size_t>(sy) * w + sx];
                    for (int ch = 0; ch < 4; ++ch) {
                        sum[static_cast<std::size_t>(ch)] += p[static_cast<std::size_t>(ch)] * wx * wy;
                    }
                }
            }
            auto* out = &rgba[(static_cast<std::size_t>(top + y) * size + left + x) * 4U];
            const double alpha = sum[3];
            for (int ch = 0; ch < 3; ++ch) {
                out[ch] = alpha > 0 ? to_srgb(static_cast<float>(sum[static_cast<std::size_t>(ch)] / alpha)) : 0U;
            }
            const int a = static_cast<int>(alpha * 255.0 + 0.5);
            out[3] = static_cast<std::uint8_t>(a < 0 ? 0 : a > 255 ? 255 : a);
        }
    }
    return rgba;
}

void put16(std::uint8_t* p, const unsigned v)
{
    p[0] = static_cast<std::uint8_t>(v);
    p[1] = static_cast<std::uint8_t>(v >> 8U);
}

void put32(std::uint8_t* p, const std::uint32_t v)
{
    for (unsigned i = 0; i < 4U; ++i) {
        p[i] = static_cast<std::uint8_t>(v >> (8U * i));
    }
}

// One icon image as a bitmap: the header, BGRA from the bottom row up, and the
// one-bit mask older renderers want, set where the picture is transparent.
std::vector<std::uint8_t> bitmap_entry(const std::vector<std::uint8_t>& rgba, const int size)
{
    const int mask_row = ((size + 31) / 32) * 4;
    const int len = 40 + size * size * 4 + mask_row * size;
    std::vector<std::uint8_t> b(static_cast<std::size_t>(len), 0U);
    put32(b.data(), 40U);
    put32(b.data() + 4, static_cast<std::uint32_t>(size));
    put32(b.data() + 8, static_cast<std::uint32_t>(size) * 2U);  // the colour and the mask, one above the other
    put16(b.data() + 12, 1U);
    put16(b.data() + 14, 32U);
    put32(b.data() + 20, static_cast<std::uint32_t>(len - 40));
    std::uint8_t* px = b.data() + 40;
    std::uint8_t* mask = px + static_cast<std::ptrdiff_t>(size) * size * 4;
    for (int y = 0; y < size; ++y) {
        const std::uint8_t* row = rgba.data() + static_cast<std::size_t>(size - 1 - y) * size * 4U;
        for (int x = 0; x < size; ++x) {
            std::uint8_t* p = px + (static_cast<std::ptrdiff_t>(y) * size + x) * 4;
            p[0] = row[x * 4 + 2];
            p[1] = row[x * 4 + 1];
            p[2] = row[x * 4 + 0];
            p[3] = row[x * 4 + 3];
            if (row[x * 4 + 3] == 0U) {
                mask[y * mask_row + x / 8] = static_cast<std::uint8_t>(mask[y * mask_row + x / 8] | (0x80U >> (x % 8)));
            }
        }
    }
    return b;
}

int fail(const std::string& message)
{
    std::fprintf(stderr, "error: %s\n", message.c_str());
    return 1;
}

} // namespace

int main(const int argc, char* argv[])
{
    if (argc < 4) {
        std::fprintf(stderr, "usage: %s <rom.pce> <out.ico> <out.h> [out.png]\n", argv[0]);
        return 2;
    }
    const std::filesystem::path rom_path = argv[1];
    const auto validation = devils_crush::rom::validate_file(rom_path);
    if (!validation.supported()) {
        return fail("'" + rom_path.string() + "' is not the supported ROM: " + validation.message);
    }
    std::ifstream in(rom_path, std::ios::binary);
    const std::vector<char> raw{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    std::vector<std::byte> rom(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        rom[i] = static_cast<std::byte>(raw[i]);
    }
    const auto animation = devils_crush::render::TitleLogoAnimation::decode(rom);
    const auto logo = animation ? animation->render(kLogoFrame) : std::nullopt;
    if (!logo || !logo->valid()) {
        return fail("the title logo could not be decoded from '" + rom_path.string() + "'");
    }
    const int w = logo->width;
    const int h = logo->height;
    const auto at = [&](const int x, const int y) {
        return logo->pixels[static_cast<std::size_t>(y) * w + x] & 0xFFFFFFU;
    };

    // The backdrop is the corners' colour. What of it the edge can reach,
    // four ways, is outside the drawing.
    const std::uint32_t backdrop = at(0, 0);
    std::vector<std::uint8_t> outside(static_cast<std::size_t>(w) * h, 0U);
    std::vector<int> stack;
    for (int x = 0; x < w; ++x) {
        stack.push_back(x);
        stack.push_back((h - 1) * w + x);
    }
    for (int y = 0; y < h; ++y) {
        stack.push_back(y * w);
        stack.push_back(y * w + w - 1);
    }
    while (!stack.empty()) {
        const int i = stack.back();
        stack.pop_back();
        const int x = i % w;
        const int y = i / w;
        if (outside[static_cast<std::size_t>(i)] != 0U || at(x, y) != backdrop) {
            continue;
        }
        outside[static_cast<std::size_t>(i)] = 1U;
        if (x > 0) stack.push_back(i - 1);
        if (x < w - 1) stack.push_back(i + 1);
        if (y > 0) stack.push_back(i - w);
        if (y < h - 1) stack.push_back(i + w);
    }

    std::vector<std::array<float, 4>> premultiplied(static_cast<std::size_t>(w) * h);
    for (int i = 0; i < w * h; ++i) {
        const std::uint32_t p = logo->pixels[static_cast<std::size_t>(i)];
        const float a = outside[static_cast<std::size_t>(i)] != 0U ? 0.0F : 1.0F;
        premultiplied[static_cast<std::size_t>(i)] = {
            to_linear(static_cast<std::uint8_t>(p >> 16U)) * a,
            to_linear(static_cast<std::uint8_t>(p >> 8U)) * a,
            to_linear(static_cast<std::uint8_t>(p)) * a,
            a,
        };
    }

    std::vector<std::vector<std::uint8_t>> images;
    for (const int size : kSizes) {
        auto rgba = render_size(premultiplied, w, h, size);
        if (size == 256 && argc > 4) {
            SDL_Surface* surface = SDL_CreateSurfaceFrom(size, size, SDL_PIXELFORMAT_RGBA32, rgba.data(), size * 4);
            const bool saved = surface != nullptr && SDL_SavePNG(surface, argv[4]);
            SDL_DestroySurface(surface);
            if (!saved) {
                return fail(std::string("cannot write '") + argv[4] + "': " + SDL_GetError());
            }
        }
        images.push_back(bitmap_entry(rgba, size));
    }

    std::vector<std::uint8_t> ico(6U, 0U);
    put16(ico.data() + 2, 1U);
    put16(ico.data() + 4, static_cast<unsigned>(kSizes.size()));
    std::uint32_t offset = 6U + 16U * static_cast<std::uint32_t>(kSizes.size());
    for (std::size_t i = 0; i < kSizes.size(); ++i) {
        std::array<std::uint8_t, 16> e{};
        e[0] = static_cast<std::uint8_t>(kSizes[i] & 0xFF);  // 0 is 256
        e[1] = static_cast<std::uint8_t>(kSizes[i] & 0xFF);
        put16(e.data() + 4, 1U);
        put16(e.data() + 6, 32U);
        put32(e.data() + 8, static_cast<std::uint32_t>(images[i].size()));
        put32(e.data() + 12, offset);
        offset += static_cast<std::uint32_t>(images[i].size());
        ico.insert(ico.end(), e.begin(), e.end());
    }
    for (const auto& image : images) {
        ico.insert(ico.end(), image.begin(), image.end());
    }
    {
        std::ofstream out(argv[2], std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(ico.data()), static_cast<std::streamsize>(ico.size()));
        if (!out) {
            return fail(std::string("cannot write '") + argv[2] + "'");
        }
    }

    {
        std::ofstream out(argv[3], std::ios::trunc);
        out << "// Made by tools/make_launcher_art.cpp from the ROM; not kept in the repository.\n";
        out << "#define DC_LOGO_W " << w << "\n#define DC_LOGO_H " << h << '\n';
        out << "static const uint32_t dc_logo[" << w * h << "] = {\n";
        char word[16];
        for (int i = 0; i < w * h; ++i) {
            std::snprintf(word, sizeof word, "0x%08x,", 0xFF000000U | logo->pixels[static_cast<std::size_t>(i)]);
            out << word << (i % 8 == 7 ? "\n" : "");
        }
        out << "};\n";
        if (!out) {
            return fail(std::string("cannot write '") + argv[3] + "'");
        }
    }
    std::printf("Wrote %s and %s: the title logo at frame %llu, %dx%d\n", argv[2], argv[3],
                static_cast<unsigned long long>(kLogoFrame), w, h);
    return 0;
}
