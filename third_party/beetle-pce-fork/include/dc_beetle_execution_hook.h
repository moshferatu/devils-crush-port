// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dc_beetle_cpu_state {
    uint16_t logical_pc;
    uint8_t physical_bank;
    uint8_t accumulator;
    uint8_t x;
    uint8_t y;
    uint8_t stack_pointer;
    uint8_t status;
    uint8_t mpr[8];
    uint32_t master_cycle;
} dc_beetle_cpu_state;

typedef void (*dc_beetle_instruction_hook)(
    const dc_beetle_cpu_state* state,
    void* user_data);

// Beetle PCE is single-instance and thread-confined. Registration and callback
// dispatch must occur on that same owning thread.
extern dc_beetle_instruction_hook dc_beetle_active_instruction_hook;
extern void* dc_beetle_active_instruction_hook_user_data;

void dc_beetle_set_instruction_hook(
    dc_beetle_instruction_hook hook,
    void* user_data);
void dc_beetle_clear_instruction_hook(void);

#ifdef __cplusplus
} // extern "C"
#endif
