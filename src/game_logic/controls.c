// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/game_logic/controls.h"

#include <stddef.h>
#include <stdint.h>

int dc_evaluate_flipper_input_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_flipper_input_gate* const gate)
{
    const size_t mask_offset = 0x1B80U + (size_t)slot;
    if (work_ram == NULL || gate == NULL || work_ram_size < 0x2000U
        || mask_offset >= work_ram_size) {
        return 0;
    }

    gate->global_input = work_ram[0x0CEEU];
    gate->piece_mask = work_ram[mask_offset];
    gate->matched_input =
        (uint8_t)(gate->global_input & gate->piece_mask);
    return 1;
}

int dc_prepare_flipper_activation(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    const dc_flipper_input_gate* const gate,
    dc_flipper_activation* const activation)
{
    const size_t countdown_offset = 0x1D80U + (size_t)slot;
    if (work_ram == NULL || gate == NULL || activation == NULL
        || work_ram_size < 0x2000U
        || countdown_offset >= work_ram_size) {
        return 0;
    }

    activation->current_slot = work_ram[0x00BDU];
    activation->coroutine_countdown = work_ram[countdown_offset];
    activation->dispatch_low = 0x8EU;
    activation->dispatch_high = 0xD4U;
    activation->compare_carry =
        (uint8_t)(slot >= activation->current_slot);
    activation->supported = (uint8_t)(
        gate->matched_input != 0U
        && slot != activation->current_slot
        && activation->coroutine_countdown == 0U);
    return 1;
}

int dc_prepare_flipper_release_arm(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_flipper_release_arm* const release)
{
    if (work_ram == NULL || release == NULL || work_ram_size < 0x2000U) {
        return 0;
    }

    release->orientation_before = work_ram[0x1C80U + (size_t)slot];
    release->orientation_after =
        (uint8_t)(release->orientation_before | 0x80U);
    release->coroutine_countdown = work_ram[0x1D80U + (size_t)slot];
    release->decrements_countdown =
        (uint8_t)(release->coroutine_countdown != 0U);
    release->coroutine_countdown_after =
        release->decrements_countdown != 0U
        ? (uint8_t)(release->coroutine_countdown - 1U)
        : 0U;
    release->installs_dispatch = (uint8_t)(
        release->coroutine_countdown == 0U
        || release->coroutine_countdown_after == 0U);
    release->dispatch_low = 0xC6U;
    release->dispatch_high = 0xD4U;
    return 1;
}

int dc_advance_flipper_phase(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_flipper_phase_step* const step)
{
    if (work_ram == NULL || step == NULL || work_ram_size < 0x2000U) {
        return 0;
    }

    step->countdown = work_ram[0x1A00U + (size_t)slot];
    step->reload = work_ram[0x1E00U + (size_t)slot];
    step->orientation = work_ram[0x1C80U + (size_t)slot];
    step->phase = work_ram[0x1980U + (size_t)slot];
    step->decrements_phase =
        (uint8_t)((step->orientation & 0x80U) != 0U);
    step->compare_carry = (uint8_t)(step->phase >= 3U);
    step->phase_next = step->decrements_phase != 0U
        ? (uint8_t)(step->phase - 1U)
        : (uint8_t)(step->phase + 1U);
    step->supported = (uint8_t)(
        step->countdown == 1U
        && ((step->decrements_phase != 0U && step->phase != 0U)
            || (step->decrements_phase == 0U && step->phase != 3U)));
    return 1;
}

int dc_lookup_flipper_geometry(
    const uint8_t state,
    const uint8_t* const geometry_table,
    const size_t geometry_table_size,
    dc_flipper_geometry* const geometry)
{
    const uint8_t table_index =
        (uint8_t)((uint8_t)(state & 0x1FU) << 1U);
    const size_t second_index = 0x40U + (size_t)table_index;
    if (geometry_table == NULL || geometry == NULL
        || geometry_table_size < 0x80U
        || second_index + 1U >= geometry_table_size) {
        return 0;
    }

    geometry->state = state;
    geometry->table_index = table_index;
    geometry->first_low = geometry_table[table_index];
    geometry->first_high = geometry_table[(size_t)table_index + 1U];
    geometry->second_low = geometry_table[second_index];
    geometry->second_high = geometry_table[second_index + 1U];
    return 1;
}

