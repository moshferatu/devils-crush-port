// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/game_logic/actor.h"

int dc_classify_actor_scan_run(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    const uint8_t maximum_skips,
    dc_actor_scan_run* const run)
{
    uint8_t current;
    uint8_t skipped;
    uint8_t accumulator;

    if (work_ram == NULL || run == NULL || work_ram_size < 0x2000U
        || slot >= DC_ACTOR_SCAN_SLOT_COUNT
        || maximum_skips > DC_ACTOR_SCAN_SLOT_COUNT) {
        return 0;
    }

    current = slot;
    skipped = 0U;
    accumulator = 0U;
    for (;;) {
        uint8_t flags;

        /* The budget is spent at the loop head, before the next slot is
           read, so the run resumes exactly where the assembly would. */
        if (skipped >= maximum_skips) {
            run->exit = DC_ACTOR_SCAN_BUDGET;
            break;
        }

        /* LDA $3000,X : BPL to the increment when the slot is idle. */
        flags = work_ram[0x1000U + (size_t)current];

        accumulator = flags;
        if ((flags & 0x80U) != 0U) {
            /* LDA $3080,X : BMI straight to the dispatch branch. */
            const uint8_t attributes =
                work_ram[0x1080U + (size_t)current];

            accumulator = attributes;
            if ((attributes & 0x80U) != 0U) {
                run->exit = DC_ACTOR_SCAN_DISPATCH_NEGATIVE;
                break;
            }
            /* AND #$02 : BNE back to the increment when bit 1 survives. */
            accumulator = (uint8_t)(attributes & 0x02U);
            if (accumulator == 0U) {
                run->exit = DC_ACTOR_SCAN_DISPATCH_MASKED;
                break;
            }
            run->skips[skipped] = (uint8_t)DC_ACTOR_SCAN_SKIP_MASKED;
        } else {
            run->skips[skipped] = (uint8_t)DC_ACTOR_SCAN_SKIP_INACTIVE;
        }

        /* INC $4D : LDX $4D : CPX #$80 : BNE back to the loop head. */
        ++skipped;
        ++current;
        if (current >= DC_ACTOR_SCAN_SLOT_COUNT) {
            run->exit = DC_ACTOR_SCAN_COMPLETE;
            break;
        }
    }

    run->skipped = skipped;
    run->slot = current;
    run->accumulator = accumulator;
    return 1;
}

int dc_classify_actor_dispatch_vector(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_actor_dispatch_vector* const vector)
{
    if (work_ram == NULL || vector == NULL || work_ram_size < 0x2000U) {
        return 0;
    }

    /* LDA $3F00,X : STA $2F : LDA $3F80,X : STA $30 : JMP ($202F). */
    vector->slot = slot;
    vector->vector_low = work_ram[0x1F00U + (size_t)slot];
    vector->vector_high = work_ram[0x1F80U + (size_t)slot];
    vector->handler = (uint16_t)(
        (uint16_t)vector->vector_low
        | ((uint16_t)vector->vector_high << 8));
    return 1;
}

int dc_classify_actor_queue_registration(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t entry_slot,
    dc_actor_queue_registration* const registration)
{
    uint8_t attributes;
    uint8_t slot;

    if (work_ram == NULL || registration == NULL
        || work_ram_size < 0x2000U) {
        return 0;
    }

    registration->entry_slot = entry_slot;
    registration->slot = entry_slot;
    registration->entry_attributes = 0U;
    registration->attributes = 0U;
    registration->clear_state = 0U;
    registration->cleared_state = 0U;
    registration->cursor = 0U;
    registration->updated_cursor = 0U;
    registration->queue_index = 0U;

    /* CPX #$00 : BEQ to the RTS. Slot zero is the loop's own sentinel. */
    if (entry_slot == 0U) {
        registration->exit = DC_ACTOR_QUEUE_IDLE_SLOT;
        return 1;
    }

    /* LDA $3080,X : BPL to the RTS. */
    registration->entry_attributes =
        work_ram[0x1080U + (size_t)entry_slot];
    if ((registration->entry_attributes & 0x80U) == 0U) {
        registration->exit = DC_ACTOR_QUEUE_INACTIVE;
        return 1;
    }

    /* LDX $4D reloads the cursor, so every read below is of the slot the
       loop rests on rather than the one the handler left in X. */
    slot = work_ram[0x004DU];
    attributes = work_ram[0x1080U + (size_t)slot];
    registration->slot = slot;
    registration->attributes = attributes;

    /* AND #$01 : BEQ past the clear request. */
    if ((attributes & 0x01U) != 0U) {
        /* LDA $3800,X : BIT #$40 : BEQ past the clear request. */
        const uint8_t state = work_ram[0x1800U + (size_t)slot];

        registration->clear_state = state;
        if ((state & 0x40U) != 0U) {
            /* AND #$BF : STA $3800,X : JSR $F605. */
            registration->cleared_state = (uint8_t)(state & 0xBFU);
            registration->exit = DC_ACTOR_QUEUE_CLEAR_REQUEST;
            return 1;
        }
    }

    /* LDA $3080,X : BIT #$20 : BNE to the RTS. */
    if ((attributes & 0x20U) != 0U) {
        registration->exit = DC_ACTOR_QUEUE_SUPPRESSED;
        return 1;
    }

    /* BIT #$04 : the $2C3A queue, filled upward from $2CBC. */
    if ((attributes & 0x04U) != 0U) {
        registration->cursor = work_ram[0x0CBCU];
        registration->updated_cursor =
            (uint8_t)(registration->cursor + 1U);
        registration->queue_index = registration->cursor;
        registration->exit = DC_ACTOR_QUEUE_FIRST;
        return 1;
    }

    /* BIT #$08 : the $2BBA queue, filled upward from $2CBA. */
    if ((attributes & 0x08U) != 0U) {
        registration->cursor = work_ram[0x0CBAU];
        registration->updated_cursor =
            (uint8_t)(registration->cursor + 1U);
        registration->queue_index = registration->cursor;
        registration->exit = DC_ACTOR_QUEUE_SECOND;
        return 1;
    }

    /* Otherwise the same $2BBA queue, filled downward from $2CBB. The
       emitter drains this one from a fixed $7F down to the cursor, so a
       descending cursor is what keeps it in registration order. */
    registration->cursor = work_ram[0x0CBBU];
    registration->updated_cursor = (uint8_t)(registration->cursor - 1U);
    registration->queue_index = registration->cursor;
    registration->exit = DC_ACTOR_QUEUE_THIRD;
    return 1;
}

int dc_classify_actor_camera_activation(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_actor_camera_activation* const activation)
{
    uint16_t anchor;
    uint16_t lower;
    uint16_t position;
    uint16_t upper;

    if (work_ram == NULL || activation == NULL
        || work_ram_size < 0x2000U) {
        return 0;
    }

    activation->occupancy = work_ram[0x1000U + (size_t)slot];
    activation->position_low =
        work_ram[0x1300U + (size_t)slot];
    activation->position_high =
        work_ram[0x1380U + (size_t)slot];
    activation->anchor_low = work_ram[0x00A9U];
    activation->anchor_high = work_ram[0x00AAU];
    activation->flags_before =
        work_ram[0x1080U + (size_t)slot];

    anchor = (uint16_t)(
        (uint16_t)activation->anchor_low
        | ((uint16_t)activation->anchor_high << 8U));
    lower = (uint16_t)(anchor - 0x0030U);
    upper = (uint16_t)(anchor + 0x00F0U);
    position = (uint16_t)(
        (uint16_t)activation->position_low
        | ((uint16_t)activation->position_high << 8U));
    activation->lower_low = (uint8_t)lower;
    activation->lower_high = (uint8_t)(lower >> 8U);
    activation->upper_low = (uint8_t)upper;
    activation->upper_high = (uint8_t)(upper >> 8U);
    activation->flags_after = activation->flags_before;

    if (activation->occupancy == 0U) {
        activation->path = DC_ACTOR_CAMERA_EMPTY;
    } else if (((uint16_t)(position - lower) & 0x8000U) != 0U) {
        activation->flags_after =
            (uint8_t)(activation->flags_before & 0x7FU);
        activation->path = DC_ACTOR_CAMERA_BELOW;
    } else if (((uint16_t)(position - upper) & 0x8000U) != 0U) {
        activation->flags_after =
            (uint8_t)(activation->flags_before | 0x80U);
        activation->path = DC_ACTOR_CAMERA_INSIDE;
    } else {
        activation->flags_after =
            (uint8_t)(activation->flags_before & 0x7FU);
        activation->path = DC_ACTOR_CAMERA_ABOVE;
    }
    return 1;
}

int dc_prepare_actor_slot_reset(
    const uint8_t slot,
    dc_actor_slot_reset* const reset)
{
    size_t index;

    if (reset == NULL || slot > 0x7FU) {
        return 0;
    }

    reset->slot = slot;
    for (index = 0U; index < DC_ACTOR_SLOT_FIELD_COUNT; ++index) {
        reset->work_ram_offsets[index] = (uint16_t)(
            0x1000U + (uint16_t)slot + (uint16_t)(index * 0x80U));
    }
    return 1;
}

int dc_classify_giant_skeleton_eyeball_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    dc_giant_skeleton_eyeball_gate* const gate)
{
    if (work_ram == NULL || gate == NULL || work_ram_size <= 0x0D20U) {
        return 0;
    }

    gate->egg_completion_latch = work_ram[0x0D20U];
    gate->egg_completion_bit =
        (uint8_t)(gate->egg_completion_latch & 0x01U);
    gate->session_flags = work_ram[0x0053U];
    gate->final_animation_bit =
        (uint8_t)(gate->session_flags & 0x02U);

    if (gate->egg_completion_bit != 0U) {
        gate->path = DC_GIANT_SKELETON_EYEBALL_ACTIVATE;
    } else if (gate->final_animation_bit != 0U) {
        gate->path = DC_GIANT_SKELETON_EYEBALL_FINAL_ANIMATION;
    } else {
        gate->path = DC_GIANT_SKELETON_EYEBALL_DORMANT;
    }
    return 1;
}

int dc_classify_giant_skeleton_eyeball_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_giant_skeleton_eyeball_initializer* const initializer)
{
    const size_t flags_offset = 0x1080U + (size_t)slot;

    if (work_ram == NULL || initializer == NULL || slot != 0x2FU
        || work_ram_size <= flags_offset) {
        return 0;
    }

    initializer->slot = slot;
    initializer->flags_before = work_ram[flags_offset];
    initializer->flags_after = (uint8_t)(initializer->flags_before & 0xBFU);
    return 1;
}

