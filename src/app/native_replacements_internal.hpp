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
replace_giant_skeleton_eyeball_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_mercy_star_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_three_blobs_controller(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_dragon_egg_controller(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_big_star_feature_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_walled_star_hit_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_tower_hit_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_tower_hit_continuation(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_tower_post_dispatch(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_dragon_head_hit_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_dragon_head_hit_continuation(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_bumper_hit_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_bumper_primary_progress(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_bumper_secondary_progress(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_devil_shooter_hit_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_devil_shooter_catch_continuation(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_skeleton_insect_left_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_skeleton_insect_right_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_skeleton_insect_center_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_skeleton_insect_center_continuation(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_devils_coffin_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_devils_coffin_opening(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_devils_coffin_closing(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_tower_guard_selector(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_fixed_scheduler_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_fixed_scheduler_submission(
    const emulation::RoutineEntry& entry);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_fixed_marker_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_fixed_marker_submission(
    const emulation::RoutineEntry& entry);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_fixed_marker_helper_segment(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_scaled_word_add(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_fixed_response_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_dragon_egg_hit_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_dragon_egg_hit_continuation(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_dragon_egg_post_allocation(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_giant_door_hit_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_giant_face_animator_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_giant_face_request_gate(
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
replace_unsigned_byte_multiply(
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
replace_collision_mask_lookup(
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
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

} // namespace devils_crush::app::detail
