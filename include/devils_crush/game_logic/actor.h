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

// The main per-frame actor loop at 01:C510 walks 128 slots, dispatching the
// ones whose flags demand it and skipping the rest. Only the skip run is
// classifiable here: a dispatch enters an emulated handler through the
// $3F00/$3F80 jump table, which native code cannot call.
#define DC_ACTOR_SCAN_SLOT_COUNT 0x80U

typedef enum dc_actor_scan_exit {
    // A slot needs its handler; the loop resumes at the dispatch branch.
    DC_ACTOR_SCAN_DISPATCH_NEGATIVE = 0,
    DC_ACTOR_SCAN_DISPATCH_MASKED = 1,
    // Every remaining slot was inactive; the loop reached its terminal
    // comparison and falls out to the routine's RTS.
    DC_ACTOR_SCAN_COMPLETE = 2,
    // The skip budget ran out first; the loop resumes at its own head with
    // the slots walked so far already committed.
    DC_ACTOR_SCAN_BUDGET = 3
} dc_actor_scan_exit;

typedef enum dc_actor_scan_skip {
    // $3000,X was positive, so the loop branched straight to its increment.
    DC_ACTOR_SCAN_SKIP_INACTIVE = 0,
    // $3000,X was negative but $3080,X was positive with bit 1 set.
    DC_ACTOR_SCAN_SKIP_MASKED = 1
} dc_actor_scan_skip;

typedef struct dc_actor_scan_run {
    // Slots skipped before the exit, and the slot index the loop rests on.
    uint8_t skipped;
    uint8_t slot;
    // The accumulator the skipped and exit instructions leave behind.
    uint8_t accumulator;
    dc_actor_scan_exit exit;
    // One dc_actor_scan_skip per skipped slot, in order. The two shapes cost
    // different cycles and place their increment write at different offsets.
    uint8_t skips[DC_ACTOR_SCAN_SLOT_COUNT];
} dc_actor_scan_run;

// Classifies one run of the 01:C510 loop starting at slot `slot`. Returns 0
// when the snapshot cannot support the walk. `slot` must be below
// DC_ACTOR_SCAN_SLOT_COUNT; the caller bounds the run with `maximum_skips` so
// an atomic native body stays inside its interrupt horizon.
int dc_classify_actor_scan_run(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    uint8_t maximum_skips,
    dc_actor_scan_run* run);

// When the 01:C510 loop dispatches a slot it reaches the handler through the
// two-byte vector the jump table holds at $3F00,X and $3F80,X. ROM routine
// 01:C52C copies that vector into the zero-page indirection at $2F/$30 and
// jumps through it, so the native form is a load, two stores, and a computed
// resume address rather than a call this transaction could make itself.
typedef struct dc_actor_dispatch_vector {
    uint8_t slot;
    uint8_t vector_low;
    uint8_t vector_high;
    uint16_t handler;
} dc_actor_dispatch_vector;

// Reads the dispatch vector for `slot`. Returns 0 when the snapshot cannot
// support the read.
int dc_classify_actor_dispatch_vector(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_actor_dispatch_vector* vector);

// Once a dispatched handler returns, ROM routine 01:C606 files the slot into
// one of the three sprite-emission queues that the bank-08 satellite emitter
// drains at 08:4056. The queue is chosen from the slot's attribute byte at
// $3080. The emitter drains all three downward — the first two from their
// own cursors, the third from a fixed $7F — so the first two are emitted in
// reverse registration order and the third, whose cursor counts down, in
// registration order.
typedef enum dc_actor_queue_exit {
    // CPX #$00 found slot zero, so the routine returned untouched.
    DC_ACTOR_QUEUE_IDLE_SLOT = 0,
    // $3080,X was positive for the dispatched slot, so nothing is filed.
    DC_ACTOR_QUEUE_INACTIVE = 1,
    // The slot asked for a one-shot clear at $3800 and the routine left for
    // the emulated helper at 01:F605 without reaching the queues.
    DC_ACTOR_QUEUE_CLEAR_REQUEST = 2,
    // Attribute bit 5 suppresses emission entirely.
    DC_ACTOR_QUEUE_SUPPRESSED = 3,
    // Attribute bit 2: the $2C3A queue, cursor $2CBC, filled upward.
    DC_ACTOR_QUEUE_FIRST = 4,
    // Attribute bit 3: the $2BBA queue, cursor $2CBA, filled upward.
    DC_ACTOR_QUEUE_SECOND = 5,
    // Neither bit: the $2BBA queue, cursor $2CBB, filled downward.
    DC_ACTOR_QUEUE_THIRD = 6
} dc_actor_queue_exit;

typedef struct dc_actor_queue_registration {
    // X on entry, and the slot the routine reloads from the $4D cursor.
    uint8_t entry_slot;
    uint8_t slot;
    // $3080,entry_slot, then $3080,slot once the cursor has been reloaded.
    uint8_t entry_attributes;
    uint8_t attributes;
    // Only meaningful on the clear-request path: $3800,slot before and after
    // bit 6 is cleared.
    uint8_t clear_state;
    uint8_t cleared_state;
    // The cursor the chosen queue reads, the value it is left at, and the
    // queue index the slot is written to.
    uint8_t cursor;
    uint8_t updated_cursor;
    uint8_t queue_index;
    dc_actor_queue_exit exit;
} dc_actor_queue_registration;

// Classifies one 01:C606 call. `entry_slot` is X as the dispatched handler
// left it. Returns 0 when the snapshot cannot support the classification.
int dc_classify_actor_queue_registration(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t entry_slot,
    dc_actor_queue_registration* registration);

#define DC_ACTOR_SLOT_FIELD_COUNT 32U

typedef struct dc_actor_slot_reset {
    uint16_t work_ram_offsets[DC_ACTOR_SLOT_FIELD_COUNT];
    uint8_t slot;
} dc_actor_slot_reset;

typedef enum dc_actor_slot_allocation_path {
    DC_ACTOR_SLOT_ALLOCATION_RESERVED = 0,
    DC_ACTOR_SLOT_ALLOCATION_EXHAUSTED = 1
} dc_actor_slot_allocation_path;

// Native result of the shared 01:C5AB allocator. The original scans the
// occupancy array from slot $04 through $7B and reserves the first zero byte.
// A nonzero global mode at $2055 changes the provisional reservation from
// $80 to $C0.
typedef struct dc_actor_slot_allocation {
    uint8_t slot;
    uint8_t occupied_slots;
    uint8_t mode;
    uint8_t reservation;
    dc_actor_slot_allocation_path path;
} dc_actor_slot_allocation;

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

// Direct ROM routine 02:4DC8 initializes the fixed Giant-Skeleton Eyeball
// actor in slot $2F before handing it to the 02:4DD9 activation gate.
typedef struct dc_giant_skeleton_eyeball_initializer {
    uint8_t slot;
    uint8_t flags_before;
    uint8_t flags_after;
} dc_giant_skeleton_eyeball_initializer;

// Direct scheduled ROM routine 02:4DF7 decrements the Eyeball opening timer.
// Only a still-positive countdown returns locally; both expiry branches retain
// the original animation, audio, and scheduler behavior.
typedef struct dc_giant_skeleton_eyeball_opening_wait {
    uint8_t slot;
    uint8_t timer;
    uint8_t decremented_timer;
} dc_giant_skeleton_eyeball_opening_wait;

// Direct scheduled ROM routine 02:4E24 returns locally while the fixed
// Eyeball has no signed collision state and the final-animation flag is clear.
// Collision/animation continuations remain in assembly.
typedef struct dc_giant_skeleton_eyeball_steady_idle {
    uint8_t slot;
    uint8_t collision_state;
    uint8_t session_flags;
} dc_giant_skeleton_eyeball_steady_idle;

