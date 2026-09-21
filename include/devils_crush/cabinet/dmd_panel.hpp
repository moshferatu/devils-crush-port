// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "devils_crush/cabinet/cabinet_state.hpp"
#include "devils_crush/cabinet/dmd_font.hpp"
#include "devils_crush/core/high_score_table.hpp"
#include "devils_crush/render/pce_background.hpp"

namespace devils_crush::cabinet {

enum class DmdScene { score, special_points, extra_ball, bonus_tally, match, bonus_result };

// Observes cartridge state only; award timing and score changes remain owned
// by the original rules. Result success is latched before its counter drains.
class DmdPanel {
public:
    // Read-only notification at the cartridge's bonus-result entry 04:456C.
    // Session bit 1 also occurs during stage entry, so it is not a result gate.
    void begin_bonus_result() noexcept;
    void observe(std::span<const std::byte> work_ram);
    [[nodiscard]] DmdScene scene() const noexcept { return scene_; }
    [[nodiscard]] bool holds_playfield() const noexcept;
    [[nodiscard]] bool attract() const noexcept;
    [[nodiscard]] render::ArgbImage compose(
        const DmdFont& font, const core::HighScoreTable* high_scores,
        std::uint64_t elapsed_milliseconds) const;

private:
    CabinetState state_{};
    DmdScene scene_ = DmdScene::score;
    // The attract loop clears RAM briefly between Best Five and title.
    // Only a live session may leave attract presentation, not that clear.
    bool attract_ = true;
    bool demo_ = false;
    bool result_won_ = false;
    bool result_active_ = false;
    bool result_counting_ = false;
    bool perfect_ = false;
    bool special_result_ = false;
    std::uint64_t award_ = 0;
    unsigned targets_ = 0;
    unsigned multiplier_ = 1;
    unsigned last_ball_ = 1;
};

} // namespace devils_crush::cabinet
