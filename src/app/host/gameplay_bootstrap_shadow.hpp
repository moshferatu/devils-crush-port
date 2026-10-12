// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"
#include "command_line.hpp"

namespace devils_crush::app::host {

// A read-only bridge from the live emulator to the human-readable gameplay
// runtime. It establishes that the native runtime can accept real, completed
// frame-boundary state and run one candidate frame without granting it frame
// ownership. Keeping this as a fresh bootstrap for each observation avoids
// carrying emulated state across an as-yet-unported gameplay transition.
struct NativeGameplayBootstrapShadow {
    struct DecodedPlayfieldMap {
        devils_crush::emulation::PlayfieldCollisionSource source;
        devils_crush::native_runtime::CollisionMapAsset map;
    };

    // A clear candidate is advanced entirely by readable native code. Compare
    // its completed ball state against the next emulator frame boundary before
    // considering any transfer of frame ownership. Contacts deliberately do
    // not install a prediction because their response owner is still split.
    struct NativeRuntimeFramePrediction {
        devils_crush::native_runtime::BallState ball;
        // The source scheduler's typed owner for the next boundary. Keep it
        // with a prediction so a frame that never reaches DC95 can be
        // attributed to a real owner rather than mistaken for failed physics.
        devils_crush::native_runtime::BallFrameSchedule schedule;
    };

    struct NativeEpochActorHandlerCount {
        std::uint16_t handler_address = 0U;
        std::uint64_t entries = 0U;
    };

    // This stays separate from the candidate runtime itself: C52C observes
    // source handler dispatches after the candidate's DC95 seed and before the
    // next comparison boundary. It identifies the live actor behaviors that
    // correlate with a persistent-state divergence without giving the shadow
    // any actor ownership.
    struct NativeEpochRuntimeChain {
        devils_crush::native_runtime::NativeGameplayRuntime runtime;
        std::array<NativeEpochActorHandlerCount, 8U> source_actor_handlers{};
        std::uint64_t unclassified_source_actor_handler_entries = 0U;
    };

    struct NativeEpochActorHandlerEvidence {
        std::uint16_t handler_address = 0U;
        std::uint64_t matching_entries = 0U;
        std::uint64_t mismatched_entries = 0U;
    };

    // Facts gathered from the emulator between two frame boundaries. They are
    // not native predictions: they classify why an otherwise-clear readable
    // candidate diverged before the next boundary.
    struct ActualFrameCollisionFacts {
        bool actor_contact = false;
        bool solid_playfield_contact = false;
        std::uint8_t motion_substep_split_entries = 0U;
        std::uint8_t position_integration_entries = 0U;
        std::uint8_t motion_damping_entries = 0U;
    };

    // C52C dispatches one dynamic actor handler, then C520 immediately calls
    // C606 to file that handler's final presentation attributes. This is a
    // narrower, regularly active boundary than C539: it can be verified on
    // the normal C510 walk without claiming the scheduler or a handler.
    struct PendingActorDispatch {
        std::uint64_t entry_absolute_master_cycle = 0U;
        std::uint8_t slot = 0U;
        // Raw C52C gate bytes. A native C510 scan may skip several slots
        // before it hands control back at this dispatch, so these describe
        // the dispatch target rather than necessarily the scan-run entry.
        std::uint8_t activity_flags = 0U;
        std::uint8_t attributes = 0U;
        // True only when this dispatch belongs to the C510 pass currently
        // bracketed by the shadow. Dynamic handlers remain emulator-owned;
        // this bit merely prevents an unrelated C606 call from being folded
        // into that pass's aggregate queue prediction.
        bool belongs_to_main_actor_pass = false;
        // This is the vector selected at C52C entry, before the dynamic
        // handler is allowed to retarget its own actor slot.
        std::uint16_t handler_address = 0U;
        // The named actor-domain state at the C52C boundary. Its eventual
        // same-slot transition is evidence for selecting a behavior family;
        // it does not give this observer permission to update that family.
        devils_crush::native_runtime::GameStateView::ActorSlot actor_before_handler;
    };

    struct PendingActorQueueFiling {
        std::uint8_t slot = 0U;
        std::uint8_t attributes_after_handler = 0U;
        std::uint16_t handler_address = 0U;
        bool belongs_to_main_actor_pass = false;
    };

    // C52C has returned to C520 and C606 is about to file the completed
    // actor. Retain the queue state from this precise entry boundary so the
    // original C606 call can be checked against the readable model after its
    // optional F605 presentation refresh. This is observation-only.
    struct PendingActorQueueFiler {
        PendingActorQueueFiling dispatch;
        devils_crush::native_runtime::ActorPresentationQueueState queues_before;
        std::uint8_t attributes_before_filing = 0U;
    };

