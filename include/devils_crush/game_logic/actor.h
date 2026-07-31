// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum dc_actor_camera_path {
    DC_ACTOR_CAMERA_EMPTY = 0,
    DC_ACTOR_CAMERA_BELOW = 1,
    DC_ACTOR_CAMERA_INSIDE = 2,
    DC_ACTOR_CAMERA_ABOVE = 3
} dc_actor_camera_path;

typedef struct dc_actor_camera_activation {
    uint8_t occupancy;
    uint8_t position_low;
    uint8_t position_high;
    uint8_t anchor_low;
    uint8_t anchor_high;
    uint8_t flags_before;
    uint8_t lower_low;
    uint8_t lower_high;
    uint8_t upper_low;
    uint8_t upper_high;
    uint8_t flags_after;
    dc_actor_camera_path path;
} dc_actor_camera_activation;

#define DC_ACTOR_SLOT_FIELD_COUNT 32U

typedef struct dc_actor_slot_reset {
    uint16_t work_ram_offsets[DC_ACTOR_SLOT_FIELD_COUNT];
    uint8_t slot;
} dc_actor_slot_reset;

typedef enum dc_giant_skeleton_eyeball_gate_path {
    DC_GIANT_SKELETON_EYEBALL_DORMANT = 0,
    DC_GIANT_SKELETON_EYEBALL_ACTIVATE = 1,
    DC_GIANT_SKELETON_EYEBALL_FINAL_ANIMATION = 2
} dc_giant_skeleton_eyeball_gate_path;

typedef struct dc_giant_skeleton_eyeball_gate {
    uint8_t egg_completion_latch;
    uint8_t egg_completion_bit;
    uint8_t session_flags;
    uint8_t final_animation_bit;
    dc_giant_skeleton_eyeball_gate_path path;
} dc_giant_skeleton_eyeball_gate;

typedef enum dc_mercy_star_gate_path {
    DC_MERCY_STAR_NON_NORMAL_MODE = 0,
    DC_MERCY_STAR_NOT_LAST_BALL = 1,
    DC_MERCY_STAR_SCORE_LIMIT = 2,
    DC_MERCY_STAR_ACTIVATE = 3
} dc_mercy_star_gate_path;

typedef struct dc_mercy_star_gate {
    uint8_t table_mode;
    uint8_t balls_remaining;
    uint8_t selected_player;
    uint8_t score_low;
    uint8_t score_middle;
    uint8_t score_high;
    uint32_t stored_score;
    dc_mercy_star_gate_path path;
} dc_mercy_star_gate;

typedef enum dc_three_blobs_controller_path {
    DC_THREE_BLOBS_FIRST_PAIR_MISMATCH = 0,
    DC_THREE_BLOBS_THIRD_VALUE_MISMATCH = 1,
    DC_THREE_BLOBS_EQUAL = 2
} dc_three_blobs_controller_path;

typedef struct dc_three_blobs_controller {
    uint8_t first_state;
    uint8_t second_state;
    uint8_t third_state;
    dc_three_blobs_controller_path path;
} dc_three_blobs_controller;

typedef enum dc_dragon_egg_controller_path {
    DC_DRAGON_EGG_CONTROLLER_ACTIVE = 0,
    DC_DRAGON_EGG_CONTROLLER_COMPLETE = 1
} dc_dragon_egg_controller_path;

typedef struct dc_dragon_egg_controller {
    uint8_t live_egg_count;
    dc_dragon_egg_controller_path path;
} dc_dragon_egg_controller;

typedef enum dc_big_star_feature_gate_path {
    DC_BIG_STAR_FEATURE_GATE_IDLE = 0,
    DC_BIG_STAR_FEATURE_GATE_PENDING = 1
} dc_big_star_feature_gate_path;

typedef struct dc_big_star_feature_gate {
    uint8_t pending_feature_bits;
    dc_big_star_feature_gate_path path;
} dc_big_star_feature_gate;

typedef enum dc_walled_star_hit_gate_path {
    DC_WALLED_STAR_HIT_GATE_IDLE = 0,
    DC_WALLED_STAR_HIT_GATE_COLLISION = 1,
    DC_WALLED_STAR_HIT_GATE_SHARED_ACTIVE = 2
} dc_walled_star_hit_gate_path;

