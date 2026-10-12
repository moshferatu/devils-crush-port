// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"
#include "host_textures.hpp"

namespace devils_crush::app::host {

struct TableMotionLayer {
    devils_crush::render::TableMotionFrame frame;
    std::array<std::optional<devils_crush::cabinet::TableActorState>,
        devils_crush::cabinet::kActorSlotCount> previous{};
    std::array<std::pair<float, float>, devils_crush::cabinet::kActorSlotCount> correction{};
    std::array<std::pair<float, float>, devils_crush::cabinet::kActorSlotCount> prior_correction{};
    // The previous frame's turn per slot, so a trail only reaches back into a
    // swing that was already going the same way.
    std::array<float, devils_crush::cabinet::kActorSlotCount> prior_turn{};
    SourceTexture background;
    SourceTexture atlas;

    void reset()
    {
        frame = {};
        previous = {};
        correction = {};
        prior_correction = {};
        prior_turn = {};
        destroy_source(background);
        destroy_source(atlas);
    }

    // Gives each table flipper's pieces the turn from this frame's pose to
    // the next one's. Bonus-room pieces arrive with theirs already measured.
    void measure_swings(devils_crush::render::TableMotionFrame& next) const
    {
        std::array<bool, devils_crush::cabinet::kActorSlotCount> measured{};
        for (const auto& sprite : next.sprites) {
            const auto slot = sprite.actor.slot;
            if (!sprite.swings || sprite.swing || measured[slot]) continue;
            measured[slot] = true;
            const auto turn = devils_crush::render::table_swing(frame.sprites, next.sprites, slot);
            for (auto& piece : next.sprites)
                if (piece.actor.slot == slot) piece.swing = turn;
        }
    }

    bool update(SDL_Renderer* renderer,
        devils_crush::render::TableMotionFrame next)
    {
        const auto older = previous;
        prior_correction = correction;
        correction = {};
        previous = {};
        for (const auto& sprite : frame.sprites)
            previous[sprite.actor.slot] = sprite.actor;
        prior_turn = {};
        for (const auto& sprite : frame.sprites)
            if (sprite.swing) prior_turn[sprite.actor.slot] = sprite.swing->degrees;
        measure_swings(next);
        frame = std::move(next);
        for (const auto& sprite : frame.sprites) {
            const auto slot = sprite.actor.slot;
            correction[slot] = devils_crush::render::table_motion_correction(sprite.actor,
                previous[slot] ? &*previous[slot] : nullptr,
                older[slot] ? &*older[slot] : nullptr, frame.half_pixel_steps);
        }
        if (!upload_image(renderer, frame.background.image, background)) return false;
        devils_crush::render::ArgbImage pixels{
            1024, static_cast<int>(std::max<std::size_t>(1,
                (frame.sprites.size() + 15) / 16)) * 64, {}};
        pixels.pixels.resize(static_cast<std::size_t>(pixels.width) * pixels.height);
        for (std::size_t i = 0; i < frame.sprites.size(); ++i) {
            const auto& image = frame.sprites[i].image;
            if (!image.valid() || image.width > 64 || image.height > 64) return false;
            const int x = static_cast<int>(i % 16) * 64;
            const int y = static_cast<int>(i / 16) * 64;
            for (int row = 0; row < image.height; ++row)
                std::copy_n(image.pixels.begin() + row * image.width, image.width,
                    pixels.pixels.begin() + (y + row) * pixels.width + x);
        }
        return upload_image(renderer, pixels, atlas)
            && SDL_SetTextureBlendMode(atlas.texture, SDL_BLENDMODE_BLEND);
    }

