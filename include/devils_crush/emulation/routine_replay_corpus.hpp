// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/emulation/pce_runtime.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace devils_crush::emulation {

inline constexpr std::uint32_t kRoutineReplayCorpusFormatVersion = 1U;

// One normalized routine invocation persisted independently of the route that
// found it. The opaque machine state resumes at checkpoint_entry; expected_call
// and post_call_work_ram are the assembly oracle against which a direct replay
// is compared.
struct RoutineReplayCorpusEntry {
    std::uint32_t format_version = kRoutineReplayCorpusFormatVersion;
    std::string rom_sha256;
    std::string core_revision;
    std::uint64_t ordinal = 0U;
    RoutineEntry checkpoint_entry;
    SaveState checkpoint;
    std::vector<std::byte> pre_call_work_ram;
    RoutineCall expected_call;
    std::vector<std::byte> post_call_work_ram;

    [[nodiscard]] bool operator==(
        const RoutineReplayCorpusEntry&) const = default;
};

struct RoutineReplayCorpusDecodeResult {
    bool decoded = false;
    RoutineReplayCorpusEntry entry;
    std::string message;
};

// Stable little-endian representation with a checksum over all metadata and
// payload bytes. Invalid, inconsistent, or unbounded entries throw
// std::invalid_argument rather than producing a fixture that cannot replay.
[[nodiscard]] std::vector<std::byte> encode_routine_replay_corpus(
    const RoutineReplayCorpusEntry& entry);

// Rejects unknown versions, corrupt or truncated payloads, unreasonable
// lengths, inconsistent entry/call state, and trailing bytes.
[[nodiscard]] RoutineReplayCorpusDecodeResult decode_routine_replay_corpus(
    std::span<const std::byte> bytes);

} // namespace devils_crush::emulation