int dc_schedule_coroutine(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    const uint8_t stack_pointer,
    dc_coroutine_schedule* const schedule)
{
    const size_t countdown_offset = 0x1D80U + (size_t)slot;
    const size_t return_low_offset =
        0x0100U | (size_t)(uint8_t)(stack_pointer + 1U);
    const size_t return_high_offset =
        0x0100U | (size_t)(uint8_t)(stack_pointer + 2U);
    if (work_ram == NULL || schedule == NULL
        || work_ram_size < 0x2000U
        || countdown_offset >= work_ram_size
        || return_low_offset >= work_ram_size
        || return_high_offset >= work_ram_size) {
        return 0;
    }

    schedule->stacked_return_low = work_ram[return_low_offset];
    schedule->stacked_return_high = work_ram[return_high_offset];
    schedule->countdown = work_ram[countdown_offset];
    schedule->decrements_countdown =
        (uint8_t)(schedule->countdown != 0U);
    schedule->countdown_after =
        schedule->decrements_countdown != 0U
        ? (uint8_t)(schedule->countdown - 1U)
        : 0U;
    schedule->installs_dispatch = (uint8_t)(
        schedule->countdown == 0U
        || schedule->countdown_after == 0U);
    schedule->dispatch_low =
        (uint8_t)(schedule->stacked_return_low + 1U);
    schedule->dispatch_low_wrapped =
        (uint8_t)(schedule->dispatch_low == 0U);
    schedule->dispatch_high = (uint8_t)(
        schedule->stacked_return_high
        + schedule->dispatch_low_wrapped);
    return 1;
}

int dc_evaluate_nudge_input_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    dc_nudge_input_gate* const gate)
{
    if (work_ram == NULL || gate == NULL || work_ram_size < 0x2000U) {
        return 0;
    }

    gate->session_flags = work_ram[0x0053U];
    gate->session_mask =
        (uint8_t)(gate->session_flags & 0x28U);
    gate->tilt_lock = work_ram[0x0CC7U];
    gate->timer = work_ram[0x0CC6U];
    gate->timer_after = (uint8_t)(gate->timer - 1U);
    gate->nudge_phase = work_ram[0x0CC4U];
    gate->active_player = work_ram[0x0251U];
    gate->button_input =
        work_ram[0x0018U + (size_t)gate->active_player];
    gate->matched_input =
        (uint8_t)(gate->button_input & 0x02U);
    gate->branch = DC_NUDGE_GATE_DEFER;

    if ((gate->session_flags & 0x02U) != 0U
        || gate->tilt_lock != 0U) {
        return 1;
    }
    if (gate->session_mask != 0U) {
        gate->branch = DC_NUDGE_GATE_SESSION_RETURN;
        return 1;
    }
    if (gate->timer_after == 0U) {
        if ((uint8_t)(gate->nudge_phase - 1U) != 0U
            && gate->matched_input == 0U) {
            gate->branch = DC_NUDGE_GATE_TIMER_RELOAD_RETURN;
        }
        return 1;
    }
    if ((uint8_t)(gate->nudge_phase - 1U) == 0U) {
        if (work_ram[0x1000U] != 0U
            && (work_ram[0x1C80U] & 0x01U) == 0U) {
            gate->branch = DC_NUDGE_GATE_ACTIVE_FINISH;
        }
        return 1;
    }
    if (gate->matched_input != 0U) {
        const uint8_t counter_after =
            (uint8_t)(work_ram[0x0CC5U] + 1U);
        if (work_ram[0x0055U] == 0U
            && counter_after < 0x14U
            && work_ram[0x1000U] != 0U
            && (work_ram[0x1C80U] & 0x01U) == 0U) {
            gate->branch = DC_NUDGE_GATE_ACTIVE_START;
        }
        return 1;
    }

    gate->branch = DC_NUDGE_GATE_IDLE_RETURN;
    return 1;
}

