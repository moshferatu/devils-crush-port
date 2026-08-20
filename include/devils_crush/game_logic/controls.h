// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dc_flipper_input_gate {
    uint8_t global_input;
    uint8_t piece_mask;
    uint8_t matched_input;
} dc_flipper_input_gate;

typedef struct dc_flipper_activation {
    uint8_t current_slot;
    uint8_t coroutine_countdown;
    uint8_t dispatch_low;
    uint8_t dispatch_high;
    uint8_t compare_carry;
    uint8_t supported;
} dc_flipper_activation;

typedef struct dc_flipper_phase_step {
    uint8_t countdown;
    uint8_t reload;
    uint8_t orientation;
    uint8_t phase;
    uint8_t phase_next;
    uint8_t decrements_phase;
    uint8_t compare_carry;
    uint8_t supported;
} dc_flipper_phase_step;

typedef struct dc_flipper_geometry {
    uint8_t state;
    uint8_t table_index;
    uint8_t first_low;
    uint8_t first_high;
    uint8_t second_low;
    uint8_t second_high;
} dc_flipper_geometry;

typedef struct dc_coroutine_schedule {
    uint8_t stacked_return_low;
    uint8_t stacked_return_high;
    uint8_t countdown;
    uint8_t countdown_after;
    uint8_t dispatch_low;
    uint8_t dispatch_high;
    uint8_t decrements_countdown;
    uint8_t installs_dispatch;
    uint8_t dispatch_low_wrapped;
} dc_coroutine_schedule;

typedef enum dc_nudge_gate_branch {
    DC_NUDGE_GATE_DEFER = 0,
    DC_NUDGE_GATE_SESSION_RETURN = 1,
    DC_NUDGE_GATE_IDLE_RETURN = 2,
    DC_NUDGE_GATE_TIMER_RELOAD_RETURN = 3,
    DC_NUDGE_GATE_ACTIVE_START = 4,
    DC_NUDGE_GATE_ACTIVE_FINISH = 5
} dc_nudge_gate_branch;

typedef struct dc_nudge_input_gate {
    uint8_t session_flags;
    uint8_t session_mask;
    uint8_t tilt_lock;
    uint8_t timer;
    uint8_t timer_after;
    uint8_t nudge_phase;
    uint8_t active_player;
    uint8_t button_input;
    uint8_t matched_input;
    dc_nudge_gate_branch branch;
} dc_nudge_input_gate;

typedef struct dc_session_input_gate {
    uint8_t session_flags;
    uint8_t active_player;
    uint8_t button_input;
    uint8_t run_input;
    uint8_t returns_idle;
} dc_session_input_gate;

typedef enum dc_ball_drain_threshold_path {
    DC_BALL_DRAIN_THRESHOLD_NORMAL = 0,
    DC_BALL_DRAIN_THRESHOLD_DRAINED = 1,
    DC_BALL_DRAIN_THRESHOLD_DEFER = 2
} dc_ball_drain_threshold_path;

// Direct ROM routine 01:DD6E compares the indexed ball Y coordinate against
// the normal-table drain threshold $02D0.  The alternate session-state
// threshold continues through a separate controller and remains assembly
// owned; the ordinary state returns locally with carry clear or set.
typedef struct dc_ball_drain_threshold {
    uint8_t slot;
    uint8_t session_state;
    uint8_t position_low;
    uint8_t position_high;
    uint8_t difference_low;
    uint8_t difference_high;
    dc_ball_drain_threshold_path path;
} dc_ball_drain_threshold;

typedef enum dc_bank7_mode_gate_path {
    DC_BANK7_MODE_GATE_INACTIVE_MODE = 0,
    DC_BANK7_MODE_GATE_BELOW_RANGE = 1,
    DC_BANK7_MODE_GATE_ABOVE_RANGE = 2,
    DC_BANK7_MODE_GATE_FORM_MISMATCH = 3,
    DC_BANK7_MODE_GATE_ALREADY_ARMED = 4,
    DC_BANK7_MODE_GATE_ARM = 5
} dc_bank7_mode_gate_path;

// Export 07:4003 jumps to a compact scene-mode gate at 07:4634. It arms
// session flag $53.2 and a 100-tick counter only for mode 9, values $3C-$45,
// persisted form 6, and a clear $CB.0 latch. Every other completed path either
// clears that latch or returns with it already set.
typedef struct dc_bank7_mode_gate {
    uint8_t mode;
    uint8_t range_value;
    uint8_t persisted_form;
    uint8_t latch_before;
    uint8_t latch_after;
    uint8_t session_flags_before;
    uint8_t session_flags_after;
    uint8_t counter_after;
    dc_bank7_mode_gate_path path;
} dc_bank7_mode_gate;

// Native equivalent of the input test at ROM routine 01:D466. It intersects
// the scheduler's held-input byte with the indexed flipper/control-piece
// mask. The function is pure; callers choose the no-input exit or continue
// into the response path from matched_input.
int dc_evaluate_flipper_input_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_flipper_input_gate* gate);

// Native classification of D466's common non-current-slot activation path.
// The supported branch marks the piece active and installs coroutine D48E;
// current-slot and nonzero coroutine-countdown variants are deferred.
int dc_prepare_flipper_activation(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    const dc_flipper_input_gate* gate,
    dc_flipper_activation* activation);

// Native equivalent of D500's timer-expiry phase step. Non-expiring timers
// and endpoint phases remain assembly-owned until their surrounding
// controller paths are ported.
int dc_advance_flipper_phase(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_flipper_phase_step* step);

// Native equivalent of D081's phase-indexed geometry lookup. The caller
// supplies the 128-byte ROM table beginning at 01:D2AE; the function keeps
// user-owned ROM data outside the native module while deriving both vectors.
int dc_lookup_flipper_geometry(
    uint8_t state,
    const uint8_t* geometry_table,
    size_t geometry_table_size,
    dc_flipper_geometry* geometry);

// Native equivalent of shared coroutine scheduler C5C9. The routine consumes
// its JSR return from the emulated stack, optionally decrements the indexed
// delay, and installs the following instruction as the slot's next dispatch
// address when the delay is zero.
int dc_schedule_coroutine(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    uint8_t stack_pointer,
    dc_coroutine_schedule* schedule);

// Native classification of the bounded session, ordinary idle, and
// timer-reload exits and the ordinary active-nudge start/finish pair at ROM
// routine 01:C3CB. Tilt and other session paths are reported as
// DC_NUDGE_GATE_DEFER for assembly fallback.
int dc_evaluate_nudge_input_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    dc_nudge_input_gate* gate);

// Native classification of the ordinary no-Run-input return at ROM routine
// 01:C49E. Pause, high-score, and session-initialization paths are deferred.
int dc_evaluate_session_input_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    dc_session_input_gate* gate);

// Native classification of the ordinary drain threshold at 01:DD6E.  A
// nonzero session-state byte selects the original alternate threshold path
// and is reported as DEFER for assembly fallback.
int dc_classify_ball_drain_threshold(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_ball_drain_threshold* threshold);

// Classifies every return path through exported ROM routine 07:4003/4634.
int dc_classify_bank7_mode_gate(
    const uint8_t* work_ram,
    size_t work_ram_size,
    dc_bank7_mode_gate* gate);

#ifdef __cplusplus
} // extern "C"
#endif
