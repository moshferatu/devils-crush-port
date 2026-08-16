// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/cabinet/cabinet_state.hpp"

#include <algorithm>

namespace devils_crush::cabinet {
namespace {

constexpr std::size_t kWorkRamSize = 8192U;
constexpr std::size_t kSessionFlagsOffset = 0x0053U;
constexpr std::size_t kScoreSlotOffset = 0x0251U;
// `$2054` says which screen the cartridge is drawing. Over an untouched
// attract loop it reads 3 on the title and attract screens, 0 while the table
// is on screen, and 4 on the High Scores screen; it is the difference between
// a session existing and a session being what the player is looking at.
constexpr std::size_t kScreenSelectorOffset = 0x0054U;
constexpr std::uint8_t kTableScreen = 0x00U;
constexpr std::size_t kTableAttributeFirst = 0x0259U;
constexpr std::size_t kTableAttributeEnd = 0x0859U;
constexpr std::size_t kBallsInSessionOffset = 0x0D52U;
constexpr std::uint8_t kStartingBallCount = 3U;
// `$2D50` stores the base BP value in hundreds: play initializes it to one,
// awards raise it one step at a time, and the original rules cap it at 99.
constexpr std::size_t kBonusPointsHundredsOffset = 0x0D50U;
constexpr std::size_t kScoreLowBase = 0x0205U;
constexpr std::size_t kScoreMiddleBase = 0x020CU;
constexpr std::size_t kScoreHighBase = 0x0213U;
constexpr std::uint8_t kBonusStageSessionBit = 0x10U;
constexpr std::size_t kActorTypeBase = 0x1000U;
constexpr std::size_t kActorFlagsBase = 0x1080U;
constexpr std::size_t kActorXLowBase = 0x1180U;
constexpr std::size_t kActorXHighBase = 0x1200U;
constexpr std::size_t kActorYLowBase = 0x1300U;
constexpr std::size_t kActorYHighBase = 0x1380U;
constexpr std::size_t kActorAppearanceBase = 0x1900U;
constexpr std::size_t kActorDispatchLowBase = 0x1F00U;
constexpr std::size_t kActorDispatchHighBase = 0x1F80U;

[[nodiscard]] constexpr std::uint8_t read_byte(
    const std::span<const std::byte> bytes,
    const std::size_t offset) noexcept
{
    return std::to_integer<std::uint8_t>(bytes[offset]);
}

} // namespace

std::optional<CabinetState> decode_cabinet_state(
    const std::span<const std::byte> work_ram) noexcept
{
    if (work_ram.size() != kWorkRamSize) {
        return std::nullopt;
    }

    const auto score_slot = read_byte(work_ram, kScoreSlotOffset);
    if (score_slot >= 7U) {
        return std::nullopt;
    }
    const auto stored_score =
        static_cast<std::uint32_t>(
            read_byte(work_ram, kScoreLowBase + score_slot))
        | (static_cast<std::uint32_t>(
               read_byte(work_ram, kScoreMiddleBase + score_slot))
           << 8U)
        | (static_cast<std::uint32_t>(
               read_byte(work_ram, kScoreHighBase + score_slot))
           << 16U);
    const auto session_flags = read_byte(work_ram, kSessionFlagsOffset);
    const auto screen_selector =
        read_byte(work_ram, kScreenSelectorOffset);
    const auto balls_in_session =
        read_byte(work_ram, kBallsInSessionOffset);
    const auto bonus_points = static_cast<std::uint32_t>(
        read_byte(work_ram, kBonusPointsHundredsOffset)) * 100U;
    const bool bonus_stage =
        (session_flags & kBonusStageSessionBit) != 0U;
    const bool table_attributes_ready = std::any_of(
        work_ram.begin()
            + static_cast<std::ptrdiff_t>(kTableAttributeFirst),
        work_ram.begin()
            + static_cast<std::ptrdiff_t>(kTableAttributeEnd),
        [](const std::byte value) {
            return value != std::byte{0};
        });

    // A live session is not the same thing as a table on screen. The screens
    // the cartridge can raise over a running session -- High Scores above
    // all -- leave the ball count and the table attributes exactly as they
    // were, and rewrite pattern VRAM for their own glyphs. Reconstructing the
    // table from the ROM tilemap and that VRAM then draws the table's shape
    // filled with the menu's letters. `$2054` is what separates the two.
    return CabinetState{
        .playfield =
            table_attributes_ready && balls_in_session != 0U
                && !bonus_stage && screen_selector == kTableScreen
            ? PlayfieldView::table_background
            : PlayfieldView::emulated_framebuffer,
        .stored_score = stored_score,
        .displayed_score =
            static_cast<std::uint64_t>(stored_score) * 100U,
        .bonus_points = bonus_points,
        .score_slot = score_slot,
        .balls_in_session = balls_in_session,
        // `$2D52` counts down from three, including the ball currently in
        // play. Inverting that countdown yields the standard session's
        // current ball number. Saturation keeps a possible extra-ball count
        // from wrapping below ball one.
        .current_ball = balls_in_session == 0U
            ? 0U
            : static_cast<std::uint8_t>(
                  kStartingBallCount
                  - std::min(balls_in_session, kStartingBallCount)
                  + 1U),
        .bonus_stage = bonus_stage,
        .screen_selector = screen_selector,
    };
}

std::optional<std::array<TableActorState, kActorSlotCount>>
decode_table_actor_state(
    const std::span<const std::byte> work_ram) noexcept
{
    if (work_ram.size() != kWorkRamSize) {
        return std::nullopt;
    }

    std::array<TableActorState, kActorSlotCount> actors{};
    for (std::size_t slot = 0; slot < actors.size(); ++slot) {
        const auto read_i16 =
            [&work_ram, slot](
                const std::size_t low_base,
                const std::size_t high_base) {
                return static_cast<std::int16_t>(
                    static_cast<std::uint16_t>(
                        read_byte(work_ram, low_base + slot))
                    | (static_cast<std::uint16_t>(
                           read_byte(work_ram, high_base + slot))
                       << 8U));
            };
        actors[slot] = {
            .slot = static_cast<std::uint8_t>(slot),
            .type = read_byte(work_ram, kActorTypeBase + slot),
            .flags = read_byte(work_ram, kActorFlagsBase + slot),
            .appearance =
                read_byte(work_ram, kActorAppearanceBase + slot),
            .world_x = read_i16(kActorXLowBase, kActorXHighBase),
            .world_y = read_i16(kActorYLowBase, kActorYHighBase),
            .dispatch = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(
                    read_byte(
                        work_ram,
                        kActorDispatchLowBase + slot))
                | (static_cast<std::uint16_t>(
                       read_byte(
                           work_ram,
                           kActorDispatchHighBase + slot))
                   << 8U)),
        };
    }
    return actors;
}

} // namespace devils_crush::cabinet
