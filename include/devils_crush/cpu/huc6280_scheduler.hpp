// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace devils_crush::cpu {

// The HuC6280's clock and event scheduler: the master-cycle timestamp every
// device in this machine measures itself against, the two horizons that
// decide when the processor stops to let something else happen, and the
// loop flag that ends a frame.
//
// This is the smaller half of what is left inside `HuC6280`. The other half
// is execution -- fetching an opcode, deciding at an instruction boundary
// that an interrupt has arrived -- and the two are worth separating because
// only one of them is a device. A cycle costing three master clocks or
// twelve, an event handler coming due, a frame boundary wrapping the clock:
// none of that needs to know what an instruction is. Taking an interrupt
// does, and is not here.
//
// Three things about this device are surprising enough to state before the
// interface rather than after it.
//
// **Reset and power do not touch it.** `HuC6280::Reset` and `HuC6280::Power`
// between them clear the whole processor and the whole interrupt controller
// and leave every field here exactly as it was. Only `HuC6280::Init` -- run
// when the machine is built and again for every ROM it loads -- puts the
// clock back to zero. So this model has an `init` and no `reset`, and that
// asymmetry is the emulator's, not a simplification.
//
// **The horizons are relative and the clock is absolute.** `next_event` and
// `next_user_event` count *down* in master clocks; the timestamp counts up.
// That is why the frame boundary can rebase the timestamp to a small
// remainder without touching either horizon, and why nothing here has to be
// recomputed when it does. It is also why a savestate carries the horizons
// but not the clock: the clock is always near zero at the moment a state is
// written, and the horizons are distances that survive the wrap.
//
// **How long a cycle is belongs here.** `speed` is written by the CSL and
// CSH opcodes, which makes it look like a processor register, but nothing
// reads it except the conversion from processor cycles to master clocks.
// That conversion is the clock's own question -- three master clocks at the
// fast rate, twelve at the slow one -- so the model holds it and offers
// `advance` in cycles as well as `advance_master` in master clocks, which
// is exactly the pair `ADDCYC` and `ADDCYC_MASTER` offer.
//
// Its downward edges are two devices rather than one. The timer is asked to
// catch up, asked for its next tick, and told when the clock is rebased;
// the user event handler -- the colour encoder, in this machine -- is called
// when its own horizon comes due and answers with the next one. Both cross
// as hooks rather than as objects, because a machine that installs a native
// timer and an emulated encoder has to be able to say so.
//
// Its upward edge is a block of memory rather than a set of getters, and
// that is deliberate. Every device before this one answered questions the
// machine asked at its own edge; this one holds values the machine reads
// from inside its instruction loop -- the clock, on every memory write and
// every replacement offer; the near horizon, in the one opcode that waits;
// the run flag, in the loop condition itself. A getter per read would put
// an indirect call in all three. So `registers()` hands out the storage,
// the machine reads it directly, and every *change* to it is still a call
// on this object. Read as a load, write as a call: that rule is what makes
// the seam both cheap and honest, and it is the reason this model needs no
// mirror on the other side and no capture-and-apply pair for a savestate.
class HuC6280Scheduler {
public:
    // The two rates the machine runs at, as the `speed` field holds them.
    // Slow is the power-on rate and the one a reset returns to.
    static constexpr std::uint8_t kSlow = 0;
    static constexpr std::uint8_t kFast = 1;

    // Master clocks in one processor cycle at each rate. The emulator
    // computes these as `(3 << ((speed ^ 1) << 1))` through a cache; they
    // are named here because the arithmetic is otherwise unreadable.
    static constexpr std::int32_t kMasterClocksPerFastCycle = 3;
    static constexpr std::int32_t kMasterClocksPerSlowCycle = 12;

    // What a missing hook answers with. A scheduler with no timer and no
    // event handler is a scheduler for a machine that has neither, which is
    // a convenience for a test rather than anything a machine does; the
    // value is far enough away that such a scheduler never stops.
    static constexpr std::int32_t kNoDeviceHorizon = 0x3FFFFFFF;

    // Catches the timer up to `timestamp` master clocks. Returns nothing:
    // the horizon is asked for separately, because the two questions are
    // asked at different moments. `HuC6280::TimerSync` runs at the top of
    // every register access and inside every `HappySync`; `CalcNextEvent`
    // runs *after* the access, when a write may have moved the divider the
    // sync just advanced.
    using TimerSyncHook = void (*)(void* context, std::int32_t timestamp);
    // Master clocks until the timer's next tick. Read, never stored: the
    // register file can move it between one catch-up and the next horizon.
    using TimerHorizonHook = std::int32_t (*)(void* context);
    // The clock has wrapped to `base`; the timer's phase must wrap with it.
    using TimerRebaseHook = void (*)(void* context, std::int32_t base);
    // The user event handler, called at `timestamp` when its horizon comes
    // due, answering with master clocks until the next one.
    using UserEventHook =
        std::int32_t (*)(void* context, std::int32_t timestamp);