    // `trail_from` is the alpha the previous present was drawn at, negative
    // when it fell in the previous frame. A swinging flipper leaves a fading
    // trail back to where that present showed it, so the angles a display
    // has no present for are swept rather than skipped.
    bool draw(SDL_Renderer* renderer, int width, int height, float alpha, bool even = true,
        float trail_from = 2.0F) const
    {
        if (!background.texture || !atlas.texture) return false;
        if (!SDL_RenderTexture(renderer, background.texture, nullptr, nullptr)) return false;
        const float sx = static_cast<float>(width) / frame.background.image.width;
        const float sy = static_cast<float>(height) / frame.background.image.height;
        for (std::size_t i = 0; i < frame.sprites.size(); ++i) {
            const auto& sprite = frame.sprites[i];
            const auto& old = previous[sprite.actor.slot];
            auto [dx, dy] = devils_crush::render::table_motion_offset(
                sprite.actor, old ? &*old : nullptr, alpha);
            if (even && devils_crush::render::table_motion_linked(sprite.actor, old ? &*old : nullptr)) {
                const auto slot = sprite.actor.slot;
                const float t = std::clamp(alpha, 0.0F, 1.0F);
                dx += prior_correction[slot].first * (1.0F - t) + correction[slot].first * t;
                dy += prior_correction[slot].second * (1.0F - t) + correction[slot].second * t;
            }
            const float x = sprite.x + dx;
            const float y = sprite.y + dy;
            const float ax = static_cast<float>(i % 16) * 64;
            const float ay = static_cast<float>(i / 16) * 64;
            const auto piece = [&](float left, float top, float right, float bottom) {
                const SDL_FRect src{ax + left - x, ay + top - y,
                    right - left, bottom - top};
                const SDL_FRect dst{left * sx, top * sy,
                    (right - left) * sx, (bottom - top) * sy};
                return SDL_RenderTexture(renderer, atlas.texture, &src, &dst);
            };
            if (sprite.foreground && sprite.swing
                && !draw_swing(renderer, sprite, x, y, ax, ay, sx, sy, alpha, trail_from)) return false;
            if (sprite.foreground && sprite.swing) continue;
            if (sprite.foreground) {
                if (!piece(x, y, x + sprite.image.width, y + sprite.image.height)) return false;
                continue;
            }
            // Clip behind-background sprites against the stationary table at
            // their interpolated location. This also preserves sprite priority:
            // painting the background back over a sprite would erase earlier actors.
            const int w = frame.background.image.width;
            const int h = frame.background.image.height;
            const int left = std::max(0, static_cast<int>(std::floor(x)));
            const int right = std::min(w, static_cast<int>(std::ceil(x + sprite.image.width)));
            const int top = std::max(0, static_cast<int>(std::floor(y)));
            const int bottom = std::min(h, static_cast<int>(std::ceil(y + sprite.image.height)));
            for (int row = top; row < bottom; ++row) {
                for (int col = left; col < right;) {
                    if (frame.background.opaque_pixels[row * w + col]) { ++col; continue; }
                    const int start = col++;
                    while (col < right && !frame.background.opaque_pixels[row * w + col]) ++col;
                    if (!piece(std::max(x, static_cast<float>(start)),
                        std::max(y, static_cast<float>(row)),
                        std::min(x + sprite.image.width, static_cast<float>(col)),
                        std::min(y + sprite.image.height, static_cast<float>(row + 1)))) return false;
                }
            }
        }
        return true;
    }

    // Every piece of a flipper turns about its one pivot. At the frame's own
    // alpha with no trail this is the plain, pixel-exact draw.
    bool draw_swing(SDL_Renderer* renderer, const devils_crush::render::TableMotionSprite& sprite,
        float x, float y, float ax, float ay, float sx, float sy, float alpha, float trail_from) const
    {
        constexpr float kLongestTrail = 0.5F;
        constexpr float kTrailSpacing = 2.0F;
        constexpr int kMostTrailCopies = 16;
        const auto& turn = *sprite.swing;
        const float t = std::clamp(alpha, 0.0F, 1.0F);
        float from = std::clamp(trail_from, t - kLongestTrail, t);
        if (from < 0.0F && prior_turn[sprite.actor.slot] * turn.degrees <= 0.0F) from = std::max(from, 0.0F);
        from = std::min(from, t);
        const auto angle_at = [&](float a) { return turn.degrees * (1.0F - a); };
        const float w = static_cast<float>(sprite.image.width);
        const float h = static_cast<float>(sprite.image.height);
        const float px = turn.pivot_x - x;
        const float py = turn.pivot_y - y;
        float reach = 0.0F;
        for (const auto [cx, cy] : {std::pair{0.0F, 0.0F}, {w, 0.0F}, {0.0F, h}, {w, h}})
            reach = std::max(reach, std::hypot((cx - px) * sx, (cy - py) * sy));
        const float arc = reach * std::abs(angle_at(from) - angle_at(t)) * std::numbers::pi_v<float> / 180.0F;
        const int copies = std::min(kMostTrailCopies, static_cast<int>(std::ceil(arc / kTrailSpacing)));
        const SDL_FRect src{ax, ay, w, h};
        const SDL_FRect dst{x * sx, y * sy, w * sx, h * sy};
        const SDL_FPoint center{px * sx, py * sy};
        const auto turned = [&](float a, float opacity) {
            return SDL_SetTextureAlphaModFloat(atlas.texture, opacity)
                && SDL_RenderTextureRotated(renderer, atlas.texture, &src, &dst,
                    angle_at(a), &center, SDL_FLIP_NONE);
        };
        bool ok = true;
        // Oldest and faintest first, so the newest angles lie on top.
        for (int copy = copies; copy >= 1 && ok; --copy)
            ok = turned(t - (t - from) * static_cast<float>(copy) / copies,
                1.0F - static_cast<float>(copy) / (copies + 1));
        ok = ok && (angle_at(t) != 0.0F ? turned(t, 1.0F)
            : SDL_SetTextureAlphaModFloat(atlas.texture, 1.0F)
                && SDL_RenderTexture(renderer, atlas.texture, &src, &dst));
        return SDL_SetTextureAlphaModFloat(atlas.texture, 1.0F) && ok;
    }
};

// Blend complete opaque compositions, rather than alpha-modulating individual
// sprites: overlapping pieces and transparent holes must not darken or leak.
struct ArtworkBlendTarget {
    SourceTexture target;

