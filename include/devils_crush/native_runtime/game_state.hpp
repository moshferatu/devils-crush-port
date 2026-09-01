// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/ball_physics.hpp"
#include "devils_crush/native_runtime/collision_detection.hpp"
#include "devils_crush/native_runtime/collision_reaction.hpp"
#include "devils_crush/native_runtime/collision_response.hpp"
#include "devils_crush/native_runtime/collision_table.hpp"
#include "devils_crush/native_runtime/table_rule_latches.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace devils_crush::native_runtime {

// Typed, read-only access to the 8 KiB image exchanged with the emulator.
// These names are the migration boundary: native gameplay code takes domain
// objects below, not raw logical addresses.
class GameStateView {
public:
    static constexpr std::size_t kWorkRamSize = 0x2000U;
    static constexpr std::uint8_t kActorSlotCount = 0x80U;

    // The game's shared allocator uses this mode to select its initial
    // reservation kind for a newly claimed dynamic actor record.
    enum class DynamicActorReservationKind {
        standard,
        alternate,
    };

    struct Fixed8_8 {
        std::int16_t raw = 0;

        [[nodiscard]] bool operator==(const Fixed8_8&) const = default;
    };

    // The ball is the one actor that retains a sub-pixel byte alongside its
    // signed 16-bit world coordinate.  Keep the ROM's signed 16.8 value
    // intact at the capture boundary instead of discarding that byte through
    // the ordinary actor-coordinate view.
    struct Fixed16_8 {
        std::int32_t raw = 0;

        [[nodiscard]] bool operator==(const Fixed16_8&) const = default;
    };

    struct ActorSlot {
        std::uint8_t index = 0;
        // `$3000,X`: negative values enter the original actor dispatcher.
        std::uint8_t activity_flags = 0;
        // `$3080,X`: scheduler/appearance attributes consumed after dispatch.
        std::uint8_t attributes = 0;
        // `$3800,X`: actor-local collision and transition latch.
        std::uint8_t collision_state = 0;
        // `$3C00,X`: actor-family-local counter. A behavior-specific capture
        // adapter gives this byte its domain meaning; generic actor code must
        // not interpret it.
        std::uint8_t behavior_counter = 0;
        // `$3F00,X` / `$3F80,X`: original dispatch vector, retained only for
        // identifying the behavior that a native scheduler will name.
        std::uint16_t handler_address = 0;
        // Signed 8.8 horizontal motion at `$3400:$3480`.
        Fixed8_8 horizontal_motion;
        // Signed 8.8 vertical motion at `$3500:$3580`.
        Fixed8_8 vertical_motion;
        // `$3700,X`: configuration selector for this actor's collision
        // table. It is decoded as table data, not exposed as a RAM offset.
        CollisionTableProgram collision_table_program;
        // `$3900,X` / `$3980,X`: selectors for the actor's collision-shape
        // directory and phase table. The geometry asset adapter resolves the
        // selected four-byte shape through the current MPR map.
        std::uint8_t collision_shape_directory_index = 0;
        std::uint8_t collision_shape_phase = 0;
        // Collision-table and reaction state, decoded so a capture adapter can
        // seed a native actor without letting raw RAM into the actor domain.
        CollisionTableState collision_table;
        CollisionReactionState collision_reaction;

        [[nodiscard]] bool operator==(const ActorSlot&) const = default;
    };

    // The actor-pass preambles use this frame-local gate before the per-slot
    // scheduler starts. A nonzero freeze latch disables the shared actor-enable
    // mask for the pass; otherwise the selected mask is retained. The short
    // C539 preamble owns no further mask selection.
    struct ActorFrameGate {
        bool frozen = false;
        std::uint8_t enabled_actor_mask = 0U;

        [[nodiscard]] bool operator==(const ActorFrameGate&) const = default;
    };