int dc_evaluate_session_input_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    dc_session_input_gate* const gate)
{
    if (work_ram == NULL || gate == NULL || work_ram_size < 0x2000U) {
        return 0;
    }

    gate->session_flags = work_ram[0x0053U];
    gate->active_player = work_ram[0x0251U];
    gate->button_input =
        work_ram[0x0018U + (size_t)gate->active_player];
    gate->run_input = (uint8_t)(gate->button_input & 0x08U);
    gate->returns_idle = (uint8_t)(
        (gate->session_flags & 0x02U) == 0U
        && gate->run_input == 0U);
    return 1;
}

int dc_classify_ball_drain_threshold(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_ball_drain_threshold* const threshold)
{
    const size_t low_offset = 0x1300U + (size_t)slot;
    const size_t high_offset = 0x1380U + (size_t)slot;
    if (work_ram == NULL || threshold == NULL || work_ram_size < 0x2000U
        || low_offset >= work_ram_size || high_offset >= work_ram_size) {
        return 0;
    }

    threshold->slot = slot;
    threshold->session_state = work_ram[0x0055U];
    threshold->position_low = work_ram[low_offset];
    threshold->position_high = work_ram[high_offset];
    threshold->difference_low =
        (uint8_t)(threshold->position_low - 0xD0U);
    threshold->difference_high =
        (uint8_t)(threshold->position_high - 0x02U
            - (threshold->position_low < 0xD0U ? 1U : 0U));
    threshold->path = DC_BALL_DRAIN_THRESHOLD_DEFER;
    if (threshold->session_state != 0U) {
        return 1;
    }

    threshold->path =
        (((uint16_t)threshold->position_high << 8U)
            | threshold->position_low) >= 0x02D0U
        ? DC_BALL_DRAIN_THRESHOLD_DRAINED
        : DC_BALL_DRAIN_THRESHOLD_NORMAL;
    return 1;
}

int dc_classify_bank7_mode_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    dc_bank7_mode_gate* const gate)
{
    if (work_ram == NULL || gate == NULL || work_ram_size <= 0x0D51U) {
        return 0;
    }

    gate->mode = work_ram[0x0D51U];
    gate->range_value = work_ram[0x0D4FU];
    gate->persisted_form = work_ram[0x0258U];
    gate->latch_before = work_ram[0x00CBU];
    gate->latch_after = (uint8_t)(gate->latch_before & 0xFEU);
    gate->session_flags_before = work_ram[0x0053U];
    gate->session_flags_after = gate->session_flags_before;
    gate->counter_after = work_ram[0x00CAU];

    if (gate->mode != 9U) {
        gate->path = DC_BANK7_MODE_GATE_INACTIVE_MODE;
    } else if (gate->range_value < 0x3CU) {
        gate->path = DC_BANK7_MODE_GATE_BELOW_RANGE;
    } else if (gate->range_value >= 0x46U) {
        gate->path = DC_BANK7_MODE_GATE_ABOVE_RANGE;
    } else if (gate->persisted_form != 6U) {
        gate->path = DC_BANK7_MODE_GATE_FORM_MISMATCH;
    } else if ((gate->latch_before & 0x01U) != 0U) {
        gate->path = DC_BANK7_MODE_GATE_ALREADY_ARMED;
        gate->latch_after = gate->latch_before;
    } else {
        gate->path = DC_BANK7_MODE_GATE_ARM;
        gate->counter_after = 0x64U;
        gate->session_flags_after =
            (uint8_t)(gate->session_flags_before | 0x04U);
        gate->latch_after = (uint8_t)(gate->latch_before | 0x01U);
    }
    return 1;
}
