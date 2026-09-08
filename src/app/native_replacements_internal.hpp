// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/emulation/pce_runtime.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace devils_crush::app::detail {

std::optional<emulation::RoutineReplacement> replace_upper_scheduler_return(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// End a transcript past its own `RTS` rather than in front of it.
//
// `at_return` is the state on the return instruction's own boundary -- the
// program counter it sits at, and the stack pointer the call was entered on --
// and `return_master_cycle_offset` is where that boundary falls in the
// transcript. The transcript's resume, output registers and length are all
// rewritten from it, and the declaration the routine-call observer needs is
// pushed. Twenty-one master cycles per call, and no write.
void absorb_return(
    emulation::RoutineReplacement& replacement,
    std::span<const std::byte> work_ram,
    std::uint8_t entry_stack_pointer,
    emulation::RoutineKey return_instruction,
    std::uint32_t return_master_cycle_offset,
    const emulation::CpuRegisters& at_return);

// Whether an atomic walk can afford to absorb its return.
//
// A transcript with no checkpoints has no boundary inside it at which an
// interrupt could be taken, so the horizon valve declines it outright if it
// would span a user event or a serviceable timer interrupt. Absorbing makes
// such a body twenty-one cycles longer, and a walk that absorbs regardless
// trades those twenty-one cycles for its whole body on every call that no
// longer fits -- 311 calls across three of bank `02`'s families, which is the
// wrong way round and an avoidable one. Ask first, and hand the return back
// when the room is not there. An interruptible transcript has no such limit
// and does not need this.
[[nodiscard]] bool absorbed_return_fits_atomically(
    const emulation::RoutineEntry& entry,
    std::uint32_t body_master_cycles);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_rng_update(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_score_accumulator(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_score_repeat_wrapper(
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
replace_eyeball_animation_prefix(const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram, std::span<const std::byte> rom);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_gray_blob_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_gray_blob_first_live_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_gray_blob_live_wait_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_upper_table_idle_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_upper_table_parent_idle_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_upper_table_actor_idle_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_shared_collision_table_call(
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
replace_mercy_star_not_last_tail(
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
replace_three_blobs_rng_initialization(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_three_blobs_first_movement_continuation(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_three_blobs_y_expiry_negation_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_three_blobs_alternate_motion_reset_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_three_blobs_alternate_motion_arithmetic(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_three_blobs_collision_marker(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_three_blobs_movement_call_seam(
    const emulation::RoutineEntry& entry);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_three_blobs_rng_loop_branch(
    const emulation::RoutineEntry& entry);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_three_blobs_direct_continuation(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_bank7_mode_gate(
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
replace_three_blobs_collision_continuation(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_three_blobs_shared_motion_idle(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_three_blobs_shared_return(
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
replace_tower_insect_source_countdown_return(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// 02:5F78 allocates one Tower-Insect child; 5F7B owns the allocator's
// carry-clear continuation, 5F7E initializes the reserved slot before the
// native camera helper, and 5FD6 restores the source/counts the child/returns.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_tower_insect_child_spawner_seam(
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
    std::span<const std::byte> work_ram,
    bool absorb_rts = true);

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

// The shared Small-Soldier child allocator is split at both of its existing
// native children: 7E87 crosses the actor-slot allocator, 7E8A initializes the
// reserved child and crosses camera activation, and 7EEE restores the source
// slot, advances its group count, and absorbs the original return.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_small_soldier_child_allocation_seam(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// Shared collision-effect allocator used after a confirmed actor hit.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_collision_effect_allocator(
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
replace_actor_slot_allocator(
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
replace_spider_controller_parent(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_front_spider_scheduler_call_seam(
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

// Shared dynamic-actor startup coroutine used by Tower insects at
// 02:5FDB/5FE6 and Small Soldiers at 03:7EFA/7F05. Each countdown/setup pair
// composes the shared coroutine scheduler while the subsequent collision
// controller remains at its already-proven native boundaries.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_dynamic_actor_initial_controller(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// Shared type-$8F impact-burst lifetime controller at 02:42B0. The waiting
// and animation paths absorb their RTS; inactive and terminal effects hand
// off to the registered generic actor clear through 01:C027.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_impact_burst_audio_command_call(
    const emulation::RoutineEntry& entry);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_impact_burst_allocator_success_return(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_impact_burst_lifetime(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_tower_insect_collision_call(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_tower_insect_animation_call(const emulation::RoutineEntry& entry);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_tower_insect_hit_gate(
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
replace_giant_skeleton_controller(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

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
replace_devil_shooter_charge_after_audio(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_devil_shooter_charge_tail(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_devil_shooter_release_countdown_tail(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_devil_shooter_appearance_submission_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_devil_shooter_appearance_submission_tail(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_devil_shooter_post_launch_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_devil_shooter_charge_position(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_devil_shooter_launch_continuation(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_devil_shooter_charge_retraction_calls(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_devil_shooter_charge_entry_controller(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_selective_scene_layout(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_scene_layout_record_loop(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_scene_layout_record_loader(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_scene_layout_record_calls(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_devil_shooter_charge_controller(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_devil_shooter_charge_controller_tail(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_devil_shooter_charge_return(
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
replace_devil_shooter_catch_retraction_tail(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_skeleton_insect_left_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_skeleton_insect_startup_countdown(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_skeleton_insect_left_post_trigger(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_skeleton_insect_right_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_skeleton_insect_right_startup_countdown(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_skeleton_insect_right_post_trigger(
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
replace_skeleton_insect_center_toggle(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_skeleton_insect_center_post_delay_handoff(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_skeleton_insect_center_opening_gate(
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
replace_fixed_marker_helper_call(
    const emulation::RoutineEntry& entry);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_fixed_marker_outer_segment(
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
replace_dragon_egg_reset_gate(
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
replace_shared_four_tick_timer_tail(
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
replace_giant_face_helmet_shared_return(
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
replace_unsigned_fixed_division(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_unsigned_byte_division(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_unsigned_square_root(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_unsigned_24_division(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_motion_damping(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_motion_substep_split(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// 01:C6C2-01:C71D and 01:C721-01:C770, the initial and resumed loops that
// spend the substeps 01:DC95 splits out. Twenty entry points, fifteen crossed
// calls and no native return of their own; see
// native_replacements_substep_loop.cpp.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_ball_substep_loop(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_ball_substep_bbr3(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// 00:F08E-00:F1B3, the shared motion step: a four-way dispatcher on the bottom
// bits of $3800,X over two position integrators and two velocity steps. Eleven
// entry points, six crossed calls -- four of them into its own leaves -- and
// five absorbed returns; see native_replacements_motion_step.cpp.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_actor_motion_step(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_unsigned_byte_multiply(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_unsigned_word_multiply_partial(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_signed_word_normalizer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_signed_word_multiply(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_side_gate_deadline_check(
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
replace_flipper_release_arm(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_flipper_active_update(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_flipper_collision_response_continuation(
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

// What the registry dispatches at 01:C5C9. Twenty-seven walks in
// native_replacements_rules.cpp run the scheduler above in line and adopt the
// `RTS` it stopped on as their own, so the routine cannot run past it and the
// absorption belongs here, where the caller is the core. Nothing composes this.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_coroutine_scheduler_entry(
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

// Work RAM is read for one byte only: the frame the absorbed return pops.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_actor_slot_reset(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// 01:C4DE-01:C50E, the actor scan's preamble: settle the frame's actor enable
// mask, reset the three list cursors, clear the scan index, and fall through
// into the registered scan at 01:C510. C4DE stops at the host's C500 input
// handoff; the host then invokes the same walker at C500 for the native cursor
// suffix. No call and no ROM read; see native_replacements_actor_scan_preamble.cpp.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_actor_scan_preamble(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// Bank 09's second actor scan. It shares the bank-01 queue filer at C606 but
// otherwise only filters actor flags and advances the same $4D cursor.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_bank9_actor_scan(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// The two bank-09 scripted actor initializers share a closed immediate-store
// prefix, then call the bank-01 common initializer under its original JSR
// frame. The longer actor scripts remain core-owned after that handoff.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_bank9_scripted_actor_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// The first upper-table scripted actor's post-motion countdown is a closed
// per-slot update. It deliberately stops at the shared initializer JSR when
// the countdown rolls over.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_bank9_upper_table_countdown(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_upper_parent_animation(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_bank9_upper_table_parent_decay(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// The two upper-table parent actors share a small animation-reset prefix that
// then calls the existing bank-01 coroutine scheduler with an ordinary JSR
// frame.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_bank9_upper_table_parent_initializer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// A bank-02 actor timer's non-expiring decrement-and-return arm.  Expiry
// continues through the original submission sequence in the core.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_bank2_actor_countdown_return(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// The common carry-clear entry branch into bank 03's actor idle handler.  The
// carry-set setup arm remains core-owned.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_bank3_actor_idle_branch(const emulation::RoutineEntry& entry);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_bonus_actor_scan(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// 05:7356's bonus-stage-6 hazard handler.  The common non-expiring timer arm
// is a closed decrement/return; expiry stays at the original scheduler seam.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_bonus_hazard_timer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// 09:60C7-09:612A, the palette-block colour rotator: rotate colours three,
// four and five of the palette-shadow block named in the accumulator, then
// flag the block and the shadow as dirty. Bank 09's first registered routine.
// Fifty-nine instructions, no branch and no ROM read; see
// native_replacements_palette_cycle.cpp.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_palette_colour_cycle(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// 09:7237-09:7268, the score panel's redraw poll: four work RAM gates in front
// of the panel body, which on this route decline the redraw 2,616 times out of
// 2,649. Ten instructions and forty-two per cent of bank 09's remainder; see
// native_replacements_score_panel.cpp.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_score_animation_left_schedule(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_score_animation_left_step(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_score_animation_right_step(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_score_panel_poll(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// 09:727D-09:72C3, the score panel's redraw head and its two naturally
// re-entered digit loops.  Each bounded transaction composes one existing
// glyph/divider child and resumes at 72A2, 72BD, or 72C5.  The final bounded
// transaction composes the byte divider and two glyphs before absorbing the
// score-panel return; see native_replacements_score_panel.cpp.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_score_panel_redraw_tail(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

// 09:72FD-09:737B, the score panel's two-by-two glyph writer. It resolves the
// four ROM tile bytes selected by A, writes both rows through the VDC ports,
// and advances the right-to-left cursor in $35:$36; see
// native_replacements_score_panel.cpp.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_score_panel_glyph(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

// 01:C154-01:C1B5, the main session loop: the frame-sync wait, the nine calls
// the frame is made of, and the tail that decides whether to run another. Ten
// entry points, one per return address, each walk ending on the call it
// crosses; see native_replacements_main_loop.cpp.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_main_session_loop(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// 01:C000-01:C0CB, bank 01's jump table: sixty-eight `JMP absolute` rows, of
// which the recorded route reaches eighteen. One instruction, twelve master
// cycles and no state at all -- the walk reads nothing, writes nothing and
// touches no flag, and ends on the row's target; see
// native_replacements_jump_table.cpp.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_bank_one_jump_table(const emulation::RoutineEntry& entry);

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
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_sat_submission_step(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// The timer-interrupt sound driver, entered from the interrupt vector at
// 00:FC00 and walked to whichever of its two RTI instructions or two chain
// exits control reaches. One walk, three entry points: the vector, and the
// instruction after each epilogue's SEI, which is re-entered separately so
// the rerun test below it reads a snapshot taken after the body ran.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_sound_timer_driver(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

// The bank-45 music sequencer's sample/envelope group. One walk is shared by
// all registered entry points, each stopping in front of its own RTS; a nested
// call the walk makes is inlined into the same transcript.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_music_sequencer(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

// The palette shadow buffer and the VCE upload that drains it. One walk shared
// by seven registered addresses. None of them calls anything, so nothing is
// inlined; each ends *past* its own RTS -- popping the return address out of
// the stack page and resuming at the caller, which is where that technique was
// established -- or in front of the JMP that closes 00:EAA7's colour loop when
// a chunk is full, or -- on 00:EB30's whole-palette arm -- in front of the TIA
// that moves all 1,024 bytes in one instruction, which the core executes before
// 00:EB4D resumes the arm.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_palette_shadow(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

// The post-interpolator tail of 00:E970's palette-fade driver.  The existing
// core-owned JSR at E976 still creates the live E9B6 call frame; this bounded
// continuation only advances the two packed-colour pointers and selects the
// next call or the terminal E994 path.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_palette_fade_caller_tail(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// The setup screen's own block stager, the shadow buffer's only producer
// outside bank 00. Two transcripts: the head builds the asset reference and
// crosses the `JSR $E1FD` that resolves it into a pointer, and 08:5982 -- the
// address that call comes back to -- does the phase arithmetic, the thirty-two
// byte copy and the two dirty flags, then ends past its own RTS.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_setup_screen_palette_stager(
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

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_audio_stack_wrapper(const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// The VDC interrupt handler. One walk shared by eleven entry points: the
// dispatcher's own two halves, the two arms it dispatches to, the two
// addresses the VBlank arm's calls come back to, the secondary handler's
// prologue and epilogue, the pad reader at 00:E195 that the VBlank arm
// calls, and the two RTIs the handler leaves through. Every transcript stops
// in front of a call, an RTS, the PLP that restores the interrupted status
// byte, another entry in this family, or the one instruction here that cannot
// be transcribed at all -- except the two RTI entries, which are one
// instruction each and end wherever the interrupt was taken.
//
// That instruction is the dispatcher's LDA $0000. Not because the value is
// unknowable -- the native VDC model answers a peek without disturbing
// anything, and the entry could carry it the way it carries the frame's pad
// state -- but because the read *is* the interrupt acknowledge: it clears
// every latched status source and is the only way the VDC's interrupt line
// falls. A replacement declares writes, and a write to $0000 selects a
// register rather than acknowledging one, so there is no channel for the side
// effect. The walk splits in front of it instead, and the core runs it.
//
// The pad reader is the other device read here, and it is portable for the
// reason the status read is not: the joypad's answer is a function of the
// frame's pad state, which the entry carries, and of latch lines the routine
// drives itself.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_vblank_handler(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

// 00:E05C is the VDC status read that acknowledges the active interrupt. Its
// hardware read stays in the core rather than becoming a transcript write.
// 01:DA09 and 01:DA0E likewise retain their selected VDC data-port reads.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_vdc_status_read(const emulation::RoutineEntry& entry);

// The VBlank epilogue's PLP samples IRQs before it restores the saved status,
// so the core retains this one instruction rather than a transcript.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_vblank_plp(const emulation::RoutineEntry& entry);

// The music driver's unmasked SEI must retain its pre-mask IRQ sample.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_music_sei(const emulation::RoutineEntry& entry);

// The music sequencer's core-retained call and return boundaries.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_music_jsr(const emulation::RoutineEntry& entry);

// Bank 03's relative call must retain its live two-byte stack frame.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_bsr(const emulation::RoutineEntry& entry);

// The Knight hit caller reloads its source slot immediately before that BSR.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_source_slot_load(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// Exact JSR and RTS instruction seams inside the Knight's 7Axx movement
// helper. The stateful helper body remains interpreted between these seams.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_movement_jsr(const emulation::RoutineEntry& entry);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_movement_rts(const emulation::RoutineEntry& entry);

// Exact stack seams shared by the fixed scheduler's repeated $C060 calls.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_fixed_scheduler_jsr(const emulation::RoutineEntry& entry);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_fixed_scheduler_rts(const emulation::RoutineEntry& entry);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_knight_bcs(const emulation::RoutineEntry& entry);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_giant_face_wait_dispatch_instruction(
    const emulation::RoutineEntry& entry);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_three_blobs_timer_decrement_instruction(
    const emulation::RoutineEntry& entry);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_music_rts(const emulation::RoutineEntry& entry);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_music_jmp(const emulation::RoutineEntry& entry);

// A declined per-voice music walk can still execute this isolated work-RAM
// load without widening the sequencer's interrupt-sensitive transcript.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_music_voice_state_load(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// The noise-note setup prefix is an interruptible three-instruction leaf in
// the duration decoder. It resumes at the original branch, retaining both
// branch selection and the decoder's live caller frame in the core.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_music_noise_note_setup_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// The transpose command's immediate compare and dispatch branch have no
// memory effects and retain the command body in the core at either target.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_music_transpose_dispatch(const emulation::RoutineEntry& entry);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_shared_actor_rts(const emulation::RoutineEntry& entry);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_actor_scheduler_instruction(const emulation::RoutineEntry& entry);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_flipper_owner_instruction(const emulation::RoutineEntry& entry);

// The graphics driver's four port-streaming loops plus the work-RAM byte
// decoder at 00:E4E3 and uncompressed word transfer at E365/E37E. One walk
// shared by the streaming entry points -- each streaming
// routine's head and its chunk/record continuation, plus 00:FA40 and the two
// arms after its hardware-executed BBR0. Every transcript stops at a bounded
// loop boundary, in front of the BBR0, or in front of an RTS.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_vram_upload(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_bank1_vdc_row_upload(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

// The first, non-device-reading segment of bank 01's sprite-attribute
// updater. It stops in front of the live VDC data-port read at 01:DA06.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_vdc_sprite_attribute_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

// The actor-script record loader at 00:F605.  Its normal record form resolves
// an MPR-backed word stream, then crosses the existing banking and VRAM
// decoder transcripts.  The two child-return entries preserve the original
// JSR/PHX/PLX boundaries.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_actor_script_record_loader(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

// The five scene-resident pattern packs at 00:F768.  The bounded setup and
// return tails retain the existing bank-call wrapper and generic VDC decoder
// as separate native transactions.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_resident_pattern_loader(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

// The first, non-device-reading segment of bank 01's sprite-attribute
// updater.  It stops in front of the live VDC data-port read at 01:DA09.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_vdc_sprite_attribute_prefix(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

// The collision-free half of bank 02's shared actor timer tail.  The collision
// table has already answered before this entry; collision and active-state
// continuations retain their original scheduler boundary.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_shared_collision_free_timer_tail(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// The background tilemap upload: the row writer at 00:F3E7 and the scroll
// driver at 00:F261 that decides which row has come into view and calls it. One
// walk shared by eight entry points -- the two routine heads and the six return
// addresses of the calls the driver makes, because a transcript stops in front
// of a call. Two of those calls land on the already-native bank helper at
// 00:E20D, and the walk ends by handing 00:F4F2 to the camera sweep.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_tilemap_upload(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_core_tii_transfer(const emulation::RoutineEntry& entry);

// Scene bootstrap/selection controller, bounded at its child calls and resumed
// at their return addresses. Frame waits and actor passes keep their owners.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_scene_selection_controller(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// The frame around the collision scanner: the playfield-tile probe at 01:CD74,
// the ball/table gate at 01:CA84, the sweep setup at 01:CB2A and its tail, the
// single-slot probe at 01:CBAB and the contact dispatcher at 01:CBCF. One walk
// shared by twenty-nine entry points -- the original twenty collision-tail
// entries plus nine bounded 01:CF33 confirmed-contact response continuations
// split around four already-native physics calls.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_collision_tail(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

// 01:CB7E is the collision sweep's BBR3 <$77. It is retained by the core so
// its two event samples remain at their original instruction boundaries.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_collision_tail_bbr3(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// 01:CB71 is the collision sweep's BBS2 <$77. It is retained by the core so
// its two event samples remain at their original instruction boundaries.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_collision_tail_bbs2(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// 01:D4B2 is an active-flipper-owner RTS. The core keeps its live stack
// return intact so it cannot be mistaken for a standalone transaction.  The
// same exact-core seam also owns the Knight and Giant-Face helper returns.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_flipper_owner_rts(const emulation::RoutineEntry& entry);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_actor_dispatch_vector(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_actor_queue_registration(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// The direct C63E queue-filing tail is also reached from dispatched actor
// handlers, outside C606's registered entry. It owns no call frame and leaves
// the common RTS at C654 to the core.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_actor_queue_terminal(
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

// The Sorcerer tick in physical bank 02: the staggered deployment delay at
// 02:586F, the arrival state at 02:5886 and its reset, and orbit at 02:58C2
// with the return from its interpreter call at 02:58DD. One walk shared by
// five entry points -- two routine heads and three addresses the core arrives
// at after a call this family makes.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_sorcerer_tick(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_bonus_stage4_orbit(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

// Three arithmetic leaves in physical bank 01, sharing one walk because they
// share a shape rather than a purpose: the path-length accumulator at 01:C8F3,
// the range-squared guard at 01:C969 and the direction-octant classifier at
// 01:DAF8. The classifier reads its answer table out of the bank's own ROM
// page, so the walk needs the ROM.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_actor_geometry_leaf(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

// The sweep composes the vertical scanner at 01:CC1A rather than stopping in
// front of the call, so it needs the ROM the scanner reads through.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_collision_scan_run(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_sprite_emit_run(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

// Bank 08's one-character text/tile writer. It selects the VDC address and
// data registers, maps a space/digit/letter to the game's tile encoding, and
// advances the word address for the next call.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_text_tile_write(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// One-shot upper-table actor initializer. It composes the bounded character
// writer and coroutine scheduler while preserving both of their return seams.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_upper_table_actor_tile_schedule(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// Selection input and old/new glyph controller, split at real child calls.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_selection_actor_controller(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_upper_table_string_record(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

// The naturally hot idle return at the head of bank 08's deferred video
// update routine. Active-update states deliberately remain in assembly.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_video_update_idle_gate(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// The bounded seams around bank 08's boot-time 4 KiB work-RAM clear. The
// central TII remains emulated because one instruction exceeds the bridge's
// ordered-write capacity; the two calls, setup prefix and VDC tail do not.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_boot_work_ram_clear_seam(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// 03:6966 as an instruction-boundary transcript. The semantic replacement below
// calls this first and keeps its own paths for the calls this one refuses, so
// every caller of the interpreter gets the transcript without knowing about it.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_collision_table_walk(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_collision_table_interpreter(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

// What the registry dispatches at 03:6966. The interpreter above is shared with
// three sites that run it in line and need it to stop in front of its `RTS`, so
// the return is absorbed here, where the caller is the core rather than another
// transcript. Nothing composes this.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_collision_table_entry(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_collision_response(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);

// What the registry dispatches at 03:7FB1, for the reason above: 03:7F57
// splices this routine in line and reads the `RTS` it stopped on.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_collision_response_entry(
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

// What the registry dispatches at 01:CC1A and 01:CC98. Both routines above are
// run in line by another transcript that reads the `RTS` they stopped on before
// it will splice them, so neither can run past its own return; the entry the
// core takes is the one arrival nothing is reading. Nothing composes these.
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_collision_scan_vertical_reject_entry(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_collision_mask_lookup_entry(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

} // namespace devils_crush::app::detail
namespace devils_crush::app::detail {
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_actor_initialization(const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram);
}
