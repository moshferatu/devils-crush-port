// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/app/native_replacements.hpp"
#include "native_replacements_internal.hpp"

#include <cstddef>
#include <optional>
#include <span>

namespace devils_crush::app {

using emulation::RoutineEntry;
using emulation::RoutineKey;
using emulation::RoutineReplacement;

namespace {
[[nodiscard]] std::optional<RoutineReplacement>
replace_native_gameplay_routine_impl(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    std::optional<RoutineReplacement> replacement;
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x00U,
            .logical_pc = 0xF1B4U,
        }) {
        replacement =
            detail::replace_position_integration(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xD081U,
        }) {
        replacement = detail::replace_flipper_geometry_lookup(
            entry,
            work_ram,
            rom);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC5C9U,
        }) {
        replacement =
            detail::replace_coroutine_scheduler(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC3CBU,
        }) {
        replacement =
            detail::replace_nudge_input_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC49EU,
        }) {
        replacement =
            detail::replace_session_input_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xD466U,
        }) {
        replacement =
            detail::replace_flipper_input_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xCC1AU,
        }) {
        replacement = detail::replace_collision_scan_vertical_reject(
            entry,
            work_ram,
            rom);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xCC98U,
        }) {
        replacement = detail::replace_collision_mask_lookup(
            entry,
            work_ram,
            rom);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xCCD5U,
        }) {
        replacement =
            detail::replace_collision_geometry(entry, work_ram, rom);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xD500U,
        }) {
        replacement =
            detail::replace_flipper_phase_step(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xD1A2U,
        }) {
        replacement = detail::replace_motion_rotation(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xCFD8U,
        }) {
        replacement = detail::replace_collision_motion_adjustment(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xD09EU,
        }) {
        replacement =
            detail::replace_flipper_projection(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xD612U,
        }) {
        replacement = detail::replace_flipper_radius_projection(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xD62EU,
        }) {
        replacement = detail::replace_flipper_angle_projection(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xDD09U,
        }) {
        replacement =
            detail::replace_motion_damping(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x00U,
            .logical_pc = 0xF55CU,
        }) {
        replacement =
            detail::replace_score_accumulator(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4DD9U,
        }) {
        replacement = detail::replace_giant_skeleton_eyeball_gate(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4FC1U,
        }) {
        replacement = detail::replace_mercy_star_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x42F6U,
        }) {
        replacement =
            detail::replace_dragon_egg_controller(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4345U,
        }) {
        replacement =
            detail::replace_dragon_egg_hit_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4357U,
        }) {
        replacement =
            detail::replace_dragon_egg_hit_continuation(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4371U,
        }) {
        replacement =
            detail::replace_dragon_egg_post_allocation(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x445BU,
        }) {
        replacement =
            detail::replace_big_star_feature_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5116U,
        }) {
        replacement =
            detail::replace_devil_shooter_hit_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x52B8U,
        }) {
        replacement =
            detail::replace_devil_shooter_catch_continuation(
                entry,
                work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x54B9U,
        }) {
        replacement =
            detail::replace_skeleton_insect_left_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5551U,
        }) {
        replacement =
            detail::replace_skeleton_insect_right_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x55F9U,
        }) {
        replacement =
            detail::replace_skeleton_insect_center_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5604U,
        }) {
        replacement =
            detail::replace_skeleton_insect_center_continuation(
                entry,
                work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5A4BU,
        }) {
        replacement =
            detail::replace_dragon_head_hit_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5A6EU,
        }) {
        replacement =
            detail::replace_dragon_head_hit_continuation(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x63F1U,
        }) {
        replacement =
            detail::replace_devils_coffin_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x640AU,
        }) {
        replacement =
            detail::replace_devils_coffin_opening(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x6445U,
        }) {
        replacement =
            detail::replace_devils_coffin_closing(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x62B8U,
        }) {
        replacement =
            detail::replace_giant_door_hit_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x74D8U,
        }) {
        replacement =
            detail::replace_giant_face_request_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x767EU,
        }) {
        replacement =
            detail::replace_giant_face_animator_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x76FAU,
        }) {
        replacement = detail::replace_giant_face_crown_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x7726U,
        }) {
        replacement = detail::replace_giant_face_mouth_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x6339U,
        }) {
        replacement =
            detail::replace_walled_star_hit_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x60C3U,
        }) {
        replacement = detail::replace_tower_hit_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x60E0U,
        }) {
        replacement =
            detail::replace_tower_hit_continuation(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x611CU,
        }) {
        replacement = detail::replace_tower_post_dispatch(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x60A6U,
        }) {
        replacement =
            detail::replace_tower_guard_selector(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x78C7U,
        }) {
        replacement =
            detail::replace_bumper_hit_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x78F5U,
        }) {
        replacement =
            detail::replace_bumper_primary_progress(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x7912U,
        }) {
        replacement =
            detail::replace_bumper_secondary_progress(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x7CAFU,
        }) {
        replacement =
            detail::replace_fixed_response_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x7B27U,
        }) {
        replacement =
            detail::replace_fixed_scheduler_gate(entry, work_ram);
    } else if (
        entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x7B4EU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x7B5DU,
            }) {
        replacement =
            detail::replace_fixed_scheduler_submission(entry);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x7B7AU,
        }) {
        replacement =
            detail::replace_fixed_marker_gate(entry, work_ram);
    } else if (
        entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x7BBBU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x7BCDU,
            }) {
        replacement =
            detail::replace_fixed_marker_submission(entry);
    } else if (
        entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x7BE0U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x7BE8U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x7BECU,
            }) {
        replacement =
            detail::replace_fixed_marker_helper_segment(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5955U,
        }) {
        replacement = detail::replace_scaled_word_add(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x65C2U,
        }) {
        replacement =
            detail::replace_three_blobs_controller(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x00U,
            .logical_pc = 0xE8B3U,
        }) {
        replacement = detail::replace_rng_update(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x00U,
            .logical_pc = 0xE837U,
        }) {
        replacement =
            detail::replace_signed_word_negation(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x00U,
            .logical_pc = 0xE905U,
        }) {
        replacement =
            detail::replace_signed_division(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x00U,
            .logical_pc = 0xE569U,
        }) {
        replacement = detail::replace_unsigned_byte_multiply(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x00U,
            .logical_pc = 0xE624U,
        }) {
        replacement = detail::replace_unsigned_multiply_round(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x00U,
            .logical_pc = 0xE65CU,
        }) {
        replacement = detail::replace_signed_fixed_multiply(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x6966U,
        }) {
        replacement = detail::replace_collision_table_interpreter(
            entry,
            work_ram,
            rom);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x7FB1U,
        }) {
        replacement =
            detail::replace_collision_response(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x7F57U,
        }) {
        replacement = detail::replace_collision_consumer_no_hit(
            entry,
            work_ram,
            rom);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x00U,
            .logical_pc = 0xF4FCU,
        }) {
        replacement =
            detail::replace_actor_camera_activation(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC574U,
        }) {
        replacement = detail::replace_actor_slot_reset(entry);
    } else {
        replacement = detail::replace_cached_position_restore(
            entry,
            work_ram);
    }
    return replacement;
}


} // namespace

std::optional<RoutineReplacement> replace_native_gameplay_routine(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    return replace_native_gameplay_routine_impl(entry, work_ram, rom);
}

} // namespace devils_crush::app