typedef struct dc_walled_star_hit_gate {
    uint8_t slot;
    uint8_t collision_state;
    uint8_t shared_access_latch;
    dc_walled_star_hit_gate_path path;
} dc_walled_star_hit_gate;

typedef enum dc_tower_hit_gate_path {
    DC_TOWER_HIT_GATE_IDLE = 0,
    DC_TOWER_HIT_GATE_TRIGGERED = 1
} dc_tower_hit_gate_path;

typedef struct dc_tower_hit_gate {
    uint8_t slot;
    uint8_t collision_state;
    dc_tower_hit_gate_path path;
} dc_tower_hit_gate;

typedef enum dc_dragon_head_hit_gate_path {
    DC_DRAGON_HEAD_HIT_GATE_IDLE = 0,
    DC_DRAGON_HEAD_HIT_GATE_TRIGGERED = 1
} dc_dragon_head_hit_gate_path;

typedef struct dc_dragon_head_hit_gate {
    uint8_t slot;
    uint8_t collision_state;
    dc_dragon_head_hit_gate_path path;
} dc_dragon_head_hit_gate;

typedef enum dc_bumper_hit_gate_path {
    DC_BUMPER_HIT_GATE_IDLE = 0,
    DC_BUMPER_HIT_GATE_TRIGGERED = 1
} dc_bumper_hit_gate_path;

typedef struct dc_bumper_hit_gate {
    uint8_t slot;
    uint8_t collision_state;
    dc_bumper_hit_gate_path path;
} dc_bumper_hit_gate;

typedef enum dc_devil_shooter_hit_gate_path {
    DC_DEVIL_SHOOTER_HIT_GATE_IDLE = 0,
    DC_DEVIL_SHOOTER_HIT_GATE_TRIGGERED = 1
} dc_devil_shooter_hit_gate_path;

typedef struct dc_devil_shooter_hit_gate {
    uint8_t slot;
    uint8_t collision_state;
    dc_devil_shooter_hit_gate_path path;
} dc_devil_shooter_hit_gate;

typedef enum dc_skeleton_insect_left_gate_path {
    DC_SKELETON_INSECT_LEFT_GATE_IDLE = 0,
    DC_SKELETON_INSECT_LEFT_GATE_TRIGGERED = 1
} dc_skeleton_insect_left_gate_path;

typedef struct dc_skeleton_insect_left_gate {
    uint8_t slot;
    uint8_t trigger_latch;
    dc_skeleton_insect_left_gate_path path;
} dc_skeleton_insect_left_gate;

typedef enum dc_skeleton_insect_right_gate_path {
    DC_SKELETON_INSECT_RIGHT_GATE_IDLE = 0,
    DC_SKELETON_INSECT_RIGHT_GATE_TRIGGERED = 1
} dc_skeleton_insect_right_gate_path;

typedef struct dc_skeleton_insect_right_gate {
    uint8_t slot;
    uint8_t trigger_latch;
    dc_skeleton_insect_right_gate_path path;
} dc_skeleton_insect_right_gate;

typedef enum dc_skeleton_insect_center_gate_path {
    DC_SKELETON_INSECT_CENTER_GATE_IDLE = 0,
    DC_SKELETON_INSECT_CENTER_GATE_TRIGGERED = 1
} dc_skeleton_insect_center_gate_path;

typedef struct dc_skeleton_insect_center_gate {
    uint8_t slot;
    uint8_t trigger_latch;
    dc_skeleton_insect_center_gate_path path;
} dc_skeleton_insect_center_gate;

typedef enum dc_devils_coffin_gate_path {
    DC_DEVILS_COFFIN_GATE_IDLE = 0,
    DC_DEVILS_COFFIN_GATE_TYPE_REFRESH = 1,
    DC_DEVILS_COFFIN_GATE_COLLISION = 2
} dc_devils_coffin_gate_path;

typedef struct dc_devils_coffin_gate {
    uint8_t slot;
    uint8_t session_state;
    uint8_t collision_state;
    dc_devils_coffin_gate_path path;
} dc_devils_coffin_gate;

