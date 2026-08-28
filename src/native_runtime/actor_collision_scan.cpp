// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/actor_collision_scan.hpp"

#include "devils_crush/native_runtime/actor_scheduler.hpp"
#include "devils_crush/native_runtime/collision_map_asset.hpp"
#include "devils_crush/native_runtime/mapped_rom.hpp"

namespace devils_crush::native_runtime {

std::optional<ActorCollisionScanContext> make_actor_collision_scan_context(
    const GameStateView& state) noexcept
{
    const auto shape_directories =
        state.actor_collision_shape_directory_pointers();
    const auto mask_tables = state.actor_collision_mask_table_pointers();
    const auto session = state.actor_collision_scan_session();
    if (!shape_directories || !mask_tables || !session) {
        return std::nullopt;
    }
    return ActorCollisionScanContext{
        .shape_directories = *shape_directories,
        .mask_tables = *mask_tables,
        .session_active = session->active,
        .side_flags = session->side_flags,
    };
}

ActorCollisionScanResult scan_actor_collision(
    const MappedRomView& rom,
    const ActorCollisionScanContext& context,
    const GameStateView::ActorSlot actor,
    const CollisionProbe probe)
{
    ActorCollisionScanResult result{
        .actor = actor,
        .probe = probe,
        .vertical_proximity = evaluate_actor_vertical_proximity(
            probe.vertical, actor.collision_table.position_y),
    };
    if (!result.vertical_proximity.intersecting) {
        return result;
    }

    result.shape = resolve_actor_collision_shape(
        rom, context.shape_directories, actor);
    if (!result.shape) {
        result.disposition = ActorCollisionScanDisposition::unresolved_shape_asset;
        return result;
    }

    result.geometry = prepare_actor_collision_geometry({
        .actor = actor,
        .vertical_proximity = result.vertical_proximity,
        .descriptor = result.shape->descriptor,
    });
    if (result.geometry->disposition
        != ActorCollisionGeometryDisposition::bounds_ready) {
        result.disposition = ActorCollisionScanDisposition::outside_horizontal_world;
        return result;
    }

    result.bounds = evaluate_collision_bounds(probe, result.geometry->bounds);
    if (result.bounds->disposition != CollisionBoundsDisposition::intersecting) {
        result.disposition = ActorCollisionScanDisposition::outside_bounds;
        return result;
    }

    result.mask_coordinates = prepare_actor_collision_mask_coordinates({
        .actor_slot = actor.index,
        .shape_phase = actor.collision_shape_phase,
        .session_active = context.session_active,
        .side_flags = context.side_flags,
        .bounds = *result.bounds,
    });
    // CCD5 replaces CC98's first two table roots with the selected pair for
    // this actor shape and phase. The mask base stays shared in `$8E:$8F`.
    result.mask = resolve_actor_collision_mask(
        rom,
        {
            .first_table = result.shape->secondary_geometry_address,
            .second_table = result.shape->descriptor_address,
            .masks = context.mask_tables.masks,
        },
        *result.mask_coordinates);
    if (!result.mask) {
        result.disposition = ActorCollisionScanDisposition::unresolved_mask_asset;
        return result;
    }

    result.disposition = result.mask->evaluation.disposition
            == CollisionMaskDisposition::solid
        ? ActorCollisionScanDisposition::solid_mask
        : ActorCollisionScanDisposition::empty_mask;
    return result;
}

std::optional<ActorCollisionScanSweepResult> scan_actor_collision_sweep(
    const MappedRomView& rom,
    const GameStateView& state,
    const CollisionProbe probe)
{
    const auto context = make_actor_collision_scan_context(state);
    if (!context) {
        return std::nullopt;
    }

    ActorCollisionScanSweepResult result{
        .side_flags_after_scan = context->side_flags,
    };
    auto candidate_context = *context;
    const ActorSchedulerView scheduler(state);
    for (const auto& actor : scheduler.actors_for_collision_retry_full_scan()) {
        auto candidate = scan_actor_collision(rom, candidate_context, actor, probe);
        if (candidate.mask_coordinates) {
            candidate_context.side_flags = candidate.mask_coordinates->side_flags;
            result.side_flags_after_scan = candidate_context.side_flags;
        }
        const auto solid = candidate.disposition
            == ActorCollisionScanDisposition::solid_mask;
        result.candidates.push_back(std::move(candidate));
        if (solid) {
            result.first_solid_actor_slot = actor.index;
            break;
        }
    }
    return result;
}

std::optional<ActorCollisionScanSweepResult> scan_normal_actor_collision_sweep(
    const MappedRomView& rom,
    const GameStateView& state,
    const GameStateView::NormalActorCollisionScanWalk walk,
    const CollisionProbe probe)
{
    const auto context = make_actor_collision_scan_context(state);
    if (!context) {
        return std::nullopt;
    }

    const auto past_last_slot = static_cast<std::uint16_t>(walk.first_slot)
        + static_cast<std::uint16_t>(walk.slot_count);
    if (past_last_slot > GameStateView::kActorSlotCount) {
        return std::nullopt;
    }

    ActorCollisionScanSweepResult result{
        .side_flags_after_scan = context->side_flags,
    };

    auto candidate_context = *context;
    result.candidates.reserve(walk.slot_count);
    for (std::uint16_t index = walk.first_slot; index < past_last_slot; ++index) {
        const auto actor = state.actor(static_cast<std::uint8_t>(index));
        if (!actor) {
            return std::nullopt;
        }
        if ((actor->attributes & 0x80U) == 0U
            || (actor->attributes & 0x40U) != 0U) {
            continue;
        }
        if (walk.require_activity_bit_6
            && (actor->activity_flags & 0x40U) == 0U) {
            continue;
        }

        auto candidate = scan_actor_collision(rom, candidate_context, *actor, probe);
        if (candidate.mask_coordinates) {
            candidate_context.side_flags = candidate.mask_coordinates->side_flags;
            result.side_flags_after_scan = candidate_context.side_flags;
        }
        const auto solid = candidate.disposition
            == ActorCollisionScanDisposition::solid_mask;
        result.candidates.push_back(std::move(candidate));
        if (solid) {
            result.first_solid_actor_slot = actor->index;
            break;
        }
    }
    return result;
}

std::optional<CollisionRetryActorScanResult>
scan_collision_retry_full_actors(
    const MappedRomView& rom,
    const GameStateView& state,
    const CollisionRetryProbeResult& retry_probe,
    const std::uint8_t parent_vertical_high_byte)
{
    const auto probe = make_collision_retry_full_scan_probe(
        retry_probe, parent_vertical_high_byte);
    if (!probe) {
        return std::nullopt;
    }
    const auto actor_sweep = scan_actor_collision_sweep(rom, state, *probe);
    if (!actor_sweep) {
        return std::nullopt;
    }
    return CollisionRetryActorScanResult{
        .probe = *probe,
        .actor_sweep = *actor_sweep,
    };
}

std::optional<CollisionRetryFullScanResult> scan_collision_retry_full(
    const MappedRomView& rom,
    const GameStateView& state,
    const CollisionRetryProbeResult& retry_probe,
    const std::uint8_t parent_vertical_high_byte)
{
    const auto actor_scan = scan_collision_retry_full_actors(
        rom, state, retry_probe, parent_vertical_high_byte);
    if (!actor_scan) {
        return std::nullopt;
    }
    if (actor_scan->actor_sweep.first_solid_actor_slot) {
        return CollisionRetryFullScanResult{
            .actor_scan = *actor_scan,
            .disposition = CollisionRetryFullScanDisposition::actor_solid,
        };
    }

    const auto pointers = state.playfield_collision_table_pointers();
    const auto map = pointers ? decode_main_playfield_collision_map(rom, *pointers)
                              : std::nullopt;
    if (!map) {
        return CollisionRetryFullScanResult{
            .actor_scan = *actor_scan,
            .disposition =
                CollisionRetryFullScanDisposition::unresolved_playfield_asset,
        };
    }

    const auto playfield = evaluate_playfield_collision(
        map->view(),
        {
            .horizontal_pixels = actor_scan->probe.horizontal,
            .vertical_pixels = actor_scan->probe.vertical,
        });
    return CollisionRetryFullScanResult{
        .actor_scan = *actor_scan,
        .playfield = playfield,
        .disposition = CollisionRetryFullScanDisposition::playfield_evaluated,
    };
}

CollisionRetryFullScanOutcome resolve_collision_retry_full_scan_outcome(
    const CollisionRetryFullScanResult& result) noexcept
{
    if (result.disposition == CollisionRetryFullScanDisposition::actor_solid) {
        return CollisionRetryFullScanOutcome::solid;
    }
    if (result.disposition
            != CollisionRetryFullScanDisposition::playfield_evaluated
        || !result.playfield) {
        return CollisionRetryFullScanOutcome::unresolved;
    }
    switch (result.playfield->disposition) {
    case PlayfieldCollisionDisposition::solid:
        return CollisionRetryFullScanOutcome::solid;
    case PlayfieldCollisionDisposition::outside_playfield:
    case PlayfieldCollisionDisposition::outside_map:
    case PlayfieldCollisionDisposition::empty:
        return CollisionRetryFullScanOutcome::clear;
    case PlayfieldCollisionDisposition::missing_mask:
        return CollisionRetryFullScanOutcome::unresolved;
    }
    return CollisionRetryFullScanOutcome::unresolved;
}

} // namespace devils_crush::native_runtime
