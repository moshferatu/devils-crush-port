// SPDX-License-Identifier: GPL-3.0-or-later

// The native timer and interrupt controller against the emulator's own,
// driven side by side from one stolen-cycle stimulus.
//
// The sound chip and the video controller could be lifted out of the
// emulator whole. The colour encoder could not, and its reference arm was
// the smallest machine it would run in. This device is a step further
// again: it is not an object at all, but a dozen fields inside `HuC6280`
// and four methods the machine's I/O page handler calls from outside. So
// the reference arm here is the emulator's own processor, initialised and
// never executed, moved forward by stolen master cycles.
//
// That substitution is exact rather than approximate, and for a reason
// worth stating. The timer's entire relationship with time is one phase
// against the processor's clock, and that clock advances whether or not
// instructions are running. A stolen cycle and an executed instruction are
// indistinguishable to this device.
//
// Stealing cycles also runs the processor's own `HappySync` whenever the
// divider comes due, so the emulator catches its timer up at the moments a
// real machine would -- sometimes several times inside one call. The native
// model catches up once, at the end. Proving those two agree is most of
// what this file is for, and it is not a formality: the tick that carries a
// reload is one clock short and the tick before it is one clock long, so a
// catch-up that lands inside that pair has to come out in the same place as
// one that steps through it.
//
// Nothing here is sampled from outside -- no ROM, no input, no wall clock --
// so both arms are pure functions of this file. The device is small enough
// that the stimulus can reach all of it, so unlike the video chips' tests
// this is closer to an enumeration than a sample: every one of the 128
// reload values is driven to its underflow, and every mask value is written.
//
// Three things are deliberately not compared. `IRQSample`, because it is
// the processor's latched copy of the masked pending set rather than
// anything this device holds -- it is read out only so a fault in the
// controller can be told apart from a fault in the sampling. Which vector
// an interrupt takes, because that is decided at an instruction boundary
// and nothing here executes an instruction. And the emulator's own
// persistence of these fields, which needs a savestate writer this shim has
// no reason to carry; the model's shuttle is round-tripped against itself
// and its one documented repair is checked directly.

#include "dc_beetle_reference_irq.h"
#include "devils_crush/cpu/huc6280_irq.hpp"

#include <cstdint>
#include <cstdio>

namespace {

using devils_crush::cpu::HuC6280Irq;

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

// The two register files, at the addresses the machine's I/O page handler
// sees after masking.
constexpr std::uint32_t kTimerCounter = 0x0C00;
constexpr std::uint32_t kTimerControl = 0x0C01;
constexpr std::uint32_t kIrqDisable = 0x1400;
constexpr std::uint32_t kIrqStatusUnmapped = 0x1401;
constexpr std::uint32_t kIrqMask = 0x1402;
constexpr std::uint32_t kIrqStatus = 0x1403;

// One tick of the timer in master clocks, and the longest a full period can
// be: 128 ticks of countdown plus the reload.
constexpr std::int32_t kTick = HuC6280Irq::kTickClocks;

// The two arms, and the I/O bus latch the native one does not own. The
// emulator holds that latch inside the processor and every register access
// here goes through it, so the test holds it explicitly -- which is the
// seam the model deliberately leaves open, made visible.
struct Pair {
    dc_beetle_reference_irq* ref = nullptr;
    HuC6280Irq native;
    std::uint8_t io = 0;
    std::int32_t ts = 0;

