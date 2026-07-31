// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    DC_CACHED_POSITION_BYTE_COUNT = 6,
    DC_INTEGRATED_POSITION_BYTE_COUNT = 6,
    DC_UNSIGNED_DIVIDE_STEP_COUNT = 16,
    DC_UNSIGNED_MULTIPLY_STEP_COUNT = 8,
};

typedef struct dc_position_integration {
    uint16_t cache_work_ram_offsets[DC_CACHED_POSITION_BYTE_COUNT];
    uint8_t cached_values[DC_CACHED_POSITION_BYTE_COUNT];
    uint16_t position_work_ram_offsets[DC_INTEGRATED_POSITION_BYTE_COUNT];
    uint8_t position_values[DC_INTEGRATED_POSITION_BYTE_COUNT];
    uint8_t cleared_motion_flags;
    uint8_t final_motion_flags;
    uint8_t first_increment_negative;
    uint8_t second_increment_negative;
    uint8_t integer_position_unchanged;
} dc_position_integration;

typedef struct dc_cached_position_restore {
    uint16_t work_ram_offsets[DC_CACHED_POSITION_BYTE_COUNT];
    uint8_t values[DC_CACHED_POSITION_BYTE_COUNT];
} dc_cached_position_restore;

typedef struct dc_motion_damping {
    uint16_t first_original;
    uint16_t second_original;
    uint16_t first_damped;
    uint16_t second_damped;
    uint16_t second_accelerated;
    uint8_t first_divisor;
    uint8_t second_divisor;
    uint8_t second_acceleration;
    uint8_t first_component_nonzero;
    uint8_t second_component_nonzero;
} dc_motion_damping;

typedef struct dc_motion_rotation {
    uint16_t first_input;
    uint16_t second_input;
    uint16_t cosine;
    uint16_t sine;
    uint16_t first_output;
    uint16_t second_output;
} dc_motion_rotation;

typedef struct dc_signed_word_negation {
    uint16_t input;
    uint16_t result;
    uint8_t complemented_high;
    uint8_t increments_high;
} dc_signed_word_negation;

typedef struct dc_unsigned_divide_step {
    uint16_t quotient_after_roll;
    uint8_t remainder_after_shift;
    uint8_t remainder_after_subtract;
    uint8_t subtracts_divisor;
} dc_unsigned_divide_step;

typedef struct dc_signed_division {
    uint16_t dividend_input;
    uint16_t dividend_magnitude;
    uint16_t quotient_magnitude;
    uint16_t result;
    uint8_t divisor_input;
    uint8_t divisor_magnitude;
    uint8_t remainder;
    uint8_t dividend_negative;
    uint8_t divisor_negative;
    uint8_t result_negative;
    uint8_t initial_carry;
    uint8_t unsigned_initial_carry;
    dc_unsigned_divide_step steps[DC_UNSIGNED_DIVIDE_STEP_COUNT];
} dc_signed_division;

typedef struct dc_unsigned_multiply_step {
    uint8_t low_after_shift;
    uint8_t high_after_shift;
    uint8_t multiplier_after_shift;
    uint8_t adds_multiplicand;
    uint8_t low_after_add;
    uint8_t high_after_add;
    uint8_t multiplier_after_add;
    uint8_t add_carry;
} dc_unsigned_multiply_step;

typedef struct dc_unsigned_multiply_round {
    uint16_t accumulator;
    uint16_t multiplicand;
    uint8_t multiplier;
    uint16_t result;
    uint8_t multiplier_after;
    uint8_t add_count;
    dc_unsigned_multiply_step steps[DC_UNSIGNED_MULTIPLY_STEP_COUNT];
} dc_unsigned_multiply_round;

typedef struct dc_unsigned_byte_multiply_step {
    uint8_t low_after_shift;
    uint8_t high_after_shift;
    uint8_t adds_multiplicand;
    uint8_t low_after_add;
    uint8_t high_after_add;
    uint8_t add_carry;
} dc_unsigned_byte_multiply_step;

typedef struct dc_unsigned_byte_multiply {
    uint8_t multiplicand;
    uint8_t multiplier;
    uint8_t result_low;
    uint8_t result_high;
    uint8_t add_count;
    uint8_t carry_count;
    dc_unsigned_byte_multiply_step steps[DC_UNSIGNED_MULTIPLY_STEP_COUNT];
} dc_unsigned_byte_multiply;

typedef struct dc_signed_fixed_multiply {
    uint16_t multiplicand_input;
    uint16_t multiplier_input;
    uint16_t multiplicand_magnitude;
    uint16_t multiplier_magnitude;
    uint16_t magnitude;
    uint16_t result;
    uint8_t sign_xor;
    uint8_t negates_multiplicand;
    uint8_t negates_multiplier;
    uint8_t runs_high_round;
    dc_unsigned_multiply_round high_round;
    dc_unsigned_multiply_round low_round;
} dc_signed_fixed_multiply;

typedef struct dc_flipper_projection {
    uint16_t first_input;
    uint16_t second_input;
    uint16_t first_coefficient;
    uint16_t second_coefficient;
    uint16_t first_direct_product;
    uint16_t second_cross_product;
    uint16_t first_cross_product;
    uint16_t second_direct_product;
    uint16_t first_output;
    uint16_t second_output;
} dc_flipper_projection;

typedef struct dc_flipper_radius_projection {
    uint16_t radius;
    uint16_t coefficient;
    uint16_t output;
} dc_flipper_radius_projection;

