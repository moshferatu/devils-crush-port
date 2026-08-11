// SPDX-License-Identifier: GPL-3.0-or-later

// The native sound generator against the emulator's own, driven directly.
//
// The census route proves the model on what the game asks the chip to do,
// which is most of it and not all of it: Devil's Crush never enables the
// low-frequency oscillator, uses the noise source on one channel, and reaches
// the accumulating output mode seven times in a minute of play. No recording
// of the game can settle those paths, so they are settled here instead --
// both devices receive the same register writes at the same clock, and both
// their output buffers and their whole internal state are compared.
//
// This is a differential test, so it lives exactly as long as the emulator
// does. What it leaves behind when the emulator goes is the evidence that the
// model was right while there was still something to be right against.

#include "devils_crush/audio/pce_psg.hpp"

#include <dc_beetle_reference_psg.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using devils_crush::audio::PcePsg;

int failures = 0;

void check(const bool condition, const std::string_view description)
{
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

// 65,536 output positions plus the 32 the emulator keeps for an impulse whose
// tail dangles past the last one.
constexpr std::size_t kBufferPositions = 65536U + 32U;

[[nodiscard]] int reference_revision(const PcePsg::Revision revision) noexcept
{
    return revision == PcePsg::Revision::HuC6280
        ? DC_BEETLE_PSG_REVISION_HUC6280
        : DC_BEETLE_PSG_REVISION_HUC6280A;
}

[[nodiscard]] std::string_view revision_name(
    const PcePsg::Revision revision) noexcept
{
    return revision == PcePsg::Revision::HuC6280 ? "HuC6280" : "HuC6280A";
}

// A deterministic generator, so a failing seed names a reproducible stream.
class Random {
public:
    explicit Random(const std::uint64_t seed) noexcept
        : state_(seed + 0x9E3779B97F4A7C15ULL)
    {
    }

    [[nodiscard]] std::uint32_t next() noexcept
    {
        std::uint64_t value = (state_ += 0x9E3779B97F4A7C15ULL);
        value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
        value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
        return static_cast<std::uint32_t>((value ^ (value >> 31)) >> 16U);
    }

    // Inclusive on both ends.
    [[nodiscard]] std::uint32_t between(
        const std::uint32_t low,
        const std::uint32_t high) noexcept
    {
        return low + (next() % ((high - low) + 1U));
    }

    [[nodiscard]] std::uint8_t byte() noexcept
    {
        return static_cast<std::uint8_t>(next() & 0xFFU);
    }

private:
    std::uint64_t state_;
};

// Both devices, the same four buffers' worth of storage, and one call each.
class Devices {
public:
    explicit Devices(const PcePsg::Revision revision)
        : native_left_(kBufferPositions, 0)
        , native_right_(kBufferPositions, 0)
        , reference_left_(kBufferPositions, 0)
        , reference_right_(kBufferPositions, 0)
        , native_(native_left_.data(), native_right_.data(), revision)
        , reference_(dc_beetle_reference_psg_create(
              reference_left_.data(),
              reference_right_.data(),
              reference_revision(revision)))
        , revision_(revision)
    {
    }

    Devices(const Devices&) = delete;
    Devices& operator=(const Devices&) = delete;

    ~Devices()
    {
        dc_beetle_reference_psg_destroy(reference_);
    }

    void write(
        const std::int32_t timestamp,
        const std::uint8_t address,
        const std::uint8_t value) noexcept
    {
        native_.write(timestamp, address, value);
        dc_beetle_reference_psg_write(reference_, timestamp, address, value);
    }

    void update(const std::int32_t timestamp) noexcept
    {
        native_.update(timestamp);
        dc_beetle_reference_psg_update(reference_, timestamp);
    }

    void reset_timestamp(const std::int32_t base) noexcept
    {
        native_.reset_timestamp(base);
        dc_beetle_reference_psg_reset_timestamp(reference_, base);
    }

    void power(const std::int32_t timestamp) noexcept
    {
        native_.power(timestamp);
        dc_beetle_reference_psg_power(reference_, timestamp);
    }

    // Selects a channel and loads a full 32-entry waveform into it, which is
    // what the game's sequencer does when a voice changes instrument.
    void load_waveform(
        const std::int32_t timestamp,
        const std::uint8_t channel,
        const std::uint8_t seed) noexcept
    {
        write(timestamp, 0x00U, channel);
        // Clearing the enable and DDA bits is what makes $0806 walk the table.
        write(timestamp, 0x04U, 0x00U);
        for (std::uint8_t entry = 0; entry < 32U; ++entry) {
            write(
                timestamp,
                0x06U,
                static_cast<std::uint8_t>((entry * 5U + seed) & 0x1FU));
        }
    }

    [[nodiscard]] const PcePsg& native() const noexcept
    {
        return native_;
    }

    [[nodiscard]] bool buffers_agree(const std::string_view label) const
    {
        return compare_side(label, "left", native_left_, reference_left_)
            && compare_side(label, "right", native_right_, reference_right_);
    }

    [[nodiscard]] bool buffers_carry_signal() const noexcept
    {
        for (const auto sample : native_left_) {
            if (sample != 0) {
                return true;
            }
        }
        for (const auto sample : native_right_) {
            if (sample != 0) {
                return true;
            }
        }
        return false;
    }

    // The output buffers only show state that has already reached them. This
    // compares the state itself, so a divergence is caught at the edge it
    // happens on rather than at the next one that emits.
    [[nodiscard]] bool state_agrees(const std::string_view label) const
    {
        const PcePsg::State mine = native_.capture_state();
        dc_beetle_psg_state theirs{};
        dc_beetle_reference_psg_export_state(reference_, &theirs);

        bool agreed = true;
        const auto compare = [&](
                                 const std::int64_t left,
                                 const std::int64_t right,
                                 const std::string_view field,
                                 const int channel) {
            if (left == right) {
                return;
            }
            agreed = false;
            std::cerr << "  " << label << " [" << revision_name(revision_)
                      << "] state " << field;
            if (channel >= 0) {
                std::cerr << " on channel " << channel;
            }
            std::cerr << ": native " << left << ", emulator " << right << '\n';
        };

        for (std::size_t index = 0; index < mine.channels.size(); ++index) {
            const PcePsg::ChannelState& a = mine.channels[index];
            const dc_beetle_psg_channel_state& b = theirs.channels[index];
            const auto channel = static_cast<int>(index);

            compare(a.counter, b.counter, "counter", channel);
            compare(a.frequency, b.frequency, "frequency", channel);
            compare(a.control, b.control, "control", channel);
            compare(a.balance, b.balance, "balance", channel);
            compare(a.waveform_index, b.waveform_index, "waveform index",
                    channel);
            compare(a.dda, b.dda, "sample latch", channel);
            compare(a.noisectrl, b.noisectrl, "noise control", channel);
            compare(a.noisecount, b.noisecount, "noise counter", channel);
            compare(a.lfsr, b.lfsr, "noise shift register", channel);
            compare(a.vl[0], b.vl[0], "left attenuation", channel);
            compare(a.vl[1], b.vl[1], "right attenuation", channel);
            for (std::size_t entry = 0; entry < a.waveform.size(); ++entry) {
                if (a.waveform[entry] != b.waveform[entry]) {
                    compare(
                        a.waveform[entry],
                        b.waveform[entry],
                        "waveform entry",
                        channel);
                    break;
                }
            }
        }

        compare(mine.select, theirs.select, "select", -1);
        compare(mine.global_balance, theirs.global_balance, "global balance",
                -1);
        compare(mine.lfo_frequency, theirs.lfo_frequency, "LFO period", -1);
        compare(mine.lfo_control, theirs.lfo_control, "LFO control", -1);
        compare(mine.volume_update_counter, theirs.volume_update_counter,
                "volume step counter", -1);
        compare(mine.volume_update_which, theirs.volume_update_which,
                "volume step", -1);
        compare(mine.volume_update_latch, theirs.volume_update_latch,
                "volume latch", -1);
        compare(
            mine.volume_update_pending ? 1 : 0,
            theirs.volume_update_pending != 0 ? 1 : 0,
            "volume update pending",
            -1);

        return agreed;
    }

private:
    [[nodiscard]] bool compare_side(
        const std::string_view label,
        const std::string_view side,
        const std::vector<std::int32_t>& mine,
        const std::vector<std::int32_t>& theirs) const
    {
        for (std::size_t position = 0; position < mine.size(); ++position) {
            if (mine[position] == theirs[position]) {
                continue;
            }
            std::cerr << "  " << label << " [" << revision_name(revision_)
                      << "] " << side << " position " << position
                      << ": native " << mine[position] << ", emulator "
                      << theirs[position] << '\n';
            return false;
        }
        return true;
    }

    std::vector<std::int32_t> native_left_;
    std::vector<std::int32_t> native_right_;
    std::vector<std::int32_t> reference_left_;
    std::vector<std::int32_t> reference_right_;
    PcePsg native_;
    dc_beetle_reference_psg* reference_ = nullptr;
    PcePsg::Revision revision_ = PcePsg::Revision::HuC6280;
};

void accumulate(PcePsg::PathCoverage& total, const PcePsg::PathCoverage& part)
{
    total.total += part.total;
    for (std::size_t index = 0; index < total.by_channel.size(); ++index) {
        total.by_channel[index] += part.by_channel[index];
    }
}

void accumulate(PcePsg::Coverage& total, const PcePsg::Coverage& part)
{
    accumulate(total.silent, part.silent);
    accumulate(total.waveform, part.waveform);
    accumulate(total.noise, part.noise);
    accumulate(total.accumulated, part.accumulated);
    total.lfo_updates += part.lfo_updates;
    total.lfo_recursions += part.lfo_recursions;
    total.accumulate_fast_paths += part.accumulate_fast_paths;
    total.lfsr_clocks += part.lfsr_clocks;
    total.sample_latch_writes += part.sample_latch_writes;
}

PcePsg::Coverage total_coverage;

// Runs a scenario, compares both devices, and folds what it exercised into the
// running coverage total.
void run(
    const std::string_view label,
    const PcePsg::Revision revision,
    void (*scenario)(Devices&))
{
    Devices devices(revision);
    scenario(devices);

    const std::string name =
        std::string(label) + " (" + std::string(revision_name(revision)) + ")";

    check(devices.buffers_agree(name), name + ": output buffers agree");
    check(devices.state_agrees(name), name + ": device state agrees");
    // A scenario that produced silence would agree with anything, including a
    // model that never ran. This is the same liveness question the census
    // could not answer on its own.
    check(devices.buffers_carry_signal(), name + ": produced sound");

    accumulate(total_coverage, devices.native().coverage());
}

// Six voices playing waveforms, which is what the game does almost all of the
// time and the one path the census route already covers well.
void scenario_waveform(Devices& devices)
{
    devices.write(0, 0x01U, 0xFFU);
    for (std::uint8_t channel = 0; channel < 6U; ++channel) {
        devices.load_waveform(0, channel, static_cast<std::uint8_t>(channel));
        devices.write(0, 0x02U, static_cast<std::uint8_t>(0x40U + channel));
        devices.write(0, 0x03U, 0x01U);
        devices.write(0, 0x05U, 0xFFU);
        devices.write(0, 0x04U, static_cast<std::uint8_t>(0x80U | 0x1AU));
    }

    for (std::int32_t timestamp = 1000; timestamp <= 200000;
         timestamp += 1000) {
        devices.update(timestamp);
    }
}

// Direct digital audio: the sample comes from the port, not the table, and the
// waveform counter stops.
void scenario_direct_digital_audio(Devices& devices)
{
    devices.write(0, 0x01U, 0xFFU);
    for (std::uint8_t channel = 0; channel < 6U; ++channel) {
        devices.write(0, 0x00U, channel);
        devices.write(0, 0x05U, 0xF0U);
        devices.write(0, 0x04U, 0xDFU);  // Enable, DDA, full volume.
    }

    std::int32_t timestamp = 0;
    for (int step = 0; step < 600; ++step) {
        const auto channel = static_cast<std::uint8_t>(step % 6);
        timestamp += 37;
        devices.write(timestamp, 0x00U, channel);
        devices.write(
            timestamp,
            0x06U,
            static_cast<std::uint8_t>((step * 7) & 0x1FU));
    }
    devices.update(timestamp + 5000);
}

// Noise on channels 4 and 5, across the whole range of noise periods. The
// route only ever reaches this on channel 5.
void scenario_noise(Devices& devices)
{
    devices.write(0, 0x01U, 0xFFU);
    for (std::uint8_t channel = 4U; channel < 6U; ++channel) {
        devices.load_waveform(0, channel, 0x03U);
        devices.write(0, 0x02U, 0x80U);
        devices.write(0, 0x03U, 0x00U);
        devices.write(0, 0x05U, 0xFFU);
        devices.write(0, 0x04U, 0x9FU);
    }

    std::int32_t timestamp = 0;
    for (std::uint8_t period = 0; period < 32U; ++period) {
        for (std::uint8_t channel = 4U; channel < 6U; ++channel) {
            devices.write(timestamp, 0x00U, channel);
            devices.write(
                timestamp,
                0x07U,
                static_cast<std::uint8_t>(0x80U | period));
        }
        timestamp += 2000;
        devices.update(timestamp);

        // Off again, so the branch that clocks the shift register without
        // emitting is exercised too.
        for (std::uint8_t channel = 4U; channel < 6U; ++channel) {
            devices.write(timestamp, 0x00U, channel);
            devices.write(timestamp, 0x07U, period);
        }
        timestamp += 500;
        devices.update(timestamp);
    }
}

// Periods at or below the cutoff, where the channel runs faster than the
// output buffer resolves and the model sums the whole waveform instead of
// stepping it. The route reaches this seven times.
void scenario_accumulating(Devices& devices)
{
    devices.write(0, 0x01U, 0xFFU);
    for (std::uint8_t channel = 0; channel < 6U; ++channel) {
        devices.load_waveform(0, channel, static_cast<std::uint8_t>(channel));
        devices.write(0, 0x05U, 0xFFU);
        devices.write(0, 0x04U, 0x9FU);
    }

    std::int32_t timestamp = 0;
    // 1, 2 and 3 give periods of 2, 4 and 6, all at or under the cutoff of 7;
    // 4 gives 8, which is the first period that steps one edge at a time.
    for (std::uint8_t period = 1U; period <= 4U; ++period) {
        for (std::uint8_t channel = 0; channel < 6U; ++channel) {
            devices.write(timestamp, 0x00U, channel);
            devices.write(timestamp, 0x03U, 0x00U);
            devices.write(timestamp, 0x02U, period);
        }
        for (int step = 0; step < 20; ++step) {
            timestamp += 1000;
            devices.update(timestamp);
        }
    }
}

// The low-frequency oscillator, which the game never turns on: channel 1's
// current sample bends channel 0's period, at one of three depths, and channel
// 1 is run from inside channel 0's loop to keep the two in step.
void scenario_low_frequency_oscillator(Devices& devices)
{
    devices.write(0, 0x01U, 0xFFU);
    for (std::uint8_t channel = 0; channel < 2U; ++channel) {
        devices.load_waveform(0, channel, static_cast<std::uint8_t>(channel));
        devices.write(0, 0x02U, 0x30U);
        devices.write(0, 0x03U, 0x00U);
        devices.write(0, 0x05U, 0xFFU);
        devices.write(0, 0x04U, 0x9FU);
    }

    std::int32_t timestamp = 0;
    for (std::uint8_t control = 1U; control <= 3U; ++control) {
        for (std::uint8_t period = 1U; period <= 3U; ++period) {
            devices.write(timestamp, 0x08U, period);
            devices.write(timestamp, 0x09U, control);
            for (int step = 0; step < 12; ++step) {
                timestamp += 1500;
                devices.update(timestamp);
            }

            // The trigger bit restarts the modulator and halts its counter,
            // which turns the oscillator back off from the mixer's side.
            devices.write(timestamp, 0x09U, static_cast<std::uint8_t>(
                                                0x80U | control));
            timestamp += 3000;
            devices.update(timestamp);
            devices.write(timestamp, 0x09U, 0x00U);
            timestamp += 3000;
            devices.update(timestamp);
        }
    }

    // And once more with the modulator disabled while the oscillator is on,
    // which is the other way the mixer turns it off mid-run.
    devices.write(timestamp, 0x08U, 0x02U);
    devices.write(timestamp, 0x09U, 0x02U);
    timestamp += 2000;
    devices.update(timestamp);
    devices.write(timestamp, 0x00U, 0x01U);
    devices.write(timestamp, 0x04U, 0x1FU);
    timestamp += 2000;
    devices.update(timestamp);
}

// The volume state machine walks 32 latch steps, 255 clocks apart, and any
// balance write while it is walking extends the walk. Short updates land
// inside those steps.
void scenario_volume_walk(Devices& devices)
{
    for (std::uint8_t channel = 0; channel < 6U; ++channel) {
        devices.load_waveform(0, channel, 0x11U);
        devices.write(0, 0x02U, 0x20U);
        devices.write(0, 0x03U, 0x00U);
        devices.write(0, 0x04U, 0x9FU);
    }

    Random random(0x5A17);
    std::int32_t timestamp = 0;
    for (int step = 0; step < 400; ++step) {
        timestamp += static_cast<std::int32_t>(random.between(1U, 400U));
        switch (step % 3) {
        case 0:
            devices.write(
                timestamp,
                0x01U,
                static_cast<std::uint8_t>(random.between(0U, 0xFFU)));
            break;
        case 1:
            devices.write(
                timestamp,
                0x00U,
                static_cast<std::uint8_t>(random.between(0U, 5U)));
            devices.write(
                timestamp,
                0x05U,
                static_cast<std::uint8_t>(random.between(0U, 0xFFU)));
            break;
        default:
            devices.write(
                timestamp,
                0x04U,
                static_cast<std::uint8_t>(0x80U | random.between(0U, 0x1FU)));
            break;
        }
    }
    devices.update(timestamp + 20000);
}

// Power and the frame-boundary rebase, in the middle of a run rather than at
// the start of one.
void scenario_power_and_rebase(Devices& devices)
{
    devices.write(0, 0x01U, 0xFFU);
    for (std::uint8_t channel = 0; channel < 6U; ++channel) {
        devices.load_waveform(0, channel, 0x07U);
        devices.write(0, 0x02U, static_cast<std::uint8_t>(0x11U + channel));
        devices.write(0, 0x03U, 0x00U);
        devices.write(0, 0x05U, 0xFFU);
        devices.write(0, 0x04U, 0x9FU);
    }

    std::int32_t timestamp = 0;
    for (int frame = 0; frame < 8; ++frame) {
        for (int step = 0; step < 10; ++step) {
            timestamp += 1200;
            devices.update(timestamp);
        }
        devices.reset_timestamp(timestamp % 4);
        timestamp = timestamp % 4;
    }

    devices.power(timestamp);
    devices.write(timestamp, 0x01U, 0xFFU);
    devices.write(timestamp, 0x00U, 0x02U);
    devices.write(timestamp, 0x04U, 0x9FU);
    devices.write(timestamp, 0x05U, 0xFFU);
    devices.write(timestamp, 0x02U, 0x40U);
    devices.update(timestamp + 40000);
}

// Everything at once, from a fixed seed, including the selections past the end
// of the register file and the writes the chip ignores.
void run_random_scenario(Devices& devices, const std::uint64_t seed)
{
    Random random(seed);
    std::int32_t timestamp = 0;

    // Something audible to start from, so the stream is not mostly silence.
    devices.write(0, 0x01U, 0xFFU);
    for (std::uint8_t channel = 0; channel < 6U; ++channel) {
        devices.load_waveform(0, channel, static_cast<std::uint8_t>(channel));
        devices.write(0, 0x05U, 0xFFU);
        devices.write(0, 0x04U, 0x9FU);
    }

    for (int step = 0; step < 6000; ++step) {
        timestamp += static_cast<std::int32_t>(random.between(0U, 900U));

        const std::uint32_t roll = random.between(0U, 99U);
        if (roll < 6U) {
            devices.update(timestamp);
            continue;
        }
        if (roll < 8U) {
            // A frame boundary: rebase, and keep the sub-position remainder
            // the way the machine does.
            devices.update(timestamp);
            const std::int32_t base = timestamp % 4;
            devices.reset_timestamp(base);
            timestamp = base;
            continue;
        }

        std::uint8_t address = static_cast<std::uint8_t>(
            random.between(0x00U, 0x09U));
        std::uint8_t value = random.byte();

        // Bias towards the values that keep channels running and towards the
        // oscillator, which an unbiased stream would leave on for a moment at
        // a time.
        switch (address) {
        case 0x00U:
            value = static_cast<std::uint8_t>(random.between(0U, 7U));
            break;
        case 0x03U:
            value = static_cast<std::uint8_t>(random.between(0U, 0x0FU));
            break;
        case 0x04U:
            value = static_cast<std::uint8_t>(
                (random.between(0U, 3U) << 6U) | random.between(0U, 0x1FU));
            break;
        case 0x08U:
            value = static_cast<std::uint8_t>(random.between(0U, 4U));
            break;
        case 0x09U:
            value = static_cast<std::uint8_t>(random.between(0U, 3U));
            if (random.between(0U, 7U) == 0U) {
                value = static_cast<std::uint8_t>(value | 0x80U);
            }
            break;
        default:
            break;
        }

        devices.write(timestamp, address, value);
    }

    devices.update(timestamp + 1000);
}

void scenario_random_a(Devices& devices)
{
    run_random_scenario(devices, 0x0000000000000001ULL);
}

void scenario_random_b(Devices& devices)
{
    run_random_scenario(devices, 0x00000000DEADBEEFULL);
}

void scenario_random_c(Devices& devices)
{
    run_random_scenario(devices, 0x123456789ABCDEF0ULL);
}

void report_coverage()
{
    const PcePsg::Coverage& coverage = total_coverage;
    const auto line = [](const std::string_view name,
                         const PcePsg::PathCoverage& path) {
        std::cout << "  " << name << ": " << path.total << " (";
        for (std::size_t channel = 0; channel < path.by_channel.size();
             ++channel) {
            std::cout << (channel != 0U ? ", " : "")
                      << path.by_channel[channel];
        }
        std::cout << ")\n";
    };

    std::cout << "Paths exercised, total and per channel:\n";
    line("silent    ", coverage.silent);
    line("waveform  ", coverage.waveform);
    line("noise     ", coverage.noise);
    line("accumulate", coverage.accumulated);
    std::cout << "  LFO updates: " << coverage.lfo_updates
              << ", modulator recursions: " << coverage.lfo_recursions
              << "\n  whole-period steps: " << coverage.accumulate_fast_paths
              << ", shift-register clocks: " << coverage.lfsr_clocks
              << ", sample-latch writes: " << coverage.sample_latch_writes
              << '\n';
}

// Agreement is only worth what it covers, so the paths the census route cannot
// reach are asserted to have run here.
void check_coverage()
{
    const PcePsg::Coverage& coverage = total_coverage;

    for (std::size_t channel = 0; channel < 6U; ++channel) {
        const std::string suffix =
            " on channel " + std::to_string(channel) + " was exercised";
        check(coverage.silent.by_channel[channel] > 0U, "silence" + suffix);
        check(
            coverage.waveform.by_channel[channel] > 0U,
            "the waveform path" + suffix);
        check(
            coverage.accumulated.by_channel[channel] > 0U,
            "the accumulating path" + suffix);
    }

    // Only channels 4 and 5 have a noise source, and the route only ever uses
    // channel 5's.
    check(
        coverage.noise.by_channel[4] > 0U,
        "the noise path on channel 4 was exercised");
    check(
        coverage.noise.by_channel[5] > 0U,
        "the noise path on channel 5 was exercised");

    check(coverage.lfo_updates > 0U, "the LFO ran");
    check(
        coverage.lfo_recursions > 0U,
        "the LFO modulator ran from inside channel 0");
    check(
        coverage.accumulate_fast_paths > 0U,
        "the whole-period waveform step ran");
    check(coverage.lfsr_clocks > 0U, "the noise shift register was clocked");
    check(coverage.sample_latch_writes > 0U, "the sample latch was written");
}

struct Scenario {
    std::string_view label;
    void (*run)(Devices&);
};

constexpr std::array<Scenario, 10> kScenarios{{
    {"waveform playback", &scenario_waveform},
    {"direct digital audio", &scenario_direct_digital_audio},
    {"noise", &scenario_noise},
    {"accumulating periods", &scenario_accumulating},
    {"low-frequency oscillator", &scenario_low_frequency_oscillator},
    {"volume walk", &scenario_volume_walk},
    {"power and rebase", &scenario_power_and_rebase},
    {"random stream A", &scenario_random_a},
    {"random stream B", &scenario_random_b},
    {"random stream C", &scenario_random_c},
}};

}  // namespace

int main()
{
    // Both revisions, because they are different devices in the two places it
    // matters -- whether waveform amplitudes are centred, and what the
    // accumulating mode subtracts -- and the host has pinned the wrong one
    // before.
    for (const auto revision :
         {PcePsg::Revision::HuC6280, PcePsg::Revision::HuC6280A}) {
        for (const auto& scenario : kScenarios) {
            run(scenario.label, revision, scenario.run);
        }
    }

    report_coverage();
    check_coverage();

    if (failures == 0) {
        std::cout << "All PSG device tests passed.\n";
        return EXIT_SUCCESS;
    }

    std::cerr << failures << " test(s) failed.\n";
    return EXIT_FAILURE;
}
