// SPDX-License-Identifier: GPL-3.0-or-later

// The native instruction boundary against the emulator's own -- not sampled,
// enumerated.
//
// Every differential test in this series so far has driven a device through
// a stimulus and compared what came out, and every one of them has had the
// same unstated weakness: a stimulus reaches what it reaches. This device is
// small enough not to need one. The boundary holds two latched values and
// otherwise computes, so its inputs can be counted -- four bits of request
// register, three of mask, one of I flag, and the two latched values -- and
// counted inputs can be exhausted.
//
// So this test enumerates. Every latch the machine can take, and then every
// decision the boundary can make from every state it can be left in, against
// the emulator's own rule for each. A disagreement anywhere is a
// disagreement here, and there is no argument to make afterwards about
// whether the stimulus was representative.
//
// The rule on the other side is the machine's own. Patch 0016 lifts the
// sampling out of `LastCycle` and the vector selection out of `RunSub` into
// two entry points that the emulator itself calls, so what is being compared
// against is the code that runs when the game runs, not a transcription of
// it.
//
// Three asymmetries are checked by name as well as by enumeration, because
// each is a thing a from-scratch processor would plausibly get wrong and
// each would be invisible in a summary of the enumeration:
//
//   the reset line is never sampled, because the mask is three bits wide;
//   the timer line is tested live where IRQ1 and IRQ2 are tested latched;
//   the I flag closes the latch *and* gates the timer branch, in that order
//   and in two different tenses.
//
// One thing is deliberately not compared: the taking of the vector. Eight
// cycles, three pushes and a two-byte fetch are the processor's work against
// the processor's own stack, and the model has no opinion about any of it.
// What the model says is which vector, and that is what is enumerated.

#include "dc_beetle_reference_boundary.h"
#include "devils_crush/cpu/huc6280_interrupt_boundary.hpp"

#include <cstdarg>
#include <cstdint>
#include <cstdio>

namespace {

using devils_crush::cpu::HuC6280InterruptBoundary;

using Decision = HuC6280InterruptBoundary::Decision;
using Lines = HuC6280InterruptBoundary::Lines;

bool g_failed = false;

void check(const bool condition, const char* const label)
{
    if (!condition) {
        g_failed = true;
        std::printf("  FAIL  %s\n", label);
    } else {
        std::printf("  ok    %s\n", label);
    }
}

void section(const char* const name)
{
    std::printf("\n%s\n", name);
}

// The request register's four meaningful bits live below 0x40, so this is
// every value a line state can take plus the two bits between them that
// nothing drives -- included because a rule that reacted to one of them
// would be worth knowing about.
constexpr std::uint32_t kRequestStates = 0x40;

// Every mask the machine can hold. `IRQStatusWrite` writes `(V & 7) ^ 7` and
// the reset acknowledge writes 7, so three bits is the whole of it.
constexpr unsigned kMaskStates = 8;

// What the emulator's decision values are, so the two sides can be compared
// as one enumeration rather than through a translation each way.
int reference_value(const Decision decision)
{
    switch (decision) {
    case Decision::kInstruction:
        return DC_BEETLE_REFERENCE_BOUNDARY_INSTRUCTION;
    case Decision::kReset:
        return DC_BEETLE_REFERENCE_BOUNDARY_RESET;
    case Decision::kTimer:
        return DC_BEETLE_REFERENCE_BOUNDARY_TIMER;
    case Decision::kIrq1:
        return DC_BEETLE_REFERENCE_BOUNDARY_IRQ1;
    case Decision::kIrq2:
        return DC_BEETLE_REFERENCE_BOUNDARY_IRQ2;
    }

    return -1;
}

const char* decision_name(const Decision decision)
{
    switch (decision) {
    case Decision::kInstruction:
        return "instruction";
    case Decision::kReset:
        return "reset";
    case Decision::kTimer:
        return "timer";
    case Decision::kIrq1:
        return "irq1";
    case Decision::kIrq2:
        return "irq2";
    }

    return "?";
}

// A running tally, so a failure names the first moment rather than a count.
struct Tally {
    unsigned long long comparisons = 0;
    unsigned long long mismatches = 0;
    char first[192] = {};
};

void disagree(Tally& tally, const char* const format, ...)
{
    tally.mismatches++;

    if (tally.first[0] != '\0') {
        return;
    }

    std::va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(tally.first, sizeof(tally.first), format, arguments);
    va_end(arguments);
}

void report(const Tally& tally, const char* const label)
{
    if (tally.mismatches != 0) {
        std::printf("  first disagreement: %s\n", tally.first);
    }

    std::printf(
        "  %llu comparisons, %llu disagreements\n",
        tally.comparisons,
        tally.mismatches);
    check(tally.mismatches == 0, label);
}

void report_coverage(const HuC6280InterruptBoundary& boundary)
{
    const auto& coverage = boundary.coverage();

    std::printf("\nInstruction boundary paths:\n");
    std::printf(
        "  latches           %llu (%llu pending, %llu behind the I flag)\n",
        static_cast<unsigned long long>(coverage.samples),
        static_cast<unsigned long long>(coverage.pending_samples),
        static_cast<unsigned long long>(coverage.masked_samples));
    std::printf(
        "  decisions         %llu\n",
        static_cast<unsigned long long>(coverage.decisions));
    std::printf(
        "    instruction     %llu (%llu declined a woken boundary)\n",
        static_cast<unsigned long long>(coverage.instructions),
        static_cast<unsigned long long>(coverage.declined));
    std::printf(
        "    reset           %llu (%llu signalled, %llu acknowledged)\n",
        static_cast<unsigned long long>(coverage.resets),
        static_cast<unsigned long long>(coverage.reset_signals),
        static_cast<unsigned long long>(coverage.reset_acknowledgements));
    std::printf(
        "    timer           %llu (%llu on a line the latch did not hold)\n",
        static_cast<unsigned long long>(coverage.timers),
        static_cast<unsigned long long>(coverage.late_timers));
    std::printf(
        "    irq1            %llu\n",
        static_cast<unsigned long long>(coverage.irq1s));
    std::printf(
        "    irq2            %llu\n",
        static_cast<unsigned long long>(coverage.irq2s));
}

}  // namespace

