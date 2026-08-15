// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/audio/music_voice_overlay.hpp"

namespace devils_crush::audio {
namespace {

constexpr std::uint16_t kWorkRamBase = 0x2000U;
constexpr std::uint8_t kMusicSlotCount = 6U;
constexpr std::uint8_t kEffectSlotFirst = 6U;
constexpr std::uint8_t kLogicalSlotCount = 9U;
constexpr std::uint8_t kSequencerInProgress = 0x04U;

[[nodiscard]] constexpr bool valid_channel(const std::uint8_t channel)
{
    return channel < PcePsg::kChannelCount;
}

} // namespace

MusicVoiceOverlay::MusicVoiceOverlay(
    const std::span<const std::byte> waveform_table)
    : waveform_table_(waveform_table.begin(), waveform_table.end())
{
    loaded_waveforms_.fill(-1);
}

void MusicVoiceOverlay::configure(
    std::int32_t* const left_buffer,
    std::int32_t* const right_buffer,
    const PcePsg::Revision revision) noexcept
{
    psg_.configure(left_buffer, right_buffer, revision);
    psg_.set_output_channel_mask(0, 0U);
}

void MusicVoiceOverlay::power(const std::int32_t timestamp) noexcept
{
    psg_.power(timestamp);
    loaded_waveforms_.fill(-1);
    pending_sequencer_select_ = false;
    displaced_channel_mask_ = 0U;
    psg_.set_output_channel_mask(timestamp, 0U);
}

void MusicVoiceOverlay::update(const std::int32_t timestamp) noexcept
{
    psg_.update(timestamp);
}

void MusicVoiceOverlay::reset_timestamp(const std::int32_t base) noexcept
{
    psg_.reset_timestamp(base);
}

void MusicVoiceOverlay::invalidate(const std::int32_t timestamp) noexcept
{
    psg_.reset_timestamp(timestamp);
    power(timestamp);
}

std::uint8_t MusicVoiceOverlay::read(
    const std::span<const std::byte> work_ram,
    const std::uint16_t logical_address) const noexcept
{
    if (logical_address < kWorkRamBase) {
        return 0U;
    }
    const auto offset = static_cast<std::size_t>(
        logical_address - kWorkRamBase);
    return offset < work_ram.size()
        ? std::to_integer<std::uint8_t>(work_ram[offset])
        : 0U;
}

std::uint8_t MusicVoiceOverlay::effect_channel_mask(
    const std::span<const std::byte> work_ram) const noexcept
{
    std::array<bool, PcePsg::kChannelCount> occupied{};
    std::uint8_t result = 0U;

    const auto claim = [&occupied, &result](const std::uint8_t channel) {
        if (!valid_channel(channel) || occupied[channel]) {
            return false;
        }
        occupied[channel] = true;
        result = static_cast<std::uint8_t>(result | (1U << channel));
        return true;
    };

    // The cartridge walks slots 8 down to 6. This is the ownership part of
    // 45:49F9: an earlier effect wins, state $02 may additionally route noise
    // to its alternate channel, and a zero-period voice claims nothing.
    for (int slot = kLogicalSlotCount - 1;
         slot >= kEffectSlotFirst;
         --slot) {
        const auto index = static_cast<std::uint8_t>(slot);
        const auto primary = read(work_ram, 0x2E82U + index);
        if (!valid_channel(primary) || occupied[primary]) {
            continue;
        }

        const auto state = read(work_ram, 0x2E46U + index);
        if ((read(work_ram, 0x2EBEU + index) & 0x02U) != 0U) {
            // The DDA commit only takes even PSG channels whose active timer
            // stream belongs to this logical slot.
            if ((primary & 1U) == 0U
                && read(work_ram, 0x20F5U + primary) != 0U
                && read(work_ram, 0x2E05U + primary)
                    == read(work_ram, 0x2F0EU + index)) {
                static_cast<void>(claim(primary));
            }
            continue;
        }

        if ((read(work_ram, 0x2F22U + index)
             | read(work_ram, 0x2F2CU + index)) == 0U) {
            continue;
        }

        const auto special = static_cast<std::uint8_t>(state & 0x0AU);
        if (special != 0U) {
            if (special != 0x02U || primary >= 4U) {
                static_cast<void>(claim(primary));
                continue;
            }
            const auto alternate = read(work_ram, 0x2EAAU + index);
            static_cast<void>(claim(alternate));
        }
        if ((state & 0x01U) != 0U) {
            static_cast<void>(claim(primary));
        }
    }

    return result;
}

std::array<MusicVoiceOverlay::Voice, PcePsg::kChannelCount>
MusicVoiceOverlay::music_voices(
    const std::span<const std::byte> work_ram) const noexcept
{
    std::array<Voice, PcePsg::kChannelCount> voices{};
    std::array<bool, PcePsg::kChannelCount> occupied{};

    const auto commit = [this, work_ram, &voices, &occupied](
                            const std::uint8_t slot,
                            const std::uint8_t channel,
                            const bool noise_only) {
        if (!valid_channel(channel) || occupied[channel]) {
            return;
        }
        auto& voice = voices[channel];
        voice.active = true;
        voice.period_low = read(work_ram, 0x2F22U + slot);
        voice.period_high = read(work_ram, 0x2F2CU + slot);
        voice.volume = read(work_ram, 0x2E37U);
        voice.balance = read(work_ram, 0x2FAEU + slot);
        if (noise_only) {
            voice.leave_waveform = true;
            voice.noise = static_cast<std::uint8_t>(
                (read(work_ram, 0x2F68U + slot) & 0x1FU) | 0x80U);
        } else {
            const auto state = read(work_ram, 0x2E46U + slot);
            voice.waveform = (state & 0x80U) != 0U
                ? read(work_ram, 0x2FB8U + slot)
                : read(work_ram, 0x2EA0U + slot);
            voice.noise = 0U;
        }
        occupied[channel] = true;
    };

    for (int slot = kMusicSlotCount - 1; slot >= 0; --slot) {
        const auto index = static_cast<std::uint8_t>(slot);
        const auto primary = read(work_ram, 0x2E82U + index);
        if (!valid_channel(primary) || occupied[primary]
            || (read(work_ram, 0x2EBEU + index) & 0x02U) != 0U
            || (read(work_ram, 0x2F22U + index)
                | read(work_ram, 0x2F2CU + index)) == 0U) {
            continue;
        }

        const auto state = read(work_ram, 0x2E46U + index);
        const auto special = static_cast<std::uint8_t>(state & 0x0AU);
        if (special != 0U) {
            if (special != 0x02U || primary >= 4U) {
                commit(index, primary, true);
                continue;
            }
            commit(
                index,
                read(work_ram, 0x2EAAU + index),
                true);
        }
        if ((state & 0x01U) != 0U) {
            commit(index, primary, false);
        }
    }

    return voices;
}

void MusicVoiceOverlay::program_voice(
    const std::int32_t timestamp,
    const std::uint8_t channel,
    const Voice& voice) noexcept
{
    psg_.write(timestamp, 0x00U, channel);
    if (!voice.active) {
        psg_.write(timestamp, 0x07U, 0U);
        psg_.write(timestamp, 0x04U, 0U);
        return;
    }

    psg_.write(timestamp, 0x02U, voice.period_low);
    psg_.write(
        timestamp,
        0x03U,
        (voice.period_low | voice.period_high) == 0U
            ? 0x0FU
            : voice.period_high);
    psg_.write(timestamp, 0x05U, voice.balance);

    if (!voice.leave_waveform
        && loaded_waveforms_[channel] != voice.waveform) {
        const auto offset = static_cast<std::size_t>(voice.waveform) * 32U;
        if (offset + 32U <= waveform_table_.size()) {
            psg_.write(timestamp, 0x04U, 0x40U);
            psg_.write(timestamp, 0x04U, 0x00U);
            for (std::size_t entry = 0; entry < 32U; ++entry) {
                psg_.write(
                    timestamp,
                    0x06U,
                    std::to_integer<std::uint8_t>(
                        waveform_table_[offset + entry]));
            }
            loaded_waveforms_[channel] = voice.waveform;
        }
    }

    psg_.write(
        timestamp,
        0x04U,
        static_cast<std::uint8_t>((voice.volume & 0x1FU) | 0x80U));
    psg_.write(timestamp, 0x07U, voice.noise);
}

void MusicVoiceOverlay::synchronize(
    const std::int32_t timestamp,
    const std::span<const std::byte> work_ram) noexcept
{
    // Remove the old contribution at this exact edge, change the derived
    // register file silently, then expose only the newly displaced voices.
    psg_.set_output_channel_mask(timestamp, 0U);
    displaced_channel_mask_ = effect_channel_mask(work_ram);
    auto voices = music_voices(work_ram);
    // Where no effect displaced the physical channel, use 45:49F9's actual
    // latch result as a parity oracle. Keeping the silent shadow in lockstep
    // there preserves its oscillator phase until an effect arrives; only a
    // stolen channel needs the music-only reconstruction above.
    for (std::uint8_t channel = 0U;
         channel < PcePsg::kChannelCount;
         ++channel) {
        if ((displaced_channel_mask_ & (1U << channel)) != 0U) {
            continue;
        }
        auto& voice = voices[channel];
        voice.active = read(work_ram, 0x2E22U + channel) != 0U;
        voice.period_low = read(work_ram, 0x2E16U + channel);
        voice.period_high = read(work_ram, 0x2E1CU + channel);
        voice.volume = read(work_ram, 0x2E22U + channel);
        voice.balance = read(work_ram, 0x2E28U + channel);
        voice.waveform = read(work_ram, 0x2E2EU + channel);
        voice.leave_waveform = voice.waveform == 0xFFU;
        voice.noise = read(work_ram, 0x2E34U + channel);
    }
    for (std::uint8_t channel = 0U;
         channel < PcePsg::kChannelCount;
         ++channel) {
        program_voice(timestamp, channel, voices[channel]);
    }
    psg_.write(timestamp, 0x08U, read(work_ram, 0x2E40U));
    psg_.write(
        timestamp,
        0x09U,
        static_cast<std::uint8_t>(read(work_ram, 0x2E41U) & 0x7FU));

    psg_.set_output_channel_mask(timestamp, displaced_channel_mask_);
}

void MusicVoiceOverlay::observe_primary_write(
    const std::int32_t timestamp,
    const std::uint8_t address,
    const std::uint8_t value,
    const std::span<const std::byte> work_ram) noexcept
{
    const auto port = static_cast<std::uint8_t>(address & 0x0FU);

    if (pending_sequencer_select_) {
        // 45:4142 selects channel zero, then writes either its period ($0802)
        // or, for a silent latch, its cleared noise register ($0807). The
        // voice-level $49D2 commit also selects channel zero but follows it
        // directly with $0804, so this two-write signature excludes it.
        if (port == 0x02U || port == 0x07U) {
            synchronize(timestamp, work_ram);
        }
        pending_sequencer_select_ = false;
    }

    if (port == 0x00U) {
        pending_sequencer_select_ = value == 0U
            && (read(work_ram, 0x20D6U) & kSequencerInProgress) != 0U;
    } else if (port == 0x01U) {
        // Global balance is not part of the per-frame latch bank. Mirror its
        // rare direct writes so both PSGs use the same master attenuation.
        psg_.write(timestamp, address, value);
    }
}

} // namespace devils_crush::audio