    // C606's three SAT-filing queues live outside the parallel actor records.
    // Keep their RAM-shaped image at this read-only boundary: the scheduler
    // adapter below gives consumers a canonical queue state, while this type
    // retains the distinct shared-queue cursors needed to validate it against
    // a live C606 call.
    struct ActorPresentationQueueImage {
        std::array<std::uint8_t, kActorSlotCount> auxiliary_slots{};
        std::array<std::uint8_t, kActorSlotCount> primary_slots{};
        std::uint8_t auxiliary_count = 0U;
        std::uint8_t primary_low_count = 0U;
        std::uint8_t primary_high_next_index = 0U;

        [[nodiscard]] bool operator==(const ActorPresentationQueueImage&) const
            = default;
    };

    // BS-4's controller owns three one-byte objective flags while the fixed
    // actor records retain their individual eight-hit counters. Expose the
    // paired capture values only to the bonus-stage adapter.
    struct BonusStage4ObjectiveSlot {
        std::uint8_t index = 0U;
        bool active = false;
        std::uint8_t hits_remaining = 0U;

        [[nodiscard]] bool operator==(const BonusStage4ObjectiveSlot&) const
            = default;
    };

    // A BS-4 paired source's type-$C2 linked target keeps its copied source
    // identity and motion-refresh countdown in parallel arrays. These bytes
    // are not generic actor state: both target dispatches decrement `$3B00,X`
    // and ask the shared RNG motion helper for a new vector only on expiry.
    struct BonusStage4LinkedTargetSlot {
        std::uint8_t index = 0U;
        std::uint8_t source_slot = 0U;
        std::uint8_t objective_index = 0U;
        std::uint8_t ticks_until_motion_refresh = 0U;

        [[nodiscard]] bool operator==(const BonusStage4LinkedTargetSlot&) const
            = default;
    };

    // The lower-table Bumpers have a fixed layout and a family-local
    // progression stored in bytes which other actor families reuse as cursors
    // or appearance counters. Keep those bytes out of ActorSlot and expose
    // them only through this Bumper-specific capture snapshot.
    struct BumperSlot {
        std::uint8_t index = 0;
        std::uint8_t side = 0;
        std::uint8_t hit_phase = 0;
        std::uint8_t primary_appearance = 0;
        std::uint8_t secondary_damage = 0;

        [[nodiscard]] bool operator==(const BumperSlot&) const = default;
    };

    // The fixed Tower's local hit progression lives outside the parallel actor
    // arrays. It is intentionally exposed only for its own capture adapter.
    struct TowerSlot {
        std::uint8_t index = 0;
        std::uint8_t hit_progress = 0;
        std::uint8_t remaining_armed_passes = 0;

        [[nodiscard]] bool operator==(const TowerSlot&) const = default;
    };

    // The fixed Tower-insect source keeps its generation-local counters
    // outside the parallel actor arrays.  The source-specific capture adapter
    // pairs this snapshot with the live dynamic child records.
    struct TowerInsectSourceSlot {
        std::uint8_t index = 0;
        std::uint8_t live_children = 0;
        std::uint8_t spawns_remaining = 0;
        // `$3A2C`: the active source's one-to-48 tick spawn countdown.
        std::uint8_t ticks_until_next_spawn = 0;

        [[nodiscard]] bool operator==(const TowerInsectSourceSlot&) const = default;
    };

    // The two fixed Side-Stars keep their five-child generation bookkeeping
    // partly in their actor records and partly in side-specific live counters.
    // Expose that coupled snapshot only to the Small-Soldier source capture;
    // generic actor code must not interpret these reused bytes as a source.
    struct SmallSoldierSourceSlot {
        std::uint8_t index = 0;
        std::uint8_t live_children = 0;
        std::uint8_t pending_spawns = 0;
        std::uint8_t ticks_until_next_spawn = 0;
        std::uint8_t appearance = 0;

        [[nodiscard]] bool operator==(const SmallSoldierSourceSlot&) const = default;
    };

