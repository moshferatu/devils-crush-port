// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace devils_crush::cpu {

// The decision a HuC6280 makes between two instructions: whether the next
// thing to happen is the next instruction or an interrupt.
//
// This is the other half of what was left inside `HuC6280` once the clock
// came out, and it is the half that knows what an instruction is. The
// scheduler says when the machine stops; the interrupt controller says which
// lines are asserted and which of them the mask admits; nothing native yet
// decides what to do about either. This does.
//
// **It is a rule rather than a device, and the shape follows from that.**
// Every model before this one replaced something with a state boundary
// around it -- a register file, a clock, a set of channels. This one holds
// two latched values and otherwise computes: given what was latched and what
// the lines are doing now, which vector wins. So the two operations that
// matter are `static` and pure, `sample` and `select` are the stateful
// wrappers a machine calls, and the whole rule can be checked by enumerating
// its inputs rather than by driving a machine through them.
//
// **The rule is not the obvious one, and three asymmetries are the reason
// this is worth modelling separately at all.**
//
// *The reset line is never sampled.* The mask is three bits wide, so
// `requests & mask` cannot produce `kReset` however the lines are driven.
// The only thing that ever puts it into the latch is `reset()`, and the only
// thing that takes it out is `acknowledge_reset()`. A reset is therefore not
// an interrupt that happened to win; it is a state the boundary is left in.
//
// *The timer line is tested live; the other two are tested latched.* A timer
// request raised after the latch and before the decision is taken
// immediately, where an IRQ1 or IRQ2 request raised in the same window waits
// for the next boundary. The emulator's own comment marks the spot with a
// disused `IRQSample & IQTIMER` beside the rule that replaced it, which is
// the strongest evidence available that the difference is deliberate.
//
// *The I flag is consulted twice, in two different tenses.* It closes the
// latch when it is set -- a sample taken behind a raised I flag latches
// nothing at all -- and it gates the timer branch again at decision time,
// through the value latched rather than the current one. Either alone would
// be a different machine.
//
// **What is deliberately not here.** Taking the vector is eight cycles of
// dummy reads, three pushes, a flag update and a two-byte fetch, all of it
// processor work against the processor's own stack and memory map. This
// model says *which* vector; the machine does the taking. The split is what
// lets the rule be proved exhaustively instead of differentially -- a claim
// about forty lines of decision, not about a processor.
class HuC6280InterruptBoundary {
public:
    // The interrupt lines, as the machine's request register holds them.
    // The three the mask can admit, and the one it cannot.
    static constexpr std::uint32_t kIrq2 = 0x001;
    static constexpr std::uint32_t kIrq1 = 0x002;
    static constexpr std::uint32_t kTimer = 0x004;
    static constexpr std::uint32_t kReset = 0x020;
    static constexpr std::uint32_t kMaskable = kIrq2 | kIrq1 | kTimer;

    // `P & I_FLAG`, which is the only encoding the emulator's savestate ever
    // holds in the latched I flag. The model holds a boolean, because that
    // is what the value is; this is where the two meet.
    static constexpr std::int32_t kIFlag = 0x04;

    // Where each vector is fetched from. The reset vector is the machine's
    // own, and the three interrupts descend from it two bytes at a time in
    // priority order -- which is a coincidence of the layout rather than the
    // rule, so the rule below does not use it.
    static constexpr std::uint16_t kResetVector = 0xFFFE;
    static constexpr std::uint16_t kTimerVector = 0xFFFA;
    static constexpr std::uint16_t kIrq1Vector = 0xFFF8;
    static constexpr std::uint16_t kIrq2Vector = 0xFFF6;

    // What the boundary decided. `kInstruction` is all but a handful of
    // them: the machine asks this question once per instruction and the
    // answer is almost always that the next thing is the next instruction.
    enum class Decision : std::uint8_t {
        kInstruction = 0,
        kReset,
        kTimer,
        kIrq1,
        kIrq2,
    };

    // What the interrupt controller is holding at the moment it is asked.
    // Both are read at the sample and again at the decision, because the
    // rule does not treat the two moments alike.
    struct Lines {
        std::uint32_t requests = 0;
        std::uint8_t mask = 0;
    };

