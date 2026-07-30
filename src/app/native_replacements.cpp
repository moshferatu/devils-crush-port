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
            work_ram);
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
