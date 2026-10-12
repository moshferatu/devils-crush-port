// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"

namespace devils_crush::app::host {

// Keeps the high-score table across runs without the player saving it.
//
// The cartridge only ever commits the table to backup RAM as part of the
// pause -> High Scores -> save-slot sequence, and only into one of five
// numbered slots, because a HuCard's two kilobytes of battery-backed RAM were
// worth spending deliberately. `08:5C4E` shows what a slot holds: `TII
// $2D78,$2200,$002A` puts all 42 bytes of the block back, so a Data Load
// restores whichever table was current when that slot was written, and a
// player who never saves keeps the ROM defaults forever. None of that budget
// exists on a PC, so the ranked columns travel with the machine instead.
//
// The file is the host's, not the cartridge's: it sits beside the backup RAM
// in the working directory and leaves all five original save slots for the
// games they were meant for.
struct HighScoreAutosave {
    std::filesystem::path path;
    // What the file held when the host started, or nothing if there was no
    // readable file. Read once, because the hook that consumes it runs from
    // inside a frame.
    std::optional<devils_crush::core::HighScoreTable> stored;
    devils_crush::core::HighScoreTable last{};
    // `01:C107` has run, so the block holds a table rather than the zeroes the
    // work-RAM clear before it leaves behind.
    bool table_seen = false;
    bool pending = false;
    bool reported_failure = false;
    int settled_samples = 0;
};

[[nodiscard]] HighScoreAutosave open_high_score_autosave(
    const std::filesystem::path& path);

[[nodiscard]] bool install_high_score_restore(
    devils_crush::emulation::PceRuntime& runtime,
    HighScoreAutosave& autosave);

void poll_high_scores(
    devils_crush::emulation::PceRuntime& runtime,
    HighScoreAutosave& autosave,
    const int frame_number);

void flush_high_scores(HighScoreAutosave& autosave);

} // namespace devils_crush::app::host