typedef enum dc_devils_coffin_opening_path {
    DC_DEVILS_COFFIN_OPENING_WAITING = 0,
    DC_DEVILS_COFFIN_OPENING_ADVANCE = 1,
    DC_DEVILS_COFFIN_OPENING_HANDOFF = 2
} dc_devils_coffin_opening_path;

typedef struct dc_devils_coffin_opening {
    uint8_t slot;
    uint8_t timer;
    uint8_t decremented_timer;
    uint8_t collision_state;
    uint8_t appearance;
    uint8_t advanced_appearance;
    dc_devils_coffin_opening_path path;
} dc_devils_coffin_opening;

typedef enum dc_devils_coffin_closing_path {
    DC_DEVILS_COFFIN_CLOSING_WAITING = 0,
    DC_DEVILS_COFFIN_CLOSING_ADVANCE = 1,
    DC_DEVILS_COFFIN_CLOSING_COMPLETE = 2
} dc_devils_coffin_closing_path;

typedef struct dc_devils_coffin_closing {
    uint8_t slot;
    uint8_t timer;
    uint8_t decremented_timer;
    uint8_t collision_state;
    uint8_t appearance;
    uint8_t decremented_appearance;
    uint8_t session_state;
    uint8_t closed_session_state;
    dc_devils_coffin_closing_path path;
} dc_devils_coffin_closing;

typedef enum dc_tower_guard_selector_path {
    DC_TOWER_GUARD_SELECTOR_CLOSED = 0,
    DC_TOWER_GUARD_SELECTOR_OPEN = 1
} dc_tower_guard_selector_path;

typedef struct dc_tower_guard_selector {
    uint8_t slot;
    uint8_t gate_counter;
    uint8_t selector;
    dc_tower_guard_selector_path path;
} dc_tower_guard_selector;

typedef enum dc_fixed_scheduler_gate_path {
    DC_FIXED_SCHEDULER_GATE_INACTIVE = 0,
    DC_FIXED_SCHEDULER_GATE_SUPPRESSED = 1,
    DC_FIXED_SCHEDULER_GATE_WAITING = 2,
    DC_FIXED_SCHEDULER_GATE_ACTIVE = 3
} dc_fixed_scheduler_gate_path;

typedef struct dc_fixed_scheduler_gate {
    uint8_t slot;
    uint8_t scene_state;
    uint8_t presentation_gate;
    uint8_t timer;
    uint8_t decremented_timer;
    dc_fixed_scheduler_gate_path path;
} dc_fixed_scheduler_gate;

typedef enum dc_fixed_response_gate_path {
    DC_FIXED_RESPONSE_GATE_INACTIVE = 0,
    DC_FIXED_RESPONSE_GATE_DORMANT = 1,
    DC_FIXED_RESPONSE_GATE_ACTIVE = 2
} dc_fixed_response_gate_path;

typedef struct dc_fixed_response_gate {
    uint8_t scene_state;
    uint8_t collision_scratch;
    dc_fixed_response_gate_path path;
} dc_fixed_response_gate;

#define DC_SCALED_WORD_SHIFT_COUNT 5U

typedef struct dc_scaled_word_add {
    uint8_t input;
    uint8_t base_low;
    uint8_t base_high;
    uint8_t shifted_low[DC_SCALED_WORD_SHIFT_COUNT];
    uint8_t shifted_high[DC_SCALED_WORD_SHIFT_COUNT];
    uint8_t result_low;
    uint8_t result_high;
} dc_scaled_word_add;

typedef enum dc_dragon_egg_hit_gate_path {
    DC_DRAGON_EGG_HIT_GATE_IDLE = 0,
    DC_DRAGON_EGG_HIT_GATE_TRIGGERED = 1
} dc_dragon_egg_hit_gate_path;

typedef struct dc_dragon_egg_hit_gate {
    uint8_t slot;
    uint8_t collision_state;
    dc_dragon_egg_hit_gate_path path;
} dc_dragon_egg_hit_gate;

typedef enum dc_giant_door_hit_gate_path {
    DC_GIANT_DOOR_HIT_GATE_IDLE = 0,
    DC_GIANT_DOOR_HIT_GATE_TRIGGERED = 1
} dc_giant_door_hit_gate_path;

typedef struct dc_giant_door_hit_gate {
    uint8_t slot;
    uint8_t collision_state;
    dc_giant_door_hit_gate_path path;
} dc_giant_door_hit_gate;

