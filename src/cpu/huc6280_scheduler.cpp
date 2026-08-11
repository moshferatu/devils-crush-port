// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/cpu/huc6280_scheduler.hpp"

namespace devils_crush::cpu {

void HuC6280Scheduler::init() noexcept
{
    timestamp_ = 0;
    timestamp_frac_ = 0;
    next_user_event_ = 0;
    next_event_ = 0;
    overclock_rate_ = 1;
}

void HuC6280Scheduler::set_timer_sync_hook(
    const TimerSyncHook hook,
    void* const context) noexcept
{
    timer_sync_hook_ = hook;
    timer_sync_context_ = context;
}

void HuC6280Scheduler::set_timer_horizon_hook(
    const TimerHorizonHook hook,
    void* const context) noexcept
{
    timer_horizon_hook_ = hook;
    timer_horizon_context_ = context;
}

void HuC6280Scheduler::set_timer_rebase_hook(
    const TimerRebaseHook hook,
    void* const context) noexcept
{
    timer_rebase_hook_ = hook;
    timer_rebase_context_ = context;
}

void HuC6280Scheduler::set_user_event_hook(
    const UserEventHook hook,
    void* const context) noexcept
{
    user_event_hook_ = hook;
    user_event_context_ = context;
}

void HuC6280Scheduler::set_overclock(const std::uint32_t rate) noexcept
{
    // A rate of zero is the one value the emulator cannot survive and the
    // one its setting cannot produce, so it is refused rather than
    // reproduced.
    overclock_rate_ = (rate == 0) ? 1 : rate;
}

void HuC6280Scheduler::set_speed(const std::uint8_t speed) noexcept
{
    if (speed_ != speed) {
        coverage_.speed_changes++;
    }

    speed_ = speed;
    speed_shift_ = (speed_ ^ 1) << 1;
}

std::int32_t HuC6280Scheduler::timer_horizon() const noexcept
{
    if (timer_horizon_hook_ == nullptr) {
        return kNoDeviceHorizon;
    }

    return timer_horizon_hook_(timer_horizon_context_);
}

void HuC6280Scheduler::sync_timer() noexcept
{
    if (timer_sync_hook_ != nullptr) {
        timer_sync_hook_(
            timer_sync_context_,
            static_cast<std::int32_t>(timestamp_));
    }
}

std::int32_t HuC6280Scheduler::divide(
    const std::int32_t master_clocks) noexcept
{
    timestamp_frac_ += static_cast<std::uint32_t>(master_clocks);

    if (timestamp_frac_ >= overclock_rate_) {
        const std::int32_t divided =
            static_cast<std::int32_t>(timestamp_frac_ / overclock_rate_);
        timestamp_frac_ %= overclock_rate_;
        return divided;
    }

    return 0;
}

void HuC6280Scheduler::step(const std::int32_t master_clocks) noexcept
{
    const std::int32_t moved = divide(master_clocks);

    if (moved == 0) {
        coverage_.stalled_advances++;
    }
    coverage_.master_clocks += static_cast<std::uint64_t>(moved);

    timestamp_ += static_cast<std::uint32_t>(moved);
    next_event_ -= moved;
    next_user_event_ -= moved;

    if (next_event_ <= 0) {
        coverage_.horizon_reached++;
        reach_horizon();
    }
}

void HuC6280Scheduler::advance(const std::int32_t cycles) noexcept
{
    coverage_.advances++;
    if (speed_ == kSlow) {
        coverage_.slow_cycles += static_cast<std::uint64_t>(cycles);
    } else {
        coverage_.fast_cycles += static_cast<std::uint64_t>(cycles);
    }

    step((cycles * kMasterClocksPerFastCycle) << speed_shift_);
}

void HuC6280Scheduler::advance_master(
    const std::int32_t master_clocks) noexcept
{
    coverage_.master_advances++;

    step(master_clocks);
}

void HuC6280Scheduler::reach_horizon() noexcept
{
    sync_timer();

    if (next_user_event_ <= 0) {
        coverage_.user_events++;
        next_user_event_ = (user_event_hook_ != nullptr)
            ? user_event_hook_(
                  user_event_context_,
                  static_cast<std::int32_t>(timestamp_))
            : kNoDeviceHorizon;
    }

    recalculate();
}

void HuC6280Scheduler::recalculate() noexcept
{
    coverage_.recalculations++;

    next_event_ = timer_horizon();

    if (next_event_ > next_user_event_) {
        next_event_ = next_user_event_;
        coverage_.user_nearer++;
    } else {
        coverage_.timer_nearer++;
    }
}

void HuC6280Scheduler::set_event(const std::int32_t cycles) noexcept
{
    coverage_.set_events++;

    next_user_event_ = cycles;
    recalculate();
}

void HuC6280Scheduler::sync_and_reset_timestamp(
    const std::int32_t base) noexcept
{
    coverage_.rebases++;

    sync_timer();

    if (timer_rebase_hook_ != nullptr) {
        timer_rebase_hook_(timer_rebase_context_, base);
    }

    timestamp_ = static_cast<std::uint32_t>(base);
}

void HuC6280Scheduler::run(const bool step_mode) noexcept
{
    coverage_.runs++;

    // Minus one rather than one so a block move in progress is not cut
    // short by the single step that resumed it.
    run_flag_ = step_mode ? -1 : 1;
}

void HuC6280Scheduler::exit() noexcept
{
    coverage_.exits++;

    run_flag_ = 0;
}

void HuC6280Scheduler::capture_state(State& state) const noexcept
{
    state.run_flag = run_flag_;
    state.speed = speed_;
    state.next_event = next_event_;
    state.next_user_event = next_user_event_;
}

void HuC6280Scheduler::apply_state(const State& state) noexcept
{
    run_flag_ = state.run_flag;
    next_event_ = state.next_event;
    next_user_event_ = state.next_user_event;

    // The emulator's own repair, applied here rather than through the
    // setter: the shift that shadows the rate has to follow it in, but a
    // restored rate is not a rate change and is not counted as one.
    speed_ = state.speed & 0x01;
    speed_shift_ = (speed_ ^ 1) << 1;
}

}  // namespace devils_crush::cpu