int dc_classify_giant_skeleton_eyeball_opening_wait(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_giant_skeleton_eyeball_opening_wait* const wait)
{
    const size_t timer_offset = 0x1A00U + (size_t)slot;
    const size_t state_offset = 0x1E00U + (size_t)slot;

    if (work_ram == NULL || wait == NULL || slot != 0x2FU
        || work_ram_size <= state_offset || work_ram[timer_offset] <= 1U
        || work_ram[state_offset] != 0x02U) {
        return 0;
    }

    wait->slot = slot;
    wait->timer = work_ram[timer_offset];
    wait->decremented_timer = (uint8_t)(wait->timer - 1U);
    return 1;
}

int dc_classify_giant_skeleton_eyeball_steady_idle(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_giant_skeleton_eyeball_steady_idle* const idle)
{
    const size_t collision_offset = 0x1800U + (size_t)slot;

    if (work_ram == NULL || idle == NULL || slot != 0x2FU
        || work_ram_size <= collision_offset
        || (work_ram[collision_offset] & 0x80U) != 0U
        || (work_ram[0x0053U] & 0x02U) != 0U) {
        return 0;
    }

    idle->slot = slot;
    idle->collision_state = work_ram[collision_offset];
    idle->session_flags = work_ram[0x0053U];
    return 1;
}

int dc_classify_giant_skeleton_eyeball_final_wait(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_giant_skeleton_eyeball_final_wait* const wait)
{
    const size_t timer_offset = 0x1A00U + (size_t)slot;
    const size_t phase_offset = 0x1B00U + (size_t)slot;
    const size_t secondary_offset = 0x1B80U + (size_t)slot;

    if (work_ram == NULL || wait == NULL || slot != 0x2FU
        || work_ram_size <= secondary_offset || work_ram[timer_offset] <= 1U
        || work_ram[phase_offset] <= 1U) {
        return 0;
    }

    wait->slot = slot;
    wait->timer = work_ram[timer_offset];
    wait->phase = work_ram[phase_offset];
    wait->secondary_counter = work_ram[secondary_offset];
    wait->decremented_timer = (uint8_t)(wait->timer - 1U);
    wait->decremented_phase = (uint8_t)(wait->phase - 1U);
    wait->final_accumulator = (uint8_t)(
        wait->decremented_phase | wait->secondary_counter);
    return 1;
}

int dc_classify_gray_blob_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_gray_blob_initializer* const initializer)
{
    const size_t position_low_offset = 0x1300U + (size_t)slot;
    const size_t position_high_offset = 0x1380U + (size_t)slot;

    if (work_ram == NULL || initializer == NULL || slot < 0x50U
        || slot > 0x56U || work_ram_size <= position_low_offset
        || work_ram_size <= position_high_offset) {
        return 0;
    }

    initializer->slot = slot;
    initializer->position_low = work_ram[position_low_offset];
    initializer->position_high = work_ram[position_high_offset];
    return 1;
}

int dc_classify_gray_blob_first_live(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_gray_blob_first_live* const first_live)
{
    const size_t flags_offset = 0x1080U + (size_t)slot;
    const size_t collision_offset = 0x1800U + (size_t)slot;
    const size_t phase_timer_offset = 0x1A00U + (size_t)slot;

    if (work_ram == NULL || first_live == NULL || slot < 0x50U
        || slot > 0x56U || work_ram_size <= flags_offset
        || work_ram_size <= collision_offset
        || work_ram_size <= phase_timer_offset
        || work_ram[flags_offset] != 0x82U
        || work_ram[collision_offset] != 0x13U
        || work_ram[phase_timer_offset] != 0x01U) {
        return 0;
    }

    first_live->slot = slot;
    first_live->flags_before = work_ram[flags_offset];
    first_live->flags_after = (uint8_t)(first_live->flags_before & 0x9FU);
    first_live->collision_before = work_ram[collision_offset];
    first_live->collision_after =
        (uint8_t)(first_live->collision_before & 0x7FU);
    return 1;
}

int dc_classify_mercy_star_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_mercy_star_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL
        || (slot != 0x5FU && slot != 0x60U && slot != 0x61U)
        || work_ram_size <= 0x1FE1U) {
        return 0;
    }

    initializer->slot = slot;
    return 1;
}

int dc_classify_devil_shooter_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_devil_shooter_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL || slot != 0x0EU
        || work_ram_size <= 0x1F8EU) {
        return 0;
    }

    initializer->slot = slot;
    return 1;
}

int dc_classify_little_skull_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    const uint8_t pair,
    dc_little_skull_initializer* const initializer)
{
    const size_t live_count_offset = 0x0CF5U + (size_t)pair;

    if (work_ram == NULL || initializer == NULL
        || (slot != 0x46U && slot != 0x47U && slot != 0x49U && slot != 0x4AU)
        || pair > 1U || pair != (slot >= 0x49U ? 1U : 0U)
        || work_ram_size <= live_count_offset || work_ram_size <= 0x1FCAU) {
        return 0;
    }

    initializer->slot = slot;
    initializer->pair = pair;
    initializer->live_count_after =
        (uint8_t)(work_ram[live_count_offset] + 1U);
    return 1;
}

int dc_classify_little_skull_pair_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    const uint8_t pair,
    dc_little_skull_pair_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL
        || (slot != 0x45U && slot != 0x48U)
        || pair > 1U || pair != (slot == 0x48U ? 1U : 0U)
        || work_ram_size <= 0x1FC8U) {
        return 0;
    }

    initializer->slot = slot;
    initializer->pair = pair;
    return 1;
}

int dc_classify_little_skull_hidden_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_little_skull_hidden_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL || slot != 0x44U
        || work_ram_size <= 0x1FC4U) {
        return 0;
    }

    initializer->slot = slot;
    return 1;
}

int dc_classify_dragon_head_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_dragon_head_initializer* const initializer)
{
    const size_t flags_offset = 0x1080U + (size_t)slot;

    if (work_ram == NULL || initializer == NULL || slot != 0x13U
        || work_ram_size <= 0x1F93U) {
        return 0;
    }

    initializer->slot = slot;
    initializer->flags_before = work_ram[flags_offset];
    initializer->flags_after = (uint8_t)(initializer->flags_before & 0xBFU);
    return 1;
}

int dc_classify_giant_skeleton_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_giant_skeleton_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL || slot != 0x30U
        || work_ram_size <= 0x1FB0U) {
        return 0;
    }

    initializer->slot = slot;
    return 1;
}

int dc_classify_giant_skeleton_controller(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_giant_skeleton_controller* const controller)
{
    const size_t collision_offset = 0x1800U + (size_t)slot;

    if (work_ram == NULL || controller == NULL || slot != 0x30U
        || work_ram_size <= collision_offset) {
        return 0;
    }

    controller->slot = slot;
    controller->collision_state = work_ram[collision_offset];
    controller->path = (controller->collision_state & 0x80U) != 0U
        ? DC_GIANT_SKELETON_CONTROLLER_HIT
        : DC_GIANT_SKELETON_CONTROLLER_IDLE;
    return 1;
}

int dc_classify_three_blobs_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    const uint8_t initial_state,
    dc_three_blobs_initializer* const initializer)
{
    static const uint8_t appearances[] = {0x8AU, 0x82U, 0x85U};

    if (work_ram == NULL || initializer == NULL || slot < 0x58U
        || slot > 0x5AU || initial_state != (uint8_t)(slot - 0x58U)
        || work_ram_size <= 0x1FDAU) {
        return 0;
    }

    initializer->slot = slot;
    initializer->initial_state = initial_state;
    initializer->appearance = appearances[initial_state];
    return 1;
}

int dc_classify_three_blobs_controller_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_three_blobs_controller_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL || slot != 0x57U
        || work_ram_size <= 0x0CFDU) {
        return 0;
    }

    initializer->slot = slot;
    return 1;
}

int dc_classify_mercy_star_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    dc_mercy_star_gate* const gate)
{
    size_t player;

    if (work_ram == NULL || gate == NULL || work_ram_size <= 0x0D52U) {
        return 0;
    }

    gate->table_mode = work_ram[0x0254U];
    gate->balls_remaining = work_ram[0x0D52U];
    gate->selected_player = work_ram[0x0251U];
    player = (size_t)gate->selected_player;
    gate->score_low = work_ram[0x0205U + player];
    gate->score_middle = work_ram[0x020CU + player];
    gate->score_high = work_ram[0x0213U + player];
    gate->stored_score =
        (uint32_t)gate->score_low
        | ((uint32_t)gate->score_middle << 8U)
        | ((uint32_t)gate->score_high << 16U);

    if (gate->table_mode != 0U) {
        gate->path = DC_MERCY_STAR_NON_NORMAL_MODE;
    } else if (gate->balls_remaining != 1U) {
        gate->path = DC_MERCY_STAR_NOT_LAST_BALL;
    } else if (gate->stored_score >= 0x001388UL) {
        gate->path = DC_MERCY_STAR_SCORE_LIMIT;
    } else {
        gate->path = DC_MERCY_STAR_ACTIVATE;
    }
    return 1;
}

int dc_classify_three_blobs_controller(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    dc_three_blobs_controller* const controller)
{
    if (work_ram == NULL || controller == NULL
        || work_ram_size <= 0x0D3BU) {
        return 0;
    }

    controller->first_state = work_ram[0x0D39U];
    controller->second_state = work_ram[0x0D3AU];
    controller->third_state = work_ram[0x0D3BU];

    if (controller->first_state != controller->second_state) {
        controller->path = DC_THREE_BLOBS_FIRST_PAIR_MISMATCH;
    } else if (controller->first_state != controller->third_state) {
        controller->path = DC_THREE_BLOBS_THIRD_VALUE_MISMATCH;
    } else {
        controller->path = DC_THREE_BLOBS_EQUAL;
    }
    return 1;
}

