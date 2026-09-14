// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/audio/pce_psg.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace devils_crush::audio {

// A second, host-owned PSG containing only the six persistent music voices.
//
// Devil's Crush owns six physical PSG channels. Slots 6-8 are effects and
// slot 9 is an exclusive sequencer voice; they are committed first. When one
// claims a channel, the corresponding music slot keeps advancing but its commit is
// skipped. Executed stores identify each slot's envelope and the actual
// winning hardware latch. This model reconstructs the music-only commit from
// slots 0-5 and makes just those displaced channels audible. The physical PSG remains
// untouched and continues to play every effect exactly as the game asks.
class MusicVoiceOverlay {
public:
    static constexpr std::size_t kWorkRamSize = 0x2000U;

    explicit MusicVoiceOverlay(
        std::span<const std::byte> waveform_table);

    void configure(
        std::int32_t* left_buffer,
        std::int32_t* right_buffer,
        PcePsg::Revision revision) noexcept;
    void power(std::int32_t timestamp) noexcept;
    void update(std::int32_t timestamp) noexcept;
    void reset_timestamp(std::int32_t base) noexcept;

    // Observes writes sent to the physical PSG. The channel-zero select and
    // period/noise prefix made by 45:4142 identifies the point after all nine
    // logical voices have been sequenced and immediately before the six
    // hardware channels are pushed.
    void observe_primary_write(
        std::int32_t timestamp,
        std::uint8_t address,
        std::uint8_t value,
        std::span<const std::byte> work_ram) noexcept;

    // Observe executed sequencer stores, including stores inside native
    // transcripts. Envelope volume is scratch RAM, not a per-slot latch.
    void observe_sequencer_write(
        std::uint8_t bank, std::uint16_t pc, std::uint8_t value,
        std::span<const std::byte> work_ram, std::uint16_t address = 0U) noexcept;

    // Silences and invalidates the derived device after a reset or savestate
    // load. The next sequencer pass rebuilds it from live work RAM.
    void invalidate(std::int32_t timestamp) noexcept;

    [[nodiscard]] std::uint8_t displaced_channel_mask() const noexcept
    {
        return displaced_channel_mask_;
    }

private:
    struct Voice {
        bool active = false;
        bool leave_waveform = false;
        std::uint8_t period_low = 0;
        std::uint8_t period_high = 0;
        std::uint8_t volume = 0;
        std::uint8_t balance = 0;
        std::uint8_t waveform = 0;
        std::uint8_t noise = 0;
    };

    [[nodiscard]] std::uint8_t read(
        std::span<const std::byte> work_ram,
        std::uint16_t logical_address) const noexcept;
    [[nodiscard]] std::array<Voice, PcePsg::kChannelCount> music_voices(
        std::span<const std::byte> work_ram) const noexcept;
    void synchronize(
        std::int32_t timestamp,
        std::span<const std::byte> work_ram) noexcept;
    void program_voice(
        std::int32_t timestamp,
        std::uint8_t channel,
        const Voice& voice) noexcept;

    PcePsg psg_{nullptr, nullptr, PcePsg::Revision::HuC6280};
    std::vector<std::byte> waveform_table_;
    std::array<int, PcePsg::kChannelCount> loaded_waveforms_{};
    std::array<bool, 10> sequenced_slots_{};
    std::array<std::uint8_t, 10> voice_volumes_{};
    std::uint8_t effect_channel_mask_ = 0;
    bool pending_sequencer_select_ = false;
    std::uint8_t displaced_channel_mask_ = 0;
};

}  // namespace devils_crush::audio