    struct PendingMainActorPassDecision {
        std::uint8_t slot = 0U;
        std::uint8_t activity_flags = 0U;
        std::uint8_t attributes = 0U;
        std::optional<std::uint8_t> expected_dispatch_slot;
        std::size_t skipped_actor_count = 0U;
        // The work-RAM write observer records the last instruction to touch
        // either byte while this C510 scan-run entry is live. A C510-to-C52C
        // disagreement is actionable only when the recovered scan target
        // differs from C52C's eventual dispatch target.
        std::optional<RoutineKey> activity_gate_writer;
        std::optional<RoutineKey> attribute_gate_writer;
        bool dispatch_expected = false;
    };

    struct ActorGateWriteEvidence {
        RoutineKey instruction;
        std::uint64_t activity_changes = 0U;
        std::uint64_t attribute_changes = 0U;
    };

    // C510 is the re-entered head of the complete 128-slot main actor scan.
    // Capture its X=0 arrival after C4DE's preamble has established the
    // zeroed presentation queues, then sample every C510 scan-run entry. A
    // native run can skip several slots before it leaves at C51E; C52C's own
    // entry therefore supplies the authoritative dispatch-slot snapshot.
    // This remains observation-only.
    struct PendingMainActorPass {
        std::optional<PendingMainActorPassDecision> pending_decision;
        devils_crush::native_runtime::MainActorPass model;
        std::uint64_t matched_dispatch_decisions = 0U;
        std::uint64_t mismatched_dispatch_decisions = 0U;
        std::uint64_t dispatched_when_skip_expected = 0U;
        std::uint64_t dispatched_for_other_slot = 0U;
        std::uint64_t dispatched_after_inactive_activity_gate = 0U;
        std::uint64_t dispatched_after_deferred_attribute_gate = 0U;
        std::uint64_t dispatched_after_changed_gate_bytes = 0U;
        std::uint64_t dispatched_after_unchanged_gate_bytes = 0U;
        std::uint64_t completed_queue_filings = 0U;
        std::uint64_t queue_chain_mismatches = 0U;
    };

    struct ActorDispatchHandlerEvidence {
        std::uint16_t handler_address = 0U;
        std::uint64_t matching_handoffs = 0U;
        std::uint64_t mismatched_handoffs = 0U;
        std::uint64_t completed_handler_states = 0U;
        std::uint64_t changed_activity_flags = 0U;
        std::uint64_t changed_attributes = 0U;
        std::uint64_t changed_behavior_counter = 0U;
        std::uint64_t changed_handler_address = 0U;
        std::uint64_t changed_motion = 0U;
        std::uint64_t changed_collision_state = 0U;
    };

    void observe_playfield_collision_source(
        const RoutineEntry& entry,
        const std::span<const std::byte> work_ram,
        const std::span<const std::byte> rom) noexcept;

    void observe_short_actor_pass_entry(
        const RoutineEntry& entry,
        const std::span<const std::byte> work_ram) noexcept;

    void observe_short_actor_pass_call_site(const RoutineEntry& entry) noexcept;

    void observe_motion_entry(
        const RoutineEntry& entry,
        const std::span<const std::byte> work_ram,
        const std::span<const std::byte> rom) noexcept;

    void observe_motion_completion(
        const RoutineCall& call,
        const std::span<const std::byte> work_ram) noexcept;

    void observe_main_actor_pass_entry(
        const RoutineEntry& entry,
        const devils_crush::native_runtime::GameStateView& state) noexcept;

    void observe_main_actor_pass_gate_write(
        const ObservedMemoryWrite& write) noexcept;

    void observe_main_actor_pass_completion(
        const RoutineCall& call,
        const std::span<const std::byte> work_ram) noexcept;

    void record_main_actor_pass_gate_writer(
        const std::optional<RoutineKey> writer,
        const bool activity) noexcept;

    void observe_actor_dispatch_entry(
        const RoutineEntry& entry,
        const devils_crush::native_runtime::GameStateView& state) noexcept;

    void observe_actor_dispatch_completion(
        const RoutineCall& call,
        const std::span<const std::byte> work_ram) noexcept;

    void observe_actor_queue_filer_entry(
        const RoutineEntry& entry,
        const devils_crush::native_runtime::GameStateView& state) noexcept;

    void observe_actor_queue_filer_completion(
        const RoutineCall&,
        const std::span<const std::byte> work_ram) noexcept;

    void record_actor_dispatch_handler_handoff(
        const std::uint16_t handler_address,
        const bool matches) noexcept;

    [[nodiscard]] static bool actor_dispatch_handler_evidence_active(
        const ActorDispatchHandlerEvidence& evidence) noexcept;

    void record_actor_dispatch_handler_state_transition(
        const std::uint16_t handler_address,
        const devils_crush::native_runtime::ActorBehaviorStateChanges& changes) noexcept;