int dc_classify_three_blobs_appearance_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_three_blobs_appearance_gate* const gate)
{
    static const uint8_t appearance_table[6] = {
        0x8AU, 0x82U, 0x85U, 0x8AU, 0x8EU, 0x85U
    };
    const size_t phase_offset = 0x1A80U + (size_t)slot;
    const size_t collision_offset = 0x1800U + (size_t)slot;

    if (work_ram == NULL || gate == NULL
        || (slot != 0x58U && slot != 0x59U && slot != 0x5AU)
        || work_ram_size <= phase_offset || work_ram_size <= collision_offset
        || work_ram_size <= 0x0D3BU) {
        return 0;
    }

    gate->slot = slot;
    gate->phase_index = work_ram[phase_offset];
    if (gate->phase_index >= 3U) {
        return 0;
    }
    gate->global_state = work_ram[0x0D39U + (size_t)gate->phase_index];
    if (gate->global_state >= sizeof(appearance_table)) {
        return 0;
    }
    gate->appearance = appearance_table[gate->global_state];
    gate->collision_state = work_ram[collision_offset];
    gate->path = (gate->collision_state & 0x80U) != 0U
        ? DC_THREE_BLOBS_APPEARANCE_GATE_COLLISION
        : DC_THREE_BLOBS_APPEARANCE_GATE_IDLE;
    return 1;
}

int dc_classify_three_blobs_collision_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_three_blobs_collision_gate* const gate)
{
    const size_t collision_offset = 0x1800U + (size_t)slot;

    if (work_ram == NULL || gate == NULL
        || (slot != 0x58U && slot != 0x59U && slot != 0x5AU)
        || work_ram_size <= collision_offset) {
        return 0;
    }

    gate->slot = slot;
    gate->collision_state = work_ram[collision_offset];
    return 1;
}

int dc_classify_three_blobs_first_movement_prefix(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_three_blobs_first_movement_prefix* const prefix)
{
    const size_t timer_offset = 0x1A00U + (size_t)slot;
    const size_t x_offset = 0x1B00U + (size_t)slot;
    const size_t y_offset = 0x1B80U + (size_t)slot;

    if (work_ram == NULL || prefix == NULL
        || (slot != 0x58U && slot != 0x59U && slot != 0x5AU)
        || work_ram_size <= timer_offset || work_ram_size <= x_offset
        || work_ram_size <= y_offset) {
        return 0;
    }

    prefix->slot = slot;
    prefix->decremented_timer = (uint8_t)(work_ram[timer_offset] - 1U);
    prefix->decremented_x = (uint8_t)(work_ram[x_offset] - 1U);
    prefix->decremented_y = (uint8_t)(work_ram[y_offset] - 1U);
    if (prefix->decremented_timer == 0U) {
        prefix->path = DC_THREE_BLOBS_FIRST_MOVEMENT_PREFIX_TIMER_EXPIRY;
        return 1;
    }
    if (prefix->decremented_x == 0U) {
        prefix->path = DC_THREE_BLOBS_FIRST_MOVEMENT_PREFIX_X_EXPIRY;
        return 1;
    }
    if (prefix->decremented_y == 0U) {
        prefix->path = DC_THREE_BLOBS_FIRST_MOVEMENT_PREFIX_Y_EXPIRY;
        return 1;
    }
    prefix->path = DC_THREE_BLOBS_FIRST_MOVEMENT_PREFIX_DIRECT;
    return 1;
}

int dc_classify_dragon_egg_controller(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    dc_dragon_egg_controller* const controller)
{
    if (work_ram == NULL || controller == NULL
        || work_ram_size <= 0x0CF4U) {
        return 0;
    }

    controller->live_egg_count = work_ram[0x0CF4U];
    controller->path = controller->live_egg_count == 0U
        ? DC_DRAGON_EGG_CONTROLLER_COMPLETE
        : DC_DRAGON_EGG_CONTROLLER_ACTIVE;
    return 1;
}

int dc_classify_dragon_egg_controller_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_dragon_egg_controller_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL || slot != 0x1FU
        || work_ram_size <= 0x0CF7U) {
        return 0;
    }

    initializer->slot = slot;
    return 1;
}

int dc_classify_dragon_egg_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_dragon_egg_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL
        || (slot < 0x20U || slot > 0x25U)
        || work_ram_size <= 0x0CF4U) {
        return 0;
    }

    initializer->slot = slot;
    initializer->live_egg_count_after =
        (uint8_t)(work_ram[0x0CF4U] + 1U);
    return 1;
}

int dc_classify_big_star_feature_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    dc_big_star_feature_gate* const gate)
{
    if (work_ram == NULL || gate == NULL
        || work_ram_size <= 0x0CFAU) {
        return 0;
    }

    gate->pending_feature_bits = work_ram[0x0CFAU];
    gate->path = gate->pending_feature_bits == 0U
        ? DC_BIG_STAR_FEATURE_GATE_IDLE
        : DC_BIG_STAR_FEATURE_GATE_PENDING;
    return 1;
}

int dc_classify_big_star_pending_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    dc_big_star_pending_gate* const gate)
{
    if (work_ram == NULL || gate == NULL
        || work_ram_size <= 0x0D04U) {
        return 0;
    }

    gate->pending_feature_bits = work_ram[0x0D04U];
    gate->selected_pending_bits =
        (uint8_t)(gate->pending_feature_bits & 0x03U);
    gate->path = gate->selected_pending_bits == 0U
        ? DC_BIG_STAR_PENDING_GATE_IDLE
        : DC_BIG_STAR_PENDING_GATE_ACTIVE;
    return 1;
}

int dc_classify_big_star_pending_helper(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_big_star_pending_helper* const helper)
{
    const size_t phase_offset = 0x1B00U + (size_t)slot;

    if (work_ram == NULL || helper == NULL || slot != 0x4BU
        || work_ram_size <= phase_offset) {
        return 0;
    }

    helper->slot = slot;
    helper->phase = work_ram[phase_offset];
    helper->next_phase = helper->phase < 3U
        ? 3U
        : (helper->phase <= 5U ? (uint8_t)(helper->phase + 1U) : helper->phase);
    return 1;
}

int dc_classify_big_star_pending_right_helper(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_big_star_pending_right_helper* const helper)
{
    const size_t phase_offset = 0x1B00U + (size_t)slot;

    if (work_ram == NULL || helper == NULL || slot != 0x4BU
        || work_ram_size <= phase_offset) {
        return 0;
    }

    helper->slot = slot;
    helper->phase = work_ram[phase_offset];
    helper->next_phase = helper->phase == 4U
        ? 3U
        : (helper->phase > 0U && helper->phase < 4U
               ? (uint8_t)(helper->phase - 1U)
               : helper->phase);
    return 1;
}

int dc_classify_big_star_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_big_star_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL || slot != 0x4BU
        || work_ram_size <= 0x1FCBU) {
        return 0;
    }

    initializer->slot = slot;
    return 1;
}

int dc_classify_undead_head_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_undead_head_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL || slot != 0x4DU
        || work_ram_size <= 0x1FCDU) {
        return 0;
    }

    initializer->slot = slot;
    return 1;
}

int dc_classify_undead_head_selector_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_undead_head_selector_gate* const gate)
{
    const size_t selector_offset = 0x0D18U;

    if (work_ram == NULL || gate == NULL || slot != 0x4DU
        || work_ram_size <= selector_offset) {
        return 0;
    }

    gate->slot = slot;
    gate->selector = work_ram[selector_offset];
    gate->path = gate->selector != 0U
        ? DC_UNDEAD_HEAD_SELECTOR_GATE_WAITING
        : DC_UNDEAD_HEAD_SELECTOR_GATE_ACTIVE;
    return 1;
}

int dc_classify_undead_head_waiting_parent(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_undead_head_waiting_parent* const parent)
{
    const size_t session_offset = 0x0D00U;
    const size_t selector_offset = 0x0D18U;

    if (work_ram == NULL || parent == NULL || slot != 0x4DU
        || work_ram_size <= selector_offset) {
        return 0;
    }
    if (work_ram[session_offset] != 0U
        || work_ram[session_offset + 1U] != 0U
        || work_ram[selector_offset] == 0U) {
        return 0;
    }

    parent->slot = slot;
    parent->selector = work_ram[selector_offset];
    return 1;
}

int dc_classify_side_worm_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_side_worm_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL
        || (slot != 0x4EU && slot != 0x4FU) || work_ram_size <= 0x1FCFU) {
        return 0;
    }

    initializer->slot = slot;
    initializer->selector = slot == 0x4EU ? 0x06U : 0x02U;
    initializer->initial_state = slot == 0x4EU ? 0x00U : 0x01U;
    return 1;
}

int dc_classify_side_worm_selector_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_side_worm_selector_gate* const gate)
{
    const size_t selector_offset = 0x0D18U;
    const size_t expected_selector_offset = 0x1780U + (size_t)slot;

    if (work_ram == NULL || gate == NULL || (slot != 0x4EU && slot != 0x4FU)
        || work_ram_size <= selector_offset
        || work_ram_size <= expected_selector_offset) {
        return 0;
    }

    gate->slot = slot;
    gate->selector = work_ram[selector_offset];
    gate->expected_selector = work_ram[expected_selector_offset];
    gate->path = gate->selector != gate->expected_selector
        ? DC_SIDE_WORM_SELECTOR_GATE_WAITING
        : DC_SIDE_WORM_SELECTOR_GATE_ACTIVE;
    return 1;
}

int dc_classify_side_worm_waiting_parent(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_side_worm_waiting_parent* const parent)
{
    dc_side_worm_selector_gate gate;
    if (work_ram == NULL || parent == NULL || work_ram_size <= 0x0D01U
        || work_ram[0x0D00U] != 0U || work_ram[0x0D01U] != 0U
        || !dc_classify_side_worm_selector_gate(
            work_ram, work_ram_size, slot, &gate)
        || gate.path != DC_SIDE_WORM_SELECTOR_GATE_WAITING) {
        return 0;
    }
    parent->slot = gate.slot;
    parent->selector = gate.selector;
    parent->expected_selector = gate.expected_selector;
    return 1;
}

int dc_classify_big_star_rotation_wait(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_big_star_rotation_wait* const rotation)
{
    const size_t timer_offset = 0x1A00U + (size_t)slot;

    if (work_ram == NULL || rotation == NULL || slot != 0x4BU
        || work_ram_size <= 0x0D04U || work_ram_size <= 0x0D01U
        || work_ram_size <= timer_offset || work_ram_size <= 0x00CBU) {
        return 0;
    }

    rotation->slot = slot;
    rotation->pending_feature_bits = work_ram[0x0D04U];
    rotation->velocity_low = work_ram[0x0D00U];
    rotation->velocity_high = work_ram[0x0D01U];
    rotation->timer = work_ram[timer_offset];
    rotation->decremented_timer = (uint8_t)(rotation->timer - 1U);
    rotation->session_flags = work_ram[0x00CBU];
    rotation->cleared_session_flags =
        (uint8_t)(rotation->session_flags & 0xFDU);
    rotation->path = ((rotation->pending_feature_bits & 0x03U) == 0U
            && (rotation->velocity_low != 0U
                || rotation->velocity_high != 0U)
            && rotation->decremented_timer != 0U)
        ? DC_BIG_STAR_ROTATION_WAIT_STABLE
        : DC_BIG_STAR_ROTATION_WAIT_ACTIVE;
    return 1;
}

