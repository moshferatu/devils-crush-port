// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace devils_crush::cpu {

// The HuC6280's timer and interrupt controller: the two register files at
// $0C00-$0C01 and $1402-$1403, the three request lines behind them, and the
// free-running divider that turns the master clock into the one interrupt
// this machine generates for itself.
//
// The timer is here rather than in a device of its own because it has no
// other outward wire. It counts down and asserts a request; the request is
// masked, reported and acknowledged entirely through the interrupt
// controller's register file -- both a read of $1402 and a write to $1403
// clear it. Splitting the two would put the raise in one object and the
// acknowledge in another.
//
// This is the first device model that is purely reactive. The sound chip
// answered, the video controller answered, the colour encoder owned the
// schedule; this one has no hooks at all. Its inputs are `begin_request`
// from whatever drives a line and a timestamp to catch up to. Its output is
// a masked pending set the processor reads directly, and a divider the
// processor folds into its own next-event horizon.
//
// Three things a reader might expect are deliberately outside it.
//
// The interrupt is not *taken* here. Which vector wins, when the line is
// sampled, and how the I flag gates it are all decided at instruction
// boundaries, and this device has no notion of an instruction. The
// processor samples `pending()` and does the rest. That split is what makes
// the model installable now, with the emulator's processor still latching.
//
// The I/O data bus latch is not held here either. Reads of both register
// files return device bits merged with the last byte the processor drove
// onto that bus, and that latch is shared with the sound chip and the
// controller port. So it is passed in rather than owned, which is also what
// keeps these two reads pure.
//
// And the reset request is an input like any other. The processor asserts
// it at power-on and clears it when it takes the vector; all this device
// does is hold it and, on `acknowledge_reset`, open the mask -- which is
// the only thing that ever initialises the mask, because nothing else
// writes it before the first $1402 store.
class HuC6280Irq {
public:
    // The request lines, in the processor's own bit order. IRQ1 is the
    // video interrupt, IRQ2 the compact disc unit's, and the timer's is
    // this device's own.
    static constexpr std::uint32_t kIrq2 = 0x001;
    static constexpr std::uint32_t kIrq1 = 0x002;
    static constexpr std::uint32_t kTimer = 0x004;
    static constexpr std::uint32_t kReset = 0x020;

    // One tick of the timer, in master clocks: 1024 processor cycles at the
    // slow speed, which is three master clocks each. The counter is seven
    // bits, so a full period is 128 ticks.
    static constexpr std::int32_t kTickClocks = 1024 * 3;
    static constexpr std::int32_t kCounterMask = 0x7F;

    // What `master_cycles_until_timer_interrupt` returns when no timer
    // interrupt can arrive at all.
    static constexpr std::uint32_t kNoTimerInterrupt = 0xFFFFFFFFu;

    // Which paths the model has actually run. The route's demands on this
    // device are narrow in a different way from the video chips': the game
    // drives the timer hard and the mask barely at all.
    struct Coverage {
        // Catch-ups asked for, and ticks the divider crossed inside them. A
        // catch-up that crosses several ticks is the interesting case, and
        // one that crosses none is by far the common one.
        std::uint64_t syncs = 0;
        std::uint64_t ticks = 0;
        std::uint64_t multi_tick_syncs = 0;
        // Timer counter underflows, each of which raises a request, and the
        // reloads that follow them one tick later.
        std::uint64_t timer_underflows = 0;
        std::uint64_t timer_reloads = 0;
        // Register file traffic, split by which byte.
        std::uint64_t counter_reads = 0;
        std::uint64_t reload_writes = 0;
        std::uint64_t control_writes = 0;
        // Control writes that started a stopped timer, which is the only
        // write that reloads the counter, and those that stopped it.
        std::uint64_t timer_starts = 0;
        std::uint64_t timer_stops = 0;
        // Interrupt register file traffic. The two acknowledge paths are
        // counted apart because they are reached from different code: the
        // read is the mask read at $1402, the write is the store to $1403.
        std::uint64_t mask_reads = 0;
        std::uint64_t mask_writes = 0;
        std::uint64_t status_reads = 0;
        std::uint64_t acknowledge_reads = 0;
        std::uint64_t acknowledge_writes = 0;
        // Requests asserted and released, per line, indexed by bit position
        // so a caller can read them out without knowing the encoding: 0 is
        // IRQ2, 1 is IRQ1, 2 is the timer.
        std::uint64_t requests_raised[3] = {};
        std::uint64_t requests_lowered[3] = {};
        // Horizon queries, and the three answers that are not a count: no
        // interrupt possible, one already pending, and one in the future.
        std::uint64_t horizon_queries = 0;
        std::uint64_t horizon_never = 0;
        std::uint64_t horizon_now = 0;
    };