    unsigned long long comparisons = 0;
    unsigned long long mismatches = 0;
    // The first disagreement, kept so a failure names a moment rather than
    // a count.
    char first_mismatch[192] = {};
};

void record_mismatch(
    Pair& pair,
    const char* const where,
    const char* const field,
    const long long reference,
    const long long native)
{
    pair.mismatches++;
    if (pair.first_mismatch[0] == '\0') {
        std::snprintf(
            pair.first_mismatch,
            sizeof(pair.first_mismatch),
            "%s: %s reference %lld native %lld (t=%ld)",
            where,
            field,
            reference,
            native,
            static_cast<long>(pair.ts));
    }
}

// Compares every field of the device. `next_event` is only meaningful after
// something recomputed it, which is exactly where the emulator's own
// callers call `CalcNextEvent`, so the caller says whether to look.
void compare(Pair& pair, const char* const where, const bool horizon_fresh)
{
    dc_beetle_reference_irq_observation observed{};
    dc_beetle_reference_irq_observe(pair.ref, &observed);

    pair.comparisons++;

    if (observed.timer_div != pair.native.divider()) {
        record_mismatch(
            pair, where, "timer_div", observed.timer_div,
            pair.native.divider());
    }
    if (observed.timer_value != pair.native.counter()) {
        record_mismatch(
            pair, where, "timer_value", observed.timer_value,
            pair.native.counter());
    }
    if (observed.timer_load != pair.native.reload()) {
        record_mismatch(
            pair, where, "timer_load", observed.timer_load,
            pair.native.reload());
    }
    if ((observed.timer_status != 0) != pair.native.running()) {
        record_mismatch(
            pair, where, "timer_status", observed.timer_status,
            pair.native.running() ? 1 : 0);
    }
    if ((observed.timer_inreload != 0) != pair.native.reloading()) {
        record_mismatch(
            pair, where, "timer_inreload", observed.timer_inreload,
            pair.native.reloading() ? 1 : 0);
    }
    if (observed.timer_lastts != pair.native.last_timestamp()) {
        record_mismatch(
            pair, where, "timer_lastts", observed.timer_lastts,
            pair.native.last_timestamp());
    }
    if (observed.irq_low != pair.native.requests()) {
        record_mismatch(
            pair, where, "irq_low", observed.irq_low,
            pair.native.requests());
    }
    if (observed.irq_mask != pair.native.mask()) {
        record_mismatch(
            pair, where, "irq_mask", observed.irq_mask, pair.native.mask());
    }
    if (horizon_fresh && observed.next_event != pair.native.next_event()) {
        record_mismatch(
            pair, where, "next_event", observed.next_event,
            pair.native.next_event());
    }
    const std::uint8_t latch =
        dc_beetle_reference_irq_io_data_buffer(pair.ref);
    if (latch != pair.io) {
        record_mismatch(pair, where, "io_data_buffer", latch, pair.io);
    }
}

void init(Pair& pair)
{
    dc_beetle_reference_irq_init(pair.ref);
    pair.native.init();
    // `HuC6280::Init` rebases the processor's clock, which is the only
    // reason this is not just another write to the device.
    pair.ts = 0;
    compare(pair, "init", true);
}

void power(Pair& pair)
{
    dc_beetle_reference_irq_power(pair.ref);
    pair.native.power();
    // `HuC6280::Power` drives the bus latch high, which is the one thing it
    // does that this device does not own.
    pair.io = 0xFF;
    compare(pair, "power", true);
}

void reset(Pair& pair)
{
    dc_beetle_reference_irq_reset(pair.ref);
    pair.native.reset();
    compare(pair, "reset", false);
}

void acknowledge_reset(Pair& pair)
{
    dc_beetle_reference_irq_acknowledge_reset(pair.ref);
    pair.native.acknowledge_reset();
    compare(pair, "acknowledge reset", false);
}

void advance(Pair& pair, const std::int32_t clocks)
{
    pair.ts = static_cast<std::int32_t>(
        dc_beetle_reference_irq_advance(pair.ref, clocks));
    pair.native.sync(pair.ts);
    compare(pair, "advance", true);
}

void rebase(Pair& pair, const std::int32_t base)
{
    dc_beetle_reference_irq_reset_timestamp(
        pair.ref, static_cast<std::uint32_t>(base));
    pair.native.sync(pair.ts);
    pair.native.reset_timestamp(base);
    pair.ts = base;
    compare(pair, "rebase", true);
}

std::uint8_t read_timer(Pair& pair, const std::uint32_t address)
{
    const std::uint8_t reference =
        dc_beetle_reference_irq_read_timer(pair.ref, address);

    pair.native.sync(pair.ts);
    const std::uint8_t native = pair.native.read_timer(pair.io);
    pair.io = native;

    if (reference != native) {
        record_mismatch(pair, "read timer", "value", reference, native);
    }
    compare(pair, "read timer", true);
    return native;
}

void write_timer(
    Pair& pair,
    const std::uint32_t address,
    const std::uint8_t value)
{
    dc_beetle_reference_irq_write_timer(pair.ref, address, value);

    pair.io = value;
    pair.native.sync(pair.ts);
    pair.native.write_timer(address, value);

    compare(pair, "write timer", true);
}

std::uint8_t read_status(Pair& pair, const std::uint32_t address)
{
    const std::uint8_t reference =
        dc_beetle_reference_irq_read_status(pair.ref, address);

    const std::uint8_t native = pair.native.read_status(address, pair.io);
    pair.io = native;

    if (reference != native) {
        record_mismatch(pair, "read status", "value", reference, native);
    }
    compare(pair, "read status", false);
    return native;
}

std::uint8_t peek_status(Pair& pair, const std::uint32_t address)
{
    const std::uint8_t reference =
        dc_beetle_reference_irq_peek_status(pair.ref, address);
    const std::uint8_t native = pair.native.peek_status(address, pair.io);

    if (reference != native) {
        record_mismatch(pair, "peek status", "value", reference, native);
    }
    compare(pair, "peek status", false);
    return native;
}

void write_status(
    Pair& pair,
    const std::uint32_t address,
    const std::uint8_t value)
{
    dc_beetle_reference_irq_write_status(pair.ref, address, value);

    pair.io = value;
    pair.native.write_status(address, value);

    compare(pair, "write status", false);
}

void begin_request(Pair& pair, const std::uint32_t lines)
{
    dc_beetle_reference_irq_begin(pair.ref, lines);
    pair.native.begin_request(lines);
    compare(pair, "begin request", false);
}

void end_request(Pair& pair, const std::uint32_t lines)
{
    dc_beetle_reference_irq_end(pair.ref, lines);
    pair.native.end_request(lines);
    compare(pair, "end request", false);
}

std::uint32_t horizon(Pair& pair, const bool i_flag)
{
    dc_beetle_reference_irq_set_i_flag(pair.ref, i_flag ? 1 : 0);

    const std::uint32_t reference =
        dc_beetle_reference_irq_timer_horizon(pair.ref);
    const std::uint32_t native =
        pair.native.master_cycles_until_timer_interrupt(pair.ts, i_flag);

    pair.comparisons++;
    if (reference != native) {
        record_mismatch(
            pair, "timer horizon", "clocks", reference, native);
    }
    return native;
}

// Brings both arms up the way the machine does: power, then the reset
// vector, which is the only thing that ever opens the interrupt mask.
void bring_up(Pair& pair)
{
    power(pair);
    acknowledge_reset(pair);
    rebase(pair, 0);
}

// A timer started from stopped: the reload byte, then the enable bit.
void start_timer(Pair& pair, const std::uint8_t load)
{
    write_timer(pair, kTimerCounter, load);
    write_timer(pair, kTimerControl, 0x00);
    write_timer(pair, kTimerControl, 0x01);
}

std::uint64_t g_lcg = 0x1234567890ABCDEFULL;

std::uint32_t next_random(void)
{
    g_lcg = g_lcg * 6364136223846793005ULL + 1442695040888963407ULL;
    return static_cast<std::uint32_t>(g_lcg >> 33);
}

}  // namespace