int dc_classify_devil_shooter_companion_dormant(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_devil_shooter_companion_dormant* const companion)
{
    if (work_ram == NULL || companion == NULL || work_ram_size <= 0x0053U
        || (slot != 0x0AU && slot != 0x0DU)) {
        return 0;
    }

    companion->slot = slot;
    companion->session_flags = work_ram[0x0053U];
    companion->active_session_bits =
        (uint8_t)(companion->session_flags & 0x08U);
    companion->path = companion->active_session_bits == 0U
        ? DC_DEVIL_SHOOTER_COMPANION_DORMANT_IDLE
        : DC_DEVIL_SHOOTER_COMPANION_DORMANT_ACTIVE;
    return 1;
}

int dc_classify_devil_shooter_companion_position(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_devil_shooter_companion_position* const position)
{
    if (work_ram == NULL || position == NULL || work_ram_size <= 0x0D15U
        || slot != 0x0BU) {
        return 0;
    }

    const uint16_t vertical =
        (uint16_t)(0x00A8U + (uint16_t)(work_ram[0x0D15U] >> 1U));
    position->slot = slot;
    position->charge = work_ram[0x0D15U];
    position->vertical_low = (uint8_t)vertical;
    position->low_add_carry = (uint8_t)(vertical >> 8U);
    position->vertical_high = (uint8_t)(0x02U + position->low_add_carry);
    return 1;
}

int dc_classify_devil_shooter_companion_appearance(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_devil_shooter_companion_appearance* const appearance)
{
    const size_t cache_offset = 0x1E00U + (size_t)slot;
    const size_t appearance_offset = 0x1980U + (size_t)slot;

    if (work_ram == NULL || appearance == NULL || slot != 0x0CU
        || work_ram_size <= 0x0D15U || work_ram_size <= cache_offset
        || work_ram_size <= appearance_offset) {
        return 0;
    }

    appearance->slot = slot;
    appearance->charge = work_ram[0x0D15U];
    appearance->cached_charge = work_ram[cache_offset];
    appearance->appearance = work_ram[appearance_offset];
    appearance->advanced_appearance = appearance->appearance;
    if (appearance->charge == appearance->cached_charge) {
        appearance->path = DC_DEVIL_SHOOTER_COMPANION_APPEARANCE_IDLE;
    } else if (appearance->appearance >= 3U) {
        return 0;
    } else {
        appearance->advanced_appearance =
            (uint8_t)(appearance->appearance + 1U);
        appearance->path = appearance->advanced_appearance == 3U
            ? DC_DEVIL_SHOOTER_COMPANION_APPEARANCE_RESET
            : DC_DEVIL_SHOOTER_COMPANION_APPEARANCE_ADVANCE;
    }
    return 1;
}

int dc_classify_devil_shooter_companion_active_timer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_devil_shooter_companion_active_timer* const timer)
{
    const size_t timer_offset = 0x1B00U + (size_t)slot;
    const size_t appearance_offset = 0x1980U + (size_t)slot;

    if (work_ram == NULL || timer == NULL || (slot != 0x0AU && slot != 0x0DU)
        || work_ram_size <= timer_offset || work_ram_size <= appearance_offset) {
        return 0;
    }

    timer->slot = slot;
    timer->timer = work_ram[timer_offset];
    timer->decremented_timer = (uint8_t)(timer->timer - 1U);
    timer->appearance = work_ram[appearance_offset];
    timer->advanced_appearance = timer->appearance;
    if (timer->decremented_timer != 0U) {
        timer->path = DC_DEVIL_SHOOTER_COMPANION_ACTIVE_TIMER_WAIT;
    } else {
        timer->advanced_appearance = (uint8_t)(timer->appearance + 1U);
        const uint8_t terminal_appearance = slot == 0x0AU ? 4U : 5U;
        timer->path = timer->advanced_appearance == terminal_appearance
            ? DC_DEVIL_SHOOTER_COMPANION_ACTIVE_TIMER_RESET
            : DC_DEVIL_SHOOTER_COMPANION_ACTIVE_TIMER_ADVANCE;
    }
    return 1;
}

int dc_classify_walled_star_hit_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_walled_star_hit_gate* const gate)
{
    const size_t collision_offset = 0x1800U + (size_t)slot;

    if (work_ram == NULL || gate == NULL || slot > 0x7FU
        || work_ram_size <= collision_offset) {
        return 0;
    }

    gate->slot = slot;
    gate->collision_state = work_ram[collision_offset];
    gate->shared_access_latch = work_ram[0x0DBCU];
    if ((gate->collision_state & 0x80U) != 0U) {
        gate->path = DC_WALLED_STAR_HIT_GATE_COLLISION;
    } else if (gate->shared_access_latch == 0U) {
        gate->path = DC_WALLED_STAR_HIT_GATE_IDLE;
    } else {
        gate->path = DC_WALLED_STAR_HIT_GATE_SHARED_ACTIVE;
    }
    return 1;
}

int dc_classify_tower_hit_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_tower_hit_gate* const gate)
{
    const size_t collision_offset = 0x1800U + (size_t)slot;

    if (work_ram == NULL || gate == NULL || slot > 0x7FU
        || work_ram_size <= collision_offset) {
        return 0;
    }

    gate->slot = slot;
    gate->collision_state = work_ram[collision_offset];
    gate->path = (gate->collision_state & 0x80U) != 0U
        ? DC_TOWER_HIT_GATE_TRIGGERED
        : DC_TOWER_HIT_GATE_IDLE;
    return 1;
}

int dc_classify_tower_bonus_terminal(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_tower_bonus_terminal* const terminal)
{
    const size_t collision_offset = 0x1800U + (size_t)slot;

    if (work_ram == NULL || terminal == NULL || slot != 0x2DU
        || work_ram_size <= collision_offset) {
        return 0;
    }

    terminal->slot = slot;
    terminal->qualifying_latch = work_ram[0x0CFCU];
    terminal->gate_counter = work_ram[0x0D4DU];
    terminal->collision_state = work_ram[collision_offset];
    return terminal->qualifying_latch == 0x01U
        && terminal->gate_counter == 0x01U;
}

int dc_classify_tower_bonus_continuation(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_tower_bonus_continuation* const continuation)
{
    const size_t collision_offset = 0x1800U + (size_t)slot;

    if (work_ram == NULL || continuation == NULL || slot != 0x2DU
        || work_ram_size <= collision_offset) {
        return 0;
    }

    continuation->slot = slot;
    continuation->collision_state = work_ram[collision_offset];
    return 1;
}

int dc_classify_tower_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_tower_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL || slot != 0x2DU
        || work_ram_size <= 0x1FADU) {
        return 0;
    }

    initializer->slot = slot;
    return 1;
}

int dc_classify_bumper_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_bumper_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL
        || (slot != 0x14U && slot != 0x15U) || work_ram_size <= 0x1F95U) {
        return 0;
    }

    initializer->slot = slot;
    initializer->side = (uint8_t)(slot - 0x14U);
    return 1;
}

int dc_classify_bumper_child_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t parent_slot,
    const uint8_t child_slot,
    dc_bumper_child_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL || parent_slot != 0x15U
        || child_slot < 0x71U || child_slot > 0x76U
        || work_ram_size <= 0x1FF6U) {
        return 0;
    }

    initializer->parent_slot = parent_slot;
    initializer->child_slot = child_slot;
    initializer->parent_horizontal_low = work_ram[0x1180U + parent_slot];
    initializer->parent_horizontal_high = work_ram[0x1200U + parent_slot];
    initializer->parent_vertical_low = work_ram[0x1300U + parent_slot];
    initializer->parent_vertical_high = work_ram[0x1380U + parent_slot];
    initializer->parent_appearance = work_ram[0x1E00U + parent_slot];
    return 1;
}

int dc_classify_small_soldier_source_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_small_soldier_source_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL
        || (slot != 0x34U && slot != 0x35U) || work_ram_size <= 0x1FB5U) {
        return 0;
    }

    initializer->slot = slot;
    initializer->side = (uint8_t)(slot - 0x34U);
    return 1;
}

int dc_classify_small_soldier_delayed_spawn_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_small_soldier_delayed_spawn_gate* const gate)
{
    const size_t timer_offset = 0x1A00U + (size_t)slot;

    if (work_ram == NULL || gate == NULL
        || (slot != 0x34U && slot != 0x35U)
        || work_ram_size <= timer_offset) {
        return 0;
    }

    gate->slot = slot;
    gate->timer_before = work_ram[timer_offset];
    gate->timer_after = (uint8_t)(gate->timer_before - 1U);
    gate->path = gate->timer_after != 0U
        ? DC_SMALL_SOLDIER_DELAYED_SPAWN_GATE_WAITING
        : DC_SMALL_SOLDIER_DELAYED_SPAWN_GATE_ACTIVE;
    return 1;
}

int dc_classify_giant_face_controller_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    const uint8_t stage,
    dc_giant_face_controller_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL || slot != 0x36U
        || stage > 2U || work_ram_size <= 0x1FB6U) {
        return 0;
    }

    initializer->slot = slot;
    initializer->stage = stage;
    initializer->persisted_form = work_ram[0x0258U];
    return stage != 2U || initializer->persisted_form == 0U;
}

int dc_classify_giant_face_animator_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_giant_face_animator_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL || slot != 0x37U
        || work_ram_size <= 0x1FB7U) {
        return 0;
    }
    initializer->slot = slot;
    return 1;
}

int dc_classify_giant_face_animator_active_wait(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_giant_face_animator_active_wait* const wait)
{
    const size_t timer_offset = 0x1A00U + (size_t)slot;

    if (work_ram == NULL || wait == NULL || slot != 0x37U
        || work_ram_size <= timer_offset || work_ram[timer_offset] <= 1U) {
        return 0;
    }

    wait->slot = slot;
    wait->timer = work_ram[timer_offset];
    wait->decremented_timer = (uint8_t)(wait->timer - 1U);
    return 1;
}

