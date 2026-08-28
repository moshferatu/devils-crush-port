// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/collision_table_asset.hpp"

namespace devils_crush::native_runtime {
namespace {

[[nodiscard]] std::optional<std::uint16_t> read_word(
    const MappedRomView& rom,
    const std::uint16_t address) noexcept
{
    const auto low = rom.byte_at(address);
    const auto high = rom.byte_at(static_cast<std::uint16_t>(address + 1U));
    if (!low || !high) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(*low)
        | (static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(*high))
           << 8U));
}

[[nodiscard]] std::optional<std::uint8_t> read_byte(
    const MappedRomView& rom,
    const std::uint16_t address) noexcept
{
    const auto byte = rom.byte_at(address);
    return byte ? std::optional<std::uint8_t>(std::to_integer<std::uint8_t>(*byte))
                : std::nullopt;
}

} // namespace

CollisionTableRecordResolution resolve_collision_table_record(
    const MappedRomView& rom,
    const std::uint8_t selector,
    CollisionTableCursor cursor) noexcept
{
    constexpr std::uint8_t kRestartMarker = 0xFEU;
    constexpr std::uint8_t kEndMarker = 0xFFU;
    constexpr std::uint8_t kNextEntryMarker = 0xC0U;
    constexpr std::uint8_t kMaximumControlTransitions = 16U;

    CollisionTableRecordResolution resolution{.cursor = cursor};
    const auto selector_address = static_cast<std::uint16_t>(
        kCollisionTableSelectorDirectory
        + (static_cast<std::uint16_t>(selector) << 1U));
    const auto table_address = read_word(rom, selector_address);
    if (!table_address) {
        return resolution;
    }

    for (std::uint8_t attempt = 0U;
         attempt < kMaximumControlTransitions;
         ++attempt) {
        auto table_entry = read_byte(
            rom,
            static_cast<std::uint16_t>(*table_address + cursor.entry));
        if (!table_entry) {
            return resolution;
        }
        if (*table_entry == kEndMarker) {
            resolution.disposition = CollisionTableRecordDisposition::end_of_table;
            resolution.cursor = cursor;
            return resolution;
        }
        if (*table_entry == kRestartMarker) {
            cursor = {};
            resolution.restarted = true;
            table_entry = read_byte(rom, *table_address);
            if (!table_entry || *table_entry >= kRestartMarker) {
                return resolution;
            }
        }

        const auto motion_directory_address = static_cast<std::uint16_t>(
            kCollisionTableMotionDirectory
            + (static_cast<std::uint16_t>(*table_entry & 0x7FU) << 1U));
        const auto motion_address = read_word(rom, motion_directory_address);
        if (!motion_address) {
            return resolution;
        }
        const auto header = read_byte(
            rom,
            static_cast<std::uint16_t>(*motion_address + cursor.motion));
        if (!header) {
            return resolution;
        }
        if (*header == kNextEntryMarker) {
            cursor.entry = static_cast<std::uint8_t>(cursor.entry + 1U);
            cursor.motion = 0U;
            continue;
        }

        resolution.disposition = CollisionTableRecordDisposition::record_ready;
        resolution.cursor = cursor;
        resolution.record.table_entry = *table_entry;
        resolution.record.header_or_delta = *header;
        if ((*header & 0x80U) != 0U) {
            const auto delta = read_byte(
                rom,
                static_cast<std::uint16_t>(*motion_address + cursor.motion + 1U));
            if (!delta) {
                return CollisionTableRecordResolution{.cursor = cursor};
            }
            resolution.record.delta_after_header = *delta;
        }
        return resolution;
    }
    return CollisionTableRecordResolution{.cursor = cursor};
}

} // namespace devils_crush::native_runtime
