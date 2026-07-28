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