int dc_classify_giant_face_crown_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_giant_face_crown_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL || slot != 0x39U
        || work_ram_size <= 0x1FB9U) {
        return 0;
    }
    initializer->slot = slot;
    return 1;
}

int dc_classify_giant_face_mouth_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_giant_face_mouth_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL || slot != 0x38U
        || work_ram_size <= 0x1FB8U) {
        return 0;
    }
    initializer->slot = slot;
    return 1;
}

int dc_classify_knight_hit_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_knight_hit_gate* const gate)
{
    const size_t collision_offset = 0x1800U + (size_t)slot;

    if (work_ram == NULL || gate == NULL
        || (slot < 0x3AU || slot > 0x3DU)
        || work_ram_size <= collision_offset) {
        return 0;
    }

    gate->slot = slot;
    gate->collision_state = work_ram[collision_offset];
    gate->path = (gate->collision_state & 0x80U) != 0U
        ? DC_KNIGHT_HIT_GATE_TRIGGERED
        : DC_KNIGHT_HIT_GATE_IDLE;
    return 1;
}

int dc_classify_knight_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_knight_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL
        || slot < 0x3AU || slot > 0x3DU
        || work_ram_size <= 0x1FBDU) {
        return 0;
    }

    initializer->slot = slot;
    initializer->queue_index = (uint8_t)(slot - 0x3AU);
    return 1;
}

int dc_classify_side_spider_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    const uint8_t side_parameter,
    dc_side_spider_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL
        || (slot != 0x28U && slot != 0x29U)
        || side_parameter != (uint8_t)(slot - 0x28U)
        || work_ram_size <= 0x1FA9U) {
        return 0;
    }

    initializer->slot = slot;
    initializer->side_parameter = side_parameter;
    return 1;
}

int dc_classify_front_spider_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    const uint8_t side_parameter,
    dc_front_spider_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL
        || (slot != 0x26U && slot != 0x27U)
        || side_parameter != (uint8_t)(slot - 0x26U)
        || work_ram_size <= 0x1FA7U) {
        return 0;
    }

    initializer->slot = slot;
    initializer->side_parameter = side_parameter;
    return 1;
}

int dc_classify_knight_waiting_parent(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_knight_waiting_parent* const parent)
{
    const size_t queue_index_offset = 0x1B00U + (size_t)slot;
    const size_t collision_offset = 0x1800U + (size_t)slot;

    if (work_ram == NULL || parent == NULL
        || slot < 0x3AU || slot > 0x3DU
        || work_ram_size <= queue_index_offset
        || work_ram_size <= collision_offset) {
        return 0;
    }

    parent->slot = slot;
    parent->queue_index = work_ram[queue_index_offset];
    if (parent->queue_index > 3U
        || work_ram_size <= 0x0D06U + (size_t)parent->queue_index) {
        return 0;
    }
    parent->door_handshake =
        work_ram[0x0D06U + (size_t)parent->queue_index];
    parent->collision_state = work_ram[collision_offset];
    if ((parent->door_handshake & 0x01U) != 0U) {
        parent->path = DC_KNIGHT_WAITING_PARENT_ACTIVATING;
    } else if ((parent->collision_state & 0x80U) != 0U) {
        parent->path = DC_KNIGHT_WAITING_PARENT_HIT;
    } else {
        parent->path = DC_KNIGHT_WAITING_PARENT_WAITING;
    }
    return 1;
}

int dc_classify_knight_movement_parent(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_knight_movement_parent* const parent)
{
    const size_t queue_index_offset = 0x1B00U + (size_t)slot;
    const size_t timer_offset = 0x1A00U + (size_t)slot;
    const size_t phase_offset = 0x1980U + (size_t)slot;

    if (work_ram == NULL || parent == NULL
        || slot < 0x3AU || slot > 0x3DU
        || work_ram_size <= queue_index_offset
        || work_ram_size <= timer_offset
        || work_ram_size <= phase_offset) {
        return 0;
    }

    parent->slot = slot;
    parent->queue_index = work_ram[queue_index_offset];
    if (parent->queue_index > 3U) {
        return 0;
    }
    parent->timer = work_ram[timer_offset];
    parent->phase = work_ram[phase_offset];
    return 1;
}

int dc_classify_spider_appearance(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_spider_appearance* const appearance)
{
    const size_t timer_offset = 0x1A00U + (size_t)slot;
    const size_t appearance_offset = 0x1980U + (size_t)slot;

    if (work_ram == NULL || appearance == NULL || slot < 0x26U
        || slot > 0x29U || work_ram_size <= timer_offset
        || work_ram_size <= appearance_offset) {
        return 0;
    }

    appearance->slot = slot;
    appearance->timer = work_ram[timer_offset];
    appearance->decremented_timer = (uint8_t)(appearance->timer - 1U);
    appearance->appearance = work_ram[appearance_offset];
    appearance->advanced_appearance =
        (uint8_t)(appearance->appearance + 1U);
    if (appearance->decremented_timer != 0U) {
        appearance->path = DC_SPIDER_APPEARANCE_WAITING;
    } else if (appearance->advanced_appearance < 4U) {
        appearance->path = DC_SPIDER_APPEARANCE_ADVANCE;
    } else {
        appearance->path = DC_SPIDER_APPEARANCE_RESET;
    }
    return 1;
}

int dc_classify_tower_insect_animation(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_tower_insect_animation* const animation)
{
    static const uint8_t frame_table[32] = {
        0x00U, 0x01U, 0x02U, 0x01U, 0x03U, 0x04U, 0x05U, 0x04U,
        0x06U, 0x07U, 0x08U, 0x07U, 0x09U, 0x0AU, 0x0BU, 0x0AU,
        0x0CU, 0x0DU, 0x0EU, 0x0DU, 0x0FU, 0x10U, 0x11U, 0x10U,
        0x12U, 0x13U, 0x14U, 0x13U, 0x15U, 0x16U, 0x17U, 0x16U,
    };
    const size_t cadence_offset = 0x1A80U + (size_t)slot;
    const size_t phase_offset = 0x1C80U + (size_t)slot;
    const size_t timer_offset = 0x1A00U + (size_t)slot;
    uint8_t lookup_index;

    if (work_ram == NULL || animation == NULL
        || (slot != 0x64U && slot != 0x67U && slot != 0x6AU
            && slot != 0x6DU)
        || work_ram_size <= cadence_offset || work_ram_size <= phase_offset
        || work_ram_size <= timer_offset) {
        return 0;
    }

    animation->slot = slot;
    animation->cadence = work_ram[cadence_offset];
    animation->phase = work_ram[phase_offset];
    lookup_index = (uint8_t)((uint8_t)((animation->cadence & 0xFEU) << 1U)
        + (uint8_t)(animation->phase & 0x03U));
    if (lookup_index >= sizeof(frame_table)) {
        return 0;
    }
    animation->lookup_index = lookup_index;
    animation->frame = frame_table[lookup_index];
    animation->timer = work_ram[timer_offset];
    animation->decremented_timer = (uint8_t)(animation->timer - 1U);
    animation->advanced_phase = (uint8_t)(animation->phase + 1U);
    animation->path = animation->decremented_timer != 0U
        ? DC_TOWER_INSECT_ANIMATION_WAITING
        : DC_TOWER_INSECT_ANIMATION_ADVANCE;
    return 1;
}

int dc_classify_tower_insect_source_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_tower_insect_source_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL || slot != 0x2CU
        || work_ram_size <= 0x1FACU) {
        return 0;
    }

    initializer->slot = slot;
    return 1;
}

int dc_classify_tower_insect_source_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_tower_insect_source_gate* const gate)
{
    const size_t child_count_offset = 0x0D2BU;

    if (work_ram == NULL || gate == NULL || slot != 0x2CU
        || work_ram_size <= child_count_offset) {
        return 0;
    }

    gate->slot = slot;
    gate->child_count = work_ram[child_count_offset];
    gate->path = gate->child_count != 0U
        ? DC_TOWER_INSECT_SOURCE_GATE_WAITING
        : DC_TOWER_INSECT_SOURCE_GATE_ACTIVE;
    return 1;
}

int dc_classify_tower_insect_source_countdown_wait(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_tower_insect_source_countdown_wait* const wait)
{
    const size_t timer_offset = 0x1A00U + (size_t)slot;

    if (work_ram == NULL || wait == NULL || slot != 0x2CU
        || work_ram_size <= timer_offset) {
        return 0;
    }

    wait->slot = slot;
    wait->timer = work_ram[timer_offset];
    wait->decremented_timer = (uint8_t)(wait->timer - 1U);
    return 1;
}

int dc_classify_small_soldier_source_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_small_soldier_source_gate* const gate)
{
    size_t group_count_offset;

    if (slot == 0x34U) {
        group_count_offset = 0x0D16U;
    } else if (slot == 0x35U) {
        group_count_offset = 0x0D17U;
    } else {
        return 0;
    }
    if (work_ram == NULL || gate == NULL
        || work_ram_size <= group_count_offset) {
        return 0;
    }

    gate->slot = slot;
    gate->group_count = work_ram[group_count_offset];
    gate->path = gate->group_count != 0U
        ? DC_SMALL_SOLDIER_SOURCE_GATE_WAITING
        : DC_SMALL_SOLDIER_SOURCE_GATE_ACTIVE;
    return 1;
}

int dc_classify_sorcerer_color_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_sorcerer_color_gate* const gate)
{
    const size_t shared_color_offset = 0x0D40U;
    const size_t cached_color_offset = 0x1980U + (size_t)slot;

    if (work_ram == NULL || gate == NULL || slot != 0x16U
        || work_ram_size <= shared_color_offset
        || work_ram_size <= cached_color_offset) {
        return 0;
    }

    gate->slot = slot;
    gate->shared_color = work_ram[shared_color_offset];
    gate->cached_color = work_ram[cached_color_offset];
    gate->path = gate->shared_color == gate->cached_color
        ? DC_SORCERER_COLOR_GATE_WAITING
        : DC_SORCERER_COLOR_GATE_ACTIVE;
    return 1;
}