    void observe_gameplay_completion(
        const RoutineCall& call,
        const std::span<const std::byte> work_ram) noexcept;

    void observe(
        const std::span<const std::byte> work_ram,
        const InputFrame& input);

    // E970's palette-fade driver calls the native E9B6 interpolator, then
    // advances two packed-colour pointers before it either loops through E976
    // or falls into E994.  This is an observation-only model of that caller
    // tail: the core retains the JSR/RTS and E9B6 retains the interpolation.
    struct PaletteFadeTailPrediction {
        devils_crush::emulation::CpuRegisters registers;
        std::array<std::uint8_t, 4U> pointers{};
        bool loops = false;
    };

    [[nodiscard]] static std::optional<std::uint16_t>
    palette_fade_caller_pc(
        const devils_crush::emulation::RoutineEntry& entry,
        const std::span<const std::byte> work_ram) noexcept;

    [[nodiscard]] static std::uint8_t palette_fade_set_nz(
        const std::uint8_t status,
        const std::uint8_t value) noexcept;

    static void palette_fade_adc(
        devils_crush::emulation::CpuRegisters& registers,
        const std::uint8_t operand) noexcept;

    void observe_palette_fade_block_entry(
        const RoutineEntry& entry,
        const std::span<const std::byte> work_ram) noexcept;

    void observe_palette_fade_block_completion(
        const RoutineCall& call,
        const std::span<const std::byte> work_ram) noexcept;

    void observe_palette_fade_terminal_entry(
        const RoutineEntry& entry,
        const std::span<const std::byte> work_ram) noexcept;

    void report() const;

