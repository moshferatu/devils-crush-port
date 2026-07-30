// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/core/backup_ram.hpp"
#include "devils_crush/cabinet/cabinet_state.hpp"
#include "devils_crush/cabinet/display_config.hpp"
#include "devils_crush/core/rom_image.hpp"
#include "devils_crush/core/save_state.hpp"
#include "devils_crush/emulation/pce_runtime.hpp"
#include "devils_crush/game_logic/actor.h"
#include "devils_crush/game_logic/collision.h"
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
            && observed_stats->rendered_fixed_actors == 0U,
        "persistent actor cache learns from the live camera");

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
            && second_phase_stats->rendered_fixed_actors == 0U,
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
            && observed_effect_stats->rendered_table_world_effects == 0U,
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
    test_native_score_accumulator();
    test_native_rng_update();
    test_native_signed_word_negation();
    test_native_unsigned_multiply_round();
    test_native_signed_fixed_multiply();
    test_native_flipper_projection();
    test_native_actor_camera_activation();
    test_native_actor_slot_reset();
    test_native_flipper_input_gate();
    test_native_nudge_input_gate();
    test_native_session_input_gate();
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
    test_runtime_boundary();

    if (failures == 0) {
        std::cout << "All core tests passed.\n";
        return EXIT_SUCCESS;
    }

    std::cerr << failures << " test(s) failed.\n";
    return EXIT_FAILURE;
}
