// SPDX-License-Identifier: GPL-3.0-or-later

#include "dc_beetle_reference_psg.h"

#include <mednafen/mednafen.h>

#include <mednafen/hw_sound/pce_psg/pce_psg.h>

// A thin lifetime and naming shim. The emulator's PSG is a C++ class behind
// private include directories, so the differential test reaches it through
// this instead of compiling Mednafen's headers itself.
struct dc_beetle_reference_psg {
    PCE_PSG psg;

    dc_beetle_reference_psg(
        int32_t* left_buffer,
        int32_t* right_buffer,
        int revision)
        : psg(left_buffer, right_buffer, revision)
    {
    }
};

dc_beetle_reference_psg* dc_beetle_reference_psg_create(
    int32_t* left_buffer,
    int32_t* right_buffer,
    const int revision)
{
    return new dc_beetle_reference_psg(left_buffer, right_buffer, revision);
}

void dc_beetle_reference_psg_destroy(dc_beetle_reference_psg* const psg)
{
    delete psg;
}

void dc_beetle_reference_psg_power(
    dc_beetle_reference_psg* const psg,
    const int32_t timestamp)
{
    psg->psg.Power(timestamp);
}

void dc_beetle_reference_psg_write(
    dc_beetle_reference_psg* const psg,
    const int32_t timestamp,
    const uint8_t address,
    const uint8_t value)
{
    psg->psg.Write(timestamp, address, value);
}

void dc_beetle_reference_psg_update(
    dc_beetle_reference_psg* const psg,
    const int32_t timestamp)
{
    psg->psg.Update(timestamp);
}

void dc_beetle_reference_psg_reset_timestamp(
    dc_beetle_reference_psg* const psg,
    const int32_t base)
{
    psg->psg.ResetTS(base);
}

void dc_beetle_reference_psg_export_state(
    const dc_beetle_reference_psg* const psg,
    dc_beetle_psg_state* const state)
{
    psg->psg.DcExportState(state);
}

void dc_beetle_reference_psg_import_state(
    dc_beetle_reference_psg* const psg,
    const dc_beetle_psg_state* const state)
{
    psg->psg.DcImportState(state);
}
