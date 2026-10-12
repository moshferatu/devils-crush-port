// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"

namespace devils_crush::app::host {

[[nodiscard]] InputFrame canonical_input_for_frame(const int frame_number);

[[nodiscard]] InputFrame gameplay_input_for_frame(const int frame_number);

[[nodiscard]] InputFrame gameplay_alternate_input_for_frame(
    const int frame_number);

[[nodiscard]] InputFrame gameplay_sweep_input_for_frame(
    const int frame_number);

[[nodiscard]] InputFrame gameplay_crossfire_input_for_frame(
    const int frame_number);

[[nodiscard]] InputFrame gameplay_orbit_input_for_frame(const int frame_number);

[[nodiscard]] InputFrame gameplay_lanes_input_for_frame(const int frame_number);

[[nodiscard]] InputFrame gameplay_stagger_input_for_frame(
    const int frame_number);

[[nodiscard]] InputFrame gameplay_pulse_input_for_frame(const int frame_number);

[[nodiscard]] InputFrame gameplay_rally_input_for_frame(const int frame_number);

[[nodiscard]] InputFrame gameplay_slalom_input_for_frame(
    const int frame_number);

[[nodiscard]] InputFrame gameplay_pendulum_input_for_frame(
    const int frame_number);

[[nodiscard]] InputFrame gameplay_nudge_input_for_frame(const int frame_number);

[[nodiscard]] InputFrame gameplay_switchback_input_for_frame(
    const int frame_number);

[[nodiscard]] InputFrame gameplay_spiral_input_for_frame(
    const int frame_number);

[[nodiscard]] InputFrame gameplay_ricochet_input_for_frame(
    const int frame_number);

[[nodiscard]] InputFrame gameplay_volley_input_for_frame(const int frame_number);

[[nodiscard]] InputFrame gameplay_whirlpool_input_for_frame(
    const int frame_number);

[[nodiscard]] InputFrame gameplay_cascade_input_for_frame(
    const int frame_number);

[[nodiscard]] InputFrame gameplay_higharc_input_for_frame(
    const int frame_number);

[[nodiscard]] InputFrame gameplay_twist_input_for_frame(const int frame_number);

[[nodiscard]] InputFrame gameplay_sidestep_input_for_frame(
    const int frame_number);

[[nodiscard]] InputFrame gameplay_weave_input_for_frame(const int frame_number);

[[nodiscard]] InputFrame gameplay_bankshot_input_for_frame(
    const int frame_number);

[[nodiscard]] InputFrame gameplay_playfield_input_for_frame(
    const int frame_number);

[[nodiscard]] InputFrame gameplay_corner_input_for_frame(const int frame_number);

[[nodiscard]] InputFrame gameplay_alley_input_for_frame(const int frame_number);

[[nodiscard]] InputFrame gameplay_rebound_input_for_frame(const int frame_number);

[[nodiscard]] InputFrame gameplay_skew_input_for_frame(const int frame_number);

[[nodiscard]] InputFrame gameplay_helix_input_for_frame(const int frame_number);

[[nodiscard]] InputFrame gameplay_vector_input_for_frame(const int frame_number);

[[nodiscard]] InputFrame gameplay_radial_input_for_frame(const int frame_number);

[[nodiscard]] InputFrame state_census_input_for_frame(const int frame_number);

} // namespace devils_crush::app::host