    // Which paths the scheduler has actually run. Unlike the device models
    // before it, almost every counter here is driven once per instruction,
    // so the interesting numbers are the ratios: how often an advance
    // reached a horizon at all, and which of the two horizons won.
    struct Coverage {
        // Advances in processor cycles and in master clocks, counted apart
        // because only the first consults the speed rate.
        std::uint64_t advances = 0;
        std::uint64_t master_advances = 0;
        // Processor cycles advanced at each rate, and master clocks the
        // clock actually moved by.
        std::uint64_t fast_cycles = 0;
        std::uint64_t slow_cycles = 0;
        std::uint64_t master_clocks = 0;
        // Advances the overclock divider swallowed whole, leaving the clock
        // where it was. Zero at the machine's own rate of one.
        std::uint64_t stalled_advances = 0;
        // Advances that reached the near horizon, and of those, the ones
        // that also found the user event due. The first is a small fraction
        // of `advances`; the second is a small fraction of the first.
        std::uint64_t horizon_reached = 0;
        std::uint64_t user_events = 0;
        // Horizon recomputations, split by which device won. A tie goes to
        // the timer, because the emulator's comparison is strict.
        std::uint64_t recalculations = 0;
        std::uint64_t timer_nearer = 0;
        std::uint64_t user_nearer = 0;
        // Explicit re-arms of the user event, from a device that knows when
        // it next wants to be called.
        std::uint64_t set_events = 0;
        // Frame boundaries, and the two ends of a run.
        std::uint64_t rebases = 0;
        std::uint64_t runs = 0;
        std::uint64_t exits = 0;
        // Rate changes that actually changed the rate.
        std::uint64_t speed_changes = 0;
    };

    // Every field the emulator's savestate carries for this device, in its
    // order. The clock itself is absent, and so are the overclock rate and
    // its remainder: the rate is a host setting applied when the machine is
    // built rather than emulator state, the remainder is always zero at the
    // rate the machine ships with, and the clock is rebased to a small
    // remainder at each frame boundary rather than persisted.
    //
    // The installed machine does not use this pair. Because the register
    // block below is the processor's own storage, the emulator's savestate
    // reads and writes these four in place and needs no shuttle -- the only
    // one of the five device models for which that is true. What `State`
    // is for is stating the format in one place, and proving in a test that
    // a round trip through it carries everything the format carries.
    struct State {
        std::int32_t run_flag = 0;
        std::uint8_t speed = kSlow;
        std::int32_t next_event = 0;
        std::int32_t next_user_event = 0;
    };

    // The whole of the device's state, in one block, so that a machine can
    // read it without asking. The field order and widths are the emulator's
    // own, because the block is meant to be pointed at from a processor that
    // still writes its savestate through those names.
    //
    // Nothing outside this class may write these. The invariant that makes
    // that worth stating is `speed_shift`: it is derived from `speed` and
    // has no independent meaning, so a writer that moved one without the
    // other would price every processor cycle wrongly while `speed` still
    // read correctly.
    struct Registers {
        std::uint32_t timestamp = 0;
        std::uint32_t timestamp_fraction = 0;
        std::uint32_t overclock_rate = 1;
        std::int32_t next_event = 0;
        std::int32_t next_user_event = 0;
        std::int32_t run_flag = 0;
        std::uint8_t speed = kSlow;
        std::uint8_t speed_shift = 2;
    };

    HuC6280Scheduler() noexcept = default;

    [[nodiscard]] Registers& registers() noexcept { return registers_; }
    [[nodiscard]] const Registers& registers() const noexcept
    {
        return registers_;
    }

    // `HuC6280::Init`. The clock, its remainder and both horizons go to
    // zero and the overclock rate goes back to one. The rate, the run flag
    // and the installed hooks survive, because the emulator's `Init` does
    // not touch `speed` or `runrunrun` and the machine reinstalls its event
    // handler separately.
    void init() noexcept;

    void set_timer_sync_hook(TimerSyncHook hook, void* context) noexcept;
    void set_timer_horizon_hook(
        TimerHorizonHook hook,
        void* context) noexcept;
    void set_timer_rebase_hook(TimerRebaseHook hook, void* context) noexcept;
    void set_user_event_hook(UserEventHook hook, void* context) noexcept;

    // The overclock divider. One is the machine's own rate and the only one
    // at which the remainder stays zero. A rate of zero is refused rather
    // than reproduced: the emulator would divide by it, and the setting it
    // comes from cannot be zero.
    void set_overclock(std::uint32_t rate) noexcept;
    [[nodiscard]] std::uint32_t overclock() const noexcept
    {
        return registers_.overclock_rate;
    }