// Direct scheduled ROM routine 02:4F3D advances the fixed Eyeball's final
// animation. Only the non-expiring timer and nonzero phase return is bounded.
typedef struct dc_giant_skeleton_eyeball_final_wait {
    uint8_t slot;
    uint8_t timer;
    uint8_t phase;
    uint8_t secondary_counter;
    uint8_t decremented_timer;
    uint8_t decremented_phase;
    uint8_t final_accumulator;
} dc_giant_skeleton_eyeball_final_wait;

// Direct ROM routine 03:6876 initializes one of the seven upper-chute Gray
// Blobs.  It preserves the layout position while installing the shared live
// animation, collision, and script fields used by 03:68A1.
typedef struct dc_gray_blob_initializer {
    uint8_t slot;
    uint8_t position_low;
    uint8_t position_high;
} dc_gray_blob_initializer;

int dc_classify_gray_blob_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_gray_blob_initializer* initializer);

// Direct ROM routine 03:68A1 begins a newly visible Gray Blob's first live
// pass.  This bounded prefix clears its transient flags, seeds the movement
// timer through the shared RNG, and then enters the original scheduler.
typedef struct dc_gray_blob_first_live {
    uint8_t slot;
    uint8_t flags_before;
    uint8_t flags_after;
    uint8_t collision_before;
    uint8_t collision_after;
} dc_gray_blob_first_live;

int dc_classify_gray_blob_first_live(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_gray_blob_first_live* first_live);

// The normal Gray-Blob live pass at 03:68BF has a small, side-effect-free
// wait arm: a still-live appearance timer, no signed collision, and a
// movement timer that does not expire.  The other arms change animation,
// invoke the collision route, or schedule another coroutine and remain in
// the ROM interpreter.
typedef struct dc_gray_blob_live_wait {
    uint8_t slot;
    uint8_t appearance_timer;
    uint8_t decremented_appearance_timer;
    uint8_t collision_state;
    uint8_t movement_timer;
    uint8_t decremented_movement_timer;
    uint8_t phase_timer;
    uint8_t decremented_phase_timer;
} dc_gray_blob_live_wait;

int dc_classify_gray_blob_live_wait(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_gray_blob_live_wait* wait);

// Direct ROM routine 02:4F9F installs the dormant appearance, collision,
// and script fields for one of the three lower-table Mercy Stars.
typedef struct dc_mercy_star_initializer {
    uint8_t slot;
} dc_mercy_star_initializer;

int dc_classify_mercy_star_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_mercy_star_initializer* initializer);

// Direct ROM routine 02:50EE initializes the fixed Devil-Shooter actor
// before handing it to the 02:5116 collision/catch gate.
typedef struct dc_devil_shooter_initializer {
    uint8_t slot;
} dc_devil_shooter_initializer;

int dc_classify_devil_shooter_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_devil_shooter_initializer* initializer);

// Direct ROM routine 02:44E4 installs one of the four fixed Little-Skull
// actors, increments its owning pair's live count, and schedules 02:4515.
typedef struct dc_little_skull_initializer {
    uint8_t slot;
    uint8_t pair;
    uint8_t live_count_after;
} dc_little_skull_initializer;

int dc_classify_little_skull_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    uint8_t pair,
    dc_little_skull_initializer* initializer);

// Direct ROM routine 02:4485 resets one Little-Skull pair's live count and
// regeneration flag before scheduling its 02:449A controller.
typedef struct dc_little_skull_pair_initializer {
    uint8_t slot;
    uint8_t pair;
} dc_little_skull_pair_initializer;

int dc_classify_little_skull_pair_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    uint8_t pair,
    dc_little_skull_pair_initializer* initializer);

// Direct ROM routine 02:4455 clears the shared Little-Skull feature latch
// before scheduling the hidden 02:445B transition controller in slot $44.
typedef struct dc_little_skull_hidden_initializer {
    uint8_t slot;
} dc_little_skull_hidden_initializer;

int dc_classify_little_skull_hidden_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_little_skull_hidden_initializer* initializer);

// Direct ROM routine 02:5A31 installs the fixed Dragon's Head appearance,
// collision counter, and script fields before its eight-hit gate begins at
// 02:5A4B.
typedef struct dc_dragon_head_initializer {
    uint8_t slot;
    uint8_t flags_before;
    uint8_t flags_after;
} dc_dragon_head_initializer;

int dc_classify_dragon_head_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_dragon_head_initializer* initializer);

// Direct ROM routine 02:5EE4 installs the fixed Giant Skeleton's initial
// appearance before its collision and animation controller begins at 02:5EEC.
typedef struct dc_giant_skeleton_initializer {
    uint8_t slot;
} dc_giant_skeleton_initializer;

int dc_classify_giant_skeleton_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_giant_skeleton_initializer* initializer);

typedef enum dc_giant_skeleton_controller_path {
    DC_GIANT_SKELETON_CONTROLLER_IDLE = 0,
    DC_GIANT_SKELETON_CONTROLLER_HIT = 1
} dc_giant_skeleton_controller_path;

// ROM routine 02:5EEC updates the fixed Giant Skeleton's appearance from the
// shared direction classifier, then either returns or enters its repeatable
// 40-point hit continuation.
typedef struct dc_giant_skeleton_controller {
    uint8_t slot;
    uint8_t collision_state;
    dc_giant_skeleton_controller_path path;
} dc_giant_skeleton_controller;

int dc_classify_giant_skeleton_controller(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_giant_skeleton_controller* controller);

// Direct ROM routine 03:664A initializes the three fixed Three-Blobs layouts
// before their alternating movement states begin at 03:66A3/03:66D3.
typedef struct dc_three_blobs_initializer {
    uint8_t slot;
    uint8_t initial_state;
    uint8_t appearance;
} dc_three_blobs_initializer;

int dc_classify_three_blobs_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    uint8_t initial_state,
    dc_three_blobs_initializer* initializer);

// Direct ROM routine 03:65BC initializes the fixed Three-Blobs result
// controller before its equality gate begins at 03:65C2.
typedef struct dc_three_blobs_controller_initializer {
    uint8_t slot;
} dc_three_blobs_controller_initializer;

int dc_classify_three_blobs_controller_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_three_blobs_controller_initializer* initializer);

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

typedef enum dc_three_blobs_appearance_gate_path {
    DC_THREE_BLOBS_APPEARANCE_GATE_IDLE = 0,
    DC_THREE_BLOBS_APPEARANCE_GATE_COLLISION = 1
} dc_three_blobs_appearance_gate_path;

// Direct ROM routine 03:67B3 maps one of the three shared Blob color states
// through its six-entry appearance table before checking the actor collision
// latch. The no-hit branch returns immediately; collision handling remains
// assembly-owned.
typedef struct dc_three_blobs_appearance_gate {
    uint8_t slot;
    uint8_t phase_index;
    uint8_t global_state;
    uint8_t appearance;
    uint8_t collision_state;
    dc_three_blobs_appearance_gate_path path;
} dc_three_blobs_appearance_gate;

// Direct ROM routine 03:67C0 is the shared final collision latch gate.  A
// nonnegative latch returns immediately; a hit continues into the original
// audio, state-advance, and appearance-update path.
typedef struct dc_three_blobs_collision_gate {
    uint8_t slot;
    uint8_t collision_state;
} dc_three_blobs_collision_gate;

typedef enum dc_three_blobs_first_movement_prefix_path {
    DC_THREE_BLOBS_FIRST_MOVEMENT_PREFIX_DIRECT = 0,
    DC_THREE_BLOBS_FIRST_MOVEMENT_PREFIX_TIMER_EXPIRY = 1,
    DC_THREE_BLOBS_FIRST_MOVEMENT_PREFIX_X_EXPIRY = 2,
    DC_THREE_BLOBS_FIRST_MOVEMENT_PREFIX_Y_EXPIRY = 3
} dc_three_blobs_first_movement_prefix_path;

