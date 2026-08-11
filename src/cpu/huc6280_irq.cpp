// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/cpu/huc6280_irq.hpp"

namespace devils_crush::cpu {

HuC6280Irq::HuC6280Irq() noexcept = default;

void HuC6280Irq::count_line_changes(
    const std::uint32_t changed,
    std::uint64_t (&counters)[3]) noexcept
{
    for (unsigned bit = 0; bit < 3; bit++) {
        if ((changed & (1u << bit)) != 0) {
            counters[bit]++;
        }
    }
}

void HuC6280Irq::init() noexcept
{
    timer_lastts_ = 0;
    timer_inreload_ = false;
    timer_status_ = 0;
    timer_value_ = 0;
    timer_load_ = 0;
    timer_div_ = 0;
}

void HuC6280Irq::power() noexcept
{
    irq_low_ = 0;
    reset();
}

void HuC6280Irq::reset() noexcept
{
    timer_inreload_ = false;
    timer_div_ = 1024;
    timer_load_ = 0;
    timer_value_ = 0;
    timer_status_ = 0;

    // Assignment, not an added line: whatever else was asserted is dropped.
    const std::uint32_t previous = irq_low_;
    irq_low_ = kReset;
    count_line_changes(previous & ~irq_low_, coverage_.requests_lowered);
}

void HuC6280Irq::reset_timestamp(const std::int32_t base) noexcept
{
    timer_lastts_ = base;
}

void HuC6280Irq::acknowledge_reset() noexcept
{
    irq_mask_ = 0x7;
    end_request(kReset);
}

void HuC6280Irq::begin_request(const std::uint32_t lines) noexcept
{
    const std::uint32_t changed = lines & ~irq_low_;
    irq_low_ |= lines;
    count_line_changes(changed, coverage_.requests_raised);
}

void HuC6280Irq::end_request(const std::uint32_t lines) noexcept
{
    const std::uint32_t changed = lines & irq_low_;
    irq_low_ &= ~lines;
    count_line_changes(changed, coverage_.requests_lowered);
}

void HuC6280Irq::raise_timer_request() noexcept
{
    coverage_.timer_underflows++;
    begin_request(kTimer);
}

void HuC6280Irq::sync(const std::int32_t timestamp) noexcept
{
    coverage_.syncs++;

    timer_div_ -= timestamp - timer_lastts_;

    std::uint64_t ticks = 0;
    while (timer_div_ <= 0) {
        // A tick is a full timer period, except for the one that carries a
        // reload. The reload tick is one clock short and the tick that
        // *starts* it is one clock long, so a period straddling an
        // underflow still costs the same as any other.
        std::int32_t reload_div = kTickClocks;

        if (timer_inreload_) {
            timer_value_ = timer_load_;
            reload_div = reload_div - 1;
            timer_inreload_ = false;
            coverage_.timer_reloads++;
        } else if (timer_status_ != 0) {
            timer_value_--;
            if (timer_value_ < 0) {
                timer_inreload_ = true;
                reload_div = 1;
                raise_timer_request();
            }
        }

        timer_div_ += reload_div;
        ticks++;
    }

    coverage_.ticks += ticks;
    if (ticks > 1) {
        coverage_.multi_tick_syncs++;
    }

    timer_lastts_ = timestamp;
}

std::uint8_t HuC6280Irq::read_timer(
    const std::uint8_t io_data_buffer) noexcept
{
    coverage_.counter_reads++;

    // The counter is seven bits and the eighth comes off the bus latch.
    return static_cast<std::uint8_t>(
        (timer_value_ & 0xFF) | (io_data_buffer & 0x80));
}

void HuC6280Irq::write_timer(
    const std::uint32_t address,
    const std::uint8_t value) noexcept
{
    switch (address & 1) {
        case 0:
            timer_load_ = value & kCounterMask;
            coverage_.reload_writes++;
            break;

        case 1:
            coverage_.control_writes++;
            if ((value & 1) != 0) {
                // Only a start from a stopped timer reloads the counter and
                // restarts the divider. Writing the enable bit again while
                // it is already set does nothing at all, which is what lets
                // the game poll this register without disturbing the phase.
                if (timer_status_ == 0) {
                    timer_div_ = kTickClocks;
                    timer_value_ = timer_load_;
                    coverage_.timer_starts++;
                }
            } else if (timer_status_ != 0) {
                coverage_.timer_stops++;
            }
            timer_status_ = value & 1;
            break;

        default:
            break;
    }
}

std::uint8_t HuC6280Irq::read_status(
    const std::uint32_t address,
    const std::uint8_t io_data_buffer) noexcept
{
    if ((address & 2) == 0) {
        return io_data_buffer;
    }

    if ((address & 1) == 0) {
        coverage_.mask_reads++;
        coverage_.acknowledge_reads++;
        end_request(kTimer);
    } else {
        coverage_.status_reads++;
    }

    return peek_status(address, io_data_buffer);
}

std::uint8_t HuC6280Irq::peek_status(
    const std::uint32_t address,
    const std::uint8_t io_data_buffer) const noexcept
{
    if ((address & 2) == 0) {
        return io_data_buffer;
    }

    if ((address & 1) == 0) {
        // The mask reads back inverted: a set bit here is a *disabled*
        // line, which is the opposite of how it is stored.
        return static_cast<std::uint8_t>(irq_mask_ ^ 0x7);
    }

    std::uint8_t status = 0;
    if ((irq_low_ & kIrq1) != 0) {
        status |= 2;
    }
    if ((irq_low_ & kIrq2) != 0) {
        status |= 1;
    }
    if ((irq_low_ & kTimer) != 0) {
        status |= 4;
    }

    return static_cast<std::uint8_t>(
        status | (io_data_buffer & static_cast<std::uint8_t>(~0x7)));
}

void HuC6280Irq::write_status(
    const std::uint32_t address,
    const std::uint8_t value) noexcept
{
    if ((address & 2) == 0) {
        return;
    }

    switch (address & 1) {
        case 0:
            irq_mask_ = static_cast<std::uint8_t>((value & 0x7) ^ 0x7);
            coverage_.mask_writes++;
            break;

        case 1:
            coverage_.acknowledge_writes++;
            end_request(kTimer);
            break;

        default:
            break;
    }
}

std::uint32_t HuC6280Irq::master_cycles_until_timer_interrupt(
    const std::int32_t timestamp,
    const bool i_flag) noexcept
{
    coverage_.horizon_queries++;

    if (i_flag || (irq_mask_ & kTimer) == 0) {
        coverage_.horizon_never++;
        return kNoTimerInterrupt;
    }
    if ((irq_low_ & kTimer) != 0) {
        coverage_.horizon_now++;
        return 0;
    }
    if (timer_status_ == 0) {
        coverage_.horizon_never++;
        return kNoTimerInterrupt;
    }

    // The divider has not been caught up to `timestamp`, so the distance
    // already run is subtracted here rather than by syncing first. A query
    // must not advance the device.
    std::int64_t remaining = static_cast<std::int64_t>(timer_div_)
        - (static_cast<std::int64_t>(timestamp)
            - static_cast<std::int64_t>(timer_lastts_));
    if (remaining < 0) {
        remaining = 0;
    }

    // Mid-reload, the counter still holds the old value and the reload is
    // what happens at the next tick, so the countdown starts from `load`
    // one short tick away.
    const std::int64_t horizon = timer_inreload_
        ? remaining + (kTickClocks - 1)
            + static_cast<std::int64_t>(timer_load_) * kTickClocks
        : remaining + static_cast<std::int64_t>(timer_value_) * kTickClocks;

    if (horizon < 0) {
        coverage_.horizon_now++;
        return 0;
    }
    if (horizon > static_cast<std::int64_t>(kNoTimerInterrupt)) {
        coverage_.horizon_never++;
        return kNoTimerInterrupt;
    }
    return static_cast<std::uint32_t>(horizon);
}

void HuC6280Irq::capture_state(State& state) const noexcept
{
    state.irq_low = irq_low_;
    state.irq_mask = irq_mask_;
    state.timer_inreload = timer_inreload_;
    state.timer_status = timer_status_;
    state.timer_value = timer_value_;
    state.timer_load = timer_load_;
    state.timer_div = timer_div_;
}

void HuC6280Irq::apply_state(const State& state) noexcept
{
    irq_low_ = state.irq_low;
    irq_mask_ = state.irq_mask;
    timer_inreload_ = state.timer_inreload;
    timer_status_ = state.timer_status;
    timer_value_ = state.timer_value;
    timer_load_ = state.timer_load;
    timer_div_ = state.timer_div;

    // A divider at or below zero would make the next catch-up spin until it
    // climbed back out one tick at a time, generating interrupts that never
    // happened. The emulator's loader makes the same repair.
    if (timer_div_ < 1) {
        timer_div_ = 1;
    }
}

}  // namespace devils_crush::cpu
