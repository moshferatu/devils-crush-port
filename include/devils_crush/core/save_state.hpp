// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace devils_crush::state {

inline constexpr std::uint32_t kSaveStateFormatVersion = 1U;

struct SaveStateContainer {
    std::uint32_t format_version = kSaveStateFormatVersion;
    std::uint64_t frame_number = 0;
    std::string rom_sha256;
    std::string core_revision;
    std::vector<std::byte> core_state;
};

struct DecodeResult {
    bool decoded = false;
    SaveStateContainer container;
    std::string message;
};

// Produces the stable, little-endian v1 on-disk representation. Invalid
// metadata or an empty/oversized core payload throws std::invalid_argument.
[[nodiscard]] std::vector<std::byte> encode(
    const SaveStateContainer& container);

// Rejects unknown versions, incompatible sizes, invalid metadata, trailing
// bytes, and payload corruption before returning any emulator-owned state.
[[nodiscard]] DecodeResult decode(std::span<const std::byte> bytes);

} // namespace devils_crush::state
