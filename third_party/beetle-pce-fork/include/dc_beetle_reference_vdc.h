// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The emulator's own video display controller, reachable without an
// emulator.
//
// The working assumption when this was planned was that the VDC could not
// be lifted out the way `PCE_PSG` was, because the game reads it back and a
// reference device would therefore need the machine around it. That is not
// what the class turned out to be. `VDC` holds its own VRAM, sprite
// attribute table, register file, and raster phase counters; `vdc.cpp`
// touches no emulator global; and every outward edge is one of two function
// pointers the owner installs. Its reads are methods on the device, not
// queries against the machine, so a test can call them on two devices and
// compare.
//
// What the VDC does need, and the PSG did not, is a clock schedule. The
// device is driven by `run`/`hsync`/`vsync`, and it is `VCE` that decides
// how those interleave, from a dot-clock ratio and two blanking counters.
// That schedule is the caller's to reproduce -- see
// `dc_beetle_reference_vdc_run`.
//
// This exists for the tests and dies with the emulator. It is deliberately
// not a hook: nothing in the machine calls it, and a device created here is
// unrelated to the one the running emulator draws with.
typedef struct dc_beetle_reference_vdc dc_beetle_reference_vdc;

// `vram_size` is in 16-bit words, and is 32768 on a stock PC Engine, which
// is what `VCE` passes. Only one reference device may exist at a time: the
// emulator's hook setters take bare function pointers with no user data, so
// there is nowhere to put a second device's identity. Creating one while
// another lives returns null.
dc_beetle_reference_vdc* dc_beetle_reference_vdc_create(uint32_t vram_size);
void dc_beetle_reference_vdc_destroy(dc_beetle_reference_vdc* vdc);

// Returns the device's next event, in dot clocks, as `VCE::Reset` records
// it. Call before anything else; the constructor does not reset.
int32_t dc_beetle_reference_vdc_reset(dc_beetle_reference_vdc* vdc);

// Port access. `address` is the low two bits of the VDC's address space:
// 0 selects a register, 2 and 3 are the data port's low and high halves.
// `next_event` is read and written, and carries the device's scheduling
// horizon across calls the way `VCE::child_event` does.
void dc_beetle_reference_vdc_write(
    dc_beetle_reference_vdc* vdc,
    uint32_t address,
    uint8_t value,
    int32_t* next_event);
uint8_t dc_beetle_reference_vdc_read(
    dc_beetle_reference_vdc* vdc,
    uint32_t address,
    int32_t* next_event,
    int peek);

// Advance the raster by `clocks` dot clocks, emitting one pixel per clock
// into `pixels` unless `skip` is set. `pixels` must hold at least `clocks`
// entries; `VCE` gives it 2048. Returns the next event.
//
// The caller owns the schedule around this. `VCE` advances a master clock,
// divides it by the dot-clock ratio, calls this with the quotient, then
// calls `hsync` when its horizontal blanking counter expires (237 clocks
// inside blanking, 1128 outside) and `vsync` when its vertical one does
// (4095 inside, 357630 - 4095 outside on a 262-line field). A driver that
// interleaves them differently will produce a different picture from the
// same register writes, which is the point: this device is an oracle for
// the VDC, not for the machine that clocks it.
int32_t dc_beetle_reference_vdc_run(
    dc_beetle_reference_vdc* vdc,
    int32_t clocks,
    uint16_t* pixels,
    int skip);
int32_t dc_beetle_reference_vdc_hsync(
    dc_beetle_reference_vdc* vdc,
    int in_hblank);
int32_t dc_beetle_reference_vdc_vsync(
    dc_beetle_reference_vdc* vdc,
    int in_vblank);

// Non-disturbing observation, in 16-bit word units for the two memories.
// `id` is one of the emulator's `VDC::GSREG_*` values.
uint16_t dc_beetle_reference_vdc_peek_vram(
    dc_beetle_reference_vdc* vdc,
    uint16_t address);
uint16_t dc_beetle_reference_vdc_peek_sat(
    dc_beetle_reference_vdc* vdc,
    uint8_t address);
uint32_t dc_beetle_reference_vdc_get_register(
    dc_beetle_reference_vdc* vdc,
    unsigned id);
int dc_beetle_reference_vdc_peek_irq(dc_beetle_reference_vdc* vdc);

// What the two hooks saw. The device calls the first whenever its
// interrupt status changes and the second when an access has to stall the
// CPU, and in the emulator both run back into machine state this device
// does not have. Counting them is how a test observes those edges instead.
//
// `irq_hook_calls` counts both directions; `irq_hook_raises` counts only
// the assertions, the device passing false when a status read acknowledges
// one. `VCE` discards that argument and re-derives the level from
// `PeekIRQ`, but the edges are the sequence a native model must reproduce.
// `wait_state_cycles` accumulates its argument, which is -1 for the VDC's
// event-based stalling and a dot-clock count otherwise.
uint64_t dc_beetle_reference_vdc_irq_hook_calls(
    const dc_beetle_reference_vdc* vdc);
uint64_t dc_beetle_reference_vdc_irq_hook_raises(
    const dc_beetle_reference_vdc* vdc);
uint64_t dc_beetle_reference_vdc_wait_state_hook_calls(
    const dc_beetle_reference_vdc* vdc);
int64_t dc_beetle_reference_vdc_wait_state_cycles(
    const dc_beetle_reference_vdc* vdc);

// The device stalls by looping until the wait-state hook refuses, so the
// refusal is load-bearing: a hook that always agrees never returns. The
// emulator's escape is a per-frame budget of 455 * 64 stolen cycles, after
// which `VCE::WS_Hook` says no and the device completes the blocking
// transfer itself. That is the default here.
//
// `charge_per_call` is what one stall costs against the budget. The
// emulator charges the cycles it steals, which it derives from machine
// state this device does not have, so the caller supplies the model; the
// default of one charges a stall per call and only affects how many
// spins precede the refusal.
void dc_beetle_reference_vdc_set_wait_state_policy(
    dc_beetle_reference_vdc* vdc,
    int32_t charge_per_call,
    int32_t budget);

// Clear the budget, as `VCE::RunPartial` does at the start of each frame.
void dc_beetle_reference_vdc_reset_wait_states(dc_beetle_reference_vdc* vdc);
int32_t dc_beetle_reference_vdc_wait_state_counter(
    const dc_beetle_reference_vdc* vdc);

#ifdef __cplusplus
}
#endif