    // The shared type-$8F collision effect keeps its source link and lifetime
    // in parallel arrays which have unrelated meanings for ordinary actors.
    // Expose those bytes only to the effect capture adapter.
    struct CollisionEffectSlot {
        std::uint8_t index = 0;
        std::uint8_t source_slot = 0;
        std::uint8_t source_association = 0;
        std::uint8_t ticks_remaining = 0;

        [[nodiscard]] bool operator==(const CollisionEffectSlot&) const = default;
    };

    // The fixed Giant Skeleton's Eyeball keeps the final mouth-animation
    // countdown outside generic actor collision state. This family-only view
    // names `$3A2F` as its appearance timer, `$3B2F:$3BAF` as its little-endian
    // final wait, and `$3980` as the current 0/1 appearance selector.
    struct GiantSkeletonEyeballFinalAnimationSlot {
        std::uint8_t index = 0;
        std::uint8_t appearance_ticks_remaining = 0;
        std::uint16_t ticks_remaining = 0;
        std::uint8_t appearance = 0;

        [[nodiscard]] bool operator==(
            const GiantSkeletonEyeballFinalAnimationSlot&) const = default;
    };

    // `$2CF4` is the Dragon's-Egg fixed-target live count, bit zero of
    // `$2CF7` arms the source reset path, and bit zero of `$2D20` records
    // the all-eggs completion observed by the Eyeball. They are cluster
    // invariants, not generic actor fields.
    struct DragonEggClusterSlot {
        std::uint8_t live_eggs = 0;
        bool reset_requested = false;
        bool completion_latched = false;

        [[nodiscard]] bool operator==(const DragonEggClusterSlot&) const = default;
    };

    // The Giant Face keeps its form and cross-owner requests outside the
    // fixed actor records. Expose that small family-local snapshot only to
    // its capture adapter; native gameplay receives named state instead of
    // the source latches.
    struct GiantFaceSlot {
        std::uint8_t persisted_form = 0;
        bool wake_requested = false;
        bool transformation_requested = false;
        std::uint8_t display_selector = 0;
        std::uint8_t animation_mode = 0;
        // Bit zero means that the mouth surface has changed into its open
        // form. Bit seven is independently raised once that open mouth has
        // captured the ball and requested bonus stage 4.
        bool mouth_open = false;
        bool bonus_stage_4_requested = false;

        [[nodiscard]] bool operator==(const GiantFaceSlot&) const = default;
    };

    // The two Giant-Face helmet openings share a four-tick idle animation,
    // but their side selector and local state use bytes that unrelated actors
    // treat as collision cursors. Keep that decoding at the capture boundary.
    struct GiantFaceHelmetChannelSlot {
        std::uint8_t index = 0;
        std::uint8_t selector = 0;
        std::uint8_t ticks_until_appearance = 0;
        std::uint8_t appearance = 0;

        [[nodiscard]] bool operator==(const GiantFaceHelmetChannelSlot&) const = default;
    };

    struct BallSlot {
        // `$3000`: the dedicated ball record is active when this is negative.
        std::uint8_t activity_flags = 0;
        // `$3100:$3180:$3200`: signed horizontal 16.8 world coordinate.
        Fixed16_8 position_x;
        // `$3280:$3300:$3380`: signed vertical 16.8 world coordinate.
        Fixed16_8 position_y;
        // `$3400:$3480` and `$3500:$3580`: signed 8.8 frame velocity.
        Fixed8_8 horizontal_motion;
        Fixed8_8 vertical_motion;
        // `$3C80`: ball-physics flags, including the fractional-only bit.
        std::uint8_t motion_flags = 0;
        // `$3800`: CF33 consults this actor-local collision marker after a
        // signed selector. Its high bit hands the contact to CDF8; a clear
        // marker continues through CF3F's ordinary reflected response.
        std::uint8_t collision_state = 0;

