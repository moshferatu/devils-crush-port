// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The emulator's own instruction boundary, reachable without an emulator.
//
// This is the thinnest shim in the series, and it is thin for a reason that
// is worth stating. Every other reference device had to be *driven*: a clock
// had to advance, a timer had to catch up, a colour encoder had to be handed
// a scanline. This one is asked. The boundary holds two latched values and
// otherwise computes, so the whole of it is reachable by writing four inputs
// and reading one answer -- which is why the test behind this shim is an
// enumeration rather than a stimulus.
//
// The rule it exposes is the machine's own. The observation patch lifts the
// sampling out of `LastCycle` and the vector selection out of `RunSub` into
// two named entry points that both the machine and this shim call, so there
// is no second copy of the rule to drift.
//
// This exists for the tests and dies with the emulator. It claims the
// emulator's own processor -- specifically its request register, its mask,
// its status register and the two latched values -- so only one reference
// device may exist at a time, none may exist while a real machine is
// running, and it must not share a process with a live reference timer,
// which owns the same registers.
typedef struct dc_beetle_reference_boundary dc_beetle_reference_boundary;

// What the boundary decided, with the emulator's own values.
enum {
    DC_BEETLE_REFERENCE_BOUNDARY_INSTRUCTION = 0,
    DC_BEETLE_REFERENCE_BOUNDARY_RESET = 1,
    DC_BEETLE_REFERENCE_BOUNDARY_TIMER = 2,
    DC_BEETLE_REFERENCE_BOUNDARY_IRQ1 = 3,
    DC_BEETLE_REFERENCE_BOUNDARY_IRQ2 = 4
};

// The inputs and the latch, read back out. The lines are included even
// though a test wrote them, because a rule that quietly changed one of its
// own inputs would otherwise be invisible.
typedef struct dc_beetle_reference_boundary_observation {
    uint32_t irq_low;
    uint8_t irq_mask;
    int32_t irq_sample;
    int32_t i_flag_sample;
} dc_beetle_reference_boundary_observation;

// Creating one while another lives returns null.
dc_beetle_reference_boundary* dc_beetle_reference_boundary_create(void);
void dc_beetle_reference_boundary_destroy(
    dc_beetle_reference_boundary* boundary);

// `HuC6280::Reset`, which is the only thing that ever puts the reset line
// into the latch. It resets the whole processor, which is more than this
// device but no less: nothing it touches outside the boundary is read here.
void dc_beetle_reference_boundary_reset(
    dc_beetle_reference_boundary* boundary);

// The four inputs. The lines and the mask are the interrupt controller's,
// and are written directly rather than through the register file because a
// test enumerating every combination has no interest in how a machine would
// have arrived at one.
void dc_beetle_reference_boundary_set_lines(
    dc_beetle_reference_boundary* boundary,
    uint32_t requests,
    uint8_t mask);
void dc_beetle_reference_boundary_set_i_flag(
    dc_beetle_reference_boundary* boundary,
    int set);
// The latch, written directly. Most of what this reaches is reachable by
// sampling instead; the reset line is not, and neither is any state a
// machine could only arrive at through a savestate.
void dc_beetle_reference_boundary_set_sample(
    dc_beetle_reference_boundary* boundary,
    int32_t irq_sample,
    int32_t i_flag_sample);

// `LastCycle` without the cycle, and the top of `RunSub` without the taking.
void dc_beetle_reference_boundary_sample(
    dc_beetle_reference_boundary* boundary);
int dc_beetle_reference_boundary_select(
    dc_beetle_reference_boundary* boundary);

// Where each decision fetches from, as the emulator's own table says.
uint32_t dc_beetle_reference_boundary_vector(int decision);

// The reset line leaving the latch, which is what `RunSub` does once it has
// taken the vector.
void dc_beetle_reference_boundary_acknowledge_reset(
    dc_beetle_reference_boundary* boundary);

void dc_beetle_reference_boundary_observe(
    dc_beetle_reference_boundary* boundary,
    dc_beetle_reference_boundary_observation* out);

#ifdef __cplusplus
}
#endif