int dc_classify_sorcerer_deployment_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_sorcerer_deployment_initializer* const initializer)
{
    static const uint8_t initial_delay_low[8] = {
        0x00U, 0x2CU, 0x58U, 0x84U, 0xB0U, 0xDCU, 0x08U, 0x34U
    };
    const size_t route_offset = 0x1700U + (size_t)slot;
    const size_t flags_offset = 0x1080U + (size_t)slot;

    if (work_ram == NULL || initializer == NULL || slot < 0x17U
        || slot > 0x1EU || work_ram_size <= route_offset
        || work_ram_size <= flags_offset || work_ram_size <= 0x0D0FU) {
        return 0;
    }

    initializer->slot = slot;
    initializer->route_index = work_ram[route_offset];
    if (initializer->route_index > 7U
        || initializer->route_index != (uint8_t)(slot - 0x17U)) {
        return 0;
    }
    initializer->flags_after = (uint8_t)(work_ram[flags_offset] | 0x60U);
    initializer->collision_selector = work_ram[0x0D0BU];
    initializer->live_count_after = (uint8_t)(work_ram[0x0D0FU] + 1U);
    initializer->initial_delay_low =
        initial_delay_low[initializer->route_index];
    initializer->initial_delay_high = initializer->route_index < 6U ? 0U : 1U;
    return 1;
}

int dc_classify_sorcerer_staging_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    const uint8_t route_index,
    dc_sorcerer_staging_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL || slot < 0x17U
        || slot > 0x1EU || route_index > 7U
        || route_index != (uint8_t)(slot - 0x17U)
        || work_ram_size <= 0x1F9EU) {
        return 0;
    }
    initializer->slot = slot;
    initializer->route_index = route_index;
    return 1;
}

int dc_classify_dragon_head_hit_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_dragon_head_hit_gate* const gate)
{
    const size_t collision_offset = 0x1800U + (size_t)slot;

    if (work_ram == NULL || gate == NULL || slot > 0x7FU
        || work_ram_size <= collision_offset) {
        return 0;
    }

    gate->slot = slot;
    gate->collision_state = work_ram[collision_offset];
    gate->path = (gate->collision_state & 0x80U) != 0U
        ? DC_DRAGON_HEAD_HIT_GATE_TRIGGERED
        : DC_DRAGON_HEAD_HIT_GATE_IDLE;
    return 1;
}

int dc_classify_bumper_hit_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_bumper_hit_gate* const gate)
{
    const size_t collision_offset = 0x1800U + (size_t)slot;

    if (work_ram == NULL || gate == NULL || slot > 0x7FU
        || work_ram_size <= collision_offset) {
        return 0;
    }

    gate->slot = slot;
    gate->collision_state = work_ram[collision_offset];
    gate->path = (gate->collision_state & 0x80U) != 0U
        ? DC_BUMPER_HIT_GATE_TRIGGERED
        : DC_BUMPER_HIT_GATE_IDLE;
    return 1;
}

int dc_classify_devil_shooter_hit_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_devil_shooter_hit_gate* const gate)
{
    const size_t collision_offset = 0x1800U + (size_t)slot;

    if (work_ram == NULL || gate == NULL || slot > 0x7FU
        || work_ram_size <= collision_offset) {
        return 0;
    }

    gate->slot = slot;
    gate->collision_state = work_ram[collision_offset];
    gate->path = (gate->collision_state & 0x80U) != 0U
        ? DC_DEVIL_SHOOTER_HIT_GATE_TRIGGERED
        : DC_DEVIL_SHOOTER_HIT_GATE_IDLE;
    return 1;
}

int dc_classify_devil_shooter_release_delay_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_devil_shooter_release_delay_gate* const gate)
{
    const size_t collision_offset = 0x1800U + (size_t)slot;
    if (work_ram == NULL || gate == NULL || slot != 0x0EU
        || work_ram_size <= 0x0254U || collision_offset >= work_ram_size) {
        return 0;
    }

    gate->slot = slot;
    gate->table_mode = work_ram[0x0254U];
    gate->active_player = work_ram[0x0251U];
    if ((size_t)gate->active_player > work_ram_size - 1U - 0x0013U) {
        return 0;
    }
    gate->button_input = work_ram[0x0013U + (size_t)gate->active_player];
    gate->collision_state = work_ram[collision_offset];
    gate->returns_idle = (uint8_t)(
        gate->table_mode == 0U
        && (gate->button_input & 0x01U) == 0U
        && (gate->collision_state & 0x80U) == 0U);
    return 1;
}

int dc_classify_devil_shooter_post_launch_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_devil_shooter_post_launch_gate* const gate)
{
    const size_t collision_offset = 0x1800U + (size_t)slot;
    if (work_ram == NULL || gate == NULL || slot != 0x0EU
        || work_ram_size <= collision_offset || work_ram_size <= 0x0053U) {
        return 0;
    }

    gate->slot = slot;
    gate->collision_state = work_ram[collision_offset];
    gate->session_flags = work_ram[0x0053U];
    gate->returns_idle = (uint8_t)(
        (gate->collision_state & 0x80U) == 0U
        && (gate->session_flags & 0x02U) == 0U);
    return 1;
}

int dc_classify_devil_shooter_charge_position(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_devil_shooter_charge_position* const position)
{
    const size_t low_offset = 0x1280U + (size_t)slot;
    const size_t middle_offset = 0x1300U + (size_t)slot;
    const size_t high_offset = 0x1380U + (size_t)slot;
    const size_t charge_base_offset = 0x1E00U + (size_t)slot;

    if (work_ram == NULL || position == NULL || slot != 0x0EU
        || work_ram_size <= charge_base_offset) {
        return 0;
    }

    const uint16_t low_sum = (uint16_t)work_ram[low_offset] + 0x40U;
    const uint8_t vertical_low = (uint8_t)low_sum;
    const uint16_t middle_sum = (uint16_t)work_ram[middle_offset]
        + (uint16_t)(low_sum >> 8U);
    const uint8_t vertical_middle = (uint8_t)middle_sum;
    const uint8_t vertical_high = (uint8_t)(work_ram[high_offset]
        + (uint8_t)(middle_sum >> 8U));
    const uint8_t charge =
        (uint8_t)(vertical_middle - work_ram[charge_base_offset]);

    // Charge values at and above $27 enter the separate coordinate-clamp
    // branch, which is intentionally left with original assembly.
    if (charge >= 0x27U) {
        return 0;
    }

    position->slot = slot;
    position->vertical_low = vertical_low;
    position->vertical_middle = vertical_middle;
    position->vertical_high = vertical_high;
    position->charge = charge;
    position->appearance = charge < 0x07U ? 0U
        : charge < 0x13U               ? 1U
        : charge < 0x1AU               ? 2U
        : charge < 0x20U               ? 3U
                                      : 4U;
    position->subtract_overflow =
        ((vertical_middle ^ work_ram[charge_base_offset])
         & (vertical_middle ^ charge) & 0x80U)
        != 0U;
    return 1;
}

int dc_classify_devil_shooter_charge_retraction(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_devil_shooter_charge_retraction* const retraction)
{
    const size_t low_offset = 0x1280U + (size_t)slot;
    const size_t middle_offset = 0x1300U + (size_t)slot;
    const size_t high_offset = 0x1380U + (size_t)slot;
    const size_t charge_base_offset = 0x1E00U + (size_t)slot;
    const size_t high_base_offset = 0x1E80U + (size_t)slot;

    if (work_ram == NULL || retraction == NULL || slot != 0x0EU
        || work_ram_size <= high_base_offset) {
        return 0;
    }

    const uint8_t vertical_low = work_ram[low_offset];
    const uint16_t middle_difference = (uint16_t)work_ram[middle_offset]
        - 0x02U;
    const uint8_t vertical_middle = (uint8_t)middle_difference;
    const uint8_t vertical_high = (uint8_t)(work_ram[high_offset]
        - (middle_difference > 0xFFU ? 1U : 0U));
    const uint8_t charge =
        (uint8_t)(vertical_middle - work_ram[charge_base_offset]);
    const uint8_t charge_borrow = vertical_middle
        < work_ram[charge_base_offset]
        ? 1U
        : 0U;
    const uint8_t high_difference =
        (uint8_t)(vertical_high - work_ram[high_base_offset]
            - charge_borrow);

    // The BPL after the high-byte subtraction takes the bounded path only
    // when bit seven is clear. The negative path clamps to the base position
    // and is intentionally left with original assembly.
    if ((high_difference & 0x80U) != 0U) {
        return 0;
    }

    retraction->slot = slot;
    retraction->vertical_low = vertical_low;
    retraction->vertical_middle = vertical_middle;
    retraction->vertical_high = vertical_high;
    retraction->charge = charge;
    retraction->has_appearance_write = charge != 0U;
    retraction->appearance = charge < 0x07U ? 0U
        : charge < 0x13U               ? 1U
        : charge < 0x1AU               ? 2U
        : charge < 0x20U               ? 3U
                                      : 4U;
    retraction->high_subtract_overflow =
        ((vertical_high
          ^ (uint8_t)(work_ram[high_base_offset] + charge_borrow))
         & (vertical_high ^ high_difference) & 0x80U)
        != 0U;
    return 1;
}

int dc_classify_skeleton_insect_left_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_skeleton_insect_left_gate* const gate)
{
    if (work_ram == NULL || gate == NULL || slot > 0x7FU
        || work_ram_size <= 0x0D13U) {
        return 0;
    }

    gate->slot = slot;
    gate->trigger_latch = work_ram[0x0D13U];
    gate->path = gate->trigger_latch != 0U
        ? DC_SKELETON_INSECT_LEFT_GATE_TRIGGERED
        : DC_SKELETON_INSECT_LEFT_GATE_IDLE;
    return 1;
}

int dc_classify_skeleton_insect_right_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_skeleton_insect_right_gate* const gate)
{
    if (work_ram == NULL || gate == NULL || slot > 0x7FU
        || work_ram_size <= 0x0D14U) {
        return 0;
    }

    gate->slot = slot;
    gate->trigger_latch = work_ram[0x0D14U];
    gate->path = gate->trigger_latch != 0U
        ? DC_SKELETON_INSECT_RIGHT_GATE_TRIGGERED
        : DC_SKELETON_INSECT_RIGHT_GATE_IDLE;
    return 1;
}

