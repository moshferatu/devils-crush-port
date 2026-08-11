// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/cpu/huc6280_interrupt_boundary.hpp"

namespace devils_crush::cpu {

std::uint32_t HuC6280InterruptBoundary::latch(
    const Lines lines,
    const bool i_flag_set) noexcept
{
    // The emulator writes this as `(requests & mask) & PIMaskCache`, where
    // the cache is all ones or all zeros according to the I flag. The branch
    // and the multiply-by-a-mask are the same function; the branch is the
    // one that says what it means.
    if (i_flag_set) {
        return 0;
    }

    return lines.requests & lines.mask;
}

HuC6280InterruptBoundary::Decision HuC6280InterruptBoundary::decide(
    const std::uint32_t irq_sample,
    const bool i_flag_sample,
    const Lines lines) noexcept
{
    // The fast exit, and the reason the boundary costs nothing on almost
    // every instruction. It tests the live request register as well as the
    // latch, which is what makes the timer's late arrival below reachable at
    // all: a machine whose latch is empty still gets this far if a line went
    // up during the last instruction.
    if ((irq_sample | lines.requests) == 0) {
        return Decision::kInstruction;
    }

    if ((irq_sample & kReset) != 0) {
        return Decision::kReset;
    }

    // Live lines, live mask, latched I flag: the one branch of the four that
    // reads the present rather than the past, and it still reads the past
    // for the gate.
    if ((lines.requests & kTimer & lines.mask) != 0 && !i_flag_sample) {
        return Decision::kTimer;
    }

    if ((irq_sample & kIrq1) != 0) {
        return Decision::kIrq1;
    }

    if ((irq_sample & kIrq2) != 0) {
        return Decision::kIrq2;
    }

    return Decision::kInstruction;
}

std::uint16_t HuC6280InterruptBoundary::vector(
    const Decision decision) noexcept
{
    switch (decision) {
    case Decision::kReset:
        return kResetVector;
    case Decision::kTimer:
        return kTimerVector;
    case Decision::kIrq1:
        return kIrq1Vector;
    case Decision::kIrq2:
        return kIrq2Vector;
    case Decision::kInstruction:
        break;
    }

    return 0;
}

void HuC6280InterruptBoundary::reset() noexcept
{
    irq_sample_ = kReset;
    coverage_.reset_signals++;
}

void HuC6280InterruptBoundary::sample(
    const Lines lines,
    const bool i_flag_set) noexcept
{
    irq_sample_ = latch(lines, i_flag_set);
    i_flag_sample_ = i_flag_set;

    coverage_.samples++;
    if (i_flag_set) {
        coverage_.masked_samples++;
    }
    if (irq_sample_ != 0) {
        coverage_.pending_samples++;
    }
}

HuC6280InterruptBoundary::Decision HuC6280InterruptBoundary::select(
    const Lines lines) noexcept
{
    const Decision decision = decide(irq_sample_, i_flag_sample_, lines);

    coverage_.decisions++;

    switch (decision) {
    case Decision::kInstruction:
        coverage_.instructions++;
        if ((irq_sample_ | lines.requests) != 0) {
            coverage_.declined++;
        }
        break;
    case Decision::kReset:
        coverage_.resets++;
        break;
    case Decision::kTimer:
        coverage_.timers++;
        if ((irq_sample_ & kTimer) == 0) {
            coverage_.late_timers++;
        }
        break;
    case Decision::kIrq1:
        coverage_.irq1s++;
        break;
    case Decision::kIrq2:
        coverage_.irq2s++;
        break;
    }

    return decision;
}

void HuC6280InterruptBoundary::acknowledge_reset() noexcept
{
    irq_sample_ &= ~kReset;
    coverage_.reset_acknowledgements++;
}

void HuC6280InterruptBoundary::capture_state(State& state) const noexcept
{
    state.i_flag_sample = i_flag_sample_ ? kIFlag : 0;
    state.irq_sample = static_cast<std::int32_t>(irq_sample_);
}

void HuC6280InterruptBoundary::apply_state(const State& state) noexcept
{
    i_flag_sample_ = state.i_flag_sample != 0;
    irq_sample_ = static_cast<std::uint32_t>(state.irq_sample);
}

}  // namespace devils_crush::cpu
