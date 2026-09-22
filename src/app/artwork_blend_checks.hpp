// SPDX-License-Identifier: GPL-3.0-or-later
// Included by main.cpp alongside the renderer diagnostic entry points.
[[nodiscard]] bool check_playfield_rotation()
{
    auto* surface = SDL_CreateSurface(120, 80, SDL_PIXELFORMAT_ARGB8888);
    auto* renderer = surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
    if (!renderer) { SDL_DestroySurface(surface); return false; }
    SourceTexture source{SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STATIC, 2, 2), 2, 2};
    StageTexture stage;
    const std::array<std::uint32_t, 4> colors{
        0xFFFF0000U, 0xFF00FF00U, 0xFF0000FFU, 0xFFFFFFFFU};
    bool ok = source.texture && configure_texture(source.texture, SDL_SCALEMODE_NEAREST)
        && SDL_UpdateTexture(source.texture, nullptr, colors.data(), 8);
    const auto pixel = [&](int x, int y) {
        return reinterpret_cast<const std::uint32_t*>(
            static_cast<const std::byte*>(surface->pixels) + y * surface->pitch)[x];
    };
    for (const int angle : {0, 90, 180, 270}) {
        ok = ok && present_frame(renderer, source, stage, 1, 1, {0, 0, 0},
            0, false, nullptr, 1, nullptr, angle);
        const std::size_t top_left = angle == 0 ? 0 : angle == 90 ? 2
            : angle == 180 ? 3 : 1;
        ok = ok && pixel(50, 30) == colors[top_left];
        // A bottom-aligned viewport must move to the rotated table's foot.
        ok = ok && present_frame(renderer, source, stage, 1, 1, {0, 0, 0},
            0, true, nullptr, 1, nullptr, angle);
        const int foot_x = angle == 90 ? 3 : angle == 270 ? 116 : 60;
        const int foot_y = angle == 0 ? 76 : angle == 180 ? 3 : 40;
        ok = ok && pixel(foot_x, foot_y) != 0xFF000000U;
        ok = ok && pixel(119 - foot_x, 79 - foot_y) == 0xFF000000U;
    }
    destroy_stage(stage);
    destroy_source(source);
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(surface);
    if (ok) std::cout << "Playfield rotation: four orientations and table-foot alignment passed.\n";
    else std::cerr << "Playfield rotation check failed.\n";
    return ok;
}