int dc_classify_skeleton_insect_right_post_trigger(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_skeleton_insect_right_post_trigger* const post_trigger)
{
    if (work_ram == NULL || post_trigger == NULL || slot != 0x33U
        || work_ram_size <= 0x1200U) {
        return 0;
    }

    post_trigger->slot = slot;
    post_trigger->secondary_state = work_ram[0x1200U];
    post_trigger->tower_state = work_ram[0x0CC7U];
    if (post_trigger->secondary_state != 0U) {
        post_trigger->path =
            DC_SKELETON_INSECT_RIGHT_POST_TRIGGER_SECONDARY_ACTIVE;
    } else if (post_trigger->tower_state != 0U) {
        post_trigger->path =
            DC_SKELETON_INSECT_RIGHT_POST_TRIGGER_TOWER_ACTIVE;
    } else {
        post_trigger->path = DC_SKELETON_INSECT_RIGHT_POST_TRIGGER_IDLE;
    }
    return 1;
}

int dc_classify_skeleton_insect_center_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_skeleton_insect_center_gate* const gate)
{
    if (work_ram == NULL || gate == NULL || slot > 0x7FU
        || work_ram_size <= 0x0CFCU) {
        return 0;
    }

    gate->slot = slot;
    gate->trigger_latch = work_ram[0x0CFCU];
    gate->path = gate->trigger_latch != 0U
        ? DC_SKELETON_INSECT_CENTER_GATE_TRIGGERED
        : DC_SKELETON_INSECT_CENTER_GATE_IDLE;
    return 1;
}

int dc_classify_devils_coffin_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_devils_coffin_gate* const gate)
{
    const size_t collision_offset = 0x1800U + (size_t)slot;

    if (work_ram == NULL || gate == NULL || slot > 0x7FU
        || work_ram_size <= collision_offset) {
        return 0;
    }

    gate->slot = slot;
    gate->session_state = work_ram[0x0053U];
    gate->collision_state = work_ram[collision_offset];
    if ((gate->collision_state & 0x80U) != 0U) {
        gate->path = DC_DEVILS_COFFIN_GATE_COLLISION;
    } else if ((gate->session_state & 0x08U) != 0U) {
        gate->path = DC_DEVILS_COFFIN_GATE_TYPE_REFRESH;
    } else {
        gate->path = DC_DEVILS_COFFIN_GATE_IDLE;
    }
    return 1;
}

int dc_classify_devils_coffin_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_devils_coffin_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL || slot != 0x0FU
        || work_ram_size <= 0x198FU) {
        return 0;
    }

    initializer->slot = slot;
    return 1;
}

int dc_classify_devils_coffin_opening(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_devils_coffin_opening* const opening)
{
    const size_t timer_offset = 0x1A00U + (size_t)slot;
    const size_t collision_offset = 0x1800U + (size_t)slot;
    const size_t appearance_offset = 0x1980U + (size_t)slot;

    if (work_ram == NULL || opening == NULL || slot > 0x7FU
        || work_ram_size <= timer_offset) {
        return 0;
    }

    opening->slot = slot;
    opening->timer = work_ram[timer_offset];
    opening->decremented_timer = (uint8_t)(opening->timer - 1U);
    opening->collision_state = work_ram[collision_offset];
    opening->appearance = work_ram[appearance_offset];
    opening->advanced_appearance = (uint8_t)(opening->appearance + 1U);
    if (opening->decremented_timer != 0U) {
        opening->path = DC_DEVILS_COFFIN_OPENING_WAITING;
    } else if (opening->advanced_appearance >= 3U) {
        opening->path = DC_DEVILS_COFFIN_OPENING_HANDOFF;
    } else {
        opening->path = DC_DEVILS_COFFIN_OPENING_ADVANCE;
    }
    return 1;
}

int dc_classify_devils_coffin_closing(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_devils_coffin_closing* const closing)
{
    const size_t timer_offset = 0x1A00U + (size_t)slot;
    const size_t collision_offset = 0x1800U + (size_t)slot;
    const size_t appearance_offset = 0x1980U + (size_t)slot;

    if (work_ram == NULL || closing == NULL || slot > 0x7FU
        || work_ram_size <= timer_offset
        || work_ram_size <= collision_offset
        || work_ram_size <= appearance_offset) {
        return 0;
    }

    closing->slot = slot;
    closing->timer = work_ram[timer_offset];
    closing->decremented_timer = (uint8_t)(closing->timer - 1U);
    closing->collision_state = work_ram[collision_offset];
    closing->appearance = work_ram[appearance_offset];
    closing->decremented_appearance =
        (uint8_t)(closing->appearance - 1U);
    closing->session_state = work_ram[0x0053U];
    closing->closed_session_state =
        (uint8_t)(closing->session_state & 0xF7U);
    if (closing->decremented_timer != 0U) {
        closing->path = DC_DEVILS_COFFIN_CLOSING_WAITING;
    } else if (closing->decremented_appearance == 0U) {
        closing->path = DC_DEVILS_COFFIN_CLOSING_COMPLETE;
    } else {
        closing->path = DC_DEVILS_COFFIN_CLOSING_ADVANCE;
    }
    return 1;
}

int dc_classify_tower_guard_selector(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_tower_guard_selector* const selector)
{
    if (work_ram == NULL || selector == NULL || slot > 0x7FU
        || work_ram_size <= 0x0D4DU) {
        return 0;
    }

    selector->slot = slot;
    selector->gate_counter = work_ram[0x0D4DU];
    if (selector->gate_counter != 0U) {
        selector->selector = 0x84U;
        selector->path = DC_TOWER_GUARD_SELECTOR_CLOSED;
    } else {
        selector->selector = 0x8EU;
        selector->path = DC_TOWER_GUARD_SELECTOR_OPEN;
    }
    return 1;
}

int dc_classify_fixed_scheduler_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_fixed_scheduler_gate* const gate)
{
    const size_t timer_offset = 0x1A00U + (size_t)slot;

    if (work_ram == NULL || gate == NULL || slot > 0x7FU
        || work_ram_size <= timer_offset) {
        return 0;
    }

    gate->slot = slot;
    gate->scene_state = work_ram[0x0CC0U];
    gate->presentation_gate = work_ram[0x0BB9U];
    gate->timer = work_ram[timer_offset];
    gate->decremented_timer = (uint8_t)(gate->timer - 1U);
    if ((gate->scene_state & 0x80U) != 0U) {
        gate->path = DC_FIXED_SCHEDULER_GATE_INACTIVE;
    } else if (gate->presentation_gate != 0U) {
        gate->path = DC_FIXED_SCHEDULER_GATE_SUPPRESSED;
    } else if (gate->decremented_timer != 0U) {
        gate->path = DC_FIXED_SCHEDULER_GATE_WAITING;
    } else {
        gate->path = DC_FIXED_SCHEDULER_GATE_ACTIVE;
    }
    return 1;
}

int dc_classify_fixed_scheduler_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_fixed_scheduler_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL || slot != 0x5CU
        || work_ram_size <= 0x1A5CU) {
        return 0;
    }

    initializer->slot = slot;
    return 1;
}

int dc_classify_fixed_marker_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_fixed_marker_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL || slot != 0x5DU
        || work_ram_size <= 0x1E5DU) {
        return 0;
    }

    initializer->slot = slot;
    return 1;
}

int dc_classify_fixed_response_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_fixed_response_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL || slot != 0x5EU
        || work_ram_size <= 0x0D38U) {
        return 0;
    }

    initializer->slot = slot;
    return 1;
}

int dc_classify_fixed_response_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    dc_fixed_response_gate* const gate)
{
    if (work_ram == NULL || gate == NULL || work_ram_size <= 0x0CC0U) {
        return 0;
    }

    gate->scene_state = work_ram[0x0CC0U];
    gate->collision_scratch = work_ram[0x00CCU];
    if ((gate->scene_state & 0x80U) != 0U) {
        gate->path = DC_FIXED_RESPONSE_GATE_INACTIVE;
    } else if (gate->collision_scratch == 0U) {
        gate->path = DC_FIXED_RESPONSE_GATE_DORMANT;
    } else {
        gate->path = DC_FIXED_RESPONSE_GATE_ACTIVE;
    }
    return 1;
}

int dc_add_scaled_byte_to_word(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t input,
    dc_scaled_word_add* const addition)
{
    uint8_t low;
    uint8_t high;
    uint16_t result;
    size_t index;

    if (work_ram == NULL || addition == NULL || work_ram_size <= 0x0030U) {
        return 0;
    }

    addition->input = input;
    addition->base_low = work_ram[0x002FU];
    addition->base_high = work_ram[0x0030U];
    low = input;
    high = 0U;
    for (index = 0U; index < DC_SCALED_WORD_SHIFT_COUNT; ++index) {
        high = (uint8_t)((uint8_t)(high << 1U) | (uint8_t)(low >> 7U));
        low = (uint8_t)(low << 1U);
        addition->shifted_low[index] = low;
        addition->shifted_high[index] = high;
    }

    result = (uint16_t)(
        (uint16_t)addition->base_low
        | ((uint16_t)addition->base_high << 8U));
    result = (uint16_t)(
        result
        + (uint16_t)(
            (uint16_t)low | ((uint16_t)high << 8U)));
    addition->result_low = (uint8_t)result;
    addition->result_high = (uint8_t)(result >> 8U);
    return 1;
}

int dc_classify_dragon_egg_hit_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_dragon_egg_hit_gate* const gate)
{
    const size_t collision_offset = 0x1800U + (size_t)slot;

    if (work_ram == NULL || gate == NULL || slot > 0x7FU
        || work_ram_size <= collision_offset) {
        return 0;
    }

    gate->slot = slot;
    gate->collision_state = work_ram[collision_offset];
    gate->path = (gate->collision_state & 0x80U) != 0U
        ? DC_DRAGON_EGG_HIT_GATE_TRIGGERED
        : DC_DRAGON_EGG_HIT_GATE_IDLE;
    return 1;
}

int dc_classify_giant_door_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_giant_door_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL || slot != 0x4CU
        || work_ram_size <= 0x1FCCU) {
        return 0;
    }

    initializer->slot = slot;
    return 1;
}

int dc_classify_walled_star_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_walled_star_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL || slot != 0x5BU
        || work_ram_size <= 0x1FDBU) {
        return 0;
    }

    initializer->slot = slot;
    return 1;
}

