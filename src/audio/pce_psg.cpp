// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/audio/pce_psg.hpp"

#include <cmath>

namespace devils_crush::audio {
namespace {

// Above this period the accumulating output mode is not worth taking: the
// waveform is stepping slowly enough that each step is an audible edge.
// At or below it the channel is running faster than the output buffer can
// represent and the whole 32-entry table collapses to its mean.
constexpr std::uint32_t kAccumulatePeriodCutoff = 0x7U;

// The balance nibbles are not linear. Each step is roughly 1.5 dB, and the
// hardware maps its sixteen settings onto the same 32-step attenuation scale
// the channel volume uses.
constexpr std::array<int, 16> kScaleTable = {
    0x00, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x0F,
    0x10, 0x13, 0x15, 0x17, 0x19, 0x1B, 0x1D, 0x1F,
};

// A band-limited step, at the two sub-positions an edge can land on within
// one output position. Each row sums to 2,048.
constexpr std::array<std::array<std::int16_t, 7>, 2> kPhaseFilter = {{
    {{35, 250, 579, 641, 425, 112, 6}},
    {{6, 112, 425, 641, 579, 250, 35}},
}};

constexpr std::uint32_t clock_lfsr(std::uint32_t lfsr) noexcept
{
    const std::uint32_t new_bit =
        ((lfsr >> 0U) ^ (lfsr >> 1U) ^ (lfsr >> 11U) ^ (lfsr >> 12U)
         ^ (lfsr >> 17U))
        & 1U;
    return (lfsr >> 1U) | (new_bit << 17U);
}

}  // namespace

PcePsg::PcePsg(
    std::int32_t* left_buffer,
    std::int32_t* right_buffer,
    const Revision revision) noexcept
    : buffers_{left_buffer, right_buffer}
    , revision_(revision)
{
    lastts_ = 0;
    for (auto& channel : channels_) {
        channel.previous_sample[0] = 0;
        channel.previous_sample[1] = 0;
        channel.lastts = 0;
    }

    build_volume_tables();
    power(0);
}

void PcePsg::configure(
    std::int32_t* left_buffer,
    std::int32_t* right_buffer,
    const Revision revision) noexcept
{
    buffers_[0] = left_buffer;
    buffers_[1] = right_buffer;

    if (revision == revision_) {
        return;
    }

    revision_ = revision;
    build_volume_tables();

    // The two revisions disagree about when a channel is off, so the output
    // modes have to be settled again against the new one.
    for (int index = 0; index < kChannelCount; ++index) {
        recalc_output_mode(index);
    }
}

void PcePsg::build_volume_tables() noexcept
{
    for (int level = 0; level < 32; ++level) {
        double scale = 1.0 * 1.0 * 8 / 6;

        if (level != 0) {
            scale /= std::pow(
                2.0,
                static_cast<double>(1) / 4 * static_cast<double>(level));
        }
        if (level == 0x1F) {
            scale = 0.0;
        }

        for (int sample = 0; sample < 32; ++sample) {
            const int effective = revision_ == Revision::HuC6280
                ? sample * 2
                : sample * 2 - 0x1F;

            volume_table_[static_cast<std::size_t>(level)]
                         [static_cast<std::size_t>(sample)] =
                static_cast<std::int32_t>(
                    scale * static_cast<double>(effective) * 128);
            volume_only_table_[static_cast<std::size_t>(level)] =
                static_cast<std::int32_t>(scale * 65536);
        }
    }
}

void PcePsg::emit(
    const std::int32_t timestamp,
    Channel& channel,
    const std::int32_t left_sample,
    const std::int32_t right_sample) noexcept
{
    const std::array<std::int32_t, 2> delta = {
        left_sample - channel.previous_sample[0],
        right_sample - channel.previous_sample[1],
    };

    const auto& taps = kPhaseFilter[
        static_cast<std::size_t>((timestamp >> 1) & 1)];
    const auto position =
        static_cast<std::size_t>((timestamp >> 2) & 0xFFFF);

    for (std::size_t side = 0; side < 2U; ++side) {
        std::int32_t* buffer = buffers_[side] + position;
        const std::int32_t side_delta = delta[side];
        for (std::size_t tap = 0; tap < 7U; ++tap) {
            buffer[tap] += side_delta * static_cast<std::int32_t>(taps[tap]);
        }
    }

    channel.previous_sample[0] = left_sample;
    channel.previous_sample[1] = right_sample;
}

void PcePsg::update_output(
    const std::int32_t timestamp,
    Channel& channel) noexcept
{
    const auto channel_index = static_cast<std::size_t>(
        &channel - channels_.data());
    const auto count = [this, &channel](PathCoverage& path) {
        ++path.total;
        ++path.by_channel[
            static_cast<std::size_t>(&channel - channels_.data())];
    };

    if ((output_channel_mask_ & (1U << channel_index)) == 0U) {
        count(coverage_.silent);
        emit(timestamp, channel, 0, 0);
        return;
    }

    switch (channel.output_mode) {
    case OutputMode::Off:
        count(coverage_.silent);
        emit(timestamp, channel, 0, 0);
        break;

    case OutputMode::Normal: {
        count(coverage_.waveform);
        const auto sample = static_cast<std::size_t>(channel.dda);
        emit(
            timestamp,
            channel,
            volume_table_[static_cast<std::size_t>(channel.vl[0])][sample],
            volume_table_[static_cast<std::size_t>(channel.vl[1])][sample]);
        break;
    }

    case OutputMode::Noise: {
        count(coverage_.noise);
        // The LFSR's low bit selects between the extremes of the five-bit
        // sample range rather than between two waveform entries.
        const auto bit = static_cast<std::int32_t>(channel.lfsr & 1U);
        const auto sample = static_cast<std::size_t>((bit << 5) - bit);
        emit(
            timestamp,
            channel,
            volume_table_[static_cast<std::size_t>(channel.vl[0])][sample],
            volume_table_[static_cast<std::size_t>(channel.vl[1])][sample]);
        break;
    }

    case OutputMode::Accumulate: {
        count(coverage_.accumulated);
        // The census route reaches this seven times against seven million for
        // the waveform path, so it is the differential test against the
        // emulator's own device that proves it, not the route.
        //
        // 31 * 32 waveform entries is 992, whose midpoint is 496. The
        // HuC6280A's samples are already centred, so only the plain part
        // subtracts it.
        const std::int32_t accumulated = revision_ == Revision::HuC6280
            ? channel.sample_accumulator
            : channel.sample_accumulator - 496;
        const std::int32_t left =
            (volume_only_table_[static_cast<std::size_t>(channel.vl[0])]
             * accumulated)
            >> (8 + 5);
        const std::int32_t right =
            (volume_only_table_[static_cast<std::size_t>(channel.vl[1])]
             * accumulated)
            >> (8 + 5);
        emit(timestamp, channel, left, right);
        break;
    }
    }
}

void PcePsg::set_output_channel_mask(
    const std::int32_t timestamp,
    const std::uint8_t mask) noexcept
{
    const auto bounded_mask = static_cast<std::uint8_t>(mask & 0x3FU);
    if (bounded_mask == output_channel_mask_) {
        return;
    }

    update(timestamp);
    const auto changed = static_cast<std::uint8_t>(
        bounded_mask ^ output_channel_mask_);
    output_channel_mask_ = bounded_mask;
    for (std::size_t index = 0; index < channels_.size(); ++index) {
        if ((changed & (1U << index)) != 0U) {
            update_output(timestamp, channels_[index]);
        }
    }
}

// Must run after recalc_frequency_period; the choice of mode reads the period
// it computes.
void PcePsg::recalc_output_mode(const int channel_index) noexcept
{
    Channel& channel = channels_[static_cast<std::size_t>(channel_index)];

    if ((revision_ != Revision::HuC6280 && (channel.control & 0xC0U) == 0U)
        || (revision_ == Revision::HuC6280
            && (channel.control & 0x80U) == 0U)) {
        channel.output_mode = OutputMode::Off;
    } else if ((channel.noisectrl & channel.control & 0x80U) != 0U) {
        channel.output_mode = OutputMode::Noise;
    } else if (
        (channel.control & 0xC0U) == 0x80U
        && channel.frequency_period <= kAccumulatePeriodCutoff
        && (channel_index != 1 || (lfo_control_ & 0x80U) == 0U)) {
        channel.output_mode = OutputMode::Accumulate;
    } else {
        channel.output_mode = OutputMode::Normal;
    }
}

void PcePsg::recalc_frequency_period(const int channel_index) noexcept
{
    Channel& channel = channels_[static_cast<std::size_t>(channel_index)];

    if (channel_index == 0 && (lfo_control_ & 0x03U) != 0U) {
        // The game never comes here. Its sequencer writes $0809 from $2E41,
        // and every store in the ROM that names that byte writes a zero to
        // it, so the LFO is off on all 3,605 of the census route's arrivals
        // and another route would report the same. This branch, the channel-1
        // multiplier below, and the modulator recursion in run_channel are
        // proved instead by driving this model and the emulator's own device
        // from the same synthetic register stream -- tests/psg_device_tests.
        //
        // Channel 1's current waveform sample, biased around its midpoint,
        // is added to channel 0's period at one of three depths.
        const auto shift =
            static_cast<std::uint32_t>(((lfo_control_ & 0x03U) - 1U) << 1U);
        const auto modulator = static_cast<std::int32_t>(channels_[1].dda);
        const std::uint32_t offset =
            static_cast<std::uint32_t>(modulator - 0x10) << shift;
        const std::uint32_t period =
            (static_cast<std::uint32_t>(channel.frequency) + offset) & 0xFFFU;

        channel.frequency_period = (period != 0U ? period : 4096U) << 1U;
    } else {
        channel.frequency_period =
            (channel.frequency != 0U
                 ? static_cast<std::uint32_t>(channel.frequency)
                 : 4096U)
            << 1U;

        if (channel_index == 1 && (lfo_control_ & 0x03U) != 0U) {
            channel.frequency_period *=
                lfo_frequency_ != 0U
                ? static_cast<std::uint32_t>(lfo_frequency_)
                : 256U;
        }
    }
}

void PcePsg::recalc_noise_period(const int channel_index) noexcept
{
    Channel& channel = channels_[static_cast<std::size_t>(channel_index)];
    std::int32_t period =
        0x1F - static_cast<std::int32_t>(channel.noisectrl & 0x1FU);

    if (period == 0) {
        period = 0x20;
    } else {
        period <<= 6;
    }
    period <<= 1;

    channel.noise_period = static_cast<std::uint32_t>(period);
}

// Total attenuation is the sum of three five-bit reductions -- the global
// balance nibble, the channel balance nibble and the channel volume -- capped
// at silence.
std::int32_t PcePsg::attenuation(
    const int channel_index,
    const int side) const noexcept
{
    const Channel& channel =
        channels_[static_cast<std::size_t>(channel_index)];
    const unsigned shift = side != 0 ? 0U : 4U;

    const int global = 0x1F
        - kScaleTable[static_cast<std::size_t>(
            (global_balance_ >> shift) & 0x0FU)];
    const int balance = 0x1F
        - kScaleTable[static_cast<std::size_t>(
            (channel.balance >> shift) & 0x0FU)];
    const int level = 0x1F - static_cast<int>(channel.control & 0x1FU);

    const int reduction = global + balance + level;
    return static_cast<std::int32_t>(reduction > 0x1F ? 0x1F : reduction);
}

void PcePsg::write(
    const std::int32_t timestamp,
    const std::uint8_t address,
    const std::uint8_t value) noexcept
{
    const std::uint8_t port = address & 0x0FU;

    if (port == 0x00U) {
        select_ = value & 0x07U;
        return;
    }

    update(timestamp);

    // The register file is only six channels wide; a selection past the end
    // reaches nothing, and the two global registers ignore it entirely.
    const bool channel_selected = select_ <= 5U;
    const auto index = static_cast<std::size_t>(select_);
    const auto channel_index = static_cast<int>(select_);

    switch (port) {
    default:
        break;

    case 0x01U:  // Global balance
        global_balance_ = value;
        volume_update_pending_ = true;
        break;

    case 0x02U:  // Channel period, low byte
        if (!channel_selected) {
            return;
        }
        channels_[index].frequency = static_cast<std::uint16_t>(
            (channels_[index].frequency & 0x0F00U) | value);
        recalc_frequency_period(channel_index);
        recalc_output_mode(channel_index);
        break;

    case 0x03U:  // Channel period, high nibble
        if (!channel_selected) {
            return;
        }
        channels_[index].frequency = static_cast<std::uint16_t>(
            (channels_[index].frequency & 0x00FFU)
            | ((value & 0x0FU) << 8U));
        recalc_frequency_period(channel_index);
        recalc_output_mode(channel_index);
        break;

    case 0x04U: {  // Channel enable, direct digital audio, volume
        if (!channel_selected) {
            return;
        }
        Channel& channel = channels_[index];

        // Leaving direct digital audio restarts the waveform.
        if ((channel.control & 0x40U) != 0U && (value & 0x40U) == 0U) {
            channel.waveform_index = 0U;
            channel.dda = channel.waveform[0];
            channel.counter =
                static_cast<std::int32_t>(channel.frequency_period);
        }

        // Enabling a waveform channel advances it one entry.
        if ((channel.control & 0x80U) == 0U && (value & 0x80U) != 0U) {
            if ((value & 0x40U) == 0U) {
                channel.waveform_index =
                    static_cast<std::uint8_t>((channel.waveform_index + 1U)
                                              & 0x1FU);
                channel.dda = channel.waveform[channel.waveform_index];
            }
        }

        channel.control = value;
        recalc_frequency_period(channel_index);
        recalc_output_mode(channel_index);
        volume_update_pending_ = true;
        break;
    }

    case 0x05U:  // Channel balance
        if (!channel_selected) {
            return;
        }
        channels_[index].balance = value;
        volume_update_pending_ = true;
        break;

    case 0x06U: {  // Channel waveform data
        if (!channel_selected) {
            return;
        }
        Channel& channel = channels_[index];
        const auto sample = static_cast<std::uint8_t>(value & 0x1FU);

        if ((channel.control & 0x40U) == 0U) {
            channel.sample_accumulator -= static_cast<std::int32_t>(
                channel.waveform[channel.waveform_index]);
            channel.waveform[channel.waveform_index] = sample;
            channel.sample_accumulator += static_cast<std::int32_t>(sample);
        }

        if ((channel.control & 0xC0U) == 0x00U) {
            channel.waveform_index = static_cast<std::uint8_t>(
                (channel.waveform_index + 1U) & 0x1FU);
        }

        // The sample latch is updated whether or not the channel is in
        // direct digital audio mode; only the write to the table is gated.
        if ((channel.control & 0x80U) != 0U) {
            channel.dda = sample;
            ++coverage_.sample_latch_writes;
        }
        break;
    }

    case 0x07U:  // Noise enable and period
        if (!channel_selected) {
            return;
        }
        if (select_ >= 4U) {
            channels_[index].noisectrl = value;
            recalc_noise_period(channel_index);
            recalc_output_mode(channel_index);
        }
        break;

    case 0x08U:  // LFO period
        lfo_frequency_ = value;
        break;

    case 0x09U:  // LFO trigger and control
        if ((value & 0x80U) != 0U) {
            channels_[1].waveform_index = 0U;
            channels_[1].dda = channels_[1].waveform[0];
            channels_[1].counter =
                static_cast<std::int32_t>(channels_[1].frequency_period);
        }
        lfo_control_ = value;
        recalc_frequency_period(0);
        recalc_output_mode(0);
        recalc_frequency_period(1);
        recalc_output_mode(1);
        break;
    }
}

void PcePsg::run_channel(
    const int channel_index,
    const std::int32_t timestamp,
    const bool lfo_on) noexcept
{
    Channel& channel = channels_[static_cast<std::size_t>(channel_index)];
    const std::int32_t running_timestamp = channel.lastts;
    const std::int32_t run_time = timestamp - channel.lastts;

    channel.lastts = timestamp;

    if (run_time == 0) {
        return;
    }

    update_output(running_timestamp, channel);

    if (channel_index >= 4) {
        const auto period = static_cast<std::int32_t>(channel.noise_period);

        channel.noisecount -= run_time;

        if (channel.output_mode == OutputMode::Noise) {
            while (channel.noisecount <= 0) {
                channel.lfsr = clock_lfsr(channel.lfsr);
                ++coverage_.lfsr_clocks;
                update_output(timestamp + channel.noisecount, channel);
                channel.noisecount += period;
            }
        } else {
            while (channel.noisecount <= 0) {
                channel.lfsr = clock_lfsr(channel.lfsr);
                ++coverage_.lfsr_clocks;
                channel.noisecount += period;
            }
        }
    }

    // A disabled channel does not clock its waveform, nor does the LFO
    // modulator while the trigger bit holds it, nor does direct digital
    // audio -- there the sample comes from the port, not the table.
    if ((channel.control & 0x80U) == 0U
        || (channel_index == 1 && (lfo_control_ & 0x80U) != 0U)
        || (channel.control & 0x40U) != 0U) {
        return;
    }

    channel.counter -= run_time;

    if (!lfo_on && channel.frequency_period <= kAccumulatePeriodCutoff) {
        // Faster than the output buffer resolves; skip the whole run in one
        // division rather than stepping it edge by edge.
        if (channel.counter <= 0) {
            ++coverage_.accumulate_fast_paths;
            const std::uint32_t steps =
                (static_cast<std::uint32_t>(0 - channel.counter)
                 / channel.frequency_period)
                + 1U;

            channel.counter = static_cast<std::int32_t>(
                static_cast<std::uint32_t>(channel.counter)
                + (steps * channel.frequency_period));

            channel.waveform_index = static_cast<std::uint8_t>(
                (static_cast<std::uint32_t>(channel.waveform_index) + steps)
                & 0x1FU);
            channel.dda = channel.waveform[channel.waveform_index];
        }
    }

    while (channel.counter <= 0) {
        channel.waveform_index = static_cast<std::uint8_t>(
            (channel.waveform_index + 1U) & 0x1FU);
        channel.dda = channel.waveform[channel.waveform_index];

        update_output(timestamp + channel.counter, channel);

        if (lfo_on) {
            // Channel 0's period depends on where channel 1 is, so channel 1
            // has to be brought up to this edge before the next one is
            // scheduled.
            ++coverage_.lfo_recursions;
            run_channel(1, timestamp + channel.counter, false);
            recalc_frequency_period(0);
            recalc_output_mode(0);

            channel.counter += static_cast<std::int32_t>(
                channel.frequency_period <= kAccumulatePeriodCutoff
                    ? kAccumulatePeriodCutoff
                    : channel.frequency_period);
        } else {
            channel.counter +=
                static_cast<std::int32_t>(channel.frequency_period);
        }
    }
}

void PcePsg::run_all_channels(
    const std::int32_t timestamp,
    const bool lfo_on) noexcept
{
    for (int index = 0; index < kChannelCount; ++index) {
        run_channel(index, timestamp, lfo_on && index == 0);
    }
}

void PcePsg::update(const std::int32_t timestamp) noexcept
{
    const std::int32_t run_time = timestamp - lastts_;

    if (volume_update_pending_ && volume_update_counter_ == 0
        && volume_update_which_ == 0) {
        volume_update_counter_ = 1;
        volume_update_pending_ = false;
    }

    bool lfo_on = (lfo_control_ & 0x03U) != 0U;

    if (lfo_on) {
        if ((channels_[1].control & 0x80U) == 0U
            || (lfo_control_ & 0x80U) != 0U) {
            lfo_on = false;
            recalc_frequency_period(0);
            recalc_output_mode(0);
        }
    }

    std::int32_t clocks = run_time;
    std::int32_t running_timestamp = lastts_;

    while (clocks > 0) {
        std::int32_t chunk = clocks;

        // The volume state machine walks one of 32 latch steps at a time and
        // has to be able to land inside this span, so the span is cut short
        // when its next step falls before the end.
        if (volume_update_counter_ > 0 && chunk > volume_update_counter_) {
            chunk = volume_update_counter_;
        }

        running_timestamp += chunk;
        clocks -= chunk;

        if (lfo_on) {
            ++coverage_.lfo_updates;
        }
        run_all_channels(running_timestamp, lfo_on);

        if (volume_update_counter_ > 0) {
            volume_update_counter_ -= chunk;
            if (volume_update_counter_ == 0) {
                const int phase = volume_update_which_ & 1;
                const int side = ((volume_update_which_ >> 1) & 1) ^ 1;
                const int channel_index = volume_update_which_ >> 2;

                if (phase == 0) {
                    if (channel_index < kChannelCount) {
                        volume_update_latch_ =
                            attenuation(channel_index, side);
                    }
                } else {
                    if (channel_index < kChannelCount) {
                        channels_[static_cast<std::size_t>(channel_index)]
                            .vl[static_cast<std::size_t>(side)] =
                            volume_update_latch_;
                    }
                }
                volume_update_which_ = (volume_update_which_ + 1) & 0x1F;

                if (volume_update_which_ != 0) {
                    volume_update_counter_ = phase != 0 ? 1 : 255;
                } else if (volume_update_pending_) {
                    volume_update_counter_ = phase != 0 ? 1 : 255;
                    volume_update_pending_ = false;
                }
            }
        }

        lastts_ = running_timestamp;
    }
}

void PcePsg::reset_timestamp(const std::int32_t base) noexcept
{
    lastts_ = base;

    for (auto& channel : channels_) {
        channel.lastts = base;
    }
}

void PcePsg::power(const std::int32_t timestamp) noexcept
{
    if (timestamp != lastts_) {
        update(timestamp);
    }

    // Deliberately not a wholesale clear: the timestamps and the previous
    // sample latches survive a reset because they describe where the output
    // buffers already are, not what the device is playing.
    select_ = 0U;
    global_balance_ = 0U;
    lfo_frequency_ = 0U;
    lfo_control_ = 0U;

    for (int index = 0; index < kChannelCount; ++index) {
        Channel& channel = channels_[static_cast<std::size_t>(index)];

        channel.frequency = 0U;
        channel.control = 0x00U;
        channel.balance = 0U;
        channel.waveform.fill(0U);
        channel.sample_accumulator = 0;

        channel.waveform_index = 0U;
        channel.dda = 0x00U;
        channel.noisectrl = 0x00U;

        channel.vl[0] = 0x1F;
        channel.vl[1] = 0x1F;

        recalc_frequency_period(index);
        recalc_output_mode(index);

        channel.counter =
            static_cast<std::int32_t>(channel.frequency_period);

        if (index >= 4) {
            recalc_noise_period(index);
        }
        channel.noisecount = 1;
        channel.lfsr = 1U;
    }

    volume_update_pending_ = false;
    volume_update_counter_ = 0;
    volume_update_which_ = 0;
}

PcePsg::State PcePsg::capture_state() const noexcept
{
    State state;

    for (std::size_t index = 0; index < channels_.size(); ++index) {
        const Channel& channel = channels_[index];
        ChannelState& saved = state.channels[index];

        saved.counter = channel.counter;
        saved.frequency = channel.frequency;
        saved.control = channel.control;
        saved.balance = channel.balance;
        saved.waveform = channel.waveform;
        saved.waveform_index = channel.waveform_index;
        saved.dda = channel.dda;
        saved.noisectrl = channel.noisectrl;
        saved.noisecount = channel.noisecount;
        saved.lfsr = channel.lfsr;
        saved.vl = channel.vl;
    }

    state.select = select_;
    state.global_balance = global_balance_;
    state.lfo_frequency = lfo_frequency_;
    state.lfo_control = lfo_control_;
    state.volume_update_counter = volume_update_counter_;
    state.volume_update_which = volume_update_which_;
    state.volume_update_latch = volume_update_latch_;
    state.volume_update_pending = volume_update_pending_;

    return state;
}

void PcePsg::apply_state(const State& state) noexcept
{
    select_ = state.select;
    global_balance_ = state.global_balance;
    lfo_frequency_ = state.lfo_frequency;
    lfo_control_ = state.lfo_control;
    volume_update_counter_ = state.volume_update_counter;
    volume_update_which_ = state.volume_update_which & 0x1F;
    volume_update_latch_ = state.volume_update_latch;
    volume_update_pending_ = state.volume_update_pending;

    for (std::size_t index = 0; index < channels_.size(); ++index) {
        Channel& channel = channels_[index];
        const ChannelState& saved = state.channels[index];

        channel.counter = saved.counter;
        channel.frequency = static_cast<std::uint16_t>(saved.frequency
                                                       & 0x0FFFU);
        channel.control = saved.control;
        channel.balance = saved.balance;
        channel.waveform = saved.waveform;
        channel.waveform_index =
            static_cast<std::uint8_t>(saved.waveform_index & 0x1FU);
        channel.dda = static_cast<std::uint8_t>(saved.dda & 0x1FU);
        channel.noisectrl = saved.noisectrl;
        channel.noisecount = saved.noisecount;
        channel.lfsr = saved.lfsr;
        channel.vl = saved.vl;
    }

    // An LFSR of zero is a stuck state the hardware cannot reach; a saved one
    // is a corrupt state, and the noise channels get their seed back.
    if (channels_[4].lfsr == 0U) {
        channels_[4].lfsr = 1U;
    }
    if (channels_[5].lfsr == 0U) {
        channels_[5].lfsr = 1U;
    }

    for (int index = 0; index < kChannelCount; ++index) {
        Channel& channel = channels_[static_cast<std::size_t>(index)];

        channel.sample_accumulator = 0;
        for (auto& entry : channel.waveform) {
            entry = static_cast<std::uint8_t>(entry & 0x1FU);
            channel.sample_accumulator += static_cast<std::int32_t>(entry);
        }

        channel.vl[0] &= 0x1F;
        channel.vl[1] &= 0x1F;

        if (channel.noisecount <= 0 && index >= 4) {
            channel.noisecount = 1;
        }
        if (channel.counter <= 0) {
            channel.counter = 1;
        }

        if (index >= 4) {
            recalc_noise_period(index);
        }
        recalc_frequency_period(index);
        recalc_output_mode(index);
    }
}

}  // namespace devils_crush::audio