    // Which paths the boundary has run. Every counter here is driven once
    // per instruction or less, so the shape of a census over these is a very
    // large `instructions` and a small tail -- and the tail is the whole
    // interest.
    struct Coverage {
        // Latches taken and decisions made. A machine takes one of each per
        // instruction, but not in step: a sample is taken on the last cycle
        // of an instruction and the decision at the top of the next, so a
        // machine stopped mid-frame has one more of the first than the
        // second.
        std::uint64_t samples = 0;
        std::uint64_t decisions = 0;
        // Samples that latched a line rather than nothing, and samples taken
        // behind a raised I flag, which latch nothing whatever the lines are
        // doing.
        std::uint64_t pending_samples = 0;
        std::uint64_t masked_samples = 0;
        // Decisions by outcome.
        std::uint64_t instructions = 0;
        std::uint64_t resets = 0;
        std::uint64_t timers = 0;
        std::uint64_t irq1s = 0;
        std::uint64_t irq2s = 0;
        // Decisions the lines woke -- something was asserted or latched --
        // that admitted no vector at all. This is the ordinary state of a
        // machine whose timer line is up but masked, so it is large.
        std::uint64_t declined = 0;
        // Timer vectors taken on a line the latch did not hold: the
        // asymmetry, counted rather than described. A machine that never
        // reached this would have been just as well served by testing the
        // sample, which is what makes the number worth printing.
        std::uint64_t late_timers = 0;
        // Resets put into the latch, and resets taken back out of it.
        std::uint64_t reset_signals = 0;
        std::uint64_t reset_acknowledgements = 0;
    };

    // The two fields the emulator's savestate carries for this boundary, in
    // its order and its encoding. The latched I flag is written out as
    // `kIFlag` or zero because that is the only thing `P & I_FLAG` can be;
    // a state carrying anything else was not written by this machine, and
    // loading one normalises rather than reproducing it.
    struct State {
        std::int32_t i_flag_sample = 0;
        std::int32_t irq_sample = 0;
    };

    HuC6280InterruptBoundary() noexcept = default;

    // The rule, with nothing latched and nothing counted.
    //
    // `latch` is `HuC6280::LastCycle` without the cycle: the lines the mask
    // admits, and nothing at all if the I flag is set.
    [[nodiscard]] static std::uint32_t latch(
        Lines lines,
        bool i_flag_set) noexcept;
    // `decide` is the top of `HuC6280::RunSub`: what a boundary holding
    // `irq_sample` and `i_flag_sample` does about lines currently reading
    // `lines`. Every input is an argument, so a test can enumerate the whole
    // of it -- including the states no machine can reach.
    [[nodiscard]] static Decision decide(
        std::uint32_t irq_sample,
        bool i_flag_sample,
        Lines lines) noexcept;
    [[nodiscard]] static std::uint16_t vector(Decision decision) noexcept;

    // `HuC6280::Reset`: the latch is set to the reset line alone. It is the
    // only thing that ever puts that line there, and the latched I flag is
    // deliberately untouched -- the emulator does not clear it here, in
    // `Power`, or in `Init`, so neither does this.
    void reset() noexcept;

    // `LastCycle` and the top of `RunSub`, latching and counting.
    void sample(Lines lines, bool i_flag_set) noexcept;
    [[nodiscard]] Decision select(Lines lines) noexcept;

    // The reset line leaves the latch when the machine takes the vector.
    // Only the latch: the request itself and the mask belong to the
    // interrupt controller, which is told separately by the same machine.
    void acknowledge_reset() noexcept;

    [[nodiscard]] std::uint32_t irq_sample() const noexcept
    {
        return irq_sample_;
    }
    [[nodiscard]] bool i_flag_sample() const noexcept
    {
        return i_flag_sample_;
    }

    [[nodiscard]] const Coverage& coverage() const noexcept
    {
        return coverage_;
    }

    void capture_state(State& state) const noexcept;
    void apply_state(const State& state) noexcept;

private:
    std::uint32_t irq_sample_ = 0;
    bool i_flag_sample_ = false;

    Coverage coverage_{};
};

}  // namespace devils_crush::cpu