typedef struct dc_three_blobs_first_movement_prefix {
    uint8_t slot;
    uint8_t decremented_timer;
    uint8_t decremented_x;
    uint8_t decremented_y;
    dc_three_blobs_first_movement_prefix_path path;
} dc_three_blobs_first_movement_prefix;

typedef enum dc_dragon_egg_controller_path {
    DC_DRAGON_EGG_CONTROLLER_ACTIVE = 0,
    DC_DRAGON_EGG_CONTROLLER_COMPLETE = 1
} dc_dragon_egg_controller_path;

typedef struct dc_dragon_egg_controller {
    uint8_t live_egg_count;
    dc_dragon_egg_controller_path path;
} dc_dragon_egg_controller;

// Direct ROM routine 02:42EB initializes the invisible fixed Dragon's-Egg
// group controller before the live-count gate at 02:42F6 begins.
typedef struct dc_dragon_egg_controller_initializer {
    uint8_t slot;
} dc_dragon_egg_controller_initializer;

// Direct ROM routine 02:433F installs each of the six fixed Dragon's-Egg
// records and increments the shared live-egg count before its 02:4345
// collision controller begins.
typedef struct dc_dragon_egg_initializer {
    uint8_t slot;
    uint8_t live_egg_count_after;
} dc_dragon_egg_initializer;

typedef enum dc_big_star_feature_gate_path {
    DC_BIG_STAR_FEATURE_GATE_IDLE = 0,
    DC_BIG_STAR_FEATURE_GATE_PENDING = 1
} dc_big_star_feature_gate_path;

typedef struct dc_big_star_feature_gate {
    uint8_t pending_feature_bits;
    dc_big_star_feature_gate_path path;
} dc_big_star_feature_gate;

typedef enum dc_big_star_pending_gate_path {
    DC_BIG_STAR_PENDING_GATE_IDLE = 0,
    DC_BIG_STAR_PENDING_GATE_ACTIVE = 1
} dc_big_star_pending_gate_path;

typedef struct dc_big_star_pending_gate {
    uint8_t pending_feature_bits;
    uint8_t selected_pending_bits;
    dc_big_star_pending_gate_path path;
} dc_big_star_pending_gate;

// Direct ROM helper 02:5722 selects the next Big-Star angular-velocity
// vector from its indexed phase. Verified pending-left routes reset fixed slot
// $4B phases zero through two to three, increment phases three through five,
// and saturate phase six.
typedef struct dc_big_star_pending_helper {
    uint8_t slot;
    uint8_t phase;
    uint8_t next_phase;
} dc_big_star_pending_helper;

// Direct ROM helper 02:5738 selects the mirrored Big-Star angular-velocity
// vector. Verified pending-right routes retain fixed slot $4B at phase zero,
// decrement phases one through three, and reset phase four to phase three
// before selecting its stopped vector.
typedef struct dc_big_star_pending_right_helper {
    uint8_t slot;
    uint8_t phase;
    uint8_t next_phase;
} dc_big_star_pending_right_helper;

// Direct ROM routine 02:565C initializes the fixed Big-Star pending feature,
// stopped direction, velocity, and timer state before 02:5674 takes over.
typedef struct dc_big_star_initializer {
    uint8_t slot;
} dc_big_star_initializer;

// Direct ROM routine 03:61A0 installs the fixed Undead-Head selector gate
// through the shared coroutine scheduler before its controller runs at 03:61A3.
typedef struct dc_undead_head_initializer {
    uint8_t slot;
} dc_undead_head_initializer;

typedef enum dc_undead_head_selector_gate_path {
    DC_UNDEAD_HEAD_SELECTOR_GATE_WAITING = 0,
    DC_UNDEAD_HEAD_SELECTOR_GATE_ACTIVE = 1
} dc_undead_head_selector_gate_path;

// Direct ROM routine 03:61AB returns while the stopped-selector is nonzero.
// Selector zero enters the original activation, collision, RNG, scoring, and
// bonus sequence.
typedef struct dc_undead_head_selector_gate {
    uint8_t slot;
    uint8_t selector;
    dc_undead_head_selector_gate_path path;
} dc_undead_head_selector_gate;

// The preceding controller entry returns through the stopped-selector gate
// while both session-transition bytes are clear.
typedef struct dc_undead_head_waiting_parent {
    uint8_t slot;
    uint8_t selector;
} dc_undead_head_waiting_parent;

// ROM routines 02:5C4B and 02:5C7C install the fixed left/right Side-Worm
// state, selector, collision, appearance, and steady-dispatch fields.
typedef struct dc_side_worm_initializer {
    uint8_t slot;
    uint8_t selector;
    uint8_t initial_state;
} dc_side_worm_initializer;

typedef enum dc_side_worm_selector_gate_path {
    DC_SIDE_WORM_SELECTOR_GATE_WAITING = 0,
    DC_SIDE_WORM_SELECTOR_GATE_ACTIVE = 1
} dc_side_worm_selector_gate_path;

// Direct ROM routine 02:5CCD returns while the stopped-selector differs from
// this Side-Worm's per-record selector. A matching selector enters the
// original motion, collision, scoring, and feature path.
typedef struct dc_side_worm_selector_gate {
    uint8_t slot;
    uint8_t selector;
    uint8_t expected_selector;
    dc_side_worm_selector_gate_path path;
} dc_side_worm_selector_gate;

typedef struct dc_side_worm_waiting_parent {
    uint8_t slot;
    uint8_t selector;
    uint8_t expected_selector;
} dc_side_worm_waiting_parent;

typedef enum dc_big_star_rotation_wait_path {
    DC_BIG_STAR_ROTATION_WAIT_STABLE = 0,
    DC_BIG_STAR_ROTATION_WAIT_ACTIVE = 1
} dc_big_star_rotation_wait_path;

// Direct ROM routine 02:5674 first consumes the Little-Skull direction
// latch, then takes this compact path while the Big Star has nonzero angular
// speed and its indexed rotation timer has not expired. The timer and session
// flag remain work-RAM owned so presentation and active rotation stay in
// assembly.
typedef struct dc_big_star_rotation_wait {
    uint8_t slot;
    uint8_t pending_feature_bits;
    uint8_t velocity_low;
    uint8_t velocity_high;
    uint8_t timer;
    uint8_t decremented_timer;
    uint8_t session_flags;
    uint8_t cleared_session_flags;
    dc_big_star_rotation_wait_path path;
} dc_big_star_rotation_wait;

typedef enum dc_devil_shooter_companion_dormant_path {
    DC_DEVIL_SHOOTER_COMPANION_DORMANT_IDLE = 0,
    DC_DEVIL_SHOOTER_COMPANION_DORMANT_ACTIVE = 1
} dc_devil_shooter_companion_dormant_path;

typedef struct dc_devil_shooter_companion_dormant {
    uint8_t slot;
    uint8_t session_flags;
    uint8_t active_session_bits;
    dc_devil_shooter_companion_dormant_path path;
} dc_devil_shooter_companion_dormant;

typedef struct dc_devil_shooter_companion_position {
    uint8_t slot;
    uint8_t charge;
    uint8_t vertical_low;
    uint8_t vertical_high;
    uint8_t low_add_carry;
} dc_devil_shooter_companion_position;

typedef enum dc_devil_shooter_companion_appearance_path {
    DC_DEVIL_SHOOTER_COMPANION_APPEARANCE_IDLE = 0,
    DC_DEVIL_SHOOTER_COMPANION_APPEARANCE_ADVANCE = 1,
    DC_DEVIL_SHOOTER_COMPANION_APPEARANCE_RESET = 2
} dc_devil_shooter_companion_appearance_path;

typedef struct dc_devil_shooter_companion_appearance {
    uint8_t slot;
    uint8_t charge;
    uint8_t cached_charge;
    uint8_t appearance;
    uint8_t advanced_appearance;
    dc_devil_shooter_companion_appearance_path path;
} dc_devil_shooter_companion_appearance;