    std::uint64_t frame_boundaries = 0U;
    std::uint64_t invalid_work_ram = 0U;
    std::uint64_t unavailable_bootstraps = 0U;
    std::uint64_t rejected_bootstraps = 0U;
    std::uint64_t seed_mismatches = 0U;
    std::uint64_t verified_bootstraps = 0U;
    std::uint64_t unavailable_motion_tuning = 0U;
    std::uint64_t waiting_for_playfield_map = 0U;
    std::optional<PaletteFadeTailPrediction> pending_palette_fade_tail;
    std::uint64_t palette_fade_block_entries = 0U;
    std::uint64_t palette_fade_block_completions = 0U;
    std::uint64_t palette_fade_loop_tail_predictions = 0U;
    std::uint64_t matched_palette_fade_loop_tails = 0U;
    std::uint64_t mismatched_palette_fade_loop_tails = 0U;
    std::uint64_t palette_fade_terminal_tail_predictions = 0U;
    std::uint64_t matched_palette_fade_terminal_tails = 0U;
    std::uint64_t mismatched_palette_fade_terminal_tails = 0U;
    std::uint64_t unavailable_palette_fade_tail_predictions = 0U;
    std::uint64_t superseded_palette_fade_tail_predictions = 0U;
    std::uint64_t missed_palette_fade_terminal_tails = 0U;
    std::uint64_t unexpected_palette_fade_terminal_entries = 0U;
    std::uint64_t verified_clear_physics_inputs = 0U;
    std::uint64_t native_runtime_frames = 0U;
    std::uint64_t native_runtime_clear_frames = 0U;
    std::uint64_t native_runtime_contact_frames = 0U;
    std::uint64_t native_runtime_actor_updates = 0U;
    std::uint64_t native_runtime_audio_events = 0U;
    std::uint64_t native_runtime_deferred_schedule_frames = 0U;
    std::uint64_t native_epoch_runtime_candidates = 0U;
    std::uint64_t native_epoch_runtime_clear_candidates = 0U;
    std::uint64_t native_epoch_runtime_contact_candidates = 0U;
    std::uint64_t native_epoch_runtime_seeds = 0U;
    std::uint64_t observed_native_epoch_runtime_boundaries = 0U;
    std::uint64_t matched_native_epoch_runtime_boundaries = 0U;
    std::uint64_t mismatched_native_epoch_runtime_boundaries = 0U;
    std::uint64_t native_epoch_runtime_unavailable = 0U;
    std::array<NativeEpochActorHandlerEvidence, 16U>
        native_epoch_actor_handler_evidence{};
    std::uint64_t unclassified_native_epoch_actor_handler_entries = 0U;
    ActualFrameCollisionFacts actual_frame_collision_facts;
    std::optional<NativeRuntimeFramePrediction>
        pending_native_runtime_frame_prediction;
    std::uint64_t native_runtime_frame_predictions = 0U;
    std::uint64_t observed_native_runtime_frame_predictions = 0U;
    std::uint64_t matched_native_runtime_frame_predictions = 0U;
    std::uint64_t mismatched_native_runtime_frame_predictions = 0U;
    std::uint64_t matched_native_runtime_frame_prediction_schedule_owners = 0U;
    std::uint64_t mismatched_native_runtime_frame_prediction_schedule_owners = 0U;
    std::uint64_t native_runtime_frame_prediction_position_x_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_position_y_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_velocity_x_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_velocity_y_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_motion_flag_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_collision_state_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_actor_contact_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_playfield_contact_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_both_contact_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_no_contact_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_input_active_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_input_neutral_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_split_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_position_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_damping_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_no_split_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_single_split_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_multiple_split_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_no_damping_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_single_damping_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_multiple_damping_mismatches = 0U;
    struct FrameScheduleOwnerCount {
        devils_crush::native_runtime::BallFrameOwner owner =
            devils_crush::native_runtime::BallFrameOwner::unrecognized;
        std::uint64_t mismatches = 0U;
    };
    std::array<FrameScheduleOwnerCount, 4U> deferred_frame_schedule_owners{};
    std::uint64_t unclassified_deferred_frame_schedule_owners = 0U;
    std::array<FrameScheduleOwnerCount, 4U> prediction_schedule_owners{};
    std::uint64_t unclassified_prediction_schedule_owners = 0U;
    std::uint64_t native_initial_motion_candidates = 0U;
    std::uint64_t native_initial_motion_schedules_normal = 0U;
    std::uint64_t native_initial_motion_slot_zero_returns = 0U;
    std::uint64_t native_initial_motion_contacts = 0U;
    std::uint64_t native_initial_motion_attempt_caps = 0U;
    std::uint64_t unavailable_native_initial_motion = 0U;
    std::uint64_t native_initial_motion_failures = 0U;
    struct UnrecognizedScheduleHandlerCount {
        std::uint16_t handler_address = 0U;
        std::uint64_t frames = 0U;
    };
    std::array<UnrecognizedScheduleHandlerCount, 8U>
        unrecognized_schedule_handlers{};
    std::uint64_t unclassified_schedule_handlers = 0U;
    std::uint64_t unavailable_native_runtime_frame_predictions = 0U;
    std::uint64_t unavailable_native_runtime_frames = 0U;
    std::uint64_t native_runtime_frame_failures = 0U;
    std::uint64_t playfield_probe_entries = 0U;
    std::uint64_t unavailable_playfield_sources = 0U;
    std::uint64_t rejected_playfield_maps = 0U;
    std::uint64_t playfield_capture_failures = 0U;
    std::uint64_t reused_playfield_maps = 0U;
    std::uint64_t decoded_playfield_maps = 0U;
    std::uint64_t short_actor_pass_entries = 0U;
    std::uint64_t short_actor_pass_main_loop_call_sites = 0U;
    std::uint64_t short_actor_pass_timer_call_sites = 0U;
    std::uint64_t captured_short_actor_passes = 0U;
    std::uint64_t unavailable_short_actor_pass_captures = 0U;
    std::uint64_t short_actor_pass_capture_failures = 0U;
    std::uint64_t scheduled_short_actor_pass_actors = 0U;
    std::uint64_t frozen_short_actor_passes = 0U;
    std::uint64_t empty_short_actor_passes = 0U;
    std::uint64_t main_actor_pass_entries = 0U;
    std::uint64_t captured_main_actor_passes = 0U;
    std::uint64_t completed_main_actor_passes = 0U;
    std::uint64_t matched_main_actor_pass_dispatch_decisions = 0U;
    std::uint64_t mismatched_main_actor_pass_dispatch_decisions = 0U;
    std::uint64_t main_actor_pass_dispatched_when_skip_expected = 0U;
    std::uint64_t main_actor_pass_dispatched_for_other_slot = 0U;
    std::uint64_t main_actor_pass_dispatched_after_inactive_activity_gate = 0U;
    std::uint64_t main_actor_pass_dispatched_after_deferred_attribute_gate = 0U;
    std::uint64_t main_actor_pass_dispatched_after_changed_gate_bytes = 0U;
    std::uint64_t main_actor_pass_dispatched_after_unchanged_gate_bytes = 0U;
    std::uint64_t native_main_actor_pass_scan_realignments = 0U;
    std::uint64_t native_main_actor_pass_skipped_slots = 0U;
    std::uint64_t main_actor_pass_changed_activity_gate_bytes = 0U;
    std::uint64_t main_actor_pass_changed_attribute_gate_bytes = 0U;
    std::uint64_t main_actor_pass_unobserved_activity_gate_writers = 0U;
    std::uint64_t main_actor_pass_unobserved_attribute_gate_writers = 0U;
    std::uint64_t unclassified_main_actor_pass_activity_gate_writers = 0U;
    std::uint64_t unclassified_main_actor_pass_attribute_gate_writers = 0U;
    std::uint64_t main_actor_pass_dispatches_without_pending_decision = 0U;
    std::uint64_t main_actor_pass_queue_predictions = 0U;
    std::uint64_t matched_main_actor_pass_queue_predictions = 0U;
    std::uint64_t mismatched_main_actor_pass_queue_predictions = 0U;
    std::uint64_t main_actor_pass_completed_queue_filings = 0U;
    std::uint64_t main_actor_pass_queue_chain_mismatches = 0U;
    std::uint64_t unavailable_main_actor_pass_inputs = 0U;
    std::uint64_t unavailable_main_actor_pass_outputs = 0U;
    std::uint64_t superseded_main_actor_passes = 0U;
    std::uint64_t unmatched_main_actor_pass_completions = 0U;
    std::uint64_t main_actor_pass_capture_failures = 0U;
    std::uint64_t actor_dispatch_entries = 0U;
    std::uint64_t completed_actor_dispatches = 0U;
    std::uint64_t actor_queue_filer_entries = 0U;
    std::uint64_t matched_actor_dispatch_queue_handoffs = 0U;
    std::uint64_t mismatched_actor_dispatch_queue_handoffs = 0U;
    std::uint64_t unpaired_actor_dispatch_completions = 0U;
    std::uint64_t unpaired_actor_queue_filer_entries = 0U;
    std::uint64_t unavailable_actor_dispatch_entries = 0U;
    std::uint64_t unavailable_actor_dispatch_outputs = 0U;
    std::uint64_t unavailable_actor_queue_filer_inputs = 0U;
    std::uint64_t actor_queue_filer_predictions = 0U;
    std::uint64_t matched_actor_queue_filer_predictions = 0U;
    std::uint64_t mismatched_actor_queue_filer_predictions = 0U;
    std::uint64_t unavailable_actor_queue_filer_queue_inputs = 0U;
    std::uint64_t unavailable_actor_queue_filer_queue_outputs = 0U;
    std::uint64_t superseded_actor_queue_filer_predictions = 0U;
    std::uint64_t unmatched_actor_dispatch_completions = 0U;
    std::uint64_t nonqueue_actor_dispatch_returns = 0U;
    std::uint64_t actor_dispatch_output_slot_mismatches = 0U;
    std::uint64_t dropped_actor_dispatch_entries = 0U;
    std::uint64_t actor_dispatch_capture_failures = 0U;
    static constexpr std::size_t kMaximumActorDispatchHandlerEvidence = 256U;
    static constexpr std::size_t kReportedActorDispatchHandlerCoverage = 16U;
    static constexpr std::size_t kReportedActorDispatchHandlerEffects = 8U;
    std::array<ActorDispatchHandlerEvidence, kMaximumActorDispatchHandlerEvidence>
        actor_dispatch_handler_evidence{};
    std::uint64_t unclassified_actor_dispatch_handler_handoffs = 0U;
    std::uint64_t unavailable_actor_dispatch_state_transitions = 0U;
    std::uint64_t unclassified_actor_dispatch_state_transitions = 0U;
    std::uint64_t solid_playfield_probes = 0U;
    std::uint64_t clear_playfield_probes = 0U;
    std::uint64_t unmatched_playfield_probe_completions = 0U;
    std::uint64_t collision_retry_entries = 0U;
    std::uint64_t collision_retry_consuming_entries = 0U;
    std::uint64_t collision_retry_rearming_entries = 0U;
    std::uint64_t collision_retry_after_marked_playfield_cf33_entries = 0U;
    std::uint64_t collision_retry_after_unmarked_playfield_cf33_entries = 0U;
    std::uint64_t collision_retry_outside_signed_playfield_cf33_entries = 0U;
    std::vector<DecodedPlayfieldMap> playfield_maps;
    std::optional<NativeEpochRuntimeChain> native_epoch_runtime;

private:
    static constexpr std::size_t kMaximumPendingActorDispatches = 16U;
    static constexpr std::size_t kMaximumMainActorPassGateWriters = 16U;
    std::array<ActorGateWriteEvidence, kMaximumMainActorPassGateWriters>
        main_actor_pass_gate_writer_evidence{};
    std::vector<PendingActorDispatch> pending_actor_dispatches;
    std::optional<PendingActorQueueFiling> pending_actor_queue_filing;
    std::optional<PendingActorQueueFiler> pending_actor_queue_filer;
    std::optional<PendingMainActorPass> active_main_actor_pass;

