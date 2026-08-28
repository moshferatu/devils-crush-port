// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstdint>

namespace devils_crush::native_runtime {

// The ROM's target-solidity data is an eight-row, eight-column bitmap. Rows
// are stored top to bottom; columns use the conventional high-bit-first order
// (`0x80`, `0x40`, …, `0x01`) verified from the table's mask selector.
struct CollisionMask {
    static constexpr std::uint8_t kWidth = 8;
    static constexpr std::uint8_t kHeight = 8;

    std::array<std::uint8_t, kHeight> rows{};

    [[nodiscard]] bool operator==(const CollisionMask&) const = default;
};

struct CollisionMaskCell {
    std::uint8_t column = 0;
    std::uint8_t row = 0;

    [[nodiscard]] bool operator==(const CollisionMaskCell&) const = default;
};

enum class CollisionMaskDisposition {
    outside_mask,
    empty,
    solid,
};

struct CollisionMaskEvaluation {
    CollisionMaskCell cell;
    std::uint8_t selected_row = 0;
    std::uint8_t column_bit = 0;
    CollisionMaskDisposition disposition = CollisionMaskDisposition::outside_mask;

    [[nodiscard]] bool operator==(const CollisionMaskEvaluation&) const
        = default;
};

// Resolves one local collision-mask cell. Only a solid result confirms the
// target geometry; a bounds candidate with an empty or outside-mask cell does
// not enter contact dispatch.
[[nodiscard]] CollisionMaskEvaluation evaluate_collision_mask(
    const CollisionMask& mask,
    CollisionMaskCell cell) noexcept;

} // namespace devils_crush::native_runtime
