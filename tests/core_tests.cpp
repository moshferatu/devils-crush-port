// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/app/native_registry.hpp"
#include "devils_crush/core/backup_ram.hpp"
#include "devils_crush/cabinet/cabinet_state.hpp"
#include "devils_crush/cabinet/display_config.hpp"
#include "devils_crush/cabinet/dmd_layout.hpp"
#include "devils_crush/core/rom_image.hpp"
#include "devils_crush/core/save_state.hpp"
#include "devils_crush/emulation/pce_runtime.hpp"
#include "devils_crush/game_logic/actor.h"
#include "devils_crush/game_logic/collision.h"
#include "devils_crush/game_logic/sprite.h"
#include "devils_crush/render/table_palette_cache.hpp"
#include "devils_crush/game_logic/controls.h"
#include "devils_crush/game_logic/physics.h"
#include "devils_crush/game_logic/rng.h"
#include "devils_crush/game_logic/score.h"
#include "devils_crush/render/pce_background.hpp"
#include "devils_crush/render/pce_sprite_overlay.hpp"
#include "devils_crush/render/sharp_pixel_scaler.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <string_view>
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

void test_native_collision_response()
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
    work_ram[0x0080U] = 0x05U;
    check(
        dc_prepare_upper_wall_pocket_hand_live_setup(
            work_ram.data(), work_ram.size(), 0x40U, 0x73U, &setup)
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
            work_ram.data(), work_ram.size(), 0x42U, 0x73U, &setup)
                == 0,
        "native 5975 rejects a non-hand actor slot");
    check(
        dc_prepare_upper_wall_pocket_hand_live_setup(
            work_ram.data(), 0x10C0U, 0x40U, 0x73U, &setup)
                == 0,
        "native 5975 rejects a truncated collision snapshot");
    check(
        dc_prepare_upper_wall_pocket_hand_live_setup(
            work_ram.data(), work_ram.size(), 0x40U, 0x73U, nullptr)
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
    check(
        dc_classify_tower_insect_animation(
            work_ram.data(), work_ram.size(), 0x65U, &animation)
            == 0,
        "native 6054 rejects another dynamic actor slot");
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

void test_native_devil_shooter_charge_position()
{
    std::array<std::uint8_t, 0x2000> work_ram{};
    dc_devil_shooter_charge_position position{};

    work_ram[0x128EU] = 0xC0U;
    work_ram[0x130EU] = 0x78U;
    work_ram[0x138EU] = 0x02U;
    work_ram[0x1E0EU] = 0x78U;
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
            == 0,
        "native 52E3 defers the coordinate-clamp branch");
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
            && retraction.vertical_middle == 0x8CU
            && retraction.vertical_high == 0x02U
            && retraction.charge == 0x14U && retraction.appearance == 2U
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
            == 0
            && dc_classify_devil_shooter_charge_retraction(
                   work_ram.data(), 0x1E8EU, 0x0EU, &retraction)
                == 0
            && dc_classify_devil_shooter_charge_retraction(
                   work_ram.data(), work_ram.size(), 0x0DU, &retraction)
                == 0,
        "native 533B defers the clamp path and rejects invalid snapshots");

    work_ram[0x1E0EU] = 0x8DU;
    work_ram[0x1E8EU] = 0x02U;
    check(
        dc_classify_devil_shooter_charge_retraction(
            work_ram.data(), work_ram.size(), 0x0EU, &retraction)
            == 0,
        "native 533B carries a charge-byte borrow into the clamp comparison");
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

    std::vector<std::byte> work_ram(8192U);
    const auto frontend = decode_cabinet_state(work_ram);
    check(frontend.has_value(), "zeroed cabinet state decodes");
    check(
        frontend
            && frontend->playfield
                == PlayfieldView::emulated_framebuffer
            && frontend->displayed_score == 0U
            && frontend->balls_in_session == 0U
            && !frontend->bonus_stage,
        "frontend state keeps the emulated playfield");

    work_ram[0x0251U] = std::byte{1};
    work_ram[0x0206U] = std::byte{0xFC};
    work_ram[0x020DU] = std::byte{0x03};
    work_ram[0x0214U] = std::byte{0};
    work_ram[0x0858U] = std::byte{1};
    work_ram[0x0D52U] = std::byte{3};
    const auto normal_table = decode_cabinet_state(work_ram);
    check(
        normal_table
            && normal_table->playfield
                == PlayfieldView::table_background
            && normal_table->stored_score == 1020U
            && normal_table->displayed_score == 102000U
            && normal_table->score_slot == 1U
            && normal_table->balls_in_session == 3U,
        "normal session selects the table and active-player score");

    work_ram[0x0053U] = std::byte{0x10};
    const auto bonus = decode_cabinet_state(work_ram);
    check(
        bonus
            && bonus->playfield
                == PlayfieldView::emulated_framebuffer
            && bonus->bonus_stage,
        "bonus session restores the emulated playfield");

    work_ram[0x0053U] = std::byte{0};
    work_ram[0x0D52U] = std::byte{0};
    const auto no_balls = decode_cabinet_state(work_ram);
    check(
        no_balls
            && no_balls->playfield
                == PlayfieldView::emulated_framebuffer,
        "session end restores the emulated playfield");

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
        .backglass_path = "cabinet art/backglass=final%2.png",
    };
    const auto encoded = encode_display_config(source);
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
        check(
            defaults[PadButton::one]
                == std::vector<std::string>{"Right Shift", "Right", "Z"},
            "Right Shift and Right work the right flipper by default");
        check(
            defaults[PadButton::right].empty(),
            "d-pad right has no default key now that Right is a flipper");

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

    // The nonzero gate selects an unmodelled test and must be refused.
    check(
        dc_classify_collision_scan_run(
            work_ram.data(), work_ram.size(), 4U, 0x7CU, 1U, 0x80U, &run) == 0,
        "collision scan refuses a nonzero gate");
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