    struct MotionRoutineSeed {
        RoutineKey routine;
        std::uint64_t entry_absolute_master_cycle = 0U;
        devils_crush::native_runtime::BallState ball;
        std::optional<devils_crush::native_runtime::BallMotionTuning> tuning;
        std::optional<devils_crush::native_runtime::BallMotionIncrement>
            substep_motion;
    };

    // DC95 gives the loop one immutable increment and F1B4 consumes it until
    // DD09 closes the clear path. The collision sweep can take a different
    // route at any point, so this observer certifies only a fully composed
    // sequence whose live state remains exactly on that no-contact path.
    struct CollisionLoopSeed {
        std::uint64_t split_entry_absolute_master_cycle = 0U;
        devils_crush::native_runtime::BallMotionSubsteps plan;
        devils_crush::native_runtime::BallState expected_ball;
        std::uint8_t completed_steps = 0U;
        std::optional<std::uint64_t> pending_position_entry;
        std::optional<std::uint64_t> pending_damping_entry;
        std::optional<devils_crush::native_runtime::BallMotionTuning>
            pending_damping_tuning;
        bool diverged_from_clear_path = false;
    };

    // CB2A exhausts actor candidates before it can fall through to CD74. That
    // makes the presence of a CD74 entry an unambiguous table-probe boundary;
    // its absence means the sweep was retained by an actor-contact path.
    struct CollisionSweepSeed {
        std::uint64_t entry_absolute_master_cycle = 0U;
        // The readable normal CB48/CC1A scan is actor-first. Preserve only
        // its carry-like boundary here: detailed target behavior remains with
        // the later native contact owner.
        std::optional<bool> native_actor_scan_found_solid;
        bool reached_playfield_probe = false;
        std::optional<std::uint64_t> playfield_probe_entry;
        std::optional<bool> playfield_probe_is_solid;
        std::optional<bool> cf33_selector_is_signed;
        std::optional<bool> cf33_signed_collision_marker;
    };

