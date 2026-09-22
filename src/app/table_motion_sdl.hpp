// SPDX-License-Identifier: GPL-3.0-or-later
// Included by main.cpp after the host's source-texture helpers.

struct TableMotionLayer {
    devils_crush::render::TableMotionFrame frame;
    std::array<std::optional<devils_crush::cabinet::TableActorState>,
        devils_crush::cabinet::kActorSlotCount> previous{};
    std::array<std::pair<float, float>, devils_crush::cabinet::kActorSlotCount> correction{};
    std::array<std::pair<float, float>, devils_crush::cabinet::kActorSlotCount> prior_correction{};
    SourceTexture background;
    SourceTexture atlas;

    void reset()
    {
        frame = {};
        previous = {};
        correction = {};
        prior_correction = {};
        destroy_source(background);
        destroy_source(atlas);
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
        frame = std::move(next);
        for (const auto& sprite : frame.sprites) {
            const auto slot = sprite.actor.slot;
            correction[slot] = devils_crush::render::table_motion_correction(sprite.actor,
                previous[slot] ? &*previous[slot] : nullptr,
                older[slot] ? &*older[slot] : nullptr);
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

    bool draw(SDL_Renderer* renderer, int width, int height, float alpha, bool even = true) const
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

    void reset()
    {
        TableMotionLayer::reset();
        old_art.reset();
        blend.reset();
    }

    bool update(SDL_Renderer* renderer, devils_crush::render::TableMotionFrame next)
    {
        // Reproject the previous pose into the current actor's coordinate
        // system. Both artwork endpoints then use precisely the same motion
        // offset, so a moving ball does not acquire a second trailing image.
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
                const bool linked = old != frame.sprites.end()
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
        return true;
    }

    bool draw(SDL_Renderer* renderer, int width, int height, float alpha, bool even = true)
    {
        const auto current = [&] {
            return TableMotionLayer::draw(renderer, width, height, alpha, even);
        };
        if (!old_art.background.texture) return current();
        return blend.draw(renderer, width, height, alpha,
            [&] { return old_art.draw(renderer, width, height, alpha, even); }, current);
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