        [[nodiscard]] bool operator==(const BallSlot&) const = default;
    };

    // F1B4 preserves the ball's six position bytes before it integrates a
    // colliding substep. CDF8 restores this exact 16.8 pair before trying its
    // placement offsets; it is therefore distinct from the live BallSlot.
    struct BallCollisionPositionCache {
        Fixed16_8 position_x;
        Fixed16_8 position_y;

        [[nodiscard]] bool operator==(const BallCollisionPositionCache&) const
            = default;
    };

    // The global table latches used by CFD8 after a target has selected its
    // response orientation. They are captured together because none is an
    // actor-local property: the force/quarter latches alter reduced response,
    // while the two bytes tune ordinary signed acceleration.
    struct CollisionResponseTableState {
        bool force_reduced_mode = false;
        bool use_quarter_reduced_scale = false;
        NormalCollisionResponseTuning normal_tuning;

        [[nodiscard]] bool operator==(const CollisionResponseTableState&) const
            = default;
    };

    // `$78` and the CFD8 table latches are one CF33 input snapshot. `$78` is
    // intentionally separate from an actor's `$1D00,X` collision-table flag:
    // CF33 reads the shared byte after contact dispatch to choose both its
    // D081 basis and its response mode.
    struct CollisionResponseInput {
        std::uint8_t orientation = 0;
        CollisionResponseTableState table_state;

        // CF33 takes its reflected-response leg only when this shared selector
        // is signed. The clear-selector leg starts at CF5B and has separate
        // local-normal handling.
        [[nodiscard]] bool selects_signed_cf33_response() const noexcept
        {
            return (orientation & 0x80U) != 0U;
        }

        [[nodiscard]] bool operator==(const CollisionResponseInput&) const
            = default;
    };

    // The selected score slot is a 24-bit binary accumulator.  The original
    // score routine selects it through `$2251`, and bit zero of `$2CC0`
    // requests a display refresh after an award.
    struct ActiveScore {
        std::uint8_t slot = 0;
        std::uint32_t stored_units = 0;
        bool display_dirty = false;

        [[nodiscard]] bool operator==(const ActiveScore&) const = default;
    };

    // Cross-target latches sampled by the recovered table-rule owners. These
    // are decoded together because their meanings belong to the table, not to
    // whichever actor happens to consume one later in actor order.
    using TableRuleLatchSnapshot = TableRuleLatches;

    // Live logical addresses of the playfield's parallel tile-ID,
    // collision-attribute, and eight-row mask tables. The native asset adapter
    // resolves these through the current ROM mapping; gameplay code only sees
    // the resulting CollisionMapAsset.
    struct PlayfieldCollisionTablePointers {
        std::uint16_t tile_ids = 0;
        std::uint16_t collision_attributes = 0;
        std::uint16_t masks = 0;

        [[nodiscard]] bool operator==(
            const PlayfieldCollisionTablePointers&) const = default;
    };

    // `$96-$9D`: four live directories used to resolve an actor's collision
    // shape. These addresses remain adapter data; native geometry receives a
    // decoded four-byte descriptor instead of a logical pointer.
    struct ActorCollisionShapeDirectoryPointers {
        std::array<std::uint16_t, 4> tables{};

        [[nodiscard]] bool operator==(
            const ActorCollisionShapeDirectoryPointers&) const = default;
    };

    // `$74:$75`, `$72:$73`, and `$8E:$8F`: live table roots used by CC98
    // after an actor shape has passed bounds and selected its local mask bit.
    // They are captured as asset inputs; native contact code sees decoded
    // masks and decisions instead of logical pointers.
    struct ActorCollisionMaskTablePointers {
        std::uint16_t first_table = 0;
        std::uint16_t second_table = 0;
        std::uint16_t masks = 0;

        [[nodiscard]] bool operator==(
            const ActorCollisionMaskTablePointers&) const = default;
    };

