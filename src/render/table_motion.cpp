// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/render/table_motion.hpp"

#include <cmath>
#include <numbers>

namespace devils_crush::render {
namespace {

// Fewer drawn pixels than this has no reliable long axis.
constexpr double kMinimumPixels = 32.0;
// Steps smaller than this are already as smooth as they can be drawn; larger
// ones are not a flipper's next pose.
constexpr double kSmallestTurn = std::numbers::pi / 180.0;
constexpr double kLargestTurn = kLargestTableSwing * std::numbers::pi / 180.0;
// A flipper is long and thin: every pose of the table's and the bonus rooms'
// flippers spreads at least 7.24 times as far along its long axis as across
// it. The swinging dragon heads of the first bonus room, the stubbiest thing
// that otherwise measures as a turn, reach 6.23 at most in one of their poses.
constexpr double kLeastElongation = 7.0;
// A flipper's poses are one picture turned: at least nine in ten of the
// previous pose's pixels, turned, land on the current pose's. Redrawn art
// such as the dragon heads fits as poorly as three in four.
constexpr double kLeastFit = 0.85;
// The pivot lies inside the flipper's round end, on its own drawn pixels. A
// shape that only shifted measures a pivot well outside itself.
constexpr int kPivotReach = 2;

struct Shape {
    double x = 0.0;
    double y = 0.0;
    double angle = 0.0;
};

// The centroid and long axis of every drawn pixel the slot's pieces cover,
// the axis measured from +x toward +y, as SDL turns.
[[nodiscard]] std::optional<Shape> shape_of(
    const std::span<const TableMotionSprite> sprites, const std::uint8_t slot)
{
    double count = 0.0;
    double sum_x = 0.0;
    double sum_y = 0.0;
    double sum_xx = 0.0;
    double sum_yy = 0.0;
    double sum_xy = 0.0;
    for (const auto& sprite : sprites) {
        if (sprite.actor.slot != slot) continue;
        for (int row = 0; row < sprite.image.height; ++row) {
            for (int column = 0; column < sprite.image.width; ++column) {
                if (sprite.image.pixels[static_cast<std::size_t>(row) * sprite.image.width + column] == 0U)
                    continue;
                const double x = sprite.x + column + 0.5;
                const double y = sprite.y + row + 0.5;
                count += 1.0;
                sum_x += x;
                sum_y += y;
                sum_xx += x * x;
                sum_yy += y * y;
                sum_xy += x * y;
            }
        }
    }
    if (count < kMinimumPixels) return std::nullopt;
    const double x = sum_x / count;
    const double y = sum_y / count;
    const double xx = sum_xx / count - x * x;
    const double yy = sum_yy / count - y * y;
    const double xy = sum_xy / count - x * y;
    const double spread = std::hypot(xx - yy, 2.0 * xy);
    const double along = 0.5 * (xx + yy + spread);
    const double across = 0.5 * (xx + yy - spread);
    if (along < kLeastElongation * across) return std::nullopt;
    return Shape{x, y, 0.5 * std::atan2(2.0 * xy, xx - yy)};
}

[[nodiscard]] bool drawn_near(const std::span<const TableMotionSprite> sprites,
    const std::uint8_t slot, const double x, const double y, const int reach = kPivotReach)
{
    const int px = static_cast<int>(std::floor(x));
    const int py = static_cast<int>(std::floor(y));
    for (const auto& sprite : sprites) {
        if (sprite.actor.slot != slot) continue;
        for (int dy = -reach; dy <= reach; ++dy) {
            for (int dx = -reach; dx <= reach; ++dx) {
                const int column = px + dx - sprite.x;
                const int row = py + dy - sprite.y;
                if (column >= 0 && row >= 0 && column < sprite.image.width && row < sprite.image.height
                    && sprite.image.pixels[static_cast<std::size_t>(row) * sprite.image.width + column] != 0U)
                    return true;
            }
        }
    }
    return false;
}

// The share of the previous pose's drawn pixels that, turned about the pivot,
// land on a drawn pixel of the current pose.
[[nodiscard]] double turned_fit(const std::span<const TableMotionSprite> previous,
    const std::span<const TableMotionSprite> current, const std::uint8_t slot,
    const double pivot_x, const double pivot_y, const double turn)
{
    const double c = std::cos(-turn);
    const double s = std::sin(-turn);
    double drawn = 0.0;
    double landed = 0.0;
    for (const auto& sprite : previous) {
        if (sprite.actor.slot != slot) continue;
        for (int row = 0; row < sprite.image.height; ++row) {
            for (int column = 0; column < sprite.image.width; ++column) {
                if (sprite.image.pixels[static_cast<std::size_t>(row) * sprite.image.width + column] == 0U)
                    continue;
                const double x = sprite.x + column + 0.5 - pivot_x;
                const double y = sprite.y + row + 0.5 - pivot_y;
                drawn += 1.0;
                if (drawn_near(current, slot, pivot_x + c * x - s * y, pivot_y + s * x + c * y, 0))
                    landed += 1.0;
            }
        }
    }
    return drawn > 0.0 ? landed / drawn : 0.0;
}

} // namespace

std::optional<TableSwing> table_swing(
    const std::span<const TableMotionSprite> previous,
    const std::span<const TableMotionSprite> current,
    const std::uint8_t slot)
{
    const auto now = std::ranges::find_if(current, [slot](const TableMotionSprite& sprite) {
        return sprite.actor.slot == slot;
    });
    const auto then = std::ranges::find_if(previous, [slot](const TableMotionSprite& sprite) {
        return sprite.actor.slot == slot;
    });
    if (now == current.end() || then == previous.end() || !now->swings
        || !table_art_linked(now->actor, &then->actor)
        || now->actor.world_x != then->actor.world_x
        || now->actor.world_y != then->actor.world_y) {
        return std::nullopt;
    }
    const auto from = shape_of(previous, slot);
    const auto to = shape_of(current, slot);
    if (!from || !to) return std::nullopt;
    double turn = from->angle - to->angle;
    if (turn > std::numbers::pi / 2.0) turn -= std::numbers::pi;
    if (turn <= -std::numbers::pi / 2.0) turn += std::numbers::pi;
    if (std::abs(turn) < kSmallestTurn || std::abs(turn) > kLargestTurn) return std::nullopt;

    // The previous pose turned by -turn about the pivot lands on the current
    // one: (I - R) p = c_now - R c_then.
    const double c = std::cos(-turn);
    const double s = std::sin(-turn);
    const double right_x = to->x - (c * from->x - s * from->y);
    const double right_y = to->y - (s * from->x + c * from->y);
    const double a = 1.0 - c;
    const double b = s;
    const double determinant = a * a + b * b;
    const double pivot_x = (a * right_x - b * right_y) / determinant;
    const double pivot_y = (b * right_x + a * right_y) / determinant;
    if (!drawn_near(current, slot, pivot_x, pivot_y)) return std::nullopt;
    if (turned_fit(previous, current, slot, pivot_x, pivot_y, turn) < kLeastFit) return std::nullopt;
    return TableSwing{
        .pivot_x = static_cast<float>(pivot_x),
        .pivot_y = static_cast<float>(pivot_y),
        .degrees = static_cast<float>(turn * 180.0 / std::numbers::pi),
    };
}

} // namespace devils_crush::render