[[nodiscard]] bool check_artwork_blending(SDL_Renderer* renderer)
{
    auto* saved_target = SDL_GetRenderTarget(renderer);
    auto* target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_TARGET, 32, 32);
    if (!target) return false;
    bool ok = SDL_SetRenderTarget(renderer, target);
    const auto pixel = [&](int x, int y, std::uint32_t expected, const char* label) {
        auto* raw = SDL_RenderReadPixels(renderer, nullptr);
        auto* image = raw ? SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888) : nullptr;
        SDL_DestroySurface(raw);
        if (!image) return false;
        const auto* row = reinterpret_cast<const std::uint32_t*>(
            static_cast<const std::byte*>(image->pixels) + y * image->pitch);
        const auto actual = row[x];
        SDL_DestroySurface(image);
        for (unsigned shift : {0U, 8U, 16U}) {
            if (std::abs(static_cast<int>((actual >> shift) & 255U)
                    - static_cast<int>((expected >> shift) & 255U)) > 2) {
                std::cerr << "Artwork blend check failed: " << label << '\n';
                return false;
            }
        }
        return true;
    };
    using devils_crush::render::TableMotionFrame;
    using devils_crush::render::TableMotionSprite;
    TableMotionFrame first;
    first.background.image = {8, 8, std::vector<std::uint32_t>(64, 0xFF000000U)};
    first.background.opaque_pixels.resize(64);
    TableMotionSprite sprite;
    sprite.actor = {.slot = 0, .type = 1, .appearance = 2, .world_x = 1, .world_y = 1};
    sprite.x = sprite.y = 1;
    sprite.image = {2, 2, std::vector<std::uint32_t>(4, 0xFFFF0000U)};
    first.sprites.push_back(sprite);
    auto second = first;
    second.background.image.pixels.assign(64, 0xFF202020U);
    second.sprites[0].actor.world_x = 3;
    second.sprites[0].x = 3;
    second.sprites[0].image = {2, 1, {0xFF0000FFU, 0xFF0000FFU}};
    TableMotionPresenter table;
    ok = ok && table.update(renderer, first) && table.update(renderer, second)
        && table.draw(renderer, 32, 32, 0.5F, false)
        && pixel(8, 4, 0xFF800080U, "moving artwork shares one position")
        && pixel(4, 4, 0xFF101010U, "no old-position trail")
        && pixel(8, 8, 0xFF901010U, "pose silhouette blends through transparency")
        && pixel(0, 0, 0xFF101010U, "background palette midpoint")
        && table.draw(renderer, 32, 32, 0.0F, false)
        && pixel(4, 4, 0xFFFF0000U, "previous endpoint")
        && table.draw(renderer, 32, 32, 1.0F, false)
        && pixel(12, 4, 0xFF0000FFU, "current endpoint");

    // Appearance changes are artwork transitions, but reused actors and
    // teleports must not blend an unrelated pose or stale position.
    table.reset();
    second = first;
    second.sprites[0].actor.appearance++;
    second.sprites[0].image.pixels.assign(4, 0xFF0000FFU);
    ok = ok && table.update(renderer, first) && table.update(renderer, second)
        && table.draw(renderer, 32, 32, 0.5F, false)
        && pixel(4, 4, 0xFF800080U, "appearance transformation");
    second.sprites[0].actor.type++;
    second.sprites[0].image.pixels.assign(4, 0xFF00FF00U);
    ok = ok && table.update(renderer, second) && table.draw(renderer, 32, 32, 0.0F, false)
        && pixel(4, 4, 0xFF00FF00U, "slot reuse cuts artwork history");
    second.sprites[0].actor.world_x += 40;
    second.sprites[0].x = 5;
    ok = ok && table.update(renderer, second) && table.draw(renderer, 32, 32, 0.5F, false)
        && pixel(4, 4, 0xFF000000U, "teleport removes old position")
        && pixel(20, 4, 0xFF00FF00U, "teleport appears at destination");

    table.reset();
    first.sprites.push_back(first.sprites[0]);
    first.sprites.back().image.pixels.assign(4, 0xFF00FF00U);
    second = first;
    second.sprites.back().image.pixels.assign(4, 0xFF0000FFU);
    ok = ok && table.update(renderer, first) && table.update(renderer, second)
        && table.draw(renderer, 32, 32, 0.5F, false)
        && pixel(4, 4, 0xFF008080U, "overlapping pieces blend after composition");
    table.reset();
    first.sprites.resize(1);
    first.sprites[0].foreground = false;
    second = first;
    second.background.opaque_pixels[1 * 8 + 1] = true;
    ok = ok && table.update(renderer, first) && table.update(renderer, second)
        && table.draw(renderer, 32, 32, 0.5F, false)
        && pixel(4, 4, 0xFF800000U, "each pose retains its background priority mask");

    FrameArtworkPresenter viewport;
    std::vector<std::uint32_t> colors(4, 0xFFFF0000U);
    const auto update = [&](int scene) {
        return viewport.update(renderer, {.pixels = colors, .width = 2, .height = 2,
            .pitch_pixels = 2}, scene);
    };
    ok = ok && update(3);
    colors.assign(4, 0xFF0000FFU);
    ok = ok && update(3) && viewport.draw(renderer, 0.5F)
        && pixel(0, 0, 0xFF800080U, "viewport palette midpoint")
        && viewport.draw(renderer, 1.0F)
        && pixel(0, 0, 0xFF0000FFU, "viewport restores opaque endpoint");
    colors.assign(4, 0xFF00FF00U);
    ok = ok && update(259) && viewport.draw(renderer, 0.0F)
        && pixel(0, 0, 0xFF00FF00U, "bonus/title scene cut");
    viewport.reset();
    table.reset();
    const bool restored = SDL_SetRenderTarget(renderer, saved_target);
    SDL_DestroyTexture(target);
    if (ok) std::cout << "Artwork blending: poses, palettes, priority, motion alignment and scene cuts passed.\n";
    return ok && restored;
}