    // CC98 consumes these already-staged scratch values. `vertical_offset`
    // is the low byte of CC95's row-major vertical-cell product, rather than
    // the raw collision-coordinate delta held before the multiply.
    struct ActorCollisionMaskLookupStage {
        std::uint8_t horizontal_cell = 0;
        std::uint8_t vertical_offset = 0;
        std::uint8_t mask_column = 0;
        std::uint8_t mask_row = 0;

        [[nodiscard]] bool operator==(const ActorCollisionMaskLookupStage&) const
            = default;
    };

    // `$0254` and `$00B7`: mutable scan-session inputs sampled before CC1A
    // derives an actor-local mask coordinate.  The native scan returns the
    // prospective flag value with its result; it never mutates this view.
    struct ActorCollisionScanSession {
        bool active = false;
        std::uint8_t side_flags = 0;

        [[nodiscard]] bool operator==(const ActorCollisionScanSession&) const
            = default;
    };

    // CB48 walks a caller-prepared slice of the fixed actor table.  Its
    // cursor/countdown and zero-only gate are distinct from CED2's later
    // retry scan, which always begins at dynamic slot four.  Keep the raw
    // zero-page bytes at this adapter boundary so native collision code can
    // name the normal sweep rather than reuse retry policy.
    struct NormalActorCollisionScanWalk {
        std::uint8_t first_slot = 0;
        std::uint8_t slot_count = 0;
        // CB48 always walks its caller-prepared range. A nonzero `$55` does
        // not close that walk: it selects the alternate subset whose `$3000,Y`
        // activity byte has bit 6 set.
        bool require_activity_bit_6 = false;

        [[nodiscard]] bool operator==(const NormalActorCollisionScanWalk&) const
            = default;
    };

