// SPDX-License-Identifier: GPL-3.0-or-later

#include "dc_beetle_execution_hook.h"

#include <mednafen/pce/pce.h>
#include <mednafen/pce/vce.h>

extern VCE* vce;

dc_beetle_instruction_hook dc_beetle_active_instruction_hook = nullptr;
void* dc_beetle_active_instruction_hook_user_data = nullptr;
dc_beetle_memory_write_hook dc_beetle_active_memory_write_hook = nullptr;
void* dc_beetle_active_memory_write_hook_user_data = nullptr;
dc_beetle_replacement_hook dc_beetle_active_replacement_hook = nullptr;
void* dc_beetle_active_replacement_hook_user_data = nullptr;
const dc_beetle_psg_model* dc_beetle_active_psg_model = nullptr;
static int32_t* dc_beetle_psg_output_buffers[2] = {nullptr, nullptr};
static int dc_beetle_psg_revision = DC_BEETLE_PSG_REVISION_HUC6280;
const dc_beetle_vdc_model* dc_beetle_active_vdc_model = nullptr;
static uint32_t dc_beetle_vdc_vram_size = 0;
uint32_t dc_beetle_replacement_nested_installs = 0;
uint32_t dc_beetle_replacement_nest_overflows = 0;
uint32_t dc_beetle_replacement_max_depth = 0;

void dc_beetle_reset_replacement_nesting_counters(void)
{
    dc_beetle_replacement_nested_installs = 0;
    dc_beetle_replacement_nest_overflows = 0;
    dc_beetle_replacement_max_depth = 0;
}

void dc_beetle_set_instruction_hook(
    const dc_beetle_instruction_hook hook,
    void* const user_data)
{
    dc_beetle_active_instruction_hook_user_data = user_data;
    dc_beetle_active_instruction_hook = hook;
}

void dc_beetle_clear_instruction_hook(void)
{
    dc_beetle_active_instruction_hook = nullptr;
    dc_beetle_active_instruction_hook_user_data = nullptr;
}

void dc_beetle_set_memory_write_hook(
    const dc_beetle_memory_write_hook hook,
    void* const user_data)
{
    dc_beetle_active_memory_write_hook_user_data = user_data;
    dc_beetle_active_memory_write_hook = hook;
}

void dc_beetle_clear_memory_write_hook(void)
{
    dc_beetle_active_memory_write_hook = nullptr;
    dc_beetle_active_memory_write_hook_user_data = nullptr;
}

void dc_beetle_set_replacement_hook(
    const dc_beetle_replacement_hook hook,
    void* const user_data)
{
    dc_beetle_active_replacement_hook_user_data = user_data;
    dc_beetle_active_replacement_hook = hook;
}

void dc_beetle_clear_replacement_hook(void)
{
    dc_beetle_active_replacement_hook = nullptr;
    dc_beetle_active_replacement_hook_user_data = nullptr;
}

void dc_beetle_set_psg_model(const dc_beetle_psg_model* const model)
{
    dc_beetle_active_psg_model = model;

    if (model != nullptr && model->configure != nullptr
        && dc_beetle_psg_output_buffers[0] != nullptr) {
        model->configure(
            model->user_data,
            dc_beetle_psg_output_buffers[0],
            dc_beetle_psg_output_buffers[1],
            dc_beetle_psg_revision);
    }
}

void dc_beetle_clear_psg_model(void)
{
    dc_beetle_active_psg_model = nullptr;
}

void dc_beetle_set_psg_device(
    int32_t* const left,
    int32_t* const right,
    const int revision)
{
    dc_beetle_psg_output_buffers[0] = left;
    dc_beetle_psg_output_buffers[1] = right;
    dc_beetle_psg_revision = revision;

    if (dc_beetle_active_psg_model != nullptr
        && dc_beetle_active_psg_model->configure != nullptr) {
        dc_beetle_active_psg_model->configure(
            dc_beetle_active_psg_model->user_data,
            left,
            right,
            revision);
    }
}

void dc_beetle_set_vdc_model(const dc_beetle_vdc_model* const model)
{
    dc_beetle_active_vdc_model = model;

    if (model != nullptr && model->configure != nullptr
        && dc_beetle_vdc_vram_size != 0) {
        model->configure(model->user_data, dc_beetle_vdc_vram_size);
    }
}

void dc_beetle_clear_vdc_model(void)
{
    dc_beetle_active_vdc_model = nullptr;
}

void dc_beetle_set_vdc_device(const uint32_t vram_size)
{
    dc_beetle_vdc_vram_size = vram_size;

    if (dc_beetle_active_vdc_model != nullptr
        && dc_beetle_active_vdc_model->configure != nullptr) {
        dc_beetle_active_vdc_model->configure(
            dc_beetle_active_vdc_model->user_data,
            vram_size);
    }
}

// The emulator's own device passes the direction of the change and `VCE`
// throws it away, re-deriving the level from every chip's latched status.
// Reproducing that rather than trusting the argument is what lets one chip be
// native while the other is not.
void dc_beetle_vdc_irq_change(int)
{
    if (vce != nullptr) {
        vce->IRQChangeCheck();
    }
}

int dc_beetle_vdc_wait_state(const int32_t cycles)
{
    // No encoder means no cycle budget to charge against, and a model that
    // kept asking would never return.
    if (vce == nullptr) {
        return 0;
    }
    return vce->WS_Hook(cycles) ? 1 : 0;
}

int dc_beetle_copy_video_snapshot(
    dc_beetle_video_snapshot* const snapshot)
{
    if (vce == nullptr || snapshot == nullptr
        || snapshot->vram_words == nullptr
        || snapshot->sat_words == nullptr
        || snapshot->palette_entries == nullptr
        || snapshot->vram_word_capacity < DC_BEETLE_VRAM_WORD_COUNT
        || snapshot->sat_word_capacity < DC_BEETLE_SAT_WORD_COUNT
        || snapshot->palette_entry_capacity
            < DC_BEETLE_PALETTE_ENTRY_COUNT) {
        return 0;
    }

    for (size_t index = 0; index < DC_BEETLE_VRAM_WORD_COUNT; ++index) {
        snapshot->vram_words[index] =
            vce->DebugPeekVRAM(static_cast<uint16_t>(index));
    }
    for (size_t index = 0; index < DC_BEETLE_SAT_WORD_COUNT; ++index) {
        snapshot->sat_words[index] =
            vce->DebugPeekSAT(static_cast<uint8_t>(index));
    }
    for (size_t index = 0;
         index < DC_BEETLE_PALETTE_ENTRY_COUNT;
         ++index) {
        snapshot->palette_entries[index] =
            vce->DebugPeekPalette(static_cast<uint16_t>(index));
    }
    snapshot->memory_width_register = static_cast<uint16_t>(
        vce->DebugGetVDCRegister(VDC::GSREG_MWR));
    snapshot->background_x_scroll = static_cast<uint16_t>(
        vce->DebugGetVDCRegister(VDC::GSREG_BXR));
    snapshot->background_y_scroll = static_cast<uint16_t>(
        vce->DebugGetVDCRegister(VDC::GSREG_BYR));
    return 1;
}