typedef enum dc_devil_shooter_companion_active_timer_path {
    DC_DEVIL_SHOOTER_COMPANION_ACTIVE_TIMER_WAIT = 0,
    DC_DEVIL_SHOOTER_COMPANION_ACTIVE_TIMER_ADVANCE = 1,
    DC_DEVIL_SHOOTER_COMPANION_ACTIVE_TIMER_RESET = 2
} dc_devil_shooter_companion_active_timer_path;

typedef struct dc_devil_shooter_companion_active_timer {
    uint8_t slot;
    uint8_t timer;
    uint8_t decremented_timer;
    uint8_t appearance;
    uint8_t advanced_appearance;
    dc_devil_shooter_companion_active_timer_path path;
} dc_devil_shooter_companion_active_timer;

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

// Direct ROM routine 03:6319 installs the fixed Walled Star's wall state,
// collision, completion, and steady-controller fields before 03:6339 begins.
typedef struct dc_walled_star_initializer {
    uint8_t slot;
} dc_walled_star_initializer;

typedef enum dc_tower_hit_gate_path {
    DC_TOWER_HIT_GATE_IDLE = 0,
    DC_TOWER_HIT_GATE_TRIGGERED = 1
} dc_tower_hit_gate_path;

typedef struct dc_tower_hit_gate {
    uint8_t slot;
    uint8_t collision_state;
    dc_tower_hit_gate_path path;
} dc_tower_hit_gate;

// Internal Tower-controller entry 03:60F9 reaches the BS-5 entrance
// scheduler only after the five qualifying passes have armed the latch and
// decremented its final gate tick. Its scheduled 03:6136 continuation is
// separately classified.
typedef struct dc_tower_bonus_terminal {
    uint8_t slot;
    uint8_t qualifying_latch;
    uint8_t gate_counter;
    uint8_t collision_state;
} dc_tower_bonus_terminal;

// Scheduled ROM continuation 03:6136 returns immediately while the Tower
// collision byte remains nonnegative. A sign-bit collision takes its bounded
// stage-selection prefix before handing off to the shared dispatcher.
typedef struct dc_tower_bonus_continuation {
    uint8_t slot;
    uint8_t collision_state;
} dc_tower_bonus_continuation;

// Direct ROM routine 03:60B3 installs the fixed Tower controller's initial
// progress, phase, and guard state.
typedef struct dc_tower_initializer {
    uint8_t slot;
} dc_tower_initializer;

int dc_classify_tower_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_tower_initializer* initializer);

// Direct ROM routines 03:789A and 03:78A4 install the two fixed Bumpers'
// side identity and initial gameplay state.
typedef struct dc_bumper_initializer {
    uint8_t slot;
    uint8_t side;
} dc_bumper_initializer;

int dc_classify_bumper_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_bumper_initializer* initializer);

// Direct ROM routine 03:79DE initializes each child spawned when the right
// fixed Bumper reaches its terminal phase.
typedef struct dc_bumper_child_initializer {
    uint8_t parent_slot;
    uint8_t child_slot;
    uint8_t parent_horizontal_low;
    uint8_t parent_horizontal_high;
    uint8_t parent_vertical_low;
    uint8_t parent_vertical_high;
    uint8_t parent_appearance;
} dc_bumper_child_initializer;

int dc_classify_bumper_child_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t parent_slot,
    uint8_t child_slot,
    dc_bumper_child_initializer* initializer);

// Direct ROM routines 03:7D4D and 03:7DE9 install the two fixed Side-Star
// sources that later spawn the Small-Soldier groups.
typedef struct dc_small_soldier_source_initializer {
    uint8_t slot;
    uint8_t side;
} dc_small_soldier_source_initializer;

int dc_classify_small_soldier_source_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_small_soldier_source_initializer* initializer);

typedef enum dc_small_soldier_delayed_spawn_gate_path {
    DC_SMALL_SOLDIER_DELAYED_SPAWN_GATE_WAITING = 0,
    DC_SMALL_SOLDIER_DELAYED_SPAWN_GATE_ACTIVE = 1
} dc_small_soldier_delayed_spawn_gate_path;

typedef struct dc_small_soldier_delayed_spawn_gate {
    uint8_t slot;
    uint8_t timer_before;
    uint8_t timer_after;
    dc_small_soldier_delayed_spawn_gate_path path;
} dc_small_soldier_delayed_spawn_gate;

int dc_classify_small_soldier_delayed_spawn_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_small_soldier_delayed_spawn_gate* gate);

// The three staged Giant-Face controller setup entries install its delayed
// scheduler states. Stage two is native only for the empty persisted form;
// nonzero persisted-form restoration remains assembly-owned.
typedef struct dc_giant_face_controller_initializer {
    uint8_t slot;
    uint8_t stage;
    uint8_t persisted_form;
} dc_giant_face_controller_initializer;

int dc_classify_giant_face_controller_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    uint8_t stage,
    dc_giant_face_controller_initializer* initializer);

typedef struct dc_giant_face_animator_initializer {
    uint8_t slot;
} dc_giant_face_animator_initializer;

int dc_classify_giant_face_animator_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_giant_face_animator_initializer* initializer);

// Direct ROM routine 03:76C5 advances the active hidden-form animator's
// local timer. Only the non-expiring decrement returns without entering the
// original phase, drawing, and scheduler path.
typedef struct dc_giant_face_animator_active_wait {
    uint8_t slot;
    uint8_t timer;
    uint8_t decremented_timer;
} dc_giant_face_animator_active_wait;

int dc_classify_giant_face_animator_active_wait(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_giant_face_animator_active_wait* wait);

// The Giant-Face crown surface has one fixed direct scheduler setup entry.
typedef struct dc_giant_face_crown_initializer {
    uint8_t slot;
} dc_giant_face_crown_initializer;

int dc_classify_giant_face_crown_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_giant_face_crown_initializer* initializer);

// The Giant-Face mouth surface has one fixed direct scheduler setup entry.
typedef struct dc_giant_face_mouth_initializer {
    uint8_t slot;
} dc_giant_face_mouth_initializer;

int dc_classify_giant_face_mouth_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_giant_face_mouth_initializer* initializer);

typedef enum dc_knight_hit_gate_path {
    DC_KNIGHT_HIT_GATE_IDLE = 0,
    DC_KNIGHT_HIT_GATE_TRIGGERED = 1
} dc_knight_hit_gate_path;

typedef struct dc_knight_hit_gate {
    uint8_t slot;
    uint8_t collision_state;
    dc_knight_hit_gate_path path;
} dc_knight_hit_gate;

typedef struct dc_knight_initializer {
    uint8_t slot;
    uint8_t queue_index;
} dc_knight_initializer;

typedef struct dc_side_spider_initializer {
    uint8_t slot;
    uint8_t side_parameter;
} dc_side_spider_initializer;

typedef struct dc_front_spider_initializer {
    uint8_t slot;
    uint8_t side_parameter;
} dc_front_spider_initializer;

typedef enum dc_knight_waiting_parent_path {
    DC_KNIGHT_WAITING_PARENT_WAITING = 0,
    DC_KNIGHT_WAITING_PARENT_ACTIVATING = 1,
    DC_KNIGHT_WAITING_PARENT_HIT = 2
} dc_knight_waiting_parent_path;

typedef struct dc_knight_waiting_parent {
    uint8_t slot;
    uint8_t queue_index;
    uint8_t door_handshake;
    uint8_t collision_state;
    dc_knight_waiting_parent_path path;
} dc_knight_waiting_parent;

typedef struct dc_knight_movement_parent {
    uint8_t slot;
    uint8_t queue_index;
    uint8_t timer;
    uint8_t phase;
} dc_knight_movement_parent;