    explicit GameStateView(std::span<const std::byte> work_ram) noexcept;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] std::optional<DynamicActorReservationKind>
    dynamic_actor_reservation_kind() const noexcept;
    [[nodiscard]] std::optional<ActorSlot> actor(
        std::uint8_t slot) const noexcept;
    [[nodiscard]] std::optional<ActorFrameGate> short_actor_frame_gate()
        const noexcept;
    [[nodiscard]] std::optional<ActorPresentationQueueImage>
    actor_presentation_queue_image() const noexcept;
    [[nodiscard]] std::optional<BonusStage4ObjectiveSlot>
    bonus_stage_4_objective(std::uint8_t index) const noexcept;
    [[nodiscard]] std::optional<BonusStage4LinkedTargetSlot>
    bonus_stage_4_linked_target(std::uint8_t slot) const noexcept;
    [[nodiscard]] std::optional<BumperSlot> bumper(
        std::uint8_t slot) const noexcept;
    [[nodiscard]] std::optional<TowerSlot> tower(
        std::uint8_t slot) const noexcept;
    [[nodiscard]] std::optional<TowerInsectSourceSlot> tower_insect_source(
        std::uint8_t slot) const noexcept;
    [[nodiscard]] std::optional<SmallSoldierSourceSlot> small_soldier_source(
        std::uint8_t slot) const noexcept;
    [[nodiscard]] std::optional<CollisionEffectSlot> collision_effect(
        std::uint8_t slot) const noexcept;
    [[nodiscard]] std::optional<GiantSkeletonEyeballFinalAnimationSlot>
    giant_skeleton_eyeball_final_animation(std::uint8_t slot) const noexcept;
    [[nodiscard]] std::optional<DragonEggClusterSlot> dragon_egg_cluster()
        const noexcept;
    [[nodiscard]] std::optional<GiantFaceSlot> giant_face() const noexcept;
    [[nodiscard]] std::optional<GiantFaceHelmetChannelSlot> giant_face_helmet_channel(
        std::uint8_t slot) const noexcept;
    [[nodiscard]] std::optional<BallSlot> ball() const noexcept;
    // `$B9:$BA` and `$BB:$BC` are the signed 8.8 increments DC95 prepared
    // for one F1B4 collision-loop integration. They are transient motion
    // state, distinct from the frame velocity preserved in BallSlot.
    [[nodiscard]] std::optional<BallMotionIncrement> ball_substep_motion()
        const noexcept;
    // `$56` is the number of prepared integrations C6/C7 has consumed and
    // `$57` is the DC95-requested total. Together with `$B9:$BC`, they are
    // the resumable clear suffix of the ball coroutine, not a new frame plan.
    [[nodiscard]] std::optional<BallSubstepProgress> ball_substep_progress()
        const noexcept;
    // `$8C`, `$8D`, and `$8B` are the per-frame table-motion inputs read by
    // DD09 after the collision loop has completed. Keep their cartridge
    // locations at this capture boundary; native physics receives only its
    // named tuning values.
    [[nodiscard]] std::optional<BallMotionTuning> ball_motion_tuning()
        const noexcept;
    [[nodiscard]] std::optional<BallCollisionPositionCache>
    ball_collision_position_cache() const noexcept;
    [[nodiscard]] std::optional<CollisionResponseTableState>
    collision_response_table_state() const noexcept;
    [[nodiscard]] std::optional<CollisionResponseInput>
    collision_response_input() const noexcept;
    [[nodiscard]] std::optional<ActiveScore> active_score() const noexcept;
    [[nodiscard]] std::optional<TableRuleLatchSnapshot>
    table_rule_latches() const noexcept;
    [[nodiscard]] std::optional<PlayfieldCollisionTablePointers>
    playfield_collision_table_pointers() const noexcept;
    [[nodiscard]] std::optional<ActorCollisionShapeDirectoryPointers>
    actor_collision_shape_directory_pointers() const noexcept;
    [[nodiscard]] std::optional<ActorCollisionMaskTablePointers>
    actor_collision_mask_table_pointers() const noexcept;
    [[nodiscard]] std::optional<ActorCollisionMaskLookupStage>
    actor_collision_mask_lookup_stage() const noexcept;
    [[nodiscard]] std::optional<ActorCollisionScanSession>
    actor_collision_scan_session() const noexcept;
    [[nodiscard]] std::optional<NormalActorCollisionScanWalk>
    normal_actor_collision_scan_walk() const noexcept;
    // CB2A refreshes `$65:$67` from the ball before falling through to CB48:
    // horizontal is the ball's whole-pixel X byte less `$20`, while vertical
    // is its whole-pixel Y word. At CB2A entry the scratch bytes can still
    // hold a previous sweep, so this is distinct from the live probe below.
    [[nodiscard]] std::optional<CollisionProbe>
    normal_actor_collision_setup_probe() const noexcept;
    // `$65:$67` is CB48/CC1A's live collision coordinate. It is a staged
    // substep value, so it is intentionally distinct from the persisted ball
    // record used to seed a physics epoch.
    [[nodiscard]] std::optional<CollisionProbe>
    normal_actor_collision_probe() const noexcept;

private:
    [[nodiscard]] std::optional<std::uint8_t> byte_at_offset(
        std::size_t offset) const noexcept;
    [[nodiscard]] std::optional<Fixed8_8> fixed_8_8(
        std::size_t low_offset,
        std::size_t high_offset) const noexcept;
    [[nodiscard]] std::optional<Fixed16_8> fixed_16_8(
        std::size_t fractional_offset,
        std::size_t low_offset,
        std::size_t high_offset) const noexcept;
    [[nodiscard]] std::optional<std::uint16_t> unsigned_word(
        std::size_t low_offset,
        std::size_t high_offset) const noexcept;
    [[nodiscard]] std::optional<std::uint16_t> little_endian_word(
        std::size_t low_offset) const noexcept;

    std::span<const std::byte> work_ram_;
};

} // namespace devils_crush::native_runtime
