// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

#include "dc_beetle_execution_hook.h"

#ifdef __cplusplus
extern "C" {
#endif

// The emulator's own sound generator, reachable without an emulator.
//
// `PCE_PSG` needs nothing from the rest of the machine: its constructor takes
// the two delta buffers and a chip revision, and every method after that takes
// a timestamp. It is therefore a reference device that a test can drive
// directly, which matters because the game does not exercise the whole chip.
// The census route never enables the low-frequency oscillator, uses the noise
// source on one channel, and reaches the accumulating output mode seven times
// in a minute of play, so those paths of the native model have no oracle in
// any recording of the game. Driving both devices from the same synthetic
// register stream gives them one.
//
// This exists for the tests and dies with the emulator. It is deliberately not
// a hook: nothing in the machine calls it, and installing a model through
// `dc_beetle_set_psg_model` does not affect the device created here.
typedef struct dc_beetle_reference_psg dc_beetle_reference_psg;

// The buffers are the caller's, and must hold 65,536 positions plus 32 more
// for an impulse that dangles past the last one. `revision` is one of the
// `DC_BEETLE_PSG_REVISION_*` values; any other aborts, as the emulator's own
// constructor does.
dc_beetle_reference_psg* dc_beetle_reference_psg_create(
    int32_t* left_buffer,
    int32_t* right_buffer,
    int revision);
void dc_beetle_reference_psg_destroy(dc_beetle_reference_psg* psg);

void dc_beetle_reference_psg_power(
    dc_beetle_reference_psg* psg,
    int32_t timestamp);
void dc_beetle_reference_psg_write(
    dc_beetle_reference_psg* psg,
    int32_t timestamp,
    uint8_t address,
    uint8_t value);
void dc_beetle_reference_psg_update(
    dc_beetle_reference_psg* psg,
    int32_t timestamp);
void dc_beetle_reference_psg_reset_timestamp(
    dc_beetle_reference_psg* psg,
    int32_t base);

void dc_beetle_reference_psg_export_state(
    const dc_beetle_reference_psg* psg,
    dc_beetle_psg_state* state);
void dc_beetle_reference_psg_import_state(
    dc_beetle_reference_psg* psg,
    const dc_beetle_psg_state* state);

#ifdef __cplusplus
}
#endif
