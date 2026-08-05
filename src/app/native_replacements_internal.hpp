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
replace_giant_skeleton_eyeball_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_giant_skeleton_eyeball_opening_wait(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_giant_skeleton_eyeball_steady_idle(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_giant_skeleton_eyeball_final_wait(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_gray_blob_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_gray_blob_first_live_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_mercy_star_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_devil_shooter_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_little_skull_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_little_skull_gray_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_little_skull_gray_release(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_little_skull_live_continuation(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_little_skull_pair_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_little_skull_hidden_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_big_star_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_dragon_egg_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_undead_head_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_undead_head_selector_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_undead_head_waiting_parent(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_side_worm_initializers(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_side_worm_selector_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_side_worm_waiting_parent(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_side_worm_active_timer_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_dragon_head_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_giant_skeleton_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_three_blobs_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_three_blobs_controller_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_walled_star_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_walled_star_access_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_side_star_initializers(
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
replace_three_blobs_first_movement_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_three_blobs_direct_continuation(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_sorcerer_live_controller_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_sorcerer_deployment_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_sorcerer_staging_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_three_blobs_appearance_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_three_blobs_collision_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_three_blobs_shared_motion_idle(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_dragon_egg_controller(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_dragon_egg_controller_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_big_star_feature_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_big_star_pending_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_big_star_pending_helper(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_big_star_pending_right_helper(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_tower_insect_source_countdown_wait(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_big_star_rotation_wait(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_devil_shooter_companion_dormant(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_devil_shooter_companion_position(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_devil_shooter_companion_appearance(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_devil_shooter_companion_active_timer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_devil_shooter_companion_active_controller(
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
replace_tower_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_bumper_initializers(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_bumper_child_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_small_soldier_source_initializers(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_small_soldier_delayed_spawn_gates(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_giant_face_controller_initializers(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_giant_face_animator_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_giant_face_animator_active_wait(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_giant_face_crown_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_giant_face_mouth_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_hit_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_initializers(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_side_spider_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_front_spider_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_waiting_parent(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_spider_appearance(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_tower_insect_animation(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_tower_insect_source_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_tower_insect_source_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_small_soldier_source_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_sorcerer_color_gate(
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
replace_devil_shooter_release_delay_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_devil_shooter_charge_position(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_devil_shooter_charge_retraction(
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
replace_devils_coffin_initializer(
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
replace_fixed_scheduler_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_fixed_marker_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_fixed_response_initializer(
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
replace_giant_door_initializer(
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
replace_giant_face_crown_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_giant_face_mouth_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_giant_face_helmet_controller(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_giant_face_helmet_hit_continuation(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_giant_face_helmet_followup_timer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_giant_face_helmet_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_giant_face_helmet_outer_initializers(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_four_star_wall_pocket_post_audio_score(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_four_star_wall_pocket_post_appearance(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_four_star_wall_pocket_animation(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_side_star_bonus_entrance_animation(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_upper_wall_pocket_hand_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_upper_wall_pocket_hand_live_setup(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_upper_wall_pocket_hand_countdown(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_upper_wall_pocket_hand_opening(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_upper_wall_pocket_hand_opening_followup(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_upper_wall_pocket_hand_opening_expiry(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_upper_wall_pocket_hand_post_opening_schedule(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_upper_wall_pocket_hand_post_opening_state(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_upper_wall_pocket_hand_post_opening_countdown(
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
replace_ball_drain_threshold(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_remaining_ball_presentation(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_remaining_ball_transition_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_remaining_ball_decision(
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