typedef enum dc_spider_appearance_path {
    DC_SPIDER_APPEARANCE_WAITING = 0,
    DC_SPIDER_APPEARANCE_ADVANCE = 1,
    DC_SPIDER_APPEARANCE_RESET = 2
} dc_spider_appearance_path;

typedef struct dc_spider_appearance {
    uint8_t slot;
    uint8_t timer;
    uint8_t decremented_timer;
    uint8_t appearance;
    uint8_t advanced_appearance;
    dc_spider_appearance_path path;
} dc_spider_appearance;

typedef enum dc_tower_insect_animation_path {
    DC_TOWER_INSECT_ANIMATION_WAITING = 0,
    DC_TOWER_INSECT_ANIMATION_ADVANCE = 1
} dc_tower_insect_animation_path;

typedef struct dc_tower_insect_animation {
    uint8_t slot;
    uint8_t cadence;
    uint8_t phase;
    uint8_t lookup_index;
    uint8_t frame;
    uint8_t timer;
    uint8_t decremented_timer;
    uint8_t advanced_phase;
    dc_tower_insect_animation_path path;
} dc_tower_insect_animation;

// Direct ROM routine 02:5F22 schedules the fixed Tower-insect source's
// initial controller continuation.
typedef struct dc_tower_insect_source_initializer {
    uint8_t slot;
} dc_tower_insect_source_initializer;

int dc_classify_tower_insect_source_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_tower_insect_source_initializer* initializer);

typedef enum dc_tower_insect_source_gate_path {
    DC_TOWER_INSECT_SOURCE_GATE_WAITING = 0,
    DC_TOWER_INSECT_SOURCE_GATE_ACTIVE = 1
} dc_tower_insect_source_gate_path;

typedef struct dc_tower_insect_source_gate {
    uint8_t slot;
    uint8_t child_count;
    dc_tower_insect_source_gate_path path;
} dc_tower_insect_source_gate;

typedef struct dc_tower_insect_source_countdown_wait {
    uint8_t slot;
    uint8_t timer;
    uint8_t decremented_timer;
} dc_tower_insect_source_countdown_wait;

typedef enum dc_small_soldier_source_gate_path {
    DC_SMALL_SOLDIER_SOURCE_GATE_WAITING = 0,
    DC_SMALL_SOLDIER_SOURCE_GATE_ACTIVE = 1
} dc_small_soldier_source_gate_path;

typedef struct dc_small_soldier_source_gate {
    uint8_t slot;
    uint8_t group_count;
    dc_small_soldier_source_gate_path path;
} dc_small_soldier_source_gate;

typedef enum dc_sorcerer_color_gate_path {
    DC_SORCERER_COLOR_GATE_WAITING = 0,
    DC_SORCERER_COLOR_GATE_ACTIVE = 1
} dc_sorcerer_color_gate_path;

typedef struct dc_sorcerer_color_gate {
    uint8_t slot;
    uint8_t shared_color;
    uint8_t cached_color;
    dc_sorcerer_color_gate_path path;
} dc_sorcerer_color_gate;

// Direct ROM routine 02:5808 moves one staged Sorcerer into the fixed orbit
// setup, assigns its initial direction/timer, increments the live count, and
// schedules the staggered deployment state at 02:586F.
typedef struct dc_sorcerer_deployment_initializer {
    uint8_t slot;
    uint8_t route_index;
    uint8_t flags_after;
    uint8_t collision_selector;
    uint8_t live_count_after;
    uint8_t initial_delay_low;
    uint8_t initial_delay_high;
} dc_sorcerer_deployment_initializer;

// Direct ROM routine 02:57F8 installs one staged Sorcerer's route index,
// collision byte, and first scheduler dispatch to the 02:5808 deployment
// initializer.
typedef struct dc_sorcerer_staging_initializer {
    uint8_t slot;
    uint8_t route_index;
} dc_sorcerer_staging_initializer;

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

// ROM routine 02:5159 dispatches the fixed Devil-Shooter's Button-I state.
// This narrow gate covers its normal-table, released-input, collision-free
// return before charge and launch behavior begins.
typedef struct dc_devil_shooter_release_delay_gate {
    uint8_t slot;
    uint8_t table_mode;
    uint8_t active_player;
    uint8_t button_input;
    uint8_t collision_state;
    uint8_t returns_idle;
} dc_devil_shooter_release_delay_gate;

int dc_classify_devil_shooter_release_delay_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_devil_shooter_release_delay_gate* gate);

// ROM routine 02:5223 services the fixed Devil Shooter immediately before
// charging begins. A nonnegative collision marker reaches the session-bit
// gate: bit $08 returns idle, while a clear bit schedules charge controller
// 02:5249. Collision-active reset handling remains assembly-owned.
typedef enum dc_devil_shooter_charge_entry_gate_path {
    DC_DEVIL_SHOOTER_CHARGE_ENTRY_COLLISION_ACTIVE = 0,
    DC_DEVIL_SHOOTER_CHARGE_ENTRY_IDLE = 1,
    DC_DEVIL_SHOOTER_CHARGE_ENTRY_SCHEDULE = 2
} dc_devil_shooter_charge_entry_gate_path;

typedef struct dc_devil_shooter_charge_entry_gate {
    uint8_t slot;
    uint8_t collision_state;
    uint8_t session_flags;
    dc_devil_shooter_charge_entry_gate_path path;
} dc_devil_shooter_charge_entry_gate;

int dc_classify_devil_shooter_charge_entry_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_devil_shooter_charge_entry_gate* gate);

// ROM routine 02:525F services the fixed Devil Shooter after the launched
// ball has left it. The observed steady path returns while the collision
// marker is nonnegative and session launch-request bit $53.1 is clear.
typedef struct dc_devil_shooter_post_launch_gate {
    uint8_t slot;
    uint8_t collision_state;
    uint8_t session_flags;
    uint8_t returns_idle;
} dc_devil_shooter_post_launch_gate;

int dc_classify_devil_shooter_post_launch_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_devil_shooter_post_launch_gate* gate);

// ROM routine 02:52E3 advances the fixed Devil-Shooter charge position,
// clamps a fully drawn shooter to its configured rest coordinate plus $27,
// derives the shared charge counter, and selects one of five appearances.
typedef struct dc_devil_shooter_charge_position {
    uint8_t slot;
    uint8_t vertical_low;
    uint8_t advanced_vertical_middle;
    uint8_t advanced_vertical_high;
    uint8_t vertical_middle;
    uint8_t vertical_high;
    uint8_t charge;
    uint8_t appearance;
    uint8_t subtract_overflow;
    uint8_t coordinate_capped;
} dc_devil_shooter_charge_position;

int dc_classify_devil_shooter_charge_position(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_devil_shooter_charge_position* position);

// ROM routine 02:533B retracts the fixed Devil-Shooter charge position.
// It subtracts two vertical units, derives the remaining charge, clamps a
// coordinate that crossed its rest position, and updates the corresponding
// charge appearance when the remaining charge is nonzero.
typedef struct dc_devil_shooter_charge_retraction {
    uint8_t slot;
    uint8_t vertical_low;
    uint8_t advanced_vertical_middle;
    uint8_t advanced_vertical_high;
    uint8_t vertical_middle;
    uint8_t vertical_high;
    uint8_t unclamped_charge;
    uint8_t charge;
    uint8_t appearance;
    uint8_t has_appearance_write;
    uint8_t high_subtract_overflow;
    uint8_t coordinate_clamped;
} dc_devil_shooter_charge_retraction;

int dc_classify_devil_shooter_charge_retraction(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_devil_shooter_charge_retraction* retraction);

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

typedef enum dc_skeleton_insect_right_post_trigger_path {
    DC_SKELETON_INSECT_RIGHT_POST_TRIGGER_IDLE = 0,
    DC_SKELETON_INSECT_RIGHT_POST_TRIGGER_SECONDARY_ACTIVE = 1,
    DC_SKELETON_INSECT_RIGHT_POST_TRIGGER_TOWER_ACTIVE = 2
} dc_skeleton_insect_right_post_trigger_path;

