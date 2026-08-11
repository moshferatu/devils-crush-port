// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstdint>

namespace devils_crush::audio {

// The HuC6280's internal programmable sound generator: six channels, each a
// 32-entry five-bit waveform table clocked by a 12-bit period, with direct
// digital audio on any channel, a low-frequency oscillator driving channel 0
// from channel 1, and an 18-bit LFSR noise source on channels 4 and 5.
//
// This is a device model, not a transcript. It owns no CPU state and reaches
// nothing outside itself: the machine drives it with the ten register writes
// at $0800-$0809 and with time, and it drives two band-limited delta buffers.
// Sample rate conversion is deliberately not here -- that belongs to whatever
// host is playing the result, and is the one part of the emulator's audio
// path that is not the PC Engine.
//
// Time is measured in PSG clocks, which are master clocks divided by three.
// One output buffer position spans four of them, so the buffers advance at
// the master clock divided by twelve, 1.7898 MHz.
class PcePsg {
public:
    enum class Revision : std::uint8_t {
        // Waveform amplitudes run 0..62 and the accumulating output mode
        // sums the raw waveform. The plain PC Engine and the CoreGrafx.
        HuC6280,
        // Amplitudes are centred, running -31..31, and the accumulating
        // output mode subtracts the midpoint. The SuperGrafx.
        HuC6280A,
    };

    static constexpr int kChannelCount = 6;
    static constexpr int kWaveformLength = 32;

    // Every field Beetle's PSG carries across a savestate, in its order.
    // Deliberately excludes the timestamps and the previous-sample latches,
    // which upstream also omits: the frame boundary rebases the first and
    // the second only affects the delta of the next edge.
    struct ChannelState {
        std::int32_t counter = 0;
        std::uint16_t frequency = 0;
        std::uint8_t control = 0;
        std::uint8_t balance = 0;
        std::array<std::uint8_t, kWaveformLength> waveform{};
        std::uint8_t waveform_index = 0;
        std::uint8_t dda = 0;
        std::uint8_t noisectrl = 0;
        std::int32_t noisecount = 0;
        std::uint32_t lfsr = 0;
        std::array<std::int32_t, 2> vl{};
    };

    // How often each output path has run, per channel where that is the
    // interesting number. A device is proved against a route census, and a
    // route exercises only what the game asks for: Devil's Crush never enables
    // the LFO, uses the noise source on one channel, and reaches the
    // accumulating mode a handful of times a minute. Counting is how that gap
    // is measured rather than guessed at, and what the differential test
    // asserts it has closed. Free relative to the emission it counts.
    struct PathCoverage {
        std::uint64_t total = 0;
        std::array<std::uint64_t, kChannelCount> by_channel{};
    };

    struct Coverage {
        PathCoverage silent;
        PathCoverage waveform;
        PathCoverage noise;
        PathCoverage accumulated;
        // Updates that ran with the LFO on, and modulator runs entered from
        // inside channel 0's waveform loop.
        std::uint64_t lfo_updates = 0;
        std::uint64_t lfo_recursions = 0;
        // Waveform steps taken by the whole-periods-at-once path rather than
        // one at a time.
        std::uint64_t accumulate_fast_paths = 0;
        std::uint64_t lfsr_clocks = 0;
        // Writes to $0806 on an enabled channel, which refresh the sample
        // latch whether or not the channel is in direct digital audio mode.
        std::uint64_t sample_latch_writes = 0;
    };

    struct State {
        std::array<ChannelState, kChannelCount> channels{};
        std::uint8_t select = 0;
        std::uint8_t global_balance = 0;
        std::uint8_t lfo_frequency = 0;
        std::uint8_t lfo_control = 0;
        std::int32_t volume_update_counter = 0;
        std::int32_t volume_update_which = 0;
        std::int32_t volume_update_latch = 0;
        bool volume_update_pending = false;
    };