    // CBAB is a caller-selected one-slot actor query. It can occur inside the
    // wider CB2A sweep, so it owns a separate source/typed correlation rather
    // than being attributed to the outer scan's CD74 continuation.
    struct SingleSlotActorCollisionProbeSeed {
        std::uint64_t entry_absolute_master_cycle = 0U;
        std::optional<std::uint8_t> scanned_slot;
        std::optional<std::uint16_t> scanned_handler_address;
        std::optional<devils_crush::native_runtime::ActorCollisionScanResult>
            native_scan;
        std::optional<devils_crush::native_runtime::ActorCollisionScanDisposition>
            native_disposition;
        // CBC2 receives CC1A's carry immediately before CBAB either returns
        // clear or enters CBCF. A later CBCF can belong to CB48's broad scan,
        // so only this direct carry handoff may retain the one-slot seed for
        // confirmed-contact comparison.
        bool returned_contact = false;
    };

    struct SingleSlotActorCollisionHandlerEvidence {
        std::uint16_t handler_address = 0U;
        std::uint64_t matching_contacts = 0U;
        std::uint64_t mismatched_contacts = 0U;
    };

    // A carry-confirmed CBAB query that the readable CC1A model calls empty
    // is a true disagreement. Keep its finite mask input rather than guessing
    // a flipper-specific rule from the handler name.
    struct SingleSlotActorCollisionMaskMismatchEvidence {
        std::uint16_t handler_address = 0U;
        std::uint8_t slot = 0U;
        std::uint8_t phase = 0U;
        std::uint8_t probe_horizontal = 0U;
        std::uint16_t probe_vertical = 0U;
        std::uint8_t relative_horizontal = 0U;
        std::uint16_t relative_vertical = 0U;
        std::uint8_t mask_index = 0U;
        std::uint8_t selected_row = 0U;
        std::uint8_t column_bit = 0U;
        std::uint64_t contacts = 0U;
    };

    void observe_native_epoch_candidate_entry(
        const devils_crush::native_runtime::GameStateView& state,
        const devils_crush::native_runtime::BallState observed_ball) noexcept;

    void observe_native_epoch_actor_handler_dispatch(
        const devils_crush::native_runtime::GameStateView& state,
        const std::uint8_t slot) noexcept;

    void record_native_epoch_actor_handler_evidence(
        const NativeEpochRuntimeChain& chain,
        const bool matches) noexcept;

    void record_deferred_frame_schedule(
        const devils_crush::native_runtime::BallFrameOwner owner) noexcept;

    void record_prediction_schedule_owner(
        const devils_crush::native_runtime::BallFrameOwner owner) noexcept;

    void record_unrecognized_schedule_handler(
        const std::uint16_t handler_address) noexcept;

    void observe_pending_native_frame_prediction(
        const std::span<const std::byte> work_ram,
        const InputFrame& input) noexcept;

