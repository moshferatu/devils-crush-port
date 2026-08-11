// SPDX-License-Identifier: GPL-3.0-or-later

// The native clock and event scheduler against the emulator's own, driven
// side by side from one stimulus.
//
// The timer's differential test drove the emulator's processor in order to
// watch a device hanging off its clock. This one watches the clock. The
// substitution that made that work is the same and slightly stronger: the
// scheduler does not know what an instruction is, so a stolen cycle and an
// executed one move it identically, and everything it does happens inside
// `StealCycles`.
//
// The comparison has three parts, and only the first is obvious.
//
// The first is state: the clock, its overclock remainder, both horizons,
// the rate, the shift the rate is used through, and the loop flag, checked
// after every single operation.
//
// The second is the *sequence of moments* the scheduler decided something
// was due. Both arms install a user event handler that records the
// timestamp it was called at into an order-sensitive digest. A scheduler
// that ended a long drive with the right clock but had called the handler
// at a different set of moments along the way would be wrong in exactly the
// way a frame of video is sensitive to, and the digest is what catches it.
//
// The third is the timer, and it is a *dependency* rather than a
// comparison. The reference arm's near horizon is bounded by the emulator's
// own timer, catching up inside `HappySync` at the moments a real machine
// would. The native arm is bounded by a `devils_crush::cpu::HuC6280Irq`
// composed underneath it through the scheduler's hooks -- which is the
// composition a native machine will use, and which is sound to rely on here
// because `dc_irq_device` has already proved those two equal field for
// field. Any difference this test sees is therefore the scheduler's.
//
// Two things are deliberately not compared. An overclock rate of zero,
// because the emulator divides by it and the setting it comes from cannot
// produce it; the model's refusal is checked on the native arm alone. And
// the emulator's own persistence of these fields, which needs a savestate
// writer this shim has no reason to carry; the model's shuttle is
// round-tripped against itself and its one documented repair is checked
// directly.

#include "dc_beetle_reference_scheduler.h"
#include "devils_crush/cpu/huc6280_irq.hpp"
#include "devils_crush/cpu/huc6280_scheduler.hpp"

#include <cstdint>
#include <cstdio>

namespace {

using devils_crush::cpu::HuC6280Irq;
using devils_crush::cpu::HuC6280Scheduler;

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

// The timer's register file, at the addresses the machine's I/O page
// handler sees after masking.
constexpr std::uint32_t kTimerCounter = 0x0C00;
constexpr std::uint32_t kTimerControl = 0x0C01;

// One tick of the timer in master clocks, which is the only thing bounding
// the near horizon whenever the user event is armed far away.
constexpr std::int32_t kTick = HuC6280Irq::kTickClocks;

// Far enough that a horizon armed at it is never reached by anything this
// test drives, and the same value the shim's handler starts at.
constexpr std::int32_t kFarEvent = 0x3FFFFFF;

// The two arms. The native one is two objects because that is what the
// device is: a scheduler whose near horizon is bounded by a timer it does
// not own.
struct Pair {
    dc_beetle_reference_scheduler* ref = nullptr;
    HuC6280Scheduler native;
    HuC6280Irq timer;

    // What both handlers return, held here so one call sets it on both.
    std::int32_t interval = kFarEvent;

    // The native arm's copy of what the shim records for the reference.
    std::uint64_t events = 0;
    std::uint64_t digest = 0xCBF29CE484222325ull;
    std::int32_t last_event = 0;

