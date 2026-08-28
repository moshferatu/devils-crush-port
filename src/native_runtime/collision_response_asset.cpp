// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/collision_response_asset.hpp"

#include "devils_crush/native_runtime/mapped_rom.hpp"

#include <bit>

namespace devils_crush::native_runtime {
namespace {

constexpr std::uint16_t kResponseGeometryTable = 0xD2AEU;
constexpr std::uint16_t kSecondCoefficientOffset = 0x0040U;

[[nodiscard]] std::optional<std::int16_t> read_fixed_8_8(
    const MappedRomView& rom,
    const std::uint16_t address) noexcept
{
    const auto low = rom.byte_at(address);
    const auto high = rom.byte_at(static_cast<std::uint16_t>(address + 1U));
    if (!low || !high) {
        return std::nullopt;
    }
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(*low)
        | (static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(*high))
            << 8U)));
}

} // namespace

std::optional<CollisionResponseBasisResolution>
decode_collision_response_basis(
    const MappedRomView& rom,
    const std::uint8_t contact_orientation) noexcept
{
    const auto table_index = static_cast<std::uint8_t>(
        static_cast<std::uint8_t>(contact_orientation & 0x1FU) << 1U);
    const auto cosine = read_fixed_8_8(
        rom,
        static_cast<std::uint16_t>(kResponseGeometryTable + table_index));
    const auto sine = read_fixed_8_8(
        rom,
        static_cast<std::uint16_t>(
            kResponseGeometryTable + kSecondCoefficientOffset + table_index));
    if (!cosine || !sine) {
        return std::nullopt;
    }
    return CollisionResponseBasisResolution{
        .contact_orientation = contact_orientation,
        .table_index = table_index,
        .basis = {
            .cosine = *cosine,
            .sine = *sine,
        },
    };
}

CollisionResponseConfig make_collision_response_config(
    const std::uint8_t contact_orientation,
    const GameStateView::CollisionResponseTableState& table_state) noexcept
{
    const auto mode = static_cast<CollisionResponseMode>(
        (contact_orientation & 0x60U) >> 5U);
    return {
        .requested_mode = mode,
        .force_reduced_mode = table_state.force_reduced_mode,
        .use_quarter_reduced_scale = table_state.use_quarter_reduced_scale,
        .normal_tuning = table_state.normal_tuning,
    };
}

} // namespace devils_crush::native_runtime
