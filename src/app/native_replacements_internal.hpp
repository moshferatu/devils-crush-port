// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/emulation/pce_runtime.hpp"

#include <cstddef>
#include <optional>
#include <span>

namespace devils_crush::app::detail {

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_rng_update(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_score_accumulator(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_position_integration(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_motion_rotation(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_flipper_radius_projection(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_flipper_angle_projection(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_collision_motion_adjustment(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_signed_division(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_motion_damping(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_unsigned_multiply_round(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_signed_fixed_multiply(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_signed_word_negation(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_flipper_projection(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_cached_position_restore(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_flipper_input_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_flipper_phase_step(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_flipper_geometry_lookup(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_coroutine_scheduler(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_nudge_input_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_session_input_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_actor_camera_activation(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_actor_slot_reset(
    const emulation::RoutineEntry& entry);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_collision_table_interpreter(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_collision_response(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_collision_consumer_no_hit(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_collision_geometry(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_collision_scan_vertical_reject(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

} // namespace devils_crush::app::detail
