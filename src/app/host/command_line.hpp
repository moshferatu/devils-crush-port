// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"
#include "recording_plans.hpp"

namespace devils_crush::app::host {

inline const std::filesystem::path kBackupRamTestPath =
    std::filesystem::path("backup-ram-test.sav");

struct LogicalAddressRange {
    std::uint16_t first = 0;
    std::uint16_t last = 0;
};

struct RoutineReplayRequest {
    RoutineKey routine;
    std::uint64_t ordinal = 0;
    bool all_calls = false;
};

struct WorkRamPatch {
    int frame = 0;
    std::uint16_t logical_address = 0;
    std::uint8_t value = 0;

    [[nodiscard]] bool operator==(const WorkRamPatch&) const = default;
};

[[nodiscard]] std::string format_routine_key(const RoutineKey routine);

// Which layers the stitched cabinet playfield draws. The retained layer and
// the copied live SAT render the same actors through different coordinate
// paths, swapping authority as the camera passes over each one, so isolating
// them is how a disagreement between the two is attributed.
enum class CabinetLayers {
    all,
    background_only,
    retained_only,
    sat_only,
};

struct CommandLine {
    std::filesystem::path rom_path = "Devil's Crush.pce";
    bool validate_only = false;
    bool smoke_test = false;
    bool cabinet_smoke_test = false;
    bool cabinet = false;
    // Zero selects the ordinary title/menu flow. Values one through six boot
    // the same session and request the cartridge's authentic bonus transition
    // before handing control to the interactive host.
    std::uint8_t bonus_stage = 0U;
    // A cheat for testing the table: the left mouse button carries the ball
    // over the playfield.
    bool drag_ball = false;
    // Records an interactive, power-on play session as exact per-frame pad
    // masks plus the runtime coverage artifacts needed to rank new code.
    std::filesystem::path capture_session_directory;
    // Replays the pad stream from an interactive capture through the ordinary
    // deterministic recording and trace machinery.
    std::filesystem::path replay_session_input_path;
    // Diagnostic: draw only one of the stitched playfield's layers.
    CabinetLayers cabinet_layers = CabinetLayers::all;
    // Diagnostic: report retained-overlay composition once a second.
    bool cabinet_overlay_stats = false;
    bool record_canonical = false;
    std::filesystem::path canonical_path = kCanonicalRecordingPath;
    bool record_gameplay = false;
    std::filesystem::path gameplay_path = kGameplayRecordingPath;
    // A second input-only table pass.  Unlike the state census it does not
    // patch work RAM, so it can establish a new natural rule branch before a
    // semantic replacement is admitted.
    bool record_gameplay_alternate = false;
    std::filesystem::path gameplay_alternate_path =
        kGameplayAlternateRecordingPath;
    // A third input-only table pass. It deliberately uses both directional
    // flippers and a different nudge cadence from the first alternate pass
    // so it can reach a naturally selected table-rule branch.
    bool record_gameplay_sweep = false;
    std::filesystem::path gameplay_sweep_path = kGameplaySweepRecordingPath;
    // A fourth input-only pass that holds each flipper through the other's
    // release window.  It deliberately changes ball deflection, rather than
    // merely the cadence of an already-screened input plan.
    bool record_gameplay_crossfire = false;
    std::filesystem::path gameplay_crossfire_path =
        kGameplayCrossfireRecordingPath;
    // A fifth input-only pass that separates the flipper holds with an
    // asymmetric directional-nudge cadence.  This samples a different live
    // table trajectory from the cadence, sweep, and crossfire routes.
    bool record_gameplay_orbit = false;
    std::filesystem::path gameplay_orbit_path = kGameplayOrbitRecordingPath;
    // A sixth input-only route with long, alternating flipper holds and
    // directional nudge bursts.  It favors lane and orbit trajectories that
    // the short cadence routes do not hold long enough to sample.
    bool record_gameplay_lanes = false;
    std::filesystem::path gameplay_lanes_path = kGameplayLanesRecordingPath;
    // A seventh input-only route. Its long, offset flipper holds and sparse
    // direction inputs intentionally avoid the dense periodic patterns used
    // by the earlier discovery routes.
    bool record_gameplay_stagger = false;
    std::filesystem::path gameplay_stagger_path =
        kGameplayStaggerRecordingPath;
    // Short, uneven flipper bursts and long neutral windows deliberately
    // explore a low-contact trajectory distinct from the sustained routes.
    bool record_gameplay_pulse = false;
    std::filesystem::path gameplay_pulse_path = kGameplayPulseRecordingPath;
    // A ninth input-only route that holds the right flipper through a longer
    // recovery window, then alternates short left saves.  This biases a
    // different set of natural lower-table deflections than the symmetric
    // discovery routes.
    bool record_gameplay_rally = false;
    std::filesystem::path gameplay_rally_path = kGameplayRallyRecordingPath;
    // A tenth input-only route.  Its wide, alternating saves and reversed
    // nudge direction deliberately target side-lane rebounds rather than the
    // lower-table recovery patterns sampled by rally.
    bool record_gameplay_slalom = false;
    std::filesystem::path gameplay_slalom_path = kGameplaySlalomRecordingPath;
    // An eleventh input-only route.  Long neutral travel between mirrored
    // saves and three distinct nudge directions favors rebounds that are not
    // sampled by the dense or sustained discovery patterns.
    bool record_gameplay_pendulum = false;
    std::filesystem::path gameplay_pendulum_path =
        kGameplayPendulumRecordingPath;
    // A twelfth input-only route. Sparse flipper saves combined with a
    // rotating nudge pattern deliberately prioritize rebound timing rather
    // than the sustained flipper rhythms of the existing discovery plans.
    bool record_gameplay_nudge = false;
    std::filesystem::path gameplay_nudge_path = kGameplayNudgeRecordingPath;
    // A thirteenth input-only route.  Its long, alternating holds leave wide
    // neutral rebound windows and vary the launch timing from every existing
    // discovery plan.
    bool record_gameplay_switchback = false;
    std::filesystem::path gameplay_switchback_path =
        kGameplaySwitchbackRecordingPath;
    // A fourteenth input-only route. Its 359-frame mixed flipper/nudge cycle
    // starts the live table earlier and visits the four directions while the
    // opposite flipper is held, selecting a trajectory none of the prior
    // discovery plans repeats.
    bool record_gameplay_spiral = false;
    std::filesystem::path gameplay_spiral_path =
        kGameplaySpiralRecordingPath;
    // A fifteenth input-only route. It trades the prior long held-launch
    // plans for repeated short Button-II impulses between staggered saves,
    // targeting collision rebounds rather than a flipper-only orbit.
    bool record_gameplay_ricochet = false;
    std::filesystem::path gameplay_ricochet_path =
        kGameplayRicochetRecordingPath;
    // A sixteenth input-only route. Its early launch, sustained alternating
    // flipper banks, and uneven nudge volleys seek a table trajectory that
    // cannot be reached by the short-tap or long-neutral discovery plans.
    bool record_gameplay_volley = false;
    std::filesystem::path gameplay_volley_path = kGameplayVolleyRecordingPath;
    // A seventeenth input-only route that makes long, deliberately offset
    // double-save windows between wide neutral rebounds.
    bool record_gameplay_whirlpool = false;
    std::filesystem::path gameplay_whirlpool_path =
        kGameplayWhirlpoolRecordingPath;
    // An eighteenth input-only route.  It releases the launcher well before
    // the current discovery routes, then makes sparse, asymmetric saves after
    // a long neutral descent to pursue a distinct upper-to-lower trajectory.
    bool record_gameplay_cascade = false;
    std::filesystem::path gameplay_cascade_path =
        kGameplayCascadeRecordingPath;
    // A nineteenth input-only route. It uses the longest launcher hold of the
    // discovery set, then saves sparsely and nudges only while neutral so the
    // ball has a materially different chance to reach the upper table.
    bool record_gameplay_higharc = false;
    std::filesystem::path gameplay_higharc_path =
        kGameplayHigharcRecordingPath;
    // A twentieth input-only route. Its delayed launch and long alternating
    // saves are separated by neutral travel, while every directional nudge is
    // outside a held-flipper window. This probes a distinct natural table
    // trajectory without mutating emulated gameplay state.
    bool record_gameplay_twist = false;
    std::filesystem::path gameplay_twist_path = kGameplayTwistRecordingPath;
    // A twenty-first input-only route.  It releases from a middle-strength
    // launcher charge, then uses deliberately asymmetric side saves and
    // Button-II pulses to pursue side-wall contacts before the ball settles
    // into the lower-table rhythms sampled by the existing routes.
    bool record_gameplay_sidestep = false;
    std::filesystem::path gameplay_sidestep_path =
        kGameplaySidestepRecordingPath;
    // A twenty-second input-only route. It interleaves longer left/right
    // recovery windows after a delayed launch, with nudges only while both
    // flippers are released, to explore a wider natural table trajectory.
    bool record_gameplay_weave = false;
    std::filesystem::path gameplay_weave_path = kGameplayWeaveRecordingPath;
    // A twenty-third input-only route. It holds a medium launch, then makes
    // sparse, offset bank saves and only nudges in neutral windows to probe
    // long side-wall rebounds absent from the dense recovery plans.
    bool record_gameplay_bankshot = false;
    std::filesystem::path gameplay_bankshot_path =
        kGameplayBankshotRecordingPath;
    // A twenty-fourth input-only route. Its longest launch hold, wide saves,
    // and neutral-only direction inputs pursue a high-table trajectory that
    // none of the earlier short or asymmetric recovery plans repeat.
    bool record_gameplay_playfield = false;
    std::filesystem::path gameplay_playfield_path =
        kGameplayPlayfieldRecordingPath;
    // A twenty-fifth input-only route. It releases later than the dense
    // recovery plans, then uses a short right-first corner save before a
    // distant left recovery to probe a fresh natural return path.
    bool record_gameplay_corner = false;
    std::filesystem::path gameplay_corner_path = kGameplayCornerRecordingPath;
    // A twenty-sixth input-only route. It charges past every prior route,
    // then favors alternating alley recoveries while leaving every nudge in
    // a neutral travel window.
    bool record_gameplay_alley = false;
    std::filesystem::path gameplay_alley_path = kGameplayAlleyRecordingPath;
    // A twenty-seventh input-only route. It releases from a medium-long
    // charge, then deliberately crosses a long right recovery with an early
    // left save while keeping every directional nudge in free travel.
    bool record_gameplay_rebound = false;
    std::filesystem::path gameplay_rebound_path =
        kGameplayReboundRecordingPath;
    // A twenty-eighth input-only route. Its delayed launch and skewed
    // left-first recovery seek a separate high-table return without any RAM
    // patching or overlapping directional inputs.
    bool record_gameplay_skew = false;
    std::filesystem::path gameplay_skew_path = kGameplaySkewRecordingPath;
    // A twenty-ninth input-only route. Its late high launch, right-first
    // recovery, then alternating wide saves use a longer prime cadence to
    // screen a fresh natural table trajectory without state patching.
    bool record_gameplay_helix = false;
    std::filesystem::path gameplay_helix_path = kGameplayHelixRecordingPath;
    // A thirtieth input-only route. Its medium-high launch and staggered
    // short/long side banks run on a distinct prime cadence, leaving every
    // Button-II and directional impulse in neutral travel.
    bool record_gameplay_vector = false;
    std::filesystem::path gameplay_vector_path = kGameplayVectorRecordingPath;
    // A thirty-first input-only route. Its late release and left-right-left
    // recovery pattern run on a new prime cadence while keeping all nudge
    // and Button-II impulses in neutral travel.
    bool record_gameplay_radial = false;
    std::filesystem::path gameplay_radial_path = kGameplayRadialRecordingPath;
    bool record_state_census = false;
    std::filesystem::path state_census_path = kStateCensusRecordingPath;
    std::filesystem::path canonical_work_ram_path;
    std::filesystem::path canonical_framebuffer_path;
    std::filesystem::path canonical_audio_path;
    std::vector<RoutineKey> pass_through_hooks;
    std::vector<RoutineKey> observe_hooks;
    std::vector<RoutineReplayRequest> replace_hooks;
    std::filesystem::path routine_trace_path;
    std::filesystem::path routine_call_trace_path;
    std::filesystem::path playfield_collision_sources_path;
    std::vector<RoutineReplayRequest> routine_replays;
    std::filesystem::path routine_replay_corpus_output_directory;
    std::vector<std::filesystem::path> routine_replay_corpus_paths;
    std::vector<WorkRamPatch> work_ram_patches;
    std::vector<LogicalAddressRange> work_ram_write_ranges;
    std::filesystem::path work_ram_write_trace_path;
    // Device-register writes, selected by logical address on the hardware
    // page. The sound driver's PSG traffic is invisible to the work-RAM
    // census, so its ports need a census channel of their own.
    std::vector<LogicalAddressRange> bus_write_ranges;
    std::filesystem::path bus_write_trace_path;
    bool replace_registry = false;
    // The sound generator is a native device model by default. This opts back
    // out to the emulator's, which is what a difference in the audio hash is
    // measured against.
    bool emulated_psg = false;
    // Experimental repair for the cartridge's deliberate effect-first PSG
    // arbitration: displaced music voices continue on a second native PSG.
    bool music_voice_overlay = false;
    // Reports which of the device model's paths a recorded route exercised,
    // which is the measure of what a census-based acceptance actually proves.
    bool report_psg_coverage = false;
    // The same for the video display controller, on the same terms: the
    // native device is the default, and this hands the chip back so a
    // difference in the framebuffer hash has something to be measured
    // against.
    bool emulated_vdc = false;
    bool report_vdc_coverage = false;
    // And the same for the colour encoder, which owns the video schedule and
    // draws. Handing it back with the controller still native is what
    // separates a difference the encoder caused from one the controller did.
    bool emulated_vce = false;
    bool report_vce_coverage = false;
    // And for the processor's timer and interrupt controller, which is the
    // narrowest of the four seams and the only one inside the processor. The
    // arm that hands it back while the three devices beside it stay native is
    // what makes a difference in the census attributable to this device.
    bool emulated_irq = false;
    bool report_irq_coverage = false;
    // And for the processor's clock and event scheduler, the fifth seam and
    // the first whose shape is a block of memory rather than a call table.
    bool emulated_scheduler = false;
    bool report_scheduler_coverage = false;
    // And for the decision the processor makes between two instructions, the
    // sixth seam and the first that is a rule rather than a device.
    bool emulated_boundary = false;
    bool report_boundary_coverage = false;
    bool execution_details = false;
    bool emulated_idle = false;
    bool report_idle_coverage = false;
    bool emulated_pad = false;
    bool report_pad_coverage = false;
    // The interactive host deploys the registry by default so a session runs
    // the ported code; this opts back out to the emulated original.
    bool emulate_only = false;
    // Runs the native gameplay bootstrap against each completed live frame,
    // without transferring frame ownership away from the emulator.
    bool native_gameplay_bootstrap_shadow = false;
    std::filesystem::path cabinet_dump_route_directory;
    bool cabinet_oracle = false;
    bool cabinet_freeze_census = false;
    std::filesystem::path cabinet_freeze_census_state;
    bool cabinet_flipper_trace = false;
    bool cabinet_drain_probe = false;
    bool cabinet_nudge_probe = false;
    double nudge_multiplier = 1.0;
    bool cabinet_launch_check = false;
    bool cabinet_launch_flipper_probe = false;
    bool cabinet_plunger_probe = false;
    bool cabinet_plunger_check = false;
    bool cabinet_ball_drag_check = false;
    bool verify_unsigned_square_root = false;
    bool verify_unsigned_word_multiply_partial = false;
    bool verify_signed_word_normalizer = false;
    bool verify_signed_word_multiply = false;
    bool verify_side_gate_deadline_check = false;
    bool high_score_check = false;
    std::filesystem::path cabinet_view_probe_directory;
    std::filesystem::path cabinet_motion_check_directory;
    bool cabinet_motion_gpu_check = false;
    bool cabinet_presentation_check = false;
    bool cabinet_seed_check = false;
    bool cabinet_palette_check = false;
    bool cabinet_attribute_check = false;
    std::filesystem::path cabinet_state_dump_directory;
    // Bisection aid: deploy only registry entries [first, first + count).
    bool registry_slice_set = false;
    std::size_t registry_slice_first = 0U;
    std::size_t registry_slice_count = 0U;
    std::vector<RoutineKey> registry_excludes;
    std::filesystem::path native_cycle_census_path;
    std::filesystem::path instruction_profile_path;
    std::filesystem::path abandonment_census_path;
    std::filesystem::path table_vram_census_path;
    bool backup_ram_test = false;
    std::filesystem::path backup_ram_test_path = kBackupRamTestPath;
    bool show_help = false;
    std::string error;
};

// What a command line says about the settings normal play otherwise takes
// from its saved configuration, which is only read once every argument is
// known.
struct CommandLineOverrides {
    std::optional<bool> cabinet;
    std::optional<bool> music_overlay;
    bool rom = false;
    bool bonus_stage = false;
    bool nudge_multiplier = false;
};

[[nodiscard]] bool read_command_line_arguments(
    const int argc,
    char* argv[],
    CommandLine& options,
    CommandLineOverrides& overrides);

[[nodiscard]] CommandLine parse_command_line(const int argc, char* argv[]);

void print_usage();

} // namespace devils_crush::app::host