typedef struct dc_skeleton_insect_right_post_trigger {
    uint8_t slot;
    uint8_t secondary_state;
    uint8_t tower_state;
    dc_skeleton_insect_right_post_trigger_path path;
} dc_skeleton_insect_right_post_trigger;

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

// Direct ROM routine 03:63E1 restores the fixed Devil's-Coffin collision
// marker and appearance, then installs its 03:63F1 steady dispatch.
typedef struct dc_devils_coffin_initializer {
    uint8_t slot;
} dc_devils_coffin_initializer;

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

// Direct ROM routine 03:7B1F initializes hidden fixed slot $5C with its
// one-tick scheduler timer before the steady 03:7B27 controller begins.
typedef struct dc_fixed_scheduler_initializer {
    uint8_t slot;
} dc_fixed_scheduler_initializer;

// Direct ROM routine 03:7B6D initializes hidden fixed slot $5D with its
// marker mode and one-tick scheduler timer before 03:7B7A begins.
typedef struct dc_fixed_marker_initializer {
    uint8_t slot;
} dc_fixed_marker_initializer;

// Direct ROM routine 03:7CA7 initializes hidden fixed slot $5E by clearing
// its shared response and collision-scratch state before 03:7CAF begins.
typedef struct dc_fixed_response_initializer {
    uint8_t slot;
} dc_fixed_response_initializer;

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

typedef enum dc_dragon_egg_reset_gate_path {
    DC_DRAGON_EGG_RESET_GATE_IDLE = 0,
    DC_DRAGON_EGG_RESET_GATE_REQUESTED = 1
} dc_dragon_egg_reset_gate_path;

typedef struct dc_dragon_egg_reset_gate {
    uint8_t slot;
    uint8_t group_reset_flags;
    dc_dragon_egg_reset_gate_path path;
} dc_dragon_egg_reset_gate;

// Direct ROM routine 03:6294 initializes the fixed Giant Door before its
// collision gate begins at 03:62B8.
typedef struct dc_giant_door_initializer {
    uint8_t slot;
} dc_giant_door_initializer;

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

typedef enum dc_giant_face_crown_gate_path {
    DC_GIANT_FACE_CROWN_GATE_IDLE = 0,
    DC_GIANT_FACE_CROWN_GATE_HIT = 1
} dc_giant_face_crown_gate_path;

typedef struct dc_giant_face_crown_gate {
    uint8_t slot;
    uint8_t collision_state;
    dc_giant_face_crown_gate_path path;
} dc_giant_face_crown_gate;

typedef enum dc_giant_face_mouth_gate_path {
    DC_GIANT_FACE_MOUTH_GATE_IDLE = 0,
    DC_GIANT_FACE_MOUTH_GATE_OPEN = 1,
    DC_GIANT_FACE_MOUTH_GATE_HIT = 2
} dc_giant_face_mouth_gate_path;

typedef struct dc_giant_face_mouth_gate {
    uint8_t mouth_state;
    uint8_t slot;
    uint8_t collision_state;
    dc_giant_face_mouth_gate_path path;
} dc_giant_face_mouth_gate;

typedef enum dc_giant_face_helmet_controller_path {
    DC_GIANT_FACE_HELMET_CONTROLLER_COLLISION = 0,
    DC_GIANT_FACE_HELMET_CONTROLLER_WAITING = 1,
    DC_GIANT_FACE_HELMET_CONTROLLER_ADVANCE = 2,
    DC_GIANT_FACE_HELMET_CONTROLLER_RESET = 3
} dc_giant_face_helmet_controller_path;

typedef struct dc_giant_face_helmet_controller {
    uint8_t slot;
    uint8_t collision_state;
    uint8_t timer;
    uint8_t decremented_timer;
    uint8_t appearance;
    uint8_t advanced_appearance;
    dc_giant_face_helmet_controller_path path;
} dc_giant_face_helmet_controller;

typedef struct dc_giant_face_helmet_initializer {
    uint8_t slot;
    uint8_t selector;
} dc_giant_face_helmet_initializer;

// ROM routines 02:4C91 and 02:4A93 share the same collision gate and
// four-tick timer/appearance tail. The latter belongs to the four Wall
// Pockets, but has the same native state representation.
typedef dc_giant_face_helmet_controller dc_four_star_wall_pocket_controller;
typedef dc_giant_face_helmet_controller_path
    dc_four_star_wall_pocket_controller_path;

#define DC_FOUR_STAR_WALL_POCKET_CONTROLLER_COLLISION \
    DC_GIANT_FACE_HELMET_CONTROLLER_COLLISION
#define DC_FOUR_STAR_WALL_POCKET_CONTROLLER_WAITING \
    DC_GIANT_FACE_HELMET_CONTROLLER_WAITING
#define DC_FOUR_STAR_WALL_POCKET_CONTROLLER_ADVANCE \
    DC_GIANT_FACE_HELMET_CONTROLLER_ADVANCE
#define DC_FOUR_STAR_WALL_POCKET_CONTROLLER_RESET \
    DC_GIANT_FACE_HELMET_CONTROLLER_RESET

// ROM routine 02:5972 initializes the mirrored upper Wall-Pocket hand
// records. Its coroutine return installs the delayed live dispatch 02:5975.
typedef struct dc_upper_wall_pocket_hand_initializer {
    uint8_t slot;
    uint16_t live_dispatch;
} dc_upper_wall_pocket_hand_initializer;

// Native state prepared by fixed upper Wall-Pocket hand live setup 02:5975.
typedef struct dc_upper_wall_pocket_hand_live_setup {
    uint8_t slot;
    uint8_t collision_flags;
    uint8_t hidden_collision_flags;
    uint8_t timer_base;
    uint8_t masked_rng;
    uint8_t timer_low;
    uint8_t timer_high;
    uint16_t countdown_dispatch;
} dc_upper_wall_pocket_hand_live_setup;

typedef enum dc_upper_wall_pocket_hand_countdown_path {
    DC_UPPER_WALL_POCKET_HAND_COUNTDOWN_WAITING = 0,
    DC_UPPER_WALL_POCKET_HAND_COUNTDOWN_HANDOFF = 1
} dc_upper_wall_pocket_hand_countdown_path;

// ROM routine 02:5991 decrements the delayed hand timer. A zero low byte
// borrows from the high byte; reaching zero transfers to the opening state.
typedef struct dc_upper_wall_pocket_hand_countdown {
    uint8_t slot;
    uint8_t timer_low;
    uint8_t timer_high;
    uint8_t decremented_low;
    uint8_t decremented_high;
    uint8_t remaining_timer;
    dc_upper_wall_pocket_hand_countdown_path path;
} dc_upper_wall_pocket_hand_countdown;

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

int dc_classify_actor_slot_allocation(
    const uint8_t* work_ram,
    size_t work_ram_size,
    dc_actor_slot_allocation* allocation);

// Native state classification for the ROM routine 02:4DD9 activation gate.
// The Dragon's-Egg completion latch starts the opening sequence; otherwise
// session flag bit 1 selects the final animation or the ordinary dormant RTS.
int dc_classify_giant_skeleton_eyeball_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    dc_giant_skeleton_eyeball_gate* gate);

int dc_classify_giant_skeleton_eyeball_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_giant_skeleton_eyeball_initializer* initializer);

int dc_classify_giant_skeleton_eyeball_opening_wait(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_giant_skeleton_eyeball_opening_wait* wait);

int dc_classify_giant_skeleton_eyeball_steady_idle(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_giant_skeleton_eyeball_steady_idle* idle);

int dc_classify_giant_skeleton_eyeball_final_wait(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_giant_skeleton_eyeball_final_wait* wait);

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