int main()
{
    dc_beetle_reference_boundary* const ref =
        dc_beetle_reference_boundary_create();

    if (ref == nullptr) {
        std::printf("could not claim the emulator's processor\n");
        return 1;
    }

    HuC6280InterruptBoundary native;
    dc_beetle_reference_boundary_observation observation;

    section("the latch, over every input a machine can present");
    {
        // `LastCycle` on both sides: the lines the mask admits, and nothing
        // at all if the I flag is set. 1,024 inputs is the whole of it.
        Tally tally;

        for (std::uint32_t requests = 0; requests < kRequestStates;
             requests++) {
            for (unsigned mask = 0; mask < kMaskStates; mask++) {
                for (unsigned flag = 0; flag < 2; flag++) {
                    const Lines lines{
                        requests, static_cast<std::uint8_t>(mask)};
                    const bool i_flag = flag != 0;

                    dc_beetle_reference_boundary_set_lines(
                        ref, requests, static_cast<std::uint8_t>(mask));
                    dc_beetle_reference_boundary_set_i_flag(
                        ref, static_cast<int>(flag));
                    dc_beetle_reference_boundary_sample(ref);
                    dc_beetle_reference_boundary_observe(ref, &observation);

                    native.sample(lines, i_flag);

                    tally.comparisons++;

                    if (static_cast<std::int32_t>(native.irq_sample())
                            != observation.irq_sample
                        || native.i_flag_sample()
                            != (observation.i_flag_sample != 0)) {
                        disagree(
                            tally,
                            "lines %02X mask %u flag %u: reference %ld/%ld,"
                            " native %lu/%d",
                            requests,
                            mask,
                            flag,
                            static_cast<long>(observation.irq_sample),
                            static_cast<long>(observation.i_flag_sample),
                            static_cast<unsigned long>(native.irq_sample()),
                            native.i_flag_sample() ? 1 : 0);
                    }

                    if (HuC6280InterruptBoundary::latch(lines, i_flag)
                        != native.irq_sample()) {
                        disagree(
                            tally,
                            "lines %02X mask %u flag %u: the pure latch and"
                            " the latching one disagree",
                            requests,
                            mask,
                            flag);
                    }
                }
            }
        }

        report(tally, "every latch the machine can take agrees");
    }

    section("the decision, over every state the boundary can be left in");
    {
        // Both latched values are enumerated rather than reached, because
        // one of the states that matters most -- the reset line in the latch
        // -- is not reachable by sampling at all, and because a savestate
        // can leave the boundary anywhere.
        Tally tally;
        unsigned long long timers_on_an_unlatched_line = 0;

        for (std::uint32_t sample = 0; sample < kRequestStates; sample++) {
            for (unsigned latched_flag = 0; latched_flag < 2;
                 latched_flag++) {
                const std::int32_t i_flag_sample = latched_flag != 0
                    ? HuC6280InterruptBoundary::kIFlag
                    : 0;

                for (std::uint32_t requests = 0; requests < kRequestStates;
                     requests++) {
                    for (unsigned mask = 0; mask < kMaskStates; mask++) {
                        const Lines lines{
                            requests, static_cast<std::uint8_t>(mask)};

                        dc_beetle_reference_boundary_set_lines(
                            ref, requests, static_cast<std::uint8_t>(mask));
                        dc_beetle_reference_boundary_set_sample(
                            ref,
                            static_cast<std::int32_t>(sample),
                            i_flag_sample);

                        const int reference =
                            dc_beetle_reference_boundary_select(ref);

                        HuC6280InterruptBoundary::State state;
                        state.irq_sample =
                            static_cast<std::int32_t>(sample);
                        state.i_flag_sample = i_flag_sample;
                        native.apply_state(state);

                        const Decision decision = native.select(lines);
                        const Decision pure = HuC6280InterruptBoundary::decide(
                            sample, latched_flag != 0, lines);

                        tally.comparisons++;

                        if (reference_value(decision) != reference) {
                            disagree(
                                tally,
                                "sample %02X flag %u lines %02X mask %u:"
                                " reference %d, native %s",
                                sample,
                                latched_flag,
                                requests,
                                mask,
                                reference,
                                decision_name(decision));
                        }

                        if (pure != decision) {
                            disagree(
                                tally,
                                "sample %02X flag %u lines %02X mask %u: the"
                                " pure rule and the latched one disagree",
                                sample,
                                latched_flag,
                                requests,
                                mask);
                        }

                        if (HuC6280InterruptBoundary::vector(decision)
                            != dc_beetle_reference_boundary_vector(
                                reference)) {
                            disagree(
                                tally,
                                "sample %02X flag %u lines %02X mask %u: the"
                                " vectors disagree",
                                sample,
                                latched_flag,
                                requests,
                                mask);
                        }

                        if (decision == Decision::kTimer
                            && (sample & HuC6280InterruptBoundary::kTimer)
                                == 0) {
                            timers_on_an_unlatched_line++;
                        }
                    }
                }
            }
        }

        report(tally, "every decision the boundary can make agrees");
        check(
            timers_on_an_unlatched_line > 0
                && native.coverage().late_timers
                    == timers_on_an_unlatched_line,
            "and the enumeration reached the timer's live line, which is"
            " the asymmetry that makes this rule worth modelling");
    }

    section("the reset line is a state, not a sample");
    {
        // The mask is three bits wide, so `requests & mask` cannot produce
        // the reset line however the lines are driven. Nothing but `Reset`
        // ever puts it in the latch, and nothing but the acknowledge takes
        // it out.
        bool ever_latched = false;

        for (std::uint32_t requests = 0; requests < kRequestStates;
             requests++) {
            for (unsigned mask = 0; mask < kMaskStates; mask++) {
                const Lines lines{requests, static_cast<std::uint8_t>(mask)};

                if ((HuC6280InterruptBoundary::latch(lines, false)
                     & HuC6280InterruptBoundary::kReset)
                    != 0) {
                    ever_latched = true;
                }
            }
        }

        check(
            !ever_latched,
            "no line state and no open mask can sample it, because the mask"
            " is three bits and the line is the sixth");

        HuC6280InterruptBoundary fresh;
        fresh.reset();
        dc_beetle_reference_boundary_reset(ref);
        dc_beetle_reference_boundary_observe(ref, &observation);

        check(
            fresh.irq_sample() == HuC6280InterruptBoundary::kReset
                && static_cast<std::int32_t>(fresh.irq_sample())
                    == observation.irq_sample,
            "a reset puts it there, on both sides");
        check(
            fresh.select(Lines{0, 0}) == Decision::kReset,
            "and a boundary left holding it decides for the reset vector"
            " even with every line quiet and every mask closed");

        fresh.acknowledge_reset();
        dc_beetle_reference_boundary_acknowledge_reset(ref);
        dc_beetle_reference_boundary_observe(ref, &observation);

        check(
            fresh.irq_sample() == 0
                && static_cast<std::int32_t>(fresh.irq_sample())
                    == observation.irq_sample,
            "taking the vector takes it back out, on both sides");
    }

    section("the I flag is consulted twice, in two tenses");
    {
        HuC6280InterruptBoundary fresh;

        const Lines armed{
            HuC6280InterruptBoundary::kTimer
                | HuC6280InterruptBoundary::kIrq1,
            static_cast<std::uint8_t>(HuC6280InterruptBoundary::kMaskable)};

        fresh.sample(armed, true);
        check(
            fresh.irq_sample() == 0,
            "a sample taken behind a raised flag latches nothing, whatever"
            " the lines are doing");
        check(
            fresh.select(armed) == Decision::kInstruction,
            "so the boundary that follows it declines both lines -- the"
            " timer because the latched flag gates it, the others because"
            " there is nothing latched to admit");

        fresh.sample(armed, false);
        check(
            fresh.select(armed) == Decision::kTimer,
            "and the same lines behind a lowered flag take the timer, which"
            " outranks IRQ1");
    }

    section("the timer is live and the other two are latched");
    {
        HuC6280InterruptBoundary fresh;

        const Lines quiet{
            0, static_cast<std::uint8_t>(HuC6280InterruptBoundary::kMaskable)};

        fresh.sample(quiet, false);

        const Lines late_timer{
            HuC6280InterruptBoundary::kTimer,
            static_cast<std::uint8_t>(HuC6280InterruptBoundary::kMaskable)};
        check(
            fresh.select(late_timer) == Decision::kTimer,
            "a timer request raised after the latch is taken at once");

        fresh.sample(quiet, false);

        const Lines late_irq1{
            HuC6280InterruptBoundary::kIrq1,
            static_cast<std::uint8_t>(HuC6280InterruptBoundary::kMaskable)};
        check(
            fresh.select(late_irq1) == Decision::kInstruction,
            "an IRQ1 request raised in the same window waits for the next"
            " boundary");

        fresh.sample(late_irq1, false);
        check(
            fresh.select(quiet) == Decision::kIrq1,
            "and is taken there, from the latch, after its own line has"
            " already gone away again");

        fresh.sample(late_timer, false);
        check(
            fresh.select(quiet) == Decision::kInstruction,
            "where a latched timer whose line has gone away is not taken at"
            " all, which is the same asymmetry read backwards");
    }

    section("priority, where more than one line is admitted");
    {
        HuC6280InterruptBoundary fresh;

        const std::uint8_t open =
            static_cast<std::uint8_t>(HuC6280InterruptBoundary::kMaskable);
        const Lines all{HuC6280InterruptBoundary::kMaskable, open};

        fresh.sample(all, false);
        check(
            fresh.select(all) == Decision::kTimer
                && HuC6280InterruptBoundary::vector(Decision::kTimer)
                    == HuC6280InterruptBoundary::kTimerVector,
            "the timer outranks both others");

        const Lines two{
            HuC6280InterruptBoundary::kIrq1 | HuC6280InterruptBoundary::kIrq2,
            open};
        fresh.sample(two, false);
        check(
            fresh.select(two) == Decision::kIrq1
                && HuC6280InterruptBoundary::vector(Decision::kIrq1)
                    == HuC6280InterruptBoundary::kIrq1Vector,
            "and IRQ1 outranks IRQ2");

        fresh.reset();
        check(
            fresh.select(all) == Decision::kReset
                && HuC6280InterruptBoundary::vector(Decision::kReset)
                    == HuC6280InterruptBoundary::kResetVector,
            "and a reset outranks everything, from a latch nothing else"
            " can be in at the same time");
    }

    section("the shuttle");
    {
        HuC6280InterruptBoundary source;
        source.sample(
            Lines{
                HuC6280InterruptBoundary::kIrq2,
                static_cast<std::uint8_t>(
                    HuC6280InterruptBoundary::kMaskable)},
            false);

        HuC6280InterruptBoundary::State state;
        source.capture_state(state);

        HuC6280InterruptBoundary restored;
        restored.apply_state(state);

        check(
            restored.irq_sample() == source.irq_sample()
                && restored.i_flag_sample() == source.i_flag_sample(),
            "both fields a savestate carries survive a round trip");
        check(
            state.i_flag_sample == 0,
            "and the latched flag is carried in the emulator's encoding");

        HuC6280InterruptBoundary::State raised;
        raised.i_flag_sample = 1;
        restored.apply_state(raised);

        HuC6280InterruptBoundary::State written;
        restored.capture_state(written);

        check(
            restored.i_flag_sample()
                && written.i_flag_sample
                    == HuC6280InterruptBoundary::kIFlag,
            "a latched flag arriving in any other encoding is normalised to"
            " it, because `P & I_FLAG` is the only thing that writes it");
    }

    report_coverage(native);

    dc_beetle_reference_boundary_destroy(ref);

    std::printf("\n%s\n", g_failed ? "FAILED" : "PASSED");
    return g_failed ? 1 : 0;
}
