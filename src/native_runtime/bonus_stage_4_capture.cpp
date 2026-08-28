// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/bonus_stage_4_capture.hpp"

namespace devils_crush::native_runtime {
namespace {

constexpr std::uint8_t kBonusStage4ObjectiveType = 0x82U;
constexpr std::uint16_t kSteadyObjectiveHandler = 0x6A46U;

} // namespace

std::optional<BonusStage4Capture> capture_bonus_stage_4(
    const GameStateView& state) noexcept
{
    BonusStage4Capture capture{};
    for (std::size_t index = 0U; index < kBonusStage4ObjectiveSlots.size(); ++index) {
        const auto objective = state.bonus_stage_4_objective(
            static_cast<std::uint8_t>(index));
        const auto actor = state.actor(kBonusStage4ObjectiveSlots[index]);
        if (!objective || !actor || !objective->active
            || objective->hits_remaining == 0U || objective->hits_remaining > 8U
            || actor->activity_flags != kBonusStage4ObjectiveType
            || actor->handler_address != kSteadyObjectiveHandler
            || (actor->collision_state & 0x80U) != 0U) {
            return std::nullopt;
        }
        capture.objectives[index] = *actor;
        capture.state.objectives[index] = {
            .hits_remaining = objective->hits_remaining,
            .active = true,
        };
    }
    return capture;
}

} // namespace devils_crush::native_runtime
