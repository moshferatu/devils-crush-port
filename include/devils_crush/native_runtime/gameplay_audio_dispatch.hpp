// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/native_gameplay_state.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace devils_crush::native_runtime {

// The game-side command gate accepts commands synchronously before the audio
// driver's descriptor loader sees them. These are policy outcomes, not audio
// playback states: descriptor loading and PSG synthesis remain later owners.
enum class GameplayAudioDispatchDisposition : std::uint8_t {
    accepted,
    ignored_sentinel,
    suppressed_while_audio_locked,
};

struct GameplayAudioDispatch {
    GameplayAudioEvent event;
    GameplayAudioDispatchDisposition disposition =
        GameplayAudioDispatchDisposition::accepted;

    [[nodiscard]] bool forwards_to_audio_driver() const noexcept;
    [[nodiscard]] bool operator==(const GameplayAudioDispatch&) const = default;
};

// Represents the game-side lock at `$2CC7`. While it is set, the original
// dispatcher suppresses only commands `$31`, `$27`, and `$47`; all other
// commands continue synchronously to the audio driver's descriptor loader.
struct GameplayAudioDispatchPolicy {
    bool audio_locked = false;

    [[nodiscard]] bool operator==(const GameplayAudioDispatchPolicy&) const = default;
};

// Classifies one native gameplay submission using the recovered `00:ECFD`
// gate. The result retains the actor/source identity because a command ID
// alone is not a gameplay mechanism identity. Callers can immediately pass
// accepted results to a native descriptor loader; this routine owns no queue
// and has no PSG-device side effects.
[[nodiscard]] GameplayAudioDispatch dispatch_gameplay_audio_event(
    GameplayAudioEvent event,
    GameplayAudioDispatchPolicy policy = {}) noexcept;

// Applies the synchronous gate to one actor-order batch. Results keep the
// caller's order, including suppressed entries, so a native runtime can audit
// the complete gameplay-to-audio boundary without recreating work-RAM latches.
[[nodiscard]] std::vector<GameplayAudioDispatch> dispatch_gameplay_audio_events(
    std::span<const GameplayAudioEvent> events,
    GameplayAudioDispatchPolicy policy = {});

} // namespace devils_crush::native_runtime