int main(void)
{
    dc_beetle_reference_irq* const reference =
        dc_beetle_reference_irq_create();
    if (reference == nullptr) {
        std::printf("could not create the reference device\n");
        return 1;
    }

    Pair pair;
    pair.ref = reference;

    section("power-on and the reset vector");
    {
        bring_up(pair);

        check(
            pair.native.divider() == 1024,
            "reset leaves the divider a third of a period short");
        check(
            pair.native.requests() == 0 && pair.native.mask() == 0x7,
            "taking the reset vector releases the line and opens the mask");
        check(!pair.native.running(), "and the timer comes up stopped");

        // `Reset` assigns the request register rather than adding to it, so
        // a line asserted beforehand is dropped rather than surviving.
        begin_request(pair, HuC6280Irq::kIrq1);
        reset(pair);
        check(
            pair.native.requests() == HuC6280Irq::kReset,
            "and reset drops any request already in flight");

        bring_up(pair);
    }

    section("the machine builds itself again");
    {
        // Every ROM the machine loads runs `Init` on a processor that has
        // already been running. It is not a reset: it puts the divider back
        // to zero and the phase back to the origin of a clock that has just
        // been rebased, and leaves the request register and the mask alone.
        // A device that ignored it would measure its first catch-up on the
        // new machine against the old machine's timestamp.
        start_timer(pair, 40);
        advance(pair, kTick * 3 + 17);
        begin_request(pair, HuC6280Irq::kIrq2);

        init(pair);
        check(
            pair.native.divider() == 0 && pair.native.last_timestamp() == 0,
            "init leaves the divider at zero and the phase at the origin");
        check(
            !pair.native.running() && pair.native.counter() == 0
                && pair.native.reload() == 0,
            "and the timer stopped, unloaded and uncounted");
        check(
            pair.native.requests() == HuC6280Irq::kIrq2
                && pair.native.mask() == 0x7,
            "while the request register and the mask survive it");

        // And a divider left at zero crosses its tick on the very next
        // clock, which is one short of a whole period rather than a whole
        // one -- the same arithmetic every other tick uses.
        advance(pair, 1);
        check(
            pair.native.divider() == kTick - 1,
            "a divider left at zero crosses its tick on the next clock");

        bring_up(pair);
    }

    section("the divider free-runs with the timer stopped");
    {
        // The 1024 left by reset, then whole periods.
        advance(pair, 1024);
        check(
            pair.native.divider() == kTick,
            "the short first tick is followed by a whole one");

        for (int i = 0; i < 4; i++) {
            advance(pair, kTick);
        }
        // One call spanning several ticks, against four that each span one.
        advance(pair, kTick * 5);

        check(
            pair.native.counter() == 0 && pair.native.requests() == 0,
            "a stopped timer neither counts nor asks for anything");
    }

    section("every reload value counts down to its underflow");
    {
        unsigned long long underflow_faults = 0;
        unsigned long long request_faults = 0;

        for (int load = 0; load <= HuC6280Irq::kCounterMask; load++) {
            bring_up(pair);
            start_timer(pair, static_cast<std::uint8_t>(load));

            if (pair.native.counter() != load) {
                underflow_faults++;
            }

            // Step tick by tick until the request appears. A counter loaded
            // with n underflows on the (n+1)th tick, because it decrements
            // to -1 rather than to zero.
            int ticks = 0;
            while ((pair.native.requests() & HuC6280Irq::kTimer) == 0
                && ticks <= HuC6280Irq::kCounterMask + 4) {
                advance(pair, kTick);
                ticks++;
            }

            if (ticks != load + 1) {
                underflow_faults++;
            }
            if ((pair.native.requests() & HuC6280Irq::kTimer) == 0) {
                request_faults++;
            }

            // The tick after an underflow is one clock long and the reload
            // it carries is one clock short, so the pair still costs a
            // whole period.
            if (pair.native.divider() != 1) {
                underflow_faults++;
            }
            advance(pair, 1);
            if (pair.native.divider() != kTick - 1
                || pair.native.counter() != load
                || pair.native.reloading()) {
                underflow_faults++;
            }
        }

        check(
            underflow_faults == 0,
            "all 128 reload values underflow on the tick they should");
        check(
            request_faults == 0,
            "and each one leaves the timer's request asserted");
    }

    section("a catch-up across a reload equals stepping through it");
    {
        // Two devices reaching the same place by different routes is the
        // one thing the emulator's own intermediate syncs make hard to
        // fake, so it is driven from both directions.
        bring_up(pair);
        start_timer(pair, 3);
        advance(pair, kTick * 4 + kTick - 1 + kTick * 2 + 17);

        const std::int32_t divider_at_once = pair.native.divider();
        const std::int32_t counter_at_once = pair.native.counter();

        bring_up(pair);
        start_timer(pair, 3);
        for (int i = 0; i < 4; i++) {
            advance(pair, kTick);
        }
        advance(pair, kTick - 1);
        advance(pair, kTick);
        advance(pair, kTick);
        advance(pair, 17);

        check(
            pair.native.divider() == divider_at_once
                && pair.native.counter() == counter_at_once,
            "one long catch-up lands where eight short ones do");
    }

    section("starting, stopping and restarting the timer");
    {
        bring_up(pair);
        start_timer(pair, 40);
        advance(pair, kTick * 10 + 100);

        const std::int32_t mid = pair.native.counter();

        // Writing the enable bit again while it is already set must do
        // nothing at all: the game polls this register.
        write_timer(pair, kTimerControl, 0x01);
        check(
            pair.native.counter() == mid,
            "re-enabling a running timer does not reload it");

        write_timer(pair, kTimerControl, 0x00);
        advance(pair, kTick * 4);
        check(
            pair.native.counter() == mid,
            "and a stopped timer holds its counter");

        write_timer(pair, kTimerControl, 0x01);
        check(
            pair.native.counter() == 40 && pair.native.divider() == kTick,
            "restarting reloads the counter and restarts the divider");

        // The reload byte is seven bits.
        write_timer(pair, kTimerCounter, 0xFF);
        write_timer(pair, kTimerControl, 0x00);
        write_timer(pair, kTimerControl, 0x01);
        check(
            pair.native.reload() == 0x7F && pair.native.counter() == 0x7F,
            "the reload byte keeps seven bits");
    }

    section("the counter read merges the I/O bus latch");
    {
        bring_up(pair);
        start_timer(pair, 0x2A);

        write_status(pair, kIrqStatus, 0xFF);
        const std::uint8_t high = read_timer(pair, kTimerCounter);
        check(
            high == (0x2A | 0x80),
            "bit 7 comes off the bus and the rest off the counter");

        write_status(pair, kIrqStatus, 0x00);
        const std::uint8_t low = read_timer(pair, kTimerCounter);
        check(low == 0x2A, "and a low bus leaves the counter alone");

        check(
            read_timer(pair, kTimerControl) == low,
            "the counter answers at both addresses");

        // Between the underflow and the reload the counter holds -1, and a
        // read in that window returns every bit set.
        bring_up(pair);
        start_timer(pair, 0);
        advance(pair, kTick);
        check(
            pair.native.reloading() && pair.native.counter() == -1,
            "the counter sits at -1 while the reload is pending");
        check(
            read_timer(pair, kTimerCounter) == 0xFF,
            "and reads back as every bit set");
    }

    section("the interrupt mask");
    {
        bring_up(pair);

        unsigned long long mask_faults = 0;
        for (unsigned value = 0; value < 8; value++) {
            write_status(pair, kIrqMask, static_cast<std::uint8_t>(value));
            if (pair.native.mask() != ((value & 0x7) ^ 0x7)) {
                mask_faults++;
            }
            // The mask reads back in the form it was written, not the form
            // it is stored in.
            if (peek_status(pair, kIrqMask) != value) {
                mask_faults++;
            }
        }

        check(mask_faults == 0, "all eight mask values round-trip inverted");

        write_status(pair, kIrqMask, 0x00);
        begin_request(pair, HuC6280Irq::kTimer);
        check(
            pair.native.pending() == HuC6280Irq::kTimer,
            "an open mask passes the request through");

        write_status(pair, kIrqMask, 0x04);
        check(
            pair.native.requests() == HuC6280Irq::kTimer
                && pair.native.pending() == 0,
            "closing the mask hides a request without clearing it");
    }

    section("the status register and both acknowledge paths");
    {
        bring_up(pair);
        write_status(pair, kIrqMask, 0x00);

        begin_request(pair, HuC6280Irq::kIrq1);
        begin_request(pair, HuC6280Irq::kIrq2);
        begin_request(pair, HuC6280Irq::kTimer);

        check(
            (peek_status(pair, kIrqStatus) & 0x7) == 0x7,
            "all three lines report at once");

        // A write to $1403 acknowledges the timer and nothing else.
        write_status(pair, kIrqStatus, 0x00);
        check(
            pair.native.requests()
                == (HuC6280Irq::kIrq1 | HuC6280Irq::kIrq2),
            "a write to the status register clears only the timer");

        begin_request(pair, HuC6280Irq::kTimer);
        peek_status(pair, kIrqMask);
        check(
            (pair.native.requests() & HuC6280Irq::kTimer) != 0,
            "peeking the mask does not acknowledge");

        // Reading $1402 reports the mask and acknowledges as a side effect,
        // which is the hardware's behaviour and the game depends on it.
        read_status(pair, kIrqMask);
        check(
            (pair.native.requests() & HuC6280Irq::kTimer) == 0,
            "but reading it does");

        end_request(pair, HuC6280Irq::kIrq1);
        check(
            pair.native.requests() == HuC6280Irq::kIrq2,
            "and a device releasing its own line leaves the others");

        // Neither of the low two addresses is this device's.
        write_status(pair, kIrqDisable, 0x55);
        check(
            pair.native.mask() == 0x7
                && pair.native.requests() == HuC6280Irq::kIrq2,
            "a write below $1402 reaches nothing");
        check(
            read_status(pair, kIrqStatusUnmapped) == 0x55,
            "and a read below it returns the bus latch");
    }

    section("the timer interrupt horizon");
    {
        unsigned long long horizon_faults = 0;

        bring_up(pair);
        if (horizon(pair, false) != HuC6280Irq::kNoTimerInterrupt) {
            horizon_faults++;
        }

        start_timer(pair, 5);
        if (horizon(pair, false) != static_cast<std::uint32_t>(kTick * 6)) {
            horizon_faults++;
        }
        // The I flag closes it, and so does the mask.
        if (horizon(pair, true) != HuC6280Irq::kNoTimerInterrupt) {
            horizon_faults++;
        }
        write_status(pair, kIrqMask, 0x04);
        if (horizon(pair, false) != HuC6280Irq::kNoTimerInterrupt) {
            horizon_faults++;
        }
        write_status(pair, kIrqMask, 0x00);

        // Partway through a period, and across the reload window.
        advance(pair, kTick + 700);
        if (horizon(pair, false)
            != static_cast<std::uint32_t>(kTick * 5 - 700)) {
            horizon_faults++;
        }
        advance(pair, kTick * 5 - 700);
        // With the request already pending it is zero regardless of how far
        // away the next underflow is.
        if (horizon(pair, false) != 0) {
            horizon_faults++;
        }

        // Acknowledged, and sitting in the one-clock window between the
        // underflow and the reload: the countdown now starts from the
        // reload byte, one short tick away.
        write_status(pair, kIrqStatus, 0x00);
        if (!pair.native.reloading() || pair.native.divider() != 1) {
            horizon_faults++;
        }
        if (horizon(pair, false) != static_cast<std::uint32_t>(kTick * 6)) {
            horizon_faults++;
        }

        check(
            horizon_faults == 0,
            "the horizon agrees with the emulator's at every gate");

        const HuC6280Irq::Coverage& coverage = pair.native.coverage();
        check(
            coverage.horizon_never > 0 && coverage.horizon_now > 0,
            "and all three of its answers were reached");
    }

    section("the savestate shuttle");
    {
        // Driven on a model of its own. The emulator persists these fields
        // through a savestate writer this shim has no reason to carry, so
        // there is nothing to compare against here, and moving the shared
        // pair to an arbitrary state would only desynchronise it.
        HuC6280Irq shuttle;
        shuttle.power();
        shuttle.acknowledge_reset();
        shuttle.reset_timestamp(0);

        shuttle.write_timer(kTimerCounter, 61);
        shuttle.write_timer(kTimerControl, 0x01);
        shuttle.sync(kTick * 20 + 511);
        shuttle.begin_request(HuC6280Irq::kIrq1);
        shuttle.write_status(kIrqMask, 0x02);

        HuC6280Irq::State saved{};
        shuttle.capture_state(saved);

        const std::int32_t divider = shuttle.divider();
        const std::int32_t counter = shuttle.counter();
        const std::uint32_t requests = shuttle.requests();
        const std::uint8_t mask = shuttle.mask();

        // Run somewhere else entirely, then come back.
        shuttle.sync(kTick * 53 + 520);
        shuttle.write_timer(kTimerControl, 0x00);
        shuttle.end_request(HuC6280Irq::kIrq1);
        shuttle.write_status(kIrqMask, 0x07);

        shuttle.apply_state(saved);

        check(
            shuttle.divider() == divider && shuttle.counter() == counter
                && shuttle.requests() == requests
                && shuttle.mask() == mask && shuttle.running(),
            "a captured state restores every field it carries");

        // The one repair. A savestate may hold a divider at or below zero;
        // the device may not, because the next catch-up would climb back
        // out one tick at a time and manufacture interrupts that never
        // happened.
        HuC6280Irq::State broken = saved;
        broken.timer_div = -5000;
        shuttle.apply_state(broken);
        check(
            shuttle.divider() == 1,
            "and a divider a savestate should not hold is repaired");
    }

    section("a long walk over every entry point");
    {
        constexpr int kSteps = 200000;

        bring_up(pair);
        write_status(pair, kIrqMask, 0x00);

        for (int step = 0; step < kSteps; step++) {
            // Each draw is taken into a local first. Argument evaluation
            // order is unspecified, and a walk whose stimulus depended on
            // it would not be the same walk on a different compiler.
            const std::uint32_t roll = next_random();
            const std::uint32_t a = next_random();
            const std::uint32_t b = next_random();

            switch (roll % 10) {
                case 0:
                case 1:
                case 2:
                    // Short advances, which mostly stay inside one tick.
                    advance(pair, static_cast<std::int32_t>((a % 400) + 1));
                    break;

                case 3:
                case 4:
                    // Advances that land next to a tick boundary, which is
                    // where the short and the long tick live.
                    advance(
                        pair,
                        static_cast<std::int32_t>(kTick - 2 + (a % 5)));
                    break;

                case 5:
                    // Advances that cross several ticks at once, which is
                    // the case the emulator reaches by a different route.
                    advance(
                        pair,
                        static_cast<std::int32_t>(
                            kTick * (1 + (a % 6)) + (b % kTick)));
                    break;

                case 6:
                    write_timer(
                        pair,
                        (a & 1) ? kTimerControl : kTimerCounter,
                        static_cast<std::uint8_t>(b & 0xFF));
                    break;

                case 7:
                    read_timer(
                        pair, (a & 1) ? kTimerControl : kTimerCounter);
                    break;

                case 8:
                    if ((a & 1) != 0) {
                        write_status(
                            pair,
                            kIrqDisable + ((a >> 1) & 3),
                            static_cast<std::uint8_t>(b & 0xFF));
                    } else {
                        read_status(pair, kIrqDisable + ((a >> 1) & 3));
                    }
                    break;

                default:
                    if ((a & 1) != 0) {
                        begin_request(
                            pair,
                            (b & 1) ? HuC6280Irq::kIrq1 : HuC6280Irq::kIrq2);
                    } else {
                        end_request(
                            pair,
                            (b & 1) ? HuC6280Irq::kIrq1 : HuC6280Irq::kIrq2);
                    }
                    break;
            }

            // The machine rebases the processor's clock at every frame
            // boundary, so the walk rebases too rather than running one
            // clock until it wraps.
            if (pair.ts > 4000000) {
                rebase(pair, 0);
            }

            if (pair.mismatches != 0) {
                break;
            }
        }

        check(pair.mismatches == 0, "the two arms never disagreed");
        if (pair.mismatches != 0) {
            std::printf("        first: %s\n", pair.first_mismatch);
        }
        std::printf(
            "        %llu comparisons\n",
            static_cast<unsigned long long>(pair.comparisons));
    }

    section("what the stimulus reached");
    {
        const HuC6280Irq::Coverage& coverage = pair.native.coverage();

        std::printf(
            "        syncs %llu, ticks %llu (%llu spanning more than one)\n"
            "        timer: %llu underflows, %llu reloads, "
            "%llu starts, %llu stops\n"
            "        timer file: %llu counter reads, %llu reload writes, "
            "%llu control writes\n"
            "        interrupt file: %llu mask reads, %llu mask writes, "
            "%llu status reads\n"
            "        acknowledges: %llu by read, %llu by write\n"
            "        lines raised: %llu irq2, %llu irq1, %llu timer\n"
            "        lines lowered: %llu irq2, %llu irq1, %llu timer\n"
            "        horizon: %llu queries, %llu never, %llu already due\n",
            static_cast<unsigned long long>(coverage.syncs),
            static_cast<unsigned long long>(coverage.ticks),
            static_cast<unsigned long long>(coverage.multi_tick_syncs),
            static_cast<unsigned long long>(coverage.timer_underflows),
            static_cast<unsigned long long>(coverage.timer_reloads),
            static_cast<unsigned long long>(coverage.timer_starts),
            static_cast<unsigned long long>(coverage.timer_stops),
            static_cast<unsigned long long>(coverage.counter_reads),
            static_cast<unsigned long long>(coverage.reload_writes),
            static_cast<unsigned long long>(coverage.control_writes),
            static_cast<unsigned long long>(coverage.mask_reads),
            static_cast<unsigned long long>(coverage.mask_writes),
            static_cast<unsigned long long>(coverage.status_reads),
            static_cast<unsigned long long>(coverage.acknowledge_reads),
            static_cast<unsigned long long>(coverage.acknowledge_writes),
            static_cast<unsigned long long>(coverage.requests_raised[0]),
            static_cast<unsigned long long>(coverage.requests_raised[1]),
            static_cast<unsigned long long>(coverage.requests_raised[2]),
            static_cast<unsigned long long>(coverage.requests_lowered[0]),
            static_cast<unsigned long long>(coverage.requests_lowered[1]),
            static_cast<unsigned long long>(coverage.requests_lowered[2]),
            static_cast<unsigned long long>(coverage.horizon_queries),
            static_cast<unsigned long long>(coverage.horizon_never),
            static_cast<unsigned long long>(coverage.horizon_now));

        check(
            coverage.multi_tick_syncs > 0,
            "catch-ups spanning several ticks were driven");
        check(
            coverage.timer_underflows > 0 && coverage.timer_reloads > 0,
            "the counter underflowed and reloaded");
        check(
            coverage.timer_starts > 0 && coverage.timer_stops > 0,
            "the timer was both started and stopped");
        check(
            coverage.acknowledge_reads > 0
                && coverage.acknowledge_writes > 0,
            "both acknowledge paths were taken");
        check(
            coverage.requests_raised[0] > 0
                && coverage.requests_raised[1] > 0
                && coverage.requests_raised[2] > 0,
            "every request line was asserted");
    }

    dc_beetle_reference_irq_destroy(reference);

    std::printf("\n%s\n", g_failed ? "FAILED" : "PASSED");
    return g_failed ? 1 : 0;
}
