// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/native_gameplay_state.hpp"

#include <algorithm>
#include <utility>

namespace devils_crush::native_runtime {

NativeGameplayState::NativeGameplayState() noexcept
    : bumper_scheduler_(score_)
    , bonus_stage_4_scheduler_(score_)
    , bonus_stage_4_linked_target_scheduler_(score_)
    , dragon_egg_scheduler_(score_)
    , dragon_head_scheduler_(score_)
    , giant_face_scheduler_(score_)
    , giant_face_helmet_scheduler_(score_)
    , giant_skeleton_scheduler_(score_)
    , front_spider_scheduler_(score_)
    , side_spider_scheduler_(score_)
    , tower_scheduler_(score_)
    , tower_insect_scheduler_(score_)
    , skeleton_insect_scheduler_(score_)
    , small_soldier_scheduler_(actors_, score_)
{
}

PhysicsState& NativeGameplayState::physics() noexcept
{
    return physics_;
}

const PhysicsState& NativeGameplayState::physics() const noexcept
{
    return physics_;
}

ActorState& NativeGameplayState::actors() noexcept
{
    return actors_;
}

const ActorState& NativeGameplayState::actors() const noexcept
{
    return actors_;
}

ScoreState& NativeGameplayState::score() noexcept
{
    return score_;
}

const ScoreState& NativeGameplayState::score() const noexcept
{
    return score_;
}

TableRuleLatches& NativeGameplayState::table_rule_latches() noexcept
{
    return table_rule_latches_;
}

const TableRuleLatches& NativeGameplayState::table_rule_latches() const noexcept
{
    return table_rule_latches_;
}

void NativeGameplayState::seed_table_rule_latches(
    const TableRuleLatches latches) noexcept
{
    table_rule_latches_ = latches;
}

std::vector<GameplayAudioEvent> NativeGameplayState::take_audio_events()
{
    auto events = std::move(pending_audio_events_);
    pending_audio_events_.clear();
    return events;
}

void NativeGameplayState::seed_ball(const BallState ball) noexcept
{
    physics_.ball = ball;
    physics_.contact = {};
    pending_playfield_contact_.reset();
    pending_playfield_collision_loop_.reset();
    pending_collision_retry_full_scan_.reset();
    pending_collision_retry_placement_.reset();
    pending_collision_retry_rearm_ = false;
}

void NativeGameplayState::seed_score(const ScoreState score) noexcept
{
    score_ = score;
}

bool NativeGameplayState::seed_bumper(const BumperState state) noexcept
{
    return bumper_scheduler_.seed(state);
}

const BumperState* NativeGameplayState::bumper(const std::uint8_t side) const noexcept
{
    return bumper_scheduler_.bumper(side);
}

bool NativeGameplayState::seed_front_spider(
    const FrontSpiderState state) noexcept
{
    return front_spider_scheduler_.seed(state);
}

const FrontSpiderState* NativeGameplayState::front_spider(
    const std::uint8_t side) const noexcept
{
    return front_spider_scheduler_.spider(side);
}

bool NativeGameplayState::seed_side_spider(const SideSpiderState state) noexcept
{
    return side_spider_scheduler_.seed(state);
}

const SideSpiderState* NativeGameplayState::side_spider(
    const std::uint8_t side) const noexcept
{
    return side_spider_scheduler_.spider(side);
}

bool NativeGameplayState::seed_skeleton_insect(
    const SkeletonInsectState state) noexcept
{
    return skeleton_insect_scheduler_.seed(state);
}

const SkeletonInsectState* NativeGameplayState::skeleton_insect(
    const SkeletonInsectKind kind) const noexcept
{
    return skeleton_insect_scheduler_.insect(kind);
}

void NativeGameplayState::seed_dragon_head(const DragonHeadState state) noexcept
{
    dragon_head_scheduler_.seed(state);
}

const DragonHeadState& NativeGameplayState::dragon_head() const noexcept
{
    return dragon_head_scheduler_.dragon();
}

void NativeGameplayState::seed_giant_face(const GiantFaceState state) noexcept
{
    giant_face_scheduler_.seed(state);
}

void NativeGameplayState::seed_giant_face(
    const GiantFaceState state,
    const GiantFaceControllerState controller,
    const GiantFaceAnimatorState animator) noexcept
{
    giant_face_scheduler_.seed(state, controller, animator);
}

const GiantFaceState& NativeGameplayState::giant_face() const noexcept
{
    return giant_face_scheduler_.giant_face();
}

const GiantFaceControllerState& NativeGameplayState::giant_face_controller() const noexcept
{
    return giant_face_scheduler_.controller();
}

const GiantFaceAnimatorState& NativeGameplayState::giant_face_animator() const noexcept
{
    return giant_face_scheduler_.animator();
}

void NativeGameplayState::seed_giant_face_helmet(
    const GiantFaceHelmetState state) noexcept
{
    giant_face_helmet_scheduler_.seed(state);
}

const GiantFaceHelmetState* NativeGameplayState::giant_face_helmet() const noexcept
{
    return giant_face_helmet_scheduler_.helmet();
}

void NativeGameplayState::seed_dragon_egg_cluster(
    const DragonEggClusterState state) noexcept
{
    dragon_egg_scheduler_.seed(state);
}

const DragonEggClusterState*
NativeGameplayState::dragon_egg_cluster() const noexcept
{
    return dragon_egg_scheduler_.cluster();
}

void NativeGameplayState::seed_giant_skeleton(
    const GiantSkeletonState state) noexcept
{
    giant_skeleton_scheduler_.seed(state);
}

const GiantSkeletonState* NativeGameplayState::giant_skeleton() const noexcept
{
    return giant_skeleton_scheduler_.giant_skeleton();
}

void NativeGameplayState::seed_giant_skeleton_eyeball(
    const GiantSkeletonEyeballState state) noexcept
{
    giant_skeleton_eyeball_scheduler_.seed(state);
}

const GiantSkeletonEyeballState*
NativeGameplayState::giant_skeleton_eyeball() const noexcept
{
    return giant_skeleton_eyeball_scheduler_.eyeball();
}

void NativeGameplayState::seed_tower(const TowerState state) noexcept
{
    tower_scheduler_.seed(state);
    tower_guard_seeded_ = false;
}

const TowerState& NativeGameplayState::tower() const noexcept
{
    return tower_scheduler_.tower();
}

void NativeGameplayState::seed_tower_guard() noexcept
{
    tower_guard_seeded_ = true;
}

std::optional<TowerGuardSelectorStep>
NativeGameplayState::advance_tower_guard_actor(const std::uint8_t slot) const noexcept
{
    if (!tower_guard_seeded_ || slot != kTowerGuardSlot) {
        return std::nullopt;
    }
    return select_tower_guard_selector(tower_scheduler_.tower());
}

std::optional<PhysicsPlayfieldFrame> NativeGameplayState::advance_physics(
    const BallMotionTuning& tuning,
    const CollisionMap& map) noexcept
{
    if (pending_playfield_contact_) {
        return std::nullopt;
    }

    const auto frame = advance_physics_with_playfield_scan(physics_, tuning, map);
    if (frame && frame->no_contact) {
        physics_ = frame->no_contact->state;
    }
    if (frame && frame->contact) {
        pending_playfield_contact_ = frame->contact;
        pending_playfield_collision_loop_ = frame->collision_loop;
    }
    return frame;
}

std::optional<NativeGameplayState::PreparedBallCollisionLoopResume>
NativeGameplayState::resume_prepared_ball_collision_loop(
    const BallSubstepProgress progress,
    const BallMotionTuning& tuning,
    const CollisionMap& map) noexcept
{
    if (pending_playfield_contact_ || pending_collision_retry_full_scan_
        || pending_collision_retry_placement_) {
        return std::nullopt;
    }

    auto loop = begin_prepared_ball_collision_loop(physics_, progress);
    if (!loop) {
        return std::nullopt;
    }

    PreparedBallCollisionLoopResume result;
    for (;;) {
        const auto update = advance_ball_collision_loop(*loop, tuning, map);
        if (!update) {
            return std::nullopt;
        }
        *loop = update->state;
        if (update->disposition
            == BallCollisionLoopDisposition::substep_clear) {
            continue;
        }
        if (update->disposition
            == BallCollisionLoopDisposition::playfield_contact) {
            pending_playfield_contact_ = update->contact;
            pending_playfield_collision_loop_ = *loop;
            result.next_contact = update->contact;
            return result;
        }
        if (update->disposition
            == BallCollisionLoopDisposition::attempt_cap_exhausted) {
            result.attempt_cap_exhausted = true;
            return result;
        }

        physics_ = loop->state;
        pending_playfield_contact_.reset();
        pending_playfield_collision_loop_.reset();
        pending_collision_retry_full_scan_.reset();
        pending_collision_retry_placement_.reset();
        pending_collision_retry_rearm_ = false;
        result.damping = update->damping;
        return result;
    }
}

std::optional<NativeGameplayState::InitialBallMotionHandoff>
NativeGameplayState::resume_initial_ball_motion(
    const BallSubstepProgress progress,
    const BallMotionTuning& tuning,
    const CollisionMap& map) noexcept
{
    const auto physics = resume_prepared_ball_collision_loop(progress, tuning, map);
    if (!physics) {
        return std::nullopt;
    }

    InitialBallMotionHandoff result{.physics = *physics};
    if (!physics->completes_frame()) {
        return result;
    }

    // C6F2 splits the DD09-adjusted velocity before C708 compares the
    // whole-pixel X word.  Splitting does not alter position, but retaining
    // this sequence prevents a future scheduler adapter from deciding the
    // gate from a stale in-flight cursor.
    result.next_substeps = split_ball_motion_substeps(physics_.ball);
    result.next_substep_progress = {
        .increment = {
            .horizontal = result.next_substeps->velocity_x_per_step,
            .vertical = result.next_substeps->velocity_y_per_step,
        },
        .requested_steps = result.next_substeps->count,
    };
    result.transition = classify_ball_initial_transition(physics_.ball);
    return result;
}

NativeGameplayState::ScheduledBallMotionResume
NativeGameplayState::resume_scheduled_ball_motion(
    const BallFrameSchedule& schedule,
    const BallMotionTuning& tuning,
    const CollisionMap& map) noexcept
{
    if (schedule.owner != BallFrameOwner::normal_motion
        && schedule.owner != BallFrameOwner::initial_substep_transition) {
        return {.disposition = ScheduledBallMotionDisposition::deferred};
    }
    if (!schedule.substep_progress) {
        return {.disposition = ScheduledBallMotionDisposition::unavailable};
    }

    if (schedule.owner == BallFrameOwner::initial_substep_transition) {
        const auto initial = resume_initial_ball_motion(
            *schedule.substep_progress, tuning, map);
        if (!initial) {
            return {.disposition = ScheduledBallMotionDisposition::unavailable};
        }
        return {
            .disposition = ScheduledBallMotionDisposition::resumed,
            .initial = initial,
        };
    }

    const auto physics = resume_prepared_ball_collision_loop(
        *schedule.substep_progress, tuning, map);
    if (!physics) {
        return {.disposition = ScheduledBallMotionDisposition::unavailable};
    }
    return {
        .disposition = ScheduledBallMotionDisposition::resumed,
        .physics = physics,
    };
}

const std::optional<PlayfieldContact>&
NativeGameplayState::pending_playfield_contact() const noexcept
{
    return pending_playfield_contact_;
}

const std::optional<BallCollisionLoop>&
NativeGameplayState::pending_playfield_collision_loop() const noexcept
{
    return pending_playfield_collision_loop_;
}

bool NativeGameplayState::commit_playfield_collision(
    const BallState resolved_ball) noexcept
{
    if (!pending_playfield_contact_) {
        return false;
    }

    physics_.ball = resolved_ball;
    pending_playfield_contact_.reset();
    pending_playfield_collision_loop_.reset();
    pending_collision_retry_full_scan_.reset();
    pending_collision_retry_placement_.reset();
    pending_collision_retry_rearm_ = false;
    return true;
}

std::optional<CollisionRetryProbeResult>
NativeGameplayState::retry_pending_playfield_collision(
    const CollisionRetryOffsets& offsets) noexcept
{
    if (!pending_playfield_contact_) {
        return std::nullopt;
    }
    if (pending_collision_retry_placement_) {
        return std::nullopt;
    }
    if (pending_collision_retry_full_scan_) {
        return pending_collision_retry_full_scan_;
    }

    const auto& contact = *pending_playfield_contact_;
    const auto probe = probe_collision_retry(retry_ball_collision(
        contact.impact_ball, contact.entry_ball, offsets));
    if (probe.disposition == CollisionRetryProbeDisposition::clear) {
        physics_.ball = probe.retry.state;
        pending_playfield_contact_.reset();
        pending_playfield_collision_loop_.reset();
        pending_collision_retry_rearm_ = false;
    } else {
        pending_collision_retry_full_scan_ = probe;
    }
    return probe;
}

const std::optional<CollisionRetryProbeResult>&
NativeGameplayState::pending_collision_retry_full_scan() const noexcept
{
    return pending_collision_retry_full_scan_;
}

std::optional<CollisionRetryPlacementUpdate>
NativeGameplayState::begin_pending_collision_retry(
    const CollisionRetryOffsetAttempts& offsets) noexcept
{
    if (!pending_playfield_contact_ || pending_collision_retry_full_scan_
        || pending_collision_retry_placement_
        || (physics_.ball.collision_state & 0x80U) == 0U) {
        return std::nullopt;
    }
    const auto& contact = *pending_playfield_contact_;
    return apply_collision_retry_placement(begin_collision_retry_placement(
        contact.impact_ball, contact.entry_ball, offsets));
}

std::optional<CollisionRetryPlacementUpdate>
NativeGameplayState::resolve_pending_collision_retry_full_scan(
    const CollisionRetryFullScanOutcome outcome) noexcept
{
    if (!pending_collision_retry_full_scan_
        || !pending_collision_retry_placement_) {
        return std::nullopt;
    }
    if (outcome != CollisionRetryFullScanOutcome::unresolved) {
        pending_collision_retry_full_scan_.reset();
    }
    return apply_collision_retry_placement(advance_collision_retry_placement(
        *pending_collision_retry_placement_, outcome));
}

std::optional<NativeGameplayState::CollisionRetryFullScanUpdate>
NativeGameplayState::resolve_pending_collision_retry_full_scan(
    const MappedRomView& rom,
    const GameStateView& state,
    const std::uint8_t parent_vertical_high_byte)
{
    if (!pending_collision_retry_full_scan_
        || !pending_collision_retry_placement_) {
        return std::nullopt;
    }

    const auto scan = scan_collision_retry_full(
        rom,
        state,
        *pending_collision_retry_full_scan_,
        parent_vertical_high_byte);
    const auto outcome = scan
        ? resolve_collision_retry_full_scan_outcome(*scan)
        : CollisionRetryFullScanOutcome::unresolved;
    const auto placement = resolve_pending_collision_retry_full_scan(outcome);
    if (!placement) {
        return std::nullopt;
    }
    return CollisionRetryFullScanUpdate{
        .scan = scan,
        .placement = *placement,
    };
}

std::optional<CollisionRetryPlacementUpdate>
NativeGameplayState::select_pending_collision_retry_direction(
    const std::uint8_t random_byte) noexcept
{
    if (pending_collision_retry_full_scan_
        || !pending_collision_retry_placement_
        || pending_collision_retry_placement_->phase
            != CollisionRetryPlacementPhase::selecting_direction) {
        return std::nullopt;
    }
    return apply_collision_retry_placement(
        select_collision_retry_placement_direction(
            *pending_collision_retry_placement_, random_byte));
}

const std::optional<CollisionRetryPlacementState>&
NativeGameplayState::pending_collision_retry_placement() const noexcept
{
    return pending_collision_retry_placement_;
}

std::optional<CollisionRetryPlacementUpdate>
NativeGameplayState::begin_actor_contact_collision_retry(
    const CollisionRetryBallCapture& capture,
    const CollisionRetryOffsetAttempts& offsets) noexcept
{
    if (pending_playfield_contact_ || pending_collision_retry_full_scan_
        || pending_collision_retry_placement_
        || pending_actor_contact_collision_retry_full_scan_
        || pending_actor_contact_collision_retry_placement_
        || physics_.ball != capture.impact_ball.state) {
        return std::nullopt;
    }
    return apply_actor_contact_collision_retry_placement(
        begin_collision_retry_placement(
            capture.impact_ball.state, capture.previous_position, offsets));
}

std::optional<CollisionRetryPlacementUpdate>
NativeGameplayState::resolve_pending_actor_contact_collision_retry_full_scan(
    const CollisionRetryFullScanOutcome outcome) noexcept
{
    if (!pending_actor_contact_collision_retry_full_scan_
        || !pending_actor_contact_collision_retry_placement_) {
        return std::nullopt;
    }
    if (outcome != CollisionRetryFullScanOutcome::unresolved) {
        pending_actor_contact_collision_retry_full_scan_.reset();
    }
    return apply_actor_contact_collision_retry_placement(
        advance_collision_retry_placement(
            *pending_actor_contact_collision_retry_placement_, outcome));
}

std::optional<NativeGameplayState::CollisionRetryFullScanUpdate>
NativeGameplayState::resolve_pending_actor_contact_collision_retry_full_scan(
    const MappedRomView& rom,
    const GameStateView& state)
{
    if (!pending_actor_contact_collision_retry_full_scan_
        || !pending_actor_contact_collision_retry_placement_) {
        return std::nullopt;
    }
    const auto scan = scan_collision_retry_full(
        rom,
        state,
        *pending_actor_contact_collision_retry_full_scan_,
        static_cast<std::uint8_t>(
            static_cast<std::uint32_t>(
                pending_actor_contact_collision_retry_placement_->impact_ball
                    .position_y.raw)
            >> 16U));
    const auto outcome = scan
        ? resolve_collision_retry_full_scan_outcome(*scan)
        : CollisionRetryFullScanOutcome::unresolved;
    const auto placement =
        resolve_pending_actor_contact_collision_retry_full_scan(outcome);
    if (!placement) {
        return std::nullopt;
    }
    return CollisionRetryFullScanUpdate{
        .scan = scan,
        .placement = *placement,
    };
}

std::optional<CollisionRetryPlacementUpdate>
NativeGameplayState::select_pending_actor_contact_collision_retry_direction(
    const std::uint8_t random_byte) noexcept
{
    if (pending_actor_contact_collision_retry_full_scan_
        || !pending_actor_contact_collision_retry_placement_
        || pending_actor_contact_collision_retry_placement_->phase
            != CollisionRetryPlacementPhase::selecting_direction) {
        return std::nullopt;
    }
    return apply_actor_contact_collision_retry_placement(
        select_collision_retry_placement_direction(
            *pending_actor_contact_collision_retry_placement_, random_byte));
}

const std::optional<CollisionRetryProbeResult>&
NativeGameplayState::pending_actor_contact_collision_retry_full_scan() const noexcept
{
    return pending_actor_contact_collision_retry_full_scan_;
}

const std::optional<CollisionRetryPlacementState>&
NativeGameplayState::pending_actor_contact_collision_retry_placement() const noexcept
{
    return pending_actor_contact_collision_retry_placement_;
}

CollisionRetryPlacementUpdate NativeGameplayState::apply_collision_retry_placement(
    CollisionRetryPlacementUpdate update) noexcept
{
    if (update.disposition == CollisionRetryPlacementDisposition::accepted) {
        // CDF8 restores its source cache before accumulating the chosen
        // offset. Its clear CEB5/CB7E route leaves that accepted candidate in
        // the live ball while clearing the marker, then returns to C6/C7.
        // Retain the collision cursor so the same frame can continue there.
        physics_.ball = update.probe->state;
        physics_.ball.collision_state &= 0x7FU;
        pending_collision_retry_rearm_ = true;
        pending_collision_retry_full_scan_.reset();
        pending_collision_retry_placement_.reset();
        update.disposition = CollisionRetryPlacementDisposition::rearmed;
        return update;
    }

    pending_collision_retry_placement_ = update.state;
    if (update.disposition
        == CollisionRetryPlacementDisposition::probe_ready) {
        const auto retry_probe = probe_collision_retry(*update.probe);
        if (retry_probe.disposition == CollisionRetryProbeDisposition::clear) {
            return apply_collision_retry_placement(
                advance_collision_retry_placement(
                    update.state, CollisionRetryFullScanOutcome::clear));
        }
        pending_collision_retry_full_scan_ = retry_probe;
    }
    return update;
}

CollisionRetryPlacementUpdate
NativeGameplayState::apply_actor_contact_collision_retry_placement(
    CollisionRetryPlacementUpdate update) noexcept
{
    if (update.disposition == CollisionRetryPlacementDisposition::accepted) {
        physics_.ball = update.probe->state;
        pending_actor_contact_collision_retry_full_scan_.reset();
        pending_actor_contact_collision_retry_placement_.reset();
        return update;
    }

    pending_actor_contact_collision_retry_placement_ = update.state;
    if (update.disposition
        == CollisionRetryPlacementDisposition::probe_ready) {
        const auto retry_probe = probe_collision_retry(*update.probe);
        if (retry_probe.disposition == CollisionRetryProbeDisposition::clear) {
            return apply_actor_contact_collision_retry_placement(
                advance_collision_retry_placement(
                    update.state, CollisionRetryFullScanOutcome::clear));
        }
        pending_actor_contact_collision_retry_full_scan_ = retry_probe;
    }
    return update;
}

NativeGameplayState::ActorContactUpdate
NativeGameplayState::confirm_actor_contact(const ContactTarget target) noexcept
{
    const auto contact = dispatch_contact(physics_.contact, target);
    physics_.contact = contact.state;
    const auto bonus_stage_4_orbiting_child =
        bonus_stage_4_linked_target_scheduler_.orbiting_child(contact.target.actor_slot)
        != nullptr;
    const auto bonus_stage_4_linked_target_queued =
        bonus_stage_4_linked_target_scheduler_.queue_confirmed_contact(contact);
    return {
        .contact = contact,
        .bumper_queued = bumper_scheduler_.queue_confirmed_contact(contact),
        .bonus_stage_4_queued = bonus_stage_4_scheduler_.queue_confirmed_contact(
            contact),
        .bonus_stage_4_linked_target_queued = bonus_stage_4_linked_target_queued
            && !bonus_stage_4_orbiting_child,
        .bonus_stage_4_orbiting_child_queued = bonus_stage_4_linked_target_queued
            && bonus_stage_4_orbiting_child,
        .dragon_egg_queued = dragon_egg_scheduler_.queue_confirmed_contact(contact),
        .dragon_head_queued = dragon_head_scheduler_.queue_confirmed_contact(contact),
        .giant_face_queued = giant_face_scheduler_.queue_confirmed_contact(contact),
        .giant_face_helmet_queued = giant_face_helmet_scheduler_.queue_confirmed_contact(
            contact),
        .giant_skeleton_queued = giant_skeleton_scheduler_.queue_confirmed_contact(contact),
        .front_spider_queued = front_spider_scheduler_.queue_confirmed_contact(contact),
        .side_spider_queued = side_spider_scheduler_.queue_confirmed_contact(contact),
        .tower_queued = tower_scheduler_.queue_confirmed_contact(contact),
        .tower_insect_queued = tower_insect_scheduler_.queue_confirmed_contact(contact),
        .small_soldier_queued = small_soldier_scheduler_.queue_confirmed_contact(contact),
    };
}

NativeGameplayState::OrientedActorContactUpdate
NativeGameplayState::confirm_oriented_actor_contact(
    const ContactTarget target,
    const CollisionResponseBasis basis,
    const CollisionResponseConfig& config,
    const CollisionNormalReflection reflection) noexcept
{
    auto contact = confirm_actor_contact(target);
    if (contact.contact.disposition != ContactDisposition::dispatched) {
        return {.contact = contact};
    }

    const auto response = apply_oriented_collision_response(
        {
            .horizontal = physics_.ball.velocity_x,
            .vertical = physics_.ball.velocity_y,
        },
        basis,
        config,
        reflection);
    physics_.ball.velocity_x = response.outgoing.horizontal;
    physics_.ball.velocity_y = response.outgoing.vertical;
    return {
        .contact = contact,
        .response = response,
    };
}

std::optional<NativeGameplayState::OrientedActorContactUpdate>
NativeGameplayState::confirm_oriented_actor_contact(
    const ContactTarget target,
    const MappedRomView& rom,
    const GameStateView& state) noexcept
{
    const auto input = state.collision_response_input();
    if (!input || !input->selects_signed_cf33_response()) {
        return std::nullopt;
    }
    const auto basis = decode_collision_response_basis(rom, input->orientation);
    if (!basis) {
        return std::nullopt;
    }
    return confirm_oriented_actor_contact(
        target,
        basis->basis,
        make_collision_response_config(input->orientation, input->table_state));
}

std::optional<NativeGameplayState::OrientedActorContactUpdate>
NativeGameplayState::confirm_cf33_actor_contact(
    const GameStateView::ActorSlot& actor,
    const MappedRomView& rom,
    const GameStateView& state) noexcept
{
    return resolve_cf33_actor_contact(actor, rom, state).contact;
}

NativeGameplayState::Cf33ActorContactResolution
NativeGameplayState::resolve_cf33_actor_contact(
    const GameStateView::ActorSlot& actor,
    const MappedRomView& rom,
    const GameStateView& state) noexcept
{
    const auto target = make_confirmed_contact_target(actor);
    const auto input = state.collision_response_input();
    if (!target || target->response_kind != 2U || !input) {
        return {};
    }
    if (input->selects_signed_cf33_response()) {
        if ((actor.collision_state & 0x80U) != 0U) {
            const auto retry_ball = capture_collision_retry_ball(state);
            if (!retry_ball) {
                return {
                    .disposition = Cf33ActorContactDisposition::unavailable,
                };
            }
            return {
                .disposition = Cf33ActorContactDisposition::position_retry_ready,
                .retry_ball = *retry_ball,
            };
        }
        const auto contact = confirm_oriented_actor_contact(*target, rom, state);
        if (!contact) {
            return {.disposition = Cf33ActorContactDisposition::unavailable};
        }
        return {
            .disposition = contact->response
                ? Cf33ActorContactDisposition::response_applied
                : Cf33ActorContactDisposition::contact_not_dispatched,
            .contact = *contact,
        };
    }

    const auto basis = decode_collision_response_basis(rom, input->orientation);
    if (!basis) {
        return {.disposition = Cf33ActorContactDisposition::unavailable};
    }
    const auto config = make_collision_response_config(
        input->orientation, input->table_state);
    const auto response = apply_oriented_collision_response(
        {
            .horizontal = physics_.ball.velocity_x,
            .vertical = physics_.ball.velocity_y,
        },
        basis->basis,
        config,
        CollisionNormalReflection::after_response);
    if (response.local.vertical < 0) {
        return {.disposition = Cf33ActorContactDisposition::separating};
    }
    const auto contact = confirm_oriented_actor_contact(
        *target,
        basis->basis,
        config,
        CollisionNormalReflection::after_response);
    return {
        .disposition = contact.response
            ? Cf33ActorContactDisposition::response_applied
            : Cf33ActorContactDisposition::contact_not_dispatched,
        .contact = contact,
    };
}

NativeGameplayState::Cf33PlayfieldContactResolution
NativeGameplayState::resolve_cf33_playfield_contact(
    const MappedRomView& rom,
    const GameStateView& state) const noexcept
{
    if (!pending_playfield_contact_) {
        return {};
    }
    const auto input = state.collision_response_input();
    if (!input) {
        return {.disposition = Cf33PlayfieldContactDisposition::unavailable};
    }

    const auto& impact_ball = pending_playfield_contact_->impact_ball;
    // CF33's signed leg does not unconditionally enter CDF8. The ball's
    // actor-local marker distinguishes that placement retry from CF3F's
    // ordinary signed reflected response, just as an actor contact does.
    if (input->selects_signed_cf33_response()
        && (physics_.ball.collision_state & 0x80U) != 0U) {
        return {
            .disposition =
                Cf33PlayfieldContactDisposition::signed_retry_required,
            .resolved_ball = impact_ball,
        };
    }

    const auto basis = decode_collision_response_basis(rom, input->orientation);
    if (!basis) {
        return {.disposition = Cf33PlayfieldContactDisposition::unavailable};
    }
    const auto response = apply_oriented_collision_response(
        {
            .horizontal = impact_ball.velocity_x,
            .vertical = impact_ball.velocity_y,
        },
        basis->basis,
        make_collision_response_config(input->orientation, input->table_state),
        CollisionNormalReflection::after_response);
    if (response.local.vertical < 0) {
        return {
            .disposition = Cf33PlayfieldContactDisposition::separating,
            .resolved_ball = impact_ball,
            .response = response,
        };
    }

    auto resolved_ball = impact_ball;
    resolved_ball.velocity_x = response.outgoing.horizontal;
    resolved_ball.velocity_y = response.outgoing.vertical;
    return {
        .disposition = Cf33PlayfieldContactDisposition::response_ready,
        .resolved_ball = resolved_ball,
        .response = response,
    };
}

std::optional<NativeGameplayState::Cf33PlayfieldLoopResume>
NativeGameplayState::resume_cf33_playfield_contact(
    const MappedRomView& rom,
    const GameStateView& state,
    const BallMotionTuning& tuning,
    const CollisionMap& map) noexcept
{
    if (!pending_playfield_contact_ || !pending_playfield_collision_loop_
        || pending_collision_retry_full_scan_
        || pending_collision_retry_placement_) {
        return std::nullopt;
    }

    Cf33PlayfieldLoopResume result{
        .response = resolve_cf33_playfield_contact(rom, state),
    };
    if (result.response.disposition
        == Cf33PlayfieldContactDisposition::signed_retry_required
        || result.response.disposition
            == Cf33PlayfieldContactDisposition::unavailable
        || result.response.disposition
            == Cf33PlayfieldContactDisposition::rejected) {
        return result;
    }

    auto loop = result.response.disposition
            == Cf33PlayfieldContactDisposition::separating
        ? retry_ball_collision_substep(*pending_playfield_collision_loop_)
        : resume_ball_collision_after_response(
            *pending_playfield_collision_loop_, result.response.resolved_ball);
    for (;;) {
        const auto update = advance_ball_collision_loop(loop, tuning, map);
        if (!update) {
            return std::nullopt;
        }
        loop = update->state;
        if (update->disposition
            == BallCollisionLoopDisposition::substep_clear) {
            continue;
        }
        if (update->disposition
            == BallCollisionLoopDisposition::playfield_contact) {
            pending_playfield_contact_ = update->contact;
            pending_playfield_collision_loop_ = loop;
            result.next_contact = update->contact;
            return result;
        }
        if (update->disposition
            == BallCollisionLoopDisposition::attempt_cap_exhausted) {
            result.attempt_cap_exhausted = true;
            return result;
        }

        physics_ = loop.state;
        pending_playfield_contact_.reset();
        pending_playfield_collision_loop_.reset();
        pending_collision_retry_full_scan_.reset();
        pending_collision_retry_placement_.reset();
        pending_collision_retry_rearm_ = false;
        result.damping = update->damping;
        return result;
    }
}

std::optional<NativeGameplayState::CollisionRetryPlacementLoopResume>
NativeGameplayState::resume_rearmed_collision_retry(
    const BallMotionTuning& tuning,
    const CollisionMap& map) noexcept
{
    if (!pending_collision_retry_rearm_ || !pending_playfield_contact_
        || !pending_playfield_collision_loop_ || pending_collision_retry_full_scan_
        || pending_collision_retry_placement_) {
        return std::nullopt;
    }

    CollisionRetryPlacementLoopResume result;
    auto loop = retry_ball_collision_substep(*pending_playfield_collision_loop_);
    // CDF8 has restored its source cache and applied the accepted placement to
    // `physics_.ball`; CB8A has cleared the signed marker. The preserved
    // cursor still names the same unconsumed C6/C7 substep, so only its active
    // ball needs replacing.
    loop.state.ball = physics_.ball;
    for (;;) {
        const auto update = advance_ball_collision_loop(loop, tuning, map);
        if (!update) {
            return std::nullopt;
        }
        loop = update->state;
        if (update->disposition
            == BallCollisionLoopDisposition::substep_clear) {
            continue;
        }
        if (update->disposition
            == BallCollisionLoopDisposition::playfield_contact) {
            pending_playfield_contact_ = update->contact;
            pending_playfield_collision_loop_ = loop;
            pending_collision_retry_rearm_ = false;
            result.next_contact = update->contact;
            return result;
        }
        if (update->disposition
            == BallCollisionLoopDisposition::attempt_cap_exhausted) {
            pending_playfield_collision_loop_ = loop;
            pending_collision_retry_rearm_ = false;
            result.attempt_cap_exhausted = true;
            return result;
        }

        physics_ = loop.state;
        pending_playfield_contact_.reset();
        pending_playfield_collision_loop_.reset();
        pending_collision_retry_full_scan_.reset();
        pending_collision_retry_placement_.reset();
        pending_collision_retry_rearm_ = false;
        result.damping = update->damping;
        return result;
    }
}

std::optional<BumperRulesUpdate>
NativeGameplayState::advance_bumper_actor(const std::uint8_t slot) noexcept
{
    const auto update = bumper_scheduler_.advance_actor(slot);
    if (update) {
        record_audio_event(slot, GameplayAudioSource::bumper,
            update->bumper.audio_command);
    }
    return update;
}

std::optional<DragonHeadRulesUpdate>
NativeGameplayState::advance_dragon_head_actor(const std::uint8_t slot) noexcept
{
    const auto update = dragon_head_scheduler_.advance_actor(slot);
    if (update) {
        record_audio_event(slot, GameplayAudioSource::dragon_head,
            update->dragon.audio_command);
    }
    return update;
}

bool NativeGameplayState::complete_dragon_head_hit_presentation(
    const std::uint8_t slot) noexcept
{
    return dragon_head_scheduler_.complete_hit_presentation(slot);
}

std::optional<GiantFaceRulesUpdate>
NativeGameplayState::advance_giant_face_actor(const std::uint8_t slot) noexcept
{
    const auto update = giant_face_scheduler_.advance_actor(slot);
    if (update) {
        record_audio_event(slot, GameplayAudioSource::giant_face,
            update->giant_face.audio_command);
        if (update->giant_face.places_ball) {
            physics_.ball.position_x.raw = static_cast<std::int32_t>(
                update->giant_face.ball_horizontal_pixels) * 256;
            physics_.ball.position_y.raw = static_cast<std::int32_t>(
                update->giant_face.ball_vertical_pixels) * 256;
        }
    }
    return update;
}

bool NativeGameplayState::complete_giant_face_impact_presentation(
    const std::uint8_t slot) noexcept
{
    return giant_face_scheduler_.complete_impact_presentation(slot);
}

bool NativeGameplayState::complete_giant_face_mouth_opening() noexcept
{
    return giant_face_scheduler_.complete_mouth_opening();
}

bool NativeGameplayState::begin_giant_face_animator(
    const GiantFaceAnimatorMode mode) noexcept
{
    return giant_face_scheduler_.begin_animator(mode);
}

bool NativeGameplayState::request_giant_face_transformation() noexcept
{
    return giant_face_scheduler_.request_transformation();
}

std::optional<GiantFaceControllerStep>
NativeGameplayState::advance_giant_face_controller_actor(
    const std::uint8_t slot) noexcept
{
    return giant_face_scheduler_.advance_controller_actor(slot);
}

std::optional<GiantFaceAnimatorStep>
NativeGameplayState::advance_giant_face_animator_actor(
    const std::uint8_t slot) noexcept
{
    return giant_face_scheduler_.advance_animator_actor(slot);
}

std::optional<GiantFaceHelmetRulesUpdate>
NativeGameplayState::advance_giant_face_helmet_actor(const std::uint8_t slot) noexcept
{
    const auto update = giant_face_helmet_scheduler_.advance_actor(slot);
    if (update) {
        if (update->helmet.requests_face_wake) {
            static_cast<void>(giant_face_scheduler_.request_wake());
        }
        record_audio_event(slot, GameplayAudioSource::giant_face_helmet,
            update->helmet.audio_command);
    }
    return update;
}

std::optional<GiantFaceHelmetCompletionStep>
NativeGameplayState::complete_giant_face_helmet_activation_presentation(
    const std::uint8_t slot) noexcept
{
    const auto completion = giant_face_helmet_scheduler_.complete_activation_presentation(slot);
    if (completion) {
        record_audio_event(slot, GameplayAudioSource::giant_face_helmet,
            completion->audio_command);
    }
    return completion;
}

bool NativeGameplayState::complete_giant_face_helmet_ball_return(
    const std::uint8_t slot) noexcept
{
    return giant_face_helmet_scheduler_.complete_ball_return(slot);
}

std::optional<DragonEggRulesUpdate>
NativeGameplayState::advance_dragon_egg_actor(const std::uint8_t slot) noexcept
{
    const auto update = dragon_egg_scheduler_.advance_egg_actor(slot);
    if (update) {
        record_audio_event(slot, GameplayAudioSource::dragon_egg,
            update->egg.audio_command);
    }
    return update;
}

std::optional<DragonEggCompletionRulesUpdate>
NativeGameplayState::advance_dragon_egg_controller_actor(
    const std::uint8_t slot) noexcept
{
    return dragon_egg_scheduler_.advance_controller_actor(slot);
}

std::optional<DragonEggGroupResetStep>
NativeGameplayState::begin_dragon_egg_group_reset() noexcept
{
    return dragon_egg_scheduler_.begin_group_reset();
}

std::optional<DragonEggFixedResetStep>
NativeGameplayState::advance_dragon_egg_fixed_reset_actor(
    const std::uint8_t slot) noexcept
{
    return dragon_egg_scheduler_.advance_fixed_reset_actor(slot);
}

std::optional<DragonEggHatchlingAllocationStep>
NativeGameplayState::allocate_dragon_egg_hatchling(
    const std::uint8_t source_egg_slot,
    const std::uint8_t dynamic_slot) noexcept
{
    if (!actors_.claim_dynamic_slot(
            dynamic_slot, ActorBehavior::dragon_egg_hatchling)) {
        return std::nullopt;
    }
    const auto allocation = dragon_egg_scheduler_.allocate_hatchling(
        source_egg_slot, dynamic_slot);
    if (!allocation || !allocation->hatchling) {
        actors_.release(dynamic_slot);
        return std::nullopt;
    }
    auto* actor = actors_.actor(dynamic_slot);
    actor->due_for_update = true;
    actor->motion.position_x.raw = static_cast<std::int32_t>(
        allocation->hatchling->position.horizontal_pixels) * 256;
    actor->motion.position_y.raw = static_cast<std::int32_t>(
        allocation->hatchling->position.vertical_pixels) * 256;
    return allocation;
}

bool NativeGameplayState::seed_dragon_egg_hatchling(
    const DragonEggHatchlingState hatchling) noexcept
{
    if (!actors_.claim_dynamic_slot(
            hatchling.dynamic_slot, ActorBehavior::dragon_egg_hatchling)) {
        return false;
    }
    if (!dragon_egg_scheduler_.seed_hatchling(hatchling)) {
        actors_.release(hatchling.dynamic_slot);
        return false;
    }
    auto* actor = actors_.actor(hatchling.dynamic_slot);
    actor->due_for_update = true;
    actor->motion.position_x.raw = static_cast<std::int32_t>(
        hatchling.position.horizontal_pixels) * 256;
    actor->motion.position_y.raw = static_cast<std::int32_t>(
        hatchling.position.vertical_pixels) * 256;
    return true;
}

const DragonEggHatchlingState* NativeGameplayState::dragon_egg_hatchling(
    const std::uint8_t dynamic_slot) const noexcept
{
    return dragon_egg_scheduler_.hatchling(dynamic_slot);
}

std::optional<DragonEggHatchlingRulesUpdate>
NativeGameplayState::advance_dragon_egg_hatchling_actor(
    const std::uint8_t dynamic_slot) noexcept
{
    const auto update = dragon_egg_scheduler_.advance_hatchling_actor(dynamic_slot);
    if (!update) {
        return std::nullopt;
    }
    record_audio_event(dynamic_slot, GameplayAudioSource::dragon_egg_hatchling,
        update->hatchling.audio_command);
    if (update->hatchling.releases_child) {
        actors_.release(dynamic_slot);
    }
    return update;
}

std::optional<GiantSkeletonRulesUpdate>
NativeGameplayState::advance_giant_skeleton_actor(
    const std::uint8_t slot,
    const GiantSkeletonTargetPosition target) noexcept
{
    const auto update = giant_skeleton_scheduler_.advance_actor(slot, target);
    if (update) {
        record_audio_event(slot, GameplayAudioSource::giant_skeleton,
            update->giant_skeleton.audio_command);
    }
    return update;
}

std::optional<GiantSkeletonEyeballStep>
NativeGameplayState::advance_giant_skeleton_eyeball_actor(
    const std::uint8_t slot) noexcept
{
    const auto* eggs = dragon_egg_scheduler_.cluster();
    return giant_skeleton_eyeball_scheduler_.advance_actor(
        slot, eggs != nullptr && eggs->completed);
}

std::optional<GiantSkeletonEyeballOpeningStartStep>
NativeGameplayState::begin_giant_skeleton_eyeball_opening_countdown() noexcept
{
    return giant_skeleton_eyeball_scheduler_.begin_opening_countdown();
}

std::optional<GiantSkeletonEyeballFinalStartStep>
NativeGameplayState::begin_giant_skeleton_eyeball_final_animation(
    const bool extended_wait) noexcept
{
    return giant_skeleton_eyeball_scheduler_.begin_final_animation(extended_wait);
}

std::optional<GiantSkeletonEyeballFinalResetStep>
NativeGameplayState::complete_giant_skeleton_eyeball_final_reset() noexcept
{
    return giant_skeleton_eyeball_scheduler_.complete_final_reset();
}

std::optional<FrontSpiderRulesUpdate>
NativeGameplayState::advance_front_spider_actor(const std::uint8_t slot) noexcept
{
    const auto update = front_spider_scheduler_.advance_actor(slot);
    if (update) {
        record_audio_event(slot, GameplayAudioSource::front_spider,
            update->spider.audio_command);
    }
    return update;
}

bool NativeGameplayState::complete_front_spider_transition(
    const std::uint8_t slot) noexcept
{
    return front_spider_scheduler_.complete_transition(slot);
}

std::optional<SideSpiderRulesUpdate>
NativeGameplayState::advance_side_spider_actor(const std::uint8_t slot) noexcept
{
    const auto update = side_spider_scheduler_.advance_actor(slot);
    if (update) {
        record_audio_event(slot, GameplayAudioSource::side_spider,
            update->spider.audio_command);
    }
    return update;
}

bool NativeGameplayState::complete_side_spider_transition(
    const std::uint8_t slot) noexcept
{
    return side_spider_scheduler_.complete_transition(slot);
}

bool NativeGameplayState::complete_skeleton_insect_transition(
    const std::uint8_t slot) noexcept
{
    return skeleton_insect_scheduler_.complete_transition(slot);
}

std::optional<TowerPresentationStep>
NativeGameplayState::advance_tower_presentation(const std::uint8_t slot) noexcept
{
    return tower_scheduler_.advance_presentation(slot);
}

std::optional<TowerBonusStageSelectionStep>
NativeGameplayState::advance_tower_bonus_stage_selection(
    const std::uint8_t slot,
    const bool negative_collision_latched) noexcept
{
    const auto update = tower_scheduler_.advance_bonus_stage_selection(
        slot, negative_collision_latched);
    if (update) {
        record_audio_event(slot, GameplayAudioSource::tower_bonus_stage_selection,
            update->audio_command);
    }
    return update;
}

std::optional<TowerInsectSpawnStep>
NativeGameplayState::spawn_tower_insect_child(const std::uint8_t slot) noexcept
{
    if (!actors_.claim_dynamic_slot(slot, ActorBehavior::tower_insect)) {
        return std::nullopt;
    }
    const auto step = tower_insect_scheduler_.spawn_child(slot);
    if (!step) {
        actors_.release(slot);
    } else if (const auto* insect = tower_insect_scheduler_.insect(slot)) {
        static_cast<void>(actors_.set_actor_appearance(slot, insect->appearance));
        static_cast<void>(actors_.set_collision_effect_association(
            slot, insect->collision_effect_association));
    }
    return step;
}

std::optional<TowerInsectSourceStep>
NativeGameplayState::begin_tower_insect_generation(
    const bool tower_insect_cycle_requested) noexcept
{
    return tower_insect_scheduler_.begin_generation(
        tower_insect_cycle_requested);
}

bool NativeGameplayState::seed_tower_insect_generation(
    const TowerInsectGenerationState generation) noexcept
{
    for (std::size_t index = 0U; index < generation.insects.size(); ++index) {
        if (!generation.insects[index]) {
            continue;
        }
        const auto* actor = actors_.actor(kTowerInsectObservedSlots[index]);
        if (actor == nullptr || actor->active()) {
            return false;
        }
    }
    if (!tower_insect_scheduler_.seed(generation)) {
        return false;
    }
    for (std::size_t index = 0U; index < generation.insects.size(); ++index) {
        if (generation.insects[index]) {
            const auto slot = kTowerInsectObservedSlots[index];
            static_cast<void>(actors_.claim_dynamic_slot(slot, ActorBehavior::tower_insect));
            static_cast<void>(actors_.set_actor_appearance(
                slot, generation.insects[index]->appearance));
            static_cast<void>(actors_.set_collision_effect_association(
                slot, generation.insects[index]->collision_effect_association));
        }
    }
    return true;
}

std::optional<TowerInsectRulesUpdate>
NativeGameplayState::advance_tower_insect_actor(const std::uint8_t slot) noexcept
{
    const auto update = tower_insect_scheduler_.advance_actor(slot);
    if (update) {
        record_audio_event(slot, GameplayAudioSource::tower_insect,
            update->insect.audio_command);
        if (update->insect.releases_child) {
            actors_.release(slot);
        }
    }
    return update;
}

std::optional<NativeGameplayState::TowerInsectContactWithEffectUpdate>
NativeGameplayState::advance_tower_insect_actor_with_collision_effect(
    const std::uint8_t slot,
    const std::uint8_t effect_slot) noexcept
{
    const auto rules = tower_insect_scheduler_.advance_actor(slot);
    if (!rules) {
        return std::nullopt;
    }

    record_audio_event(slot, GameplayAudioSource::tower_insect,
        rules->insect.audio_command);

    std::optional<ActorCollisionEffectAllocation> collision_effect;
    if (rules->insect.requests_collision_effect) {
        collision_effect = actors_.claim_collision_effect(slot, effect_slot);
    }
    if (rules->insect.releases_child) {
        actors_.release(slot);
    }
    return TowerInsectContactWithEffectUpdate{
        .rules = *rules,
        .collision_effect = collision_effect,
    };
}

std::optional<TowerInsectSourceStep>
NativeGameplayState::advance_tower_insect_source(const std::uint8_t slot) noexcept
{
    return tower_insect_scheduler_.advance_source(slot);
}

std::optional<TowerInsectSourceStep>
NativeGameplayState::restart_tower_insect_generation(
    const bool tower_insect_cycle_requested) noexcept
{
    return tower_insect_scheduler_.restart_generation(
        tower_insect_cycle_requested);
}

const TowerInsectSourceState& NativeGameplayState::tower_insect_source() const noexcept
{
    return tower_insect_scheduler_.source();
}

const TowerInsectState* NativeGameplayState::tower_insect(
    const std::uint8_t slot) const noexcept
{
    return tower_insect_scheduler_.insect(slot);
}

ActorCollisionEffectAllocation NativeGameplayState::claim_collision_effect(
    const std::uint8_t source_slot,
    const std::uint8_t effect_slot) noexcept
{
    return actors_.claim_collision_effect(source_slot, effect_slot);
}

bool NativeGameplayState::seed_collision_effect(
    const std::uint8_t effect_slot,
    const CollisionEffectState state) noexcept
{
    return actors_.seed_collision_effect(effect_slot, state);
}

bool NativeGameplayState::seed_captured_small_soldier(
    const SmallSoldierActorCapture& capture) noexcept
{
    if (!actors_.claim_dynamic_slot(capture.actor.index,
            ActorBehavior::small_soldier)) {
        return false;
    }
    if (seed_small_soldier_collision_actor(actors_, capture)) {
        return true;
    }
    actors_.release(capture.actor.index);
    return false;
}

bool NativeGameplayState::seed_captured_small_soldier_from_source(
    const SmallSoldierActorCapture& capture) noexcept
{
    if (!seed_captured_small_soldier(capture)) {
        return false;
    }
    if (actors_.register_captured_small_soldier(capture.actor.index)) {
        return true;
    }
    actors_.release(capture.actor.index);
    return false;
}

bool NativeGameplayState::seed_small_soldier_source(
    const SmallSoldierSourceState state) noexcept
{
    return actors_.seed_small_soldier_source(state);
}

std::optional<SmallSoldierSourceStep>
NativeGameplayState::advance_captured_small_soldier_source(
    const SmallSoldierGroupSide side) noexcept
{
    return actors_.advance_captured_small_soldier_source(side);
}

std::optional<SmallSoldierRulesUpdate>
NativeGameplayState::advance_small_soldier_actor(const std::uint8_t slot) noexcept
{
    const auto update = small_soldier_scheduler_.advance_actor(slot);
    if (update) {
        record_audio_event(slot, GameplayAudioSource::small_soldier,
            update->audio_command);
    }
    return update;
}

std::optional<SmallSoldierRulesUpdate>
NativeGameplayState::complete_small_soldier_animation(
    const std::uint8_t slot) noexcept
{
    const auto update = small_soldier_scheduler_.complete_animation(slot);
    if (update) {
        record_audio_event(slot, GameplayAudioSource::small_soldier,
            update->audio_command);
    }
    return update;
}

void NativeGameplayState::seed_bonus_stage_4(const BonusStage4State state) noexcept
{
    bonus_stage_4_scheduler_.seed(state);
}

const BonusStage4State* NativeGameplayState::bonus_stage_4() const noexcept
{
    return bonus_stage_4_scheduler_.stage();
}

std::optional<BonusStage4ObjectiveRulesUpdate>
NativeGameplayState::advance_bonus_stage_4_actor(const std::uint8_t slot) noexcept
{
    const auto update = bonus_stage_4_scheduler_.advance_actor(slot);
    if (update) {
        record_audio_event(slot, GameplayAudioSource::bonus_stage_4,
            update->objective.audio_command);
    }
    return update;
}

bool NativeGameplayState::seed_bonus_stage_4_linked_target(
    const BonusStage4LinkedTargetState target) noexcept
{
    return bonus_stage_4_linked_target_scheduler_.seed(target);
}

bool NativeGameplayState::seed_bonus_stage_4_linked_target(
    const BonusStage4LinkedTargetState target,
    const std::array<std::optional<BonusStage4OrbitingChildState>, 4U> children) noexcept
{
    return bonus_stage_4_linked_target_scheduler_.seed(target, children);
}

const BonusStage4LinkedTargetState*
NativeGameplayState::bonus_stage_4_linked_target() const noexcept
{
    return bonus_stage_4_linked_target_scheduler_.target();
}

const BonusStage4OrbitingChildState*
NativeGameplayState::bonus_stage_4_orbiting_child(const std::uint8_t slot) const noexcept
{
    return bonus_stage_4_linked_target_scheduler_.orbiting_child(slot);
}

std::optional<BonusStage4LinkedTargetRulesUpdate>
NativeGameplayState::advance_bonus_stage_4_linked_target_actor(
    const std::uint8_t slot) noexcept
{
    const auto update = bonus_stage_4_linked_target_scheduler_.advance_actor(slot);
    if (update) {
        record_audio_event(slot, GameplayAudioSource::bonus_stage_4,
            update->target.audio_command);
    }
    return update;
}

std::optional<BonusStage4LinkedTargetMotionStep>
NativeGameplayState::advance_bonus_stage_4_linked_target_motion_actor(
    const std::uint8_t slot) noexcept
{
    return bonus_stage_4_linked_target_scheduler_.advance_motion_actor(slot);
}

std::optional<BonusStage4OrbitingChildFrameUpdate>
NativeGameplayState::advance_bonus_stage_4_orbiting_child_actor(
    const std::uint8_t slot) noexcept
{
    return bonus_stage_4_linked_target_scheduler_.advance_orbiting_child_actor(slot);
}

std::optional<BonusStage4OrbitingChildFrameUpdate>
NativeGameplayState::advance_bonus_stage_4_orbiting_child_orbit_actor(
    const std::uint8_t slot) noexcept
{
    return bonus_stage_4_linked_target_scheduler_.advance_orbiting_child_orbit_actor(slot);
}

std::optional<BonusStage4LinkedTargetHitPresentationStep>
NativeGameplayState::complete_bonus_stage_4_linked_target_hit_presentation() noexcept
{
    return bonus_stage_4_linked_target_scheduler_.complete_hit_presentation();
}

std::optional<BonusStage4OrbitingChildAllocationStep>
NativeGameplayState::allocate_bonus_stage_4_orbiting_child(
    const std::uint8_t dynamic_slot) noexcept
{
    return bonus_stage_4_linked_target_scheduler_.allocate_orbiting_child(dynamic_slot);
}

std::vector<NativeGameplayState::ActorPhaseUpdate>
NativeGameplayState::advance_actor_phase_slots(
    const std::size_t first_slot,
    const std::size_t slot_count,
    const PlayfieldBounds playfield)
{
    std::vector<ActorPhaseUpdate> updates;
    const auto ball_position = GiantSkeletonTargetPosition{
        .horizontal_pixels = static_cast<std::int16_t>(
            physics_.ball.position_x.raw / 256),
        .vertical_pixels = static_cast<std::int16_t>(
            physics_.ball.position_y.raw / 256),
    };
    const auto last_slot = std::min(first_slot + slot_count,
        static_cast<std::size_t>(ActorState::kSlotCount));
    for (auto slot_index = first_slot; slot_index < last_slot; ++slot_index) {
        const auto slot = static_cast<std::uint8_t>(slot_index);
        if (const auto linked_target =
                bonus_stage_4_linked_target_scheduler_.advance_actor(slot)) {
            record_audio_event(slot, GameplayAudioSource::bonus_stage_4,
                linked_target->target.audio_command);
            updates.push_back({
                .slot = slot,
                .kind = ActorPhaseUpdateKind::bonus_stage_4_linked_target,
                .bonus_stage_4_linked_target = linked_target,
            });
            continue;
        }
        if (const auto linked_target_motion =
                bonus_stage_4_linked_target_scheduler_.advance_motion_actor(slot)) {
            updates.push_back({
                .slot = slot,
                .kind = ActorPhaseUpdateKind::bonus_stage_4_linked_target_motion,
                .bonus_stage_4_linked_target_motion = linked_target_motion,
            });
            continue;
        }
        if (const auto orbiting_child =
                bonus_stage_4_linked_target_scheduler_.advance_orbiting_child_actor(slot)) {
            updates.push_back({
                .slot = slot,
                .kind = ActorPhaseUpdateKind::bonus_stage_4_orbiting_child,
                .bonus_stage_4_orbiting_child = orbiting_child,
            });
            continue;
        }
        if (const auto orbiting_child =
                bonus_stage_4_linked_target_scheduler_.advance_orbiting_child_orbit_actor(slot)) {
            updates.push_back({
                .slot = slot,
                .kind = ActorPhaseUpdateKind::bonus_stage_4_orbiting_child_orbit,
                .bonus_stage_4_orbiting_child = orbiting_child,
            });
            continue;
        }
        if (const auto bonus_stage_4 = bonus_stage_4_scheduler_.advance_actor(slot)) {
            record_audio_event(slot, GameplayAudioSource::bonus_stage_4,
                bonus_stage_4->objective.audio_command);
            updates.push_back({
                .slot = slot,
                .kind = ActorPhaseUpdateKind::bonus_stage_4_objective,
                .bonus_stage_4 = bonus_stage_4,
            });
            continue;
        }
        if (const auto dragon_egg_controller =
                dragon_egg_scheduler_.advance_controller_actor(slot)) {
            updates.push_back({
                .slot = slot,
                .kind = ActorPhaseUpdateKind::dragon_egg_controller,
                .dragon_egg_controller = dragon_egg_controller,
            });
            continue;
        }
        if (const auto dragon_egg_fixed_reset =
                dragon_egg_scheduler_.advance_fixed_reset_actor(slot)) {
            updates.push_back({
                .slot = slot,
                .kind = ActorPhaseUpdateKind::dragon_egg_fixed_reset,
                .dragon_egg_fixed_reset = dragon_egg_fixed_reset,
            });
            continue;
        }
        if (const auto dragon_egg = dragon_egg_scheduler_.advance_egg_actor(slot)) {
            record_audio_event(slot, GameplayAudioSource::dragon_egg,
                dragon_egg->egg.audio_command);
            updates.push_back({
                .slot = slot,
                .kind = ActorPhaseUpdateKind::dragon_egg_contact,
                .dragon_egg = dragon_egg,
            });
            continue;
        }
        if (const auto dragon_egg_hatchling =
                dragon_egg_scheduler_.advance_hatchling_actor(slot)) {
            record_audio_event(slot, GameplayAudioSource::dragon_egg_hatchling,
                dragon_egg_hatchling->hatchling.audio_command);
            if (dragon_egg_hatchling->hatchling.releases_child) {
                actors_.release(slot);
            }
            updates.push_back({
                .slot = slot,
                .kind = ActorPhaseUpdateKind::dragon_egg_hatchling,
                .dragon_egg_hatchling = dragon_egg_hatchling,
            });
            continue;
        }
        if (const auto tower_insect_source = tower_insect_scheduler_.advance_source(slot)) {
            updates.push_back({
                .slot = slot,
                .kind = ActorPhaseUpdateKind::tower_insect_source,
                .tower_insect_source = tower_insect_source,
            });
            continue;
        }
        if (const auto bumper = bumper_scheduler_.advance_actor(slot)) {
            record_audio_event(slot, GameplayAudioSource::bumper,
                bumper->bumper.audio_command);
            updates.push_back({
                .slot = slot,
                .kind = ActorPhaseUpdateKind::bumper_contact,
                .bumper = bumper,
            });
            continue;
        }
        if (const auto dragon_head = dragon_head_scheduler_.advance_actor(slot)) {
            record_audio_event(slot, GameplayAudioSource::dragon_head,
                dragon_head->dragon.audio_command);
            updates.push_back({
                .slot = slot,
                .kind = ActorPhaseUpdateKind::dragon_head_contact,
                .dragon_head = dragon_head,
            });
            continue;
        }
        if (const auto giant_face_helmet = advance_giant_face_helmet_actor(slot)) {
            updates.push_back({
                .slot = slot,
                .kind = ActorPhaseUpdateKind::giant_face_helmet,
                .giant_face_helmet = giant_face_helmet,
            });
            continue;
        }
        if (const auto giant_face_controller =
                advance_giant_face_controller_actor(slot)) {
            updates.push_back({
                .slot = slot,
                .kind = ActorPhaseUpdateKind::giant_face_controller,
                .giant_face_controller = giant_face_controller,
            });
            continue;
        }
        if (const auto giant_face_animator =
                advance_giant_face_animator_actor(slot)) {
            updates.push_back({
                .slot = slot,
                .kind = ActorPhaseUpdateKind::giant_face_animator,
                .giant_face_animator = giant_face_animator,
            });
            continue;
        }
        if (const auto giant_face = advance_giant_face_actor(slot)) {
            updates.push_back({
                .slot = slot,
                .kind = ActorPhaseUpdateKind::giant_face_contact,
                .giant_face = giant_face,
            });
            continue;
        }
        if (const auto giant_skeleton = giant_skeleton_scheduler_.advance_actor(
                slot, ball_position)) {
            record_audio_event(slot, GameplayAudioSource::giant_skeleton,
                giant_skeleton->giant_skeleton.audio_command);
            updates.push_back({
                .slot = slot,
                .kind = ActorPhaseUpdateKind::giant_skeleton_update,
                .giant_skeleton = giant_skeleton,
            });
            continue;
        }
        if (const auto giant_skeleton_eyeball =
                advance_giant_skeleton_eyeball_actor(slot)) {
            updates.push_back({
                .slot = slot,
                .kind = ActorPhaseUpdateKind::giant_skeleton_eyeball,
                .giant_skeleton_eyeball = giant_skeleton_eyeball,
            });
            continue;
        }
        if (const auto front_spider = front_spider_scheduler_.advance_actor(slot)) {
            record_audio_event(slot, GameplayAudioSource::front_spider,
                front_spider->spider.audio_command);
            if (front_spider->spider.sets_side_safety) {
                if (front_spider->spider.state.side == 0U) {
                    table_rule_latches_.left_front_spider_safety = true;
                } else {
                    table_rule_latches_.right_front_spider_safety = true;
                }
            }
            updates.push_back({
                .slot = slot,
                .kind = ActorPhaseUpdateKind::front_spider_contact,
                .front_spider = front_spider,
            });
            continue;
        }
        if (const auto side_spider = side_spider_scheduler_.advance_actor(slot)) {
            record_audio_event(slot, GameplayAudioSource::side_spider,
                side_spider->spider.audio_command);
            if (side_spider->spider.sets_shared_transition_latch) {
                table_rule_latches_.side_spider_transition_requested = true;
            }
            updates.push_back({
                .slot = slot,
                .kind = ActorPhaseUpdateKind::side_spider_contact,
                .side_spider = side_spider,
            });
            continue;
        }
        if (const auto tower = tower_scheduler_.advance_actor(
                slot, table_rule_latches_.tower_completed)) {
            record_audio_event(slot, GameplayAudioSource::tower,
                tower->tower.audio_command);
            if (tower->tower.sets_center_insect_source) {
                table_rule_latches_.tower_completed = true;
            }
            updates.push_back({
                .slot = slot,
                .kind = ActorPhaseUpdateKind::tower_contact,
                .tower = tower,
            });
            continue;
        }
        if (const auto tower_guard = advance_tower_guard_actor(slot)) {
            updates.push_back({
                .slot = slot,
                .kind = ActorPhaseUpdateKind::tower_guard_selector,
                .tower_guard = tower_guard,
            });
            continue;
        }
        if (const auto tower_insect = tower_insect_scheduler_.advance_actor(slot)) {
            record_audio_event(slot, GameplayAudioSource::tower_insect,
                tower_insect->insect.audio_command);
            updates.push_back({
                .slot = slot,
                .kind = ActorPhaseUpdateKind::tower_insect_contact,
                .tower_insect = tower_insect,
            });
            continue;
        }
        if (const auto skeleton_insect = skeleton_insect_scheduler_.advance_actor(
                slot, table_rule_latches_)) {
            updates.push_back({
                .slot = slot,
                .kind = ActorPhaseUpdateKind::skeleton_insect_trigger,
                .skeleton_insect = skeleton_insect,
            });
            continue;
        }
        const auto* actor = actors_.actor(slot);
        if (actor == nullptr || !actor->due_for_update) {
            continue;
        }

        if (actor->behavior == ActorBehavior::shared_motion) {
            const auto motion = actors_.advance_due_actor(slot, playfield);
            if (motion) {
                updates.push_back({
                    .slot = slot,
                    .kind = ActorPhaseUpdateKind::shared_motion,
                    .motion = motion,
                });
            }
            continue;
        }

        if (actor->behavior == ActorBehavior::collision_effect) {
            const auto collision_effect = actors_.advance_collision_effect(slot);
            if (collision_effect) {
                updates.push_back({
                    .slot = slot,
                    .kind = ActorPhaseUpdateKind::collision_effect,
                    .collision_effect = collision_effect,
                });
            }
            continue;
        }

        if (actor->behavior == ActorBehavior::small_soldier) {
            const auto small_soldier = small_soldier_scheduler_.advance_actor(slot);
            if (small_soldier) {
                record_audio_event(slot, GameplayAudioSource::small_soldier,
                    small_soldier->audio_command);
                updates.push_back({
                    .slot = slot,
                    .kind = ActorPhaseUpdateKind::small_soldier_contact,
                    .small_soldier = small_soldier,
                });
            }
        }
    }
    return updates;
}

std::optional<NativeGameplayState::ActorPhaseUpdate>
NativeGameplayState::advance_actor_phase_slot(
    const std::uint8_t slot,
    const PlayfieldBounds playfield)
{
    const auto updates = advance_actor_phase_slots(slot, 1U, playfield);
    if (updates.empty()) {
        return std::nullopt;
    }
    return updates.front();
}

std::vector<NativeGameplayState::ActorPhaseUpdate>
NativeGameplayState::advance_actor_phase(const PlayfieldBounds playfield)
{
    return advance_actor_phase_slots(0U,
        static_cast<std::size_t>(ActorState::kSlotCount), playfield);
}

void NativeGameplayState::record_audio_event(
    const std::uint8_t slot,
    const GameplayAudioSource source,
    const std::uint8_t command)
{
    if (command != 0U) {
        pending_audio_events_.push_back({
            .slot = slot,
            .source = source,
            .command = command,
        });
    }
}

} // namespace devils_crush::native_runtime
