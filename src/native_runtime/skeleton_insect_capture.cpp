// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/skeleton_insect_capture.hpp"

namespace devils_crush::native_runtime {
namespace {

struct SkeletonInsectSteadySpec {
    std::uint8_t activity_flags = 0;
    std::uint16_t handler_address = 0;
};

[[nodiscard]] std::optional<SkeletonInsectSteadySpec> steady_spec_for(
    const SkeletonInsectKind kind) noexcept
{
    switch (kind) {
    case SkeletonInsectKind::left:
        return {{.activity_flags = 0x82U, .handler_address = 0x54B9U}};
    case SkeletonInsectKind::center:
        return {{.activity_flags = 0x81U, .handler_address = 0x55F9U}};
    case SkeletonInsectKind::right:
        return {{.activity_flags = 0x82U, .handler_address = 0x5551U}};
    }
    return std::nullopt;
}

} // namespace

std::optional<SkeletonInsectCapture> capture_skeleton_insect_actor(
    const GameStateView& state,
    const std::uint8_t slot) noexcept
{
    const auto kind = skeleton_insect_kind_for_slot(slot);
    const auto actor = state.actor(slot);
    if (!kind || !actor) {
        return std::nullopt;
    }
    const auto expected = steady_spec_for(*kind);
    if (!expected || actor->activity_flags != expected->activity_flags
        || actor->handler_address != expected->handler_address) {
        return std::nullopt;
    }

    return SkeletonInsectCapture{
        .actor = *actor,
        .rule_state = {.kind = *kind},
    };
}

} // namespace devils_crush::native_runtime
