// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"
#include "command_line.hpp"

namespace devils_crush::app::host {

// One admitted routine's share of a registry-deployed recording.
struct RegistryCensusRow {
    RoutineKey routine;
    std::string_view evidence;
    std::uint64_t entries = 0;
    std::uint64_t accepted = 0;
    std::uint64_t declined_at_horizon = 0;
    std::uint64_t native_master_cycles = 0;
    // True when an explicit --replace-hook or --observe-hook already owns the
    // key. Each key may be registered only once.
    bool skipped = false;
};

// The core reports absolute master cycles at hook entry only, so a run's
// elapsed span is bracketed by its first and last registry entry.
struct NativeCycleSpan {
    std::uint64_t first = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t last = 0;

    void observe(const std::uint64_t absolute_master_cycle) noexcept
    {
        first = std::min(first, absolute_master_cycle);
        last = std::max(last, absolute_master_cycle);
    }

    [[nodiscard]] std::uint64_t elapsed() const noexcept
    {
        return last > first ? last - first : 0U;
    }
};

// One registry deployment's live state. The row vector is sized once and never
// resized afterwards, so the installed hooks may capture row indices.
struct NativeRegistryDeployment {
    std::vector<RegistryCensusRow> rows;
    NativeCycleSpan span;
    // Core-native instruction capabilities installed beside (rather than as)
    // admitted game-routine rows. The full-palette TIA is the first: its
    // precise body belongs in the cycle share, while 00:EB30 remains the
    // registry owner reported by the routine census.
    std::uint64_t core_capability_entries = 0U;
    std::uint64_t core_capability_accepted = 0U;
    std::uint64_t core_capability_master_cycles = 0U;
    std::uint64_t core_tia_transfers = 0U;
    std::uint64_t core_bbr0_instructions = 0U;
    std::uint64_t core_bbr3_instructions = 0U;
    std::uint64_t core_bbs2_instructions = 0U;
    std::uint64_t core_rts_instructions = 0U;
    std::uint64_t core_vdc_status_reads = 0U;
    std::uint64_t core_vdc_data_reads = 0U;
    std::uint64_t core_vdc_address_steps = 0U;
    std::uint64_t core_plp_instructions = 0U;
    std::uint64_t core_sei_instructions = 0U;
    std::uint64_t core_jsr_instructions = 0U;
    std::uint64_t core_jmp_instructions = 0U;
    std::uint64_t core_giant_face_wait_dispatch_instructions = 0U;
    std::uint64_t core_four_tick_timer_tail_entries = 0U;
    std::uint64_t core_music_duration_tail_entries = 0U;
    std::uint64_t core_music_duration_return_entries = 0U;
    std::uint64_t core_mercy_star_not_last_tail_entries = 0U;
    std::uint64_t core_three_blobs_timer_decrement_instructions = 0U;
    std::uint64_t core_upper_table_idle_gate_entries = 0U;
    std::uint64_t core_upper_table_parent_idle_gate_entries = 0U;
    std::uint64_t core_upper_table_actor_idle_gate_entries = 0U;
    std::uint64_t core_bonus_hazard_timer_entries = 0U;
    std::uint64_t core_gray_blob_live_wait_entries = 0U;
    std::uint64_t core_bank1_vdc_row_upload_entries = 0U;
    std::uint64_t core_shared_collision_free_timer_tail_entries = 0U;
    std::uint64_t core_flipper_owner_instructions = 0U;
    std::uint64_t core_actor_scheduler_instructions = 0U;
    std::uint64_t core_tii_transfers = 0U;
    // Bounded native transcripts at instruction continuations that have not
    // yet been promoted to admitted routine rows. They remain visible in the
    // cycle share while live play validates their broader ownership.
    std::uint64_t auxiliary_transcript_entries = 0U;
    std::uint64_t auxiliary_transcript_accepted = 0U;
    std::uint64_t auxiliary_transcript_master_cycles = 0U;
    std::uint64_t auxiliary_bank9_actor_scan_entries = 0U;
    std::uint64_t auxiliary_bank9_actor_scan_head_entries = 0U;
    std::uint64_t auxiliary_bank9_actor_scan_advance_entries = 0U;
    std::uint64_t auxiliary_bank9_actor_scan_decimal_entries = 0U;
    std::uint64_t auxiliary_bank9_actor_scan_transfer_entries = 0U;
    std::uint64_t auxiliary_bank9_actor_scan_queue_bank_mismatch_entries = 0U;
    std::uint64_t auxiliary_bank9_actor_scan_wrapped_counter_entries = 0U;
    std::uint64_t auxiliary_bank9_scripted_actor_initializer_entries = 0U;
    std::uint64_t auxiliary_bank9_upper_table_countdown_entries = 0U;
    std::uint64_t auxiliary_bank9_upper_table_parent_decay_entries = 0U;
    std::uint64_t auxiliary_bank9_upper_table_parent_initializer_entries = 0U;
    std::uint64_t auxiliary_bank2_actor_countdown_return_entries = 0U;
    std::uint64_t auxiliary_bank3_actor_idle_branch_entries = 0U;
    std::uint64_t auxiliary_actor_queue_terminal_entries = 0U;
    std::uint64_t auxiliary_bonus_actor_scan_entries = 0U;
    std::uint64_t auxiliary_palette_fade_tail_entries = 0U;
    std::uint64_t auxiliary_coroutine_scheduler_rts_entries = 0U;
    std::uint64_t auxiliary_knight_source_slot_load_entries = 0U;
    std::uint64_t auxiliary_knight_bsr_entries = 0U;
    std::uint64_t auxiliary_knight_bcs_entries = 0U;
    std::uint64_t auxiliary_knight_movement_jsr_entries = 0U;
    std::uint64_t auxiliary_knight_movement_rts_entries = 0U;
    std::uint64_t auxiliary_fixed_scheduler_jsr_entries = 0U;
    std::uint64_t auxiliary_fixed_scheduler_rts_entries = 0U;
    std::uint64_t auxiliary_knight_counter_rts_entries = 0U;
    std::uint64_t auxiliary_music_voice_state_load_entries = 0U;
    std::uint64_t auxiliary_vdc_sprite_attribute_prefix_entries = 0U;
    std::uint64_t auxiliary_actor_script_record_loader_entries = 0U;
    std::uint64_t auxiliary_devil_shooter_release_countdown_entries = 0U;
};

// Admitted routines that are nonetheless withheld from a composed deployment.
//
// This is empty, and the mechanism stays for the next routine that needs it.
// A routine belongs here when it passes an @all regression but moves the state
// census off the assembly-only baseline 9036a9de..., and its own regression
// blesses the moved hash rather than the baseline: what it changes is then
// locked in by its evidence instead of caught by it, so it is not eligible for
// live play or for a faithful cycle measurement.
//
// Both former entries turned out to be replacement faults rather than
// properties of the routines, and both were the same shape -- a boundary
// charged the wrong number of cycles, with the error taken back off a later
// instruction so the body total stayed right. A correct total is why the state
// census could see that something was wrong and never where.
//
//   03:6054  `ADC $26` charged 9 master cycles instead of 12, putting both
//            remaining writes three cycles early; the taken branch at 606E was
//            charged 15 to absorb it. One write-trace diff against assembly,
//            scoped to the four addresses the routine touches plus the stack,
//            localised it to two rows.
//   02:57E9  `CMP $3980,X` charged 18 instead of 15, with the taken branch
//            charged 9 to absorb it -- and the checkpoint after `LDA $2D40`
//            published the caller's N and Z instead of the load's. This
//            routine writes nothing at all, so no write trace could have shown
//            the second fault; it is visible only as the status an interrupt
//            stacks when it lands on that boundary.
constexpr std::array<RoutineKey, 0U> kWithheldRoutines{};

// Installs every admitted routine that is neither withheld nor already claimed
// by an explicit hook. The C4DE actor-scan prefix returns at 01:C500 after it
// stores `$2CEE`, so the host can restore cabinet flipper input at that exact
// instruction boundary without claiming the complete preamble back.
constexpr RoutineKey kShooterGateInstruction{
    .physical_bank = 0x01U,
    .logical_pc = 0xC500U,
};

using NativeRegistryEntryObserver = std::function<void(
    const RoutineEntry&,
    std::span<const std::byte>)>;

[[nodiscard]] bool install_native_registry(
    PceRuntime& runtime,
    const CommandLine& options,
    const std::vector<std::byte>& replacement_rom,
    const std::span<const RoutineKey> claimed_routines,
    NativeRegistryDeployment& deployment,
    const NativeRegistryEntryObserver& entry_observer = {});

} // namespace devils_crush::app::host
