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
replace_walled_star_post_destruction_idle(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_tower_hit_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_tower_bonus_terminal(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_tower_bonus_continuation(
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
replace_knight_terminal_score_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_acknowledgement_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_setup_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_table_setup_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_call_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_pointer_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_shared_call_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_mode_branch_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_pointer_low_load_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_pointer_low_store_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_pointer_high_load_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_pointer_high_store_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_scratch_clear_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_mode_load_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_mode_signed_branch_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_carry_clear_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_base_load_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_mode_add_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_store_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_high_load_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_high_add_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_high_store_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_high_scratch_clear_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_high_mode_load_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_high_mode_signed_branch_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_high_carry_clear_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_high_base_load_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_high_scratch_add_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_position_high_actor_store_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_field_load_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_field_scratch_add_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_field_store_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_field_clear_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_field_zero_store_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_field_second_zero_store_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_attribute_load_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_attribute_mask_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_attribute_mode_merge_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_attribute_store_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_field_constant_load_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_field_constant_store_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_field_second_constant_load_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_post_return_actor_field_second_constant_store_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_shared_jump_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_shared_scan_initializer_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_shared_scan_read_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_shared_scan_nonzero_branch_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_shared_scan_increment_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_shared_scan_range_compare_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_shared_scan_loop_branch_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_allocator_load_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_allocator_store_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_allocator_mode_load_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_allocator_mode_branch_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_helper_allocator_return_carry_clear_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_scheduler(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_terminal_release_prefix(
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
replace_knight_movement_parent(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

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
replace_giant_door_hit_continuation(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_giant_door_completion_score_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_giant_door_completion_scheduler_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_giant_door_completion_request_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_giant_door_completion_wait_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_giant_door_completion_channel_advance(
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
replace_zero_ball_countdown_tick(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_zero_ball_expiry_scheduler(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_zero_ball_presentation_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_zero_ball_state_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_remaining_ball_alternate_scheduler(
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
replace_actor_scan_run(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_actor_scan_advance(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_sat_tail_clear(
    const emulation::RoutineEntry& entry);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_sat_submission_step(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// The timer-interrupt sound driver, entered from the interrupt vector at
// 00:FC00 and walked to whichever of its two RTI instructions or two chain
// exits control reaches.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_sound_timer_driver(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

// The bank-45 music sequencer's sample/envelope group. One walk shared by
// seven subroutine entry points, each stopping in front of its own RTS; a
// nested call the walk makes is inlined into the same transcript.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_music_sequencer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

// The palette shadow buffer's three leaf maintainers. One walk shared by
// three entry points, each stopping in front of its own RTS. None of them
// calls anything, so nothing is inlined.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_palette_shadow(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

// The bank-switching call wrappers. One walk shared by nine entry points:
// four heads and the addresses each one's call comes back to. Every transcript
// stops in front of an instruction the core executes itself -- an RTS, one of
// the two JSRs, or one of the two indirect jumps.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_bank_call_wrappers(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

// The VDC interrupt handler's body. One walk shared by six entry points: the
// two arms 00:E058 dispatches to, the two addresses the VBlank arm's calls come
// back to, and the secondary handler's prologue and epilogue. Every transcript
// stops in front of a call or an RTI the core executes itself. The dispatcher
// at 00:E058 is not here: it decides on a byte read from the VDC status
// register, which no replacement can see.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_vblank_handler(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_actor_dispatch_vector(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_actor_queue_registration(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_actor_dispatch_call(
    const emulation::RoutineEntry& entry);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_actor_queue_call(
    const emulation::RoutineEntry& entry);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_actor_queue_return(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_actor_camera_sweep(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_collision_scan_candidate_branch(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_collision_scan_advance(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_collision_scan_run(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_sprite_emit_run(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

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
