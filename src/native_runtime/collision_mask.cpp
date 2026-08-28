// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/collision_mask.hpp"

namespace devils_crush::native_runtime {

CollisionMaskEvaluation evaluate_collision_mask(
    const CollisionMask& mask,
    const CollisionMaskCell cell) noexcept
{
    CollisionMaskEvaluation evaluation{.cell = cell};
    if (cell.column >= CollisionMask::kWidth
        || cell.row >= CollisionMask::kHeight) {
        return evaluation;
    }

    evaluation.selected_row = mask.rows[cell.row];
    evaluation.column_bit = static_cast<std::uint8_t>(0x80U >> cell.column);
    evaluation.disposition = (evaluation.selected_row & evaluation.column_bit)
            != 0U
        ? CollisionMaskDisposition::solid
        : CollisionMaskDisposition::empty;
    return evaluation;
}

} // namespace devils_crush::native_runtime