    unsigned long long comparisons = 0;
    unsigned long long mismatches = 0;
    // The first disagreement, kept so a failure names a moment rather than
    // a count.
    char first_mismatch[192] = {};
};

// FNV-1a over the four bytes of a timestamp, byte for byte the same as the
// shim's, so the two digests are comparable rather than merely both
// order-sensitive.
void absorb(std::uint64_t& digest, const std::int32_t timestamp)
{
    const std::uint32_t bits = static_cast<std::uint32_t>(timestamp);

    for (int i = 0; i < 4; i++) {
        digest ^= static_cast<std::uint64_t>((bits >> (i * 8)) & 0xFF);
        digest *= 0x100000001B3ull;
    }
}

void timer_sync_hook(void* const context, const std::int32_t timestamp)
{
    static_cast<Pair*>(context)->timer.sync(timestamp);
}

std::int32_t timer_horizon_hook(void* const context)
{
    return static_cast<Pair*>(context)->timer.next_event();
}

void timer_rebase_hook(void* const context, const std::int32_t base)
{
    static_cast<Pair*>(context)->timer.reset_timestamp(base);
}

std::int32_t user_event_hook(
    void* const context,
    const std::int32_t timestamp)
{
    Pair& pair = *static_cast<Pair*>(context);

    pair.events++;
    pair.last_event = timestamp;
    absorb(pair.digest, timestamp);

    return pair.interval;
}

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
            "%s: %s reference %lld native %lld (t=%lu)",
            where,
            field,
            reference,
            native,
            static_cast<unsigned long>(pair.native.timestamp()));
    }
}

// Every field of the device, plus the record of what the handler saw. There
// is no "only meaningful after a recomputation" caveat here as there was
// for the timer: both arms are driven through the same entry points, so
// every field is comparable at every moment.
void compare(Pair& pair, const char* const where)
{
    dc_beetle_reference_scheduler_observation observed{};
    dc_beetle_reference_scheduler_observe(pair.ref, &observed);

    dc_beetle_reference_scheduler_event_log log{};
    dc_beetle_reference_scheduler_events(pair.ref, &log);

    pair.comparisons++;

    if (observed.timestamp != pair.native.timestamp()) {
        record_mismatch(
            pair, where, "timestamp", observed.timestamp,
            pair.native.timestamp());
    }
    if (observed.timestamp_frac != pair.native.timestamp_fraction()) {
        record_mismatch(
            pair, where, "timestamp_frac", observed.timestamp_frac,
            pair.native.timestamp_fraction());
    }
    if (observed.overclock_rate != pair.native.overclock()) {
        record_mismatch(
            pair, where, "overclock_rate", observed.overclock_rate,
            pair.native.overclock());
    }
    if (observed.next_event != pair.native.next_event()) {
        record_mismatch(
            pair, where, "next_event", observed.next_event,
            pair.native.next_event());
    }
    if (observed.next_user_event != pair.native.next_user_event()) {
        record_mismatch(
            pair, where, "next_user_event", observed.next_user_event,
            pair.native.next_user_event());
    }
    if (observed.runrunrun != pair.native.run_flag()) {
        record_mismatch(
            pair, where, "runrunrun", observed.runrunrun,
            pair.native.run_flag());
    }
    if (observed.speed != pair.native.speed()) {
        record_mismatch(
            pair, where, "speed", observed.speed, pair.native.speed());
    }
    // The derived shift, compared through what it is for rather than
    // directly: a model that held the rate right and shadowed it wrong
    // would price every cycle wrongly and be invisible in `speed`.
    const std::int32_t reference_cycle =
        HuC6280Scheduler::kMasterClocksPerFastCycle
        << observed.speed_shift_cache;
    if (reference_cycle != pair.native.master_clocks_per_cycle()) {
        record_mismatch(
            pair, where, "master clocks per cycle", reference_cycle,
            pair.native.master_clocks_per_cycle());
    }

    if (log.count != pair.events) {
        record_mismatch(
            pair, where, "event count",
            static_cast<long long>(log.count),
            static_cast<long long>(pair.events));
    }
    if (log.digest != pair.digest) {
        record_mismatch(
            pair, where, "event digest",
            static_cast<long long>(log.digest),
            static_cast<long long>(pair.digest));
    }
    if (log.last_timestamp != pair.last_event) {
        record_mismatch(
            pair, where, "last event timestamp", log.last_timestamp,
            pair.last_event);
    }
}

// Every operation the device offers, driven on both arms and compared. The
// timer's register file is here too, because the near horizon has to be
// bounded by something real for any of this to mean anything.

void init(Pair& pair)
{
    dc_beetle_reference_scheduler_init(pair.ref);
    pair.native.init();
    // `HuC6280::Init` re-initialises the timer in the same breath, which is
    // the only reason this is not purely a scheduler operation.
    pair.timer.init();
    compare(pair, "init");
}

