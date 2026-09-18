// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace devils_crush::cabinet {

inline constexpr int kTableSourceWidth = 256;
inline constexpr int kTableSourceHeight = 768;
inline constexpr int kTableWorldTop = 20;
inline constexpr int kTableWorldWidth = 256;
inline constexpr int kTableWorldHeight = 668;
inline constexpr std::uint32_t kTableAspectNumerator = 80U;
inline constexpr std::uint32_t kTableAspectDenominator = 167U;
inline constexpr std::size_t kActorSlotCount = 0x7FU;
inline constexpr std::size_t kBallActorSlot = 0x00U;
inline constexpr std::size_t kFirstFlipperControlSlot = 0x04U;
inline constexpr std::size_t kLastFlipperControlSlot = 0x09U;
inline constexpr std::size_t kSorcererControllerSlot = 0x16U;
inline constexpr std::size_t kLastSorcererSlot = 0x1EU;
inline constexpr std::size_t kBigStarSlot = 0x4BU;
inline constexpr std::size_t kFirstGrayBlobSlot = 0x50U;
inline constexpr std::size_t kLastGrayBlobSlot = 0x56U;
inline constexpr std::size_t kFirstSpiderSlot = 0x26U;
inline constexpr std::size_t kLastSpiderSlot = 0x29U;
inline constexpr std::size_t kFirstThreeBlobSlot = 0x58U;
inline constexpr std::size_t kLastThreeBlobSlot = 0x5AU;
inline constexpr std::size_t kFirstFixedActorSlot = 0x0AU;
inline constexpr std::size_t kLastFixedActorSlot = 0x61U;
inline constexpr std::size_t kFirstDynamicActorSlot = 0x62U;
inline constexpr std::size_t kLastDynamicActorSlot = 0x7EU;

enum class PlayfieldView {
    emulated_framebuffer,
    table_background,
};

struct CabinetState {
    PlayfieldView playfield = PlayfieldView::emulated_framebuffer;
    std::uint32_t stored_score = 0;
    std::uint64_t displayed_score = 0;
    std::uint32_t bonus_points = 0;
    std::uint8_t score_slot = 0;
    std::uint8_t balls_in_session = 0;
    std::uint8_t current_ball = 0;
    bool bonus_stage = false;
    // `$2054`, the cartridge's own screen selector: 0 is the table, 3 the
    // title and attract screens, 4 the High Scores screen. Only 0 may be
    // reconstructed; everything else has to come from the emulator, because
    // the other screens repurpose the pattern VRAM the reconstruction reads.
    std::uint8_t screen_selector = 0;

    // The request precedes the bonus screen by the outgoing flash/fade and
    // scene load. Keep the last stitched image until the screen changes;
    // neither the outgoing viewport nor repurposed VRAM belongs on it.
    [[nodiscard]] bool bonus_entry_pending() const noexcept
    {
        return bonus_stage && screen_selector == 0U;
    }
};

struct TableActorState {
    std::uint8_t slot = 0;
    std::uint8_t type = 0;
    std::uint8_t flags = 0;
    std::uint8_t appearance = 0;
    std::int16_t world_x = 0;
    std::int16_t world_y = 0;
    std::uint16_t dispatch = 0;

    [[nodiscard]] bool occupied() const noexcept
    {
        return type != 0U;
    }

    [[nodiscard]] bool camera_active() const noexcept
    {
        return (flags & 0x80U) != 0U;
    }

    [[nodiscard]] bool fixed_mechanism() const noexcept
    {
        return slot >= kFirstFixedActorSlot
            && slot <= kLastFixedActorSlot;
    }
};

[[nodiscard]] std::optional<CabinetState> decode_cabinet_state(
    std::span<const std::byte> work_ram) noexcept;

// Selects the fixed actors whose original camera cull cabinet mode may
// disable. The Big Star, Sorcerers, and upper-chute Gray Blobs run even
// before the first camera visit.
// The middle-table Three Blobs are admitted only after the camera has activated
// them once. Confirm the actor is inside the original camera interval because
// initialization can set the active flag before the first camera sweep.
[[nodiscard]] bool should_disable_actor_camera_cull(
    std::size_t slot,
    std::uint8_t flags,
    std::uint16_t actor_y,
    std::uint16_t camera_y) noexcept;

// Decodes the mapped parallel actor arrays at logical $3000-$3FFF. This is
// observation only: the original assembly remains the sole owner of actor
// scheduling and state transitions.
[[nodiscard]] std::optional<std::array<TableActorState, kActorSlotCount>>
decode_table_actor_state(std::span<const std::byte> work_ram) noexcept;

} // namespace devils_crush::cabinet
