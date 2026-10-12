// SPDX-License-Identifier: GPL-3.0-or-later
#include "gameplay_bootstrap_shadow.hpp"

namespace devils_crush::app::host {

// The host-frame shadow above deliberately creates a fresh candidate at
// every presentation boundary. DC95 is the narrower, recovered physics
// boundary, so retain a separate candidate here and compare its completed
// native state at the next live DC95 entry. A collision handoff resets the
// chain because CF33/CDF8 still belong to the source resolver.
void NativeGameplayBootstrapShadow::observe_native_epoch_candidate_entry(
    const devils_crush::native_runtime::GameStateView& state,
    const devils_crush::native_runtime::BallState observed_ball) noexcept
{
    if (native_epoch_runtime) {
        const auto* const gameplay = native_epoch_runtime->runtime.gameplay();
        if (gameplay == nullptr) {
            ++native_epoch_runtime_unavailable;
            native_epoch_runtime.reset();
        } else {
            ++observed_native_epoch_runtime_boundaries;
            if (gameplay->physics().ball == observed_ball) {
                ++matched_native_epoch_runtime_boundaries;
                record_native_epoch_actor_handler_evidence(
                    *native_epoch_runtime, true);
            } else {
                ++mismatched_native_epoch_runtime_boundaries;
                record_native_epoch_actor_handler_evidence(
                    *native_epoch_runtime, false);
                native_epoch_runtime.reset();
            }
        }
    }

    const auto tuning = state.ball_motion_tuning();
    if (!tuning || playfield_maps.empty()) {
        ++native_epoch_runtime_unavailable;
        native_epoch_runtime.reset();
        return;
    }

    try {
        if (!native_epoch_runtime) {
            const auto bootstrap = devils_crush::native_runtime::
                capture_native_gameplay_bootstrap(state);
            if (!bootstrap) {
                ++native_epoch_runtime_unavailable;
                return;
            }
            native_epoch_runtime.emplace();
            if (!native_epoch_runtime->runtime.bootstrap(*bootstrap)) {
                ++native_epoch_runtime_unavailable;
                native_epoch_runtime.reset();
                return;
            }
            ++native_epoch_runtime_seeds;
        }

        const auto frame = native_epoch_runtime->runtime.advance_physics_epoch(
            *tuning, playfield_maps.back().map.view());
        if (!frame) {
            ++native_epoch_runtime_unavailable;
            native_epoch_runtime.reset();
            return;
        }
        ++native_epoch_runtime_candidates;
        if (frame->requires_collision_resolution()) {
            ++native_epoch_runtime_contact_candidates;
            native_epoch_runtime.reset();
        } else {
            ++native_epoch_runtime_clear_candidates;
        }
    } catch (...) {
        // This chain is diagnostic only. Never let an allocation or
        // decoding failure alter live execution at a registry hook.
        ++native_epoch_runtime_unavailable;
        native_epoch_runtime.reset();
    }
}

void NativeGameplayBootstrapShadow::observe_native_epoch_actor_handler_dispatch(
    const devils_crush::native_runtime::GameStateView& state,
    const std::uint8_t slot) noexcept
{
    if (!native_epoch_runtime) {
        return;
    }
    const auto actor = state.valid() ? state.actor(slot) : std::nullopt;
    if (!actor) {
        ++native_epoch_runtime->unclassified_source_actor_handler_entries;
        return;
    }
    for (auto& handler : native_epoch_runtime->source_actor_handlers) {
        if (handler.entries != 0U
            && handler.handler_address == actor->handler_address) {
            ++handler.entries;
            return;
        }
    }
    for (auto& handler : native_epoch_runtime->source_actor_handlers) {
        if (handler.entries == 0U) {
            handler = {
                .handler_address = actor->handler_address,
                .entries = 1U,
            };
            return;
        }
    }
    ++native_epoch_runtime->unclassified_source_actor_handler_entries;
}

void NativeGameplayBootstrapShadow::record_native_epoch_actor_handler_evidence(
    const NativeEpochRuntimeChain& chain,
    const bool matches) noexcept
{
    for (const auto& source_handler : chain.source_actor_handlers) {
        if (source_handler.entries == 0U) {
            continue;
        }
        bool recorded_handler = false;
        for (auto& recorded : native_epoch_actor_handler_evidence) {
            if (recorded.matching_entries + recorded.mismatched_entries != 0U
                && recorded.handler_address == source_handler.handler_address) {
                if (matches) {
                    recorded.matching_entries += source_handler.entries;
                } else {
                    recorded.mismatched_entries += source_handler.entries;
                }
                recorded_handler = true;
                break;
            }
        }
        if (!recorded_handler) {
            for (auto& recorded : native_epoch_actor_handler_evidence) {
                if (recorded.matching_entries + recorded.mismatched_entries == 0U) {
                    recorded.handler_address = source_handler.handler_address;
                    if (matches) {
                        recorded.matching_entries = source_handler.entries;
                    } else {
                        recorded.mismatched_entries = source_handler.entries;
                    }
                    recorded_handler = true;
                    break;
                }
            }
        }
        if (!recorded_handler) {
            unclassified_native_epoch_actor_handler_entries
                += source_handler.entries;
        }
    }
    unclassified_native_epoch_actor_handler_entries
        += chain.unclassified_source_actor_handler_entries;
}

void NativeGameplayBootstrapShadow::record_deferred_frame_schedule(
    const devils_crush::native_runtime::BallFrameOwner owner) noexcept
{
    for (auto& recorded : deferred_frame_schedule_owners) {
        if (recorded.mismatches != 0U && recorded.owner == owner) {
            ++recorded.mismatches;
            return;
        }
    }
    for (auto& recorded : deferred_frame_schedule_owners) {
        if (recorded.mismatches == 0U) {
            recorded = {
                .owner = owner,
                .mismatches = 1U,
            };
            return;
        }
    }
    ++unclassified_deferred_frame_schedule_owners;
}

void NativeGameplayBootstrapShadow::record_prediction_schedule_owner(
    const devils_crush::native_runtime::BallFrameOwner owner) noexcept
{
    for (auto& recorded : prediction_schedule_owners) {
        if (recorded.mismatches != 0U && recorded.owner == owner) {
            ++recorded.mismatches;
            return;
        }
    }
    for (auto& recorded : prediction_schedule_owners) {
        if (recorded.mismatches == 0U) {
            recorded = {
                .owner = owner,
                .mismatches = 1U,
            };
            return;
        }
    }
    ++unclassified_prediction_schedule_owners;
}

void NativeGameplayBootstrapShadow::record_unrecognized_schedule_handler(
    const std::uint16_t handler_address) noexcept
{
    for (auto& recorded : unrecognized_schedule_handlers) {
        if (recorded.frames != 0U
            && recorded.handler_address == handler_address) {
            ++recorded.frames;
            return;
        }
    }
    for (auto& recorded : unrecognized_schedule_handlers) {
        if (recorded.frames == 0U) {
            recorded = {
                .handler_address = handler_address,
                .frames = 1U,
            };
            return;
        }
    }
    ++unclassified_schedule_handlers;
}

void NativeGameplayBootstrapShadow::observe_pending_native_frame_prediction(
    const std::span<const std::byte> work_ram,
    const InputFrame& input) noexcept
{
    if (!pending_native_runtime_frame_prediction) {
        return;
    }
    const auto expected = *pending_native_runtime_frame_prediction;
    pending_native_runtime_frame_prediction.reset();
    const devils_crush::native_runtime::GameStateView state{work_ram};
    const auto ball = state.valid()
        ? devils_crush::native_runtime::capture_active_ball(state)
        : std::nullopt;
    if (!ball) {
        ++unavailable_native_runtime_frame_predictions;
        return;
    }
    const auto actual_schedule =
        devils_crush::native_runtime::capture_ball_frame_schedule(state);
    ++observed_native_runtime_frame_predictions;
    if (actual_schedule.owner == expected.schedule.owner) {
        ++matched_native_runtime_frame_prediction_schedule_owners;
    } else {
        ++mismatched_native_runtime_frame_prediction_schedule_owners;
        record_prediction_schedule_owner(actual_schedule.owner);
    }
    if (ball->state == expected.ball) {
        ++matched_native_runtime_frame_predictions;
    } else {
        ++mismatched_native_runtime_frame_predictions;
        if (input.player_one == 0U) {
            ++native_runtime_frame_prediction_input_neutral_mismatches;
        } else {
            ++native_runtime_frame_prediction_input_active_mismatches;
        }
        if (actual_frame_collision_facts.motion_substep_split_entries != 0U) {
            ++native_runtime_frame_prediction_split_mismatches;
            if (actual_frame_collision_facts.motion_substep_split_entries == 1U) {
                ++native_runtime_frame_prediction_single_split_mismatches;
            } else {
                ++native_runtime_frame_prediction_multiple_split_mismatches;
            }
        } else {
            ++native_runtime_frame_prediction_no_split_mismatches;
        }
        if (actual_frame_collision_facts.position_integration_entries != 0U) {
            ++native_runtime_frame_prediction_position_mismatches;
        }
        if (actual_frame_collision_facts.motion_damping_entries != 0U) {
            ++native_runtime_frame_prediction_damping_mismatches;
            if (actual_frame_collision_facts.motion_damping_entries == 1U) {
                ++native_runtime_frame_prediction_single_damping_mismatches;
            } else {
                ++native_runtime_frame_prediction_multiple_damping_mismatches;
            }
        } else {
            ++native_runtime_frame_prediction_no_damping_mismatches;
        }
        if (actual_frame_collision_facts.actor_contact
            && actual_frame_collision_facts.solid_playfield_contact) {
            ++native_runtime_frame_prediction_both_contact_mismatches;
        } else if (actual_frame_collision_facts.actor_contact) {
            ++native_runtime_frame_prediction_actor_contact_mismatches;
        } else if (actual_frame_collision_facts.solid_playfield_contact) {
            ++native_runtime_frame_prediction_playfield_contact_mismatches;
        } else {
            ++native_runtime_frame_prediction_no_contact_mismatches;
        }
        if (ball->state.position_x != expected.ball.position_x) {
            ++native_runtime_frame_prediction_position_x_mismatches;
        }
        if (ball->state.position_y != expected.ball.position_y) {
            ++native_runtime_frame_prediction_position_y_mismatches;
        }
        if (ball->state.velocity_x != expected.ball.velocity_x) {
            ++native_runtime_frame_prediction_velocity_x_mismatches;
        }
        if (ball->state.velocity_y != expected.ball.velocity_y) {
            ++native_runtime_frame_prediction_velocity_y_mismatches;
        }
        if (ball->state.motion_flags != expected.ball.motion_flags) {
            ++native_runtime_frame_prediction_motion_flag_mismatches;
        }
        if (ball->state.collision_state != expected.ball.collision_state) {
            ++native_runtime_frame_prediction_collision_state_mismatches;
        }
    }
}

} // namespace devils_crush::app::host