void reset(Pair& pair)
{
    dc_beetle_reference_scheduler_reset(pair.ref);
    pair.timer.reset();
    compare(pair, "reset");
}

void power(Pair& pair)
{
    dc_beetle_reference_scheduler_power(pair.ref);
    pair.timer.power();
    compare(pair, "power");
}

void advance(Pair& pair, const std::int32_t cycles)
{
    dc_beetle_reference_scheduler_advance(pair.ref, cycles);
    pair.native.advance(cycles);
    compare(pair, "advance");
}

void advance_master(Pair& pair, const std::int32_t master_clocks)
{
    dc_beetle_reference_scheduler_advance_master(pair.ref, master_clocks);
    pair.native.advance_master(master_clocks);
    compare(pair, "advance master");
}

void set_event(Pair& pair, const std::int32_t cycles)
{
    dc_beetle_reference_scheduler_set_event(pair.ref, cycles);
    pair.native.set_event(cycles);
    compare(pair, "set event");
}

void recalculate(Pair& pair)
{
    dc_beetle_reference_scheduler_recalculate(pair.ref);
    pair.native.recalculate();
    compare(pair, "recalculate");
}

void sync_timer(Pair& pair)
{
    dc_beetle_reference_scheduler_sync_timer(pair.ref);
    pair.native.sync_timer();
    compare(pair, "sync timer");
}

void reset_timestamp(Pair& pair, const std::int32_t base)
{
    dc_beetle_reference_scheduler_reset_timestamp(pair.ref, base);
    pair.native.sync_and_reset_timestamp(base);
    compare(pair, "reset timestamp");
}

void set_overclock(Pair& pair, const std::uint32_t rate)
{
    dc_beetle_reference_scheduler_set_overclock(pair.ref, rate);
    pair.native.set_overclock(rate);
    compare(pair, "set overclock");
}

void set_speed(Pair& pair, const std::uint8_t speed)
{
    dc_beetle_reference_scheduler_set_speed(pair.ref, speed);
    pair.native.set_speed(speed);
    compare(pair, "set speed");
}

void run(Pair& pair, const bool step_mode)
{
    dc_beetle_reference_scheduler_run(pair.ref, step_mode ? 1 : 0);
    pair.native.run(step_mode);
    compare(pair, "run");
}

void leave(Pair& pair)
{
    dc_beetle_reference_scheduler_exit(pair.ref);
    pair.native.exit();
    compare(pair, "exit");
}

void set_interval(Pair& pair, const std::int32_t interval)
{
    dc_beetle_reference_scheduler_set_event_interval(pair.ref, interval);
    pair.interval = interval;
    compare(pair, "set interval");
}

// `HuC6280::TimerWrite` is a synchronize, a write and a recomputation, and
// the first and last of those three are the scheduler's. Writing it out
// here rather than hiding it behind one call is the point: this is the
// shape a native machine's I/O page handler will have.
void write_timer(
    Pair& pair,
    const std::uint32_t address,
    const std::uint8_t value)
{
    dc_beetle_reference_scheduler_write_timer(pair.ref, address, value);

    pair.native.sync_timer();
    pair.timer.write_timer(address, value);
    pair.native.recalculate();

    compare(pair, "write timer");
}

// A deterministic stimulus. Nothing here is sampled from outside the file,
// so both arms are pure functions of it.
struct Random {
    std::uint32_t state = 0x9E3779B9u;

    std::uint32_t next()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    std::uint32_t below(const std::uint32_t bound)
    {
        return next() % bound;
    }
};

