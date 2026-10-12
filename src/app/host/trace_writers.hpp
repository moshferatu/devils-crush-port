// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"
#include "command_line.hpp"

namespace devils_crush::app::host {

[[nodiscard]] std::string sha256(
    const std::span<const std::byte> bytes);

template <typename Value>
[[nodiscard]] std::string sha256_values(
    const std::span<const Value> values)
{
    return sha256(std::as_bytes(values));
}

bool write_pcm16_wave_header(
    std::ostream& output,
    const std::uint32_t sample_rate,
    const std::uint32_t data_bytes);

[[nodiscard]] bool write_routine_trace(
    const std::filesystem::path& path,
    const std::span<const RoutineKey> routines,
    const std::span<const RoutineEntry> entries,
    const std::string_view core_revision);

[[nodiscard]] bool write_playfield_collision_sources(
    const std::filesystem::path& path,
    const std::span<const devils_crush::emulation::PlayfieldCollisionSource>
        sources,
    const std::string_view core_revision);

[[nodiscard]] bool write_routine_call_trace(
    const std::filesystem::path& path,
    const std::span<const RoutineKey> routines,
    const std::span<const RoutineCall> calls,
    const std::string_view core_revision);

[[nodiscard]] bool write_work_ram_write_trace(
    const std::filesystem::path& path,
    const std::span<const LogicalAddressRange> ranges,
    const std::span<const ObservedMemoryWrite> writes,
    const std::string_view core_revision);

[[nodiscard]] bool write_bus_write_trace(
    const std::filesystem::path& path,
    const std::span<const LogicalAddressRange> ranges,
    const std::span<const ObservedBusWrite> writes,
    const std::string_view core_revision);

} // namespace devils_crush::app::host
