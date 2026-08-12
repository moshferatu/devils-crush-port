// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/input/pce_pad.hpp"

#include <cstddef>
#include <cstdint>

namespace devils_crush::input {

void PcePad::configure(
    const bool multitap_enabled,
    const bool disable_soft_reset,
    const bool cd_attached) noexcept
{
    multitap_enabled_ = multitap_enabled;
    disable_soft_reset_ = disable_soft_reset;
    cd_attached_ = cd_attached;
}

void PcePad::configure_port(
    const std::size_t port,
    const Device device) noexcept
{
    if (port >= kPortCount) {
        return;
    }

    // A device this model does not answer for is recorded as an empty port,
    // and counted, because answering as though the port were empty is a
    // defensible thing for a model to do and pretending it did not happen is
    // not.
    if (device == Device::mouse || device == Device::keyboard) {
        coverage_.unmodelled_port_devices++;
    }

    ports_[port].device = device;
}

void PcePad::set_frontend_policy(const FrontendPolicy& policy) noexcept
{
    policy_ = policy;
}

std::uint16_t PcePad::filtered(const std::uint16_t buttons) noexcept
{
    std::uint16_t value = buttons;

    // SELECT and RUN together are what a PC Engine game reads as a request to
    // start over, so a host that suppresses it has to suppress it here: the
    // machine below cannot tell a combination that was never pressed from one
    // that was filtered out.
    if (disable_soft_reset_ && (value & kSoftReset) == kSoftReset) {
        value &= static_cast<std::uint16_t>(~kSoftReset);
        coverage_.soft_resets_suppressed++;
    }

    // A pad's stick cannot be pushed two ways at once and a host's keyboard
    // can, so unless the host says otherwise the impossible pair is dropped
    // rather than passed to a game that has never had to consider it.
    if (!policy_.allow_opposing_directions) {
        if ((value & kUpAndDown) == kUpAndDown) {
            value &= static_cast<std::uint16_t>(~kUpAndDown);
            coverage_.opposing_directions_filtered++;
        }
        if ((value & kLeftAndRight) == kLeftAndRight) {
            value &= static_cast<std::uint16_t>(~kLeftAndRight);
            coverage_.opposing_directions_filtered++;
        }
    }

    return value;
}

void PcePad::set_port_input(
    const std::size_t port,
    const std::uint16_t buttons) noexcept
{
    if (port >= kPortCount) {
        return;
    }

    ports_[port].pending_buttons = filtered(buttons);
}

void PcePad::power(const std::int32_t) noexcept
{
    coverage_.powers++;

    read_index_ = 0;
    select_ = false;
    clear_ = false;

    for (Port& port : ports_) {
        port.buttons = 0;
        port.select = false;
        port.clear = false;
        port.six_button_half = false;
    }

    // What the host is holding down is deliberately untouched. A reset is the
    // machine's, and a button that is still pressed through one is still
    // pressed after it -- the next frame latch is where that becomes visible
    // again, exactly as it does for the emulator's own devices.
}

void PcePad::frame(const bool start_frame) noexcept
{
    coverage_.frames++;
    if (start_frame) {
        coverage_.frame_starts++;
    }

    // The one moment a device's answer can change. Everything between two of
    // these reads the same buttons however many times the game strobes the
    // port, which is what makes a frame's input a single fixed thing rather
    // than something a long frame could see move underneath it.
    for (Port& port : ports_) {
        if (port.device == Device::gamepad) {
            port.buttons = port.pending_buttons;
        }
    }
}

void PcePad::transform_input() noexcept
{
    coverage_.transforms++;

    if (!disable_soft_reset_) {
        return;
    }

    for (Port& port : ports_) {
        if ((port.pending_buttons & kSoftReset) == kSoftReset) {
            port.pending_buttons &=
                static_cast<std::uint16_t>(~kSoftReset);
            coverage_.soft_resets_suppressed++;
        }
    }
}

void PcePad::adjust_timestamp(const std::int32_t) noexcept
{
    // Nothing to rebase. A two-button gamepad answers from a latch and holds
    // no timestamp of its own, so the machine's frame rebase reaches a device
    // with no time in it -- which is worth counting rather than eliding,
    // because the count is what says the port is still being clocked.
    coverage_.timestamp_rebases++;
}

std::uint8_t PcePad::read_port(const std::size_t port) noexcept
{
    // An empty port floats every line high, which is also what a port whose
    // device this model does not answer for gets.
    if (ports_[port].device != Device::gamepad) {
        coverage_.absent_port_reads++;
        return 0x0FU;
    }

    const Port& pad = ports_[port];
    std::uint8_t value = 0x0FU;

    if (pad.six_button_half && (pad.buttons & kSixButtonPad) != 0U) {
        coverage_.six_button_reads++;
        value ^= pad.select
            ? 0x0FU
            : static_cast<std::uint8_t>((pad.buttons >> 8U) & 0x0FU);
    } else if (pad.select) {
        coverage_.select_high_reads++;
        value ^= static_cast<std::uint8_t>((pad.buttons >> 4U) & 0x0FU);
    } else {
        coverage_.select_low_reads++;
        value ^= static_cast<std::uint8_t>(pad.buttons & 0x0FU);
    }

    // A pad answers active-low, so any bit cleared here is a button held.
    if (value != 0x0FU) {
        coverage_.reads_with_a_button_down++;
    }

    return value;
}

std::uint8_t PcePad::read(const std::int32_t, const std::uint32_t) noexcept
{
    coverage_.reads++;

    std::uint8_t value = 0;

    // A keyboard on the first port takes the multitap out of the path
    // entirely, because it is a device that answers the whole port itself.
    if (multitap_enabled_ && ports_[0].device != Device::keyboard) {
        coverage_.multitap_reads++;
        if (read_index_ >= kPortCount) {
            // Zero rather than the all-high pattern of an empty port, and the
            // difference is the whole point of it: a game walks the counter
            // past the end and reads zero to learn how many devices there are.
            coverage_.reads_past_the_last_port++;
        } else {
            value = read_port(read_index_);
        }
    } else {
        coverage_.direct_reads++;
        value = read_port(0);
    }

    // The port's own bits, which belong to no device: whether a CD unit is
    // attached, and two that read back set on this hardware regardless.
    if (!cd_attached_) {
        value |= kNoCdUnit;
    }
    value |= kAlwaysSet;

    return value;
}

void PcePad::write_port(
    const std::size_t port,
    const bool old_select,
    const bool new_select,
    const bool old_clear,
    const bool new_clear) noexcept
{
    if (ports_[port].device != Device::gamepad) {
        return;
    }

    Port& pad = ports_[port];
    pad.select = new_select;
    pad.clear = new_clear;

    // A six-button pad swaps which half of itself it will answer with on the
    // falling edge of the clear line while select is held. The swap happens on
    // a two-button pad too; it is simply never visible, because the half it
    // selects is only reached by a pad that says it has one.
    if (old_select && new_select && old_clear && !new_clear) {
        pad.six_button_half = !pad.six_button_half;
        coverage_.six_button_toggles++;
    }
}

void PcePad::write(
    const std::int32_t,
    const std::uint32_t,
    const std::uint8_t value) noexcept
{
    coverage_.writes++;

    const bool new_select = (value & 0x01U) != 0U;
    const bool new_clear = (value & 0x02U) != 0U;

    if (multitap_enabled_) {
        for (std::size_t port = 0; port < kPortCount; port++) {
            write_port(port, select_, new_select, clear_, new_clear);
        }

        // Which device answers next. The reset edge is qualified by select
        // being high so that a game which pulses clear on its own does not
        // rewind the counter it is walking; the increment is the select edge,
        // taken only while clear has stayed low across the write.
        if (new_select && !clear_ && new_clear) {
            read_index_ = 0;
            coverage_.counter_resets++;
        } else if (!clear_ && !new_clear && !select_ && new_select) {
            if (read_index_ < 255U) {
                read_index_++;
                coverage_.counter_increments++;
            } else {
                coverage_.counter_saturations++;
            }
        }
    } else {
        write_port(0, select_, new_select, clear_, new_clear);
    }

    select_ = new_select;
    clear_ = new_clear;
}

void PcePad::capture_state(State& state) const noexcept
{
    state.select = select_;
    state.clear = clear_;
    state.read_index = read_index_;

    for (std::size_t port = 0; port < kPortCount; port++) {
        state.ports[port].buttons = ports_[port].buttons;
        state.ports[port].select = ports_[port].select;
        state.ports[port].clear = ports_[port].clear;
        state.ports[port].six_button_half = ports_[port].six_button_half;
    }
}

void PcePad::apply_state(const State& state) noexcept
{
    select_ = state.select;
    clear_ = state.clear;
    read_index_ = state.read_index;

    for (std::size_t port = 0; port < kPortCount; port++) {
        ports_[port].buttons = state.ports[port].buttons;
        ports_[port].select = state.ports[port].select;
        ports_[port].clear = state.ports[port].clear;
        ports_[port].six_button_half = state.ports[port].six_button_half;
    }

    // What the host is holding is not in a savestate and must not be: it is
    // the present, and a state that restored it would be a machine pressing
    // whatever was pressed when the state was written.
}

}  // namespace devils_crush::input