    // A marked CF33 contact enters CDF8 with CB7E's retry bit clear. Its first
    // child is F241, which restores F1B4's cache before CDF8 tries any
    // whole-pixel candidate. The later DC95 boundary may include additional
    // retry-loop motion, so this direct child-return boundary is the only
    // sound place to validate the readable rollback state.
    struct MarkedPlayfieldRetryRestoreSeed {
        std::uint64_t cdf8_entry_absolute_master_cycle = 0U;
        devils_crush::native_runtime::GameStateView::
            BallCollisionPositionCache cached_position;
    };

    enum class CollisionSweepExit {
        next_substep,
        damping,
        rearmed,
    };

    void observe_collision_sweep_entry(
        const RoutineEntry& entry,
        const devils_crush::native_runtime::GameStateView& state,
        const std::span<const std::byte> rom) noexcept;

    void begin_single_slot_actor_collision_probe(
        const RoutineEntry& entry) noexcept;

    void observe_single_slot_actor_collision_scan(
        const RoutineEntry& entry,
        const devils_crush::native_runtime::GameStateView& state,
        const std::span<const std::byte> rom) noexcept;

    void observe_single_slot_actor_collision_mask_stage(
        const RoutineEntry& entry,
        const devils_crush::native_runtime::GameStateView& state,
        const std::span<const std::byte> rom) noexcept;

    void observe_single_slot_confirmed_actor_contact() noexcept;

    void observe_single_slot_actor_collision_result(
        const RoutineEntry& entry) noexcept;

    void record_single_slot_actor_collision_disposition(
        const std::optional<devils_crush::native_runtime::
            ActorCollisionScanDisposition> disposition) noexcept;

    void record_single_slot_actor_collision_handler(
        const std::optional<std::uint16_t> handler_address,
        const bool matches) noexcept;

    void record_single_slot_actor_collision_mask_mismatch(
        const std::optional<devils_crush::native_runtime::
            ActorCollisionScanResult>& scan) noexcept;

    void observe_playfield_collision_completion(const RoutineCall& call) noexcept;

    void observe_playfield_contact_response_entry(
        const devils_crush::native_runtime::GameStateView& state) noexcept;

    void observe_collision_retry_entry(
        const RoutineEntry& entry,
        const devils_crush::native_runtime::GameStateView& state,
        const std::span<const std::byte> work_ram) noexcept;

    void observe_marked_playfield_retry_restore(
        const RoutineCall& call,
        const std::span<const std::byte> work_ram) noexcept;

    void finish_collision_sweep(const CollisionSweepExit exit) noexcept;

    void record_playfield_contact_response_exit(
        const CollisionSweepExit exit) noexcept;

    void observe_collision_loop_split_entry(
        const RoutineEntry& entry,
        const devils_crush::native_runtime::BallState ball) noexcept;

    void observe_collision_loop_position_entry(
        const RoutineEntry& entry,
        const devils_crush::native_runtime::BallState ball,
        const devils_crush::native_runtime::BallMotionIncrement increment) noexcept;

    void observe_collision_loop_position_completion(
        const RoutineCall& call,
        const devils_crush::native_runtime::BallState ball) noexcept;

    void observe_collision_loop_damping_entry(
        const RoutineEntry& entry,
        const devils_crush::native_runtime::BallState ball,
        const devils_crush::native_runtime::BallMotionTuning& tuning) noexcept;

    void observe_collision_loop_damping_completion(
        const RoutineCall& call,
        const devils_crush::native_runtime::BallState ball) noexcept;

    void report_collision_loop_shadow() const;

    void report_motion_routine_shadow() const;