    // Every field the emulator's savestate carries for this device, in its
    // order. `timer_lastts` is absent because the emulator does not persist
    // it either: it is a phase against a processor clock that is itself
    // rebased rather than saved, so a load leaves it and the next
    // `reset_timestamp` fixes it. `IRQSample` is absent because it is the
    // processor's copy, not this device's.
    struct State {
        std::uint32_t irq_low = 0;
        std::uint8_t irq_mask = 0;
        bool timer_inreload = false;
        std::uint8_t timer_status = 0;
        std::int32_t timer_value = 0;
        std::int32_t timer_load = 0;
        std::int32_t timer_div = 0;
    };

    HuC6280Irq() noexcept;

    // `HuC6280::Init`, which the machine runs once when it builds itself and
    // again for every ROM it loads. It is not a reset: it leaves the divider
    // at zero rather than at a partial period, does not touch the request
    // register or the mask, and is the only thing that ever puts the phase
    // back to the start of a clock. A device that skipped it would measure
    // its first catch-up on a new machine against the last timestamp of the
    // previous one.
    void init() noexcept;

    // `HuC6280::Power`, which for this device is `reset` and nothing else:
    // it clears the request register first, but reset then assigns the
    // register outright, so the clear is invisible. Both are kept because
    // the machine calls both and a model that answered only one would have
    // to be installed at a seam that knows they are the same.
    void power() noexcept;
    // `HuC6280::Reset`: the timer stops and the divider takes a partial
    // period, and the request register is *assigned* the reset line rather
    // than having it added, so any other request in flight is dropped.
    void reset() noexcept;
    // Rebases the timestamp the next `sync` measures from, for a processor
    // that has just wrapped its clock. `HuC6280::SyncAndResetTimestamp`
    // catches the timer up first, and so must a caller.
    void reset_timestamp(std::int32_t base) noexcept;

    // The processor took the reset vector: the request is released and the
    // mask is opened to all three lines. Nothing else ever opens it.
    void acknowledge_reset() noexcept;

    // A device asserting or releasing its line. Both are level, not edge:
    // the video interrupt in particular is re-derived and republished
    // rather than pulsed, so a raise on an already-raised line is normal
    // and is not counted twice.
    void begin_request(std::uint32_t lines) noexcept;
    void end_request(std::uint32_t lines) noexcept;

    // Catches the timer up to `timestamp` in master clocks, crossing as
    // many ticks as have elapsed and asserting the timer's request on each
    // underflow. This is the whole of the device's relationship with time.
    //
    // It does not republish the next-event horizon; `HuC6280::TimerSync`
    // does not either, and every one of its callers calls
    // `CalcNextEvent` afterwards for itself.
    void sync(std::int32_t timestamp) noexcept;

    // Master clocks until the divider crosses its next tick, which is this
    // device's whole contribution to the processor's next-event horizon.
    [[nodiscard]] std::int32_t next_event() const noexcept
    {
        return timer_div_;
    }

    // The timer's register file at $0C00-$0C01. Both are pure: a caller
    // syncs to the access's timestamp first and recomputes its horizon
    // afterwards, exactly as `HuC6280::TimerRead` and `TimerWrite` do
    // around the same bodies.
    //
    // `io_data_buffer` is the last byte the processor drove onto the I/O
    // bus. Only bit 7 of it survives into a counter read; the low seven
    // bits are the counter's.
    [[nodiscard]] std::uint8_t read_timer(
        std::uint8_t io_data_buffer) noexcept;
    void write_timer(std::uint32_t address, std::uint8_t value) noexcept;