void test_dmd_digit_layout()
{
    using devils_crush::cabinet::calculate_dmd_digit_layout;
    using devils_crush::cabinet::kDmdScoreFieldDigits;
    using devils_crush::cabinet::kDmdWidthUsage;

    // Every shape a DMD role can take: the default window, a dedicated DMD
    // strip, a fullscreen 16:9 panel, a fullscreen 16:10 panel, and a panel
    // rotated into portrait. The last three are what the score used to run
    // off the edge of.
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
        for (std::size_t digits = 1U; digits <= 12U; ++digits) {
            const auto layout = calculate_dmd_digit_layout(
                output.width, output.height, digits);
            check(layout.valid(), "the DMD layout is drawable");

            // Digits are laid left to right from the origin, so the field
            // runs from origin_x to the right edge of the last digit.
            const auto drawn_width =
                static_cast<float>(digits - 1U) * layout.pitch()
                + layout.digit_width;
            check(
                layout.origin_x >= 0.0F
                    && layout.origin_x + drawn_width
                        <= static_cast<float>(output.width) + 0.5F,
                "the score fits horizontally");
            check(
                layout.origin_y >= 0.0F
                    && layout.origin_y + layout.digit_height
                        <= static_cast<float>(output.height) + 0.5F,
                "the score fits vertically");
        }
    }

    // Digit size must not change as the score grows: a scoreboard that
    // rescales itself on every carry reads as a fault.
    const auto one_digit = calculate_dmd_digit_layout(1920, 360, 1U);
    const auto full_field =
        calculate_dmd_digit_layout(1920, 360, kDmdScoreFieldDigits);
    check(
        one_digit.digit_height == full_field.digit_height,
        "a short score is drawn at the same size as a full one");

    // A score wider than the nominal field still has to fit rather than
    // overflow, so the field grows instead.
    const auto over_field =
        calculate_dmd_digit_layout(1920, 360, kDmdScoreFieldDigits + 2U);
    check(
        over_field.digit_height < full_field.digit_height,
        "a score wider than the field shrinks to fit it");

    // On a strip wide enough for the height bound to bind, the field must
    // still be the one that binds -- this is the direction the old formula
    // got wrong by applying the digit aspect a second time.
    const auto strip = calculate_dmd_digit_layout(1920, 360, 10U);
    const auto strip_width =
        static_cast<float>(kDmdScoreFieldDigits - 1U) * strip.pitch()
        + strip.digit_width;
    check(
        strip_width
            <= static_cast<float>(1920) * kDmdWidthUsage + 0.5F,
        "the full field stays inside its width budget");

    check(
        !calculate_dmd_digit_layout(0, 240, 4U).valid()
            && !calculate_dmd_digit_layout(960, 0, 4U).valid()
            && !calculate_dmd_digit_layout(960, 240, 0U).valid(),
        "a degenerate DMD output yields no layout");
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
    test_native_position_integrator();
    test_native_motion_damping();
    test_native_signed_division();
    test_native_motion_rotation();
    test_native_collision_table_pass();
    test_native_collision_response();
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
    test_native_flipper_projection();
    test_native_actor_camera_activation();
    test_native_actor_slot_reset();
    test_native_giant_skeleton_eyeball_gate();
    test_native_giant_skeleton_eyeball_initializer();
    test_native_giant_skeleton_eyeball_opening_wait();
    test_native_giant_skeleton_eyeball_steady_idle();
    test_native_giant_skeleton_eyeball_final_wait();
    test_native_gray_blob_initializer();
    test_native_gray_blob_first_live();
    test_native_mercy_star_initializer();
    test_native_devil_shooter_initializer();
    test_native_little_skull_initializer();
    test_native_little_skull_pair_initializer();
    test_native_little_skull_hidden_initializer();
    test_native_dragon_head_initializer();
    test_native_giant_skeleton_initializer();
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
    test_native_devil_shooter_charge_position();
    test_native_devil_shooter_charge_retraction();
    test_native_skeleton_insect_left_gate();
    test_native_skeleton_insect_right_gate();
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
    test_native_giant_door_hit_gate();
    test_native_giant_door_initializer();
    test_native_flipper_input_gate();
    test_native_nudge_input_gate();
    test_native_session_input_gate();
    test_native_ball_drain_threshold();
    test_native_flipper_phase_step();
    test_native_flipper_geometry();
    test_native_coroutine_schedule();
    test_native_cached_position_restore();
    test_supported_rom_metadata();
    test_scaler_examples();
    test_scaler_edges();
    test_cabinet_state();
    test_cabinet_display_config();
    test_pce_background_decode();
    test_background_stitch_and_crop();
    test_pce_sprite_overlay();
    test_save_state_container();
    test_backup_ram_storage();
    test_actor_scan_run();
    test_collision_scan_run();
    test_sprite_emit_run();
    test_dmd_digit_layout();
    test_table_palette_cache();
    test_native_registry();
    test_runtime_boundary();

    if (failures == 0) {
        std::cout << "All core tests passed.\n";
        return EXIT_SUCCESS;
    }

    std::cerr << failures << " test(s) failed.\n";
    return EXIT_FAILURE;
}
