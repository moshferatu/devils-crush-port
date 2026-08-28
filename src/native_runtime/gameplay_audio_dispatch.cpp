// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/gameplay_audio_dispatch.hpp"

namespace devils_crush::native_runtime {

bool GameplayAudioDispatch::forwards_to_audio_driver() const noexcept
{
    return disposition == GameplayAudioDispatchDisposition::accepted;
}

GameplayAudioDispatch dispatch_gameplay_audio_event(
    const GameplayAudioEvent event,
    const GameplayAudioDispatchPolicy policy) noexcept
{
    if (event.command == 0xFFU) {
        return {.event = event,
                .disposition = GameplayAudioDispatchDisposition::ignored_sentinel};
    }

    const bool is_locked_command = event.command == 0x31U
        || event.command == 0x27U || event.command == 0x47U;
    if (policy.audio_locked && is_locked_command) {
        return {
            .event = event,
            .disposition = GameplayAudioDispatchDisposition::suppressed_while_audio_locked,
        };
    }

    return {.event = event};
}

std::vector<GameplayAudioDispatch> dispatch_gameplay_audio_events(
    const std::span<const GameplayAudioEvent> events,
    const GameplayAudioDispatchPolicy policy)
{
    std::vector<GameplayAudioDispatch> dispatches;
    dispatches.reserve(events.size());
    for (const auto event : events) {
        dispatches.push_back(dispatch_gameplay_audio_event(event, policy));
    }
    return dispatches;
}

} // namespace devils_crush::native_runtime
