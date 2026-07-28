// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/cabinet/cabinet_state.hpp"

#include <algorithm>

namespace devils_crush::cabinet {
namespace {

constexpr std::size_t kWorkRamSize = 8192U;
constexpr std::size_t kSessionFlagsOffset = 0x0053U;
constexpr std::size_t kScoreSlotOffset = 0x0251U;
constexpr std::size_t kTableAttributeFirst = 0x0259U;
constexpr std::size_t kTableAttributeEnd = 0x0859U;
constexpr std::size_t kBallsInSessionOffset = 0x0D52U;
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
    const auto balls_in_session =
        read_byte(work_ram, kBallsInSessionOffset);
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

    return CabinetState{
        .playfield =
            table_attributes_ready && balls_in_session != 0U
                && !bonus_stage
            ? PlayfieldView::table_background
            : PlayfieldView::emulated_framebuffer,
        .stored_score = stored_score,
        .displayed_score =
            static_cast<std::uint64_t>(stored_score) * 100U,
        .score_slot = score_slot,
        .balls_in_session = balls_in_session,
        .bonus_stage = bonus_stage,
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
