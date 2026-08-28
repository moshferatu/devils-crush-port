// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/actor_collision_geometry.hpp"
#include "devils_crush/native_runtime/actor_collision_mask_asset.hpp"
#include "devils_crush/native_runtime/collision_map.hpp"
#include "devils_crush/native_runtime/collision_retry_probe.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace devils_crush::native_runtime {

class MappedRomView;

// Table/session state that is shared by every candidate in one CED2 scan.
// The collision probe remains an explicit argument because the retry route's
// high vertical byte is not yet certified by a natural full-scan trace.
struct ActorCollisionScanContext {
    GameStateView::ActorCollisionShapeDirectoryPointers shape_directories;
    GameStateView::ActorCollisionMaskTablePointers mask_tables;
    bool session_active = false;
    std::uint8_t side_flags = 0;

    [[nodiscard]] bool operator==(const ActorCollisionScanContext&) const
        = default;
};

// Captures all shared CED2 scan inputs from the typed emulator boundary. This
// keeps pointer and session decoding out of the future native retry owner.
[[nodiscard]] std::optional<ActorCollisionScanContext>
make_actor_collision_scan_context(const GameStateView& state) noexcept;

enum class ActorCollisionScanDisposition {
    outside_vertical_proximity,
    unresolved_shape_asset,
    outside_horizontal_world,
    outside_bounds,
    unresolved_mask_asset,
    empty_mask,
    solid_mask,
};

// The complete result of one native candidate scan. Every accepted stage is
// retained for diagnostics and for the later target-specific contact owner;
// failure to read a mapped asset is never classified as a clear collision.
struct ActorCollisionScanResult {
    GameStateView::ActorSlot actor;
    CollisionProbe probe;
    ActorVerticalProximity vertical_proximity;
    std::optional<ActorCollisionShapeResolution> shape;
    std::optional<ActorCollisionGeometry> geometry;
    std::optional<CollisionBoundsEvaluation> bounds;
    // CC41 updates the shared side flags before CC98 reads the mask asset.
    // Retain that output even when the later mapped-ROM lookup is unresolved.
    std::optional<ActorCollisionMaskCoordinates> mask_coordinates;
    std::optional<ActorCollisionMaskResolution> mask;
    ActorCollisionScanDisposition disposition =
        ActorCollisionScanDisposition::outside_vertical_proximity;

    [[nodiscard]] bool operator==(const ActorCollisionScanResult&) const
        = default;
};

// The recovered actor portion of CED2: every eligible dynamic slot is tested
// in table order and the sweep stops at its first solid mask. The playfield
// continuation and target-specific response stay outside this pure result
// until a natural retry-to-CED2 trace proves their handoff.
struct ActorCollisionScanSweepResult {
    std::vector<ActorCollisionScanResult> candidates;
    std::optional<std::uint8_t> first_solid_actor_slot;
    // The CC1A session flag value after the last attempted candidate. This is
    // carried into every later candidate exactly as the original sweep does.
    std::uint8_t side_flags_after_scan = 0;

    [[nodiscard]] bool operator==(const ActorCollisionScanSweepResult&) const
        = default;
};

// The recovered actor-only portion of CED2 entered from a retry. A result is
// available only when CEB5 selected the full scan and the captured table state
// was valid. CED2's following playfield probe and all target responses remain
// deliberately outside this typed result.
struct CollisionRetryActorScanResult {
    CollisionProbe probe;
    ActorCollisionScanSweepResult actor_sweep;

    [[nodiscard]] bool operator==(const CollisionRetryActorScanResult&) const
        = default;
};

// CED2 returns on the first solid actor; only an actor-clear sweep reaches
// CD74's playfield probe. The result exposes that source order without
// attaching an unproven collision response to either solid outcome.
enum class CollisionRetryFullScanDisposition {
    actor_solid,
    unresolved_playfield_asset,
    playfield_evaluated,
};

struct CollisionRetryFullScanResult {
    CollisionRetryActorScanResult actor_scan;
    std::optional<PlayfieldCollisionEvaluation> playfield;
    CollisionRetryFullScanDisposition disposition =
        CollisionRetryFullScanDisposition::unresolved_playfield_asset;

    [[nodiscard]] bool operator==(const CollisionRetryFullScanResult&) const
        = default;
};

// CED2 returns its carry directly to CDF8's placement loop. This preserves
// that control result without conflating a solid actor or playfield with its
// later target-specific contact response.
enum class CollisionRetryFullScanOutcome : std::uint8_t {
    clear,
    solid,
    unresolved,
};

// Runs the recovered geometry and mask portion of one actor collision scan.
// `solid_mask` proves shape overlap only; the caller still chooses the named
// target response and submits it through the native contact dispatcher.
[[nodiscard]] ActorCollisionScanResult scan_actor_collision(
    const MappedRomView& rom,
    const ActorCollisionScanContext& context,
    GameStateView::ActorSlot actor,
    CollisionProbe probe);

// Runs the typed CED2 actor sweep from one captured state image. The collision
// probe remains explicit because the retry route's vertical high byte is not
// certified yet; a malformed state image produces no result.
[[nodiscard]] std::optional<ActorCollisionScanSweepResult>
scan_actor_collision_sweep(
    const MappedRomView& rom,
    const GameStateView& state,
    CollisionProbe probe);

// Runs CB48's normal-table actor portion.  Unlike CED2, the normal sweep
// honors its caller's cursor/countdown range and only admits geometry when
// CB55's gate is zero.  The source actor table is 128 records wide; a
// caller-prepared range outside it stays unavailable rather than wrapping a
// guessed actor record.
[[nodiscard]] std::optional<ActorCollisionScanSweepResult>
scan_normal_actor_collision_sweep(
    const MappedRomView& rom,
    const GameStateView& state,
    GameStateView::NormalActorCollisionScanWalk walk,
    CollisionProbe probe);

// Composes CEB5's recovered full-scan handoff with CED2's actor sweep. The
// inherited vertical high byte stays explicit at this boundary because CEB5
// intentionally carries it forward from its parent collision scan.
[[nodiscard]] std::optional<CollisionRetryActorScanResult>
scan_collision_retry_full_actors(
    const MappedRomView& rom,
    const GameStateView& state,
    const CollisionRetryProbeResult& retry_probe,
    std::uint8_t parent_vertical_high_byte);

// Runs CED2's recovered actor-first/full-scan structure. A no-actor-contact
// result evaluates the CD74-equivalent playfield map from the same captured
// table state. Solid results stay descriptive until the owning response path
// is observed and ported.
[[nodiscard]] std::optional<CollisionRetryFullScanResult>
scan_collision_retry_full(
    const MappedRomView& rom,
    const GameStateView& state,
    const CollisionRetryProbeResult& retry_probe,
    std::uint8_t parent_vertical_high_byte);

// Converts a completed CED2 result into the carry-like condition consumed by
// CDF8. A missing mapped asset or an invalid map-mask result remains explicit
// rather than being silently classified as an empty collision probe.
[[nodiscard]] CollisionRetryFullScanOutcome
resolve_collision_retry_full_scan_outcome(
    const CollisionRetryFullScanResult& result) noexcept;

} // namespace devils_crush::native_runtime
