// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/giant_face_helmet_capture.hpp"

namespace devils_crush::native_runtime {
namespace {

constexpr std::uint8_t kHelmetType = 0x84U;
constexpr std::uint16_t kHelmetIdleHandler = 0x4C91U;

[[nodiscard]] bool is_idle_channel(
    const GameStateView::ActorSlot& actor,
    const GameStateView::GiantFaceHelmetChannelSlot& channel,
    const std::uint8_t slot,
    const std::uint8_t selector) noexcept
{
    return actor.index == slot && actor.activity_flags == kHelmetType
        && actor.handler_address == kHelmetIdleHandler && actor.collision_state == 0U
        && channel.index == slot && channel.selector == selector
        && channel.ticks_until_appearance >= 1U
        && channel.ticks_until_appearance <= 4U && channel.appearance <= 3U;
}

} // namespace

std::optional<GiantFaceHelmetCapture> capture_giant_face_helmet(
    const GameStateView& state) noexcept
{
    const auto left_actor = state.actor(kGiantFaceLeftHelmetSlot);
    const auto right_actor = state.actor(kGiantFaceRightHelmetSlot);
    const auto left_channel = state.giant_face_helmet_channel(kGiantFaceLeftHelmetSlot);
    const auto right_channel = state.giant_face_helmet_channel(kGiantFaceRightHelmetSlot);
    if (!left_actor || !right_actor || !left_channel || !right_channel
        || !is_idle_channel(*left_actor, *left_channel, kGiantFaceLeftHelmetSlot, 0x13U)
        || !is_idle_channel(
            *right_actor, *right_channel, kGiantFaceRightHelmetSlot, 0x1DU)) {
        return std::nullopt;
    }

    return GiantFaceHelmetCapture{
        .actors = {*left_actor, *right_actor},
        .rule_state = {
            .channels = {{
                {
                    .side = GiantFaceHelmetSide::left,
                    .collision_ready = true,
                    .ticks_until_appearance = left_channel->ticks_until_appearance,
                    .appearance = left_channel->appearance,
                },
                {
                    .side = GiantFaceHelmetSide::right,
                    .collision_ready = true,
                    .ticks_until_appearance = right_channel->ticks_until_appearance,
                    .appearance = right_channel->appearance,
                },
            }},
        },
    };
}

} // namespace devils_crush::native_runtime
