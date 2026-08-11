// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The emulator's own timer and interrupt controller, reachable without an
// emulator.
//
// Unlike the sound chip and the video controller, this device is not an
// object at all: it is a handful of fields inside `HuC6280` and four public
// methods the machine's I/O page handler calls from outside. There is
// nothing to lift out. So this shim supplies the smallest machine those
// fields live in -- the emulator's own processor, initialised but never
// executed -- and drives it by stealing master cycles, exactly as the
// colour encoder's shim does.
//
// That works here for a stronger reason than it did there. The timer's only
// relationship with time is `timer_lastts`, a phase against the processor's
// clock, and the processor's clock advances whether or not it is executing.
// A stolen cycle and an executed instruction are the same thing to this
// device.
//
// Two consequences are worth stating plainly. Stealing cycles runs the
// processor's own `HappySync` whenever the timer's divider comes due, so
// the emulator catches the timer up at the moments a real machine would,
// possibly several times inside one call -- which is what makes comparing
// it against a single catch-up worth doing. And the interrupt mask comes up
// closed: only taking the reset vector opens it, and nothing here executes,
// so `dc_beetle_reference_irq_acknowledge_reset` stands in for that.
//
// This exists for the tests and dies with the emulator. It claims the
// emulator's own processor, so only one reference device may exist at a
// time, none may exist while a real machine is running, and it must not
// share a process with a live reference colour encoder.
typedef struct dc_beetle_reference_irq dc_beetle_reference_irq;

// The device's private state, read out. A differential test that could only
// compare register reads would be unable to see the divider's phase, which
// is the one piece of this device that no register exposes and the one the
// census is most sensitive to.
typedef struct dc_beetle_reference_irq_observation {
    uint32_t timestamp;
    int32_t timer_lastts;
    int32_t timer_div;
    int32_t timer_value;
    int32_t timer_load;
    uint8_t timer_status;
    int timer_inreload;
    uint32_t irq_low;
    uint8_t irq_mask;
    // The processor's latched copy of the masked pending set. Not part of
    // the device -- it is compared to nothing -- but reading it out is how
    // a test tells a controller fault from a sampling one.
    int32_t irq_sample;
    // `min(timer_div, next_user_event)`. With the user event armed far
    // away, this is the timer's divider and nothing else.
    int32_t next_event;
} dc_beetle_reference_irq_observation;

// Creating one while another lives returns null.
dc_beetle_reference_irq* dc_beetle_reference_irq_create(void);
void dc_beetle_reference_irq_destroy(dc_beetle_reference_irq* irq);

// `HuC6280::Power` and `HuC6280::Reset`, both of which the machine calls.
// Neither rebases the processor's clock, so a test that wants the timer's
// phase to start at zero asks for that separately.
void dc_beetle_reference_irq_power(dc_beetle_reference_irq* irq);
void dc_beetle_reference_irq_reset(dc_beetle_reference_irq* irq);
void dc_beetle_reference_irq_reset_timestamp(
    dc_beetle_reference_irq* irq,
    uint32_t base);

// What `RunSub` does to the controller when it takes the reset vector.
void dc_beetle_reference_irq_acknowledge_reset(dc_beetle_reference_irq* irq);

// A device asserting or releasing a line, in the processor's bit order:
// 0x001 is IRQ2, 0x002 is IRQ1, 0x004 is the timer, 0x020 is reset.
void dc_beetle_reference_irq_begin(
    dc_beetle_reference_irq* irq,
    uint32_t lines);
void dc_beetle_reference_irq_end(dc_beetle_reference_irq* irq, uint32_t lines);

// Advances the processor's clock by `master_clocks` and returns the
// timestamp afterwards. The timer catches up on its own inside this, at
// whichever moments its divider comes due; the catch-up at the end is a
// no-op unless the last one landed short, which is the point.
uint32_t dc_beetle_reference_irq_advance(
    dc_beetle_reference_irq* irq,
    int32_t master_clocks);
uint32_t dc_beetle_reference_irq_timestamp(dc_beetle_reference_irq* irq);

// The two register files, reached exactly as the machine's I/O page handler
// reaches them -- including its ordering around the I/O bus latch, which
// differs between a read and a write and which a read of $0C00 depends on.
uint8_t dc_beetle_reference_irq_read_timer(
    dc_beetle_reference_irq* irq,
    uint32_t address);
void dc_beetle_reference_irq_write_timer(
    dc_beetle_reference_irq* irq,
    uint32_t address,
    uint8_t value);
uint8_t dc_beetle_reference_irq_read_status(
    dc_beetle_reference_irq* irq,
    uint32_t address);
// The same read without its side effect. A read of $1402 acknowledges the
// timer while reporting the mask, so the emulator carries a peek for the
// debugger and the model has to carry the same distinction.
uint8_t dc_beetle_reference_irq_peek_status(
    dc_beetle_reference_irq* irq,
    uint32_t address);
void dc_beetle_reference_irq_write_status(
    dc_beetle_reference_irq* irq,
    uint32_t address,
    uint8_t value);

uint8_t dc_beetle_reference_irq_io_data_buffer(dc_beetle_reference_irq* irq);
void dc_beetle_reference_irq_set_io_data_buffer(
    dc_beetle_reference_irq* irq,
    uint8_t value);

// The horizon a native routine transcript declares itself against, and the
// I flag it is gated on.
void dc_beetle_reference_irq_set_i_flag(
    dc_beetle_reference_irq* irq,
    int set);
uint32_t dc_beetle_reference_irq_timer_horizon(
    dc_beetle_reference_irq* irq);

void dc_beetle_reference_irq_observe(
    dc_beetle_reference_irq* irq,
    dc_beetle_reference_irq_observation* out);

#ifdef __cplusplus
}
#endif