    void reset() { destroy_source(target); }

    template<class Old, class Current>
    bool draw(SDL_Renderer* renderer, int width, int height, float alpha,
        Old draw_old, Current draw_current)
    {
        const float t = std::clamp(alpha, 0.0F, 1.0F);
        if (t >= 1.0F) return draw_current();
        if (t <= 0.0F) return draw_old();
        if (!target.texture || target.width != width || target.height != height) {
            reset();
            target.texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                SDL_TEXTUREACCESS_TARGET, width, height);
            target.width = width;
            target.height = height;
            if (!target.texture
                || !SDL_SetTextureScaleMode(target.texture, SDL_SCALEMODE_NEAREST)
                || !SDL_SetTextureBlendMode(target.texture, SDL_BLENDMODE_BLEND)) return false;
        }
        auto* destination = SDL_GetRenderTarget(renderer);
        if (!SDL_SetRenderTarget(renderer, target.texture)) return false;
        const bool composed = draw_current();
        const bool restored = SDL_SetRenderTarget(renderer, destination);
        return composed && restored && draw_old()
            && SDL_SetTextureAlphaModFloat(target.texture, t)
            && SDL_RenderTexture(renderer, target.texture, nullptr, nullptr);
    }
};

struct TableMotionPresenter : TableMotionLayer {
    TableMotionLayer old_art;
    ArtworkBlendTarget blend;
    // The alpha of the last draw: within [0, 1] for this frame, negative for
    // the frame before, and above one when neither drew.
    float drawn_alpha = 2.0F;

    void reset()
    {
        TableMotionLayer::reset();
        old_art.reset();
        blend.reset();
        drawn_alpha = 2.0F;
    }

    bool update(SDL_Renderer* renderer, devils_crush::render::TableMotionFrame next)
    {
        // Reproject the previous pose into the current actor's coordinate
        // system. Both artwork endpoints then use precisely the same motion
        // offset, so a moving ball does not acquire a second trailing image.
        // Flippers are measured first, so both endpoints turn them alike.
        measure_swings(next);
        drawn_alpha = drawn_alpha >= 0.0F && drawn_alpha <= 1.0F ? drawn_alpha - 1.0F : 2.0F;
        devils_crush::render::TableMotionFrame prior;
        if (frame.background.image.valid()
            && frame.background.image.width == next.background.image.width
            && frame.background.image.height == next.background.image.height) {
            prior.background = frame.background;
            std::array<bool, devils_crush::cabinet::kActorSlotCount> visited{};
            for (const auto& sprite : next.sprites) {
                const auto slot = sprite.actor.slot;
                if (visited[slot]) continue;
                visited[slot] = true;
                const auto old = std::ranges::find_if(frame.sprites, [&](const auto& item) {
                    return item.actor.slot == slot;
                });
                // A flipper's poses turn into one another instead of fading.
                const bool linked = old != frame.sprites.end() && !sprite.swings && !sprite.swing
                    && devils_crush::render::table_art_linked(sprite.actor, &old->actor);
                const auto& pieces = linked ? frame.sprites : next.sprites;
                for (const auto& piece : pieces) {
                    if (piece.actor.slot != slot) continue;
                    auto copy = piece;
                    copy.x += sprite.actor.world_x - piece.actor.world_x;
                    copy.y += sprite.actor.world_y - piece.actor.world_y;
                    copy.actor = sprite.actor;
                    prior.sprites.push_back(std::move(copy));
                }
            }
        }
        if (!TableMotionLayer::update(renderer, std::move(next))) return false;
        if (!prior.background.image.valid()) {
            old_art.reset();
            return true;
        }
        if (!old_art.update(renderer, std::move(prior))) return false;
        old_art.previous = previous;
        old_art.correction = correction;
        old_art.prior_correction = prior_correction;
        old_art.prior_turn = prior_turn;
        return true;
    }

    bool draw(SDL_Renderer* renderer, int width, int height, float alpha, bool even = true)
    {
        const float from = drawn_alpha;
        drawn_alpha = alpha;
        const auto current = [&] {
            return TableMotionLayer::draw(renderer, width, height, alpha, even, from);
        };
        if (!old_art.background.texture) return current();
        return blend.draw(renderer, width, height, alpha,
            [&] { return old_art.draw(renderer, width, height, alpha, even, from); }, current);
    }
};

