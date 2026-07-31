// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/game_logic/actor.h"

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
