// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/audio/music_voice_overlay.hpp"

namespace devils_crush::audio {
namespace {

constexpr std::uint16_t kWorkRamBase = 0x2000U;
constexpr std::uint8_t kMusicSlotCount = 6U;
constexpr std::uint8_t kEffectSlotFirst = 6U;

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
    upload_pending_ = false;
    displaced_channel_mask_ = 0U;
    sequenced_slots_.fill(false);
    voice_volumes_.fill(0U);
    effect_channel_mask_ = 0U;
    psg_.set_output_channel_mask(timestamp, 0U);
}

void MusicVoiceOverlay::observe_sequencer_write(
    const std::uint8_t bank, const std::uint16_t pc,
    const std::uint8_t value,
    const std::span<const std::byte> work_ram, const std::uint16_t address) noexcept
{
    if (bank != 0x45U) return;
    if (pc == 0x49B6U) {
        // The physical latch clear also starts a fresh music-only pass.
        // In particular, exclusive slot 9 can stop the walk before music.
        sequenced_slots_.fill(false);
        effect_channel_mask_ = 0U;
    } else if (pc == 0x48A5U || pc == 0x48B0U) {
        const auto slot = read(work_ram, 0x2E34U);
        if (slot < voice_volumes_.size()) {
            const auto fade = read(work_ram, 0x2E44U);
            voice_volumes_[slot] =
                (read(work_ram, 0x2EB4U + slot) & 0x08U) != 0U
                ? static_cast<std::uint8_t>(value + fade < 31U
                    ? 0U : value + fade - 31U)
                : value;
        }
    } else if (pc == 0x4138U || pc == 0x4B75U) {
        // The return-address push of the JSR into the 4142 upload walk: at
        // 4138 after a sequencer pass, or at 4B75 on the reset arm, whose
        // 49B6 clears the latches before it jumps into the walk. The latch
        // bank is final, and the walk's first PSG write follows.
        upload_pending_ = true;
    } else if (pc == 0x40B1U) {
        const auto slot = read(work_ram, 0x2E34U);
        if (slot < sequenced_slots_.size()) sequenced_slots_[slot] = true;
    } else if ((pc == 0x4A92U || pc == 0x49EFU || pc == 0x4A25U)
        && address >= 0x2E22U && address < 0x2E28U) {
        // Use the actual winning latch, including zero-volume rests and
        // auxiliary channels. A silent effect does not own a channel: the
        // cartridge allows a later music voice to claim it in the same pass.
        const auto bit = static_cast<std::uint8_t>(1U << (address - 0x2E22U));
        effect_channel_mask_ = static_cast<std::uint8_t>(effect_channel_mask_ & ~bit);
        if (value != 0U && read(work_ram, 0x2E34U) >= kEffectSlotFirst) {
            effect_channel_mask_ = static_cast<std::uint8_t>(effect_channel_mask_ | bit);
        }
    }
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
        voice.volume = voice_volumes_[slot];
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
        if (!sequenced_slots_[index] || voice_volumes_[index] == 0U) continue;
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
    displaced_channel_mask_ = effect_channel_mask_;
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

    if (upload_pending_) {
        // The walk does not always begin at channel zero: while a
        // timer-driven voice holds channel zero ($F5), the walk skips it and
        // starts by selecting channel one. The end-of-game and game-over
        // tunes do this for seconds at a time, so waiting for a channel-zero
        // select left the previous pass's displaced voices sounding.
        upload_pending_ = false;
        synchronize(timestamp, work_ram);
    }

    if (port == 0x01U) {
        // Global balance is not part of the per-frame latch bank. Mirror its
        // rare direct writes so both PSGs use the same master attenuation.
        psg_.write(timestamp, address, value);
    }
}

} // namespace devils_crush::audio
