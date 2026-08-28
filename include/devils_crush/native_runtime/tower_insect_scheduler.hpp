// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/contact_dispatch.hpp"
#include "devils_crush/native_runtime/tower_insect.hpp"

#include <array>
#include <optional>

namespace devils_crush::native_runtime {

class TowerInsectRuleScheduler {
public:
    explicit TowerInsectRuleScheduler(ScoreState& score) noexcept;

    [[nodiscard]] std::optional<TowerInsectSpawnStep> spawn_child(
        std::uint8_t slot) noexcept;
    [[nodiscard]] std::optional<TowerInsectSourceStep> begin_generation(
        bool tower_insect_cycle_requested) noexcept;
    // Seeds the source and all observed children together, so live-child
    // accounting cannot temporarily diverge during session capture.
    [[nodiscard]] bool seed(TowerInsectGenerationState generation) noexcept;
    [[nodiscard]] bool queue_confirmed_contact(const ContactDispatch& contact) noexcept;
    [[nodiscard]] std::optional<TowerInsectRulesUpdate> advance_actor(
        std::uint8_t slot) noexcept;
    [[nodiscard]] std::optional<TowerInsectSourceStep> advance_source(
        std::uint8_t slot) noexcept;
    [[nodiscard]] std::optional<TowerInsectSourceStep> restart_generation(
        bool tower_insect_cycle_requested) noexcept;
    [[nodiscard]] const TowerInsectSourceState& source() const noexcept;
    [[nodiscard]] const TowerInsectState* insect(std::uint8_t slot) const noexcept;

private:
    [[nodiscard]] static std::optional<std::size_t> index_for(
        std::uint8_t slot) noexcept;

    ScoreState& score_;
    TowerInsectSourceState source_{};
    std::array<std::optional<TowerInsectState>, 4U> insects_{};
    std::array<bool, 4U> contact_queued_{};
};

} // namespace devils_crush::native_runtime
