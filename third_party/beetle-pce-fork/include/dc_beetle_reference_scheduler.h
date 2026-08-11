// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The emulator's own clock and event scheduler, reachable without an
// emulator.
//
// This shim is the timer's shim taken one step further. That one drove the
// processor's clock in order to watch a device hanging off it; this one
// watches the clock itself. So the substitution it relies on is the same and
// slightly stronger: the clock does not know what an instruction is, and a
// stolen cycle moves it exactly as an executed one would. Everything the
// scheduler does -- dividing a cycle into master clocks, bringing two
// horizons nearer, stopping when the near one arrives -- happens inside
// `StealCycles`, which is public.
//
// Two of the scheduler's fields have no public entry point at all. `speed`
// is written by the CSL and CSH opcodes and the loop flag by `Run`, which
// runs. Both are reached here through setters the observation patch adds,
// which is the one place this shim asks the emulator for something it does
// not already offer.
//
// The user event handler is installed by this shim rather than by the
// machine, because the handler *is* half of what is being tested: when it is
// called, and with what timestamp. It answers with a configurable interval
// and records every call, so a test can compare not just where the clock
// ended up but the whole sequence of moments the scheduler decided something
// was due.
//
// The timer on this side is the emulator's own, catching up inside
// `HappySync` at the moments a real machine would. A test drives it through
// the register file below and pairs it with a native `HuC6280Irq` on the
// other arm -- which is sound because `dc_irq_device` has already proved
// those two equal field for field, so any difference this test sees is the
// scheduler's.
//
// This exists for the tests and dies with the emulator. It claims the
// emulator's own processor, so only one reference device may exist at a
// time, none may exist while a real machine is running, and it must not
// share a process with a live reference colour encoder or a live reference
// timer.
typedef struct dc_beetle_reference_scheduler dc_beetle_reference_scheduler;

// The scheduler's state, read out. `speed_shift_cache` is included even
// though it is derived, because it is the value the conversion from cycles
// to master clocks actually uses and a model that held the rate correctly
// but shadowed it wrongly would be invisible otherwise.
typedef struct dc_beetle_reference_scheduler_observation {
    uint32_t timestamp;
    uint32_t timestamp_frac;
    uint32_t overclock_rate;
    int32_t next_event;
    int32_t next_user_event;
    int32_t runrunrun;
    uint8_t speed;
    uint8_t speed_shift_cache;
} dc_beetle_reference_scheduler_observation;

// What the installed user event handler has been asked to do, and what it
// has seen. The digest is order-sensitive, so two runs that called the
// handler the same number of times at a different set of moments do not
// agree by accident.
typedef struct dc_beetle_reference_scheduler_events {
    uint64_t count;
    uint64_t digest;
    int32_t last_timestamp;
} dc_beetle_reference_scheduler_event_log;

// Creating one while another lives returns null.
dc_beetle_reference_scheduler* dc_beetle_reference_scheduler_create(void);
void dc_beetle_reference_scheduler_destroy(
    dc_beetle_reference_scheduler* scheduler);

// `HuC6280::Init`: the clock, its remainder and both horizons go to zero and
// the overclock rate goes back to one. It does not touch the rate or the
// loop flag, which is why those two are set separately below.
void dc_beetle_reference_scheduler_init(
    dc_beetle_reference_scheduler* scheduler);

// `HuC6280::Reset` and `HuC6280::Power`, neither of which touches a single
// field of this device. They are offered so a test can show that rather than
// assert it.
void dc_beetle_reference_scheduler_reset(
    dc_beetle_reference_scheduler* scheduler);
void dc_beetle_reference_scheduler_power(
    dc_beetle_reference_scheduler* scheduler);

// `ADDCYC` and `ADDCYC_MASTER`, reached through the two entry points the
// machine's own devices steal time with.
void dc_beetle_reference_scheduler_advance(
    dc_beetle_reference_scheduler* scheduler,
    int32_t cycles);
void dc_beetle_reference_scheduler_advance_master(
    dc_beetle_reference_scheduler* scheduler,
    int32_t master_clocks);

// `SetEvent`: a device declaring when it next wants to be called.
void dc_beetle_reference_scheduler_set_event(
    dc_beetle_reference_scheduler* scheduler,
    int32_t cycles);

// `CalcNextEvent` alone. The emulator has no entry point for it, so this
// re-arms the user event at the value it already holds, which is the same
// two statements with the first one a no-op.
void dc_beetle_reference_scheduler_recalculate(
    dc_beetle_reference_scheduler* scheduler);

// `TimerSync`: catch the timer up to now, without recomputing anything.
void dc_beetle_reference_scheduler_sync_timer(
    dc_beetle_reference_scheduler* scheduler);

// `SyncAndResetTimestamp`: the frame boundary.
void dc_beetle_reference_scheduler_reset_timestamp(
    dc_beetle_reference_scheduler* scheduler,
    int32_t base);

// `SetOverclock`, and the two fields no public entry point reaches.
void dc_beetle_reference_scheduler_set_overclock(
    dc_beetle_reference_scheduler* scheduler,
    uint32_t rate);
void dc_beetle_reference_scheduler_set_speed(
    dc_beetle_reference_scheduler* scheduler,
    uint8_t speed);
void dc_beetle_reference_scheduler_run(
    dc_beetle_reference_scheduler* scheduler,
    int step_mode);
void dc_beetle_reference_scheduler_exit(
    dc_beetle_reference_scheduler* scheduler);

// The timer's register file at $0C00-$0C01, reached exactly as the machine's
// I/O page handler reaches it. Present so a test can give the near horizon
// something real to be bounded by; the device behind it is the timer's
// business and is proved elsewhere.
void dc_beetle_reference_scheduler_write_timer(
    dc_beetle_reference_scheduler* scheduler,
    uint32_t address,
    uint8_t value);

// What the installed handler returns. A test sets this before the advance
// that will call it, exactly as it sets the same value on the other arm.
void dc_beetle_reference_scheduler_set_event_interval(
    dc_beetle_reference_scheduler* scheduler,
    int32_t interval);
void dc_beetle_reference_scheduler_events(
    dc_beetle_reference_scheduler* scheduler,
    dc_beetle_reference_scheduler_event_log* out);

void dc_beetle_reference_scheduler_observe(
    dc_beetle_reference_scheduler* scheduler,
    dc_beetle_reference_scheduler_observation* out);

#ifdef __cplusplus
}
#endif