typedef enum dc_giant_face_animator_gate_path {
    DC_GIANT_FACE_ANIMATOR_GATE_IDLE = 0,
    DC_GIANT_FACE_ANIMATOR_GATE_ACTIVE = 1
} dc_giant_face_animator_gate_path;

typedef struct dc_giant_face_animator_gate {
    uint8_t animation_mode;
    dc_giant_face_animator_gate_path path;
} dc_giant_face_animator_gate;

typedef enum dc_giant_face_request_gate_path {
    DC_GIANT_FACE_REQUEST_GATE_IDLE = 0,
    DC_GIANT_FACE_REQUEST_GATE_ACTIVE = 1
} dc_giant_face_request_gate_path;

typedef struct dc_giant_face_request_gate {
    uint8_t request_flags;
    dc_giant_face_request_gate_path path;
} dc_giant_face_request_gate;

// Native classification performed by ROM routine 00:F4FC. Empty slots are
// unchanged. Occupied slots set camera flag bit 7 exactly when their signed
// integer Y coordinate lies in [anchor - 48, anchor + 240).
int dc_classify_actor_camera_activation(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_actor_camera_activation* activation);

// Native destination map for ROM routine 01:C574. The routine clears one
// actor slot across 32 parallel arrays spaced 0x80 bytes apart.
int dc_prepare_actor_slot_reset(
    uint8_t slot,
    dc_actor_slot_reset* reset);

// Native state classification for the ROM routine 02:4DD9 activation gate.
// The Dragon's-Egg completion latch starts the opening sequence; otherwise
// session flag bit 1 selects the final animation or the ordinary dormant RTS.
int dc_classify_giant_skeleton_eyeball_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    dc_giant_skeleton_eyeball_gate* gate);

// Native state classification for the ROM routine 02:4FC1 activation gate.
// Mercy Stars activate only during normal-table play, on the last ball, and
// below 5,000 stored score units (500,000 displayed points).
int dc_classify_mercy_star_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    dc_mercy_star_gate* gate);

// Native state classification for ROM routine 03:65C2. The Three-Blobs
// result controller returns at the first unequal state byte; equal states
// continue into the score and randomized-reset outcome dispatcher.
int dc_classify_three_blobs_controller(
    const uint8_t* work_ram,
    size_t work_ram_size,
    dc_three_blobs_controller* controller);

// Native state classification for ROM routine 02:42F6. A nonzero live-egg
// count returns immediately; zero enters the group-completion award and reset
// sequence.
int dc_classify_dragon_egg_controller(
    const uint8_t* work_ram,
    size_t work_ram_size,
    dc_dragon_egg_controller* controller);

// Native state classification for ROM routine 02:445B. A clear shared
// Little-Skull feature latch returns immediately; pending bits continue into
// the Big-Star direction and scoring update.
int dc_classify_big_star_feature_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    dc_big_star_feature_gate* gate);

// Native state classification for ROM routine 03:6339. A pending collision
// enters the wall-damage path; otherwise a clear shared access latch returns
// immediately and an active latch continues into the post-destruction path.
int dc_classify_walled_star_hit_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_walled_star_hit_gate* gate);

// Native state classification for ROM routine 03:60C3. The fixed Tower
// returns while its indexed collision state is nonnegative; bit 7 enters the
// score, audio, progress, and entrance-gate state machine.
int dc_classify_tower_hit_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_tower_hit_gate* gate);

// Native state classification for ROM routine 02:5A4B. The fixed Dragon's
// Head returns while its indexed collision state is nonnegative; bit 7 enters
// the score, audio, eight-hit counter, and mouth-opening state machine.
int dc_classify_dragon_head_hit_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_dragon_head_hit_gate* gate);

// Native state classification for ROM routine 03:78C7. Either fixed Bumper
// returns while its indexed collision state is nonnegative; bit 7 enters the
// damage phase, score, audio, and eventual six-child release state machine.
int dc_classify_bumper_hit_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_bumper_hit_gate* gate);

// Native state classification for ROM routine 02:5116. The fixed Devil
// Shooter returns while its indexed collision state is nonnegative; bit 7
// marks it occupied and enters the ball-catch state machine.
int dc_classify_devil_shooter_hit_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_devil_shooter_hit_gate* gate);