// The original viewport has no stable actor layers. Blend its final pictures
// for bonus stages, title/high scores and the single-window presentation.
struct FrameArtworkPresenter {
    SourceTexture previous;
    SourceTexture current;
    int scene = -1;

    void reset()
    {
        destroy_source(previous);
        destroy_source(current);
        scene = -1;
    }

    bool update(SDL_Renderer* renderer, const FramebufferView& frame, int next_scene)
    {
        if (scene != next_scene || current.width != frame.width || current.height != frame.height)
            reset();
        std::swap(previous, current);
        scene = next_scene;
        return upload_framebuffer(renderer, frame, current);
    }

    bool draw(SDL_Renderer* renderer, float alpha) const
    {
        if (!current.texture) return false;
        const float t = previous.texture ? std::clamp(alpha, 0.0F, 1.0F) : 1.0F;
        // Restore modulation on every draw, including F6 / endpoint draws.
        if (!SDL_SetTextureBlendMode(current.texture, SDL_BLENDMODE_NONE)
            || !SDL_SetTextureAlphaModFloat(current.texture, 1.0F)) return false;
        if (t >= 1.0F) return SDL_RenderTexture(renderer, current.texture, nullptr, nullptr);
        if (!SDL_SetTextureBlendMode(previous.texture, SDL_BLENDMODE_NONE)
            || !SDL_SetTextureAlphaModFloat(previous.texture, 1.0F)
            || !SDL_RenderTexture(renderer, previous.texture, nullptr, nullptr)) return false;
        return SDL_SetTextureBlendMode(current.texture, SDL_BLENDMODE_BLEND)
            && SDL_SetTextureAlphaModFloat(current.texture, t)
            && SDL_RenderTexture(renderer, current.texture, nullptr, nullptr);
    }
};

// Bonus rooms drawn as their background and hardware sprites
// (render/viewport_motion.hpp), so the sprites move between game frames the
// way the table's actors do. `ready` is false on any frame the pieces could
// not reproduce exactly; the caller then blends complete pictures instead.
struct ViewportMotionPresenter {
    devils_crush::render::ViewportMotionTracker tracker;
    TableMotionPresenter pieces;
    bool ready = false;

    void reset()
    {
        tracker.reset();
        pieces.reset();
        ready = false;
    }

    void update(SDL_Renderer* renderer, const devils_crush::emulation::VideoMemorySnapshot& video,
        const FramebufferView& view)
    {
        auto frame = tracker.update(
            {
                .vram_words = video.vram_words,
                .sat_words = video.sat_words,
                .palette_entries = video.palette_entries,
                .memory_width_register = video.memory_width_register,
                .background_x_scroll = video.background_x_scroll,
                .background_y_scroll = video.background_y_scroll,
            },
            {.pixels = view.pixels, .width = view.width, .height = view.height,
                .pitch_pixels = view.pitch_pixels});
        ready = frame && pieces.update(renderer, std::move(*frame));
        if (!ready) pieces.reset();
    }
};

// Measure completed presents, not the monitor mode or the requested VSync.
// A short sample after each toggle exposes a renderer that is repeating one
// game picture despite accepting the high-refresh configuration.
struct TablePresentationSample {
    using Clock = std::chrono::steady_clock;
    Clock::time_point start{};
    Clock::time_point last{};
    int first_tick = 0;
    int last_tick = 0;
    std::uint64_t presents = 0;
    std::uint64_t intermediate = 0;
    double longest_gap_ms = 0;
    bool reported = false;

    void observe(int tick, bool interpolated, Clock::time_point now)
    {
        if (reported) return;
        if (presents == 0) {
            start = now;
            first_tick = tick;
        } else {
            longest_gap_ms = std::max(longest_gap_ms,
                std::chrono::duration<double, std::milli>(now - last).count());
            if (interpolated && tick == last_tick) ++intermediate;
        }
        last = now;
        last_tick = tick;
        ++presents;
    }

    void report(bool enabled)
    {
        const double seconds = std::chrono::duration<double>(last - start).count();
        if (reported || presents < 2 || seconds < 1.0) return;
        std::cout << std::format(
            "Playfield presentation (smoothing {}): {:.1f} presents/s, {:.1f} game frames/s; "
            "{} intermediate presents in {:.1f}s; longest gap {:.1f} ms.\n",
            enabled ? "on" : "off", (presents - 1) / seconds,
            (last_tick - first_tick) / seconds, intermediate, seconds, longest_gap_ms);
        reported = true;
    }
};

void update_bonus_room_motion(SDL_Renderer* renderer, PceRuntime& runtime,
    const std::optional<devils_crush::cabinet::CabinetState>& state,
    const FramebufferView& view, ViewportMotionPresenter& motion);

} // namespace devils_crush::app::host
