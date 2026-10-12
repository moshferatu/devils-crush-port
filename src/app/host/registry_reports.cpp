// SPDX-License-Identifier: GPL-3.0-or-later
#include "registry_reports.hpp"

#include "command_line.hpp"
#include "native_registry_install.hpp"

namespace devils_crush::app::host {

namespace {

// Actual clock ownership is independent of accepted-body registry estimates.
void print_execution_accounting(const devils_crush::emulation::PceRuntime& runtime,
                                const bool detailed = true)
{
    using Accounting = devils_crush::emulation::ExecutionAccounting;
    const auto counters = runtime.execution_accounting();
    if (!counters) { return; }
    std::uint64_t accounted = 0;
    for (const auto cycles : counters->cycles) { accounted += cycles; }
    const auto error = static_cast<std::int64_t>(counters->elapsed_master_cycles)
                     - static_cast<std::int64_t>(accounted);
    if (detailed) {
        std::cout << "Execution cycles: elapsed=" << counters->elapsed_master_cycles
                  << "; accounted=" << accounted << "; unaccounted=" << error << '\n';
        constexpr std::array<const char*, Accounting::owner_count> names{
            "translated", "retained", "fallback", "interrupt", "idle", "device", "other"};
        std::cout << "Execution ownership:";
        for (std::size_t i = 0; i < names.size(); ++i) {
            std::cout << (i == 0 ? " " : "; ") << names[i] << '=' << counters->cycles[i];
        }
        std::cout << '\n';
    }
    const auto working = counters->elapsed_master_cycles
        - counters->cycles[Accounting::idle] - counters->cycles[Accounting::device];
    std::cout << "Native execution outside wait/stalls: "
              << counters->cycles[Accounting::translated] << " of " << working;
    if (working != 0) {
        std::cout << " (" << std::fixed << std::setprecision(4)
                  << 100.0 * static_cast<double>(counters->cycles[Accounting::translated])
                     / static_cast<double>(working) << "%)" << std::defaultfloat;
    }
    if (!detailed) {
        std::cout << '\n'
                  << "Wait cycles: " << counters->cycles[Accounting::idle]
                  << "; device stall cycles: " << counters->cycles[Accounting::device] << '\n'
                  << "Execution accounting: " << counters->elapsed_master_cycles
                  << " elapsed; " << error << " unaccounted\n";
        for (const auto owner : {Accounting::retained, Accounting::fallback,
                                 Accounting::interrupt, Accounting::other}) {
            if (counters->cycles[owner] != 0U) {
                constexpr std::array<const char*, Accounting::owner_count> labels{
                    "translated", "retained", "fallback", "interrupt", "idle", "device", "other"};
                std::cout << "Non-native execution: " << labels[owner] << '='
                          << counters->cycles[owner] << " master cycles\n";
            }
        }
        return;
    }
    std::cout << '\n'
              << "Original opcode dispatches: retained=" << counters->opcode_dispatches[Accounting::retained]
              << "; fallback=" << counters->opcode_dispatches[Accounting::fallback]
              << "; idle=" << counters->opcode_dispatches[Accounting::idle] << '\n'
              << "Core replacement outcomes: translated=" << counters->translated_installs
              << "; retained=" << counters->retained_offers
              << "; rejected=" << counters->rejected_offers << '\n'
              << "Core interrupt entries: " << counters->interrupt_entries << '\n'
              << "Native interrupt entries: " << counters->native_interrupt_entries << '\n'
              << "Native block moves: TIA=" << counters->native_block_moves[0]
              << "; TAI=" << counters->native_block_moves[1]
              << "; TDD=" << counters->native_block_moves[2]
              << "; TII=" << counters->native_block_moves[3]
              << "; TIN=" << counters->native_block_moves[4]
              << "; resumes=" << counters->native_block_resumes
              << "; CPU cycles=" << counters->native_block_cycles << '\n';
    std::cout << "Native bit branches: instructions=" << counters->native_bit_branches
              << "; CPU cycles=" << counters->native_bit_branch_cycles << '\n';
    std::cout << "Native control flow: instructions=" << counters->native_control_flows
              << "; CPU cycles=" << counters->native_control_flow_cycles << '\n';
    std::cout << "Native load/status: instructions=" << counters->native_load_status_instructions
              << "; CPU cycles=" << counters->native_load_status_cycles << '\n';
    std::cout << "Native branches: instructions=" << counters->native_branches
              << "; CPU cycles=" << counters->native_branch_cycles << '\n';
    std::cout << "Native stores: instructions=" << counters->native_stores
              << "; CPU cycles=" << counters->native_store_cycles << '\n';
    std::cout << "Native loads: instructions=" << counters->native_loads
              << "; CPU cycles=" << counters->native_load_cycles << '\n';
    std::cout << "Native logical operations: instructions=" << counters->native_logical_instructions
              << "; CPU cycles=" << counters->native_logical_cycles << '\n';
    std::cout << "Native rmw operations: instructions=" << counters->native_rmw_instructions
              << "; CPU cycles=" << counters->native_rmw_cycles << '\n';
    std::cout << "Native compare operations: instructions=" << counters->native_compare_instructions
              << "; CPU cycles=" << counters->native_compare_cycles << '\n';
    std::cout << "Native stack/branch operations: instructions=" << counters->native_stack_branch_instructions
              << "; CPU cycles=" << counters->native_stack_branch_cycles << '\n';
    std::cout << "Native register/mapping operations: instructions=" << counters->native_register_instructions
              << "; CPU cycles=" << counters->native_register_cycles << '\n';
    std::cout << "Native arithmetic/speed operations: instructions=" << counters->native_arithmetic_instructions
              << "; CPU cycles=" << counters->native_arithmetic_cycles << '\n';
    std::cout << "Native bit test/update operations: instructions=" << counters->native_bit_operations_instructions
              << "; CPU cycles=" << counters->native_bit_operations_cycles << '\n';
}

void print_native_cycle_share(
    const std::uint64_t native_cycles,
    const std::uint64_t elapsed,
    const devils_crush::emulation::PceRuntime& runtime)
{
    const auto share = [](const std::uint64_t part,
                          const std::uint64_t whole) {
        return whole == 0U
            ? 0.0
            : 100.0 * static_cast<double>(part) / static_cast<double>(whole);
    };

    print_execution_accounting(runtime);
    std::cout << "Registry cycle estimates below count accepted bodies, not executed ownership.\n";
    std::cout << "Registry cycles: " << native_cycles << " native of "
              << elapsed << " elapsed master cycles";
    if (elapsed != 0U) {
        std::cout << " (" << std::fixed << std::setprecision(2)
                  << share(native_cycles, elapsed) << "%)"
                  << std::defaultfloat;
    }
    std::cout << '\n';

    const auto idle = runtime.native_idle_skip_coverage();
    if (!idle || idle->wait_master_cycles == 0U
        || idle->wait_master_cycles >= elapsed) {
        std::cout << "Registry cycles outside the frame-sync wait: not "
                     "measured; the wait is only counted while the "
                     "fast-forward is installed\n";
        return;
    }

    const std::uint64_t working = elapsed - idle->wait_master_cycles;
    std::cout << "Registry cycles outside the frame-sync wait: "
              << native_cycles << " native of " << working << " ("
              << std::fixed << std::setprecision(2)
              << share(native_cycles, working) << "%)" << std::defaultfloat
              << "; the machine waited at 00:E323 for "
              << idle->wait_master_cycles << " master cycles ("
              << std::fixed << std::setprecision(2)
              << share(idle->wait_master_cycles, elapsed) << "% of the route)"
              << std::defaultfloat << '\n';

    // Every point at which the machine decided that the next thing to happen
    // was an instruction rather than an interrupt -- the other thing the cycle
    // share cannot show, because fast-forwarding the wait removes those points
    // without removing the cycles they would have taken.
    //
    // Not the same as "instructions the host executed", and the sound driver's
    // interrupt return is what showed the difference: absorbing it stopped
    // 254,603 `RTI`s being interpreted and left this figure exactly where it
    // was, because applying a checkpoint returns to the top of the same loop
    // and asks the same question. That is the reassuring direction -- the
    // machine judged its interrupts at the same points either way -- but it is
    // a count of decisions, not of interpreted work.
    const auto boundary = runtime.native_boundary_coverage();
    if (boundary) {
        std::cout << "Instruction-boundary decisions: " << boundary->instructions
                  << '\n';
    }
}

// The native idle model verifies and advances the frame-sync wait. The
// profile still sees its entrance, non-skippable turns, and exit tail; they
// belong in the raw record, but are not a gameplay owner to port.
[[nodiscard]] constexpr bool is_frame_sync_wait_instruction(
    const RoutineKey routine) noexcept
{
    return routine.physical_bank == 0x00U
        && routine.logical_pc >= 0xE321U
        && routine.logical_pc <= 0xE32BU;
}

// The registry's total cycle share says how much native work it accepted; it
// cannot say where the rest of the interpreter time went. Keep that residue
// separate from the whole-ROM profile: a native transcript has an instruction
// entry boundary too, but its body is not a fallback.
void print_interpreted_residue_summary(
    const devils_crush::emulation::PceRuntime& runtime)
{
    const auto entries = runtime.export_interpreted_instruction_profile();
    std::uint64_t executions = 0U;
    std::uint64_t cycles = 0U;
    std::uint64_t frame_sync_executions = 0U;
    std::uint64_t frame_sync_cycles = 0U;
    for (const auto& entry : entries) {
        executions += entry.executions;
        cycles += entry.master_cycles;
        if (is_frame_sync_wait_instruction(entry.instruction)) {
            frame_sync_executions += entry.executions;
            frame_sync_cycles += entry.master_cycles;
        }
    }

    std::cout << "Interpreter residue: " << executions
              << " executed instruction interval(s), " << cycles
              << " master cycles across " << entries.size()
              << " address(es)";
    if (entries.empty()) {
        std::cout << "; no interpreter residue recorded\n";
        return;
    }
    std::cout << "; " << frame_sync_cycles
              << " cycles/" << frame_sync_executions
              << " interval(s) are the already-native frame-sync wait\n";

    std::vector<devils_crush::emulation::InstructionProfileEntry> actionable;
    actionable.reserve(entries.size());
    for (const auto& entry : entries) {
        if (!is_frame_sync_wait_instruction(entry.instruction)) {
            actionable.push_back(entry);
        }
    }
    std::uint64_t actionable_executions = 0U;
    std::uint64_t actionable_cycles = 0U;
    for (const auto& entry : actionable) {
        actionable_executions += entry.executions;
        actionable_cycles += entry.master_cycles;
    }
    std::cout << "Interpreter residue outside the frame-sync wait: "
              << actionable_executions << " executed instruction interval(s), "
              << actionable_cycles << " master cycles across "
              << actionable.size() << " address(es)\n";

    if (actionable.empty()) { return; }
    constexpr std::size_t kTopResidueAddresses = 12U;
    std::cout << "Interpreter residue, top "
              << std::min(kTopResidueAddresses, actionable.size())
              << " by master cycles:";
    for (std::size_t index = 0U;
         index < std::min(kTopResidueAddresses, actionable.size());
         ++index) {
        const auto& entry = actionable[index];
        std::cout << (index == 0U ? " " : "; ")
                  << format_routine_key(entry.instruction) << ' '
                  << entry.master_cycles << " cycles/"
                  << entry.executions << " interval(s)";
    }
    std::cout << '\n';
}

} // namespace

void print_native_registry_summary(
    const NativeRegistryDeployment& deployment,
    const ReplacementNesting nesting,
    const devils_crush::emulation::PceRuntime& runtime,
    const bool detailed,
    const bool cabinet_flipper_hook)
{
    const auto deployed = static_cast<std::size_t>(std::count_if(
        deployment.rows.begin(),
        deployment.rows.end(),
        [](const RegistryCensusRow& row) { return !row.skipped; }));
    std::uint64_t entered_routines = 0U;
    std::uint64_t total_entries = 0U;
    std::uint64_t total_accepted = 0U;
    std::uint64_t total_declined = 0U;
    std::uint64_t native_cycles = 0U;
    for (const auto& row : deployment.rows) {
        if (row.entries != 0U) {
            ++entered_routines;
        }
        total_entries += row.entries;
        total_accepted += row.accepted;
        total_declined += row.declined_at_horizon;
        native_cycles += row.native_master_cycles;
    }
    if (!detailed) {
        const auto host_count = cabinet_flipper_hook ? 1U : 0U;
        std::cout << "Registry: " << deployed << " routines installed directly";
        if (cabinet_flipper_hook) {
            std::cout << "; 1 via native cabinet flipper input hook (01:C500)";
        }
        std::cout << "; " << kWithheldRoutines.size() << " withheld";
        const auto unavailable = deployment.rows.size() - deployed - host_count;
        if (unavailable > kWithheldRoutines.size()) {
            std::cout << "; " << unavailable - kWithheldRoutines.size()
                      << " otherwise excluded";
        }
        std::cout << "\nRegistry routines entered: " << entered_routines << '\n';
        print_execution_accounting(runtime, false);
        const auto profile = runtime.export_interpreted_instruction_profile();
        const auto remaining = std::count_if(profile.begin(), profile.end(),
            [](const auto& entry) {
                return !is_frame_sync_wait_instruction(entry.instruction);
            });
        if (remaining == 0) {
            std::cout << "Interpreter residue outside frame-sync wait: none\n";
        } else {
            std::cout << "Interpreter residue outside frame-sync wait: "
                      << remaining << " address(es); use --execution-details for ranking\n";
        }
        if (nesting.overflowed != 0U) {
            std::cout << "Registry depth refusals: " << nesting.overflowed << '\n';
        }
        std::cout << "Registry abandonments: " << nesting.abandoned << '\n';
        return;
    }
    total_entries += deployment.core_capability_entries;
    total_accepted += deployment.core_capability_accepted;
    native_cycles += deployment.core_capability_master_cycles;
    total_entries += deployment.auxiliary_transcript_entries;
    total_accepted += deployment.auxiliary_transcript_accepted;
    native_cycles += deployment.auxiliary_transcript_master_cycles;
    const auto elapsed = deployment.span.elapsed();
    if (kWithheldRoutines.empty()) {
        std::cout << "Registry hold-backs: none; no routines withheld from "
                     "deployment.\n";
    } else {
        std::cout << "Registry hold-backs: ";
        for (std::size_t index = 0U; index < kWithheldRoutines.size();
             ++index) {
            std::cout << (index == 0U ? "" : ", ")
                      << format_routine_key(kWithheldRoutines[index]);
        }
        std::cout << " withheld; "
                  << (kWithheldRoutines.size() == 1U ? "it moves" : "each moves")
                  << " the state census off the assembly baseline.\n";
    }
    std::cout << "Registry deployment: " << deployed << " of "
              << deployment.rows.size() << " admitted routines installed, "
              << entered_routines << " entered\n"
              << "Registry calls: " << total_accepted << " accepted of "
              << total_entries << " entered (" << total_declined
              << " declined at an interrupt horizon)\n";
    if (deployment.core_capability_entries != 0U) {
        std::cout << "Registry core capabilities: "
                  << deployment.core_capability_accepted
                  << " native core instruction(s) of "
                  << deployment.core_capability_entries << " entered ("
                  << deployment.core_capability_master_cycles
                  << " master cycles; TIA="
                  << deployment.core_tia_transfers << ", BBR0="
                  << deployment.core_bbr0_instructions << ", BBR3="
                  << deployment.core_bbr3_instructions << ", BBS2="
              << deployment.core_bbs2_instructions << ", RTS="
              << deployment.core_rts_instructions << ", VDC-read="
              << deployment.core_vdc_status_reads << ", VDC-data-read="
              << deployment.core_vdc_data_reads << ", VDC-address="
              << deployment.core_vdc_address_steps << ", PLP="
                  << deployment.core_plp_instructions << ", SEI="
                  << deployment.core_sei_instructions << ", JSR="
                  << deployment.core_jsr_instructions << ", JMP="
                  << deployment.core_jmp_instructions << ", Giant-face-wait="
                  << deployment.core_giant_face_wait_dispatch_instructions
                  << ", Four-tick-tail="
                  << deployment.core_four_tick_timer_tail_entries
                  << ", Music-duration-tail="
                  << deployment.core_music_duration_tail_entries
                  << ", Music-duration-RTS="
                  << deployment.core_music_duration_return_entries
                  << ", Mercy-star-tail="
                  << deployment.core_mercy_star_not_last_tail_entries
                  << ", Three-blobs-timer="
                  << deployment.core_three_blobs_timer_decrement_instructions
                  << ", Upper-table-idle="
                  << deployment.core_upper_table_idle_gate_entries
                  << ", Upper-table-parent-idle="
                  << deployment.core_upper_table_parent_idle_gate_entries
                  << ", Upper-table-actor-idle="
                  << deployment.core_upper_table_actor_idle_gate_entries
                  << ", Bonus-hazard-timer="
                  << deployment.core_bonus_hazard_timer_entries
                  << ", Gray-blob-wait="
                  << deployment.core_gray_blob_live_wait_entries
                  << ", Bank-01-VDC-row="
                  << deployment.core_bank1_vdc_row_upload_entries
                  << ", Collision-free-timer="
                  << deployment.core_shared_collision_free_timer_tail_entries
                  << ", Flipper="
                  << deployment.core_flipper_owner_instructions << ", Actor-scheduler="
                  << deployment.core_actor_scheduler_instructions << ", TII="
                  << deployment.core_tii_transfers << ")\n";
    }
    if (deployment.auxiliary_transcript_entries != 0U) {
        std::cout << "Registry auxiliary transcripts: "
                  << deployment.auxiliary_transcript_accepted
                  << " native of " << deployment.auxiliary_transcript_entries
                  << " entered (" << deployment.auxiliary_transcript_master_cycles
                  << " master cycles; bank-09 actor-scan="
                  << deployment.auxiliary_bank9_actor_scan_entries
                  << ", bank-09 scripted actor initializer="
                  << deployment.auxiliary_bank9_scripted_actor_initializer_entries
                  << ", bank-09 upper-table countdown="
                  << deployment.auxiliary_bank9_upper_table_countdown_entries
                  << ", bank-09 upper-table parent decay="
                  << deployment.auxiliary_bank9_upper_table_parent_decay_entries
                  << ", bank-09 upper-table parent initializer="
                  << deployment.auxiliary_bank9_upper_table_parent_initializer_entries
                  << ", bank-02 actor countdown-return="
                  << deployment.auxiliary_bank2_actor_countdown_return_entries
                  << ", bank-03 actor idle-branch="
                  << deployment.auxiliary_bank3_actor_idle_branch_entries
                  << ", actor queue-terminal="
                  << deployment.auxiliary_actor_queue_terminal_entries
                  << ", bonus actor-scan="
                  << deployment.auxiliary_bonus_actor_scan_entries
                  << ", palette-fade tail="
                  << deployment.auxiliary_palette_fade_tail_entries
                  << ", coroutine-scheduler RTS="
                  << deployment.auxiliary_coroutine_scheduler_rts_entries
                  << ", knight source-slot load="
                  << deployment.auxiliary_knight_source_slot_load_entries
                  << ", knight BSR="
                  << deployment.auxiliary_knight_bsr_entries
                  << ", knight BCS="
                  << deployment.auxiliary_knight_bcs_entries
                  << ", knight movement JSR="
                  << deployment.auxiliary_knight_movement_jsr_entries
                  << ", knight movement RTS="
                  << deployment.auxiliary_knight_movement_rts_entries
                  << ", fixed-scheduler JSR="
                  << deployment.auxiliary_fixed_scheduler_jsr_entries
                  << ", fixed-scheduler RTS="
                  << deployment.auxiliary_fixed_scheduler_rts_entries
                  << ", knight counter RTS="
                  << deployment.auxiliary_knight_counter_rts_entries
                  << ", music voice-state load="
                  << deployment.auxiliary_music_voice_state_load_entries
                  << ", VDC sprite-attribute prefix="
                  << deployment.auxiliary_vdc_sprite_attribute_prefix_entries
                  << ", actor-script record loader="
                  << deployment.auxiliary_actor_script_record_loader_entries
                  << ", Devil Shooter release countdown="
                  << deployment.auxiliary_devil_shooter_release_countdown_entries
                  << ")\n";
        if (deployment.auxiliary_bank9_actor_scan_entries != 0U) {
            std::cout << "Bank-09 actor-scan entries: head="
                      << deployment.auxiliary_bank9_actor_scan_head_entries
                      << ", advance="
                      << deployment.auxiliary_bank9_actor_scan_advance_entries
                      << ", decimal="
                      << deployment.auxiliary_bank9_actor_scan_decimal_entries
                      << ", transfer="
                      << deployment.auxiliary_bank9_actor_scan_transfer_entries
                      << ", queue-bank mismatch="
                      << deployment.auxiliary_bank9_actor_scan_queue_bank_mismatch_entries
                      << ", counter-at-or-after-80="
                      << deployment.auxiliary_bank9_actor_scan_wrapped_counter_entries
                      << "\n";
        }
    }
    print_native_cycle_share(native_cycles, elapsed, runtime);
    print_interpreted_residue_summary(runtime);
    // The cycle line above credits every call this process offered. An
    // overflow is a call the runtime then refused, so a nonzero count is the
    // one condition under which that line overstates the share.
    std::cout << "Registry nesting: " << nesting.nested
              << " transcripts installed inside a suspended transcript, "
              << nesting.deepest << " deep at the most ("
              << nesting.overflowed << " refused for depth)\n"
              // A transcript given up on part-way through runs the rest of the
              // routine as assembly, so this costs cycles rather than
              // correctness -- but a promise it made and did not keep has to
              // be withdrawn, which is what the count is here to make visible.
              << "Registry abandonments: " << nesting.abandoned
              << " transcripts given up part-way through\n";
}

} // namespace devils_crush::app::host
