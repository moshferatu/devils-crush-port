// SPDX-License-Identifier: GPL-3.0-or-later
#include "coverage_reports.hpp"

namespace devils_crush::app::host {

// How much of the sound generator a route actually asked for. The device is
// accepted against a census, and a census can only prove the paths the game
// takes; printing the counts is what keeps the difference between "the model
// is right" and "the model is right about what this route does" visible.
void report_psg_coverage(
    const devils_crush::emulation::PceRuntime& runtime)
{
    const auto coverage = runtime.native_psg_coverage();
    if (!coverage) {
        std::cout << "The emulator owned the sound generator; no native "
                     "paths to report.\n";
        return;
    }

    const auto line =
        [](const std::string_view name,
           const devils_crush::audio::PcePsg::PathCoverage& path) {
            std::cout << "  " << name << ": " << path.total << " (";
            for (std::size_t channel = 0U;
                 channel < path.by_channel.size();
                 ++channel) {
                std::cout << (channel != 0U ? ", " : "")
                          << path.by_channel[channel];
            }
            std::cout << ")\n";
        };

    std::cout << "Sound generator paths, total and per channel:\n";
    line("silent    ", coverage->silent);
    line("waveform  ", coverage->waveform);
    line("noise     ", coverage->noise);
    line("accumulate", coverage->accumulated);
    std::cout << "  LFO updates: " << coverage->lfo_updates
              << ", modulator recursions: " << coverage->lfo_recursions
              << "\n  whole-period steps: "
              << coverage->accumulate_fast_paths
              << ", shift-register clocks: " << coverage->lfsr_clocks
              << ", sample-latch writes: "
              << coverage->sample_latch_writes << '\n';
}

// The same for the video display controller. Its paths sit further from
// what a pinball table asks for than the sound chip's do -- a route may
// never overflow a line, never collide sprite zero, and never fetch a
// 16-colour sprite -- so these counts are what separates "the model is
// right" from "the model is right about this route".
void report_vdc_coverage(
    const devils_crush::emulation::PceRuntime& runtime)
{
    const auto coverage = runtime.native_vdc_coverage();
    if (!coverage) {
        std::cout << "The emulator owned the video display controller; no "
                     "native paths to report.\n";
        return;
    }

    std::cout << "Video display controller paths:\n"
              << "  fields " << coverage->fields << ", scanlines "
              << coverage->scanlines << " (background "
              << coverage->background_lines << ", sprites "
              << coverage->sprite_lines << ", burst "
              << coverage->burst_lines << ")\n"
              << "  sprites " << coverage->sprites_fetched << " fetched, "
              << coverage->wide_sprite_halves << " wide halves, "
              << coverage->narrow_colour_fetches << " 16-colour\n"
              << "  overflows " << coverage->sprite_overflows
              << ", sprite-zero hits " << coverage->sprite_zero_hits << '\n'
              << "  transfers: " << coverage->vram_dma_words << " words, "
              << coverage->sat_dma_transfers << " tables\n"
              << "  video memory through the port: " << coverage->port_writes
              << " words written, " << coverage->port_reads << " read\n"
              << "  stalls: " << coverage->wait_state_requests
              << " requested, " << coverage->forced_completions
              << " forced to completion\n"
              << "  interrupts: " << coverage->raster_compares
              << " raster compares, " << coverage->vertical_blanks
              << " vertical blanks\n";
}

// And the colour encoder's, where the interesting gaps are not about drawing
// but about rates and stalls. A pinball table runs at one dot rate, in
// colour, on a 262-line field, and never exhausts a frame's budget of stolen
// cycles -- so most of what this device can do, a route like this one asks
// for none of.
void report_vce_coverage(
    const devils_crush::emulation::PceRuntime& runtime)
{
    const auto coverage = runtime.native_vce_coverage();
    if (!coverage) {
        std::cout << "The emulator owned the colour encoder; no native paths "
                     "to report.\n";
        return;
    }

    std::cout << "Colour encoder paths:\n"
              << "  frames " << coverage->frames << ", partials "
              << coverage->partials << ", completed "
              << coverage->frames_completed << '\n'
              << "  scanlines " << coverage->scanlines << ", restarts "
              << coverage->field_restarts << ", vertical blanks "
              << coverage->vertical_blanks << '\n'
              << "  chunks " << coverage->chunks << " ("
              << coverage->floored_chunks << " floored), dot clocks "
              << coverage->dot_clocks << '\n'
              << "  pixels " << coverage->pixels << ", off-screen runs "
              << coverage->off_screen_runs << '\n'
              << "  rates: " << coverage->lines_at_rate[0] << ' '
              << coverage->lines_at_rate[1] << ' '
              << coverage->lines_at_rate[2] << ' '
              << coverage->lines_at_rate[3] << '\n'
              << "  register changes: " << coverage->dot_clock_changes
              << " dot clock, " << coverage->monochrome_changes
              << " monochrome, " << coverage->line_count_changes
              << " line count\n"
              << "  palette: " << coverage->palette_writes << " written, "
              << coverage->palette_reads << " read\n"
              << "  stalls: " << coverage->wait_state_requests
              << " requested, " << coverage->wait_state_refusals
              << " refused, " << coverage->stolen_cycles << " cycles\n"
              << "  interrupts: " << coverage->irq_raises << " raised, "
              << coverage->irq_lowers << " lowered\n"
              << "  exits " << coverage->cpu_exits
              << ", compact disc events " << coverage->cd_events << '\n';
}

// And the timer and interrupt controller's. This device is small enough that
// a route drives nearly all of it -- the gaps are in the other direction from
// the video chips': the game leans on the timer constantly and touches the
// mask barely at all.
void report_irq_coverage(
    const devils_crush::emulation::PceRuntime& runtime)
{
    const auto coverage = runtime.native_irq_coverage();
    if (!coverage) {
        std::cout << "The emulator owned the timer and interrupt controller; "
                     "no native paths to report.\n";
        return;
    }

    std::cout << "Timer and interrupt controller paths:\n"
              << "  syncs " << coverage->syncs << ", ticks "
              << coverage->ticks << " (" << coverage->multi_tick_syncs
              << " spanning more than one)\n"
              << "  timer: " << coverage->timer_underflows << " underflows, "
              << coverage->timer_reloads << " reloads, "
              << coverage->timer_starts << " starts, "
              << coverage->timer_stops << " stops\n"
              << "  timer file: " << coverage->counter_reads
              << " counter reads, " << coverage->reload_writes
              << " reload writes, " << coverage->control_writes
              << " control writes\n"
              << "  interrupt file: " << coverage->mask_reads
              << " mask reads, " << coverage->mask_writes
              << " mask writes, " << coverage->status_reads
              << " status reads\n"
              << "  acknowledges: " << coverage->acknowledge_reads
              << " by read, " << coverage->acknowledge_writes
              << " by write\n"
              << "  lines raised: " << coverage->requests_raised[0]
              << " irq2, " << coverage->requests_raised[1] << " irq1, "
              << coverage->requests_raised[2] << " timer\n"
              << "  horizon: " << coverage->horizon_queries << " queries, "
              << coverage->horizon_never << " never, "
              << coverage->horizon_now << " already due\n";
}

// And the clock and event scheduler's. Unlike every device before it, almost
// everything here is driven once per instruction, so the counts are enormous
// and the interesting numbers are the ratios: how rarely an advance reaches a
// horizon at all, and which of the two devices behind that horizon won.
void report_scheduler_coverage(
    const devils_crush::emulation::PceRuntime& runtime)
{
    const auto coverage = runtime.native_scheduler_coverage();
    if (!coverage) {
        std::cout << "The emulator owned the clock and event scheduler; no "
                     "native paths to report.\n";
        return;
    }

    std::cout << "Clock and event scheduler paths:\n"
              << "  advances: " << coverage->advances << " in cycles ("
              << coverage->slow_cycles << " slow, " << coverage->fast_cycles
              << " fast), " << coverage->master_advances
              << " in master clocks\n"
              << "  clock moved " << coverage->master_clocks
              << " master clocks, " << coverage->stalled_advances
              << " advances stalled\n"
              << "  horizon reached " << coverage->horizon_reached
              << " times, user event due " << coverage->user_events
              << " of those\n"
              << "  recomputed " << coverage->recalculations
              << " times: timer nearer " << coverage->timer_nearer
              << ", user nearer " << coverage->user_nearer << '\n'
              << "  set events " << coverage->set_events << ", rebases "
              << coverage->rebases << ", runs " << coverage->runs
              << ", exits " << coverage->exits << ", rate changes "
              << coverage->speed_changes << '\n';
}

// And the instruction boundary's. Two counters per instruction and a tail of
// everything else, so the shape to read here is the ratio: how rarely a
// boundary that woke at all admitted a vector, and how often the timer won on
// a line the latch did not hold -- the one asymmetry in this rule a route can
// be asked about rather than argued about.
void report_boundary_coverage(
    const devils_crush::emulation::PceRuntime& runtime)
{
    const auto coverage = runtime.native_boundary_coverage();
    if (!coverage) {
        std::cout << "The emulator owned the instruction boundary; no native "
                     "paths to report.\n";
        return;
    }

    std::cout << "Instruction boundary paths:\n"
              << "  samples: " << coverage->samples << " ("
              << coverage->pending_samples << " latched a line, "
              << coverage->masked_samples << " behind the I flag)\n"
              << "  decisions: " << coverage->decisions << ", instruction "
              << coverage->instructions << " (" << coverage->declined
              << " woken and declined)\n"
              << "  vectors: " << coverage->resets << " reset, "
              << coverage->timers << " timer (" << coverage->late_timers
              << " on a line the latch did not hold), " << coverage->irq1s
              << " irq1, " << coverage->irq2s << " irq2\n"
              << "  resets: " << coverage->reset_signals << " signalled, "
              << coverage->reset_acknowledgements << " acknowledged\n";
}

void report_idle_coverage(
    const devils_crush::emulation::PceRuntime& runtime)
{
    const auto coverage = runtime.native_idle_skip_coverage();
    if (!coverage) {
        std::cout << "The frame-sync spin was executed; no skipped "
                     "iterations to report.\n";
        return;
    }

    std::cout << "Frame-sync spin:\n"
              << "  arrivals: " << coverage->arrivals << ", idle "
              << coverage->idle << ", about to exit " << coverage->exits
              << ", refused " << coverage->refusals << '\n'
              << "  skips: " << coverage->skips << " covering "
              << coverage->iterations << " iterations and "
              << coverage->skipped_master_cycles
              << " master cycles (longest " << coverage->longest_skip
              << " iterations)\n"
              << "  the machine waited here for "
              << coverage->wait_master_cycles
              << " master cycles in total\n";
    if (coverage->skips != 0U) {
        std::cout << "  iterations a skip covered on average: "
                  << coverage->iterations / coverage->skips << '\n';
    }
    // A refusal is a fault rather than a variation: it says the four bytes
    // this model describes were not at the address it was pointed at.
    if (coverage->refusals != 0U) {
        std::cout << "  the watched address did not hold the spin this "
                     "model recognises\n";
    }
}

void report_pad_coverage(
    const devils_crush::emulation::PceRuntime& runtime)
{
    const auto coverage = runtime.native_pad_coverage();
    if (!coverage) {
        std::cout << "The emulator owned the controller port; no native "
                     "paths to report.\n";
        return;
    }

    std::cout << "Controller port:\n"
              << "  port: " << coverage->reads << " reads, "
              << coverage->writes << " writes, " << coverage->frames
              << " frame latches (" << coverage->frame_starts
              << " at a frame start), " << coverage->timestamp_rebases
              << " rebases, " << coverage->transforms << " transforms, "
              << coverage->powers << " resets\n"
              << "  routing: " << coverage->multitap_reads
              << " through the multitap, " << coverage->direct_reads
              << " straight to the first port, "
              << coverage->reads_past_the_last_port << " past the last "
              << "device, " << coverage->absent_port_reads
              << " of an empty port\n"
              << "  halves: " << coverage->select_low_reads
              << " with select low, " << coverage->select_high_reads
              << " with select high, " << coverage->six_button_reads
              << " of a six-button pad\n"
              << "  input: " << coverage->reads_with_a_button_down
              << " reads answered with a button held\n"
              << "  strobe: " << coverage->counter_resets
              << " counter resets, " << coverage->counter_increments
              << " increments, " << coverage->counter_saturations
              << " saturations, " << coverage->six_button_toggles
              << " half-selector toggles\n"
              << "  filtered: " << coverage->soft_resets_suppressed
              << " soft resets, " << coverage->opposing_directions_filtered
              << " opposing directions\n";

    // A route exercises what the game asks the port for, which on a
    // single-player two-button machine is a proper subset of the port. Saying
    // which subset is the point of the counters above; these two are the ones
    // whose absence is expected and whose presence would be news.
    if (coverage->six_button_reads == 0U) {
        std::cout << "  the six-button read path was never reached, which is "
                     "a two-button pad\n";
    }
    // The port answered every read with nothing held. On a route that presses
    // buttons that is not a quiet port, it is input that never arrived.
    if (coverage->reads_with_a_button_down == 0U && coverage->reads != 0U) {
        std::cout << "  no read answered with a button held\n";
    }
    // Unlike the rest, this one is a fault: a port carrying a device this
    // model does not answer for has been answering as an empty port.
    if (coverage->unmodelled_port_devices != 0U) {
        std::cout << "  " << coverage->unmodelled_port_devices
                  << " port(s) hold a device this model does not answer "
                     "for\n";
    }
}

} // namespace devils_crush::app::host