// Native state classification for ROM routine 02:54B9. The left Skeleton
// Insect returns while its Front-Spider trigger latch is clear; nonzero
// enters the one-time 4,000-point award and opening animation.
int dc_classify_skeleton_insect_left_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_skeleton_insect_left_gate* gate);

// Native state classification for ROM routine 02:5551. The right Skeleton
// Insect returns while its Front-Spider trigger latch is clear; nonzero
// enters the one-time 4,000-point award and opening animation.
int dc_classify_skeleton_insect_right_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_skeleton_insect_right_gate* gate);

// Native state classification for ROM routine 02:55F9. The center Skeleton
// Insect returns while its Center-Spider trigger latch is clear; nonzero
// enters the one-time 10,000-point award and opening animation.
int dc_classify_skeleton_insect_center_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_skeleton_insect_center_gate* gate);

// Native state classification for ROM routine 03:63F1. A nonnegative
// collision marker returns immediately; while Shooter session bit $53.3 is
// set, that return first refreshes the Coffin's actor type to $83. A negative
// collision marker enters the visual return-gate cycle.
int dc_classify_devils_coffin_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_devils_coffin_gate* gate);

// Native state classification for ROM routine 03:640A. A nonzero timer
// after the indexed decrement returns immediately. Timer expiry advances
// the Devil's-Coffin opening appearance; values below three return, while
// three or greater enters the assembly-backed reverse-pass handoff.
int dc_classify_devils_coffin_opening(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_devils_coffin_opening* opening);

// Native state classification for ROM routine 03:6445. A nonzero timer
// after the indexed decrement returns immediately. Timer expiry decrements
// the Devil's-Coffin closing appearance; nonzero values return, while zero
// clears the session-open bit and enters the steady-state initializer.
int dc_classify_devils_coffin_closing(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_devils_coffin_closing* closing);

// Native state classification for ROM routine 03:60A6. A nonzero Tower
// gate counter keeps the adjacent Guard's completion selector closed at
// $84; zero selects the open BS-5 entrance value $8E.
int dc_classify_tower_guard_selector(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_tower_guard_selector* selector);

// Native state classification for ROM routines 03:7B27 and 03:7B7A. A
// negative scene takes the shared inactive path, and a nonzero presentation
// gate suppresses work. Otherwise the indexed eight-frame timer either keeps
// waiting after one decrement or expires into the routine's active body.
int dc_classify_fixed_scheduler_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_fixed_scheduler_gate* gate);

// Native state classification for ROM routine 03:7CAF. A negative scene
// state takes the shared inactive path. Otherwise zero collision scratch
// returns dormant, while a nonzero byte enters the response state machine.
int dc_classify_fixed_response_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    dc_fixed_response_gate* gate);

// Native arithmetic performed by ROM routine 02:5955. The input byte is
// shifted left five places and added to the little-endian scratch word at
// $2F:$30; each shift is exposed for the instruction-faithful adapter.
int dc_add_scaled_byte_to_word(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t input,
    dc_scaled_word_add* addition);

// Native state classification for ROM routine 02:4345. Each fixed egg
// returns while its indexed collision state is nonnegative; bit 7 enters the
// score, impact, disable, and hatchling-allocation path.
int dc_classify_dragon_egg_hit_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_dragon_egg_hit_gate* gate);

// Native state classification for ROM routine 03:62B8. The Giant Door
// returns while its indexed collision state is nonnegative; bit 7 enters the
// score, hit-counter, and completion-handshake path.
int dc_classify_giant_door_hit_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_giant_door_hit_gate* gate);

// Native state classification for ROM routine 03:767E. Animation mode one
// enters the hidden Giant-Face form animator; every other mode returns at
// this dispatch entry without changing actor state.
int dc_classify_giant_face_animator_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    dc_giant_face_animator_gate* gate);

// Native state classification for ROM routine 03:74D8. Request bit zero
// enters the Giant-Face wake sequence; while it is clear this dispatch entry
// returns without changing the persisted form or actor state.
int dc_classify_giant_face_request_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    dc_giant_face_request_gate* gate);

#ifdef __cplusplus
} // extern "C"
#endif