typedef struct dc_flipper_angle_projection {
    uint8_t target_flags;
    uint8_t source_angle;
    uint8_t target_angle;
    uint8_t angle_difference;
    uint16_t scale;
    uint16_t fixed_difference;
    uint16_t product;
    uint16_t output;
    uint8_t uses_wrapped_difference;
    uint8_t returns_zero;
    uint8_t negates_product;
    uint8_t increments_high;
} dc_flipper_angle_projection;

typedef struct dc_collision_motion_adjustment {
    uint16_t input;
    uint16_t output;
    uint8_t mode;
    uint8_t effective_mode;
    uint8_t uses_quarter_scale;
    uint8_t negative_input;
    uint8_t clamps_high;
} dc_collision_motion_adjustment;

// Native equivalent of ROM routine 00:F1B4. It caches and integrates the two
// signed 16.8 position components for one indexed physics slot using the
// signed 8.8 increments at zero-page B9:BA and BB:BC. The function is pure:
// callers commit the returned writes.
int dc_integrate_position(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_position_integration* integration);

// Native equivalent of ROM routine 01:DD09. Each nonzero signed 8.8 motion
// component is reduced by its signed, truncating quotient using divisors 8C
// and 8D. The unsigned low-byte term at 8B is then added to the second
// component. The function is pure: callers commit the returned writes.
int dc_damp_and_accelerate_motion(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_motion_damping* damping);

// Native equivalent of ROM routine 01:D1A2. It rotates two signed 8.8
// motion components by signed 8.8 cosine/sine coefficients using the
// original sign-magnitude multiply and fractional truncation. The function
// is pure: callers commit the returned writes.
int dc_rotate_motion(
    const uint8_t* work_ram,
    size_t work_ram_size,
    dc_motion_rotation* rotation);

// Native equivalent of ROM routine 00:E837. It negates the signed 16-bit
// scratch word at zero-page 2F:30 using the original complement/increment
// sequence. The function is pure: callers commit the returned writes.
int dc_negate_signed_word(
    const uint8_t* work_ram,
    size_t work_ram_size,
    dc_signed_word_negation* negation);

// Native equivalent of ROM routine 00:E905. It normalizes the signed
// divisor at 2D and signed dividend at 2F:30, performs the original
// carry-sensitive 16-round unsigned division, and restores the quotient
// sign. The function is pure: callers commit the returned scratch writes.
int dc_divide_signed_word(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t carry_in,
    dc_signed_division* division);

// Native equivalent of ROM routine 00:E624. It performs eight unsigned
// shift/add rounds over accumulator 42:43, multiplier 30, and multiplicand
// 2D:2E. The function is pure: callers commit the returned writes.
int dc_multiply_unsigned_round(
    const uint8_t* work_ram,
    size_t work_ram_size,
    dc_unsigned_multiply_round* round);

// Native equivalent of ROM routine 00:E569. It multiplies the unsigned
// bytes at zero-page $2F and $30 through the original eight shift/add
// rounds. The function is pure: callers commit the returned product.
int dc_multiply_unsigned_bytes(
    const uint8_t* work_ram,
    size_t work_ram_size,
    dc_unsigned_byte_multiply* multiply);

// Native equivalent of ROM routine 00:E65C. It normalizes two signed 8.8
// words at 2D:2E and 2F:30, composes one or two E624 unsigned rounds, shifts
// the product back to 8.8, and restores the combined sign. The function is
// pure: callers commit the returned scratch writes.
int dc_multiply_signed_fixed(
    const uint8_t* work_ram,
    size_t work_ram_size,
    dc_signed_fixed_multiply* multiply);

// Native equivalent of ROM routine 01:D09E. It projects the indexed pair at
// 3400:3480 and 3500:3580 through coefficients 7D:7E and 7F:80, retaining
// the original signed 8.8 truncation at each multiply. The function is pure:
// callers commit the returned scratch writes.
int dc_project_flipper_vectors(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_flipper_projection* projection);

// Native equivalent of ROM routine 01:D612. It scales the signed 8.8
// flipper radius at 31:32 by the signed 8.8 coefficient at 7D:7E and
// returns the projected component written to C2:C3. The function is pure:
// callers commit the returned scratch writes.
int dc_project_flipper_radius(
    const uint8_t* work_ram,
    size_t work_ram_size,
    dc_flipper_radius_projection* projection);

// Native equivalent of ROM routine 01:D62E. It derives the directional
// flipper-angle delta selected by 3C80,Y, converts that delta to signed 8.8,
// scales it by 9E:9F, and returns the component written to 31:32. The
// function is pure: callers commit the returned scratch writes.
int dc_project_flipper_angle(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t source_slot,
    uint8_t target_slot,
    dc_flipper_angle_projection* projection);

// Native equivalent of ROM routine 01:CFD8. It applies the collision-mode
// velocity adjustment selected by 78 bits 5-6: ordinary signed clamp,
// 3/16 or 1/4 scaling, 1/2 scaling, or 1/16 scaling. The function is pure:
// callers commit the returned scratch and motion writes.
int dc_adjust_collision_motion(
    const uint8_t* work_ram,
    size_t work_ram_size,
    dc_collision_motion_adjustment* adjustment);

// Native equivalent of ROM routine 00:F241. It restores one indexed
// physics slot from the six-byte pre-integration cache at work-RAM offsets
// 0085-008A. The function is pure: callers commit the returned writes.
int dc_restore_cached_position(
    const uint8_t* work_ram,
    size_t work_ram_size,
    uint8_t slot,
    dc_cached_position_restore* restore);

#ifdef __cplusplus
} // extern "C"
#endif
