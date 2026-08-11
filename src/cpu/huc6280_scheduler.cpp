// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/cpu/huc6280_scheduler.hpp"

namespace devils_crush::cpu {

void HuC6280Scheduler::init() noexcept
{
    registers_.timestamp = 0;
    registers_.timestamp_fraction = 0;
    registers_.next_user_event = 0;
    registers_.next_event = 0;
    registers_.overclock_rate = 1;
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
    registers_.overclock_rate = (rate == 0) ? 1 : rate;
}

void HuC6280Scheduler::set_speed(const std::uint8_t speed) noexcept
{
    if (registers_.speed != speed) {
        coverage_.speed_changes++;
    }

    registers_.speed = speed;
    registers_.speed_shift =
        static_cast<std::uint8_t>((registers_.speed ^ 1) << 1);
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
            static_cast<std::int32_t>(registers_.timestamp));
    }
}

std::int32_t HuC6280Scheduler::divide(
    const std::int32_t master_clocks) noexcept
{
    std::uint32_t& fraction = registers_.timestamp_fraction;
    const std::uint32_t rate = registers_.overclock_rate;

    fraction += static_cast<std::uint32_t>(master_clocks);

    if (fraction >= rate) {
        const std::int32_t divided =
            static_cast<std::int32_t>(fraction / rate);
        fraction %= rate;
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

    registers_.timestamp += static_cast<std::uint32_t>(moved);
    registers_.next_event -= moved;
    registers_.next_user_event -= moved;

    if (registers_.next_event <= 0) {
        coverage_.horizon_reached++;
        reach_horizon();
    }
}

void HuC6280Scheduler::advance(const std::int32_t cycles) noexcept
{
    coverage_.advances++;
    if (registers_.speed == kSlow) {
        coverage_.slow_cycles += static_cast<std::uint64_t>(cycles);
    } else {
        coverage_.fast_cycles += static_cast<std::uint64_t>(cycles);
    }

    step((cycles * kMasterClocksPerFastCycle) << registers_.speed_shift);
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

    if (registers_.next_user_event <= 0) {
        coverage_.user_events++;
        registers_.next_user_event = (user_event_hook_ != nullptr)
            ? user_event_hook_(
                  user_event_context_,
                  static_cast<std::int32_t>(registers_.timestamp))
            : kNoDeviceHorizon;
    }

    recalculate();
}

void HuC6280Scheduler::recalculate() noexcept
{
    coverage_.recalculations++;

    registers_.next_event = timer_horizon();

    if (registers_.next_event > registers_.next_user_event) {
        registers_.next_event = registers_.next_user_event;
        coverage_.user_nearer++;
    } else {
        coverage_.timer_nearer++;
    }
}

void HuC6280Scheduler::set_event(const std::int32_t cycles) noexcept
{
    coverage_.set_events++;

    registers_.next_user_event = cycles;
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

    registers_.timestamp = static_cast<std::uint32_t>(base);
}

void HuC6280Scheduler::run(const bool step_mode) noexcept
{
    coverage_.runs++;

    // Minus one rather than one so a block move in progress is not cut
    // short by the single step that resumed it.
    registers_.run_flag = step_mode ? -1 : 1;
}

void HuC6280Scheduler::exit() noexcept
{
    coverage_.exits++;

    registers_.run_flag = 0;
}

void HuC6280Scheduler::capture_state(State& state) const noexcept
{
    state.run_flag = registers_.run_flag;
    state.speed = registers_.speed;
    state.next_event = registers_.next_event;
    state.next_user_event = registers_.next_user_event;
}

void HuC6280Scheduler::apply_state(const State& state) noexcept
{
    registers_.run_flag = state.run_flag;
    registers_.next_event = state.next_event;
    registers_.next_user_event = state.next_user_event;

    // The emulator's own repair, applied here rather than through the
    // setter: the shift that shadows the rate has to follow it in, but a
    // restored rate is not a rate change and is not counted as one.
    registers_.speed = state.speed & 0x01;
    registers_.speed_shift =
        static_cast<std::uint8_t>((registers_.speed ^ 1) << 1);
}

}  // namespace devils_crush::cpu
