// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "devils_crush/emulation/pce_runtime.hpp"

namespace devils_crush::cabinet {
// The perfect-result star resumes music after its 120-frame animation.
// Returning from the bonus room has already resumed the table track.
class MusicTransition {
public:
    void table_return(std::span<const std::byte> ram) noexcept;
    std::optional<emulation::RoutineReplacement> finish_star(
        const emulation::RoutineEntry& entry, std::span<const std::byte> ram);
private:
    bool table_resumed_ = false;
};
}
