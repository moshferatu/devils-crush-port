// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "devils_crush/emulation/pce_runtime.hpp"

namespace devils_crush::cabinet {
// A perfect bonus result sets the table's star pending. During the return to
// the table the star plays a jingle (command 8) over the black fade, waits
// 120 frames, then resumes music. Table entry has already started the table
// theme about 26 frames into the jingle, cutting it off.
//
// The star's second resume is always skipped, since the theme is already
// playing. With `skip_perfect_jingle` the jingle is skipped too.
class MusicTransition {
public:
    MusicTransition() noexcept = default;
    explicit MusicTransition(bool skip_perfect_jingle) noexcept
        : skip_perfect_jingle_(skip_perfect_jingle) {}
    void set_skip_perfect_jingle(bool skip) noexcept { skip_perfect_jingle_ = skip; }
    // The bonus stage's perfect result set the star pending (04:46A2).
    void perfect_result() noexcept { perfect_pending_ = true; }
    // Table return finished (01:C204).
    void table_return(std::span<const std::byte> ram) noexcept;
    // The star's jingle (03:7CC1 or 05:6984).
    std::optional<emulation::RoutineReplacement> star_jingle(
        const emulation::RoutineEntry& entry, std::span<const std::byte> ram);
    // The star's resume after its wait (03:7CF6).
    std::optional<emulation::RoutineReplacement> finish_star(
        const emulation::RoutineEntry& entry, std::span<const std::byte> ram);
    // Hooks every address named above. The transition must outlive the runtime.
    [[nodiscard]] bool install(emulation::PceRuntime& runtime);
private:
    bool skip_perfect_jingle_ = false;
    bool perfect_pending_ = false;
    bool table_resumed_ = false;
};
}