    // The two buffers receive band-limited deltas and are owned by the
    // caller. Each must have room for 65,536 positions plus 32 more, because
    // an edge lands a seven-tap impulse that may dangle past the last
    // position of the frame.
    PcePsg(
        std::int32_t* left_buffer,
        std::int32_t* right_buffer,
        Revision revision) noexcept;

    void power(std::int32_t timestamp) noexcept;
    void write(
        std::int32_t timestamp,
        std::uint8_t address,
        std::uint8_t value) noexcept;
    void update(std::int32_t timestamp) noexcept;

    // Rebases every timestamp the model holds. The frame boundary calls this
    // with the master-cycle remainder that did not fit in the frame, divided
    // by three, so the next frame's timestamps stay relative to the same
    // twelve-cycle output position.
    void reset_timestamp(std::int32_t base) noexcept;

    // Rebinds the output buffers and the chip revision together, because the
    // machine settles both at the same moment and a model that guesses the
    // revision separately is silently a different device.
    void configure(
        std::int32_t* left_buffer,
        std::int32_t* right_buffer,
        Revision revision) noexcept;

    // Counted from construction, and deliberately not cleared by power or by
    // a loaded state: the question it answers is what the model has ever run,
    // not what it ran since the machine last reset.
    [[nodiscard]] const Coverage& coverage() const noexcept
    {
        return coverage_;
    }

    [[nodiscard]] State capture_state() const noexcept;
    // Applies a captured state and repairs the values a savestate is allowed
    // to carry out of range, exactly as the emulator does on load.
    void apply_state(const State& state) noexcept;

private:
    enum class OutputMode : std::uint8_t {
        Off,
        Normal,
        Noise,
        Accumulate,
    };

    struct Channel {
        std::array<std::uint8_t, kWaveformLength> waveform{};
        std::uint8_t waveform_index = 0;
        std::uint8_t dda = 0;
        std::uint8_t control = 0;
        std::uint8_t noisectrl = 0;
        std::array<std::int32_t, 2> vl{};
        std::int32_t counter = 0;
        OutputMode output_mode = OutputMode::Off;
        std::uint32_t frequency_period = 0;
        std::uint32_t noise_period = 0;
        std::int32_t noisecount = 0;
        std::uint32_t lfsr = 0;
        std::int32_t sample_accumulator = 0;
        std::array<std::int32_t, 2> previous_sample{};
        std::int32_t lastts = 0;
        std::uint16_t frequency = 0;
        std::uint8_t balance = 0;
    };

    void build_volume_tables() noexcept;

    void emit(
        std::int32_t timestamp,
        Channel& channel,
        std::int32_t left_sample,
        std::int32_t right_sample) noexcept;
    void update_output(std::int32_t timestamp, Channel& channel) noexcept;

    void recalc_output_mode(int channel_index) noexcept;
    void recalc_frequency_period(int channel_index) noexcept;
    void recalc_noise_period(int channel_index) noexcept;

    [[nodiscard]] std::int32_t attenuation(
        int channel_index,
        int side) const noexcept;

    void run_channel(
        int channel_index,
        std::int32_t timestamp,
        bool lfo_on) noexcept;
    void run_all_channels(std::int32_t timestamp, bool lfo_on) noexcept;

    std::array<std::int32_t*, 2> buffers_{};
    std::array<Channel, kChannelCount> channels_{};

    std::uint8_t select_ = 0;
    std::uint8_t global_balance_ = 0;
    std::uint8_t lfo_frequency_ = 0;
    std::uint8_t lfo_control_ = 0;

    std::int32_t volume_update_counter_ = 0;
    std::int32_t volume_update_which_ = 0;
    std::int32_t volume_update_latch_ = 0;
    bool volume_update_pending_ = false;

    std::int32_t lastts_ = 0;
    Revision revision_ = Revision::HuC6280;

    Coverage coverage_{};

    std::array<std::int32_t, 32> volume_only_table_{};
    std::array<std::array<std::int32_t, 32>, 32> volume_table_{};
};

}  // namespace devils_crush::audio