void report_coverage(const HuC6280Scheduler& scheduler)
{
    const HuC6280Scheduler::Coverage& coverage = scheduler.coverage();

    std::printf(
        "\nscheduler coverage:\n"
        "  advances: %llu in cycles (%llu slow, %llu fast),"
        " %llu in master clocks\n"
        "  clock moved %llu master clocks, %llu advances stalled\n"
        "  horizon reached %llu times, user event due %llu of those\n"
        "  recomputed %llu times: timer nearer %llu, user nearer %llu\n"
        "  set events %llu, rebases %llu, runs %llu, exits %llu,"
        " rate changes %llu\n",
        static_cast<unsigned long long>(coverage.advances),
        static_cast<unsigned long long>(coverage.slow_cycles),
        static_cast<unsigned long long>(coverage.fast_cycles),
        static_cast<unsigned long long>(coverage.master_advances),
        static_cast<unsigned long long>(coverage.master_clocks),
        static_cast<unsigned long long>(coverage.stalled_advances),
        static_cast<unsigned long long>(coverage.horizon_reached),
        static_cast<unsigned long long>(coverage.user_events),
        static_cast<unsigned long long>(coverage.recalculations),
        static_cast<unsigned long long>(coverage.timer_nearer),
        static_cast<unsigned long long>(coverage.user_nearer),
        static_cast<unsigned long long>(coverage.set_events),
        static_cast<unsigned long long>(coverage.rebases),
        static_cast<unsigned long long>(coverage.runs),
        static_cast<unsigned long long>(coverage.exits),
        static_cast<unsigned long long>(coverage.speed_changes));
}

}  // namespace