// Native state classification for direct ROM routine 03:67B3. The three
// fixed Blob slots map their selected global color through the original
// appearance table, then return while their collision state is nonnegative.
int dc_classify_three_blobs_appearance_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_three_blobs_appearance_gate* gate);

int dc_classify_three_blobs_collision_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_three_blobs_collision_gate* gate);

// Native state classification for ROM routine 03:66A3. Direct no-zero and
// timer/x/y-expiry prefixes each resume through original assembly before their
// respective mixed movement, reset, or coroutine work.
int dc_classify_three_blobs_first_movement_prefix(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_three_blobs_first_movement_prefix* prefix);

// Native state classification for ROM routine 02:42F6. A nonzero live-egg
// count returns immediately; zero enters the group-completion award and reset
// sequence.
int dc_classify_dragon_egg_controller(
    const uint8_t* work_ram,
    size_t work_ram_size,
    dc_dragon_egg_controller* controller);

int dc_classify_dragon_egg_controller_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_dragon_egg_controller_initializer* initializer);

int dc_classify_dragon_egg_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_dragon_egg_initializer* initializer);

// Native state classification for ROM routine 02:445B. A clear shared
// Little-Skull feature latch returns immediately; pending bits continue into
// the Big-Star direction and scoring update.
int dc_classify_big_star_feature_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    dc_big_star_feature_gate* gate);

// Native state classification for the Big-Star pending-bit consumer at
// 02:570A. Only bits zero and one invoke the direction-selection helpers;
// when both are clear the routine clears the source byte and returns.
int dc_classify_big_star_pending_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    dc_big_star_pending_gate* gate);

int dc_classify_big_star_pending_helper(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_big_star_pending_helper* helper);

int dc_classify_big_star_pending_right_helper(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_big_star_pending_right_helper* helper);

int dc_classify_big_star_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_big_star_initializer* initializer);

int dc_classify_undead_head_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_undead_head_initializer* initializer);

int dc_classify_undead_head_selector_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_undead_head_selector_gate* gate);

int dc_classify_undead_head_waiting_parent(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_undead_head_waiting_parent* parent);

int dc_classify_side_worm_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_side_worm_initializer* initializer);

int dc_classify_side_worm_selector_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_side_worm_selector_gate* gate);

int dc_classify_side_worm_waiting_parent(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_side_worm_waiting_parent* parent);

int dc_classify_big_star_rotation_wait(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_big_star_rotation_wait* rotation);

// Native state classification for the Devil-Shooter side-companion dormant
// dispatches at 02:53DD and 02:5456. Slots $0A and $0D loop back into their
// active animation only while session bit 3 remains set.
int dc_classify_devil_shooter_companion_dormant(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_devil_shooter_companion_dormant* companion);

// Native state classification for the Devil-Shooter charge-position leaf at
// 02:5397. Fixed slot $0B derives its 16-bit vertical coordinate directly
// from the shared charge counter at $2D15.
int dc_classify_devil_shooter_companion_position(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_devil_shooter_companion_position* position);

// Native state classification for the Devil-Shooter charge-appearance leaf
// at 02:5408. Fixed slot $0C caches the shared charge counter, then advances
// its three-frame appearance only when the counter changes.
int dc_classify_devil_shooter_companion_appearance(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_devil_shooter_companion_appearance* appearance);

// Native state classification for the Devil-Shooter side-companion active
// timer leaves at 02:53E4 and 02:545D. Fixed slots $0A/$0D decrement their
// timers; expiry reloads eight ticks and advances their four-/five-frame
// appearances, clearing them at frames 4/5 respectively.
int dc_classify_devil_shooter_companion_active_timer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_devil_shooter_companion_active_timer* timer);

int dc_classify_walled_star_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_walled_star_initializer* initializer);

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

// Native state classification for the terminal 03:60F9 Tower prefix. It
// admits only the armed slot-$2D, final-gate state that falls through 03:612B
// to schedule 03:6136.
int dc_classify_tower_bonus_terminal(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_tower_bonus_terminal* terminal);

int dc_classify_tower_bonus_continuation(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_tower_bonus_continuation* continuation);

// Native state classification for ROM routine 03:73D8. The four queued
// Knights return while their indexed collision state is nonnegative; bit 7
// enters the assembly-backed four-hit score and completion path.
int dc_classify_knight_hit_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_knight_hit_gate* gate);

// Native state classification for the four fixed queued-Knight setup
// routines 03:734E/7353/735A/7361. Each maps its fixed slot $3A-$3D to
// the matching Giant-Door queue index before installing the common state.
int dc_classify_knight_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_knight_initializer* initializer);

// Native state classification for shared ROM initializer 03:64FA. The two
// fixed Side Spiders preserve side parameter zero/one in slots $28/$29.
int dc_classify_side_spider_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    uint8_t side_parameter,
    dc_side_spider_initializer* initializer);

// Native state classification for shared ROM initializer 03:646B. The two
// fixed Front Spiders preserve side parameter zero/one in slots $26/$27.
int dc_classify_front_spider_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    uint8_t side_parameter,
    dc_front_spider_initializer* initializer);

// Native state classification for ROM routine 03:7378. The queued Knights
// remain in their waiting parent while their indexed Giant-Door handshake bit
// is clear and the shared child collision gate is idle. Door activation and
// four-hit handling remain assembly-backed.
int dc_classify_knight_waiting_parent(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_knight_waiting_parent* parent);

// Native state classification for ROM routine 03:73A6. Released queued
// Knights use the same four slots and retain their Giant-Door queue index
// while a four-tick movement timer advances their phase.
int dc_classify_knight_movement_parent(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_knight_movement_parent* parent);

// Native state classification for ROM routine 03:65A4. The shared Front-
// and Side-Spider appearance helper counts down four ticks, advances its
// phase through zero to three, then resets it. Only fixed Spider slots
// $26-$29 are accepted.
int dc_classify_spider_appearance(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_spider_appearance* appearance);

// Native state classification for direct ROM branch target 03:6054. Every
// actor whose cadence/phase resolves inside its shared 32-byte frame table
// selects a frame, then decrements a six-tick animation timer and advances the
// phase on expiry.
int dc_classify_tower_insect_animation(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_tower_insect_animation* animation);

// Native state classification for direct ROM entry 02:5F5F. The fixed
// Tower-insect source returns while its live child count is nonzero; zero
// continues into the assembly-owned group-clear award path.
int dc_classify_tower_insect_source_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_tower_insect_source_gate* gate);

int dc_classify_tower_insect_source_countdown_wait(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_tower_insect_source_countdown_wait* wait);

// Native state classification for ROM routines 03:7D85 and 03:7E23. The
// fixed Small-Soldier sources return while their side-selected group count is
// nonzero; zero continues into the assembly-owned group-completion path.
int dc_classify_small_soldier_source_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_small_soldier_source_gate* gate);

// Native state classification for direct ROM branch target 02:57E9. The
// hidden Sorcerer controller returns when its shared color matches its cached
// color; a mismatch writes the new color and enters the assembly-owned helper.
int dc_classify_sorcerer_color_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_sorcerer_color_gate* gate);

int dc_classify_sorcerer_deployment_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_sorcerer_deployment_initializer* initializer);

int dc_classify_sorcerer_staging_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    uint8_t route_index,
    dc_sorcerer_staging_initializer* initializer);

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

// Native state classification for ROM routine 02:5590. The right Skeleton
// Insect returns after its opening animation while both shared follow-up gates
// are clear. Either active gate enters an assembly-owned collision/animation
// continuation that is deliberately left outside the admitted native path.
int dc_classify_skeleton_insect_right_post_trigger(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_skeleton_insect_right_post_trigger* post_trigger);

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

int dc_classify_devils_coffin_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_devils_coffin_initializer* initializer);

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

