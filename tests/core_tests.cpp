// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/app/native_registry.hpp"
#include "devils_crush/audio/music_voice_overlay.hpp"
#include "devils_crush/core/atomic_file.hpp"
#include "devils_crush/core/backup_ram.hpp"
#include "devils_crush/core/data_files.hpp"
#include "devils_crush/core/high_score_table.hpp"
#include "devils_crush/cpu/huc6280_idle_skip.hpp"
#include "devils_crush/cabinet/cabinet_state.hpp"
#include "devils_crush/cabinet/display_config.hpp"
#include "devils_crush/cabinet/dmd_layout.hpp"
#include "devils_crush/cabinet/dmd_panel.hpp"
#include "devils_crush/core/rom_image.hpp"
#include "devils_crush/core/save_state.hpp"
#include "devils_crush/emulation/pce_runtime.hpp"
#include "devils_crush/emulation/routine_replay_corpus.hpp"
#include "devils_crush/input/pce_pad.hpp"
#include "devils_crush/input/plunger.hpp"
#include "devils_crush/native_runtime/actor_collision_geometry.hpp"
#include "devils_crush/native_runtime/actor_collision_mask_asset.hpp"
#include "devils_crush/native_runtime/actor_collision_mask_coordinates.hpp"
#include "devils_crush/native_runtime/actor_collision_scan.hpp"
#include "devils_crush/native_runtime/actor_motion.hpp"
#include "devils_crush/native_runtime/actor_motion_capture.hpp"
#include "devils_crush/native_runtime/actor_scheduler.hpp"
#include "devils_crush/native_runtime/actor_state.hpp"
#include "devils_crush/native_runtime/actor_collision_seed.hpp"
#include "devils_crush/native_runtime/ball_capture.hpp"
#include "devils_crush/native_runtime/ball_frame_schedule.hpp"
#include "devils_crush/native_runtime/ball_physics.hpp"
#include "devils_crush/native_runtime/bonus_stage_launch.hpp"
#include "devils_crush/native_runtime/bonus_stage_4.hpp"
#include "devils_crush/native_runtime/bonus_stage_4_capture.hpp"
#include "devils_crush/native_runtime/bonus_stage_4_linked_target.hpp"
#include "devils_crush/native_runtime/bonus_stage_4_linked_target_capture.hpp"
#include "devils_crush/native_runtime/bonus_stage_4_orbiting_child_capture.hpp"
#include "devils_crush/native_runtime/bumper.hpp"
#include "devils_crush/native_runtime/bumper_capture.hpp"
#include "devils_crush/native_runtime/collision_detection.hpp"
#include "devils_crush/native_runtime/collision_effect.hpp"
#include "devils_crush/native_runtime/collision_effect_capture.hpp"
#include "devils_crush/native_runtime/collision_map.hpp"
#include "devils_crush/native_runtime/collision_map_asset.hpp"
#include "devils_crush/native_runtime/collision_mask.hpp"
#include "devils_crush/native_runtime/collision_reaction.hpp"
#include "devils_crush/native_runtime/collision_retry.hpp"
#include "devils_crush/native_runtime/collision_retry_asset.hpp"
#include "devils_crush/native_runtime/collision_retry_placement.hpp"
#include "devils_crush/native_runtime/collision_retry_probe.hpp"
#include "devils_crush/native_runtime/collision_response.hpp"
#include "devils_crush/native_runtime/collision_response_asset.hpp"
#include "devils_crush/native_runtime/collision_table.hpp"
#include "devils_crush/native_runtime/collision_table_asset.hpp"
#include "devils_crush/native_runtime/collision_table_driver.hpp"
#include "devils_crush/native_runtime/contact_dispatch.hpp"
#include "devils_crush/native_runtime/dragon_egg.hpp"
#include "devils_crush/native_runtime/dragon_egg_capture.hpp"
#include "devils_crush/native_runtime/dragon_egg_hatchling.hpp"
#include "devils_crush/native_runtime/dragon_head.hpp"
#include "devils_crush/native_runtime/dragon_head_capture.hpp"
#include "devils_crush/native_runtime/frame_oracle.hpp"
#include "devils_crush/native_runtime/emulator_oracle_adapter.hpp"
#include "devils_crush/native_runtime/front_spider.hpp"
#include "devils_crush/native_runtime/front_spider_capture.hpp"
#include "devils_crush/native_runtime/game_state.hpp"
#include "devils_crush/native_runtime/gameplay_bootstrap.hpp"
#include "devils_crush/native_runtime/gameplay_audio_dispatch.hpp"
#include "devils_crush/native_runtime/giant_skeleton.hpp"
#include "devils_crush/native_runtime/giant_skeleton_capture.hpp"
#include "devils_crush/native_runtime/giant_skeleton_eyeball.hpp"
#include "devils_crush/native_runtime/giant_skeleton_eyeball_capture.hpp"
#include "devils_crush/native_runtime/giant_face_capture.hpp"
#include "devils_crush/native_runtime/giant_face_helmet_capture.hpp"
#include "devils_crush/native_runtime/mapped_rom.hpp"
#include "devils_crush/native_runtime/native_gameplay_state.hpp"
#include "devils_crush/native_runtime/native_gameplay_runtime.hpp"
#include "devils_crush/native_runtime/physics_state.hpp"
#include "devils_crush/native_runtime/playfield_collision.hpp"
#include "devils_crush/native_runtime/score_state.hpp"
#include "devils_crush/native_runtime/score_capture.hpp"
#include "devils_crush/native_runtime/side_spider.hpp"
#include "devils_crush/native_runtime/side_spider_capture.hpp"
#include "devils_crush/native_runtime/skeleton_insect.hpp"
#include "devils_crush/native_runtime/skeleton_insect_capture.hpp"
#include "devils_crush/native_runtime/small_soldier.hpp"
#include "devils_crush/native_runtime/small_soldier_capture.hpp"
#include "devils_crush/native_runtime/small_soldier_rules.hpp"
#include "devils_crush/native_runtime/small_soldier_scheduler.hpp"
#include "devils_crush/native_runtime/tower.hpp"
#include "devils_crush/native_runtime/tower_capture.hpp"
#include "devils_crush/native_runtime/tower_guard.hpp"
#include "devils_crush/native_runtime/tower_guard_capture.hpp"
#include "devils_crush/native_runtime/tower_insect.hpp"
#include "devils_crush/native_runtime/tower_insect_capture.hpp"
#include "devils_crush/native_runtime/tower_insect_scheduler.hpp"
#include "devils_crush/emulation/playfield_collision_adapter.hpp"
#include "devils_crush/game_logic/actor.h"
#include "devils_crush/game_logic/collision.h"
#include "devils_crush/game_logic/sprite.h"
#include "devils_crush/game_logic/video.h"
#include "devils_crush/render/table_palette_cache.hpp"
#include "devils_crush/game_logic/controls.h"
#include "devils_crush/game_logic/physics.h"
#include "devils_crush/game_logic/rng.h"
#include "devils_crush/game_logic/score.h"
#include "devils_crush/render/pce_background.hpp"
#include "devils_crush/render/pce_sprite_overlay.hpp"
#include "devils_crush/render/sharp_pixel_scaler.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

int failures = 0;

void check(const bool condition, const std::string_view description)
{
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

void check_rect(
    const devils_crush::render::IntRect actual,
    const devils_crush::render::IntRect expected,
    const std::string_view description)
{
    check(
        actual.x == expected.x && actual.y == expected.y
            && actual.width == expected.width
            && actual.height == expected.height,
        description);
}

void test_hash_vectors()
{
    constexpr std::array<std::byte, 0> empty{};
    const auto empty_digests =
        devils_crush::rom::compute_digests(empty);
    check(
        empty_digests.md5 == "d41d8cd98f00b204e9800998ecf8427e",
        "MD5 empty vector");
    check(
        empty_digests.sha256
            == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991"
               "b7852b855",
        "SHA-256 empty vector");

    constexpr std::array bytes{
        std::byte{'a'},
        std::byte{'b'},
        std::byte{'c'},
    };
    const auto digests = devils_crush::rom::compute_digests(bytes);
    check(
        digests.md5 == "900150983cd24fb0d6963f7d28e17f72",
        "MD5 known vector");
    check(
        digests.sha256
            == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61"
               "f20015ad",
        "SHA-256 known vector");
}

void test_music_voice_overlay()
{
    constexpr std::size_t kBufferPositions = 65536U + 32U;
    std::vector<std::int32_t> left(kBufferPositions);
    std::vector<std::int32_t> right(kBufferPositions);
    std::array<std::byte, 32> waveform{};
    for (std::size_t index = 0; index < waveform.size(); ++index) {
        waveform[index] = static_cast<std::byte>(
            (index & 1U) != 0U ? 0x1FU : 0x00U);
    }

    devils_crush::audio::MusicVoiceOverlay overlay(waveform);
    overlay.configure(
        left.data(),
        right.data(),
        devils_crush::audio::PcePsg::Revision::HuC6280A);
    overlay.power(0);

    std::array<std::byte,
               devils_crush::audio::MusicVoiceOverlay::kWorkRamSize>
        ram{};
    const auto store = [&ram](
                           const std::uint16_t address,
                           const std::uint8_t value) {
        ram[static_cast<std::size_t>(address - 0x2000U)] =
            static_cast<std::byte>(value);
    };

    // Effect slot 6 and music slot 2 both target channel 2. The effect wins
    // the cartridge's physical latch, while the overlay reconstructs slot 2.
    store(0x20D6U, 0x04U);
    store(0x2E46U + 6U, 0x01U);
    store(0x2E82U + 6U, 0x02U);
    store(0x2F22U + 6U, 0x40U);
    store(0x2F2CU + 6U, 0x01U);
    store(0x2E46U + 2U, 0x01U);
    store(0x2E82U + 2U, 0x02U);
    store(0x2F22U + 2U, 0x60U);
    store(0x2F2CU + 2U, 0x01U);
    store(0x2EA0U + 2U, 0x00U);
    store(0x2FAEU + 2U, 0xFFU);
    store(0x2E37U, 0x1FU);

    const auto sequenced = [&](std::uint8_t slot, std::uint8_t volume) {
        store(0x2E34U, slot);
        overlay.observe_sequencer_write(0x45U, 0x48B0U, volume, ram);
        if (slot == 6U) overlay.observe_sequencer_write(
            0x45U, 0x4A92U, volume, ram, 0x2E24U);
        overlay.observe_sequencer_write(0x45U, 0x40B1U, 0x04U, ram);
    };
    overlay.observe_sequencer_write(0x45U, 0x49B6U, 0U, ram);
    sequenced(6U, 31U);
    sequenced(2U, 31U);

    overlay.observe_primary_write(0, 0x01U, 0xFFU, ram);
    overlay.observe_primary_write(0, 0x00U, 0x00U, ram);
    overlay.observe_primary_write(0, 0x02U, 0x40U, ram);
    check(
        overlay.displaced_channel_mask() == 0x04U,
        "music overlay identifies the effect-displaced PSG channel");

    overlay.update(20000);
    check(
        std::ranges::any_of(left, [](const std::int32_t sample) {
            return sample != 0;
        }),
        "music overlay emits the displaced persistent voice");

    // With the effect gone the same sequencer prefix mutes the derived voice;
    // it is not a duplicate of music already audible on the physical PSG.
    store(0x2E46U + 6U, 0x00U);
    store(0x2F22U + 6U, 0x00U);
    store(0x2F2CU + 6U, 0x00U);
    overlay.observe_sequencer_write(0x45U, 0x4A25U, 0U, ram, 0x2E24U);
    overlay.observe_primary_write(20000, 0x00U, 0x00U, ram);
    overlay.observe_primary_write(20000, 0x02U, 0x40U, ram);
    check(
        overlay.displaced_channel_mask() == 0x00U,
        "music overlay mutes itself when the physical music voice returns");

    // An exclusive announcement sequences slot 9 and stops the walk. Old
    // music and effect records deliberately remain populated in work RAM.
    store(0x2E46U + 6U, 1U);
    store(0x2F22U + 6U, 0x40U);
    overlay.observe_sequencer_write(0x45U, 0x49B6U, 0U, ram);
    sequenced(9U, 31U);
    overlay.observe_primary_write(20000, 0x00U, 0U, ram);
    overlay.observe_primary_write(20000, 0x07U, 0U, ram);
    check(overlay.displaced_channel_mask() == 0U,
        "exclusive sequencer pass does not resurrect stale music/effects");

    // A different voice's final scratch volume must not change the displaced
    // voice's audio. Compare independently powered devices sample for sample.
    const auto render = [&](std::uint8_t scratch, std::uint8_t volume,
                            bool fade) {
        std::vector<std::int32_t> samples(kBufferPositions);
        std::vector<std::int32_t> right_samples(kBufferPositions);
        devils_crush::audio::MusicVoiceOverlay subject(waveform);
        subject.configure(samples.data(), right_samples.data(),
            devils_crush::audio::PcePsg::Revision::HuC6280A);
        subject.power(0);
        store(0x2E37U, scratch);
        store(0x2EB4U + 2U, fade ? 8U : 0U);
        store(0x2E44U, 25U);
        subject.observe_sequencer_write(0x45U, 0x49B6U, 0U, ram);
        for (auto slot : {6U, 2U}) {
            store(0x2E34U, static_cast<std::uint8_t>(slot));
            subject.observe_sequencer_write(0x45U, 0x48A5U, volume, ram);
            if (slot == 6U) subject.observe_sequencer_write(
                0x45U, 0x4A92U, 31U, ram, 0x2E24U);
            subject.observe_sequencer_write(0x45U, 0x40B1U, 4U, ram);
        }
        subject.observe_primary_write(0, 1U, 0xFFU, ram);
        subject.observe_primary_write(0, 0U, 0U, ram);
        subject.observe_primary_write(0, 2U, 0U, ram);
        subject.update(20000);
        return samples;
    };
    const auto quiet = render(31U, 15U, false);
    check(quiet == render(1U, 15U, false),
        "other voices' scratch volumes do not modulate displaced music");
    check(quiet != render(31U, 31U, false),
        "displaced music follows its own envelope volume");
    check(quiet == render(31U, 21U, true),
        "displaced music follows the original fade attenuation");

    overlay.observe_sequencer_write(0x45U, 0x49B6U, 0U, ram);
    sequenced(6U, 0U);
    sequenced(2U, 31U);
    overlay.observe_sequencer_write(0x45U, 0x4A92U, 31U, ram, 0x2E24U);
    overlay.observe_primary_write(20000, 0U, 0U, ram);
    overlay.observe_primary_write(20000, 2U, 0U, ram);
    check(overlay.displaced_channel_mask() == 0U,
        "silent effect cannot double the music that won the physical channel");
}

void test_native_cached_position_restore()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    constexpr std::array<std::uint8_t, DC_CACHED_POSITION_BYTE_COUNT>
        cached{0x12U, 0x34U, 0x56U, 0x78U, 0x9AU, 0xBCU};
    std::copy(
        cached.begin(),
        cached.end(),
        work_ram.begin() + 0x85);

    dc_cached_position_restore restore{};
    check(
        dc_restore_cached_position(
            work_ram.data(),
            work_ram.size(),
            3U,
            &restore)
            != 0,
        "native F241 accepts a complete work-RAM snapshot");
    constexpr std::array<std::uint16_t, DC_CACHED_POSITION_BYTE_COUNT>
        expected_offsets{
            0x1103U,
            0x1183U,
            0x1203U,
            0x1283U,
            0x1303U,
            0x1383U,
        };
    check(
        std::ranges::equal(
            restore.work_ram_offsets,
            expected_offsets),
        "native F241 indexes all six position arrays");
    check(
        std::ranges::equal(restore.values, cached),
        "native F241 returns all six cached bytes in store order");
    check(
        dc_restore_cached_position(
            work_ram.data(),
            work_ram.size() - 1U,
            0U,
            &restore)
            == 0,
        "native F241 rejects a truncated work-RAM snapshot");
}

void test_native_position_integrator()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    constexpr std::uint8_t slot = 3U;
    work_ram[0x1280U + slot] = 0x9CU;
    work_ram[0x1300U + slot] = 0x96U;
    work_ram[0x1380U + slot] = 0x02U;
    work_ram[0x1100U + slot] = 0x00U;
    work_ram[0x1180U + slot] = 0x0EU;
    work_ram[0x1200U + slot] = 0x01U;
    work_ram[0x00BBU + slot] = 0xCFU;
    work_ram[0x00BCU + slot] = 0xFFU;
    work_ram[0x1C80U + slot] = 0xA7U;

    dc_position_integration integration{};
    check(
        dc_integrate_position(
            work_ram.data(),
            work_ram.size(),
            slot,
            &integration)
            != 0,
        "native F1B4 accepts a complete work-RAM snapshot");
    constexpr std::array<std::uint16_t, DC_CACHED_POSITION_BYTE_COUNT>
        expected_cache_offsets{
            0x0088U,
            0x0089U,
            0x008AU,
            0x0085U,
            0x0086U,
            0x0087U,
        };
    constexpr std::array<std::uint8_t, DC_CACHED_POSITION_BYTE_COUNT>
        expected_cached_values{
            0x9CU,
            0x96U,
            0x02U,
            0x00U,
            0x0EU,
            0x01U,
        };
    constexpr std::array<
        std::uint16_t,
        DC_INTEGRATED_POSITION_BYTE_COUNT>
        expected_position_offsets{
            0x1283U,
            0x1303U,
            0x1383U,
            0x1103U,
            0x1183U,
            0x1203U,
        };
    constexpr std::array<
        std::uint8_t,
        DC_INTEGRATED_POSITION_BYTE_COUNT>
        expected_position_values{
            0x6BU,
            0x96U,
            0x02U,
            0x00U,
            0x0EU,
            0x01U,
        };
    check(
        std::ranges::equal(
            integration.cache_work_ram_offsets,
            expected_cache_offsets)
            && std::ranges::equal(
                integration.cached_values,
                expected_cached_values),
        "native F1B4 caches both original signed 16.8 positions");
    check(
        std::ranges::equal(
            integration.position_work_ram_offsets,
            expected_position_offsets)
            && std::ranges::equal(
                integration.position_values,
                expected_position_values),
        "native F1B4 sign-extends and integrates both signed 8.8 increments");
    check(
        integration.cleared_motion_flags == 0xA5U
            && integration.final_motion_flags == 0xA7U
            && integration.first_increment_negative == 0U
            && integration.second_increment_negative == 1U
            && integration.integer_position_unchanged == 1U,
        "native F1B4 marks a fractional-only update without losing other flags");

    work_ram[0x1280U + slot] = 0x00U;
    work_ram[0x1300U + slot] = 0x8FU;
    work_ram[0x1380U + slot] = 0x02U;
    work_ram[0x1100U + slot] = 0x00U;
    work_ram[0x1180U + slot] = 0x0EU;
    work_ram[0x1200U + slot] = 0x01U;
    work_ram[0x00B9U + slot] = 0U;
    work_ram[0x00BAU + slot] = 0U;
    work_ram[0x00BBU + slot] = 0x10U;
    work_ram[0x00BCU + slot] = 0U;
    work_ram[0x1C80U + slot] = 0U;
    check(
        dc_integrate_position(
            work_ram.data(),
            work_ram.size(),
            slot,
            &integration)
                != 0
            && integration.position_values[0] == 0x10U
            && integration.position_values[1] == 0x8FU
            && integration.position_values[2] == 0x02U
            && integration.position_values[3] == 0x00U
            && integration.position_values[4] == 0x0EU
            && integration.position_values[5] == 0x01U
            && integration.first_increment_negative == 0U
            && integration.second_increment_negative == 0U
            && integration.integer_position_unchanged == 1U
            && integration.final_motion_flags == 2U,
        "native F1B4 matches the bounded nonnegative integration call");

    work_ram[0x1C80U + slot] = 0xA7U;
    work_ram[0x00BAU + slot] = 0x01U;
    check(
        dc_integrate_position(
            work_ram.data(),
            work_ram.size(),
            slot,
            &integration)
                != 0
            && integration.integer_position_unchanged == 0U
            && integration.final_motion_flags == 0xA5U,
        "native F1B4 leaves the motion marker clear after an integer step");
    check(
        dc_integrate_position(
            work_ram.data(),
            work_ram.size() - 1U,
            slot,
            &integration)
            == 0,
        "native F1B4 rejects a truncated work-RAM snapshot");
}

void test_native_motion_damping()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    constexpr std::uint8_t slot = 5U;
    work_ram[0x1400U + slot] = 0x80U;
    work_ram[0x1480U + slot] = 0x01U;
    work_ram[0x1500U + slot] = 0x80U;
    work_ram[0x1580U + slot] = 0xFEU;
    work_ram[0x008BU] = 0x20U;
    work_ram[0x008CU] = 0xFCU;
    work_ram[0x008DU] = 0x04U;

    dc_motion_damping damping{};
    check(
        dc_damp_and_accelerate_motion(
            work_ram.data(),
            work_ram.size(),
            slot,
            &damping)
            != 0,
        "native DD09 accepts a complete work-RAM snapshot");
    check(
        damping.first_original == 0x0180U
            && damping.second_original == 0xFE80U
            && damping.first_damped == 0x01E0U
            && damping.second_damped == 0xFEE0U
            && damping.second_accelerated == 0xFF00U,
        "native DD09 truncates signed quotients and accelerates axis two");
    check(
        damping.first_component_nonzero == 1U
            && damping.second_component_nonzero == 1U
            && damping.first_divisor == 0xFCU
            && damping.second_divisor == 0x04U
            && damping.second_acceleration == 0x20U,
        "native DD09 reports its branch and coefficient inputs");

    work_ram[0x1400U + slot] = 0U;
    work_ram[0x1480U + slot] = 0U;
    work_ram[0x1500U + slot] = 0x10U;
    work_ram[0x1580U + slot] = 0U;
    work_ram[0x008BU] = 0x10U;
    work_ram[0x008CU] = 0x80U;
    work_ram[0x008DU] = 0x80U;
    check(
        dc_damp_and_accelerate_motion(
            work_ram.data(),
            work_ram.size(),
            slot,
            &damping)
                != 0
            && damping.first_component_nonzero == 0U
            && damping.second_component_nonzero == 1U
            && damping.second_original == 0x0010U
            && damping.second_damped == 0x0010U
            && damping.second_accelerated == 0x0020U,
        "native DD09 matches the bounded nonzero-divider call");

    work_ram[0x1400U + slot] = 0U;
    work_ram[0x1480U + slot] = 0U;
    work_ram[0x1500U + slot] = 0U;
    work_ram[0x1580U + slot] = 0U;
    work_ram[0x008BU] = 0x10U;
    work_ram[0x008CU] = 0U;
    work_ram[0x008DU] = 0U;
    check(
        dc_damp_and_accelerate_motion(
            work_ram.data(),
            work_ram.size(),
            slot,
            &damping)
                != 0
            && damping.first_component_nonzero == 0U
            && damping.second_component_nonzero == 0U
            && damping.first_damped == 0U
            && damping.second_damped == 0U
            && damping.second_accelerated == 0x0010U,
        "native DD09 skips unused divisors on its zero-component path");

    work_ram[0x1400U + slot] = 1U;
    check(
        dc_damp_and_accelerate_motion(
            work_ram.data(),
            work_ram.size(),
            slot,
            &damping)
            == 0,
        "native DD09 rejects a zero divisor for a live component");
    check(
        dc_damp_and_accelerate_motion(
            work_ram.data(),
            work_ram.size() - 1U,
            slot,
            &damping)
            == 0,
        "native DD09 rejects a truncated work-RAM snapshot");
}

void test_native_motion_substep_split()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    constexpr std::uint8_t slot = 5U;

    const auto load = [&work_ram](
                          const std::uint16_t first,
                          const std::uint16_t second) {
        work_ram[0x1400U + slot] = static_cast<std::uint8_t>(first);
        work_ram[0x1480U + slot] = static_cast<std::uint8_t>(first >> 8U);
        work_ram[0x1500U + slot] = static_cast<std::uint8_t>(second);
        work_ram[0x1580U + slot] = static_cast<std::uint8_t>(second >> 8U);
    };

    dc_motion_substep_split split{};
    load(0x0340U, 0x0180U);
    check(
        dc_split_motion_substeps(
            work_ram.data(),
            work_ram.size(),
            slot,
            &split)
                != 0
            && split.magnitude == 0x03U
            && split.substeps == 1U
            && split.shifts == 0U
            && split.first_step == 0x0340U
            && split.second_step == 0x0180U,
        "native DC95 leaves motion below four whole units in one substep");

    load(0x0180U, 0x0480U);
    check(
        dc_split_motion_substeps(
            work_ram.data(),
            work_ram.size(),
            slot,
            &split)
                != 0
            && split.first_magnitude == 0x01U
            && split.second_magnitude == 0x04U
            && split.magnitude == 0x04U
            && split.substeps == 2U
            && split.shifts == 1U
            && split.first_step == 0x00C0U
            && split.second_step == 0x0240U,
        "native DC95 halves both components when either reaches four");

    load(0xF800U, 0x0100U);
    check(
        dc_split_motion_substeps(
            work_ram.data(),
            work_ram.size(),
            slot,
            &split)
                != 0
            && split.first_magnitude == 0x08U
            && split.magnitude == 0x08U
            && split.substeps == 4U
            && split.shifts == 2U
            && split.first_step == 0xFE00U
            && split.second_step == 0x0040U,
        "native DC95 quarters a negative component arithmetically");

    // The odd low bit is shifted out rather than rounded, so four substeps of
    // the quotient fall short of the original motion. The ROM does that, and
    // 01:C721 relies on it: the residue is what the damping pass reads back.
    load(0x0803U, 0x0000U);
    check(
        dc_split_motion_substeps(
            work_ram.data(),
            work_ram.size(),
            slot,
            &split)
                != 0
            && split.substeps == 4U
            && split.first_step == 0x0200U
            && split.second_magnitude == 0U,
        "native DC95 truncates rather than rounds the per-substep motion");

    // EOR #$FF / INC A maps $80 to itself, which is still far above the eight
    // that selects four substeps, so the fixed point costs nothing.
    load(0x8000U, 0x0000U);
    check(
        dc_split_motion_substeps(
            work_ram.data(),
            work_ram.size(),
            slot,
            &split)
                != 0
            && split.first_magnitude == 0x80U
            && split.substeps == 4U
            && split.first_step == 0xE000U,
        "native DC95 keeps the negation fixed point at four substeps");

    check(
        dc_split_motion_substeps(
            work_ram.data(),
            work_ram.size() - 1U,
            slot,
            &split)
            == 0,
        "native DC95 rejects a truncated work-RAM snapshot");
    check(
        dc_split_motion_substeps(
            work_ram.data(),
            work_ram.size(),
            0x80U,
            &split)
            == 0,
        "native DC95 rejects a slot outside the indexed tables");
}

void test_native_motion_rotation()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    work_ram[0x00C0U] = 0x80U;
    work_ram[0x00C1U] = 0x01U;
    work_ram[0x00BEU] = 0x80U;
    work_ram[0x00BFU] = 0xFFU;
    work_ram[0x007DU] = 0x00U;
    work_ram[0x007EU] = 0x00U;
    work_ram[0x007FU] = 0x00U;
    work_ram[0x0080U] = 0x01U;

    dc_motion_rotation rotation{};
    check(
        dc_rotate_motion(
            work_ram.data(),
            work_ram.size(),
            &rotation)
            != 0,
        "native D1A2 accepts a complete work-RAM snapshot");
    check(
        rotation.first_input == 0x0180U
            && rotation.second_input == 0xFF80U
            && rotation.cosine == 0x0000U
            && rotation.sine == 0x0100U
            && rotation.first_output == 0x0080U
            && rotation.second_output == 0x0180U,
        "native D1A2 rotates signed 8.8 components by a quarter turn");

    work_ram[0x007DU] = 0x80U;
    work_ram[0x007EU] = 0x00U;
    work_ram[0x007FU] = 0x00U;
    work_ram[0x0080U] = 0x00U;
    check(
        dc_rotate_motion(
            work_ram.data(),
            work_ram.size(),
            &rotation)
                != 0
            && rotation.first_output == 0x00C0U
            && rotation.second_output == 0xFFC0U,
        "native D1A2 truncates signed fractional products toward zero");
    check(
        dc_rotate_motion(
            work_ram.data(),
            work_ram.size() - 1U,
            &rotation)
            == 0,
        "native D1A2 rejects a truncated work-RAM snapshot");

    work_ram[0x0031U] = 0x80U;
    work_ram[0x0032U] = 0x01U;
    work_ram[0x007DU] = 0x80U;
    work_ram[0x007EU] = 0x00U;
    dc_flipper_radius_projection radius_projection{};
    check(
        dc_project_flipper_radius(
            work_ram.data(),
            work_ram.size(),
            &radius_projection)
                != 0,
        "native D612 accepts a complete work-RAM snapshot");
    check(
        radius_projection.radius == 0x0180U
            && radius_projection.coefficient == 0x0080U
            && radius_projection.output == 0x00C0U,
        "native D612 scales a signed 8.8 flipper radius");

    work_ram[0x0032U] = 0xFEU;
    check(
        dc_project_flipper_radius(
            work_ram.data(),
            work_ram.size(),
            &radius_projection)
                != 0
            && radius_projection.output == 0xFF40U,
        "native D612 preserves signed fractional truncation");
    check(
        dc_project_flipper_radius(
            work_ram.data(),
            work_ram.size() - 1U,
            &radius_projection)
            == 0,
        "native D612 rejects a truncated work-RAM snapshot");

    work_ram[0x1C85U] = 0x80U;
    work_ram[0x1180U] = 0x30U;
    work_ram[0x1185U] = 0x20U;
    work_ram[0x009EU] = 0x00U;
    work_ram[0x009FU] = 0x02U;
    dc_flipper_angle_projection angle_projection{};
    check(
        dc_project_flipper_angle(
            work_ram.data(),
            work_ram.size(),
            0U,
            5U,
            &angle_projection)
                != 0,
        "native D62E accepts a complete work-RAM snapshot");
    check(
        angle_projection.angle_difference == 0x10U
            && angle_projection.fixed_difference == 0x0080U
            && angle_projection.product == 0x0100U
            && angle_projection.output == 0x0100U
            && angle_projection.negates_product == 0U,
        "native D62E projects the direct directional angle delta");

    work_ram[0x1C85U] = 0x01U;
    work_ram[0x1180U] = 0x20U;
    work_ram[0x1185U] = 0x18U;
    check(
        dc_project_flipper_angle(
            work_ram.data(),
            work_ram.size(),
            0U,
            5U,
            &angle_projection)
                != 0
            && angle_projection.uses_wrapped_difference != 0U
            && angle_projection.angle_difference == 0x08U
            && angle_projection.product == 0x0080U
            && angle_projection.output == 0xFF80U
            && angle_projection.negates_product != 0U,
        "native D62E projects and negates the wrapped angle delta");

    work_ram[0x1180U] = 0x40U;
    check(
        dc_project_flipper_angle(
            work_ram.data(),
            work_ram.size(),
            0U,
            5U,
            &angle_projection)
                != 0
            && angle_projection.returns_zero != 0U
            && angle_projection.output == 0U,
        "native D62E clears output for a negative directional delta");
    check(
        dc_project_flipper_angle(
            work_ram.data(),
            work_ram.size() - 1U,
            0U,
            5U,
            &angle_projection)
            == 0,
        "native D62E rejects a truncated work-RAM snapshot");

    work_ram[0x0078U] = 0x40U;
    work_ram[0x00BEU] = 0x00U;
    work_ram[0x00BFU] = 0xFEU;
    dc_collision_motion_adjustment motion_adjustment{};
    check(
        dc_adjust_collision_motion(
            work_ram.data(),
            work_ram.size(),
            &motion_adjustment)
                != 0
            && motion_adjustment.output == 0xFF00U,
        "native CFD8 performs signed one-half scaling");

    work_ram[0x0078U] = 0x20U;
    work_ram[0x022CU] = 0U;
    work_ram[0x00BEU] = 0U;
    work_ram[0x00BFU] = 0x08U;
    check(
        dc_adjust_collision_motion(
            work_ram.data(),
            work_ram.size(),
            &motion_adjustment)
                != 0
            && motion_adjustment.output == 0x0180U,
        "native CFD8 combines signed one-eighth and one-sixteenth scaling");

    work_ram[0x022CU] = 1U;
    check(
        dc_adjust_collision_motion(
            work_ram.data(),
            work_ram.size(),
            &motion_adjustment)
                != 0
            && motion_adjustment.output == 0x0200U,
        "native CFD8 selects signed one-quarter scaling");

    work_ram[0x0078U] = 0U;
    work_ram[0x0CC7U] = 0U;
    work_ram[0x0081U] = 0x20U;
    work_ram[0x0082U] = 0x01U;
    work_ram[0x00BEU] = 0xF0U;
    work_ram[0x00BFU] = 0x05U;
    check(
        dc_adjust_collision_motion(
            work_ram.data(),
            work_ram.size(),
            &motion_adjustment)
                != 0
            && motion_adjustment.output == 0x0610U
            && motion_adjustment.clamps_high != 0U,
        "native CFD8 applies the positive high-byte clamp");
    check(
        dc_adjust_collision_motion(
            work_ram.data(),
            work_ram.size() - 1U,
            &motion_adjustment)
            == 0,
        "native CFD8 rejects a truncated work-RAM snapshot");
}

void test_native_signed_division()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    work_ram[0x002DU] = 0x80U;
    work_ram[0x002FU] = 0x0FU;
    work_ram[0x0030U] = 0x00U;

    dc_signed_division division{};
    check(
        dc_divide_signed_word(
            work_ram.data(),
            work_ram.size(),
            1U,
            &division)
                != 0
            && division.dividend_input == 0x000FU
            && division.divisor_input == 0x80U
            && division.divisor_magnitude == 0x80U
            && division.dividend_magnitude == 0x000FU
            && division.unsigned_initial_carry == 1U
            && division.quotient_magnitude == 0U
            && division.result == 0U
            && division.remainder == 0x0FU
            && division.result_negative == 1U,
        "native E905 preserves carry-sensitive positive division");

    work_ram[0x002DU] = 0x80U;
    work_ram[0x002FU] = 0x0DU;
    work_ram[0x0030U] = 0xFFU;
    check(
        dc_divide_signed_word(
            work_ram.data(),
            work_ram.size(),
            1U,
            &division)
                != 0
            && division.dividend_input == 0xFF0DU
            && division.dividend_magnitude == 0x00F3U
            && division.unsigned_initial_carry == 0U
            && division.quotient_magnitude == 1U
            && division.result == 1U
            && division.remainder == 0x73U
            && division.result_negative == 0U,
        "native E905 normalizes two negative operands");

    work_ram[0x002DU] = 0x04U;
    work_ram[0x002FU] = 0x80U;
    work_ram[0x0030U] = 0xFEU;
    check(
        dc_divide_signed_word(
            work_ram.data(),
            work_ram.size(),
            0U,
            &division)
                != 0
            && division.dividend_magnitude == 0x0180U
            && division.quotient_magnitude == 0x0060U
            && division.result == 0xFFA0U
            && division.result_negative == 1U,
        "native E905 restores a negative quotient");

    check(
        dc_divide_signed_word(
            work_ram.data(),
            work_ram.size() - 1U,
            0U,
            &division)
            == 0,
        "native E905 rejects a truncated work-RAM snapshot");
}

void test_native_unsigned_byte_division()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    work_ram[0x002FU] = 0x07U;
    work_ram[0x0030U] = 0xB6U;

    dc_unsigned_byte_division division{};
    check(
        dc_divide_unsigned_byte(
            work_ram.data(),
            work_ram.size(),
            0U,
            &division)
                != 0
            && division.dividend_input == 0xB6U
            && division.dividend_after_initial_roll == 0x6CU
            && division.divisor == 0x07U
            && division.quotient == 0x1AU
            && division.remainder == 0U
            && division.subtraction_count == 3U,
        "native E6DC divides the byte after its preliminary roll");

    work_ram[0x002FU] = 0U;
    work_ram[0x0030U] = 0x12U;
    check(
        dc_divide_unsigned_byte(
            work_ram.data(),
            work_ram.size(),
            1U,
            &division)
                != 0
            && division.dividend_after_initial_roll == 0x25U
            && division.quotient == 0xFFU
            && division.remainder == 0x12U
            && division.subtraction_count
                == DC_UNSIGNED_BYTE_DIVIDE_STEP_COUNT,
        "native E6DC preserves the zero-divisor loop semantics");

    check(
        dc_divide_unsigned_byte(
            work_ram.data(),
            work_ram.size() - 1U,
            0U,
            &division)
            == 0,
        "native E6DC rejects a truncated work-RAM snapshot");
}

void test_native_unsigned_24_division()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    work_ram[0x002DU] = 0x07U;
    work_ram[0x002EU] = 0x56U;
    work_ram[0x002FU] = 0x34U;
    work_ram[0x0030U] = 0x12U;

    dc_unsigned_24_division division{};
    check(
        dc_divide_unsigned_24(
            work_ram.data(),
            work_ram.size(),
            0U,
            &division)
                != 0
            && division.dividend_input == 0x123456U
            && division.dividend_after_initial_roll == 0x2468ACU
            && division.divisor == 0x07U
            && division.quotient == 0x0299C3U
            && division.remainder == 0x01U,
        "native E7D7 divides the 24-bit dividend after its preliminary roll");

    work_ram[0x002DU] = 0U;
    check(
        dc_divide_unsigned_24(
            work_ram.data(),
            work_ram.size(),
            1U,
            &division)
                != 0
            && division.dividend_after_initial_roll == 0x2468ADU
            && division.quotient == 0xFFFFFFU
            && division.remainder == 0x56U
            && division.subtraction_count
                == DC_UNSIGNED_24_DIVIDE_STEP_COUNT,
        "native E7D7 preserves the zero-divisor loop semantics");

    check(
        dc_divide_unsigned_24(
            work_ram.data(),
            work_ram.size() - 1U,
            0U,
            &division)
            == 0,
        "native E7D7 rejects a truncated work-RAM snapshot");
}

void test_native_unsigned_fixed_division()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    work_ram[0x002DU] = 0x02U;
    work_ram[0x002EU] = 0x00U;
    work_ram[0x002FU] = 0x03U;
    work_ram[0x0030U] = 0x00U;

    dc_unsigned_fixed_division division{};
    check(
        dc_divide_unsigned_fixed(
            work_ram.data(), work_ram.size(), &division)
            != 0
            && division.dividend_input == 0x0003U
            && division.divisor == 0x0002U
            && division.first_quotient == 0x0001U
            && division.first_remainder == 0x0001U
            && division.fractional_quotient == 0x0080U
            && division.fractional_remainder == 0U
            && division.result == 0x0180U,
        "native E728 retains the integer and fractional quotient bytes");

    work_ram[0x002DU] = 0U;
    work_ram[0x002EU] = 0U;
    work_ram[0x002FU] = 0x34U;
    work_ram[0x0030U] = 0x12U;
    check(
        dc_divide_unsigned_fixed(
            work_ram.data(), work_ram.size(), &division)
            != 0
            && division.first_quotient == 0xFFFFU
            && division.first_remainder == 0x1234U
            && division.fractional_quotient == 0xFFFFU
            && division.result == 0xFFFFU,
        "native E728 preserves the zero-divisor restoring-loop result");

    check(
        dc_divide_unsigned_fixed(
            work_ram.data(), work_ram.size() - 1U, &division)
            == 0,
        "native E728 rejects a truncated work-RAM snapshot");
}

void test_native_runtime_collision_response()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    constexpr std::uint8_t slot = 0x62U;
    work_ram[0x1A80U + slot] = 0x0FU;
    work_ram[0x1A00U + slot] = 4U;
    work_ram[0x1C80U + slot] = 0U;

    dc_collision_response response{};
    check(
        dc_advance_collision_response(
            work_ram.data(),
            work_ram.size(),
            slot,
            &response)
            != 0,
        "native 7FB1 accepts a complete work-RAM snapshot");
    check(
        response.event == 0x0FU
            && response.response_base == 0x1CU
            && response.response == 0x1CU
            && response.countdown == 3U
            && response.phase == 0U
            && response.countdown_reloaded == 0U,
        "native 7FB1 advances the bounded collision response");
    check(
        response.response_work_ram_offset == 0x19E2U
            && response.countdown_work_ram_offset == 0x1A62U
            && response.phase_work_ram_offset == 0x1CE2U,
        "native 7FB1 indexes the actor response fields");

    work_ram[0x1A80U + slot] = 0x81U;
    work_ram[0x1A00U + slot] = 1U;
    work_ram[0x1C80U + slot] = 0xFFU;
    check(
        dc_advance_collision_response(
            work_ram.data(),
            work_ram.size(),
            slot,
            &response)
                != 0
            && response.response_base == 0U
            && response.response == 3U
            && response.countdown == 4U
            && response.phase == 0U
            && response.countdown_reloaded == 1U,
        "native 7FB1 reloads the countdown and wraps the phase");

    work_ram[0x1A80U + slot] = 0x0FU;
    work_ram[0x1A00U + slot] = 1U;
    work_ram[0x1C80U + slot] = 0U;
    check(
        dc_advance_collision_response(
            work_ram.data(),
            work_ram.size(),
            slot,
            &response)
                != 0
            && response.response_base == 0x1CU
            && response.response == 0x1CU
            && response.countdown == 4U
            && response.phase == 1U
            && response.countdown_reloaded == 1U,
        "native 7FB1 matches the bounded countdown-reload call");
    check(
        dc_advance_collision_response(
            work_ram.data(),
            work_ram.size() - 1U,
            slot,
            &response)
            == 0,
        "native 7FB1 rejects a truncated work-RAM snapshot");
}

void test_native_collision_scan_vertical_reject()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    constexpr std::uint8_t slot = 0x17U;
    dc_collision_scan_vertical_reject reject{};

    work_ram[0x0066U] = 0x20U;
    work_ram[0x0067U] = 0x03U;
    work_ram[0x1300U + slot] = 0x10U;
    work_ram[0x1380U + slot] = 0x01U;
    check(
        dc_classify_collision_scan_vertical_reject(
            work_ram.data(),
            work_ram.size(),
            slot,
            &reject)
                != 0
            && reject.branch
                == DC_COLLISION_SCAN_VERTICAL_POSITIVE_PAGE
            && reject.difference_low == 0x10U
            && reject.difference_high == 0x02U,
        "native CC1A classifies a positive-page vertical rejection");

    work_ram[0x0066U] = 0xF0U;
    work_ram[0x0067U] = 0x01U;
    work_ram[0x1300U + slot] = 0x20U;
    work_ram[0x1380U + slot] = 0x01U;
    check(
        dc_classify_collision_scan_vertical_reject(
            work_ram.data(),
            work_ram.size(),
            slot,
            &reject)
                != 0
            && reject.branch
                == DC_COLLISION_SCAN_VERTICAL_SAME_PAGE_LOW
            && reject.difference_low == 0xD0U
            && reject.difference_high == 0U,
        "native CC1A classifies a same-page low-byte rejection");

    work_ram[0x0066U] = 0x10U;
    work_ram[0x0067U] = 0x01U;
    work_ram[0x1300U + slot] = 0x20U;
    work_ram[0x1380U + slot] = 0x03U;
    check(
        dc_classify_collision_scan_vertical_reject(
            work_ram.data(),
            work_ram.size(),
            slot,
            &reject)
                != 0
            && reject.branch
                == DC_COLLISION_SCAN_VERTICAL_NEGATIVE_PAGE
            && reject.magnitude_low == 0x10U
            && reject.magnitude_high == 0x02U,
        "native CC1A classifies a negative-page vertical rejection");

    work_ram[0x0066U] = 0x20U;
    work_ram[0x0067U] = 0x02U;
    work_ram[0x1300U + slot] = 0xF0U;
    work_ram[0x1380U + slot] = 0x02U;
    check(
        dc_classify_collision_scan_vertical_reject(
            work_ram.data(),
            work_ram.size(),
            slot,
            &reject)
                != 0
            && reject.branch
                == DC_COLLISION_SCAN_VERTICAL_NEGATIVE_LOW
            && reject.magnitude_low == 0xD0U
            && reject.magnitude_high == 0U,
        "native CC1A classifies a negative low-byte rejection");

    work_ram[0x0066U] = 0x30U;
    work_ram[0x0067U] = 0x02U;
    work_ram[0x1300U + slot] = 0x20U;
    work_ram[0x1380U + slot] = 0x02U;
    check(
        dc_classify_collision_scan_vertical_reject(
            work_ram.data(),
            work_ram.size(),
            slot,
            &reject)
                != 0
            && reject.branch == DC_COLLISION_SCAN_VERTICAL_DEFER,
        "native CC1A defers a nearby vertical coordinate");
    check(
        dc_classify_collision_scan_vertical_reject(
            work_ram.data(),
            work_ram.size() - 1U,
            slot,
            &reject)
            == 0,
        "native CC1A rejects a truncated work-RAM snapshot");
}

void test_native_collision_geometry()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    constexpr std::uint8_t slot = 0x15U;
    dc_collision_geometry_tables tables{
        .first_directory_low = 0x70U,
        .first_directory_high = 0x87U,
        .second_directory_low = 0x7EU,
        .second_directory_high = 0x87U,
        .first_geometry_low = 0x8CU,
        .first_geometry_high = 0x87U,
        .second_geometry_low = 0xC0U,
        .second_geometry_high = 0x87U,
        .horizontal_offset = 0xF8U,
        .vertical_offset = 0xD4U,
        .horizontal_limit = 0x30U,
        .vertical_limit = 0x40U,
    };
    dc_collision_geometry geometry{};

    work_ram[0x0066U] = 0x34U;
    work_ram[0x0067U] = 0x02U;
    work_ram[0x1300U + slot] = 0x20U;
    work_ram[0x1380U + slot] = 0x02U;
    work_ram[0x1180U + slot] = 0x4AU;
    work_ram[0x1200U + slot] = 0x00U;
    work_ram[0x1900U + slot] = 0x06U;
    work_ram[0x1980U + slot] = 0x03U;

    check(
        dc_prepare_collision_geometry(
            work_ram.data(),
            work_ram.size(),
            slot,
            &tables,
            &geometry)
                != 0
            && geometry.vertical.branch
                == DC_COLLISION_SCAN_VERTICAL_DEFER
            && geometry.directory_index == 0x06U
            && geometry.phase_index == 0x06U
            && geometry.position_low == 0x2AU
            && geometry.position_high == 0U
            && geometry.adjusted_position_low == 0x22U
            && geometry.collision_low == 0xF4U
            && geometry.collision_high == 0x01U
            && geometry.vertical_offset_negative != 0U
            && geometry.position_rejected == 0U,
        "native CCD5 prepares the negative-offset geometry path");

    tables.vertical_offset = 0x30U;
    check(
        dc_prepare_collision_geometry(
            work_ram.data(),
            work_ram.size(),
            slot,
            &tables,
            &geometry)
                != 0
            && geometry.collision_low == 0x50U
            && geometry.collision_high == 0x02U
            && geometry.vertical_offset_negative == 0U,
        "native CCD5 prepares the positive-offset geometry path");

    work_ram[0x1200U + slot] = 0x01U;
    check(
        dc_prepare_collision_geometry(
            work_ram.data(),
            work_ram.size(),
            slot,
            &tables,
            &geometry)
                != 0
            && geometry.position_rejected != 0U
            && geometry.position_high == 0x01U,
        "native CCD5 reports its late position-page rejection");
    check(
        dc_prepare_collision_geometry(
            work_ram.data(),
            work_ram.size() - 1U,
            slot,
            &tables,
            &geometry)
            == 0,
        "native CCD5 rejects a truncated work-RAM snapshot");
}

void test_native_collision_scan_bounds_reject()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_collision_scan_bounds_reject reject{};

    work_ram[0x0065U] = 0x50U;
    work_ram[0x0068U] = 0x20U;
    work_ram[0x006BU] = 0x20U;
    check(
        dc_classify_collision_scan_bounds_reject(
            work_ram.data(),
            work_ram.size(),
            &reject)
                != 0
            && reject.branch
                == DC_COLLISION_SCAN_BOUNDS_HORIZONTAL
            && reject.horizontal_difference == 0x30U,
        "native CC1A classifies a post-geometry horizontal rejection");

    work_ram[0x0065U] = 0x30U;
    work_ram[0x0068U] = 0x20U;
    work_ram[0x006BU] = 0x20U;
    work_ram[0x0066U] = 0x10U;
    work_ram[0x0067U] = 0x02U;
    work_ram[0x0069U] = 0x20U;
    work_ram[0x006AU] = 0x02U;
    work_ram[0x006CU] = 0x40U;
    check(
        dc_classify_collision_scan_bounds_reject(
            work_ram.data(),
            work_ram.size(),
            &reject)
                != 0
            && reject.branch
                == DC_COLLISION_SCAN_BOUNDS_VERTICAL_NEGATIVE
            && reject.vertical_difference_low == 0xF0U
            && reject.vertical_difference_high == 0xFFU,
        "native CC1A classifies a negative post-geometry vertical rejection");

    work_ram[0x0066U] = 0x20U;
    work_ram[0x0067U] = 0x04U;
    work_ram[0x0069U] = 0x10U;
    work_ram[0x006AU] = 0x02U;
    check(
        dc_classify_collision_scan_bounds_reject(
            work_ram.data(),
            work_ram.size(),
            &reject)
                != 0
            && reject.branch
                == DC_COLLISION_SCAN_BOUNDS_VERTICAL_PAGE
            && reject.vertical_difference_high == 0x02U,
        "native CC1A classifies a post-geometry vertical-page rejection");

    work_ram[0x0066U] = 0x70U;
    work_ram[0x0067U] = 0x02U;
    work_ram[0x0069U] = 0x20U;
    work_ram[0x006AU] = 0x02U;
    work_ram[0x006CU] = 0x40U;
    check(
        dc_classify_collision_scan_bounds_reject(
            work_ram.data(),
            work_ram.size(),
            &reject)
                != 0
            && reject.branch
                == DC_COLLISION_SCAN_BOUNDS_VERTICAL_LOW
            && reject.vertical_difference_low == 0x50U,
        "native CC1A classifies a same-page post-geometry vertical rejection");

    work_ram[0x0066U] = 0x50U;
    check(
        dc_classify_collision_scan_bounds_reject(
            work_ram.data(),
            work_ram.size(),
            &reject)
                != 0
            && reject.branch == DC_COLLISION_SCAN_BOUNDS_DEFER
            && reject.vertical_difference_low == 0x30U,
        "native CC1A defers an in-range post-geometry candidate");
    check(
        dc_classify_collision_scan_bounds_reject(
            work_ram.data(),
            work_ram.size() - 1U,
            &reject)
            == 0,
        "native CC1A post-geometry classifier rejects truncated work RAM");
}

void test_native_collision_mask_lookup()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_collision_mask_lookup lookup{};

    work_ram[0x002DU] = 0xF8U;
    work_ram[0x002FU] = 0x0CU;
    work_ram[0x005EU] = 0x03U;
    work_ram[0x005FU] = 0x05U;
    work_ram[0x0072U] = 0x00U;
    work_ram[0x0073U] = 0xA0U;
    work_ram[0x0074U] = 0x00U;
    work_ram[0x0075U] = 0xB0U;
    work_ram[0x008EU] = 0xF8U;
    work_ram[0x008FU] = 0xC0U;

    check(
        dc_resolve_collision_mask_lookup(
            work_ram.data(),
            work_ram.size(),
            0x2AU,
            0x07U,
            0x28U,
            0x08U,
            &lookup)
                != 0
            && lookup.first_table_pointer == 0xB000U
            && lookup.second_table_pointer == 0xA000U
            && lookup.mask_base_pointer == 0xC0F8U
            && lookup.table_index == 0x04U
            && lookup.first_table_value == 0x2AU
            && lookup.second_table_value == 0x07U
            && lookup.mask_pointer == 0xC130U
            && lookup.mask_row == 0x05U
            && lookup.mask_column == 0x03U
            && lookup.mask_value == 0x28U
            && lookup.column_bit == 0x08U
            && lookup.collision != 0U,
        "native CC98 resolves a wrapping table index and collision mask");

    check(
        dc_resolve_collision_mask_lookup(
            work_ram.data(),
            work_ram.size(),
            0x2AU,
            0x07U,
            0x20U,
            0x08U,
            &lookup)
                != 0
            && lookup.collision == 0U,
        "native CC98 resolves a clear collision mask");
    check(
        dc_resolve_collision_mask_lookup(
            work_ram.data(),
            work_ram.size() - 1U,
            0U,
            0U,
            0U,
            0U,
            &lookup)
            == 0,
        "native CC98 rejects a truncated work-RAM snapshot");
}

void test_native_collision_mask_coordinates()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_collision_mask_coordinates coordinates{};

    work_ram[0x002DU] = 0xA7U;
    work_ram[0x002FU] = 0x5EU;
    work_ram[0x006BU] = 0x39U;
    work_ram[0x00B7U] = 0x10U;
    work_ram[0x0254U] = 1U;
    work_ram[0x1984U] = 2U;
    check(
        dc_prepare_collision_mask_coordinates(
            work_ram.data(),
            work_ram.size(),
            4U,
            &coordinates)
                != 0
            && coordinates.side_flags_updated != 0U
            && coordinates.side_flags == 0x90U
            && coordinates.mask_column == 0x07U
            && coordinates.horizontal_cell == 0x14U
            && coordinates.rounded_horizontal_span == 0x08U
            && coordinates.mask_row == 0x06U
            && coordinates.vertical_cell == 0x0BU,
        "native CC1A prepares collision-mask coordinates and even side flag");

    work_ram[0x1985U] = 3U;
    check(
        dc_prepare_collision_mask_coordinates(
            work_ram.data(),
            work_ram.size(),
            5U,
            &coordinates)
                != 0
            && coordinates.side_flags_updated == 0U
            && coordinates.side_flags == 0x10U,
        "native CC1A suppresses side flags for excluded target state");
    work_ram[0x1985U] = 2U;
    check(
        dc_prepare_collision_mask_coordinates(
            work_ram.data(),
            work_ram.size(),
            5U,
            &coordinates)
                != 0
            && coordinates.side_flags_updated != 0U
            && coordinates.side_flags == 0x11U,
        "native CC1A prepares the odd-target side flag");
    check(
        dc_prepare_collision_mask_coordinates(
            work_ram.data(),
            work_ram.size() - 1U,
            5U,
            &coordinates)
            == 0,
        "native CC1A mask-coordinate preparation rejects truncated work RAM");
}

void test_native_score_accumulator()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    constexpr std::uint8_t slot = 0U;
    work_ram[0x0251U] = slot;
    work_ram[0x002FU] = 0x34U;
    work_ram[0x0030U] = 0x12U;
    work_ram[0x0205U + slot] = 0xF0U;
    work_ram[0x020CU + slot] = 0x00U;
    work_ram[0x0213U + slot] = 0x01U;
    work_ram[0x0200U] = 0xFFU;
    work_ram[0x0207U] = 0xFFU;
    work_ram[0x020EU] = 0x01U;

    dc_score_update update{};
    check(
        dc_accumulate_score_ordinary(
            work_ram.data(),
            work_ram.size(),
            &update)
            != 0,
        "native F55C accepts an ordinary score update");
    check(
        update.slot == slot && update.award == 0x1234U
            && update.score == 0x011324U
            && update.score_low == 0x24U
            && update.score_middle == 0x13U
            && update.score_high == 0x01U
            && update.dirty_flags == 0x01U
            && update.comparison_score == 0x01FFFFU,
        "native F55C adds a 16-bit award to the selected 24-bit score");

    work_ram[0x0CC7U] = 1U;
    check(
        dc_accumulate_score_ordinary(
            work_ram.data(),
            work_ram.size(),
            &update)
            == 0,
        "native F55C defers the gated score path");
    work_ram[0x0CC7U] = 0U;
    work_ram[0x0205U + slot] = 0x70U;
    work_ram[0x020CU + slot] = 0x96U;
    work_ram[0x0213U + slot] = 0x98U;
    check(
        dc_accumulate_score_ordinary(
            work_ram.data(),
            work_ram.size(),
            &update)
            == 0,
        "native F55C defers score saturation");
    check(
        dc_accumulate_score_ordinary(
            work_ram.data(),
            work_ram.size() - 1U,
            &update)
            == 0,
        "native F55C rejects a truncated work-RAM snapshot");
}

void test_native_rng_update()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    work_ram[0x0000U] = 0xCBU;
    work_ram[0x022AU] = 0xCBU;
    work_ram[0x022BU] = 0x04U;

    dc_rng_update update{};
    check(
        dc_advance_rng(work_ram.data(), work_ram.size(), &update) != 0,
        "native E8B3 accepts a complete work-RAM snapshot");
    check(
        update.constant_sum_low == 0x0AU
            && update.constant_sum_high == 0x55U
            && update.doubled_low == 0x14U
            && update.doubled_high == 0xAAU
            && update.frame_sum_low == 0x96U
            && update.mixed_high == 0xAFU
            && update.seed_low == 0x96U
            && update.seed_high == 0x7AU
            && update.final_carry == 1U
            && update.final_overflow == 1U,
        "native E8B3 matches the first routed RNG recurrence");
    check(
        work_ram[0x022AU] == 0xCBU && work_ram[0x022BU] == 0x04U,
        "native E8B3 leaves source work RAM unchanged");
    check(
        dc_advance_rng(
            work_ram.data(),
            work_ram.size() - 1U,
            &update)
            == 0,
        "native E8B3 rejects a truncated work-RAM snapshot");
}

void test_native_signed_word_negation()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_signed_word_negation negation{};

    work_ram[0x002FU] = 0x01U;
    work_ram[0x0030U] = 0x00U;
    check(
        dc_negate_signed_word(
            work_ram.data(),
            work_ram.size(),
            &negation)
            != 0
            && negation.input == 0x0001U
            && negation.result == 0xFFFFU
            && negation.complemented_high == 0xFFU
            && negation.increments_high == 0U,
        "native E837 negates without a high-byte increment");

    work_ram[0x002FU] = 0x00U;
    work_ram[0x0030U] = 0x80U;
    check(
        dc_negate_signed_word(
            work_ram.data(),
            work_ram.size(),
            &negation)
            != 0
            && negation.input == 0x8000U
            && negation.result == 0x8000U
            && negation.complemented_high == 0x7FU
            && negation.increments_high != 0U,
        "native E837 carries the low-byte wrap into the high byte");
    check(
        work_ram[0x002FU] == 0x00U
            && work_ram[0x0030U] == 0x80U,
        "native E837 leaves source work RAM unchanged");
    check(
        dc_negate_signed_word(
            work_ram.data(),
            work_ram.size() - 1U,
            &negation)
            == 0,
        "native E837 rejects a truncated work-RAM snapshot");
}

void test_native_unsigned_byte_multiply()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_unsigned_byte_multiply multiply{};

    work_ram[0x002FU] = 0x38U;
    work_ram[0x0030U] = 0x17U;
    check(
        dc_multiply_unsigned_bytes(
            work_ram.data(),
            work_ram.size(),
            &multiply)
                != 0
            && multiply.multiplicand == 0x38U
            && multiply.multiplier == 0x17U
            && multiply.result_low == 0x08U
            && multiply.result_high == 0x05U
            && multiply.add_count == 4U
            && multiply.steps[0].adds_multiplicand == 0U
            && multiply.steps[3].adds_multiplicand != 0U
            && multiply.steps[7].low_after_add == 0x08U,
        "native E569 multiplies unsigned scratch bytes");

    work_ram[0x002FU] = 0xFFU;
    work_ram[0x0030U] = 0xFFU;
    check(
        dc_multiply_unsigned_bytes(
            work_ram.data(),
            work_ram.size(),
            &multiply)
                != 0
            && multiply.result_low == 0x01U
            && multiply.result_high == 0xFEU
            && multiply.add_count == 8U
            && multiply.carry_count != 0U,
        "native E569 preserves high-byte carries");
    check(
        dc_multiply_unsigned_bytes(
            work_ram.data(),
            work_ram.size() - 1U,
            &multiply)
            == 0,
        "native E569 rejects truncated work RAM");
}

void test_native_unsigned_multiply_round()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    work_ram[0x002DU] = 0x34U;
    work_ram[0x002EU] = 0x12U;
    work_ram[0x0030U] = 0x81U;
    work_ram[0x0042U] = 0x02U;
    work_ram[0x0043U] = 0x01U;

    dc_unsigned_multiply_round round{};
    check(
        dc_multiply_unsigned_round(
            work_ram.data(),
            work_ram.size(),
            &round)
            != 0
            && round.accumulator == 0x0102U
            && round.multiplicand == 0x1234U
            && round.multiplier == 0x81U
            && round.add_count == 2U,
        "native E624 derives the eight-round multiply inputs");

    std::uint32_t expected =
        static_cast<std::uint32_t>(round.accumulator)
        | (static_cast<std::uint32_t>(round.multiplier) << 16U);
    for (std::size_t index = 0U;
         index < DC_UNSIGNED_MULTIPLY_STEP_COUNT;
         ++index) {
        const bool adds = (expected & 0x800000U) != 0U;
        expected = (expected << 1U) & 0xFFFFFFU;
        if (adds) {
            const auto sum =
                (expected & 0xFFFFU) + round.multiplicand;
            expected = (expected & 0xFF0000U)
                | (sum & 0xFFFFU);
            if (sum > 0xFFFFU) {
                expected =
                    (expected + 0x10000U) & 0xFFFFFFU;
            }
        }
        const auto& step = round.steps[index];
        check(
            step.adds_multiplicand == static_cast<std::uint8_t>(adds)
                && step.low_after_add
                    == static_cast<std::uint8_t>(expected)
                && step.high_after_add
                    == static_cast<std::uint8_t>(expected >> 8U)
                && step.multiplier_after_add
                    == static_cast<std::uint8_t>(expected >> 16U),
            "native E624 exposes each exact shift/add result");
    }
    check(
        round.result == static_cast<std::uint16_t>(expected)
            && round.multiplier_after
                == static_cast<std::uint8_t>(expected >> 16U),
        "native E624 returns the complete 24-bit round result");
    check(
        dc_multiply_unsigned_round(
            work_ram.data(),
            work_ram.size() - 1U,
            &round)
            == 0,
        "native E624 rejects a truncated work-RAM snapshot");
}

void test_native_signed_fixed_multiply()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    work_ram[0x002DU] = 0x00U;
    work_ram[0x002EU] = 0x02U;
    work_ram[0x002FU] = 0x80U;
    work_ram[0x0030U] = 0x01U;

    dc_signed_fixed_multiply multiply{};
    check(
        dc_multiply_signed_fixed(
            work_ram.data(),
            work_ram.size(),
            &multiply)
            != 0
            && multiply.multiplicand_input == 0x0200U
            && multiply.multiplier_input == 0x0180U
            && multiply.runs_high_round != 0U
            && multiply.high_round.multiplier == 0x01U
            && multiply.low_round.multiplier == 0x80U
            && multiply.magnitude == 0x0300U
            && multiply.result == 0x0300U,
        "native E65C composes both unsigned fixed-point rounds");

    work_ram[0x002DU] = 0x00U;
    work_ram[0x002EU] = 0xFEU;
    work_ram[0x002FU] = 0x80U;
    work_ram[0x0030U] = 0x00U;
    check(
        dc_multiply_signed_fixed(
            work_ram.data(),
            work_ram.size(),
            &multiply)
            != 0
            && multiply.negates_multiplicand != 0U
            && multiply.negates_multiplier == 0U
            && multiply.runs_high_round == 0U
            && multiply.multiplicand_magnitude == 0x0200U
            && multiply.multiplier_magnitude == 0x0080U
            && multiply.magnitude == 0x0100U
            && multiply.result == 0xFF00U,
        "native E65C normalizes and restores a negative result");
    check(
        dc_multiply_signed_fixed(
            work_ram.data(),
            work_ram.size() - 1U,
            &multiply)
            == 0,
        "native E65C rejects a truncated work-RAM snapshot");
}

void test_native_signed_word_multiply()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    work_ram[0x002DU] = 0x00U;
    work_ram[0x002EU] = 0xFEU;
    work_ram[0x002FU] = 0x80U;
    work_ram[0x0030U] = 0x01U;

    dc_signed_word_multiply multiply{};
    check(
        dc_multiply_signed_words(work_ram.data(), work_ram.size(), &multiply)
            != 0
            && multiply.negates_multiplicand != 0U
            && multiply.negates_multiplier == 0U
            && multiply.runs_high_round != 0U
            && multiply.multiplicand_magnitude == 0x0200U
            && multiply.multiplier_magnitude == 0x0180U
            && multiply.magnitude == 0x0000U
            && multiply.result == 0x0000U,
        "native E5EC composes signed 16-bit multiplication rounds");

    work_ram[0x002DU] = 0x02U;
    work_ram[0x002EU] = 0x00U;
    work_ram[0x002FU] = 0xFEU;
    work_ram[0x0030U] = 0xFFU;
    check(
        dc_multiply_signed_words(work_ram.data(), work_ram.size(), &multiply)
            != 0
            && multiply.negates_multiplicand == 0U
            && multiply.negates_multiplier != 0U
            && multiply.magnitude == 0x0004U
            && multiply.result == 0xFFFCU,
        "native E5EC restores the signed word product");
    check(
        dc_multiply_signed_words(
            work_ram.data(), work_ram.size() - 1U, &multiply)
            == 0,
        "native E5EC rejects a truncated work-RAM snapshot");
}

void test_native_flipper_projection()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    constexpr std::uint8_t slot = 0x05U;
    work_ram[0x1400U + slot] = 0x00U;
    work_ram[0x1480U + slot] = 0x02U;
    work_ram[0x1500U + slot] = 0x00U;
    work_ram[0x1580U + slot] = 0x01U;
    work_ram[0x007DU] = 0x00U;
    work_ram[0x007EU] = 0x01U;
    work_ram[0x007FU] = 0x80U;
    work_ram[0x0080U] = 0x00U;

    dc_flipper_projection projection{};
    check(
        dc_project_flipper_vectors(
            work_ram.data(),
            work_ram.size(),
            slot,
            &projection)
            != 0
            && projection.first_input == 0x0200U
            && projection.second_input == 0x0100U
            && projection.first_direct_product == 0x0200U
            && projection.second_cross_product == 0x0080U
            && projection.first_cross_product == 0x0100U
            && projection.second_direct_product == 0x0100U
            && projection.first_output == 0x0280U
            && projection.second_output == 0U,
        "native D09E composes all four signed fixed-point products");
    check(
        dc_project_flipper_vectors(
            work_ram.data(),
            work_ram.size(),
            0x80U,
            &projection)
            == 0,
        "native D09E rejects a slot outside the physics arrays");
    check(
        dc_project_flipper_vectors(
            work_ram.data(),
            work_ram.size() - 1U,
            slot,
            &projection)
            == 0,
        "native D09E rejects a truncated work-RAM snapshot");
}

void test_native_flipper_input_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    constexpr std::uint8_t slot = 0x09U;
    work_ram[0x0CEEU] = 0x81U;
    work_ram[0x1B80U + slot] = 0x01U;

    dc_flipper_input_gate gate{};
    check(
        dc_evaluate_flipper_input_gate(
            work_ram.data(),
            work_ram.size(),
            slot,
            &gate)
            != 0,
        "native D466 accepts a complete work-RAM snapshot");
    check(
        gate.global_input == 0x81U
            && gate.piece_mask == 0x01U
            && gate.matched_input == 0x01U,
        "native D466 intersects global input with the piece mask");

    work_ram[0x0CEEU] = 0x02U;
    check(
        dc_evaluate_flipper_input_gate(
            work_ram.data(),
            work_ram.size(),
            slot,
            &gate)
            != 0
            && gate.matched_input == 0U,
        "native D466 identifies the no-input early return");

    work_ram[0x0CEEU] = 0x01U;
    work_ram[0x00BDU] = 0U;
    work_ram[0x1D80U + slot] = 0U;
    check(
        dc_evaluate_flipper_input_gate(
            work_ram.data(),
            work_ram.size(),
            slot,
            &gate)
            != 0,
        "native D466 refreshes activation input");
    dc_flipper_activation activation{};
    check(
        dc_prepare_flipper_activation(
            work_ram.data(),
            work_ram.size(),
            slot,
            &gate,
            &activation)
            != 0
            && activation.supported != 0U
            && activation.dispatch_low == 0x8EU
            && activation.dispatch_high == 0xD4U
            && activation.compare_carry != 0U,
        "native D466 identifies the common coroutine activation");
    work_ram[0x1D80U + slot] = 1U;
    check(
        dc_prepare_flipper_activation(
            work_ram.data(),
            work_ram.size(),
            slot,
            &gate,
            &activation)
            != 0
            && activation.supported == 0U,
        "native D466 defers a nonzero coroutine countdown");
    check(
        dc_evaluate_flipper_input_gate(
            work_ram.data(),
            work_ram.size() - 1U,
            slot,
            &gate)
            == 0,
        "native D466 rejects a truncated work-RAM snapshot");
}

void test_native_actor_camera_activation()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    constexpr std::uint8_t slot = 0x2AU;
    work_ram[0x00A9U] = 0x00U;
    work_ram[0x00AAU] = 0x02U;
    work_ram[0x1080U + slot] = 0xA5U;

    dc_actor_camera_activation activation{};
    check(
        dc_classify_actor_camera_activation(
            work_ram.data(),
            work_ram.size(),
            slot,
            &activation)
            != 0
            && activation.path == DC_ACTOR_CAMERA_EMPTY
            && activation.flags_after == 0xA5U
            && activation.lower_low == 0xD0U
            && activation.lower_high == 0x01U
            && activation.upper_low == 0xF0U
            && activation.upper_high == 0x02U,
        "native F4FC preserves an empty actor slot");

    work_ram[0x1000U + slot] = 0x82U;
    work_ram[0x1300U + slot] = 0xCFU;
    work_ram[0x1380U + slot] = 0x01U;
    check(
        dc_classify_actor_camera_activation(
            work_ram.data(),
            work_ram.size(),
            slot,
            &activation)
            != 0
            && activation.path == DC_ACTOR_CAMERA_BELOW
            && activation.flags_after == 0x25U,
        "native F4FC clears the flag below the camera window");

    work_ram[0x1300U + slot] = 0xD0U;
    check(
        dc_classify_actor_camera_activation(
            work_ram.data(),
            work_ram.size(),
            slot,
            &activation)
            != 0
            && activation.path == DC_ACTOR_CAMERA_INSIDE
            && activation.flags_after == 0xA5U,
        "native F4FC includes the lower camera-window boundary");

    work_ram[0x1300U + slot] = 0xF0U;
    work_ram[0x1380U + slot] = 0x02U;
    check(
        dc_classify_actor_camera_activation(
            work_ram.data(),
            work_ram.size(),
            slot,
            &activation)
            != 0
            && activation.path == DC_ACTOR_CAMERA_ABOVE
            && activation.flags_after == 0x25U,
        "native F4FC excludes the upper camera-window boundary");
    check(
        dc_classify_actor_camera_activation(
            work_ram.data(),
            work_ram.size() - 1U,
            slot,
            &activation)
            == 0,
        "native F4FC rejects a truncated work-RAM view");
}

void test_native_actor_slot_reset()
{
    dc_actor_slot_reset reset{};
    check(
        dc_prepare_actor_slot_reset(0x62U, &reset) != 0
            && reset.slot == 0x62U
            && reset.work_ram_offsets[0] == 0x1062U
            && reset.work_ram_offsets[1] == 0x10E2U
            && reset.work_ram_offsets[31] == 0x1FE2U,
        "native C574 maps all 32 parallel actor fields");
    check(
        dc_prepare_actor_slot_reset(0x80U, &reset) == 0,
        "native C574 rejects a slot outside the actor array");
}

void test_native_actor_slot_allocator()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_actor_slot_allocation allocation{};

    for (std::size_t slot = 0x04U; slot < 0x62U; ++slot) {
        work_ram[0x1000U + slot] = 0x80U;
    }
    check(
        dc_classify_actor_slot_allocation(
            work_ram.data(), work_ram.size(), &allocation)
                != 0
            && allocation.path == DC_ACTOR_SLOT_ALLOCATION_RESERVED
            && allocation.slot == 0x62U
            && allocation.occupied_slots == 0x5EU
            && allocation.mode == 0U
            && allocation.reservation == 0x80U,
        "native C5AB selects the first free generic actor slot");

    work_ram[0x0055U] = 1U;
    check(
        dc_classify_actor_slot_allocation(
            work_ram.data(), work_ram.size(), &allocation)
                != 0
            && allocation.reservation == 0xC0U,
        "native C5AB preserves the alternate reservation mode");

    for (std::size_t slot = 0x62U; slot < 0x7CU; ++slot) {
        work_ram[0x1000U + slot] = 0x80U;
    }
    check(
        dc_classify_actor_slot_allocation(
            work_ram.data(), work_ram.size(), &allocation)
                != 0
            && allocation.path == DC_ACTOR_SLOT_ALLOCATION_EXHAUSTED
            && allocation.slot == 0x7CU
            && allocation.occupied_slots == 0x78U,
        "native C5AB reports an exhausted generic actor pool");
    check(
        dc_classify_actor_slot_allocation(
            work_ram.data(), 0x107BU, &allocation)
                == 0,
        "native C5AB rejects a truncated work-RAM view");
}

void test_native_giant_skeleton_eyeball_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_giant_skeleton_eyeball_gate gate{};

    check(
        dc_classify_giant_skeleton_eyeball_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
            != 0
            && gate.path == DC_GIANT_SKELETON_EYEBALL_DORMANT
            && gate.egg_completion_bit == 0U
            && gate.final_animation_bit == 0U,
        "native 4DD9 identifies the dormant return");

    work_ram[0x0D20U] = 0x81U;
    work_ram[0x0053U] = 0x02U;
    check(
        dc_classify_giant_skeleton_eyeball_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
            != 0
            && gate.path == DC_GIANT_SKELETON_EYEBALL_ACTIVATE
            && gate.egg_completion_bit == 1U,
        "native 4DD9 gives the egg-completion latch priority");

    work_ram[0x0D20U] = 0x80U;
    check(
        dc_classify_giant_skeleton_eyeball_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
            != 0
            && gate.path
                == DC_GIANT_SKELETON_EYEBALL_FINAL_ANIMATION
            && gate.final_animation_bit == 0x02U,
        "native 4DD9 identifies the final-animation branch");

    check(
        dc_classify_giant_skeleton_eyeball_gate(
            work_ram.data(),
            0x0D20U,
            &gate)
            == 0,
        "native 4DD9 rejects a truncated work-RAM snapshot");
}

void test_native_giant_skeleton_eyeball_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_giant_skeleton_eyeball_initializer initializer{};

    work_ram[0x10AFU] = 0xCBU;
    check(
        dc_classify_giant_skeleton_eyeball_initializer(
            work_ram.data(), work_ram.size(), 0x2FU, &initializer)
            != 0
            && initializer.slot == 0x2FU
            && initializer.flags_before == 0xCBU
            && initializer.flags_after == 0x8BU,
        "native 4DC8 clears the Eyeball transient flag");
    check(
        dc_classify_giant_skeleton_eyeball_initializer(
            work_ram.data(), work_ram.size(), 0x30U, &initializer)
            == 0,
        "native 4DC8 rejects a non-Eyeball slot");
    check(
        dc_classify_giant_skeleton_eyeball_initializer(
            work_ram.data(), 0x10AFU, 0x2FU, &initializer)
            == 0,
        "native 4DC8 rejects a truncated work-RAM snapshot");
}

void test_native_giant_skeleton_eyeball_opening_wait()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_giant_skeleton_eyeball_opening_wait wait{};

    work_ram[0x1A2FU] = 0x08U;
    work_ram[0x1E2FU] = 0x02U;
    check(
        dc_classify_giant_skeleton_eyeball_opening_wait(
            work_ram.data(), work_ram.size(), 0x2FU, &wait)
            != 0
            && wait.timer == 0x08U && wait.decremented_timer == 0x07U,
        "native 4DF7 identifies the Eyeball opening timer wait");
    work_ram[0x1A2FU] = 0x01U;
    check(
        dc_classify_giant_skeleton_eyeball_opening_wait(
            work_ram.data(), work_ram.size(), 0x2FU, &wait)
            == 0,
        "native 4DF7 defers the Eyeball opening expiry");
    work_ram[0x1A2FU] = 0x08U;
    work_ram[0x1E2FU] = 0x01U;
    check(
        dc_classify_giant_skeleton_eyeball_opening_wait(
            work_ram.data(), work_ram.size(), 0x2FU, &wait)
            == 0,
        "native 4DF7 requires the Eyeball opening state");
    work_ram[0x1E2FU] = 0x02U;
    check(
        dc_classify_giant_skeleton_eyeball_opening_wait(
            work_ram.data(), 0x1E2FU, 0x2FU, &wait)
            == 0,
        "native 4DF7 rejects truncated work RAM");
    check(
        dc_classify_giant_skeleton_eyeball_opening_wait(
            work_ram.data(), work_ram.size(), 0x30U, &wait)
            == 0,
        "native 4DF7 rejects another actor slot");
}

void test_native_giant_skeleton_eyeball_steady_idle()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_giant_skeleton_eyeball_steady_idle idle{};

    work_ram[0x182FU] = 0x7FU;
    check(
        dc_classify_giant_skeleton_eyeball_steady_idle(
            work_ram.data(), work_ram.size(), 0x2FU, &idle)
            != 0
            && idle.slot == 0x2FU && idle.collision_state == 0x7FU
            && idle.session_flags == 0U,
        "native 4E24 identifies the Eyeball steady idle return");
    work_ram[0x182FU] = 0x80U;
    check(
        dc_classify_giant_skeleton_eyeball_steady_idle(
            work_ram.data(), work_ram.size(), 0x2FU, &idle)
            == 0,
        "native 4E24 defers a signed Eyeball collision state");
    work_ram[0x182FU] = 0U;
    work_ram[0x0053U] = 0x02U;
    check(
        dc_classify_giant_skeleton_eyeball_steady_idle(
            work_ram.data(), work_ram.size(), 0x2FU, &idle)
            == 0,
        "native 4E24 defers final Eyeball animation");
    work_ram[0x0053U] = 0U;
    check(
        dc_classify_giant_skeleton_eyeball_steady_idle(
            work_ram.data(), 0x182FU, 0x2FU, &idle)
            == 0,
        "native 4E24 rejects truncated work RAM");
    check(
        dc_classify_giant_skeleton_eyeball_steady_idle(
            work_ram.data(), work_ram.size(), 0x30U, &idle)
            == 0,
        "native 4E24 rejects another actor slot");
}

void test_native_giant_skeleton_eyeball_final_wait()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_giant_skeleton_eyeball_final_wait wait{};

    work_ram[0x1A2FU] = 0x08U;
    work_ram[0x1B2FU] = 0x40U;
    work_ram[0x1BAFU] = 0x80U;
    check(
        dc_classify_giant_skeleton_eyeball_final_wait(
            work_ram.data(), work_ram.size(), 0x2FU, &wait)
            != 0
            && wait.decremented_timer == 0x07U
            && wait.decremented_phase == 0x3FU
            && wait.final_accumulator == 0xBFU,
        "native 4F3D identifies the Eyeball final animation wait");
    work_ram[0x1A2FU] = 0x01U;
    check(
        dc_classify_giant_skeleton_eyeball_final_wait(
            work_ram.data(), work_ram.size(), 0x2FU, &wait)
            == 0,
        "native 4F3D defers final-animation timer expiry");
    work_ram[0x1A2FU] = 0x08U;
    work_ram[0x1B2FU] = 0x01U;
    check(
        dc_classify_giant_skeleton_eyeball_final_wait(
            work_ram.data(), work_ram.size(), 0x2FU, &wait)
            == 0,
        "native 4F3D defers final-animation phase transition");
    work_ram[0x1B2FU] = 0x40U;
    check(
        dc_classify_giant_skeleton_eyeball_final_wait(
            work_ram.data(), 0x1BAFU, 0x2FU, &wait)
            == 0,
        "native 4F3D rejects truncated work RAM");
    check(
        dc_classify_giant_skeleton_eyeball_final_wait(
            work_ram.data(), work_ram.size(), 0x30U, &wait)
            == 0,
        "native 4F3D rejects another actor slot");
}

void test_native_gray_blob_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_gray_blob_initializer initializer{};

    work_ram[0x1352U] = 0x32U;
    work_ram[0x13D2U] = 0x01U;
    check(
        dc_classify_gray_blob_initializer(
            work_ram.data(), work_ram.size(), 0x52U, &initializer)
            != 0
            && initializer.slot == 0x52U
            && initializer.position_low == 0x32U
            && initializer.position_high == 0x01U,
        "native 6876 preserves the Gray-Blob layout position");
    check(
        dc_classify_gray_blob_initializer(
            work_ram.data(), work_ram.size(), 0x4FU, &initializer)
            == 0,
        "native 6876 rejects a non-Gray-Blob slot");
    check(
        dc_classify_gray_blob_initializer(
            work_ram.data(), 0x13D2U, 0x52U, &initializer)
            == 0,
        "native 6876 rejects a truncated work-RAM snapshot");
}

void test_native_gray_blob_first_live()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_gray_blob_first_live first_live{};

    work_ram[0x10D2U] = 0x82U;
    work_ram[0x1852U] = 0x13U;
    work_ram[0x1A52U] = 0x01U;
    check(
        dc_classify_gray_blob_first_live(
            work_ram.data(), work_ram.size(), 0x52U, &first_live)
            != 0
            && first_live.slot == 0x52U
            && first_live.flags_after == 0x82U
            && first_live.collision_after == 0x13U,
        "native 68A1 classifies a Gray-Blob first-live prefix");
    work_ram[0x1A52U] = 0x04U;
    check(
        dc_classify_gray_blob_first_live(
            work_ram.data(), work_ram.size(), 0x52U, &first_live)
            == 0,
        "native 68A1 leaves later Gray-Blob live passes in assembly");
    check(
        dc_classify_gray_blob_first_live(
            work_ram.data(), 0x1A52U, 0x52U, &first_live)
            == 0,
        "native 68A1 rejects a truncated work-RAM snapshot");
}

void test_native_gray_blob_live_wait()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_gray_blob_live_wait wait{};

    work_ram[0x1A50U] = 0x03U;
    work_ram[0x1850U] = 0x13U;
    work_ram[0x1BD0U] = 0x16U;
    work_ram[0x1B50U] = 0x07U;
    check(
        dc_classify_gray_blob_live_wait(
            work_ram.data(), work_ram.size(), 0x50U, &wait)
            != 0
            && wait.slot == 0x50U
            && wait.appearance_timer == 0x03U
            && wait.decremented_appearance_timer == 0x02U
            && wait.collision_state == 0x13U
            && wait.movement_timer == 0x16U
            && wait.decremented_movement_timer == 0x15U
            && wait.phase_timer == 0x07U
            && wait.decremented_phase_timer == 0x06U,
        "native 68BF classifies a non-expiring Gray-Blob live wait");

    work_ram[0x1A50U] = 0x01U;
    check(
        dc_classify_gray_blob_live_wait(
            work_ram.data(), work_ram.size(), 0x50U, &wait)
            == 0,
        "native 68BF leaves the appearance-expiry arm in assembly");
    work_ram[0x1A50U] = 0x03U;
    work_ram[0x1BD0U] = 0x01U;
    check(
        dc_classify_gray_blob_live_wait(
            work_ram.data(), work_ram.size(), 0x50U, &wait)
            == 0,
        "native 68BF leaves the movement-expiry arm in assembly");
    work_ram[0x1BD0U] = 0x16U;
    work_ram[0x1B50U] = 0x01U;
    check(
        dc_classify_gray_blob_live_wait(
            work_ram.data(), work_ram.size(), 0x50U, &wait)
            == 0,
        "native 68BF leaves the phase-expiry arm in assembly");
}

void test_native_mercy_star_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_mercy_star_initializer initializer{};

    check(
        dc_classify_mercy_star_initializer(
            work_ram.data(), work_ram.size(), 0x5FU, &initializer)
            != 0
            && initializer.slot == 0x5FU,
        "native 4F9F identifies the first Mercy-Star slot");
    check(
        dc_classify_mercy_star_initializer(
            work_ram.data(), work_ram.size(), 0x61U, &initializer)
            != 0
            && initializer.slot == 0x61U,
        "native 4F9F identifies the final Mercy-Star slot");
    check(
        dc_classify_mercy_star_initializer(
            work_ram.data(), work_ram.size(), 0x62U, &initializer)
            == 0,
        "native 4F9F rejects a non-Mercy-Star slot");
    check(
        dc_classify_mercy_star_initializer(
            work_ram.data(), 0x1FE1U, 0x5FU, &initializer)
            == 0,
        "native 4F9F rejects a truncated work-RAM snapshot");
}

void test_native_devil_shooter_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_devil_shooter_initializer initializer{};

    check(
        dc_classify_devil_shooter_initializer(
            work_ram.data(), work_ram.size(), 0x0EU, &initializer)
            != 0
            && initializer.slot == 0x0EU,
        "native 50EE identifies the fixed Devil-Shooter slot");
    check(
        dc_classify_devil_shooter_initializer(
            work_ram.data(), work_ram.size(), 0x0FU, &initializer)
            == 0,
        "native 50EE rejects a non-Devil-Shooter slot");
    check(
        dc_classify_devil_shooter_initializer(
            work_ram.data(), 0x1F8EU, 0x0EU, &initializer)
            == 0,
        "native 50EE rejects a truncated work-RAM snapshot");
}

void test_native_little_skull_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_little_skull_initializer initializer{};

    work_ram[0x0CF5U] = 1U;
    check(
        dc_classify_little_skull_initializer(
            work_ram.data(), work_ram.size(), 0x47U, 0U, &initializer)
                != 0
            && initializer.slot == 0x47U && initializer.pair == 0U
            && initializer.live_count_after == 2U,
        "native 44E4 identifies the left Little-Skull pair counter");
    work_ram[0x0CF6U] = 0xFFU;
    check(
        dc_classify_little_skull_initializer(
            work_ram.data(), work_ram.size(), 0x49U, 1U, &initializer)
                != 0
            && initializer.live_count_after == 0U,
        "native 44E4 preserves the Little-Skull pair counter wrap");
    check(
        dc_classify_little_skull_initializer(
            work_ram.data(), work_ram.size(), 0x49U, 0U, &initializer)
            == 0,
        "native 44E4 rejects a mismatched Little-Skull pair parameter");
    check(
        dc_classify_little_skull_initializer(
            work_ram.data(), 0x1FCAU, 0x46U, 0U, &initializer)
            == 0,
        "native 44E4 rejects truncated work RAM");
}

void test_native_little_skull_pair_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_little_skull_pair_initializer initializer{};

    check(
        dc_classify_little_skull_pair_initializer(
            work_ram.data(), work_ram.size(), 0x45U, 0U, &initializer)
                != 0
            && initializer.slot == 0x45U && initializer.pair == 0U,
        "native 4485 identifies the left Little-Skull pair controller");
    check(
        dc_classify_little_skull_pair_initializer(
            work_ram.data(), work_ram.size(), 0x48U, 1U, &initializer)
                != 0
            && initializer.slot == 0x48U && initializer.pair == 1U,
        "native 4485 identifies the right Little-Skull pair controller");
    check(
        dc_classify_little_skull_pair_initializer(
            work_ram.data(), work_ram.size(), 0x48U, 0U, &initializer)
            == 0,
        "native 4485 rejects a mismatched Little-Skull pair parameter");
    check(
        dc_classify_little_skull_pair_initializer(
            work_ram.data(), 0x1FC8U, 0x45U, 0U, &initializer)
            == 0,
        "native 4485 rejects truncated work RAM");
}

void test_native_little_skull_hidden_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_little_skull_hidden_initializer initializer{};

    check(
        dc_classify_little_skull_hidden_initializer(
            work_ram.data(), work_ram.size(), 0x44U, &initializer)
                != 0
            && initializer.slot == 0x44U,
        "native 4455 identifies the fixed Little-Skull hidden controller");
    check(
        dc_classify_little_skull_hidden_initializer(
            work_ram.data(), work_ram.size(), 0x45U, &initializer)
            == 0,
        "native 4455 rejects a non-hidden Little-Skull slot");
    check(
        dc_classify_little_skull_hidden_initializer(
            work_ram.data(), 0x1FC4U, 0x44U, &initializer)
            == 0,
        "native 4455 rejects truncated work RAM");
}

void test_native_dragon_head_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_dragon_head_initializer initializer{};

    work_ram[0x1093U] = 0xCAU;
    check(
        dc_classify_dragon_head_initializer(
            work_ram.data(), work_ram.size(), 0x13U, &initializer)
            != 0
            && initializer.slot == 0x13U
            && initializer.flags_before == 0xCAU
            && initializer.flags_after == 0x8AU,
        "native 5A31 clears the Dragon's Head transient flag");
    check(
        dc_classify_dragon_head_initializer(
            work_ram.data(), work_ram.size(), 0x14U, &initializer)
            == 0,
        "native 5A31 rejects a non-Dragon's-Head slot");
    check(
        dc_classify_dragon_head_initializer(
            work_ram.data(), 0x1F93U, 0x13U, &initializer)
            == 0,
        "native 5A31 rejects a truncated work-RAM snapshot");
}

void test_native_giant_skeleton_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_giant_skeleton_initializer initializer{};

    check(
        dc_classify_giant_skeleton_initializer(
            work_ram.data(), work_ram.size(), 0x30U, &initializer)
            != 0
            && initializer.slot == 0x30U,
        "native 5EE4 identifies the fixed Giant Skeleton slot");
    check(
        dc_classify_giant_skeleton_initializer(
            work_ram.data(), work_ram.size(), 0x31U, &initializer)
            == 0,
        "native 5EE4 rejects a non-Giant-Skeleton slot");
    check(
        dc_classify_giant_skeleton_initializer(
            work_ram.data(), 0x1FB0U, 0x30U, &initializer)
            == 0,
        "native 5EE4 rejects a truncated work-RAM snapshot");
}

void test_native_giant_skeleton_controller()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_giant_skeleton_controller controller{};

    work_ram[0x1830U] = 0x06U;
    check(
        dc_classify_giant_skeleton_controller(
            work_ram.data(), work_ram.size(), 0x30U, &controller)
                != 0
            && controller.slot == 0x30U
            && controller.collision_state == 0x06U
            && controller.path == DC_GIANT_SKELETON_CONTROLLER_IDLE,
        "native 5EEC identifies the Giant Skeleton animation/idle path");

    work_ram[0x1830U] = 0x83U;
    check(
        dc_classify_giant_skeleton_controller(
            work_ram.data(), work_ram.size(), 0x30U, &controller)
                != 0
            && controller.collision_state == 0x83U
            && controller.path == DC_GIANT_SKELETON_CONTROLLER_HIT,
        "native 5EEC identifies the Giant Skeleton repeatable hit path");
    check(
        dc_classify_giant_skeleton_controller(
            work_ram.data(), work_ram.size(), 0x31U, &controller)
            == 0,
        "native 5EEC rejects a non-Giant-Skeleton slot");
    check(
        dc_classify_giant_skeleton_controller(
            work_ram.data(), 0x1830U, 0x30U, &controller)
            == 0,
        "native 5EEC rejects a truncated work-RAM snapshot");
}

void test_native_three_blobs_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_three_blobs_initializer initializer{};

    check(
        dc_classify_three_blobs_initializer(
            work_ram.data(), work_ram.size(), 0x58U, 0U, &initializer)
                != 0
            && initializer.slot == 0x58U && initializer.initial_state == 0U
            && initializer.appearance == 0x8AU,
        "native 664A identifies the first Three-Blobs layout");
    check(
        dc_classify_three_blobs_initializer(
            work_ram.data(), work_ram.size(), 0x5AU, 2U, &initializer)
                != 0
            && initializer.appearance == 0x85U,
        "native 664A derives the final Three-Blobs appearance");
    check(
        dc_classify_three_blobs_initializer(
            work_ram.data(), work_ram.size(), 0x5AU, 1U, &initializer)
            == 0,
        "native 664A rejects a mismatched layout state");
    check(
        dc_classify_three_blobs_initializer(
            work_ram.data(), 0x1FDAU, 0x58U, 0U, &initializer)
            == 0,
        "native 664A rejects a truncated work-RAM snapshot");
}

void test_native_three_blobs_controller_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_three_blobs_controller_initializer initializer{};

    check(
        dc_classify_three_blobs_controller_initializer(
            work_ram.data(), work_ram.size(), 0x57U, &initializer)
            != 0
            && initializer.slot == 0x57U,
        "native 65BC identifies the fixed Three-Blobs controller slot");
    check(
        dc_classify_three_blobs_controller_initializer(
            work_ram.data(), work_ram.size(), 0x58U, &initializer)
            == 0,
        "native 65BC rejects a visible Three-Blobs slot");
    check(
        dc_classify_three_blobs_controller_initializer(
            work_ram.data(), 0x0CFDU, 0x57U, &initializer)
            == 0,
        "native 65BC rejects a truncated work-RAM snapshot");
}

void test_native_walled_star_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_walled_star_initializer initializer{};

    check(
        dc_classify_walled_star_initializer(
            work_ram.data(), work_ram.size(), 0x5BU, &initializer)
            != 0
            && initializer.slot == 0x5BU,
        "native 6319 identifies the fixed Walled Star slot");
    check(
        dc_classify_walled_star_initializer(
            work_ram.data(), work_ram.size(), 0x5AU, &initializer)
            == 0,
        "native 6319 rejects an adjacent actor slot");
    check(
        dc_classify_walled_star_initializer(
            work_ram.data(), 0x1FDBU, 0x5BU, &initializer)
            == 0,
        "native 6319 rejects a truncated work-RAM snapshot");
}

void test_native_mercy_star_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_mercy_star_gate gate{};

    work_ram[0x0254U] = 1U;
    work_ram[0x0D52U] = 1U;
    check(
        dc_classify_mercy_star_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
            != 0
            && gate.path == DC_MERCY_STAR_NON_NORMAL_MODE,
        "native 4FC1 identifies the non-normal-table return");

    work_ram[0x0254U] = 0U;
    work_ram[0x0D52U] = 3U;
    check(
        dc_classify_mercy_star_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
            != 0
            && gate.path == DC_MERCY_STAR_NOT_LAST_BALL,
        "native 4FC1 identifies the not-last-ball return");

    work_ram[0x0D52U] = 1U;
    work_ram[0x0251U] = 1U;
    work_ram[0x0206U] = 0x88U;
    work_ram[0x020DU] = 0x13U;
    check(
        dc_classify_mercy_star_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
            != 0
            && gate.path == DC_MERCY_STAR_SCORE_LIMIT
            && gate.stored_score == 0x001388UL,
        "native 4FC1 identifies the 500000-point limit");

    work_ram[0x020DU] = 0x12U;
    check(
        dc_classify_mercy_star_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
            != 0
            && gate.path == DC_MERCY_STAR_ACTIVATE
            && gate.selected_player == 1U,
        "native 4FC1 identifies the activation branch");

    check(
        dc_classify_mercy_star_gate(
            work_ram.data(),
            0x0D52U,
            &gate)
            == 0,
        "native 4FC1 rejects a truncated work-RAM snapshot");
}

void test_native_three_blobs_controller()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_three_blobs_controller controller{};

    work_ram[0x0D39U] = 2U;
    work_ram[0x0D3AU] = 1U;
    work_ram[0x0D3BU] = 2U;
    check(
        dc_classify_three_blobs_controller(
            work_ram.data(),
            work_ram.size(),
            &controller)
            != 0
            && controller.path == DC_THREE_BLOBS_FIRST_PAIR_MISMATCH
            && controller.first_state == 2U
            && controller.second_state == 1U,
        "native 65C2 identifies the first-pair mismatch return");

    work_ram[0x0D3AU] = 2U;
    work_ram[0x0D3BU] = 1U;
    check(
        dc_classify_three_blobs_controller(
            work_ram.data(),
            work_ram.size(),
            &controller)
            != 0
            && controller.path
                == DC_THREE_BLOBS_THIRD_VALUE_MISMATCH
            && controller.third_state == 1U,
        "native 65C2 identifies the third-value mismatch return");

    work_ram[0x0D3BU] = 2U;
    check(
        dc_classify_three_blobs_controller(
            work_ram.data(),
            work_ram.size(),
            &controller)
            != 0
            && controller.path == DC_THREE_BLOBS_EQUAL,
        "native 65C2 identifies the equal-state outcome");

    check(
        dc_classify_three_blobs_controller(
            work_ram.data(),
            0x0D3BU,
            &controller)
            == 0,
        "native 65C2 rejects a truncated work-RAM snapshot");
}

void test_native_three_blobs_appearance_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_three_blobs_appearance_gate gate{};

    work_ram[0x1AD8U] = 1U;
    work_ram[0x0D3AU] = 4U;
    work_ram[0x1858U] = 0U;
    check(
        dc_classify_three_blobs_appearance_gate(
            work_ram.data(), work_ram.size(), 0x58U, &gate)
                != 0
            && gate.path == DC_THREE_BLOBS_APPEARANCE_GATE_IDLE
            && gate.phase_index == 1U && gate.global_state == 4U
            && gate.appearance == 0x8EU,
        "native 67B3 maps the second Blob state on an idle actor");

    work_ram[0x1858U] = 0x80U;
    check(
        dc_classify_three_blobs_appearance_gate(
            work_ram.data(), work_ram.size(), 0x58U, &gate)
                != 0
            && gate.path == DC_THREE_BLOBS_APPEARANCE_GATE_COLLISION,
        "native 67B3 defers the Blob collision state");

    work_ram[0x1AD8U] = 3U;
    check(
        dc_classify_three_blobs_appearance_gate(
            work_ram.data(), work_ram.size(), 0x58U, &gate)
            == 0,
        "native 67B3 rejects an invalid Blob phase index");
    check(
        dc_classify_three_blobs_appearance_gate(
            work_ram.data(), 0x1AD8U, 0x58U, &gate)
            == 0,
        "native 67B3 rejects a truncated work-RAM snapshot");
    check(
        dc_classify_three_blobs_appearance_gate(
            work_ram.data(), work_ram.size(), 0x57U, &gate)
            == 0,
        "native 67B3 rejects a non-Blob actor slot");
}

void test_native_three_blobs_first_movement_prefix()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_three_blobs_first_movement_prefix prefix{};

    work_ram[0x1A58U] = 4U;
    work_ram[0x1B58U] = 0xBAU;
    work_ram[0x1BD8U] = 7U;
    check(
        dc_classify_three_blobs_first_movement_prefix(
            work_ram.data(), work_ram.size(), 0x58U, &prefix)
                != 0
            && prefix.decremented_timer == 3U
            && prefix.decremented_x == 0xB9U
            && prefix.decremented_y == 6U
            && prefix.path == DC_THREE_BLOBS_FIRST_MOVEMENT_PREFIX_DIRECT,
        "native 66A3 identifies the no-zero Three-Blobs movement prefix");
    work_ram[0x1A58U] = 1U;
    check(
        dc_classify_three_blobs_first_movement_prefix(
            work_ram.data(), work_ram.size(), 0x58U, &prefix)
                != 0
            && prefix.path
                == DC_THREE_BLOBS_FIRST_MOVEMENT_PREFIX_TIMER_EXPIRY,
        "native 66A3 identifies the timer-expiry phase-toggle prefix");
    work_ram[0x1A58U] = 4U;
    work_ram[0x1B58U] = 1U;
    check(
        dc_classify_three_blobs_first_movement_prefix(
            work_ram.data(), work_ram.size(), 0x58U, &prefix)
                != 0
            && prefix.path == DC_THREE_BLOBS_FIRST_MOVEMENT_PREFIX_X_EXPIRY,
        "native 66A3 identifies the x-expiry scheduler handoff prefix");
    work_ram[0x1B58U] = 0xBAU;
    work_ram[0x1BD8U] = 1U;
    check(
        dc_classify_three_blobs_first_movement_prefix(
            work_ram.data(), work_ram.size(), 0x58U, &prefix)
            != 0
            && prefix.path == DC_THREE_BLOBS_FIRST_MOVEMENT_PREFIX_Y_EXPIRY,
        "native 66A3 identifies the y-expiry reset/coroutine handoff prefix");
    check(
        dc_classify_three_blobs_first_movement_prefix(
            work_ram.data(), work_ram.size(), 0x57U, &prefix)
            == 0,
        "native 66A3 rejects a non-Three-Blobs slot");
}

void test_native_dragon_egg_controller()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_dragon_egg_controller controller{};

    work_ram[0x0CF4U] = 6U;
    check(
        dc_classify_dragon_egg_controller(
            work_ram.data(),
            work_ram.size(),
            &controller)
            != 0
            && controller.path == DC_DRAGON_EGG_CONTROLLER_ACTIVE
            && controller.live_egg_count == 6U,
        "native 42F6 identifies the nonzero-count return");

    work_ram[0x0CF4U] = 0U;
    check(
        dc_classify_dragon_egg_controller(
            work_ram.data(),
            work_ram.size(),
            &controller)
            != 0
            && controller.path == DC_DRAGON_EGG_CONTROLLER_COMPLETE,
        "native 42F6 identifies the group-completion path");

    check(
        dc_classify_dragon_egg_controller(
            work_ram.data(),
            0x0CF4U,
            &controller)
            == 0,
        "native 42F6 rejects a truncated work-RAM snapshot");
}

void test_native_dragon_egg_controller_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_dragon_egg_controller_initializer initializer{};

    check(
        dc_classify_dragon_egg_controller_initializer(
            work_ram.data(), work_ram.size(), 0x1FU, &initializer)
                != 0
            && initializer.slot == 0x1FU,
        "native 42EB identifies the Dragon's-Egg controller slot");
    check(
        dc_classify_dragon_egg_controller_initializer(
            work_ram.data(), work_ram.size(), 0x20U, &initializer)
            == 0,
        "native 42EB rejects a Dragon's-Egg layout slot");
    check(
        dc_classify_dragon_egg_controller_initializer(
            work_ram.data(), 0x0CF7U, 0x1FU, &initializer)
            == 0,
        "native 42EB rejects truncated work RAM");
}

void test_native_big_star_feature_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_big_star_feature_gate gate{};

    work_ram[0x0CFAU] = 0U;
    check(
        dc_classify_big_star_feature_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
            != 0
            && gate.path == DC_BIG_STAR_FEATURE_GATE_IDLE
            && gate.pending_feature_bits == 0U,
        "native 445B identifies the idle feature return");

    work_ram[0x0CFAU] = 0x05U;
    check(
        dc_classify_big_star_feature_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
            != 0
            && gate.path == DC_BIG_STAR_FEATURE_GATE_PENDING
            && gate.pending_feature_bits == 0x05U,
        "native 445B identifies the pending feature path");

    check(
        dc_classify_big_star_feature_gate(
            work_ram.data(),
            0x0CFAU,
            &gate)
            == 0,
        "native 445B rejects a truncated work-RAM snapshot");
}

void test_native_big_star_pending_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_big_star_pending_gate gate{};

    work_ram[0x0D04U] = 0U;
    check(
        dc_classify_big_star_pending_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
            != 0
            && gate.path == DC_BIG_STAR_PENDING_GATE_IDLE
            && gate.selected_pending_bits == 0U,
        "native 570A identifies the no-pending return");

    work_ram[0x0D04U] = 0x04U;
    check(
        dc_classify_big_star_pending_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
            != 0
            && gate.path == DC_BIG_STAR_PENDING_GATE_IDLE
            && gate.pending_feature_bits == 0x04U,
        "native 570A ignores non-direction pending bits");

    work_ram[0x0D04U] = 0x01U;
    check(
        dc_classify_big_star_pending_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
            != 0
            && gate.path == DC_BIG_STAR_PENDING_GATE_ACTIVE,
        "native 570A defers the first direction helper");

    work_ram[0x0D04U] = 0x02U;
    check(
        dc_classify_big_star_pending_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
            != 0
            && gate.path == DC_BIG_STAR_PENDING_GATE_ACTIVE,
        "native 570A defers the second direction helper");

    check(
        dc_classify_big_star_pending_gate(
            work_ram.data(),
            0x0D04U,
            &gate)
            == 0,
        "native 570A rejects a truncated work-RAM snapshot");
}

void test_native_big_star_pending_helper()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_big_star_pending_helper helper{};

    work_ram[0x1B4BU] = 0x03U;
    check(
        dc_classify_big_star_pending_helper(
            work_ram.data(), work_ram.size(), 0x4BU, &helper)
                != 0
            && helper.phase == 0x03U && helper.next_phase == 0x04U,
        "native 5722 identifies the observed Big-Star pending-left phase");

    work_ram[0x1B4BU] = 0x04U;
    check(
        dc_classify_big_star_pending_helper(
            work_ram.data(), work_ram.size(), 0x4BU, &helper)
                != 0
            && helper.next_phase == 0x05U,
        "native 5722 identifies the verified Big-Star pending-left phase-four transition");
    work_ram[0x1B4BU] = 0x05U;
    check(
        dc_classify_big_star_pending_helper(
            work_ram.data(), work_ram.size(), 0x4BU, &helper)
                != 0
            && helper.next_phase == 0x06U,
        "native 5722 identifies the verified Big-Star pending-left phase-five transition");
    work_ram[0x1B4BU] = 0x06U;
    check(
        dc_classify_big_star_pending_helper(
            work_ram.data(), work_ram.size(), 0x4BU, &helper)
                != 0
            && helper.next_phase == 0x06U,
        "native 5722 identifies the verified Big-Star pending-left phase-six saturation");
    work_ram[0x1B4BU] = 0x02U;
    check(
        dc_classify_big_star_pending_helper(
            work_ram.data(), work_ram.size(), 0x4BU, &helper)
                != 0
            && helper.next_phase == 0x03U,
        "native 5722 identifies the verified Big-Star pending-left lower-phase reset");
    check(
        dc_classify_big_star_pending_helper(
            work_ram.data(), 0x1B4BU, 0x4BU, &helper)
            == 0,
        "native 5722 rejects truncated work RAM");
    check(
        dc_classify_big_star_pending_helper(
            work_ram.data(), work_ram.size(), 0x4AU, &helper)
            == 0,
        "native 5722 rejects another actor slot");
}

void test_native_big_star_pending_right_helper()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_big_star_pending_right_helper helper{};

    work_ram[0x1B4BU] = 0x03U;
    check(
        dc_classify_big_star_pending_right_helper(
            work_ram.data(), work_ram.size(), 0x4BU, &helper)
                != 0
            && helper.phase == 0x03U && helper.next_phase == 0x02U,
        "native 5738 identifies the observed Big-Star pending-right phase");

    work_ram[0x1B4BU] = 0x02U;
    check(
        dc_classify_big_star_pending_right_helper(
            work_ram.data(), work_ram.size(), 0x4BU, &helper)
                != 0
            && helper.next_phase == 0x01U,
        "native 5738 identifies the verified Big-Star pending-right phase-two transition");
    work_ram[0x1B4BU] = 0x01U;
    check(
        dc_classify_big_star_pending_right_helper(
            work_ram.data(), work_ram.size(), 0x4BU, &helper)
                != 0
            && helper.next_phase == 0x00U,
        "native 5738 identifies the verified Big-Star pending-right phase-one transition");
    work_ram[0x1B4BU] = 0x00U;
    check(
        dc_classify_big_star_pending_right_helper(
            work_ram.data(), work_ram.size(), 0x4BU, &helper)
                != 0
            && helper.next_phase == 0x00U,
        "native 5738 identifies the verified Big-Star pending-right phase-zero hold");
    work_ram[0x1B4BU] = 0x04U;
    check(
        dc_classify_big_star_pending_right_helper(
            work_ram.data(), work_ram.size(), 0x4BU, &helper)
                != 0
            && helper.next_phase == 0x03U,
        "native 5738 identifies the verified Big-Star pending-right phase-four reset");
    check(
        dc_classify_big_star_pending_right_helper(
            work_ram.data(), 0x1B4BU, 0x4BU, &helper)
                == 0,
        "native 5738 rejects truncated work RAM");
    check(
        dc_classify_big_star_pending_right_helper(
            work_ram.data(), work_ram.size(), 0x4AU, &helper)
                == 0,
        "native 5738 rejects another actor slot");
}

void test_native_tower_insect_source_countdown_wait()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_tower_insect_source_countdown_wait wait{};

    work_ram[0x1A2CU] = 0x30U;
    check(
        dc_classify_tower_insect_source_countdown_wait(
            work_ram.data(), work_ram.size(), 0x2CU, &wait)
                != 0
            && wait.decremented_timer == 0x2FU,
        "native 5F4A identifies the Tower-insect source countdown wait");
    work_ram[0x1A2CU] = 0x01U;
    check(
        dc_classify_tower_insect_source_countdown_wait(
            work_ram.data(), work_ram.size(), 0x2CU, &wait)
                != 0
            && wait.decremented_timer == 0U,
        "native 5F4A exposes the source countdown expiry for fallback");
    check(
        dc_classify_tower_insect_source_countdown_wait(
            work_ram.data(), 0x1A2CU, 0x2CU, &wait)
                == 0
            && dc_classify_tower_insect_source_countdown_wait(
                work_ram.data(), work_ram.size(), 0x2DU, &wait)
                == 0,
        "native 5F4A rejects truncated RAM and another source slot");
}

void test_native_big_star_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_big_star_initializer initializer{};

    check(
        dc_classify_big_star_initializer(
            work_ram.data(), work_ram.size(), 0x4BU, &initializer)
                != 0
            && initializer.slot == 0x4BU,
        "native 565C identifies the fixed Big-Star slot");
    check(
        dc_classify_big_star_initializer(
            work_ram.data(), work_ram.size(), 0x4AU, &initializer)
            == 0,
        "native 565C rejects a non-Big-Star slot");
    check(
        dc_classify_big_star_initializer(
            work_ram.data(), 0x1FCBU, 0x4BU, &initializer)
            == 0,
        "native 565C rejects truncated work RAM");
}

void test_native_undead_head_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_undead_head_initializer initializer{};

    check(
        dc_classify_undead_head_initializer(
            work_ram.data(), work_ram.size(), 0x4DU, &initializer)
                != 0
            && initializer.slot == 0x4DU,
        "native 61A0 identifies the fixed Undead-Head slot");
    check(
        dc_classify_undead_head_initializer(
            work_ram.data(), work_ram.size(), 0x4EU, &initializer)
            == 0,
        "native 61A0 rejects a non-Undead-Head slot");
    check(
        dc_classify_undead_head_initializer(
            work_ram.data(), 0x1FCDU, 0x4DU, &initializer)
            == 0,
        "native 61A0 rejects truncated work RAM");
}

void test_native_undead_head_selector_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_undead_head_selector_gate gate{};

    work_ram[0x0D18U] = 0x80U;
    check(
        dc_classify_undead_head_selector_gate(
            work_ram.data(), work_ram.size(), 0x4DU, &gate)
                != 0
            && gate.path == DC_UNDEAD_HEAD_SELECTOR_GATE_WAITING
            && gate.slot == 0x4DU && gate.selector == 0x80U,
        "native 61AB identifies the stopped Undead-Head selector return");

    work_ram[0x0D18U] = 0U;
    check(
        dc_classify_undead_head_selector_gate(
            work_ram.data(), work_ram.size(), 0x4DU, &gate)
                != 0
            && gate.path == DC_UNDEAD_HEAD_SELECTOR_GATE_ACTIVE,
        "native 61AB defers the Undead-Head activation path");
    check(
        dc_classify_undead_head_selector_gate(
            work_ram.data(), 0x0D18U, 0x4DU, &gate)
            == 0,
        "native 61AB rejects truncated work RAM");
    check(
        dc_classify_undead_head_selector_gate(
            work_ram.data(), work_ram.size(), 0x4EU, &gate)
            == 0,
        "native 61AB rejects another actor slot");
}

void test_native_undead_head_waiting_parent()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_undead_head_waiting_parent parent{};
    work_ram[0x0D18U] = 0x80U;
    check(
        dc_classify_undead_head_waiting_parent(
            work_ram.data(), work_ram.size(), 0x4DU, &parent)
                != 0
            && parent.slot == 0x4DU && parent.selector == 0x80U,
        "native 61A3 identifies the stopped Undead-Head parent");
    work_ram[0x0D00U] = 1U;
    check(
        dc_classify_undead_head_waiting_parent(
            work_ram.data(), work_ram.size(), 0x4DU, &parent)
            == 0,
        "native 61A3 defers a session-transition parent path");
    work_ram[0x0D00U] = 0U;
    work_ram[0x0D18U] = 0U;
    check(
        dc_classify_undead_head_waiting_parent(
            work_ram.data(), work_ram.size(), 0x4DU, &parent)
            == 0,
        "native 61A3 defers the Undead-Head activation path");
    work_ram[0x0D18U] = 0x80U;
    check(
        dc_classify_undead_head_waiting_parent(
            work_ram.data(), 0x0D18U, 0x4DU, &parent)
            == 0,
        "native 61A3 rejects truncated work RAM");
    check(
        dc_classify_undead_head_waiting_parent(
            work_ram.data(), work_ram.size(), 0x4EU, &parent)
            == 0,
        "native 61A3 rejects another actor slot");
}

void test_native_dragon_egg_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_dragon_egg_initializer initializer{};

    work_ram[0x0CF4U] = 0x05U;
    check(
        dc_classify_dragon_egg_initializer(
            work_ram.data(), work_ram.size(), 0x23U, &initializer)
                != 0
            && initializer.slot == 0x23U
            && initializer.live_egg_count_after == 0x06U,
        "native 433F identifies an Egg layout and increments its live count");
    check(
        dc_classify_dragon_egg_initializer(
            work_ram.data(), work_ram.size(), 0x26U, &initializer)
            == 0,
        "native 433F rejects a non-Egg slot");
    check(
        dc_classify_dragon_egg_initializer(
            work_ram.data(), 0x0CF4U, 0x20U, &initializer)
            == 0,
        "native 433F rejects truncated work RAM");
}

void test_native_side_worm_initializers()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_side_worm_initializer initializer{};

    check(
        dc_classify_side_worm_initializer(
            work_ram.data(), work_ram.size(), 0x4EU, &initializer)
                != 0
            && initializer.selector == 0x06U
            && initializer.initial_state == 0x00U,
        "native 5C4B identifies the fixed left Side-Worm setup");
    check(
        dc_classify_side_worm_initializer(
            work_ram.data(), work_ram.size(), 0x4FU, &initializer)
                != 0
            && initializer.selector == 0x02U
            && initializer.initial_state == 0x01U,
        "native 5C7C identifies the fixed right Side-Worm setup");
    check(
        dc_classify_side_worm_initializer(
            work_ram.data(), work_ram.size(), 0x4DU, &initializer)
            == 0,
        "native Side-Worm setup rejects a non-Worm slot");
    check(
        dc_classify_side_worm_initializer(
            work_ram.data(), 0x1FCFU, 0x4EU, &initializer)
            == 0,
        "native Side-Worm setup rejects truncated work RAM");
}

void test_native_side_worm_selector_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_side_worm_selector_gate gate{};

    work_ram[0x0D18U] = 0x80U;
    work_ram[0x17CEU] = 0x06U;
    check(
        dc_classify_side_worm_selector_gate(
            work_ram.data(), work_ram.size(), 0x4EU, &gate)
                != 0
            && gate.path == DC_SIDE_WORM_SELECTOR_GATE_WAITING
            && gate.selector == 0x80U && gate.expected_selector == 0x06U,
        "native 5CCD identifies the stopped left Side-Worm return");

    work_ram[0x17CEU] = 0x80U;
    check(
        dc_classify_side_worm_selector_gate(
            work_ram.data(), work_ram.size(), 0x4EU, &gate)
                != 0
            && gate.path == DC_SIDE_WORM_SELECTOR_GATE_ACTIVE,
        "native 5CCD defers the matching left Side-Worm activation");
    check(
        dc_classify_side_worm_selector_gate(
            work_ram.data(), 0x17CEU, 0x4EU, &gate)
            == 0,
        "native 5CCD rejects truncated work RAM");
    check(
        dc_classify_side_worm_selector_gate(
            work_ram.data(), work_ram.size(), 0x4DU, &gate)
            == 0,
        "native 5CCD rejects another actor slot");
}

void test_native_side_worm_waiting_parent()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_side_worm_waiting_parent parent{};
    work_ram[0x0D18U] = 0x80U; work_ram[0x17CEU] = 0x06U;
    check(dc_classify_side_worm_waiting_parent(work_ram.data(), work_ram.size(), 0x4EU, &parent) != 0
            && parent.selector == 0x80U && parent.expected_selector == 0x06U,
        "native 5CC5 identifies the stopped left Side-Worm parent");
    work_ram[0x0D00U] = 1U;
    check(dc_classify_side_worm_waiting_parent(work_ram.data(), work_ram.size(), 0x4EU, &parent) == 0,
        "native 5CC5 defers a session-transition parent path");
    work_ram[0x0D00U] = 0U; work_ram[0x17CEU] = 0x80U;
    check(dc_classify_side_worm_waiting_parent(work_ram.data(), work_ram.size(), 0x4EU, &parent) == 0,
        "native 5CC5 defers matching Side-Worm activation");
    work_ram[0x17CEU] = 0x06U;
    check(dc_classify_side_worm_waiting_parent(work_ram.data(), work_ram.size(), 0x4DU, &parent) == 0,
        "native 5CC5 rejects another actor slot");
    check(dc_classify_side_worm_waiting_parent(work_ram.data(), 0x17CEU, 0x4EU, &parent) == 0,
        "native 5CC5 rejects truncated work RAM");
}

void test_native_big_star_rotation_wait()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_big_star_rotation_wait rotation{};

    work_ram[0x0D00U] = 0x40U;
    work_ram[0x1A4BU] = 0x03U;
    work_ram[0x00CBU] = 0x03U;
    check(
        dc_classify_big_star_rotation_wait(
            work_ram.data(), work_ram.size(), 0x4BU, &rotation)
            != 0
            && rotation.path == DC_BIG_STAR_ROTATION_WAIT_STABLE
            && rotation.decremented_timer == 0x02U
            && rotation.cleared_session_flags == 0x01U,
        "native 5674 identifies the rotating timer wait");

    work_ram[0x0D00U] = 0U;
    work_ram[0x0D01U] = 0U;
    check(
        dc_classify_big_star_rotation_wait(
            work_ram.data(), work_ram.size(), 0x4BU, &rotation)
            != 0
            && rotation.path == DC_BIG_STAR_ROTATION_WAIT_ACTIVE,
        "native 5674 defers the stopped-star path");

    work_ram[0x0D00U] = 0x40U;
    work_ram[0x1A4BU] = 0x01U;
    check(
        dc_classify_big_star_rotation_wait(
            work_ram.data(), work_ram.size(), 0x4BU, &rotation)
            != 0
            && rotation.path == DC_BIG_STAR_ROTATION_WAIT_ACTIVE,
        "native 5674 defers the timer-expiry path");

    work_ram[0x1A4BU] = 0x03U;
    work_ram[0x0D04U] = 0x01U;
    check(
        dc_classify_big_star_rotation_wait(
            work_ram.data(), work_ram.size(), 0x4BU, &rotation)
            != 0
            && rotation.path == DC_BIG_STAR_ROTATION_WAIT_ACTIVE,
        "native 5674 defers the pending-direction helper");

    check(
        dc_classify_big_star_rotation_wait(
            work_ram.data(), work_ram.size(), 0x4AU, &rotation)
            == 0
            && dc_classify_big_star_rotation_wait(
                work_ram.data(), 0x1A4BU, 0x4BU, &rotation)
                == 0,
        "native 5674 rejects invalid slot and truncated work RAM");
}

void test_native_giant_face_animator_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_giant_face_animator_gate gate{};

    check(
        dc_classify_giant_face_animator_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
                != 0
            && gate.animation_mode == 0U
            && gate.path == DC_GIANT_FACE_ANIMATOR_GATE_IDLE,
        "native 767E identifies the idle hidden-form animator");

    work_ram[0x0D0EU] = 1U;
    check(
        dc_classify_giant_face_animator_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
                != 0
            && gate.animation_mode == 1U
            && gate.path == DC_GIANT_FACE_ANIMATOR_GATE_ACTIVE,
        "native 767E identifies the active hidden-form animator");

    work_ram[0x0D0EU] = 2U;
    check(
        dc_classify_giant_face_animator_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
                != 0
            && gate.animation_mode == 2U
            && gate.path == DC_GIANT_FACE_ANIMATOR_GATE_IDLE,
        "native 767E returns at this entry for non-one animator modes");

    check(
        dc_classify_giant_face_animator_gate(
            work_ram.data(),
            0x0D0EU,
            &gate)
            == 0,
        "native 767E rejects a truncated work-RAM snapshot");
}

void test_native_giant_face_animator_active_wait()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_giant_face_animator_active_wait wait{};

    work_ram[0x1A37U] = 0x08U;
    check(
        dc_classify_giant_face_animator_active_wait(
            work_ram.data(), work_ram.size(), 0x37U, &wait)
            != 0
            && wait.timer == 0x08U
            && wait.decremented_timer == 0x07U,
        "native 76C5 identifies the active Giant-Face animator timer wait");
    work_ram[0x1A37U] = 0x01U;
    check(
        dc_classify_giant_face_animator_active_wait(
            work_ram.data(), work_ram.size(), 0x37U, &wait)
            == 0,
        "native 76C5 defers the active animator timer expiry");
    work_ram[0x1A37U] = 0x08U;
    check(
        dc_classify_giant_face_animator_active_wait(
            work_ram.data(), 0x1A37U, 0x37U, &wait)
            == 0,
        "native 76C5 rejects truncated work RAM");
    check(
        dc_classify_giant_face_animator_active_wait(
            work_ram.data(), work_ram.size(), 0x36U, &wait)
            == 0,
        "native 76C5 rejects an adjacent actor slot");
}

void test_native_giant_face_request_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_giant_face_request_gate gate{};

    check(
        dc_classify_giant_face_request_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
                != 0
            && gate.request_flags == 0U
            && gate.path == DC_GIANT_FACE_REQUEST_GATE_IDLE,
        "native 74D8 identifies the idle Giant-Face request gate");

    work_ram[0x0D0CU] = 0x02U;
    check(
        dc_classify_giant_face_request_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
                != 0
            && gate.request_flags == 0x02U
            && gate.path == DC_GIANT_FACE_REQUEST_GATE_IDLE,
        "native 74D8 ignores unrelated Giant-Face request bits");

    work_ram[0x0D0CU] = 0x01U;
    check(
        dc_classify_giant_face_request_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
                != 0
            && gate.request_flags == 0x01U
            && gate.path == DC_GIANT_FACE_REQUEST_GATE_ACTIVE,
        "native 74D8 identifies the Giant-Face wake request");

    check(
        dc_classify_giant_face_request_gate(
            work_ram.data(),
            0x0D0CU,
            &gate)
            == 0,
        "native 74D8 rejects a truncated work-RAM snapshot");
}

void test_native_giant_face_crown_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_giant_face_crown_gate gate{};

    work_ram[0x1839U] = 0x10U;
    check(
        dc_classify_giant_face_crown_gate(
            work_ram.data(),
            work_ram.size(),
            0x39U,
            &gate)
                != 0
            && gate.path == DC_GIANT_FACE_CROWN_GATE_IDLE
            && gate.slot == 0x39U
            && gate.collision_state == 0x10U,
        "native 76FA identifies the Giant-Face crown no-hit return");

    work_ram[0x1839U] = 0x90U;
    check(
        dc_classify_giant_face_crown_gate(
            work_ram.data(),
            work_ram.size(),
            0x39U,
            &gate)
                != 0
            && gate.path == DC_GIANT_FACE_CROWN_GATE_HIT
            && gate.collision_state == 0x90U,
        "native 76FA identifies the deferred Giant-Face crown hit path");

    check(
        dc_classify_giant_face_crown_gate(
            work_ram.data(),
            0x1839U,
            0x39U,
            &gate)
            == 0,
        "native 76FA rejects a truncated work-RAM snapshot");
    check(
        dc_classify_giant_face_crown_gate(
            work_ram.data(),
            work_ram.size(),
            0x80U,
            &gate)
            == 0,
        "native 76FA rejects an out-of-range actor slot");
}

void test_native_giant_face_mouth_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_giant_face_mouth_gate gate{};

    work_ram[0x0D21U] = 0U;
    work_ram[0x1838U] = 0x10U;
    check(
        dc_classify_giant_face_mouth_gate(
            work_ram.data(), work_ram.size(), 0x38U, &gate)
                != 0
            && gate.path == DC_GIANT_FACE_MOUTH_GATE_IDLE
            && gate.mouth_state == 0U && gate.slot == 0x38U
            && gate.collision_state == 0x10U,
        "native 7726 identifies the Giant-Face mouth no-hit return");

    work_ram[0x0D21U] = 1U;
    check(
        dc_classify_giant_face_mouth_gate(
            work_ram.data(), work_ram.size(), 0x38U, &gate)
                != 0
            && gate.path == DC_GIANT_FACE_MOUTH_GATE_OPEN,
        "native 7726 identifies the deferred Giant-Face open-mouth path");

    work_ram[0x0D21U] = 0U;
    work_ram[0x1838U] = 0x90U;
    check(
        dc_classify_giant_face_mouth_gate(
            work_ram.data(), work_ram.size(), 0x38U, &gate)
                != 0
            && gate.path == DC_GIANT_FACE_MOUTH_GATE_HIT
            && gate.collision_state == 0x90U,
        "native 7726 identifies the deferred Giant-Face mouth hit path");

    check(
        dc_classify_giant_face_mouth_gate(
            work_ram.data(), 0x1838U, 0x38U, &gate)
            == 0,
        "native 7726 rejects a truncated work-RAM snapshot");
    check(
        dc_classify_giant_face_mouth_gate(
            work_ram.data(), work_ram.size(), 0x80U, &gate)
            == 0,
        "native 7726 rejects an out-of-range actor slot");
}

void test_native_giant_face_helmet_controller()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_giant_face_helmet_controller controller{};

    work_ram[0x182AU] = 0x00U;
    work_ram[0x1A2AU] = 3U;
    work_ram[0x19AAU] = 1U;
    check(
        dc_classify_giant_face_helmet_controller(
            work_ram.data(), work_ram.size(), 0x2AU, &controller)
                != 0
            && controller.path == DC_GIANT_FACE_HELMET_CONTROLLER_WAITING
            && controller.decremented_timer == 2U
            && controller.advanced_appearance == 2U,
        "native 4C91 identifies the Giant-Face helmet timer wait");

    work_ram[0x1A2AU] = 1U;
    check(
        dc_classify_giant_face_helmet_controller(
            work_ram.data(), work_ram.size(), 0x2AU, &controller)
                != 0
            && controller.path == DC_GIANT_FACE_HELMET_CONTROLLER_ADVANCE
            && controller.decremented_timer == 0U
            && controller.advanced_appearance == 2U,
        "native 4C91 identifies the Giant-Face helmet appearance advance");

    work_ram[0x19AAU] = 2U;
    check(
        dc_classify_giant_face_helmet_controller(
            work_ram.data(), work_ram.size(), 0x2AU, &controller)
                != 0
            && controller.path == DC_GIANT_FACE_HELMET_CONTROLLER_RESET
            && controller.advanced_appearance == 3U,
        "native 4C91 identifies the Giant-Face helmet appearance reset");

    work_ram[0x182AU] = 0x80U;
    check(
        dc_classify_giant_face_helmet_controller(
            work_ram.data(), work_ram.size(), 0x2AU, &controller)
                != 0
            && controller.path == DC_GIANT_FACE_HELMET_CONTROLLER_COLLISION,
        "native 4C91 defers the Giant-Face helmet collision path");

    check(
        dc_classify_giant_face_helmet_controller(
            work_ram.data(), 0x1A2AU, 0x2AU, &controller)
            == 0,
        "native 4C91 rejects a truncated work-RAM snapshot");
    check(
        dc_classify_giant_face_helmet_controller(
            work_ram.data(), work_ram.size(), 0x80U, &controller)
            == 0,
        "native 4C91 rejects an out-of-range actor slot");
}

void test_native_giant_face_helmet_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_giant_face_helmet_initializer initializer{};

    check(
        dc_classify_giant_face_helmet_initializer(
            work_ram.data(), work_ram.size(), 0x2AU, 0x13U, &initializer)
                != 0
            && initializer.slot == 0x2AU && initializer.selector == 0x13U,
        "native Giant-Face helmet setup preserves the left selector");
    check(
        dc_classify_giant_face_helmet_initializer(
            work_ram.data(), work_ram.size(), 0x2BU, 0x1DU, &initializer)
                != 0
            && initializer.slot == 0x2BU && initializer.selector == 0x1DU,
        "native Giant-Face helmet setup preserves the right selector");
    check(
        dc_classify_giant_face_helmet_initializer(
            work_ram.data(), work_ram.size(), 0x2BU, 0x13U, &initializer)
            == 0,
        "native Giant-Face helmet setup rejects a mismatched selector");
    work_ram[0x182AU] = 0x80U;
    check(
        dc_classify_giant_face_helmet_initializer(
            work_ram.data(), work_ram.size(), 0x2AU, 0x13U, &initializer)
            == 0,
        "native Giant-Face helmet setup defers a collision-marked entry");
    check(
        dc_classify_giant_face_helmet_initializer(
            work_ram.data(), 0x1FABU, 0x2BU, 0x1DU, &initializer)
            == 0,
        "native Giant-Face helmet setup rejects truncated work RAM");
}

void test_native_four_star_wall_pocket_controller()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_four_star_wall_pocket_controller controller{};

    work_ram[0x183EU] = 0x00U;
    work_ram[0x1A3EU] = 1U;
    work_ram[0x19BEU] = 1U;
    check(
        dc_classify_four_star_wall_pocket_controller(
            work_ram.data(), work_ram.size(), 0x3EU, &controller)
                != 0
            && controller.path == DC_FOUR_STAR_WALL_POCKET_CONTROLLER_ADVANCE
            && controller.decremented_timer == 0U
            && controller.advanced_appearance == 2U,
        "native 4A93 identifies the Four-Star Wall-Pocket appearance advance");

    work_ram[0x19BEU] = 2U;
    check(
        dc_classify_four_star_wall_pocket_controller(
            work_ram.data(), work_ram.size(), 0x3EU, &controller)
                != 0
            && controller.path == DC_FOUR_STAR_WALL_POCKET_CONTROLLER_RESET,
        "native 4A93 identifies the Four-Star Wall-Pocket appearance reset");

    work_ram[0x183EU] = 0x80U;
    check(
        dc_classify_four_star_wall_pocket_controller(
            work_ram.data(), work_ram.size(), 0x3EU, &controller)
                != 0
            && controller.path
                == DC_FOUR_STAR_WALL_POCKET_CONTROLLER_COLLISION,
        "native 4A93 defers the Four-Star Wall-Pocket collision path");
}

void test_native_walled_star_access_controller()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_walled_star_access_controller controller{};

    work_ram[0x1811U] = 0x00U;
    work_ram[0x1A11U] = 1U;
    work_ram[0x1991U] = 1U;
    check(
        dc_classify_walled_star_access_controller(
            work_ram.data(), work_ram.size(), 0x11U, &controller)
                != 0
            && controller.path == DC_WALLED_STAR_ACCESS_CONTROLLER_ADVANCE
            && controller.decremented_timer == 0U
            && controller.advanced_appearance == 2U,
        "native 4B3F identifies the Walled-Star access appearance advance");

    work_ram[0x1991U] = 2U;
    check(
        dc_classify_walled_star_access_controller(
            work_ram.data(), work_ram.size(), 0x11U, &controller)
                != 0
            && controller.path == DC_WALLED_STAR_ACCESS_CONTROLLER_RESET,
        "native 4B3F identifies the Walled-Star access appearance reset");

    work_ram[0x1811U] = 0x80U;
    check(
        dc_classify_walled_star_access_controller(
            work_ram.data(), work_ram.size(), 0x11U, &controller)
                != 0
            && controller.path == DC_WALLED_STAR_ACCESS_CONTROLLER_COLLISION,
        "native 4B3F keeps the Walled-Star access collision in assembly");
}

void test_native_walled_star_access_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_walled_star_access_initializer initializer{};

    check(
        dc_classify_walled_star_access_initializer(
            work_ram.data(), work_ram.size(), 0x11U, &initializer)
            != 0
            && initializer.slot == 0x11U,
        "native 4A44 identifies the fixed Walled-Star access slot");
    check(
        dc_classify_walled_star_access_initializer(
            work_ram.data(), work_ram.size(), 0x10U, &initializer)
            == 0,
        "native 4A44 rejects the adjacent Side-Star access slot");
    check(
        dc_classify_walled_star_access_initializer(
            work_ram.data(), 0x1F91U, 0x11U, &initializer)
            == 0,
        "native 4A44 rejects a truncated work-RAM snapshot");
}

void test_native_side_star_initializers()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_side_star_initializer initializer{};

    check(
        dc_classify_side_star_initializer(
            work_ram.data(), work_ram.size(), 0x10U, &initializer)
            != 0
            && initializer.slot == 0x10U && initializer.appearance == 0x1CU
            && initializer.first_dispatch == 0x4BBCU
            && initializer.steady_dispatch == 0x4BD5U,
        "native 4A54 prepares the lower-left Side-Star setup");
    check(
        dc_classify_side_star_initializer(
            work_ram.data(), work_ram.size(), 0x12U, &initializer)
            != 0
            && initializer.slot == 0x12U && initializer.appearance == 0x0AU
            && initializer.first_dispatch == 0x4BF1U
            && initializer.steady_dispatch == 0x4C0AU,
        "native 4A4C prepares the upper-right Side-Star setup");
    check(
        dc_classify_side_star_initializer(
            work_ram.data(), work_ram.size(), 0x11U, &initializer)
            == 0,
        "native Side-Star initializers reject the Walled-Star access slot");
    check(
        dc_classify_side_star_initializer(
            work_ram.data(), 0x1F92U, 0x10U, &initializer)
            == 0,
        "native Side-Star initializers reject a truncated work-RAM snapshot");
}

void test_native_side_star_access_controller()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_side_star_access_controller controller{};

    work_ram[0x1810U] = 0x00U;
    work_ram[0x1A10U] = 1U;
    work_ram[0x1990U] = 1U;
    check(
        dc_classify_side_star_access_controller(
            work_ram.data(), work_ram.size(), 0x10U, &controller)
                != 0
            && controller.path == DC_SIDE_STAR_ACCESS_CONTROLLER_ADVANCE
            && controller.decremented_timer == 0U
            && controller.advanced_appearance == 2U,
        "native Side-Star controller identifies the lower-left appearance advance");

    work_ram[0x1A12U] = 2U;
    work_ram[0x1992U] = 2U;
    check(
        dc_classify_side_star_access_controller(
            work_ram.data(), work_ram.size(), 0x12U, &controller)
                != 0
            && controller.path == DC_SIDE_STAR_ACCESS_CONTROLLER_WAITING
            && controller.decremented_timer == 1U,
        "native Side-Star controller identifies the upper-right timer wait");

    work_ram[0x1A12U] = 1U;
    check(
        dc_classify_side_star_access_controller(
            work_ram.data(), work_ram.size(), 0x12U, &controller)
                != 0
            && controller.path == DC_SIDE_STAR_ACCESS_CONTROLLER_RESET,
        "native Side-Star controller identifies the upper-right appearance reset");

    work_ram[0x1812U] = 0x80U;
    check(
        dc_classify_side_star_access_controller(
            work_ram.data(), work_ram.size(), 0x12U, &controller)
                != 0
            && controller.path == DC_SIDE_STAR_ACCESS_CONTROLLER_COLLISION,
        "native Side-Star controller leaves collision outcomes in assembly");
}

void test_native_four_star_wall_pocket_animation()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_four_star_wall_pocket_animation animation{};

    work_ram[0x1A3EU] = 2U;
    work_ram[0x1980U] = 4U;
    check(
        dc_classify_four_star_wall_pocket_animation(
            work_ram.data(), work_ram.size(), 0x3EU, &animation)
                != 0
            && animation.path == DC_FOUR_STAR_WALL_POCKET_ANIMATION_WAITING
            && animation.decremented_timer == 1U,
        "native 4AEB identifies the Four-Star animation countdown");

    work_ram[0x1A3EU] = 1U;
    check(
        dc_classify_four_star_wall_pocket_animation(
            work_ram.data(), work_ram.size(), 0x3EU, &animation)
                != 0
            && animation.path == DC_FOUR_STAR_WALL_POCKET_ANIMATION_ADVANCE
            && animation.advanced_appearance == 5U,
        "native 4AEB identifies the Four-Star animation phase advance");

    work_ram[0x1980U] = 6U;
    check(
        dc_classify_four_star_wall_pocket_animation(
            work_ram.data(), work_ram.size(), 0x3EU, &animation)
                != 0
            && animation.path
                == DC_FOUR_STAR_WALL_POCKET_ANIMATION_TRANSITION,
        "native 4AEB keeps the untraced scene transition in assembly");
    check(
        dc_classify_four_star_wall_pocket_animation(
            work_ram.data(), 0x1A3EU, 0x3EU, &animation)
                == 0,
        "native 4AEB rejects a truncated animation snapshot");
    check(
        dc_classify_four_star_wall_pocket_animation(
            work_ram.data(), work_ram.size(), 0x80U, &animation)
                == 0,
        "native 4AEB rejects an out-of-range actor slot");
}

void test_native_upper_wall_pocket_hand_initializer()
{
    dc_upper_wall_pocket_hand_initializer initializer{};
    check(
        dc_prepare_upper_wall_pocket_hand_initializer(0x40U, &initializer)
                != 0
            && initializer.slot == 0x40U
            && initializer.live_dispatch == 0x5975U,
        "native 5972 prepares the upper-left Wall-Pocket hand dispatch");
    check(
        dc_prepare_upper_wall_pocket_hand_initializer(0x41U, &initializer)
                != 0
            && initializer.slot == 0x41U
            && initializer.live_dispatch == 0x5975U,
        "native 5972 prepares the upper-right Wall-Pocket hand dispatch");
    check(
        dc_prepare_upper_wall_pocket_hand_initializer(0x42U, &initializer)
                == 0,
        "native 5972 rejects a non-hand actor slot");
    check(
        dc_prepare_upper_wall_pocket_hand_initializer(0x40U, nullptr) == 0,
        "native 5972 rejects a null initializer output");
}

void test_native_upper_wall_pocket_hand_live_setup()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_upper_wall_pocket_hand_live_setup setup{};

    work_ram[0x10C0U] = 0x82U;
    work_ram[0x0080U] = 0xA5U; // Deliberately differs from the live bus.
    check(
        dc_prepare_upper_wall_pocket_hand_live_setup(
            work_ram.data(), work_ram.size(), 0x40U, 0x73U, 0x05U, &setup)
                != 0
            && setup.slot == 0x40U
            && setup.collision_flags == 0x82U
            && setup.hidden_collision_flags == 0xC2U
            && setup.masked_rng == 0x33U
            && setup.timer_low == 0x38U
            && setup.timer_high == 0x01U
            && setup.countdown_dispatch == 0x5991U,
        "native 5975 hides a routed hand and derives its delayed timer");
    check(
        dc_prepare_upper_wall_pocket_hand_live_setup(
            work_ram.data(), work_ram.size(), 0x42U, 0x73U, 0x05U, &setup)
                == 0,
        "native 5975 rejects a non-hand actor slot");
    check(
        dc_prepare_upper_wall_pocket_hand_live_setup(
            work_ram.data(), 0x10C0U, 0x40U, 0x73U, 0x05U, &setup)
                == 0,
        "native 5975 rejects a truncated collision snapshot");
    check(
        dc_prepare_upper_wall_pocket_hand_live_setup(
            work_ram.data(), work_ram.size(), 0x40U, 0x73U, 0x05U, nullptr)
                == 0,
        "native 5975 rejects a null setup output");
}

void test_native_upper_wall_pocket_hand_countdown()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_upper_wall_pocket_hand_countdown countdown{};

    work_ram[0x1B40U] = 0x33U;
    work_ram[0x1BC0U] = 0x01U;
    check(
        dc_classify_upper_wall_pocket_hand_countdown(
            work_ram.data(), work_ram.size(), 0x40U, &countdown)
                != 0
            && countdown.path == DC_UPPER_WALL_POCKET_HAND_COUNTDOWN_WAITING
            && countdown.decremented_low == 0x32U
            && countdown.decremented_high == 0x01U
            && countdown.remaining_timer == 0x33U,
        "native 5991 decrements an upper-hand timer without borrowing");

    work_ram[0x1B40U] = 0x00U;
    work_ram[0x1BC0U] = 0x01U;
    check(
        dc_classify_upper_wall_pocket_hand_countdown(
            work_ram.data(), work_ram.size(), 0x40U, &countdown)
                != 0
            && countdown.path == DC_UPPER_WALL_POCKET_HAND_COUNTDOWN_WAITING
            && countdown.decremented_low == 0xFFU
            && countdown.decremented_high == 0x00U
            && countdown.remaining_timer == 0xFFU,
        "native 5991 borrows from the high timer byte");

    work_ram[0x1B40U] = 0x01U;
    work_ram[0x1BC0U] = 0x00U;
    check(
        dc_classify_upper_wall_pocket_hand_countdown(
            work_ram.data(), work_ram.size(), 0x40U, &countdown)
                != 0
            && countdown.path == DC_UPPER_WALL_POCKET_HAND_COUNTDOWN_HANDOFF
            && countdown.remaining_timer == 0x00U,
        "native 5991 identifies the scheduler-backed expiry handoff");

    check(
        dc_classify_upper_wall_pocket_hand_countdown(
            work_ram.data(), work_ram.size(), 0x42U, &countdown)
                == 0,
        "native 5991 rejects a non-hand actor slot");
    check(
        dc_classify_upper_wall_pocket_hand_countdown(
            work_ram.data(), 0x1BC0U, 0x40U, &countdown)
                == 0,
        "native 5991 rejects a truncated timer snapshot");
    check(
        dc_classify_upper_wall_pocket_hand_countdown(
            work_ram.data(), work_ram.size(), 0x40U, nullptr)
                == 0,
        "native 5991 rejects a null countdown output");
}

void test_native_walled_star_hit_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_walled_star_hit_gate gate{};

    work_ram[0x185BU] = 0x10U;
    work_ram[0x0DBCU] = 0U;
    check(
        dc_classify_walled_star_hit_gate(
            work_ram.data(),
            work_ram.size(),
            0x5BU,
            &gate)
            != 0
            && gate.path == DC_WALLED_STAR_HIT_GATE_IDLE
            && gate.slot == 0x5BU
            && gate.collision_state == 0x10U
            && gate.shared_access_latch == 0U,
        "native 6339 identifies the idle wall return");

    work_ram[0x185BU] = 0x90U;
    check(
        dc_classify_walled_star_hit_gate(
            work_ram.data(),
            work_ram.size(),
            0x5BU,
            &gate)
            != 0
            && gate.path == DC_WALLED_STAR_HIT_GATE_COLLISION,
        "native 6339 identifies the pending wall collision");

    work_ram[0x185BU] = 0x10U;
    work_ram[0x0DBCU] = 0x01U;
    check(
        dc_classify_walled_star_hit_gate(
            work_ram.data(),
            work_ram.size(),
            0x5BU,
            &gate)
            != 0
            && gate.path == DC_WALLED_STAR_HIT_GATE_SHARED_ACTIVE,
        "native 6339 identifies the shared access path");

    check(
        dc_classify_walled_star_hit_gate(
            work_ram.data(),
            0x185BU,
            0x5BU,
            &gate)
            == 0,
        "native 6339 rejects a truncated work-RAM snapshot");
}

void test_native_devil_shooter_companion_dormant()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_devil_shooter_companion_dormant companion{};

    work_ram[0x0053U] = 0x00U;
    check(
        dc_classify_devil_shooter_companion_dormant(
            work_ram.data(), work_ram.size(), 0x0AU, &companion)
                != 0
            && companion.path == DC_DEVIL_SHOOTER_COMPANION_DORMANT_IDLE
            && companion.slot == 0x0AU
            && companion.active_session_bits == 0x00U,
        "native Devil-Shooter left companion identifies the dormant return");

    work_ram[0x0053U] = 0x09U;
    check(
        dc_classify_devil_shooter_companion_dormant(
            work_ram.data(), work_ram.size(), 0x0DU, &companion)
                != 0
            && companion.path == DC_DEVIL_SHOOTER_COMPANION_DORMANT_ACTIVE
            && companion.slot == 0x0DU
            && companion.active_session_bits == 0x08U,
        "native Devil-Shooter right companion identifies the active loop");

    check(
        dc_classify_devil_shooter_companion_dormant(
            work_ram.data(), 0x0053U, 0x0AU, &companion)
            == 0,
        "native Devil-Shooter companion rejects a truncated work-RAM snapshot");
    check(
        dc_classify_devil_shooter_companion_dormant(
            work_ram.data(), work_ram.size(), 0x0BU, &companion)
            == 0,
        "native Devil-Shooter companion rejects a non-companion slot");
}

void test_native_devil_shooter_companion_position()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_devil_shooter_companion_position position{};

    work_ram[0x0D15U] = 0x18U;
    check(
        dc_classify_devil_shooter_companion_position(
            work_ram.data(), work_ram.size(), 0x0BU, &position)
                != 0
            && position.slot == 0x0BU && position.charge == 0x18U
            && position.vertical_low == 0xB4U
            && position.vertical_high == 0x02U
            && position.low_add_carry == 0x00U,
        "native Devil-Shooter charge position derives the low charge range");

    work_ram[0x0D15U] = 0xF0U;
    check(
        dc_classify_devil_shooter_companion_position(
            work_ram.data(), work_ram.size(), 0x0BU, &position)
                != 0
            && position.vertical_low == 0x20U
            && position.vertical_high == 0x03U
            && position.low_add_carry == 0x01U,
        "native Devil-Shooter charge position preserves the add carry");

    check(
        dc_classify_devil_shooter_companion_position(
            work_ram.data(), 0x0D15U, 0x0BU, &position)
            == 0,
        "native Devil-Shooter charge position rejects truncated work RAM");
    check(
        dc_classify_devil_shooter_companion_position(
            work_ram.data(), work_ram.size(), 0x0CU, &position)
            == 0,
        "native Devil-Shooter charge position rejects a non-position slot");
}

void test_native_devil_shooter_companion_appearance()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_devil_shooter_companion_appearance appearance{};

    work_ram[0x0D15U] = 0x27U;
    work_ram[0x1E0CU] = 0x27U;
    work_ram[0x198CU] = 0x01U;
    check(
        dc_classify_devil_shooter_companion_appearance(
            work_ram.data(), work_ram.size(), 0x0CU, &appearance)
                != 0
            && appearance.path == DC_DEVIL_SHOOTER_COMPANION_APPEARANCE_IDLE,
        "native Devil-Shooter charge appearance identifies the cached return");

    work_ram[0x1E0CU] = 0x20U;
    check(
        dc_classify_devil_shooter_companion_appearance(
            work_ram.data(), work_ram.size(), 0x0CU, &appearance)
                != 0
            && appearance.path
                == DC_DEVIL_SHOOTER_COMPANION_APPEARANCE_ADVANCE
            && appearance.advanced_appearance == 0x02U,
        "native Devil-Shooter charge appearance advances its second frame");

    work_ram[0x198CU] = 0x02U;
    check(
        dc_classify_devil_shooter_companion_appearance(
            work_ram.data(), work_ram.size(), 0x0CU, &appearance)
                != 0
            && appearance.path == DC_DEVIL_SHOOTER_COMPANION_APPEARANCE_RESET
            && appearance.advanced_appearance == 0x03U,
        "native Devil-Shooter charge appearance resets after its third frame");

    work_ram[0x198CU] = 0x03U;
    check(
        dc_classify_devil_shooter_companion_appearance(
            work_ram.data(), work_ram.size(), 0x0CU, &appearance)
            == 0,
        "native Devil-Shooter charge appearance rejects an invalid phase");
    check(
        dc_classify_devil_shooter_companion_appearance(
            work_ram.data(), 0x1E0CU, 0x0CU, &appearance)
            == 0,
        "native Devil-Shooter charge appearance rejects truncated work RAM");
    check(
        dc_classify_devil_shooter_companion_appearance(
            work_ram.data(), work_ram.size(), 0x0BU, &appearance)
            == 0,
        "native Devil-Shooter charge appearance rejects a non-appearance slot");
}

void test_native_devil_shooter_companion_active_timer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_devil_shooter_companion_active_timer timer{};

    work_ram[0x1B0DU] = 0x05U;
    work_ram[0x198DU] = 0x00U;
    check(
        dc_classify_devil_shooter_companion_active_timer(
            work_ram.data(), work_ram.size(), 0x0DU, &timer)
                != 0
            && timer.path == DC_DEVIL_SHOOTER_COMPANION_ACTIVE_TIMER_WAIT
            && timer.decremented_timer == 0x04U,
        "native Devil-Shooter active companion decrements a nonzero timer");

    work_ram[0x1B0DU] = 0x01U;
    work_ram[0x198DU] = 0x00U;
    check(
        dc_classify_devil_shooter_companion_active_timer(
            work_ram.data(), work_ram.size(), 0x0DU, &timer)
                != 0
            && timer.path == DC_DEVIL_SHOOTER_COMPANION_ACTIVE_TIMER_ADVANCE
            && timer.advanced_appearance == 0x01U,
        "native Devil-Shooter active companion reloads and advances on expiry");

    work_ram[0x198DU] = 0x04U;
    check(
        dc_classify_devil_shooter_companion_active_timer(
            work_ram.data(), work_ram.size(), 0x0DU, &timer)
                != 0
            && timer.path == DC_DEVIL_SHOOTER_COMPANION_ACTIVE_TIMER_RESET
            && timer.advanced_appearance == 0x05U,
        "native Devil-Shooter active companion resets at its fifth frame");

    work_ram[0x1B0AU] = 0x01U;
    work_ram[0x198AU] = 0x03U;
    check(
        dc_classify_devil_shooter_companion_active_timer(
            work_ram.data(), work_ram.size(), 0x0AU, &timer)
                != 0
            && timer.path == DC_DEVIL_SHOOTER_COMPANION_ACTIVE_TIMER_RESET
            && timer.advanced_appearance == 0x04U,
        "native Devil-Shooter left active companion resets at its fourth frame");

    check(
        dc_classify_devil_shooter_companion_active_timer(
            work_ram.data(), 0x1B0DU, 0x0DU, &timer)
            == 0,
        "native Devil-Shooter active companion rejects truncated work RAM");
    check(
        dc_classify_devil_shooter_companion_active_timer(
            work_ram.data(), work_ram.size(), 0x0CU, &timer)
            == 0,
        "native Devil-Shooter active companion rejects a non-active slot");
}

void test_native_tower_hit_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_tower_hit_gate gate{};

    work_ram[0x182DU] = 0x00U;
    check(
        dc_classify_tower_hit_gate(
            work_ram.data(),
            work_ram.size(),
            0x2DU,
            &gate)
            != 0
            && gate.path == DC_TOWER_HIT_GATE_IDLE
            && gate.slot == 0x2DU
            && gate.collision_state == 0x00U,
        "native 60C3 identifies the idle Tower return");

    work_ram[0x182DU] = 0x80U;
    check(
        dc_classify_tower_hit_gate(
            work_ram.data(),
            work_ram.size(),
            0x2DU,
            &gate)
            != 0
            && gate.path == DC_TOWER_HIT_GATE_TRIGGERED
            && gate.collision_state == 0x80U,
        "native 60C3 identifies the pending Tower collision");

    check(
        dc_classify_tower_hit_gate(
            work_ram.data(),
            0x182DU,
            0x2DU,
            &gate)
            == 0,
        "native 60C3 rejects a truncated work-RAM snapshot");
}

void test_native_tower_bonus_terminal()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_tower_bonus_terminal terminal{};

    work_ram[0x0CFCU] = 0x01U;
    work_ram[0x0D4DU] = 0x01U;
    work_ram[0x182DU] = 0x80U;
    check(
        dc_classify_tower_bonus_terminal(
            work_ram.data(), work_ram.size(), 0x2DU, &terminal)
                != 0
            && terminal.slot == 0x2DU
            && terminal.qualifying_latch == 0x01U
            && terminal.gate_counter == 0x01U
            && terminal.collision_state == 0x80U,
        "native 60F9 identifies the armed final Tower pass");

    work_ram[0x0D4DU] = 0x02U;
    check(
        dc_classify_tower_bonus_terminal(
            work_ram.data(), work_ram.size(), 0x2DU, &terminal)
            == 0,
        "native 60F9 defers a nonterminal Tower gate");
    check(
        dc_classify_tower_bonus_terminal(
            work_ram.data(), 0x182DU, 0x2DU, &terminal)
            == 0,
        "native 60F9 rejects a truncated work-RAM snapshot");
}

void test_native_tower_bonus_continuation()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_tower_bonus_continuation continuation{};

    work_ram[0x182DU] = 0x10U;
    check(
        dc_classify_tower_bonus_continuation(
            work_ram.data(), work_ram.size(), 0x2DU, &continuation)
                != 0
            && continuation.slot == 0x2DU
            && continuation.collision_state == 0x10U,
        "native 6136 identifies the Tower no-hit continuation");

    work_ram[0x182DU] = 0x90U;
    check(
        dc_classify_tower_bonus_continuation(
            work_ram.data(), work_ram.size(), 0x2DU, &continuation)
                != 0
            && continuation.collision_state == 0x90U,
        "native 6136 identifies the Tower stage-selection continuation");
    check(
        dc_classify_tower_bonus_continuation(
            work_ram.data(), 0x182DU, 0x2DU, &continuation)
                == 0,
        "native 6136 rejects a truncated work-RAM snapshot");
}

void test_native_knight_hit_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_knight_hit_gate gate{};

    work_ram[0x183AU] = 0x10U;
    check(
        dc_classify_knight_hit_gate(
            work_ram.data(), work_ram.size(), 0x3AU, &gate)
                != 0
            && gate.path == DC_KNIGHT_HIT_GATE_IDLE
            && gate.slot == 0x3AU && gate.collision_state == 0x10U,
        "native 73D8 identifies the queued-Knight no-hit return");

    work_ram[0x183AU] = 0x90U;
    check(
        dc_classify_knight_hit_gate(
            work_ram.data(), work_ram.size(), 0x3AU, &gate)
                != 0
            && gate.path == DC_KNIGHT_HIT_GATE_TRIGGERED
            && gate.collision_state == 0x90U,
        "native 73D8 identifies the deferred queued-Knight hit path");

    check(
        dc_classify_knight_hit_gate(
            work_ram.data(), 0x183AU, 0x3AU, &gate)
            == 0,
        "native 73D8 rejects a truncated work-RAM snapshot");
    check(
        dc_classify_knight_hit_gate(
            work_ram.data(), work_ram.size(), 0x3EU, &gate)
            == 0,
        "native 73D8 rejects a non-Knight actor slot");
}

void test_native_knight_initializers()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_knight_initializer initializer{};

    check(
        dc_classify_knight_initializer(
            work_ram.data(), work_ram.size(), 0x3AU, &initializer)
                != 0
            && initializer.slot == 0x3AU && initializer.queue_index == 0U,
        "native Knight setup maps slot 3A to queue zero");
    check(
        dc_classify_knight_initializer(
            work_ram.data(), work_ram.size(), 0x3DU, &initializer)
                != 0
            && initializer.slot == 0x3DU && initializer.queue_index == 3U,
        "native Knight setup maps slot 3D to queue three");
    check(
        dc_classify_knight_initializer(
            work_ram.data(), 0x1FBDU, 0x3AU, &initializer)
            == 0,
        "native Knight setup rejects a truncated work-RAM snapshot");
    check(
        dc_classify_knight_initializer(
            work_ram.data(), work_ram.size(), 0x39U, &initializer)
            == 0,
        "native Knight setup rejects a non-Knight actor slot");
}

void test_native_side_spider_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_side_spider_initializer initializer{};

    check(
        dc_classify_side_spider_initializer(
            work_ram.data(), work_ram.size(), 0x28U, 0U, &initializer)
                != 0
            && initializer.slot == 0x28U && initializer.side_parameter == 0U,
        "native Side-Spider setup preserves the left side parameter");
    check(
        dc_classify_side_spider_initializer(
            work_ram.data(), work_ram.size(), 0x29U, 1U, &initializer)
                != 0
            && initializer.slot == 0x29U && initializer.side_parameter == 1U,
        "native Side-Spider setup preserves the right side parameter");
    check(
        dc_classify_side_spider_initializer(
            work_ram.data(), work_ram.size(), 0x29U, 0U, &initializer)
            == 0,
        "native Side-Spider setup rejects a mismatched side parameter");
    check(
        dc_classify_side_spider_initializer(
            work_ram.data(), 0x1FA9U, 0x28U, 0U, &initializer)
            == 0,
        "native Side-Spider setup rejects a truncated work-RAM snapshot");
}

void test_native_front_spider_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_front_spider_initializer initializer{};

    check(
        dc_classify_front_spider_initializer(
            work_ram.data(), work_ram.size(), 0x26U, 0U, &initializer)
                != 0
            && initializer.slot == 0x26U && initializer.side_parameter == 0U,
        "native Front-Spider setup preserves the left side parameter");
    check(
        dc_classify_front_spider_initializer(
            work_ram.data(), work_ram.size(), 0x27U, 1U, &initializer)
                != 0
            && initializer.slot == 0x27U && initializer.side_parameter == 1U,
        "native Front-Spider setup preserves the right side parameter");
    check(
        dc_classify_front_spider_initializer(
            work_ram.data(), work_ram.size(), 0x27U, 0U, &initializer)
            == 0,
        "native Front-Spider setup rejects a mismatched side parameter");
    check(
        dc_classify_front_spider_initializer(
            work_ram.data(), 0x1FA7U, 0x26U, 0U, &initializer)
            == 0,
        "native Front-Spider setup rejects a truncated work-RAM snapshot");
}

void test_native_knight_waiting_parent()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_knight_waiting_parent parent{};

    work_ram[0x1B3AU] = 0x00U;
    work_ram[0x0D06U] = 0x00U;
    work_ram[0x183AU] = 0x10U;
    check(
        dc_classify_knight_waiting_parent(
            work_ram.data(), work_ram.size(), 0x3AU, &parent)
                != 0
            && parent.path == DC_KNIGHT_WAITING_PARENT_WAITING
            && parent.queue_index == 0x00U && parent.door_handshake == 0x00U,
        "native 7378 identifies the queued-Knight waiting parent");

    work_ram[0x0D06U] = 0x01U;
    check(
        dc_classify_knight_waiting_parent(
            work_ram.data(), work_ram.size(), 0x3AU, &parent)
                != 0
            && parent.path == DC_KNIGHT_WAITING_PARENT_ACTIVATING,
        "native 7378 defers the queued-Knight door activation");

    work_ram[0x0D06U] = 0x00U;
    work_ram[0x183AU] = 0x90U;
    check(
        dc_classify_knight_waiting_parent(
            work_ram.data(), work_ram.size(), 0x3AU, &parent)
                != 0
            && parent.path == DC_KNIGHT_WAITING_PARENT_HIT,
        "native 7378 defers the queued-Knight hit path");

    work_ram[0x1B3AU] = 0x04U;
    check(
        dc_classify_knight_waiting_parent(
            work_ram.data(), work_ram.size(), 0x3AU, &parent)
            == 0,
        "native 7378 rejects an invalid queued-Knight index");
}

void test_native_spider_appearance()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_spider_appearance appearance{};

    work_ram[0x1A26U] = 0x03U;
    work_ram[0x19A6U] = 0x01U;
    check(
        dc_classify_spider_appearance(
            work_ram.data(), work_ram.size(), 0x26U, &appearance)
                != 0
            && appearance.path == DC_SPIDER_APPEARANCE_WAITING
            && appearance.decremented_timer == 0x02U,
        "native 65A4 identifies the Spider timer wait");

    work_ram[0x1A26U] = 0x01U;
    check(
        dc_classify_spider_appearance(
            work_ram.data(), work_ram.size(), 0x26U, &appearance)
                != 0
            && appearance.path == DC_SPIDER_APPEARANCE_ADVANCE
            && appearance.advanced_appearance == 0x02U,
        "native 65A4 identifies the Spider phase advance");

    work_ram[0x19A6U] = 0x03U;
    check(
        dc_classify_spider_appearance(
            work_ram.data(), work_ram.size(), 0x26U, &appearance)
                != 0
            && appearance.path == DC_SPIDER_APPEARANCE_RESET
            && appearance.advanced_appearance == 0x04U,
        "native 65A4 identifies the Spider phase reset");
    check(
        dc_classify_spider_appearance(
            work_ram.data(), 0x1A26U, 0x26U, &appearance)
            == 0,
        "native 65A4 rejects a truncated work-RAM snapshot");
    check(
        dc_classify_spider_appearance(
            work_ram.data(), work_ram.size(), 0x2AU, &appearance)
            == 0,
        "native 65A4 rejects a non-Spider actor slot");
}

void test_native_tower_insect_animation()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_tower_insect_animation animation{};

    work_ram[0x1AE4U] = 0x08U;
    work_ram[0x1CE4U] = 0x01U;
    work_ram[0x1A64U] = 0x03U;
    check(
        dc_classify_tower_insect_animation(
            work_ram.data(), work_ram.size(), 0x64U, &animation)
                != 0
            && animation.path == DC_TOWER_INSECT_ANIMATION_WAITING
            && animation.lookup_index == 0x11U && animation.frame == 0x0DU
            && animation.decremented_timer == 0x02U,
        "native 6054 identifies the Tower-insect frame wait");

    work_ram[0x1A64U] = 0x01U;
    check(
        dc_classify_tower_insect_animation(
            work_ram.data(), work_ram.size(), 0x64U, &animation)
                != 0
            && animation.path == DC_TOWER_INSECT_ANIMATION_ADVANCE
            && animation.advanced_phase == 0x02U,
        "native 6054 identifies the Tower-insect phase advance");

    check(
        dc_classify_tower_insect_animation(
            work_ram.data(), 0x1CE4U, 0x64U, &animation)
            == 0,
        "native 6054 rejects a truncated work-RAM snapshot");
    work_ram[0x1AE5U] = 0x0EU;
    work_ram[0x1CE5U] = 0x03U;
    work_ram[0x1A65U] = 0x01U;
    check(
        dc_classify_tower_insect_animation(
            work_ram.data(), work_ram.size(), 0x65U, &animation)
                != 0
            && animation.lookup_index == 0x1FU && animation.frame == 0x16U
            && animation.path == DC_TOWER_INSECT_ANIMATION_ADVANCE,
        "native 6054 shares its valid frame-table leaf with another actor slot");
    work_ram[0x1AE4U] = 0x20U;
    check(
        dc_classify_tower_insect_animation(
            work_ram.data(), work_ram.size(), 0x64U, &animation)
            == 0,
        "native 6054 rejects an out-of-table cadence state");
}

void test_native_small_soldier_source_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_small_soldier_source_gate gate{};

    work_ram[0x0D16U] = 0x05U;
    check(
        dc_classify_small_soldier_source_gate(
            work_ram.data(), work_ram.size(), 0x34U, &gate)
                != 0
            && gate.path == DC_SMALL_SOLDIER_SOURCE_GATE_WAITING
            && gate.slot == 0x34U && gate.group_count == 0x05U,
        "native 7D85 identifies the left Small-Soldier source wait");

    work_ram[0x0D17U] = 0x00U;
    check(
        dc_classify_small_soldier_source_gate(
            work_ram.data(), work_ram.size(), 0x35U, &gate)
                != 0
            && gate.path == DC_SMALL_SOLDIER_SOURCE_GATE_ACTIVE
            && gate.slot == 0x35U && gate.group_count == 0x00U,
        "native 7E23 defers the right Small-Soldier group-completion path");

    check(
        dc_classify_small_soldier_source_gate(
            work_ram.data(), 0x0D16U, 0x34U, &gate)
            == 0,
        "native 7D85 rejects a truncated work-RAM snapshot");
    check(
        dc_classify_small_soldier_source_gate(
            work_ram.data(), work_ram.size(), 0x36U, &gate)
            == 0,
        "native Small-Soldier source gate rejects another actor slot");
}

void test_native_tower_insect_source_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_tower_insect_source_gate gate{};

    work_ram[0x0D2BU] = 0x04U;
    check(
        dc_classify_tower_insect_source_gate(
            work_ram.data(), work_ram.size(), 0x2CU, &gate)
                != 0
            && gate.path == DC_TOWER_INSECT_SOURCE_GATE_WAITING
            && gate.slot == 0x2CU && gate.child_count == 0x04U,
        "native 5F5F identifies the Tower-insect source wait");

    work_ram[0x0D2BU] = 0x00U;
    check(
        dc_classify_tower_insect_source_gate(
            work_ram.data(), work_ram.size(), 0x2CU, &gate)
                != 0
            && gate.path == DC_TOWER_INSECT_SOURCE_GATE_ACTIVE
            && gate.child_count == 0x00U,
        "native 5F5F defers the Tower-insect group-clear path");

    check(
        dc_classify_tower_insect_source_gate(
            work_ram.data(), 0x0D2BU, 0x2CU, &gate)
            == 0,
        "native 5F5F rejects a truncated work-RAM snapshot");
    check(
        dc_classify_tower_insect_source_gate(
            work_ram.data(), work_ram.size(), 0x2DU, &gate)
            == 0,
        "native 5F5F rejects the adjacent Tower slot");
}

void test_native_tower_insect_source_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_tower_insect_source_initializer initializer{};

    check(
        dc_classify_tower_insect_source_initializer(
            work_ram.data(), work_ram.size(), 0x2CU, &initializer)
                != 0
            && initializer.slot == 0x2CU,
        "native 5F22 identifies the fixed Tower-insect source");
    check(
        dc_classify_tower_insect_source_initializer(
            work_ram.data(), 0x1FACU, 0x2CU, &initializer)
            == 0,
        "native 5F22 rejects a truncated work-RAM snapshot");
    check(
        dc_classify_tower_insect_source_initializer(
            work_ram.data(), work_ram.size(), 0x2DU, &initializer)
            == 0,
        "native 5F22 rejects the adjacent Tower slot");
}

void test_native_tower_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_tower_initializer initializer{};

    check(
        dc_classify_tower_initializer(
            work_ram.data(), work_ram.size(), 0x2DU, &initializer)
                != 0
            && initializer.slot == 0x2DU,
        "native 60B3 identifies the fixed Tower");
    check(
        dc_classify_tower_initializer(
            work_ram.data(), 0x1FADU, 0x2DU, &initializer)
            == 0,
        "native 60B3 rejects a truncated work-RAM snapshot");
    check(
        dc_classify_tower_initializer(
            work_ram.data(), work_ram.size(), 0x2CU, &initializer)
            == 0,
        "native 60B3 rejects the Tower-insect source slot");
}

void test_native_bumper_initializers()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_bumper_initializer initializer{};

    check(
        dc_classify_bumper_initializer(
            work_ram.data(), work_ram.size(), 0x14U, &initializer)
                != 0
            && initializer.slot == 0x14U && initializer.side == 0U,
        "native 789A identifies the left fixed Bumper");
    check(
        dc_classify_bumper_initializer(
            work_ram.data(), work_ram.size(), 0x15U, &initializer)
                != 0
            && initializer.slot == 0x15U && initializer.side == 1U,
        "native 78A4 identifies the right fixed Bumper");
    check(
        dc_classify_bumper_initializer(
            work_ram.data(), 0x1F95U, 0x14U, &initializer)
            == 0,
        "native Bumper initializers reject a truncated work-RAM snapshot");
    check(
        dc_classify_bumper_initializer(
            work_ram.data(), work_ram.size(), 0x16U, &initializer)
            == 0,
        "native Bumper initializers reject an adjacent non-Bumper slot");
}

void test_native_bumper_child_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_bumper_child_initializer initializer{};
    work_ram[0x1195U] = 0x4AU;
    work_ram[0x1315U] = 0x5CU;
    work_ram[0x1395U] = 0x02U;
    work_ram[0x1E15U] = 0x01U;

    check(
        dc_classify_bumper_child_initializer(
            work_ram.data(), work_ram.size(), 0x15U, 0x72U, &initializer)
                != 0
            && initializer.parent_slot == 0x15U
            && initializer.child_slot == 0x72U
            && initializer.parent_horizontal_low == 0x4AU
            && initializer.parent_vertical_low == 0x5CU
            && initializer.parent_vertical_high == 0x02U
            && initializer.parent_appearance == 0x01U,
        "native 79DE identifies a terminal Bumper child initializer");
    check(
        dc_classify_bumper_child_initializer(
            work_ram.data(), 0x1FF6U, 0x15U, 0x72U, &initializer)
            == 0,
        "native 79DE rejects a truncated work-RAM snapshot");
    check(
        dc_classify_bumper_child_initializer(
            work_ram.data(), work_ram.size(), 0x14U, 0x72U, &initializer)
            == 0,
        "native 79DE rejects the left Bumper");
    check(
        dc_classify_bumper_child_initializer(
            work_ram.data(), work_ram.size(), 0x15U, 0x70U, &initializer)
            == 0,
        "native 79DE rejects an adjacent non-child slot");
}

void test_native_small_soldier_source_initializers()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_small_soldier_source_initializer initializer{};

    check(
        dc_classify_small_soldier_source_initializer(
            work_ram.data(), work_ram.size(), 0x34U, &initializer)
                != 0
            && initializer.slot == 0x34U && initializer.side == 0U,
        "native 7D4D identifies the left Small-Soldier source");
    check(
        dc_classify_small_soldier_source_initializer(
            work_ram.data(), work_ram.size(), 0x35U, &initializer)
                != 0
            && initializer.slot == 0x35U && initializer.side == 1U,
        "native 7DE9 identifies the right Small-Soldier source");
    check(
        dc_classify_small_soldier_source_initializer(
            work_ram.data(), 0x1FB5U, 0x34U, &initializer)
            == 0,
        "native Small-Soldier source initializers reject truncated work RAM");
    check(
        dc_classify_small_soldier_source_initializer(
            work_ram.data(), work_ram.size(), 0x36U, &initializer)
            == 0,
        "native Small-Soldier source initializers reject adjacent slots");
}

void test_native_small_soldier_delayed_spawn_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_small_soldier_delayed_spawn_gate gate{};

    work_ram[0x1A34U] = 0x30U;
    check(
        dc_classify_small_soldier_delayed_spawn_gate(
            work_ram.data(), work_ram.size(), 0x34U, &gate)
                != 0
            && gate.path == DC_SMALL_SOLDIER_DELAYED_SPAWN_GATE_WAITING
            && gate.timer_after == 0x2FU,
        "native 7D6F identifies the left delayed-spawn timer wait");
    work_ram[0x1A35U] = 1U;
    check(
        dc_classify_small_soldier_delayed_spawn_gate(
            work_ram.data(), work_ram.size(), 0x35U, &gate)
                != 0
            && gate.path == DC_SMALL_SOLDIER_DELAYED_SPAWN_GATE_ACTIVE,
        "native 7E0D defers right delayed-spawn expiry");
    check(
        dc_classify_small_soldier_delayed_spawn_gate(
            work_ram.data(), 0x1A35U, 0x35U, &gate)
            == 0,
        "native delayed-spawn gate rejects truncated work RAM");
    check(
        dc_classify_small_soldier_delayed_spawn_gate(
            work_ram.data(), work_ram.size(), 0x36U, &gate)
            == 0,
        "native delayed-spawn gate rejects an adjacent slot");
}

void test_native_giant_face_controller_initializers()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_giant_face_controller_initializer initializer{};

    check(
        dc_classify_giant_face_controller_initializer(
            work_ram.data(), work_ram.size(), 0x36U, 0U, &initializer)
                != 0
            && initializer.slot == 0x36U && initializer.stage == 0U
            && initializer.persisted_form == 0U,
        "native 7428 identifies the first Giant-Face controller stage");
    check(
        dc_classify_giant_face_controller_initializer(
            work_ram.data(), work_ram.size(), 0x36U, 1U, &initializer)
                != 0
            && initializer.stage == 1U,
        "native 742B identifies the second Giant-Face controller stage");
    check(
        dc_classify_giant_face_controller_initializer(
            work_ram.data(), work_ram.size(), 0x36U, 2U, &initializer)
                != 0
            && initializer.stage == 2U,
        "native 742E identifies empty-form Giant-Face finalization");

    work_ram[0x0258U] = 1U;
    check(
        dc_classify_giant_face_controller_initializer(
            work_ram.data(), work_ram.size(), 0x36U, 2U, &initializer)
            == 0,
        "native 742E defers persisted-form restoration");
    check(
        dc_classify_giant_face_controller_initializer(
            work_ram.data(), 0x1FB6U, 0x36U, 0U, &initializer)
            == 0,
        "native Giant-Face controller setup rejects truncated work RAM");
    check(
        dc_classify_giant_face_controller_initializer(
            work_ram.data(), work_ram.size(), 0x37U, 0U, &initializer)
            == 0,
        "native Giant-Face controller setup rejects an adjacent slot");
}

void test_native_giant_face_animator_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_giant_face_animator_initializer initializer{};
    check(
        dc_classify_giant_face_animator_initializer(
            work_ram.data(), work_ram.size(), 0x37U, &initializer)
                != 0
            && initializer.slot == 0x37U,
        "native 766C identifies the fixed Giant-Face hidden animator");
    check(
        dc_classify_giant_face_animator_initializer(
            work_ram.data(), 0x1FB7U, 0x37U, &initializer)
            == 0,
        "native Giant-Face hidden animator setup rejects truncated work RAM");
    check(
        dc_classify_giant_face_animator_initializer(
            work_ram.data(), work_ram.size(), 0x36U, &initializer)
            == 0,
        "native Giant-Face hidden animator setup rejects an adjacent slot");
}

void test_native_giant_face_crown_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_giant_face_crown_initializer initializer{};
    check(
        dc_classify_giant_face_crown_initializer(
            work_ram.data(), work_ram.size(), 0x39U, &initializer)
                != 0
            && initializer.slot == 0x39U,
        "native 76F7 identifies the fixed Giant-Face crown surface");
    check(
        dc_classify_giant_face_crown_initializer(
            work_ram.data(), 0x1FB9U, 0x39U, &initializer)
            == 0,
        "native Giant-Face crown setup rejects truncated work RAM");
    check(
        dc_classify_giant_face_crown_initializer(
            work_ram.data(), work_ram.size(), 0x38U, &initializer)
            == 0,
        "native Giant-Face crown setup rejects an adjacent slot");
}

void test_native_giant_face_mouth_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_giant_face_mouth_initializer initializer{};
    check(
        dc_classify_giant_face_mouth_initializer(
            work_ram.data(), work_ram.size(), 0x38U, &initializer)
                != 0
            && initializer.slot == 0x38U,
        "native 7718 identifies the fixed Giant-Face mouth surface");
    check(
        dc_classify_giant_face_mouth_initializer(
            work_ram.data(), 0x1FB8U, 0x38U, &initializer)
            == 0,
        "native Giant-Face mouth setup rejects truncated work RAM");
    check(
        dc_classify_giant_face_mouth_initializer(
            work_ram.data(), work_ram.size(), 0x39U, &initializer)
            == 0,
        "native Giant-Face mouth setup rejects an adjacent slot");
}

void test_native_sorcerer_color_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_sorcerer_color_gate gate{};

    work_ram[0x0D40U] = 0x03U;
    work_ram[0x1996U] = 0x03U;
    check(
        dc_classify_sorcerer_color_gate(
            work_ram.data(), work_ram.size(), 0x16U, &gate)
                != 0
            && gate.path == DC_SORCERER_COLOR_GATE_WAITING
            && gate.slot == 0x16U && gate.shared_color == 0x03U
            && gate.cached_color == 0x03U,
        "native 57E9 identifies the stable Sorcerer color return");

    work_ram[0x1996U] = 0x04U;
    check(
        dc_classify_sorcerer_color_gate(
            work_ram.data(), work_ram.size(), 0x16U, &gate)
                != 0
            && gate.path == DC_SORCERER_COLOR_GATE_ACTIVE
            && gate.cached_color == 0x04U,
        "native 57E9 defers the Sorcerer color-update helper");

    check(
        dc_classify_sorcerer_color_gate(
            work_ram.data(), 0x1996U, 0x16U, &gate)
            == 0,
        "native 57E9 rejects a truncated work-RAM snapshot");
    check(
        dc_classify_sorcerer_color_gate(
            work_ram.data(), work_ram.size(), 0x17U, &gate)
            == 0,
        "native 57E9 rejects another actor slot");
}

void test_native_sorcerer_deployment_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_sorcerer_deployment_initializer initializer{};

    work_ram[0x171AU] = 0x03U;
    work_ram[0x109AU] = 0x80U;
    work_ram[0x0D0BU] = 0x28U;
    work_ram[0x0D0FU] = 0x03U;
    check(
        dc_classify_sorcerer_deployment_initializer(
            work_ram.data(), work_ram.size(), 0x1AU, &initializer)
                != 0
            && initializer.route_index == 0x03U
            && initializer.flags_after == 0xE0U
            && initializer.collision_selector == 0x28U
            && initializer.live_count_after == 0x04U
            && initializer.initial_delay_low == 0x84U
            && initializer.initial_delay_high == 0x00U,
        "native 5808 identifies the fourth Sorcerer deployment setup");

    work_ram[0x171AU] = 0x04U;
    check(
        dc_classify_sorcerer_deployment_initializer(
            work_ram.data(), work_ram.size(), 0x1AU, &initializer)
            == 0,
        "native 5808 rejects a mismatched Sorcerer route index");
    work_ram[0x171AU] = 0x03U;
    check(
        dc_classify_sorcerer_deployment_initializer(
            work_ram.data(), 0x171AU, 0x1AU, &initializer)
            == 0,
        "native 5808 rejects a truncated work-RAM snapshot");
    check(
        dc_classify_sorcerer_deployment_initializer(
            work_ram.data(), work_ram.size(), 0x1FU, &initializer)
            == 0,
        "native 5808 rejects another actor slot");
}

void test_native_sorcerer_staging_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_sorcerer_staging_initializer initializer{};

    check(
        dc_classify_sorcerer_staging_initializer(
            work_ram.data(), work_ram.size(), 0x1CU, 0x05U, &initializer)
                != 0
            && initializer.slot == 0x1CU && initializer.route_index == 0x05U,
        "native 57F8 identifies the sixth Sorcerer staging setup");
    check(
        dc_classify_sorcerer_staging_initializer(
            work_ram.data(), work_ram.size(), 0x1CU, 0x04U, &initializer)
            == 0,
        "native 57F8 rejects a mismatched Sorcerer route index");
    check(
        dc_classify_sorcerer_staging_initializer(
            work_ram.data(), 0x1F9EU, 0x1CU, 0x05U, &initializer)
            == 0,
        "native 57F8 rejects a truncated work-RAM snapshot");
    check(
        dc_classify_sorcerer_staging_initializer(
            work_ram.data(), work_ram.size(), 0x1FU, 0x08U, &initializer)
            == 0,
        "native 57F8 rejects another actor slot");
}

void test_native_dragon_head_hit_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_dragon_head_hit_gate gate{};

    work_ram[0x1813U] = 0x00U;
    check(
        dc_classify_dragon_head_hit_gate(
            work_ram.data(),
            work_ram.size(),
            0x13U,
            &gate)
            != 0
            && gate.path == DC_DRAGON_HEAD_HIT_GATE_IDLE
            && gate.slot == 0x13U
            && gate.collision_state == 0x00U,
        "native 5A4B identifies the idle Dragon's-Head return");

    work_ram[0x1813U] = 0x80U;
    check(
        dc_classify_dragon_head_hit_gate(
            work_ram.data(),
            work_ram.size(),
            0x13U,
            &gate)
            != 0
            && gate.path == DC_DRAGON_HEAD_HIT_GATE_TRIGGERED
            && gate.collision_state == 0x80U,
        "native 5A4B identifies the pending Dragon's-Head collision");

    check(
        dc_classify_dragon_head_hit_gate(
            work_ram.data(),
            0x1813U,
            0x13U,
            &gate)
            == 0,
        "native 5A4B rejects a truncated work-RAM snapshot");
    check(
        dc_classify_dragon_head_hit_gate(
            work_ram.data(),
            work_ram.size(),
            0x80U,
            &gate)
            == 0,
        "native 5A4B rejects an out-of-range actor slot");
}

void test_native_bumper_hit_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_bumper_hit_gate gate{};

    work_ram[0x1814U] = 0x00U;
    check(
        dc_classify_bumper_hit_gate(
            work_ram.data(),
            work_ram.size(),
            0x14U,
            &gate)
            != 0
            && gate.path == DC_BUMPER_HIT_GATE_IDLE
            && gate.slot == 0x14U
            && gate.collision_state == 0x00U,
        "native 78C7 identifies the idle Bumper return");

    work_ram[0x1815U] = 0x80U;
    check(
        dc_classify_bumper_hit_gate(
            work_ram.data(),
            work_ram.size(),
            0x15U,
            &gate)
            != 0
            && gate.path == DC_BUMPER_HIT_GATE_TRIGGERED
            && gate.slot == 0x15U
            && gate.collision_state == 0x80U,
        "native 78C7 identifies the pending Bumper collision");

    check(
        dc_classify_bumper_hit_gate(
            work_ram.data(),
            0x1814U,
            0x14U,
            &gate)
            == 0,
        "native 78C7 rejects a truncated work-RAM snapshot");
    check(
        dc_classify_bumper_hit_gate(
            work_ram.data(),
            work_ram.size(),
            0x80U,
            &gate)
            == 0,
        "native 78C7 rejects an out-of-range actor slot");
}

void test_native_devil_shooter_hit_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_devil_shooter_hit_gate gate{};

    work_ram[0x180EU] = 0x00U;
    check(
        dc_classify_devil_shooter_hit_gate(
            work_ram.data(),
            work_ram.size(),
            0x0EU,
            &gate)
            != 0
            && gate.path == DC_DEVIL_SHOOTER_HIT_GATE_IDLE
            && gate.slot == 0x0EU
            && gate.collision_state == 0x00U,
        "native 5116 identifies the idle Devil-Shooter return");

    work_ram[0x180EU] = 0x80U;
    check(
        dc_classify_devil_shooter_hit_gate(
            work_ram.data(),
            work_ram.size(),
            0x0EU,
            &gate)
            != 0
            && gate.path == DC_DEVIL_SHOOTER_HIT_GATE_TRIGGERED
            && gate.collision_state == 0x80U,
        "native 5116 identifies the pending Devil-Shooter catch");

    check(
        dc_classify_devil_shooter_hit_gate(
            work_ram.data(),
            0x180EU,
            0x0EU,
            &gate)
            == 0,
        "native 5116 rejects a truncated work-RAM snapshot");
    check(
        dc_classify_devil_shooter_hit_gate(
            work_ram.data(),
            work_ram.size(),
            0x80U,
            &gate)
            == 0,
        "native 5116 rejects an out-of-range actor slot");
}

void test_native_devil_shooter_release_delay_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_devil_shooter_release_delay_gate gate{};

    work_ram[0x0254U] = 0U;
    work_ram[0x0251U] = 0U;
    work_ram[0x0013U] = 0U;
    work_ram[0x180EU] = 0U;
    check(
        dc_classify_devil_shooter_release_delay_gate(
            work_ram.data(), work_ram.size(), 0x0EU, &gate)
            != 0
            && gate.returns_idle != 0U
            && gate.active_player == 0U
            && gate.button_input == 0U,
        "native 5159 recognizes the released Button-I idle return");

    work_ram[0x0013U] = 0x01U;
    check(
        dc_classify_devil_shooter_release_delay_gate(
            work_ram.data(), work_ram.size(), 0x0EU, &gate)
            != 0
            && gate.returns_idle == 0U,
        "native 5159 defers the held Button-I path");

    work_ram[0x0013U] = 0U;
    work_ram[0x180EU] = 0x80U;
    check(
        dc_classify_devil_shooter_release_delay_gate(
            work_ram.data(), work_ram.size(), 0x0EU, &gate)
            != 0
            && gate.returns_idle == 0U,
        "native 5159 defers the occupied shooter path");

    work_ram[0x180EU] = 0U;
    work_ram[0x0254U] = 0x01U;
    check(
        dc_classify_devil_shooter_release_delay_gate(
            work_ram.data(), work_ram.size(), 0x0EU, &gate)
            != 0
            && gate.returns_idle == 0U,
        "native 5159 defers alternate table modes");

    check(
        dc_classify_devil_shooter_release_delay_gate(
            work_ram.data(), work_ram.size(), 0x0DU, &gate)
            == 0
            && dc_classify_devil_shooter_release_delay_gate(
                work_ram.data(), 0x180EU, 0x0EU, &gate)
                == 0,
        "native 5159 rejects invalid slots and truncated work RAM");
}

void test_native_devil_shooter_post_launch_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_devil_shooter_post_launch_gate gate{};

    work_ram[0x180EU] = 0x00U;
    work_ram[0x0053U] = 0x08U;
    check(
        dc_classify_devil_shooter_post_launch_gate(
            work_ram.data(), work_ram.size(), 0x0EU, &gate)
            != 0
            && gate.returns_idle != 0U
            && gate.collision_state == 0x00U
            && gate.session_flags == 0x08U,
        "native 525F recognizes the steady post-launch return");

    work_ram[0x180EU] = 0x80U;
    check(
        dc_classify_devil_shooter_post_launch_gate(
            work_ram.data(), work_ram.size(), 0x0EU, &gate)
            != 0
            && gate.returns_idle == 0U,
        "native 525F defers the collision-active path");

    work_ram[0x180EU] = 0x00U;
    work_ram[0x0053U] = 0x02U;
    check(
        dc_classify_devil_shooter_post_launch_gate(
            work_ram.data(), work_ram.size(), 0x0EU, &gate)
            != 0
            && gate.returns_idle == 0U,
        "native 525F defers the launch-request path");

    check(
        dc_classify_devil_shooter_post_launch_gate(
            work_ram.data(), work_ram.size(), 0x0DU, &gate)
            == 0
            && dc_classify_devil_shooter_post_launch_gate(
                   work_ram.data(), 0x180EU, 0x0EU, &gate)
                == 0,
        "native 525F rejects invalid slots and truncated work RAM");
}

void test_native_devil_shooter_charge_entry_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_devil_shooter_charge_entry_gate gate{};

    work_ram[0x180EU] = 0x00U;
    work_ram[0x0053U] = 0x08U;
    check(
        dc_classify_devil_shooter_charge_entry_gate(
            work_ram.data(), work_ram.size(), 0x0EU, &gate)
            != 0
            && gate.path == DC_DEVIL_SHOOTER_CHARGE_ENTRY_IDLE
            && gate.collision_state == 0x00U
            && gate.session_flags == 0x08U,
        "native 5223 recognizes the pre-charge idle return");

    work_ram[0x0053U] = 0x00U;
    check(
        dc_classify_devil_shooter_charge_entry_gate(
            work_ram.data(), work_ram.size(), 0x0EU, &gate)
            != 0
            && gate.path == DC_DEVIL_SHOOTER_CHARGE_ENTRY_SCHEDULE,
        "native 5223 recognizes the charge-controller schedule path");

    work_ram[0x180EU] = 0x80U;
    check(
        dc_classify_devil_shooter_charge_entry_gate(
            work_ram.data(), work_ram.size(), 0x0EU, &gate)
            != 0
            && gate.path == DC_DEVIL_SHOOTER_CHARGE_ENTRY_COLLISION_ACTIVE,
        "native 5223 defers collision-active reset handling");

    check(
        dc_classify_devil_shooter_charge_entry_gate(
            work_ram.data(), work_ram.size(), 0x0DU, &gate)
            == 0
            && dc_classify_devil_shooter_charge_entry_gate(
                   work_ram.data(), 0x180EU, 0x0EU, &gate)
                == 0,
        "native 5223 rejects invalid slots and truncated work RAM");
}

void test_native_devil_shooter_charge_position()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_devil_shooter_charge_position position{};

    work_ram[0x128EU] = 0xC0U;
    work_ram[0x130EU] = 0x78U;
    work_ram[0x138EU] = 0x02U;
    work_ram[0x1E0EU] = 0x78U;
    work_ram[0x1E8EU] = 0x02U;
    check(
        dc_classify_devil_shooter_charge_position(
            work_ram.data(), work_ram.size(), 0x0EU, &position)
            != 0
            && position.vertical_low == 0x00U
            && position.vertical_middle == 0x79U
            && position.vertical_high == 0x02U
            && position.charge == 0x01U && position.appearance == 0U,
        "native 52E3 advances the first Shooter charge position");

    work_ram[0x1E0EU] = 0x72U;
    check(
        dc_classify_devil_shooter_charge_position(
            work_ram.data(), work_ram.size(), 0x0EU, &position)
            != 0
            && position.charge == 0x07U && position.appearance == 1U,
        "native 52E3 selects the first charge appearance");

    work_ram[0x1E0EU] = 0x65U;
    check(
        dc_classify_devil_shooter_charge_position(
            work_ram.data(), work_ram.size(), 0x0EU, &position)
            != 0
            && position.charge == 0x14U && position.appearance == 2U,
        "native 52E3 selects the second charge appearance");

    work_ram[0x1E0EU] = 0x5EU;
    check(
        dc_classify_devil_shooter_charge_position(
            work_ram.data(), work_ram.size(), 0x0EU, &position)
            != 0
            && position.charge == 0x1BU && position.appearance == 3U,
        "native 52E3 selects the third charge appearance");

    work_ram[0x1E0EU] = 0x58U;
    check(
        dc_classify_devil_shooter_charge_position(
            work_ram.data(), work_ram.size(), 0x0EU, &position)
            != 0
            && position.charge == 0x21U && position.appearance == 4U,
        "native 52E3 selects the saturated charge appearance");

    work_ram[0x1E0EU] = 0x50U;
    check(
        dc_classify_devil_shooter_charge_position(
            work_ram.data(), work_ram.size(), 0x0EU, &position)
            != 0
            && position.advanced_vertical_middle == 0x79U
            && position.advanced_vertical_high == 0x02U
            && position.vertical_middle == 0x77U
            && position.vertical_high == 0x02U
            && position.charge == 0x27U
            && position.appearance == 4U
            && position.coordinate_capped != 0U,
        "native 52E3 reproduces the coordinate-clamp branch");
    check(
        dc_classify_devil_shooter_charge_position(
            work_ram.data(), 0x1E0EU, 0x0EU, &position)
            == 0
            && dc_classify_devil_shooter_charge_position(
                   work_ram.data(), work_ram.size(), 0x0DU, &position)
                == 0,
        "native 52E3 rejects truncated work RAM and other slots");
}

void test_native_devil_shooter_charge_retraction()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_devil_shooter_charge_retraction retraction{};

    work_ram[0x128EU] = 0x00U;
    work_ram[0x130EU] = 0x8EU;
    work_ram[0x138EU] = 0x02U;
    work_ram[0x1E0EU] = 0x78U;
    work_ram[0x1E8EU] = 0x02U;
    check(
        dc_classify_devil_shooter_charge_retraction(
            work_ram.data(), work_ram.size(), 0x0EU, &retraction)
            != 0
            && retraction.vertical_low == 0x00U
            && retraction.advanced_vertical_middle == 0x8CU
            && retraction.advanced_vertical_high == 0x02U
            && retraction.vertical_middle == 0x8CU
            && retraction.vertical_high == 0x02U
            && retraction.unclamped_charge == 0x14U
            && retraction.charge == 0x14U && retraction.appearance == 2U
            && retraction.coordinate_clamped == 0U
            && retraction.has_appearance_write != 0U,
        "native 533B retracts the Shooter charge position");

    work_ram[0x1E0EU] = 0x86U;
    check(
        dc_classify_devil_shooter_charge_retraction(
            work_ram.data(), work_ram.size(), 0x0EU, &retraction)
            != 0
            && retraction.charge == 0x06U && retraction.appearance == 0U,
        "native 533B selects the zero appearance range");

    work_ram[0x1E0EU] = 0x7DU;
    check(
        dc_classify_devil_shooter_charge_retraction(
            work_ram.data(), work_ram.size(), 0x0EU, &retraction)
            != 0
            && retraction.charge == 0x0FU && retraction.appearance == 1U,
        "native 533B selects the first retraction appearance");

    work_ram[0x1E0EU] = 0x71U;
    check(
        dc_classify_devil_shooter_charge_retraction(
            work_ram.data(), work_ram.size(), 0x0EU, &retraction)
            != 0
            && retraction.charge == 0x1BU && retraction.appearance == 3U,
        "native 533B selects the third retraction appearance");

    work_ram[0x1E0EU] = 0x6CU;
    check(
        dc_classify_devil_shooter_charge_retraction(
            work_ram.data(), work_ram.size(), 0x0EU, &retraction)
            != 0
            && retraction.charge == 0x20U && retraction.appearance == 4U,
        "native 533B selects the saturated retraction appearance");

    work_ram[0x1E0EU] = 0x8CU;
    check(
        dc_classify_devil_shooter_charge_retraction(
            work_ram.data(), work_ram.size(), 0x0EU, &retraction)
            != 0
            && retraction.charge == 0U
            && retraction.has_appearance_write == 0U,
        "native 533B preserves the zero-charge four-store return");

    work_ram[0x1E8EU] = 0x03U;
    check(
        dc_classify_devil_shooter_charge_retraction(
            work_ram.data(), work_ram.size(), 0x0EU, &retraction)
            != 0
            && retraction.advanced_vertical_middle == 0x8CU
            && retraction.advanced_vertical_high == 0x02U
            && retraction.vertical_middle == 0x8CU
            && retraction.vertical_high == 0x03U
            && retraction.unclamped_charge == 0U
            && retraction.charge == 0U
            && retraction.has_appearance_write == 0U
            && retraction.coordinate_clamped != 0U
            && dc_classify_devil_shooter_charge_retraction(
                   work_ram.data(), 0x1E8EU, 0x0EU, &retraction)
                == 0
            && dc_classify_devil_shooter_charge_retraction(
                   work_ram.data(), work_ram.size(), 0x0DU, &retraction)
                == 0,
        "native 533B reproduces the clamp path and rejects invalid snapshots");

    work_ram[0x1E0EU] = 0x8DU;
    work_ram[0x1E8EU] = 0x02U;
    check(
        dc_classify_devil_shooter_charge_retraction(
            work_ram.data(), work_ram.size(), 0x0EU, &retraction)
            != 0
            && retraction.unclamped_charge == 0xFFU
            && retraction.charge == 0U
            && retraction.coordinate_clamped != 0U,
        "native 533B carries a charge-byte borrow into the coordinate clamp");
}

void test_native_skeleton_insect_left_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_skeleton_insect_left_gate gate{};

    work_ram[0x0D13U] = 0x00U;
    check(
        dc_classify_skeleton_insect_left_gate(
            work_ram.data(),
            work_ram.size(),
            0x31U,
            &gate)
            != 0
            && gate.path == DC_SKELETON_INSECT_LEFT_GATE_IDLE
            && gate.slot == 0x31U
            && gate.trigger_latch == 0x00U,
        "native 54B9 identifies the idle left Skeleton-Insect return");

    work_ram[0x0D13U] = 0x01U;
    check(
        dc_classify_skeleton_insect_left_gate(
            work_ram.data(),
            work_ram.size(),
            0x31U,
            &gate)
            != 0
            && gate.path == DC_SKELETON_INSECT_LEFT_GATE_TRIGGERED
            && gate.trigger_latch == 0x01U,
        "native 54B9 identifies the pending left Skeleton-Insect award");

    check(
        dc_classify_skeleton_insect_left_gate(
            work_ram.data(),
            0x0D13U,
            0x31U,
            &gate)
            == 0,
        "native 54B9 rejects a truncated work-RAM snapshot");
    check(
        dc_classify_skeleton_insect_left_gate(
            work_ram.data(),
            work_ram.size(),
            0x80U,
            &gate)
            == 0,
        "native 54B9 rejects an out-of-range actor slot");
}

void test_native_skeleton_insect_right_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_skeleton_insect_right_gate gate{};

    work_ram[0x0D14U] = 0x00U;
    check(
        dc_classify_skeleton_insect_right_gate(
            work_ram.data(),
            work_ram.size(),
            0x33U,
            &gate)
            != 0
            && gate.path == DC_SKELETON_INSECT_RIGHT_GATE_IDLE
            && gate.slot == 0x33U
            && gate.trigger_latch == 0x00U,
        "native 5551 identifies the idle right Skeleton-Insect return");

    work_ram[0x0D14U] = 0x01U;
    check(
        dc_classify_skeleton_insect_right_gate(
            work_ram.data(),
            work_ram.size(),
            0x33U,
            &gate)
            != 0
            && gate.path == DC_SKELETON_INSECT_RIGHT_GATE_TRIGGERED
            && gate.trigger_latch == 0x01U,
        "native 5551 identifies the pending right Skeleton-Insect award");

    check(
        dc_classify_skeleton_insect_right_gate(
            work_ram.data(),
            0x0D14U,
            0x33U,
            &gate)
            == 0,
        "native 5551 rejects a truncated work-RAM snapshot");
    check(
        dc_classify_skeleton_insect_right_gate(
            work_ram.data(),
            work_ram.size(),
            0x80U,
            &gate)
            == 0,
        "native 5551 rejects an out-of-range actor slot");
}

void test_native_skeleton_insect_right_post_trigger()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_skeleton_insect_right_post_trigger post_trigger{};

    check(
        dc_classify_skeleton_insect_right_post_trigger(
            work_ram.data(),
            work_ram.size(),
            0x33U,
            &post_trigger)
            != 0
            && post_trigger.path
                == DC_SKELETON_INSECT_RIGHT_POST_TRIGGER_IDLE
            && post_trigger.slot == 0x33U
            && post_trigger.secondary_state == 0x00U
            && post_trigger.tower_state == 0x00U,
        "native 5590 identifies the idle right Skeleton-Insect return");

    work_ram[0x1200U] = 0x01U;
    check(
        dc_classify_skeleton_insect_right_post_trigger(
            work_ram.data(),
            work_ram.size(),
            0x33U,
            &post_trigger)
            != 0
            && post_trigger.path
                == DC_SKELETON_INSECT_RIGHT_POST_TRIGGER_SECONDARY_ACTIVE
            && post_trigger.secondary_state == 0x01U,
        "native 5590 preserves the secondary-active assembly branch");

    work_ram[0x1200U] = 0x00U;
    work_ram[0x0CC7U] = 0x80U;
    check(
        dc_classify_skeleton_insect_right_post_trigger(
            work_ram.data(),
            work_ram.size(),
            0x33U,
            &post_trigger)
            != 0
            && post_trigger.path
                == DC_SKELETON_INSECT_RIGHT_POST_TRIGGER_TOWER_ACTIVE
            && post_trigger.tower_state == 0x80U,
        "native 5590 preserves the Tower-active assembly branch");

    check(
        dc_classify_skeleton_insect_right_post_trigger(
            work_ram.data(),
            0x1200U,
            0x33U,
            &post_trigger)
            == 0,
        "native 5590 rejects a truncated work-RAM snapshot");
    check(
        dc_classify_skeleton_insect_right_post_trigger(
            work_ram.data(),
            work_ram.size(),
            0x32U,
            &post_trigger)
            == 0,
        "native 5590 rejects the wrong actor slot");
}

void test_native_skeleton_insect_center_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_skeleton_insect_center_gate gate{};

    work_ram[0x0CFCU] = 0x00U;
    check(
        dc_classify_skeleton_insect_center_gate(
            work_ram.data(),
            work_ram.size(),
            0x32U,
            &gate)
            != 0
            && gate.path == DC_SKELETON_INSECT_CENTER_GATE_IDLE
            && gate.slot == 0x32U
            && gate.trigger_latch == 0x00U,
        "native 55F9 identifies the idle center Skeleton-Insect return");

    work_ram[0x0CFCU] = 0x01U;
    check(
        dc_classify_skeleton_insect_center_gate(
            work_ram.data(),
            work_ram.size(),
            0x32U,
            &gate)
            != 0
            && gate.path == DC_SKELETON_INSECT_CENTER_GATE_TRIGGERED
            && gate.trigger_latch == 0x01U,
        "native 55F9 identifies the pending center Skeleton-Insect award");

    check(
        dc_classify_skeleton_insect_center_gate(
            work_ram.data(),
            0x0CFCU,
            0x32U,
            &gate)
            == 0,
        "native 55F9 rejects a truncated work-RAM snapshot");
    check(
        dc_classify_skeleton_insect_center_gate(
            work_ram.data(),
            work_ram.size(),
            0x80U,
            &gate)
            == 0,
        "native 55F9 rejects an out-of-range actor slot");
}

void test_native_devils_coffin_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_devils_coffin_gate gate{};

    work_ram[0x0053U] = 0x00U;
    work_ram[0x180FU] = 0x40U;
    check(
        dc_classify_devils_coffin_gate(
            work_ram.data(),
            work_ram.size(),
            0x0FU,
            &gate)
            != 0
            && gate.path == DC_DEVILS_COFFIN_GATE_IDLE
            && gate.slot == 0x0FU
            && gate.session_state == 0x00U
            && gate.collision_state == 0x40U,
        "native 63F1 identifies the idle Devil's-Coffin return");

    work_ram[0x0053U] = 0x08U;
    check(
        dc_classify_devils_coffin_gate(
            work_ram.data(),
            work_ram.size(),
            0x0FU,
            &gate)
            != 0
            && gate.path == DC_DEVILS_COFFIN_GATE_TYPE_REFRESH
            && gate.session_state == 0x08U,
        "native 63F1 identifies the Devil's-Coffin type refresh");

    work_ram[0x180FU] = 0xC0U;
    check(
        dc_classify_devils_coffin_gate(
            work_ram.data(),
            work_ram.size(),
            0x0FU,
            &gate)
            != 0
            && gate.path == DC_DEVILS_COFFIN_GATE_COLLISION
            && gate.collision_state == 0xC0U,
        "native 63F1 identifies the Devil's-Coffin collision path");

    check(
        dc_classify_devils_coffin_gate(
            work_ram.data(),
            0x180FU,
            0x0FU,
            &gate)
            == 0,
        "native 63F1 rejects a truncated work-RAM snapshot");
    check(
        dc_classify_devils_coffin_gate(
            work_ram.data(),
            work_ram.size(),
            0x80U,
            &gate)
            == 0,
        "native 63F1 rejects an out-of-range actor slot");
}

void test_native_devils_coffin_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_devils_coffin_initializer initializer{};

    check(
        dc_classify_devils_coffin_initializer(
            work_ram.data(), work_ram.size(), 0x0FU, &initializer)
                != 0
            && initializer.slot == 0x0FU,
        "native 63E1 identifies the fixed Devil's-Coffin slot");
    check(
        dc_classify_devils_coffin_initializer(
            work_ram.data(), work_ram.size(), 0x10U, &initializer)
            == 0,
        "native 63E1 rejects a non-Coffin slot");
    check(
        dc_classify_devils_coffin_initializer(
            work_ram.data(), 0x198FU, 0x0FU, &initializer)
            == 0,
        "native 63E1 rejects truncated work RAM");
}

void test_native_devils_coffin_opening()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_devils_coffin_opening opening{};

    work_ram[0x1A0FU] = 0x04U;
    check(
        dc_classify_devils_coffin_opening(
            work_ram.data(),
            work_ram.size(),
            0x0FU,
            &opening)
            != 0
            && opening.path == DC_DEVILS_COFFIN_OPENING_WAITING
            && opening.slot == 0x0FU
            && opening.timer == 0x04U
            && opening.decremented_timer == 0x03U,
        "native 640A identifies a waiting Devil's-Coffin opening tick");

    work_ram[0x1A0FU] = 0x01U;
    work_ram[0x180FU] = 0x80U;
    work_ram[0x198FU] = 0x01U;
    check(
        dc_classify_devils_coffin_opening(
            work_ram.data(),
            work_ram.size(),
            0x0FU,
            &opening)
            != 0
            && opening.path == DC_DEVILS_COFFIN_OPENING_ADVANCE
            && opening.decremented_timer == 0x00U
            && opening.collision_state == 0x80U
            && opening.appearance == 0x01U
            && opening.advanced_appearance == 0x02U,
        "native 640A identifies an appearance-advance tick");

    work_ram[0x198FU] = 0x02U;
    check(
        dc_classify_devils_coffin_opening(
            work_ram.data(),
            work_ram.size(),
            0x0FU,
            &opening)
            != 0
            && opening.path == DC_DEVILS_COFFIN_OPENING_HANDOFF
            && opening.advanced_appearance == 0x03U,
        "native 640A leaves the reverse-pass handoff classified separately");

    work_ram[0x1A0FU] = 0x00U;
    check(
        dc_classify_devils_coffin_opening(
            work_ram.data(),
            work_ram.size(),
            0x0FU,
            &opening)
            != 0
            && opening.path == DC_DEVILS_COFFIN_OPENING_WAITING
            && opening.decremented_timer == 0xFFU,
        "native 640A preserves the indexed-decrement wraparound");

    check(
        dc_classify_devils_coffin_opening(
            work_ram.data(),
            0x1A0FU,
            0x0FU,
            &opening)
            == 0,
        "native 640A rejects a truncated work-RAM snapshot");
    check(
        dc_classify_devils_coffin_opening(
            work_ram.data(),
            work_ram.size(),
            0x80U,
            &opening)
            == 0,
        "native 640A rejects an out-of-range actor slot");
}

void test_native_devils_coffin_closing()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_devils_coffin_closing closing{};

    work_ram[0x1A0FU] = 0x04U;
    check(
        dc_classify_devils_coffin_closing(
            work_ram.data(),
            work_ram.size(),
            0x0FU,
            &closing)
            != 0
            && closing.path == DC_DEVILS_COFFIN_CLOSING_WAITING
            && closing.slot == 0x0FU
            && closing.timer == 0x04U
            && closing.decremented_timer == 0x03U,
        "native 6445 identifies a waiting Devil's-Coffin closing tick");

    work_ram[0x1A0FU] = 0x01U;
    work_ram[0x180FU] = 0x23U;
    work_ram[0x198FU] = 0x03U;
    work_ram[0x0053U] = 0xABU;
    check(
        dc_classify_devils_coffin_closing(
            work_ram.data(),
            work_ram.size(),
            0x0FU,
            &closing)
            != 0
            && closing.path == DC_DEVILS_COFFIN_CLOSING_ADVANCE
            && closing.decremented_timer == 0x00U
            && closing.collision_state == 0x23U
            && closing.appearance == 0x03U
            && closing.decremented_appearance == 0x02U
            && closing.session_state == 0xABU
            && closing.closed_session_state == 0xA3U,
        "native 6445 identifies an appearance-decrement tick");

    work_ram[0x198FU] = 0x01U;
    check(
        dc_classify_devils_coffin_closing(
            work_ram.data(),
            work_ram.size(),
            0x0FU,
            &closing)
            != 0
            && closing.path == DC_DEVILS_COFFIN_CLOSING_COMPLETE
            && closing.decremented_appearance == 0x00U
            && closing.closed_session_state == 0xA3U,
        "native 6445 identifies the final closure transition");

    work_ram[0x1A0FU] = 0x00U;
    check(
        dc_classify_devils_coffin_closing(
            work_ram.data(),
            work_ram.size(),
            0x0FU,
            &closing)
            != 0
            && closing.path == DC_DEVILS_COFFIN_CLOSING_WAITING
            && closing.decremented_timer == 0xFFU,
        "native 6445 preserves the indexed-decrement wraparound");

    check(
        dc_classify_devils_coffin_closing(
            work_ram.data(),
            0x1A0FU,
            0x0FU,
            &closing)
            == 0,
        "native 6445 rejects a truncated work-RAM snapshot");
    check(
        dc_classify_devils_coffin_closing(
            work_ram.data(),
            work_ram.size(),
            0x80U,
            &closing)
            == 0,
        "native 6445 rejects an out-of-range actor slot");
}

void test_native_tower_guard_selector()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_tower_guard_selector selector{};

    work_ram[0x0D4DU] = 0x05U;
    check(
        dc_classify_tower_guard_selector(
            work_ram.data(),
            work_ram.size(),
            0x2EU,
            &selector)
            != 0
            && selector.path == DC_TOWER_GUARD_SELECTOR_CLOSED
            && selector.slot == 0x2EU
            && selector.gate_counter == 0x05U
            && selector.selector == 0x84U,
        "native 60A6 identifies the closed Tower Guard selector");

    work_ram[0x0D4DU] = 0x00U;
    check(
        dc_classify_tower_guard_selector(
            work_ram.data(),
            work_ram.size(),
            0x2EU,
            &selector)
            != 0
            && selector.path == DC_TOWER_GUARD_SELECTOR_OPEN
            && selector.gate_counter == 0x00U
            && selector.selector == 0x8EU,
        "native 60A6 identifies the open Tower Guard selector");

    check(
        dc_classify_tower_guard_selector(
            work_ram.data(),
            0x0D4DU,
            0x2EU,
            &selector)
            == 0,
        "native 60A6 rejects a truncated work-RAM snapshot");
}

void test_native_fixed_response_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_fixed_response_gate gate{};

    work_ram[0x0CC0U] = 0x00U;
    work_ram[0x00CCU] = 0x00U;
    check(
        dc_classify_fixed_response_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
            != 0
            && gate.path == DC_FIXED_RESPONSE_GATE_DORMANT
            && gate.scene_state == 0x00U
            && gate.collision_scratch == 0x00U,
        "native 7CAF identifies the dormant response return");

    work_ram[0x0CC0U] = 0x80U;
    work_ram[0x00CCU] = 0x31U;
    check(
        dc_classify_fixed_response_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
            != 0
            && gate.path == DC_FIXED_RESPONSE_GATE_INACTIVE
            && gate.scene_state == 0x80U
            && gate.collision_scratch == 0x31U,
        "native 7CAF identifies the inactive scene path");

    work_ram[0x0CC0U] = 0x01U;
    check(
        dc_classify_fixed_response_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
            != 0
            && gate.path == DC_FIXED_RESPONSE_GATE_ACTIVE
            && gate.scene_state == 0x01U
            && gate.collision_scratch == 0x31U,
        "native 7CAF identifies the active response path");

    check(
        dc_classify_fixed_response_gate(
            work_ram.data(),
            0x0CC0U,
            &gate)
            == 0,
        "native 7CAF rejects a truncated work-RAM snapshot");
}

void test_native_fixed_scheduler_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_fixed_scheduler_gate gate{};

    work_ram[0x0CC0U] = 0x00U;
    work_ram[0x0BB9U] = 0xFFU;
    work_ram[0x1A5CU] = 0x08U;
    check(
        dc_classify_fixed_scheduler_gate(
            work_ram.data(),
            work_ram.size(),
            0x5CU,
            &gate)
            != 0
            && gate.path == DC_FIXED_SCHEDULER_GATE_SUPPRESSED
            && gate.slot == 0x5CU
            && gate.scene_state == 0x00U
            && gate.presentation_gate == 0xFFU
            && gate.timer == 0x08U
            && gate.decremented_timer == 0x07U,
        "native 7B27 identifies the presentation-suppressed return");

    work_ram[0x0BB9U] = 0x00U;
    check(
        dc_classify_fixed_scheduler_gate(
            work_ram.data(),
            work_ram.size(),
            0x5CU,
            &gate)
            != 0
            && gate.path == DC_FIXED_SCHEDULER_GATE_WAITING
            && gate.decremented_timer == 0x07U,
        "native 7B27 identifies a non-expiring timer decrement");

    work_ram[0x1A5CU] = 0x01U;
    check(
        dc_classify_fixed_scheduler_gate(
            work_ram.data(),
            work_ram.size(),
            0x5CU,
            &gate)
            != 0
            && gate.path == DC_FIXED_SCHEDULER_GATE_ACTIVE
            && gate.decremented_timer == 0x00U,
        "native 7B27 identifies an expiring scheduler tick");

    work_ram[0x0CC0U] = 0x80U;
    check(
        dc_classify_fixed_scheduler_gate(
            work_ram.data(),
            work_ram.size(),
            0x5CU,
            &gate)
            != 0
            && gate.path == DC_FIXED_SCHEDULER_GATE_INACTIVE,
        "native 7B27 identifies the inactive scene path");

    check(
        dc_classify_fixed_scheduler_gate(
            work_ram.data(),
            0x1A5CU,
            0x5CU,
            &gate)
            == 0,
        "native 7B27 rejects a truncated work-RAM snapshot");
}

void test_native_fixed_scheduler_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_fixed_scheduler_initializer initializer{};

    check(
        dc_classify_fixed_scheduler_initializer(
            work_ram.data(), work_ram.size(), 0x5CU, &initializer)
                != 0
            && initializer.slot == 0x5CU,
        "native 7B1F identifies the fixed scheduler slot");
    check(
        dc_classify_fixed_scheduler_initializer(
            work_ram.data(), work_ram.size(), 0x5DU, &initializer)
            == 0,
        "native 7B1F rejects a non-scheduler slot");
    check(
        dc_classify_fixed_scheduler_initializer(
            work_ram.data(), 0x1A5CU, 0x5CU, &initializer)
            == 0,
        "native 7B1F rejects truncated work RAM");
}

void test_native_fixed_marker_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_fixed_marker_initializer initializer{};

    check(
        dc_classify_fixed_marker_initializer(
            work_ram.data(), work_ram.size(), 0x5DU, &initializer)
                != 0
            && initializer.slot == 0x5DU,
        "native 7B6D identifies the fixed marker slot");
    check(
        dc_classify_fixed_marker_initializer(
            work_ram.data(), work_ram.size(), 0x5CU, &initializer)
            == 0,
        "native 7B6D rejects a non-marker slot");
    check(
        dc_classify_fixed_marker_initializer(
            work_ram.data(), 0x1E5DU, 0x5DU, &initializer)
            == 0,
        "native 7B6D rejects truncated work RAM");
}

void test_native_fixed_response_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_fixed_response_initializer initializer{};

    check(
        dc_classify_fixed_response_initializer(
            work_ram.data(), work_ram.size(), 0x5EU, &initializer)
                != 0
            && initializer.slot == 0x5EU,
        "native 7CA7 identifies the fixed response slot");
    check(
        dc_classify_fixed_response_initializer(
            work_ram.data(), work_ram.size(), 0x5DU, &initializer)
            == 0,
        "native 7CA7 rejects a non-response slot");
    check(
        dc_classify_fixed_response_initializer(
            work_ram.data(), 0x0D38U, 0x5EU, &initializer)
            == 0,
        "native 7CA7 rejects truncated work RAM");
}

void test_native_fixed_marker_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_fixed_scheduler_gate gate{};

    work_ram[0x0CC0U] = 0x00U;
    work_ram[0x0BB9U] = 0x00U;
    work_ram[0x1A5DU] = 0x08U;
    check(
        dc_classify_fixed_scheduler_gate(
            work_ram.data(),
            work_ram.size(),
            0x5DU,
            &gate)
            != 0
            && gate.path == DC_FIXED_SCHEDULER_GATE_WAITING
            && gate.slot == 0x5DU
            && gate.timer == 0x08U
            && gate.decremented_timer == 0x07U,
        "native 7B7A classifies the slot-5D marker timer");

    check(
        dc_classify_fixed_scheduler_gate(
            work_ram.data(),
            0x1A5DU,
            0x5DU,
            &gate)
            == 0,
        "native 7B7A rejects a truncated work-RAM snapshot");
}

void test_native_scaled_word_add()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_scaled_word_add addition{};

    work_ram[0x002FU] = 0x97U;
    work_ram[0x0030U] = 0xB3U;
    check(
        dc_add_scaled_byte_to_word(
            work_ram.data(),
            work_ram.size(),
            0x04U,
            &addition)
                != 0
            && addition.input == 0x04U
            && addition.shifted_low[0] == 0x08U
            && addition.shifted_low[4] == 0x80U
            && addition.shifted_high[4] == 0x00U
            && addition.result_low == 0x17U
            && addition.result_high == 0xB4U,
        "native 5955 adds the five-bit-scaled byte to the scratch word");

    work_ram[0x002FU] = 0xF0U;
    work_ram[0x0030U] = 0x12U;
    check(
        dc_add_scaled_byte_to_word(
            work_ram.data(),
            work_ram.size(),
            0xFFU,
            &addition)
                != 0
            && addition.shifted_low[4] == 0xE0U
            && addition.shifted_high[4] == 0x1FU
            && addition.result_low == 0xD0U
            && addition.result_high == 0x32U,
        "native 5955 preserves shift and addition carries");

    check(
        dc_add_scaled_byte_to_word(
            work_ram.data(),
            0x0030U,
            0x01U,
            &addition)
            == 0,
        "native 5955 rejects a truncated work-RAM snapshot");
}

void test_native_dragon_egg_hit_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_dragon_egg_hit_gate gate{};

    work_ram[0x1822U] = 0x00U;
    check(
        dc_classify_dragon_egg_hit_gate(
            work_ram.data(),
            work_ram.size(),
            0x22U,
            &gate)
            != 0
            && gate.path == DC_DRAGON_EGG_HIT_GATE_IDLE
            && gate.slot == 0x22U
            && gate.collision_state == 0x00U,
        "native 4345 identifies the idle egg return");

    work_ram[0x1822U] = 0x80U;
    check(
        dc_classify_dragon_egg_hit_gate(
            work_ram.data(),
            work_ram.size(),
            0x22U,
            &gate)
            != 0
            && gate.path == DC_DRAGON_EGG_HIT_GATE_TRIGGERED
            && gate.collision_state == 0x80U,
        "native 4345 identifies the triggered hit path");

    check(
        dc_classify_dragon_egg_hit_gate(
            work_ram.data(),
            0x1822U,
            0x22U,
            &gate)
            == 0,
        "native 4345 rejects a truncated work-RAM snapshot");
    check(
        dc_classify_dragon_egg_hit_gate(
            work_ram.data(),
            work_ram.size(),
            0x80U,
            &gate)
            == 0,
        "native 4345 rejects an out-of-range actor slot");
}

void test_native_dragon_egg_reset_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_dragon_egg_reset_gate gate{};

    work_ram[0x0CF7U] = 0x80U;
    check(
        dc_classify_dragon_egg_reset_gate(
            work_ram.data(), work_ram.size(), 0x22U, &gate)
            != 0
            && gate.path == DC_DRAGON_EGG_RESET_GATE_IDLE
            && gate.slot == 0x22U
            && gate.group_reset_flags == 0x80U,
        "native 4387 identifies a dormant destroyed egg");

    work_ram[0x0CF7U] = 0x81U;
    check(
        dc_classify_dragon_egg_reset_gate(
            work_ram.data(), work_ram.size(), 0x25U, &gate)
            != 0
            && gate.path == DC_DRAGON_EGG_RESET_GATE_REQUESTED
            && gate.slot == 0x25U
            && gate.group_reset_flags == 0x81U,
        "native 4387 identifies a requested group reset");

    check(
        dc_classify_dragon_egg_reset_gate(
            work_ram.data(), 0x0CF7U, 0x20U, &gate)
            == 0,
        "native 4387 rejects a truncated work-RAM snapshot");
    check(
        dc_classify_dragon_egg_reset_gate(
            work_ram.data(), work_ram.size(), 0x1FU, &gate)
            == 0,
        "native 4387 rejects a slot below the fixed egg cluster");
    check(
        dc_classify_dragon_egg_reset_gate(
            work_ram.data(), work_ram.size(), 0x26U, &gate)
            == 0,
        "native 4387 rejects a slot above the fixed egg cluster");
}

void test_native_giant_door_hit_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_giant_door_hit_gate gate{};

    work_ram[0x184CU] = 0x10U;
    check(
        dc_classify_giant_door_hit_gate(
            work_ram.data(),
            work_ram.size(),
            0x4CU,
            &gate)
            != 0
            && gate.path == DC_GIANT_DOOR_HIT_GATE_IDLE
            && gate.slot == 0x4CU
            && gate.collision_state == 0x10U,
        "native 62B8 identifies the Giant Door idle return");

    work_ram[0x184CU] = 0x90U;
    check(
        dc_classify_giant_door_hit_gate(
            work_ram.data(),
            work_ram.size(),
            0x4CU,
            &gate)
            != 0
            && gate.path == DC_GIANT_DOOR_HIT_GATE_TRIGGERED
            && gate.collision_state == 0x90U,
        "native 62B8 identifies the Giant Door hit path");

    check(
        dc_classify_giant_door_hit_gate(
            work_ram.data(),
            0x184CU,
            0x4CU,
            &gate)
            == 0,
        "native 62B8 rejects a truncated work-RAM snapshot");
    check(
        dc_classify_giant_door_hit_gate(
            work_ram.data(),
            work_ram.size(),
            0x80U,
            &gate)
            == 0,
        "native 62B8 rejects an out-of-range actor slot");
}

void test_native_giant_door_initializer()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_giant_door_initializer initializer{};

    check(
        dc_classify_giant_door_initializer(
            work_ram.data(), work_ram.size(), 0x4CU, &initializer)
            != 0
            && initializer.slot == 0x4CU,
        "native 6294 identifies the fixed Giant Door slot");
    check(
        dc_classify_giant_door_initializer(
            work_ram.data(), work_ram.size(), 0x4DU, &initializer)
            == 0,
        "native 6294 rejects a non-Giant-Door slot");
    check(
        dc_classify_giant_door_initializer(
            work_ram.data(), 0x1FCCU, 0x4CU, &initializer)
            == 0,
        "native 6294 rejects a truncated work-RAM snapshot");
}

void test_native_nudge_input_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_nudge_input_gate lock{};
    work_ram[0x0053U] = 2U;
    check(dc_evaluate_nudge_input_gate(work_ram.data(), work_ram.size(), &lock)
            && lock.branch == DC_NUDGE_GATE_LOCK_RETURN,
        "native C3CB identifies session lock");
    work_ram[0x0053U] = 0U;
    work_ram[0x0CC7U] = 0x80U;
    check(dc_evaluate_nudge_input_gate(work_ram.data(), work_ram.size(), &lock)
            && lock.branch == DC_NUDGE_GATE_LOCK_RETURN,
        "native C3CB identifies tilt lock");
    work_ram[0x0CC7U] = 0U;
    work_ram[0x0053U] = 0x08U;

    dc_nudge_input_gate gate{};
    check(
        dc_evaluate_nudge_input_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
            != 0
            && gate.branch == DC_NUDGE_GATE_SESSION_RETURN
            && gate.session_mask == 0x08U,
        "native C3CB identifies the session-state return");

    work_ram[0x0053U] = 0U;
    work_ram[0x0CC6U] = 0x78U;
    work_ram[0x0CC4U] = 0U;
    work_ram[0x0251U] = 1U;
    work_ram[0x0019U] = 0U;
    check(
        dc_evaluate_nudge_input_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
            != 0
            && gate.branch == DC_NUDGE_GATE_IDLE_RETURN
            && gate.timer_after == 0x77U
            && gate.active_player == 1U
            && gate.matched_input == 0U,
        "native C3CB identifies the timer-decrement idle return");

    work_ram[0x0019U] = 0x02U;
    check(
        dc_evaluate_nudge_input_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
            != 0
            && gate.branch == DC_NUDGE_GATE_DEFER,
        "native C3CB defers an active Button-II path");

    work_ram[0x1000U] = 1U;
    check(
        dc_evaluate_nudge_input_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
            != 0
            && gate.branch == DC_NUDGE_GATE_ACTIVE_START,
        "native C3CB identifies the ordinary active-nudge start");

    work_ram[0x0019U] = 0U;
    work_ram[0x0CC4U] = 1U;
    check(
        dc_evaluate_nudge_input_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
            != 0
            && gate.branch == DC_NUDGE_GATE_ACTIVE_FINISH,
        "native C3CB identifies the ordinary active-nudge finish");

    work_ram[0x0CC4U] = 0U;
    work_ram[0x0CC6U] = 1U;
    check(
        dc_evaluate_nudge_input_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
            != 0
            && gate.branch == DC_NUDGE_GATE_TIMER_RELOAD_RETURN
            && gate.timer_after == 0U,
        "native C3CB identifies the timer-reload idle return");

    work_ram[0x0019U] = 2U;
    work_ram[0x0CC5U] = 0xFEU;
    check(dc_evaluate_nudge_input_gate(work_ram.data(), work_ram.size(), &gate)
            && gate.branch == DC_NUDGE_GATE_ACTIVE_START,
        "native C3CB tests the reset counter on timer-reload active start");
    work_ram[0x0CC4U] = 1U;
    check(dc_evaluate_nudge_input_gate(work_ram.data(), work_ram.size(), &gate)
            && gate.branch == DC_NUDGE_GATE_ACTIVE_FINISH,
        "native C3CB recognizes timer-reload active finish");

    check(
        dc_evaluate_nudge_input_gate(
            work_ram.data(),
            work_ram.size() - 1U,
            &gate)
            == 0,
        "native C3CB rejects a truncated work-RAM snapshot");
}

void test_native_session_input_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    work_ram[0x0251U] = 1U;
    work_ram[0x0019U] = 0x04U;

    dc_session_input_gate gate{};
    check(
        dc_evaluate_session_input_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
            != 0
            && gate.returns_idle != 0U
            && gate.active_player == 1U
            && gate.run_input == 0U,
        "native C49E identifies the ordinary no-Run-input return");

    work_ram[0x0019U] = 0x08U;
    check(
        dc_evaluate_session_input_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
            != 0
            && gate.returns_idle == 0U,
        "native C49E defers a Run-input path");
    work_ram[0x0019U] = 0U;
    work_ram[0x0053U] = 0x02U;
    check(
        dc_evaluate_session_input_gate(
            work_ram.data(),
            work_ram.size(),
            &gate)
            != 0
            && gate.returns_idle == 0U,
        "native C49E defers the alternate session-state return");
    check(
        dc_evaluate_session_input_gate(
            work_ram.data(),
            work_ram.size() - 1U,
            &gate)
            == 0,
        "native C49E rejects a truncated work-RAM snapshot");
}

void test_native_ball_drain_threshold()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    constexpr std::uint8_t slot = 0x00U;
    dc_ball_drain_threshold threshold{};

    work_ram[0x1300U + slot] = 0xCFU;
    work_ram[0x1380U + slot] = 0x02U;
    check(
        dc_classify_ball_drain_threshold(
            work_ram.data(), work_ram.size(), slot, &threshold)
            != 0
            && threshold.path == DC_BALL_DRAIN_THRESHOLD_NORMAL
            && threshold.difference_low == 0xFFU
            && threshold.difference_high == 0xFFU,
        "native DD6E keeps a ball immediately above the drain line in play");

    work_ram[0x1300U + slot] = 0xD0U;
    work_ram[0x1380U + slot] = 0x02U;
    check(
        dc_classify_ball_drain_threshold(
            work_ram.data(), work_ram.size(), slot, &threshold)
            != 0
            && threshold.path == DC_BALL_DRAIN_THRESHOLD_DRAINED
            && threshold.difference_low == 0U
            && threshold.difference_high == 0U,
        "native DD6E admits the exact drain threshold");

    work_ram[0x1300U + slot] = 0x00U;
    work_ram[0x1380U + slot] = 0x03U;
    check(
        dc_classify_ball_drain_threshold(
            work_ram.data(), work_ram.size(), slot, &threshold)
            != 0
            && threshold.path == DC_BALL_DRAIN_THRESHOLD_DRAINED
            && threshold.difference_low == 0x30U
            && threshold.difference_high == 0U,
        "native DD6E preserves the low-byte borrow on a drained ball");

    work_ram[0x0055U] = 1U;
    check(
        dc_classify_ball_drain_threshold(
            work_ram.data(), work_ram.size(), slot, &threshold)
            != 0
            && threshold.path == DC_BALL_DRAIN_THRESHOLD_DEFER,
        "native DD6E defers the alternate session-state threshold");
    check(
        dc_classify_ball_drain_threshold(
            work_ram.data(), work_ram.size() - 1U, slot, &threshold)
            == 0,
        "native DD6E rejects a truncated work-RAM snapshot");
}

void test_native_ball_frame_schedule()
{
    using devils_crush::native_runtime::BallFrameOwner;
    using devils_crush::native_runtime::BallFrameSchedule;
    using devils_crush::native_runtime::BallCollisionShapeSelection;
    using devils_crush::native_runtime::BallInitialTransitionDisposition;
    using devils_crush::native_runtime::BallState;
    using devils_crush::native_runtime::Fixed24_8;
    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::capture_ball_frame_schedule;
    using devils_crush::native_runtime::advance_initial_ball_frame_schedule;
    using devils_crush::native_runtime::classify_ball_initial_transition;

    std::array<std::byte, GameStateView::kWorkRamSize> work_ram{};
    const auto store = [&work_ram](
                           const std::size_t offset,
                           const std::uint8_t value) {
        work_ram[offset] = static_cast<std::byte>(value);
    };
    const auto set_handler = [&store](const std::uint16_t handler) {
        store(0x1F00U, static_cast<std::uint8_t>(handler & 0x00FFU));
        store(0x1F80U, static_cast<std::uint8_t>(handler >> 8U));
    };
    store(0x1000U, 0x80U);
    const GameStateView state(work_ram);

    set_handler(0xC721U);
    store(0x0056U, 0x01U);
    store(0x0057U, 0x04U);
    store(0x00B9U, 0x40U);
    store(0x00BAU, 0x01U);
    store(0x00BBU, 0x80U);
    store(0x00BCU, 0xFFU);
    check(
        capture_ball_frame_schedule(state).owner
            == BallFrameOwner::normal_motion,
        "native ball frame schedule admits the clear resumed substep loop");
    check(
        capture_ball_frame_schedule(state).source_handler_address == 0xC721U,
        "native ball frame schedule retains an unported continuation only at its capture boundary");
    const auto resumed_schedule = capture_ball_frame_schedule(state);
    check(
        resumed_schedule.substep_progress
            == devils_crush::native_runtime::BallSubstepProgress{
                .increment = {.horizontal = 0x0140, .vertical = -128},
                .completed_steps = 1U,
                .requested_steps = 4U,
            },
        "native ball frame schedule retains the prepared C721 substep cursor");

    store(0x1C80U, 0x01U);
    check(
        capture_ball_frame_schedule(state).owner
            == BallFrameOwner::off_table_transition,
        "native ball frame schedule defers an off-table resumed loop");

    store(0x1C80U, 0x00U);
    set_handler(0xC7CFU);
    check(
        capture_ball_frame_schedule(state).owner
            == BallFrameOwner::table_control_wait,
        "native ball frame schedule names the table-control wait continuation");

    set_handler(0xC8DAU);
    check(
        capture_ball_frame_schedule(state).owner
            == BallFrameOwner::off_table_transition,
        "native ball frame schedule names the off-table continuation");

    set_handler(0xC6C2U);
    check(
        capture_ball_frame_schedule(state).owner
            == BallFrameOwner::initial_substep_transition,
        "native ball frame schedule names the initial substep transition");
    check(
        classify_ball_initial_transition({
            .position_x = Fixed24_8{.raw = 239 * 256 + 255},
        }) == devils_crush::native_runtime::BallInitialTransition{
            .horizontal_pixels = 239U,
            .disposition = BallInitialTransitionDisposition::schedules_normal_motion,
        },
        "native initial ball transition schedules C721 below its unsigned X gate");
    check(
        classify_ball_initial_transition({
            .position_x = Fixed24_8{.raw = 240 * 256},
        }) == devils_crush::native_runtime::BallInitialTransition{
            .horizontal_pixels = 240U,
            .disposition = BallInitialTransitionDisposition::returns_to_caller,
        },
        "native initial ball transition preserves C6C2's inclusive horizontal gate");

    store(0x0056U, 0x04U);
    store(0x0057U, 0x04U);
    store(0x1180U, 0xEFU);
    store(0x1200U, 0x00U);
    const auto initial_schedule = capture_ball_frame_schedule(state);
    check(
        initial_schedule.initial_transition
            == devils_crush::native_runtime::BallInitialTransition{
                .horizontal_pixels = 239U,
                .disposition = BallInitialTransitionDisposition::schedules_normal_motion,
            },
        "native ball frame schedule retains C6C2's normal-motion decision");

    store(0x1180U, 0xF0U);
    check(
        capture_ball_frame_schedule(state).initial_transition
            == devils_crush::native_runtime::BallInitialTransition{
                .horizontal_pixels = 240U,
                .disposition = BallInitialTransitionDisposition::returns_to_caller,
            },
        "native ball frame schedule retains C6C2's caller-return decision");

    store(0x0056U, 0x00U);
    store(0x0057U, 0x04U);
    check(
        !capture_ball_frame_schedule(state).initial_transition,
        "native ball frame schedule waits for C6C2's cursor before classifying its gate");

    store(0x0056U, 0x05U);
    check(
        !capture_ball_frame_schedule(state).substep_progress,
        "native ball frame schedule rejects an incoherent prepared substep cursor");

    const BallFrameSchedule initial_scheduler_state{
        .owner = BallFrameOwner::initial_substep_transition,
        .collision_shape = BallCollisionShapeSelection{
            .directory_index = 0x42U,
            .phase = 0x17U,
        },
    };
    const auto scheduled_normal = advance_initial_ball_frame_schedule(
        initial_scheduler_state,
        {.horizontal_pixels = 239U,
         .disposition = BallInitialTransitionDisposition::schedules_normal_motion},
        {.increment = {.horizontal = 0x0180, .vertical = -0x0080},
         .requested_steps = 4U});
    const auto returned_initial = advance_initial_ball_frame_schedule(
        initial_scheduler_state,
        {.horizontal_pixels = 240U,
         .disposition = BallInitialTransitionDisposition::returns_to_caller},
        {.increment = {.horizontal = 0x0100}, .requested_steps = 1U});
    check(
        scheduled_normal
            && scheduled_normal->owner == BallFrameOwner::normal_motion
            && scheduled_normal->substep_progress
                == devils_crush::native_runtime::BallSubstepProgress{
                    .increment = {.horizontal = 0x0180, .vertical = -0x0080},
                    .requested_steps = 4U,
                }
            && scheduled_normal->collision_shape
                == BallCollisionShapeSelection{}
            && returned_initial
            && returned_initial->owner == BallFrameOwner::initial_substep_transition
            && returned_initial->collision_shape
                == initial_scheduler_state.collision_shape
            && !advance_initial_ball_frame_schedule(
                {.owner = BallFrameOwner::initial_substep_transition},
                {.disposition = BallInitialTransitionDisposition::returns_to_caller},
                {.requested_steps = 1U}),
        "native C6C2 scheduler applies its typed C721 and caller-return actions");
}

void test_synthetic_bonus_stage_return()
{
    using devils_crush::native_runtime::release_synthetic_bonus_return_ball;

    std::array<std::byte, 0x2000U> work_ram{};
    const auto store = [&work_ram](
                           const std::size_t offset,
                           const std::uint8_t value) {
        work_ram[offset] = static_cast<std::byte>(value);
    };
    store(0x0053U, 0x00U);
    store(0x0054U, 0x00U);
    store(0x1080U, 0x80U);
    store(0x1180U, 0x0FU);
    store(0x1200U, 0x01U);
    store(0x1300U, 0x2CU);
    store(0x1380U, 0x02U);
    store(0x0D22U, 0x0FU);
    store(0x0D23U, 0x01U);
    store(0x0D24U, 0x2CU);
    store(0x0D25U, 0x02U);
    store(0x1C80U, 0x03U);
    store(0x1F00U, 0xDAU);
    store(0x1F80U, 0xC8U);

    check(
        release_synthetic_bonus_return_ball(work_ram)
            && std::to_integer<std::uint8_t>(work_ram[0x1C80U]) == 0x02U,
        "synthetic bonus return releases only the table-feature hold bit");

    store(0x1C80U, 0x03U);
    store(0x0053U, 0x10U);
    check(
        !release_synthetic_bonus_return_ball(work_ram)
            && std::to_integer<std::uint8_t>(work_ram[0x1C80U]) == 0x03U,
        "synthetic bonus return remains held while the bonus session is active");

    store(0x0053U, 0x00U);
    store(0x1F00U, 0xE7U);
    check(
        !release_synthetic_bonus_return_ball(work_ram),
        "synthetic bonus return rejects a different ball continuation");

    store(0x1F00U, 0xDAU);
    store(0x0D22U, 0x10U);
    check(
        !release_synthetic_bonus_return_ball(work_ram),
        "synthetic bonus return rejects a ball outside the restored position");

    check(
        !release_synthetic_bonus_return_ball(
            std::span<std::byte>(work_ram).first(work_ram.size() - 1U)),
        "synthetic bonus return rejects truncated work RAM");
}

void test_native_bank7_mode_gate()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_bank7_mode_gate gate{};

    work_ram[0x00CBU] = 0xA5U;
    check(
        dc_classify_bank7_mode_gate(
            work_ram.data(), work_ram.size(), &gate)
                != 0
            && gate.path == DC_BANK7_MODE_GATE_INACTIVE_MODE
            && gate.latch_after == 0xA4U,
        "native 07:4003 clears the latch outside mode nine");

    work_ram[0x0D51U] = 9U;
    work_ram[0x0D4FU] = 0x3BU;
    check(
        dc_classify_bank7_mode_gate(
            work_ram.data(), work_ram.size(), &gate)
                != 0
            && gate.path == DC_BANK7_MODE_GATE_BELOW_RANGE,
        "native 07:4003 rejects the value below its active range");

    work_ram[0x0D4FU] = 0x46U;
    check(
        dc_classify_bank7_mode_gate(
            work_ram.data(), work_ram.size(), &gate)
                != 0
            && gate.path == DC_BANK7_MODE_GATE_ABOVE_RANGE,
        "native 07:4003 rejects the value above its active range");

    work_ram[0x0D4FU] = 0x3CU;
    work_ram[0x0258U] = 5U;
    check(
        dc_classify_bank7_mode_gate(
            work_ram.data(), work_ram.size(), &gate)
                != 0
            && gate.path == DC_BANK7_MODE_GATE_FORM_MISMATCH,
        "native 07:4003 requires persisted form six");

    work_ram[0x0258U] = 6U;
    check(
        dc_classify_bank7_mode_gate(
            work_ram.data(), work_ram.size(), &gate)
                != 0
            && gate.path == DC_BANK7_MODE_GATE_ALREADY_ARMED
            && gate.latch_after == 0xA5U,
        "native 07:4003 preserves an already armed latch");

    work_ram[0x00CBU] = 0xA4U;
    work_ram[0x0053U] = 0x08U;
    work_ram[0x00CAU] = 0x11U;
    check(
        dc_classify_bank7_mode_gate(
            work_ram.data(), work_ram.size(), &gate)
                != 0
            && gate.path == DC_BANK7_MODE_GATE_ARM
            && gate.counter_after == 0x64U
            && gate.session_flags_after == 0x0CU
            && gate.latch_after == 0xA5U,
        "native 07:4003 arms the active mode-nine state");

    check(
        dc_classify_bank7_mode_gate(
            work_ram.data(), 0x0D51U, &gate)
            == 0,
        "native 07:4003 rejects a truncated work-RAM snapshot");
}

void test_native_flipper_phase_step()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    constexpr std::uint8_t slot = 0x05U;
    work_ram[0x1A00U + slot] = 1U;
    work_ram[0x1E00U + slot] = 1U;
    work_ram[0x1C80U + slot] = 0U;
    work_ram[0x1980U + slot] = 0U;

    dc_flipper_phase_step step{};
    check(
        dc_advance_flipper_phase(
            work_ram.data(),
            work_ram.size(),
            slot,
            &step)
            != 0
            && step.supported != 0U
            && step.decrements_phase == 0U
            && step.phase_next == 1U,
        "native D500 advances a forward flipper phase");

    work_ram[0x1C80U + slot] = 0x80U;
    work_ram[0x1980U + slot] = 1U;
    check(
        dc_advance_flipper_phase(
            work_ram.data(),
            work_ram.size(),
            slot,
            &step)
            != 0
            && step.supported != 0U
            && step.decrements_phase != 0U
            && step.phase_next == 0U,
        "native D500 advances a reverse flipper phase");
    work_ram[0x1A00U + slot] = 2U;
    check(
        dc_advance_flipper_phase(
            work_ram.data(),
            work_ram.size(),
            slot,
            &step)
            != 0
            && step.supported == 0U,
        "native D500 defers a non-expiring timer");
    check(
        dc_advance_flipper_phase(
            work_ram.data(),
            work_ram.size() - 1U,
            slot,
            &step)
            == 0,
        "native D500 rejects a truncated work-RAM snapshot");
}

void test_native_flipper_release_arm()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    constexpr std::uint8_t slot = 0x05U;
    work_ram[0x1C80U + slot] = 0x21U;

    dc_flipper_release_arm release{};
    check(
        dc_prepare_flipper_release_arm(
            work_ram.data(), work_ram.size(), slot, &release)
            != 0
            && release.orientation_before == 0x21U
            && release.orientation_after == 0xA1U
            && release.installs_dispatch != 0U
            && release.decrements_countdown == 0U
            && release.dispatch_low == 0xC6U
            && release.dispatch_high == 0xD4U,
        "native D4B3 arms reverse motion and installs its continuation");

    work_ram[0x1D80U + slot] = 1U;
    check(
        dc_prepare_flipper_release_arm(
            work_ram.data(), work_ram.size(), slot, &release)
            != 0
            && release.decrements_countdown != 0U
            && release.coroutine_countdown_after == 0U
            && release.installs_dispatch != 0U,
        "native D4B3 installs its continuation when countdown one expires");

    work_ram[0x1D80U + slot] = 2U;
    check(
        dc_prepare_flipper_release_arm(
            work_ram.data(), work_ram.size(), slot, &release)
            != 0
            && release.coroutine_countdown_after == 1U
            && release.installs_dispatch == 0U,
        "native D4B3 preserves the prior dispatch while its countdown runs");
    check(
        dc_prepare_flipper_release_arm(
            work_ram.data(), work_ram.size() - 1U, slot, &release)
            == 0,
        "native D4B3 rejects a truncated work-RAM snapshot");
}

void test_native_flipper_release_prefix()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    constexpr std::uint8_t slot = 0x06U;
    work_ram[0x1C80U + slot] = 0xA5U;
    work_ram[0x1E00U + slot] = 0x01U;

    dc_flipper_release_prefix release{};
    check(
        dc_prepare_flipper_release_prefix(
            work_ram.data(), work_ram.size(), slot, &release)
                != 0
            && release.orientation_before == 0xA5U
            && release.orientation_after == 0x25U
            && release.active_before == 0x01U
            && release.active_after == 0U,
        "native D458 clears reverse orientation and the active latch");
    check(
        dc_prepare_flipper_release_prefix(
            work_ram.data(), work_ram.size() - 1U, slot, &release)
            == 0,
        "native D458 rejects a truncated work-RAM snapshot");
}

void test_native_flipper_active_tail()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    constexpr std::uint8_t slot = 0x07U;
    work_ram[0x1C80U + slot] = 0xA5U;

    dc_flipper_active_head head{};
    work_ram[0x1980U + slot] = 3U;
    check(
        dc_classify_flipper_active_head(
            work_ram.data(), work_ram.size(), slot, &head)
                != 0
            && head.path == DC_FLIPPER_ACTIVE_HEAD_ENDPOINT,
        "native D494 enters the endpoint scheduler at phase three");

    work_ram[0x1980U + slot] = 2U;
    work_ram[0x0CEEU] = 0x01U;
    work_ram[0x1B80U + slot] = 0x02U;
    check(
        dc_classify_flipper_active_head(
            work_ram.data(), work_ram.size(), slot, &head)
                != 0
            && head.path == DC_FLIPPER_ACTIVE_HEAD_RELEASE
            && head.matched_input == 0U,
        "native D494 arms release when flipper input is absent");

    work_ram[0x0CEEU] = 0x02U;
    check(
        dc_classify_flipper_active_head(
            work_ram.data(), work_ram.size(), slot, &head)
                != 0
            && head.path == DC_FLIPPER_ACTIVE_HEAD_RETURN
            && head.matched_input == 0x02U,
        "native D494 returns while flipper input remains held");
    check(
        dc_classify_flipper_active_head(
            work_ram.data(), work_ram.size() - 1U, slot, &head)
            == 0,
        "native D494 rejects a truncated work-RAM snapshot");

    dc_flipper_active_tail tail{};
    work_ram[0x1980U + slot] = 0U;
    check(
        dc_classify_flipper_active_tail(
            work_ram.data(), work_ram.size(), slot, &tail)
                != 0
            && tail.path == DC_FLIPPER_ACTIVE_TAIL_RELEASE
            && tail.state_after == 0xA5U,
        "native D4CC releases a flipper whose phase reached zero");

    work_ram[0x1980U + slot] = 2U;
    work_ram[0x0CEEU] = 0x01U;
    work_ram[0x1B80U + slot] = 0x02U;
    check(
        dc_classify_flipper_active_tail(
            work_ram.data(), work_ram.size(), slot, &tail)
                != 0
            && tail.path == DC_FLIPPER_ACTIVE_TAIL_IDLE_RETURN
            && tail.matched_input == 0U
            && tail.state_after == 0xA5U,
        "native D4CC returns while the flipper input is inactive");

    work_ram[0x0CEEU] = 0x02U;
    check(
        dc_classify_flipper_active_tail(
            work_ram.data(), work_ram.size(), slot, &tail)
                != 0
            && tail.path == DC_FLIPPER_ACTIVE_TAIL_REARM
            && tail.matched_input == 0x02U
            && tail.state_after == 0x25U,
        "native D4CC clears reverse motion before rearming input");
    check(
        dc_classify_flipper_active_tail(
            work_ram.data(), work_ram.size() - 1U, slot, &tail)
            == 0,
        "native D4CC rejects a truncated work-RAM snapshot");
}

void test_native_flipper_geometry()
{
    std::array<std::uint8_t, 0x80> geometry_table{};
    geometry_table[0x3EU] = 0x34U;
    geometry_table[0x3FU] = 0x12U;
    geometry_table[0x7EU] = 0x78U;
    geometry_table[0x7FU] = 0x56U;

    dc_flipper_geometry geometry{};
    check(
        dc_lookup_flipper_geometry(
            0xFFU,
            geometry_table.data(),
            geometry_table.size(),
            &geometry)
            != 0
            && geometry.state == 0xFFU
            && geometry.table_index == 0x3EU
            && geometry.first_low == 0x34U
            && geometry.first_high == 0x12U
            && geometry.second_low == 0x78U
            && geometry.second_high == 0x56U,
        "native D081 resolves both phase-indexed geometry vectors");
    check(
        dc_lookup_flipper_geometry(
            0x1FU,
            geometry_table.data(),
            geometry_table.size() - 1U,
            &geometry)
            == 0,
        "native D081 rejects a truncated geometry table");
}

void test_native_coroutine_schedule()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    constexpr std::uint8_t slot = 0x09U;
    constexpr std::uint8_t stack_pointer = 0xF5U;
    work_ram[0x01F6U] = 0x8DU;
    work_ram[0x01F7U] = 0xD4U;

    dc_coroutine_schedule schedule{};
    check(
        dc_schedule_coroutine(
            work_ram.data(),
            work_ram.size(),
            slot,
            stack_pointer,
            &schedule)
            != 0
            && schedule.decrements_countdown == 0U
            && schedule.installs_dispatch != 0U
            && schedule.dispatch_low == 0x8EU
            && schedule.dispatch_high == 0xD4U
            && schedule.dispatch_low_wrapped == 0U,
        "native C5C9 installs an immediate coroutine dispatch");

    work_ram[0x1D80U + slot] = 1U;
    work_ram[0x01F6U] = 0xFFU;
    work_ram[0x01F7U] = 0x12U;
    check(
        dc_schedule_coroutine(
            work_ram.data(),
            work_ram.size(),
            slot,
            stack_pointer,
            &schedule)
            != 0
            && schedule.decrements_countdown != 0U
            && schedule.countdown_after == 0U
            && schedule.installs_dispatch != 0U
            && schedule.dispatch_low == 0U
            && schedule.dispatch_high == 0x13U
            && schedule.dispatch_low_wrapped != 0U,
        "native C5C9 carries a delayed dispatch across a page");

    work_ram[0x1D80U + slot] = 2U;
    check(
        dc_schedule_coroutine(
            work_ram.data(),
            work_ram.size(),
            slot,
            stack_pointer,
            &schedule)
            != 0
            && schedule.countdown_after == 1U
            && schedule.installs_dispatch == 0U,
        "native C5C9 yields while its coroutine delay remains active");
    check(
        dc_schedule_coroutine(
            work_ram.data(),
            work_ram.size() - 1U,
            slot,
            stack_pointer,
            &schedule)
            == 0,
        "native C5C9 rejects a truncated work-RAM snapshot");
}

void test_native_collision_table_pass()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    constexpr std::uint8_t slot = 0x26U;
    work_ram[0x1880U + slot] = 0x81U;

    dc_collision_table_pass pass{};
    check(
        dc_select_collision_table_pass(
            work_ram.data(),
            work_ram.size(),
            slot,
            0x6C2AU,
            &pass)
            != 0,
        "native 6966 accepts a complete work-RAM snapshot");
    check(
        pass.table_pointer == 0x6C2AU
            && pass.collision_work_ram_offset == 0x18A6U
            && pass.collision_state == 0x81U
            && pass.collision_state_next == 0x80U
            && pass.pass_index == 0U
            && pass.returns_without_step == 1U,
        "native 6966 matches the bounded alternate-pass return");

    constexpr std::uint8_t second_pass_slot = 0x27U;
    work_ram[0x1880U + second_pass_slot] = 0x81U;
    check(
        dc_select_collision_table_pass(
            work_ram.data(),
            work_ram.size(),
            second_pass_slot,
            0x6C2DU,
            &pass)
                != 0
            && pass.table_pointer == 0x6C2DU
            && pass.collision_work_ram_offset == 0x18A7U
            && pass.collision_state == 0x81U
            && pass.collision_state_next == 0x80U
            && pass.pass_index == 0U
            && pass.returns_without_step == 1U,
        "native 6966 matches the second bounded alternate-pass return");

    constexpr std::uint8_t third_pass_slot = 0x28U;
    work_ram[0x1880U + third_pass_slot] = 0x81U;
    check(
        dc_select_collision_table_pass(
            work_ram.data(),
            work_ram.size(),
            third_pass_slot,
            0x6C30U,
            &pass)
                != 0
            && pass.table_pointer == 0x6C30U
            && pass.collision_work_ram_offset == 0x18A8U
            && pass.collision_state == 0x81U
            && pass.collision_state_next == 0x80U
            && pass.pass_index == 0U
            && pass.returns_without_step == 1U,
        "native 6966 matches the third bounded alternate-pass return");

    constexpr std::uint8_t fourth_pass_slot = 0x29U;
    work_ram[0x1880U + fourth_pass_slot] = 0x81U;
    check(
        dc_select_collision_table_pass(
            work_ram.data(),
            work_ram.size(),
            fourth_pass_slot,
            0x6C35U,
            &pass)
                != 0
            && pass.table_pointer == 0x6C35U
            && pass.collision_work_ram_offset == 0x18A9U
            && pass.collision_state == 0x81U
            && pass.collision_state_next == 0x80U
            && pass.pass_index == 0U
            && pass.returns_without_step == 1U,
        "native 6966 matches the fourth bounded alternate-pass return");

    constexpr std::uint8_t alternate_pass_slot = 0x17U;
    work_ram[0x1880U + alternate_pass_slot] = 0x81U;
    check(
        dc_select_collision_table_pass(
            work_ram.data(),
            work_ram.size(),
            alternate_pass_slot,
            0x6B0AU,
            &pass)
                != 0
            && pass.table_pointer == 0x6B0AU
            && pass.collision_work_ram_offset == 0x1897U
            && pass.collision_state == 0x81U
            && pass.collision_state_next == 0x80U
            && pass.pass_index == 0U
            && pass.returns_without_step == 1U,
        "native 6966 matches the alternate-selector pass return");

    work_ram[0x1880U + slot] = 0x80U;
    check(
        dc_select_collision_table_pass(
            work_ram.data(),
            work_ram.size(),
            slot,
            0x6C2AU,
            &pass)
                != 0
            && pass.collision_state_next == 0x81U
            && pass.pass_index == 1U
            && pass.returns_without_step == 0U,
        "native 6966 selects the other collision-table pass");

    work_ram[0x1880U + slot] = 0x01U;
    check(
        dc_select_collision_table_pass(
            work_ram.data(),
            work_ram.size(),
            slot,
            0x6C2AU,
            &pass)
                != 0
            && pass.collision_state_next == 0x01U
            && pass.pass_index == 1U
            && pass.returns_without_step == 0U,
        "native 6966 preserves a direct collision-table pass");
    check(
        dc_select_collision_table_pass(
            work_ram.data(),
            work_ram.size() - 1U,
            slot,
            0x6C2AU,
            &pass)
            == 0,
        "native 6966 rejects a truncated work-RAM snapshot");

    work_ram[0x1880U + slot] = 0x80U;
    work_ram[0x1D00U + slot] = 0x0CU;
    work_ram[0x1E80U + slot] = 0U;
    work_ram[0x1180U + slot] = 0x40U;
    dc_collision_table_record record{};
    check(
        dc_advance_collision_table_record(
            work_ram.data(),
            work_ram.size(),
            slot,
            0x6C2AU,
            0x03U,
            0x6D56U,
            0x82U,
            0x20U,
            &record)
            != 0,
        "native 6966 accepts the bounded full collision record");
    check(
        record.pass.collision_state_next == 0x81U
            && record.pass.returns_without_step == 0U
            && record.motion_pointer == 0x6D56U
            && record.event == 0x02U
            && record.orientation == 0x8CU
            && record.cursor_after_header == 1U
            && record.cursor_after_delta == 2U
            && record.horizontal_low == 0x41U
            && record.horizontal_high == 0U
            && record.event_written == 1U
            && record.horizontal_written == 1U
            && record.horizontal_high_written == 0U
            && record.vertical_written == 0U,
        "native 6966 advances the bounded full collision record");

    work_ram[0x1880U + slot] = 0x80U;
    work_ram[0x1A80U + slot] = 0x02U;
    work_ram[0x1E80U + slot] = 0x02U;
    work_ram[0x1300U + slot] = 0x7CU;
    work_ram[0x1380U + slot] = 0U;
    check(
        dc_advance_collision_table_record(
            work_ram.data(),
            work_ram.size(),
            slot,
            0x6C2AU,
            0x03U,
            0x6D56U,
            0x08U,
            0x00U,
            &record)
                != 0
            && record.pass.collision_state_next == 0x81U
            && record.motion_header == 0x08U
            && record.motion_delta == 0x08U
            && record.event == 0x02U
            && record.event_written == 0U
            && record.cursor_after_header == 2U
            && record.cursor_after_delta == 3U
            && record.vertical_low == 0x7DU
            && record.vertical_high == 0U
            && record.horizontal_written == 0U
            && record.vertical_written == 1U
            && record.vertical_high_written == 0U,
        "native 6966 advances a headerless continuation record");

    constexpr std::uint8_t alternate_continuation_slot = 0x17U;
    work_ram[0x1880U + alternate_continuation_slot] = 0x80U;
    work_ram[0x1A80U + alternate_continuation_slot] = 0x0AU;
    work_ram[0x1E80U + alternate_continuation_slot] = 0x02U;
    work_ram[0x1300U + alternate_continuation_slot] = 0xC4U;
    work_ram[0x1380U + alternate_continuation_slot] = 0U;
    check(
        dc_advance_collision_table_record(
            work_ram.data(),
            work_ram.size(),
            alternate_continuation_slot,
            0x6B0AU,
            0x0BU,
            0x6DE6U,
            0x10U,
            0x00U,
            &record)
                != 0
            && record.pass.collision_state_next == 0x81U
            && record.motion_header == 0x10U
            && record.motion_delta == 0x10U
            && record.event == 0x0AU
            && record.event_written == 0U
            && record.cursor_after_header == 2U
            && record.cursor_after_delta == 3U
            && record.vertical_low == 0xC3U
            && record.vertical_high == 0U
            && record.horizontal_written == 0U
            && record.vertical_written == 1U
            && record.vertical_high_written == 0U,
        "native 6966 advances the alternate reverse-vertical continuation");

    work_ram[0x1880U + slot] = 0x80U;
    work_ram[0x1A80U + slot] = 0x02U;
    work_ram[0x1E80U + slot] = 0x03U;
    work_ram[0x1180U + slot] = 0x41U;
    work_ram[0x1200U + slot] = 0U;
    check(
        dc_advance_collision_table_record(
            work_ram.data(),
            work_ram.size(),
            slot,
            0x6C2AU,
            0x03U,
            0x6D56U,
            0x20U,
            0x00U,
            &record)
                != 0
            && record.pass.collision_state_next == 0x81U
            && record.motion_header == 0x20U
            && record.motion_delta == 0x20U
            && record.event == 0x02U
            && record.event_written == 0U
            && record.cursor_after_header == 3U
            && record.cursor_after_delta == 4U
            && record.horizontal_low == 0x42U
            && record.horizontal_high == 0U
            && record.horizontal_written == 1U
            && record.horizontal_high_written == 0U
            && record.vertical_written == 0U,
        "native 6966 advances a headerless horizontal continuation");

    constexpr std::uint8_t reverse_continuation_slot = 0x27U;
    work_ram[0x1880U + reverse_continuation_slot] = 0x80U;
    work_ram[0x1A80U + reverse_continuation_slot] = 0x06U;
    work_ram[0x1E80U + reverse_continuation_slot] = 0x03U;
    work_ram[0x1180U + reverse_continuation_slot] = 0xD7U;
    work_ram[0x1200U + reverse_continuation_slot] = 0U;
    check(
        dc_advance_collision_table_record(
            work_ram.data(),
            work_ram.size(),
            reverse_continuation_slot,
            0x6C2DU,
            0x07U,
            0x6D9EU,
            0x40U,
            0x00U,
            &record)
                != 0
            && record.pass.collision_state_next == 0x81U
            && record.motion_header == 0x40U
            && record.motion_delta == 0x40U
            && record.event == 0x06U
            && record.event_written == 0U
            && record.cursor_after_header == 3U
            && record.cursor_after_delta == 4U
            && record.horizontal_low == 0xD6U
            && record.horizontal_high == 0U
            && record.horizontal_written == 1U
            && record.horizontal_high_written == 0U
            && record.vertical_written == 0U,
        "native 6966 advances a reverse-horizontal continuation");

    constexpr std::uint8_t vertical_slot = 0x28U;
    work_ram[0x1880U + vertical_slot] = 0x80U;
    work_ram[0x1A80U + vertical_slot] = 0x04U;
    work_ram[0x1E80U + vertical_slot] = 0x03U;
    work_ram[0x1300U + vertical_slot] = 0x84U;
    work_ram[0x1380U + vertical_slot] = 0U;
    check(
        dc_advance_collision_table_record(
            work_ram.data(),
            work_ram.size(),
            vertical_slot,
            0x6C30U,
            0x05U,
            0x6D7AU,
            0x08U,
            0x00U,
            &record)
                != 0
            && record.pass.collision_state_next == 0x81U
            && record.motion_header == 0x08U
            && record.motion_delta == 0x08U
            && record.event == 0x04U
            && record.event_written == 0U
            && record.cursor_after_header == 3U
            && record.cursor_after_delta == 4U
            && record.vertical_low == 0x85U
            && record.vertical_high == 0U
            && record.horizontal_written == 0U
            && record.vertical_written == 1U
            && record.vertical_high_written == 0U,
        "native 6966 advances the later vertical continuation");

    constexpr std::uint8_t second_vertical_slot = 0x29U;
    work_ram[0x1880U + second_vertical_slot] = 0x80U;
    work_ram[0x1A80U + second_vertical_slot] = 0x04U;
    work_ram[0x1E80U + second_vertical_slot] = 0x03U;
    work_ram[0x1300U + second_vertical_slot] = 0x84U;
    work_ram[0x1380U + second_vertical_slot] = 0U;
    check(
        dc_advance_collision_table_record(
            work_ram.data(),
            work_ram.size(),
            second_vertical_slot,
            0x6C35U,
            0x05U,
            0x6D7AU,
            0x08U,
            0x00U,
            &record)
                != 0
            && record.pass.collision_state_next == 0x81U
            && record.motion_header == 0x08U
            && record.motion_delta == 0x08U
            && record.event == 0x04U
            && record.event_written == 0U
            && record.cursor_after_header == 3U
            && record.cursor_after_delta == 4U
            && record.vertical_low == 0x85U
            && record.vertical_high == 0U
            && record.horizontal_written == 0U
            && record.vertical_written == 1U
            && record.vertical_high_written == 0U,
        "native 6966 advances the second later vertical continuation");

    work_ram[0x1880U + vertical_slot] = 0x80U;
    work_ram[0x1A80U + vertical_slot] = 0x04U;
    work_ram[0x1E80U + vertical_slot] = 0x02U;
    work_ram[0x1300U + vertical_slot] = 0x83U;
    work_ram[0x1380U + vertical_slot] = 0U;
    check(
        dc_advance_collision_table_record(
            work_ram.data(),
            work_ram.size(),
            vertical_slot,
            0x6C30U,
            0x05U,
            0x6D7AU,
            0x08U,
            0x00U,
            &record)
                != 0
            && record.pass.collision_state_next == 0x81U
            && record.motion_header == 0x08U
            && record.motion_delta == 0x08U
            && record.event == 0x04U
            && record.event_written == 0U
            && record.cursor_after_header == 2U
            && record.cursor_after_delta == 3U
            && record.vertical_low == 0x84U
            && record.vertical_high == 0U
            && record.horizontal_written == 0U
            && record.vertical_written == 1U
            && record.vertical_high_written == 0U,
        "native 6966 advances the second headerless continuation");

    constexpr std::uint8_t reverse_slot = 0x27U;
    work_ram[0x1880U + reverse_slot] = 0x80U;
    work_ram[0x1D00U + reverse_slot] = 0x0CU;
    work_ram[0x1E80U + reverse_slot] = 0U;
    work_ram[0x1180U + reverse_slot] = 0xD8U;
    work_ram[0x1200U + reverse_slot] = 0U;
    check(
        dc_advance_collision_table_record(
            work_ram.data(),
            work_ram.size(),
            reverse_slot,
            0x6C2DU,
            0x07U,
            0x6D9EU,
            0x86U,
            0x40U,
            &record)
                != 0
            && record.pass.collision_state_next == 0x81U
            && record.motion_pointer == 0x6D9EU
            && record.event == 0x06U
            && record.orientation == 0x8CU
            && record.cursor_after_header == 1U
            && record.cursor_after_delta == 2U
            && record.horizontal_low == 0xD7U
            && record.horizontal_high == 0U
            && record.horizontal_written == 1U
            && record.horizontal_high_written == 0U
            && record.vertical_written == 0U,
        "native 6966 matches the bounded reverse full collision record");

    constexpr std::uint8_t alternate_reverse_slot = 0x17U;
    work_ram[0x1880U + alternate_reverse_slot] = 0x80U;
    work_ram[0x1D00U + alternate_reverse_slot] = 0x0DU;
    work_ram[0x1E80U + alternate_reverse_slot] = 0U;
    work_ram[0x1180U + alternate_reverse_slot] = 0xF6U;
    work_ram[0x1200U + alternate_reverse_slot] = 0U;
    check(
        dc_advance_collision_table_record(
            work_ram.data(),
            work_ram.size(),
            alternate_reverse_slot,
            0x6B0AU,
            0x0BU,
            0x6DE6U,
            0x8AU,
            0x40U,
            &record)
                != 0
            && record.pass.collision_state_next == 0x81U
            && record.motion_pointer == 0x6DE6U
            && record.event == 0x0AU
            && record.orientation == 0x8DU
            && record.cursor_after_header == 1U
            && record.cursor_after_delta == 2U
            && record.horizontal_low == 0xF5U
            && record.horizontal_high == 0U
            && record.horizontal_written == 1U
            && record.horizontal_high_written == 0U
            && record.vertical_written == 0U,
        "native 6966 matches the alternate bounded reverse collision record");

    work_ram[0x1880U + vertical_slot] = 0x80U;
    work_ram[0x1D00U + vertical_slot] = 0x0CU;
    work_ram[0x1E80U + vertical_slot] = 0U;
    work_ram[0x1300U + vertical_slot] = 0x82U;
    work_ram[0x1380U + vertical_slot] = 0U;
    check(
        dc_advance_collision_table_record(
            work_ram.data(),
            work_ram.size(),
            vertical_slot,
            0x6C30U,
            0x05U,
            0x6D7AU,
            0x84U,
            0x08U,
            &record)
                != 0
            && record.pass.collision_state_next == 0x81U
            && record.motion_pointer == 0x6D7AU
            && record.event == 0x04U
            && record.orientation == 0x8CU
            && record.cursor_after_header == 1U
            && record.cursor_after_delta == 2U
            && record.vertical_low == 0x83U
            && record.vertical_high == 0U
            && record.horizontal_written == 0U
            && record.vertical_written == 1U
            && record.vertical_high_written == 0U,
        "native 6966 matches the bounded vertical full collision record");

    work_ram[0x1880U + second_vertical_slot] = 0x80U;
    work_ram[0x1D00U + second_vertical_slot] = 0x0CU;
    work_ram[0x1E80U + second_vertical_slot] = 0U;
    work_ram[0x1300U + second_vertical_slot] = 0x82U;
    work_ram[0x1380U + second_vertical_slot] = 0U;
    check(
        dc_advance_collision_table_record(
            work_ram.data(),
            work_ram.size(),
            second_vertical_slot,
            0x6C35U,
            0x05U,
            0x6D7AU,
            0x84U,
            0x08U,
            &record)
                != 0
            && record.pass.collision_state_next == 0x81U
            && record.motion_pointer == 0x6D7AU
            && record.event == 0x04U
            && record.orientation == 0x8CU
            && record.cursor_after_header == 1U
            && record.cursor_after_delta == 2U
            && record.vertical_low == 0x83U
            && record.vertical_high == 0U
            && record.horizontal_written == 0U
            && record.vertical_written == 1U
            && record.vertical_high_written == 0U,
        "native 6966 matches the second bounded vertical collision record");

    work_ram[0x1880U + slot] = 0U;
    work_ram[0x1D00U + slot] = 0x8CU;
    work_ram[0x1180U + slot] = 0U;
    work_ram[0x1200U + slot] = 1U;
    work_ram[0x1300U + slot] = 0xFFU;
    work_ram[0x1380U + slot] = 0U;
    check(
        dc_advance_collision_table_record(
            work_ram.data(),
            work_ram.size(),
            slot,
            0x6C2AU,
            0x83U,
            0x6D56U,
            0x82U,
            0x48U,
            &record)
                != 0
            && record.pass.collision_state_next == 0U
            && record.orientation == 0x0CU
            && record.horizontal_low == 0xFFU
            && record.horizontal_high == 0U
            && record.horizontal_high_written == 1U
            && record.vertical_low == 0U
            && record.vertical_high == 1U
            && record.vertical_high_written == 1U,
        "native 6966 applies reverse orientation and signed motion carries");
    check(
        dc_advance_collision_table_record(
            work_ram.data(),
            work_ram.size() - 1U,
            slot,
            0x6C2AU,
            0x03U,
            0x6D56U,
            0x82U,
            0x20U,
            &record)
            == 0,
        "native 6966 full record rejects a truncated snapshot");
}

void test_supported_rom_metadata()
{
    const devils_crush::rom::Digests expected{
        .md5 = std::string(devils_crush::rom::kSupportedMd5),
        .sha256 = std::string(devils_crush::rom::kSupportedSha256),
    };
    check(
        devils_crush::rom::matches_supported_rom(
            devils_crush::rom::kSupportedRomSize,
            expected),
        "supported ROM metadata is accepted");

    auto changed = expected;
    changed.sha256.front() = '0';
    check(
        !devils_crush::rom::matches_supported_rom(
            devils_crush::rom::kSupportedRomSize,
            changed),
        "changed ROM hash is rejected");
    check(
        !devils_crush::rom::matches_supported_rom(
            devils_crush::rom::kSupportedRomSize + 1U,
            expected),
        "changed ROM size is rejected");
}

void test_scaler_examples()
{
    using devils_crush::render::IntRect;
    using devils_crush::render::calculate_scale_layout;

    const auto hd =
        calculate_scale_layout(1920, 1080, 256, 240, 4U, 3U);
    check_rect(hd.destination, IntRect{240, 0, 1440, 1080}, "1080p fit");
    check(
        hd.prescale_x == 5 && hd.prescale_y == 4
            && hd.stage_width == 1280 && hd.stage_height == 960,
        "1080p prescale");

    const auto qhd =
        calculate_scale_layout(2560, 1440, 256, 240, 4U, 3U);
    check_rect(qhd.destination, IntRect{320, 0, 1920, 1440}, "1440p fit");
    check(
        qhd.prescale_x == 7 && qhd.prescale_y == 6
            && qhd.stage_width == 1792 && qhd.stage_height == 1440,
        "1440p prescale");

    const auto uhd =
        calculate_scale_layout(3840, 2160, 256, 240, 4U, 3U);
    check_rect(uhd.destination, IntRect{480, 0, 2880, 2160}, "4K fit");
    check(
        uhd.prescale_x == 11 && uhd.prescale_y == 9
            && uhd.stage_width == 2816 && uhd.stage_height == 2160,
        "4K prescale");

    const auto five_four =
        calculate_scale_layout(1280, 1024, 256, 240, 4U, 3U);
    check_rect(
        five_four.destination,
        IntRect{0, 32, 1280, 960},
        "5:4 monitor fit");

    const auto odd =
        calculate_scale_layout(1366, 768, 256, 240, 4U, 3U);
    check_rect(
        odd.destination,
        IntRect{171, 0, 1024, 768},
        "odd-width monitor fit");
}

void test_scaler_edges()
{
    using devils_crush::render::calculate_scale_layout;

    check(
        !calculate_scale_layout(0, 1080, 256, 240, 4U, 3U).valid(),
        "zero output is invalid");
    check(
        !calculate_scale_layout(1920, 1080, 0, 240, 4U, 3U).valid(),
        "zero source is invalid");
    check(
        !calculate_scale_layout(1920, 1080, 256, 240, 0U, 3U).valid(),
        "zero aspect is invalid");

    const auto portrait =
        calculate_scale_layout(1080, 1920, 256, 240, 4U, 3U);
    check(portrait.valid(), "portrait layout is valid");
    check(
        portrait.destination.x >= 0 && portrait.destination.y >= 0
            && portrait.destination.x + portrait.destination.width <= 1080
            && portrait.destination.y + portrait.destination.height <= 1920,
        "portrait layout stays inside output");
    check(
        portrait.stage_width % 256 == 0
            && portrait.stage_height % 240 == 0,
        "stage uses exact source multiples");

    constexpr std::array output_sizes{
        std::pair{1, 1},
        std::pair{255, 239},
        std::pair{1001, 1000},
        std::pair{1920, 1200},
        std::pair{2160, 3840},
        std::pair{7680, 4320},
    };
    for (const auto [width, height] : output_sizes) {
        const auto layout =
            calculate_scale_layout(width, height, 256, 240, 4U, 3U);
        check(layout.valid(), "property layout is valid");
        check(
            layout.destination.x >= 0 && layout.destination.y >= 0
                && layout.destination.x + layout.destination.width <= width
                && layout.destination.y + layout.destination.height <= height,
            "property layout stays inside output");
        check(
            layout.destination.width == width
                || layout.destination.height == height,
            "property layout fills one output dimension");
        check(
            layout.stage_width % 256 == 0
                && layout.stage_height % 240 == 0,
            "property stage uses exact source multiples");
    }

    const auto four_three =
        calculate_scale_layout(1920, 1200, 256, 240, 4U, 3U);
    const auto eight_six =
        calculate_scale_layout(1920, 1200, 256, 240, 8U, 6U);
    check_rect(
        four_three.destination,
        eight_six.destination,
        "equivalent aspect ratios produce the same fit");

    const auto maximum = calculate_scale_layout(
        std::numeric_limits<int>::max(),
        std::numeric_limits<int>::max(),
        1,
        1,
        std::numeric_limits<std::uint32_t>::max(),
        1U);
    check(maximum.valid(), "maximum integer inputs remain valid");
}

void test_cabinet_state()
{
    using devils_crush::cabinet::PlayfieldView;
    using devils_crush::cabinet::decode_cabinet_state;
    using devils_crush::cabinet::decode_table_actor_state;
    using devils_crush::cabinet::should_disable_actor_camera_cull;

    std::vector<std::byte> work_ram(8192U);
    const auto frontend = decode_cabinet_state(work_ram);
    check(frontend.has_value(), "zeroed cabinet state decodes");
    check(
        frontend
            && frontend->playfield
                == PlayfieldView::emulated_framebuffer
            && frontend->displayed_score == 0U
            && frontend->balls_in_session == 0U
            && frontend->current_ball == 0U
            && !frontend->bonus_stage,
        "frontend state keeps the emulated playfield");

    work_ram[0x0251U] = std::byte{1};
    work_ram[0x0206U] = std::byte{0xFC};
    work_ram[0x020DU] = std::byte{0x03};
    work_ram[0x0214U] = std::byte{0};
    work_ram[0x0858U] = std::byte{1};
    work_ram[0x0D50U] = std::byte{37};
    work_ram[0x0D52U] = std::byte{3};
    const auto normal_table = decode_cabinet_state(work_ram);
    check(
        normal_table
            && normal_table->playfield
                == PlayfieldView::table_background
            && normal_table->stored_score == 1020U
            && normal_table->displayed_score == 102000U
            && normal_table->bonus_points == 3700U
            && normal_table->score_slot == 1U
            && normal_table->balls_in_session == 3U
            && normal_table->current_ball == 1U,
        "normal session decodes the table and all three DMD values");

    work_ram[0x0D52U] = std::byte{2};
    check(
        decode_cabinet_state(work_ram)
            && decode_cabinet_state(work_ram)->current_ball == 2U,
        "the second live ball is reported as ball two");
    work_ram[0x0D52U] = std::byte{1};
    check(
        decode_cabinet_state(work_ram)
            && decode_cabinet_state(work_ram)->current_ball == 3U,
        "the final live ball is reported as ball three");
    work_ram[0x0D52U] = std::byte{3};

    work_ram[0x0053U] = std::byte{0x10};
    const auto bonus = decode_cabinet_state(work_ram);
    check(
        bonus
            && bonus->playfield
                == PlayfieldView::emulated_framebuffer
            && bonus->bonus_stage && bonus->bonus_entry_pending(),
        "bonus request retains the preceding table until the screen is ready");
    work_ram[0x0054U] = std::byte{1};
    const auto bonus_ready = decode_cabinet_state(work_ram);
    check(bonus_ready && bonus_ready->bonus_stage
              && !bonus_ready->bonus_entry_pending()
              && bonus_ready->playfield == PlayfieldView::emulated_framebuffer,
          "loaded bonus screen releases the retained table for its fade-in");
    work_ram[0x0054U] = std::byte{2};
    check(!decode_cabinet_state(work_ram)->bonus_entry_pending(),
          "bonus result screens do not count as pending entry");
    work_ram[0x0053U] = std::byte{0};
    work_ram[0x0054U] = std::byte{0};
    check(!decode_cabinet_state(work_ram)->bonus_entry_pending(),
          "normal table play resumes after the bonus request clears");

    // The High Scores screen leaves the ball count and the table attributes
    // exactly as a live session left them and changes only `$2054`, so that
    // byte alone has to keep the reconstruction off the screen.
    work_ram[0x0053U] = std::byte{0};
    work_ram[0x0054U] = std::byte{4};
    const auto high_scores = decode_cabinet_state(work_ram);
    check(
        high_scores
            && high_scores->playfield
                == PlayfieldView::emulated_framebuffer
            && high_scores->balls_in_session == 3U
            && high_scores->screen_selector == 4U,
        "a screen raised over a live session keeps the emulated playfield");

    work_ram[0x0054U] = std::byte{3};
    check(
        decode_cabinet_state(work_ram)
            && decode_cabinet_state(work_ram)->playfield
                == PlayfieldView::emulated_framebuffer,
        "the title and attract screens keep the emulated playfield");

    work_ram[0x0054U] = std::byte{0};
    check(
        decode_cabinet_state(work_ram)
            && decode_cabinet_state(work_ram)->playfield
                == PlayfieldView::table_background,
        "the table screen selects the reconstruction again");

    work_ram[0x0D52U] = std::byte{0};
    const auto no_balls = decode_cabinet_state(work_ram);
    check(
        no_balls
            && no_balls->playfield
                == PlayfieldView::table_background
            && no_balls->current_ball == 0U,
        "game over keeps the stitched table visible without inventing a ball");

    work_ram[0x0054U] = std::byte{3};
    check(
        decode_cabinet_state(work_ram)
            && decode_cabinet_state(work_ram)->playfield
                == PlayfieldView::emulated_framebuffer,
        "the main menu restores the emulated playfield after game over");

    work_ram[0x0251U] = std::byte{7};
    check(
        !decode_cabinet_state(work_ram),
        "invalid score slot is rejected");
    check(
        !decode_cabinet_state(
            std::span<const std::byte>(work_ram).first(8191U)),
        "incomplete cabinet work RAM is rejected");

    work_ram[0x100AU] = std::byte{0x82};
    work_ram[0x108AU] = std::byte{0x8B};
    work_ram[0x118AU] = std::byte{0x08};
    work_ram[0x120AU] = std::byte{0x01};
    work_ram[0x130AU] = std::byte{0x9E};
    work_ram[0x138AU] = std::byte{0x02};
    work_ram[0x190AU] = std::byte{0x43};
    work_ram[0x1F0AU] = std::byte{0x16};
    work_ram[0x1F8AU] = std::byte{0x51};
    const auto actors = decode_table_actor_state(work_ram);
    check(
        actors && (*actors)[0x0AU].slot == 0x0AU
            && (*actors)[0x0AU].type == 0x82U
            && (*actors)[0x0AU].flags == 0x8BU
            && (*actors)[0x0AU].appearance == 0x43U
            && (*actors)[0x0AU].world_x == 264
            && (*actors)[0x0AU].world_y == 670
            && (*actors)[0x0AU].dispatch == 0x5116U
            && (*actors)[0x0AU].occupied()
            && (*actors)[0x0AU].camera_active()
            && (*actors)[0x0AU].fixed_mechanism(),
        "mapped parallel actor arrays decode into typed table state");
    check(
        !decode_table_actor_state(
            std::span<const std::byte>(work_ram).first(8191U)),
        "actor-state decode rejects incomplete work RAM");

    check(
        should_disable_actor_camera_cull(0x04U, 0x02U, 254U, 208U)
            && should_disable_actor_camera_cull(0x29U, 0x02U, 254U, 208U),
        "flippers and spiders always bypass the cabinet camera cull");
    for (const auto slot : {0x16U, 0x17U, 0x18U, 0x19U, 0x1AU,
                            0x1BU, 0x1CU, 0x1DU, 0x1EU, 0x4BU}) {
        check(should_disable_actor_camera_cull(slot, 0x02U, 126U, 480U)
                  && should_disable_actor_camera_cull(slot, 0x82U, 126U, 0U)
                  && !should_disable_actor_camera_cull(slot, 0x80U, 126U, 480U),
              "upper star and orb group run before and after a camera visit");
    }
    for (const auto slot : {0x50U, 0x51U, 0x52U, 0x53U, 0x54U, 0x55U, 0x56U}) {
        check(should_disable_actor_camera_cull(slot, 0x02U, 30U, 480U)
                  && should_disable_actor_camera_cull(slot, 0x82U, 30U, 0U)
                  && !should_disable_actor_camera_cull(slot, 0x80U, 30U, 480U),
              "upper-left gray orbs animate before and after a camera visit");
    }
    for (const auto slot : {0x3EU, 0x3FU, 0x42U, 0x43U, 0x44U, 0x45U,
                            0x46U, 0x47U, 0x48U, 0x49U, 0x4AU}) {
        check(should_disable_actor_camera_cull(slot, 0x02U, 62U, 480U)
                  && should_disable_actor_camera_cull(slot, 0x82U, 62U, 0U)
                  && !should_disable_actor_camera_cull(slot, 0x80U, 62U, 480U),
              "upper pockets and skull controllers run before the ball arrives");
    }
    for (const auto slot : {0x10U, 0x11U, 0x12U, 0x2AU, 0x2BU}) {
        check(should_disable_actor_camera_cull(slot, 0x02U, 292U, 480U)
                  && should_disable_actor_camera_cull(slot, 0x02U, 292U, 0U)
                  && should_disable_actor_camera_cull(slot, 0x82U, 292U, 208U)
                  && !should_disable_actor_camera_cull(slot, 0x80U, 292U, 480U),
              "middle portals rotate with the ball below, above, or alongside them");
    }
    for (const auto slot : {0x0FU, 0x13U, 0x15U, 0x1FU, 0x2CU,
                            0x3DU, 0x40U, 0x41U,
                            0x4CU, 0x4FU, 0x57U}) {
        check(!should_disable_actor_camera_cull(slot, 0x82U, 126U, 480U),
              "upper animation policy preserves neighboring actor culls");
    }
    check(
        !should_disable_actor_camera_cull(0x58U, 0x02U, 254U, 208U)
            && should_disable_actor_camera_cull(0x58U, 0x82U, 254U, 208U)
            && should_disable_actor_camera_cull(0x5AU, 0x82U, 254U, 208U),
        "the Three Blobs bypass the cull only after camera activation");
    check(
        !should_disable_actor_camera_cull(0x58U, 0x86U, 254U, 480U),
        "orb initialization's active flag must not latch off-camera motion");
    check(
        should_disable_actor_camera_cull(0x58U, 0x86U, 254U, 302U)
            && !should_disable_actor_camera_cull(0x58U, 0x86U, 254U, 303U)
            && should_disable_actor_camera_cull(0x58U, 0x86U, 254U, 15U)
            && !should_disable_actor_camera_cull(0x58U, 0x86U, 254U, 14U)
            && should_disable_actor_camera_cull(0x58U, 0x86U, 0U, 0U),
        "orb activation uses the ROM's inclusive lower and exclusive upper camera bounds");
    check(
        !should_disable_actor_camera_cull(0x58U, 0x80U, 254U, 208U)
            && !should_disable_actor_camera_cull(0x57U, 0x82U, 254U, 208U)
            && !should_disable_actor_camera_cull(0x5BU, 0x82U, 254U, 208U),
        "the cabinet cull policy leaves uncullable and unrelated actors alone");
}

// The plunger, without a plunger. Everything the rod's motion means is decided
// here rather than in the host, so all of it can be driven from a list of axis
// readings -- which is also the only way to test a spring return, since the
// interesting frames are the two or three it takes and nobody can produce
// those by hand.
void test_nudge()
{
    using namespace devils_crush::input;
    check(parse_nudge_multiplier("2.5") == 2.5, "fractional nudge force accepted");
    check(parse_nudge_multiplier("0") == 0 && parse_nudge_multiplier("16") == 16,
          "nudge multiplier boundaries accepted");
    for (const auto* text : {"", "-1", "17", "nan", "inf", "2x", "1e999"}) {
        check(!parse_nudge_multiplier(text), "invalid nudge multiplier rejected");
    }
    check(scale_nudge_motion(900, 100, 1) == 900, "default preserves original motion");
    check(scale_nudge_motion(900, 100, 2.5) == 1050, "scale impulse, not whole velocity");
    check(scale_nudge_motion(700, 0xFF9C, 2.5) == 550, "negative impulse amplified");
    check(scale_nudge_motion(700, 0xFF9C, 0) == 800, "zero cancels original impulse");
    check(scale_nudge_motion(32760, 100, 16) == 32767, "positive motion saturates");
    check(scale_nudge_motion(0x8008, 0xFF9C, 16) == 0x8000, "negative motion saturates");
    check(scale_nudge_motion(0x8008, 100, 0) == 32676, "recover wrapped original addition");
    const NudgeBinding binding;
    Nudge nudge;
    check(!nudge.read(binding, 20000, 0, 0, true), "nudge starts disarmed");
    check(!nudge.read(binding, 0, 0, 10, true), "nudge waits for quiet");
    check(!nudge.read(binding, 0, 0, 110, true), "quiet arms without pulsing");
    check(!nudge.read(binding, 6500, 0, 120, true), "motion below threshold ignored");
    check(nudge.read(binding, -7000, 0, 130, true), "negative X triggers nudge");
    check(nudge.read(binding, -7000, 0, 179, true), "pulse lasts fifty milliseconds");
    check(!nudge.read(binding, -7000, 0, 180, true), "held acceleration does not repeat");
    check(!nudge.read(binding, 0, 0, 200, true), "pulse ended at rest");
    check(!nudge.read(binding, 0, 0, 300, true), "cooldown prevents early rearm");
    check(!nudge.read(binding, 0, 32767, 310, true), "rebound cannot retrigger");
    check(!nudge.read(binding, 0, 0, 380, true), "settle again after rebound");
    check(!nudge.read(binding, 0, 0, 480, true), "settled after cooldown");
    check(nudge.read(binding, 0, -32768, 490, true), "negative Y full scale triggers safely");
    check(!nudge.read(binding, 0, 0, 491, false), "inactive cancels pulse");
    check(!nudge.read(binding, 32767, 0, 1000, true), "reactivation must settle first");
    std::array<std::byte, 8192> ram{};
    ram[0xD52] = std::byte{3};
    check(nudge_play_active(ram), "live table allows nudge");
    ram[0x54] = std::byte{1};
    check(nudge_play_active(ram), "live bonus allows nudge");
    ram[0x254] = std::byte{1};
    check(!nudge_play_active(ram), "demo ignores physical nudge");
    ram[0x254] = std::byte{0};
    for (unsigned screen = 2; screen <= 4; ++screen) {
        ram[0x54] = static_cast<std::byte>(screen);
        check(!nudge_play_active(ram), "results and menus ignore physical nudge");
    }
    ram[0x54] = std::byte{0};
    ram[0x53] = std::byte{2};
    check(!nudge_play_active(ram), "drained ball ignores physical nudge");
    ram[0x53] = std::byte{0};
    ram[0xD52] = std::byte{0};
    check(!nudge_play_active(ram), "game over ignores physical nudge");
    check(!nudge_play_active({}), "missing RAM ignores physical nudge");
}

void test_plunger()
{
    using devils_crush::input::Plunger;
    using devils_crush::input::PlungerCalibration;

    // Every launchable keyboard-charge tick must be selectable with the rod.
    // Sweep the calibrated range to catch missing steps and off-grid values.
    {
        std::array<bool, 157> reached{};
        std::uint16_t previous = 0;
        for (std::uint32_t units = 0; units <= 25600U; ++units) {
            const devils_crush::input::PlungerSample sample{
                .draw_units = static_cast<std::uint16_t>(units)};
            const auto position = sample.shooter_position();
            check(position >= previous && position % 64U == 0U
                      && position >= 256U && position <= 9984U,
                  "analog positions follow the keyboard quarter-pixel grid");
            reached[position / 64U] = true;
            previous = position;
        }
        for (std::size_t step = 4; step <= 156; ++step) {
            check(reached[step], "every launchable keyboard step is reachable");
        }
    }

    // An axis that reads its draw directly, so the readings below are the
    // percentages they produce and nothing has to be worked backwards.
    constexpr PlungerCalibration kDirect{
        .rest = 0,
        .drawn = 100,
        .deadzone = 0,
        .release = 25,
    };

    // The same release sampled at different points in the spring's travel
    // must retain the chosen pull, including an initially sub-threshold fall.
    for (const auto middle : {60, 55, 45, 40}) {
        Plunger plunger;
        plunger.configure(kDirect);
        static_cast<void>(plunger.update(0));
        static_cast<void>(plunger.update(60));
        const auto first = plunger.update(middle);
        const auto shot = first.released ? first : plunger.update(20);
        check(shot.released && shot.draw == 60,
              "spring sampling phase preserves the sixty-percent pull");
    }
    {
        Plunger plunger;
        plunger.configure(kDirect);
        static_cast<void>(plunger.update(0));
        static_cast<void>(plunger.update(80));
        for (const auto value : {70, 60, 50, 40}) {
            check(!plunger.update(value).released,
                  "slow forward adjustment does not accumulate into a release");
        }
        static_cast<void>(plunger.update(40));
        static_cast<void>(plunger.update(30));
        const auto shot = plunger.update(0);
        check(shot.released && shot.draw == 40,
              "release after easing uses the adjusted pull, not the old peak");
    }
    {
        Plunger plunger;
        plunger.configure({.rest = 0, .drawn = 25600, .deadzone = 0});
        static_cast<void>(plunger.update(0));
        const auto a = plunger.update(12800);
        const auto b = plunger.update(12864);
        check(a.draw == b.draw && b.draw_units - a.draw_units == 64,
              "sub-percent sensor travel survives calibration");
        const auto shot = plunger.update(0);
        check(shot.released && shot.draw_units == 12864,
              "release preserves fractional pull precision");
    }

    // A rod found already back is not somebody pulling it. This is what keeps
    // a controller that is not the plunger -- one whose axis happens to sit at
    // full scale, or one that answers nothing -- from launching the ball.
    {
        Plunger plunger;
        plunger.configure({});
        check(
            !plunger.update(32767).drawn && !plunger.armed(),
            "a plunger found drawn does not count as a pull");
        check(
            !plunger.update(32767).drawn,
            "and stays inert for as long as it is held there");
        check(
            !plunger.update(0).drawn && plunger.armed(),
            "coming home arms it, and is not itself a pull");
        check(
            plunger.update(32767).drawn,
            "and only then does drawing it count");
    }

    // The default calibration is the PinOne's: an eight-bit Z axis resting at
    // the centre and reading positive as the rod is drawn, which SDL widens to
    // the sixteen-bit range.
    {
        Plunger plunger;
        plunger.configure({});
        static_cast<void>(plunger.update(0));
        check(
            plunger.update(2000).draw == 0,
            "inside the deadzone the rod still reads as at rest");
        check(
            plunger.update(16384).draw == 45,
            "half the travel is 45 percent once the deadzone is taken off");
        check(
            plunger.update(32767).draw == 100,
            "and the end of the travel is still a full draw");
    }

    {
        Plunger plunger;
        plunger.configure({});
        static_cast<void>(plunger.update(0));
        check(
            plunger.update(-4000).draw == 0,
            "a rod pushed past its rest position reads as no draw at all");
    }

    // Let go. The spring covers the travel in two or three frames, so the
    // one-frame fall is what distinguishes it, and the shot is the draw the
    // rod held before the spring took it rather than where it was caught.
    {
        Plunger plunger;
        plunger.configure(kDirect);
        static_cast<void>(plunger.update(0));
        const auto drawing = plunger.update(80);
        check(
            drawing.drawn && !drawing.released && drawing.draw == 80,
            "the shooter follows the rod while it is being drawn");
        const auto let_go = plunger.update(20);
        check(
            let_go.released && let_go.drawn && let_go.draw == 80,
            "a fall of 60 in one frame is a release, and it fires at 80");
        const auto rebound = plunger.update(80);
        check(
            !rebound.drawn,
            "a rod that never came home cannot start a second draw");
        static_cast<void>(plunger.update(0));
        check(
            plunger.update(80).drawn,
            "and once it is home it draws again");
    }

    // A hand easing the rod forward moves it a few percent a frame, which is
    // the same journey as a release and a different intention. Nothing fires,
    // and the charge follows the rod down -- which is what easing it forward
    // asks for.
    {
        Plunger plunger;
        plunger.configure(kDirect);
        static_cast<void>(plunger.update(0));
        static_cast<void>(plunger.update(80));
        const auto eased = plunger.update(70);
        check(
            eased.drawn && !eased.released && eased.draw == 70,
            "easing the rod forward is not letting it go");
        const auto eased_more = plunger.update(60);
        check(
            !eased_more.released && eased_more.draw == 60,
            "and the shooter follows it back down instead");
    }

    // Which leaves the rod eased all the way home, where nothing ever falls
    // fast enough to look like a release. Reaching rest has to fire too, or
    // the shooter would sit at no charge with the button still held and the
    // ball would never leave.
    {
        Plunger plunger;
        plunger.configure(kDirect);
        static_cast<void>(plunger.update(0));
        static_cast<void>(plunger.update(20));
        check(
            !plunger.update(10).released,
            "a ten-point step is not fast enough to be a release");
        const auto home = plunger.update(0);
        check(
            home.released && home.draw == 10,
            "but reaching rest is, at the draw the rod last held");
    }

    // A ball is served. Whatever the rod was doing before does not count, so a
    // player already holding it back has to let it go first -- which is also
    // what the cartridge wants, since a button held from before the ball is
    // seated never launches.
    {
        Plunger plunger;
        plunger.configure(kDirect);
        static_cast<void>(plunger.update(0));
        check(plunger.update(80).drawn, "the rod is drawn");
        plunger.rearm();
        check(
            !plunger.update(80).drawn,
            "and a rearm holds it inert until it comes home again");
    }

    // Two endpoints rather than a range and an inversion flag: a plunger whose
    // axis runs backwards is one whose drawn end is the smaller number, and it
    // needs no setting of its own to say so.
    {
        Plunger plunger;
        plunger.configure(
            {.rest = 100, .drawn = 0, .deadzone = 0, .release = 25});
        static_cast<void>(plunger.update(100));
        check(
            plunger.update(50).draw == 50,
            "an axis running the other way needs no inversion setting");
    }

    {
        Plunger plunger;
        plunger.configure(
            {.rest = 0, .drawn = 0, .deadzone = 0, .release = 25});
        check(
            !plunger.update(20000).drawn,
            "two endpoints that are the same describe no plunger");
    }
}

void test_cabinet_display_config()
{
    using devils_crush::cabinet::ConfigLoadStatus;
    using devils_crush::cabinet::DisplayConfig;
    using devils_crush::cabinet::DisplayDescriptor;
    using devils_crush::cabinet::DisplayRect;
    using devils_crush::cabinet::DisplayRotation;
    using devils_crush::cabinet::WindowPlacement;
    using devils_crush::cabinet::decode_display_config;
    using devils_crush::cabinet::encode_display_config;
    using devils_crush::cabinet::fit_window_geometry;
    using devils_crush::cabinet::resolve_display;

    const DisplayConfig source{
        .format_version =
            devils_crush::cabinet::kDisplayConfigVersion,
        .playfield = {
            .display_identity = "Portrait % Display",
            .geometry = {-1080, 0, 1080, 1920},
            .rotation = DisplayRotation::portrait,
            .fullscreen = true,
        },
        .dmd = {
            .display_identity = "DMD=Panel",
            .geometry = {0, 0, 1920, 480},
            .rotation = DisplayRotation::landscape,
            .fullscreen = false,
        },
        .backglass = {
            .display_identity = "Backglass\nPanel",
            .geometry = {0, 480, 1920, 1080},
            .rotation = DisplayRotation::landscape_flipped,
            .fullscreen = true,
        },
    };
    const auto encoded = encode_display_config(source);
    for (const int angle : {0, 90, 180, 270}) {
        auto rotated = source;
        rotated.playfield_rotation_degrees = angle;
        const auto restored = decode_display_config(encode_display_config(rotated));
        check(restored && *restored == rotated,
            "cabinet artwork rotation round trips independently of monitor orientation");
    }
    for (const int angle : {-90, 45, 360}) {
        auto rotated = source;
        rotated.playfield_rotation_degrees = angle;
        check(encode_display_config(rotated).empty(),
            "cabinet artwork rotation rejects invalid angles");
    }
    const auto decoded = decode_display_config(encoded);
    check(
        decoded && *decoded == source,
        "cabinet display configuration round trips escaped fields");


    auto invalid_version = encoded;
    const auto version_digit = std::find(
        invalid_version.begin(),
        invalid_version.end(),
        std::byte{'1'});
    if (version_digit != invalid_version.end()) {
        *version_digit = std::byte{'2'};
    }
    check(
        !decode_display_config(invalid_version),
        "unknown cabinet display configuration version is rejected");
    check(
        !decode_display_config(
            std::span<const std::byte>(encoded).first(8U)),
        "truncated cabinet display configuration is rejected");

    {
        using devils_crush::cabinet::InputBindings;
        using devils_crush::cabinet::PadButton;
        using devils_crush::cabinet::default_input_bindings;

        const auto to_bytes = [](const std::string_view text) {
            std::vector<std::byte> bytes(text.size());
            std::transform(
                text.begin(),
                text.end(),
                bytes.begin(),
                [](const char value) {
                    return static_cast<std::byte>(
                        static_cast<unsigned char>(value));
                });
            return bytes;
        };

        // The requested defaults: the left flipper is a d-pad direction and
        // the right flipper is button I, so this is what those two keys pairs
        // have to land on.
        const auto defaults = default_input_bindings();
        check(
            defaults[PadButton::left]
                == std::vector<std::string>{"Left Shift", "Left"},
            "Left Shift and Left work the left flipper by default");
        // Button I accepts a letter during high-score entry, so only the
        // confirm key may send it -- no flipper key, and no Z.
        check(
            defaults[PadButton::one] == std::vector<std::string>{"Return"},
            "only Return sends button I");
        // Both right-hand flipper keys arrive as the same direction, so they
        // behave identically everywhere -- including scrolling a letter.
        check(
            defaults[PadButton::right]
                == std::vector<std::string>{"Right", "Right Shift"},
            "Right and Right Shift both reach the flipper as a direction");

        // No key that works a flipper may also confirm.
        for (const auto& name : defaults[PadButton::one]) {
            check(
                std::ranges::find(defaults[PadButton::right], name)
                        == defaults[PadButton::right].end()
                    && std::ranges::find(defaults[PadButton::left], name)
                        == defaults[PadButton::left].end(),
                "the confirm key is not also a flipper key");
        }
        // Right drives two pad inputs at once, which is only safe because the
        // left flipper no longer answers to d-pad right.
        check(
            defaults.flippers.left
                == std::vector<PadButton>{PadButton::left},
            "only d-pad left works the left flipper by default");
        check(
            defaults.flippers.right
                == std::vector<PadButton>{PadButton::right, PadButton::up},
            "the right flipper accepts d-pad right and a non-opposing "
            "stand-in");

        // A pad cannot be pushed left and right at once, and the cartridge
        // zeroes the whole byte rather than the two bits. Two flipper buttons
        // on opposing directions would silence each other, so one is moved to
        // an input its own flipper already accepts.
        using devils_crush::cabinet::has_opposing_directions;
        using devils_crush::cabinet::resolve_opposing_directions;
        constexpr std::uint8_t kLeft = 0x80U;
        constexpr std::uint8_t kRight = 0x20U;
        constexpr std::uint8_t kUp = 0x10U;
        constexpr std::uint8_t kDown = 0x40U;

        check(
            has_opposing_directions(kLeft | kRight)
                && has_opposing_directions(kUp | kDown)
                && !has_opposing_directions(kLeft | kUp)
                && !has_opposing_directions(kLeft | kDown)
                && !has_opposing_directions(kLeft | 0x01U),
            "only left-with-right and up-with-down are impossible");

        const auto both = resolve_opposing_directions(
            static_cast<std::uint8_t>(kLeft | kRight), defaults.flippers);
        check(
            !has_opposing_directions(both) && (both & kLeft) != 0U,
            "holding both flipper buttons yields a byte the pad can express");
        check(
            (both
             & devils_crush::cabinet::flipper_input_mask(
                 defaults.flippers.right))
                != 0U,
            "and it still opens the right flipper's gate");
        check(
            (both
             & devils_crush::cabinet::flipper_input_mask(
                 defaults.flippers.left))
                != 0U,
            "and the left flipper's gate as well");

        // Pressed alone, each flipper button keeps its natural direction --
        // that is what the menus and high-score entry see.
        check(
            resolve_opposing_directions(kRight, defaults.flippers) == kRight
                && resolve_opposing_directions(kLeft, defaults.flippers)
                    == kLeft,
            "a single flipper press is passed through untouched");

        // Nothing to substitute with leaves the byte alone rather than
        // inventing an input the flipper does not accept.
        const devils_crush::cabinet::FlipperInputs narrow{
            .left = {PadButton::left},
            .right = {PadButton::right},
        };
        check(
            resolve_opposing_directions(
                static_cast<std::uint8_t>(kLeft | kRight), narrow)
                == (kLeft | kRight),
            "a configuration with no alternative is left unchanged");

        // The masks those lists come to are the bytes written to $3B80,X.
        check(
            devils_crush::cabinet::flipper_input_mask(defaults.flippers.left)
                == 0x80U,
            "the left flipper mask is d-pad left alone");
        check(
            devils_crush::cabinet::flipper_input_mask(defaults.flippers.right)
                == 0x30U,
            "the right flipper mask is d-pad right plus its stand-in");
        check(
            devils_crush::cabinet::flipper_input_mask({}) == 0x00U,
            "an empty flipper list leaves the cartridge mask alone");

        // $3B80,X is shared with other actors -- 03:6697 keeps a counter
        // there -- so the slot only holds an input mask while a flipper piece
        // occupies it. Writing regardless corrupted whatever else owned the
        // slot; measured, that silenced the title-screen music, which keeps a
        // counter in slot $05.
        using devils_crush::cabinet::kCartridgeLeftFlipperMask;
        using devils_crush::cabinet::kCartridgeRightFlipperMask;
        using devils_crush::cabinet::rewrites_flipper_input_mask;
        check(
            rewrites_flipper_input_mask(
                kCartridgeLeftFlipperMask, kCartridgeLeftFlipperMask, 0x80U),
            "a byte the initializer wrote is rewritten");
        check(
            !rewrites_flipper_input_mask(
                0x0AU, kCartridgeRightFlipperMask, 0x30U),
            "a byte holding anything else is left alone");
        check(
            !rewrites_flipper_input_mask(
                0x80U, kCartridgeLeftFlipperMask, 0x80U),
            "an already-rewritten byte is not written again");
        check(
            !rewrites_flipper_input_mask(
                kCartridgeLeftFlipperMask, kCartridgeLeftFlipperMask, 0x00U),
            "an empty flipper list writes nothing");
        // Re-initializing a piece mid-session restores the cartridge value,
        // and the rewrite has to take hold again from there.
        check(
            rewrites_flipper_input_mask(
                kCartridgeRightFlipperMask,
                kCartridgeRightFlipperMask,
                0x30U),
            "a re-initialized piece is rewritten again");

        auto authentic = source;
        authentic.input.flippers = {};
        const auto authentic_decoded =
            decode_display_config(encode_display_config(authentic));
        check(
            authentic_decoded && *authentic_decoded == authentic,
            "empty flipper lists round trip");


        // Names with spaces have to survive the escaping the format applies
        // to every other value.
        auto rebound = source;
        rebound.input[PadButton::left] = {"Left Ctrl", "A"};
        rebound.input[PadButton::one] = {"Right Ctrl"};
        rebound.input[PadButton::two] = {};
        const auto rebound_decoded =
            decode_display_config(encode_display_config(rebound));
        check(
            rebound_decoded && *rebound_decoded == rebound,
            "rebound inputs round trip through the cabinet configuration");

        // A configuration written before bindings existed must keep its
        // display assignment rather than being rejected wholesale.
        std::string legacy;
        for (const auto value : encoded) {
            legacy.push_back(
                static_cast<char>(std::to_integer<unsigned char>(value)));
        }
        const auto input_start = legacy.find("input.");
        check(
            input_start != std::string::npos,
            "the encoder writes the input section");
        legacy.resize(input_start);
        const auto legacy_decoded = decode_display_config(to_bytes(legacy));
        check(
            legacy_decoded && legacy_decoded->playfield == source.playfield
                && legacy_decoded->input == defaults,
            "a configuration without an input section takes the defaults");
        check(legacy_decoded && legacy_decoded->play == devils_crush::cabinet::PlaySettings{},
            "and one from before the player's settings plays with their defaults");

        // The settings the launcher keeps. Each case below swaps one good
        // line for a bad one, so the file is otherwise valid.
        auto chosen = source;
        chosen.play = {
            .rom = "D:\\Games\\Devil's Crush (USA).pce",
            .cabinet = false,
            .bonus_stage = 4U,
            .native = false,
            .music_voice_overlay = false,
            .smoothing = false,
            .nudge_multiplier = 2.5,
        };
        const auto chosen_encoded = encode_display_config(chosen);
        const auto chosen_decoded = decode_display_config(chosen_encoded);
        check(chosen_decoded && chosen_decoded->play == chosen.play,
            "the player's settings round trip through devils-crush.ini");
        std::string chosen_text;
        for (const auto value : chosen_encoded) {
            chosen_text.push_back(
                static_cast<char>(std::to_integer<unsigned char>(value)));
        }
        check(chosen_text.find("input.nudge_multiplier=2.5\n") != std::string::npos,
            "a nudge multiplier is written as it was typed");
        const std::array<std::pair<std::string_view, std::string_view>, 4> unreadable{{
            {"game.cabinet=0\n", "game.cabinet=2\n"},
            {"game.bonus_stage=4\n", "game.bonus_stage=7\n"},
            {"input.nudge_multiplier=2.5\n", "input.nudge_multiplier=17\n"},
            {"video.smoothing=0\n", "video.smoothing=off\n"},
        }};
        for (const auto& [good, bad] : unreadable) {
            auto text = chosen_text;
            const auto at = text.find(good);
            check(at != std::string::npos, "the encoder writes every player's setting");
            if (at != std::string::npos) {
                text.replace(at, good.size(), bad);
                check(!decode_display_config(to_bytes(text)),
                    "a player's setting that cannot be read makes the file invalid");
            }
        }
        auto out_of_range = chosen;
        out_of_range.play.bonus_stage = 7U;
        check(encode_display_config(out_of_range).empty(),
            "a bonus stage past six is not written");

        // Every cabinet configuration written before the backglass image was
        // removed still carries `backglass.path`. That field is gone, and an
        // unknown one is ignored rather than fatal, so those files keep their
        // display assignment instead of being rejected wholesale.
        auto retired_field = legacy;
        retired_field += "backglass.path=cabinet art/glass%3Dfinal.png\n";
        const auto retired_decoded =
            decode_display_config(to_bytes(retired_field));
        check(
            retired_decoded
                && retired_decoded->backglass == source.backglass
                && retired_decoded->input == defaults,
            "a configuration naming a backglass image still loads");
        const auto rewritten = encode_display_config(*retired_decoded);
        check(
            std::ranges::search(rewritten, to_bytes("backglass.path"))
                .empty(),
            "and the retired field is dropped when it is written back");

        // Several Windows editors add a byte-order mark on save, and the
        // documented way to change a binding is to hand-edit this file. Left
        // in the first key, the mark would cost the user the display
        // assignment as well as the bindings.
        auto marked = std::string("\xEF\xBB\xBF");
        marked += legacy;
        const auto marked_decoded = decode_display_config(to_bytes(marked));
        check(
            marked_decoded && marked_decoded->playfield == source.playfield,
            "a hand-edited file with a byte-order mark still loads");

        // A stored field always wins over the default, including one the
        // defaults have since moved away from. The host rewrites the file in
        // full on exit, so an existing configuration pins every binding until
        // it is edited -- deleting its `input.` lines restores the defaults.
        auto pinned = legacy;
        pinned += "input.one=Right Shift,Right,Z\n";
        const auto pinned_decoded = decode_display_config(to_bytes(pinned));
        check(
            pinned_decoded
                && pinned_decoded->input[PadButton::one]
                    == std::vector<std::string>{"Right Shift", "Right", "Z"},
            "a stored binding overrides the default it disagrees with");

        // An input named but left empty is unbound, which is not the same as
        // the field being absent.
        auto unbound = legacy;
        unbound += "input.two=\n";
        const auto unbound_decoded = decode_display_config(to_bytes(unbound));
        check(
            unbound_decoded && unbound_decoded->input[PadButton::two].empty()
                && unbound_decoded->input[PadButton::one]
                    == defaults[PadButton::one],
            "an empty binding unbinds only that input");

        // Whitespace around a name is an editing artefact, not part of it.
        auto spaced = legacy;
        spaced += "input.run= Space , Return \n";
        const auto spaced_decoded = decode_display_config(to_bytes(spaced));
        check(
            spaced_decoded
                && spaced_decoded->input[PadButton::run]
                    == std::vector<std::string>{"Space", "Return"},
            "surrounding whitespace is trimmed from bound key names");

        // Flipper input lists name pad inputs, not keys.
        auto flippers = legacy;
        flippers += "input.left_flipper=left,down\n";
        const auto flippers_decoded = decode_display_config(to_bytes(flippers));
        check(
            flippers_decoded
                && flippers_decoded->input.flippers.left
                    == std::vector<PadButton>{PadButton::left, PadButton::down}
                && flippers_decoded->input.flippers.right
                    == defaults.flippers.right,
            "a flipper input list is parsed and the other keeps its default");

        auto bad_flipper = legacy;
        bad_flipper += "input.left_flipper=sideways\n";
        check(
            !decode_display_config(to_bytes(bad_flipper)),
            "an unknown pad input in a flipper list is rejected");

        // Launch keys send button I only while the ball is in the shooter,
        // which is what lets Right launch without accepting a high-score
        // letter and lets Return launch without pausing.
        check(
            defaults.launch_keys == std::vector<std::string>{"Return"},
            "only Return launches from the shooter");
        check(
            std::ranges::find(defaults.launch_keys, "Right")
                == defaults.launch_keys.end(),
            "Right no longer launches");
        check(
            std::ranges::find(defaults[PadButton::run], "Return")
                != defaults[PadButton::run].end(),
            "Return still sends Run everywhere else");

        auto launch = legacy;
        launch += "input.launch=Space\n";
        const auto launch_decoded = decode_display_config(to_bytes(launch));
        check(
            launch_decoded
                && launch_decoded->input.launch_keys
                    == std::vector<std::string>{"Space"},
            "launch keys are configurable");

        auto no_launch = legacy;
        no_launch += "input.launch=\n";
        const auto no_launch_decoded =
            decode_display_config(to_bytes(no_launch));
        check(
            no_launch_decoded && no_launch_decoded->input.launch_keys.empty(),
            "an empty launch list disables the shooter override");

        check(defaults.nudge == devils_crush::input::NudgeBinding{},
              "legacy settings receive the Alien Crush PinOne nudge defaults");
        const auto nudging = decode_display_config(to_bytes(legacy +
            "input.nudge.device=Custom Board\ninput.nudge.enabled=0\n"
            "input.nudge.axis_x=3\ninput.nudge.axis_y=4\n"
            "input.nudge.threshold=30\ninput.nudge.release=10\n"
            "input.nudge.cooldown_ms=400\n"));
        check(nudging && !nudging->input.nudge.enabled
            && nudging->input.nudge.device == "Custom Board"
            && nudging->input.nudge.axis_x == 3 && nudging->input.nudge.axis_y == 4
            && nudging->input.nudge.threshold == 30 && nudging->input.nudge.release == 10
            && nudging->input.nudge.cooldown_ms == 400,
            "custom nudge settings decode");
        if (nudging) {
            const auto roundtrip = decode_display_config(encode_display_config(*nudging));
            check(roundtrip && roundtrip->input.nudge == nudging->input.nudge,
                  "nudge settings survive configuration save");
        }
        for (const auto* invalid : {"axis_x=1", "axis_y=-1", "threshold=0",
                 "release=20", "cooldown_ms=99", "enabled=2", "device=", "typo=1"}) {
            check(!decode_display_config(to_bytes(legacy + "input.nudge." + invalid + "\n")),
                  "invalid nudge setting rejected");
        }

        // The plunger's four numbers are what `--cabinet-plunger-probe`
        // prints, so they have to survive being typed straight back in.
        // Off by default, because a plunger and a thumbstick look the same
        // from here: both rest at the centre of an axis and read to one end
        // when moved, and axis 2 on an ordinary gamepad is the right stick.
        check(
            !defaults.plunger.enabled() && defaults.plunger.device.empty(),
            "no plunger is assumed until somebody names its axis");
        check(
            defaults.plunger.calibration.rest == 0
                && defaults.plunger.calibration.drawn == 32767,
            "but the endpoints default to the PinOne's, so it needs only the "
            "axis");

        auto plunger = legacy;
        plunger +=
            "input.plunger.device=PinOne\n"
            "input.plunger.axis=4\n"
            "input.plunger.rest=-1200\n"
            "input.plunger.drawn=30000\n"
            "input.plunger.deadzone=12\n"
            "input.plunger.release=30\n";
        const auto plunger_decoded = decode_display_config(to_bytes(plunger));
        check(
            plunger_decoded
                && plunger_decoded->input.plunger
                    == devils_crush::input::PlungerBinding{
                        .device = "PinOne",
                        .axis = 4,
                        .calibration = {
                            .rest = -1200,
                            .drawn = 30000,
                            .deadzone = 12,
                            .release = 30}},
            "a plunger binding round-trips through the file");

        // Endpoints the wrong way round are a plunger whose axis runs
        // backwards, not a mistake, so nothing rejects them.
        auto reversed = legacy;
        reversed += "input.plunger.rest=32767\ninput.plunger.drawn=-32768\n";
        const auto reversed_decoded =
            decode_display_config(to_bytes(reversed));
        check(
            reversed_decoded
                && reversed_decoded->input.plunger.calibration.rest == 32767
                && reversed_decoded->input.plunger.calibration.drawn
                    == -32768,
            "a plunger axis may run in either direction");

        auto enabled = legacy;
        enabled += "input.plunger.axis=2\n";
        const auto enabled_decoded = decode_display_config(to_bytes(enabled));
        check(
            enabled_decoded && enabled_decoded->input.plunger.enabled()
                && enabled_decoded->input.plunger.axis == 2,
            "naming an axis turns the plunger on");

        auto bad_percent = legacy;
        bad_percent += "input.plunger.deadzone=140\n";
        check(
            !decode_display_config(to_bytes(bad_percent)),
            "a percentage outside 0-100 is rejected");

        const auto plunger_rewritten = decode_display_config(
            encode_display_config(*plunger_decoded));
        check(
            plunger_rewritten
                && plunger_rewritten->input.plunger
                    == plunger_decoded->input.plunger,
            "the host's own rewrite preserves the plunger");
    }

    const std::array displays{
        DisplayDescriptor{
            .identity = "Primary",
            .bounds = {0, 0, 1920, 1080},
            .rotation = DisplayRotation::landscape,
            .primary = true,
        },
        DisplayDescriptor{
            .identity = "Portrait % Display",
            .bounds = {-1080, 0, 1080, 1920},
            .rotation = DisplayRotation::portrait,
            .primary = false,
        },
        DisplayDescriptor{
            .identity = "DMD",
            .bounds = {1920, 0, 1280, 720},
            .rotation = DisplayRotation::landscape,
            .primary = false,
        },
    };
    check(
        resolve_display(source.playfield, displays)
            == std::optional<std::size_t>{1U},
        "cabinet display identity survives enumeration reorder");

    const std::array duplicate_names{
        DisplayDescriptor{
            .identity = "Identical panel",
            .bounds = {0, 0, 1920, 1080},
            .rotation = DisplayRotation::landscape,
            .primary = true,
        },
        DisplayDescriptor{
            .identity = "Identical panel",
            .bounds = {1920, 0, 1920, 1080},
            .rotation = DisplayRotation::landscape,
            .primary = false,
        },
    };
    const WindowPlacement duplicate_assignment{
        .display_identity = "Identical panel",
        .geometry = {2200, 100, 960, 540},
    };
    check(
        resolve_display(duplicate_assignment, duplicate_names)
            == std::optional<std::size_t>{1U},
        "saved geometry disambiguates identical display names");

    auto missing_identity = source.playfield;
    missing_identity.display_identity = "Disconnected display";
    missing_identity.geometry = {2100, 20, 900, 600};
    check(
        resolve_display(missing_identity, displays)
            == std::optional<std::size_t>{2U},
        "missing cabinet display falls back by saved geometry");

    missing_identity.geometry = {10000, 10000, 900, 600};
    check(
        resolve_display(missing_identity, displays)
            == std::optional<std::size_t>{0U},
        "off-desktop cabinet geometry falls back to the primary display");
    check(
        !resolve_display(missing_identity, {}).has_value(),
        "empty cabinet display inventory is rejected");

    const auto fitted = fit_window_geometry(
        WindowPlacement{
            .display_identity = "DMD",
            .geometry = {3100, 700, 2000, 900},
        },
        displays[2],
        960,
        240);
    check(
        fitted == DisplayRect{1920, 0, 1280, 720},
        "cabinet fallback geometry is sized and clamped to its display");

    const auto path =
        std::filesystem::current_path() / "dc-cabinet-config-test.ini";
    auto temporary_path = path;
    temporary_path += ".tmp";
    std::error_code error;
    (void)std::filesystem::remove(path, error);
    error.clear();
    (void)std::filesystem::remove(temporary_path, error);
    check(
        devils_crush::cabinet::save_display_config_atomic(path, source)
            .saved,
        "cabinet display configuration is atomically saved");
    const auto loaded =
        devils_crush::cabinet::load_display_config(path);
    check(
        loaded.status == ConfigLoadStatus::loaded
            && loaded.config == source,
        "cabinet display configuration file round trips");
    error.clear();
    check(
        std::filesystem::remove(path, error) && !error,
        "cabinet display configuration test file is removed");
}

void test_pce_background_decode()
{
    std::array<std::uint16_t, 48> vram{};
    std::array<std::uint16_t, 256> palette{};

    // Two BAT cells select tiles 1 and 2. Tile 1 uses subpalette 3.
    vram[0] = 0x3001U;
    vram[1] = 0x0002U;

    // Tile 1, row 0: color 3 at the left edge and color 1 at the right.
    vram[16] = 0x8081U;
    // Tile 1, row 1: color 4 at the left edge.
    vram[25] = 0x0080U;
    palette[0] = 0x0007U;
    palette[0x31] = 0x0038U;
    palette[0x33] = 0x01C0U;
    palette[0x34] = 0x01FFU;

    const auto decoded =
        devils_crush::render::decode_pce_background({
            .vram_words = vram,
            .palette_entries = palette,
            .bat_width_tiles = 2,
            .bat_height_tiles = 1,
        });
    check(decoded.has_value(), "PCE background snapshot decodes");
    if (!decoded) {
        return;
    }

    check(
        decoded->width == 16 && decoded->height == 8
            && decoded->pixels.size() == 128U,
        "PCE BAT dimensions expand to 8x8 tiles");
    check(
        decoded->pixels[0] == 0xFF00FC00U,
        "PCE planar bit order and subpalette selection");
    check(
        decoded->pixels[7] == 0xFFFC0000U,
        "PCE tile right-edge bit order");
    check(
        decoded->pixels[16] == 0xFFFCFCFCU,
        "PCE upper bitplanes decode");
    check(
        decoded->pixels[1] == 0xFF0000FCU,
        "PCE background color zero uses shared palette entry zero");

    auto invalid_vram = vram;
    invalid_vram[1] = 0x0FFFU;
    const auto dormant_tile =
        devils_crush::render::decode_pce_background({
            .vram_words = invalid_vram,
            .palette_entries = palette,
            .bat_width_tiles = 2,
            .bat_height_tiles = 1,
        });
    check(
        dormant_tile && dormant_tile->pixels[8] == 0xFF0000FCU,
        "PCE decoder blanks a dormant out-of-cache tile");
    check(
        !devils_crush::render::decode_pce_background({
            .vram_words =
                std::span<const std::uint16_t>(invalid_vram).first(1),
            .palette_entries = palette,
            .bat_width_tiles = 2,
            .bat_height_tiles = 1,
        }),
        "PCE decoder rejects a truncated BAT");

    const std::array<std::byte, 4> packed_tiles{
        std::byte{0x01},
        std::byte{0x00},
        std::byte{0x02},
        std::byte{0x00},
    };
    const std::array<std::byte, 1> packed_palettes{
        std::byte{0x30},
    };
    const auto packed =
        devils_crush::render::decode_packed_pce_background({
            .tile_words = packed_tiles,
            .packed_palette_nibbles = packed_palettes,
            .vram_words = vram,
            .palette_entries = palette,
            .width_tiles = 2,
            .height_tiles = 1,
        });
    check(
        packed && packed->pixels == decoded->pixels,
        "packed streamed BAT format decodes to the live BAT image");
    const auto packed_layer =
        devils_crush::render::decode_packed_pce_background_layer({
            .tile_words = packed_tiles,
            .packed_palette_nibbles = packed_palettes,
            .vram_words = vram,
            .palette_entries = palette,
            .width_tiles = 2,
            .height_tiles = 1,
        });
    check(
        packed_layer && packed_layer->valid()
            && packed_layer->image.pixels == decoded->pixels
            && packed_layer->opaque_pixels[0] == 1U
            && packed_layer->opaque_pixels[1] == 0U
            && packed_layer->opaque_pixels[7] == 1U,
        "packed background layer preserves raw pixel opacity");
    const auto layer_crop = packed_layer
        ? devils_crush::render::crop_background_layer(
              *packed_layer,
              0,
              0,
              8,
              1)
        : std::nullopt;
    check(
        layer_crop && layer_crop->valid()
            && layer_crop->opaque_pixels.size() == 8U
            && layer_crop->opaque_pixels[0] == 1U
            && layer_crop->opaque_pixels[1] == 0U,
        "background-layer crop preserves the opacity plane");
    check(
        !devils_crush::render::decode_packed_pce_background({
            .tile_words = packed_tiles,
            .packed_palette_nibbles = {},
            .vram_words = vram,
            .palette_entries = palette,
            .width_tiles = 2,
            .height_tiles = 1,
        }),
        "packed decoder rejects truncated palette nibbles");
}

void test_background_stitch_and_crop()
{
    using devils_crush::render::ArgbImage;
    using devils_crush::render::PlacedImage;

    const ArgbImage upper{
        .width = 2,
        .height = 2,
        .pixels = {
            0xFF100000U,
            0xFF200000U,
            0xFF300000U,
            0xFF400000U,
        },
    };
    const ArgbImage lower{
        .width = 2,
        .height = 2,
        .pixels = {
            0xFF001000U,
            0xFF002000U,
            0xFF003000U,
            0xFF004000U,
        },
    };
    const std::array regions{
        PlacedImage{.image = &upper, .world_x = -1, .world_y = -2},
        PlacedImage{.image = &lower, .world_x = -1, .world_y = 0},
    };
    const auto stitched = devils_crush::render::stitch_images(regions);
    check(stitched.has_value(), "table regions stitch");
    if (!stitched) {
        return;
    }
    check(
        stitched->world_x == -1 && stitched->world_y == -2
            && stitched->image.width == 2
            && stitched->image.height == 4,
        "stitched table retains its world origin and extent");
    check(
        stitched->image.pixels[3] == 0xFF400000U
            && stitched->image.pixels[4] == 0xFF001000U,
        "stitched table preserves the region seam");

    const auto crop = devils_crush::render::crop_image(
        stitched->image,
        0,
        1,
        2,
        2);
    check(
        crop && crop->pixels
                == std::vector<std::uint32_t>{
                    0xFF300000U,
                    0xFF400000U,
                    0xFF001000U,
                    0xFF002000U,
                },
        "camera-sized crop crosses a stitched seam exactly");
    check(
        !devils_crush::render::crop_image(
            stitched->image,
            1,
            3,
            2,
            2),
        "crop rejects out-of-range camera rectangles");
}

void test_pce_sprite_overlay()
{
    using devils_crush::render::ArgbImage;
    using devils_crush::render::PceSpriteSnapshot;

    std::vector<std::uint16_t> vram(32768U, 0U);
    std::vector<std::uint16_t> sat(256U, 0U);
    std::vector<std::uint16_t> palette(512U, 0U);
    ArgbImage image{
        .width = 64,
        .height = 64,
        .pixels = std::vector<std::uint32_t>(64U * 64U, 0xFF101010U),
    };

    const auto set_sprite = [&sat](
                                const std::size_t index,
                                const int x,
                                const int y,
                                const std::uint16_t pattern,
                                const std::uint16_t flags) {
        const auto offset = index * 4U;
        sat[offset] = static_cast<std::uint16_t>(y + 0x40);
        sat[offset + 1U] = static_cast<std::uint16_t>(x + 0x20);
        sat[offset + 2U] =
            static_cast<std::uint16_t>(pattern << 1U);
        sat[offset + 3U] = flags;
    };

    // Sprite 1 is painted first, then higher-priority sprite 0 wins.
    set_sprite(1U, 4, 5, 1U, 0x0001U);
    vram[64U] = 0x8000U;
    palette[256U + 16U + 1U] = 0x0007U;
    set_sprite(0U, 4, 5, 2U, 0x0002U);
    vram[128U + 16U] = 0x8000U;
    palette[256U + 32U + 2U] = 0x0038U;

    // Horizontal and vertical flips move a source top-left pixel to the
    // opposite edge of the 16-by-16 hardware sprite.
    set_sprite(2U, 10, 10, 3U, 0x0803U);
    vram[192U] = 0x8000U;
    palette[256U + 48U + 1U] = 0x01C0U;
    set_sprite(3U, 30, 10, 4U, 0x8004U);
    vram[256U] = 0x8000U;
    palette[256U + 64U + 1U] = 0x01FFU;

    const auto stats =
        devils_crush::render::composite_pce_sprites(
            image,
            PceSpriteSnapshot{
                .vram_words = vram,
                .sat_words = sat,
                .palette_entries = palette,
            },
            0,
            0,
            0,
            64);
    check(
        stats && stats->visible_sprites == 4U
            && stats->opaque_pixel_writes == 4U,
        "sprite overlay reports visible sprites and opaque writes");
    check(
        image.pixels[5U * 64U + 4U]
            == devils_crush::render::decode_pce_color(0x0038U),
        "lower SAT index wins sprite-to-sprite priority");
    check(
        image.pixels[10U * 64U + 25U]
            == devils_crush::render::decode_pce_color(0x01C0U),
        "sprite horizontal flip mirrors the pattern");
    check(
        image.pixels[25U * 64U + 30U]
            == devils_crush::render::decode_pce_color(0x01FFU),
        "sprite vertical flip mirrors the pattern");
    check(
        image.pixels[10U * 64U + 10U] == 0xFF101010U,
        "sprite color zero remains transparent");

    std::fill(sat.begin(), sat.end(), std::uint16_t{0});
    set_sprite(0U, 4, 5, 1U, 0x0001U);
    std::vector<std::uint8_t> background_opaque(64U * 64U, 0U);
    background_opaque[5U * 64U + 4U] = 1U;
    auto priority_image = ArgbImage{
        .width = 64,
        .height = 64,
        .pixels = std::vector<std::uint32_t>(
            64U * 64U,
            0xFF101010U),
    };
    const auto behind_stats =
        devils_crush::render::composite_pce_sprites(
            priority_image,
            PceSpriteSnapshot{
                .vram_words = vram,
                .sat_words = sat,
                .palette_entries = palette,
            },
            0,
            0,
            0,
            64,
            background_opaque);
    check(
        behind_stats && behind_stats->visible_sprites == 0U
            && behind_stats->opaque_pixel_writes == 0U
            && priority_image.pixels[5U * 64U + 4U]
                == 0xFF101010U,
        "behind-background sprite pixel respects background opacity");

    sat[3U] = 0x0081U;
    const auto foreground_stats =
        devils_crush::render::composite_pce_sprites(
            priority_image,
            PceSpriteSnapshot{
                .vram_words = vram,
                .sat_words = sat,
                .palette_entries = palette,
            },
            0,
            0,
            0,
            64,
            background_opaque);
    check(
        foreground_stats && foreground_stats->visible_sprites == 1U
            && foreground_stats->opaque_pixel_writes == 1U
            && priority_image.pixels[5U * 64U + 4U]
                == devils_crush::render::decode_pce_color(0x0007U),
        "foreground-priority sprite pixel crosses opaque background");

    std::fill(sat.begin(), sat.end(), std::uint16_t{0});
    for (std::size_t index = 0; index < 17U; ++index) {
        set_sprite(
            index,
            static_cast<int>(index),
            5,
            1U,
            0x0081U);
    }
    auto limited_image = ArgbImage{
        .width = 64,
        .height = 64,
        .pixels = std::vector<std::uint32_t>(
            64U * 64U,
            0xFF101010U),
    };
    const auto limited_stats =
        devils_crush::render::composite_pce_sprites(
            limited_image,
            PceSpriteSnapshot{
                .vram_words = vram,
                .sat_words = sat,
                .palette_entries = palette,
            },
            0,
            0,
            0,
            64);
    check(
        limited_stats && limited_stats->visible_sprites == 16U
            && limited_stats->opaque_pixel_writes == 16U
            && limited_image.pixels[5U * 64U + 15U]
                == devils_crush::render::decode_pce_color(0x0007U)
            && limited_image.pixels[5U * 64U + 16U]
                == 0xFF101010U,
        "sprite overlay enforces the 16-half scanline limit");

    std::fill(sat.begin(), sat.end(), std::uint16_t{0});
    set_sprite(0U, 20, 20, 1U, 0x0081U);
    vram[64U] = 0x8000U;
    std::vector<std::byte> work_ram(8192U);
    {
        // Learn a ball from the SAT, then retain it below the camera just
        // as the stitched renderer does during the drain coroutine.
        auto ball_ram = work_ram;
        ball_ram[0x1000U] = std::byte{0x82};
        ball_ram[0x1080U] = std::byte{0x80};
        ball_ram[0x1180U] = std::byte{20};
        ball_ram[0x1300U] = std::byte{20};
        devils_crush::render::PersistentActorSpriteCache ball_cache;
        const auto draw_ball = [&](const int y, const unsigned stage) {
            ball_ram[0x1300U] = static_cast<std::byte>(y & 0xFF);
            ball_ram[0x1380U] = static_cast<std::byte>(y >> 8);
            ball_ram[0x55U] = static_cast<std::byte>(stage);
            ArgbImage image{64, 768,
                std::vector<std::uint32_t>(64U * 768U, 0xFF101010U)};
            const auto stats = ball_cache.update_and_composite(
                image, PceSpriteSnapshot{vram, sat, palette}, ball_ram,
                0, 0, 0, 64);
            return stats ? stats->opaque_pixel_writes : 0U;
        };
        check(draw_ball(20, 0) == 1U, "drain fixture learns the visible ball");
        check(draw_ball(719, 0) == 1U, "ball remains visible before the drain threshold");
        ball_ram[0x53U] = std::byte{0x08};
        check(draw_ball(720, 0) == 1U, "shooter flight is not mistaken for a registered drain");
        ball_ram[0x53U] = std::byte{0x02};
        check(draw_ball(720, 0) == 0U, "drained ball is omitted from the stitched playfield");
        check(draw_ball(724, 0) == 0U, "ball stays hidden after crossing the drain threshold");
        check(draw_ball(720, 1) == 1U, "main-table drain cutoff does not apply to bonus stages");
        check(draw_ball(700, 0) == 1U, "ball reappears when reset for the next launch");
    }
    for (const std::size_t foreground_slot : {0x62U, 0x58U, 0x59U, 0x5AU}) {
        // Learn separate actors, then overlap their retained artwork off
        // camera to exercise soldier/portal and orb/flipper layering.
        const std::size_t background_slot = foreground_slot == 0x62U
            ? 0x10U : 0x08U;
        auto layer_ram = work_ram;
        auto layer_palette = palette;
        auto layer_sat = sat;
        std::fill(sat.begin(), sat.end(), std::uint16_t{0});
        const std::array<std::size_t, 3> slots{
            background_slot, foreground_slot, 0x00U};
        for (std::size_t i = 0; i < slots.size(); ++i) {
            const auto slot = slots[i];
            const auto x = static_cast<unsigned>(8U + 20U * i);
            layer_ram[0x1000U + slot] = std::byte{0x82};
            layer_ram[0x1080U + slot] = std::byte{0x80};
            layer_ram[0x1180U + slot] = static_cast<std::byte>(x);
            layer_ram[0x1300U + slot] = std::byte{20};
            layer_ram[0x1900U + slot] = slot == 0x62U
                ? std::byte{0x1A} : std::byte{3};
            set_sprite(i, static_cast<int>(x), 20, 1U,
                static_cast<std::uint16_t>(0x0081U + i));
        }
        layer_palette[256U + 17U] = 0x0007U;
        layer_palette[256U + 33U] = 0x0038U;
        layer_palette[256U + 49U] = 0x01C0U;
        devils_crush::render::PersistentActorSpriteCache layer_cache;
        ArgbImage layer_image{64, 128,
            std::vector<std::uint32_t>(64U * 128U, 0xFF101010U)};
        const auto learned = layer_cache.update_and_composite(layer_image,
            PceSpriteSnapshot{vram, sat, layer_palette}, layer_ram, 0, 0, 0, 64);
        for (const auto slot : slots) {
            layer_ram[0x1080U + slot] = std::byte{0};
            layer_ram[0x1180U + slot] = std::byte{20};
        }
        devils_crush::render::TableMotionFrame motion;
        const auto ball = layer_cache.update_and_composite(layer_image,
            PceSpriteSnapshot{vram, sat, layer_palette}, layer_ram,
            64, 0, 0, 64, {}, {}, &motion);
        check(ball && layer_image.pixels[20U * 64U + 20U]
                    == devils_crush::render::decode_pce_color(0x01C0U),
            "cabinet ball remains in front of soldiers and middle orbs");
        check(motion.sprites.size() == 3U
                && motion.sprites[0].actor.slot == background_slot
                && motion.sprites[1].actor.slot == foreground_slot
                && motion.sprites[2].actor.slot == 0x00U,
            "smoothing receives the same cabinet actor priority order");
        layer_ram[0x1000U] = std::byte{0};
        const auto soldiers = layer_cache.update_and_composite(layer_image,
            PceSpriteSnapshot{vram, sat, layer_palette}, layer_ram, 64, 0, 0, 64);
        check(learned && soldiers
                && layer_image.pixels[20U * 64U + 20U]
                    == devils_crush::render::decode_pce_color(0x0038U),
            "cabinet soldiers cover portals and middle orbs cover flippers");
        sat = std::move(layer_sat);
    }
    constexpr std::size_t actor_slot = 0x17U;
    work_ram[0x1000U + actor_slot] = std::byte{0x82};
    work_ram[0x1080U + actor_slot] = std::byte{0x80};
    work_ram[0x1180U + actor_slot] = std::byte{20};
    work_ram[0x1300U + actor_slot] = std::byte{20};
    work_ram[0x1900U + actor_slot] = std::byte{3};
    work_ram[0x1F00U + actor_slot] = std::byte{0x34};
    work_ram[0x1F80U + actor_slot] = std::byte{0x12};
    auto persistent_image = ArgbImage{
        .width = 64,
        .height = 128,
        .pixels = std::vector<std::uint32_t>(
            64U * 128U,
            0xFF101010U),
    };
    devils_crush::render::PersistentActorSpriteCache actor_cache;
    const auto observed_stats =
        actor_cache.update_and_composite(
            persistent_image,
            PceSpriteSnapshot{
                .vram_words = vram,
                .sat_words = sat,
                .palette_entries = palette,
            },
            work_ram,
            0,
            0,
            0,
            64);
    check(
        observed_stats
            && observed_stats->observed_fixed_actors == 1U
            && observed_stats->cached_fixed_actors == 1U
            // Single-layer: an actor inside the camera is drawn from work RAM
            // like any other. It used to be withheld for the live SAT, and the
            // seam between the two layers is what disagreed.
            && observed_stats->rendered_fixed_actors == 1U,
        "persistent actor cache learns from and draws the live camera");

    work_ram[0x1980U + actor_slot] = std::byte{1};
    vram[64U] = 0x4000U;
    auto second_phase_observation = ArgbImage{
        .width = 64,
        .height = 128,
        .pixels = std::vector<std::uint32_t>(
            64U * 128U,
            0xFF101010U),
    };
    const auto second_phase_stats =
        actor_cache.update_and_composite(
            second_phase_observation,
            PceSpriteSnapshot{
                .vram_words = vram,
                .sat_words = sat,
                .palette_entries = palette,
            },
            work_ram,
            0,
            0,
            0,
            64);
    check(
        second_phase_stats
            && second_phase_stats->observed_fixed_actors == 1U
            && second_phase_stats->cached_fixed_actors == 1U
            && second_phase_stats->rendered_fixed_actors == 1U,
        "persistent actor cache learns a second live animation phase");

    work_ram[0x1080U + actor_slot] = std::byte{0};
    work_ram[0x1980U + actor_slot] = std::byte{0};
    vram[64U] = 0U;
    palette[256U + 16U + 1U] = 0x01C0U;
    const auto persistent_stats =
        actor_cache.update_and_composite(
            persistent_image,
            PceSpriteSnapshot{
                .vram_words = vram,
                .sat_words = sat,
                .palette_entries = palette,
            },
            work_ram,
            64,
            0,
            0,
            64);
    check(
        persistent_stats
            && persistent_stats->observed_fixed_actors == 0U
            && persistent_stats->cached_fixed_actors == 1U
            && persistent_stats->rendered_fixed_actors == 1U
            && persistent_stats->rendered_retained_fixed_actors == 1U
            && persistent_stats->opaque_pixel_writes == 1U
            && persistent_image.pixels[20U * 64U + 20U]
                == devils_crush::render::decode_pce_color(0x01C0U),
        "cached fixed mechanism uses the live settled palette outside the camera");

    auto inactive_in_view_image = ArgbImage{
        .width = 64,
        .height = 128,
        .pixels = std::vector<std::uint32_t>(
            64U * 128U,
            0xFF101010U),
    };
    const auto inactive_in_view_stats =
        actor_cache.update_and_composite(
            inactive_in_view_image,
            PceSpriteSnapshot{
                .vram_words = vram,
                .sat_words = sat,
                .palette_entries = palette,
            },
            work_ram,
            0,
            0,
            0,
            64);
    check(
        inactive_in_view_stats
            && inactive_in_view_stats->observed_fixed_actors == 0U
            && inactive_in_view_stats->rendered_retained_fixed_actors
                == 1U
            && inactive_in_view_image.pixels[20U * 64U + 20U]
                == devils_crush::render::decode_pce_color(0x01C0U),
        "inactive retained actor uses the live palette inside the camera band");

    work_ram[0x1980U + actor_slot] = std::byte{1};
    auto phase_changed_image = ArgbImage{
        .width = 64,
        .height = 128,
        .pixels = std::vector<std::uint32_t>(
            64U * 128U,
            0xFF101010U),
    };
    const auto phase_changed_stats =
        actor_cache.update_and_composite(
            phase_changed_image,
            PceSpriteSnapshot{
                .vram_words = vram,
                .sat_words = sat,
                .palette_entries = palette,
            },
            work_ram,
            64,
            0,
            0,
            64);
    check(
        phase_changed_stats
            && phase_changed_stats->cached_fixed_actors == 1U
            && phase_changed_stats->rendered_fixed_actors == 1U
            && phase_changed_stats->restored_phase_variants == 1U
            && phase_changed_stats->
                   restored_sorcerer_phase_variants
                == 1U
            && phase_changed_image.pixels[20U * 64U + 21U]
                == devils_crush::render::decode_pce_color(0x01C0U),
        "off-camera Sorcerer restores its exact learned walking phase");

    std::fill(sat.begin(), sat.end(), std::uint16_t{0});
    set_sprite(0U, 20, 20, 1U, 0x0081U);
    vram[64U] = 0x8000U;
    std::vector<std::byte> effect_ram(8192U);
    constexpr std::size_t effect_slot = 0x62U;
    effect_ram[0x1000U + effect_slot] = std::byte{0x8F};
    effect_ram[0x1080U + effect_slot] = std::byte{0x80};
    effect_ram[0x1180U + effect_slot] = std::byte{20};
    effect_ram[0x1300U + effect_slot] = std::byte{20};
    effect_ram[0x1900U + effect_slot] = std::byte{0x10};
    effect_ram[0x1F00U + effect_slot] = std::byte{0x90};
    effect_ram[0x1F80U + effect_slot] = std::byte{0x42};
    auto effect_image = ArgbImage{
        .width = 64,
        .height = 128,
        .pixels = std::vector<std::uint32_t>(
            64U * 128U,
            0xFF101010U),
    };
    devils_crush::render::PersistentActorSpriteCache effect_cache;
    const auto observed_effect_stats =
        effect_cache.update_and_composite(
            effect_image,
            PceSpriteSnapshot{
                .vram_words = vram,
                .sat_words = sat,
                .palette_entries = palette,
            },
            effect_ram,
            0,
            0,
            0,
            64);
    check(
        observed_effect_stats
            && observed_effect_stats->observed_table_world_effects == 1U
            && observed_effect_stats->cached_table_world_effects == 1U
            && observed_effect_stats->rendered_table_world_effects == 1U,
        "impact burst learns its exact visual from the live camera");

    effect_ram[0x1080U + effect_slot] = std::byte{0};
    effect_ram[0x1F00U + effect_slot] = std::byte{0xB0};
    const auto retained_effect_stats =
        effect_cache.update_and_composite(
            effect_image,
            PceSpriteSnapshot{
                .vram_words = vram,
                .sat_words = sat,
                .palette_entries = palette,
            },
            effect_ram,
            64,
            0,
            0,
            64);
    check(
        retained_effect_stats
            && retained_effect_stats->cached_table_world_effects == 1U
            && retained_effect_stats->rendered_table_world_effects == 1U
            && retained_effect_stats->
                   rendered_retained_table_world_effects
                == 1U,
        "live impact actor remains visible outside the copied camera");

    effect_ram[0x1F00U + effect_slot] = std::byte{0x34};
    effect_ram[0x1F80U + effect_slot] = std::byte{0x12};
    const auto reused_effect_stats =
        effect_cache.update_and_composite(
            effect_image,
            PceSpriteSnapshot{
                .vram_words = vram,
                .sat_words = sat,
                .palette_entries = palette,
            },
            effect_ram,
            64,
            0,
            0,
            64);
    check(
        reused_effect_stats
            && reused_effect_stats->cached_table_world_effects == 0U
            && reused_effect_stats->rendered_table_world_effects == 0U,
        "dynamic slot reuse clears retained impact art immediately");

    std::vector<std::byte> descriptor_rom(0x12000U);
    std::vector<std::byte> descriptor_ram(8192U);
    constexpr std::size_t descriptor_bank = 8U * 0x2000U;
    descriptor_rom[descriptor_bank + 0x01ACU] = std::byte{0x00};
    descriptor_rom[descriptor_bank + 0x01F1U] = std::byte{0x42};
    descriptor_rom[descriptor_bank + 0x0200U] = std::byte{0x10};
    descriptor_rom[descriptor_bank + 0x0201U] = std::byte{0x42};
    descriptor_rom[descriptor_bank + 0x0210U] = std::byte{1};
    descriptor_rom[descriptor_bank + 0x0211U] = std::byte{0xFC};
    descriptor_rom[descriptor_bank + 0x0212U] = std::byte{0x24};
    descriptor_rom[descriptor_bank + 0x0213U] = std::byte{2};
    descriptor_rom[descriptor_bank + 0x0214U] = std::byte{0};
    descriptor_rom[descriptor_bank + 0x0215U] = std::byte{0x81};
    descriptor_rom[descriptor_bank + 0x0216U] = std::byte{0};
    descriptor_ram[0x0051U] = std::byte{0x10};
    descriptor_ram[0x1000U + actor_slot] = std::byte{0x82};
    descriptor_ram[0x1080U + actor_slot] = std::byte{0x80};
    descriptor_ram[0x1180U + actor_slot] = std::byte{100};
    descriptor_ram[0x1300U + actor_slot] = std::byte{200};
    descriptor_ram[0x1800U + actor_slot] = std::byte{0x30};
    descriptor_ram[0x1D00U + actor_slot] = std::byte{0x02};
    descriptor_ram[0x1E00U + actor_slot] = std::byte{0x08};
    const auto decoded_actor_sprites =
        devils_crush::render::decode_fixed_actor_sprites(
            descriptor_rom,
            descriptor_ram);
    check(
        decoded_actor_sprites
            && *decoded_actor_sprites
                == std::vector{
                    devils_crush::render::FixedActorSprite{
                        .actor_slot =
                            static_cast<std::uint8_t>(actor_slot),
                        .world_x = 104,
                        .world_y = 196,
                        .pattern = 0x0012U,
                        .flags = 0x0883U,
                    }},
        "fixed actor decoder follows the original descriptor tables");
    descriptor_ram[0x1900U + actor_slot] = std::byte{0x45};
    check(
        !devils_crush::render::decode_fixed_actor_sprites(
            descriptor_rom,
            descriptor_ram),
        "fixed actor decoder rejects an invalid appearance selector");

    std::vector<std::byte> stream_rom(2U * 0x2000U);
    const std::array<std::uint8_t, 8> stream_mpr{
        0U,
        0U,
        0U,
        0U,
        1U,
        0U,
        0U,
        0U,
    };
    const std::array stream_bytes{
        std::byte{2},
        std::byte{0x34},
        std::byte{0x12},
        std::byte{0x78},
        std::byte{0x56},
        std::byte{0x83},
        std::byte{0xBC},
        std::byte{0x9A},
        std::byte{0},
    };
    std::ranges::copy(
        stream_bytes,
        stream_rom.begin()
            + static_cast<std::ptrdiff_t>(0x2000U));
    const auto decoded_stream =
        devils_crush::render::decode_pce_vram_stream(
            stream_rom,
            stream_mpr,
            0x8000U);
    check(
        decoded_stream
            && *decoded_stream
                == std::vector<std::uint16_t>{
                    0x1234U,
                    0x5678U,
                    0x9ABCU,
                    0x9ABCU,
                    0x9ABCU,
                },
        "native VRAM stream decoder reproduces raw and repeated words");
    check(
        !devils_crush::render::decode_pce_vram_stream(
            stream_rom,
            stream_mpr,
            0x8000U,
            4U),
        "VRAM stream decoder enforces its output bound");

    std::vector<std::byte> loader_rom(3U * 0x2000U);
    constexpr std::size_t loader_descriptor_offset = 0x1000U;
    const std::array compressed_descriptor{
        std::byte{1},
        std::byte{0x20},
        std::byte{0x01},
        std::byte{0},
        std::byte{0},
        std::byte{0},
        std::byte{1},
        std::byte{2},
        std::byte{1},
    };
    std::ranges::copy(
        compressed_descriptor,
        loader_rom.begin()
            + static_cast<std::ptrdiff_t>(
                loader_descriptor_offset));
    loader_rom[0x2000U] = std::byte{0x00};
    loader_rom[0x2001U] = std::byte{0x81};
    loader_rom[0x2002U] = std::byte{0x10};
    loader_rom[0x2003U] = std::byte{0x81};
    const std::array first_loader_stream{
        std::byte{1},
        std::byte{0x34},
        std::byte{0x12},
        std::byte{0},
    };
    const std::array second_loader_stream{
        std::byte{0x82},
        std::byte{0x78},
        std::byte{0x56},
        std::byte{0},
    };
    std::ranges::copy(
        first_loader_stream,
        loader_rom.begin()
            + static_cast<std::ptrdiff_t>(0x2100U));
    std::ranges::copy(
        second_loader_stream,
        loader_rom.begin()
            + static_cast<std::ptrdiff_t>(0x2110U));
    const auto compressed_uploads =
        devils_crush::render::
            decode_pce_pattern_loader_descriptor(
                loader_rom,
                0xF000U,
                compressed_descriptor.size(),
                0x0200U);
    check(
        compressed_uploads
            && compressed_uploads->uploads.size() == 2U
            && compressed_uploads->uploads[0].phase == 0U
            && compressed_uploads->uploads[0].destination_word
                == 0x0320U
            && compressed_uploads->uploads[0].source_bank == 1U
            && compressed_uploads->uploads[0].source_address
                == 0x8100U
            && compressed_uploads->uploads[0].encoding
                == devils_crush::render::
                       PceVramUploadEncoding::compressed_stream
            && compressed_uploads->uploads[0].words
                == std::vector<std::uint16_t>{0x1234U}
            && compressed_uploads->uploads[1].phase == 1U
            && compressed_uploads->uploads[1].source_address
                == 0x8110U
            && compressed_uploads->uploads[1].words
                == std::vector<std::uint16_t>{
                    0x5678U,
                    0x5678U,
                },
        "pattern atlas decodes phase-selected compressed uploads");

    constexpr std::size_t raw_descriptor_offset = 0x1020U;
    const std::array raw_descriptor{
        std::byte{0},
        std::byte{4},
        std::byte{1},
        std::byte{0x40},
        std::byte{0},
        std::byte{2},
        std::byte{0},
        std::byte{2},
        std::byte{0},
    };
    std::ranges::copy(
        raw_descriptor,
        loader_rom.begin()
            + static_cast<std::ptrdiff_t>(raw_descriptor_offset));
    loader_rom[0x2004U] = std::byte{0x20};
    loader_rom[0x2005U] = std::byte{0x81};
    loader_rom[0x2122U] = std::byte{0xCD};
    loader_rom[0x2123U] = std::byte{0xAB};
    loader_rom[0x2124U] = std::byte{0x57};
    loader_rom[0x2125U] = std::byte{0x13};
    const auto raw_uploads =
        devils_crush::render::
            decode_pce_pattern_loader_descriptor(
                loader_rom,
                0xF020U,
                raw_descriptor.size(),
                0x0200U);
    check(
        raw_uploads && raw_uploads->uploads.size() == 1U
            && raw_uploads->uploads[0].destination_word
                == 0x0240U
            && raw_uploads->uploads[0].asset_reference
                == 0x0104U
            && raw_uploads->uploads[0].source_bank == 1U
            && raw_uploads->uploads[0].source_address
                == 0x8122U
            && raw_uploads->uploads[0].encoding
                == devils_crush::render::
                       PceVramUploadEncoding::raw_words
            && raw_uploads->uploads[0].words
                == std::vector<std::uint16_t>{
                    0xABCDU,
                    0x1357U,
                },
        "pattern atlas reproduces the F606 raw-word upload mode");
    check(
        !devils_crush::render::
            decode_pce_pattern_loader_descriptor(
                loader_rom,
                0xF020U,
                raw_descriptor.size(),
                0x0200U,
                1U),
        "raw pattern upload enforces its output bound");

    sat.resize(255U);
    check(
        !devils_crush::render::composite_pce_sprites(
            image,
            PceSpriteSnapshot{
                .vram_words = vram,
                .sat_words = sat,
                .palette_entries = palette,
            },
            0,
            0,
            0,
            64),
        "sprite overlay rejects a truncated SAT snapshot");
}

void test_save_state_container()
{
    const devils_crush::state::SaveStateContainer source{
        .format_version =
            devils_crush::state::kSaveStateFormatVersion,
        .frame_number = 0x1020304050607080ULL,
        .rom_sha256 =
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
        .core_revision = "ae99235c2139c176c1a8d0fde2957bf701d3cab0",
        .core_state = {
            std::byte{0x00},
            std::byte{0x7F},
            std::byte{0x80},
            std::byte{0xFF},
        },
    };

    const auto encoded = devils_crush::state::encode(source);
    const auto decoded = devils_crush::state::decode(encoded);
    check(decoded.decoded, "save-state container round trip decodes");
    check(
        decoded.container.format_version == source.format_version
            && decoded.container.frame_number == source.frame_number
            && decoded.container.rom_sha256 == source.rom_sha256
            && decoded.container.core_revision == source.core_revision
            && decoded.container.core_state == source.core_state,
        "save-state container round trip preserves fields");

    auto corrupted = encoded;
    corrupted.back() ^= std::byte{0x01};
    check(
        !devils_crush::state::decode(corrupted).decoded,
        "save-state payload corruption is rejected");

    auto unknown_version = encoded;
    unknown_version[8] = std::byte{0x02};
    check(
        !devils_crush::state::decode(unknown_version).decoded,
        "unknown save-state version is rejected");

    check(
        !devils_crush::state::decode(
             std::span<const std::byte>(encoded).first(32U))
             .decoded,
        "truncated save-state container is rejected");

    auto invalid_metadata = source;
    invalid_metadata.rom_sha256.front() = 'G';
    bool invalid_rejected = false;
    try {
        (void)devils_crush::state::encode(invalid_metadata);
    } catch (const std::invalid_argument&) {
        invalid_rejected = true;
    }
    check(invalid_rejected, "invalid save-state metadata is rejected");
}

void test_routine_replay_corpus()
{
    using namespace devils_crush::emulation;

    const RoutineEntry checkpoint_entry{
        .routine = {.physical_bank = 0x02U, .logical_pc = 0x420EU},
        .registers = {
            .program_counter = 0x420EU,
            .accumulator = 0x12U,
            .x = 0x62U,
            .y = 0x70U,
            .stack_pointer = 0xF9U,
            .status = 0x02U,
            .mpr = {0xFFU, 0xF8U, 0x02U, 0x03U,
                    0x04U, 0x05U, 0x06U, 0x07U},
        },
        .input = {.player_one = 0x0081U, .player_two = 0x0002U},
        .frame_number = 1852U,
        .master_cycle = 12345U,
        .absolute_master_cycle = 987654321U,
        .master_cycles_until_user_event = 4567U,
        .master_cycles_until_timer_interrupt = 890U,
    };
    const RoutineReplayCorpusEntry source{
        .format_version = kRoutineReplayCorpusFormatVersion,
        .rom_sha256 =
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
        .core_revision = "ae99235c2139c176c1a8d0fde2957bf701d3cab0",
        .ordinal = 7U,
        .checkpoint_entry = checkpoint_entry,
        .checkpoint = {
            .core_state = {
                std::byte{0x00}, std::byte{0x7F}, std::byte{0x80},
                std::byte{0xFF}},
            .frame_number = checkpoint_entry.frame_number - 1U,
        },
        .pre_call_work_ram = {
            std::byte{0x10}, std::byte{0x20}, std::byte{0x30}},
        .expected_call = {
            .entry = checkpoint_entry,
            .return_instruction = {
                .physical_bank = 0x02U,
                .logical_pc = 0x427CU,
            },
            .output_registers = {
                .program_counter = 0x427CU,
                .accumulator = 0x8FU,
                .x = 0x62U,
                .y = 0x70U,
                .stack_pointer = 0xF9U,
                .status = 0x00U,
                .mpr = checkpoint_entry.registers.mpr,
            },
            .caller_pc = 0x4429U,
            .return_pc = 0x442CU,
            .return_master_cycle = 12867U,
            .return_absolute_master_cycle =
                checkpoint_entry.absolute_master_cycle + 522U,
            .body_master_cycles = 522U,
            .work_ram_writes = {{
                .instruction = {
                    .physical_bank = 0x02U,
                    .logical_pc = 0x4215U,
                },
                .logical_address = 0x3E70U,
                .work_ram_offset = 0x1E70U,
                .value = 0x34U,
                .frame_number = checkpoint_entry.frame_number,
                .master_cycle = 12402U,
                .absolute_master_cycle =
                    checkpoint_entry.absolute_master_cycle + 57U,
            }},
        },
        .post_call_work_ram = {
            std::byte{0x10}, std::byte{0x21}, std::byte{0x30}},
    };

    const auto encoded = encode_routine_replay_corpus(source);
    const auto decoded = decode_routine_replay_corpus(encoded);
    check(decoded.decoded, "routine-replay corpus round trip decodes");
    check(
        decoded.entry == source,
        "routine-replay corpus round trip preserves every field");

    auto corrupted = encoded;
    corrupted.back() ^= std::byte{0x01};
    check(
        !decode_routine_replay_corpus(corrupted).decoded,
        "routine-replay corpus corruption is rejected");
    check(
        !decode_routine_replay_corpus(
             std::span<const std::byte>(encoded).first(20U))
             .decoded,
        "truncated routine-replay corpus is rejected");

    auto inconsistent = source;
    inconsistent.post_call_work_ram.pop_back();
    bool invalid_rejected = false;
    try {
        (void)encode_routine_replay_corpus(inconsistent);
    } catch (const std::invalid_argument&) {
        invalid_rejected = true;
    }
    check(
        invalid_rejected,
        "inconsistent routine-replay corpus is rejected before encoding");
}

void test_backup_ram_storage()
{
    const auto path =
        std::filesystem::current_path() / "dc-core-tests-backup.sav";
    auto temporary_path = path;
    temporary_path += ".tmp";
    std::error_code error;
    (void)std::filesystem::remove(path, error);
    error.clear();
    (void)std::filesystem::remove(temporary_path, error);

    const auto missing = devils_crush::backup::load_exact(path, 32U);
    check(
        missing.status == devils_crush::backup::LoadStatus::not_found,
        "missing backup RAM is reported distinctly");

    std::vector<std::byte> expected(32U);
    for (std::size_t index = 0; index < expected.size(); ++index) {
        expected[index] =
            static_cast<std::byte>((index * 13U + 7U) & 0xFFU);
    }
    check(
        devils_crush::backup::save_atomic(path, expected).saved,
        "backup RAM is atomically saved");

    const auto loaded =
        devils_crush::backup::load_exact(path, expected.size());
    check(loaded.loaded(), "backup RAM is loaded");
    check(loaded.bytes == expected, "backup RAM bytes round trip");
    check(
        devils_crush::backup::load_exact(path, expected.size() - 1U).status
            == devils_crush::backup::LoadStatus::invalid_size,
        "backup RAM with the wrong exact size is rejected");

    expected.front() ^= std::byte{0xFF};
    check(
        devils_crush::backup::save_atomic(path, expected).saved,
        "an existing backup RAM file is atomically replaced");
    check(
        devils_crush::backup::load_exact(path, expected.size()).bytes
            == expected,
        "replacement backup RAM bytes round trip");
    check(
        !std::filesystem::exists(temporary_path),
        "successful backup save leaves no temporary file");

    error.clear();
    check(
        std::filesystem::remove(path, error) && !error,
        "backup RAM test file is removed");
}

// The atomic replace every recorder ends with, against the thing that was
// actually interfering with it.
//
// This reproduces the suite's own intermittent failure rather than a model of
// it. A second handle is opened with no sharing at all, which is what a malware
// scanner does to a file in the moment after it is closed, and `MoveFileEx` is
// refused for exactly as long as that handle is open. Under `-j 24` this landed
// about once per full Release run, on a different regression every time, and
// read as nondeterminism in the port until the captured message turned out to
// name an I/O error on a file instead of a mismatch in anything.
//
// **Which of the two files is held decides the error, and the suite's error
// names the source.** Holding the temporary gives ERROR_SHARING_VIOLATION and
// holding the destination gives ERROR_ACCESS_DENIED; every failure captured
// from the suite was error 32, so what the scanner had open was the artifact
// this process had just written and closed, not the one it was replacing. That
// is the expected shape -- a freshly written file is exactly what an on-close
// scan is for -- and it is worth pinning, because the destination case was the
// intuitive guess and it is the wrong one. Both are covered below since both
// are absorbed.
void test_atomic_commit()
{
    using devils_crush::core::commit_replacement;
    using devils_crush::core::is_transient_commit_error;

    check(
        is_transient_commit_error(ERROR_SHARING_VIOLATION)
            && is_transient_commit_error(ERROR_ACCESS_DENIED)
            && is_transient_commit_error(ERROR_LOCK_VIOLATION)
            && is_transient_commit_error(ERROR_USER_MAPPED_FILE),
        "the errors another process can stop causing are retried");
    check(
        !is_transient_commit_error(ERROR_FILE_NOT_FOUND)
            && !is_transient_commit_error(ERROR_PATH_NOT_FOUND)
            && !is_transient_commit_error(ERROR_DISK_FULL)
            && !is_transient_commit_error(ERROR_SUCCESS),
        "an error that says the replace itself was wrong is not retried");

    const auto directory = std::filesystem::current_path();
    const auto destination = directory / "dc-core-tests-commit.bin";
    auto source = destination;
    source += ".tmp";

    const auto write_file = [](const std::filesystem::path& path,
                               const std::string_view contents) {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(
            contents.data(),
            static_cast<std::streamsize>(contents.size()));
        output.flush();
        return static_cast<bool>(output);
    };
    const auto read_file = [](const std::filesystem::path& path) {
        std::ifstream input(path, std::ios::binary);
        return std::string(
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>());
    };
    // Open a file the way a scanner does: read access and no sharing, so a
    // replace involving it is refused until this handle closes.
    const auto hold = [](const std::filesystem::path& path) {
        return CreateFileW(
            path.c_str(),
            FILE_READ_DATA,
            0U,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
    };

    std::error_code error;
    (void)std::filesystem::remove(destination, error);
    error.clear();
    (void)std::filesystem::remove(source, error);

    check(write_file(source, "first"), "commit source is written");
    const auto fresh = commit_replacement(source, destination);
    check(
        fresh.committed && fresh.retries == 0 && fresh.last_error == 0U,
        "an uncontended commit succeeds on the first attempt");
    check(read_file(destination) == "first", "the committed bytes are there");
    check(
        !std::filesystem::exists(source),
        "a committed source no longer exists under its temporary name");

    // The defect itself: the temporary is held, and let go part way through the
    // wait. The commit has to survive the refusals and then land.
    check(write_file(source, "second"), "contended commit source is written");
    HANDLE holder = hold(source);
    check(holder != INVALID_HANDLE_VALUE, "the temporary is held open");
    std::thread release([holder]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        CloseHandle(holder);
    });
    const auto contended = commit_replacement(source, destination);
    release.join();
    check(contended.committed, "a contended commit still lands");
    check(
        contended.retried()
            && contended.last_error == ERROR_SHARING_VIOLATION,
        "a contended commit reports the sharing violation it waited out");
    check(
        read_file(destination) == "second",
        "the contended commit published its own bytes");

    // The other side of the same race, which reports a different error and is
    // absorbed just the same.
    check(write_file(source, "third"), "destination-held source is written");
    HANDLE destination_holder = hold(destination);
    check(
        destination_holder != INVALID_HANDLE_VALUE,
        "the destination is held open");
    std::thread destination_release([destination_holder]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        CloseHandle(destination_holder);
    });
    const auto denied = commit_replacement(source, destination);
    destination_release.join();
    check(denied.committed, "a commit against a held destination still lands");
    check(
        denied.retried() && denied.last_error == ERROR_ACCESS_DENIED,
        "a held destination is refused with access denied, not sharing");
    check(
        read_file(destination) == "third",
        "the destination-held commit published its own bytes");

    // The same contention with no budget to wait it out is the behavior this
    // replaced, and it is exactly what the suite was failing on.
    check(write_file(source, "fourth"), "unbudgeted commit source is written");
    holder = hold(source);
    check(holder != INVALID_HANDLE_VALUE, "the temporary is held open again");
    const auto refused = commit_replacement(source, destination, 0);
    check(
        !refused.committed && refused.retries == 0
            && refused.last_error == ERROR_SHARING_VIOLATION,
        "a single attempt against a held temporary fails as it always did");

    // A budget that expires gives up rather than hanging, and still says why.
    const auto expired = commit_replacement(source, destination, 40);
    check(
        !expired.committed && expired.retried()
            && expired.last_error == ERROR_SHARING_VIOLATION,
        "a commit that outlasts its budget fails with the reason");
    CloseHandle(holder);
    check(
        read_file(destination) == "third",
        "a refused commit leaves the previous artifact whole");

    // Nothing this process can wait out: the source does not exist.
    error.clear();
    (void)std::filesystem::remove(source, error);
    const auto missing = commit_replacement(source, destination);
    check(
        !missing.committed && missing.retries == 0
            && missing.last_error == ERROR_FILE_NOT_FOUND,
        "a commit with no source fails immediately rather than waiting");

    error.clear();
    check(
        std::filesystem::remove(destination, error) && !error,
        "commit test file is removed");
}

void test_legacy_data_migration()
{
    namespace fs = std::filesystem;
    const auto base = fs::current_path() / "dc-data-migration-test";
    std::error_code error;
    fs::remove_all(base, error);
    const auto write = [](const fs::path& path, const char* text) {
        std::ofstream(path, std::ios::binary) << text;
    };
    const auto read = [](const fs::path& path) {
        std::ifstream input(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(input), {});
    };

    fs::create_directories(base / "portable-data");
    write(base / "portable-data" / "cabinet.ini", "settings");
    write(base / "portable-data" / "devils-crush.sav", "save");
    write(base / "portable-data" / "devils-crush.hiscore", "scores");
    auto result = devils_crush::core::migrate_legacy_data_folder(base);
    check(result.moved.size() == 3U && result.problems.empty()
            && read(base / "devils-crush.ini") == "settings"
            && read(base / "devils-crush.sav") == "save"
            && read(base / "devils-crush.hiscore") == "scores",
        "an earlier build's settings and saves move out of portable-data");
    check(!fs::exists(base / "portable-data"),
        "the emptied portable-data folder is removed");
    result = devils_crush::core::migrate_legacy_data_folder(base);
    check(result.moved.empty() && result.problems.empty(),
        "migrating again without the folder does nothing");

    // A file at the new name was written by a build that has already moved,
    // so it wins, and the old one and its folder stay for the player to see.
    fs::create_directories(base / "portable-data");
    write(base / "portable-data" / "cabinet.ini", "older");
    write(base / "portable-data" / "notes.txt", "mine");
    result = devils_crush::core::migrate_legacy_data_folder(base);
    check(result.moved.empty() && result.problems.size() == 1U
            && read(base / "devils-crush.ini") == "settings"
            && fs::exists(base / "portable-data" / "cabinet.ini")
            && fs::exists(base / "portable-data" / "notes.txt"),
        "a newer file is never replaced, and other files stay");
    fs::remove_all(base, error);
}

void test_high_score_table()
{
    using devils_crush::core::HighScoreTable;
    using devils_crush::core::kHighScoreRankCount;
    using devils_crush::core::kHighScoreRowCount;
    using devils_crush::core::kHighScoreRowOffsets;

    std::vector<std::byte> work_ram(0x2000U);
    // The cartridge's own defaults, as `01:C107` lays them down: five entries
    // of ten thousand stored units named AAA, with both live player columns
    // zeroed.
    for (std::size_t rank = 0; rank < 7U; ++rank) {
        const bool ranked = rank < kHighScoreRankCount;
        work_ram[kHighScoreRowOffsets[0] + rank] =
            static_cast<std::byte>(ranked ? 0x10U : 0x00U);
        work_ram[kHighScoreRowOffsets[1] + rank] =
            static_cast<std::byte>(ranked ? 0x27U : 0x00U);
        for (std::size_t row = 3U; row < kHighScoreRowCount; ++row) {
            work_ram[kHighScoreRowOffsets[row] + rank] =
                static_cast<std::byte>(ranked ? 0x41U : 0x00U);
        }
    }

    const auto defaults =
        devils_crush::core::read_high_score_table(work_ram);
    check(defaults.has_value(), "the default high-score table is read");
    check(
        devils_crush::core::high_score_at(*defaults, 0) == 10'000U,
        "a default entry holds ten thousand stored units");
    check(
        devils_crush::core::high_score_at(*defaults, kHighScoreRankCount)
            == 0U,
        "a rank past the table reads as zero rather than out of bounds");
    check(
        defaults->bytes[3U * kHighScoreRankCount] == 0x41U,
        "the first name character of the first entry is read");
    check(
        devils_crush::core::high_score_name_at(*defaults, 0U) == "AAA"
            && devils_crush::core::high_score_name_at(
                   *defaults, kHighScoreRankCount)
                .empty(),
        "a high-score name is decoded and an invalid rank is empty");
    {
        auto named = *defaults;
        named.bytes[3U * kHighScoreRankCount] = 0x43U;
        named.bytes[4U * kHighScoreRankCount] = 0x5BU;
        named.bytes[5U * kHighScoreRankCount] = 0x5CU;
        check(devils_crush::core::high_score_name_at(named, 0U) == "C. ",
            "the letters past Z read as the High Scores screen draws them");
    }
    check(
        !devils_crush::core::read_high_score_table(
            std::span<const std::byte>(work_ram).first(0x1FFFU)),
        "a short work-RAM image is refused");

    HighScoreTable replacement;
    for (std::size_t index = 0; index < replacement.bytes.size(); ++index) {
        replacement.bytes[index] =
            static_cast<std::uint8_t>((index * 7U + 3U) & 0xFFU);
    }
    check(
        devils_crush::core::write_high_score_table(work_ram, replacement),
        "a high-score table is written into work RAM");
    check(
        devils_crush::core::read_high_score_table(work_ram) == replacement,
        "a high-score table round trips through work RAM");
    // The two live player columns of every row belong to a session, so
    // restoring a table must leave them exactly as the game left them.
    bool live_untouched = true;
    for (const auto row : kHighScoreRowOffsets) {
        for (std::size_t column = kHighScoreRankCount; column < 7U; ++column) {
            live_untouched = live_untouched
                && work_ram[row + column] == std::byte{0};
        }
    }
    check(live_untouched, "writing a table leaves the live columns alone");
    check(
        !devils_crush::core::write_high_score_table(
            std::span<std::byte>(work_ram).first(0x1FFFU), replacement),
        "a short work-RAM image is not written");

    const auto encoded =
        devils_crush::core::encode_high_score_table(replacement);
    check(
        encoded.size() == devils_crush::core::kHighScoreFileSize,
        "an encoded high-score table has the documented size");
    check(
        devils_crush::core::decode_high_score_table(encoded) == replacement,
        "an encoded high-score table round trips");

    auto corrupt = encoded;
    corrupt.front() ^= std::byte{0xFF};
    check(
        !devils_crush::core::decode_high_score_table(corrupt),
        "a high-score file with the wrong magic is refused");
    corrupt = encoded;
    corrupt[4] = std::byte{0xFFU};
    check(
        !devils_crush::core::decode_high_score_table(corrupt),
        "a high-score file with an unknown version is refused");
    corrupt = encoded;
    corrupt[5] = std::byte{0x04U};
    check(
        !devils_crush::core::decode_high_score_table(corrupt),
        "a high-score file ranking a different count is refused");
    corrupt = encoded;
    corrupt.pop_back();
    check(
        !devils_crush::core::decode_high_score_table(corrupt),
        "a truncated high-score file is refused");
}

// The predicate half of the frame-sync fast-forward, enumerated rather than
// sampled. The model reads four bytes and one accumulator, so its whole input
// space is small enough to walk: every zero-page operand, and every
// accumulator value against the byte that operand names.
//
// What is deliberately not here is the skip itself. How far the machine may
// be advanced is the scheduler's horizon and the fork's arithmetic, and the
// oracle for that is the census route producing byte-identical artifacts with
// the capability installed and handed back.
void test_idle_skip()
{
    using devils_crush::cpu::HuC6280IdleSkip;

    // `00:E323`, as the ROM holds it: CMP <$00 / BEQ back four.
    constexpr std::array<std::uint8_t, 4> kSpin{0xC5U, 0x00U, 0xF0U, 0xFCU};

    std::array<std::uint8_t, 0x100> zero_page{};
    zero_page[0x00U] = 0x4BU;

    const auto ask = [&zero_page](
                         HuC6280IdleSkip& model,
                         const std::uint8_t accumulator,
                         const std::span<const std::uint8_t> code) {
        return model.iteration_cycles(accumulator, code, zero_page);
    };

    HuC6280IdleSkip model{
        HuC6280IdleSkip::Loop{.physical_bank = 0x00U, .logical_pc = 0xE323U}};
    check(
        model.loop().physical_bank == 0x00U
            && model.loop().logical_pc == 0xE323U,
        "the idle model keeps the loop it was pointed at");

    // Equal is the branch being taken, which is the machine still waiting.
    check(
        ask(model, 0x4BU, kSpin) == HuC6280IdleSkip::kIterationCycles,
        "a waiting frame-sync spin answers with one iteration's cost");
    check(
        model.coverage().arrivals == 1U && model.coverage().idle == 1U,
        "an idle answer is counted as one arrival and one idle");

    // Different is the wait already over: the very next instruction leaves
    // the loop, so there is nothing there to skip.
    check(
        ask(model, 0x4CU, kSpin) == 0,
        "a spin about to exit is executed rather than skipped");
    check(
        model.coverage().exits == 1U && model.coverage().idle == 1U,
        "an exit is counted apart from an idle arrival");

    // Every operand, every accumulator. The model must answer idle exactly
    // when the compare would be equal, whichever zero-page byte is named.
    for (std::size_t operand = 0; operand < zero_page.size(); ++operand) {
        zero_page[operand] = static_cast<std::uint8_t>(operand ^ 0x5AU);
    }
    bool enumerated = true;
    for (std::size_t operand = 0; enumerated && operand < zero_page.size();
         ++operand) {
        std::array<std::uint8_t, 4> code = kSpin;
        code[1] = static_cast<std::uint8_t>(operand);
        for (unsigned accumulator = 0; accumulator <= 0xFFU; ++accumulator) {
            const bool equal =
                static_cast<std::uint8_t>(accumulator) == zero_page[operand];
            const auto answer =
                ask(model,
                    static_cast<std::uint8_t>(accumulator),
                    code);
            enumerated = enumerated
                && answer
                    == (equal ? HuC6280IdleSkip::kIterationCycles : 0);
        }
    }
    check(
        enumerated,
        "the spin is idle for exactly the accumulator the operand names");
    zero_page[0x00U] = 0x4BU;
    for (std::size_t operand = 1; operand < zero_page.size(); ++operand) {
        zero_page[operand] = 0U;
    }

    // The four bytes are read from the machine every time, so an address
    // holding something else -- a different opcode, a displacement landing
    // somewhere other than back on the compare, or a stream that runs out --
    // is refused rather than skipped through.
    HuC6280IdleSkip refuser{HuC6280IdleSkip::Loop{}};
    const auto refused = [&](const std::array<std::uint8_t, 4>& code) {
        return ask(refuser, 0x4BU, code) == 0;
    };
    check(
        refused({0xC9U, 0x00U, 0xF0U, 0xFCU}),
        "an immediate compare is not this spin");
    check(
        refused({0xC5U, 0x00U, 0xD0U, 0xFCU}),
        "a branch on difference is not this spin");
    check(
        refused({0xC5U, 0x00U, 0xF0U, 0xFEU}),
        "a branch that lands anywhere but the compare is not this spin");
    check(
        ask(refuser, 0x4BU, std::span<const std::uint8_t>(kSpin).first(3U))
            == 0,
        "an instruction stream too short to hold the spin is refused");
    check(
        refuser.iteration_cycles(0x4BU, kSpin, {}) == 0,
        "a machine offering no readable zero page is refused");
    check(
        refuser.iteration_cycles(
            0x4BU,
            std::array<std::uint8_t, 4>{0xC5U, 0x80U, 0xF0U, 0xFCU},
            std::span<const std::uint8_t>(zero_page).first(0x80U))
            == 0,
        "an operand beyond the mapped zero page is refused");
    check(
        refuser.coverage().refusals == 6U
            && refuser.coverage().idle == 0U
            && refuser.coverage().exits == 0U,
        "every refusal is counted as a refusal and nothing else");

    // What the machine did with the answer, which the model cannot work out
    // for itself: neither the horizon it was measured against nor the rate
    // that prices a turn is the model's to see.
    HuC6280IdleSkip counter{HuC6280IdleSkip::Loop{}};
    counter.advanced(12, 288, 24);
    counter.advanced(5, 120, 24);
    // An idle answer the horizon or a standing request left no room to skip.
    // It is still a turn of the loop the machine waited through.
    counter.advanced(0, 0, 24);
    check(
        counter.coverage().skips == 2U
            && counter.coverage().iterations == 17U
            && counter.coverage().skipped_master_cycles == 408U
            && counter.coverage().longest_skip == 12U,
        "skipped iterations, master cycles and the longest skip accumulate");
    // Three idle answers cost a turn each, and the skips add their own.
    check(
        counter.coverage().wait_master_cycles == 408U + 72U,
        "the wait counts executed turns as well as skipped ones");
    // A wait that skipped nothing must still register as waiting, or the
    // measured wait would shrink as the fast-forward improved.
    HuC6280IdleSkip unskipped{HuC6280IdleSkip::Loop{}};
    unskipped.advanced(0, 0, 96);
    check(
        unskipped.coverage().skips == 0U
            && unskipped.coverage().wait_master_cycles == 96U,
        "a turn nobody skipped is still a turn spent waiting");
}

void test_pce_pad()
{
    using devils_crush::input::PcePad;

    // What every read carries no matter what is plugged in: no CD unit, and
    // two bits this hardware reads back set.
    constexpr std::uint8_t kPortBits = 0xB0U;

    const auto plug_in_five_gamepads = [](PcePad& pad) {
        pad.configure(true, false, false);
        for (std::size_t port = 0; port < PcePad::kPortCount; ++port) {
            pad.configure_port(port, PcePad::Device::gamepad);
        }
    };

    PcePad pad;
    plug_in_five_gamepads(pad);

    // A press is the host's until the machine latches it. Nothing a game can
    // do between two frames makes it visible early, which is what makes a
    // frame's input one fixed thing.
    pad.set_port_input(0, 0x0001U);
    check(
        pad.read(0, 0x1000U) == (kPortBits | 0x0FU),
        "a device answers nothing until the machine latches the frame");
    pad.frame(true);
    check(
        pad.read(0, 0x1000U) == (kPortBits | 0x0EU),
        "the frame latch is where a press becomes visible");

    // Select picks which half of the pad answers: the four buttons low, the
    // four directions high. A pad answers active-low, so a held button is a
    // cleared bit.
    pad.set_port_input(0, 0x0010U);
    pad.frame(false);
    check(
        pad.read(0, 0x1000U) == (kPortBits | 0x0FU),
        "a direction is not in the half select answers with low");
    pad.write(0, 0x1000U, 0x03U);
    check(
        pad.read(0, 0x1000U) == (kPortBits | 0x0EU),
        "select high answers with the direction half");

    // Walking the multitap: the counter is reset by a clear edge taken while
    // select is high, and stepped by a select edge taken while clear stays
    // low. Only the first port has anything pressed on it, so the counter's
    // position is readable in the answer.
    pad.set_port_input(1, 0x0002U);
    pad.frame(false);
    pad.write(0, 0x1000U, 0x00U);
    pad.write(0, 0x1000U, 0x01U);
    check(
        pad.read(0, 0x1000U) == (kPortBits | 0x0FU),
        "a select edge with clear low steps the multitap to the next port");
    pad.write(0, 0x1000U, 0x03U);
    check(
        pad.read(0, 0x1000U) == (kPortBits | 0x0EU),
        "a clear edge with select high rewinds the multitap to the first");

    // Past the last device the port answers zero rather than the all-high
    // pattern of an empty one, which is how a game counts how many pads are
    // attached.
    PcePad walker;
    plug_in_five_gamepads(walker);
    walker.frame(true);
    for (int step = 0; step < static_cast<int>(PcePad::kPortCount) + 1;
         ++step) {
        walker.write(0, 0x1000U, 0x00U);
        walker.write(0, 0x1000U, 0x01U);
    }
    check(
        walker.read(0, 0x1000U) == kPortBits,
        "a read past the last device answers zero, not an empty port");
    check(
        walker.coverage().reads_past_the_last_port == 1U
            && walker.coverage().counter_increments
                == PcePad::kPortCount + 1U,
        "walking past the last device is counted as its own path");

    // An empty port floats high, and so does one holding a device this model
    // does not answer for -- which is counted, because answering as an empty
    // port is a defensible thing to do and hiding it is not.
    PcePad sparse;
    sparse.configure(true, false, false);
    sparse.configure_port(0, PcePad::Device::none);
    sparse.configure_port(1, PcePad::Device::mouse);
    sparse.set_port_input(0, 0x00FFU);
    sparse.frame(true);
    check(
        sparse.read(0, 0x1000U) == (kPortBits | 0x0FU),
        "an empty port answers every line high");
    check(
        sparse.coverage().unmodelled_port_devices == 1U
            && sparse.coverage().absent_port_reads == 1U,
        "a device this model does not answer for is counted, not assumed");

    // The CD-unit bit is the port's rather than a device's.
    PcePad with_cd;
    with_cd.configure(true, false, true);
    with_cd.configure_port(0, PcePad::Device::gamepad);
    with_cd.frame(true);
    check(
        with_cd.read(0, 0x1000U) == (0x30U | 0x0FU),
        "an attached CD unit clears the bit that says there is none");

    // The six-button half-selector toggles on the same edge whichever pad is
    // plugged in; what a two-button pad never does is answer from the half it
    // selects, because the half is only reached by a pad that says it has one.
    PcePad six;
    six.configure(true, false, false);
    six.configure_port(0, PcePad::Device::gamepad);
    six.set_port_input(0, 0x0200U);
    six.frame(true);
    six.write(0, 0x1000U, 0x03U);
    six.write(0, 0x1000U, 0x01U);
    check(
        six.coverage().six_button_toggles == 1U
            && six.coverage().six_button_reads == 0U,
        "a two-button pad toggles the half-selector and never answers from "
        "it");
    six.set_port_input(0, static_cast<std::uint16_t>(0x0200U
        | PcePad::kSixButtonPad));
    six.frame(false);
    check(
        six.read(0, 0x1000U) == kPortBits
            && six.coverage().six_button_reads == 1U,
        "a six-button pad answers its selected half with nothing at all "
        "while select is high");
    six.write(0, 0x1000U, 0x00U);
    check(
        six.read(0, 0x1000U) == (kPortBits | 0x0DU)
            && six.coverage().six_button_reads == 2U,
        "and with the four extra buttons while select is low");

    // The two filters. Both act where a frontend acts, on the way in, so the
    // machine below cannot tell a combination that was filtered out from one
    // that was never pressed.
    PcePad filtered;
    filtered.configure(true, true, false);
    filtered.configure_port(0, PcePad::Device::gamepad);
    filtered.set_port_input(0, 0x000CU);
    filtered.frame(true);
    check(
        filtered.read(0, 0x1000U) == (kPortBits | 0x0FU)
            && filtered.coverage().soft_resets_suppressed == 1U,
        "select with run is suppressed when the machine asks for it");
    filtered.set_port_input(0, 0x0050U);
    filtered.frame(false);
    filtered.write(0, 0x1000U, 0x03U);
    check(
        filtered.read(0, 0x1000U) == (kPortBits | 0x0FU)
            && filtered.coverage().opposing_directions_filtered == 1U,
        "up and down together are dropped, because a pad cannot do it");

    PcePad unfiltered;
    unfiltered.configure(true, false, false);
    unfiltered.configure_port(0, PcePad::Device::gamepad);
    unfiltered.set_frontend_policy({.allow_opposing_directions = true});
    unfiltered.set_port_input(0, 0x005CU);
    unfiltered.frame(true);
    check(
        unfiltered.read(0, 0x1000U) == (kPortBits | 0x03U)
            && unfiltered.coverage().soft_resets_suppressed == 0U
            && unfiltered.coverage().opposing_directions_filtered == 0U,
        "a host that asks for both keeps both");

    // A reset clears what the machine latched, and deliberately not what the
    // host is holding: a button still pressed through a reset is still
    // pressed after it, and the next latch is where that shows.
    PcePad reset;
    reset.configure(true, false, false);
    reset.configure_port(0, PcePad::Device::gamepad);
    reset.set_port_input(0, 0x0001U);
    reset.frame(true);
    reset.write(0, 0x1000U, 0x03U);
    reset.power(0);
    check(
        reset.read(0, 0x1000U) == (kPortBits | 0x0FU),
        "a reset clears the latched buttons and the strobe lines");
    reset.frame(true);
    check(
        reset.read(0, 0x1000U) == (kPortBits | 0x0EU),
        "what the host is holding survives a reset and returns at the latch");

    // The savestate shuttle carries every field the emulator's format does,
    // and deliberately not the host's present.
    PcePad::State captured;
    reset.capture_state(captured);
    check(
        captured.ports[0].buttons == 0x0001U && captured.read_index == 0U
            && !captured.select && !captured.clear,
        "the captured state is the port and its devices as they stand");

    PcePad restored;
    restored.configure(true, false, false);
    restored.configure_port(0, PcePad::Device::gamepad);
    PcePad::State written = captured;
    written.read_index = 3U;
    written.select = true;
    written.ports[0].six_button_half = true;
    restored.apply_state(written);
    PcePad::State round_trip;
    restored.capture_state(round_trip);
    check(round_trip == written, "an applied state captures back unchanged");
    check(
        restored.read(0, 0x1000U) == (kPortBits | 0x0FU),
        "a restored read counter answers from the device it names");

    // The counter that the ownership claim rests on: input reaching the
    // machine, measured where it arrives.
    check(
        reset.coverage().reads_with_a_button_down == 1U,
        "a read that answered with a button held is counted as one");
}

void test_actor_scan_run()
{
    std::vector<std::uint8_t> work_ram(0x2000U, 0U);
    const auto reset = [&work_ram]() {
        std::ranges::fill(work_ram, static_cast<std::uint8_t>(0));
    };
    dc_actor_scan_run run{};

    // Every slot idle: the walk runs out of slots and exits at the loop's
    // terminal comparison.
    reset();
    check(
        dc_classify_actor_scan_run(
            work_ram.data(), work_ram.size(), 0U, 0x80U, &run) == 1,
        "actor scan classifies an entirely idle sweep");
    check(run.exit == DC_ACTOR_SCAN_COMPLETE, "idle sweep completes");
    check(run.skipped == 0x80U && run.slot == 0x80U, "idle sweep walks 128 slots");

    // A negative attribute byte dispatches immediately.
    reset();
    work_ram[0x1000U + 3U] = 0x80U;
    work_ram[0x1080U + 3U] = 0x80U;
    check(
        dc_classify_actor_scan_run(
            work_ram.data(), work_ram.size(), 0U, 0x80U, &run) == 1,
        "actor scan classifies a negative dispatch");
    check(
        run.exit == DC_ACTOR_SCAN_DISPATCH_NEGATIVE && run.slot == 3U
            && run.skipped == 3U && run.accumulator == 0x80U,
        "negative dispatch stops on its slot with the attribute byte");

    // Bit 1 clear on a positive attribute byte also dispatches, leaving the
    // masked accumulator behind.
    reset();
    work_ram[0x1000U + 1U] = 0x80U;
    work_ram[0x1080U + 1U] = 0x04U;
    check(
        dc_classify_actor_scan_run(
            work_ram.data(), work_ram.size(), 0U, 0x80U, &run) == 1,
        "actor scan classifies a masked dispatch");
    check(
        run.exit == DC_ACTOR_SCAN_DISPATCH_MASKED && run.slot == 1U
            && run.accumulator == 0U,
        "masked dispatch clears the accumulator");

    // Bit 1 set keeps the slot idle and records the costlier skip shape.
    reset();
    work_ram[0x1000U + 0U] = 0x80U;
    work_ram[0x1080U + 0U] = 0x02U;
    work_ram[0x1000U + 1U] = 0x80U;
    work_ram[0x1080U + 1U] = 0x80U;
    check(
        dc_classify_actor_scan_run(
            work_ram.data(), work_ram.size(), 0U, 0x80U, &run) == 1,
        "actor scan classifies a masked skip");
    check(
        run.skipped == 1U
            && run.skips[0] == static_cast<std::uint8_t>(
                   DC_ACTOR_SCAN_SKIP_MASKED),
        "masked skip is recorded with its own shape");

    // A spent budget rests at the loop head without consuming a slot.
    reset();
    check(
        dc_classify_actor_scan_run(
            work_ram.data(), work_ram.size(), 0U, 4U, &run) == 1,
        "actor scan honours its skip budget");
    check(
        run.exit == DC_ACTOR_SCAN_BUDGET && run.skipped == 4U
            && run.slot == 4U,
        "budgeted walk stops after its allowance");

    // Malformed inputs are refused rather than guessed at.
    check(
        dc_classify_actor_scan_run(nullptr, 0x2000U, 0U, 1U, &run) == 0,
        "actor scan rejects a missing snapshot");
    check(
        dc_classify_actor_scan_run(
            work_ram.data(), 0x100U, 0U, 1U, &run) == 0,
        "actor scan rejects a short snapshot");
    check(
        dc_classify_actor_scan_run(
            work_ram.data(), work_ram.size(), 0x80U, 1U, &run) == 0,
        "actor scan rejects an out-of-range slot");
}

void test_actor_dispatch_vector()
{
    std::vector<std::uint8_t> work_ram(0x2000U, 0U);
    dc_actor_dispatch_vector vector{};

    work_ram[0x1F00U + 0x12U] = 0x9AU;
    work_ram[0x1F80U + 0x12U] = 0xD3U;
    check(
        dc_classify_actor_dispatch_vector(
            work_ram.data(), work_ram.size(), 0x12U, &vector) == 1,
        "dispatch vector reads its jump-table pair");
    check(
        vector.vector_low == 0x9AU && vector.vector_high == 0xD3U
            && vector.handler == 0xD39AU,
        "dispatch vector assembles the handler little-endian");

    // Malformed inputs are refused rather than guessed at.
    check(
        dc_classify_actor_dispatch_vector(nullptr, 0x2000U, 0U, &vector) == 0,
        "dispatch vector rejects a missing snapshot");
    check(
        dc_classify_actor_dispatch_vector(
            work_ram.data(), 0x100U, 0U, &vector) == 0,
        "dispatch vector rejects a short snapshot");
}

void test_actor_queue_registration()
{
    std::vector<std::uint8_t> work_ram(0x2000U, 0U);
    const auto reset = [&work_ram]() {
        std::ranges::fill(work_ram, static_cast<std::uint8_t>(0));
    };
    dc_actor_queue_registration registration{};

    // Slot zero is the loop's own sentinel and files nothing.
    reset();
    check(
        dc_classify_actor_queue_registration(
            work_ram.data(), work_ram.size(), 0U, &registration) == 1,
        "queue registration classifies the sentinel slot");
    check(
        registration.exit == DC_ACTOR_QUEUE_IDLE_SLOT,
        "sentinel slot returns untouched");

    // A positive attribute byte on the dispatched slot files nothing either,
    // and the $4D cursor is never consulted.
    reset();
    work_ram[0x1080U + 5U] = 0x7FU;
    check(
        dc_classify_actor_queue_registration(
            work_ram.data(), work_ram.size(), 5U, &registration) == 1,
        "queue registration classifies an inactive slot");
    check(
        registration.exit == DC_ACTOR_QUEUE_INACTIVE
            && registration.slot == 5U,
        "inactive slot returns before the cursor reload");

    // Attribute bit 0 with state bit 6 set leaves for the emulated helper
    // after clearing the bit, and files nothing.
    reset();
    work_ram[0x004DU] = 9U;
    work_ram[0x1080U + 5U] = 0x80U;
    work_ram[0x1080U + 9U] = 0x81U;
    work_ram[0x1800U + 9U] = 0xC3U;
    check(
        dc_classify_actor_queue_registration(
            work_ram.data(), work_ram.size(), 5U, &registration) == 1,
        "queue registration classifies a clear request");
    check(
        registration.exit == DC_ACTOR_QUEUE_CLEAR_REQUEST
            && registration.slot == 9U
            && registration.clear_state == 0xC3U
            && registration.cleared_state == 0x83U,
        "clear request drops state bit 6 and stops at the helper");

    // The same bit 0 with state bit 6 already clear falls through to the
    // queue selection instead.
    reset();
    work_ram[0x004DU] = 9U;
    work_ram[0x1080U + 5U] = 0x80U;
    work_ram[0x1080U + 9U] = 0xA1U;
    work_ram[0x1800U + 9U] = 0x03U;
    check(
        dc_classify_actor_queue_registration(
            work_ram.data(), work_ram.size(), 5U, &registration) == 1,
        "queue registration classifies a satisfied clear request");
    check(
        registration.exit == DC_ACTOR_QUEUE_SUPPRESSED,
        "attribute bit 5 suppresses emission");

    // Bit 2 files into the $2C3A queue and advances its cursor upward.
    reset();
    work_ram[0x004DU] = 9U;
    work_ram[0x1080U + 5U] = 0x80U;
    work_ram[0x1080U + 9U] = 0x04U;
    work_ram[0x0CBCU] = 7U;
    check(
        dc_classify_actor_queue_registration(
            work_ram.data(), work_ram.size(), 5U, &registration) == 1,
        "queue registration classifies the first queue");
    check(
        registration.exit == DC_ACTOR_QUEUE_FIRST
            && registration.cursor == 7U && registration.queue_index == 7U
            && registration.updated_cursor == 8U,
        "first queue writes at its cursor and bumps it");

    // Bit 3 files into the shared queue from the ascending cursor.
    reset();
    work_ram[0x004DU] = 9U;
    work_ram[0x1080U + 5U] = 0x80U;
    work_ram[0x1080U + 9U] = 0x08U;
    work_ram[0x0CBAU] = 2U;
    check(
        dc_classify_actor_queue_registration(
            work_ram.data(), work_ram.size(), 5U, &registration) == 1,
        "queue registration classifies the second queue");
    check(
        registration.exit == DC_ACTOR_QUEUE_SECOND
            && registration.queue_index == 2U
            && registration.updated_cursor == 3U,
        "second queue writes at its cursor and bumps it");

    // Neither bit files into the same shared queue from the descending
    // cursor, which is what keeps the emitter's $7F walk in order.
    reset();
    work_ram[0x004DU] = 9U;
    work_ram[0x1080U + 5U] = 0x80U;
    work_ram[0x1080U + 9U] = 0x00U;
    work_ram[0x0CBBU] = 0x7FU;
    check(
        dc_classify_actor_queue_registration(
            work_ram.data(), work_ram.size(), 5U, &registration) == 1,
        "queue registration classifies the third queue");
    check(
        registration.exit == DC_ACTOR_QUEUE_THIRD
            && registration.queue_index == 0x7FU
            && registration.updated_cursor == 0x7EU,
        "third queue writes at its cursor and lowers it");

    // Malformed inputs are refused rather than guessed at.
    check(
        dc_classify_actor_queue_registration(nullptr, 0x2000U, 1U, &registration)
            == 0,
        "queue registration rejects a missing snapshot");
    check(
        dc_classify_actor_queue_registration(
            work_ram.data(), 0x100U, 1U, &registration) == 0,
        "queue registration rejects a short snapshot");
}

void test_collision_scan_run()
{
    std::vector<std::uint8_t> work_ram(0x2000U, 0U);
    const auto reset = [&work_ram]() {
        std::ranges::fill(work_ram, static_cast<std::uint8_t>(0));
    };
    dc_collision_scan_run run{};

    // Every slot positive: the countdown runs out and the sweep falls through.
    reset();
    check(
        dc_classify_collision_scan_run(
            work_ram.data(), work_ram.size(), 4U, 0x7CU, 0U, 0x80U, &run) == 1,
        "collision scan classifies an entirely rejected sweep");
    check(
        run.exit == DC_COLLISION_SCAN_EXHAUSTED && run.skipped == 0x7CU
            && run.remaining == 0U && run.slot == 0x80U,
        "rejected sweep exhausts its countdown");

    // A negative attribute byte without bit 6 is eligible.
    reset();
    work_ram[0x1080U + 6U] = 0x80U;
    check(
        dc_classify_collision_scan_run(
            work_ram.data(), work_ram.size(), 4U, 0x7CU, 0U, 0x80U, &run) == 1,
        "collision scan finds an eligible slot");
    check(
        run.exit == DC_COLLISION_SCAN_CANDIDATE && run.slot == 6U
            && run.skipped == 2U && run.remaining == 0x7AU
            && run.accumulator == 0U,
        "eligible slot stops the sweep with the zero gate loaded");

    // Bit 6 rejects a negative slot and records the costlier shape.
    reset();
    work_ram[0x1080U + 4U] = 0xC0U;
    work_ram[0x1080U + 5U] = 0x80U;
    check(
        dc_classify_collision_scan_run(
            work_ram.data(), work_ram.size(), 4U, 0x7CU, 0U, 0x80U, &run) == 1,
        "collision scan rejects a masked slot");
    check(
        run.skipped == 1U
            && run.skips[0] == static_cast<std::uint8_t>(
                   DC_COLLISION_SCAN_SKIP_MASKED)
            && run.exit == DC_COLLISION_SCAN_CANDIDATE,
        "masked rejection is recorded with its own shape");

    // A spent budget rests at the sweep head.
    reset();
    check(
        dc_classify_collision_scan_run(
            work_ram.data(), work_ram.size(), 4U, 0x7CU, 0U, 3U, &run) == 1,
        "collision scan honours its skip budget");
    check(
        run.exit == DC_COLLISION_SCAN_BUDGET && run.skipped == 3U
            && run.slot == 7U && run.remaining == 0x79U,
        "budgeted sweep stops after its allowance");

    // A nonzero gate uses the companion table: its clear bit-six rejects the
    // first otherwise eligible slot, and its set bit-six enters the scan.
    reset();
    work_ram[0x1080U + 4U] = 0x80U;
    work_ram[0x1080U + 5U] = 0x80U;
    work_ram[0x1000U + 5U] = 0x40U;
    check(
        dc_classify_collision_scan_run(
            work_ram.data(), work_ram.size(), 4U, 0x7CU, 1U, 0x80U, &run) == 1,
        "collision scan classifies a nonzero gate");
    check(
        run.skipped == 1U,
        "nonzero gate advances past its rejected companion bit");
    check(
        run.skips[0] == static_cast<std::uint8_t>(
            DC_COLLISION_SCAN_SKIP_GATED),
        "nonzero gate records its companion-bit rejection shape");
    check(
        run.exit == DC_COLLISION_SCAN_CANDIDATE && run.slot == 5U
            && run.remaining == 0x7BU && run.accumulator == 0x40U,
        "nonzero gate accepts its set companion bit");
    check(
        dc_classify_collision_scan_run(
            work_ram.data(), work_ram.size(), 4U, 0U, 0U, 0x80U, &run) == 0,
        "collision scan refuses an empty countdown");
    check(
        dc_classify_collision_scan_run(
            nullptr, 0x2000U, 4U, 0x7CU, 0U, 0x80U, &run) == 0,
        "collision scan rejects a missing snapshot");
}

void test_sprite_emit_run()
{
    // A synthetic bank standing in for the routine's own: one descriptor
    // pointer table, one frame pointer, then a single six-byte entry.
    std::vector<std::uint8_t> rom(DC_SPRITE_EMIT_ROM_SIZE, 0U);
    std::vector<std::uint8_t> work_ram(0x2000U, 0U);
    const auto at = [&rom](const std::uint16_t address) -> std::uint8_t& {
        return rom[address - DC_SPRITE_EMIT_ROM_BASE];
    };
    constexpr std::uint16_t kDescriptor = 0x4300U;
    constexpr std::uint16_t kEntries = 0x4400U;

    const auto reset = [&]() {
        std::ranges::fill(rom, static_cast<std::uint8_t>(0));
        std::ranges::fill(work_ram, static_cast<std::uint8_t>(0));
        // Descriptor id 0 resolves to kDescriptor, whose frame 0 entry list
        // is kEntries.
        at(0x41ACU) = static_cast<std::uint8_t>(kDescriptor);
        at(0x41F1U) = static_cast<std::uint8_t>(kDescriptor >> 8U);
        at(kDescriptor) = static_cast<std::uint8_t>(kEntries);
        at(kDescriptor + 1U) = static_cast<std::uint8_t>(kEntries >> 8U);
        at(kEntries) = 1U;  // one entry
    };

    dc_sprite_emit_run run{};

    // A plain entry: every offset positive, no attribute merge.
    reset();
    at(kEntries + 1U) = 0x10U;  // vertical offset
    at(kEntries + 2U) = 0x20U;  // horizontal offset
    at(kEntries + 3U) = 0x30U;  // pattern low
    at(kEntries + 4U) = 0x00U;  // pattern high
    at(kEntries + 5U) = 0x44U;  // attribute low
    at(kEntries + 6U) = 0x55U;  // attribute high
    work_ram[0x1300U + 7U] = 0x04U;  // vertical low
    work_ram[0x1380U + 7U] = 0x01U;  // vertical high
    work_ram[0x1180U + 7U] = 0x05U;  // horizontal low
    work_ram[0x1200U + 7U] = 0x02U;  // horizontal high
    check(
        dc_classify_sprite_emit_run(
            work_ram.data(), work_ram.size(), rom.data(), rom.size(), 7U,
            &run) == 1,
        "sprite emit classifies a single positive entry");
    check(
        run.count == 1U && run.cursor == 1U && run.final_y == 7U
            && run.entry_pointer == kEntries,
        "single entry advances the cursor and the descriptor index");
    check(
        run.entries[0].widen_low == 0x14U && run.entries[0].widen_high == 0x01U,
        "positive vertical offset widens without sign extension");
    check(
        run.entries[0].vertical_low == 0x54U
            && run.entries[0].vertical_high == 0x01U,
        "vertical origin adds after the camera subtracts");
    check(
        run.entries[0].horizontal_low == 0x25U
            && run.entries[0].horizontal_high == 0x02U,
        "positive horizontal offset widens without sign extension");
    check(
        run.entries[0].attribute_low == 0x44U
            && run.entries[0].attribute_high == 0x55U
            && run.entries[0].attribute_low_merged == 0U,
        "an unflagged actor passes its attribute bytes through");
    check(
        run.entries[0].descriptor[0] == 0x10U
            && run.entries[0].descriptor[1] == 0x20U
            && run.entries[0].descriptor[2] == 0x30U
            && run.entries[0].descriptor[3] == 0x00U
            && run.entries[0].descriptor[4] == 0x44U
            && run.entries[0].descriptor[5] == 0x55U,
        "the six descriptor bytes are reported alongside the results");

    // A negative vertical offset sign-extends through $3380,X.
    reset();
    at(kEntries + 1U) = 0xF0U;
    work_ram[0x1300U + 7U] = 0x04U;
    work_ram[0x1380U + 7U] = 0x01U;
    check(
        dc_classify_sprite_emit_run(
            work_ram.data(), work_ram.size(), rom.data(), rom.size(), 7U,
            &run) == 1,
        "sprite emit classifies a negative vertical entry");
    check(
        run.entries[0].vertical_negative == 1U
            && run.entries[0].widen_low == 0xF4U
            && run.entries[0].widen_high == 0x00U,
        "negative vertical offset borrows from the high byte");

    // The camera origin subtracts out of the widened coordinate.
    reset();
    at(kEntries + 1U) = 0x10U;
    work_ram[0x1300U + 7U] = 0x04U;
    work_ram[0x1380U + 7U] = 0x01U;
    work_ram[0xA9U] = 0x20U;  // camera vertical low
    work_ram[0xAAU] = 0x01U;  // camera vertical high
    check(
        dc_classify_sprite_emit_run(
            work_ram.data(), work_ram.size(), rom.data(), rom.size(), 7U,
            &run) == 1,
        "sprite emit classifies a camera-relative entry");
    // $0114 - $0120 borrows to $FFF4, and the $40 origin carries it back.
    check(
        run.entries[0].camera_low == 0xF4U
            && run.entries[0].camera_high == 0xFFU
            && run.entries[0].vertical_low == 0x34U
            && run.entries[0].vertical_high == 0x00U,
        "camera subtraction borrows before the origin adds");

    // A bit-4 actor merges $3D00,X into the attribute low byte.
    reset();
    at(kEntries + 5U) = 0x41U;
    work_ram[0x1800U + 7U] = 0x10U;
    work_ram[0x1D00U + 7U] = 0x82U;
    check(
        dc_classify_sprite_emit_run(
            work_ram.data(), work_ram.size(), rom.data(), rom.size(), 7U,
            &run) == 1,
        "sprite emit classifies a merged-attribute entry");
    check(
        run.entries[0].attribute_low_merged == 1U
            && run.entries[0].attribute_low == 0xC3U,
        "a bit-4 actor merges its attribute low byte");

    // Two entries advance six descriptor bytes at a time.
    reset();
    at(kEntries) = 2U;
    at(kEntries + 7U) = 0x01U;
    at(kEntries + 11U) = 0x66U;
    at(kEntries + 12U) = 0x77U;
    check(
        dc_classify_sprite_emit_run(
            work_ram.data(), work_ram.size(), rom.data(), rom.size(), 7U,
            &run) == 1,
        "sprite emit classifies a two-entry descriptor");
    check(
        run.count == 2U && run.cursor == 2U && run.final_y == 13U
            && run.entries[1].vertical_low == 0x41U
            && run.entries[1].attribute_low == 0x66U
            && run.entries[1].attribute_high == 0x77U,
        "the second entry reads six bytes further on");
    check(
        run.accumulator == 0x77U,
        "the accumulator returns the last attribute high byte");

    // Shapes the classifier refuses rather than model.
    reset();
    at(kEntries) = 0U;
    check(
        dc_classify_sprite_emit_run(
            work_ram.data(), work_ram.size(), rom.data(), rom.size(), 7U,
            &run) == 0,
        "sprite emit refuses a zero entry count, which would underflow $2C");

    reset();
    work_ram[0x4EU] = 0x3FU;
    check(
        dc_classify_sprite_emit_run(
            work_ram.data(), work_ram.size(), rom.data(), rom.size(), 7U,
            &run) == 0,
        "sprite emit refuses a cursor overflow, which returns early");

    reset();
    work_ram[0x1800U + 7U] = 0x20U;
    check(
        dc_classify_sprite_emit_run(
            work_ram.data(), work_ram.size(), rom.data(), rom.size(), 7U,
            &run) == 0,
        "sprite emit refuses the unobserved attribute-high merge");

    reset();
    at(kDescriptor) = 0x00U;
    at(kDescriptor + 1U) = 0x20U;  // $2000 is outside the routine's bank
    check(
        dc_classify_sprite_emit_run(
            work_ram.data(), work_ram.size(), rom.data(), rom.size(), 7U,
            &run) == 0,
        "sprite emit refuses a pointer outside its own bank");

    reset();
    check(
        dc_classify_sprite_emit_run(
            nullptr, 0x2000U, rom.data(), rom.size(), 7U, &run) == 0,
        "sprite emit rejects a missing snapshot");
}

void test_text_tile_write()
{
    dc_text_tile_write write{};
    check(
        dc_prepare_text_tile_write(0x41U, 0x02CCU, &write) == 1,
        "text tile writer accepts a letter");
    check(
        write.character == 0x41U && write.tile_low == 0x8AU
            && write.tile_high == 0x02U && write.next_address == 0x02CDU,
        "text tile writer maps a letter and advances its VDC address");

    check(
        dc_prepare_text_tile_write(0x30U, 0x12FFU, &write) == 1
            && write.character == 0x30U && write.tile_low == 0x80U
            && write.tile_high == 0x02U && write.next_address == 0x1300U,
        "text tile writer maps a digit and carries the VDC address");
    check(
        dc_prepare_text_tile_write(0x20U, 0xFFFFU, &write) == 1
            && write.character == 0x5BU && write.tile_low == 0xA4U
            && write.tile_high == 0x02U && write.next_address == 0x0000U,
        "text tile writer maps a space to the blank glyph and wraps cleanly");
    check(
        dc_prepare_text_tile_write(0xFFU, 0U, &write) == 1
            && write.tile_low == 0x48U && write.tile_high == 0x03U,
        "text tile writer preserves the tile-index carry in the high byte");
    check(
        dc_prepare_text_tile_write(0x41U, 0U, nullptr) == 0,
        "text tile writer rejects a missing result");

    check(
        dc_video_update_gate_is_idle(0U, 0U) == 1,
        "video update gate recognizes the idle return");
    check(
        dc_video_update_gate_is_idle(1U, 0U) == 0
            && dc_video_update_gate_is_idle(0U, 1U) == 0,
        "video update gate preserves both active renderer paths");
}

void test_table_motion()
{
    using devils_crush::render::table_motion_offset;
    using devils_crush::render::table_motion_correction;
    devils_crush::cabinet::TableActorState old{
        .slot = 0, .type = 1, .appearance = 2, .world_x = 100, .world_y = 200};
    auto now = old;
    now.world_x += 3;
    now.world_y -= 2;
    check(table_motion_offset(now, &old, 0.25F) == std::pair{-2.25F, 1.5F},
        "table motion retains fractional positions on both axes");
    check(table_motion_offset(now, &old, 1.0F) == std::pair{0.0F, 0.0F},
        "table motion arrives at the current actor position");
    now.dispatch = 0x1234;
    check(table_motion_offset(now, &old, 0.5F).first == -1.5F,
        "normal coroutine changes do not interrupt motion");
    check(table_motion_offset(now, nullptr, 0.0F) == std::pair{0.0F, 0.0F},
        "new actors appear at their actual position");
    now.world_x += 40;
    check(table_motion_offset(now, &old, 0.5F) == std::pair{0.0F, 0.0F},
        "teleports do not slide across the table");
    now = old;
    now.world_x += 1;
    now.type = 3;
    check(table_motion_offset(now, &old, 0.5F) == std::pair{0.0F, 0.0F},
        "reused actor slots do not inherit their predecessor's motion");

    auto older = old;
    old.world_x = 102;
    now = old;
    now.world_x = 103;
    const auto short_step = table_motion_correction(now, &old, &older);
    check(short_step == std::pair{0.25F, 0.0F},
        "uneven pixel steps receive a quarter-pixel correction");
    auto next = now;
    next.world_x = 105;
    const auto long_step = table_motion_correction(next, &now, &old);
    check(next.world_x + long_step.first - (now.world_x + short_step.first) == 1.5F,
        "alternating two/one-pixel steps become even 1.5-pixel motion");
    now.world_x = 104;
    check(table_motion_correction(now, &old, &older) == std::pair{0.0F, 0.0F},
        "constant velocity acquires no filter delay");
    now.world_x = 101;
    check(table_motion_correction(now, &old, &older) == std::pair{0.0F, 0.0F},
        "a collision reversal bypasses the step correction");
    now.world_x = 109;
    check(table_motion_correction(now, &old, &older) == std::pair{0.0F, 0.0F},
        "sharp acceleration bypasses the step correction");
    now.world_x = 102;
    check(table_motion_correction(now, &old, &older).first == 0.5F,
        "the ball's small-stop correction is bounded to half a source pixel");
    check(table_motion_correction(now, &old, nullptr) == std::pair{0.0F, 0.0F},
        "new motion history starts without a correction");
}

void test_dmd_attract_transition()
{
    using devils_crush::cabinet::DmdPanel;
    using devils_crush::cabinet::DmdScene;
    DmdPanel panel;
    std::array<std::byte, 8192> ram{};
    panel.observe(ram);
    check(panel.attract(), "boot RAM clear stays in DMD attract mode");
    ram[0x54] = std::byte{3};
    panel.observe(ram);
    ram[0x254] = std::byte{1};
    ram[0x54] = std::byte{0};
    ram[0xD52] = std::byte{1};
    panel.observe(ram);
    check(panel.attract(), "demo ball does not start a live DMD session");
    ram[0x54] = std::byte{4};
    panel.observe(ram);
    // Recorded attract frames 5553-5558 clear all three discriminators.
    ram.fill(std::byte{0});
    for (int frame = 0; frame < 6; ++frame) {
        panel.observe(ram);
        check(panel.attract(), "Best Five to title clear cannot flash game over");
    }
    ram[0x54] = std::byte{3};
    panel.observe(ram);
    check(panel.attract(), "title continues the same attract presentation");
    ram[0x54] = std::byte{0};
    ram[0xD52] = std::byte{3};
    panel.observe(ram);
    check(!panel.attract(), "live session leaves attract presentation");
    ram[0x54] = std::byte{2};
    ram[0xD52] = std::byte{0};
    panel.observe(ram);
    check(!panel.attract() && panel.scene() == DmdScene::bonus_tally,
          "last live ball still shows its bonus tally");
    ram[0x54] = std::byte{0};
    panel.observe(ram);
    check(!panel.attract() && panel.scene() == DmdScene::score,
          "real game over remains visible after a live session");
    ram[0x54] = std::byte{4};
    panel.observe(ram);
    ram.fill(std::byte{0});
    panel.observe(ram);
    check(panel.attract(), "live game high scores also return cleanly to title");
}

void test_dmd_name_entry()
{
    using devils_crush::cabinet::DmdPanel;
    DmdPanel panel;
    std::array<std::byte, 8192> ram{};
    ram[0x54] = std::byte{0};
    ram[0xD52] = std::byte{3};
    panel.observe(ram);
    ram[0xD52] = std::byte{0};
    panel.observe(ram);
    check(!panel.attract(), "a game over shows the live page");

    // Measured: High Scores is up three frames before 09:6629 starts the entry.
    ram[0x54] = std::byte{4};
    for (int frame = 0; frame < 3; ++frame) {
        panel.observe(ram);
        check(!panel.attract() && !panel.name_entry(),
            "the carousel does not flash up before the entry");
    }
    panel.begin_name_entry();
    ram[0xD61] = std::byte{1};
    ram[0xD65] = std::byte{0};
    for (std::size_t i = 0; i < 3U; ++i) ram[0xD66 + i] = std::byte{'A'};
    panel.observe(ram);
    check(panel.name_entry() && panel.entry_name() == "AAA", "the entry starts at AAA");
    // The letters live at $2D66-$2D68 until the entry ends; the table, where
    // the carousel reads names, still has the rank's old one.
    ram[0xD66] = std::byte{'C'};
    ram[0xD67] = std::byte{0x5B};
    ram[0xD68] = std::byte{0x5C};
    ram[0xD65] = std::byte{2};
    panel.observe(ram);
    check(panel.entry_name() == "C. ", "the entry shows each letter as it is chosen");
    panel.end_name_entry();
    panel.observe(ram);
    check(panel.name_entry(), "the finished name stays up until the screen changes");
    ram[0x54] = std::byte{3};
    panel.observe(ram);
    check(!panel.name_entry() && panel.attract(), "the title returns to the carousel");

    // A game over whose score does not rank has no entry: the carousel
    // follows once the hold runs out.
    ram[0x54] = std::byte{0};
    ram[0xD52] = std::byte{3};
    panel.observe(ram);
    ram[0xD52] = std::byte{0};
    panel.observe(ram);
    ram[0x54] = std::byte{4};
    bool carousel = false;
    for (int frame = 0; frame < 12 && !carousel; ++frame) {
        panel.observe(ram);
        carousel = panel.attract();
    }
    check(carousel && !panel.name_entry(), "High Scores without an entry shows the carousel");

    // The pause menu's High Scores, left inside the hold, resumes play.
    ram[0x54] = std::byte{0};
    ram[0xD52] = std::byte{2};
    panel.observe(ram);
    ram[0x54] = std::byte{4};
    panel.observe(ram);
    ram[0x54] = std::byte{0};
    panel.observe(ram);
    check(!panel.attract(), "leaving the pause menu's High Scores returns to the live page");
}

void test_dmd_text_layout()
{
    using devils_crush::cabinet::calculate_dmd_text_layout;
    using devils_crush::cabinet::calculate_dmd_high_score_layout;
    using devils_crush::cabinet::dmd_high_score_rank;
    using devils_crush::cabinet::kDmdGlyphHeight;
    using devils_crush::cabinet::kDmdHighScoreDurationMilliseconds;
    using devils_crush::cabinet::make_dmd_high_score_content;
    using devils_crush::cabinet::make_dmd_text_content;

    const auto content = make_dmd_text_content(102000U, 3700U, 1U);
    check(
        content.text[0] == "102,000"
            && content.text[1] == "Ball 1"
            && content.text[2] == "Bonus 3,700",
        "the DMD exposes comma-formatted live values and labels");
    check(
        content.colors
            == std::array{
                devils_crush::cabinet::kDmdOrange,
                devils_crush::cabinet::kDmdOrange,
                devils_crush::cabinet::kDmdOrange},
        "the live DMD keeps its original orange text");

    // Every shape a DMD role can take: the default window, a dedicated DMD
    // strip, fullscreen 16:9 and 16:10 panels, and a portrait rotation.
    struct Output {
        int width;
        int height;
        const char* name;
    };
    constexpr std::array<Output, 5> outputs{
        Output{960, 240, "the default DMD window"},
        Output{1920, 360, "a dedicated DMD strip"},
        Output{1920, 1080, "a fullscreen 16:9 panel"},
        Output{1920, 1200, "a fullscreen 16:10 panel"},
        Output{1080, 1920, "a portrait-rotated panel"},
    };

    for (const auto& output : outputs) {
        const auto layout = calculate_dmd_text_layout(
            output.width, output.height, content);
        check(layout.valid(), "the two-tier DMD layout is drawable");

        for (const auto& line : layout.lines) {
            check(
                line.origin_x >= 0
                    && line.origin_x + line.width <= output.width,
                "each DMD text line fits horizontally");
            check(
                line.origin_y >= 0
                    && line.origin_y + line.glyph_height
                        <= output.height,
                "each DMD text line fits vertically");
        }
        check(
            layout.lines[0].cell_size > layout.lines[1].cell_size * 2
                && layout.lines[1].cell_size
                    == layout.lines[2].cell_size,
            "the score is much larger than both matching status fields");
        check(
            layout.lines[1].origin_y == layout.lines[2].origin_y
                && layout.lines[1].origin_x + layout.lines[1].width
                    < layout.lines[2].origin_x,
            "Ball and Bonus share the lower row without overlapping");
    }

    const auto default_layout =
        calculate_dmd_text_layout(960, 240, content);
    check(
        default_layout.lines[0].glyph_height
                == default_layout.lines[0].cell_size * kDmdGlyphHeight
            && default_layout.lines[1].glyph_height
                == default_layout.lines[1].cell_size * kDmdGlyphHeight,
        "both DMD tiers retain square 8-by-8 bitmap cells");
    check(
        default_layout.lines[0].width
            == (7 * devils_crush::cabinet::kDmdGlyphAdvance - 3)
                * default_layout.lines[0].cell_size,
        "a comma uses asymmetric kerning and the score stays centered");
    check(
        default_layout.lines[0].origin_y == 54
            && default_layout.lines[1].origin_y == 174
            && default_layout.lines[2].origin_y == 174,
        "the live DMD pulls both text tiers inward for larger outer margins");

    // Value growth must not resize either tier. The widest legal values use
    // the same fixed reserves as the initial values.
    const auto maximum_content =
        make_dmd_text_content(999999900U, 9900U, 3U);
    check(
        maximum_content.text[0] == "999,999,900"
            && maximum_content.text[2] == "Bonus 9,900",
        "maximum DMD point values retain their thousands separators");
    const auto maximum_layout =
        calculate_dmd_text_layout(960, 240, maximum_content);
    check(
        maximum_layout.lines[0].cell_size
                == default_layout.lines[0].cell_size
            && maximum_layout.lines[1].cell_size
                == default_layout.lines[1].cell_size,
        "DMD tier sizes stay fixed as their numbers grow");

    check(
        maximum_layout.lines[0].origin_y
                < maximum_layout.lines[1].origin_y
            && maximum_layout.lines[1].origin_y
                == maximum_layout.lines[2].origin_y,
        "the score sits above the shared Ball and Bonus row");

    check(
        !calculate_dmd_text_layout(0, 240, content).valid()
            && !calculate_dmd_text_layout(960, 0, content).valid()
            && !calculate_dmd_text_layout(40, 20, content).valid(),
        "a degenerate DMD output yields no layout");

    devils_crush::core::HighScoreTable high_scores;
    constexpr auto kRanks = devils_crush::core::kHighScoreRankCount;
    high_scores.bytes[0U] = 0x10U;
    high_scores.bytes[kRanks] = 0x27U;
    high_scores.bytes[3U * kRanks] = 'D';
    high_scores.bytes[4U * kRanks] = 'E';
    high_scores.bytes[5U * kRanks] = 'V';
    high_scores.bytes[1U] = 0xE8U;
    high_scores.bytes[kRanks + 1U] = 0x03U;
    high_scores.bytes[3U * kRanks + 1U] = 'A';
    high_scores.bytes[4U * kRanks + 1U] = 'C';
    high_scores.bytes[5U * kRanks + 1U] = 'E';

    const auto champion = make_dmd_high_score_content(high_scores, 0U);
    const auto runner_up = make_dmd_high_score_content(high_scores, 1U);
    const auto glowing_score =
        make_dmd_high_score_content(high_scores, 0U, 500U);
    const auto dimmed_score =
        make_dmd_high_score_content(high_scores, 0U, 1'000U);
    check(
        champion.text[0] == "Grand Champion"
            && champion.text[1] == "DEV"
            && champion.text[2] == "1,000,000"
            && runner_up.text[0] == "High Score #1"
            && runner_up.text[1] == "ACE"
            && runner_up.text[2] == "100,000",
        "startup DMD pages contain each rank label, initials, and formatted "
        "score");
    check(
        champion.colors[0] == devils_crush::cabinet::kDmdYellow
            && champion.colors[1] == devils_crush::cabinet::kDmdWhite
            && champion.colors[2] == devils_crush::cabinet::kDmdOrange
            && runner_up.colors == champion.colors
            && glowing_score.colors[0] == champion.colors[0]
            && glowing_score.colors[1] == champion.colors[1]
            && glowing_score.colors[2]
                == devils_crush::cabinet::kDmdBrightOrange
            && dimmed_score.colors == champion.colors,
        "high-score DMD values flash orange slowly while labels stay yellow "
        "and initials stay white");

    for (const auto& output : outputs) {
        const auto layout = calculate_dmd_high_score_layout(
            output.width, output.height, champion);
        check(layout.valid(), "the high-score DMD layout is drawable");
        for (const auto& line : layout.lines) {
            check(
                line.origin_x >= 0
                    && line.origin_x + line.width <= output.width
                    && line.origin_y >= 0
                    && line.origin_y + line.glyph_height <= output.height,
                "each high-score DMD line fits its output");
        }
        check(
            layout.lines[2].cell_size > layout.lines[0].cell_size,
            "the high-score value is larger than its rank label");
    }
    const auto default_high_score_layout =
        calculate_dmd_high_score_layout(960, 240, champion);
    check(
        default_high_score_layout.lines[0].origin_y == 26
            && default_high_score_layout.lines[1].origin_y == 96
            && default_high_score_layout.lines[2].origin_y == 158,
        "the high-score DMD pulls its outer rows inward around the initials");
    check(
        dmd_high_score_rank(0U) == 0U
            && dmd_high_score_rank(
                   kDmdHighScoreDurationMilliseconds - 1U)
                == 0U
            && dmd_high_score_rank(kDmdHighScoreDurationMilliseconds) == 1U
            && dmd_high_score_rank(
                   kDmdHighScoreDurationMilliseconds * 5U)
                == 0U,
        "the startup DMD advances every three seconds and wraps");
    check(
        !calculate_dmd_high_score_layout(40, 20, champion).valid()
            && !calculate_dmd_high_score_layout(
                    960,
                    240,
                    make_dmd_high_score_content(high_scores, kRanks))
                    .valid(),
        "invalid high-score DMD pages do not produce a layout");
}

void test_table_palette_cache()
{
    using devils_crush::render::TablePaletteCache;
    constexpr auto kBytes = TablePaletteCache::kByteCount;
    constexpr auto kPerRow = TablePaletteCache::kBytesPerRow;

    std::vector<std::byte> live(kBytes, std::byte{0x11});
    TablePaletteCache cache;

    // The first sight of the plane is the scene-start whole-table write.
    auto plane = cache.update(live, 0, 16);
    check(plane.size() == kBytes, "palette cache returns the whole plane");
    check(
        std::ranges::all_of(
            plane,
            [](const std::byte value) { return value == std::byte{0x11}; }),
        "the first update adopts the whole plane as its baseline");
    // A 16-row viewport at the top covers exactly tile rows 0 and 1.
    check(cache.learned_rows() == 2U, "only the covered rows are learned");

    // Rewrite every row, then update with the same two rows on camera.
    std::ranges::fill(live, std::byte{0x22});
    plane = cache.update(live, 0, 16);
    check(
        plane[0] == std::byte{0x22} && plane[kPerRow] == std::byte{0x22},
        "rows under the camera follow the live plane");
    // Every row follows the live plane, on camera or off. Holding rows back
    // froze them on stale animation phases, which is what put a rectangular
    // band of wrong subpalettes across the middle of the table.
    check(
        plane[2U * kPerRow] == std::byte{0x22}
            && plane[kBytes - 1U] == std::byte{0x22},
        "rows outside the camera follow the live plane too");

    // Scroll down two rows: row 2 learns, rows 0 and 1 freeze at 0x22.
    std::ranges::fill(live, std::byte{0x33});
    plane = cache.update(live, 16, 16);
    check(
        plane[2U * kPerRow] == std::byte{0x33}
            && plane[3U * kPerRow] == std::byte{0x33},
        "scrolling learns the newly covered rows");
    check(
        plane[0] == std::byte{0x33} && plane[kPerRow] == std::byte{0x33},
        "rows the camera has left keep following the live plane");
    check(
        plane[4U * kPerRow] == std::byte{0x33},
        "rows the camera has never covered follow it as well");
    check(cache.learned_rows() == 4U, "learned rows accumulate");

    // A partial viewport row still counts as covered.
    std::ranges::fill(live, std::byte{0x44});
    plane = cache.update(live, 36, 4);
    check(
        plane[4U * kPerRow] == std::byte{0x44},
        "a partially covered row is refreshed");

    // A plane shorter than the attribute region passes straight through.
    std::vector<std::byte> stub(4U, std::byte{0x55});
    const auto passthrough = cache.update(stub, 0, 16);
    check(
        passthrough.size() == stub.size(),
        "a short plane is returned unchanged");

    cache.reset();
    check(cache.learned_rows() == 0U, "reset forgets every learned row");
}

void test_native_registry()
{
    const auto admitted = devils_crush::app::admitted_native_routines();
    check(!admitted.empty(), "native registry is populated");

    const auto ordered = std::ranges::is_sorted(
        admitted,
        [](const devils_crush::app::AdmittedRoutine& left,
           const devils_crush::app::AdmittedRoutine& right) {
            return std::pair{left.routine.physical_bank, left.routine.logical_pc}
            < std::pair{right.routine.physical_bank, right.routine.logical_pc};
        });
    check(ordered, "native registry is sorted by bank and program counter");

    const auto duplicate = std::ranges::adjacent_find(
        admitted,
        [](const devils_crush::app::AdmittedRoutine& left,
           const devils_crush::app::AdmittedRoutine& right) {
            return left.routine == right.routine;
        });
    check(duplicate == admitted.end(), "native registry has no duplicate keys");

    const auto counted = std::ranges::all_of(
        admitted,
        [](const devils_crush::app::AdmittedRoutine& routine) {
            return routine.proven_native_calls != 0U
                && routine.proven_native_calls <= routine.proven_observed_calls
                && !routine.evidence.empty();
        });
    check(
        counted,
        "every admitted routine cites evidence and a proven call count");
}

void test_native_game_state_view()
{
    using devils_crush::native_runtime::ActorBehavior;
    using devils_crush::native_runtime::ActorState;
    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::capture_active_ball;
    using devils_crush::native_runtime::capture_collision_retry_ball;
    using devils_crush::native_runtime::capture_active_score;
    using devils_crush::native_runtime::capture_shared_actor_motion;
    using devils_crush::native_runtime::seed_actor_collision_state;
    using devils_crush::native_runtime::seed_shared_actor_motion;

    std::array<std::byte, GameStateView::kWorkRamSize> work_ram{};
    const auto store = [&work_ram](
                           const std::size_t offset,
                           const std::uint8_t value) {
        work_ram[offset] = static_cast<std::byte>(value);
    };
    constexpr std::uint8_t kSlot = 0x2AU;
    store(0x1000U + kSlot, 0x81U);
    store(0x1080U + kSlot, 0x2CU);
    store(0x1800U + kSlot, 0x80U);
    store(0x1F00U + kSlot, 0x34U);
    store(0x1F80U + kSlot, 0xC5U);
    store(0x1400U + kSlot, 0x80U);
    store(0x1480U + kSlot, 0xFEU);
    store(0x1500U + kSlot, 0x00U);
    store(0x1580U + kSlot, 0x02U);
    store(0x1700U + kSlot, 0x2AU);
    store(0x1880U + kSlot, 0x81U);
    store(0x1900U + kSlot, 0x06U);
    store(0x1980U + kSlot, 0x03U);
    store(0x1C00U + kSlot, 0x02U);
    store(0x1A00U + kSlot, 0x03U);
    store(0x1A80U + kSlot, 0x06U);
    store(0x1C80U + kSlot, 0x02U);
    store(0x1D00U + kSlot, 0x8CU);
    store(0x1E00U + kSlot, 0x04U);
    store(0x1E80U + kSlot, 0x09U);
    store(0x1180U + kSlot, 0x34U);
    store(0x1200U + kSlot, 0x12U);
    store(0x1300U + kSlot, 0xCDU);
    store(0x1380U + kSlot, 0xABU);

    const GameStateView state(work_ram);
    check(state.valid(), "native game-state view accepts the complete RAM image");
    const auto actor = state.actor(kSlot);
    check(actor.has_value(), "native game-state view decodes an actor slot");
    check(
        actor && actor->index == kSlot && actor->activity_flags == 0x81U
            && actor->attributes == 0x2CU && actor->collision_state == 0x80U
            && actor->behavior_counter == 0x02U,
        "native game-state view names actor scheduler fields");
    check(
        actor && actor->handler_address == 0xC534U,
        "native game-state view decodes the actor handler vector");
    check(
        actor && actor->horizontal_motion.raw == -384
            && actor->vertical_motion.raw == 512,
        "native game-state view decodes signed 8.8 actor motion");
    check(
        actor && actor->collision_table_program.selector == 0x2AU
            && actor->collision_table.pass_state == 0x81U
            && actor->collision_table.collision_event == 0x06U
            && actor->collision_table.orientation_flags == 0x8CU
            && actor->collision_table.collision_entry_cursor == 0x04U
            && actor->collision_table.record_cursor == 0x09U
            && actor->collision_table.position_x == 0x1234U
            && actor->collision_table.position_y == 0xABCDU
            && actor->collision_shape_directory_index == 0x06U
            && actor->collision_shape_phase == 0x03U
            && actor->collision_reaction.collision_event == 0x06U
            && actor->collision_reaction.phase == 0x02U
            && actor->collision_reaction.ticks_until_next_phase == 0x03U,
        "native game-state view decodes a collision-table actor seed");

    store(0x1000U, 0x81U);

    store(0x0251U, 0x03U);
    store(0x0205U + 0x03U, 0x56U);
    store(0x020CU + 0x03U, 0x34U);
    store(0x0213U + 0x03U, 0x12U);
    store(0x0CC0U, 0x01U);
    const auto active_score = state.active_score();
    check(
        active_score && active_score->slot == 0x03U
            && active_score->stored_units == 0x123456U
            && active_score->display_dirty,
        "native game-state view decodes the selected 24-bit score accumulator");
    const auto score_capture = capture_active_score(state);
    check(
        score_capture && score_capture->source == *active_score
            && score_capture->state.stored_units == 0x123456U
            && score_capture->state.display_dirty,
        "native score capture transfers the active accumulator and display latch");
    store(0x0251U, 0x07U);
    check(
        !capture_active_score(GameStateView(work_ram)),
        "native score capture rejects an invalid active-score slot");
    store(0x0251U, 0x03U);
    store(0x1100U, 0x80U);
    store(0x1180U, 0x34U);
    store(0x1200U, 0x12U);
    store(0x1280U, 0x40U);
    store(0x1300U, 0xFEU);
    store(0x1380U, 0xFFU);
    store(0x1400U, 0x80U);
    store(0x1480U, 0xFEU);
    store(0x1500U, 0x00U);
    store(0x1580U, 0x02U);
    store(0x1C80U, 0xA2U);
    store(0x1800U, 0x80U);
    store(0x00B9U, 0x80U);
    store(0x00BAU, 0xFEU);
    store(0x00BBU, 0x00U);
    store(0x00BCU, 0x02U);
    const auto ball = state.ball();
    check(
        ball && ball->activity_flags == 0x81U
            && ball->position_x.raw == 0x123480
            && ball->position_y.raw == -448
            && ball->horizontal_motion.raw == -384
            && ball->vertical_motion.raw == 512
            && ball->motion_flags == 0xA2U
            && ball->collision_state == 0x80U,
        "native game-state view preserves the ball's signed 16.8 state and collision marker");
    const auto ball_capture = capture_active_ball(state);
    check(
        ball_capture && ball_capture->source == *ball
            && ball_capture->state.position_x.raw == 0x123480
            && ball_capture->state.position_y.raw == -448
            && ball_capture->state.velocity_x == -384
            && ball_capture->state.velocity_y == 512
            && ball_capture->state.motion_flags == 0xA2U
            && ball_capture->state.collision_state == 0x80U,
        "native ball capture transfers fractional position, velocity, flags, and collision marker");
    const auto substep_motion = state.ball_substep_motion();
    check(
        substep_motion && substep_motion->horizontal == -384
            && substep_motion->vertical == 512,
        "native game-state view captures F1B4's signed split motion");
    store(0x008BU, 0x13U);
    store(0x008CU, 0xF8U);
    store(0x008DU, 0x08U);
    const auto motion_tuning = state.ball_motion_tuning();
    check(
        motion_tuning && motion_tuning->horizontal_drag_divisor == -8
            && motion_tuning->vertical_drag_divisor == 8
            && motion_tuning->vertical_gravity == 0x13U,
        "native game-state view captures DD09's signed drag and gravity inputs");
    store(0x0085U, 0x40U);
    store(0x0086U, 0x34U);
    store(0x0087U, 0x12U);
    store(0x0088U, 0x80U);
    store(0x0089U, 0xFEU);
    store(0x008AU, 0xFFU);
    const auto collision_cache = state.ball_collision_position_cache();
    const auto retry_ball_capture = capture_collision_retry_ball(state);
    check(
        collision_cache && collision_cache->position_x.raw == 0x123440
            && collision_cache->position_y.raw == -384
            && retry_ball_capture && retry_ball_capture->impact_ball == *ball_capture
            && retry_ball_capture->previous_position.position_x.raw == 0x123440
            && retry_ball_capture->previous_position.position_y.raw == -384
            && retry_ball_capture->previous_position.velocity_x == -384
            && retry_ball_capture->previous_position.velocity_y == 512
            && retry_ball_capture->parent_vertical_high_byte() == 0xFFU,
        "native retry-ball capture pairs CDF8's live impact with F1B4's cached position");
    store(0x1000U, 0x01U);
    check(
        !capture_active_ball(GameStateView(work_ram)),
        "native ball capture rejects an inactive slot-zero record");
    check(
        !capture_collision_retry_ball(GameStateView(work_ram)),
        "native retry-ball capture rejects an inactive live impact record");
    store(0x1000U, 0x81U);

    ActorState native_actors;
    check(
        native_actors.claim_dynamic_slot(kSlot, ActorBehavior::shared_motion)
            && actor && seed_actor_collision_state(native_actors, *actor),
        "native actor state accepts a typed collision capture seed");
    const auto* seeded_actor = native_actors.actor(kSlot);
    check(
        seeded_actor && seeded_actor->collision_table_program
            && seeded_actor->collision_table
            && seeded_actor->collision_reaction
            && seeded_actor->collision_table_program == actor->collision_table_program
            && *seeded_actor->collision_table == actor->collision_table
            && *seeded_actor->collision_reaction == actor->collision_reaction,
        "native collision seed preserves table and reaction state exactly");
    check(
        !seed_actor_collision_state(native_actors, GameStateView::ActorSlot{}),
        "native collision seed rejects an inactive destination actor");

    const auto motion_capture = capture_shared_actor_motion(state, kSlot);
    ActorState motion_actors;
    check(
        motion_capture && motion_capture->due_for_update
            && motion_capture->motion.position_x.raw == 0x1234 * 256
            && motion_capture->motion.position_y.raw == 0xABCD * 256
            && motion_capture->motion.velocity_x == -384
            && motion_capture->motion.velocity_y == 512
            && motion_actors.claim_dynamic_slot(kSlot, ActorBehavior::shared_motion)
            && seed_shared_actor_motion(motion_actors, *motion_capture),
        "native shared-motion capture transfers typed positions, velocity, and due state");
    const auto* seeded_motion_actor = motion_actors.actor(kSlot);
    ActorState non_motion_actors;
    check(
        seeded_motion_actor && seeded_motion_actor->motion == motion_capture->motion
            && seeded_motion_actor->due_for_update
            && non_motion_actors.claim_dynamic_slot(
                kSlot, ActorBehavior::small_soldier)
            && !seed_shared_actor_motion(non_motion_actors, *motion_capture),
        "native shared-motion capture requires an owned matching behavior");
    store(0x1000U + kSlot, 0x01U);
    check(
        !capture_shared_actor_motion(GameStateView(work_ram), kSlot),
        "native shared-motion capture rejects inactive actor records");
    store(0x1000U + kSlot, 0x81U);
    store(0x1080U + kSlot, 0x02U);
    const auto deferred_motion = capture_shared_actor_motion(GameStateView(work_ram), kSlot);
    check(
        deferred_motion && !deferred_motion->due_for_update,
        "native shared-motion capture preserves the scheduler's deferred turn");
    store(0x1080U + kSlot, 0x2CU);

    store(0x00C6U, 0x40U);
    store(0x00C7U, 0x8AU);
    store(0x00C8U, 0x80U);
    store(0x00C9U, 0x8AU);
    store(0x0090U, 0x00U);
    store(0x0091U, 0xCFU);
    const auto collision_tables = state.playfield_collision_table_pointers();
    check(
        collision_tables && collision_tables->tile_ids == 0x8A80U
            && collision_tables->collision_attributes == 0x8A40U
            && collision_tables->masks == 0xCF00U,
        "native game-state view decodes live playfield collision table pointers");
    store(0x0096U, 0x00U);
    store(0x0097U, 0x80U);
    store(0x0098U, 0x00U);
    store(0x0099U, 0x81U);
    store(0x009AU, 0x00U);
    store(0x009BU, 0x82U);
    store(0x009CU, 0x00U);
    store(0x009DU, 0x83U);
    const auto collision_shape_directories =
        state.actor_collision_shape_directory_pointers();
    check(
        collision_shape_directories
            && collision_shape_directories->tables
                == std::array<std::uint16_t, 4>{
                    0x8000U, 0x8100U, 0x8200U, 0x8300U},
        "native game-state view captures actor collision-shape directories");
    store(0x0074U, 0x00U);
    store(0x0075U, 0x84U);
    store(0x0072U, 0x00U);
    store(0x0073U, 0x85U);
    store(0x008EU, 0x00U);
    store(0x008FU, 0x86U);
    const auto collision_mask_tables = state.actor_collision_mask_table_pointers();
    check(
        collision_mask_tables
            && collision_mask_tables->first_table == 0x8400U
            && collision_mask_tables->second_table == 0x8500U
            && collision_mask_tables->masks == 0x8600U,
        "native game-state view captures actor collision-mask table roots");
    store(0x0254U, 0x01U);
    store(0x00B7U, 0xA4U);
    const auto collision_scan_session = state.actor_collision_scan_session();
    check(
        collision_scan_session && collision_scan_session->active
            && collision_scan_session->side_flags == 0xA4U,
        "native game-state view captures the actor collision scan session");
    store(0x0D13U, 0x01U);
    store(0x0D14U, 0x00U);
    store(0x0CFCU, 0x80U);
    store(0x0D32U, 0x02U);
    const auto table_rule_latches = state.table_rule_latches();
    check(
        table_rule_latches && table_rule_latches->left_front_spider_safety
            && !table_rule_latches->right_front_spider_safety
            && table_rule_latches->tower_completed
            && table_rule_latches->side_spider_transition_requested,
        "native game-state view captures typed cross-target table latches");
    check(
        !state.actor(GameStateView::kActorSlotCount),
        "native game-state view rejects slots outside the actor table");

    const GameStateView truncated(
        std::span<const std::byte>(work_ram).first(work_ram.size() - 1U));
    check(
        !truncated.valid() && !truncated.actor(kSlot)
            && !truncated.ball_substep_motion() && !truncated.ball_motion_tuning()
            && !truncated.playfield_collision_table_pointers(),
        "native game-state view rejects a truncated RAM image");
}

void test_native_small_soldier_capture()
{
    using devils_crush::native_runtime::ActorBehavior;
    using devils_crush::native_runtime::ActorState;
    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::NativeGameplayState;
    using devils_crush::native_runtime::NativeGameplayRuntime;
    using devils_crush::native_runtime::SmallSoldierGroupSide;
    using devils_crush::native_runtime::apply_native_gameplay_bootstrap;
    using devils_crush::native_runtime::capture_native_gameplay_bootstrap;
    using devils_crush::native_runtime::capture_collision_effect;
    using devils_crush::native_runtime::capture_small_soldier_collision_actor;
    using devils_crush::native_runtime::capture_small_soldier_source;
    using devils_crush::native_runtime::seed_small_soldier_collision_actor;

    std::array<std::byte, GameStateView::kWorkRamSize> work_ram{};
    const auto store = [&work_ram](
                           const std::size_t offset,
                           const std::uint8_t value) {
        work_ram[offset] = static_cast<std::byte>(value);
    };
    constexpr std::uint8_t kSlot = 0x62U;
    store(0x1000U + kSlot, 0x82U);
    store(0x1800U + kSlot, 0x90U);
    store(0x1F00U + kSlot, 0x57U);
    store(0x1F80U + kSlot, 0x7FU);
    store(0x1480U + kSlot, 0x01U);
    store(0x1C00U + kSlot, 0x01U);
    store(0x1D00U + kSlot, 0x8FU);
    store(0x1700U + kSlot, 0x21U);
    store(0x1880U + kSlot, 0x80U);
    store(0x1A00U + kSlot, 0x04U);
    store(0x1A80U + kSlot, 0x06U);
    store(0x1C80U + kSlot, 0x02U);
    store(0x1E00U + kSlot, 0x03U);
    store(0x1E80U + kSlot, 0x07U);

    constexpr std::uint8_t kUpperRightSourceSlot = 0x35U;
    store(0x1000U + kUpperRightSourceSlot, 0x82U);
    store(0x1F00U + kUpperRightSourceSlot, 0x0DU);
    store(0x1F80U + kUpperRightSourceSlot, 0x7EU);
    store(0x1480U + kUpperRightSourceSlot, 0x01U);
    store(0x1A00U + kUpperRightSourceSlot, 48U);
    store(0x1B00U + kUpperRightSourceSlot, 4U);
    store(0x1980U + kUpperRightSourceSlot, 2U);
    store(0x0D17U, 1U);

    constexpr std::uint8_t kEffectSlot = 0x64U;
    store(0x1000U + kEffectSlot, 0x8FU);
    store(0x1080U + kEffectSlot, 0xE0U);
    store(0x1F00U + kEffectSlot, 0x34U);
    store(0x1F80U + kEffectSlot, 0x7CU);
    store(0x1D80U + kEffectSlot, 4U);
    store(0x1E00U + kEffectSlot, kSlot);
    store(0x1E80U + kEffectSlot, 0x80U);

    const GameStateView state(work_ram);
    const auto captured = capture_small_soldier_collision_actor(state, kSlot);
    check(
        captured && captured->actor.index == kSlot
            && captured->rule_state.hits_remaining == 1U
            && captured->rule_state.subtype == 0x0FU
            && captured->rule_state.group_side == SmallSoldierGroupSide::upper_right
            && captured->rule_state.contact_pending,
        "native Small-Soldier capture gives overloaded actor fields their proven family meanings");

    const auto source_capture = capture_small_soldier_source(
        state, SmallSoldierGroupSide::upper_right);
    const auto effect_capture = capture_collision_effect(state, kEffectSlot);
    check(
        source_capture && source_capture->actor.index == kUpperRightSourceSlot
            && source_capture->source_state.group.pending_spawns == 4U
            && source_capture->source_state.group.live_children == 1U
            && source_capture->source_state.appearance == 2U
            && source_capture->source_state.ticks_until_next_spawn == 48U,
        "native Side-Star capture preserves the pending, live, appearance, and delay state of one generation");
    check(
        effect_capture && effect_capture->effect.source_slot == kSlot
            && effect_capture->effect.ticks_remaining == 4U,
        "native collision-effect capture identifies an active Small Soldier by its exact source slot");

    ActorState actors;
    check(
        actors.claim_dynamic_slot(kSlot, ActorBehavior::small_soldier)
            && captured
            && seed_small_soldier_collision_actor(actors, *captured),
        "native Small-Soldier capture seeds an already-owned actor");
    const auto* seeded = actors.actor(kSlot);
    check(
        seeded && seeded->small_soldier
            && *seeded->small_soldier == captured->rule_state
            && !seeded->small_soldier_group_registered
            && seeded->appearance == 0U
            && seeded->collision_effect_association == 0x8FU
            && seeded->collision_table
            && *seeded->collision_table == captured->actor.collision_table,
        "native Small-Soldier capture preserves collision state without inventing a Side-Star generation");

    store(0x1000U, 0x81U);
    const auto bootstrap = capture_native_gameplay_bootstrap(state);
    NativeGameplayState bootstrapped_gameplay;
    const bool bootstrap_applied = bootstrap
        && apply_native_gameplay_bootstrap(bootstrapped_gameplay, *bootstrap);
    constexpr std::size_t kBootstrapIndex = kSlot - ActorState::kFirstDynamicSlot;
    const auto* bootstrapped = bootstrapped_gameplay.actors().actor(kSlot);
    const auto* bootstrapped_source = bootstrapped_gameplay.actors()
        .small_soldier_source(SmallSoldierGroupSide::upper_right);
    check(
        bootstrap_applied
            && bootstrap->small_soldiers.children[kBootstrapIndex] == captured
            && bootstrap->small_soldiers.sources[1U] == source_capture
            && bootstrap->small_soldiers.collision_effects[kBootstrapIndex]
                == effect_capture
            && bootstrapped && bootstrapped->small_soldier
            && *bootstrapped->small_soldier == captured->rule_state
            && bootstrapped->small_soldier_group_registered
            && bootstrapped_source
            && *bootstrapped_source == source_capture->source_state,
        "native gameplay bootstrap reconnects a complete Side-Star source and its collision-ready child");
    const auto* bootstrapped_effect = bootstrapped_gameplay.actors().actor(kEffectSlot);
    check(
        bootstrapped_effect && bootstrapped_effect->collision_effect
            && bootstrapped_effect->collision_effect->ticks_remaining == 4U
            && bootstrapped && bootstrapped->collision_effect_active,
        "native gameplay bootstrap restores a Small Soldier's explicitly captured shared effect");
    const auto bootstrapped_source_wait =
        bootstrapped_gameplay.advance_captured_small_soldier_source(
            SmallSoldierGroupSide::upper_right);
    check(
        bootstrapped_source_wait
            && bootstrapped_source_wait->disposition
                == devils_crush::native_runtime::SmallSoldierSourceDisposition::waiting
            && bootstrapped_source_wait->state.ticks_until_next_spawn == 47U,
        "native gameplay advances a captured Side-Star countdown without inventing a dynamic slot");
    store(0x0D17U, 2U);
    store(0x1B00U + kUpperRightSourceSlot, 3U);
    const auto mismatched_bootstrap = capture_native_gameplay_bootstrap(state);
    check(
        mismatched_bootstrap
            && !mismatched_bootstrap->small_soldiers.sources[1U]
            && mismatched_bootstrap->small_soldiers.children[kBootstrapIndex]
                == captured,
        "native gameplay bootstrap leaves a child independent when its Side-Star live count cannot be fully recovered");
    store(0x0D17U, 1U);
    store(0x1B00U + kUpperRightSourceSlot, 4U);
    store(0x1000U, 0x00U);

    ActorState wrong_behavior;
    check(
        wrong_behavior.claim_dynamic_slot(kSlot, ActorBehavior::shared_motion)
            && captured
            && !seed_small_soldier_collision_actor(wrong_behavior, *captured),
        "native Small-Soldier capture rejects a destination owned by another behavior");

    store(0x1F00U + kSlot, 0x81U);
    check(
        !capture_small_soldier_collision_actor(state, kSlot),
        "native Small-Soldier capture refuses the deferred-completion handler");
    store(0x1F00U + kSlot, 0x57U);
    store(0x1480U + kSlot, 0x02U);
    check(
        !capture_small_soldier_collision_actor(state, kSlot),
        "native Small-Soldier capture rejects a non-Side-Star selector");
    store(0x1480U + kSlot, 0x01U);
    store(0x1C00U + kSlot, 0x00U);
    check(
        !capture_small_soldier_collision_actor(state, kSlot),
        "native Small-Soldier capture refuses an already-complete child");
    store(0x1C00U + kSlot, 0x01U);
    store(0x1000U + kSlot, 0x81U);
    check(
        !capture_small_soldier_collision_actor(state, kSlot),
        "native Small-Soldier capture refuses another type's overloaded fields");

    const GameStateView truncated(
        std::span<const std::byte>(work_ram).first(work_ram.size() - 1U));
    check(
        !capture_small_soldier_collision_actor(truncated, kSlot),
        "native Small-Soldier capture rejects a truncated work-RAM image");
}

void test_native_bumper_capture()
{
    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::NativeGameplayState;
    using devils_crush::native_runtime::apply_native_gameplay_bootstrap;
    using devils_crush::native_runtime::capture_bumper_actor;
    using devils_crush::native_runtime::capture_native_gameplay_bootstrap;

    std::array<std::byte, GameStateView::kWorkRamSize> work_ram{};
    const auto store = [&work_ram](
                           const std::size_t offset,
                           const std::uint8_t value) {
        work_ram[offset] = static_cast<std::byte>(value);
    };
    constexpr std::uint8_t kRightSlot = 0x15U;
    store(0x1000U + kRightSlot, 0x82U);
    store(0x1F00U + kRightSlot, 0xC7U);
    store(0x1F80U + kRightSlot, 0x78U);
    store(0x1E00U + kRightSlot, 0x01U);
    store(0x1E80U + kRightSlot, 0x08U);
    store(0x1B00U + kRightSlot, 0x06U);
    store(0x1B80U + kRightSlot, 0x02U);

    const GameStateView state(work_ram);
    const auto captured = capture_bumper_actor(state, kRightSlot);
    check(
        captured && captured->actor.index == kRightSlot
            && captured->rule_state.side == 1U
            && captured->rule_state.hit_phase == 8U
            && captured->rule_state.primary_appearance == 6U
            && captured->rule_state.secondary_damage == 2U
            && captured->rule_state.active,
        "native Bumper capture decodes the fixed target's proven steady progression");

    NativeGameplayState gameplay;
    check(
        captured && gameplay.seed_bumper(captured->rule_state)
            && gameplay.bumper(1U) != nullptr
            && *gameplay.bumper(1U) == captured->rule_state,
        "native Bumper capture seeds the fixed-target scheduler directly");

    store(0x1000U, 0x81U);
    const auto bootstrap = capture_native_gameplay_bootstrap(state);
    NativeGameplayState bootstrapped_gameplay;
    const bool bootstrap_applied = bootstrap
        && apply_native_gameplay_bootstrap(bootstrapped_gameplay, *bootstrap);
    check(
        bootstrap_applied && !bootstrap->bumpers[0U]
            && bootstrap->bumpers[1U] == captured
            && bootstrapped_gameplay.bumper(1U)
            && *bootstrapped_gameplay.bumper(1U) == captured->rule_state,
        "native gameplay bootstrap restores each clear Bumper independently");
    store(0x1000U, 0x00U);

    store(0x1E00U + kRightSlot, 0x00U);
    check(
        !capture_bumper_actor(state, kRightSlot),
        "native Bumper capture rejects a mismatched fixed-slot side");
    store(0x1E00U + kRightSlot, 0x01U);
    store(0x1B80U + kRightSlot, 0x01U);
    check(
        !capture_bumper_actor(state, kRightSlot),
        "native Bumper capture rejects inconsistent phase counters");
    store(0x1B80U + kRightSlot, 0x02U);
    store(0x1F00U + kRightSlot, 0x2BU);
    store(0x1F80U + kRightSlot, 0x79U);
    check(
        !capture_bumper_actor(state, kRightSlot),
        "native Bumper capture excludes terminal allocation ownership");
    store(0x1F00U + kRightSlot, 0xC7U);
    store(0x1F80U + kRightSlot, 0x78U);
    check(
        !capture_bumper_actor(state, 0x16U),
        "native Bumper capture rejects an adjacent non-Bumper slot");

    const GameStateView truncated(
        std::span<const std::byte>(work_ram).first(work_ram.size() - 1U));
    check(
        !capture_bumper_actor(truncated, kRightSlot),
        "native Bumper capture rejects a truncated work-RAM image");
}

void test_native_front_spider_capture()
{
    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::NativeGameplayState;
    using devils_crush::native_runtime::apply_native_gameplay_bootstrap;
    using devils_crush::native_runtime::capture_front_spider_actor;
    using devils_crush::native_runtime::capture_native_gameplay_bootstrap;

    std::array<std::byte, GameStateView::kWorkRamSize> work_ram{};
    const auto store = [&work_ram](
                           const std::size_t offset,
                           const std::uint8_t value) {
        work_ram[offset] = static_cast<std::byte>(value);
    };
    constexpr std::uint8_t kLeftSlot = 0x26U;
    store(0x1000U + kLeftSlot, 0x83U);
    store(0x1F00U + kLeftSlot, 0x90U);
    store(0x1F80U + kLeftSlot, 0x64U);
    store(0x1C80U + kLeftSlot, 0x00U);

    const GameStateView state(work_ram);
    const auto captured = capture_front_spider_actor(state, kLeftSlot);
    check(
        captured && captured->actor.index == kLeftSlot
            && captured->rule_state.side == 0U
            && captured->rule_state.collision_ready,
        "native Front-Spider capture decodes the fixed steady target");

    NativeGameplayState gameplay;
    check(
        captured && gameplay.seed_front_spider(captured->rule_state)
            && gameplay.front_spider(0U) != nullptr
            && *gameplay.front_spider(0U) == captured->rule_state,
        "native Front-Spider capture seeds the fixed-target scheduler directly");

    store(0x1000U, 0x81U);
    const auto bootstrap = capture_native_gameplay_bootstrap(state);
    NativeGameplayState bootstrapped_gameplay;
    const bool bootstrap_applied = bootstrap
        && apply_native_gameplay_bootstrap(bootstrapped_gameplay, *bootstrap);
    check(
        bootstrap_applied && bootstrap->front_spiders[0U] == captured
            && !bootstrap->front_spiders[1U]
            && bootstrapped_gameplay.front_spider(0U)
            && *bootstrapped_gameplay.front_spider(0U) == captured->rule_state,
        "native gameplay bootstrap restores each clear Front Spider independently");
    store(0x1000U, 0x00U);

    store(0x1C80U + kLeftSlot, 0x01U);
    check(
        !capture_front_spider_actor(state, kLeftSlot),
        "native Front-Spider capture rejects a mismatched fixed-slot side");
    store(0x1C80U + kLeftSlot, 0x00U);
    store(0x1F00U + kLeftSlot, 0xD2U);
    check(
        !capture_front_spider_actor(state, kLeftSlot),
        "native Front-Spider capture excludes appearance-transition ownership");
    store(0x1F00U + kLeftSlot, 0x90U);
    check(
        !capture_front_spider_actor(state, 0x28U),
        "native Front-Spider capture rejects the adjacent Side Spider slot");

    const GameStateView truncated(
        std::span<const std::byte>(work_ram).first(work_ram.size() - 1U));
    check(
        !capture_front_spider_actor(truncated, kLeftSlot),
        "native Front-Spider capture rejects a truncated work-RAM image");
}

void test_native_side_spider_capture()
{
    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::NativeGameplayState;
    using devils_crush::native_runtime::apply_native_gameplay_bootstrap;
    using devils_crush::native_runtime::capture_side_spider_actor;
    using devils_crush::native_runtime::capture_native_gameplay_bootstrap;

    std::array<std::byte, GameStateView::kWorkRamSize> work_ram{};
    const auto store = [&work_ram](
                           const std::size_t offset,
                           const std::uint8_t value) {
        work_ram[offset] = static_cast<std::byte>(value);
    };
    constexpr std::uint8_t kRightSlot = 0x29U;
    store(0x1000U + kRightSlot, 0x83U);
    store(0x1F00U + kRightSlot, 0x1FU);
    store(0x1F80U + kRightSlot, 0x65U);
    store(0x1C80U + kRightSlot, 0x01U);

    const GameStateView state(work_ram);
    const auto captured = capture_side_spider_actor(state, kRightSlot);
    check(
        captured && captured->actor.index == kRightSlot
            && captured->rule_state.side == 1U
            && captured->rule_state.collision_ready,
        "native Side-Spider capture decodes the fixed steady target");

    NativeGameplayState gameplay;
    check(
        captured && gameplay.seed_side_spider(captured->rule_state)
            && gameplay.side_spider(1U) != nullptr
            && *gameplay.side_spider(1U) == captured->rule_state,
        "native Side-Spider capture seeds the fixed-target scheduler directly");

    store(0x1000U, 0x81U);
    const auto bootstrap = capture_native_gameplay_bootstrap(state);
    NativeGameplayState bootstrapped_gameplay;
    const bool bootstrap_applied = bootstrap
        && apply_native_gameplay_bootstrap(bootstrapped_gameplay, *bootstrap);
    check(
        bootstrap_applied && !bootstrap->side_spiders[0U]
            && bootstrap->side_spiders[1U] == captured
            && bootstrapped_gameplay.side_spider(1U)
            && *bootstrapped_gameplay.side_spider(1U) == captured->rule_state,
        "native gameplay bootstrap restores each clear Side Spider independently");
    store(0x1000U, 0x00U);

    store(0x1C80U + kRightSlot, 0x00U);
    check(
        !capture_side_spider_actor(state, kRightSlot),
        "native Side-Spider capture rejects a mismatched fixed-slot side");
    store(0x1C80U + kRightSlot, 0x01U);
    store(0x1F00U + kRightSlot, 0x39U);
    store(0x1F80U + kRightSlot, 0x65U);
    check(
        !capture_side_spider_actor(state, kRightSlot),
        "native Side-Spider capture excludes appearance-transition ownership");
    store(0x1F00U + kRightSlot, 0x1FU);
    check(
        !capture_side_spider_actor(state, 0x27U),
        "native Side-Spider capture rejects an adjacent Front Spider slot");

    const GameStateView truncated(
        std::span<const std::byte>(work_ram).first(work_ram.size() - 1U));
    check(
        !capture_side_spider_actor(truncated, kRightSlot),
        "native Side-Spider capture rejects a truncated work-RAM image");
}

void test_native_skeleton_insect_capture()
{
    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::NativeGameplayState;
    using devils_crush::native_runtime::SkeletonInsectKind;
    using devils_crush::native_runtime::SkeletonInsectTriggerDisposition;
    using devils_crush::native_runtime::apply_native_gameplay_bootstrap;
    using devils_crush::native_runtime::capture_skeleton_insect_actor;
    using devils_crush::native_runtime::capture_native_gameplay_bootstrap;

    std::array<std::byte, GameStateView::kWorkRamSize> work_ram{};
    const auto store = [&work_ram](
                           const std::size_t offset,
                           const std::uint8_t value) {
        work_ram[offset] = static_cast<std::byte>(value);
    };
    constexpr std::uint8_t kCenterSlot = 0x32U;
    store(0x1000U + kCenterSlot, 0x81U);
    store(0x1F00U + kCenterSlot, 0xF9U);
    store(0x1F80U + kCenterSlot, 0x55U);
    store(0x0CFCU, 0x01U);

    const GameStateView state(work_ram);
    const auto captured = capture_skeleton_insect_actor(state, kCenterSlot);
    const auto latches = state.table_rule_latches();
    check(
        captured && captured->actor.index == kCenterSlot
            && captured->rule_state.kind == SkeletonInsectKind::center
            && latches && latches->tower_completed,
        "native Skeleton-Insect capture pairs a steady actor with its typed table source");

    NativeGameplayState gameplay;
    if (latches) {
        gameplay.seed_table_rule_latches(*latches);
    }
    const auto updates = captured && gameplay.seed_skeleton_insect(captured->rule_state)
        ? gameplay.advance_actor_phase()
        : std::vector<NativeGameplayState::ActorPhaseUpdate>{};
    check(
        captured && gameplay.skeleton_insect(SkeletonInsectKind::center)
            && updates.size() == 1U
            && updates[0].kind
                == NativeGameplayState::ActorPhaseUpdateKind::skeleton_insect_trigger
            && updates[0].skeleton_insect
            && updates[0].skeleton_insect->insect.disposition
                == SkeletonInsectTriggerDisposition::triggered
            && gameplay.score().stored_units == 100U
            && gameplay.advance_actor_phase().empty()
            && !gameplay.seed_skeleton_insect({
                .kind = static_cast<SkeletonInsectKind>(3U)})
            && gameplay.skeleton_insect(static_cast<SkeletonInsectKind>(3U))
                == nullptr,
        "native Skeleton-Insect capture preserves a pending Tower source for one actor-order trigger");

    store(0x1000U, 0x81U);
    const auto bootstrap = capture_native_gameplay_bootstrap(state);
    NativeGameplayState bootstrapped_gameplay;
    const bool bootstrap_applied = bootstrap
        && apply_native_gameplay_bootstrap(bootstrapped_gameplay, *bootstrap);
    check(
        bootstrap_applied && !bootstrap->skeleton_insects[0U]
            && bootstrap->skeleton_insects[1U] == captured
            && !bootstrap->skeleton_insects[2U]
            && bootstrapped_gameplay.skeleton_insect(SkeletonInsectKind::center)
            && *bootstrapped_gameplay.skeleton_insect(SkeletonInsectKind::center)
                == captured->rule_state,
        "native gameplay bootstrap restores each clear Skeleton Insect independently");
    store(0x1000U, 0x00U);

    store(0x1F00U + kCenterSlot, 0xFAU);
    check(
        !capture_skeleton_insect_actor(state, kCenterSlot),
        "native Skeleton-Insect capture excludes transition ownership");
    store(0x1F00U + kCenterSlot, 0xF9U);
    store(0x1000U + kCenterSlot, 0x82U);
    check(
        !capture_skeleton_insect_actor(state, kCenterSlot),
        "native Skeleton-Insect capture rejects a mismatched fixed actor type");
    store(0x1000U + kCenterSlot, 0x81U);
    check(
        !capture_skeleton_insect_actor(state, 0x30U),
        "native Skeleton-Insect capture rejects a non-Skeleton slot");

    const GameStateView truncated(
        std::span<const std::byte>(work_ram).first(work_ram.size() - 1U));
    check(
        !capture_skeleton_insect_actor(truncated, kCenterSlot)
            && !truncated.table_rule_latches(),
        "native Skeleton-Insect capture rejects a truncated work-RAM image");
}

void test_native_dragon_egg_cluster_capture()
{
    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::NativeGameplayState;
    using devils_crush::native_runtime::apply_native_gameplay_bootstrap;
    using devils_crush::native_runtime::capture_dragon_egg_cluster;
    using devils_crush::native_runtime::capture_native_gameplay_bootstrap;

    std::array<std::byte, GameStateView::kWorkRamSize> work_ram{};
    const auto store = [&work_ram](
                           const std::size_t offset,
                           const std::uint8_t value) {
        work_ram[offset] = static_cast<std::byte>(value);
    };
    for (std::uint8_t slot = 0x20U; slot <= 0x25U; ++slot) {
        store(0x1000U + slot, 0x82U);
        store(0x1800U + slot, 0x00U);
        store(0x1F00U + slot, 0x45U);
        store(0x1F80U + slot, 0x43U);
    }
    store(0x0CF4U, 6U);
    store(0x0CF7U, 0x00U);
    store(0x0D20U, 0x00U);

    const GameStateView state(work_ram);
    const auto captured = capture_dragon_egg_cluster(state);
    check(
        captured && captured->actors[0U].index == 0x20U
            && captured->actors[5U].index == 0x25U
            && captured->rule_state
                == devils_crush::native_runtime::DragonEggClusterState{},
        "native Dragon's-Egg capture accepts a complete six-target steady cluster");

    store(0x0CF4U, 5U);
    check(
        !capture_dragon_egg_cluster(state),
        "native Dragon's-Egg capture requires its full live-count invariant");
    store(0x0CF4U, 6U);

    NativeGameplayState gameplay;
    if (captured) {
        gameplay.seed_dragon_egg_cluster(captured->rule_state);
    }
    check(
        captured && gameplay.dragon_egg_cluster()
            && *gameplay.dragon_egg_cluster() == captured->rule_state,
        "native Dragon's-Egg capture seeds the fixed-target cluster directly");

    store(0x1000U, 0x81U);
    const auto bootstrap = capture_native_gameplay_bootstrap(state);
    NativeGameplayState bootstrapped_gameplay;
    const bool bootstrap_applied = bootstrap
        && apply_native_gameplay_bootstrap(bootstrapped_gameplay, *bootstrap);
    check(
        bootstrap_applied && bootstrap->dragon_eggs
            && !bootstrap->dragon_eggs->hatchling
            && bootstrapped_gameplay.dragon_egg_cluster()
            && *bootstrapped_gameplay.dragon_egg_cluster()
                == captured->rule_state,
        "native gameplay bootstrap transfers a complete Dragon's-Egg cluster");
    store(0x1000U, 0x00U);

    store(0x1800U + 0x22U, 0x80U);
    check(
        !capture_dragon_egg_cluster(state),
        "native Dragon's-Egg capture leaves a pending hit with its collision owner");
    store(0x1800U + 0x22U, 0x00U);
    store(0x1F00U + 0x22U, 0x87U);
    check(
        !capture_dragon_egg_cluster(state),
        "native Dragon's-Egg capture excludes a destroyed egg dispatcher");
    store(0x1F00U + 0x22U, 0x45U);

    const GameStateView truncated(
        std::span<const std::byte>(work_ram).first(work_ram.size() - 1U));
    check(
        !capture_dragon_egg_cluster(truncated),
        "native Dragon's-Egg capture rejects a truncated work-RAM image");
}

void test_native_partial_dragon_egg_cluster_capture()
{
    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::NativeGameplayState;
    using devils_crush::native_runtime::capture_partial_dragon_egg_cluster;

    std::array<std::byte, GameStateView::kWorkRamSize> work_ram{};
    const auto store = [&work_ram](
                           const std::size_t offset,
                           const std::uint8_t value) {
        work_ram[offset] = static_cast<std::byte>(value);
    };
    for (std::uint8_t slot = 0x20U; slot <= 0x25U; ++slot) {
        store(0x1000U + slot, 0x82U);
        store(0x1F00U + slot, 0x45U);
        store(0x1F80U + slot, 0x43U);
    }
    // Slot $22 completed the source's post-hatchling disable transition.
    store(0x1F00U + 0x22U, 0x87U);
    store(0x1F80U + 0x22U, 0x43U);
    store(0x0CF4U, 0x05U);
    store(0x0CF7U, 0x00U);

    const GameStateView state(work_ram);
    const auto captured = capture_partial_dragon_egg_cluster(state);
    check(
        captured && captured->rule_state.eggs[0U].active
            && captured->rule_state.eggs[1U].active
            && !captured->rule_state.eggs[2U].active
            && captured->rule_state.eggs[3U].active
            && !captured->rule_state.completed
            && !captured->rule_state.completion_pending,
        "native Dragon's-Egg capture accepts a stable partial cluster after its hatchling releases");

    NativeGameplayState gameplay;
    if (captured) {
        gameplay.seed_dragon_egg_cluster(captured->rule_state);
    }
    check(
        captured && gameplay.dragon_egg_cluster()
            && *gameplay.dragon_egg_cluster() == captured->rule_state,
        "native partial Dragon's-Egg capture seeds the fixed-target scheduler directly");

    store(0x0CF4U, 0x04U);
    check(
        !capture_partial_dragon_egg_cluster(state),
        "native partial Dragon's-Egg capture requires the source live count to match fixed records");
    store(0x0CF4U, 0x05U);
    store(0x0CF7U, 0x01U);
    check(
        !capture_partial_dragon_egg_cluster(state),
        "native partial Dragon's-Egg capture excludes the reset path");
    store(0x0CF7U, 0x00U);
    store(0x1000U + 0x64U, 0x82U);
    store(0x1F00U + 0x64U, 0x1EU);
    store(0x1F80U + 0x64U, 0x44U);
    check(
        !capture_partial_dragon_egg_cluster(state),
        "native partial Dragon's-Egg capture leaves live hatchlings with their source owner");
    store(0x1000U + 0x64U, 0x00U);
    store(0x1F00U + 0x22U, 0x45U);
    check(
        !capture_partial_dragon_egg_cluster(state),
        "native partial Dragon's-Egg capture requires at least one completed disable transition");

    const GameStateView truncated(
        std::span<const std::byte>(work_ram).first(work_ram.size() - 1U));
    check(
        !capture_partial_dragon_egg_cluster(truncated),
        "native partial Dragon's-Egg capture rejects a truncated work-RAM image");
}

void test_native_dragon_egg_hatchling_capture()
{
    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::NativeGameplayState;
    using devils_crush::native_runtime::capture_dragon_egg_cluster_with_hatchling;

    std::array<std::byte, GameStateView::kWorkRamSize> work_ram{};
    const auto store = [&work_ram](
                           const std::size_t offset,
                           const std::uint8_t value) {
        work_ram[offset] = static_cast<std::byte>(value);
    };
    for (std::uint8_t slot = 0x20U; slot <= 0x25U; ++slot) {
        store(0x1000U + slot, 0x82U);
        store(0x1F00U + slot, 0x45U);
        store(0x1F80U + slot, 0x43U);
    }
    store(0x1F00U + 0x22U, 0x87U);
    store(0x1F80U + 0x22U, 0x43U);
    store(0x0CF4U, 0x05U);
    store(0x1000U + 0x64U, 0x82U);
    store(0x1F00U + 0x64U, 0x1EU);
    store(0x1F80U + 0x64U, 0x44U);
    store(0x1180U + 0x64U, 68U);
    store(0x1300U + 0x64U, 44U);
    store(0x1380U + 0x64U, 2U);
    store(0x1A00U + 0x64U, 2U);

    const GameStateView state(work_ram);
    const auto captured = capture_dragon_egg_cluster_with_hatchling(state);
    check(
        captured && !captured->cluster.rule_state.eggs[2U].active
            && captured->hatchling.source_egg_slot == 0x22U
            && captured->hatchling.dynamic_slot == 0x64U
            && captured->hatchling.position.horizontal_pixels == 68
            && captured->hatchling.position.vertical_pixels == 556
            && captured->hatchling.animation_timer == 2U,
        "native Dragon's-Egg capture restores the sole in-flight hatchling with its disabled parent");

    NativeGameplayState gameplay;
    if (captured) {
        gameplay.seed_dragon_egg_cluster(captured->cluster.rule_state);
        check(
            gameplay.seed_dragon_egg_hatchling(captured->hatchling),
            "native Dragon's-Egg capture claims its existing dynamic hatchling without replaying allocation");
    }
    const auto phase = gameplay.advance_actor_phase();
    check(
        captured && phase.size() == 2U && phase[1].slot == 0x64U
            && phase[1].dragon_egg_hatchling
            && phase[1].dragon_egg_hatchling->hatchling.state.animation_timer == 1U,
        "native Dragon's-Egg capture resumes the hatchling on its next actor turn");

    store(0x1000U + 0x65U, 0x82U);
    store(0x1F00U + 0x65U, 0x1EU);
    store(0x1F80U + 0x65U, 0x44U);
    check(
        !capture_dragon_egg_cluster_with_hatchling(state),
        "native Dragon's-Egg hatchling capture rejects ambiguous multiple children");
    store(0x1000U + 0x65U, 0x00U);
    store(0x0CF4U, 0x04U);
    check(
        !capture_dragon_egg_cluster_with_hatchling(state),
        "native Dragon's-Egg hatchling capture requires exactly one disabled parent");
    store(0x0CF4U, 0x05U);
    store(0x1F00U + 0x22U, 0x45U);
    check(
        !capture_dragon_egg_cluster_with_hatchling(state),
        "native Dragon's-Egg hatchling capture requires its parent disable transition");

    const GameStateView truncated(
        std::span<const std::byte>(work_ram).first(work_ram.size() - 1U));
    check(
        !capture_dragon_egg_cluster_with_hatchling(truncated),
        "native Dragon's-Egg hatchling capture rejects a truncated work-RAM image");
}

void test_native_completed_dragon_egg_cluster_capture()
{
    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::NativeGameplayState;
    using devils_crush::native_runtime::capture_completed_dragon_egg_cluster;

    std::array<std::byte, GameStateView::kWorkRamSize> work_ram{};
    const auto store = [&work_ram](
                           const std::size_t offset,
                           const std::uint8_t value) {
        work_ram[offset] = static_cast<std::byte>(value);
    };
    store(0x1000U + 0x1FU, 0x80U);
    store(0x1F00U + 0x1FU, 0xF6U);
    store(0x1F80U + 0x1FU, 0x42U);
    for (std::uint8_t slot = 0x20U; slot <= 0x25U; ++slot) {
        store(0x1000U + slot, 0x82U);
        store(0x1F00U + slot, 0x87U);
        store(0x1F80U + slot, 0x43U);
    }
    store(0x0CF4U, 0x00U);
    store(0x0CF7U, 0x00U);
    store(0x0D20U, 0x01U);

    const GameStateView state(work_ram);
    const auto captured = capture_completed_dragon_egg_cluster(state);
    check(
        captured && captured->rule_state.completed
            && !captured->rule_state.completion_pending
            && !captured->rule_state.reset_pending
            && std::ranges::none_of(captured->rule_state.eggs, [](const auto& egg) {
                   return egg.active || egg.hatchling_spawn_pending;
               }),
        "native Dragon's-Egg capture accepts the completed pre-reset cluster");

    NativeGameplayState gameplay;
    if (captured) {
        gameplay.seed_dragon_egg_cluster(captured->rule_state);
    }
    const auto phase = gameplay.advance_actor_phase();
    check(
        captured && phase.size() == 1U && phase[0].slot == 0x1FU
            && phase[0].kind
                == NativeGameplayState::ActorPhaseUpdateKind::dragon_egg_controller
            && phase[0].dragon_egg_controller
            && phase[0].dragon_egg_controller->controller.disposition
                == devils_crush::native_runtime::DragonEggCompletionDisposition::already_completed,
        "native completed Dragon's-Egg capture preserves the already-awarded controller state");

    store(0x0CF7U, 0x01U);
    check(
        !capture_completed_dragon_egg_cluster(state),
        "native completed Dragon's-Egg capture excludes the reset transition");
    store(0x0CF7U, 0x00U);
    store(0x0D20U, 0x00U);
    check(
        !capture_completed_dragon_egg_cluster(state),
        "native completed Dragon's-Egg capture requires the completion latch");
    store(0x0D20U, 0x01U);
    store(0x1000U + 0x64U, 0x82U);
    store(0x1F00U + 0x64U, 0x1EU);
    store(0x1F80U + 0x64U, 0x44U);
    check(
        !capture_completed_dragon_egg_cluster(state),
        "native completed Dragon's-Egg capture leaves live hatchlings with their source owner");
    store(0x1000U + 0x64U, 0x00U);
    store(0x1F00U + 0x1FU, 0xEBU);
    check(
        !capture_completed_dragon_egg_cluster(state),
        "native completed Dragon's-Egg capture requires the steady controller dispatch");
    store(0x1F00U + 0x1FU, 0xF6U);
    store(0x1F00U + 0x22U, 0x45U);
    check(
        !capture_completed_dragon_egg_cluster(state),
        "native completed Dragon's-Egg capture requires every fixed egg to be disabled");

    const GameStateView truncated(
        std::span<const std::byte>(work_ram).first(work_ram.size() - 1U));
    check(
        !capture_completed_dragon_egg_cluster(truncated),
        "native completed Dragon's-Egg capture rejects a truncated work-RAM image");
}

void test_native_dragon_egg_reset_cluster_capture()
{
    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::NativeGameplayState;
    using devils_crush::native_runtime::capture_dragon_egg_reset_cluster;

    std::array<std::byte, GameStateView::kWorkRamSize> work_ram{};
    const auto store = [&work_ram](
                           const std::size_t offset,
                           const std::uint8_t value) {
        work_ram[offset] = static_cast<std::byte>(value);
    };
    store(0x1000U + 0x1FU, 0x80U);
    store(0x1F00U + 0x1FU, 0xF6U);
    store(0x1F80U + 0x1FU, 0x42U);
    for (std::uint8_t slot = 0x20U; slot <= 0x25U; ++slot) {
        store(0x1000U + slot, 0x82U);
        store(0x1F00U + slot, 0x87U);
        store(0x1F80U + slot, 0x43U);
    }
    // Slots $20 and $21 already reinitialized in the same reset phase.
    constexpr std::array<std::uint8_t, 2U> kReinitializedSlots{0x20U, 0x21U};
    for (const auto slot : kReinitializedSlots) {
        store(0x1F00U + slot, 0x45U);
    }
    store(0x0CF4U, 0x02U);
    store(0x0CF7U, 0x01U);
    store(0x0D20U, 0x01U);

    const GameStateView state(work_ram);
    const auto captured = capture_dragon_egg_reset_cluster(state);
    check(
        captured && captured->rule_state.completed
            && captured->rule_state.reset_pending
            && captured->rule_state.eggs[0U].active
            && captured->rule_state.eggs[1U].active
            && !captured->rule_state.eggs[2U].active,
        "native Dragon's-Egg capture accepts a stable in-progress per-slot reset");

    NativeGameplayState gameplay;
    if (captured) {
        gameplay.seed_dragon_egg_cluster(captured->rule_state);
    }
    const auto phase = gameplay.advance_actor_phase();
    check(
        captured && phase.size() == 5U && phase[0].slot == 0x1FU
            && phase[0].kind
                == NativeGameplayState::ActorPhaseUpdateKind::dragon_egg_controller
            && phase[1].slot == 0x22U
            && phase[1].kind
                == NativeGameplayState::ActorPhaseUpdateKind::dragon_egg_fixed_reset
            && phase.back().slot == 0x25U
            && phase.back().dragon_egg_fixed_reset
            && phase.back().dragon_egg_fixed_reset->disposition
                == devils_crush::native_runtime::DragonEggFixedResetDisposition::cluster_reinitialized,
        "native Dragon's-Egg reset capture resumes remaining fixed targets in actor order");

    store(0x0CF4U, 0x03U);
    check(
        !capture_dragon_egg_reset_cluster(state),
        "native Dragon's-Egg reset capture requires the source live count to match fixed records");
    store(0x0CF4U, 0x02U);
    store(0x0CF7U, 0x00U);
    check(
        !capture_dragon_egg_reset_cluster(state),
        "native Dragon's-Egg reset capture requires the reset signal");
    store(0x0CF7U, 0x01U);
    store(0x0D20U, 0x00U);
    check(
        !capture_dragon_egg_reset_cluster(state),
        "native Dragon's-Egg reset capture requires prior completion");
    store(0x0D20U, 0x01U);
    store(0x1000U + 0x64U, 0x82U);
    store(0x1F00U + 0x64U, 0x1EU);
    store(0x1F80U + 0x64U, 0x44U);
    check(
        !capture_dragon_egg_reset_cluster(state),
        "native Dragon's-Egg reset capture leaves live hatchlings with their source owner");
    store(0x1000U + 0x64U, 0x00U);
    store(0x1F00U + 0x23U, 0x1EU);
    check(
        !capture_dragon_egg_reset_cluster(state),
        "native Dragon's-Egg reset capture rejects an unknown fixed dispatch");

    const GameStateView truncated(
        std::span<const std::byte>(work_ram).first(work_ram.size() - 1U));
    check(
        !capture_dragon_egg_reset_cluster(truncated),
        "native Dragon's-Egg reset capture rejects a truncated work-RAM image");
}

void test_native_dragon_head_capture()
{
    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::NativeGameplayState;
    using devils_crush::native_runtime::apply_native_gameplay_bootstrap;
    using devils_crush::native_runtime::capture_dragon_head_actor;
    using devils_crush::native_runtime::capture_native_gameplay_bootstrap;
    using devils_crush::native_runtime::kDragonHeadSlot;

    std::array<std::byte, GameStateView::kWorkRamSize> work_ram{};
    const auto store = [&work_ram](
                           const std::size_t offset,
                           const std::uint8_t value) {
        work_ram[offset] = static_cast<std::byte>(value);
    };
    store(0x1000U + kDragonHeadSlot, 0x82U);
    store(0x1800U + kDragonHeadSlot, 0x00U);
    store(0x1C00U + kDragonHeadSlot, 0x06U);
    store(0x1F00U + kDragonHeadSlot, 0x4BU);
    store(0x1F80U + kDragonHeadSlot, 0x5AU);

    const GameStateView state(work_ram);
    const auto captured = capture_dragon_head_actor(state, kDragonHeadSlot);
    check(
        captured && captured->actor.index == kDragonHeadSlot
            && captured->rule_state.hits_remaining == 6U
            && captured->rule_state.collision_ready
            && !captured->rule_state.mouth_opening_transition_pending,
        "native Dragon's Head capture decodes the fixed steady hit counter");

    NativeGameplayState gameplay;
    if (captured) {
        gameplay.seed_dragon_head(captured->rule_state);
    }
    check(
        captured && gameplay.dragon_head() == captured->rule_state,
        "native Dragon's Head capture seeds the fixed-target scheduler directly");

    store(0x1000U, 0x81U);
    const auto bootstrap = capture_native_gameplay_bootstrap(state);
    NativeGameplayState bootstrapped_gameplay;
    const bool bootstrap_applied = bootstrap
        && apply_native_gameplay_bootstrap(bootstrapped_gameplay, *bootstrap);
    check(
        bootstrap_applied && bootstrap->dragon_head == captured
            && bootstrapped_gameplay.dragon_head() == captured->rule_state,
        "native gameplay bootstrap restores a clear Dragon's Head steady state");
    store(0x1000U, 0x00U);

    store(0x1800U + kDragonHeadSlot, 0x80U);
    check(
        !capture_dragon_head_actor(state, kDragonHeadSlot),
        "native Dragon's Head capture leaves a pending hit with its collision owner");
    store(0x1800U + kDragonHeadSlot, 0x00U);
    store(0x1C00U + kDragonHeadSlot, 0x00U);
    check(
        !capture_dragon_head_actor(state, kDragonHeadSlot),
        "native Dragon's Head capture excludes its terminal opening state");
    store(0x1C00U + kDragonHeadSlot, 0x06U);
    store(0x1F00U + kDragonHeadSlot, 0x74U);
    check(
        !capture_dragon_head_actor(state, kDragonHeadSlot),
        "native Dragon's Head capture excludes non-steady handler ownership");
    store(0x1F00U + kDragonHeadSlot, 0x4BU);
    check(
        !capture_dragon_head_actor(state, 0x14U),
        "native Dragon's Head capture rejects the adjacent Bumper slot");

    const GameStateView truncated(
        std::span<const std::byte>(work_ram).first(work_ram.size() - 1U));
    check(
        !capture_dragon_head_actor(truncated, kDragonHeadSlot),
        "native Dragon's Head capture rejects a truncated work-RAM image");
}

void test_native_giant_skeleton_capture()
{
    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::NativeGameplayState;
    using devils_crush::native_runtime::apply_native_gameplay_bootstrap;
    using devils_crush::native_runtime::capture_giant_skeleton_actor;
    using devils_crush::native_runtime::capture_native_gameplay_bootstrap;
    using devils_crush::native_runtime::kGiantSkeletonSlot;

    std::array<std::byte, GameStateView::kWorkRamSize> work_ram{};
    const auto store = [&work_ram](
                           const std::size_t offset,
                           const std::uint8_t value) {
        work_ram[offset] = static_cast<std::byte>(value);
    };
    store(0x1000U + kGiantSkeletonSlot, 0x83U);
    store(0x1800U + kGiantSkeletonSlot, 0x00U);
    store(0x1980U + kGiantSkeletonSlot, 0x05U);
    store(0x1F00U + kGiantSkeletonSlot, 0xECU);
    store(0x1F80U + kGiantSkeletonSlot, 0x5EU);

    const GameStateView state(work_ram);
    const auto captured = capture_giant_skeleton_actor(state, kGiantSkeletonSlot);
    check(
        captured && captured->actor.index == kGiantSkeletonSlot
            && captured->rule_state.appearance == 5U,
        "native Giant Skeleton capture decodes its fixed live direction appearance");

    NativeGameplayState gameplay;
    if (captured) {
        gameplay.seed_giant_skeleton(captured->rule_state);
    }
    check(
        captured && gameplay.giant_skeleton()
            && *gameplay.giant_skeleton() == captured->rule_state,
        "native Giant Skeleton capture seeds the fixed-target scheduler directly");

    store(0x1000U, 0x81U);
    const auto bootstrap = capture_native_gameplay_bootstrap(state);
    NativeGameplayState bootstrapped_gameplay;
    const bool bootstrap_applied = bootstrap
        && apply_native_gameplay_bootstrap(bootstrapped_gameplay, *bootstrap);
    check(
        bootstrap_applied && bootstrap->giant_skeleton
            && bootstrap->giant_skeleton->skeleton == captured
            && !bootstrap->giant_skeleton->eyeball
            && bootstrapped_gameplay.giant_skeleton()
            && *bootstrapped_gameplay.giant_skeleton() == captured->rule_state,
        "native gameplay bootstrap restores a clear Giant Skeleton steady state");
    store(0x1000U, 0x00U);

    store(0x1800U + kGiantSkeletonSlot, 0x80U);
    check(
        !capture_giant_skeleton_actor(state, kGiantSkeletonSlot),
        "native Giant Skeleton capture leaves a pending hit with its collision owner");
    store(0x1800U + kGiantSkeletonSlot, 0x00U);
    store(0x1980U + kGiantSkeletonSlot, 0x01U);
    check(
        !capture_giant_skeleton_actor(state, kGiantSkeletonSlot),
        "native Giant Skeleton capture rejects a non-live appearance phase");
    store(0x1980U + kGiantSkeletonSlot, 0x05U);
    store(0x1F00U + kGiantSkeletonSlot, 0xE4U);
    check(
        !capture_giant_skeleton_actor(state, kGiantSkeletonSlot),
        "native Giant Skeleton capture excludes setup-handler ownership");
    store(0x1F00U + kGiantSkeletonSlot, 0xECU);
    check(
        !capture_giant_skeleton_actor(state, 0x31U),
        "native Giant Skeleton capture rejects an adjacent Skeleton-Insect slot");

    const GameStateView truncated(
        std::span<const std::byte>(work_ram).first(work_ram.size() - 1U));
    check(
        !capture_giant_skeleton_actor(truncated, kGiantSkeletonSlot),
        "native Giant Skeleton capture rejects a truncated work-RAM image");
}

void test_native_giant_face_capture()
{
    using devils_crush::native_runtime::BallMotionTuning;
    using devils_crush::native_runtime::CollisionMap;
    using devils_crush::native_runtime::CollisionMask;
    using devils_crush::native_runtime::Fixed24_8;
    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::NativeGameplayState;
    using devils_crush::native_runtime::NativeGameplayRuntime;
    using devils_crush::native_runtime::apply_native_gameplay_bootstrap;
    using devils_crush::native_runtime::capture_awake_lower_giant_face;
    using devils_crush::native_runtime::capture_awake_upper_giant_face;
    using devils_crush::native_runtime::capture_dormant_giant_face;
    using devils_crush::native_runtime::capture_giant_face_helmet;
    using devils_crush::native_runtime::capture_native_gameplay_bootstrap;
    using devils_crush::native_runtime::kGiantFaceAnimatorSlot;
    using devils_crush::native_runtime::kGiantFaceControllerSlot;
    using devils_crush::native_runtime::kGiantFaceCrownSlot;
    using devils_crush::native_runtime::kGiantFaceMouthSlot;
    using devils_crush::native_runtime::kGiantFaceLeftHelmetSlot;
    using devils_crush::native_runtime::kGiantFaceRightHelmetSlot;

    std::array<std::byte, GameStateView::kWorkRamSize> work_ram{};
    const auto store = [&work_ram](
                           const std::size_t offset,
                           const std::uint8_t value) {
        work_ram[offset] = static_cast<std::byte>(value);
    };
    const auto seed_actor = [&store](
                                const std::uint8_t slot,
                                const std::uint8_t type,
                                const std::uint16_t handler) {
        store(0x1000U + slot, type);
        store(0x1800U + slot, 0x00U);
        store(0x1F00U + slot, static_cast<std::uint8_t>(handler & 0x00FFU));
        store(0x1F80U + slot, static_cast<std::uint8_t>(handler >> 8U));
    };
    seed_actor(kGiantFaceControllerSlot, 0x83U, 0x74D8U);
    seed_actor(kGiantFaceAnimatorSlot, 0x83U, 0x767EU);
    seed_actor(kGiantFaceMouthSlot, 0x82U, 0x7726U);
    seed_actor(kGiantFaceCrownSlot, 0x82U, 0x76FAU);
    seed_actor(kGiantFaceLeftHelmetSlot, 0x84U, 0x4C91U);
    seed_actor(kGiantFaceRightHelmetSlot, 0x84U, 0x4C91U);
    store(0x1A00U + kGiantFaceLeftHelmetSlot, 3U);
    store(0x1980U + kGiantFaceLeftHelmetSlot, 1U);
    store(0x1C80U + kGiantFaceLeftHelmetSlot, 0x13U);
    store(0x1A00U + kGiantFaceRightHelmetSlot, 1U);
    store(0x1980U + kGiantFaceRightHelmetSlot, 3U);
    store(0x1C80U + kGiantFaceRightHelmetSlot, 0x1DU);

    const GameStateView state(work_ram);
    const auto snapshot = state.giant_face();
    const auto captured = capture_dormant_giant_face(state);
    const auto helmet_capture = capture_giant_face_helmet(state);
    check(
        snapshot && snapshot->persisted_form == 0U && !snapshot->wake_requested
            && !snapshot->transformation_requested
            && snapshot->display_selector == 0U && snapshot->animation_mode == 0U
            && !snapshot->mouth_open && !snapshot->bonus_stage_4_requested
            && captured && captured->actors.size() == 4U
            && captured->actors[0].index == kGiantFaceControllerSlot
            && captured->actors[3].index == kGiantFaceCrownSlot,
        "native Giant Face capture decodes the fixed dormant family and its clear latches");
    check(
        helmet_capture && helmet_capture->actors[0].index == kGiantFaceLeftHelmetSlot
            && helmet_capture->actors[1].index == kGiantFaceRightHelmetSlot
            && helmet_capture->rule_state.channels[0U].ticks_until_appearance == 3U
            && helmet_capture->rule_state.channels[0U].appearance == 1U
            && helmet_capture->rule_state.channels[1U].ticks_until_appearance == 1U
            && helmet_capture->rule_state.channels[1U].appearance == 3U,
        "native Giant Face helmet capture restores both independent idle cadences");

    NativeGameplayState gameplay;
    if (captured) {
        gameplay.seed_giant_face(captured->rule_state);
    }
    check(
        captured && gameplay.giant_face() == captured->rule_state,
        "native Giant Face dormant capture seeds the native rule scheduler directly");

    store(0x1000U, 0x81U);
    const auto bootstrap = capture_native_gameplay_bootstrap(state);
    NativeGameplayState bootstrapped_gameplay;
    bool bootstrap_applied = false;
    if (bootstrap) {
        bootstrap_applied = apply_native_gameplay_bootstrap(
            bootstrapped_gameplay, *bootstrap);
    }
    check(
        bootstrap && bootstrap_applied && bootstrap->giant_face == captured
            && bootstrap->giant_face_helmet == helmet_capture
            && bootstrapped_gameplay.physics().ball.position_x.raw == 0
            && bootstrapped_gameplay.score().stored_units == 0U
            && bootstrapped_gameplay.table_rule_latches()
                == bootstrap->table_rule_latches
            && bootstrapped_gameplay.giant_face() == captured->rule_state
            && bootstrapped_gameplay.giant_face_helmet()
            && bootstrapped_gameplay.giant_face_helmet()->channels
                == helmet_capture->rule_state.channels,
        "native gameplay bootstrap applies coherent ball, score, table, and dormant Face captures");

    NativeGameplayRuntime runtime;
    const bool runtime_bootstrapped = bootstrap && runtime.bootstrap(*bootstrap);
    const auto runtime_frame = runtime.advance_actor_frame();
    check(
        runtime_bootstrapped && runtime.ready() && runtime.frame_number() == 1U
            && runtime.gameplay()
            && runtime.gameplay()->giant_face() == captured->rule_state
            && runtime_frame && runtime_frame->frame_number == 1U
            && !runtime.bootstrap(*bootstrap),
        "native gameplay runtime owns one bootstrap and advances the actor phase");

    NativeGameplayRuntime frame_runtime;
    const bool frame_runtime_bootstrapped = bootstrap && frame_runtime.bootstrap(*bootstrap);
    if (frame_runtime.gameplay()) {
        frame_runtime.gameplay()->seed_ball({
            .position_x = Fixed24_8{.raw = 16 * 256},
            .position_y = Fixed24_8{.raw = 32 * 256},
            .velocity_x = 0x0100,
        });
    }
    std::array<std::uint8_t, CollisionMap::kColumns> clear_tile_ids{};
    std::array<std::uint8_t, CollisionMap::kColumns> clear_mask_indices{};
    std::array<CollisionMask, 2U> clear_masks{};
    const CollisionMap clear_map{
        .tile_ids = clear_tile_ids,
        .mask_indices = clear_mask_indices,
        .masks = clear_masks,
    };
    const auto completed_frame = frame_runtime.advance_physics_epoch(
        BallMotionTuning{
            .horizontal_drag_divisor = 8,
            .vertical_drag_divisor = 8,
            .vertical_gravity = 1U,
        },
        clear_map);
    check(
        frame_runtime_bootstrapped && completed_frame
            && completed_frame->frame_number == 1U
            && completed_frame->physics.no_contact
            && !completed_frame->requires_collision_resolution()
            && frame_runtime.gameplay()
            && frame_runtime.gameplay()->physics()
                == completed_frame->physics.no_contact->state,
        "native gameplay runtime joins a clear physics epoch to the actor boundary");

    NativeGameplayRuntime contact_runtime;
    const bool contact_runtime_bootstrapped = bootstrap && contact_runtime.bootstrap(*bootstrap);
    if (contact_runtime.gameplay()) {
        contact_runtime.gameplay()->seed_ball({
            .position_x = Fixed24_8{.raw = 15 * 256},
            .position_y = Fixed24_8{.raw = 32 * 256},
            .velocity_x = 0x0100,
        });
    }
    clear_tile_ids[2U] = 0x7CU;
    clear_mask_indices[2U] = 1U;
    clear_masks[1U].rows[0U] = 0x80U;
    const CollisionMap solid_map{
        .tile_ids = clear_tile_ids,
        .mask_indices = clear_mask_indices,
        .masks = clear_masks,
    };
    const auto contact_frame = contact_runtime.advance_frame(
        BallMotionTuning{.horizontal_drag_divisor = 8}, solid_map);
    check(
        contact_runtime_bootstrapped && contact_frame
            && contact_frame->frame_number == 1U
            && contact_frame->requires_collision_resolution()
            && contact_frame->actor_updates.empty()
            && contact_frame->audio_events.empty()
            && contact_runtime.gameplay()
            && contact_runtime.gameplay()->pending_playfield_contact()
            && contact_runtime.gameplay()->pending_playfield_collision_loop()
            && contact_runtime.gameplay()->pending_playfield_collision_loop()
                    ->completed_substeps == 0U
            && !contact_runtime.advance_frame(
                BallMotionTuning{.horizontal_drag_divisor = 8}, solid_map)
            && !contact_runtime.advance_actor_frame(),
        "native gameplay runtime preserves a solid contact for its later resolver");
    const auto resolved_contact_ball = contact_frame && contact_frame->physics.contact
        ? contact_frame->physics.contact->entry_ball
        : devils_crush::native_runtime::BallState{};
    const auto completed_contact_frame = contact_runtime.complete_playfield_collision(
        resolved_contact_ball);
    check(
        completed_contact_frame
            && completed_contact_frame->frame_number == 1U
            && contact_frame && contact_frame->physics.contact
            && completed_contact_frame->contact == *contact_frame->physics.contact
            && completed_contact_frame->resolved_ball == resolved_contact_ball
            && contact_runtime.frame_number() == 1U
            && contact_runtime.gameplay()
            && contact_runtime.gameplay()->physics().ball == resolved_contact_ball
            && !contact_runtime.gameplay()->pending_playfield_contact()
            && !contact_runtime.complete_playfield_collision(resolved_contact_ball),
        "native gameplay runtime completes the pending contact in its original frame");

    NativeGameplayRuntime retry_runtime;
    const bool retry_runtime_bootstrapped = bootstrap && retry_runtime.bootstrap(*bootstrap);
    if (retry_runtime.gameplay()) {
        retry_runtime.gameplay()->seed_ball({
            .position_x = Fixed24_8{.raw = 15 * 256},
            .position_y = Fixed24_8{.raw = 32 * 256},
            .velocity_x = 0x0100,
        });
    }
    const auto retry_contact_frame = retry_runtime.advance_frame(
        BallMotionTuning{.horizontal_drag_divisor = 8}, solid_map);
    const auto retry_update = retry_runtime.retry_pending_playfield_collision({
        .by_direction = {{
            {.horizontal_pixels = 1},
        }},
    });
    check(
        retry_runtime_bootstrapped && retry_contact_frame
            && retry_contact_frame->requires_collision_resolution()
            && retry_update && retry_update->frame_number == 1U
            && retry_update->retry.disposition
                == devils_crush::native_runtime::CollisionRetryProbeDisposition::clear
            && retry_update->completes_frame()
            && retry_runtime.frame_number() == 1U
            && retry_runtime.gameplay()
            && retry_runtime.gameplay()->physics().ball == retry_update->retry.retry.state
            && retry_runtime.gameplay()->physics().ball.position_x.raw == 16 * 256
            && !retry_runtime.gameplay()->pending_playfield_contact()
            && !retry_runtime.retry_pending_playfield_collision({}),
        "native gameplay runtime completes a clear collision retry in its original frame");

    NativeGameplayRuntime full_scan_retry_runtime;
    const bool full_scan_retry_bootstrapped = bootstrap
        && full_scan_retry_runtime.bootstrap(*bootstrap);
    if (full_scan_retry_runtime.gameplay()) {
        full_scan_retry_runtime.gameplay()->seed_ball({
            .position_x = Fixed24_8{.raw = 32 * 256},
            .position_y = Fixed24_8{.raw = 256 * 256},
            .velocity_x = 0x0100,
        });
    }
    std::array<std::uint8_t, CollisionMap::kColumns * 29U> full_scan_tile_ids{};
    std::array<std::uint8_t, CollisionMap::kColumns * 29U> full_scan_mask_indices{};
    constexpr auto full_scan_collision_index = 4U;
    full_scan_tile_ids[full_scan_collision_index + CollisionMap::kColumns * 28U]
        = 0x7CU;
    full_scan_mask_indices[
        full_scan_collision_index + CollisionMap::kColumns * 28U] = 1U;
    auto full_scan_masks = clear_masks;
    full_scan_masks[1U].rows[0U] = 0x40U;
    const CollisionMap full_scan_map{
        .tile_ids = full_scan_tile_ids,
        .mask_indices = full_scan_mask_indices,
        .masks = full_scan_masks,
    };
    const auto full_scan_retry_contact = full_scan_retry_runtime.advance_frame(
        BallMotionTuning{.horizontal_drag_divisor = 8}, full_scan_map);
    const auto full_scan_retry_update =
        full_scan_retry_runtime.retry_pending_playfield_collision({
            .by_direction = {{
                {.horizontal_pixels = 1},
            }},
        });
    check(
        full_scan_retry_bootstrapped && full_scan_retry_contact
            && full_scan_retry_contact->requires_collision_resolution()
            && full_scan_retry_update && full_scan_retry_update->frame_number == 1U
            && full_scan_retry_update->retry.disposition
                == devils_crush::native_runtime::CollisionRetryProbeDisposition::requires_full_scan
            && !full_scan_retry_update->completes_frame()
            && full_scan_retry_update->actor_updates.empty()
            && full_scan_retry_update->audio_events.empty()
            && full_scan_retry_runtime.frame_number() == 1U
            && full_scan_retry_runtime.gameplay()
            && full_scan_retry_runtime.gameplay()->pending_playfield_contact()
            && full_scan_retry_runtime.gameplay()->pending_collision_retry_full_scan()
            && !full_scan_retry_runtime.advance_frame(
                BallMotionTuning{.horizontal_drag_divisor = 8}, full_scan_map),
        "native gameplay runtime retains a full-scan collision retry at its resolver boundary");

    NativeGameplayRuntime placement_retry_runtime;
    const bool placement_retry_bootstrapped = bootstrap
        && placement_retry_runtime.bootstrap(*bootstrap);
    if (placement_retry_runtime.gameplay()) {
        placement_retry_runtime.gameplay()->seed_ball({
            .position_x = Fixed24_8{.raw = 32 * 256},
            .position_y = Fixed24_8{.raw = 256 * 256},
            .velocity_x = 0x0100,
            .collision_state = 0x80U,
        });
    }
    const devils_crush::native_runtime::CollisionRetryOffsetAttempts
        placement_retry_offsets{
            .by_direction = {{
                {{{.horizontal_pixels = 1}, {}, {}}},
                {{{}, {}, {}}},
                {{{}, {}, {}}},
                {{{}, {}, {}}},
            }},
        };
    const auto placement_retry_contact = placement_retry_runtime.advance_frame(
        BallMotionTuning{.horizontal_drag_divisor = 8}, full_scan_map);
    const auto placement_retry_initial =
        placement_retry_runtime.begin_pending_collision_retry(
            placement_retry_offsets);
    const auto placement_retry_accepted =
        placement_retry_runtime.resolve_pending_collision_retry_full_scan(
            devils_crush::native_runtime::CollisionRetryFullScanOutcome::clear);
    const auto placement_retry_rearmed_ball = placement_retry_runtime.gameplay()
        ? placement_retry_runtime.gameplay()->physics().ball
        : devils_crush::native_runtime::BallState{};
    const bool placement_retry_rearmed_contact = placement_retry_runtime.gameplay()
        && placement_retry_runtime.gameplay()->pending_playfield_contact().has_value();
    const auto placement_retry_resume =
        placement_retry_runtime.resume_rearmed_collision_retry(
            BallMotionTuning{.horizontal_drag_divisor = 8}, clear_map);
    check(
        placement_retry_bootstrapped && placement_retry_contact
            && placement_retry_contact->requires_collision_resolution()
            && placement_retry_initial
            && placement_retry_initial->frame_number == 1U
            && placement_retry_initial->placement.disposition
                == devils_crush::native_runtime::CollisionRetryPlacementDisposition::probe_ready
            && !placement_retry_initial->completes_frame()
            && placement_retry_initial->actor_updates.empty()
            && placement_retry_initial->audio_events.empty()
            && placement_retry_accepted
            && placement_retry_accepted->frame_number == 1U
            && placement_retry_accepted->placement.disposition
                == devils_crush::native_runtime::CollisionRetryPlacementDisposition::rearmed
            && !placement_retry_accepted->completes_frame()
            && placement_retry_runtime.frame_number() == 1U
            && placement_retry_runtime.gameplay()
            && placement_retry_rearmed_contact
            && !placement_retry_runtime.gameplay()->pending_collision_retry_placement()
            && placement_retry_accepted->placement.probe
            && placement_retry_rearmed_ball.position_x
                == placement_retry_accepted->placement.probe->state.position_x
            && placement_retry_rearmed_ball.position_y
                == placement_retry_accepted->placement.probe->state.position_y
            && placement_retry_rearmed_ball.velocity_x
                == placement_retry_accepted->placement.probe->state.velocity_x
            && placement_retry_rearmed_ball.velocity_y
                == placement_retry_accepted->placement.probe->state.velocity_y
            && (placement_retry_rearmed_ball.collision_state
                & 0x80U) == 0U
            && placement_retry_resume
            && placement_retry_resume->frame_number == 1U
            && placement_retry_resume->completes_frame()
            && !placement_retry_resume->physics.next_contact
            && !placement_retry_runtime.gameplay()->pending_playfield_contact()
            && !placement_retry_runtime.resolve_pending_collision_retry_full_scan(
                devils_crush::native_runtime::CollisionRetryFullScanOutcome::clear),
        "native gameplay runtime rearms a clear CDF8 probe into its original C6/C7 frame");

    NativeGameplayRuntime random_placement_retry_runtime;
    const bool random_placement_retry_bootstrapped = bootstrap
        && random_placement_retry_runtime.bootstrap(*bootstrap);
    if (random_placement_retry_runtime.gameplay()) {
        random_placement_retry_runtime.gameplay()->seed_ball({
            .position_x = Fixed24_8{.raw = 32 * 256},
            .position_y = Fixed24_8{.raw = 256 * 256},
            .velocity_x = 0x0100,
            .collision_state = 0x80U,
        });
    }
    const devils_crush::native_runtime::CollisionRetryOffsetAttempts
        random_placement_retry_offsets{
            .by_direction = {{
                {{{.horizontal_pixels = 1}, {.horizontal_pixels = 1},
                  {.horizontal_pixels = 1}}},
                {{{.vertical_pixels = 1}, {}, {}}},
                {{{}, {}, {}}},
                {{{}, {}, {}}},
            }},
        };
    const auto random_placement_retry_contact =
        random_placement_retry_runtime.advance_frame(
            BallMotionTuning{.horizontal_drag_divisor = 8}, full_scan_map);
    const auto random_placement_retry_initial =
        random_placement_retry_runtime.begin_pending_collision_retry(
            random_placement_retry_offsets);
    const auto random_placement_retry_second =
        random_placement_retry_runtime.resolve_pending_collision_retry_full_scan(
            devils_crush::native_runtime::CollisionRetryFullScanOutcome::solid);
    const auto random_placement_retry_third =
        random_placement_retry_runtime.resolve_pending_collision_retry_full_scan(
            devils_crush::native_runtime::CollisionRetryFullScanOutcome::solid);
    const auto random_placement_retry_select =
        random_placement_retry_runtime.resolve_pending_collision_retry_full_scan(
            devils_crush::native_runtime::CollisionRetryFullScanOutcome::solid);
    const auto duplicate_random_selection =
        random_placement_retry_runtime.select_pending_collision_retry_direction(0x00U);
    const auto accepted_random_selection =
        random_placement_retry_runtime.select_pending_collision_retry_direction(0x40U);
    check(
        random_placement_retry_bootstrapped && random_placement_retry_contact
            && random_placement_retry_contact->requires_collision_resolution()
            && random_placement_retry_initial
            && random_placement_retry_second
            && random_placement_retry_third
            && random_placement_retry_select
            && random_placement_retry_select->placement.disposition
                == devils_crush::native_runtime::CollisionRetryPlacementDisposition::needs_random_direction
            && duplicate_random_selection
            && duplicate_random_selection->placement.disposition
                == devils_crush::native_runtime::CollisionRetryPlacementDisposition::needs_random_direction
            && !duplicate_random_selection->completes_frame()
            && accepted_random_selection
            && accepted_random_selection->placement.disposition
                == devils_crush::native_runtime::CollisionRetryPlacementDisposition::rearmed
            && !accepted_random_selection->completes_frame()
            && accepted_random_selection->frame_number == 1U
            && random_placement_retry_runtime.frame_number() == 1U
            && random_placement_retry_runtime.gameplay()
            && random_placement_retry_runtime.gameplay()->pending_playfield_contact()
            && !random_placement_retry_runtime.gameplay()
                ->pending_collision_retry_placement()
            && accepted_random_selection->placement.probe
            && random_placement_retry_runtime.gameplay()->physics().ball.position_x
                == accepted_random_selection->placement.probe->state.position_x
            && random_placement_retry_runtime.gameplay()->physics().ball.position_y
                == accepted_random_selection->placement.probe->state.position_y
            && (random_placement_retry_runtime.gameplay()->physics().ball
                    .collision_state
                & 0x80U) == 0U,
        "native gameplay runtime retains CDF8's duplicate RNG wait and rearms its selected retry");

    NativeGameplayRuntime asset_scan_retry_runtime;
    const bool asset_scan_retry_bootstrapped = bootstrap
        && asset_scan_retry_runtime.bootstrap(*bootstrap);
    if (asset_scan_retry_runtime.gameplay()) {
        asset_scan_retry_runtime.gameplay()->seed_ball({
            .position_x = Fixed24_8{.raw = 32 * 256},
            .position_y = Fixed24_8{.raw = 256 * 256},
            .velocity_x = 0x0100,
            .collision_state = 0x80U,
        });
    }
    const auto asset_scan_retry_contact = asset_scan_retry_runtime.advance_frame(
        BallMotionTuning{.horizontal_drag_divisor = 8}, full_scan_map);
    const auto asset_scan_retry_initial =
        asset_scan_retry_runtime.begin_pending_collision_retry(
            placement_retry_offsets);
    std::vector<std::byte> asset_scan_rom(
        devils_crush::native_runtime::MappedRomView::kSupportedRomBytes);
    std::array<std::uint8_t, 8U> asset_scan_mpr{};
    asset_scan_mpr[4U] = 0x01U;
    asset_scan_mpr[5U] = 0x02U;
    asset_scan_mpr[6U] = 0x03U;
    std::array<
        std::byte,
        devils_crush::native_runtime::GameStateView::kWorkRamSize>
        asset_scan_ram{};
    const auto store_asset_scan_word = [&asset_scan_ram](
                                          const std::size_t offset,
                                          const std::uint16_t value) {
        asset_scan_ram[offset] = static_cast<std::byte>(value);
        asset_scan_ram[offset + 1U] = static_cast<std::byte>(value >> 8U);
    };
    for (std::size_t index = 0U; index < 4U; ++index) {
        store_asset_scan_word(0x0096U + index * 2U, 0x8000U);
    }
    store_asset_scan_word(0x0074U, 0x8000U);
    store_asset_scan_word(0x0072U, 0x8000U);
    store_asset_scan_word(0x008EU, 0x8000U);
    store_asset_scan_word(0x00C8U, 0x9000U);
    store_asset_scan_word(0x00C6U, 0xA000U);
    store_asset_scan_word(0x0090U, 0xC000U);
    const auto asset_scan_retry_resolution =
        asset_scan_retry_runtime.resolve_pending_collision_retry_full_scan(
            devils_crush::native_runtime::MappedRomView(
                asset_scan_rom, asset_scan_mpr),
            devils_crush::native_runtime::GameStateView(asset_scan_ram),
            0x01U);
    check(
        asset_scan_retry_bootstrapped && asset_scan_retry_contact
            && asset_scan_retry_contact->requires_collision_resolution()
            && asset_scan_retry_initial
            && asset_scan_retry_resolution
            && asset_scan_retry_resolution->scan
            && asset_scan_retry_resolution->scan->disposition
                == devils_crush::native_runtime::CollisionRetryFullScanDisposition::playfield_evaluated
            && asset_scan_retry_resolution->placement.disposition
                == devils_crush::native_runtime::CollisionRetryPlacementDisposition::rearmed
            && !asset_scan_retry_resolution->completes_frame()
            && asset_scan_retry_runtime.frame_number() == 1U
            && asset_scan_retry_runtime.gameplay()
            && asset_scan_retry_runtime.gameplay()->pending_playfield_contact()
            && !asset_scan_retry_runtime.gameplay()->pending_collision_retry_placement(),
        "native gameplay runtime completes a CDF8 placement through its CED2/CD74 asset scan");

    store(0x0258U, 0x01U);
    store(0x0D0DU, 0x01U);
    store(0x0D0EU, 0x01U);
    store(0x1A00U + kGiantFaceAnimatorSlot, 0x05U);
    store(0x1A80U + kGiantFaceAnimatorSlot, 0x01U);
    const auto awake_capture = capture_awake_lower_giant_face(state);
    NativeGameplayState awake_gameplay;
    if (awake_capture) {
        awake_gameplay.seed_giant_face(
            awake_capture->rule_state,
            awake_capture->controller_state,
            awake_capture->animator_state);
    }
    const auto awake_phase = awake_gameplay.advance_actor_phase();
    const auto awake_bootstrap = capture_native_gameplay_bootstrap(state);
    NativeGameplayState awake_bootstrapped_gameplay;
    const bool awake_bootstrap_applied = awake_bootstrap
        && apply_native_gameplay_bootstrap(awake_bootstrapped_gameplay, *awake_bootstrap);
    check(
        awake_capture
            && awake_capture->controller_state.persisted_form == 1U
            && awake_capture->controller_state.accepts_transformations
            && awake_capture->animator_state
                == devils_crush::native_runtime::GiantFaceAnimatorState{
                    .mode = devils_crush::native_runtime::GiantFaceAnimatorMode::lower_forms,
                    .ticks_until_next_form = 5U,
                    .displayed_form = 1U,
                }
            && awake_phase.size() == 1U
            && awake_phase.front().giant_face_animator
            && awake_phase.front().giant_face_animator->state.ticks_until_next_form == 4U
            && awake_bootstrap_applied && awake_bootstrap->giant_face == awake_capture
            && awake_bootstrapped_gameplay.giant_face_controller()
                == awake_capture->controller_state
            && awake_bootstrapped_gameplay.giant_face_animator()
                == awake_capture->animator_state,
        "native Giant Face capture restores the settled lower-form controller and animator loop");
    store(0x0258U, 0x03U);
    const auto transformed_lower_capture = capture_awake_lower_giant_face(state);
    check(
        transformed_lower_capture
            && transformed_lower_capture->controller_state
                == devils_crush::native_runtime::GiantFaceControllerState{
                    .persisted_form = 3U,
                    .accepts_transformations = true,
                    .transformations_completed = 2U,
                },
        "native Giant Face lower-form capture retains completed transformation progress");
    store(0x0258U, 0x04U);
    store(0x0D0DU, 0x04U);
    store(0x0D0EU, 0x02U);
    store(0x1A00U + kGiantFaceAnimatorSlot, 0x08U);
    store(0x1A80U + kGiantFaceAnimatorSlot, 0x05U);
    const auto upper_capture = capture_awake_upper_giant_face(state);
    const auto upper_bootstrap = capture_native_gameplay_bootstrap(state);
    check(
        upper_capture
            && upper_capture->controller_state
                == devils_crush::native_runtime::GiantFaceControllerState{
                    .persisted_form = 4U,
                    .accepts_transformations = true,
                    .transformations_completed = 3U,
                }
            && upper_capture->animator_state
                == devils_crush::native_runtime::GiantFaceAnimatorState{
                    .mode = devils_crush::native_runtime::GiantFaceAnimatorMode::upper_forms,
                    .ticks_until_next_form = 8U,
                    .displayed_form = 5U,
                }
            && upper_bootstrap && upper_bootstrap->giant_face == upper_capture,
        "native Giant Face capture restores the settled upper-form loop");
    store(0x0258U, 0x00U);
    store(0x0D0DU, 0x00U);
    store(0x0D0EU, 0x00U);
    store(0x1A00U + kGiantFaceAnimatorSlot, 0x00U);
    store(0x1A80U + kGiantFaceAnimatorSlot, 0x00U);

    store(0x1000U, 0x00U);
    check(
        !capture_native_gameplay_bootstrap(state),
        "native gameplay bootstrap rejects a frame without a live ball");
    store(0x1000U, 0x81U);

    store(0x1800U + kGiantFaceMouthSlot, 0x80U);
    check(
        !capture_dormant_giant_face(state),
        "native Giant Face capture leaves a pending mouth contact with its collision owner");
    store(0x1800U + kGiantFaceMouthSlot, 0x00U);
    store(0x0D0CU, 0x01U);
    check(
        !capture_dormant_giant_face(state),
        "native Giant Face capture excludes an in-flight helmet wake request");
    store(0x0D0CU, 0x00U);
    store(0x0258U, 0x01U);
    store(0x0D0EU, 0x01U);
    store(0x1A00U + kGiantFaceAnimatorSlot, 0x00U);
    check(
        !capture_awake_lower_giant_face(state),
        "native Giant Face active capture excludes an expired animator turn");
    store(0x0258U, 0x00U);
    store(0x0D0EU, 0x00U);
    store(0x0D21U, 0x01U);
    check(
        !capture_dormant_giant_face(state),
        "native Giant Face capture excludes an open-mouth transformation state");
    store(0x0D21U, 0x80U);
    check(
        !capture_dormant_giant_face(state),
        "native Giant Face capture excludes a completed mouth bonus request");
    store(0x0D21U, 0x00U);
    store(0x1F00U + kGiantFaceControllerSlot, 0x28U);
    check(
        !capture_dormant_giant_face(state),
        "native Giant Face capture rejects its setup-handler ownership");
    store(0x1F00U + kGiantFaceControllerSlot, 0xD8U);
    store(0x1800U + kGiantFaceLeftHelmetSlot, 0x80U);
    check(
        !capture_giant_face_helmet(state),
        "native Giant Face helmet capture excludes a collision-owned opening");
    store(0x1800U + kGiantFaceLeftHelmetSlot, 0x00U);
    store(0x1C80U + kGiantFaceRightHelmetSlot, 0x13U);
    check(
        !capture_giant_face_helmet(state),
        "native Giant Face helmet capture retains each fixed side selector");
    store(0x1C80U + kGiantFaceRightHelmetSlot, 0x1DU);

    const GameStateView truncated(
        std::span<const std::byte>(work_ram).first(work_ram.size() - 1U));
    check(
        !capture_dormant_giant_face(truncated)
            && !capture_giant_face_helmet(truncated),
        "native Giant Face captures reject a truncated work-RAM image");
}

void test_native_giant_skeleton_eyeball_capture()
{
    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::NativeGameplayState;
    using devils_crush::native_runtime::apply_native_gameplay_bootstrap;
    using devils_crush::native_runtime::capture_giant_skeleton_eyeball_actor;
    using devils_crush::native_runtime::capture_native_gameplay_bootstrap;
    using devils_crush::native_runtime::kGiantSkeletonEyeballSlot;

    std::array<std::byte, GameStateView::kWorkRamSize> work_ram{};
    const auto store = [&work_ram](
                           const std::size_t offset,
                           const std::uint8_t value) {
        work_ram[offset] = static_cast<std::byte>(value);
    };
    store(0x1000U + kGiantSkeletonEyeballSlot, 0x82U);
    store(0x1A00U + kGiantSkeletonEyeballSlot, 0x08U);
    store(0x1C00U + kGiantSkeletonEyeballSlot, 0x02U);
    store(0x1F00U + kGiantSkeletonEyeballSlot, 0xF7U);
    store(0x1F80U + kGiantSkeletonEyeballSlot, 0x4DU);

    const GameStateView state(work_ram);
    const auto captured = capture_giant_skeleton_eyeball_actor(
        state, kGiantSkeletonEyeballSlot);
    check(
        captured && captured->actor.index == kGiantSkeletonEyeballSlot
            && captured->rule_state.opening_countdown_active
            && captured->rule_state.opening_ticks_remaining == 8U,
        "native Giant Skeleton Eyeball capture decodes its live opening countdown");

    NativeGameplayState gameplay;
    if (captured) {
        gameplay.seed_giant_skeleton_eyeball(captured->rule_state);
    }
    const auto phase = gameplay.advance_actor_phase();
    check(
        captured && phase.size() == 1U
            && phase.front().slot == kGiantSkeletonEyeballSlot
            && phase.front().giant_skeleton_eyeball
            && phase.front().giant_skeleton_eyeball->state.opening_ticks_remaining
                == 7U,
        "native Giant Skeleton Eyeball capture seeds the opening scheduler directly");

    store(0x1000U, 0x81U);
    const auto bootstrap = capture_native_gameplay_bootstrap(state);
    NativeGameplayState bootstrapped_gameplay;
    const bool bootstrap_applied = bootstrap
        && apply_native_gameplay_bootstrap(bootstrapped_gameplay, *bootstrap);
    check(
        bootstrap_applied && bootstrap->giant_skeleton
            && !bootstrap->giant_skeleton->skeleton
            && bootstrap->giant_skeleton->eyeball == captured
            && bootstrapped_gameplay.giant_skeleton_eyeball()
            && *bootstrapped_gameplay.giant_skeleton_eyeball()
                == captured->rule_state,
        "native gameplay bootstrap restores an Eyeball opening countdown");
    store(0x1000U, 0x00U);

    store(0x1A00U + kGiantSkeletonEyeballSlot, 0x01U);
    check(
        !capture_giant_skeleton_eyeball_actor(state, kGiantSkeletonEyeballSlot),
        "native Giant Skeleton Eyeball capture leaves expiry with its animation owner");
    store(0x1A00U + kGiantSkeletonEyeballSlot, 0x08U);
    store(0x1C00U + kGiantSkeletonEyeballSlot, 0x01U);
    check(
        !capture_giant_skeleton_eyeball_actor(state, kGiantSkeletonEyeballSlot),
        "native Giant Skeleton Eyeball capture rejects another state");
    store(0x1C00U + kGiantSkeletonEyeballSlot, 0x02U);
    store(0x1F00U + kGiantSkeletonEyeballSlot, 0xD9U);
    check(
        !capture_giant_skeleton_eyeball_actor(state, kGiantSkeletonEyeballSlot),
        "native Giant Skeleton Eyeball capture excludes dormant dispatcher ownership");
    store(0x1F00U + kGiantSkeletonEyeballSlot, 0xF7U);
    check(
        !capture_giant_skeleton_eyeball_actor(state, 0x30U),
        "native Giant Skeleton Eyeball capture rejects the adjacent Giant Skeleton slot");

    const GameStateView truncated(
        std::span<const std::byte>(work_ram).first(work_ram.size() - 1U));
    check(
        !capture_giant_skeleton_eyeball_actor(
            truncated, kGiantSkeletonEyeballSlot),
        "native Giant Skeleton Eyeball capture rejects a truncated work-RAM image");
}

void test_native_giant_skeleton_eyeball_final_animation_capture()
{
    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::NativeGameplayState;
    using devils_crush::native_runtime::capture_giant_skeleton_eyeball_final_animation_actor;
    using devils_crush::native_runtime::kGiantSkeletonEyeballSlot;

    std::array<std::byte, GameStateView::kWorkRamSize> work_ram{};
    const auto store = [&work_ram](
                           const std::size_t offset,
                           const std::uint8_t value) {
        work_ram[offset] = static_cast<std::byte>(value);
    };
    store(0x1000U + kGiantSkeletonEyeballSlot, 0x82U);
    store(0x1A00U + kGiantSkeletonEyeballSlot, 0x08U);
    store(0x1B00U + kGiantSkeletonEyeballSlot, 0xE0U);
    store(0x1B80U + kGiantSkeletonEyeballSlot, 0x03U);
    store(0x1980U + kGiantSkeletonEyeballSlot, 0x01U);
    store(0x1F00U + kGiantSkeletonEyeballSlot, 0x3DU);
    store(0x1F80U + kGiantSkeletonEyeballSlot, 0x4FU);

    const GameStateView state(work_ram);
    const auto captured = capture_giant_skeleton_eyeball_final_animation_actor(
        state, kGiantSkeletonEyeballSlot);
    check(
        captured && captured->actor.index == kGiantSkeletonEyeballSlot
            && captured->rule_state.final_animation_active
            && captured->rule_state.final_ticks_remaining == 0x03E0U
            && captured->rule_state.final_appearance_ticks_remaining == 8U
            && captured->rule_state.final_appearance == 1U,
        "native Giant Skeleton Eyeball capture decodes its direct final-animation wait");

    NativeGameplayState gameplay;
    if (captured) {
        gameplay.seed_giant_skeleton_eyeball(captured->rule_state);
    }
    const auto phase = gameplay.advance_actor_phase();
    check(
        captured && phase.size() == 1U
            && phase.front().slot == kGiantSkeletonEyeballSlot
            && phase.front().giant_skeleton_eyeball
            && phase.front().giant_skeleton_eyeball->state.final_ticks_remaining
                == 0x03DFU
            && phase.front().giant_skeleton_eyeball->state
                   .final_appearance_ticks_remaining
                == 7U,
        "native Giant Skeleton Eyeball final capture seeds the fixed actor scheduler directly");

    store(0x1A00U + kGiantSkeletonEyeballSlot, 0x01U);
    check(
        !capture_giant_skeleton_eyeball_final_animation_actor(
            state, kGiantSkeletonEyeballSlot),
        "native Giant Skeleton Eyeball final capture leaves appearance expiry with its source owner");
    store(0x1A00U + kGiantSkeletonEyeballSlot, 0x08U);
    store(0x1B00U + kGiantSkeletonEyeballSlot, 0x01U);
    check(
        !capture_giant_skeleton_eyeball_final_animation_actor(
            state, kGiantSkeletonEyeballSlot),
        "native Giant Skeleton Eyeball final capture leaves phase transitions with its source owner");
    store(0x1B00U + kGiantSkeletonEyeballSlot, 0xE0U);
    store(0x1980U + kGiantSkeletonEyeballSlot, 0x02U);
    check(
        !capture_giant_skeleton_eyeball_final_animation_actor(
            state, kGiantSkeletonEyeballSlot),
        "native Giant Skeleton Eyeball final capture rejects an unproven appearance");
    store(0x1980U + kGiantSkeletonEyeballSlot, 0x01U);
    store(0x1F00U + kGiantSkeletonEyeballSlot, 0x24U);
    check(
        !capture_giant_skeleton_eyeball_final_animation_actor(
            state, kGiantSkeletonEyeballSlot),
        "native Giant Skeleton Eyeball final capture requires its direct wait continuation");

    const GameStateView truncated(
        std::span<const std::byte>(work_ram).first(work_ram.size() - 1U));
    check(
        !capture_giant_skeleton_eyeball_final_animation_actor(
            truncated, kGiantSkeletonEyeballSlot),
        "native Giant Skeleton Eyeball final capture rejects a truncated work-RAM image");
}

void test_native_tower_capture()
{
    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::NativeGameplayState;
    using devils_crush::native_runtime::apply_native_gameplay_bootstrap;
    using devils_crush::native_runtime::capture_tower_guard_actor;
    using devils_crush::native_runtime::capture_tower_actor;
    using devils_crush::native_runtime::capture_native_gameplay_bootstrap;
    using devils_crush::native_runtime::kTowerGuardSlot;

    std::array<std::byte, GameStateView::kWorkRamSize> work_ram{};
    const auto store = [&work_ram](
                           const std::size_t offset,
                           const std::uint8_t value) {
        work_ram[offset] = static_cast<std::byte>(value);
    };
    constexpr std::uint8_t kTowerSlot = 0x2DU;
    store(0x1000U + kTowerSlot, 0x83U);
    store(0x1800U + kTowerSlot, 0x00U);
    store(0x1F00U + kTowerSlot, 0xC3U);
    store(0x1F80U + kTowerSlot, 0x60U);
    store(0x0D3FU, 0x01U);
    store(0x0D4DU, 0x04U);
    store(0x1000U + kTowerGuardSlot, 0x82U);
    store(0x1F00U + kTowerGuardSlot, 0xA6U);
    store(0x1F80U + kTowerGuardSlot, 0x60U);

    const GameStateView state(work_ram);
    const auto captured = capture_tower_actor(state, kTowerSlot);
    const auto guard_capture = capture_tower_guard_actor(state, kTowerGuardSlot);
    check(
        captured && captured->actor.index == kTowerSlot
            && captured->rule_state.collision_ready
            && captured->rule_state.hit_progress == 1U
            && captured->rule_state.remaining_armed_passes == 4U,
        "native Tower capture decodes its fixed steady state and hit progress");
    check(
        guard_capture && guard_capture->selector.remaining_armed_passes == 4U,
        "native Tower capture keeps the paired Guard's derived selector available");

    NativeGameplayState gameplay;
    if (captured) {
        gameplay.seed_tower(captured->rule_state);
    }
    check(
        captured && gameplay.tower() == captured->rule_state,
        "native Tower capture seeds the fixed-target scheduler directly");

    store(0x1000U, 0x81U);
    const auto bootstrap = capture_native_gameplay_bootstrap(state);
    NativeGameplayState bootstrapped_gameplay;
    const bool bootstrap_applied = bootstrap
        && apply_native_gameplay_bootstrap(bootstrapped_gameplay, *bootstrap);
    const auto guard_phase = bootstrapped_gameplay.advance_actor_phase();
    check(
        bootstrap_applied && bootstrap->tower == captured
            && bootstrap->tower_guard == guard_capture
            && bootstrapped_gameplay.tower() == captured->rule_state
            && guard_phase.size() == 1U
            && guard_phase.front().slot == kTowerGuardSlot
            && guard_phase.front().kind
                == NativeGameplayState::ActorPhaseUpdateKind::tower_guard_selector
            && guard_phase.front().tower_guard == guard_capture->selector,
        "native gameplay bootstrap restores Tower and derives its Guard selector in actor order");
    store(0x1000U, 0x00U);

    store(0x1800U + kTowerSlot, 0x80U);
    check(
        !capture_tower_actor(state, kTowerSlot),
        "native Tower capture excludes a pending collision");
    store(0x1800U + kTowerSlot, 0x00U);
    store(0x1F00U + kTowerSlot, 0x1CU);
    store(0x1F80U + kTowerSlot, 0x61U);
    check(
        !capture_tower_actor(state, kTowerSlot),
        "native Tower capture excludes presentation-transition ownership");
    store(0x1F00U + kTowerSlot, 0xC3U);
    store(0x1F80U + kTowerSlot, 0x60U);
    check(
        !capture_tower_actor(state, 0x2EU),
        "native Tower capture rejects the adjacent Tower Guard slot");

    const GameStateView truncated(
        std::span<const std::byte>(work_ram).first(work_ram.size() - 1U));
    check(
        !capture_tower_actor(truncated, kTowerSlot)
            && !truncated.tower(kTowerSlot),
        "native Tower capture rejects a truncated work-RAM image");
}

void test_native_tower_guard_capture()
{
    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::TowerGuardSelectorDisposition;
    using devils_crush::native_runtime::capture_tower_guard_actor;
    using devils_crush::native_runtime::kTowerGuardSlot;

    std::array<std::byte, GameStateView::kWorkRamSize> work_ram{};
    const auto store = [&work_ram](
                           const std::size_t offset,
                           const std::uint8_t value) {
        work_ram[offset] = static_cast<std::byte>(value);
    };
    store(0x1000U + kTowerGuardSlot, 0x82U);
    store(0x1F00U + kTowerGuardSlot, 0xA6U);
    store(0x1F80U + kTowerGuardSlot, 0x60U);
    store(0x0D4DU, 0x03U);

    const GameStateView state(work_ram);
    const auto closed = capture_tower_guard_actor(state, kTowerGuardSlot);
    check(
        closed && closed->actor.index == kTowerGuardSlot
            && closed->selector.remaining_armed_passes == 3U
            && closed->selector.disposition == TowerGuardSelectorDisposition::closed,
        "native Tower Guard capture derives the closed selector from the Tower gate");

    store(0x0D4DU, 0x00U);
    const auto open = capture_tower_guard_actor(state, kTowerGuardSlot);
    check(
        open && open->selector.completion_selector == 0x8EU
            && open->selector.disposition
                == TowerGuardSelectorDisposition::open_bonus_stage_entrance,
        "native Tower Guard capture derives the open bonus-stage selector");

    store(0x1F00U + kTowerGuardSlot, 0xC3U);
    check(
        !capture_tower_guard_actor(state, kTowerGuardSlot),
        "native Tower Guard capture excludes a non-Guard dispatcher");
    store(0x1F00U + kTowerGuardSlot, 0xA6U);
    check(
        !capture_tower_guard_actor(state, 0x2DU),
        "native Tower Guard capture rejects the adjacent Tower slot");

    const GameStateView truncated(
        std::span<const std::byte>(work_ram).first(work_ram.size() - 1U));
    check(
        !capture_tower_guard_actor(truncated, kTowerGuardSlot),
        "native Tower Guard capture rejects a truncated work-RAM image");
}

void test_playfield_collision_capture_adapter()
{
    using devils_crush::emulation::CpuRegisters;
    using devils_crush::emulation::capture_playfield_collision_source;
    using devils_crush::native_runtime::GameStateView;

    std::array<std::byte, GameStateView::kWorkRamSize> work_ram{};
    work_ram[0x00C6U] = std::byte{0x40U};
    work_ram[0x00C7U] = std::byte{0x8AU};
    work_ram[0x00C8U] = std::byte{0x80U};
    work_ram[0x00C9U] = std::byte{0x8AU};
    work_ram[0x0090U] = std::byte{0x00U};
    work_ram[0x0091U] = std::byte{0xCFU};
    CpuRegisters registers{};
    registers.mpr[4] = 0x23U;
    registers.mpr[5] = 0x24U;

    const auto source = capture_playfield_collision_source(registers, work_ram);
    check(
        source && source->tables.tile_ids == 0x8A80U
            && source->tables.collision_attributes == 0x8A40U
            && source->tables.masks == 0xCF00U && source->mpr == registers.mpr,
        "playfield collision capture joins live table pointers to the MPR map");
    check(
        !capture_playfield_collision_source(
            registers,
            std::span<const std::byte>{work_ram}.first(work_ram.size() - 1U)),
        "playfield collision capture rejects truncated work RAM");
}

void test_native_actor_scheduler_view()
{
    using devils_crush::native_runtime::ActorSchedulerView;
    using devils_crush::native_runtime::ActorPresentationQueueKind;
    using devils_crush::native_runtime::ActorPresentationQueueState;
    using devils_crush::native_runtime::ActorPostHandlerPresentation;
    using devils_crush::native_runtime::ActorPassBehaviorDispatcher;
    using devils_crush::native_runtime::NativeGameplayActorBehaviorDispatcher;
    using devils_crush::native_runtime::NativeGameplayState;
    using devils_crush::native_runtime::ActorBehavior;
    using devils_crush::native_runtime::ActorMotionState;
    using devils_crush::native_runtime::Fixed24_8;
    using devils_crush::native_runtime::ShortActorPassPresentation;
    using devils_crush::native_runtime::ShortActorPass;
    using devils_crush::native_runtime::MainActorPass;
    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::actor_presentation_drain_order;
    using devils_crush::native_runtime::capture_actor_presentation_queue_state;
    using devils_crush::native_runtime::capture_actor_behavior_state_changes;
    using devils_crush::native_runtime::capture_actor_post_handler_presentation;
    using devils_crush::native_runtime::capture_scheduled_actor_handler_boundary;
    using devils_crush::native_runtime::file_completed_actor_for_presentation;
    using devils_crush::native_runtime::file_actor_for_presentation;
    using devils_crush::native_runtime::join_native_gameplay_actor_behavior_boundary;
    using devils_crush::native_runtime::join_native_gameplay_actor_behavior_presentation;

    std::array<std::byte, GameStateView::kWorkRamSize> work_ram{};
    const auto store_actor = [&work_ram](
                                 const std::uint8_t slot,
                                 const std::uint8_t activity,
                                 const std::uint8_t attributes,
                                 const std::uint16_t handler) {
        work_ram[0x1000U + slot] = static_cast<std::byte>(activity);
        work_ram[0x1080U + slot] = static_cast<std::byte>(attributes);
        work_ram[0x1F00U + slot] = static_cast<std::byte>(handler & 0x00FFU);
        work_ram[0x1F80U + slot] =
            static_cast<std::byte>(handler >> 8U);
    };

    store_actor(0x01U, 0x00U, 0x80U, 0xC001U);
    store_actor(0x02U, 0x80U, 0x80U, 0xC002U);
    store_actor(0x03U, 0x80U, 0x00U, 0xC003U);
    store_actor(0x04U, 0x80U, 0x02U, 0xC004U);
    store_actor(0x05U, 0xFFU, 0x82U, 0xC005U);

    const GameStateView state(work_ram);
    const ActorSchedulerView scheduler(state);

    work_ram[0x0D35U] = std::byte{0x00U};
    work_ram[0x0CEEU] = std::byte{0x5AU};
    const auto unfrozen_short_pass = scheduler.begin_short_actor_pass();
    const auto unfrozen_short_pass_capture = scheduler.capture_short_actor_pass();
    work_ram[0x0D35U] = std::byte{0x01U};
    const auto frozen_short_pass = scheduler.begin_short_actor_pass();
    const auto frozen_short_pass_capture = scheduler.capture_short_actor_pass();
    check(
        unfrozen_short_pass && !unfrozen_short_pass->gate.frozen
            && unfrozen_short_pass->gate.enabled_actor_mask == 0x5AU
            && unfrozen_short_pass->presentation_queues
                == ActorPresentationQueueState{}
            && unfrozen_short_pass->completed_ball_substeps == 0U
            && unfrozen_short_pass_capture
            && unfrozen_short_pass_capture->seed == *unfrozen_short_pass
            && unfrozen_short_pass_capture->scheduled_actors.size() == 3U
            && unfrozen_short_pass_capture->scheduled_actors[0].index == 0x02U
            && unfrozen_short_pass_capture->scheduled_actors[1].index == 0x03U
            && unfrozen_short_pass_capture->scheduled_actors[2].index == 0x05U
            && frozen_short_pass && frozen_short_pass->gate.frozen
            && frozen_short_pass->gate.enabled_actor_mask == 0U
            && frozen_short_pass_capture
            && frozen_short_pass_capture->seed == *frozen_short_pass
            && frozen_short_pass_capture->scheduled_actors
                == unfrozen_short_pass_capture->scheduled_actors,
        "native actor scheduler captures C539's gate and schedule together");
    work_ram[0x0D35U] = std::byte{0x00U};

    ShortActorPassPresentation short_pass(*unfrozen_short_pass);
    const auto short_pass_auxiliary = short_pass.complete_handler({
        .slot = 0x11U,
        .attributes_before_prequeue_presentation_refresh = 0x81U,
        .attributes_after_prequeue_presentation_refresh = 0x84U,
    });
    const auto short_pass_slot_zero = short_pass.complete_handler({
        .slot = 0x00U,
        .attributes_before_prequeue_presentation_refresh = 0x00U,
        .attributes_after_prequeue_presentation_refresh = 0x00U,
    });
    const auto short_pass_primary = short_pass.complete_handler({
        .slot = 0x21U,
        .attributes_before_prequeue_presentation_refresh = 0x80U,
        .attributes_after_prequeue_presentation_refresh = 0x88U,
    });
    check(
        !short_pass.gate().frozen
            && short_pass.gate().enabled_actor_mask == 0x5AU
            && short_pass.completed_ball_substeps() == 0U
            && short_pass_auxiliary.queue == ActorPresentationQueueKind::auxiliary
            && short_pass_auxiliary.requires_prequeue_presentation_refresh
            && short_pass_slot_zero.queue
                == ActorPresentationQueueKind::slot_zero_deferred
            && short_pass_primary.queue == ActorPresentationQueueKind::primary_low
            && actor_presentation_drain_order(short_pass.presentation_queues())
                == std::vector<std::uint8_t>{0x11U, 0x21U},
        "native short actor pass carries C606 queue state across completed handlers");

    const auto scheduled = scheduler.actors_due_for_update();
    check(
        scheduled.size() == 3U && scheduled[0].index == 0x02U
            && scheduled[0].handler_address == 0xC002U
            && scheduled[1].index == 0x03U
            && scheduled[1].handler_address == 0xC003U
            && scheduled[2].index == 0x05U
            && scheduled[2].handler_address == 0xC005U,
        "native actor scheduler selects due actors in table order");
    check(
        scheduler.actor_is_due_for_update(*state.actor(0x02U))
            && scheduler.actor_is_due_for_update(*state.actor(0x03U))
            && !scheduler.actor_is_due_for_update(*state.actor(0x04U))
            && scheduler.actor_is_due_for_update(*state.actor(0x05U)),
        "native actor scheduler exposes C510's single-slot eligibility gate");
    const auto first_scan_run = scheduler.scan_main_actor_run(0x00U);
    const auto deferred_scan_run = scheduler.scan_main_actor_run(0x04U);
    const auto terminal_scan_run = scheduler.scan_main_actor_run(0x06U);
    check(
        first_scan_run && first_scan_run->dispatch_actor
            && first_scan_run->dispatch_actor->index == 0x02U
            && first_scan_run->skipped_actors.size() == 2U
            && first_scan_run->skipped_actors[0].index == 0x00U
            && first_scan_run->skipped_actors[1].index == 0x01U
            && deferred_scan_run && deferred_scan_run->dispatch_actor
            && deferred_scan_run->dispatch_actor->index == 0x05U
            && deferred_scan_run->skipped_actors.size() == 1U
            && deferred_scan_run->skipped_actors[0].index == 0x04U
            && terminal_scan_run && terminal_scan_run->terminal()
            && terminal_scan_run->skipped_actors.size() == 122U
            && terminal_scan_run->skipped_actors.front().index == 0x06U
            && terminal_scan_run->skipped_actors.back().index == 0x7FU,
        "native actor scheduler captures C510's contiguous skip run and exit");

    MainActorPass main_pass(ActorPresentationQueueState{});
    const auto premature_second_run = first_scan_run
        ? main_pass.submit_scan_run(*first_scan_run)
            && !main_pass.submit_scan_run(*deferred_scan_run)
        : false;
    const auto rejected_main_completion = main_pass.complete_handler({
        .slot = 0x03U,
        .attributes_before_prequeue_presentation_refresh = 0x80U,
        .attributes_after_prequeue_presentation_refresh = 0x80U,
    });
    const auto first_main_completion = main_pass.complete_handler({
        .slot = 0x02U,
        .attributes_before_prequeue_presentation_refresh = 0x80U,
        .attributes_after_prequeue_presentation_refresh = 0x88U,
    });
    const auto second_main_run = deferred_scan_run
        ? main_pass.submit_scan_run(*deferred_scan_run)
        : false;
    const auto second_main_completion = main_pass.complete_handler({
        .slot = 0x05U,
        .attributes_before_prequeue_presentation_refresh = 0x82U,
        .attributes_after_prequeue_presentation_refresh = 0x84U,
    });
    const auto terminal_main_run = terminal_scan_run
        ? main_pass.submit_scan_run(*terminal_scan_run)
        : false;
    check(
        premature_second_run && !rejected_main_completion
            && first_main_completion
            && first_main_completion->queue
                == ActorPresentationQueueKind::primary_low
            && second_main_run && second_main_completion
            && second_main_completion->queue == ActorPresentationQueueKind::auxiliary
            && terminal_main_run && main_pass.terminal()
            && !main_pass.pending_actor()
            && actor_presentation_drain_order(main_pass.presentation_queues())
                == std::vector<std::uint8_t>{0x05U, 0x02U}
            && !main_pass.submit_scan_run(*first_scan_run),
        "native main actor pass preserves C510 scan boundaries through C606 filing");

    const auto before_handler_ram = work_ram;
    auto after_handler_ram = work_ram;
    after_handler_ram[0x1000U + 0x05U] = std::byte{0x81U};
    after_handler_ram[0x1080U + 0x05U] = std::byte{0x84U};
    after_handler_ram[0x1800U + 0x05U] = std::byte{0x01U};
    after_handler_ram[0x1C00U + 0x05U] = std::byte{0x02U};
    after_handler_ram[0x1F80U + 0x05U] = std::byte{0xC1U};
    after_handler_ram[0x1400U + 0x05U] = std::byte{0x03U};
    const GameStateView before_handler(before_handler_ram);
    const GameStateView after_handler(after_handler_ram);
    const auto captured_presentation = capture_actor_post_handler_presentation(
        before_handler, after_handler, 0x05U);
    const auto captured_handler_boundary =
        capture_scheduled_actor_handler_boundary(
            before_handler, after_handler, scheduled[2]);
    auto stale_scheduled_actor = scheduled[2];
    stale_scheduled_actor.handler_address = 0xC006U;
    const auto stale_handler_boundary =
        capture_scheduled_actor_handler_boundary(
            before_handler, after_handler, stale_scheduled_actor);
    const GameStateView truncated_after_handler(
        std::span<const std::byte>(after_handler_ram).first(
            after_handler_ram.size() - 1U));
    const auto behavior_changes = before_handler.actor(0x05U)
            && after_handler.actor(0x05U)
        ? capture_actor_behavior_state_changes(
              *before_handler.actor(0x05U), *after_handler.actor(0x05U))
        : std::nullopt;
    auto other_slot = after_handler.actor(0x05U);
    if (other_slot) {
        other_slot->index = 0x06U;
    }
    check(
        captured_presentation
            == ActorPostHandlerPresentation{
                .slot = 0x05U,
                .attributes_before_prequeue_presentation_refresh = 0x82U,
                .attributes_after_prequeue_presentation_refresh = 0x84U,
            }
            && !capture_actor_post_handler_presentation(
                before_handler, truncated_after_handler, 0x05U)
            && captured_handler_boundary
            && captured_handler_boundary->scheduled_actor == scheduled[2]
            && captured_handler_boundary->presentation == *captured_presentation
            && !stale_handler_boundary && behavior_changes
            && behavior_changes->activity_flags && behavior_changes->attributes
            && behavior_changes->behavior_counter
            && behavior_changes->handler_address
            && behavior_changes->horizontal_motion
            && behavior_changes->collision_state
            && !behavior_changes->vertical_motion
            && other_slot
            && !capture_actor_behavior_state_changes(
                *before_handler.actor(0x05U), *other_slot),
        "native C539 presentation capture binds snapshots to the scheduled handler record");

    NativeGameplayState native_behavior_gameplay;
    check(
        native_behavior_gameplay.actors().claim_dynamic_slot(
            0x05U, ActorBehavior::shared_motion),
        "native C539 behavior dispatcher accepts a seeded named actor");
    auto* native_behavior_actor = native_behavior_gameplay.actors().actor(0x05U);
    if (native_behavior_actor != nullptr) {
        native_behavior_actor->due_for_update = true;
        native_behavior_actor->motion = ActorMotionState{
            .position_x = Fixed24_8{.raw = 30 * 256},
            .velocity_x = 0x0100,
            .integrate_x = true,
        };
    }
    NativeGameplayActorBehaviorDispatcher native_behavior_dispatcher(
        native_behavior_gameplay);
    const auto unavailable_native_behavior = native_behavior_dispatcher.advance_actor(
        scheduled[0]);
    const auto native_behavior = native_behavior_dispatcher.advance_actor(scheduled[2]);
    const auto native_behavior_completion = native_behavior
        && captured_presentation
        ? join_native_gameplay_actor_behavior_presentation(
            *native_behavior, *captured_presentation)
        : std::nullopt;
    const auto boundary_native_behavior_completion = native_behavior
        && captured_handler_boundary
        ? join_native_gameplay_actor_behavior_boundary(
            *native_behavior, *captured_handler_boundary)
        : std::nullopt;
    auto drifted_handler_boundary = captured_handler_boundary;
    if (drifted_handler_boundary) {
        drifted_handler_boundary->scheduled_actor.handler_address = 0xC006U;
    }
    const auto stale_boundary_native_behavior_completion = native_behavior
        && drifted_handler_boundary
        ? join_native_gameplay_actor_behavior_boundary(
            *native_behavior, *drifted_handler_boundary)
        : std::nullopt;
    const auto mismatched_native_behavior_completion = native_behavior
        ? join_native_gameplay_actor_behavior_presentation(
            *native_behavior,
            {
                .slot = 0x04U,
                .attributes_before_prequeue_presentation_refresh = 0x00U,
                .attributes_after_prequeue_presentation_refresh = 0x00U,
            })
        : std::nullopt;
    check(
        !unavailable_native_behavior && native_behavior
            && native_behavior->actor == scheduled[2]
            && native_behavior->update.slot == 0x05U
            && native_behavior->update.kind
                == NativeGameplayState::ActorPhaseUpdateKind::shared_motion
            && native_behavior->update.motion
            && native_behavior->update.motion->step.state.position_x.raw == 31 * 256
            && native_behavior_completion
            && native_behavior_completion->presentation == *captured_presentation
            && boundary_native_behavior_completion
            && boundary_native_behavior_completion->presentation
                == captured_handler_boundary->presentation
            && !stale_boundary_native_behavior_completion
            && !mismatched_native_behavior_completion,
        "native C539 behavior dispatcher preserves scheduled actor identity at the capture join");

    auto completion_short_pass = scheduler.begin_short_actor_pass_session();
    const auto completion_prefix_first = completion_short_pass->complete_handler({
        .slot = 0x02U,
        .attributes_before_prequeue_presentation_refresh = 0x80U,
        .attributes_after_prequeue_presentation_refresh = 0x84U,
    });
    const auto completion_prefix_second = completion_short_pass->complete_handler({
        .slot = 0x03U,
        .attributes_before_prequeue_presentation_refresh = 0x80U,
        .attributes_after_prequeue_presentation_refresh = 0x88U,
    });
    auto malformed_native_behavior_completion = native_behavior_completion;
    if (malformed_native_behavior_completion) {
        malformed_native_behavior_completion->behavior.update.slot = 0x04U;
    }
    auto drifted_pass_actor_completion = native_behavior_completion;
    if (drifted_pass_actor_completion) {
        drifted_pass_actor_completion->behavior.actor.handler_address = 0xC006U;
    }
    const auto malformed_native_completion = malformed_native_behavior_completion
        ? completion_short_pass->complete_native_gameplay_behavior(
            *malformed_native_behavior_completion)
        : std::nullopt;
    const auto drifted_pass_actor_completion_step = drifted_pass_actor_completion
        ? completion_short_pass->complete_native_gameplay_behavior(
            *drifted_pass_actor_completion)
        : std::nullopt;
    const auto native_completion_step = native_behavior_completion
        ? completion_short_pass->complete_native_gameplay_behavior(
            *native_behavior_completion)
        : std::nullopt;
    check(
        completion_short_pass && completion_prefix_first && completion_prefix_second
            && !malformed_native_completion && !drifted_pass_actor_completion_step
            && native_completion_step
            && native_completion_step->slot == 0x05U
            && native_completion_step->queue == ActorPresentationQueueKind::auxiliary
            && completion_short_pass->complete(),
        "native C539 pass files only its current verified actor completion");

    const std::vector<devils_crush::native_runtime::NativeGameplayActorBehaviorCompletion>
        captured_native_completions = native_behavior_completion
        ? std::vector{*native_behavior_completion}
        : std::vector<devils_crush::native_runtime::NativeGameplayActorBehaviorCompletion>{};
    ShortActorPass replay_short_pass(
        *unfrozen_short_pass, {scheduled[2]});
    const auto replayed_completion =
        replay_short_pass.replay_native_gameplay_behavior_completions(
            captured_native_completions);
    auto surplus_native_behavior_completion = malformed_native_behavior_completion;
    if (surplus_native_behavior_completion && native_behavior_completion) {
        surplus_native_behavior_completion->behavior.actor = scheduled[2];
        surplus_native_behavior_completion->behavior.update.slot = 0x05U;
        surplus_native_behavior_completion->presentation = native_behavior_completion->presentation;
    }
    ShortActorPass surplus_replay_short_pass(
        *unfrozen_short_pass, {scheduled[2]});
    const auto surplus_replay =
        surplus_replay_short_pass.replay_native_gameplay_behavior_completions(
            native_behavior_completion && surplus_native_behavior_completion
                ? std::vector{
                    *native_behavior_completion,
                    *surplus_native_behavior_completion,
                }
                : std::vector<devils_crush::native_runtime::NativeGameplayActorBehaviorCompletion>{});
    ShortActorPass blocked_replay_short_pass(
        *unfrozen_short_pass, {scheduled[2]});
    const auto blocked_replay =
        blocked_replay_short_pass.replay_native_gameplay_behavior_completions(
            malformed_native_behavior_completion
                ? std::vector{*malformed_native_behavior_completion}
                : std::vector<devils_crush::native_runtime::NativeGameplayActorBehaviorCompletion>{});
    ShortActorPass awaiting_replay_short_pass(
        *unfrozen_short_pass, {scheduled[2]});
    const auto awaiting_replay =
        awaiting_replay_short_pass.replay_native_gameplay_behavior_completions({});
    auto first_actor_native_behavior_completion = native_behavior_completion;
    if (first_actor_native_behavior_completion) {
        first_actor_native_behavior_completion->behavior.actor = scheduled[0];
        first_actor_native_behavior_completion->behavior.update.slot = 0x02U;
        first_actor_native_behavior_completion->presentation = {
            .slot = 0x02U,
            .attributes_before_prequeue_presentation_refresh = 0x80U,
            .attributes_after_prequeue_presentation_refresh = 0x84U,
        };
    }
    ShortActorPass out_of_order_replay_short_pass(
        *unfrozen_short_pass, scheduled);
    const auto out_of_order_replay =
        out_of_order_replay_short_pass.replay_native_gameplay_behavior_completions(
            first_actor_native_behavior_completion && native_behavior_completion
                ? std::vector{
                    *first_actor_native_behavior_completion,
                    *native_behavior_completion,
                }
                : std::vector<devils_crush::native_runtime::NativeGameplayActorBehaviorCompletion>{});
    check(
        replayed_completion.pass_complete
            && replayed_completion.completed_handlers.size() == 1U
            && replayed_completion.completed_handlers.front().slot == 0x05U
            && replayed_completion.consumed_completions == 1U
            && replayed_completion.unconsumed_completions == 0U
            && replayed_completion.coherent()
            && !replayed_completion.blocked_actor
            && !replayed_completion.block_reason
            && !replayed_completion.rejected_completion
            && !replayed_completion.first_surplus_completion && replay_short_pass.complete()
            && surplus_replay.pass_complete
            && surplus_replay.completed_handlers.size() == 1U
            && surplus_replay.consumed_completions == 1U
            && surplus_replay.unconsumed_completions == 1U
            && surplus_replay.coherent()
            && surplus_native_behavior_completion
            && surplus_replay.first_surplus_completion
                == *surplus_native_behavior_completion
            && !surplus_replay.blocked_actor && !surplus_replay.block_reason
            && !surplus_replay.rejected_completion && surplus_replay_short_pass.complete()
            && !blocked_replay.pass_complete && blocked_replay.blocked_actor
            && blocked_replay.blocked_actor->index == 0x05U
            && blocked_replay.block_reason
                == devils_crush::native_runtime::ShortActorPassCompletionReplayBlockReason::rejected_completion
            && malformed_native_behavior_completion
            && blocked_replay.rejected_completion
                == *malformed_native_behavior_completion
            && !blocked_replay.first_surplus_completion
            && blocked_replay.consumed_completions == 0U
            && blocked_replay.unconsumed_completions == 1U
            && blocked_replay.coherent()
            && !blocked_replay_short_pass.complete()
            && !awaiting_replay.pass_complete && awaiting_replay.blocked_actor
            && awaiting_replay.blocked_actor->index == 0x05U
            && awaiting_replay.block_reason
                == devils_crush::native_runtime::ShortActorPassCompletionReplayBlockReason::awaiting_completion
            && !awaiting_replay.rejected_completion
            && !awaiting_replay.first_surplus_completion
            && awaiting_replay.consumed_completions == 0U
            && awaiting_replay.unconsumed_completions == 0U
            && awaiting_replay.coherent()
            && !awaiting_replay_short_pass.complete(),
        "native C539 completion replay distinguishes missing, rejected, and surplus captures");
    check(
        first_actor_native_behavior_completion && native_behavior_completion
            && !out_of_order_replay.pass_complete
            && out_of_order_replay.completed_handlers.size() == 1U
            && out_of_order_replay.completed_handlers.front().slot == 0x02U
            && out_of_order_replay.blocked_actor
            && out_of_order_replay.blocked_actor->index == 0x03U
            && out_of_order_replay.block_reason
                == devils_crush::native_runtime::ShortActorPassCompletionReplayBlockReason::rejected_completion
            && out_of_order_replay.rejected_completion == *native_behavior_completion
            && !out_of_order_replay.first_surplus_completion
            && out_of_order_replay.consumed_completions == 1U
            && out_of_order_replay.unconsumed_completions == 1U
            && out_of_order_replay.coherent()
            && !out_of_order_replay_short_pass.complete(),
        "native C539 completion replay never skips a missing middle actor");
    auto incoherent_replay = surplus_replay;
    incoherent_replay.pass_complete = false;
    check(
        !incoherent_replay.coherent(),
        "native C539 completion replay rejects contradictory terminal state");

    auto typed_short_pass = scheduler.begin_short_actor_pass_session();
    const auto out_of_order_completion = typed_short_pass->complete_handler({
        .slot = 0x03U,
        .attributes_before_prequeue_presentation_refresh = 0x80U,
        .attributes_after_prequeue_presentation_refresh = 0x88U,
    });
    const auto first_completion = typed_short_pass->complete_handler({
        .slot = 0x02U,
        .attributes_before_prequeue_presentation_refresh = 0x81U,
        .attributes_after_prequeue_presentation_refresh = 0x84U,
    });
    const auto second_completion = typed_short_pass->complete_handler({
        .slot = 0x03U,
        .attributes_before_prequeue_presentation_refresh = 0x80U,
        .attributes_after_prequeue_presentation_refresh = 0x88U,
    });
    const auto third_completion = typed_short_pass->complete_handler({
        .slot = 0x05U,
        .attributes_before_prequeue_presentation_refresh = 0x80U,
        .attributes_after_prequeue_presentation_refresh = 0x80U,
    });
    const auto duplicate_completion = typed_short_pass->complete_handler({
        .slot = 0x05U,
        .attributes_before_prequeue_presentation_refresh = 0x00U,
        .attributes_after_prequeue_presentation_refresh = 0x00U,
    });
    check(
        typed_short_pass && typed_short_pass->scheduled_actors() == scheduled
            && !out_of_order_completion && first_completion
            && second_completion && third_completion && !duplicate_completion
            && typed_short_pass->complete()
            && !typed_short_pass->next_actor()
            && actor_presentation_drain_order(
                   typed_short_pass->presentation_queues())
                == std::vector<std::uint8_t>{0x02U, 0x03U, 0x05U},
        "native short actor pass keeps C539 handler completions in table order");

    struct RecordingActorDispatcher final : ActorPassBehaviorDispatcher {
        std::vector<std::uint8_t> observed_slots;
        std::optional<std::uint8_t> unavailable_slot;
        bool return_wrong_slot = false;

        std::optional<ActorPostHandlerPresentation> advance_actor(
            const GameStateView::ActorSlot& actor) override
        {
            observed_slots.push_back(actor.index);
            if (unavailable_slot && actor.index == *unavailable_slot) {
                return std::nullopt;
            }
            return ActorPostHandlerPresentation{
                .slot = return_wrong_slot
                    ? static_cast<std::uint8_t>(actor.index + 1U)
                    : actor.index,
                .attributes_before_prequeue_presentation_refresh = 0x80U,
                .attributes_after_prequeue_presentation_refresh = actor.index == 0x02U
                    ? 0x84U
                    : actor.index == 0x03U ? 0x88U : 0x80U,
            };
        }
    };
    auto dispatched_short_pass = scheduler.begin_short_actor_pass_session();
    RecordingActorDispatcher dispatcher;
    dispatcher.unavailable_slot = static_cast<std::uint8_t>(0x03U);
    const auto dispatched_first = dispatched_short_pass->advance_next_actor(dispatcher);
    const auto unavailable_dispatch = dispatched_short_pass->advance_next_actor(dispatcher);
    dispatcher.unavailable_slot = std::nullopt;
    dispatcher.return_wrong_slot = true;
    const auto wrong_slot_dispatch = dispatched_short_pass->advance_next_actor(dispatcher);
    dispatcher.return_wrong_slot = false;
    const auto dispatched_second = dispatched_short_pass->advance_next_actor(dispatcher);
    const auto dispatched_third = dispatched_short_pass->advance_next_actor(dispatcher);
    const auto completed_dispatch = dispatched_short_pass->advance_next_actor(dispatcher);
    check(
        dispatched_short_pass && dispatched_first && !unavailable_dispatch
            && !wrong_slot_dispatch && dispatched_second && dispatched_third
            && !completed_dispatch && dispatched_short_pass->complete()
            && dispatcher.observed_slots
                == std::vector<std::uint8_t>{0x02U, 0x03U, 0x03U, 0x03U, 0x05U}
            && actor_presentation_drain_order(
                   dispatched_short_pass->presentation_queues())
                == std::vector<std::uint8_t>{0x02U, 0x03U, 0x05U},
        "native short actor pass advances only completed typed behavior results");

    auto prefix_short_pass = scheduler.begin_short_actor_pass_session();
    RecordingActorDispatcher prefix_dispatcher;
    prefix_dispatcher.unavailable_slot = static_cast<std::uint8_t>(0x03U);
    const auto blocked_prefix = prefix_short_pass->advance_available_prefix(
        prefix_dispatcher);
    prefix_dispatcher.unavailable_slot = std::nullopt;
    const auto completed_prefix = prefix_short_pass->advance_available_prefix(
        prefix_dispatcher);
    check(
        prefix_short_pass && blocked_prefix.completed_handlers.size() == 1U
            && blocked_prefix.completed_handlers.front().slot == 0x02U
            && blocked_prefix.blocked_actor
            && blocked_prefix.blocked_actor->index == 0x03U
            && !blocked_prefix.complete && completed_prefix.completed_handlers.size() == 2U
            && !completed_prefix.blocked_actor && completed_prefix.complete
            && prefix_short_pass->complete()
            && prefix_dispatcher.observed_slots
                == std::vector<std::uint8_t>{0x02U, 0x03U, 0x03U, 0x05U},
        "native short actor pass reports a contiguous typed-dispatch prefix");

    store_actor(0x03U, 0x00U, 0x80U, 0xC003U);
    store_actor(0x04U, 0x00U, 0x80U, 0xC004U);
    store_actor(0x05U, 0x80U, 0xC0U, 0xC005U);
    store_actor(0x06U, 0x00U, 0x40U, 0xC006U);
    store_actor(0x7FU, 0x00U, 0x80U, 0xC07FU);
    const auto retry_scan = scheduler.actors_for_collision_retry_full_scan();
    check(
        retry_scan.size() == 2U && retry_scan[0].index == 0x04U
            && retry_scan[1].index == 0x7FU,
        "native actor scheduler preserves the retry full-scan slot order and eligibility");

    work_ram[0x1300U + 0x04U] = std::byte{0x7FU};
    work_ram[0x1380U + 0x04U] = std::byte{0x01U};
    work_ram[0x1300U + 0x7FU] = std::byte{0x80U};
    work_ram[0x1380U + 0x7FU] = std::byte{0x01U};
    const auto vertical_candidates =
        scheduler.actors_for_collision_retry_vertical_scan(0x0100U);
    check(
        vertical_candidates.size() == 1U
            && vertical_candidates[0].actor.index == 0x04U
            && vertical_candidates[0].vertical_proximity.signed_difference
                == -127
            && vertical_candidates[0].vertical_proximity.intersecting,
        "native retry scan retains only vertically eligible actors in slot order");

    using ReservationKind = GameStateView::DynamicActorReservationKind;
    work_ram[0x0055U] = std::byte{0x00U};
    work_ram[0x1000U + 0x04U] = std::byte{0x01U};
    const auto standard_reservation = scheduler.next_dynamic_actor_reservation();
    check(
        standard_reservation && standard_reservation->slot == 0x06U
            && standard_reservation->kind == ReservationKind::standard,
        "native actor scheduler selects the first standard dynamic reservation");

    work_ram[0x0055U] = std::byte{0x01U};
    const auto alternate_reservation = scheduler.next_dynamic_actor_reservation();
    check(
        alternate_reservation && alternate_reservation->slot == 0x06U
            && alternate_reservation->kind == ReservationKind::alternate,
        "native actor scheduler preserves the alternate reservation mode");

    for (std::uint8_t slot = 0x04U; slot < 0x7CU; ++slot) {
        work_ram[0x1000U + slot] = std::byte{0x01U};
    }
    check(
        !scheduler.next_dynamic_actor_reservation(),
        "native actor scheduler reports an exhausted dynamic pool");

    const GameStateView truncated(
        std::span<const std::byte>(work_ram).first(work_ram.size() - 1U));
    check(
        ActorSchedulerView(truncated).actors_due_for_update().empty(),
        "native actor scheduler rejects a truncated state image");
    check(
        !ActorSchedulerView(truncated).begin_short_actor_pass(),
        "native short actor-pass seed rejects a truncated state image");
    check(
        ActorSchedulerView(truncated).actors_for_collision_retry_full_scan().empty(),
        "native retry full scan rejects a truncated state image");
    check(
        ActorSchedulerView(truncated)
            .actors_for_collision_retry_vertical_scan(0x0100U)
            .empty(),
        "native retry vertical scan rejects a truncated state image");

    ActorPresentationQueueState presentation_queues;
    const auto auxiliary_first = file_actor_for_presentation(
        presentation_queues, 0x11U, 0x84U, true);
    const auto auxiliary_second = file_actor_for_presentation(
        auxiliary_first.state, 0x12U, 0x84U);
    const auto primary_low_first = file_actor_for_presentation(
        auxiliary_second.state, 0x21U, 0x88U);
    const auto primary_low_second = file_actor_for_presentation(
        primary_low_first.state, 0x22U, 0x88U);
    const auto primary_high_first = file_actor_for_presentation(
        primary_low_second.state, 0x31U, 0x80U);
    const auto primary_high_second = file_actor_for_presentation(
        primary_high_first.state, 0x32U, 0x80U);
    const auto drained = actor_presentation_drain_order(primary_high_second.state);
    const auto hidden = file_actor_for_presentation(
        primary_high_second.state, 0x41U, 0xA0U);
    const auto not_presentation_eligible = file_completed_actor_for_presentation(
        hidden.state,
        ActorPostHandlerPresentation{
            .slot = 0x42U,
            .attributes_before_prequeue_presentation_refresh = 0x04U,
            .attributes_after_prequeue_presentation_refresh = 0x84U,
        });
    const auto slot_zero = file_actor_for_presentation(
        hidden.state, 0x00U, 0x00U, true);
    check(
        auxiliary_first.queue == ActorPresentationQueueKind::auxiliary
            && auxiliary_first.requires_prequeue_presentation_refresh
            && auxiliary_second.queue == ActorPresentationQueueKind::auxiliary
            && primary_low_first.queue == ActorPresentationQueueKind::primary_low
            && primary_low_second.queue == ActorPresentationQueueKind::primary_low
            && primary_high_first.queue == ActorPresentationQueueKind::primary_high
            && primary_high_second.queue == ActorPresentationQueueKind::primary_high
            && drained == std::vector<std::uint8_t>{
                0x12U, 0x11U, 0x22U, 0x21U, 0x31U, 0x32U}
            && hidden.queue == ActorPresentationQueueKind::hidden
            && hidden.state == primary_high_second.state
            && not_presentation_eligible.queue
                == ActorPresentationQueueKind::not_presentation_eligible
            && not_presentation_eligible.state == hidden.state
            && slot_zero.queue == ActorPresentationQueueKind::slot_zero_deferred
            && !slot_zero.requires_prequeue_presentation_refresh
            && slot_zero.state == hidden.state,
        "native actor scheduler preserves C606's post-handler SAT queue order and eligibility gate");

    ActorPresentationQueueState full_primary_queue;
    full_primary_queue.primary_low_count = ActorPresentationQueueState::kQueueCapacity;
    const auto full_primary = file_actor_for_presentation(
        full_primary_queue, 0x51U, 0x80U);
    ActorPresentationQueueState full_auxiliary_queue;
    full_auxiliary_queue.auxiliary_count = ActorPresentationQueueState::kQueueCapacity;
    const auto full_auxiliary = file_actor_for_presentation(
        full_auxiliary_queue, 0x52U, 0x84U);
    check(
        full_primary.queue == ActorPresentationQueueKind::capacity_exhausted
            && full_primary.state == full_primary_queue
            && full_auxiliary.queue == ActorPresentationQueueKind::capacity_exhausted
            && full_auxiliary.state == full_auxiliary_queue,
        "native actor scheduler keeps the SAT queue capacities explicit");

    const auto reset_then_hidden = file_completed_actor_for_presentation(
        {},
        ActorPostHandlerPresentation{
            .slot = 0x61U,
            .attributes_before_prequeue_presentation_refresh = 0x81U,
            .attributes_after_prequeue_presentation_refresh = 0xA0U,
        });
    const auto post_reset_bit_zero = file_completed_actor_for_presentation(
        {},
        ActorPostHandlerPresentation{
            .slot = 0x62U,
            .attributes_before_prequeue_presentation_refresh = 0x80U,
            .attributes_after_prequeue_presentation_refresh = 0x85U,
        });
    check(
        reset_then_hidden.requires_prequeue_presentation_refresh
            && reset_then_hidden.queue == ActorPresentationQueueKind::hidden
            && !post_reset_bit_zero.requires_prequeue_presentation_refresh
            && post_reset_bit_zero.queue == ActorPresentationQueueKind::auxiliary,
        "native actor scheduler preserves C606's before-refresh and reread attributes");

    // C606 stores its high primary queue from $7F downward. The typed capture
    // normalizes that physical direction into the same registration order the
    // readable filing model uses, so a live before/after C606 oracle need not
    // reason about source RAM cursors itself.
    work_ram[0x0CBCU] = std::byte{0x02U};
    work_ram[0x0CBAU] = std::byte{0x02U};
    work_ram[0x0CBBU] = std::byte{0x7DU};
    work_ram[0x0C3AU] = std::byte{0x3AU};
    work_ram[0x0C3BU] = std::byte{0x3BU};
    work_ram[0x0BBAU] = std::byte{0x11U};
    work_ram[0x0BBBU] = std::byte{0x12U};
    work_ram[0x0C39U] = std::byte{0x21U};
    work_ram[0x0C38U] = std::byte{0x22U};
    const auto queue_before = capture_actor_presentation_queue_state(state);
    work_ram[0x0BBCU] = std::byte{0x44U};
    work_ram[0x0CBAU] = std::byte{0x03U};
    const auto queue_after = capture_actor_presentation_queue_state(state);
    const auto queue_expected = queue_before
        ? file_completed_actor_for_presentation(
              *queue_before,
              ActorPostHandlerPresentation{
                  .slot = 0x44U,
                  .attributes_before_prequeue_presentation_refresh = 0x88U,
                  .attributes_after_prequeue_presentation_refresh = 0x88U,
              })
        : devils_crush::native_runtime::ActorPresentationQueueStep{};
    check(
        queue_before && queue_after
            && actor_presentation_drain_order(*queue_before)
                == std::vector<std::uint8_t>{0x3BU, 0x3AU, 0x12U, 0x11U, 0x21U, 0x22U}
            && queue_after->primary_low_count == 3U
            && queue_after->primary_low_slots[2] == 0x44U
            && queue_after == queue_expected.state,
        "native actor scheduler decodes C606's physical queues and predicts its filing");
    work_ram[0x0CBBU] = std::byte{0x80U};
    check(
        !capture_actor_presentation_queue_state(state),
        "native actor scheduler rejects an invalid C606 high-queue cursor");
}

void test_native_bonus_stage_4()
{
    using devils_crush::native_runtime::BonusStage4ObjectiveDisposition;
    using devils_crush::native_runtime::BonusStage4State;
    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::ScoreState;
    using devils_crush::native_runtime::capture_bonus_stage_4;
    using devils_crush::native_runtime::resolve_bonus_stage_4_objective_contact;

    BonusStage4State stage{};
    stage.objectives[0U].hits_remaining = 2U;
    stage.objectives[1U].hits_remaining = 1U;
    stage.objectives[2U].hits_remaining = 1U;
    const auto ordinary = resolve_bonus_stage_4_objective_contact(
        stage, ScoreState{}, 0U);
    const auto first_terminal = resolve_bonus_stage_4_objective_contact(
        ordinary.objective.state, ordinary.destruction_score_award.state, 0U);
    const auto second_terminal = resolve_bonus_stage_4_objective_contact(
        first_terminal.objective.state,
        first_terminal.destruction_score_award.state, 1U);
    const auto final_terminal = resolve_bonus_stage_4_objective_contact(
        second_terminal.objective.state,
        second_terminal.destruction_score_award.state, 2U);
    const auto repeated = resolve_bonus_stage_4_objective_contact(
        final_terminal.objective.state,
        final_terminal.destruction_score_award.state, 2U);
    const auto invalid = resolve_bonus_stage_4_objective_contact(
        final_terminal.objective.state,
        final_terminal.destruction_score_award.state, 3U);
    devils_crush::native_runtime::NativeGameplayState scheduled_gameplay;
    BonusStage4State scheduled_stage{};
    scheduled_stage.objectives[0U].hits_remaining = 1U;
    scheduled_gameplay.seed_bonus_stage_4(scheduled_stage);
    const auto scheduled_contact = scheduled_gameplay.confirm_actor_contact({
        .actor_slot = 0x34U,
        .response_kind = 2U,
    });
    const auto scheduled_phase = scheduled_gameplay.advance_actor_phase();
    const auto scheduled_audio = scheduled_gameplay.take_audio_events();
    std::array<std::byte, GameStateView::kWorkRamSize> work_ram{};
    constexpr std::array<std::uint8_t, 3U> objective_slots{0x34U, 0x52U, 0x56U};
    for (std::size_t index = 0U; index < objective_slots.size(); ++index) {
        const auto slot = objective_slots[index];
        work_ram[0x1000U + slot] = std::byte{0x82U};
        work_ram[0x1C00U + slot] = static_cast<std::byte>(8U - index);
        work_ram[0x1F00U + slot] = std::byte{0x46U};
        work_ram[0x1F80U + slot] = std::byte{0x6AU};
        work_ram[0x0D2FU + index] = std::byte{0x01U};
    }
    const auto captured = capture_bonus_stage_4(GameStateView(work_ram));
    work_ram[0x1834U] = std::byte{0x80U};
    const auto transitional_capture = capture_bonus_stage_4(GameStateView(work_ram));
    check(
        ordinary.objective.disposition
                == BonusStage4ObjectiveDisposition::survived_hit
            && ordinary.objective.state.objectives[0U].hits_remaining == 1U
            && ordinary.objective.hit_score_award_units == 100U
            && ordinary.objective.audio_command == 0x2EU
            && ordinary.destruction_score_award.awarded_units == 0U
            && first_terminal.objective.disposition
                == BonusStage4ObjectiveDisposition::destroyed
            && !first_terminal.objective.state.objectives[0U].active
            && first_terminal.objective.destruction_score_award_units == 1'000U
            && first_terminal.objective.requests_destruction_presentation
            && !second_terminal.objective.state.completion_requested
            && final_terminal.objective.state.completion_requested
            && final_terminal.objective.state.completion_multiplier == 20U
            && final_terminal.destruction_score_award.state.stored_units == 3'400U
            && repeated.objective.disposition
                == BonusStage4ObjectiveDisposition::ignored
            && repeated.objective.state == final_terminal.objective.state
            && invalid.objective.disposition
                == BonusStage4ObjectiveDisposition::ignored
            && scheduled_contact.bonus_stage_4_queued
            && scheduled_phase.size() == 1U
            && scheduled_phase.front().slot == 0x34U
            && scheduled_phase.front().kind
                == devils_crush::native_runtime::
                    NativeGameplayState::ActorPhaseUpdateKind::bonus_stage_4_objective
            && scheduled_phase.front().bonus_stage_4
            && scheduled_phase.front().bonus_stage_4->objective.disposition
                == BonusStage4ObjectiveDisposition::destroyed
            && scheduled_audio
                == std::vector<devils_crush::native_runtime::GameplayAudioEvent>{ {
                    .slot = 0x34U,
                    .source = devils_crush::native_runtime::GameplayAudioSource::bonus_stage_4,
                    .command = 0x2EU,
                } }
            && captured && captured->state.objectives[0U].hits_remaining == 8U
            && captured->state.objectives[1U].hits_remaining == 7U
            && captured->state.objectives[2U].hits_remaining == 6U
            && !transitional_capture,
        "native BS-4 objectives preserve capture, actor-order scoring, and multiplier-20 completion");
}

void test_native_bonus_stage_4_linked_target()
{
    using devils_crush::native_runtime::BonusStage4LinkedTargetAllocationDisposition;
    using devils_crush::native_runtime::BonusStage4LinkedTargetContactDisposition;
    using devils_crush::native_runtime::BonusStage4LinkedTargetHitPresentationDisposition;
    using devils_crush::native_runtime::BonusStage4LinkedTargetMotionDisposition;
    using devils_crush::native_runtime::BonusStage4LinkedTargetPosition;
    using devils_crush::native_runtime::BonusStage4OrbitingChildAllocationDisposition;
    using devils_crush::native_runtime::BonusStage4OrbitingChildFrameDisposition;
    using devils_crush::native_runtime::BonusStage4OrbitingChildOrbitDisposition;
    using devils_crush::native_runtime::ScoreState;
    using devils_crush::native_runtime::allocate_bonus_stage_4_linked_target;
    using devils_crush::native_runtime::allocate_bonus_stage_4_orbiting_child;
    using devils_crush::native_runtime::advance_bonus_stage_4_linked_target_motion;
    using devils_crush::native_runtime::advance_bonus_stage_4_orbiting_child_orbit;
    using devils_crush::native_runtime::advance_bonus_stage_4_orbiting_child_frame;
    using devils_crush::native_runtime::bonus_stage_4_orbit_position;
    using devils_crush::native_runtime::complete_bonus_stage_4_linked_target_hit_presentation;
    using devils_crush::native_runtime::resolve_bonus_stage_4_linked_target_contact;
    using devils_crush::native_runtime::resolve_bonus_stage_4_orbiting_child_contact;

    const auto allocation = allocate_bonus_stage_4_linked_target(
        0x35U, 0U, 0x66U,
        BonusStage4LinkedTargetPosition{
            .horizontal_pixels = 0xFFF8U,
            .vertical_pixels = 0xFFF8U,
        });
    const auto invalid_allocation = allocate_bonus_stage_4_linked_target(
        0x34U, 0U, 0x66U, {});
    const auto mismatched_allocation = allocate_bonus_stage_4_linked_target(
        0x53U, 0U, 0x66U, {});
    const auto target_hit = resolve_bonus_stage_4_linked_target_contact(
        *allocation.target, ScoreState{});
    const auto motion_refresh = advance_bonus_stage_4_linked_target_motion(
        *allocation.target);
    const auto motion_wait = advance_bonus_stage_4_linked_target_motion(
        motion_refresh.state);
    const auto repeated_target_hit = resolve_bonus_stage_4_linked_target_contact(
        target_hit.target.state, target_hit.score_award.state);
    const auto premature_child = allocate_bonus_stage_4_orbiting_child(
        target_hit.target.state, 0x67U);
    const auto completed_target_presentation =
        complete_bonus_stage_4_linked_target_hit_presentation(target_hit.target.state);
    const auto duplicate_presentation_completion =
        complete_bonus_stage_4_linked_target_hit_presentation(
            completed_target_presentation.state);
    const auto hit_during_child_handoff = resolve_bonus_stage_4_linked_target_contact(
        completed_target_presentation.state, target_hit.score_award.state);
    auto target = completed_target_presentation.state;
    const auto first_child = allocate_bonus_stage_4_orbiting_child(target, 0x67U);
    target = first_child.target;
    const auto second_child = allocate_bonus_stage_4_orbiting_child(target, 0x68U);
    target = second_child.target;
    const auto third_child = allocate_bonus_stage_4_orbiting_child(target, 0x69U);
    target = third_child.target;
    const auto fourth_child = allocate_bonus_stage_4_orbiting_child(target, 0x6AU);
    target = fourth_child.target;
    const auto exhausted_child = allocate_bonus_stage_4_orbiting_child(target, 0x6BU);
    const auto rearmed_target_hit = resolve_bonus_stage_4_linked_target_contact(
        target, target_hit.score_award.state);
    devils_crush::native_runtime::NativeGameplayState scheduled_gameplay;
    const bool scheduled_seeded = scheduled_gameplay.seed_bonus_stage_4_linked_target(
        *allocation.target);
    auto invalid_scheduled_target = *allocation.target;
    invalid_scheduled_target.dynamic_slot = 0x02U;
    devils_crush::native_runtime::NativeGameplayState invalid_scheduled_gameplay;
    const bool invalid_scheduled_seeded =
        invalid_scheduled_gameplay.seed_bonus_stage_4_linked_target(invalid_scheduled_target);
    const auto scheduled_contact = scheduled_gameplay.confirm_actor_contact({
        .actor_slot = allocation.target->dynamic_slot,
        .response_kind = 2U,
    });
    const auto scheduled_phase = scheduled_gameplay.advance_actor_phase();
    const auto scheduled_audio = scheduled_gameplay.take_audio_events();
    const auto scheduled_presentation =
        scheduled_gameplay.complete_bonus_stage_4_linked_target_hit_presentation();
    const auto scheduled_first_child =
        scheduled_gameplay.allocate_bonus_stage_4_orbiting_child(0x67U);
    const auto scheduled_second_child =
        scheduled_gameplay.allocate_bonus_stage_4_orbiting_child(0x68U);
    const auto scheduled_third_child =
        scheduled_gameplay.allocate_bonus_stage_4_orbiting_child(0x69U);
    const auto scheduled_fourth_child =
        scheduled_gameplay.allocate_bonus_stage_4_orbiting_child(0x6AU);
    const auto scheduled_motion_phase = scheduled_gameplay.advance_actor_phase();
    const auto scheduled_child_contact = scheduled_gameplay.confirm_actor_contact({
        .actor_slot = 0x67U,
        .response_kind = 2U,
    });
    const auto scheduled_child_phase = scheduled_gameplay.advance_actor_phase();
    const auto scheduled_child_audio = scheduled_gameplay.take_audio_events();
    const auto orbit_advance = advance_bonus_stage_4_orbiting_child_orbit(
        *first_child.child, allocation.target->position, true, false);
    const auto orbit_contact = advance_bonus_stage_4_orbiting_child_orbit(
        orbit_advance.state, allocation.target->position, true, true);
    const auto orbit_parent_inactive = advance_bonus_stage_4_orbiting_child_orbit(
        orbit_advance.state, allocation.target->position, false, false);
    const auto child_hit = resolve_bonus_stage_4_orbiting_child_contact(
        *first_child.child, target_hit.score_award.state);
    const auto clear_child_frame = advance_bonus_stage_4_orbiting_child_frame(
        *first_child.child, allocation.target->position, true, false, ScoreState{});
    const auto collided_child_frame = advance_bonus_stage_4_orbiting_child_frame(
        *first_child.child, allocation.target->position, true, true, ScoreState{});
    const auto parent_lost_child_frame = advance_bonus_stage_4_orbiting_child_frame(
        *first_child.child, allocation.target->position, false, false, ScoreState{});

    check(
        allocation.disposition == BonusStage4LinkedTargetAllocationDisposition::allocated
            && allocation.target
            && allocation.target->objective_index == 0U
            && allocation.target->source_slot == 0x35U
            && allocation.target->dynamic_slot == 0x66U
            && allocation.target->position.horizontal_pixels == 0x0008U
            && allocation.target->position.vertical_pixels == 0x0008U
            && allocation.target->ticks_until_motion_refresh == 1U
            && invalid_allocation.disposition
                == BonusStage4LinkedTargetAllocationDisposition::invalid_source
            && mismatched_allocation.disposition
                == BonusStage4LinkedTargetAllocationDisposition::invalid_source,
        "native BS-4 paired source allocation retains linkage and wrapped offset");
    check(
        motion_refresh.disposition
                == BonusStage4LinkedTargetMotionDisposition::random_motion_requested
            && motion_refresh.state.ticks_until_motion_refresh == 32U
            && motion_wait.disposition == BonusStage4LinkedTargetMotionDisposition::waiting
            && motion_wait.state.ticks_until_motion_refresh == 31U,
        "native BS-4 linked-target motion refresh preserves the recovered 32-tick cadence");
    check(
        target_hit.target.disposition
                == BonusStage4LinkedTargetContactDisposition::accepted
            && target_hit.target.score_award_units == 100U
            && target_hit.target.audio_command == 0x3BU
            && !target_hit.target.requests_orbiting_children
            && target_hit.target.state.hit_presentation_pending
            && target_hit.target.state.orbiting_children_pending == 0U
            && target_hit.score_award.state.stored_units == 100U
            && repeated_target_hit.target.disposition
                == BonusStage4LinkedTargetContactDisposition::ignored
            && premature_child.disposition
                == BonusStage4OrbitingChildAllocationDisposition::no_pending_child,
        "native BS-4 linked target defers its child handoff behind the recovered presentation");
    check(
        completed_target_presentation.disposition
                == BonusStage4LinkedTargetHitPresentationDisposition::child_allocation_requested
            && completed_target_presentation.requests_orbiting_children
            && !completed_target_presentation.state.hit_presentation_pending
            && completed_target_presentation.state.orbiting_children_pending == 4U
            && duplicate_presentation_completion.disposition
                == BonusStage4LinkedTargetHitPresentationDisposition::no_pending_presentation
            && hit_during_child_handoff.target.disposition
                == BonusStage4LinkedTargetContactDisposition::ignored,
        "native BS-4 resumed hit continuation requests four children behind its collision latch");
    check(
        first_child.disposition == BonusStage4OrbitingChildAllocationDisposition::allocated
            && first_child.child && first_child.child->target_slot == 0x66U
            && first_child.child->orbit_phase == 24U
            && first_child.child->position
                == BonusStage4LinkedTargetPosition{
                    .horizontal_pixels = 0x0010U,
                    .vertical_pixels = 0x0000U,
                }
            && second_child.disposition == BonusStage4OrbitingChildAllocationDisposition::allocated
            && second_child.child && second_child.child->orbit_phase == 16U
            && third_child.disposition == BonusStage4OrbitingChildAllocationDisposition::allocated
            && third_child.child && third_child.child->orbit_phase == 8U
            && fourth_child.disposition == BonusStage4OrbitingChildAllocationDisposition::allocated
            && fourth_child.child && fourth_child.child->orbit_phase == 0U
            && fourth_child.target.orbiting_children_pending == 0U
            && exhausted_child.disposition
                == BonusStage4OrbitingChildAllocationDisposition::no_pending_child
            && rearmed_target_hit.target.disposition
                == BonusStage4LinkedTargetContactDisposition::accepted
            && child_hit.child.releases_child
            && child_hit.child.score_award_units == 100U
            && child_hit.score_award.state.stored_units == 200U,
        "native BS-4 target rearms only after four allocator-selected children finish");
    check(
        scheduled_seeded && !invalid_scheduled_seeded
            && scheduled_contact.bonus_stage_4_linked_target_queued
            && scheduled_phase.size() == 1U
            && scheduled_phase.front().slot == 0x66U
            && scheduled_phase.front().kind
                == devils_crush::native_runtime::
                    NativeGameplayState::ActorPhaseUpdateKind::bonus_stage_4_linked_target
            && scheduled_phase.front().bonus_stage_4_linked_target
            && scheduled_phase.front().bonus_stage_4_linked_target->target.disposition
                == BonusStage4LinkedTargetContactDisposition::accepted
            && scheduled_audio
                == std::vector<devils_crush::native_runtime::GameplayAudioEvent>{ {
                    .slot = 0x66U,
                    .source = devils_crush::native_runtime::GameplayAudioSource::bonus_stage_4,
                    .command = 0x3BU,
                } }
            && scheduled_presentation
            && scheduled_presentation->requests_orbiting_children
            && scheduled_first_child && scheduled_first_child->child
            && scheduled_first_child->child->orbit_phase == 24U
            && scheduled_second_child && scheduled_third_child && scheduled_fourth_child
            && scheduled_motion_phase.size() == 5U
            && scheduled_motion_phase.front().slot == 0x66U
            && scheduled_motion_phase.front().kind
                == devils_crush::native_runtime::NativeGameplayState::ActorPhaseUpdateKind::
                    bonus_stage_4_linked_target_motion
            && scheduled_motion_phase.front().bonus_stage_4_linked_target_motion
            && scheduled_motion_phase.front().bonus_stage_4_linked_target_motion->disposition
                == BonusStage4LinkedTargetMotionDisposition::random_motion_requested
            && scheduled_motion_phase[1U].slot == 0x67U
            && scheduled_motion_phase[1U].kind
                == devils_crush::native_runtime::NativeGameplayState::ActorPhaseUpdateKind::
                    bonus_stage_4_orbiting_child_orbit
            && scheduled_motion_phase[1U].bonus_stage_4_orbiting_child
            && scheduled_motion_phase[1U].bonus_stage_4_orbiting_child->child.state
                .orbit_phase == 25U
            && scheduled_child_contact.bonus_stage_4_orbiting_child_queued
            && scheduled_child_phase.size() == 5U
            && scheduled_child_phase[1U].slot == 0x67U
            && scheduled_child_phase[1U].kind
                == devils_crush::native_runtime::NativeGameplayState::ActorPhaseUpdateKind::
                    bonus_stage_4_orbiting_child
            && scheduled_child_phase[1U].bonus_stage_4_orbiting_child
            && scheduled_child_phase[1U].bonus_stage_4_orbiting_child->child.releases_child
            && scheduled_child_phase[1U].bonus_stage_4_orbiting_child->child.score_award_units
                == 100U
            && scheduled_child_audio.empty()
            && !scheduled_gameplay.bonus_stage_4_orbiting_child(0x67U)
            && scheduled_gameplay.bonus_stage_4_linked_target()
            && scheduled_gameplay.bonus_stage_4_linked_target()
                ->ticks_until_motion_refresh == 31U
            && scheduled_gameplay.score().stored_units == 200U,
        "native gameplay owns the BS-4 linked target, child release, and motion cadence");
    check(
        orbit_advance.disposition == BonusStage4OrbitingChildOrbitDisposition::advanced
            && orbit_advance.state.orbit_phase == 25U
            && orbit_advance.state.position
                == BonusStage4LinkedTargetPosition{
                    .horizontal_pixels = 0x0013U,
                    .vertical_pixels = 0x0001U,
                }
            && orbit_contact.disposition
                == BonusStage4OrbitingChildOrbitDisposition::collision_pending
            && orbit_parent_inactive.disposition
                == BonusStage4OrbitingChildOrbitDisposition::parent_inactive,
        "native BS-4 orbiting child preserves the clear modulo-32 orbit phase boundary");
    check(
        clear_child_frame.child.disposition
                == BonusStage4OrbitingChildFrameDisposition::advanced
            && clear_child_frame.child.state == orbit_advance.state
            && clear_child_frame.score_award.state.stored_units == 0U
            && collided_child_frame.child.disposition
                == BonusStage4OrbitingChildFrameDisposition::released_after_collision
            && collided_child_frame.child.releases_child
            && collided_child_frame.child.score_award_units == 100U
            && collided_child_frame.score_award.state.stored_units == 100U
            && parent_lost_child_frame.child.disposition
                == BonusStage4OrbitingChildFrameDisposition::released_after_parent_loss
            && parent_lost_child_frame.child.releases_child
            && parent_lost_child_frame.score_award.state.stored_units == 100U,
        "native BS-4 child frame sends parent loss and collision through the shared release");

    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::capture_bonus_stage_4_linked_target;
    std::array<std::byte, GameStateView::kWorkRamSize> work_ram{};
    constexpr std::uint8_t target_slot = 0x66U;
    constexpr std::uint8_t source_slot = 0x35U;
    work_ram[0x1000U + target_slot] = std::byte{0xC2U};
    work_ram[0x1F00U + target_slot] = std::byte{0x7EU};
    work_ram[0x1F80U + target_slot] = std::byte{0x6BU};
    work_ram[0x1E00U + target_slot] = std::byte{0x00U};
    work_ram[0x1E80U + target_slot] = static_cast<std::byte>(source_slot);
    work_ram[0x1180U + target_slot] = std::byte{0x60U};
    work_ram[0x1200U + target_slot] = std::byte{0x00U};
    work_ram[0x1300U + target_slot] = std::byte{0x48U};
    work_ram[0x1380U + target_slot] = std::byte{0x00U};
    work_ram[0x1B00U + target_slot] = std::byte{0x20U};
    work_ram[0x1C80U + source_slot] = std::byte{0xFFU};
    const auto captured = capture_bonus_stage_4_linked_target(GameStateView(work_ram));
    work_ram[0x1F00U + target_slot] = std::byte{0x21U};
    work_ram[0x1F80U + target_slot] = std::byte{0x6CU};
    work_ram[0x1980U + target_slot] = std::byte{0x04U};
    work_ram[0x1800U + target_slot] = std::byte{0x80U};
    const auto captured_hit_presentation = capture_bonus_stage_4_linked_target(
        GameStateView(work_ram));
    work_ram[0x1980U + target_slot] = std::byte{0x03U};
    const auto mismatched_hit_presentation = capture_bonus_stage_4_linked_target(
        GameStateView(work_ram));
    work_ram[0x1F00U + target_slot] = std::byte{0x7EU};
    work_ram[0x1F80U + target_slot] = std::byte{0x6BU};
    work_ram[0x1980U + target_slot] = std::byte{0x00U};
    work_ram[0x1800U + target_slot] = std::byte{0x00U};
    work_ram[0x1C80U + source_slot] = std::byte{0x00U};
    const auto unlinked_capture = capture_bonus_stage_4_linked_target(
        GameStateView(work_ram));
    check(
        captured && captured->state.dynamic_slot == target_slot
            && captured->state.source_slot == source_slot
            && captured->state.objective_index == 0U
            && captured->state.ticks_until_motion_refresh == 0x20U
            && captured->state.position
                == BonusStage4LinkedTargetPosition{
                    .horizontal_pixels = 0x0060U,
                    .vertical_pixels = 0x0048U,
                }
            && captured_hit_presentation
            && captured_hit_presentation->state.hit_presentation_pending
            && !mismatched_hit_presentation
            && !unlinked_capture,
        "native BS-4 capture accepts only proven steady or hit-presentation targets");

    using devils_crush::native_runtime::capture_bonus_stage_4_orbiting_child;
    using devils_crush::native_runtime::capture_bonus_stage_4_linked_target_generation;
    using devils_crush::native_runtime::apply_native_gameplay_bootstrap;
    using devils_crush::native_runtime::capture_native_gameplay_bootstrap;
    constexpr std::uint8_t child_slot = 0x67U;
    work_ram.fill(std::byte{});
    work_ram[0x1000U + target_slot] = std::byte{0xC2U};
    work_ram[0x1F00U + target_slot] = std::byte{0xA0U};
    work_ram[0x1F80U + target_slot] = std::byte{0x6CU};
    work_ram[0x1180U + target_slot] = std::byte{0x60U};
    work_ram[0x1200U + target_slot] = std::byte{0x00U};
    work_ram[0x1300U + target_slot] = std::byte{0x48U};
    work_ram[0x1380U + target_slot] = std::byte{0x00U};
    work_ram[0x1E00U + target_slot] = std::byte{0x00U};
    work_ram[0x1E80U + target_slot] = std::byte{source_slot};
    work_ram[0x1B00U + target_slot] = std::byte{0x20U};
    work_ram[0x1C80U + source_slot] = std::byte{0xFFU};
    work_ram[0x1000U + child_slot] = std::byte{0xC2U};
    work_ram[0x1F00U + child_slot] = std::byte{0x27U};
    work_ram[0x1F80U + child_slot] = std::byte{0x6DU};
    work_ram[0x1E00U + child_slot] = std::byte{24U};
    work_ram[0x1E80U + child_slot] = std::byte{target_slot};
    const auto expected_child_position = bonus_stage_4_orbit_position(
        BonusStage4LinkedTargetPosition{
            .horizontal_pixels = 0x0060U,
            .vertical_pixels = 0x0048U,
        },
        24U);
    work_ram[0x1180U + child_slot] = static_cast<std::byte>(
        expected_child_position.horizontal_pixels & 0x00FFU);
    work_ram[0x1200U + child_slot] = static_cast<std::byte>(
        expected_child_position.horizontal_pixels >> 8U);
    work_ram[0x1300U + child_slot] = static_cast<std::byte>(
        expected_child_position.vertical_pixels & 0x00FFU);
    work_ram[0x1380U + child_slot] = static_cast<std::byte>(
        expected_child_position.vertical_pixels >> 8U);
    const auto captured_child = capture_bonus_stage_4_orbiting_child(
        GameStateView(work_ram));
    constexpr std::uint8_t second_child_slot = 0x68U;
    constexpr std::uint8_t second_child_phase = 8U;
    const auto expected_second_child_position = bonus_stage_4_orbit_position(
        BonusStage4LinkedTargetPosition{
            .horizontal_pixels = 0x0060U,
            .vertical_pixels = 0x0048U,
        },
        second_child_phase);
    work_ram[0x1000U + second_child_slot] = std::byte{0xC2U};
    work_ram[0x1F00U + second_child_slot] = std::byte{0x27U};
    work_ram[0x1F80U + second_child_slot] = std::byte{0x6DU};
    work_ram[0x1E00U + second_child_slot] = std::byte{second_child_phase};
    work_ram[0x1E80U + second_child_slot] = std::byte{target_slot};
    work_ram[0x1180U + second_child_slot] = static_cast<std::byte>(
        expected_second_child_position.horizontal_pixels & 0x00FFU);
    work_ram[0x1200U + second_child_slot] = static_cast<std::byte>(
        expected_second_child_position.horizontal_pixels >> 8U);
    work_ram[0x1300U + second_child_slot] = static_cast<std::byte>(
        expected_second_child_position.vertical_pixels & 0x00FFU);
    work_ram[0x1380U + second_child_slot] = static_cast<std::byte>(
        expected_second_child_position.vertical_pixels >> 8U);
    const auto captured_generation = capture_bonus_stage_4_linked_target_generation(
        GameStateView(work_ram));
    devils_crush::native_runtime::NativeGameplayState captured_generation_gameplay;
    const auto captured_generation_seeded = captured_generation
        && captured_generation_gameplay.seed_bonus_stage_4_linked_target(
            captured_generation->target.state,
            {captured_generation->children[0U]
                    ? std::optional<devils_crush::native_runtime::BonusStage4OrbitingChildState>{
                        captured_generation->children[0U]->state}
                    : std::nullopt,
                captured_generation->children[1U]
                    ? std::optional<devils_crush::native_runtime::BonusStage4OrbitingChildState>{
                        captured_generation->children[1U]->state}
                    : std::nullopt,
                captured_generation->children[2U]
                    ? std::optional<devils_crush::native_runtime::BonusStage4OrbitingChildState>{
                        captured_generation->children[2U]->state}
                    : std::nullopt,
                captured_generation->children[3U]
                    ? std::optional<devils_crush::native_runtime::BonusStage4OrbitingChildState>{
                        captured_generation->children[3U]->state}
                    : std::nullopt});
    const auto captured_generation_phase = captured_generation_gameplay.advance_actor_phase();
    work_ram[0x1000U] = std::byte{0x81U};
    const auto captured_bootstrap = capture_native_gameplay_bootstrap(GameStateView(work_ram));
    devils_crush::native_runtime::NativeGameplayState bootstrapped_generation_gameplay;
    const auto captured_bootstrap_applied = captured_bootstrap
        && apply_native_gameplay_bootstrap(
            bootstrapped_generation_gameplay, *captured_bootstrap);
    work_ram[0x1180U + child_slot] = std::byte{0x00U};
    const auto displaced_child = capture_bonus_stage_4_orbiting_child(
        GameStateView(work_ram));
    const auto displaced_generation = capture_bonus_stage_4_linked_target_generation(
        GameStateView(work_ram));
    check(
        captured_child && captured_child->state.dynamic_slot == child_slot
            && captured_child->state.target_slot == target_slot
            && captured_child->state.orbit_phase == 24U
            && captured_child->state.position == expected_child_position
            && captured_generation && captured_generation->children[0U]
            && captured_generation->children[1U]
            && captured_generation->children[0U]->state.dynamic_slot == child_slot
            && captured_generation->children[1U]->state.dynamic_slot == second_child_slot
            && captured_generation_seeded
            && captured_generation_gameplay.bonus_stage_4_orbiting_child(child_slot)
            && captured_generation_gameplay.bonus_stage_4_orbiting_child(second_child_slot)
            && captured_generation_phase.size() == 3U
            && captured_bootstrap_applied
            && captured_bootstrap->bonus_stage_4_linked_target == captured_generation
            && bootstrapped_generation_gameplay.bonus_stage_4_linked_target()
            && bootstrapped_generation_gameplay.bonus_stage_4_orbiting_child(child_slot)
            && bootstrapped_generation_gameplay.bonus_stage_4_orbiting_child(second_child_slot)
            && !displaced_child && !displaced_generation,
        "native BS-4 generation capture and bootstrap restore each proven clear child orbit state");
}

void test_native_actor_motion()
{
    using devils_crush::native_runtime::ActorMotionState;
    using devils_crush::native_runtime::Fixed24_8;
    using devils_crush::native_runtime::PlayfieldBounds;
    using devils_crush::native_runtime::advance_actor_motion;

    ActorMotionState state{
        .position_x = Fixed24_8{.raw = 10 * 256 + 128},
        .position_y = Fixed24_8{.raw = 20 * 256 + 64},
        .velocity_x = 384,
        .velocity_y = -320,
        .target_x = 20,
        .target_y = 10,
        .velocity_adjustment_x = 16,
        .velocity_adjustment_y = 8,
        .integrate_x = true,
        .integrate_y = true,
        .steer_x = true,
        .steer_y = true,
    };
    const auto moved = advance_actor_motion(state);
    check(
        moved.state.position_x.raw == 12 * 256
            && moved.state.position_y.raw == 19 * 256,
        "native actor motion integrates signed fractional coordinates");
    check(
        moved.state.velocity_x == 400 && moved.state.velocity_y == -328,
        "native actor motion steers velocity after moving toward each target");
    check(!moved.left_playfield, "native actor motion keeps in-bounds actors live");

    state.position_x = Fixed24_8{.raw = 20 * 256};
    state.position_y = Fixed24_8{.raw = 10 * 256};
    state.velocity_x = 123;
    state.velocity_y = -456;
    state.integrate_x = false;
    state.integrate_y = false;
    const auto on_target = advance_actor_motion(state);
    check(
        on_target.state.velocity_x == 123 && on_target.state.velocity_y == -456,
        "native actor motion does not steer an axis already at its target");

    state.integrate_x = true;
    state.steer_y = false;
    state.position_x = Fixed24_8{.raw = 319 * 256 + 200};
    state.velocity_x = 100;
    const auto departed = advance_actor_motion(
        state, PlayfieldBounds{.width = 320, .height = 720});
    check(
        departed.left_playfield,
        "native actor motion reports a departure after an enabled axis crosses a bound");

    state.position_x = Fixed24_8{.raw = 0};
    state.velocity_x = -1;
    const auto fractional_departure = advance_actor_motion(state);
    check(
        fractional_departure.left_playfield,
        "native actor motion treats a negative fractional position as departed");

    state.position_x = Fixed24_8{.raw = 0x007FFFFF};
    state.velocity_x = 1;
    const auto position_wrapped = advance_actor_motion(state);
    check(
        position_wrapped.state.position_x.raw == -0x00800000,
        "native actor motion preserves the game's signed 24-bit position wrap");

    state.integrate_x = false;
    state.steer_x = true;
    state.position_x = Fixed24_8{.raw = 40 * 256};
    state.target_x = 20;
    state.velocity_x = -32768;
    state.velocity_adjustment_x = 1;
    const auto wrapped = advance_actor_motion(state);
    check(
        wrapped.state.velocity_x == 32767,
        "native actor motion preserves the game's signed 16-bit velocity wrap");
}

void test_native_actor_state()
{
    using devils_crush::native_runtime::ActorBehavior;
    using devils_crush::native_runtime::ActorState;
    using devils_crush::native_runtime::Fixed24_8;

    ActorState actors;
    check(
        actors.actor(0x04U) && !actors.actor(0x04U)->active(),
        "native actor state initializes every record as inactive");
    check(
        !actors.claim_dynamic_slot(0x03U, ActorBehavior::shared_motion)
            && !actors.claim_dynamic_slot(0x7CU, ActorBehavior::shared_motion),
        "native actor state protects fixed and out-of-range dynamic slots");
    check(
        actors.claim_dynamic_slot(0x04U, ActorBehavior::shared_motion)
            && !actors.claim_dynamic_slot(0x04U, ActorBehavior::shared_motion),
        "native actor state owns each dynamic-slot claim exactly once");

    auto* actor = actors.actor(0x04U);
    check(actor != nullptr, "native actor state retrieves an active record");
    if (actor != nullptr) {
        actor->due_for_update = true;
        actor->motion = {
            .position_x = Fixed24_8{.raw = 10 * 256},
            .position_y = Fixed24_8{.raw = 20 * 256},
            .velocity_x = 128,
            .velocity_y = -256,
            .integrate_x = true,
            .integrate_y = true,
        };
    }
    const auto updates = actors.advance_due_actors();
    check(
        updates.size() == 1U && updates.front().slot == 0x04U
            && updates.front().step.state.position_x.raw == 10 * 256 + 128
            && updates.front().step.state.position_y.raw == 19 * 256,
        "native actor state applies shared motion to due actors in place");
    check(
        actor && actor->motion == updates.front().step.state,
        "native actor state retains the behavior result as authoritative state");

    actors.release(0x04U);
    check(
        actor && !actor->active() && !actor->due_for_update
            && actors.claim_dynamic_slot(0x04U, ActorBehavior::shared_motion),
        "native actor state releases a dynamic slot for later reuse");
}

void test_native_collision_effect()
{
    using devils_crush::native_runtime::ActorBehavior;
    using devils_crush::native_runtime::ActorState;
    using devils_crush::native_runtime::CollisionEffectAdvanceDisposition;
    using devils_crush::native_runtime::CollisionEffectAllocationDisposition;
    using devils_crush::native_runtime::advance_collision_effect;

    const auto invalid = advance_collision_effect({.ticks_remaining = 0U});
    check(
        invalid.disposition == CollisionEffectAdvanceDisposition::invalid_state,
        "native collision effect rejects an uninitialized lifetime");

    ActorState actors;
    check(
        actors.claim_dynamic_slot(0x04U, ActorBehavior::shared_motion)
            && actors.set_actor_appearance(0x04U, 0x5AU),
        "native collision effect has an active source with a captured appearance");
    const auto allocated = actors.claim_collision_effect(0x04U, 0x05U);
    const auto duplicate = actors.claim_collision_effect(0x04U, 0x06U);
    const auto* effect = actors.actor(0x05U);
    check(
        allocated.disposition == CollisionEffectAllocationDisposition::allocated
            && allocated.effect
            && allocated.effect->source_slot == 0x04U
            && allocated.effect->source_association == 0U
            && allocated.effect->source_appearance == 0x5AU
            && effect && effect->behavior == ActorBehavior::collision_effect
            && effect->collision_effect && effect->due_for_update
            && duplicate.disposition
                == CollisionEffectAllocationDisposition::source_already_has_effect,
        "native collision-effect allocator creates one eight-turn child per source");

    std::optional<devils_crush::native_runtime::ActorCollisionEffectUpdate> update;
    for (std::uint8_t tick = 0U; tick < 8U; ++tick) {
        update = actors.advance_collision_effect(0x05U);
    }
    const auto* released = actors.actor(0x05U);
    const auto* source = actors.actor(0x04U);
    check(
        update && update->step.disposition
                == CollisionEffectAdvanceDisposition::expired
            && released && !released->active() && source
            && !source->collision_effect_active
            && source->collision_effect_association == 0U,
        "native collision-effect lifetime releases its child and source association");

    const auto old_effect = actors.claim_collision_effect(0x04U, 0x05U);
    actors.release(0x04U);
    const bool source_reused = actors.claim_dynamic_slot(
        0x04U, ActorBehavior::shared_motion);
    const auto new_effect = actors.claim_collision_effect(0x04U, 0x06U);
    for (std::uint8_t tick = 0U; tick < 8U; ++tick) {
        static_cast<void>(actors.advance_collision_effect(0x05U));
    }
    source = actors.actor(0x04U);
    const bool new_association_survived_old_cleanup = source != nullptr
        && source->collision_effect_active;
    for (std::uint8_t tick = 0U; tick < 8U; ++tick) {
        static_cast<void>(actors.advance_collision_effect(0x06U));
    }
    source = actors.actor(0x04U);
    check(
        old_effect.disposition == CollisionEffectAllocationDisposition::allocated
            && source_reused
            && new_effect.disposition == CollisionEffectAllocationDisposition::allocated
            && new_association_survived_old_cleanup && source
            && !source->collision_effect_active,
        "native collision-effect cleanup cannot clear a reused source slot");

    devils_crush::native_runtime::NativeGameplayState gameplay;
    const bool phase_source_claimed = gameplay.actors().claim_dynamic_slot(
        0x04U, ActorBehavior::shared_motion);
    const auto phase_effect = gameplay.claim_collision_effect(0x04U, 0x05U);
    const auto phase_updates = gameplay.advance_actor_phase();
    check(
        phase_source_claimed
            && phase_effect.disposition
                == CollisionEffectAllocationDisposition::allocated
            && phase_updates.size() == 1U
            && phase_updates.front().kind
                == devils_crush::native_runtime::NativeGameplayState::ActorPhaseUpdateKind::collision_effect
            && phase_updates.front().collision_effect
            && phase_updates.front().collision_effect->step.state.ticks_remaining == 7U,
        "native actor phase advances a claimed collision-effect child");

    std::array<std::byte, devils_crush::native_runtime::GameStateView::kWorkRamSize>
        work_ram{};
    const auto store = [&work_ram](
                           const std::size_t offset,
                           const std::uint8_t value) {
        work_ram[offset] = static_cast<std::byte>(value);
    };
    constexpr std::uint8_t captured_source_slot = 0x2CU;
    constexpr std::uint8_t captured_effect_slot = 0x64U;
    store(0x1000U + captured_source_slot, 0x82U);
    store(0x1900U + captured_source_slot, 0x66U);
    store(0x1D00U + captured_source_slot, 0x8FU);
    store(0x1000U + captured_effect_slot, 0x8FU);
    store(0x1080U + captured_effect_slot, 0xE0U);
    store(0x1900U + captured_effect_slot, 0x66U);
    store(0x1D80U + captured_effect_slot, 4U);
    store(0x1E00U + captured_effect_slot, captured_source_slot);
    store(0x1E80U + captured_effect_slot, 0x80U);
    store(0x1F00U + captured_effect_slot, 0x34U);
    store(0x1F80U + captured_effect_slot, 0x7CU);
    const devils_crush::native_runtime::GameStateView captured_state(work_ram);
    const auto captured = devils_crush::native_runtime::capture_collision_effect(
        captured_state, captured_effect_slot);
    devils_crush::native_runtime::NativeGameplayState seeded_gameplay;
    const bool captured_source_claimed = seeded_gameplay.actors().claim_dynamic_slot(
        captured_source_slot, ActorBehavior::shared_motion);
    const bool captured_appearance_seeded = seeded_gameplay.actors().set_actor_appearance(
        captured_source_slot, 0x66U);
    const bool captured_association_seeded = seeded_gameplay.actors()
        .set_collision_effect_association(captured_source_slot, 0x8FU);
    const bool captured_effect_seeded = captured && seeded_gameplay.seed_collision_effect(
        captured_effect_slot, captured->effect);
    const auto* seeded_effect = seeded_gameplay.actors().actor(captured_effect_slot);
    store(0x1E80U + captured_effect_slot, 0x70U);
    const devils_crush::native_runtime::GameStateView invalid_capture_state(work_ram);
    check(
        captured && captured->effect.source_slot == captured_source_slot
            && captured->effect.source_appearance == 0x66U
            && captured->effect.source_association == 0x80U
            && captured->effect.ticks_remaining == 4U
            && captured_source_claimed && captured_appearance_seeded
            && captured_association_seeded
            && captured_effect_seeded && seeded_effect
            && seeded_effect->collision_effect
            && seeded_effect->collision_effect->ticks_remaining == 4U
            && !devils_crush::native_runtime::capture_collision_effect(
                invalid_capture_state, captured_effect_slot),
        "native collision-effect capture validates and seeds a linked live source");
}

void test_native_collision_reaction()
{
    using devils_crush::native_runtime::ActorBehavior;
    using devils_crush::native_runtime::ActorState;
    using devils_crush::native_runtime::CollisionReactionState;
    using devils_crush::native_runtime::CollisionTableCursor;
    using devils_crush::native_runtime::CollisionTableRecordDisposition;
    using devils_crush::native_runtime::CollisionTableRecordResolution;
    using devils_crush::native_runtime::advance_collision_reaction;

    const auto phase_step = advance_collision_reaction({
        .collision_event = 0x05U,
        .phase = 2U,
        .ticks_until_next_phase = 1U,
    });
    check(
        phase_step.response_code == 0x0AU && phase_step.phase_advanced
            && phase_step.state.phase == 3U
            && phase_step.state.ticks_until_next_phase == 4U,
        "native collision reaction advances its event-selected phase cadence");

    const auto ordinary_step = advance_collision_reaction(phase_step.state);
    check(
        ordinary_step.response_code == 0x0BU && !ordinary_step.phase_advanced
            && ordinary_step.state.ticks_until_next_phase == 3U,
        "native collision reaction retains a phase for its remaining ticks");

    const auto wrapped_countdown = advance_collision_reaction(
        CollisionReactionState{});
    check(
        !wrapped_countdown.phase_advanced
            && wrapped_countdown.state.ticks_until_next_phase == 0xFFU,
        "native collision reaction preserves an explicit zero-countdown state");

    ActorState actors;
    check(
        actors.claim_dynamic_slot(0x04U, ActorBehavior::shared_motion),
        "native actor state claims a target for collision-reaction ownership");
    auto* actor = actors.actor(0x04U);
    if (actor != nullptr) {
        actor->collision_reaction = {
            .collision_event = 0x02U,
            .phase = 1U,
            .ticks_until_next_phase = 2U,
        };
    }
    const auto actor_step = actors.advance_collision_reaction(0x04U);
    check(
        actor_step && actor_step->step.response_code == 0x05U
            && actor && actor->collision_reaction
            && actor->collision_reaction->ticks_until_next_phase == 1U,
        "native actor state retains a target collision reaction in place");

    if (actor != nullptr) {
        actor->collision_table = {
            .collision_event = 0x00U,
            .record_cursor = 2U,
        };
        actor->collision_reaction = {
            .collision_event = 0x00U,
            .phase = 1U,
            .ticks_until_next_phase = 2U,
        };
    }
    const auto table_step = actors.advance_collision_table(
        0x04U,
        {
            .header_or_delta = 0x83U,
        });
    const auto synchronized_reaction = actors.advance_collision_reaction(0x04U);
    check(
        table_step && table_step->step.event_changed && actor
            && actor->collision_table
            && actor->collision_table->collision_event == 0x03U
            && actor->collision_reaction
            && actor->collision_reaction->collision_event == 0x03U
            && synchronized_reaction
            && synchronized_reaction->step.response_code == 0x05U,
        "native actor state feeds a collision-table event into its reaction");

    if (actor != nullptr) {
        actor->collision_table =
            devils_crush::native_runtime::CollisionTableState{};
        actor->collision_reaction = {
            .phase = 2U,
            .ticks_until_next_phase = 3U,
        };
    }
    const auto resolved_step = actors.apply_collision_table_resolution(
        0x04U,
        {
            .disposition = CollisionTableRecordDisposition::record_ready,
            .cursor = {.entry = 3U, .motion = 4U},
            .record = {
                .header_or_delta = 0x86U,
                .delta_after_header = 0x40U,
            },
        });
    check(
        resolved_step && resolved_step->advance
            && resolved_step->advance->event_changed && actor
            && actor->collision_table
            && actor->collision_table->collision_entry_cursor == 3U
            && actor->collision_table->record_cursor == 6U
            && actor->collision_table->position_x == 0xFFFFU
            && actor->collision_reaction
            && actor->collision_reaction->collision_event == 0x06U,
        "native actor state consumes a decoded collision-table record and cursor");

    const auto ended_table = actors.apply_collision_table_resolution(
        0x04U,
        {
            .disposition = CollisionTableRecordDisposition::end_of_table,
            .cursor = CollisionTableCursor{.entry = 4U},
        });
    check(
        ended_table && !ended_table->advance && actor && actor->collision_table
            && actor->collision_table->collision_entry_cursor == 4U
            && actor->collision_table->record_cursor == 0U,
        "native actor state commits a terminal collision-table cursor");
    actors.release(0x04U);
    check(
        !actors.advance_collision_reaction(0x04U),
        "native actor state does not advance an inactive target reaction");
}

void test_native_collision_table()
{
    using devils_crush::native_runtime::CollisionTableAdvanceDisposition;
    using devils_crush::native_runtime::CollisionTableRecord;
    using devils_crush::native_runtime::CollisionTableState;
    using devils_crush::native_runtime::advance_collision_table;
    using devils_crush::native_runtime::select_collision_table_pass;

    const auto ordinary_pass = select_collision_table_pass(0x12U);
    const auto alternating_pass = select_collision_table_pass(0x80U);
    check(
        ordinary_pass.index == 0x12U && !ordinary_pass.alternating
            && ordinary_pass.next_pass_state == 0x12U
            && alternating_pass.index == 1U && alternating_pass.alternating
            && alternating_pass.next_pass_state == 0x81U
            && !alternating_pass.completes_before_record,
        "native collision table selects ordinary and alternating passes");

    const auto completed_pass = advance_collision_table(
        CollisionTableState{.pass_state = 0x81U}, CollisionTableRecord{});
    check(
        completed_pass.disposition
                == CollisionTableAdvanceDisposition::alternating_pass_complete
            && completed_pass.state.pass_state == 0x80U,
        "native collision table completes an alternating pass before a record");

    const auto header_record = advance_collision_table(
        CollisionTableState{
            .pass_state = 0x80U,
            .collision_event = 0x02U,
            .orientation_flags = 0x00U,
            .record_cursor = 0xFFU,
            .position_x = 0x00FFU,
            .position_y = 0x0100U,
        },
        CollisionTableRecord{
            .table_entry = 0x00U,
            .header_or_delta = 0x85U,
            .delta_after_header = 0x28U,
        });
    check(
        header_record.disposition == CollisionTableAdvanceDisposition::advanced
            && header_record.event_changed && header_record.applied_delta == 0x28U
            && header_record.state.pass_state == 0x81U
            && header_record.state.collision_event == 0x05U
            && header_record.state.orientation_flags == 0x80U
            && header_record.state.record_cursor == 1U
            && header_record.state.position_x == 0x0100U
            && header_record.state.position_y == 0x0101U,
        "native collision table applies a header event, orientation, cursor, and motion");

    const auto continuation = advance_collision_table(
        CollisionTableState{
            .pass_state = 0x01U,
            .collision_event = 0x07U,
            .record_cursor = 5U,
        },
        CollisionTableRecord{.header_or_delta = 0x50U});
    check(
        continuation.disposition == CollisionTableAdvanceDisposition::advanced
            && !continuation.event_changed
            && continuation.state.collision_event == 0x07U
            && continuation.state.record_cursor == 6U
            && continuation.state.position_x == 0xFFFFU
            && continuation.state.position_y == 0xFFFFU,
        "native collision table applies a headerless continuation delta");

    const auto invalid = advance_collision_table(
        CollisionTableState{.position_x = 12U},
        CollisionTableRecord{.table_entry = 0xFEU});
    check(
        invalid.disposition == CollisionTableAdvanceDisposition::invalid_record
            && invalid.state == invalid.previous,
        "native collision table rejects record sentinels without mutation");
}

void test_native_collision_table_asset()
{
    using devils_crush::native_runtime::ActorBehavior;
    using devils_crush::native_runtime::ActorState;
    using devils_crush::native_runtime::CollisionTableCursor;
    using devils_crush::native_runtime::CollisionTableRecordDisposition;
    using devils_crush::native_runtime::MappedRomView;
    using devils_crush::native_runtime::advance_actor_collision_table_from_asset;
    using devils_crush::native_runtime::kCollisionTableMotionDirectory;
    using devils_crush::native_runtime::kCollisionTableSelectorDirectory;
    using devils_crush::native_runtime::resolve_collision_table_record;

    std::vector<std::byte> rom(MappedRomView::kSupportedRomBytes);
    std::array<std::uint8_t, 8> mpr{};
    mpr[3] = 0x03U;
    const auto store = [&rom](const std::uint16_t address,
                              const std::uint8_t value) {
        rom[0x03U * MappedRomView::kBankBytes
            + (address & (MappedRomView::kBankBytes - 1U))] =
            static_cast<std::byte>(value);
    };
    const auto store_word = [&store](
                                const std::uint16_t address,
                                const std::uint16_t value) {
        store(address, static_cast<std::uint8_t>(value));
        store(
            static_cast<std::uint16_t>(address + 1U),
            static_cast<std::uint8_t>(value >> 8U));
    };
    store_word(
        static_cast<std::uint16_t>(kCollisionTableSelectorDirectory + 4U),
        0x6E00U);
    store(0x6E00U, 0x01U);
    store_word(
        static_cast<std::uint16_t>(kCollisionTableMotionDirectory + 2U),
        0x6F00U);
    store(0x6F00U, 0x83U);
    store(0x6F01U, 0x20U);

    const MappedRomView mapped_rom(rom, mpr);
    const auto header = resolve_collision_table_record(
        mapped_rom, 2U, CollisionTableCursor{});
    check(
        header.disposition == CollisionTableRecordDisposition::record_ready
            && header.cursor == CollisionTableCursor{}
            && header.record.table_entry == 0x01U
            && header.record.header_or_delta == 0x83U
            && header.record.delta_after_header == 0x20U,
        "native collision-table asset decodes a selected header record");

    ActorState actors;
    check(
        actors.claim_dynamic_slot(0x04U, ActorBehavior::shared_motion),
        "native collision-table driver claims an actor target");
    auto* actor = actors.actor(0x04U);
    if (actor != nullptr) {
        actor->collision_reaction = {
            .phase = 1U,
            .ticks_until_next_phase = 2U,
        };
    }
    check(
        !advance_actor_collision_table_from_asset(actors, 0x04U, mapped_rom)
            && actors.configure_collision_table(0x04U, {.selector = 2U}),
        "native collision-table driver requires an actor table program");
    const auto actor_header = advance_actor_collision_table_from_asset(
        actors, 0x04U, mapped_rom);
    check(
        actor_header && actor_header->advance
            && actor_header->resolution == header && actor
            && actor->collision_table_program
            && actor->collision_table_program->selector == 2U
            && actor->collision_table
            && actor->collision_table->collision_entry_cursor == 0U
            && actor->collision_table->record_cursor == 2U
            && actor->collision_table->position_x == 1U
            && actor->collision_reaction
            && actor->collision_reaction->collision_event == 0x03U,
        "native collision-table driver connects an asset record to actor state");

    store(0x6E01U, 0x02U);
    store_word(
        static_cast<std::uint16_t>(kCollisionTableMotionDirectory + 4U),
        0x6F20U);
    store(0x6F00U, 0xC0U);
    store(0x6F20U, 0x10U);
    const auto continued = resolve_collision_table_record(
        mapped_rom, 2U, CollisionTableCursor{});
    check(
        continued.disposition == CollisionTableRecordDisposition::record_ready
            && continued.cursor == CollisionTableCursor{.entry = 1U}
            && continued.record.table_entry == 0x02U
            && continued.record.header_or_delta == 0x10U,
        "native collision-table asset follows an entry-advance marker");

    store(0x6E01U, 0xFEU);
    store(0x6E00U, 0x01U);
    store(0x6F00U, 0x08U);
    const auto restarted = resolve_collision_table_record(
        mapped_rom, 2U, CollisionTableCursor{.entry = 1U, .motion = 5U});
    check(
        restarted.disposition == CollisionTableRecordDisposition::record_ready
            && restarted.restarted && restarted.cursor == CollisionTableCursor{}
            && restarted.record.table_entry == 0x01U
            && restarted.record.header_or_delta == 0x08U,
        "native collision-table asset restarts a control-marked table");

    store(0x6E00U, 0xFFU);
    const auto ended = resolve_collision_table_record(
        mapped_rom, 2U, CollisionTableCursor{});
    check(
        ended.disposition == CollisionTableRecordDisposition::end_of_table,
        "native collision-table asset reports a terminal table marker");
}

void test_native_small_soldier()
{
    using devils_crush::native_runtime::ActorBehavior;
    using devils_crush::native_runtime::ActorState;
    using devils_crush::native_runtime::SmallSoldierContactDisposition;
    using devils_crush::native_runtime::SmallSoldierGroupDisposition;
    using devils_crush::native_runtime::SmallSoldierGroupSide;
    using devils_crush::native_runtime::SmallSoldierGroupState;
    using devils_crush::native_runtime::SmallSoldierSourceDisposition;
    using devils_crush::native_runtime::SmallSoldierSourceState;
    using devils_crush::native_runtime::SmallSoldierState;
    using devils_crush::native_runtime::complete_small_soldier_group_child;
    using devils_crush::native_runtime::complete_small_soldier_animation;
    using devils_crush::native_runtime::complete_small_soldier_source_child;
    using devils_crush::native_runtime::consume_small_soldier_contact;
    using devils_crush::native_runtime::advance_small_soldier_source_clock;
    using devils_crush::native_runtime::regenerate_small_soldier_source;
    using devils_crush::native_runtime::spawn_small_soldier_child;
    using devils_crush::native_runtime::spawn_small_soldier_source_child;

    const auto untouched = consume_small_soldier_contact(SmallSoldierState{});
    check(
        untouched.disposition == SmallSoldierContactDisposition::no_contact
            && untouched.state == untouched.previous,
        "native Small-Soldier ignores an unconfirmed contact");

    const auto first_hit = consume_small_soldier_contact({
        .contact_pending = true,
    });
    check(
        first_hit.disposition == SmallSoldierContactDisposition::survived_hit
            && !first_hit.state.contact_pending
            && first_hit.state.hits_remaining == 1U
            && first_hit.hit_score_units == 30U
            && first_hit.completion_score_units == 0U
            && !first_hit.decrement_group
            && !first_hit.release_actor,
        "native Small-Soldier consumes its first confirmed hit");

    const auto final_hit = consume_small_soldier_contact({
        .hits_remaining = 1U,
        .contact_pending = true,
    });
    check(
        final_hit.disposition == SmallSoldierContactDisposition::completed
            && final_hit.state.hits_remaining == 0U
            && final_hit.hit_score_units == 30U
            && final_hit.completion_score_units == 50U
            && final_hit.decrement_group
            && final_hit.release_actor,
        "native Small-Soldier completes a normal second hit");

    const auto deferred_hit = consume_small_soldier_contact({
        .hits_remaining = 1U,
        .subtype = 0x0FU,
        .contact_pending = true,
    });
    check(
        deferred_hit.disposition
                == SmallSoldierContactDisposition::awaiting_completion_animation
            && deferred_hit.state.completion_animation_pending
            && deferred_hit.hit_score_units == 30U
            && deferred_hit.completion_score_units == 0U
            && !deferred_hit.decrement_group && !deferred_hit.release_actor,
        "native Small-Soldier defers subtype completion to its animation");

    const auto deferred_completion = complete_small_soldier_animation(
        deferred_hit.state);
    check(
        deferred_completion.disposition == SmallSoldierContactDisposition::completed
            && !deferred_completion.state.completion_animation_pending
            && deferred_completion.hit_score_units == 0U
            && deferred_completion.completion_score_units == 50U
            && deferred_completion.decrement_group
            && deferred_completion.release_actor,
        "native Small-Soldier completes its subtype animation transition");

    const auto invalid_hit = consume_small_soldier_contact({
        .hits_remaining = 0U,
        .contact_pending = true,
    });
    check(
        invalid_hit.disposition == SmallSoldierContactDisposition::invalid_state
            && invalid_hit.state == invalid_hit.previous,
        "native Small-Soldier rejects a contact after completion");

    const auto source_wait = advance_small_soldier_source_clock({
        .group = {
            .side = SmallSoldierGroupSide::lower_left,
            .pending_spawns = 2U,
            .live_children = 1U,
        },
        .ticks_until_next_spawn = 2U,
    });
    const auto source_due = advance_small_soldier_source_clock(source_wait.state);
    check(
        source_wait.disposition == SmallSoldierSourceDisposition::waiting
            && source_wait.state.ticks_until_next_spawn == 1U
            && source_due.disposition
                == SmallSoldierSourceDisposition::spawn_requested
            && source_due.state.ticks_until_next_spawn == 48U
            && source_due.state.group == source_wait.state.group,
        "native Side-Star clock restores its 48-tick spawn delay before requesting allocation");

    SmallSoldierGroupState group{
        .side = SmallSoldierGroupSide::upper_right,
    };
    for (std::uint8_t child = 0U; child < 5U; ++child) {
        const auto spawn = spawn_small_soldier_child(group);
        check(
            spawn.disposition == SmallSoldierGroupDisposition::child_spawned
                && spawn.state.side == SmallSoldierGroupSide::upper_right
                && spawn.state.pending_spawns == 4U - child
                && spawn.state.live_children == child + 1U,
            "native Small-Soldier source tracks each of its five child spawns");
        group = spawn.state;
    }
    const auto exhausted_spawn = spawn_small_soldier_child(group);
    check(
        exhausted_spawn.disposition == SmallSoldierGroupDisposition::invalid_state
            && exhausted_spawn.state == group,
        "native Small-Soldier source rejects a sixth child spawn");
    for (std::uint8_t child = 0U; child < 4U; ++child) {
        const auto completion = complete_small_soldier_group_child(group);
        check(
            completion.disposition == SmallSoldierGroupDisposition::child_completed
                && completion.state.live_children == 4U - child,
            "native Small-Soldier source retains its group until every child completes");
        group = completion.state;
    }
    const auto source_completion = complete_small_soldier_group_child(group);
    check(
        source_completion.disposition
                == SmallSoldierGroupDisposition::source_completed
            && source_completion.state.live_children == 0U
            && source_completion.source_completion_score_units == 200U,
        "native Small-Soldier source emits its final group-completion award");
    const auto invalid_completion = complete_small_soldier_group_child(
        source_completion.state);
    check(
        invalid_completion.disposition == SmallSoldierGroupDisposition::invalid_state
            && invalid_completion.state == source_completion.state,
        "native Small-Soldier source cannot complete a nonexistent child");

    SmallSoldierSourceState source{
        .group = {.side = SmallSoldierGroupSide::upper_right},
        .appearance = 3U,
    };
    for (std::uint8_t child = 0U; child < 5U; ++child) {
        const auto spawn = spawn_small_soldier_source_child(source);
        check(
            spawn.disposition == SmallSoldierSourceDisposition::child_spawned
                && spawn.group_step
                && spawn.group_step->disposition
                    == SmallSoldierGroupDisposition::child_spawned,
            "native Side-Star source allocates a child through its generation state");
        source = spawn.state;
    }
    for (std::uint8_t child = 0U; child < 4U; ++child) {
        source = complete_small_soldier_source_child(source).state;
    }
    const auto generation_completion = complete_small_soldier_source_child(source);
    check(
        generation_completion.disposition
                == SmallSoldierSourceDisposition::generation_completed
            && generation_completion.group_step
            && generation_completion.group_step->source_completion_score_units == 200U
            && generation_completion.state.group.live_children == 0U
            && generation_completion.state.appearance == 0U
            && generation_completion.state.regeneration_pending,
        "native Side-Star completion advances its appearance modulo four and requests regeneration");
    const auto blocked_spawn = spawn_small_soldier_source_child(
        generation_completion.state);
    check(
        blocked_spawn.disposition == SmallSoldierSourceDisposition::invalid_state
            && blocked_spawn.state == generation_completion.state,
        "native Side-Star source blocks children until its regeneration handoff completes");
    const auto regenerated = regenerate_small_soldier_source(
        generation_completion.state);
    check(
        regenerated.disposition == SmallSoldierSourceDisposition::regenerated
            && regenerated.state.group.side == SmallSoldierGroupSide::upper_right
            && regenerated.state.group.pending_spawns == 5U
            && regenerated.state.group.live_children == 0U
            && regenerated.state.appearance == 0U
            && !regenerated.state.regeneration_pending,
        "native Side-Star regeneration restores only the next five-child budget");

    ActorState actors;
    check(
        actors.claim_dynamic_slot(0x04U, ActorBehavior::small_soldier)
            && actors.configure_small_soldier(0x04U)
            && !actors.configure_small_soldier(0x05U),
        "native actor state configures Small-Soldier rules only on its owner");
    const auto owned_first_hit = actors.apply_small_soldier_contact(0x04U);
    const auto* owned_soldier = actors.actor(0x04U);
    check(
        owned_first_hit
            && owned_first_hit->step.disposition
                == SmallSoldierContactDisposition::survived_hit
            && owned_first_hit->step.hit_score_units == 30U && owned_soldier
            && owned_soldier->small_soldier
            && owned_soldier->small_soldier->hits_remaining == 1U,
        "native actor state turns a confirmed contact into a retained Small-Soldier hit");

    const auto owned_completion = actors.apply_small_soldier_contact(0x04U);
    check(
        owned_completion
            && owned_completion->step.disposition
                == SmallSoldierContactDisposition::completed
            && owned_completion->step.hit_score_units == 30U
            && owned_completion->step.completion_score_units == 50U
            && owned_completion->step.decrement_group
            && owned_completion->step.release_actor && owned_soldier
            && !owned_soldier->active(),
        "native actor state releases a completed Small-Soldier after returning its effects");

    check(
        actors.claim_dynamic_slot(0x04U, ActorBehavior::small_soldier)
            && actors.configure_small_soldier(0x04U, {
                .hits_remaining = 1U,
                .subtype = 0x0FU,
            }),
        "native actor state configures the deferred Small-Soldier subtype");
    const auto owned_deferred_hit = actors.apply_small_soldier_contact(0x04U);
    const auto owned_deferred_completion =
        actors.complete_small_soldier_animation(0x04U);
    check(
        owned_deferred_hit
            && owned_deferred_hit->step.disposition
                == SmallSoldierContactDisposition::awaiting_completion_animation
            && owned_deferred_completion
            && owned_deferred_completion->step.disposition
                == SmallSoldierContactDisposition::completed
            && owned_deferred_completion->step.completion_score_units == 50U
            && owned_deferred_completion->step.release_actor
            && !actors.actor(0x04U)->active(),
        "native actor state owns the Small-Soldier deferred completion lifecycle");

    check(
        actors.claim_dynamic_slot(0x05U, ActorBehavior::small_soldier),
        "native actor state claims a slot before registering a Small-Soldier spawn");
    const auto actor_spawn = actors.spawn_small_soldier(
        0x05U,
        {
            .hits_remaining = 1U,
            .group_side = SmallSoldierGroupSide::upper_right,
        });
    const auto actor_completion = actors.apply_small_soldier_contact(0x05U);
    const auto* upper_right_group = actors.small_soldier_group(
        SmallSoldierGroupSide::upper_right);
    const auto* upper_right_source = actors.small_soldier_source(
        SmallSoldierGroupSide::upper_right);
    check(
        actor_spawn
            && actor_spawn->group_step.disposition
                == SmallSoldierGroupDisposition::child_spawned
            && actor_spawn->source_step.disposition
                == SmallSoldierSourceDisposition::child_spawned
            && actor_completion && actor_completion->group_step
            && actor_completion->group_step->disposition
                == SmallSoldierGroupDisposition::child_completed
            && actor_completion->source_step
            && actor_completion->source_step->disposition
                == SmallSoldierSourceDisposition::child_completed
            && upper_right_group && upper_right_group->pending_spawns == 4U
            && upper_right_group->live_children == 0U
            && upper_right_source && !upper_right_source->regeneration_pending
            && !actors.actor(0x05U)->active(),
        "native actor state connects a completed Small-Soldier to its Side-Star group");
    check(
        !actors.spawn_small_soldier(0x05U),
        "native actor state rejects a Small-Soldier spawn without a claimed slot");

    ActorState source_actors;
    std::optional<devils_crush::native_runtime::ActorSmallSoldierUpdate>
        last_source_completion;
    for (std::uint8_t child = 0U; child < 5U; ++child) {
        const auto slot = static_cast<std::uint8_t>(0x10U + child);
        check(
            source_actors.claim_dynamic_slot(slot, ActorBehavior::small_soldier)
                && source_actors.spawn_small_soldier(slot, {
                    .hits_remaining = 1U,
                    .group_side = SmallSoldierGroupSide::lower_left,
                }),
            "native actor state allocates each child into its Side-Star source");
        last_source_completion = source_actors.apply_small_soldier_contact(slot);
    }
    const auto* completed_source = source_actors.small_soldier_source(
        SmallSoldierGroupSide::lower_left);
    const auto regeneration_was_pending = completed_source
        && completed_source->regeneration_pending;
    const auto actor_regeneration = source_actors.regenerate_small_soldier_source(
        SmallSoldierGroupSide::lower_left);
    check(
        last_source_completion && last_source_completion->group_step
            && last_source_completion->group_step->disposition
                == SmallSoldierGroupDisposition::source_completed
            && last_source_completion->source_step
            && last_source_completion->source_step->disposition
                == SmallSoldierSourceDisposition::generation_completed
            && regeneration_was_pending
            && actor_regeneration
            && actor_regeneration->disposition
                == SmallSoldierSourceDisposition::regenerated
            && source_actors.small_soldier_group(
                   SmallSoldierGroupSide::lower_left)->pending_spawns == 5U,
        "native actor state carries the fifth child through source completion and regeneration");
}

void test_native_bumper()
{
    using devils_crush::native_runtime::BumperHitDisposition;
    using devils_crush::native_runtime::BumperState;
    using devils_crush::native_runtime::ScoreAwardDisposition;
    using devils_crush::native_runtime::ScoreState;
    using devils_crush::native_runtime::ActorBehavior;
    using devils_crush::native_runtime::NativeGameplayState;
    using devils_crush::native_runtime::consume_bumper_contact;
    using devils_crush::native_runtime::resolve_bumper_contact;

    auto state = BumperState{.side = 1U};
    BumperState state_after_sixth;
    bool primary_progression_matches = true;
    for (std::uint8_t hit = 1U; hit <= 6U; ++hit) {
        const auto step = consume_bumper_contact(state);
        primary_progression_matches = primary_progression_matches
            && step.disposition == BumperHitDisposition::primary_damage
            && step.state.hit_phase == hit
            && step.state.primary_appearance == hit
            && step.state.secondary_damage == 0U
            && step.score_award_units == 10U && step.audio_command == 0x20U
            && step.refresh_primary_appearance
            && !step.updates_secondary_damage
            && step.requested_child_count == 0U;
        state = step.state;
        if (hit == 6U) {
            state_after_sixth = state;
        }
    }
    check(
        primary_progression_matches && state_after_sixth.side == 1U,
        "native Bumper rule advances all six primary hit appearances");

    const auto seventh = consume_bumper_contact(state);
    const auto eighth = consume_bumper_contact(seventh.state);
    const auto destruction = consume_bumper_contact(eighth.state);
    check(
        seventh.disposition == BumperHitDisposition::secondary_damage
            && seventh.state.hit_phase == 7U
            && seventh.state.primary_appearance == 6U
            && seventh.state.secondary_damage == 1U
            && seventh.score_award_units == 10U
            && seventh.audio_command == 0x20U
            && !seventh.refresh_primary_appearance
            && seventh.updates_secondary_damage
            && eighth.disposition == BumperHitDisposition::secondary_damage
            && eighth.state.secondary_damage == 2U
            && destruction.disposition == BumperHitDisposition::destroyed
            && destruction.state.hit_phase == 9U && !destruction.state.active
            && destruction.score_award_units == 200U
            && destruction.audio_command == 0U
            && destruction.requested_child_count == 6U,
        "native Bumper rule advances secondary damage then requests six-child destruction");

    const auto inactive = consume_bumper_contact(destruction.state);
    const auto invalid = consume_bumper_contact({.hit_phase = 9U});
    check(
        inactive.disposition == BumperHitDisposition::inactive
            && inactive.state == destruction.state
            && invalid.disposition == BumperHitDisposition::invalid_state
            && invalid.state.hit_phase == 9U && invalid.state.active,
        "native Bumper rule keeps inactive and malformed states explicit");

    const auto scored = resolve_bumper_contact(
        BumperState{}, ScoreState{.stored_units = 90U});
    const auto capped_destruction = resolve_bumper_contact(
        BumperState{.hit_phase = 8U},
        ScoreState{.stored_units = ScoreState::kExclusiveLimit - 100U});
    check(
        scored.bumper.disposition == BumperHitDisposition::primary_damage
            && scored.score_award.state.stored_units == 100U
            && capped_destruction.bumper.disposition
                == BumperHitDisposition::destroyed
            && capped_destruction.score_award.disposition
                == ScoreAwardDisposition::reaches_limit
            && capped_destruction.score_award.state.stored_units
                == ScoreState::kExclusiveLimit - 100U,
        "native Bumper scoring preserves the independent ordinary-score ceiling");
}

void test_native_front_spider()
{
    using devils_crush::native_runtime::FrontSpiderContactDisposition;
    using devils_crush::native_runtime::FrontSpiderState;
    using devils_crush::native_runtime::ScoreAwardDisposition;
    using devils_crush::native_runtime::ScoreState;
    using devils_crush::native_runtime::complete_front_spider_transition;
    using devils_crush::native_runtime::consume_front_spider_contact;
    using devils_crush::native_runtime::resolve_front_spider_contact;

    const auto hit = consume_front_spider_contact({.side = 1U});
    check(
        hit.disposition == FrontSpiderContactDisposition::hit
            && !hit.state.collision_ready && hit.score_award_units == 40U
            && hit.audio_command == 0x34U && hit.sets_side_safety
            && hit.requested_transition_stages == 4U,
        "native Front-Spider rule names the one-hit safety, score, audio, and transition effects");
    check(
        consume_front_spider_contact(hit.state).disposition
            == FrontSpiderContactDisposition::transition_pending
            && complete_front_spider_transition(hit.state).collision_ready,
        "native Front-Spider rule waits for the appearance owner before rearming");
    check(
        consume_front_spider_contact({.side = 2U}).disposition
            == FrontSpiderContactDisposition::invalid_state,
        "native Front-Spider rule rejects an invalid fixed-side state");

    const auto capped = resolve_front_spider_contact(
        {.side = 0U}, {.stored_units = ScoreState::kExclusiveLimit - 30U});
    check(
        capped.spider.disposition == FrontSpiderContactDisposition::hit
            && !capped.spider.state.collision_ready
            && capped.score_award.disposition == ScoreAwardDisposition::reaches_limit,
        "native Front-Spider state transitions even when its score award reaches the limit");
}

void test_native_side_spider()
{
    using devils_crush::native_runtime::ScoreAwardDisposition;
    using devils_crush::native_runtime::ScoreState;
    using devils_crush::native_runtime::SideSpiderContactDisposition;
    using devils_crush::native_runtime::SideSpiderState;
    using devils_crush::native_runtime::complete_side_spider_transition;
    using devils_crush::native_runtime::consume_side_spider_contact;
    using devils_crush::native_runtime::resolve_side_spider_contact;

    const auto hit = consume_side_spider_contact({.side = 0U});
    check(
        hit.disposition == SideSpiderContactDisposition::hit
            && !hit.state.collision_ready && hit.score_award_units == 100U
            && hit.audio_command == 0x34U && hit.sets_shared_transition_latch,
        "native Side-Spider rule names the shared latch, score, and audio effects");
    check(
        consume_side_spider_contact(hit.state).disposition
            == SideSpiderContactDisposition::transition_pending
            && complete_side_spider_transition(hit.state).collision_ready,
        "native Side-Spider rule waits for the shared appearance transition");
    check(
        consume_side_spider_contact({.side = 2U}).disposition
            == SideSpiderContactDisposition::invalid_state,
        "native Side-Spider rule rejects an invalid fixed-side state");

    const auto capped = resolve_side_spider_contact(
        {.side = 1U}, {.stored_units = ScoreState::kExclusiveLimit - 50U});
    check(
        capped.spider.disposition == SideSpiderContactDisposition::hit
            && !capped.spider.state.collision_ready
            && capped.score_award.disposition == ScoreAwardDisposition::reaches_limit,
        "native Side-Spider state transitions even when its score award reaches the limit");
}

void test_native_skeleton_insect()
{
    using devils_crush::native_runtime::ScoreAwardDisposition;
    using devils_crush::native_runtime::ScoreState;
    using devils_crush::native_runtime::SkeletonInsectKind;
    using devils_crush::native_runtime::SkeletonInsectLifecycle;
    using devils_crush::native_runtime::SkeletonInsectTriggerDisposition;
    using devils_crush::native_runtime::TableRuleLatches;
    using devils_crush::native_runtime::complete_skeleton_insect_transition;
    using devils_crush::native_runtime::consume_skeleton_insect_source;
    using devils_crush::native_runtime::resolve_skeleton_insect_source;

    const auto left_wait = consume_skeleton_insect_source({}, {});
    const auto left_hit = consume_skeleton_insect_source(
        {}, {.left_front_spider_safety = true});
    const auto center_hit = consume_skeleton_insect_source(
        {.kind = SkeletonInsectKind::center}, {.tower_completed = true});
    const auto right_hit = consume_skeleton_insect_source(
        {.kind = SkeletonInsectKind::right}, {.right_front_spider_safety = true});
    check(
        left_wait.disposition == SkeletonInsectTriggerDisposition::waiting_for_source
            && left_hit.disposition == SkeletonInsectTriggerDisposition::triggered
            && left_hit.score_award_units == 40U
            && left_hit.requested_startup_rounds == 3U
            && left_hit.audio_command == 0U
            && center_hit.score_award_units == 100U
            && center_hit.requested_startup_rounds == 16U
            && right_hit.score_award_units == 40U
            && right_hit.requested_startup_rounds == 2U,
        "native Skeleton-Insect rule binds each fixed target to its source and score schedule");
    check(
        consume_skeleton_insect_source(left_hit.state, {.left_front_spider_safety = true})
                .disposition == SkeletonInsectTriggerDisposition::transition_pending
            && complete_skeleton_insect_transition(left_hit.state).lifecycle
                == SkeletonInsectLifecycle::post_trigger,
        "native Skeleton-Insect rule makes an asserted source latch one-shot");

    const auto capped = resolve_skeleton_insect_source(
        {.kind = SkeletonInsectKind::center}, {.tower_completed = true},
        {.stored_units = ScoreState::kExclusiveLimit - 50U});
    check(
        capped.insect.disposition == SkeletonInsectTriggerDisposition::triggered
            && capped.score_award.disposition == ScoreAwardDisposition::reaches_limit,
        "native Skeleton-Insect trigger survives a rejected terminal score award");
}

void test_native_tower()
{
    using devils_crush::native_runtime::ScoreAwardDisposition;
    using devils_crush::native_runtime::ScoreState;
    using devils_crush::native_runtime::TowerBonusStageSelectionDisposition;
    using devils_crush::native_runtime::TowerContactDisposition;
    using devils_crush::native_runtime::TowerPresentationDisposition;
    using devils_crush::native_runtime::advance_tower_presentation;
    using devils_crush::native_runtime::consume_tower_bonus_stage_selection;
    using devils_crush::native_runtime::consume_tower_contact;
    using devils_crush::native_runtime::resolve_tower_contact;

    const auto hit = consume_tower_contact({}, false);
    check(
        hit.disposition == TowerContactDisposition::first_source_hit
            && !hit.state.collision_ready && hit.state.hit_progress == 1U
            && hit.score_award_units == 50U && hit.audio_command == 0x33U
            && hit.sets_center_insect_source,
        "native Tower rule names the first qualifying source hit");
    check(
        consume_tower_contact(hit.state, false).disposition
                == TowerContactDisposition::transition_pending
            && advance_tower_presentation(hit.state).disposition
                == TowerPresentationDisposition::countdown
            && consume_tower_contact({}, true).disposition
                == TowerContactDisposition::armed_pass_decremented
            && consume_tower_contact(
                   {.hit_progress = 1U, .remaining_armed_passes = 1U}, true)
                    .disposition
                == TowerContactDisposition::bonus_stage_selection_armed
            && advance_tower_presentation(
                   {.collision_ready = false, .bonus_stage_selection_pending = true})
                   .disposition == TowerPresentationDisposition::not_pending,
        "native Tower rule preserves presentation, armed-pass, and bonus-selection boundaries");

    auto presentation = hit.state;
    const auto first_presentation = advance_tower_presentation(presentation);
    presentation = first_presentation.state;
    const auto second_presentation = advance_tower_presentation(presentation);
    presentation = second_presentation.state;
    const auto third_presentation = advance_tower_presentation(presentation);
    presentation = third_presentation.state;
    const auto installed_steady_dispatch = advance_tower_presentation(presentation);
    check(
        first_presentation.disposition == TowerPresentationDisposition::countdown
            && first_presentation.state.presentation_updates_remaining == 3U
            && second_presentation.disposition == TowerPresentationDisposition::countdown
            && third_presentation.disposition == TowerPresentationDisposition::countdown
            && installed_steady_dispatch.disposition
                == TowerPresentationDisposition::steady_dispatch_installed
            && installed_steady_dispatch.state.collision_ready
            && installed_steady_dispatch.state.presentation_updates_remaining == 0U,
        "native Tower presentation preserves three countdowns before steady rearm");

    const auto capped = resolve_tower_contact(
        {}, false, {.stored_units = ScoreState::kExclusiveLimit - 40U});
    check(
        capped.tower.disposition == TowerContactDisposition::first_source_hit
            && capped.score_award.disposition == ScoreAwardDisposition::reaches_limit,
        "native Tower source transition survives a rejected terminal score award");

    const auto waiting_for_bonus_collision = consume_tower_bonus_stage_selection(
        {.collision_ready = false, .hit_progress = 1U,
         .remaining_armed_passes = 0U, .bonus_stage_selection_pending = true},
        false);
    const auto selected_bonus_stage = consume_tower_bonus_stage_selection(
        waiting_for_bonus_collision.state, true);
    check(
        waiting_for_bonus_collision.disposition
                == TowerBonusStageSelectionDisposition::waiting_for_collision
            && selected_bonus_stage.disposition
                == TowerBonusStageSelectionDisposition::bonus_stage_selected
            && selected_bonus_stage.bonus_stage == 5U
            && selected_bonus_stage.audio_command == 0x41U
            && selected_bonus_stage.sets_tower_stage_flag
            && selected_bonus_stage.requests_session_transition
            && selected_bonus_stage.starts_next_tower_insect_cycle
            && selected_bonus_stage.state.remaining_armed_passes == 5U
            && !selected_bonus_stage.state.collision_ready
            && !selected_bonus_stage.state.bonus_stage_selection_pending
            && selected_bonus_stage.state.bonus_stage_transition_requested
            && consume_tower_bonus_stage_selection(selected_bonus_stage.state, true)
                    .disposition
                == TowerBonusStageSelectionDisposition::transition_pending,
        "native Tower 03:6136 selects BS-5 only for its later qualifying collision");
}

void test_native_tower_insect()
{
    using devils_crush::native_runtime::ActorBehavior;
    using devils_crush::native_runtime::NativeGameplayState;
    using devils_crush::native_runtime::ScoreState;
    using devils_crush::native_runtime::TowerInsectContactDisposition;
    using devils_crush::native_runtime::TowerInsectRuleScheduler;
    using devils_crush::native_runtime::TowerInsectSourceDisposition;
    using devils_crush::native_runtime::TowerInsectGenerationState;
    using devils_crush::native_runtime::TowerInsectSpawnDisposition;
    using devils_crush::native_runtime::TowerInsectSourceState;
    using devils_crush::native_runtime::advance_tower_insect_source;
    using devils_crush::native_runtime::begin_tower_insect_generation;
    using devils_crush::native_runtime::consume_tower_insect_contact;
    using devils_crush::native_runtime::resolve_tower_insect_contact;
    using devils_crush::native_runtime::restart_tower_insect_generation;
    using devils_crush::native_runtime::spawn_tower_insect_child;

    auto source = TowerInsectSourceState{};
    const auto source_started = begin_tower_insect_generation(source, true);
    source = source_started.state;
    const auto first_spawn_request = advance_tower_insect_source(source);
    source = first_spawn_request.state;
    bool spawned_all_children = true;
    for (std::uint8_t count = 0U; count < 4U; ++count) {
        const auto step = spawn_tower_insect_child(source);
        spawned_all_children = spawned_all_children
            && step.disposition == TowerInsectSpawnDisposition::child_spawned;
        source = step.state;
        if (count + 1U < 4U) {
            source.ticks_until_next_spawn = 1U;
            const auto spawn_request = advance_tower_insect_source(source);
            spawned_all_children = spawned_all_children
                && spawn_request.disposition
                    == TowerInsectSourceDisposition::child_spawn_requested;
            source = spawn_request.state;
        }
    }
    check(
        source_started.disposition == TowerInsectSourceDisposition::generation_started
            && source_started.state.generation_active
            && first_spawn_request.disposition
                == TowerInsectSourceDisposition::child_spawn_requested
            && first_spawn_request.requests_child_spawn
            && first_spawn_request.state.ticks_until_next_spawn == 0x30U
            && spawned_all_children && source.spawns_remaining == 0U
            && source.live_children == 4U
            && spawn_tower_insect_child(source).disposition
                == TowerInsectSpawnDisposition::source_exhausted,
        "native Tower-insect source paces and limits one generation to four children");

    const auto survived = consume_tower_insect_contact({}, source);
    const auto completed = consume_tower_insect_contact(
        survived.state, survived.source);
    const auto inactive_source_rejected = consume_tower_insect_contact(
        {}, {.live_children = 1U});
    check(
        survived.disposition == TowerInsectContactDisposition::survived_hit
            && survived.state.hits_remaining == 1U
            && survived.hit_score_award_units == 10U
            && survived.audio_command == 0x29U
            && completed.disposition == TowerInsectContactDisposition::child_completed
            && completed.releases_child && completed.completion_score_award_units == 50U
            && completed.source.live_children == 3U
            && inactive_source_rejected.disposition
                == TowerInsectContactDisposition::invalid_state,
        "native Tower-insect rule preserves its two-hit score and release sequence");

    const auto group_clear = consume_tower_insect_contact(
        {.hits_remaining = 1U},
        {.generation_active = true, .spawns_remaining = 0U, .live_children = 1U});
    const auto scored = resolve_tower_insect_contact(
        {.hits_remaining = 1U},
        {.generation_active = true, .spawns_remaining = 0U, .live_children = 1U},
        {.stored_units = 100U});
    const auto source_clear = devils_crush::native_runtime::advance_tower_insect_source(
        group_clear.source);
    const auto waiting_for_cycle = restart_tower_insect_generation(
        source_clear.state, false);
    const auto restarted_generation = restart_tower_insect_generation(
        source_clear.state, true);
    check(
        group_clear.disposition == TowerInsectContactDisposition::group_clear_armed
            && group_clear.source.group_clear_pending
            && source_clear.disposition
                == TowerInsectSourceDisposition::group_clear_awarded
            && source_clear.score_award_units == 200U
            && waiting_for_cycle.disposition
                == TowerInsectSourceDisposition::waiting_for_next_cycle
            && restarted_generation.disposition
                == TowerInsectSourceDisposition::generation_restarted
            && restarted_generation.state.spawns_remaining == 4U
            && restarted_generation.state.live_children == 0U
            && scored.completion_score_award.state.stored_units == 160U,
        "native Tower-insect source gates its next generation after the 200-unit award");

    ScoreState score{};
    TowerInsectRuleScheduler scheduler(score);
    const auto scheduled_generation_started = scheduler.begin_generation(true);
    const auto first_scheduled_spawn_request = scheduler.advance_source(0x2CU);
    const auto first_spawn = scheduler.spawn_child(0x64U);
    std::optional<devils_crush::native_runtime::TowerInsectSourceStep>
        second_scheduled_spawn_request;
    for (std::uint8_t tick = 0U; tick < 0x30U; ++tick) {
        second_scheduled_spawn_request = scheduler.advance_source(0x2CU);
    }
    const auto second_spawn = scheduler.spawn_child(0x67U);
    const bool source_has_two_children = scheduler.insect(0x64U)
        && scheduler.source().live_children == 2U;
    const bool duplicate_claim_rejected = !scheduler.spawn_child(0x64U)
        && !scheduler.spawn_child(0x65U);
    const devils_crush::native_runtime::ContactDispatch contact{
        .target = {.actor_slot = 0x64U, .marked_for_response = true},
        .disposition = devils_crush::native_runtime::ContactDisposition::dispatched,
    };
    const auto queued_first_hit = scheduler.queue_confirmed_contact(contact);
    const auto scheduled_first_hit = scheduler.advance_actor(0x64U);
    const auto queued_terminal_hit = scheduler.queue_confirmed_contact(contact);
    const auto scheduled_terminal_hit = scheduler.advance_actor(0x64U);
    check(
        scheduled_generation_started
            && scheduled_generation_started->disposition
                == TowerInsectSourceDisposition::generation_started
            && first_scheduled_spawn_request
            && first_scheduled_spawn_request->requests_child_spawn
            && second_scheduled_spawn_request
            && second_scheduled_spawn_request->requests_child_spawn
            && first_spawn && second_spawn && source_has_two_children
            && queued_first_hit && scheduled_first_hit
            && queued_terminal_hit && scheduled_terminal_hit
            && !scheduler.insect(0x64U) && scheduler.source().live_children == 1U
            && score.stored_units == 70U
            && duplicate_claim_rejected,
        "native Tower-insect scheduler owns dynamic child contact, score, and release");

    ScoreState cycle_score{};
    TowerInsectRuleScheduler cycle_scheduler(cycle_score);
    const TowerInsectGenerationState final_child_generation{
        .source = {.generation_active = true, .spawns_remaining = 0U,
                   .live_children = 1U},
        .insects = {devils_crush::native_runtime::TowerInsectState{
            .hits_remaining = 1U}},
    };
    const bool final_child_seeded = cycle_scheduler.seed(final_child_generation);
    const bool final_child_queued = cycle_scheduler.queue_confirmed_contact(contact);
    const auto final_child_completed = cycle_scheduler.advance_actor(0x64U);
    const auto group_awarded = cycle_scheduler.advance_source(0x2CU);
    const auto scheduler_waiting = cycle_scheduler.restart_generation(false);
    const auto scheduler_restarted = cycle_scheduler.restart_generation(true);
    check(
        final_child_seeded && final_child_queued && final_child_completed
            && group_awarded && scheduler_waiting
            && scheduler_waiting->disposition
                == TowerInsectSourceDisposition::waiting_for_next_cycle
            && scheduler_restarted
            && scheduler_restarted->disposition
                == TowerInsectSourceDisposition::generation_restarted
            && cycle_scheduler.source().spawns_remaining == 4U
            && !cycle_scheduler.insect(0x64U) && cycle_score.stored_units == 260U,
        "native Tower-insect scheduler restarts only on the Tower-owned cycle latch");

    NativeGameplayState gameplay;
    const auto native_generation_started = gameplay.begin_tower_insect_generation(true);
    const auto native_spawn_requested = gameplay.advance_tower_insect_source(0x2CU);
    const auto native_spawn = gameplay.spawn_tower_insect_child(0x64U);
    const auto* claimed_insect = gameplay.actors().actor(0x64U);
    const bool insect_claimed = claimed_insect
        && claimed_insect->behavior == ActorBehavior::tower_insect
        && claimed_insect->appearance == 0x39U
        && claimed_insect->collision_effect_association == 0x8CU;
    const auto first_native_contact = gameplay.confirm_actor_contact({
        .actor_slot = 0x64U,
        .response_kind = 2U,
    });
    const auto first_native_hit = gameplay.advance_tower_insect_actor(0x64U);
    gameplay.physics().contact = {};
    const auto terminal_native_contact = gameplay.confirm_actor_contact({
        .actor_slot = 0x64U,
        .response_kind = 2U,
    });
    const auto terminal_native_hit = gameplay.advance_tower_insect_actor(0x64U);
    const auto tower_insect_audio = gameplay.take_audio_events();
    const auto* released_insect = gameplay.actors().actor(0x64U);
    check(
        native_generation_started && native_spawn_requested
            && native_spawn_requested->requests_child_spawn && native_spawn
            && insect_claimed
            && first_native_contact.tower_insect_queued && first_native_hit
            && terminal_native_contact.tower_insect_queued && terminal_native_hit
            && terminal_native_hit->insect.releases_child && released_insect
            && !released_insect->active() && gameplay.score().stored_units == 70U
            && tower_insect_audio
                == std::vector<devils_crush::native_runtime::GameplayAudioEvent>{
                    {
                        .slot = 0x64U,
                        .source = devils_crush::native_runtime::GameplayAudioSource::tower_insect,
                        .command = 0x29U,
                    },
                    {
                        .slot = 0x64U,
                        .source = devils_crush::native_runtime::GameplayAudioSource::tower_insect,
                        .command = 0x29U,
                    },
                },
        "native Tower-insect lifecycle claims/releases its actor and preserves both hit audio events");

    NativeGameplayState effect_gameplay;
    static_cast<void>(effect_gameplay.begin_tower_insect_generation(true));
    static_cast<void>(effect_gameplay.advance_tower_insect_source(0x2CU));
    static_cast<void>(effect_gameplay.spawn_tower_insect_child(0x64U));
    const auto effect_contact = effect_gameplay.confirm_actor_contact({
        .actor_slot = 0x64U,
        .response_kind = 2U,
    });
    const auto effect_hit = effect_gameplay
        .advance_tower_insect_actor_with_collision_effect(0x64U, 0x65U);
    effect_gameplay.physics().contact = {};
    const auto terminal_effect_contact = effect_gameplay.confirm_actor_contact({
        .actor_slot = 0x64U,
        .response_kind = 2U,
    });
    const auto terminal_effect_hit = effect_gameplay
        .advance_tower_insect_actor_with_collision_effect(0x64U, 0x66U);
    const auto effect_audio = effect_gameplay.take_audio_events();
    const auto* active_effect = effect_gameplay.actors().actor(0x65U);
    const auto* completed_child = effect_gameplay.actors().actor(0x64U);
    check(
        effect_contact.tower_insect_queued && effect_hit
            && effect_hit->collision_effect
            && effect_hit->collision_effect->disposition
                == devils_crush::native_runtime::CollisionEffectAllocationDisposition::allocated
            && effect_hit->collision_effect->effect
            && effect_hit->collision_effect->effect->source_appearance == 0x39U
            && effect_hit->collision_effect->effect->source_association == 0x8CU
            && terminal_effect_contact.tower_insect_queued && terminal_effect_hit
            && terminal_effect_hit->collision_effect
            && terminal_effect_hit->collision_effect->disposition
                == devils_crush::native_runtime::CollisionEffectAllocationDisposition::source_already_has_effect
            && active_effect && active_effect->behavior == ActorBehavior::collision_effect
            && completed_child && !completed_child->active()
            && effect_audio
                == std::vector<devils_crush::native_runtime::GameplayAudioEvent>{
                    {
                        .slot = 0x64U,
                        .source = devils_crush::native_runtime::GameplayAudioSource::tower_insect,
                        .command = 0x29U,
                    },
                    {
                        .slot = 0x64U,
                        .source = devils_crush::native_runtime::GameplayAudioSource::tower_insect,
                        .command = 0x29U,
                    },
                },
        "native Tower-insect contacts preserve appearance, effect order, and audio through the shared effect");
}

void test_native_tower_insect_capture()
{
    using devils_crush::native_runtime::CollisionEffectAdvanceDisposition;
    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::NativeGameplayState;
    using devils_crush::native_runtime::apply_native_gameplay_bootstrap;
    using devils_crush::native_runtime::capture_collision_effect;
    using devils_crush::native_runtime::capture_native_gameplay_bootstrap;
    using devils_crush::native_runtime::capture_tower_insect_generation;
    using devils_crush::native_runtime::kTowerInsectSourceSlot;

    std::array<std::byte, GameStateView::kWorkRamSize> work_ram{};
    const auto store = [&work_ram](
                           const std::size_t offset,
                           const std::uint8_t value) {
        work_ram[offset] = static_cast<std::byte>(value);
    };
    const auto seed_actor = [&store](
                                const std::uint8_t slot,
                                const std::uint16_t handler,
                                const std::uint8_t hits) {
        store(0x1000U + slot, 0x82U);
        store(0x1800U + slot, 0x00U);
        store(0x1C00U + slot, hits);
        store(0x1F00U + slot, static_cast<std::uint8_t>(handler));
        store(0x1F80U + slot, static_cast<std::uint8_t>(handler >> 8U));
    };

    seed_actor(kTowerInsectSourceSlot, 0x5F5FU, 0U);
    store(0x1B00U + kTowerInsectSourceSlot, 0x00U);
    store(0x0D2BU, 0x02U);
    store(0x1A00U + kTowerInsectSourceSlot, 0x30U);
    store(0x1900U + kTowerInsectSourceSlot, 0x39U);
    store(0x1D00U + kTowerInsectSourceSlot, 0x8CU);
    seed_actor(0x64U, 0x6016U, 2U);
    seed_actor(0x6AU, 0x6016U, 1U);
    store(0x1900U + 0x64U, 0x39U);
    store(0x1900U + 0x6AU, 0x39U);
    store(0x1D00U + 0x64U, 0x8CU);
    store(0x1D00U + 0x6AU, 0x8CU);

    const GameStateView state(work_ram);
    const auto captured = capture_tower_insect_generation(state);
    NativeGameplayState gameplay;
    const bool seeded = captured
        && gameplay.seed_tower_insect_generation(captured->generation);
    check(
        captured && captured->source_actor.index == kTowerInsectSourceSlot
            && captured->generation.source.spawns_remaining == 0U
            && captured->generation.source.live_children == 2U
            && captured->generation.source.child_appearance == 0x39U
            && captured->generation.source.child_collision_effect_association == 0x8CU
            && captured->generation.insects[0U]
            && captured->generation.insects[0U]->hits_remaining == 2U
            && captured->generation.insects[0U]->appearance == 0x39U
            && captured->generation.insects[0U]->collision_effect_association == 0x8CU
            && captured->generation.insects[2U]
            && captured->generation.insects[2U]->hits_remaining == 1U
            && seeded && gameplay.tower_insect(0x64U)
            && gameplay.tower_insect(0x64U)->hits_remaining == 2U
            && gameplay.tower_insect(0x6AU)
            && gameplay.tower_insect(0x6AU)->hits_remaining == 1U
            && gameplay.actors().actor(0x64U)->appearance == 0x39U
            && gameplay.actors().actor(0x64U)->collision_effect_association == 0x8CU
            && gameplay.actors().actor(0x6AU)->appearance == 0x39U
            && gameplay.actors().actor(0x6AU)->collision_effect_association == 0x8CU,
        "native Tower-insect capture seeds a coherent steady generation");

    store(0x1000U, 0x81U);
    const auto bootstrap = capture_native_gameplay_bootstrap(state);
    NativeGameplayState bootstrapped_gameplay;
    const bool bootstrap_applied = bootstrap
        && apply_native_gameplay_bootstrap(bootstrapped_gameplay, *bootstrap);
    check(
        bootstrap_applied && bootstrap->tower_insects
            && bootstrap->tower_insects->generation == captured
            && bootstrapped_gameplay.tower_insect(0x64U)
            && bootstrapped_gameplay.tower_insect(0x64U)->hits_remaining == 2U
            && bootstrapped_gameplay.tower_insect(0x6AU)
            && bootstrapped_gameplay.tower_insect(0x6AU)->hits_remaining == 1U,
        "native gameplay bootstrap restores a coherent Tower-Insect generation");
    store(0x1000U, 0x00U);

    store(0x1900U + 0x6AU, 0x3AU);
    check(
        !capture_tower_insect_generation(state),
        "native Tower-insect capture rejects a child with a mismatched source appearance");
    store(0x1900U + 0x6AU, 0x39U);
    store(0x1D00U + 0x6AU, 0x7CU);
    check(
        !capture_tower_insect_generation(state),
        "native Tower-insect capture rejects a child with a mismatched source association");
    store(0x1D00U + 0x6AU, 0x8FU);
    check(
        capture_tower_insect_generation(state)
            && capture_tower_insect_generation(state)->generation.insects[2U]
            && capture_tower_insect_generation(state)->generation.insects[2U]
                   ->collision_effect_association
                == 0x8FU,
        "native Tower-insect capture retains a live shared-effect association");
    store(0x1D00U + 0x6AU, 0x8CU);

    store(0x1D00U + 0x64U, 0x8FU);
    store(0x1000U + 0x65U, 0x8FU);
    store(0x1080U + 0x65U, 0xE0U);
    store(0x1900U + 0x65U, 0x39U);
    store(0x1D80U + 0x65U, 0x04U);
    store(0x1E00U + 0x65U, 0x64U);
    store(0x1E80U + 0x65U, 0x8CU);
    store(0x1F00U + 0x65U, 0x34U);
    store(0x1F80U + 0x65U, 0x7CU);
    const auto effect_generation = capture_tower_insect_generation(state);
    const auto effect_capture = capture_collision_effect(state, 0x65U);
    NativeGameplayState effect_gameplay;
    const bool effect_generation_seeded = effect_generation
        && effect_gameplay.seed_tower_insect_generation(effect_generation->generation);
    const bool effect_seeded = effect_capture
        && effect_gameplay.seed_collision_effect(0x65U, effect_capture->effect);
    const auto* effect_source = effect_gameplay.actors().actor(0x64U);
    const auto* captured_effect_actor = effect_gameplay.actors().actor(0x65U);
    check(
        effect_generation_seeded && effect_seeded && effect_source
            && effect_source->collision_effect_active
            && effect_source->collision_effect_association == 0x8FU
            && captured_effect_actor
            && captured_effect_actor->collision_effect
            && captured_effect_actor->collision_effect->ticks_remaining == 4U,
        "native Tower-insect capture seeds an associated live collision effect");

    store(0x1000U, 0x81U);
    const auto effect_bootstrap = capture_native_gameplay_bootstrap(state);
    NativeGameplayState effect_bootstrapped_gameplay;
    const bool effect_bootstrap_applied = effect_bootstrap
        && apply_native_gameplay_bootstrap(effect_bootstrapped_gameplay,
            *effect_bootstrap);
    const auto* bootstrapped_effect = effect_bootstrapped_gameplay.actors().actor(0x65U);
    check(
        effect_bootstrap_applied && effect_bootstrap->tower_insects
            && effect_bootstrap->tower_insects->collision_effects[0U]
                == effect_capture
            && bootstrapped_effect && bootstrapped_effect->collision_effect
            && bootstrapped_effect->collision_effect->ticks_remaining == 4U,
        "native gameplay bootstrap restores a Tower-Insect collision-effect child");
    store(0x1000U, 0x00U);

    std::vector<NativeGameplayState::ActorPhaseUpdate> effect_expiry_updates;
    for (std::uint8_t tick = 0U; tick < 4U; ++tick) {
        effect_expiry_updates = effect_gameplay.advance_actor_phase();
    }
    effect_source = effect_gameplay.actors().actor(0x64U);
    captured_effect_actor = effect_gameplay.actors().actor(0x65U);
    check(
        effect_expiry_updates.size() == 1U
            && effect_expiry_updates.front().kind
                == NativeGameplayState::ActorPhaseUpdateKind::collision_effect
            && effect_expiry_updates.front().collision_effect
            && effect_expiry_updates.front().collision_effect->step.disposition
                == CollisionEffectAdvanceDisposition::expired
            && effect_source && !effect_source->collision_effect_active
            && effect_source->collision_effect_association == 0x8CU
            && captured_effect_actor && !captured_effect_actor->active(),
        "native Tower-insect effect capture restores its source association on expiry");
    store(0x1D00U + 0x64U, 0x8CU);

    store(0x1C00U + 0x6AU, 0x00U);
    check(
        !capture_tower_insect_generation(state),
        "native Tower-insect capture excludes a completed child transition");
    store(0x1C00U + 0x6AU, 0x01U);
    store(0x0D2BU, 0x03U);
    check(
        !capture_tower_insect_generation(state),
        "native Tower-insect capture rejects a mismatched live-child count");
    store(0x0D2BU, 0x02U);
    store(0x1F00U + kTowerInsectSourceSlot, 0x4AU);
    store(0x1B00U + kTowerInsectSourceSlot, 0x02U);
    const auto spawning_capture = capture_tower_insect_generation(state);
    NativeGameplayState spawning_gameplay;
    const bool spawning_seeded = spawning_capture
        && spawning_gameplay.seed_tower_insect_generation(
            spawning_capture->generation);
    static_cast<void>(spawning_gameplay.advance_tower_insect_source(
        kTowerInsectSourceSlot));
    check(
        spawning_capture && spawning_capture->generation.source.spawns_remaining == 2U
            && spawning_capture->generation.source.ticks_until_next_spawn == 0x30U
            && spawning_seeded
            && spawning_gameplay.tower_insect_source().ticks_until_next_spawn
                == 0x2FU,
        "native Tower-insect capture resumes the source spawn countdown");
    store(0x0D2BU, 0x01U);
    store(0x1000U + 0x6AU, 0x00U);
    check(
        capture_tower_insect_generation(state)
            && capture_tower_insect_generation(state)->generation.source.live_children
                == 1U
            && !capture_tower_insect_generation(state)->generation.insects[2U],
        "native Tower-insect capture permits a destroyed child before spawning ends");
    store(0x0D2BU, 0x02U);
    store(0x1000U + 0x6AU, 0x82U);
    store(0x1A00U + kTowerInsectSourceSlot, 0x00U);
    check(
        !capture_tower_insect_generation(state),
        "native Tower-insect capture excludes an expired source countdown");
    store(0x1A00U + kTowerInsectSourceSlot, 0x30U);
    store(0x1B00U + kTowerInsectSourceSlot, 0x03U);
    check(
        !capture_tower_insect_generation(state),
        "native Tower-insect capture rejects a mismatched spawn budget");
    store(0x1B00U + kTowerInsectSourceSlot, 0x00U);
    store(0x1F00U + kTowerInsectSourceSlot, 0x5FU);

    const GameStateView truncated(
        std::span<const std::byte>(work_ram).first(work_ram.size() - 1U));
    check(
        !capture_tower_insect_generation(truncated)
            && !truncated.tower_insect_source(kTowerInsectSourceSlot),
        "native Tower-insect capture rejects a truncated work-RAM image");
}

void test_native_tower_guard()
{
    using devils_crush::native_runtime::NativeGameplayState;
    using devils_crush::native_runtime::TowerGuardSelectorDisposition;
    using devils_crush::native_runtime::kTowerGuardClosedSelector;
    using devils_crush::native_runtime::kTowerGuardOpenSelector;
    using devils_crush::native_runtime::kTowerGuardSlot;
    using devils_crush::native_runtime::select_tower_guard_selector;

    const auto closed = select_tower_guard_selector({.remaining_armed_passes = 5U});
    const auto open = select_tower_guard_selector({.remaining_armed_passes = 0U});
    check(
        closed.disposition == TowerGuardSelectorDisposition::closed
            && closed.completion_selector == kTowerGuardClosedSelector
            && open.disposition
                == TowerGuardSelectorDisposition::open_bonus_stage_entrance
            && open.completion_selector == kTowerGuardOpenSelector,
        "native Tower Guard maps the shared gate to its recovered selectors");

    NativeGameplayState gameplay;
    gameplay.seed_tower({.remaining_armed_passes = 0U});
    gameplay.seed_tower_guard();
    const auto native_open = gameplay.advance_tower_guard_actor(kTowerGuardSlot);
    check(
        !gameplay.advance_tower_guard_actor(0x2DU) && native_open
            && *native_open == open,
        "native gameplay state exposes the Tower Guard selector at its fixed slot");
}

void test_native_score_state()
{
    using devils_crush::native_runtime::ActorBehavior;
    using devils_crush::native_runtime::ActorState;
    using devils_crush::native_runtime::ScoreAwardDisposition;
    using devils_crush::native_runtime::ScoreState;
    using devils_crush::native_runtime::SmallSoldierContactDisposition;
    using devils_crush::native_runtime::SmallSoldierGroupDisposition;
    using devils_crush::native_runtime::SmallSoldierGroupState;
    using devils_crush::native_runtime::apply_score_award;
    using devils_crush::native_runtime::complete_small_soldier_group_child;
    using devils_crush::native_runtime::consume_small_soldier_contact;
    using devils_crush::native_runtime::resolve_small_soldier_completion_animation;
    using devils_crush::native_runtime::resolve_small_soldier_contact;
    using devils_crush::native_runtime::small_soldier_score_events;
    using devils_crush::native_runtime::spawn_small_soldier_child;

    const auto ordinary = apply_score_award({}, 30U);
    check(
        ordinary.disposition == ScoreAwardDisposition::applied
            && ordinary.state.stored_units == 30U && ordinary.state.display_dirty,
        "native score state applies a stored-score award and dirties presentation");
    const auto zero = apply_score_award(ordinary.state, 0U);
    check(
        zero.disposition == ScoreAwardDisposition::applied
            && zero.state == ordinary.state,
        "native score state preserves a zero-unit event without changing state");
    const auto limited = apply_score_award(
        {.stored_units = ScoreState::kExclusiveLimit - 30U}, 30U);
    check(
        limited.disposition == ScoreAwardDisposition::reaches_limit
            && limited.state.stored_units == ScoreState::kExclusiveLimit - 30U,
        "native score state rejects an award that reaches the original limit");
    const auto invalid_seed = apply_score_award(
        {.stored_units = ScoreState::kExclusiveLimit}, 1U);
    check(
        invalid_seed.disposition == ScoreAwardDisposition::reaches_limit
            && invalid_seed.state.stored_units == ScoreState::kExclusiveLimit,
        "native score state rejects an already out-of-range capture seed");

    const auto completed_child = consume_small_soldier_contact({
        .hits_remaining = 1U,
        .contact_pending = true,
    });
    check(
        completed_child.disposition == SmallSoldierContactDisposition::completed,
        "native score events receive a terminal Small-Soldier child result");
    auto group = SmallSoldierGroupState{};
    for (std::uint8_t child = 0U; child < 5U; ++child) {
        group = spawn_small_soldier_child(group).state;
    }
    for (std::uint8_t child = 0U; child < 4U; ++child) {
        group = complete_small_soldier_group_child(group).state;
    }
    const auto final_group = complete_small_soldier_group_child(group);
    check(
        final_group.disposition == SmallSoldierGroupDisposition::source_completed,
        "native score events receive a terminal Small-Soldier source result");
    const auto events = small_soldier_score_events(completed_child, final_group);
    check(
        events.count == 3U && events.awards[0] == 30U
            && events.awards[1] == 50U && events.awards[2] == 200U,
        "native Small-Soldier score events preserve hit, child, then source order");

    ScoreState score{};
    for (std::uint8_t index = 0U; index < events.count; ++index) {
        score = apply_score_award(score, events.awards[index]).state;
    }
    check(
        score.stored_units == 280U && score.display_dirty,
        "native score state consumes all Small-Soldier terminal effects");

    ActorState actors;
    ScoreState ruled_score{};
    check(
        actors.claim_dynamic_slot(0x04U, ActorBehavior::small_soldier)
            && actors.configure_small_soldier(0x04U, {.hits_remaining = 1U}),
        "native Small-Soldier rules receive a configured actor");
    const auto terminal_rule = resolve_small_soldier_contact(
        actors,
        ruled_score,
        0x04U);
    check(
        terminal_rule && terminal_rule->score_award_count == 2U
            && terminal_rule->score_awards[0].awarded_units == 30U
            && terminal_rule->score_awards[1].awarded_units == 50U
            && ruled_score.stored_units == 80U
            && !actors.actor(0x04U)->active(),
        "native Small-Soldier rules apply terminal hit and completion scores separately");

    check(
        actors.claim_dynamic_slot(0x04U, ActorBehavior::small_soldier)
            && actors.configure_small_soldier(0x04U, {
                .hits_remaining = 1U,
                .subtype = 0x0FU,
            }),
        "native Small-Soldier rules receive the deferred subtype");
    const auto deferred_rule = resolve_small_soldier_contact(
        actors,
        ruled_score,
        0x04U);
    const auto deferred_completion_rule = resolve_small_soldier_completion_animation(
        actors,
        ruled_score,
        0x04U);
    check(
        deferred_rule && deferred_rule->score_award_count == 1U
            && deferred_rule->score_awards[0].awarded_units == 30U
            && deferred_completion_rule
            && deferred_completion_rule->score_award_count == 1U
            && deferred_completion_rule->score_awards[0].awarded_units == 50U
            && ruled_score.stored_units == 160U,
        "native Small-Soldier rules preserve the deferred animation score boundary");

    ActorState limited_actors;
    ScoreState limited_score{.stored_units = ScoreState::kExclusiveLimit - 20U};
    check(
        limited_actors.claim_dynamic_slot(0x04U, ActorBehavior::small_soldier)
            && limited_actors.configure_small_soldier(0x04U, {
                .hits_remaining = 1U,
            }),
        "native Small-Soldier rules receive a near-limit score case");
    const auto limited_rule = resolve_small_soldier_contact(
        limited_actors,
        limited_score,
        0x04U);
    check(
        limited_rule && limited_rule->score_award_count == 2U
            && limited_rule->score_awards[0].disposition
                == ScoreAwardDisposition::reaches_limit
            && limited_rule->score_awards[1].disposition
                == ScoreAwardDisposition::reaches_limit
            && limited_score.stored_units == ScoreState::kExclusiveLimit - 20U,
        "native Small-Soldier rules preserve independent score-limit decisions");
}

void test_native_small_soldier_rule_scheduler()
{
    using devils_crush::native_runtime::ActorBehavior;
    using devils_crush::native_runtime::ActorState;
    using devils_crush::native_runtime::ContactDispatch;
    using devils_crush::native_runtime::ContactDisposition;
    using devils_crush::native_runtime::ContactState;
    using devils_crush::native_runtime::ContactTarget;
    using devils_crush::native_runtime::ScoreState;
    using devils_crush::native_runtime::SmallSoldierContactDisposition;
    using devils_crush::native_runtime::SmallSoldierRuleScheduler;
    using devils_crush::native_runtime::dispatch_contact;

    ActorState actors;
    ScoreState score;
    SmallSoldierRuleScheduler scheduler(actors, score);
    constexpr std::uint8_t kSlot = 0x04U;
    check(
        actors.claim_dynamic_slot(kSlot, ActorBehavior::small_soldier)
            && actors.configure_small_soldier(kSlot),
        "native Small-Soldier scheduler receives an initialized actor");

    const auto confirmed = dispatch_contact(
        {},
        {.actor_slot = kSlot, .response_kind = 2U});
    check(
        confirmed.disposition == ContactDisposition::dispatched
            && confirmed.target.marked_for_response
            && scheduler.queue_confirmed_contact(confirmed)
            && scheduler.contact_queued(kSlot)
            && !scheduler.queue_confirmed_contact(confirmed),
        "native Small-Soldier scheduler preserves one confirmed contact until actor order reaches it");
    const auto hit = scheduler.advance_actor(kSlot);
    check(
        hit && hit->actor.step.disposition
                == SmallSoldierContactDisposition::survived_hit
            && hit->score_award_count == 1U
            && hit->score_awards[0].awarded_units == 30U
            && hit->audio_command == 0x21U
            && score.stored_units == 30U
            && !scheduler.contact_queued(kSlot),
        "native Small-Soldier scheduler consumes the mark in the later actor phase");
    check(
        !scheduler.advance_actor(kSlot),
        "native Small-Soldier scheduler does not invent an unmarked second hit");

    const ContactDispatch ignored{
        .disposition = ContactDisposition::contact_already_handled,
    };
    check(
        !scheduler.queue_confirmed_contact(ignored),
        "native Small-Soldier scheduler rejects a contact dispatcher non-result");

    const auto terminal_contact = dispatch_contact(
        ContactState{.last_target = static_cast<std::uint8_t>(0x7FU)},
        {.actor_slot = kSlot, .response_kind = 2U});
    const auto terminal_queued = scheduler.queue_confirmed_contact(terminal_contact);
    const auto terminal = scheduler.advance_actor(kSlot);
    check(
        terminal_queued && terminal
            && terminal->actor.step.disposition
                == SmallSoldierContactDisposition::completed
            && terminal->score_award_count == 2U
            && terminal->score_awards[0].awarded_units == 30U
            && terminal->score_awards[1].awarded_units == 50U
            && terminal->audio_command == 0x21U
            && score.stored_units == 110U
            && !actors.actor(kSlot)->active(),
        "native Small-Soldier scheduler retains ordered hit and completion scores");

    check(
        actors.claim_dynamic_slot(kSlot, ActorBehavior::small_soldier)
            && actors.configure_small_soldier(kSlot, {
                .hits_remaining = 1U,
                .subtype = 0x0FU,
            }),
        "native Small-Soldier scheduler prepares the deferred subtype");
    const auto deferred_contact = dispatch_contact(
        {}, {.actor_slot = kSlot, .response_kind = 2U});
    const auto deferred = scheduler.queue_confirmed_contact(deferred_contact)
        ? scheduler.advance_actor(kSlot)
        : std::optional<devils_crush::native_runtime::SmallSoldierRulesUpdate>{};
    const auto deferred_completion = scheduler.complete_animation(kSlot);
    check(
        deferred && deferred->actor.step.disposition
                == SmallSoldierContactDisposition::awaiting_completion_animation
            && deferred->score_award_count == 1U
            && deferred->audio_command == 0x21U
            && deferred_completion
            && deferred_completion->actor.step.disposition
                == SmallSoldierContactDisposition::completed
            && deferred_completion->score_award_count == 1U
            && deferred_completion->audio_command == 0U
            && score.stored_units == 190U
            && !actors.actor(kSlot)->active(),
        "native Small-Soldier scheduler keeps deferred completion outside the contact queue");

    ActorState stale_actors;
    ScoreState stale_score;
    SmallSoldierRuleScheduler stale_scheduler(stale_actors, stale_score);
    check(
        stale_actors.claim_dynamic_slot(kSlot, ActorBehavior::small_soldier)
            && stale_actors.configure_small_soldier(kSlot)
            && stale_scheduler.queue_confirmed_contact(dispatch_contact(
                {}, {.actor_slot = kSlot, .response_kind = 2U})),
        "native Small-Soldier scheduler queues a contact before a stale-slot check");
    stale_actors.release(kSlot);
    check(
        !stale_scheduler.advance_actor(kSlot)
            && !stale_scheduler.contact_queued(kSlot)
            && stale_score.stored_units == 0U,
        "native Small-Soldier scheduler discards a released slot's stale contact");
}

void test_native_dragon_head()
{
    using devils_crush::native_runtime::DragonHeadContactDisposition;
    using devils_crush::native_runtime::DragonHeadState;
    using devils_crush::native_runtime::NativeGameplayState;
    using devils_crush::native_runtime::complete_dragon_head_hit_presentation;
    using devils_crush::native_runtime::kDragonHeadSlot;
    using devils_crush::native_runtime::resolve_dragon_head_contact;

    const auto ordinary_hit = resolve_dragon_head_contact({}, {});
    auto after_ordinary_hit = ordinary_hit.dragon.state;
    const bool rearmed = complete_dragon_head_hit_presentation(after_ordinary_hit);
    const auto terminal_hit = resolve_dragon_head_contact({
        .hits_remaining = 1U,
    }, {});
    auto terminal_state = terminal_hit.dragon.state;
    check(
        ordinary_hit.dragon.disposition
                == DragonHeadContactDisposition::survived_hit
            && ordinary_hit.dragon.state.hits_remaining == 7U
            && !ordinary_hit.dragon.state.collision_ready
            && ordinary_hit.dragon.hit_score_award_units == 50U
            && ordinary_hit.dragon.audio_command == 0x2EU
            && ordinary_hit.dragon.requests_impact_effect
            && ordinary_hit.dragon.requested_appearance == 3U
            && ordinary_hit.hit_score_award.state.stored_units == 50U
            && rearmed && after_ordinary_hit.collision_ready
            && terminal_hit.dragon.disposition
                == DragonHeadContactDisposition::mouth_opening_requested
            && terminal_hit.dragon.state.hits_remaining == 0U
            && terminal_hit.dragon.state.mouth_opening_transition_pending
            && terminal_hit.dragon.hit_score_award_units == 50U
            && terminal_hit.dragon.terminal_score_award_units == 50U
            && terminal_hit.terminal_score_award.state.stored_units == 100U
            && !complete_dragon_head_hit_presentation(terminal_state),
        "native Dragon's Head preserves its eight-hit score/audio rule and distinct terminal mouth-opening handoff");

    NativeGameplayState gameplay;
    const auto contact = gameplay.confirm_actor_contact({
        .actor_slot = kDragonHeadSlot,
        .response_kind = 2U,
    });
    const auto phase = gameplay.advance_actor_phase();
    const auto audio = gameplay.take_audio_events();
    check(
        contact.dragon_head_queued && phase.size() == 1U
            && phase[0].slot == kDragonHeadSlot
            && phase[0].kind
                == NativeGameplayState::ActorPhaseUpdateKind::dragon_head_contact
            && phase[0].dragon_head
            && gameplay.score().stored_units == 50U
            && audio
                == std::vector<devils_crush::native_runtime::GameplayAudioEvent>{{
                    .slot = kDragonHeadSlot,
                    .source = devils_crush::native_runtime::GameplayAudioSource::dragon_head,
                    .command = 0x2EU,
                }}
            && gameplay.complete_dragon_head_hit_presentation(kDragonHeadSlot)
            && gameplay.dragon_head().collision_ready,
        "native gameplay state queues the Dragon's Head in fixed actor order and hands its audio to the native dispatcher boundary");
}

void test_native_giant_face()
{
    using devils_crush::native_runtime::GiantFaceAnimatorDisposition;
    using devils_crush::native_runtime::GiantFaceAnimatorMode;
    using devils_crush::native_runtime::GiantFaceAnimatorState;
    using devils_crush::native_runtime::GiantFaceControllerDisposition;
    using devils_crush::native_runtime::GiantFaceControllerState;
    using devils_crush::native_runtime::GiantFaceContactDisposition;
    using devils_crush::native_runtime::GiantFaceHelmetDisposition;
    using devils_crush::native_runtime::GiantFaceHelmetSide;
    using devils_crush::native_runtime::GiantFaceHelmetState;
    using devils_crush::native_runtime::GiantFaceState;
    using devils_crush::native_runtime::NativeGameplayState;
    using devils_crush::native_runtime::complete_giant_face_impact_presentation;
    using devils_crush::native_runtime::complete_giant_face_mouth_opening;
    using devils_crush::native_runtime::begin_giant_face_animator;
    using devils_crush::native_runtime::advance_giant_face_animator;
    using devils_crush::native_runtime::advance_giant_face_controller;
    using devils_crush::native_runtime::advance_giant_face_helmet_channel;
    using devils_crush::native_runtime::complete_giant_face_helmet_activation_presentation;
    using devils_crush::native_runtime::complete_giant_face_helmet_ball_return;
    using devils_crush::native_runtime::kGiantFaceCrownSlot;
    using devils_crush::native_runtime::kGiantFaceMouthSlot;
    using devils_crush::native_runtime::resolve_giant_face_crown_contact;
    using devils_crush::native_runtime::resolve_giant_face_mouth_contact;
    using devils_crush::native_runtime::request_giant_face_wake;
    using devils_crush::native_runtime::request_giant_face_transformation;

    const auto crown = resolve_giant_face_crown_contact({}, {});
    auto crown_rearmed = crown.giant_face.state;
    const auto closed_mouth = resolve_giant_face_mouth_contact({}, {});
    auto opened_face = GiantFaceState{};
    const bool mouth_opened = complete_giant_face_mouth_opening(opened_face);
    const auto open_mouth = resolve_giant_face_mouth_contact(opened_face, {});
    check(
        crown.giant_face.disposition == GiantFaceContactDisposition::crown_scored
            && crown.giant_face.score_award_units == 3U
            && crown.giant_face.audio_command == 0x20U
            && crown.giant_face.requests_impact_effect
            && crown.giant_face.state.crown_impact_presentation_pending
            && crown.score_award.state.stored_units == 3U
            && complete_giant_face_impact_presentation(
                crown_rearmed, kGiantFaceCrownSlot)
            && crown_rearmed.crown_collision_ready
            && closed_mouth.giant_face.disposition
                == GiantFaceContactDisposition::closed_mouth_scored
            && mouth_opened && opened_face.mouth_open
            && open_mouth.giant_face.disposition
                == GiantFaceContactDisposition::open_mouth_captured
            && open_mouth.giant_face.score_award_units == 100U
            && open_mouth.giant_face.audio_command == 0x41U
            && open_mouth.giant_face.requests_bonus_stage_4
            && open_mouth.giant_face.places_ball
            && open_mouth.giant_face.ball_horizontal_pixels == 144
            && open_mouth.giant_face.ball_vertical_pixels == 344
            && open_mouth.score_award.state.stored_units == 100U,
        "native Giant Face keeps crown, closed-mouth, and open-mouth collision rules distinct");

    GiantFaceAnimatorState lower_animator{};
    const bool lower_started = begin_giant_face_animator(
        lower_animator, GiantFaceAnimatorMode::lower_forms);
    const auto lower_first = advance_giant_face_animator(lower_animator);
    const auto lower_wait = advance_giant_face_animator(lower_first.state);
    auto lower_cycle = lower_first.state;
    for (std::uint8_t tick = 0U; tick < 7U; ++tick) {
        lower_cycle = advance_giant_face_animator(lower_cycle).state;
    }
    const auto lower_second = advance_giant_face_animator(lower_cycle);

    GiantFaceAnimatorState upper_animator{};
    const bool upper_started = begin_giant_face_animator(
        upper_animator, GiantFaceAnimatorMode::upper_forms);
    const auto upper_first = advance_giant_face_animator(upper_animator);
    const bool idle_rejected = !begin_giant_face_animator(
        upper_animator, GiantFaceAnimatorMode::idle);
    check(
        lower_started
            && lower_first.disposition
                == GiantFaceAnimatorDisposition::form_advanced
            && lower_first.state.displayed_form == 1U
            && lower_first.state.ticks_until_next_form == 8U
            && lower_wait.disposition == GiantFaceAnimatorDisposition::waiting
            && lower_wait.state.ticks_until_next_form == 7U
            && lower_second.disposition
                == GiantFaceAnimatorDisposition::form_advanced
            && lower_second.state.displayed_form == 2U
            && upper_started
            && upper_first.disposition
                == GiantFaceAnimatorDisposition::form_advanced
            && upper_first.state.displayed_form == 4U
            && idle_rejected,
        "native Giant Face animator preserves the source-defined first delay and bounded form ranges");

    const auto helmet_idle = advance_giant_face_helmet_channel({}, 0x2AU, false);
    const auto helmet_wait = advance_giant_face_helmet_channel(
        helmet_idle.state, 0x2AU, false);
    const auto helmet_hit = advance_giant_face_helmet_channel(
        helmet_wait.state, 0x2AU, true);
    const auto helmet_completion = complete_giant_face_helmet_activation_presentation(
        helmet_hit.state, 0x2AU);
    auto helmet_rearmed = helmet_completion.state;
    const bool helmet_returned = complete_giant_face_helmet_ball_return(
        helmet_rearmed, 0x2AU);
    check(
        helmet_idle.disposition == GiantFaceHelmetDisposition::appearance_advanced
            && helmet_idle.state.channels[0U].appearance == 1U
            && helmet_idle.state.channels[0U].ticks_until_appearance == 4U
            && helmet_wait.disposition == GiantFaceHelmetDisposition::timer_waiting
            && helmet_hit.disposition == GiantFaceHelmetDisposition::activation_started
            && helmet_hit.score_award_units == 3U && helmet_hit.audio_command == 0x20U
            && helmet_hit.requests_face_wake
            && helmet_hit.requests_opening_presentation
            && helmet_completion.completed && helmet_completion.audio_command == 0x25U
            && helmet_completion.requests_ball_return
            && helmet_completion.ball_return_side == GiantFaceHelmetSide::left
            && helmet_returned && helmet_rearmed.channels[0U].collision_ready,
        "native Giant Face helmet preserves its idle cadence, wake request, and side-specific ball-return handoff");

    GiantFaceState waking_face{};
    const bool wake_requested = request_giant_face_wake(waking_face);
    const auto wake_started = advance_giant_face_controller({}, waking_face);
    auto controller_state = wake_started.state;
    auto controller_face = wake_started.face_state;
    devils_crush::native_runtime::GiantFaceControllerStep wake_step{};
    bool wake_timing_matches = wake_requested
        && wake_started.disposition == GiantFaceControllerDisposition::wake_started;
    for (std::uint8_t expected_step = 1U; expected_step <= 4U; ++expected_step) {
        if (expected_step > 1U) {
            for (std::uint8_t tick = 0U; tick < 3U; ++tick) {
                const auto wait = advance_giant_face_controller(
                    controller_state, controller_face);
                wake_timing_matches = wake_timing_matches
                    && wait.disposition == GiantFaceControllerDisposition::wake_waiting;
                controller_state = wait.state;
                controller_face = wait.face_state;
            }
        }
        wake_step = advance_giant_face_controller(controller_state, controller_face);
        wake_timing_matches = wake_timing_matches
            && wake_step.state.wake_steps_completed == expected_step
            && wake_step.requests_wake_effect;
        controller_state = wake_step.state;
        controller_face = wake_step.face_state;
    }
    check(
        wake_timing_matches
            && wake_step.disposition == GiantFaceControllerDisposition::wake_completed
            && wake_step.starts_lower_form_animation
            && wake_step.state.persisted_form == 1U
            && !wake_step.face_state.wake_requested,
        "native Giant Face controller consumes the helmet wake through the source-defined four-step cadence");

    bool transformations_match = wake_step.state.accepts_transformations;
    auto transforming_controller = wake_step.state;
    auto transforming_face = wake_step.face_state;
    devils_crush::native_runtime::GiantFaceControllerStep transformation{};
    for (std::uint8_t expected = 2U; expected <= 5U; ++expected) {
        transformations_match = transformations_match
            && request_giant_face_transformation(transforming_face);
        transformation = advance_giant_face_controller(
            transforming_controller, transforming_face);
        transformations_match = transformations_match
            && transformation.disposition
                == GiantFaceControllerDisposition::transformation_selected
            && transformation.state.persisted_form == expected
            && transformation.requests_transformation_effect
            && transformation.starts_upper_form_animation == (expected == 4U);
        transforming_controller = transformation.state;
        transforming_face = transformation.face_state;
    }
    transformations_match = transformations_match
        && request_giant_face_transformation(transforming_face);
    const auto mouth_transformation = advance_giant_face_controller(
        transforming_controller, transforming_face);
    transforming_controller = mouth_transformation.state;
    transforming_face = mouth_transformation.face_state;
    transformations_match = transformations_match
        && mouth_transformation.disposition == GiantFaceControllerDisposition::mouth_opened
        && mouth_transformation.state.persisted_form == 6U
        && mouth_transformation.face_state.mouth_open
        && mouth_transformation.opens_mouth
        && request_giant_face_transformation(transforming_face);
    const auto final_transformation = advance_giant_face_controller(
        transforming_controller, transforming_face);
    check(
        transformations_match
            && final_transformation.disposition
                == GiantFaceControllerDisposition::reset_completed
            && final_transformation.state == GiantFaceControllerState{}
            && final_transformation.stops_form_animation
            && final_transformation.face_state.mouth_open,
        "native Giant Face controller preserves the six selected forms, upper animator handoff, mouth opening, and final dormant reset");

    NativeGameplayState gameplay;
    const auto crown_contact = gameplay.confirm_actor_contact({
        .actor_slot = kGiantFaceCrownSlot,
        .response_kind = 2U,
    });
    const auto crown_phase = gameplay.advance_actor_phase();
    const auto crown_audio = gameplay.take_audio_events();
    const bool crown_ready = gameplay.complete_giant_face_impact_presentation(
        kGiantFaceCrownSlot);

    NativeGameplayState mouth_gameplay;
    mouth_gameplay.seed_ball({
        .position_x = {.raw = 1 * 256},
        .position_y = {.raw = 2 * 256},
    });
    const bool native_mouth_opened = mouth_gameplay.complete_giant_face_mouth_opening();
    const auto mouth_contact = mouth_gameplay.confirm_actor_contact({
        .actor_slot = kGiantFaceMouthSlot,
        .response_kind = 2U,
    });
    const auto mouth_phase = mouth_gameplay.advance_actor_phase();
    const auto mouth_audio = mouth_gameplay.take_audio_events();

    NativeGameplayState animator_gameplay;
    const bool gameplay_animator_started = animator_gameplay.begin_giant_face_animator(
        GiantFaceAnimatorMode::upper_forms);
    const auto animator_phase = animator_gameplay.advance_actor_phase();

    NativeGameplayState helmet_gameplay;
    helmet_gameplay.seed_giant_face_helmet(GiantFaceHelmetState{});
    const auto helmet_contact = helmet_gameplay.confirm_actor_contact({
        .actor_slot = 0x2AU,
        .response_kind = 2U,
    });
    const auto helmet_phase = helmet_gameplay.advance_actor_phase();
    const auto helmet_audio = helmet_gameplay.take_audio_events();
    const auto helmet_presentation =
        helmet_gameplay.complete_giant_face_helmet_activation_presentation(0x2AU);
    const auto helmet_completion_audio = helmet_gameplay.take_audio_events();
    const bool gameplay_transformation_requested =
        helmet_gameplay.request_giant_face_transformation();
    check(
        crown_contact.giant_face_queued && crown_phase.size() == 1U
            && crown_phase[0].slot == kGiantFaceCrownSlot
            && crown_phase[0].kind
                == NativeGameplayState::ActorPhaseUpdateKind::giant_face_contact
            && crown_phase[0].giant_face && crown_ready
            && crown_audio
                == std::vector<devils_crush::native_runtime::GameplayAudioEvent>{ {
                    .slot = kGiantFaceCrownSlot,
                    .source = devils_crush::native_runtime::GameplayAudioSource::giant_face,
                    .command = 0x20U,
                } }
            && native_mouth_opened && mouth_contact.giant_face_queued
            && mouth_phase.size() == 1U && mouth_phase[0].slot == kGiantFaceMouthSlot
            && mouth_phase[0].giant_face
            && mouth_gameplay.score().stored_units == 100U
            && mouth_gameplay.physics().ball.position_x.raw == 144 * 256
            && mouth_gameplay.physics().ball.position_y.raw == 344 * 256
            && mouth_audio
                == std::vector<devils_crush::native_runtime::GameplayAudioEvent>{ {
                    .slot = kGiantFaceMouthSlot,
                    .source = devils_crush::native_runtime::GameplayAudioSource::giant_face,
                    .command = 0x41U,
                } }
            && gameplay_animator_started && animator_phase.size() == 1U
            && animator_phase[0].slot
                == devils_crush::native_runtime::kGiantFaceAnimatorSlot
            && animator_phase[0].kind
                == NativeGameplayState::ActorPhaseUpdateKind::giant_face_animator
            && animator_phase[0].giant_face_animator
            && animator_phase[0].giant_face_animator->state.displayed_form == 4U
            && animator_gameplay.giant_face_animator().displayed_form == 4U
            && helmet_contact.giant_face_helmet_queued && helmet_phase.size() == 3U
            && helmet_phase[0].slot == 0x2AU
            && helmet_phase[0].kind
                == NativeGameplayState::ActorPhaseUpdateKind::giant_face_helmet
            && helmet_phase[0].giant_face_helmet
            && helmet_phase[2].slot
                == devils_crush::native_runtime::kGiantFaceControllerSlot
            && helmet_phase[2].kind
                == NativeGameplayState::ActorPhaseUpdateKind::giant_face_controller
            && helmet_phase[2].giant_face_controller
            && helmet_phase[2].giant_face_controller->disposition
                == GiantFaceControllerDisposition::wake_started
            && helmet_gameplay.score().stored_units == 3U
            && helmet_gameplay.giant_face().wake_requested
            && helmet_audio
                == std::vector<devils_crush::native_runtime::GameplayAudioEvent>{ {
                    .slot = 0x2AU,
                    .source = devils_crush::native_runtime::GameplayAudioSource::giant_face_helmet,
                    .command = 0x20U,
                } }
            && helmet_presentation && helmet_presentation->requests_ball_return
            && helmet_completion_audio
                == std::vector<devils_crush::native_runtime::GameplayAudioEvent>{ {
                    .slot = 0x2AU,
                    .source = devils_crush::native_runtime::GameplayAudioSource::giant_face_helmet,
                    .command = 0x25U,
                } }
            && helmet_gameplay.complete_giant_face_helmet_ball_return(0x2AU),
        "native gameplay runs Giant Face collisions, helmet channels, and hidden animation at their fixed actor turns");
    check(
        gameplay_transformation_requested
            && helmet_gameplay.giant_face().transformation_requested,
        "native gameplay retains a Giant Face transformation signal until the awakened controller owns it");
}

void test_native_dragon_egg_cluster()
{
    using devils_crush::native_runtime::DragonEggClusterState;
    using devils_crush::native_runtime::DragonEggCompletionDisposition;
    using devils_crush::native_runtime::DragonEggFixedResetDisposition;
    using devils_crush::native_runtime::DragonEggGroupResetDisposition;
    using devils_crush::native_runtime::DragonEggHatchlingAdvanceDisposition;
    using devils_crush::native_runtime::DragonEggHatchlingAllocationDisposition;
    using devils_crush::native_runtime::DragonEggHitDisposition;
    using devils_crush::native_runtime::NativeGameplayState;
    using devils_crush::native_runtime::allocate_dragon_egg_hatchling;
    using devils_crush::native_runtime::advance_dragon_egg_fixed_reset;
    using devils_crush::native_runtime::begin_dragon_egg_group_reset;
    using devils_crush::native_runtime::consume_dragon_egg_contact;
    using devils_crush::native_runtime::kDragonEggControllerSlot;
    using devils_crush::native_runtime::resolve_dragon_egg_hatchling;
    using devils_crush::native_runtime::resolve_dragon_egg_contact;
    using devils_crush::native_runtime::resolve_dragon_egg_controller;

    const auto first_hit = resolve_dragon_egg_contact({}, 0x22U, {});
    check(
        first_hit.egg.disposition == DragonEggHitDisposition::destroyed
            && !first_hit.egg.state.eggs[2U].active
            && first_hit.egg.state.eggs[2U].hatchling_spawn_pending
            && first_hit.egg.score_award_units == 10U
            && first_hit.egg.audio_command == 0x39U
            && first_hit.egg.requests_hatchling_spawn
            && first_hit.score_award.state.stored_units == 10U,
        "native Dragon's Egg destroys one fixed target and requests its hatchling");

    const std::array<std::optional<
        devils_crush::native_runtime::DragonEggHatchlingState>, 6U>
        no_hatchlings{};
    const auto allocation = allocate_dragon_egg_hatchling(
        first_hit.egg.state, 0x22U, 0x64U, no_hatchlings);
    const auto animated = resolve_dragon_egg_hatchling(
        *allocation.hatchling, false, first_hit.score_award.state);
    const auto released = resolve_dragon_egg_hatchling(
        animated.hatchling.state, true, animated.score_award.state);
    check(
        allocation.disposition
                == DragonEggHatchlingAllocationDisposition::allocated
            && allocation.hatchling
            && allocation.hatchling->position.horizontal_pixels == 68
            && allocation.hatchling->position.vertical_pixels == 556
            && allocation.hatchling->animation_timer == 3U
            && !allocation.cluster.eggs[2U].hatchling_spawn_pending
            && animated.hatchling.disposition
                == DragonEggHatchlingAdvanceDisposition::animation_advanced
            && animated.hatchling.state.animation_timer == 2U
            && released.hatchling.disposition
                == DragonEggHatchlingAdvanceDisposition::released_on_contact
            && released.hatchling.score_award_units == 200U
            && released.hatchling.audio_command == 0x34U
            && released.score_award.state.stored_units == 210U,
        "native Dragon's-Egg hatchling keeps allocation, animation, and terminal contact distinct");

    auto cluster = first_hit.egg.state;
    auto score = first_hit.score_award.state;
    constexpr std::array<std::uint8_t, 5U> kRemainingEggSlots{
        0x20U, 0x21U, 0x23U, 0x24U, 0x25U};
    for (const auto slot : kRemainingEggSlots) {
        const auto hit = resolve_dragon_egg_contact(cluster, slot, score);
        cluster = hit.egg.state;
        score = hit.score_award.state;
    }
    const auto completion = resolve_dragon_egg_controller(cluster, score);
    const auto duplicate = consume_dragon_egg_contact(completion.controller.state, 0x22U);
    check(
        cluster.completion_pending && !cluster.completed
            && completion.controller.disposition
                == DragonEggCompletionDisposition::completion_requested
            && completion.controller.requests_group_reset
            && completion.controller.score_award_units == 100U
            && completion.score_award.state.stored_units == 160U
            && duplicate.disposition == DragonEggHitDisposition::ignored,
        "native Dragon's-Egg controller keeps group completion separate from the last hit");

    auto reset = begin_dragon_egg_group_reset(completion.controller.state);
    for (const auto slot : kRemainingEggSlots) {
        reset.state = advance_dragon_egg_fixed_reset(reset.state, slot).state;
    }
    const auto final_reset = advance_dragon_egg_fixed_reset(reset.state, 0x22U);
    check(
        reset.disposition == DragonEggGroupResetDisposition::reset_armed
            && final_reset.disposition
                == DragonEggFixedResetDisposition::cluster_reinitialized
            && std::ranges::all_of(final_reset.state.eggs,
                [](const auto& egg) { return egg.active; })
            && !final_reset.state.completed && !final_reset.state.reset_pending,
        "native Dragon's-Egg reset restores each fixed target in actor order");

    NativeGameplayState gameplay;
    gameplay.seed_dragon_egg_cluster(DragonEggClusterState{});
    const auto contact = gameplay.confirm_actor_contact({
        .actor_slot = 0x22U,
        .response_kind = 2U,
    });
    const auto phase = gameplay.advance_actor_phase();
    const auto audio = gameplay.take_audio_events();
    const auto* seeded_cluster = gameplay.dragon_egg_cluster();
    const auto gameplay_allocation = gameplay.allocate_dragon_egg_hatchling(
        0x22U, 0x64U);
    const auto hatchling_phase = gameplay.advance_actor_phase();
    const bool hatchling_alive_after_tick =
        gameplay.dragon_egg_hatchling(0x64U) != nullptr;
    const auto hatchling_contact = gameplay.confirm_actor_contact({
        .actor_slot = 0x64U,
        .response_kind = 2U,
    });
    const auto hatchling_release_phase = gameplay.advance_actor_phase();
    const auto hatchling_audio = gameplay.take_audio_events();
    DragonEggClusterState completed_for_reset{};
    for (auto& egg : completed_for_reset.eggs) {
        egg.active = false;
    }
    completed_for_reset.completed = true;
    NativeGameplayState reset_gameplay;
    reset_gameplay.seed_dragon_egg_cluster(completed_for_reset);
    const auto reset_arm = reset_gameplay.begin_dragon_egg_group_reset();
    const auto reset_phase = reset_gameplay.advance_actor_phase();
    check(
        contact.dragon_egg_queued && phase.size() == 2U
            && phase[0].slot == kDragonEggControllerSlot
            && phase[0].kind
                == NativeGameplayState::ActorPhaseUpdateKind::dragon_egg_controller
            && phase[1].slot == 0x22U
            && phase[1].kind
                == NativeGameplayState::ActorPhaseUpdateKind::dragon_egg_contact
            && phase[1].dragon_egg
            && seeded_cluster && !seeded_cluster->eggs[2U].active
            && audio
                == std::vector<devils_crush::native_runtime::GameplayAudioEvent>{ {
                    .slot = 0x22U,
                    .source = devils_crush::native_runtime::GameplayAudioSource::dragon_egg,
                    .command = 0x39U,
                } }
            && gameplay_allocation && hatchling_alive_after_tick
            && hatchling_phase.size() == 2U
            && hatchling_phase[1].slot == 0x64U
            && hatchling_phase[1].kind
                == NativeGameplayState::ActorPhaseUpdateKind::dragon_egg_hatchling
            && hatchling_contact.dragon_egg_queued
            && hatchling_release_phase.size() == 2U
            && hatchling_release_phase[1].dragon_egg_hatchling
            && hatchling_release_phase[1].dragon_egg_hatchling->hatchling.releases_child
            && gameplay.dragon_egg_hatchling(0x64U) == nullptr
            && hatchling_audio
                == std::vector<devils_crush::native_runtime::GameplayAudioEvent>{ {
                    .slot = 0x64U,
                    .source = devils_crush::native_runtime::GameplayAudioSource::dragon_egg_hatchling,
                    .command = 0x34U,
                } }
            && reset_arm && reset_phase.size() == 7U
            && reset_phase[1].slot == 0x20U
            && reset_phase[1].kind
                == NativeGameplayState::ActorPhaseUpdateKind::dragon_egg_fixed_reset
            && reset_phase.back().slot == 0x25U
            && reset_phase.back().dragon_egg_fixed_reset
            && reset_phase.back().dragon_egg_fixed_reset->disposition
                == DragonEggFixedResetDisposition::cluster_reinitialized,
        "native gameplay state delivers Dragon's-Egg child allocation and reset in actor order");
}

void test_native_giant_skeleton_eyeball()
{
    using devils_crush::native_runtime::DragonEggClusterState;
    using devils_crush::native_runtime::GiantSkeletonEyeballDisposition;
    using devils_crush::native_runtime::GiantSkeletonEyeballFinalStartDisposition;
    using devils_crush::native_runtime::GiantSkeletonEyeballFinalResetDisposition;
    using devils_crush::native_runtime::GiantSkeletonEyeballOpeningStartDisposition;
    using devils_crush::native_runtime::GiantSkeletonEyeballState;
    using devils_crush::native_runtime::NativeGameplayState;
    using devils_crush::native_runtime::advance_giant_skeleton_eyeball;
    using devils_crush::native_runtime::begin_giant_skeleton_eyeball_opening_countdown;
    using devils_crush::native_runtime::begin_giant_skeleton_eyeball_final_animation;
    using devils_crush::native_runtime::complete_giant_skeleton_eyeball_final_reset;
    using devils_crush::native_runtime::kDragonEggControllerSlot;
    using devils_crush::native_runtime::kGiantSkeletonEyeballSlot;

    const auto dormant = advance_giant_skeleton_eyeball({}, false);
    const auto opening = advance_giant_skeleton_eyeball(dormant.state, true);
    const auto waiting = advance_giant_skeleton_eyeball(opening.state, true);
    const auto countdown = begin_giant_skeleton_eyeball_opening_countdown(
        opening.state);
    const auto countdown_wait = advance_giant_skeleton_eyeball(
        countdown.state, true);
    auto expiring_state = countdown_wait.state;
    expiring_state.opening_ticks_remaining = 1U;
    const auto expiry = advance_giant_skeleton_eyeball(expiring_state, true);
    const auto final_start = begin_giant_skeleton_eyeball_final_animation(
        expiry.state, false);
    const auto final_wait = advance_giant_skeleton_eyeball(
        final_start.state, false);
    auto final_expiring_state = final_wait.state;
    final_expiring_state.final_ticks_remaining = 1U;
    final_expiring_state.final_appearance_ticks_remaining = 1U;
    const auto final_expiry = advance_giant_skeleton_eyeball(
        final_expiring_state, false);
    const auto final_reset = complete_giant_skeleton_eyeball_final_reset(
        final_expiry.state);
    const auto extended_final_start = begin_giant_skeleton_eyeball_final_animation(
        expiry.state, true);
    check(
        dormant.disposition == GiantSkeletonEyeballDisposition::dormant
            && opening.disposition
                == GiantSkeletonEyeballDisposition::opening_requested
            && opening.state.opening_transition_pending
            && waiting.disposition
                == GiantSkeletonEyeballDisposition::waiting_for_opening_transition
            && countdown.disposition
                == GiantSkeletonEyeballOpeningStartDisposition::countdown_started
            && countdown.state.opening_ticks_remaining == 8U
            && countdown_wait.disposition
                == GiantSkeletonEyeballDisposition::opening_countdown_wait
            && countdown_wait.state.opening_ticks_remaining == 7U
            && expiry.disposition
                == GiantSkeletonEyeballDisposition::opening_timer_expired
            && expiry.state.opening_expiry_pending
            && final_start.disposition
                == GiantSkeletonEyeballFinalStartDisposition::final_animation_started
            && final_start.state.final_ticks_remaining == 0x00E0U
            && final_wait.disposition
                == GiantSkeletonEyeballDisposition::final_animation_wait
            && final_wait.state.final_ticks_remaining == 0x00DFU
            && final_wait.state.final_appearance == 1U
            && final_expiry.disposition
                == GiantSkeletonEyeballDisposition::final_animation_completed
            && final_expiry.state.final_reset_pending
            && final_reset.disposition
                == GiantSkeletonEyeballFinalResetDisposition::dormant_reinitialized
            && final_reset.state == GiantSkeletonEyeballState{}
            && extended_final_start.disposition
                == GiantSkeletonEyeballFinalStartDisposition::final_animation_started
            && extended_final_start.state.final_ticks_remaining == 0x03E0U,
        "native Giant Skeleton Eyeball turns egg completion into opening and final-animation countdown handoffs");

    DragonEggClusterState completed_eggs{};
    for (auto& egg : completed_eggs.eggs) {
        egg.active = false;
    }
    completed_eggs.completion_pending = true;
    NativeGameplayState gameplay;
    gameplay.seed_dragon_egg_cluster(completed_eggs);
    gameplay.seed_giant_skeleton_eyeball(GiantSkeletonEyeballState{});
    const auto phase = gameplay.advance_actor_phase();
    const auto native_countdown =
        gameplay.begin_giant_skeleton_eyeball_opening_countdown();
    const auto countdown_phase = gameplay.advance_actor_phase();
    NativeGameplayState final_gameplay;
    final_gameplay.seed_giant_skeleton_eyeball({
        .opening_expiry_pending = true,
    });
    const auto native_final =
        final_gameplay.begin_giant_skeleton_eyeball_final_animation(false);
    const auto final_phase = final_gameplay.advance_actor_phase();
    final_gameplay.seed_giant_skeleton_eyeball({
        .final_reset_pending = true,
    });
    const auto native_final_reset =
        final_gameplay.complete_giant_skeleton_eyeball_final_reset();
    check(
        phase.size() == 2U && phase[0].slot == kDragonEggControllerSlot
            && phase[0].kind
                == NativeGameplayState::ActorPhaseUpdateKind::dragon_egg_controller
            && phase[1].slot == kGiantSkeletonEyeballSlot
            && phase[1].kind
                == NativeGameplayState::ActorPhaseUpdateKind::giant_skeleton_eyeball
            && phase[1].giant_skeleton_eyeball
            && phase[1].giant_skeleton_eyeball->disposition
                == GiantSkeletonEyeballDisposition::opening_requested
            && gameplay.giant_skeleton_eyeball()
            && native_countdown && countdown_phase.size() == 2U
            && countdown_phase[1].slot == kGiantSkeletonEyeballSlot
            && countdown_phase[1].giant_skeleton_eyeball
            && countdown_phase[1].giant_skeleton_eyeball->disposition
                == GiantSkeletonEyeballDisposition::opening_countdown_wait
            && gameplay.giant_skeleton_eyeball()->opening_ticks_remaining == 7U
            && native_final && final_phase.size() == 1U
            && final_phase[0].slot == kGiantSkeletonEyeballSlot
            && final_phase[0].giant_skeleton_eyeball
            && final_phase[0].giant_skeleton_eyeball->disposition
                == GiantSkeletonEyeballDisposition::final_animation_wait
            && final_gameplay.giant_skeleton_eyeball()
            && native_final_reset
            && native_final_reset->disposition
                == GiantSkeletonEyeballFinalResetDisposition::dormant_reinitialized
            && *final_gameplay.giant_skeleton_eyeball()
                == GiantSkeletonEyeballState{},
        "native gameplay delivers Dragon's-Egg completion and both Eyeball countdowns in fixed actor order");
}

void test_native_giant_skeleton()
{
    using devils_crush::native_runtime::GiantSkeletonState;
    using devils_crush::native_runtime::GiantSkeletonTargetPosition;
    using devils_crush::native_runtime::GiantSkeletonUpdateDisposition;
    using devils_crush::native_runtime::NativeGameplayState;
    using devils_crush::native_runtime::advance_giant_skeleton;
    using devils_crush::native_runtime::kGiantSkeletonSlot;
    using devils_crush::native_runtime::resolve_giant_skeleton_update;

    const auto idle = advance_giant_skeleton(
        {}, {.horizontal_pixels = 223, .vertical_pixels = 774}, false);
    const auto hit = resolve_giant_skeleton_update(
        idle.state, {.horizontal_pixels = 480, .vertical_pixels = 518}, true, {});
    check(
        idle.disposition == GiantSkeletonUpdateDisposition::idle
            && idle.direction_code == 0x0CU && idle.state.appearance == 6U
            && hit.giant_skeleton.disposition
                == GiantSkeletonUpdateDisposition::repeatable_hit
            && hit.giant_skeleton.direction_code == 0U
            && hit.giant_skeleton.state.appearance == 2U
            && hit.giant_skeleton.hit_score_award_units == 40U
            && hit.giant_skeleton.audio_command == 0x2EU
            && hit.giant_skeleton.requests_impact_presentation
            && hit.giant_skeleton.requested_appearance == 2U
            && hit.hit_score_award.state.stored_units == 40U,
        "native Giant Skeleton preserves its direction-selected appearance and repeatable 40-unit hit");

    NativeGameplayState gameplay;
    gameplay.seed_giant_skeleton(GiantSkeletonState{});
    gameplay.seed_ball({
        .position_x = {.raw = 480 * 256},
        .position_y = {.raw = 518 * 256},
    });
    const auto contact = gameplay.confirm_actor_contact({
        .actor_slot = kGiantSkeletonSlot,
        .response_kind = 2U,
    });
    const auto phase = gameplay.advance_actor_phase();
    const auto audio = gameplay.take_audio_events();
    const auto idle_phase = gameplay.advance_actor_phase();
    check(
        contact.giant_skeleton_queued && phase.size() == 1U
            && phase[0].slot == kGiantSkeletonSlot
            && phase[0].kind
                == NativeGameplayState::ActorPhaseUpdateKind::giant_skeleton_update
            && phase[0].giant_skeleton
            && phase[0].giant_skeleton->giant_skeleton.disposition
                == GiantSkeletonUpdateDisposition::repeatable_hit
            && gameplay.score().stored_units == 40U
            && gameplay.giant_skeleton()
            && gameplay.giant_skeleton()->appearance == 2U
            && audio
                == std::vector<devils_crush::native_runtime::GameplayAudioEvent>{ {
                    .slot = kGiantSkeletonSlot,
                    .source = devils_crush::native_runtime::GameplayAudioSource::giant_skeleton,
                    .command = 0x2EU,
                } }
            && idle_phase.size() == 1U && idle_phase[0].giant_skeleton
            && idle_phase[0].giant_skeleton->giant_skeleton.disposition
                == GiantSkeletonUpdateDisposition::idle
            && gameplay.take_audio_events().empty(),
        "native gameplay state runs the Giant Skeleton in fixed actor order and leaves it live after a hit");
}

void test_native_gameplay_state()
{
    static_assert(!std::is_copy_constructible_v<
                  devils_crush::native_runtime::NativeGameplayState>);
    static_assert(!std::is_copy_assignable_v<
                  devils_crush::native_runtime::NativeGameplayState>);
    static_assert(!std::is_move_constructible_v<
                  devils_crush::native_runtime::NativeGameplayState>);
    static_assert(!std::is_move_assignable_v<
                  devils_crush::native_runtime::NativeGameplayState>);

    using devils_crush::native_runtime::ActorBehavior;
    using devils_crush::native_runtime::BallMotionTuning;
    using devils_crush::native_runtime::CollisionMap;
    using devils_crush::native_runtime::CollisionMask;
    using devils_crush::native_runtime::CollisionRetryFullScanOutcome;
    using devils_crush::native_runtime::CollisionRetryOffsetAttempts;
    using devils_crush::native_runtime::CollisionRetryPlacementDisposition;
    using devils_crush::native_runtime::CollisionRetryPlacementPhase;
    using devils_crush::native_runtime::CollisionRetryPlacementUpdate;
    using devils_crush::native_runtime::CollisionResponseBasis;
    using devils_crush::native_runtime::CollisionResponseConfig;
    using devils_crush::native_runtime::ContactDisposition;
    using devils_crush::native_runtime::ContactTarget;
    using devils_crush::native_runtime::Fixed24_8;
    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::MappedRomView;
    using devils_crush::native_runtime::NativeGameplayState;
    using Cf33ActorContactDisposition =
        devils_crush::native_runtime::NativeGameplayState::Cf33ActorContactDisposition;
    using Cf33PlayfieldContactDisposition =
        devils_crush::native_runtime::NativeGameplayState::Cf33PlayfieldContactDisposition;
    using devils_crush::native_runtime::ActorMotionState;
    using devils_crush::native_runtime::SmallSoldierContactDisposition;

    NativeGameplayState gameplay;
    gameplay.seed_ball({
        .position_x = Fixed24_8{.raw = 16 * 256},
        .position_y = Fixed24_8{.raw = 32 * 256},
        .velocity_x = 0x0100,
    });
    std::array<std::uint8_t, CollisionMap::kColumns> tile_ids{};
    std::array<std::uint8_t, CollisionMap::kColumns> mask_indices{};
    std::array<CollisionMask, 2U> masks{};
    const CollisionMap empty_map{
        .tile_ids = tile_ids,
        .mask_indices = mask_indices,
        .masks = masks,
    };
    const auto clear_frame = gameplay.advance_physics(
        BallMotionTuning{
            .horizontal_drag_divisor = 8,
            .vertical_drag_divisor = 8,
            .vertical_gravity = 1U,
        },
        empty_map);
    check(
        clear_frame && clear_frame->no_contact
            && gameplay.physics() == clear_frame->no_contact->state,
        "native gameplay state commits only a clear native physics frame");

    gameplay.seed_ball({
        .position_x = Fixed24_8{.raw = 15 * 256},
        .position_y = Fixed24_8{.raw = 32 * 256},
        .velocity_x = 0x0100,
    });
    const auto collision_index = 2U;
    tile_ids[collision_index] = 0x7CU;
    mask_indices[collision_index] = 1U;
    masks[1].rows[0] = 0x80U;
    const CollisionMap solid_map{
        .tile_ids = tile_ids,
        .mask_indices = mask_indices,
        .masks = masks,
    };
    const auto physics_before_contact = gameplay.physics();
    const auto blocked_frame = gameplay.advance_physics(
        BallMotionTuning{.horizontal_drag_divisor = 8}, solid_map);
    check(
        blocked_frame && blocked_frame->contact
            && gameplay.pending_playfield_contact() == blocked_frame->contact
            && gameplay.physics() == physics_before_contact
            && !gameplay.advance_physics(
                BallMotionTuning{.horizontal_drag_divisor = 8}, solid_map),
        "native gameplay state retains a solid impact until a resolver commits it");
    auto resolved_ball = blocked_frame && blocked_frame->contact
        ? blocked_frame->contact->entry_ball
        : physics_before_contact.ball;
    resolved_ball.velocity_x = -0x0100;
    check(
        gameplay.commit_playfield_collision(resolved_ball)
            && !gameplay.pending_playfield_contact()
            && gameplay.physics().ball == resolved_ball
            && !gameplay.commit_playfield_collision(resolved_ball),
        "native gameplay state commits one resolved solid impact exactly once");

    gameplay.seed_ball({
        .position_x = Fixed24_8{.raw = 15 * 256},
        .position_y = Fixed24_8{.raw = 32 * 256},
        .velocity_x = 0x0100,
    });
    const auto retry_frame = gameplay.advance_physics(
        BallMotionTuning{.horizontal_drag_divisor = 8}, solid_map);
    const auto retry_probe = gameplay.retry_pending_playfield_collision({
        .by_direction = {{
            {.horizontal_pixels = 1},
        }},
    });
    check(
        retry_frame && retry_frame->contact && retry_probe
            && retry_probe->disposition
                == devils_crush::native_runtime::CollisionRetryProbeDisposition::clear
            && !gameplay.pending_playfield_contact()
            && gameplay.physics().ball == retry_probe->retry.state
            && gameplay.physics().ball.position_x.raw == 16 * 256
            && !gameplay.retry_pending_playfield_collision({}),
        "native gameplay state commits only the recovered clear retry branch");

    std::array<std::uint8_t, CollisionMap::kColumns * 29U> full_scan_tile_ids{};
    std::array<std::uint8_t, CollisionMap::kColumns * 29U> full_scan_mask_indices{};
    constexpr auto full_scan_collision_index = 4U;
    full_scan_tile_ids[full_scan_collision_index + CollisionMap::kColumns * 28U]
        = 0x7CU;
    full_scan_mask_indices[
        full_scan_collision_index + CollisionMap::kColumns * 28U] = 1U;
    auto full_scan_masks = masks;
    full_scan_masks[1].rows[0] = 0x40U;
    const CollisionMap full_scan_map{
        .tile_ids = full_scan_tile_ids,
        .mask_indices = full_scan_mask_indices,
        .masks = full_scan_masks,
    };
    gameplay.seed_ball({
        .position_x = Fixed24_8{.raw = 32 * 256},
        .position_y = Fixed24_8{.raw = 256 * 256},
        .velocity_x = 0x0100,
        .collision_state = 0x80U,
    });
    const auto full_scan_frame = gameplay.advance_physics(
        BallMotionTuning{.horizontal_drag_divisor = 8}, full_scan_map);
    const auto full_scan_retry = gameplay.retry_pending_playfield_collision({
        .by_direction = {{
            {.horizontal_pixels = 1},
        }},
    });
    const auto repeated_full_scan_retry = gameplay.retry_pending_playfield_collision({});
    const auto full_scan_resolution = full_scan_frame && full_scan_frame->contact
        ? full_scan_frame->contact->entry_ball
        : gameplay.physics().ball;
    check(
        full_scan_frame && full_scan_frame->contact && full_scan_retry
            && full_scan_retry->disposition
                == devils_crush::native_runtime::CollisionRetryProbeDisposition::requires_full_scan
            && gameplay.pending_collision_retry_full_scan() == full_scan_retry
            && repeated_full_scan_retry == full_scan_retry
            && gameplay.pending_playfield_contact()
            && gameplay.commit_playfield_collision(full_scan_resolution)
            && !gameplay.pending_collision_retry_full_scan(),
        "native gameplay state retains one full-scan retry until its resolver commits it");

    gameplay.seed_ball({
        .position_x = Fixed24_8{.raw = 32 * 256},
        .position_y = Fixed24_8{.raw = 256 * 256},
        .velocity_x = 0x0100,
        .collision_state = 0x80U,
    });
    const auto direct_placement_frame = gameplay.advance_physics(
        BallMotionTuning{.horizontal_drag_divisor = 8}, full_scan_map);
    const devils_crush::native_runtime::CollisionRetryOffsetAttempts
        placement_offsets{
            .by_direction = {{
                {{{.horizontal_pixels = 1}, {}, {}}},
                {{{.horizontal_pixels = 1}, {}, {}}},
                {{{}, {}, {}}},
                {{{}, {}, {}}},
            }},
        };
    const auto direct_placement_initial = gameplay.begin_pending_collision_retry(
        placement_offsets);
    std::vector<std::byte> direct_scan_rom(
        devils_crush::native_runtime::MappedRomView::kSupportedRomBytes);
    std::array<std::uint8_t, 8U> direct_scan_mpr{};
    direct_scan_mpr[4U] = 0x01U;
    direct_scan_mpr[5U] = 0x02U;
    direct_scan_mpr[6U] = 0x03U;
    std::array<
        std::byte,
        devils_crush::native_runtime::GameStateView::kWorkRamSize>
        direct_scan_ram{};
    const auto store_direct_scan_word = [&direct_scan_ram](
                                           const std::size_t offset,
                                           const std::uint16_t value) {
        direct_scan_ram[offset] = static_cast<std::byte>(value);
        direct_scan_ram[offset + 1U] = static_cast<std::byte>(value >> 8U);
    };
    for (std::size_t index = 0U; index < 4U; ++index) {
        store_direct_scan_word(0x0096U + index * 2U, 0x8000U);
    }
    store_direct_scan_word(0x0074U, 0x8000U);
    store_direct_scan_word(0x0072U, 0x8000U);
    store_direct_scan_word(0x008EU, 0x8000U);
    store_direct_scan_word(0x00C8U, 0x9000U);
    store_direct_scan_word(0x00C6U, 0xA000U);
    store_direct_scan_word(0x0090U, 0xC000U);
    const auto direct_scan_resolution =
        gameplay.resolve_pending_collision_retry_full_scan(
            devils_crush::native_runtime::MappedRomView(
                direct_scan_rom, direct_scan_mpr),
            devils_crush::native_runtime::GameStateView(direct_scan_ram),
            0x01U);
    check(
        direct_placement_frame && direct_placement_frame->contact
            && direct_placement_initial && direct_scan_resolution
            && direct_scan_resolution->scan
            && direct_scan_resolution->scan->disposition
                == devils_crush::native_runtime::CollisionRetryFullScanDisposition::playfield_evaluated
            && direct_scan_resolution->placement.disposition
                == CollisionRetryPlacementDisposition::rearmed
            && gameplay.pending_playfield_contact()
            && !gameplay.pending_collision_retry_placement(),
        "native gameplay state composes its pending CED2/CD74 scan into CDF8");

    gameplay.seed_ball({
        .position_x = Fixed24_8{.raw = 32 * 256},
        .position_y = Fixed24_8{.raw = 256 * 256},
        .velocity_x = 0x0100,
        .collision_state = 0x80U,
    });
    const auto unresolved_scan_frame = gameplay.advance_physics(
        BallMotionTuning{.horizontal_drag_divisor = 8}, full_scan_map);
    const auto unresolved_scan_initial = gameplay.begin_pending_collision_retry(
        placement_offsets);
    const auto unresolved_scan_resolution =
        gameplay.resolve_pending_collision_retry_full_scan(
            devils_crush::native_runtime::MappedRomView(
                std::span<const std::byte>{}, {}),
            devils_crush::native_runtime::GameStateView(direct_scan_ram),
            0x01U);
    check(
        unresolved_scan_frame && unresolved_scan_frame->contact
            && unresolved_scan_initial && unresolved_scan_resolution
            && unresolved_scan_resolution->scan
            && unresolved_scan_resolution->scan->disposition
                == devils_crush::native_runtime::CollisionRetryFullScanDisposition::unresolved_playfield_asset
            && unresolved_scan_resolution->placement.disposition
                == CollisionRetryPlacementDisposition::unresolved
            && gameplay.pending_collision_retry_full_scan()
            && gameplay.pending_collision_retry_placement(),
        "native gameplay state retains an unavailable CED2/CD74 asset as unresolved");

    gameplay.seed_ball({
        .position_x = Fixed24_8{.raw = 32 * 256},
        .position_y = Fixed24_8{.raw = 256 * 256},
        .velocity_x = 0x0100,
        .collision_state = 0x80U,
    });
    const auto placement_frame = gameplay.advance_physics(
        BallMotionTuning{.horizontal_drag_divisor = 8}, full_scan_map);
    const auto placement_initial = gameplay.begin_pending_collision_retry(
        placement_offsets);
    const auto initial_full_scan = gameplay.pending_collision_retry_full_scan();
    const auto placement_unresolved =
        gameplay.resolve_pending_collision_retry_full_scan(
            CollisionRetryFullScanOutcome::unresolved);
    const auto unresolved_full_scan = gameplay.pending_collision_retry_full_scan();
    const auto placement_remains_pending =
        gameplay.pending_collision_retry_placement().has_value();
    const auto placement_second = gameplay.resolve_pending_collision_retry_full_scan(
        CollisionRetryFullScanOutcome::solid);
    const auto second_full_scan = gameplay.pending_collision_retry_full_scan();
    const auto placement_third = gameplay.resolve_pending_collision_retry_full_scan(
        CollisionRetryFullScanOutcome::solid);
    const auto third_full_scan = gameplay.pending_collision_retry_full_scan();
    const auto placement_needs_rng =
        gameplay.resolve_pending_collision_retry_full_scan(
            CollisionRetryFullScanOutcome::solid);
    const auto needs_rng_full_scan = gameplay.pending_collision_retry_full_scan();
    const auto placement_new_direction =
        gameplay.select_pending_collision_retry_direction(0x40U);
    const auto new_direction_full_scan = gameplay.pending_collision_retry_full_scan();
    const auto placement_accepted =
        gameplay.resolve_pending_collision_retry_full_scan(
            CollisionRetryFullScanOutcome::clear);
    check(
        placement_frame && placement_frame->contact && placement_initial
            && placement_initial->disposition
                == CollisionRetryPlacementDisposition::probe_ready
            && initial_full_scan && placement_initial->probe
            && initial_full_scan->retry == *placement_initial->probe,
        "native gameplay state starts CDF8 placement at a pending CED2 scan");
    check(
        placement_unresolved
            && placement_unresolved->disposition
                == CollisionRetryPlacementDisposition::unresolved
            && placement_unresolved->probe == placement_initial->probe
            && unresolved_full_scan == initial_full_scan
            && placement_remains_pending,
        "native gameplay state holds an unresolved CED2 placement candidate");
    check(
        placement_second && placement_second->probe && second_full_scan
            && placement_third && placement_third->probe && third_full_scan
            && placement_needs_rng
            && placement_needs_rng->disposition
                == CollisionRetryPlacementDisposition::needs_random_direction
            && placement_needs_rng->state.phase
                == CollisionRetryPlacementPhase::selecting_direction
            && !needs_rng_full_scan,
        "native gameplay state advances solid CED2 candidates to CDF8's RNG boundary");
    check(
        placement_new_direction && placement_new_direction->probe
            && new_direction_full_scan
            && placement_new_direction->state.attempted_direction_flags == 0xC0U
            && placement_accepted
            && placement_accepted->disposition
                == CollisionRetryPlacementDisposition::rearmed
            && gameplay.pending_playfield_contact()
            && !gameplay.pending_collision_retry_placement()
            && placement_accepted->probe
            && gameplay.physics().ball.position_x
                == placement_accepted->probe->state.position_x
            && gameplay.physics().ball.position_y
                == placement_accepted->probe->state.position_y
            && (gameplay.physics().ball.collision_state & 0x80U) == 0U,
        "native gameplay state rearms a CDF8 probe after its typed CED2 result");

    gameplay.physics().contact = {
        .last_target = 0x2AU,
        .contact_latched = true,
    };
    const auto reseeded_ball = devils_crush::native_runtime::BallState{
        .position_x = Fixed24_8{.raw = 23 * 256},
        .position_y = Fixed24_8{.raw = 41 * 256},
        .velocity_x = -0x0180,
        .velocity_y = 0x0080,
        .motion_flags = 0x82U,
    };
    gameplay.seed_ball(reseeded_ball);
    check(
        gameplay.physics().ball == reseeded_ball
            && gameplay.physics().contact
                == devils_crush::native_runtime::ContactState{}
            && !gameplay.pending_playfield_contact(),
        "native ball seeding starts a clean collision lifetime");

    NativeGameplayState oriented_contact_gameplay;
    oriented_contact_gameplay.seed_ball(reseeded_ball);
    const auto oriented_contact =
        oriented_contact_gameplay.confirm_oriented_actor_contact(
            {.actor_slot = 0x2AU, .response_kind = 2U},
            {},
            CollisionResponseConfig{});
    const auto repeated_oriented_contact =
        oriented_contact_gameplay.confirm_oriented_actor_contact(
            {.actor_slot = 0x2AU, .response_kind = 2U},
            CollisionResponseBasis{},
            {});
    check(
        oriented_contact.contact.contact.disposition
                == ContactDisposition::dispatched
            && oriented_contact.response
            && oriented_contact.response->incoming.horizontal == -0x0180
            && oriented_contact.response->incoming.vertical == 0x0080
            && oriented_contact_gameplay.physics().ball.velocity_x == -0x0180
            && oriented_contact_gameplay.physics().ball.velocity_y == -0x0080
            && repeated_oriented_contact.contact.contact.disposition
                == ContactDisposition::repeated_target
            && !repeated_oriented_contact.response,
        "native gameplay applies a target-selected oriented response once with confirmed actor contact bookkeeping");

    std::vector<std::byte> oriented_contact_rom(
        MappedRomView::kSupportedRomBytes);
    std::array<std::uint8_t, 8U> oriented_contact_mpr{};
    oriented_contact_mpr[6U] = 0x01U;
    constexpr std::size_t kOrientedContactGeometryOffset = MappedRomView::kBankBytes
        + (0xD2AEU & (MappedRomView::kBankBytes - 1U));
    oriented_contact_rom[kOrientedContactGeometryOffset] = std::byte{0x00U};
    oriented_contact_rom[kOrientedContactGeometryOffset + 1U] = std::byte{0x01U};
    std::array<std::byte, GameStateView::kWorkRamSize> oriented_contact_ram{};
    oriented_contact_ram[0x0078U] = std::byte{0xC0U};
    NativeGameplayState asset_oriented_contact_gameplay;
    asset_oriented_contact_gameplay.seed_ball(reseeded_ball);
    const auto asset_oriented_contact =
        asset_oriented_contact_gameplay.confirm_cf33_actor_contact(
            {.index = 0x2AU, .activity_flags = 0x82U},
            MappedRomView(oriented_contact_rom, oriented_contact_mpr),
            GameStateView(oriented_contact_ram));
    oriented_contact_mpr[6U] = 0x80U;
    NativeGameplayState rejected_asset_oriented_contact_gameplay;
    rejected_asset_oriented_contact_gameplay.seed_ball(reseeded_ball);
    check(
        asset_oriented_contact
            && asset_oriented_contact->response
            && asset_oriented_contact_gameplay.physics().ball.velocity_x == -0x0180
            && asset_oriented_contact_gameplay.physics().ball.velocity_y == -0x0040
            && !rejected_asset_oriented_contact_gameplay.confirm_cf33_actor_contact(
                {.index = 0x2AU, .activity_flags = 0x82U},
                MappedRomView(oriented_contact_rom, oriented_contact_mpr),
                GameStateView(oriented_contact_ram))
            && rejected_asset_oriented_contact_gameplay.physics().ball
                == reseeded_ball,
        "native gameplay consumes a captured type-82 CF33 actor and response input once");

    NativeGameplayState suppressed_cf33_contact_gameplay;
    suppressed_cf33_contact_gameplay.seed_ball(reseeded_ball);
    suppressed_cf33_contact_gameplay.physics().contact.last_target =
        static_cast<std::uint8_t>(0x2AU);
    oriented_contact_mpr[6U] = 0x01U;
    const auto suppressed_cf33_resolution =
        suppressed_cf33_contact_gameplay.resolve_cf33_actor_contact(
            {.index = 0x2AU, .activity_flags = 0x82U},
            MappedRomView(oriented_contact_rom, oriented_contact_mpr),
            GameStateView(oriented_contact_ram));
    check(
        suppressed_cf33_resolution.disposition
                == Cf33ActorContactDisposition::contact_not_dispatched
            && suppressed_cf33_resolution.contact
            && !suppressed_cf33_resolution.contact->response
            && suppressed_cf33_contact_gameplay.physics().ball == reseeded_ball,
        "native CF33 distinguishes a dispatcher-suppressed contact from a response");

    NativeGameplayState non_cf33_contact_gameplay;
    non_cf33_contact_gameplay.seed_ball(reseeded_ball);
    check(
        !non_cf33_contact_gameplay.confirm_cf33_actor_contact(
            {.index = 0x2AU, .activity_flags = 0x83U},
            MappedRomView(oriented_contact_rom, oriented_contact_mpr),
            GameStateView(oriented_contact_ram))
            && non_cf33_contact_gameplay.physics().ball == reseeded_ball,
        "native CF33 entrypoint leaves non-type-82 contacts for their own response owner");

    NativeGameplayState clear_selector_cf33_contact_gameplay;
    clear_selector_cf33_contact_gameplay.seed_ball(reseeded_ball);
    oriented_contact_mpr[6U] = 0x01U;
    oriented_contact_ram[0x0078U] = std::byte{0x40U};
    const auto clear_selector_contact =
        clear_selector_cf33_contact_gameplay.confirm_cf33_actor_contact(
            {.index = 0x2AU, .activity_flags = 0x82U},
            MappedRomView(oriented_contact_rom, oriented_contact_mpr),
            GameStateView(oriented_contact_ram));
    check(
        clear_selector_contact && clear_selector_contact->response
            && clear_selector_contact->response->normal.incoming_velocity == 0x0080
            && clear_selector_cf33_contact_gameplay.physics().ball.velocity_x == -0x0180
            && clear_selector_cf33_contact_gameplay.physics().ball.velocity_y == -0x0040,
        "native CF33 clear selector applies CFD8 before reflecting its nonnegative local normal");

    NativeGameplayState clear_selector_playfield_gameplay;
    clear_selector_playfield_gameplay.seed_ball({
        .position_x = Fixed24_8{.raw = 15 * 256},
        .position_y = Fixed24_8{.raw = 32 * 256},
        .velocity_x = 0x0100,
        .velocity_y = 0x0080,
    });
    const auto clear_selector_playfield_frame =
        clear_selector_playfield_gameplay.advance_physics(
            BallMotionTuning{.horizontal_drag_divisor = 8}, solid_map);
    const auto clear_selector_playfield =
        clear_selector_playfield_gameplay.resolve_cf33_playfield_contact(
            MappedRomView(oriented_contact_rom, oriented_contact_mpr),
            GameStateView(oriented_contact_ram));
    check(
        clear_selector_playfield_frame && clear_selector_playfield_frame->contact
            && clear_selector_playfield.disposition
                == Cf33PlayfieldContactDisposition::response_ready
            && clear_selector_playfield.response
            && clear_selector_playfield.resolved_ball.velocity_x == 0x0100
            && clear_selector_playfield.resolved_ball.velocity_y == -0x0040
            && clear_selector_playfield_gameplay.pending_playfield_contact()
            && clear_selector_playfield_gameplay.physics().ball.velocity_y == 0x0080,
        "native CF5B evaluates a pending playfield impact without claiming its frame");
    const auto clear_selector_playfield_resume =
        clear_selector_playfield_gameplay.resume_cf33_playfield_contact(
            MappedRomView(oriented_contact_rom, oriented_contact_mpr),
            GameStateView(oriented_contact_ram),
            BallMotionTuning{.horizontal_drag_divisor = 8},
            empty_map);
    check(
        clear_selector_playfield_resume
            && clear_selector_playfield_resume->response.disposition
                == Cf33PlayfieldContactDisposition::response_ready
            && clear_selector_playfield_resume->completes_frame()
            && !clear_selector_playfield_resume->next_contact
            && !clear_selector_playfield_gameplay.pending_playfield_contact()
            && !clear_selector_playfield_gameplay.pending_playfield_collision_loop()
            && clear_selector_playfield_gameplay.physics().ball.velocity_x == 0x00E0
            && clear_selector_playfield_gameplay.physics().ball.velocity_y == 0,
        "native CF5B resumes DC95 and reaches DD09 before releasing its frame");

    NativeGameplayState separating_playfield_gameplay;
    separating_playfield_gameplay.seed_ball({
        .position_x = Fixed24_8{.raw = 15 * 256},
        // Keep the half-pixel upward substep inside the same collision tile.
        .position_y = Fixed24_8{.raw = 33 * 256},
        .velocity_x = 0x0100,
        .velocity_y = -0x0080,
    });
    auto separating_masks = masks;
    separating_masks[1].rows.fill(0xFFU);
    const CollisionMap separating_solid_map{
        .tile_ids = tile_ids,
        .mask_indices = mask_indices,
        .masks = separating_masks,
    };
    const auto separating_playfield_frame = separating_playfield_gameplay.advance_physics(
        BallMotionTuning{.horizontal_drag_divisor = 8}, separating_solid_map);
    const auto separating_playfield =
        separating_playfield_gameplay.resolve_cf33_playfield_contact(
            MappedRomView(oriented_contact_rom, oriented_contact_mpr),
            GameStateView(oriented_contact_ram));
    oriented_contact_ram[0x0078U] = std::byte{0xC0U};
    separating_playfield_gameplay.physics().ball.collision_state = 0x80U;
    const auto signed_playfield =
        separating_playfield_gameplay.resolve_cf33_playfield_contact(
            MappedRomView(oriented_contact_rom, oriented_contact_mpr),
            GameStateView(oriented_contact_ram));
    check(
        separating_playfield_frame && separating_playfield_frame->contact,
        "native CF33 playfield test establishes a pending solid impact");
    check(
        separating_playfield.disposition
            == Cf33PlayfieldContactDisposition::separating
            && separating_playfield.response,
        "native CF5B recognizes a separating pending playfield impact");
    check(
        separating_playfield_frame && separating_playfield_frame->contact
            && separating_playfield.resolved_ball
                == separating_playfield_frame->contact->impact_ball,
        "native CF5B retains a separating impact ball for its caller");
    check(
        signed_playfield.disposition
                == Cf33PlayfieldContactDisposition::signed_retry_required
            && !signed_playfield.response
            && separating_playfield_gameplay.pending_playfield_contact(),
        "native CF33 retains a signed playfield retry for its frame owner");
    separating_playfield_gameplay.physics().ball.collision_state = 0x00U;
    const auto signed_unmarked_playfield =
        separating_playfield_gameplay.resolve_cf33_playfield_contact(
            MappedRomView(oriented_contact_rom, oriented_contact_mpr),
            GameStateView(oriented_contact_ram));
    check(
        signed_unmarked_playfield.disposition
                != Cf33PlayfieldContactDisposition::signed_retry_required
            && signed_unmarked_playfield.response,
        "native CF33 sends an unmarked signed playfield contact through CF3F");
    oriented_contact_ram[0x0078U] = std::byte{0x40U};

    NativeGameplayState separating_resume_gameplay;
    separating_resume_gameplay.seed_ball({
        .position_x = Fixed24_8{.raw = 15 * 256},
        .position_y = Fixed24_8{.raw = 33 * 256},
        .velocity_x = 0x0100,
        .velocity_y = -0x0080,
    });
    const auto separating_resume_frame = separating_resume_gameplay.advance_physics(
        BallMotionTuning{.horizontal_drag_divisor = 8}, separating_solid_map);
    std::array<std::uint8_t, CollisionMap::kColumns> retry_clear_tile_ids{};
    std::array<std::uint8_t, CollisionMap::kColumns> retry_clear_mask_indices{};
    std::array<CollisionMask, 2U> retry_clear_masks{};
    const CollisionMap retry_clear_map{
        .tile_ids = retry_clear_tile_ids,
        .mask_indices = retry_clear_mask_indices,
        .masks = retry_clear_masks,
    };
    const auto separating_resume =
        separating_resume_gameplay.resume_cf33_playfield_contact(
            MappedRomView(oriented_contact_rom, oriented_contact_mpr),
            GameStateView(oriented_contact_ram),
            BallMotionTuning{.horizontal_drag_divisor = 8},
            retry_clear_map);
    check(
        separating_resume_frame && separating_resume_frame->contact
            && separating_resume
            && separating_resume->response.disposition
                == Cf33PlayfieldContactDisposition::separating
            && separating_resume->completes_frame()
            && !separating_resume_gameplay.pending_playfield_contact()
            && separating_resume_gameplay.physics().ball.position_x.raw == 16 * 256,
        "native CF8F restores and retries its unconsumed substep before DD09");

    NativeGameplayState separating_cf5b_contact_gameplay;
    separating_cf5b_contact_gameplay.seed_ball({
        .velocity_x = -0x0180,
        .velocity_y = -0x0080,
    });
    check(
        !separating_cf5b_contact_gameplay.confirm_cf33_actor_contact(
            {.index = 0x2AU, .activity_flags = 0x82U},
            MappedRomView(oriented_contact_rom, oriented_contact_mpr),
            GameStateView(oriented_contact_ram))
            && separating_cf5b_contact_gameplay.physics().ball.velocity_y == -0x0080,
        "native CF5B leaves separating local normals for its marker-clear return");

    NativeGameplayState marked_cf33_contact_gameplay;
    marked_cf33_contact_gameplay.seed_ball(reseeded_ball);
    oriented_contact_ram[0x0078U] = std::byte{0xC0U};
    oriented_contact_ram[0x1000U] = std::byte{0x80U};
    oriented_contact_ram[0x1100U] = std::byte{0x00U};
    oriented_contact_ram[0x1180U] = std::byte{23U};
    oriented_contact_ram[0x1200U] = std::byte{0x00U};
    oriented_contact_ram[0x1280U] = std::byte{0x00U};
    oriented_contact_ram[0x1300U] = std::byte{41U};
    oriented_contact_ram[0x1380U] = std::byte{0x00U};
    oriented_contact_ram[0x1400U] = std::byte{0x80U};
    oriented_contact_ram[0x1480U] = std::byte{0xFEU};
    oriented_contact_ram[0x1500U] = std::byte{0x80U};
    oriented_contact_ram[0x1580U] = std::byte{0x00U};
    oriented_contact_ram[0x1C80U] = std::byte{0x82U};
    oriented_contact_ram[0x0085U] = std::byte{0x80U};
    oriented_contact_ram[0x0086U] = std::byte{0x34U};
    oriented_contact_ram[0x0087U] = std::byte{0x12U};
    oriented_contact_ram[0x0088U] = std::byte{0x40U};
    oriented_contact_ram[0x0089U] = std::byte{0x80U};
    oriented_contact_ram[0x008AU] = std::byte{0xFEU};
    const auto marked_cf33_resolution =
        marked_cf33_contact_gameplay.resolve_cf33_actor_contact(
            {.index = 0x2AU, .activity_flags = 0x82U, .collision_state = 0x80U},
            MappedRomView(oriented_contact_rom, oriented_contact_mpr),
            GameStateView(oriented_contact_ram));
    check(
        !marked_cf33_contact_gameplay.confirm_cf33_actor_contact(
            {.index = 0x2AU, .activity_flags = 0x82U, .collision_state = 0x80U},
            MappedRomView(oriented_contact_rom, oriented_contact_mpr),
            GameStateView(oriented_contact_ram))
            && marked_cf33_contact_gameplay.physics().ball == reseeded_ball,
        "native CF33 signed route leaves marked contacts for the CDF8 retry owner");
    check(
        marked_cf33_resolution.disposition
                == Cf33ActorContactDisposition::position_retry_ready
            && marked_cf33_resolution.retry_ball
            && marked_cf33_resolution.retry_ball->impact_ball.state == reseeded_ball
            && marked_cf33_resolution.retry_ball->previous_position.position_x.raw
                == 0x00123480
            && marked_cf33_resolution.retry_ball->previous_position.position_y.raw
                == -0x00017FC0
            && marked_cf33_contact_gameplay.physics().ball == reseeded_ball,
        "native CF33 exposes its signed marked CDF8 handoff without changing the ball");
    const auto actor_retry = marked_cf33_resolution.retry_ball
        ? marked_cf33_contact_gameplay.begin_actor_contact_collision_retry(
            *marked_cf33_resolution.retry_ball,
            CollisionRetryOffsetAttempts{})
        : std::optional<CollisionRetryPlacementUpdate>{};
    check(
        actor_retry
            && actor_retry->disposition
                == CollisionRetryPlacementDisposition::accepted
            && actor_retry->probe
            && marked_cf33_contact_gameplay.physics().ball
                == actor_retry->probe->state
            && !marked_cf33_contact_gameplay.pending_playfield_contact()
            && !marked_cf33_contact_gameplay
                    .pending_actor_contact_collision_retry_placement()
            && !marked_cf33_contact_gameplay
                    .pending_actor_contact_collision_retry_full_scan(),
        "native CDF8 actor retry accepts a clear cached-position candidate independently");

    constexpr std::uint8_t kSoldierSlot = 0x04U;
    check(
        gameplay.actors().claim_dynamic_slot(
            kSoldierSlot, ActorBehavior::small_soldier)
            && gameplay.actors().configure_small_soldier(kSoldierSlot),
        "native gameplay state owns an initialized Small-Soldier actor");

    const auto contact = gameplay.confirm_actor_contact({
        .actor_slot = kSoldierSlot,
        .response_kind = 2U,
    });
    check(
        contact.contact.disposition == ContactDisposition::dispatched
            && contact.contact.target.marked_for_response
            && contact.small_soldier_queued
            && gameplay.physics().contact == contact.contact.state,
        "native gameplay state commits contact bookkeeping before queuing the actor rule");
    const auto hit = gameplay.advance_small_soldier_actor(kSoldierSlot);
    const auto direct_small_soldier_audio = gameplay.take_audio_events();
    check(
        hit && hit->actor.step.disposition
                == SmallSoldierContactDisposition::survived_hit
            && gameplay.score().stored_units == 30U
            && direct_small_soldier_audio
                == std::vector<devils_crush::native_runtime::GameplayAudioEvent>{{
                    .slot = kSoldierSlot,
                    .source = devils_crush::native_runtime::GameplayAudioSource::small_soldier,
                    .command = 0x21U,
                }}
            && gameplay.take_audio_events().empty(),
        "native gameplay state delivers a direct Small-Soldier hit to the native audio handoff exactly once");

    constexpr std::uint8_t kMotionSlot = 0x05U;
    check(
        gameplay.actors().claim_dynamic_slot(kMotionSlot, ActorBehavior::shared_motion),
        "native gameplay state prepares a due shared-motion actor");
    auto* motion_actor = gameplay.actors().actor(kMotionSlot);
    if (motion_actor != nullptr) {
        motion_actor->due_for_update = true;
        motion_actor->motion = ActorMotionState{
            .position_x = Fixed24_8{.raw = 10 * 256},
            .velocity_x = 0x0100,
            .integrate_x = true,
        };
    }
    gameplay.physics().contact = {};
    const auto ordered_contact = gameplay.confirm_actor_contact({
        .actor_slot = kSoldierSlot,
        .response_kind = 2U,
    });
    auto* soldier_actor = gameplay.actors().actor(kSoldierSlot);
    if (soldier_actor != nullptr) {
        soldier_actor->due_for_update = true;
    }
    const auto actor_phase = gameplay.advance_actor_phase();
    const auto actor_phase_audio = gameplay.take_audio_events();
    check(
        ordered_contact.small_soldier_queued && actor_phase.size() == 2U
            && actor_phase[0].slot == kSoldierSlot
            && actor_phase[0].kind
                == NativeGameplayState::ActorPhaseUpdateKind::small_soldier_contact
            && actor_phase[0].small_soldier
            && actor_phase[1].slot == kMotionSlot
            && actor_phase[1].kind
                == NativeGameplayState::ActorPhaseUpdateKind::shared_motion
            && actor_phase[1].motion
            && actor_phase[1].motion->step.state.position_x.raw == 11 * 256
            && actor_phase_audio
                == std::vector<devils_crush::native_runtime::GameplayAudioEvent>{{
                    .slot = kSoldierSlot,
                    .source = devils_crush::native_runtime::GameplayAudioSource::small_soldier,
                    .command = 0x21U,
                }},
        "native gameplay state applies recovered behaviors and their audio commands in actor slot order");

    constexpr std::uint8_t kOtherSlot = 0x06U;
    check(
        gameplay.actors().claim_dynamic_slot(kOtherSlot, ActorBehavior::shared_motion),
        "native gameplay state accepts a non-Small-Soldier actor");
    const auto other_contact = gameplay.confirm_actor_contact({
        .actor_slot = kOtherSlot,
        .response_kind = 2U,
    });
    check(
        other_contact.contact.disposition == ContactDisposition::dispatched
            && !other_contact.small_soldier_queued
            && gameplay.physics().contact.last_target == kOtherSlot
            && !gameplay.physics().contact.contact_latched,
        "native gameplay state preserves non-Small-Soldier contact handoff");

    auto* other_actor = gameplay.actors().actor(kOtherSlot);
    if (other_actor != nullptr) {
        other_actor->due_for_update = true;
        other_actor->motion = ActorMotionState{
            .position_x = Fixed24_8{.raw = 20 * 256},
            .velocity_x = 0x0100,
            .integrate_x = true,
        };
    }
    const auto isolated_actor_phase = gameplay.advance_actor_phase_slot(kOtherSlot);
    check(
        isolated_actor_phase
            && isolated_actor_phase->slot == kOtherSlot
            && isolated_actor_phase->kind
                == NativeGameplayState::ActorPhaseUpdateKind::shared_motion
            && isolated_actor_phase->motion
            && isolated_actor_phase->motion->step.state.position_x.raw == 21 * 256
            && motion_actor && motion_actor->motion.position_x.raw == 11 * 256
            && !gameplay.advance_actor_phase_slot(
                devils_crush::native_runtime::ActorState::kSlotCount),
        "native gameplay state advances a selected actor without touching other slots");

    if (other_actor != nullptr) {
        other_actor->due_for_update = false;
    }
    if (motion_actor != nullptr) {
        motion_actor->due_for_update = false;
    }
    gameplay.physics().contact = {};
    check(
        gameplay.seed_bumper({.side = 0U}),
        "native gameplay state accepts a captured left-Bumper state");
    const auto score_before_bumper = gameplay.score().stored_units;
    const auto bumper_contact = gameplay.confirm_actor_contact({
        .actor_slot = devils_crush::native_runtime::kLeftBumperSlot,
        .response_kind = 2U,
    });
    const auto bumper_phase = gameplay.advance_actor_phase();
    const auto* left_bumper = gameplay.bumper(0U);
    check(
        bumper_contact.contact.disposition == ContactDisposition::dispatched
            && bumper_contact.bumper_queued
            && !bumper_contact.small_soldier_queued
            && bumper_phase.size() == 1U
            && bumper_phase[0].slot
                == devils_crush::native_runtime::kLeftBumperSlot
            && bumper_phase[0].kind
                == NativeGameplayState::ActorPhaseUpdateKind::bumper_contact
            && bumper_phase[0].bumper
            && bumper_phase[0].bumper->bumper.disposition
                == devils_crush::native_runtime::BumperHitDisposition::primary_damage
            && left_bumper && left_bumper->hit_phase == 1U
            && gameplay.score().stored_units == score_before_bumper + 10U,
        "native gameplay state resolves a queued fixed Bumper in slot order");

    gameplay.physics().contact = {};
    check(
        gameplay.seed_front_spider({.side = 1U}),
        "native gameplay state accepts a captured right Front-Spider state");
    const auto score_before_front_spider = gameplay.score().stored_units;
    const auto front_spider_contact = gameplay.confirm_actor_contact({
        .actor_slot = devils_crush::native_runtime::kRightFrontSpiderSlot,
        .response_kind = 2U,
    });
    const auto front_spider_phase = gameplay.advance_actor_phase();
    const auto* right_front_spider = gameplay.front_spider(1U);
    check(
        front_spider_contact.contact.disposition == ContactDisposition::dispatched
            && front_spider_contact.front_spider_queued
            && front_spider_phase.size() == 2U
            && front_spider_phase[0].slot
                == devils_crush::native_runtime::kRightFrontSpiderSlot
            && front_spider_phase[0].kind
                == NativeGameplayState::ActorPhaseUpdateKind::front_spider_contact
            && front_spider_phase[0].front_spider
            && front_spider_phase[0].front_spider->spider.disposition
                == devils_crush::native_runtime::FrontSpiderContactDisposition::hit
            && front_spider_phase[1].slot
                == devils_crush::native_runtime::kRightSkeletonInsectSlot
            && front_spider_phase[1].kind
                == NativeGameplayState::ActorPhaseUpdateKind::skeleton_insect_trigger
            && front_spider_phase[1].skeleton_insect
            && front_spider_phase[1].skeleton_insect->insect.disposition
                == devils_crush::native_runtime::SkeletonInsectTriggerDisposition::triggered
            && right_front_spider && !right_front_spider->collision_ready
            && gameplay.table_rule_latches().right_front_spider_safety
            && gameplay.score().stored_units == score_before_front_spider + 80U
            && gameplay.complete_front_spider_transition(
                devils_crush::native_runtime::kRightFrontSpiderSlot)
            && gameplay.front_spider(1U)->collision_ready
            && gameplay.complete_skeleton_insect_transition(
                devils_crush::native_runtime::kRightSkeletonInsectSlot),
        "native gameplay state carries a Front-Spider safety latch to its later Skeleton Insect in slot order");

    gameplay.physics().contact = {};
    check(
        gameplay.seed_side_spider({.side = 0U}),
        "native gameplay state accepts a captured left Side-Spider state");
    const auto score_before_side_spider = gameplay.score().stored_units;
    const auto side_spider_contact = gameplay.confirm_actor_contact({
        .actor_slot = devils_crush::native_runtime::kLeftSideSpiderSlot,
        .response_kind = 2U,
    });
    const auto side_spider_phase = gameplay.advance_actor_phase();
    const auto* left_side_spider = gameplay.side_spider(0U);
    check(
        side_spider_contact.contact.disposition == ContactDisposition::dispatched
            && side_spider_contact.side_spider_queued
            && side_spider_phase.size() == 1U
            && side_spider_phase[0].slot
                == devils_crush::native_runtime::kLeftSideSpiderSlot
            && side_spider_phase[0].kind
                == NativeGameplayState::ActorPhaseUpdateKind::side_spider_contact
            && side_spider_phase[0].side_spider
            && side_spider_phase[0].side_spider->spider.disposition
                == devils_crush::native_runtime::SideSpiderContactDisposition::hit
            && left_side_spider && !left_side_spider->collision_ready
            && gameplay.score().stored_units == score_before_side_spider + 100U
            && gameplay.complete_side_spider_transition(
                devils_crush::native_runtime::kLeftSideSpiderSlot)
            && gameplay.side_spider(0U)->collision_ready,
        "native gameplay state resolves a Side Spider in slot order and preserves its transition boundary");

    gameplay.physics().contact = {};
    gameplay.seed_tower({});
    gameplay.table_rule_latches().tower_completed = false;
    const auto score_before_tower = gameplay.score().stored_units;
    const auto tower_contact = gameplay.confirm_actor_contact({
        .actor_slot = devils_crush::native_runtime::kTowerSlot,
        .response_kind = 2U,
    });
    const auto tower_phase = gameplay.advance_actor_phase();
    const auto fixed_target_audio = gameplay.take_audio_events();
    check(
        tower_contact.contact.disposition == ContactDisposition::dispatched
            && tower_contact.tower_queued && tower_phase.size() == 2U
            && tower_phase[0].slot == devils_crush::native_runtime::kTowerSlot
            && tower_phase[0].kind
                == NativeGameplayState::ActorPhaseUpdateKind::tower_contact
            && tower_phase[0].tower
            && tower_phase[0].tower->tower.disposition
                == devils_crush::native_runtime::TowerContactDisposition::first_source_hit
            && tower_phase[1].slot
                == devils_crush::native_runtime::kCenterSkeletonInsectSlot
            && tower_phase[1].kind
                == NativeGameplayState::ActorPhaseUpdateKind::skeleton_insect_trigger
            && tower_phase[1].skeleton_insect
            && gameplay.table_rule_latches().tower_completed
            && !gameplay.tower().collision_ready
            && gameplay.score().stored_units == score_before_tower + 150U
            && fixed_target_audio
                == std::vector<devils_crush::native_runtime::GameplayAudioEvent>{
                    {
                        .slot = devils_crush::native_runtime::kLeftBumperSlot,
                        .source = devils_crush::native_runtime::GameplayAudioSource::bumper,
                        .command = 0x20U,
                    },
                    {
                        .slot = devils_crush::native_runtime::kRightFrontSpiderSlot,
                        .source = devils_crush::native_runtime::GameplayAudioSource::front_spider,
                        .command = 0x34U,
                    },
                    {
                        .slot = devils_crush::native_runtime::kLeftSideSpiderSlot,
                        .source = devils_crush::native_runtime::GameplayAudioSource::side_spider,
                        .command = 0x34U,
                    },
                    {
                        .slot = devils_crush::native_runtime::kTowerSlot,
                        .source = devils_crush::native_runtime::GameplayAudioSource::tower,
                        .command = 0x33U,
                    },
                }
            && gameplay.complete_skeleton_insect_transition(
                devils_crush::native_runtime::kCenterSkeletonInsectSlot),
        "native gameplay state carries a Tower source hit to the center Skeleton Insect in slot order");

    const auto tower_presentation_one = gameplay.advance_tower_presentation(
        devils_crush::native_runtime::kTowerSlot);
    const auto tower_presentation_two = gameplay.advance_tower_presentation(
        devils_crush::native_runtime::kTowerSlot);
    const auto tower_presentation_three = gameplay.advance_tower_presentation(
        devils_crush::native_runtime::kTowerSlot);
    const auto tower_presentation_install = gameplay.advance_tower_presentation(
        devils_crush::native_runtime::kTowerSlot);
    check(
        tower_presentation_one && tower_presentation_two && tower_presentation_three
            && tower_presentation_install
            && tower_presentation_one->disposition
                == devils_crush::native_runtime::TowerPresentationDisposition::countdown
            && tower_presentation_install->disposition
                == devils_crush::native_runtime::TowerPresentationDisposition::steady_dispatch_installed
            && gameplay.tower().collision_ready,
        "native gameplay state advances all recovered Tower presentation updates before rearming");

    gameplay.physics().contact = {};
    const auto score_before_armed_pass = gameplay.score().stored_units;
    const auto armed_pass_contact = gameplay.confirm_actor_contact({
        .actor_slot = devils_crush::native_runtime::kTowerSlot,
        .response_kind = 2U,
    });
    const auto armed_pass_phase = gameplay.advance_actor_phase();
    check(
        armed_pass_contact.tower_queued && armed_pass_phase.size() == 1U
            && armed_pass_phase[0].kind
                == NativeGameplayState::ActorPhaseUpdateKind::tower_contact
            && armed_pass_phase[0].tower
            && armed_pass_phase[0].tower->tower.disposition
                == devils_crush::native_runtime::TowerContactDisposition::armed_pass_decremented
            && gameplay.tower().remaining_armed_passes == 4U
            && gameplay.score().stored_units == score_before_armed_pass,
        "native gameplay state advances later Tower gate passes without replaying the source award");

    gameplay.seed_tower({
        .collision_ready = false,
        .hit_progress = 1U,
        .remaining_armed_passes = 0U,
        .bonus_stage_selection_pending = true,
    });
    const auto tower_bonus_wait = gameplay.advance_tower_bonus_stage_selection(
        devils_crush::native_runtime::kTowerSlot, false);
    const auto tower_bonus_selection = gameplay.advance_tower_bonus_stage_selection(
        devils_crush::native_runtime::kTowerSlot, true);
    const auto tower_bonus_audio = gameplay.take_audio_events();
    check(
        !gameplay.advance_tower_bonus_stage_selection(0x2CU, true)
            && tower_bonus_wait
            && tower_bonus_wait->disposition
                == devils_crush::native_runtime::TowerBonusStageSelectionDisposition::waiting_for_collision
            && tower_bonus_selection
            && tower_bonus_selection->requests_session_transition
            && tower_bonus_selection->bonus_stage == 5U
            && gameplay.tower().bonus_stage_transition_requested
            && tower_bonus_audio
                == std::vector<devils_crush::native_runtime::GameplayAudioEvent>{{
                    .slot = devils_crush::native_runtime::kTowerSlot,
                    .source = devils_crush::native_runtime::GameplayAudioSource::tower_bonus_stage_selection,
                    .command = 0x41U,
                }},
        "native gameplay state exposes Tower BS-5 selection and its audio event only through its pending collision owner");

    gameplay.seed_score({
        .stored_units = 0x123456U,
        .display_dirty = true,
    });
    check(
        gameplay.score()
            == devils_crush::native_runtime::ScoreState{
                .stored_units = 0x123456U,
                .display_dirty = true,
            },
        "native gameplay state accepts a captured score as its scoring authority");
}

void test_native_gameplay_audio_dispatch()
{
    using devils_crush::native_runtime::GameplayAudioDispatchDisposition;
    using devils_crush::native_runtime::GameplayAudioDispatchPolicy;
    using devils_crush::native_runtime::GameplayAudioEvent;
    using devils_crush::native_runtime::GameplayAudioSource;
    using devils_crush::native_runtime::dispatch_gameplay_audio_event;
    using devils_crush::native_runtime::dispatch_gameplay_audio_events;

    const auto ordinary = dispatch_gameplay_audio_event({
        .slot = 0x14U,
        .source = GameplayAudioSource::bumper,
        .command = 0x20U,
    });
    const auto sentinel = dispatch_gameplay_audio_event({
        .slot = 0x2DU,
        .source = GameplayAudioSource::tower,
        .command = 0xFFU,
    });
    const auto protected_while_locked = dispatch_gameplay_audio_event(
        {
            .slot = 0x26U,
            .source = GameplayAudioSource::front_spider,
            .command = 0x31U,
        },
        {.audio_locked = true});
    const auto ordinary_while_locked = dispatch_gameplay_audio_event(
        {
            .slot = 0x64U,
            .source = GameplayAudioSource::tower_insect,
            .command = 0x29U,
        },
        {.audio_locked = true});
    check(
        ordinary.forwards_to_audio_driver()
            && sentinel.disposition
                == GameplayAudioDispatchDisposition::ignored_sentinel
            && !sentinel.forwards_to_audio_driver()
            && protected_while_locked.disposition
                == GameplayAudioDispatchDisposition::suppressed_while_audio_locked
            && !protected_while_locked.forwards_to_audio_driver()
            && ordinary_while_locked.forwards_to_audio_driver(),
        "native game-to-audio dispatch preserves ECFD's sentinel and locked-command gates");

    constexpr std::array<GameplayAudioEvent, 4U> events{{
        {
            .slot = 0x14U,
            .source = GameplayAudioSource::bumper,
            .command = 0x20U,
        },
        {
            .slot = 0x26U,
            .source = GameplayAudioSource::front_spider,
            .command = 0x27U,
        },
        {
            .slot = 0x2DU,
            .source = GameplayAudioSource::tower,
            .command = 0x47U,
        },
        {
            .slot = 0x64U,
            .source = GameplayAudioSource::tower_insect,
            .command = 0x29U,
        },
    }};
    const auto batch = dispatch_gameplay_audio_events(
        events, GameplayAudioDispatchPolicy{.audio_locked = true});
    check(
        batch.size() == events.size() && batch[0].event == events[0]
            && batch[0].forwards_to_audio_driver() && batch[1].event == events[1]
            && batch[1].disposition
                == GameplayAudioDispatchDisposition::suppressed_while_audio_locked
            && batch[2].event == events[2]
            && batch[2].disposition
                == GameplayAudioDispatchDisposition::suppressed_while_audio_locked
            && batch[3].event == events[3] && batch[3].forwards_to_audio_driver(),
        "native game-to-audio dispatch retains source-tagged actor order through the synchronous gate");
}

void test_native_ball_motion()
{
    using devils_crush::native_runtime::BallState;
    using devils_crush::native_runtime::BallMotionIncrement;
    using devils_crush::native_runtime::BallSubstepProgress;
    using devils_crush::native_runtime::BallMotionTuning;
    using devils_crush::native_runtime::Fixed24_8;
    using devils_crush::native_runtime::apply_ball_drag_and_gravity;
    using devils_crush::native_runtime::advance_ball_motion;
    using devils_crush::native_runtime::advance_ball_motion_substep;
    using devils_crush::native_runtime::advance_ball_motion_substeps;
    using devils_crush::native_runtime::advance_prepared_ball_motion_substeps;
    using devils_crush::native_runtime::rollback_ball_position;
    using devils_crush::native_runtime::split_ball_motion_substeps;

    BallState ball{
        .position_x = Fixed24_8{.raw = 12 * 256 + 128},
        .position_y = Fixed24_8{.raw = 20 * 256 + 64},
        .velocity_x = 384,
        .velocity_y = -320,
        .motion_flags = 0xA7U,
    };
    const auto moved = advance_ball_motion(ball);
    check(
        moved.previous == ball && moved.state.position_x.raw == 14 * 256
            && moved.state.position_y.raw == 19 * 256,
        "native ball motion integrates both signed 8.8 velocity components");
    check(
        moved.crossed_whole_pixel && moved.state.motion_flags == 0xA5U,
        "native ball motion clears the fractional-only marker after an integer step");

    const auto split_motion = advance_ball_motion_substep(
        ball,
        {
            .horizontal = 96,
            .vertical = -80,
        });
    check(
        split_motion.state.position_x.raw == 12 * 256 + 224
            && split_motion.state.position_y.raw == 19 * 256 + 240
            && split_motion.state.velocity_x == ball.velocity_x
            && split_motion.state.velocity_y == ball.velocity_y,
        "native ball substep integrates DC95's increment without replacing frame velocity");

    ball.position_x = Fixed24_8{.raw = 12 * 256 + 1};
    ball.position_y = Fixed24_8{.raw = 20 * 256 + 2};
    ball.velocity_x = 16;
    ball.velocity_y = -1;
    ball.motion_flags = 0xA5U;
    const auto fractional = advance_ball_motion(ball);
    check(
        !fractional.crossed_whole_pixel && fractional.state.motion_flags == 0xA7U,
        "native ball motion marks an update that remains within the same pixels");

    ball.position_x = Fixed24_8{.raw = 0x7FFFFFU};
    ball.position_y = Fixed24_8{.raw = -0x800000};
    ball.velocity_x = 1;
    ball.velocity_y = -1;
    const auto wrapped = advance_ball_motion(ball);
    check(
        wrapped.state.position_x.raw == -0x800000
            && wrapped.state.position_y.raw == 0x7FFFFF,
        "native ball motion preserves signed 24-bit coordinate wrap");

    const auto rollback = rollback_ball_position(
        {
            .position_x = Fixed24_8{.raw = 28 * 256 + 64},
            .position_y = Fixed24_8{.raw = 44 * 256 + 128},
            .velocity_x = -0x0180,
            .velocity_y = 0x0240,
            .motion_flags = 0xA2U,
        },
        {
            .position_x = Fixed24_8{.raw = 26 * 256 + 192},
            .position_y = Fixed24_8{.raw = 41 * 256 + 16},
            .velocity_x = 0x0100,
            .velocity_y = -0x0100,
            .motion_flags = 0x80U,
        });
    check(
        rollback.position_x.raw == 26 * 256 + 192
            && rollback.position_y.raw == 41 * 256 + 16
            && rollback.velocity_x == -0x0180
            && rollback.velocity_y == 0x0240
            && rollback.motion_flags == 0xA2U,
        "native ball rollback restores positions without undoing response state");

    const auto single_step = split_ball_motion_substeps(BallState{
        .velocity_x = 0x0340,
        .velocity_y = 0x0180,
    });
    check(
        single_step.count == 1U && single_step.shifts == 0U
            && single_step.maximum_whole_speed == 3U
            && single_step.velocity_x_per_step == 0x0340
            && single_step.velocity_y_per_step == 0x0180,
        "native ball physics keeps slow motion in one collision step");

    const auto half_steps = split_ball_motion_substeps(BallState{
        .velocity_x = 0x0180,
        .velocity_y = 0x0480,
    });
    check(
        half_steps.count == 2U && half_steps.shifts == 1U
            && half_steps.maximum_whole_speed == 4U
            && half_steps.velocity_x_per_step == 0x00C0
            && half_steps.velocity_y_per_step == 0x0240,
        "native ball physics halves both components for four-pixel motion");

    const auto quarter_steps = split_ball_motion_substeps(BallState{
        .velocity_x = static_cast<std::int16_t>(0xF800U),
        .velocity_y = 0x0100,
    });
    check(
        quarter_steps.count == 4U && quarter_steps.shifts == 2U
            && quarter_steps.maximum_whole_speed == 8U
            && quarter_steps.velocity_x_per_step
                == static_cast<std::int16_t>(0xFE00U)
            && quarter_steps.velocity_y_per_step == 0x0040,
        "native ball physics arithmetically quarters fast negative motion");

    const auto fixed_point_steps = split_ball_motion_substeps(BallState{
        .velocity_x = static_cast<std::int16_t>(0x8000U),
    });
    check(
        fixed_point_steps.count == 4U
            && fixed_point_steps.maximum_whole_speed == 0x80U
            && fixed_point_steps.velocity_x_per_step
                == static_cast<std::int16_t>(0xE000U),
        "native ball physics retains the signed whole-speed fixed point");

    const auto unobstructed = advance_ball_motion_substeps(BallState{
        .position_x = Fixed24_8{.raw = 16 * 256},
        .position_y = Fixed24_8{.raw = 32 * 256},
        .velocity_x = 0x0803,
        .velocity_y = -0x0401,
    });
    check(
        unobstructed.completed_steps == 4U && unobstructed.plan.count == 4U
            && unobstructed.steps[0].state.position_x.raw == 18 * 256
            && unobstructed.state.position_x.raw == 24 * 256
            && unobstructed.state.position_y.raw == 28 * 256 - 4,
        "native ball physics moves once per collision substep without rounding");
    check(
        unobstructed.state.velocity_x == 0x0803
            && unobstructed.state.velocity_y == -0x0401,
        "native ball physics retains frame velocity after an unobstructed loop");

    const auto resumed = advance_prepared_ball_motion_substeps(
        BallState{
            .position_x = Fixed24_8{.raw = 20 * 256},
            .position_y = Fixed24_8{.raw = 40 * 256},
            .velocity_x = 0x0803,
            .velocity_y = -0x0401,
        },
        BallSubstepProgress{
            .increment = {.horizontal = 0x0200, .vertical = -0x0100},
            .completed_steps = 1U,
            .requested_steps = 4U,
        });
    check(
        resumed && resumed->completed_steps == 3U
            && resumed->progress.complete()
            && resumed->state.position_x.raw == 26 * 256
            && resumed->state.position_y.raw == 37 * 256
            && resumed->state.velocity_x == 0x0803
            && resumed->state.velocity_y == -0x0401,
        "native ball physics resumes only DC95's remaining prepared substeps");
    check(
        !advance_prepared_ball_motion_substeps(
            BallState{},
            BallSubstepProgress{
                .completed_steps = 1U,
                .requested_steps = 3U,
            }),
        "native ball physics rejects a non-DC95 prepared substep count");

    const auto damped = apply_ball_drag_and_gravity(
        BallState{
            .velocity_x = 0x0800,
            .velocity_y = -0x0800,
        },
        BallMotionTuning{
            .horizontal_drag_divisor = 8,
            .vertical_drag_divisor = 8,
            .vertical_gravity = 0x20U,
        });
    check(
        damped && damped->previous.velocity_x == 0x0800
            && damped->state.velocity_x == 0x0700
            && damped->state.velocity_y == -0x06E0,
        "native ball physics applies signed drag before vertical gravity");

    const auto fractional_drag = apply_ball_drag_and_gravity(
        BallState{
            .velocity_x = -5,
        },
        BallMotionTuning{
            .horizontal_drag_divisor = 8,
            .vertical_drag_divisor = 0,
        });
    check(
        fractional_drag && fractional_drag->state.velocity_x == -5,
        "native ball physics preserves truncating signed sub-pixel drag");

    const auto invalid_tuning = apply_ball_drag_and_gravity(
        BallState{
            .velocity_y = 1,
        },
        BallMotionTuning{
            .vertical_drag_divisor = 0,
        });
    check(
        !invalid_tuning,
        "native ball physics rejects nonzero motion with a zero drag divisor");
}

void test_native_collision_retry()
{
    using devils_crush::native_runtime::BallState;
    using devils_crush::native_runtime::CollisionRetryDirection;
    using devils_crush::native_runtime::CollisionRetryDirectionSelectionDisposition;
    using devils_crush::native_runtime::CollisionRetryOffsetAttempts;
    using devils_crush::native_runtime::CollisionRetryOffsets;
    using devils_crush::native_runtime::CollisionRetryPlacementDisposition;
    using devils_crush::native_runtime::CollisionRetryPlacementPhase;
    using devils_crush::native_runtime::CollisionRetryProbeDisposition;
    using devils_crush::native_runtime::CollisionRetryFullScanOutcome;
    using devils_crush::native_runtime::Fixed24_8;
    using devils_crush::native_runtime::MappedRomView;
    using devils_crush::native_runtime::classify_collision_retry_direction;
    using devils_crush::native_runtime::begin_collision_retry_placement;
    using devils_crush::native_runtime::advance_collision_retry_placement;
    using devils_crush::native_runtime::decode_collision_retry_offset_attempts;
    using devils_crush::native_runtime::decode_collision_retry_offsets;
    using devils_crush::native_runtime::kCollisionRetryOffsetTable;
    using devils_crush::native_runtime::make_collision_retry_full_scan_probe;
    using devils_crush::native_runtime::make_collision_retry_attempt_sequence;
    using devils_crush::native_runtime::make_collision_retry_attempt_sequence_for_direction;
    using devils_crush::native_runtime::make_collision_retry_vertical_fallback_sequence;
    using devils_crush::native_runtime::probe_collision_retry;
    using devils_crush::native_runtime::retry_ball_collision;
    using devils_crush::native_runtime::select_collision_retry_direction;
    using devils_crush::native_runtime::select_collision_retry_placement_direction;

    check(
        classify_collision_retry_direction(0, 0)
                == CollisionRetryDirection::positive_x_positive_y
            && classify_collision_retry_direction(1, -1)
                == CollisionRetryDirection::positive_x_negative_y
            && classify_collision_retry_direction(-1, 1)
                == CollisionRetryDirection::negative_x_positive_y
            && classify_collision_retry_direction(-1, -1)
                == CollisionRetryDirection::negative_x_negative_y,
        "native collision retry classifies all signed motion directions");

    const CollisionRetryOffsets offsets{
        .by_direction = {{
            {.horizontal_pixels = 1, .vertical_pixels = 2},
            {.horizontal_pixels = 3, .vertical_pixels = -4},
            {.horizontal_pixels = -5, .vertical_pixels = 6},
            {.horizontal_pixels = -7, .vertical_pixels = -8},
        }},
    };
    const auto retry = retry_ball_collision(
        {
            .position_x = Fixed24_8{.raw = 28 * 256 + 64},
            .position_y = Fixed24_8{.raw = 44 * 256 + 128},
            .velocity_x = -0x0180,
            .velocity_y = 0x0240,
            .motion_flags = 0xA2U,
        },
        {
            .position_x = Fixed24_8{.raw = 26 * 256 + 192},
            .position_y = Fixed24_8{.raw = 41 * 256 + 16},
            .velocity_x = 0x0100,
            .velocity_y = -0x0100,
            .motion_flags = 0x80U,
        },
        offsets);
    check(
        retry.direction == CollisionRetryDirection::negative_x_positive_y
            && retry.offset.horizontal_pixels == -5
            && retry.offset.vertical_pixels == 6
            && retry.state.position_x.raw == 21 * 256 + 192
            && retry.state.position_y.raw == 47 * 256 + 16
            && retry.state.velocity_x == -0x0180
            && retry.state.velocity_y == 0x0240
            && retry.state.motion_flags == 0xA2U
            && !retry.vertical_integer_addition_carried,
        "native collision retry restores position, preserves response state, and applies its direction offset");

    const auto sequence = make_collision_retry_attempt_sequence(
        {
            .position_x = Fixed24_8{.raw = 28 * 256 + 64},
            .position_y = Fixed24_8{.raw = 44 * 256 + 128},
            .velocity_x = -0x0180,
            .velocity_y = 0x0240,
            .motion_flags = 0xA2U,
        },
        {
            .position_x = Fixed24_8{.raw = 26 * 256 + 192},
            .position_y = Fixed24_8{.raw = 41 * 256 + 16},
            .velocity_x = 0x0100,
            .velocity_y = -0x0100,
            .motion_flags = 0x80U,
        },
        CollisionRetryOffsetAttempts{
            .by_direction = {{
                {},
                {},
                {{{.horizontal_pixels = -5, .vertical_pixels = 6},
                  {.horizontal_pixels = 2, .vertical_pixels = -3},
                  {.horizontal_pixels = -1, .vertical_pixels = 4}}},
                {},
            }},
        });
    check(
        sequence.direction == CollisionRetryDirection::negative_x_positive_y
            && sequence.restored_ball.position_x.raw == 26 * 256 + 192
            && sequence.restored_ball.position_y.raw == 41 * 256 + 16
            && sequence.attempts[0U].state.position_x.raw == 21 * 256 + 192
            && sequence.attempts[0U].state.position_y.raw == 47 * 256 + 16
            && sequence.attempts[1U].state.position_x.raw == 23 * 256 + 192
            && sequence.attempts[1U].state.position_y.raw == 44 * 256 + 16
            && sequence.attempts[2U].state.position_x.raw == 22 * 256 + 192
            && sequence.attempts[2U].state.position_y.raw == 48 * 256 + 16,
        "native collision retry keeps CDF8's three same-quadrant moves cumulative");
    const auto selected_direction = select_collision_retry_direction(0x80U, 0xC0U);
    const auto repeated_direction = select_collision_retry_direction(0x00U, 0xC0U);
    const auto exhausted_directions = select_collision_retry_direction(0x00U, 0xF3U);
    const auto selected_sequence = make_collision_retry_attempt_sequence_for_direction(
        retry.impact_ball,
        sequence.restored_ball,
        CollisionRetryOffsetAttempts{
            .by_direction = {{
                {},
                {},
                {{{.horizontal_pixels = -5, .vertical_pixels = 6},
                  {.horizontal_pixels = 2, .vertical_pixels = -3},
                  {.horizontal_pixels = -1, .vertical_pixels = 4}}},
                {},
            }},
        },
        *selected_direction.direction);
    check(
        selected_direction.disposition
                == CollisionRetryDirectionSelectionDisposition::selected
            && selected_direction.direction
                == CollisionRetryDirection::negative_x_positive_y
            && selected_direction.direction_mask == 0x20U
            && selected_direction.attempted_direction_flags_after == 0xE0U
            && repeated_direction.disposition
                == CollisionRetryDirectionSelectionDisposition::already_attempted
            && repeated_direction.direction
                == CollisionRetryDirection::positive_x_positive_y
            && repeated_direction.attempted_direction_flags_after == 0xC0U
            && exhausted_directions.disposition
                == CollisionRetryDirectionSelectionDisposition::fallback_required
            && !exhausted_directions.direction
            && selected_sequence == sequence,
        "native collision retry preserves CDF8's RNG direction selection and retry mask");

    const auto fallback = make_collision_retry_vertical_fallback_sequence(
        retry.impact_ball, sequence.attempts[2U].state);
    check(
        fallback.attempts[0U].state.position_x.raw == 22 * 256 + 192
            && fallback.attempts[0U].state.position_y.raw == 47 * 256 + 16
            && fallback.attempts[6U].state.position_x.raw == 22 * 256 + 192
            && fallback.attempts[6U].state.position_y.raw == 41 * 256 + 16
            && fallback.attempts[0U].vertical_integer_addition_carried
            && fallback.attempts[6U].vertical_integer_addition_carried,
        "native collision retry preserves CDF8's seven cumulative upward probes");
    const auto wrapping_fallback = make_collision_retry_vertical_fallback_sequence(
        retry.impact_ball,
        {.position_y = Fixed24_8{.raw = 0}});
    check(
        wrapping_fallback.attempts[0U].state.position_y.raw == -256
            && !wrapping_fallback.attempts[0U].vertical_integer_addition_carried,
        "native collision retry preserves the fallback subtraction borrow into CEB5");

    const CollisionRetryOffsetAttempts placement_offsets{
        .by_direction = {{
            {{{.horizontal_pixels = 1},
              {.vertical_pixels = 1},
              {.horizontal_pixels = 1, .vertical_pixels = 1}}},
            {},
            {{{.horizontal_pixels = -5, .vertical_pixels = 6},
              {.horizontal_pixels = 2, .vertical_pixels = -3},
              {.horizontal_pixels = -1, .vertical_pixels = 4}}},
            {},
        }},
    };
    const auto placement_initial = begin_collision_retry_placement(
        retry.impact_ball, sequence.restored_ball, placement_offsets);
    const auto placement_unresolved = advance_collision_retry_placement(
        placement_initial.state, CollisionRetryFullScanOutcome::unresolved);
    const auto placement_second = advance_collision_retry_placement(
        placement_initial.state, CollisionRetryFullScanOutcome::solid);
    const auto placement_third = advance_collision_retry_placement(
        placement_second.state, CollisionRetryFullScanOutcome::solid);
    const auto placement_needs_rng = advance_collision_retry_placement(
        placement_third.state, CollisionRetryFullScanOutcome::solid);
    const auto placement_repeated_direction =
        select_collision_retry_placement_direction(
            placement_needs_rng.state, 0x80U);
    const auto placement_selected_direction =
        select_collision_retry_placement_direction(
            placement_needs_rng.state, 0x00U);
    const auto invalid_placement_advance = advance_collision_retry_placement(
        placement_needs_rng.state, CollisionRetryFullScanOutcome::clear);
    const auto invalid_placement_direction =
        select_collision_retry_placement_direction(placement_initial.state, 0x00U);
    check(
        placement_initial.disposition
                == CollisionRetryPlacementDisposition::probe_ready
            && placement_initial.probe == sequence.attempts[0U]
            && placement_initial.state.attempted_direction_flags == 0x20U
            && placement_unresolved.disposition
                == CollisionRetryPlacementDisposition::unresolved
            && placement_unresolved.state == placement_initial.state
            && placement_unresolved.probe == placement_initial.probe
            && placement_second.probe == sequence.attempts[1U]
            && placement_third.probe == sequence.attempts[2U]
            && placement_needs_rng.disposition
                == CollisionRetryPlacementDisposition::needs_random_direction
            && placement_needs_rng.state.phase
                == CollisionRetryPlacementPhase::selecting_direction
            && placement_repeated_direction.disposition
                == CollisionRetryPlacementDisposition::needs_random_direction
            && placement_selected_direction.disposition
                == CollisionRetryPlacementDisposition::probe_ready
            && placement_selected_direction.state.direction
                == CollisionRetryDirection::positive_x_positive_y
            && placement_selected_direction.state.attempted_direction_flags == 0xA0U
            && placement_selected_direction.probe->state.position_x.raw
                == 27 * 256 + 192
            && placement_selected_direction.probe->state.position_y.raw
                == 41 * 256 + 16
            && invalid_placement_advance.disposition
                == CollisionRetryPlacementDisposition::invalid_state
            && invalid_placement_advance.state == placement_needs_rng.state
            && invalid_placement_direction.disposition
                == CollisionRetryPlacementDisposition::invalid_state
            && invalid_placement_direction.state == placement_initial.state,
        "native collision retry owns CDF8's candidate, unresolved, and RNG boundaries");

    auto exhausted_placement = placement_needs_rng.state;
    exhausted_placement.attempted_direction_flags = 0xF0U;
    const auto fallback_placement = select_collision_retry_placement_direction(
        exhausted_placement, 0x00U);
    auto end_of_fallback_pass = fallback_placement;
    for (std::size_t index = 0U; index < 6U; ++index) {
        end_of_fallback_pass = advance_collision_retry_placement(
            end_of_fallback_pass.state, CollisionRetryFullScanOutcome::solid);
    }
    const auto next_fallback_pass = advance_collision_retry_placement(
        end_of_fallback_pass.state, CollisionRetryFullScanOutcome::solid);
    const auto accepted_placement = advance_collision_retry_placement(
        fallback_placement.state, CollisionRetryFullScanOutcome::clear);
    check(
        fallback_placement.disposition
                == CollisionRetryPlacementDisposition::probe_ready
            && fallback_placement.state.phase
                == CollisionRetryPlacementPhase::vertical_fallback
            && fallback_placement.probe->state.position_y.raw == 47 * 256 + 16
            && end_of_fallback_pass.state.attempt_index == 6U
            && end_of_fallback_pass.probe->state.position_y.raw == 41 * 256 + 16
            && next_fallback_pass.state.attempt_index == 0U
            && next_fallback_pass.probe->state.position_y.raw == 40 * 256 + 16
            && accepted_placement.disposition
                == CollisionRetryPlacementDisposition::accepted
            && accepted_placement.state.phase
                == CollisionRetryPlacementPhase::accepted
            && accepted_placement.probe == fallback_placement.probe,
        "native collision retry repeats CE98 fallback passes and retains the accepted candidate");

    const auto clear_probe = probe_collision_retry(retry);
    check(
        clear_probe.adjusted_horizontal_low_byte == 0xF4U
            && clear_probe.adjusted_horizontal_high_byte == 0xFFU
            && clear_probe.vertical_low_byte == 47U
            && clear_probe.disposition == CollisionRetryProbeDisposition::clear,
        "native collision retry keeps its observed clear probe separate from full scanning");

    const auto full_scan_probe = probe_collision_retry(retry_ball_collision(
        {
            .velocity_x = 1,
            .velocity_y = 1,
        },
        {
            .position_x = Fixed24_8{.raw = 32 * 256},
            .position_y = Fixed24_8{},
        },
        CollisionRetryOffsets{
            .by_direction = {{
                {.horizontal_pixels = 1},
            }},
        }));
    check(
        full_scan_probe.adjusted_horizontal_low_byte == 0U
            && full_scan_probe.adjusted_horizontal_high_byte == 0U
            && full_scan_probe.vertical_low_byte == 0U
            && full_scan_probe.disposition
                == CollisionRetryProbeDisposition::requires_full_scan,
        "native collision retry reports the unported scanner handoff exactly");
    const auto full_scan_source = make_collision_retry_full_scan_probe(
        full_scan_probe, 0x23U);
    check(
        full_scan_source && full_scan_source->horizontal == 0U
            && full_scan_source->vertical == 0x2300U
            && !make_collision_retry_full_scan_probe(clear_probe, 0x23U),
        "native collision retry exposes CED2's mixed parent/retry scan source");

    const auto carried_clear_probe = probe_collision_retry(retry_ball_collision(
        {
            .velocity_x = 1,
            .velocity_y = 1,
        },
        {
            .position_x = Fixed24_8{.raw = 32 * 256},
            .position_y = Fixed24_8{.raw = 0xFFFF * 256},
        },
        CollisionRetryOffsets{
            .by_direction = {{
                {.vertical_pixels = 2},
            }},
        }));
    check(
        carried_clear_probe.retry.vertical_integer_addition_carried
            && carried_clear_probe.adjusted_horizontal_high_byte == 0U
            && carried_clear_probe.vertical_low_byte == 1U
            && carried_clear_probe.disposition
                == CollisionRetryProbeDisposition::clear,
        "native collision retry preserves vertical-add carry into the probe");

    const auto wrapped_retry = retry_ball_collision(
        {
            .velocity_x = 1,
            .velocity_y = 1,
        },
        {
            .position_x = Fixed24_8{.raw = 0x7FFF00},
        },
        offsets);
    check(
        wrapped_retry.state.position_x.raw == -0x800000,
        "native collision retry preserves signed 24-bit position wrap");

    std::vector<std::byte> rom(MappedRomView::kSupportedRomBytes);
    std::array<std::uint8_t, 8U> mpr{};
    mpr[6U] = 0x01U;
    constexpr std::array retry_table{
        std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x01}, std::byte{0x00}, std::byte{0x01}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x01}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0xFF}, std::byte{0xFF},
        std::byte{0x01}, std::byte{0x00}, std::byte{0xFF}, std::byte{0xFF},
        std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0xFF}, std::byte{0xFF}, std::byte{0x00}, std::byte{0x00},
        std::byte{0xFF}, std::byte{0xFF}, std::byte{0x01}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x01}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0xFF}, std::byte{0xFF},
        std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF},
        std::byte{0xFF}, std::byte{0xFF}, std::byte{0x00}, std::byte{0x00},
    };
    std::ranges::copy(
        retry_table,
        rom.begin() + static_cast<std::ptrdiff_t>(
            MappedRomView::kBankBytes
            + (kCollisionRetryOffsetTable & (MappedRomView::kBankBytes - 1U))));
    const auto decoded_offsets = decode_collision_retry_offsets(
        MappedRomView(rom, mpr));
    check(
        decoded_offsets && *decoded_offsets == CollisionRetryOffsets{
                .by_direction = {{
                    {.horizontal_pixels = 1, .vertical_pixels = 0},
                    {.horizontal_pixels = 0, .vertical_pixels = -1},
                    {.horizontal_pixels = -1, .vertical_pixels = 0},
                    {.horizontal_pixels = 0, .vertical_pixels = -1},
                }},
            },
        "native collision retry decodes each direction's first CDF8 candidate");
    const auto decoded_attempts = decode_collision_retry_offset_attempts(
        MappedRomView(rom, mpr));
    check(
        decoded_attempts
            && decoded_attempts->by_direction[0U][0U]
                == devils_crush::native_runtime::CollisionRetryOffset{
                    .horizontal_pixels = 1, .vertical_pixels = 0}
            && decoded_attempts->by_direction[0U][2U]
                == devils_crush::native_runtime::CollisionRetryOffset{
                    .horizontal_pixels = 0, .vertical_pixels = 1}
            && decoded_attempts->by_direction[1U][1U]
                == devils_crush::native_runtime::CollisionRetryOffset{
                    .horizontal_pixels = 1, .vertical_pixels = -1}
            && decoded_attempts->by_direction[2U][1U]
                == devils_crush::native_runtime::CollisionRetryOffset{
                    .horizontal_pixels = -1, .vertical_pixels = 1}
            && decoded_attempts->by_direction[3U][2U]
                == devils_crush::native_runtime::CollisionRetryOffset{
                    .horizontal_pixels = -1, .vertical_pixels = 0},
        "native collision retry preserves CDF8's three ordered candidates per quadrant");
    mpr[6U] = 0x80U;
    check(
        !decode_collision_retry_offset_attempts(MappedRomView(rom, mpr))
            && !decode_collision_retry_offsets(MappedRomView(rom, mpr)),
        "native collision retry rejects an unmapped offset table");
}

void test_native_collision_response()
{
    using devils_crush::native_runtime::CollisionResponseConfig;
    using devils_crush::native_runtime::CollisionResponseBasis;
    using devils_crush::native_runtime::CollisionResponseMode;
    using devils_crush::native_runtime::CollisionNormalReflection;
    using devils_crush::native_runtime::CollisionResponseVector;
    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::MappedRomView;
    using devils_crush::native_runtime::NormalCollisionResponseTuning;
    using devils_crush::native_runtime::apply_collision_response;
    using devils_crush::native_runtime::apply_oriented_collision_response;
    using devils_crush::native_runtime::decode_collision_response_basis;
    using devils_crush::native_runtime::make_collision_response_config;

    auto config = CollisionResponseConfig{
        .requested_mode = CollisionResponseMode::half,
    };
    const auto half = apply_collision_response(
        static_cast<std::int16_t>(0xFE00U), config);
    check(
        half.outgoing_velocity == -256 && !half.clamped,
        "native collision response applies signed half-speed scaling");

    config = {
        .requested_mode = CollisionResponseMode::reduced,
    };
    const auto reduced = apply_collision_response(0x0800, config);
    check(
        reduced.outgoing_velocity == 0x0180,
        "native collision response applies three-sixteenths reduced scaling");

    config.use_quarter_reduced_scale = true;
    const auto quarter = apply_collision_response(0x0800, config);
    check(
        quarter.outgoing_velocity == 0x0200,
        "native collision response selects quarter reduced scaling");

    config = {
        .requested_mode = CollisionResponseMode::normal,
        .force_reduced_mode = true,
    };
    const auto promoted = apply_collision_response(0x0800, config);
    check(
        promoted.effective_mode == CollisionResponseMode::reduced
            && promoted.outgoing_velocity == 0x0180,
        "native collision response promotes normal impacts in reduced table state");

    config = {
        .requested_mode = CollisionResponseMode::normal,
        .normal_tuning = NormalCollisionResponseTuning{
            .negative_low_byte_decrement = 0x20U,
            .positive_low_byte_increment = 0x20U,
            .positive_high_byte_increment = 0x01U,
        },
    };
    const auto positive_cap = apply_collision_response(0x05F0, config);
    check(
        positive_cap.outgoing_velocity == 0x0610 && positive_cap.clamped,
        "native collision response applies the normal positive speed cap");

    const auto negative_cap = apply_collision_response(
        static_cast<std::int16_t>(0xF510U), config);
    check(
        negative_cap.outgoing_velocity == static_cast<std::int16_t>(0xFAF0U)
            && negative_cap.clamped,
        "native collision response applies the normal negative speed cap");

    const auto identity_reflection = apply_oriented_collision_response(
        {.horizontal = 0x0200, .vertical = 0x0300},
        {},
        {});
    check(
        identity_reflection.local
            == CollisionResponseVector{.horizontal = 0x0200, .vertical = 0x0300}
            && identity_reflection.normal.incoming_velocity == -0x0300
            && identity_reflection.outgoing
                == CollisionResponseVector{.horizontal = 0x0200, .vertical = -0x0300},
        "native oriented collision response reflects the local normal in an identity contact basis");

    const auto quarter_turn_reflection = apply_oriented_collision_response(
        {.horizontal = 0x0200, .vertical = 0x0300},
        CollisionResponseBasis{.cosine = 0, .sine = 0x0100},
        {});
    check(
        quarter_turn_reflection.local
            == CollisionResponseVector{.horizontal = 0x0300, .vertical = -0x0200}
            && quarter_turn_reflection.normal.incoming_velocity == 0x0200
            && quarter_turn_reflection.outgoing
                == CollisionResponseVector{.horizontal = -0x0200, .vertical = 0x0300},
        "native oriented collision response preserves CF33's projection, normal reflection, and inverse projection");

    const auto clear_selector_reflection = apply_oriented_collision_response(
        {.horizontal = 0x0000, .vertical = 0x0100},
        {},
        {
            .normal_tuning = {
                .negative_low_byte_decrement = 0x20U,
                .positive_low_byte_increment = 0x10U,
            },
        },
        CollisionNormalReflection::after_response);
    check(
        clear_selector_reflection.normal.incoming_velocity == 0x0100
            && clear_selector_reflection.normal.outgoing_velocity == 0x0110
            && clear_selector_reflection.outgoing
                == CollisionResponseVector{.horizontal = 0x0000, .vertical = -0x0110},
        "native oriented collision response preserves CF5B's post-response normal negation");

    std::vector<std::byte> geometry_rom(MappedRomView::kSupportedRomBytes);
    std::array<std::uint8_t, 8U> geometry_mpr{};
    geometry_mpr[6U] = 0x01U;
    constexpr std::size_t kGeometryTableOffset = MappedRomView::kBankBytes
        + (0xD2AEU & (MappedRomView::kBankBytes - 1U));
    constexpr auto kContactOrientation = 0xE3U;
    constexpr auto kGeometryIndex = (kContactOrientation & 0x1FU) << 1U;
    geometry_rom[kGeometryTableOffset + kGeometryIndex] = std::byte{0x80U};
    geometry_rom[kGeometryTableOffset + kGeometryIndex + 1U] = std::byte{0x01U};
    geometry_rom[kGeometryTableOffset + 0x40U + kGeometryIndex]
        = std::byte{0x40U};
    geometry_rom[kGeometryTableOffset + 0x40U + kGeometryIndex + 1U]
        = std::byte{0xFFU};
    const auto decoded_basis = decode_collision_response_basis(
        MappedRomView(geometry_rom, geometry_mpr), kContactOrientation);
    geometry_mpr[6U] = 0x80U;
    check(
        decoded_basis
            && decoded_basis->contact_orientation == kContactOrientation
            && decoded_basis->table_index == kGeometryIndex
            && decoded_basis->basis
                == CollisionResponseBasis{.cosine = 0x0180, .sine = -0x00C0}
            && !decode_collision_response_basis(
                MappedRomView(geometry_rom, geometry_mpr), kContactOrientation),
        "native collision response decodes D081's signed contact basis through the mapped ROM asset");

    std::array<std::byte, GameStateView::kWorkRamSize> response_ram{};
    response_ram[0x0078U] = std::byte{0x40U};
    response_ram[0x0CC7U] = std::byte{0x01U};
    response_ram[0x022CU] = std::byte{0x01U};
    response_ram[0x0081U] = std::byte{0x18U};
    response_ram[0x0082U] = std::byte{0x02U};
    const auto table_response_state =
        GameStateView(response_ram).collision_response_table_state();
    const auto response_input = GameStateView(response_ram).collision_response_input();
    const auto table_response_config = table_response_state
        ? make_collision_response_config(0x00U, *table_response_state)
        : CollisionResponseConfig{};
    const auto half_response_config = table_response_state
        ? make_collision_response_config(0x40U, *table_response_state)
        : CollisionResponseConfig{};
    check(
        table_response_state
            && response_input
            && response_input->orientation == 0x40U
            && !response_input->selects_signed_cf33_response()
            && response_input->table_state == *table_response_state
            && table_response_config.requested_mode == CollisionResponseMode::normal
            && table_response_config.force_reduced_mode
            && table_response_config.use_quarter_reduced_scale
            && table_response_config.normal_tuning
                == NormalCollisionResponseTuning{
                    .negative_low_byte_decrement = 0x18U,
                    .positive_low_byte_increment = 0x18U,
                    .positive_high_byte_increment = 0x02U,
                }
            && half_response_config.requested_mode == CollisionResponseMode::half,
        "native collision response captures CF33's shared orientation selector and CFD8 table tuning");
}

void test_native_collision_bounds()
{
    using devils_crush::native_runtime::ActorCollisionGeometryDisposition;
    using devils_crush::native_runtime::ActorCollisionGeometryInput;
    using devils_crush::native_runtime::ActorCollisionShapeDescriptor;
    using devils_crush::native_runtime::CollisionBounds;
    using devils_crush::native_runtime::CollisionBoundsDisposition;
    using devils_crush::native_runtime::CollisionProbe;
    using devils_crush::native_runtime::evaluate_actor_vertical_proximity;
    using devils_crush::native_runtime::evaluate_collision_bounds;
    using devils_crush::native_runtime::prepare_actor_collision_geometry;
    using devils_crush::native_runtime::resolve_actor_collision_shape;
    using devils_crush::native_runtime::MappedRomView;

    const auto inside = evaluate_collision_bounds(
        CollisionProbe{.horizontal = 0x42U, .vertical = 0x0200U},
        CollisionBounds{
            .horizontal_origin = 0x40U,
            .vertical_origin = 0x01FEU,
            .horizontal_extent = 3U,
            .vertical_extent = 3U,
        });
    check(
        inside.disposition == CollisionBoundsDisposition::intersecting
            && inside.relative_horizontal == 2U
            && inside.relative_vertical == 2U,
        "native collision bounds retains an in-range one-sided candidate");

    const auto horizontal_edge = evaluate_collision_bounds(
        CollisionProbe{.horizontal = 0x43U, .vertical = 0x0200U}, inside.bounds);
    check(
        horizontal_edge.disposition
            == CollisionBoundsDisposition::outside_horizontal,
        "native collision bounds rejects its exclusive horizontal edge");

    const auto vertical_edge = evaluate_collision_bounds(
        CollisionProbe{.horizontal = 0x42U, .vertical = 0x0201U}, inside.bounds);
    check(
        vertical_edge.disposition == CollisionBoundsDisposition::outside_vertical,
        "native collision bounds rejects its exclusive vertical edge");

    const auto before_origin = evaluate_collision_bounds(
        CollisionProbe{.horizontal = 0x3FU, .vertical = 0x01FDU}, inside.bounds);
    check(
        before_origin.relative_horizontal == 0xFFU
            && before_origin.relative_vertical == 0xFFFFU
            && before_origin.disposition
                == CollisionBoundsDisposition::outside_horizontal,
        "native collision bounds preserves unsigned wrapped rejection");

    const auto empty = evaluate_collision_bounds(
        CollisionProbe{.horizontal = 0x40U, .vertical = 0x01FEU},
        CollisionBounds{
            .horizontal_origin = 0x40U,
            .vertical_origin = 0x01FEU,
        });
    check(
        empty.disposition == CollisionBoundsDisposition::outside_horizontal,
        "native collision bounds treats a zero horizontal extent as empty");

    const auto vertical_near = evaluate_actor_vertical_proximity(0x0100U, 0x017FU);
    const auto vertical_positive_edge = evaluate_actor_vertical_proximity(
        0x0180U, 0x0100U);
    const auto vertical_negative_edge = evaluate_actor_vertical_proximity(
        0x0100U, 0x0180U);
    check(
        vertical_near.signed_difference == -127 && vertical_near.intersecting
            && vertical_positive_edge.signed_difference == 128
            && !vertical_positive_edge.intersecting
            && vertical_negative_edge.signed_difference == -128
            && !vertical_negative_edge.intersecting,
        "native actor collision accepts only the recovered vertical proximity window");

    const auto prepared = prepare_actor_collision_geometry({
        .actor = {
            .collision_table = {.position_x = 0x0050U, .position_y = 0x0300U},
        },
        .vertical_proximity = evaluate_actor_vertical_proximity(0x030AU, 0x0300U),
        .descriptor = {
            .horizontal_offset = -4,
            .vertical_offset = -16,
            .horizontal_extent = 12U,
            .vertical_extent = 20U,
        },
    });
    check(
        prepared.disposition == ActorCollisionGeometryDisposition::bounds_ready
            && prepared.position_after_ball_bias == 0x0030U
            && prepared.bounds.horizontal_origin == 0x2CU
            && prepared.bounds.vertical_origin == 0x02F0U
            && prepared.bounds.horizontal_extent == 12U
            && prepared.bounds.vertical_extent == 20U,
        "native actor collision turns a decoded descriptor into a target window");

    const auto vertically_rejected = prepare_actor_collision_geometry({
        .actor = prepared.input.actor,
        .vertical_proximity = evaluate_actor_vertical_proximity(0x0380U, 0x0300U),
        .descriptor = prepared.input.descriptor,
    });
    const auto page_rejected = prepare_actor_collision_geometry({
        .actor = {
            .collision_table = {.position_x = 0x0120U, .position_y = 0x0300U},
        },
        .vertical_proximity = prepared.input.vertical_proximity,
        .descriptor = prepared.input.descriptor,
    });
    check(
        vertically_rejected.disposition
                == ActorCollisionGeometryDisposition::outside_vertical_proximity
            && page_rejected.disposition
                == ActorCollisionGeometryDisposition::outside_horizontal_world,
        "native actor collision preserves its vertical and one-byte-world rejections");

    std::vector<std::byte> rom(MappedRomView::kSupportedRomBytes);
    std::array<std::uint8_t, 8> mpr{};
    mpr[4] = 0x01U;
    const auto store_rom = [&rom](
                               const std::uint16_t address,
                               const std::uint8_t value) {
        rom[MappedRomView::kBankBytes + (address & 0x1FFFU)] =
            static_cast<std::byte>(value);
    };
    store_rom(0x8006U, 0x00U);
    store_rom(0x8106U, 0x84U);
    store_rom(0x8206U, 0x00U);
    store_rom(0x8306U, 0x85U);
    store_rom(0x8406U, 0x00U);
    store_rom(0x8407U, 0x86U);
    store_rom(0x8506U, 0x00U);
    store_rom(0x8507U, 0x87U);
    store_rom(0x8600U, 0xFCU);
    store_rom(0x8601U, 0xF0U);
    store_rom(0x8602U, 0x0CU);
    store_rom(0x8603U, 0x14U);
    const auto shape = resolve_actor_collision_shape(
        MappedRomView(rom, mpr),
        {.tables = {0x8000U, 0x8100U, 0x8200U, 0x8300U}},
        0x06U,
        0x03U);
    check(
        shape && shape->geometry_tables
                == std::array<std::uint16_t, 2>{0x8400U, 0x8500U}
            && shape->descriptor_address == 0x8600U
            && shape->secondary_geometry_address == 0x8700U
            && shape->descriptor.horizontal_offset == -4
            && shape->descriptor.vertical_offset == -16
            && shape->descriptor.horizontal_extent == 12U
            && shape->descriptor.vertical_extent == 20U,
        "native actor collision resolves the selected shape through mapped ROM");
    const auto actor_shape = resolve_actor_collision_shape(
        MappedRomView(rom, mpr),
        {.tables = {0x8000U, 0x8100U, 0x8200U, 0x8300U}},
        {.collision_shape_directory_index = 0x06U,
         .collision_shape_phase = 0x03U});
    check(
        actor_shape == shape,
        "native actor collision resolves a captured actor's selected shape");
    mpr[4] = 0x80U;
    check(
        !resolve_actor_collision_shape(
            MappedRomView(rom, mpr),
            {.tables = {0x8000U, 0x8100U, 0x8200U, 0x8300U}},
            0x06U,
            0x03U),
        "native actor collision rejects an unmapped shape directory");
}

void test_native_collision_mask()
{
    using devils_crush::native_runtime::CollisionMask;
    using devils_crush::native_runtime::CollisionMaskCell;
    using devils_crush::native_runtime::CollisionMaskDisposition;
    using devils_crush::native_runtime::evaluate_collision_mask;

    CollisionMask mask{};
    mask.rows[5] = 0x28U;
    const auto solid = evaluate_collision_mask(
        mask, CollisionMaskCell{.column = 2U, .row = 5U});
    check(
        solid.disposition == CollisionMaskDisposition::solid
            && solid.selected_row == 0x28U && solid.column_bit == 0x20U,
        "native collision mask uses the ROM's high-bit-first column order");

    const auto empty = evaluate_collision_mask(
        mask, CollisionMaskCell{.column = 3U, .row = 5U});
    check(
        empty.disposition == CollisionMaskDisposition::empty
            && empty.column_bit == 0x10U,
        "native collision mask distinguishes a clear cell in a solid row");

    const auto second_solid = evaluate_collision_mask(
        mask, CollisionMaskCell{.column = 4U, .row = 5U});
    check(
        second_solid.disposition == CollisionMaskDisposition::solid
            && second_solid.column_bit == 0x08U,
        "native collision mask selects independent solidity bits");

    const auto outside = evaluate_collision_mask(
        mask, CollisionMaskCell{.column = CollisionMask::kWidth, .row = 0U});
    check(
        outside.disposition == CollisionMaskDisposition::outside_mask
            && outside.column_bit == 0U,
        "native collision mask rejects a cell outside its eight-by-eight shape");
}

void test_native_actor_collision_mask_coordinates()
{
    using devils_crush::native_runtime::ActorCollisionMaskCoordinateInput;
    using devils_crush::native_runtime::CollisionBounds;
    using devils_crush::native_runtime::CollisionBoundsDisposition;
    using devils_crush::native_runtime::CollisionBoundsEvaluation;
    using devils_crush::native_runtime::CollisionProbe;
    using devils_crush::native_runtime::prepare_actor_collision_mask_coordinates;

    const auto coordinates = prepare_actor_collision_mask_coordinates({
        .actor_slot = 0x08U,
        .shape_phase = 0x02U,
        .session_active = true,
        .side_flags = 0x11U,
        .bounds = {
            .probe = CollisionProbe{.horizontal = 0x2DU, .vertical = 0x021BU},
            .bounds = CollisionBounds{
                .horizontal_origin = 0x20U,
                .vertical_origin = 0x0200U,
                .horizontal_extent = 12U,
                .vertical_extent = 20U,
            },
            .relative_horizontal = 13U,
            .relative_vertical = 27U,
            .disposition = CollisionBoundsDisposition::intersecting,
        },
    });
    check(
        coordinates.updates_side_flags && coordinates.side_flags == 0x91U
            && coordinates.cell.column == 5U && coordinates.cell.row == 3U
            && coordinates.horizontal_cell == 1U
            && coordinates.vertical_cell == 3U
            && coordinates.rounded_horizontal_span == 2U,
        "native actor collision derives mask coordinates after bounds pass");

    const auto rejected = prepare_actor_collision_mask_coordinates({
        .actor_slot = 0x09U,
        .shape_phase = 0x03U,
        .session_active = true,
        .side_flags = 0x11U,
        .bounds = {
            .relative_horizontal = 2U,
            .relative_vertical = 4U,
            .disposition = CollisionBoundsDisposition::outside_vertical,
        },
    });
    check(
        !rejected.updates_side_flags && rejected.side_flags == 0x11U
            && rejected.cell.column == 0U && rejected.cell.row == 0U
            && rejected.rounded_horizontal_span == 0U,
        "native actor collision leaves rejected bounds outside mask preparation");
}

void test_native_actor_collision_mask_asset()
{
    using devils_crush::native_runtime::ActorCollisionMaskCoordinateInput;
    using devils_crush::native_runtime::CollisionBounds;
    using devils_crush::native_runtime::CollisionBoundsDisposition;
    using devils_crush::native_runtime::CollisionBoundsEvaluation;
    using devils_crush::native_runtime::CollisionProbe;
    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::MappedRomView;
    using devils_crush::native_runtime::prepare_actor_collision_mask_coordinates;
    using devils_crush::native_runtime::resolve_actor_collision_mask;

    const auto coordinates = prepare_actor_collision_mask_coordinates({
        .bounds = {
            .probe = CollisionProbe{.horizontal = 0x2DU, .vertical = 0x021BU},
            .bounds = CollisionBounds{
                .horizontal_origin = 0x20U,
                .vertical_origin = 0x0200U,
                .horizontal_extent = 12U,
                .vertical_extent = 20U,
            },
            .relative_horizontal = 13U,
            .relative_vertical = 27U,
            .disposition = CollisionBoundsDisposition::intersecting,
        },
    });
    std::vector<std::byte> rom(MappedRomView::kSupportedRomBytes);
    std::array<std::uint8_t, 8> mpr{};
    mpr[4] = 0x01U;
    const auto store_rom = [&rom](
                               const std::uint16_t address,
                               const std::uint8_t value) {
        rom[MappedRomView::kBankBytes + (address & 0x1FFFU)] =
            static_cast<std::byte>(value);
    };
    store_rom(0x8007U, 0xA2U);
    store_rom(0x810BU, 0x02U);
    store_rom(0x8213U, 0x04U);
    const auto mask = resolve_actor_collision_mask(
        MappedRomView(rom, mpr),
        GameStateView::ActorCollisionMaskTablePointers{
            .first_table = 0x8000U,
            .second_table = 0x8100U,
            .masks = 0x8200U,
        },
        coordinates);
    check(
        mask && mask->table_index == 0x07U
            && mask->first_table_value == 0xA2U && mask->mask_index == 0x02U
            && mask->mask_address == 0x8210U && mask->mask.rows[3] == 0x04U
            && mask->evaluation.disposition
                == devils_crush::native_runtime::CollisionMaskDisposition::solid,
        "native actor collision resolves a row-major selected mapped mask");

    mpr[4] = 0x80U;
    check(
        !resolve_actor_collision_mask(
            MappedRomView(rom, mpr),
            GameStateView::ActorCollisionMaskTablePointers{
                .first_table = 0x8000U,
                .second_table = 0x8100U,
                .masks = 0x8200U,
            },
            coordinates),
        "native actor collision rejects an unmapped selected mask");
}

void test_native_actor_collision_scan()
{
    using devils_crush::native_runtime::ActorCollisionScanContext;
    using devils_crush::native_runtime::ActorCollisionScanDisposition;
    using devils_crush::native_runtime::CollisionRetryFullScanDisposition;
    using devils_crush::native_runtime::CollisionRetryFullScanOutcome;
    using devils_crush::native_runtime::CollisionRetryProbeDisposition;
    using devils_crush::native_runtime::CollisionRetryProbeResult;
    using devils_crush::native_runtime::CollisionProbe;
    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::MappedRomView;
    using devils_crush::native_runtime::make_actor_collision_scan_context;
    using devils_crush::native_runtime::scan_actor_collision;
    using devils_crush::native_runtime::scan_actor_collision_sweep;
    using devils_crush::native_runtime::scan_normal_actor_collision_sweep;
    using devils_crush::native_runtime::scan_collision_retry_full_actors;
    using devils_crush::native_runtime::scan_collision_retry_full;
    using devils_crush::native_runtime::resolve_collision_retry_full_scan_outcome;

    std::vector<std::byte> rom(MappedRomView::kSupportedRomBytes);
    std::array<std::uint8_t, 8> mpr{};
    mpr[4] = 0x01U;
    mpr[5] = 0x02U;
    mpr[6] = 0x03U;
    const auto store_rom = [&rom](
                               const std::uint16_t address,
                               const std::uint8_t value) {
        rom[MappedRomView::kBankBytes + (address & 0x1FFFU)] =
            static_cast<std::byte>(value);
    };
    store_rom(0x8006U, 0x00U);
    store_rom(0x8106U, 0x84U);
    store_rom(0x8206U, 0x00U);
    store_rom(0x8306U, 0x85U);
    store_rom(0x8406U, 0x00U);
    store_rom(0x8407U, 0x86U);
    store_rom(0x8506U, 0x00U);
    store_rom(0x8507U, 0x87U);
    store_rom(0x8404U, 0x00U);
    store_rom(0x8405U, 0x86U);
    store_rom(0x8504U, 0x00U);
    store_rom(0x8505U, 0x87U);
    store_rom(0x8600U, 0xFCU);
    store_rom(0x8601U, 0xF0U);
    store_rom(0x8602U, 0x0CU);
    store_rom(0x8603U, 0x14U);
    store_rom(0x8700U, 0xA2U);
    store_rom(0x8604U, 0x02U);
    store_rom(0x8A13U, 0x40U);
    const ActorCollisionScanContext context{
        .shape_directories = {.tables = {0x8000U, 0x8100U, 0x8200U, 0x8300U}},
        .mask_tables = {
            .first_table = 0x8800U,
            .second_table = 0x8900U,
            .masks = 0x8A00U,
        },
    };
    std::array<std::byte, GameStateView::kWorkRamSize> work_ram{};
    const auto store_ram = [&work_ram](
                               const std::size_t offset,
                               const std::uint8_t value) {
        work_ram[offset] = static_cast<std::byte>(value);
    };
    for (std::size_t index = 0U; index < context.shape_directories.tables.size();
         ++index) {
        const auto pointer = context.shape_directories.tables[index];
        store_ram(0x0096U + index * 2U, static_cast<std::uint8_t>(pointer));
        store_ram(
            0x0097U + index * 2U,
            static_cast<std::uint8_t>(pointer >> 8U));
    }
    store_ram(0x0074U, static_cast<std::uint8_t>(context.mask_tables.first_table));
    store_ram(
        0x0075U,
        static_cast<std::uint8_t>(context.mask_tables.first_table >> 8U));
    store_ram(0x0072U, static_cast<std::uint8_t>(context.mask_tables.second_table));
    store_ram(
        0x0073U,
        static_cast<std::uint8_t>(context.mask_tables.second_table >> 8U));
    store_ram(0x008EU, static_cast<std::uint8_t>(context.mask_tables.masks));
    store_ram(0x008FU, static_cast<std::uint8_t>(context.mask_tables.masks >> 8U));
    store_ram(0x0254U, 0x01U);
    store_ram(0x00B7U, 0xD0U);
    const auto captured_context = make_actor_collision_scan_context(
        GameStateView(work_ram));
    check(
        captured_context
            && captured_context->shape_directories == context.shape_directories
            && captured_context->mask_tables == context.mask_tables
            && captured_context->session_active
            && captured_context->side_flags == 0xD0U,
        "native actor collision scan captures shared inputs from typed state");
    const GameStateView::ActorSlot actor{
        .index = 0x08U,
        .collision_shape_directory_index = 0x06U,
        .collision_shape_phase = 0x03U,
        .collision_table = {.position_x = 0x0050U, .position_y = 0x0300U},
    };
    const auto solid = scan_actor_collision(
        MappedRomView(rom, mpr),
        context,
        actor,
        CollisionProbe{.horizontal = 0x2DU, .vertical = 0x02F3U});
    check(
        solid.disposition == ActorCollisionScanDisposition::solid_mask
            && solid.vertical_proximity.intersecting
            && solid.shape && solid.geometry && solid.bounds && solid.mask
            && solid.bounds->relative_horizontal == 1U
            && solid.bounds->relative_vertical == 3U
            && solid.mask->evaluation.column_bit == 0x40U,
        "native actor collision composes its shape, bounds, and mask stages");

    store_rom(0x8A13U, 0x00U);
    const auto empty = scan_actor_collision(
        MappedRomView(rom, mpr),
        context,
        actor,
        CollisionProbe{.horizontal = 0x2DU, .vertical = 0x02F3U});
    check(
        empty.disposition == ActorCollisionScanDisposition::empty_mask
            && empty.mask,
        "native actor collision preserves an empty target mask as a non-contact");

    std::array<std::byte, GameStateView::kWorkRamSize> sweep_ram{};
    const auto store_sweep_ram = [&sweep_ram](
                                     const std::size_t offset,
                                     const std::uint8_t value) {
        sweep_ram[offset] = static_cast<std::byte>(value);
    };
    for (std::size_t index = 0U; index < context.shape_directories.tables.size();
         ++index) {
        const auto pointer = context.shape_directories.tables[index];
        store_sweep_ram(
            0x0096U + index * 2U, static_cast<std::uint8_t>(pointer));
        store_sweep_ram(
            0x0097U + index * 2U,
            static_cast<std::uint8_t>(pointer >> 8U));
    }
    const auto store_sweep_word = [&store_sweep_ram](
                                      const std::size_t offset,
                                      const std::uint16_t value) {
        store_sweep_ram(offset, static_cast<std::uint8_t>(value));
        store_sweep_ram(offset + 1U, static_cast<std::uint8_t>(value >> 8U));
    };
    store_sweep_word(0x0074U, context.mask_tables.first_table);
    store_sweep_word(0x0072U, context.mask_tables.second_table);
    store_sweep_word(0x008EU, context.mask_tables.masks);
    store_sweep_word(0x00C8U, 0x9000U);
    store_sweep_word(0x00C6U, 0xA000U);
    store_sweep_word(0x0090U, 0xC000U);
    store_sweep_ram(0x0254U, 0x01U);
    store_sweep_ram(0x00B7U, 0x02U);
    for (const auto slot : {0x04U, 0x06U}) {
        store_sweep_ram(0x1080U + slot, 0x80U);
        store_sweep_ram(0x1180U + slot, 0x50U);
        store_sweep_ram(0x1200U + slot, 0x00U);
        store_sweep_ram(0x1300U + slot, 0x00U);
        store_sweep_ram(0x1380U + slot, 0x03U);
        store_sweep_ram(0x1900U + slot, 0x06U);
        store_sweep_ram(0x1980U + slot, 0x03U);
    }
    store_sweep_ram(0x1980U + 0x04U, 0x02U);
    store_sweep_ram(0x1080U + 0x05U, 0xC0U);
    constexpr std::size_t kFullScanPlayfieldIndex = 90U * 32U + 5U;
    rom[MappedRomView::kBankBytes + 0x1000U + kFullScanPlayfieldIndex] =
        std::byte{0x5AU};
    rom[2U * MappedRomView::kBankBytes + kFullScanPlayfieldIndex] =
        std::byte{0x01U};
    rom[3U * MappedRomView::kBankBytes + 8U + 3U] = std::byte{0x04U};
    const auto sweep_state = GameStateView(sweep_ram);
    const auto first_sweep_actor = sweep_state.actor(0x04U);
    const auto first_sweep_candidate = first_sweep_actor
        ? scan_actor_collision(
              MappedRomView(rom, mpr),
              *make_actor_collision_scan_context(sweep_state),
              *first_sweep_actor,
              CollisionProbe{.horizontal = 0x2DU, .vertical = 0x02F3U})
        : devils_crush::native_runtime::ActorCollisionScanResult{};
    check(
        first_sweep_actor && first_sweep_actor->collision_shape_phase == 0x02U,
        "native actor collision sweep decodes the first candidate phase");
    check(
        first_sweep_candidate.disposition
                == ActorCollisionScanDisposition::empty_mask
            && first_sweep_candidate.mask_coordinates
            && first_sweep_candidate.mask,
        "native actor collision scan resolves the phase-two sweep fixture");
    check(
        first_sweep_candidate.mask_coordinates
            && first_sweep_candidate.mask_coordinates->updates_side_flags
            && first_sweep_candidate.mask_coordinates->side_flags == 0x82U,
        "native actor collision scan exposes CC1A's pre-lookup side-flag update");
    const auto no_contact_sweep = scan_actor_collision_sweep(
        MappedRomView(rom, mpr),
        sweep_state,
        CollisionProbe{.horizontal = 0x2DU, .vertical = 0x02F3U});
    check(
        no_contact_sweep && no_contact_sweep->candidates.size() == 2U
            && !no_contact_sweep->first_solid_actor_slot
            && no_contact_sweep->candidates[0].actor.index == 0x04U
            && no_contact_sweep->candidates[1].actor.index == 0x06U
            && no_contact_sweep->candidates[0].disposition
                == ActorCollisionScanDisposition::empty_mask
            && no_contact_sweep->candidates[0].mask_coordinates
            && no_contact_sweep->candidates[0].mask_coordinates->updates_side_flags
            && no_contact_sweep->candidates[1].mask_coordinates
            && no_contact_sweep->candidates[1].mask_coordinates->input.side_flags
                == 0x82U
            && no_contact_sweep->side_flags_after_scan == 0x82U,
        "native actor collision sweep keeps CED2 order and side-flag state");
    store_sweep_ram(0x0055U, 0x00U);
    store_sweep_ram(0x0060U, 0x03U);
    store_sweep_ram(0x0064U, 0x04U);
    store_sweep_ram(0x0065U, 0x2DU);
    store_sweep_ram(0x0066U, 0xF3U);
    store_sweep_ram(0x0067U, 0x02U);
    store_sweep_ram(0x1180U, 0x4DU);
    store_sweep_ram(0x1300U, 0xF3U);
    store_sweep_ram(0x1380U, 0x02U);
    const auto normal_walk = sweep_state.normal_actor_collision_scan_walk();
    const auto normal_probe = sweep_state.normal_actor_collision_probe();
    const auto normal_setup_probe =
        sweep_state.normal_actor_collision_setup_probe();
    const auto normal_no_contact_sweep = normal_walk
        ? scan_normal_actor_collision_sweep(
              MappedRomView(rom, mpr),
              sweep_state,
              *normal_walk,
              normal_setup_probe.value_or(CollisionProbe{}))
        : std::nullopt;
    check(
        normal_walk && normal_walk->first_slot == 0x04U
            && normal_walk->slot_count == 0x03U
            && !normal_walk->require_activity_bit_6
            && normal_probe
                == CollisionProbe{.horizontal = 0x2DU, .vertical = 0x02F3U}
            && normal_setup_probe
                == CollisionProbe{.horizontal = 0x2DU, .vertical = 0x02F3U}
            && normal_no_contact_sweep
            && normal_no_contact_sweep->candidates.size() == 2U
            && normal_no_contact_sweep->candidates[0].actor.index == 0x04U
            && normal_no_contact_sweep->candidates[1].actor.index == 0x06U,
        "native normal actor scan preserves CB2A's setup and CB48's walk");
    store_sweep_ram(0x0055U, 0x01U);
    const auto gated_normal_walk = sweep_state.normal_actor_collision_scan_walk();
    const auto gated_normal_sweep = gated_normal_walk
        ? scan_normal_actor_collision_sweep(
              MappedRomView(rom, mpr),
              sweep_state,
              *gated_normal_walk,
              CollisionProbe{.horizontal = 0x2DU, .vertical = 0x02F3U})
        : std::nullopt;
    check(
        gated_normal_walk && gated_normal_walk->require_activity_bit_6
            && gated_normal_sweep && gated_normal_sweep->candidates.empty(),
        "native normal actor scan excludes actors outside CB55's alternate subset");
    store_sweep_ram(0x1000U + 0x04U, 0x40U);
    const auto alternate_normal_sweep = gated_normal_walk
        ? scan_normal_actor_collision_sweep(
              MappedRomView(rom, mpr),
              sweep_state,
              *gated_normal_walk,
              CollisionProbe{.horizontal = 0x2DU, .vertical = 0x02F3U})
        : std::nullopt;
    check(
        alternate_normal_sweep && alternate_normal_sweep->candidates.size() == 1U
            && alternate_normal_sweep->candidates.front().actor.index == 0x04U,
        "native normal actor scan admits CB55's activity-bit-six subset");
    store_sweep_ram(0x1000U + 0x04U, 0x00U);
    store_sweep_ram(0x0055U, 0x00U);
    store_sweep_ram(0x0060U, 0x02U);
    store_sweep_ram(0x0064U, 0x7FU);
    const auto out_of_range_normal_walk =
        sweep_state.normal_actor_collision_scan_walk();
    check(
        out_of_range_normal_walk
            && !scan_normal_actor_collision_sweep(
                MappedRomView(rom, mpr),
                sweep_state,
                *out_of_range_normal_walk,
                CollisionProbe{.horizontal = 0x2DU, .vertical = 0x02F3U}),
        "native normal actor scan refuses an unmodelled wrapped actor range");
    store_sweep_ram(0x0060U, 0x03U);
    store_sweep_ram(0x0064U, 0x04U);

    const CollisionRetryProbeResult retry_full_scan_input{
        .adjusted_horizontal_low_byte = 0x2DU,
        .adjusted_horizontal_high_byte = 0U,
        .vertical_low_byte = 0xF3U,
        .disposition = CollisionRetryProbeDisposition::requires_full_scan,
    };
    const auto retry_actor_scan = scan_collision_retry_full_actors(
        MappedRomView(rom, mpr), sweep_state, retry_full_scan_input, 0x02U);
    check(
        retry_actor_scan && retry_actor_scan->probe
                == CollisionProbe{.horizontal = 0x2DU, .vertical = 0x02F3U}
            && retry_actor_scan->actor_sweep == *no_contact_sweep
            && !scan_collision_retry_full_actors(
                MappedRomView(rom, mpr),
                sweep_state,
                CollisionRetryProbeResult{},
                0x02U),
        "native retry actor scan composes only CEB5's full-scan handoff");
    const auto retry_full_scan = scan_collision_retry_full(
        MappedRomView(rom, mpr), sweep_state, retry_full_scan_input, 0x02U);
    check(
        retry_full_scan && retry_full_scan->actor_scan == *retry_actor_scan
            && retry_full_scan->disposition
                == CollisionRetryFullScanDisposition::playfield_evaluated
            && retry_full_scan->playfield
            && retry_full_scan->playfield->disposition
                == devils_crush::native_runtime::PlayfieldCollisionDisposition::solid
            && retry_full_scan->playfield->map.tile_id == 0x5AU,
        "native retry full scan runs CD74 only after its actor sweep is clear");
    auto clear_retry_full_scan = *retry_full_scan;
    clear_retry_full_scan.playfield->disposition =
        devils_crush::native_runtime::PlayfieldCollisionDisposition::empty;
    clear_retry_full_scan.playfield->map.disposition =
        devils_crush::native_runtime::CollisionMapDisposition::empty;
    auto unresolved_playfield_ram = sweep_ram;
    unresolved_playfield_ram[0x00C8U] = std::byte{0x00U};
    unresolved_playfield_ram[0x00C9U] = std::byte{0xE0U};
    auto unresolved_playfield_mpr = mpr;
    unresolved_playfield_mpr[7U] = 0x80U;
    const auto unresolved_playfield_scan = scan_collision_retry_full(
        MappedRomView(rom, unresolved_playfield_mpr),
        GameStateView(unresolved_playfield_ram),
        retry_full_scan_input,
        0x02U);
    check(
        unresolved_playfield_scan && unresolved_playfield_scan->disposition
                == CollisionRetryFullScanDisposition::unresolved_playfield_asset
            && !unresolved_playfield_scan->playfield,
        "native retry full scan preserves an unmapped CD74 asset as unresolved");
    check(
        resolve_collision_retry_full_scan_outcome(*retry_full_scan)
                == CollisionRetryFullScanOutcome::solid
            && resolve_collision_retry_full_scan_outcome(clear_retry_full_scan)
                == CollisionRetryFullScanOutcome::clear
            && resolve_collision_retry_full_scan_outcome(*unresolved_playfield_scan)
                == CollisionRetryFullScanOutcome::unresolved,
        "native retry full scan exposes CED2's carry-like playfield outcomes");

    store_rom(0x8A13U, 0x40U);
    const auto solid_sweep = scan_actor_collision_sweep(
        MappedRomView(rom, mpr),
        GameStateView(sweep_ram),
        CollisionProbe{.horizontal = 0x2DU, .vertical = 0x02F3U});
    check(
        solid_sweep && solid_sweep->candidates.size() == 1U
            && solid_sweep->first_solid_actor_slot == 0x04U
            && solid_sweep->candidates.front().disposition
                == ActorCollisionScanDisposition::solid_mask,
        "native actor collision sweep stops at CED2's first solid mask");
    const auto actor_first_full_scan = scan_collision_retry_full(
        MappedRomView(rom, mpr), sweep_state, retry_full_scan_input, 0x02U);
    check(
        actor_first_full_scan && actor_first_full_scan->disposition
                == CollisionRetryFullScanDisposition::actor_solid
            && !actor_first_full_scan->playfield,
        "native retry full scan bypasses CD74 after CED2's first solid actor");
    check(
        resolve_collision_retry_full_scan_outcome(*actor_first_full_scan)
            == CollisionRetryFullScanOutcome::solid,
        "native retry full scan exposes a solid actor as CED2's set-carry outcome");
}

void test_native_collision_map()
{
    using devils_crush::native_runtime::CollisionMap;
    using devils_crush::native_runtime::CollisionMapDisposition;
    using devils_crush::native_runtime::CollisionMapPoint;
    using devils_crush::native_runtime::CollisionMask;
    using devils_crush::native_runtime::PlayfieldCollisionDisposition;
    using devils_crush::native_runtime::PlayfieldCollisionProbe;
    using devils_crush::native_runtime::evaluate_collision_map;
    using devils_crush::native_runtime::evaluate_playfield_collision;

    std::array<std::uint8_t, CollisionMap::kColumns * 2U> tile_ids{};
    std::array<std::uint8_t, CollisionMap::kColumns * 2U> mask_indices{};
    std::array<CollisionMask, 2> masks{};
    tile_ids[0] = 0x4EU;
    mask_indices[0] = 1U;
    masks[1].rows[4] = 0x10U;
    const CollisionMap map{
        .tile_ids = tile_ids,
        .mask_indices = mask_indices,
        .masks = masks,
    };
    const auto solid = evaluate_collision_map(
        map, CollisionMapPoint{.horizontal_pixels = 3U, .vertical_pixels = 4U});
    check(
        solid.disposition == CollisionMapDisposition::solid
            && solid.tile_column == 0U && solid.tile_row == 0U
            && solid.tile_id == 0x4EU && solid.mask_index == 1U
            && solid.mask.column_bit == 0x10U,
        "native collision map preserves tile identity while testing its mask");

    const auto empty = evaluate_collision_map(
        map, CollisionMapPoint{.horizontal_pixels = 2U, .vertical_pixels = 4U});
    check(
        empty.disposition == CollisionMapDisposition::empty,
        "native collision map preserves clear cells within a selected mask");

    const auto outside = evaluate_collision_map(
        map,
        CollisionMapPoint{
            .horizontal_pixels = static_cast<std::uint16_t>(
                CollisionMap::kColumns * CollisionMap::kTilePixels),
        });
    check(
        outside.disposition == CollisionMapDisposition::outside_map,
        "native collision map rejects a point beyond its 32-tile width");

    mask_indices[1] = 2U;
    const auto missing = evaluate_collision_map(
        map, CollisionMapPoint{.horizontal_pixels = 8U});
    check(
        missing.disposition == CollisionMapDisposition::missing_mask,
        "native collision map reports malformed mask selection explicitly");

    const auto playfield_solid = evaluate_playfield_collision(
        map,
        PlayfieldCollisionProbe{.horizontal_pixels = 3U, .vertical_pixels = 36U});
    check(
        playfield_solid.disposition == PlayfieldCollisionDisposition::solid
            && playfield_solid.map_point.vertical_pixels == 4U
            && playfield_solid.map.tile_id == 0x4EU,
        "native playfield collision converts table coordinates before lookup");

    const auto above_playfield = evaluate_playfield_collision(
        map,
        PlayfieldCollisionProbe{.horizontal_pixels = 3U, .vertical_pixels = 31U});
    check(
        above_playfield.disposition
            == PlayfieldCollisionDisposition::outside_playfield,
        "native playfield collision rejects a point above the table");
}

void test_native_collision_map_asset()
{
    using devils_crush::native_runtime::CollisionMapDisposition;
    using devils_crush::native_runtime::CollisionMapEvaluation;
    using devils_crush::native_runtime::CollisionMapPoint;
    using devils_crush::native_runtime::CollisionMapAssetLayout;
    using devils_crush::native_runtime::MappedRomView;
    using devils_crush::native_runtime::decode_collision_map_asset;
    using devils_crush::native_runtime::decode_main_playfield_collision_map;
    using devils_crush::native_runtime::evaluate_collision_map;
    using devils_crush::native_runtime::main_playfield_collision_map_layout;

    std::array<std::byte, 32> tile_bytes{};
    std::array<std::byte, 32> attribute_bytes{};
    std::array<std::byte, 16> mask_bytes{};
    tile_bytes[0] = std::byte{0x3AU};
    attribute_bytes[0] = std::byte{1U};
    mask_bytes[8U + 6U] = std::byte{0x04U};

    const auto asset = decode_collision_map_asset(
        tile_bytes, attribute_bytes, mask_bytes);
    const auto evaluation = asset
        ? evaluate_collision_map(
              asset->view(),
              CollisionMapPoint{.horizontal_pixels = 5U, .vertical_pixels = 6U})
        : CollisionMapEvaluation{};
    check(
        asset && evaluation.disposition == CollisionMapDisposition::solid
            && evaluation.tile_id == 0x3AU && evaluation.mask_index == 1U,
        "native collision asset decodes parallel map planes and mask rows");

    attribute_bytes[0] = std::byte{2U};
    check(
        !decode_collision_map_asset(tile_bytes, attribute_bytes, mask_bytes),
        "native collision asset rejects an unresolved mask attribute");
    check(
        !decode_collision_map_asset(
            std::span<const std::byte>{tile_bytes}.first(31U),
            attribute_bytes,
            mask_bytes),
        "native collision asset rejects incomplete map rows");

    std::vector<std::byte> rom(MappedRomView::kSupportedRomBytes);
    std::array<std::uint8_t, 8> mpr{};
    mpr[4] = 0x01U;
    const auto store = [&rom](const std::uint16_t address,
                               const std::uint8_t value) {
        rom[0x01U * MappedRomView::kBankBytes
            + (address & (MappedRomView::kBankBytes - 1U))] =
            static_cast<std::byte>(value);
    };
    store(0x8100U, 0x6BU);
    store(0x8140U, 0x01U);
    store(0x818FU, 0x04U);
    const MappedRomView mapped_rom(rom, mpr);
    const auto mapped_asset = decode_collision_map_asset(
        mapped_rom,
        CollisionMapAssetLayout{
            .tile_ids_address = 0x8100U,
            .collision_attributes_address = 0x8140U,
            .masks_address = 0x8180U,
            .map_rows = 1U,
            .mask_count = 2U,
        });
    const auto mapped_evaluation = mapped_asset
        ? evaluate_collision_map(
              mapped_asset->view(),
              CollisionMapPoint{.horizontal_pixels = 5U, .vertical_pixels = 7U})
        : CollisionMapEvaluation{};
    check(
        mapped_asset
            && mapped_evaluation.disposition == CollisionMapDisposition::solid
            && mapped_evaluation.tile_id == 0x6BU,
        "native collision asset resolves its table layout through mapped ROM");

    mpr[4] = 0x41U;
    rom[0x21U * MappedRomView::kBankBytes + 0x100U] = std::byte{0x9CU};
    check(
        MappedRomView(rom, mpr).byte_at(0x8100U) == std::byte{0x9CU},
        "native mapped ROM preserves the cartridge's upper-bank mirror");

    const auto main_layout = main_playfield_collision_map_layout({
        .tile_ids = 0xABC6U,
        .collision_attributes = 0xA006U,
        .masks = 0xB686U,
    });
    check(
        main_layout.tile_ids_address == 0xABC6U
            && main_layout.collision_attributes_address == 0xA006U
            && main_layout.masks_address == 0xB686U
            && main_layout.map_rows == 94U && main_layout.mask_count == 250U,
        "native main-table collision layout binds live pointers to verified dimensions");

    std::array<std::uint8_t, 8> main_mpr{};
    main_mpr[5] = 0x43U;
    const auto store_main = [&rom](const std::uint16_t address,
                                   const std::uint8_t value) {
        rom[0x23U * MappedRomView::kBankBytes
            + (address & (MappedRomView::kBankBytes - 1U))] =
            static_cast<std::byte>(value);
    };
    store_main(0xABC6U, 0x4AU);
    store_main(0xA006U, 0x01U);
    store_main(0xB692U, 0x10U);
    const auto main_asset = decode_main_playfield_collision_map(
        MappedRomView(rom, main_mpr),
        {
            .tile_ids = 0xABC6U,
            .collision_attributes = 0xA006U,
            .masks = 0xB686U,
        });
    const auto main_evaluation = main_asset
        ? evaluate_collision_map(
              main_asset->view(),
              CollisionMapPoint{.horizontal_pixels = 3U, .vertical_pixels = 4U})
        : CollisionMapEvaluation{};
    check(
        main_asset
            && main_evaluation.disposition == CollisionMapDisposition::solid
            && main_evaluation.tile_id == 0x4AU
            && main_evaluation.mask_index == 1U,
        "native main-table decoder resolves captured pointers through mapped ROM");
}

void test_native_playfield_collision_scan()
{
    using devils_crush::native_runtime::BallState;
    using devils_crush::native_runtime::CollisionMap;
    using devils_crush::native_runtime::CollisionMask;
    using devils_crush::native_runtime::Fixed24_8;
    using devils_crush::native_runtime::PlayfieldCollisionDisposition;
    using devils_crush::native_runtime::advance_ball_motion_substeps;
    using devils_crush::native_runtime::first_playfield_contact;
    using devils_crush::native_runtime::scan_playfield_collisions;

    std::array<std::uint8_t, CollisionMap::kColumns> tile_ids{};
    std::array<std::uint8_t, CollisionMap::kColumns> mask_indices{};
    std::array<CollisionMask, 2> masks{};
    tile_ids[0] = 0x52U;
    mask_indices[0] = 1U;
    masks[1].rows[4] = 0x04U;
    const CollisionMap map{
        .tile_ids = tile_ids,
        .mask_indices = mask_indices,
        .masks = masks,
    };
    const auto scan = scan_playfield_collisions(
        advance_ball_motion_substeps({
            .position_x = Fixed24_8{.raw = 1 * 256},
            .position_y = Fixed24_8{.raw = 36 * 256},
            .velocity_x = 0x0800,
        }),
        map);
    check(
        scan.completed_probes == 2U && scan.first_solid_substep == 1U
            && scan.probes[1].collision.disposition
                == PlayfieldCollisionDisposition::solid
            && scan.probes[1].collision.map.tile_id == 0x52U,
        "native playfield scan stops at its first solid collision substep");
    const auto contact = first_playfield_contact(scan);
    check(
        contact && contact->substep_index == 1U
            && contact->entry_ball.position_x.raw == 3 * 256
            && contact->impact_ball.position_x.raw == 5 * 256
            && contact->impact.map.tile_id == 0x52U,
        "native playfield contact preserves the first impact state and tile");

    const auto outside = scan_playfield_collisions(
        advance_ball_motion_substeps({
            .position_x = Fixed24_8{.raw = -256},
            .position_y = Fixed24_8{.raw = 36 * 256},
        }),
        map);
    check(
        outside.completed_probes == 1U && !outside.probes[0].position_in_world
            && !outside.first_solid_substep,
        "native playfield scan rejects a ball outside world coordinates");
}

void test_native_physics_state()
{
    using devils_crush::native_runtime::BallMotionTuning;
    using devils_crush::native_runtime::BallSubstepProgress;
    using devils_crush::native_runtime::CollisionMap;
    using devils_crush::native_runtime::CollisionMask;
    using devils_crush::native_runtime::BallCollisionLoopDisposition;
    using devils_crush::native_runtime::BallState;
    using devils_crush::native_runtime::Fixed24_8;
    using devils_crush::native_runtime::PhysicsState;
    using devils_crush::native_runtime::advance_physics_with_playfield_scan;
    using devils_crush::native_runtime::advance_physics_without_contact;
    using devils_crush::native_runtime::advance_ball_collision_loop;
    using devils_crush::native_runtime::begin_ball_collision_loop;
    using devils_crush::native_runtime::begin_prepared_ball_collision_loop;
    using devils_crush::native_runtime::resume_ball_collision_after_response;
    using devils_crush::native_runtime::retry_ball_collision_substep;

    const PhysicsState state{
        .ball = {
            .position_x = Fixed24_8{.raw = 16 * 256},
            .position_y = Fixed24_8{.raw = 32 * 256},
            .velocity_x = 0x0803,
            .velocity_y = -0x0401,
        },
        .contact = {.contact_latched = true},
    };
    const auto advanced = advance_physics_without_contact(
        state,
        BallMotionTuning{
            .horizontal_drag_divisor = 8,
            .vertical_drag_divisor = 8,
            .vertical_gravity = 1U,
        });
    check(
        advanced && advanced->previous == state
            && advanced->movement.completed_steps == 4U
            && advanced->state.ball.position_x.raw == 24 * 256
            && advanced->state.ball.position_y.raw == 28 * 256 - 4,
        "native physics state composes unobstructed movement into one frame");
    check(
        advanced && advanced->state.ball.velocity_x == 0x0703
            && advanced->state.ball.velocity_y == -0x0380
            && advanced->state.contact == state.contact,
        "native physics state damps post-loop velocity without losing contacts");

    const auto invalid = advance_physics_without_contact(
        PhysicsState{.ball = {.velocity_x = 1}},
        BallMotionTuning{.horizontal_drag_divisor = 0});
    check(
        !invalid,
        "native physics state rejects invalid moving-axis damping safely");

    std::array<std::uint8_t, 4U * CollisionMap::kColumns> tile_ids{};
    std::array<std::uint8_t, 4U * CollisionMap::kColumns> mask_indices{};
    std::array<CollisionMask, 2> masks{};
    const CollisionMap empty_map{
        .tile_ids = tile_ids,
        .mask_indices = mask_indices,
        .masks = masks,
    };
    const auto clear = advance_physics_with_playfield_scan(
        state,
        BallMotionTuning{
            .horizontal_drag_divisor = 8,
            .vertical_drag_divisor = 8,
            .vertical_gravity = 1U,
        },
        empty_map);
    check(
        clear && !clear->requires_collision_resolution()
            && clear->no_contact == advanced
            && clear->collision_scan.completed_probes == 4U,
        "native physics completes a clear playfield frame through damping");

    const auto solid_map_index = 3U * CollisionMap::kColumns + 4U;
    tile_ids[solid_map_index] = 0x7CU;
    mask_indices[solid_map_index] = 1U;
    masks[1].rows[3] = 0x80U;
    const CollisionMap solid_map{
        .tile_ids = tile_ids,
        .mask_indices = mask_indices,
        .masks = masks,
    };
    const auto blocked = advance_physics_with_playfield_scan(
        PhysicsState{
            .ball = {
                .position_x = Fixed24_8{.raw = 31 * 256},
                .position_y = Fixed24_8{.raw = 59 * 256},
                .velocity_x = 0x0100,
            },
        },
        BallMotionTuning{.horizontal_drag_divisor = 0},
        solid_map);
    check(
        blocked && blocked->requires_collision_resolution()
            && blocked->contact && !blocked->no_contact
            && blocked->contact->substep_index == 0U
            && blocked->contact->entry_ball.position_x.raw == 31 * 256
            && blocked->contact->impact_ball.position_x.raw == 32 * 256
            && blocked->contact->impact.map.tile_id == 0x7CU,
        "native physics defers a solid playfield contact before damping");

    const auto clear_loop_step = advance_ball_collision_loop(
        begin_ball_collision_loop(state),
        BallMotionTuning{
            .horizontal_drag_divisor = 8,
            .vertical_drag_divisor = 8,
            .vertical_gravity = 1U,
        },
        empty_map);
    check(
        clear_loop_step
            && clear_loop_step->disposition
                == BallCollisionLoopDisposition::substep_clear
            && clear_loop_step->movement
            && clear_loop_step->state.completed_substeps == 1U
            && clear_loop_step->state.remaining_attempts == 15U
            && clear_loop_step->state.state.ball.velocity_x == state.ball.velocity_x
            && clear_loop_step->state.state.ball.velocity_y == state.ball.velocity_y,
        "native collision loop retains frame velocity while consuming a clear substep");

    const PhysicsState prepared_state{
        .ball = {
            .position_x = Fixed24_8{.raw = 20 * 256},
            .position_y = Fixed24_8{.raw = 40 * 256},
            // These velocities deliberately do not produce the captured
            // increment if they are split again. C721 retains DC95's cursor.
            .velocity_x = 0x0A03,
            .velocity_y = -0x0901,
        },
    };
    const auto prepared_loop = begin_prepared_ball_collision_loop(
        prepared_state,
        BallSubstepProgress{
            .increment = {.horizontal = 0x0180, .vertical = -0x0080},
            .completed_steps = 1U,
            .requested_steps = 4U,
        });
    const auto prepared_step = prepared_loop
        ? advance_ball_collision_loop(
            *prepared_loop,
            BallMotionTuning{.horizontal_drag_divisor = 8,
                              .vertical_drag_divisor = 8},
            empty_map)
        : std::optional<devils_crush::native_runtime::BallCollisionLoopUpdate>{};
    check(
        prepared_loop && prepared_loop->completed_substeps == 1U
            && prepared_loop->remaining_attempts == 16U
            && prepared_loop->plan.count == 4U
            && prepared_loop->plan.velocity_x_per_step == 0x0180
            && prepared_loop->plan.velocity_y_per_step == -0x0080
            && prepared_step
            && prepared_step->disposition
                == BallCollisionLoopDisposition::substep_clear
            && prepared_step->probe
            && prepared_step->probe->substep_index == 1U
            && prepared_step->state.completed_substeps == 2U
            && prepared_step->state.state.ball.position_x.raw == 21 * 256 + 128
            && prepared_step->state.state.ball.position_y.raw == 39 * 256 + 128
            && prepared_step->state.state.ball.velocity_x
                == prepared_state.ball.velocity_x
            && prepared_step->state.state.ball.velocity_y
                == prepared_state.ball.velocity_y
            && !begin_prepared_ball_collision_loop(
                prepared_state,
                BallSubstepProgress{
                    .completed_steps = 1U,
                    .requested_steps = 3U,
                }),
        "native C721 collision loop resumes DC95's captured increment and cursor");

    const PhysicsState contact_state{
        .ball = {
            .position_x = Fixed24_8{.raw = 31 * 256},
            .position_y = Fixed24_8{.raw = 59 * 256},
            .velocity_x = 0x0100,
        },
    };
    const auto collision_loop_contact = advance_ball_collision_loop(
        begin_ball_collision_loop(contact_state),
        BallMotionTuning{.horizontal_drag_divisor = 8},
        solid_map);
    const auto separating_retry = collision_loop_contact
        ? retry_ball_collision_substep(collision_loop_contact->state)
        : devils_crush::native_runtime::BallCollisionLoop{};
    check(
        collision_loop_contact
            && collision_loop_contact->disposition
                == BallCollisionLoopDisposition::playfield_contact
            && collision_loop_contact->contact
            && collision_loop_contact->state.completed_substeps == 0U
            && collision_loop_contact->state.remaining_attempts == 15U
            && collision_loop_contact->state.state.ball == contact_state.ball
            && separating_retry == collision_loop_contact->state,
        "native collision loop keeps CF8F's separating contact on the same substep");

    auto response_ball = collision_loop_contact && collision_loop_contact->contact
        ? collision_loop_contact->contact->impact_ball
        : BallState{};
    response_ball.velocity_x = 0x0800;
    const auto response_loop = resume_ball_collision_after_response(
        collision_loop_contact ? collision_loop_contact->state
                               : devils_crush::native_runtime::BallCollisionLoop{},
        response_ball);
    const auto response_loop_step = advance_ball_collision_loop(
        response_loop,
        BallMotionTuning{.horizontal_drag_divisor = 8},
        empty_map);
    check(
        collision_loop_contact && response_loop.completed_substeps == 1U
            && response_loop.plan.count == 4U
            && response_loop.remaining_attempts == 15U
            && response_loop_step
            && response_loop_step->disposition
                == BallCollisionLoopDisposition::substep_clear
            && response_loop_step->movement
            && response_loop_step->movement->state.position_x.raw == 34 * 256,
        "native collision loop applies CF33's DC95 resplit after its consumed contact");
}

void test_native_prepared_gameplay_collision_loop()
{
    using devils_crush::native_runtime::BallFrameOwner;
    using devils_crush::native_runtime::BallFrameSchedule;
    using devils_crush::native_runtime::BallMotionTuning;
    using devils_crush::native_runtime::BallSubstepProgress;
    using devils_crush::native_runtime::CollisionMap;
    using devils_crush::native_runtime::CollisionMask;
    using devils_crush::native_runtime::Fixed24_8;
    using devils_crush::native_runtime::NativeGameplayState;
    using devils_crush::native_runtime::NativeGameplayRuntime;

    std::array<std::uint8_t, 4U * CollisionMap::kColumns> tile_ids{};
    std::array<std::uint8_t, 4U * CollisionMap::kColumns> mask_indices{};
    std::array<CollisionMask, 1U> masks{};
    const CollisionMap clear_map{
        .tile_ids = tile_ids,
        .mask_indices = mask_indices,
        .masks = masks,
    };

    NativeGameplayState gameplay;
    gameplay.seed_ball({
        .position_x = Fixed24_8{.raw = 20 * 256},
        .position_y = Fixed24_8{.raw = 40 * 256},
        .velocity_x = 0x0A03,
        .velocity_y = -0x0901,
    });
    const auto completed = gameplay.resume_prepared_ball_collision_loop(
        {
            .increment = {.horizontal = 0x0180, .vertical = -0x0080},
            .completed_steps = 1U,
            .requested_steps = 4U,
        },
        BallMotionTuning{.horizontal_drag_divisor = 8,
                         .vertical_drag_divisor = 8},
        clear_map);
    check(
        completed && completed->completes_frame() && !completed->next_contact
            && gameplay.physics().ball.position_x.raw == 24 * 256 + 128
            && gameplay.physics().ball.position_y.raw == 38 * 256 + 128
            && gameplay.physics().ball.velocity_x == 0x08C3
            && gameplay.physics().ball.velocity_y == -0x07E1,
        "native gameplay state completes a captured C721 suffix through DD09");

    gameplay.seed_ball({
        .position_x = Fixed24_8{.raw = 239 * 256},
        .velocity_x = 0x0100,
    });
    const auto initial_handoff = gameplay.resume_initial_ball_motion(
        {
            .increment = {.horizontal = 0x0100},
            .requested_steps = 1U,
        },
        BallMotionTuning{.horizontal_drag_divisor = 8}, clear_map);
    check(
        initial_handoff && initial_handoff->completes_initial_pass()
            && initial_handoff->physics.completes_frame()
            && initial_handoff->transition
                == devils_crush::native_runtime::BallInitialTransition{
                    .horizontal_pixels = 240U,
                    .disposition = devils_crush::native_runtime::
                        BallInitialTransitionDisposition::returns_to_caller,
                }
            && initial_handoff->next_substeps
            && initial_handoff->next_substeps->count == 1U
            && initial_handoff->next_substep_progress
            && initial_handoff->next_substep_progress->requested_steps == 1U,
        "native C6C2 handoff evaluates its gate after the captured cursor");

    gameplay.seed_ball({
        .position_x = Fixed24_8{.raw = 239 * 256},
        .velocity_x = 0x0100,
    });
    const auto initial_schedule = gameplay.resume_initial_ball_motion(
        {
            .increment = {.horizontal = 0x0100},
            .completed_steps = 1U,
            .requested_steps = 1U,
        },
        BallMotionTuning{.horizontal_drag_divisor = 8}, clear_map);
    check(
        initial_schedule && initial_schedule->completes_initial_pass()
            && initial_schedule->transition
                == devils_crush::native_runtime::BallInitialTransition{
                    .horizontal_pixels = 239U,
                    .disposition = devils_crush::native_runtime::
                        BallInitialTransitionDisposition::schedules_normal_motion,
                },
        "native C6C2 handoff preserves the low-side C721 schedule decision");

    masks[0].rows.fill(0xFFU);
    tile_ids[3U * CollisionMap::kColumns + 4U] = 0x7CU;
    const CollisionMap solid_map{
        .tile_ids = tile_ids,
        .mask_indices = mask_indices,
        .masks = masks,
    };
    gameplay.seed_ball({
        .position_x = Fixed24_8{.raw = 31 * 256},
        .position_y = Fixed24_8{.raw = 59 * 256},
        .velocity_x = 0x0100,
    });
    const auto contact = gameplay.resume_prepared_ball_collision_loop(
        {
            .increment = {.horizontal = 0x0100},
            .requested_steps = 1U,
        },
        BallMotionTuning{.horizontal_drag_divisor = 8},
        solid_map);
    check(
        contact && !contact->completes_frame() && contact->next_contact
            && contact->next_contact->substep_index == 0U
            && gameplay.pending_playfield_contact() == contact->next_contact
            && gameplay.pending_playfield_collision_loop()
            && gameplay.pending_playfield_collision_loop()->completed_substeps == 0U
            && gameplay.pending_playfield_collision_loop()->plan.velocity_x_per_step
                == 0x0100,
        "native gameplay state retains a captured C721 contact for CF33/CDF8");

    masks[0].rows.fill(0U);
    gameplay.seed_ball({
        .position_x = Fixed24_8{.raw = 20 * 256},
        .position_y = Fixed24_8{.raw = 40 * 256},
        .velocity_x = 0x0A03,
        .velocity_y = -0x0901,
    });
    const auto scheduled = gameplay.resume_scheduled_ball_motion(
        {
            .owner = BallFrameOwner::normal_motion,
            .substep_progress = BallSubstepProgress{
                .increment = {.horizontal = 0x0180, .vertical = -0x0080},
                .completed_steps = 1U,
                .requested_steps = 4U,
            },
        },
        BallMotionTuning{.horizontal_drag_divisor = 8,
                         .vertical_drag_divisor = 8},
        clear_map);
    check(
        scheduled.disposition
                == NativeGameplayState::ScheduledBallMotionDisposition::resumed
            && scheduled.physics && scheduled.physics->completes_frame(),
        "native gameplay state dispatches a captured C721 owner to its prepared loop");

    gameplay.seed_ball({
        .position_x = Fixed24_8{.raw = 239 * 256},
        .velocity_x = 0x0100,
    });
    const auto initial = gameplay.resume_scheduled_ball_motion(
        {
            .owner = BallFrameOwner::initial_substep_transition,
            .substep_progress = BallSubstepProgress{
                .increment = {.horizontal = 0x0100},
                .completed_steps = 1U,
                .requested_steps = 1U,
            },
        },
        BallMotionTuning{.horizontal_drag_divisor = 8,
                         .vertical_drag_divisor = 8}, clear_map);
    const auto missing_cursor = gameplay.resume_scheduled_ball_motion(
        {.owner = BallFrameOwner::normal_motion}, BallMotionTuning{}, clear_map);
    check(
        initial.disposition
                == NativeGameplayState::ScheduledBallMotionDisposition::resumed
            && !initial.physics && initial.initial
            && initial.initial->completes_initial_pass()
            && initial.initial->transition
                && initial.initial->transition->disposition
                    == devils_crush::native_runtime::
                        BallInitialTransitionDisposition::schedules_normal_motion
            && missing_cursor.disposition
                == NativeGameplayState::ScheduledBallMotionDisposition::unavailable
            && !missing_cursor.physics,
        "native gameplay scheduler resumes C6C2 and rejects normal motion without a cursor");

    NativeGameplayRuntime initial_runtime;
    const bool initial_runtime_bootstrapped = initial_runtime.bootstrap({
        .ball = {
            .state = {
                .position_x = Fixed24_8{.raw = 239 * 256},
                .velocity_x = 0x0100,
            },
        },
        .ball_frame_schedule = {
            .owner = BallFrameOwner::initial_substep_transition,
            .substep_progress = BallSubstepProgress{
                .increment = {.horizontal = 0x0100},
                .completed_steps = 1U,
                .requested_steps = 1U,
            },
            .collision_shape = devils_crush::native_runtime::
                BallCollisionShapeSelection{
                    .directory_index = 0x42U,
                    .phase = 0x17U,
                },
        },
    });
    const auto initial_runtime_update = initial_runtime.advance_initial_ball_motion(
        BallMotionTuning{.horizontal_drag_divisor = 8}, clear_map);
    check(
        initial_runtime_bootstrapped && initial_runtime_update
            && initial_runtime_update->motion.completes_initial_pass()
            && !initial_runtime_update->returned_to_slot_zero_actor_loop
            && initial_runtime_update->next_schedule.owner
                == BallFrameOwner::normal_motion
            && initial_runtime_update->next_schedule.substep_progress
                == initial_runtime_update->motion.next_substep_progress
            && initial_runtime_update->next_schedule.collision_shape
                == devils_crush::native_runtime::BallCollisionShapeSelection{}
            && initial_runtime.ball_frame_schedule()
                == initial_runtime_update->next_schedule,
        "native gameplay runtime owns C6C2's next C721 schedule");

    NativeGameplayRuntime returning_initial_runtime;
    const bool returning_initial_runtime_bootstrapped =
        returning_initial_runtime.bootstrap({
            .ball = {
                .state = {
                    .position_x = Fixed24_8{.raw = 240 * 256},
                    .velocity_x = 0x0100,
                },
            },
            .ball_frame_schedule = {
                .owner = BallFrameOwner::initial_substep_transition,
                .substep_progress = BallSubstepProgress{
                    .increment = {.horizontal = 0x0100},
                    .completed_steps = 1U,
                    .requested_steps = 1U,
                },
                .collision_shape = devils_crush::native_runtime::
                    BallCollisionShapeSelection{
                        .directory_index = 0x42U,
                        .phase = 0x17U,
                    },
            },
        });
    const auto returning_initial_runtime_update =
        returning_initial_runtime.advance_initial_ball_motion(
            BallMotionTuning{.horizontal_drag_divisor = 8}, clear_map);
    check(
        returning_initial_runtime_bootstrapped
            && returning_initial_runtime_update
            && returning_initial_runtime_update->motion.completes_initial_pass()
            && returning_initial_runtime_update->returned_to_slot_zero_actor_loop
            && returning_initial_runtime_update->next_schedule.owner
                == BallFrameOwner::initial_substep_transition
            && returning_initial_runtime_update->next_schedule.collision_shape
                == devils_crush::native_runtime::BallCollisionShapeSelection{
                    .directory_index = 0x42U,
                    .phase = 0x17U,
                },
        "native gameplay runtime records C6C2's slot-zero actor-loop return");
}

void test_native_contact_dispatch()
{
    using devils_crush::native_runtime::ContactDisposition;
    using devils_crush::native_runtime::ContactState;
    using devils_crush::native_runtime::ContactTarget;
    using devils_crush::native_runtime::GameStateView;
    using devils_crush::native_runtime::dispatch_contact;
    using devils_crush::native_runtime::make_confirmed_contact_target;

    const auto type_82_target = make_confirmed_contact_target({
        .index = 0x62U,
        .activity_flags = 0x82U,
    });
    const auto inactive_target = make_confirmed_contact_target({
        .index = 0x62U,
        .activity_flags = 0x02U,
    });
    check(
        type_82_target
            && *type_82_target == ContactTarget{
                .actor_slot = 0x62U,
                .response_kind = 2U,
            }
            && !inactive_target,
        "native contact target mapper preserves CBCF's active type-82 response selection");

    const auto repeated = dispatch_contact(
        ContactState{.last_target = std::uint8_t{0x11U}},
        ContactTarget{.actor_slot = 0x11U, .response_kind = 5U});
    check(
        repeated.disposition == ContactDisposition::repeated_target
            && !repeated.target.marked_for_response,
        "native contact dispatch suppresses an immediate repeat target");

    const auto latched = dispatch_contact(
        ContactState{.contact_latched = true},
        ContactTarget{.actor_slot = 0x12U, .response_kind = 5U});
    check(
        latched.disposition == ContactDisposition::contact_already_handled
            && !latched.target.marked_for_response,
        "native contact dispatch defers a target after an earlier handled contact");

    const auto dispatched = dispatch_contact(
        ContactState{},
        ContactTarget{.actor_slot = 0x12U, .response_kind = 1U});
    check(
        dispatched.disposition == ContactDisposition::dispatched
            && dispatched.target.marked_for_response
            && dispatched.state.last_target == 0x12U,
        "native contact dispatch marks and records a new target");

    const auto released = dispatch_contact(
        ContactState{},
        ContactTarget{.actor_slot = 0x13U, .response_kind = 2U});
    check(
        released.disposition == ContactDisposition::dispatched
            && !released.state.contact_latched,
        "native contact dispatch releases the latch for higher response kinds");
}

void test_native_frame_oracle()
{
    using devils_crush::native_runtime::FrameMismatch;
    using devils_crush::native_runtime::FrameMismatchField;
    using devils_crush::native_runtime::FrameObservation;
    using devils_crush::native_runtime::FrameSnapshot;
    using devils_crush::native_runtime::compare_frame_observations;

    std::array<std::byte, 3> work_ram{
        std::byte{0x01U}, std::byte{0x02U}, std::byte{0x03U}};
    std::array<std::uint32_t, 2> framebuffer{0xFF000000U, 0xFFFFFFFFU};
    std::array<float, 4> audio{0.0F, 0.25F, -0.25F, 0.0F};
    const FrameObservation reference{
        .frame_number = 7U,
        .work_ram = work_ram,
        .framebuffer = framebuffer,
        .framebuffer_width = 2,
        .framebuffer_height = 1,
        .framebuffer_pitch_pixels = 2,
        .audio = audio,
        .audio_sample_rate = 44100,
    };
    auto candidate_work_ram = work_ram;
    auto candidate_framebuffer = framebuffer;
    auto candidate_audio = audio;
    FrameObservation candidate{
        .frame_number = reference.frame_number,
        .work_ram = candidate_work_ram,
        .framebuffer = candidate_framebuffer,
        .framebuffer_width = reference.framebuffer_width,
        .framebuffer_height = reference.framebuffer_height,
        .framebuffer_pitch_pixels = reference.framebuffer_pitch_pixels,
        .audio = candidate_audio,
        .audio_sample_rate = reference.audio_sample_rate,
    };
    FrameMismatch mismatch{};
    check(
        compare_frame_observations(reference, candidate, mismatch) == nullptr,
        "native frame oracle accepts an exact frame");

    const FrameSnapshot snapshot(reference);
    work_ram[0] = std::byte{0xFFU};
    framebuffer[0] = 0U;
    audio[0] = 1.0F;
    check(
        compare_frame_observations(
            snapshot.observation(), candidate, mismatch) == nullptr,
        "native frame snapshot owns reference buffers beyond the source frame");
    work_ram[0] = std::byte{0x01U};
    framebuffer[0] = 0xFF000000U;
    audio[0] = 0.0F;

    candidate_work_ram[1] = std::byte{0x04U};
    check(
        compare_frame_observations(reference, candidate, mismatch) == &mismatch
            && mismatch.field == FrameMismatchField::work_ram
            && mismatch.index == 1U,
        "native frame oracle reports the first work-RAM difference");
    candidate_work_ram[1] = std::byte{0x02U};

    candidate.framebuffer_pitch_pixels = 3;
    check(
        compare_frame_observations(reference, candidate, mismatch) == &mismatch
            && mismatch.field == FrameMismatchField::framebuffer_shape,
        "native frame oracle rejects a framebuffer shape change");
    candidate.framebuffer_pitch_pixels = reference.framebuffer_pitch_pixels;

    candidate_audio[2] = 0.5F;
    check(
        compare_frame_observations(reference, candidate, mismatch) == &mismatch
            && mismatch.field == FrameMismatchField::audio
            && mismatch.index == 2U,
        "native frame oracle reports the first audio difference");
}

void test_emulator_oracle_adapter()
{
    using devils_crush::emulation::EmulatedFrame;
    using devils_crush::emulation::create_pce_runtime;
    using devils_crush::native_runtime::capture_emulator_frame;

    std::array<std::uint32_t, 2> framebuffer{0xFF000000U, 0xFFFFFFFFU};
    std::array<float, 2> audio{0.25F, -0.25F};
    const EmulatedFrame frame{
        .framebuffer = {
            .pixels = framebuffer,
            .width = 2,
            .height = 1,
            .pitch_pixels = 2,
        },
        .audio = {
            .interleaved_stereo = audio,
            .sample_rate = 44100,
        },
        .frame_number = 23U,
    };

    const auto runtime = create_pce_runtime();
    // `observation()` is deliberately a view into the owning FrameSnapshot.
    // Retain that owner while asserting the copied output, rather than
    // keeping spans into a temporary snapshot after it is destroyed.
    const auto owned_snapshot = capture_emulator_frame(*runtime, frame);
    const auto snapshot = owned_snapshot.observation();
    check(
        snapshot.frame_number == 23U && snapshot.work_ram.empty()
            && std::ranges::equal(snapshot.framebuffer, framebuffer)
            && snapshot.framebuffer_width == 2
            && snapshot.framebuffer_height == 1
            && snapshot.framebuffer_pitch_pixels == 2
            && std::ranges::equal(snapshot.audio, audio)
            && snapshot.audio_sample_rate == 44100,
        "emulator oracle adapter captures an emulated frame boundary");

    framebuffer[0] = 0U;
    audio[0] = 0.0F;
    check(
        snapshot.framebuffer[0] == 0xFF000000U && snapshot.audio[0] == 0.25F,
        "emulator oracle adapter snapshot remains valid after source reuse");
}

void test_runtime_boundary()
{
    const auto runtime = devils_crush::emulation::create_pce_runtime();
    const auto info = runtime->info();
    check(!info.name.empty(), "runtime has a diagnostic name");
    check(
        info.core_available == !info.upstream_revision.empty(),
        "runtime availability and revision agree");
    check(
        runtime->backup_ram_size() == 0U,
        "backup RAM is unavailable before ROM load");
    check(
        runtime->export_backup_ram().empty(),
        "backup RAM export is empty before ROM load");
    constexpr std::array invalid_backup{std::byte{0}};
    check(
        !runtime->import_backup_ram(invalid_backup),
        "backup RAM import is rejected before ROM load");
    check(
        runtime->work_ram_size() == 0U,
        "work RAM is unavailable before ROM load");
    check(
        runtime->export_work_ram().empty(),
        "work RAM export is empty before ROM load");
    check(
        !runtime->install_execution_hook({
            .routine = {
                .physical_bank = 0,
                .logical_pc = 0xE462,
            },
            .mode =
                devils_crush::emulation::HookMode::pass_through,
        }),
        "execution hook installation is rejected before ROM load");
    check(
        runtime->execution_hook_count() == 0U,
        "execution-hook registry is empty before ROM load");
    check(
        !runtime->remove_execution_hook({
            .physical_bank = 0,
            .logical_pc = 0xE462,
        }),
        "execution-hook removal is rejected before ROM load");
    runtime->clear_execution_hooks();
    check(
        !runtime->install_routine_call_observer({
            .routine = {
                .physical_bank = 0,
                .logical_pc = 0xE462,
            },
            .callback = [](const devils_crush::emulation::RoutineCall&) {},
        }),
        "routine-call observer installation is rejected before ROM load");
    check(
        runtime->routine_call_observer_count() == 0U,
        "routine-call observer registry is empty before ROM load");
    check(
        !runtime->remove_routine_call_observer({
            .physical_bank = 0,
            .logical_pc = 0xE462,
        }),
        "routine-call observer removal is rejected before ROM load");
    runtime->clear_routine_call_observers();
    check(
        !runtime->set_work_ram_write_observer(
            [](const devils_crush::emulation::ObservedMemoryWrite&) {}),
        "work-RAM write observer installation is rejected before ROM load");
    check(
        !runtime->has_work_ram_write_observer(),
        "work-RAM write observer is empty before ROM load");
    runtime->clear_work_ram_write_observer();
    check(
        !runtime->set_bus_write_observer(
            [](const devils_crush::emulation::ObservedBusWrite&) {}),
        "bus-write observer installation is rejected before ROM load");
    check(
        !runtime->has_bus_write_observer(),
        "bus-write observer is empty before ROM load");
    runtime->clear_bus_write_observer();
    check(
        !runtime->export_video_memory(),
        "video-memory export is empty before ROM load");
    check(
        runtime->serialize().core_state.empty(),
        "savestate serialization is empty before ROM load");
    check(
        !runtime->deserialize({}),
        "savestate restore is rejected before ROM load");

    bool second_runtime_rejected = false;
    try {
        const auto second =
            devils_crush::emulation::create_pce_runtime();
        (void)second;
        second_runtime_rejected = false;
    } catch (const std::logic_error&) {
        second_runtime_rejected = true;
    }
    check(
        second_runtime_rejected == info.core_available,
        "real runtime enforces the single-instance contract");
}

} // namespace

int main()
{
    test_hash_vectors();
    test_music_voice_overlay();
    test_native_position_integrator();
    test_native_motion_damping();
    test_native_motion_substep_split();
    test_native_signed_division();
    test_native_unsigned_byte_division();
    test_native_unsigned_24_division();
    test_native_unsigned_fixed_division();
    test_native_motion_rotation();
    test_native_collision_table_pass();
    test_native_runtime_collision_response();
    test_native_collision_scan_vertical_reject();
    test_native_collision_geometry();
    test_native_collision_scan_bounds_reject();
    test_native_collision_mask_lookup();
    test_native_collision_mask_coordinates();
    test_native_score_accumulator();
    test_native_rng_update();
    test_native_signed_word_negation();
    test_native_unsigned_byte_multiply();
    test_native_unsigned_multiply_round();
    test_native_signed_fixed_multiply();
    test_native_signed_word_multiply();
    test_native_flipper_projection();
    test_native_actor_camera_activation();
    test_native_actor_slot_reset();
    test_native_actor_slot_allocator();
    test_native_giant_skeleton_eyeball_gate();
    test_native_giant_skeleton_eyeball_initializer();
    test_native_giant_skeleton_eyeball_opening_wait();
    test_native_giant_skeleton_eyeball_steady_idle();
    test_native_giant_skeleton_eyeball_final_wait();
    test_native_gray_blob_initializer();
    test_native_gray_blob_first_live();
    test_native_gray_blob_live_wait();
    test_native_mercy_star_initializer();
    test_native_devil_shooter_initializer();
    test_native_little_skull_initializer();
    test_native_little_skull_pair_initializer();
    test_native_little_skull_hidden_initializer();
    test_native_dragon_head_initializer();
    test_native_giant_skeleton_initializer();
    test_native_giant_skeleton_controller();
    test_native_three_blobs_initializer();
    test_native_three_blobs_controller_initializer();
    test_native_walled_star_initializer();
    test_native_mercy_star_gate();
    test_native_three_blobs_controller();
    test_native_three_blobs_appearance_gate();
    test_native_three_blobs_first_movement_prefix();
    test_native_dragon_egg_controller();
    test_native_dragon_egg_controller_initializer();
    test_native_big_star_feature_gate();
    test_native_big_star_pending_gate();
    test_native_big_star_pending_helper();
    test_native_big_star_pending_right_helper();
    test_native_tower_insect_source_countdown_wait();
    test_native_big_star_initializer();
    test_native_dragon_egg_initializer();
    test_native_undead_head_initializer();
    test_native_undead_head_selector_gate();
    test_native_undead_head_waiting_parent();
    test_native_side_worm_initializers();
    test_native_side_worm_selector_gate();
    test_native_side_worm_waiting_parent();
    test_native_big_star_rotation_wait();
    test_native_devil_shooter_companion_dormant();
    test_native_devil_shooter_companion_position();
    test_native_devil_shooter_companion_appearance();
    test_native_devil_shooter_companion_active_timer();
    test_native_giant_face_request_gate();
    test_native_giant_face_animator_gate();
    test_native_giant_face_animator_active_wait();
    test_native_giant_face_crown_gate();
    test_native_giant_face_mouth_gate();
    test_native_giant_face_helmet_controller();
    test_native_giant_face_helmet_initializer();
    test_native_four_star_wall_pocket_controller();
    test_native_walled_star_access_controller();
    test_native_walled_star_access_initializer();
    test_native_side_star_initializers();
    test_native_side_star_access_controller();
    test_native_four_star_wall_pocket_animation();
    test_native_upper_wall_pocket_hand_initializer();
    test_native_upper_wall_pocket_hand_live_setup();
    test_native_upper_wall_pocket_hand_countdown();
    test_native_walled_star_hit_gate();
    test_native_tower_hit_gate();
    test_native_tower_bonus_terminal();
    test_native_tower_bonus_continuation();
    test_native_knight_hit_gate();
    test_native_knight_initializers();
    test_native_side_spider_initializer();
    test_native_front_spider_initializer();
    test_native_knight_waiting_parent();
    test_native_spider_appearance();
    test_native_tower_insect_animation();
    test_native_small_soldier_source_gate();
    test_native_tower_insect_source_gate();
    test_native_tower_insect_source_initializer();
    test_native_tower_initializer();
    test_native_bumper_initializers();
    test_native_bumper_child_initializer();
    test_native_small_soldier_source_initializers();
    test_native_small_soldier_delayed_spawn_gate();
    test_native_giant_face_controller_initializers();
    test_native_giant_face_animator_initializer();
    test_native_giant_face_crown_initializer();
    test_native_giant_face_mouth_initializer();
    test_native_sorcerer_color_gate();
    test_native_sorcerer_deployment_initializer();
    test_native_sorcerer_staging_initializer();
    test_native_dragon_head_hit_gate();
    test_native_bumper_hit_gate();
    test_native_devil_shooter_hit_gate();
    test_native_devil_shooter_release_delay_gate();
    test_native_devil_shooter_charge_entry_gate();
    test_native_devil_shooter_post_launch_gate();
    test_native_devil_shooter_charge_position();
    test_native_devil_shooter_charge_retraction();
    test_native_skeleton_insect_left_gate();
    test_native_skeleton_insect_right_gate();
    test_native_skeleton_insect_right_post_trigger();
    test_native_skeleton_insect_center_gate();
    test_native_devils_coffin_gate();
    test_native_devils_coffin_initializer();
    test_native_devils_coffin_opening();
    test_native_devils_coffin_closing();
    test_native_tower_guard_selector();
    test_native_fixed_response_gate();
    test_native_fixed_scheduler_gate();
    test_native_fixed_scheduler_initializer();
    test_native_fixed_marker_initializer();
    test_native_fixed_response_initializer();
    test_native_fixed_marker_gate();
    test_native_scaled_word_add();
    test_native_dragon_egg_hit_gate();
    test_native_dragon_egg_reset_gate();
    test_native_giant_door_hit_gate();
    test_native_giant_door_initializer();
    test_native_flipper_input_gate();
    test_native_nudge_input_gate();
    test_native_session_input_gate();
    test_native_ball_drain_threshold();
    test_native_bank7_mode_gate();
      test_native_flipper_release_arm();
      test_native_flipper_release_prefix();
      test_native_flipper_active_tail();
    test_native_flipper_phase_step();
    test_native_flipper_geometry();
    test_native_coroutine_schedule();
    test_native_cached_position_restore();
    test_supported_rom_metadata();
    test_scaler_examples();
    test_scaler_edges();
    test_cabinet_state();
    test_plunger();
    test_nudge();
    test_cabinet_display_config();
    test_pce_background_decode();
    test_background_stitch_and_crop();
    test_pce_sprite_overlay();
    test_save_state_container();
    test_routine_replay_corpus();
    test_backup_ram_storage();
    test_atomic_commit();
    test_legacy_data_migration();
    test_high_score_table();
    test_idle_skip();
    test_pce_pad();
    test_actor_scan_run();
    test_actor_dispatch_vector();
    test_actor_queue_registration();
    test_collision_scan_run();
    test_sprite_emit_run();
    test_text_tile_write();
    test_dmd_text_layout();
    test_dmd_attract_transition();
    test_dmd_name_entry();
    test_table_motion();
    test_table_palette_cache();
    test_native_registry();
    test_native_game_state_view();
    test_native_ball_frame_schedule();
    test_synthetic_bonus_stage_return();
    test_native_gameplay_audio_dispatch();
    test_native_small_soldier_capture();
    test_playfield_collision_capture_adapter();
    test_native_actor_scheduler_view();
    test_native_bonus_stage_4();
    test_native_bonus_stage_4_linked_target();
    test_native_actor_motion();
    test_native_actor_state();
    test_native_bumper_capture();
    test_native_front_spider_capture();
    test_native_side_spider_capture();
    test_native_skeleton_insect_capture();
    test_native_dragon_egg_cluster_capture();
    test_native_partial_dragon_egg_cluster_capture();
    test_native_dragon_egg_hatchling_capture();
    test_native_completed_dragon_egg_cluster_capture();
    test_native_dragon_egg_reset_cluster_capture();
    test_native_dragon_head_capture();
    test_native_giant_skeleton_capture();
    test_native_giant_face_capture();
    test_native_giant_skeleton_eyeball_capture();
    test_native_giant_skeleton_eyeball_final_animation_capture();
    test_native_tower_capture();
    test_native_tower_guard_capture();
    test_native_collision_effect();
    test_native_collision_reaction();
    test_native_collision_table();
    test_native_collision_table_asset();
    test_native_small_soldier();
    test_native_bumper();
    test_native_front_spider();
    test_native_side_spider();
    test_native_skeleton_insect();
    test_native_tower();
    test_native_tower_insect();
    test_native_tower_insect_capture();
    test_native_tower_guard();
    test_native_score_state();
    test_native_small_soldier_rule_scheduler();
    test_native_dragon_head();
    test_native_giant_face();
    test_native_dragon_egg_cluster();
    test_native_giant_skeleton_eyeball();
    test_native_giant_skeleton();
    test_native_gameplay_state();
    test_native_ball_motion();
    test_native_collision_retry();
    test_native_collision_response();
    test_native_collision_bounds();
    test_native_collision_mask();
    test_native_actor_collision_mask_coordinates();
    test_native_actor_collision_mask_asset();
    test_native_actor_collision_scan();
    test_native_collision_map();
    test_native_collision_map_asset();
    test_native_playfield_collision_scan();
    test_native_physics_state();
    test_native_prepared_gameplay_collision_loop();
    test_native_contact_dispatch();
    test_native_frame_oracle();
    test_emulator_oracle_adapter();
    test_runtime_boundary();

    if (failures == 0) {
        std::cout << "All core tests passed.\n";
        return EXIT_SUCCESS;
    }

    std::cerr << failures << " test(s) failed.\n";
    return EXIT_FAILURE;
}