[[nodiscard]] bool check_viewport_artwork(SDL_Renderer* renderer, PceRuntime& runtime,
    const std::filesystem::path& directory)
{
    auto* saved_target = SDL_GetRenderTarget(renderer);
    SourceTexture target;
    FrameArtworkPresenter presenter;
    bool ok = true;
    for (std::uint8_t route = 0; route <= 6 && ok; ++route) {
        runtime.reset();
        presenter.reset();
        if (route != 0 && !prepare_bonus_stage_launch(runtime, route)) { ok = false; break; }
        bool returning = route != 0;
        int endpoints = 0;
        int changed = 0;
        for (int tick = 1; tick <= 240 && ok; ++tick) {
            const auto frame = runtime.run_frame({});
            if (route != 0 && !service_synthetic_bonus_stage_return(runtime, returning)) {
                ok = false;
                break;
            }
            const auto ram = runtime.export_work_ram();
            const auto state = devils_crush::cabinet::decode_cabinet_state(ram);
            if (!state) { ok = false; break; }
            const auto& view = frame.framebuffer;
            if (!target.texture || target.width != view.width || target.height != view.height) {
                SDL_SetRenderTarget(renderer, saved_target);
                destroy_source(target);
                target.texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                    SDL_TEXTUREACCESS_TARGET, view.width, view.height);
                target.width = view.width;
                target.height = view.height;
            }
            ok = target.texture && SDL_SetRenderTarget(renderer, target.texture)
                && presenter.update(renderer, view,
                    state->screen_selector + (state->bonus_stage ? 256 : 0));
            if (!ok || tick % 7 != 0) continue;
            const auto read = [&](float alpha) -> SDL_Surface* {
                if (!presenter.draw(renderer, alpha)) return nullptr;
                auto* raw = SDL_RenderReadPixels(renderer, nullptr);
                auto* result = raw ? SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888) : nullptr;
                SDL_DestroySurface(raw);
                return result;
            };
            auto* endpoint = read(1.0F);
            if (!endpoint) { ok = false; break; }
            for (int row = 0; row < view.height && ok; ++row) {
                ok = std::memcmp(static_cast<const std::byte*>(endpoint->pixels) + row * endpoint->pitch,
                    view.pixels.data() + row * view.pitch_pixels,
                    static_cast<std::size_t>(view.width) * sizeof(std::uint32_t)) == 0;
            }
            if (ok) ++endpoints;
            auto* quarter = read(0.25F);
            if (!quarter) ok = false;
            bool differs = false;
            for (int row = 0; quarter && row < view.height; ++row) {
                differs |= std::memcmp(static_cast<const std::byte*>(endpoint->pixels) + row * endpoint->pitch,
                    static_cast<const std::byte*>(quarter->pixels) + row * quarter->pitch,
                    static_cast<std::size_t>(view.width) * sizeof(std::uint32_t)) != 0;
            }
            if (differs && ++changed == 3) {
                const auto name = route == 0 ? std::string{"title"} : "bonus-" + std::to_string(route);
                ok = ok && SDL_SaveBMP(quarter, (directory / (name + "-quarter.bmp")).string().c_str());
            }
            SDL_DestroySurface(endpoint);
            SDL_DestroySurface(quarter);
            ok = ok && runtime.export_work_ram() == ram;
        }
        std::cout << (route == 0 ? "Title" : "Bonus " + std::to_string(route))
                  << " artwork: " << endpoints << " exact endpoints; " << changed
                  << " distinct quarter-frame pictures; gameplay RAM unchanged.\n";
        ok = ok && endpoints > 0 && changed > 0;
    }
    presenter.reset();
    const bool restored = SDL_SetRenderTarget(renderer, saved_target);
    destroy_source(target);
    return ok && restored;
}