int dc_classify_fixed_scheduler_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_fixed_scheduler_initializer* initializer);

int dc_classify_fixed_marker_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_fixed_marker_initializer* initializer);

int dc_classify_fixed_response_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_fixed_response_initializer* initializer);

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

// Native state classification for persistent dispatch 02:4387. A destroyed
// fixed egg remains dormant while group-reset flag $2CF7.0 is clear; a set
// flag enters the assembly-owned actor reset and initializer path.
int dc_classify_dragon_egg_reset_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_dragon_egg_reset_gate* gate);

int dc_classify_giant_door_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_giant_door_initializer* initializer);

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

// Native state classification for ROM routine 03:76FA. The Giant-Face crown
// returns while its indexed collision state is nonnegative; bit 7 enters the
// 300-point score, impact, and audio path.
int dc_classify_giant_face_crown_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_giant_face_crown_gate* gate);

// Native state classification for ROM routine 03:7726. A nonzero mouth
// state enters the open-mouth capture path. While closed, the Giant Face
// returns when its indexed collision state is nonnegative; bit 7 enters the
// shared 300-point score, impact, and audio path.
int dc_classify_giant_face_mouth_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_giant_face_mouth_gate* gate);

// Native state classification for ROM routine 02:4C91. A collision enters
// the assembly-backed Giant-Face activation path. Otherwise each mirrored
// helmet channel decrements its four-tick timer, advances appearance 0-3 on
// expiry, then resets appearance after the fourth frame.
int dc_classify_giant_face_helmet_controller(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_giant_face_helmet_controller* controller);

// Shared ROM setup 02:4C78 installs the fixed Giant-Face helmet channels.
// Collision-marked entries continue through the original active path.
int dc_classify_giant_face_helmet_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    uint8_t selector,
    dc_giant_face_helmet_initializer* initializer);

// Native state classification for ROM routine 02:4A93. Collision access
// remains assembly-backed; collision-free Wall-Pocket calls use the shared
// four-tick timer and appearance controller.
int dc_classify_four_star_wall_pocket_controller(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_four_star_wall_pocket_controller* controller);

// ROM routine 02:4B3F is the Walled-Star access pocket's collision gate and
// shares the same four-tick timer/appearance tail. Its collision/bonus-stage
// transition remains a distinct assembly-owned path.
typedef dc_giant_face_helmet_controller dc_walled_star_access_controller;
typedef dc_giant_face_helmet_controller_path
    dc_walled_star_access_controller_path;

// Direct ROM routine 02:4A44 initializes the fixed Walled-Star access
// pocket before its collision and bonus-stage controller begins at 02:4B3F.
typedef struct dc_walled_star_access_initializer {
    uint8_t slot;
} dc_walled_star_access_initializer;

#define DC_WALLED_STAR_ACCESS_CONTROLLER_COLLISION \
    DC_GIANT_FACE_HELMET_CONTROLLER_COLLISION
#define DC_WALLED_STAR_ACCESS_CONTROLLER_WAITING \
    DC_GIANT_FACE_HELMET_CONTROLLER_WAITING
#define DC_WALLED_STAR_ACCESS_CONTROLLER_ADVANCE \
    DC_GIANT_FACE_HELMET_CONTROLLER_ADVANCE
#define DC_WALLED_STAR_ACCESS_CONTROLLER_RESET \
    DC_GIANT_FACE_HELMET_CONTROLLER_RESET

int dc_classify_walled_star_access_controller(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_walled_star_access_controller* controller);

int dc_classify_walled_star_access_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_walled_star_access_initializer* initializer);

// ROM routines 02:4BD5 and 02:4C0A are the lower-left and upper-right
// Screen-2 Side-Star collision gates.  Their collision-free paths share the
// same four-tick timer/appearance tail; scoring, transformation, and bonus
// entrance paths remain separately assembly-backed.
typedef dc_giant_face_helmet_controller dc_side_star_access_controller;
typedef dc_giant_face_helmet_controller_path dc_side_star_access_controller_path;

// Direct ROM routines 02:4A54 and 02:4A4C initialize the lower-left and
// upper-right Side-Star access surfaces before their respective controllers.
typedef struct dc_side_star_initializer {
    uint8_t slot;
    uint8_t appearance;
    uint16_t first_dispatch;
    uint16_t steady_dispatch;
} dc_side_star_initializer;

#define DC_SIDE_STAR_ACCESS_CONTROLLER_COLLISION \
    DC_GIANT_FACE_HELMET_CONTROLLER_COLLISION
#define DC_SIDE_STAR_ACCESS_CONTROLLER_WAITING \
    DC_GIANT_FACE_HELMET_CONTROLLER_WAITING
#define DC_SIDE_STAR_ACCESS_CONTROLLER_ADVANCE \
    DC_GIANT_FACE_HELMET_CONTROLLER_ADVANCE
#define DC_SIDE_STAR_ACCESS_CONTROLLER_RESET \
    DC_GIANT_FACE_HELMET_CONTROLLER_RESET

int dc_classify_side_star_access_controller(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_side_star_access_controller* controller);

int dc_classify_side_star_initializer(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_side_star_initializer* initializer);

// ROM routine 02:4AEB is the scheduled Four-Star Wall-Pocket animation
// continuation.  Appearance six begins the scene-transition path, which
// remains assembly-owned; lower appearances run the four-tick animation.
typedef enum dc_four_star_wall_pocket_animation_path {
    DC_FOUR_STAR_WALL_POCKET_ANIMATION_WAITING = 0,
    DC_FOUR_STAR_WALL_POCKET_ANIMATION_ADVANCE = 1,
    DC_FOUR_STAR_WALL_POCKET_ANIMATION_TRANSITION = 2
} dc_four_star_wall_pocket_animation_path;

typedef struct dc_four_star_wall_pocket_animation {
    uint8_t slot;
    uint8_t timer;
    uint8_t decremented_timer;
    uint8_t appearance;
    uint8_t advanced_appearance;
    dc_four_star_wall_pocket_animation_path path;
} dc_four_star_wall_pocket_animation;

// Native state classification for the Four-Star scheduled animation at
// 02:4AEB.  The transition path is reported distinctly so its scene and
// scheduler behavior can safely remain in assembly until traced.
int dc_classify_four_star_wall_pocket_animation(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_four_star_wall_pocket_animation* animation);

// ROM routine 02:4C33 is the upper-right Side-Star's six-step bonus-entrance
// animation. It has the same timer/appearance shape as 02:4AEB, but only
// fixed slot $12 is valid and appearance six remains a scene transition.
typedef dc_four_star_wall_pocket_animation_path
    dc_side_star_bonus_entrance_animation_path;
typedef dc_four_star_wall_pocket_animation dc_side_star_bonus_entrance_animation;

int dc_classify_side_star_bonus_entrance_animation(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_side_star_bonus_entrance_animation* animation);

// Native classification for the two fixed upper Wall-Pocket hand initializers
// at ROM routine 02:5972.
int dc_prepare_upper_wall_pocket_hand_initializer(
    uint8_t slot,
    dc_upper_wall_pocket_hand_initializer* initializer);

// Native classification for the fixed upper Wall-Pocket hand live setup at
// ROM routine 02:5975. It hides the hand collision surface, masks the RNG
// high-byte result into the low timer byte, and selects countdown 02:5991.
int dc_prepare_upper_wall_pocket_hand_live_setup(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    uint8_t rng_result,
    dc_upper_wall_pocket_hand_live_setup* setup);

// Native classification for the fixed upper Wall-Pocket hand countdown at
// ROM routine 02:5991. The zero-timer coroutine handoff is reported to the
// adapter so it can remain assembly-backed until separately traced.
int dc_classify_upper_wall_pocket_hand_countdown(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_upper_wall_pocket_hand_countdown* countdown);

#ifdef __cplusplus
} // extern "C"
#endif