    // The interrupt controller's register file. Addresses without bit 1 set
    // -- $1400 and $1401 -- are not this device's and read back as the bus
    // latch, which is what the processor's own handler returns for them.
    //
    // A read of $1402 acknowledges the timer as a side effect of reporting
    // the mask. That is the hardware's behaviour and the game depends on
    // it, so `peek_status` exists for a caller that wants to look without
    // acknowledging.
    [[nodiscard]] std::uint8_t read_status(
        std::uint32_t address,
        std::uint8_t io_data_buffer) noexcept;
    [[nodiscard]] std::uint8_t peek_status(
        std::uint32_t address,
        std::uint8_t io_data_buffer) const noexcept;
    void write_status(std::uint32_t address, std::uint8_t value) noexcept;

    // The requests currently asserted, and those of them the mask lets
    // through. The processor samples the second at an instruction boundary
    // and decides what to do about it.
    [[nodiscard]] std::uint32_t requests() const noexcept
    {
        return irq_low_;
    }
    [[nodiscard]] std::uint32_t pending() const noexcept
    {
        return irq_low_ & irq_mask_;
    }
    [[nodiscard]] std::uint8_t mask() const noexcept { return irq_mask_; }

    // Master clocks until a timer interrupt the processor would actually
    // service, given the I flag it is currently holding. `kNoTimerInterrupt`
    // means one cannot arrive: the flag is set, the mask is closed, or the
    // timer is stopped. Zero means one is pending already.
    //
    // This exists because a native routine transcript has to declare how
    // far it may run before an interrupt would have landed inside it. It is
    // the only entry point that reads processor state, and it takes it as
    // an argument rather than reaching for it. It is not `const` only
    // because it records which of its three answers it gave.
    [[nodiscard]] std::uint32_t master_cycles_until_timer_interrupt(
        std::int32_t timestamp,
        bool i_flag) noexcept;

    // Non-disturbing reads of the timer, for a test or a debugger.
    [[nodiscard]] std::int32_t counter() const noexcept
    {
        return timer_value_;
    }
    [[nodiscard]] std::int32_t reload() const noexcept
    {
        return timer_load_;
    }
    [[nodiscard]] bool running() const noexcept
    {
        return timer_status_ != 0;
    }
    [[nodiscard]] bool reloading() const noexcept
    {
        return timer_inreload_;
    }
    [[nodiscard]] std::int32_t divider() const noexcept
    {
        return timer_div_;
    }
    [[nodiscard]] std::int32_t last_timestamp() const noexcept
    {
        return timer_lastts_;
    }

    [[nodiscard]] const Coverage& coverage() const noexcept
    {
        return coverage_;
    }

    void capture_state(State& state) const noexcept;
    // Applies a captured state and repairs the one value a savestate is
    // allowed to hold but the device is not: a divider at or below zero,
    // which would make the next catch-up spin. The emulator's own loader
    // makes the same repair.
    void apply_state(const State& state) noexcept;

private:
    void raise_timer_request() noexcept;
    void count_line_changes(
        std::uint32_t changed,
        std::uint64_t (&counters)[3]) noexcept;

    // Construction leaves this at zero, which is what `HuC6280::Init`
    // leaves it at; `reset` puts it at 1024 rather than at a whole tick.
    // That is the emulator's value and it is reproduced rather than
    // corrected -- it makes the first tick after a reset a third of a
    // period short, and the census is sensitive to where the first timer
    // interrupt lands.
    std::int32_t timer_div_ = 0;
    std::int32_t timer_value_ = 0;
    std::int32_t timer_load_ = 0;
    std::int32_t timer_lastts_ = 0;
    std::uint8_t timer_status_ = 0;
    bool timer_inreload_ = false;

    std::uint32_t irq_low_ = 0;
    // Zero until the reset vector is taken, which closes every line. The
    // processor cannot get anywhere without taking that vector first, so no
    // real machine observes this value.
    std::uint8_t irq_mask_ = 0;

    Coverage coverage_{};
};

}  // namespace devils_crush::cpu