    // The clock rate, as CSL and CSH set it.
    void set_speed(std::uint8_t speed) noexcept;
    [[nodiscard]] std::uint8_t speed() const noexcept
    {
        return registers_.speed;
    }
    // Master clocks in one processor cycle at the current rate. The
    // replacement machinery prices a stalled bus write with this, which is
    // the one place outside `advance` that asks.
    [[nodiscard]] std::int32_t master_clocks_per_cycle() const noexcept
    {
        return kMasterClocksPerFastCycle << registers_.speed_shift;
    }

    // `ADDCYC` and `ADDCYC_MASTER`: move the clock forward, bring both
    // horizons nearer, and stop for whatever is due if the near one has
    // arrived. Everything the machine does to time goes through one of
    // these two.
    void advance(std::int32_t cycles) noexcept;
    void advance_master(std::int32_t master_clocks) noexcept;

    // `CalcNextEvent`: the near horizon is whichever of the timer's next
    // tick and the user event's next call is closer. Public because the
    // timer's register file moves the divider behind the scheduler's back
    // and every one of its callers recomputes for itself afterwards --
    // which is exactly what `HuC6280::TimerRead` and `TimerWrite` do.
    void recalculate() noexcept;

    // `SetEvent`: a device declaring when it next wants to be called, and
    // recomputing the near horizon in the same breath.
    void set_event(std::int32_t cycles) noexcept;

    // Catches the timer up to now. `HuC6280::TimerSync` with no arguments:
    // the timestamp it syncs to is always this clock's current one. The top
    // of every timer register access is this call, and so is the top of
    // every stop at the near horizon.
    void sync_timer() noexcept;

    // `SyncAndResetTimestamp`: the frame boundary. The timer is caught up
    // and rebased and the clock wraps to `base`; neither horizon moves,
    // because both are distances rather than moments.
    void sync_and_reset_timestamp(std::int32_t base) noexcept;

    // `Run` and `Exit`. The flag is three-valued: one for a normal run,
    // minus one for a single step -- which must not be interrupted in the
    // middle of a block move -- and zero for stopped. The loop runs while
    // it is positive and a block move continues while it is non-zero, so
    // both readings are offered rather than one.
    void run(bool step_mode) noexcept;
    void exit() noexcept;
    [[nodiscard]] std::int32_t run_flag() const noexcept
    {
        return registers_.run_flag;
    }
    [[nodiscard]] bool running() const noexcept
    {
        return registers_.run_flag > 0;
    }
    [[nodiscard]] bool stopped() const noexcept
    {
        return registers_.run_flag == 0;
    }

    [[nodiscard]] std::uint32_t timestamp() const noexcept
    {
        return registers_.timestamp;
    }
    [[nodiscard]] std::uint32_t timestamp_fraction() const noexcept
    {
        return registers_.timestamp_fraction;
    }
    [[nodiscard]] std::int32_t next_event() const noexcept
    {
        return registers_.next_event;
    }
    [[nodiscard]] std::int32_t next_user_event() const noexcept
    {
        return registers_.next_user_event;
    }

    [[nodiscard]] const Coverage& coverage() const noexcept
    {
        return coverage_;
    }

    void capture_state(State& state) const noexcept;
    // Applies a captured state and makes the one repair the emulator's own
    // loader makes: a speed outside its single bit is masked back into it.
    void apply_state(const State& state) noexcept;

private:
    // `HappySync`: the timer catches up, the user event handler runs if its
    // own horizon has arrived, and the near horizon is recomputed. Private
    // because nothing reaches it except an advance that ran out of horizon.
    void reach_horizon() noexcept;
    // `Overclock`: divides master clocks by the overclock rate, keeping the
    // remainder. Returns what the clock should actually move by, which at
    // any rate above one is sometimes nothing.
    [[nodiscard]] std::int32_t divide(std::int32_t master_clocks) noexcept;
    void step(std::int32_t master_clocks) noexcept;
    [[nodiscard]] std::int32_t timer_horizon() const noexcept;

    Registers registers_{};

    TimerSyncHook timer_sync_hook_ = nullptr;
    void* timer_sync_context_ = nullptr;
    TimerHorizonHook timer_horizon_hook_ = nullptr;
    void* timer_horizon_context_ = nullptr;
    TimerRebaseHook timer_rebase_hook_ = nullptr;
    void* timer_rebase_context_ = nullptr;
    UserEventHook user_event_hook_ = nullptr;
    void* user_event_context_ = nullptr;

    Coverage coverage_{};
};

}  // namespace devils_crush::cpu