int dc_classify_giant_door_hit_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_giant_door_hit_gate* const gate)
{
    const size_t collision_offset = 0x1800U + (size_t)slot;

    if (work_ram == NULL || gate == NULL || slot > 0x7FU
        || work_ram_size <= collision_offset) {
        return 0;
    }

    gate->slot = slot;
    gate->collision_state = work_ram[collision_offset];
    gate->path = (gate->collision_state & 0x80U) != 0U
        ? DC_GIANT_DOOR_HIT_GATE_TRIGGERED
        : DC_GIANT_DOOR_HIT_GATE_IDLE;
    return 1;
}

int dc_classify_giant_face_animator_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    dc_giant_face_animator_gate* const gate)
{
    if (work_ram == NULL || gate == NULL
        || work_ram_size <= 0x0D0EU) {
        return 0;
    }

    gate->animation_mode = work_ram[0x0D0EU];
    gate->path = gate->animation_mode == 1U
        ? DC_GIANT_FACE_ANIMATOR_GATE_ACTIVE
        : DC_GIANT_FACE_ANIMATOR_GATE_IDLE;
    return 1;
}

int dc_classify_giant_face_request_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    dc_giant_face_request_gate* const gate)
{
    if (work_ram == NULL || gate == NULL
        || work_ram_size <= 0x0D0CU) {
        return 0;
    }

    gate->request_flags = work_ram[0x0D0CU];
    gate->path = (gate->request_flags & 0x01U) != 0U
        ? DC_GIANT_FACE_REQUEST_GATE_ACTIVE
        : DC_GIANT_FACE_REQUEST_GATE_IDLE;
    return 1;
}

int dc_classify_giant_face_crown_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_giant_face_crown_gate* const gate)
{
    const size_t collision_offset = 0x1800U + (size_t)slot;

    if (work_ram == NULL || gate == NULL || slot > 0x7FU
        || work_ram_size <= collision_offset) {
        return 0;
    }

    gate->slot = slot;
    gate->collision_state = work_ram[collision_offset];
    gate->path = (gate->collision_state & 0x80U) != 0U
        ? DC_GIANT_FACE_CROWN_GATE_HIT
        : DC_GIANT_FACE_CROWN_GATE_IDLE;
    return 1;
}

int dc_classify_giant_face_mouth_gate(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_giant_face_mouth_gate* const gate)
{
    const size_t collision_offset = 0x1800U + (size_t)slot;

    if (work_ram == NULL || gate == NULL || slot > 0x7FU
        || work_ram_size <= 0x0D21U || work_ram_size <= collision_offset) {
        return 0;
    }

    gate->mouth_state = work_ram[0x0D21U];
    gate->slot = slot;
    gate->collision_state = work_ram[collision_offset];
    if (gate->mouth_state != 0U) {
        gate->path = DC_GIANT_FACE_MOUTH_GATE_OPEN;
    } else {
        gate->path = (gate->collision_state & 0x80U) != 0U
            ? DC_GIANT_FACE_MOUTH_GATE_HIT
            : DC_GIANT_FACE_MOUTH_GATE_IDLE;
    }
    return 1;
}

int dc_classify_giant_face_helmet_controller(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_giant_face_helmet_controller* const controller)
{
    const size_t collision_offset = 0x1800U + (size_t)slot;
    const size_t timer_offset = 0x1A00U + (size_t)slot;
    const size_t appearance_offset = 0x1980U + (size_t)slot;

    if (work_ram == NULL || controller == NULL || slot > 0x7FU
        || work_ram_size <= collision_offset || work_ram_size <= timer_offset
        || work_ram_size <= appearance_offset) {
        return 0;
    }

    controller->slot = slot;
    controller->collision_state = work_ram[collision_offset];
    controller->timer = work_ram[timer_offset];
    controller->decremented_timer = (uint8_t)(controller->timer - 1U);
    controller->appearance = work_ram[appearance_offset];
    controller->advanced_appearance =
        (uint8_t)(controller->appearance + 1U);
    if ((controller->collision_state & 0x80U) != 0U) {
        controller->path = DC_GIANT_FACE_HELMET_CONTROLLER_COLLISION;
    } else if (controller->decremented_timer != 0U) {
        controller->path = DC_GIANT_FACE_HELMET_CONTROLLER_WAITING;
    } else if (controller->advanced_appearance < 3U) {
        controller->path = DC_GIANT_FACE_HELMET_CONTROLLER_ADVANCE;
    } else {
        controller->path = DC_GIANT_FACE_HELMET_CONTROLLER_RESET;
    }
    return 1;
}

int dc_classify_giant_face_helmet_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    const uint8_t selector,
    dc_giant_face_helmet_initializer* const initializer)
{
    const size_t collision_offset = 0x1800U + (size_t)slot;

    if (work_ram == NULL || initializer == NULL
        || (slot != 0x2AU && slot != 0x2BU)
        || selector != (slot == 0x2AU ? 0x13U : 0x1DU)
        || work_ram_size <= collision_offset || work_ram_size <= 0x1FABU
        || (work_ram[collision_offset] & 0x80U) != 0U) {
        return 0;
    }

    initializer->slot = slot;
    initializer->selector = selector;
    return 1;
}

int dc_classify_four_star_wall_pocket_controller(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_four_star_wall_pocket_controller* const controller)
{
    return dc_classify_giant_face_helmet_controller(
        work_ram,
        work_ram_size,
        slot,
        controller);
}

int dc_classify_walled_star_access_controller(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_walled_star_access_controller* const controller)
{
    return dc_classify_giant_face_helmet_controller(
        work_ram,
        work_ram_size,
        slot,
        controller);
}

int dc_classify_walled_star_access_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_walled_star_access_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL || slot != 0x11U
        || work_ram_size <= 0x1F91U) {
        return 0;
    }

    initializer->slot = slot;
    return 1;
}

int dc_classify_side_star_initializer(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_side_star_initializer* const initializer)
{
    if (work_ram == NULL || initializer == NULL
        || (slot != 0x10U && slot != 0x12U) || work_ram_size <= 0x1F92U) {
        return 0;
    }

    initializer->slot = slot;
    if (slot == 0x10U) {
        initializer->appearance = 0x1CU;
        initializer->first_dispatch = 0x4BBCU;
        initializer->steady_dispatch = 0x4BD5U;
    } else {
        initializer->appearance = 0x0AU;
        initializer->first_dispatch = 0x4BF1U;
        initializer->steady_dispatch = 0x4C0AU;
    }
    return 1;
}

int dc_classify_side_star_access_controller(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_side_star_access_controller* const controller)
{
    return dc_classify_giant_face_helmet_controller(
        work_ram,
        work_ram_size,
        slot,
        controller);
}

int dc_classify_four_star_wall_pocket_animation(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_four_star_wall_pocket_animation* const animation)
{
    const size_t timer_offset = 0x1A00U + (size_t)slot;
    const size_t appearance_offset = 0x1980U;

    if (work_ram == NULL || animation == NULL || slot > 0x7FU
        || work_ram_size <= timer_offset || work_ram_size <= appearance_offset) {
        return 0;
    }

    animation->slot = slot;
    animation->timer = work_ram[timer_offset];
    animation->decremented_timer = (uint8_t)(animation->timer - 1U);
    animation->appearance = work_ram[appearance_offset];
    animation->advanced_appearance =
        (uint8_t)(animation->appearance + 1U);
    if (animation->appearance == 6U) {
        animation->path = DC_FOUR_STAR_WALL_POCKET_ANIMATION_TRANSITION;
    } else if (animation->decremented_timer != 0U) {
        animation->path = DC_FOUR_STAR_WALL_POCKET_ANIMATION_WAITING;
    } else {
        animation->path = DC_FOUR_STAR_WALL_POCKET_ANIMATION_ADVANCE;
    }
    return 1;
}

int dc_classify_side_star_bonus_entrance_animation(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_side_star_bonus_entrance_animation* const animation)
{
    if (slot != 0x12U) {
        return 0;
    }
    return dc_classify_four_star_wall_pocket_animation(
        work_ram, work_ram_size, slot, animation);
}

int dc_prepare_upper_wall_pocket_hand_initializer(
    const uint8_t slot,
    dc_upper_wall_pocket_hand_initializer* const initializer)
{
    if (initializer == NULL || (slot != 0x40U && slot != 0x41U)) {
        return 0;
    }

    initializer->slot = slot;
    initializer->live_dispatch = 0x5975U;
    return 1;
}

int dc_prepare_upper_wall_pocket_hand_live_setup(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    const uint8_t rng_result,
    dc_upper_wall_pocket_hand_live_setup* const setup)
{
    const size_t collision_flags_offset = 0x1080U + (size_t)slot;

    if (work_ram == NULL || setup == NULL
        || (slot != 0x40U && slot != 0x41U)
        || work_ram_size <= collision_flags_offset
        || work_ram_size <= 0x0080U) {
        return 0;
    }

    setup->slot = slot;
    setup->collision_flags = work_ram[collision_flags_offset];
    setup->hidden_collision_flags =
        (uint8_t)(setup->collision_flags | 0x40U);
    setup->timer_base = work_ram[0x0080U];
    setup->masked_rng = (uint8_t)(rng_result & 0x3FU);
    setup->timer_low = (uint8_t)(setup->masked_rng + setup->timer_base);
    setup->timer_high = 0x01U;
    setup->countdown_dispatch = 0x5991U;
    return 1;
}

int dc_classify_upper_wall_pocket_hand_countdown(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_upper_wall_pocket_hand_countdown* const countdown)
{
    const size_t timer_low_offset = 0x1B00U + (size_t)slot;
    const size_t timer_high_offset = 0x1B80U + (size_t)slot;

    if (work_ram == NULL || countdown == NULL
        || (slot != 0x40U && slot != 0x41U)
        || work_ram_size <= timer_low_offset
        || work_ram_size <= timer_high_offset) {
        return 0;
    }

    countdown->slot = slot;
    countdown->timer_low = work_ram[timer_low_offset];
    countdown->timer_high = work_ram[timer_high_offset];
    countdown->decremented_high = countdown->timer_high;
    if (countdown->timer_low == 0U) {
        countdown->decremented_high =
            (uint8_t)(countdown->decremented_high - 1U);
    }
    countdown->decremented_low = (uint8_t)(countdown->timer_low - 1U);
    countdown->remaining_timer = (uint8_t)(
        countdown->decremented_low | countdown->decremented_high);
    countdown->path = countdown->remaining_timer == 0U
        ? DC_UPPER_WALL_POCKET_HAND_COUNTDOWN_HANDOFF
        : DC_UPPER_WALL_POCKET_HAND_COUNTDOWN_WAITING;
    return 1;
}