    static constexpr std::size_t kMaximumMotionRoutineSeeds = 16U;
    std::vector<MotionRoutineSeed> motion_routine_seeds;
    std::uint64_t unavailable_motion_routine_seeds = 0U;
    std::uint64_t unavailable_motion_routine_outputs = 0U;
    std::uint64_t unmatched_motion_routine_completions = 0U;
    std::uint64_t dropped_motion_routine_seeds = 0U;
    std::uint64_t motion_routine_capture_failures = 0U;
    std::uint64_t position_integration_predictions = 0U;
    std::uint64_t position_integration_matches = 0U;
    std::uint64_t position_integration_mismatches = 0U;
    std::uint64_t rejected_position_integration_predictions = 0U;
    std::uint64_t motion_damping_predictions = 0U;
    std::uint64_t motion_damping_matches = 0U;
    std::uint64_t motion_damping_mismatches = 0U;
    std::uint64_t rejected_motion_damping_predictions = 0U;
    std::optional<CollisionLoopSeed> active_collision_loop;
    std::uint64_t collision_loop_seeds = 0U;
    std::uint64_t composed_clear_collision_loops = 0U;
    std::uint64_t collision_or_control_routed_loops = 0U;
    std::uint64_t collision_loop_rearmed_before_completion = 0U;
    std::uint64_t unexpected_composed_clear_loop_results = 0U;
    std::optional<CollisionSweepSeed> active_collision_sweep;
    std::uint64_t collision_sweep_entries = 0U;
    std::uint64_t clear_collision_sweeps = 0U;
    std::uint64_t actor_contact_sweeps_rearmed = 0U;
    std::uint64_t playfield_contact_sweeps_rearmed = 0U;
    std::uint64_t actor_contact_sweeps_to_damping = 0U;
    std::uint64_t playfield_contact_sweeps_to_damping = 0U;
    std::uint64_t normal_actor_collision_scans = 0U;
    std::uint64_t unavailable_normal_actor_collision_scans = 0U;
    std::uint64_t matched_normal_actor_collision_boundaries = 0U;
    std::uint64_t mismatched_normal_actor_collision_boundaries = 0U;
    std::uint64_t unresolved_normal_actor_collision_boundaries = 0U;
    std::optional<SingleSlotActorCollisionProbeSeed>
        active_single_slot_actor_collision_probe;
    std::uint64_t single_slot_actor_collision_probe_entries = 0U;
    std::uint64_t single_slot_actor_collision_scans = 0U;
    std::uint64_t unavailable_single_slot_actor_collision_scans = 0U;
    std::uint64_t single_slot_actor_collision_contact_returns = 0U;
    std::uint64_t single_slot_confirmed_actor_contacts = 0U;
    std::uint64_t matched_single_slot_actor_collision_contacts = 0U;
    std::uint64_t mismatched_single_slot_actor_collision_contacts = 0U;
    std::uint64_t single_slot_confirmed_contact_vertical_misses = 0U;
    std::uint64_t single_slot_confirmed_contact_shape_asset_misses = 0U;
    std::uint64_t single_slot_confirmed_contact_horizontal_world_misses = 0U;
    std::uint64_t single_slot_confirmed_contact_bounds_misses = 0U;
    std::uint64_t single_slot_confirmed_contact_mask_asset_misses = 0U;
    std::uint64_t single_slot_confirmed_contact_empty_mask_misses = 0U;
    std::uint64_t unavailable_single_slot_confirmed_contact_scans = 0U;
    std::array<SingleSlotActorCollisionHandlerEvidence, 8U>
        single_slot_actor_collision_handlers{};
    std::uint64_t unclassified_single_slot_actor_collision_contacts = 0U;
    std::array<SingleSlotActorCollisionMaskMismatchEvidence, 8U>
        single_slot_actor_collision_mask_mismatches{};
    std::uint64_t unclassified_single_slot_actor_collision_mask_mismatches = 0U;
    std::uint64_t single_slot_collision_mask_stage_entries = 0U;
    std::uint64_t unavailable_single_slot_collision_mask_stages = 0U;
    std::uint64_t matched_single_slot_collision_mask_stage_indices = 0U;
    std::uint64_t matched_single_slot_collision_mask_stage_cells = 0U;
    std::uint64_t matched_single_slot_collision_mask_stage_lookups = 0U;
    std::uint64_t matched_single_slot_collision_mask_stage_decisions = 0U;
    std::uint64_t unavailable_playfield_contact_responses = 0U;
    std::uint64_t unavailable_playfield_contact_markers = 0U;
    std::uint64_t signed_playfield_contact_responses = 0U;
    std::uint64_t signed_playfield_contact_marked_retries = 0U;
    std::uint64_t signed_playfield_contact_unmarked_responses = 0U;
    std::uint64_t clear_playfield_contact_responses = 0U;
    std::uint64_t playfield_contacts_without_cf33_response = 0U;
    std::uint64_t signed_playfield_contact_rearms = 0U;
    std::uint64_t clear_playfield_contact_rearms = 0U;
    std::uint64_t signed_playfield_contact_damping_paths = 0U;
    std::uint64_t clear_playfield_contact_damping_paths = 0U;
    std::uint64_t signed_playfield_contact_clear_paths = 0U;
    std::uint64_t clear_playfield_contact_clear_paths = 0U;
    std::optional<MarkedPlayfieldRetryRestoreSeed>
        pending_marked_playfield_retry_restore;
    std::uint64_t marked_playfield_retry_restore_candidates = 0U;
    std::uint64_t observed_marked_playfield_retry_restores = 0U;
    std::uint64_t verified_marked_playfield_retry_restores = 0U;
    std::uint64_t mismatched_marked_playfield_retry_restores = 0U;
    std::uint64_t unavailable_marked_playfield_retry_restore_caches = 0U;
    std::uint64_t superseded_marked_playfield_retry_restores = 0U;
};

[[nodiscard]] bool install_gameplay_bootstrap_shadow_observers(
    PceRuntime& runtime,
    NativeGameplayBootstrapShadow& shadow,
    const std::vector<std::byte>& rom,
    const bool registry_deployed);

} // namespace devils_crush::app::host
