// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "devils_crush/cabinet/cabinet_state.hpp"
#include "devils_crush/cabinet/dmd_font.hpp"
#include "devils_crush/core/high_score_table.hpp"
#include "devils_crush/render/ending_head.hpp"
#include "devils_crush/render/pce_background.hpp"

#include <cstddef>
#include <string>

namespace devils_crush::cabinet {

enum class DmdScene { score, special_points, extra_ball, bonus_tally, match, bonus_result };

// One turn of the attract carousel is the ending's head, when there is one to
// show, and then the five high-score pages. The head's 190 frames are the
// game's, at its sixty a second. The game cuts from the last of them to
// black; the panel instead takes a second over it, the face going out a fade
// step every other frame and the panel then staying dark.
inline constexpr std::uint64_t kDmdEndingHeadTailFrames = 60U;
inline constexpr std::uint64_t kDmdEndingHeadMilliseconds =
    (render::kEndingHeadFrames + kDmdEndingHeadTailFrames) * 1'000U / 60U;

// Observes cartridge state only; award timing and score changes remain owned
// by the original rules. Result success is latched before its counter drains.
class DmdPanel {
public:
    // Read-only notification at the cartridge's bonus-result entry 04:456C.
    // Session bit 1 also occurs during stage entry, so it is not a result gate.
    void begin_bonus_result() noexcept;
    // Read-only notifications from the cartridge's name entry: `09:6629`,
    // once a new score has ranked and the entry is being set up, and
    // `09:6698`, once it has ended, by a third letter or by timing out.
    //
    // They exist because the letters are not in the high-score table while
    // they are being chosen. The loop keeps them at `$2D66-$2D68`, with the
    // cursor at `$2D65`, and copies them to the table's name rows only at the
    // end (`09:6791`, or `09:6683` on the timeout), so the table still holds
    // the name the rank had before -- `AAA` from a fresh boot -- throughout.
    void begin_name_entry() noexcept;
    void end_name_entry() noexcept;
    void observe(std::span<const std::byte> work_ram);
    [[nodiscard]] DmdScene scene() const noexcept { return scene_; }
    [[nodiscard]] bool holds_playfield() const noexcept;
    [[nodiscard]] bool attract() const noexcept;
    // The ball number the score page shows.
    [[nodiscard]] unsigned ball() const noexcept { return last_ball_; }
    // While the name entry page is up: the three letters as it shows them.
    [[nodiscard]] bool name_entry() const noexcept { return name_entry_; }
    [[nodiscard]] const std::string& entry_name() const noexcept { return entry_name_; }
    // `elapsed_milliseconds` is how long the attract carousel has been
    // turning. It opens on `ending_head`, alone on the panel, when given one.
    [[nodiscard]] render::ArgbImage compose(
        const DmdFont& font, const core::HighScoreTable* high_scores,
        std::uint64_t elapsed_milliseconds,
        const render::EndingHeadAnimation* ending_head = nullptr) const;

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
    // Name entry, from its setup until the High Scores screen it runs on
    // gives way to the next one. The page stays up through the pause after
    // the last letter, which is when the table takes the name.
    bool name_entry_ = false;
    bool name_entry_ended_ = false;
    bool name_entry_screen_seen_ = false;
    std::size_t entry_rank_ = 0;
    std::string entry_name_;
    std::size_t entry_cursor_ = 0;
    bool entry_blink_ = false;
    std::uint64_t entry_score_ = 0;
    // Frames the live page is kept after the High Scores screen replaces a
    // session; three are measured before the entry begins.
    static constexpr unsigned kHighScoresHoldFrames = 10U;
    unsigned high_scores_hold_ = 0U;
    unsigned last_selector_ = 0U;
    std::uint64_t held_score_ = 0;
    std::uint64_t award_ = 0;
    unsigned targets_ = 0;
    unsigned multiplier_ = 1;
    static constexpr unsigned kStartingBalls = 3U;
    unsigned fewest_balls_left_ = kStartingBalls;
    unsigned last_ball_ = 1;
};

} // namespace devils_crush::cabinet