int main()
{
    Pair pair;

    pair.ref = dc_beetle_reference_scheduler_create();
    if (pair.ref == nullptr) {
        std::printf("could not claim the emulator's processor\n");
        return 1;
    }

    pair.native.set_timer_sync_hook(timer_sync_hook, &pair);
    pair.native.set_timer_horizon_hook(timer_horizon_hook, &pair);
    pair.native.set_timer_rebase_hook(timer_rebase_hook, &pair);
    pair.native.set_user_event_hook(user_event_hook, &pair);

    section("the machine builds itself");
    init(pair);
    check(pair.native.timestamp() == 0, "the clock starts at zero");
    check(
        pair.native.timestamp_fraction() == 0,
        "and so does the overclock remainder");
    check(pair.native.overclock() == 1, "the overclock rate is one");
    check(
        pair.native.next_event() == 0 && pair.native.next_user_event() == 0,
        "both horizons start due");
    check(
        pair.native.speed() == HuC6280Scheduler::kSlow,
        "the machine comes up at the slow rate");
    check(
        pair.native.master_clocks_per_cycle()
            == HuC6280Scheduler::kMasterClocksPerSlowCycle,
        "where a cycle costs twelve master clocks");
    check(pair.native.stopped(), "and the loop is not running");

    section("nothing about a reset belongs to this device");
    set_interval(pair, kFarEvent);
    advance(pair, 40);
    {
        const std::uint32_t before = pair.native.timestamp();
        const std::int32_t user_before = pair.native.next_user_event();

        reset(pair);
        power(pair);

        check(
            pair.native.timestamp() == before,
            "reset and power leave the clock exactly where it was");
        check(
            pair.native.next_user_event() == user_before,
            "and leave the user event's horizon alone");
    }

    section("a cycle is three master clocks, or twelve");
    init(pair);
    reset(pair);
    set_event(pair, kFarEvent);
    {
        const std::uint32_t before = pair.native.timestamp();
        advance(pair, 10);
        check(
            pair.native.timestamp() - before
                == static_cast<std::uint32_t>(
                    10 * HuC6280Scheduler::kMasterClocksPerSlowCycle),
            "ten slow cycles cost a hundred and twenty master clocks");
    }
    set_speed(pair, HuC6280Scheduler::kFast);
    check(
        pair.native.master_clocks_per_cycle()
            == HuC6280Scheduler::kMasterClocksPerFastCycle,
        "the fast rate prices a cycle at three");
    {
        const std::uint32_t before = pair.native.timestamp();
        advance(pair, 10);
        check(
            pair.native.timestamp() - before
                == static_cast<std::uint32_t>(
                    10 * HuC6280Scheduler::kMasterClocksPerFastCycle),
            "and ten of them cost thirty");
    }
    {
        const std::uint32_t before = pair.native.timestamp();
        advance_master(pair, 77);
        check(
            pair.native.timestamp() - before == 77,
            "a master-clock advance ignores the rate entirely");
    }
    set_speed(pair, HuC6280Scheduler::kSlow);

    section("the near horizon is whichever device is closer");
    init(pair);
    set_event(pair, kFarEvent);
    // The first catch-up is what puts a divider on a timer that `Init` left
    // at zero, so the horizon only becomes the timer's after it.
    sync_timer(pair);
    recalculate(pair);
    check(
        pair.native.next_event() == kTick,
        "with the user event far away the horizon is the timer's tick");
    set_event(pair, 100);
    check(
        pair.native.next_event() == 100,
        "an event nearer than the tick takes the horizon");
    set_event(pair, kTick);
    check(
        pair.native.next_event() == kTick,
        "and a tie goes to the timer, which is what the emulator does");

    section("the user event comes due, and only when it is due");
    init(pair);
    set_interval(pair, 5000);
    {
        const std::uint64_t before = pair.events;

        // The clock starts with both horizons at zero, so the very first
        // advance stops for everything at once.
        advance(pair, 1);
        check(
            pair.events == before + 1,
            "the first advance finds the handler due");
        check(
            pair.last_event
                == static_cast<std::int32_t>(pair.native.timestamp()),
            "and calls it at the clock's current moment");

        const std::uint64_t after_first = pair.events;
        advance(pair, 100);
        check(
            pair.events == after_first,
            "an advance inside the interval does not call it again");

        // Twelve hundred slow cycles is fourteen thousand four hundred
        // master clocks, well past the five thousand it asked for.
        advance(pair, 1200);
        check(
            pair.events == after_first + 1,
            "an advance past the interval calls it exactly once");
    }

    section("the overclock divides master clocks");
    init(pair);
    set_interval(pair, kFarEvent);
    set_event(pair, kFarEvent);
    set_overclock(pair, 5);
    {
        const std::uint32_t before = pair.native.timestamp();
        const std::uint64_t stalled_before =
            pair.native.coverage().stalled_advances;

        // Three master clocks at a rate of five: the first two advances are
        // swallowed whole and the third carries all three across.
        advance_master(pair, 3);
        check(
            pair.native.timestamp() == before,
            "an advance smaller than the rate moves nothing");
        check(
            pair.native.coverage().stalled_advances == stalled_before + 1,
            "and is counted as swallowed");
        advance_master(pair, 3);
        advance_master(pair, 3);
        check(
            pair.native.timestamp() == before + 1,
            "nine master clocks at a rate of five become one");
        check(
            pair.native.timestamp_fraction() == 4,
            "with the remainder kept for the next one");
    }
    set_overclock(pair, 1);
    {
        // The remainder left over from the previous rate is not discarded,
        // which is the emulator's behaviour and the reason this is checked
        // rather than assumed.
        const std::uint32_t before = pair.native.timestamp();
        advance_master(pair, 1);
        check(
            pair.native.timestamp() == before + 5,
            "the remainder from the old rate crosses into the new one");
    }

    section("the frame boundary wraps the clock and nothing else");
    init(pair);
    set_event(pair, kFarEvent);
    advance(pair, 3000);
    {
        const std::int32_t event_before = pair.native.next_event();
        const std::int32_t user_before = pair.native.next_user_event();

        reset_timestamp(pair, 7);

        check(
            pair.native.timestamp() == 7,
            "the clock wraps to the remainder it was given");
        check(
            pair.native.next_event() == event_before
                && pair.native.next_user_event() == user_before,
            "and both horizons survive it, being distances not moments");
        check(
            pair.timer.last_timestamp() == 7,
            "the timer's phase wraps with it");
    }

    section("the loop flag");
    run(pair, false);
    check(
        pair.native.run_flag() == 1 && pair.native.running(),
        "a run sets the flag positive");
    run(pair, true);
    check(
        pair.native.run_flag() == -1 && !pair.native.running()
            && !pair.native.stopped(),
        "a single step sets it negative, so a block move is not cut short");
    leave(pair);
    check(pair.native.stopped(), "and an exit clears it");

    section("a long drive");
    init(pair);
    set_interval(pair, 5000);
    set_event(pair, kFarEvent);
    {
        Random random;

        for (int step = 0; step < 120000; step++) {
            switch (random.below(14)) {
            case 0:
            case 1:
            case 2:
            case 3:
                advance(pair, 1 + static_cast<std::int32_t>(random.below(40)));
                break;
            case 4:
                // Half of these are small enough that an overclocked rate
                // can swallow one whole, which is the only way the divider
                // is ever asked to move the clock by nothing.
                advance_master(
                    pair,
                    1
                        + static_cast<std::int32_t>(
                            random.below(random.below(2) == 0 ? 4 : 500)));
                break;
            case 5:
                set_event(pair, static_cast<std::int32_t>(random.below(8000)));
                break;
            case 6:
                recalculate(pair);
                break;
            case 7:
                sync_timer(pair);
                break;
            case 8:
                write_timer(
                    pair,
                    kTimerCounter,
                    static_cast<std::uint8_t>(random.below(128)));
                break;
            case 9:
                write_timer(
                    pair,
                    kTimerControl,
                    static_cast<std::uint8_t>(random.below(2)));
                break;
            case 10:
                set_speed(
                    pair, static_cast<std::uint8_t>(random.below(2)));
                break;
            case 11:
                // Rates the machine itself never runs at, changed while a
                // remainder is outstanding, which is the only way the
                // remainder is ever carried across a rate. Mostly back to
                // one, so the clock still covers ground between them.
                set_overclock(
                    pair,
                    (random.below(4) == 0) ? (2 + random.below(3)) : 1);
                break;
            case 12:
                if (random.below(2) == 0) {
                    run(pair, random.below(2) == 0);
                } else {
                    leave(pair);
                }
                break;
            default:
                if (random.below(4) == 0) {
                    // The machine rebases to the remainder of a frame, which
                    // is always small.
                    reset_timestamp(
                        pair, static_cast<std::int32_t>(random.below(12)));
                } else {
                    set_interval(
                        pair,
                        1 + static_cast<std::int32_t>(random.below(6000)));
                }
                break;
            }
        }
    }

    section("what the two arms saw");
    std::printf(
        "  %llu comparisons, %llu disagreements\n",
        pair.comparisons,
        pair.mismatches);
    if (pair.mismatches != 0) {
        std::printf("  first: %s\n", pair.first_mismatch);
    }
    check(pair.mismatches == 0, "the two arms never disagreed");
    check(
        pair.events > 500,
        "and the handler was called often enough to mean something");
    check(
        pair.native.coverage().timer_nearer > 0
            && pair.native.coverage().user_nearer > 0,
        "with both devices winning the horizon at some point");
    check(
        pair.native.coverage().stalled_advances > 100,
        "and with the overclock swallowing advances whole along the way");

    section("the shuttle");
    {
        HuC6280Scheduler::State state;
        pair.native.capture_state(state);

        HuC6280Scheduler restored;
        restored.apply_state(state);

        HuC6280Scheduler::State round_trip;
        restored.capture_state(round_trip);

        check(
            round_trip.run_flag == state.run_flag
                && round_trip.speed == state.speed
                && round_trip.next_event == state.next_event
                && round_trip.next_user_event == state.next_user_event,
            "every field a savestate carries survives a round trip");
        check(
            restored.master_clocks_per_cycle()
                == pair.native.master_clocks_per_cycle(),
            "and the shift the rate is used through follows it in");
        check(
            restored.timestamp() == 0,
            "the clock is not among them, because a state is written at a"
            " boundary where it is about to be rebased anyway");
    }
    {
        HuC6280Scheduler::State corrupt;
        corrupt.speed = 0xFF;

        HuC6280Scheduler restored;
        restored.apply_state(corrupt);

        check(
            restored.speed() == 1
                && restored.master_clocks_per_cycle()
                    == HuC6280Scheduler::kMasterClocksPerFastCycle,
            "a rate outside its one bit is masked back into it on load");
    }

    section("a rate of zero is refused rather than reproduced");
    {
        // Driven on the native arm alone: the emulator would divide by it.
        HuC6280Scheduler alone;
        alone.set_overclock(0);
        check(
            alone.overclock() == 1,
            "because the emulator would divide by it and its setting"
            " cannot produce it");
    }

    report_coverage(pair.native);

    dc_beetle_reference_scheduler_destroy(pair.ref);

    std::printf("\n%s\n", g_failed ? "FAILED" : "PASSED");
    return g_failed ? 1 : 0;
}
