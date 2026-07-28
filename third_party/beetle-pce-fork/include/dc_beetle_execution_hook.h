// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dc_beetle_cpu_state {
    uint16_t logical_pc;
    uint8_t physical_bank;
    uint8_t opcode;
    uint8_t accumulator;
    uint8_t x;
    uint8_t y;
    uint8_t stack_pointer;
    uint8_t status;
    uint8_t mpr[8];
    uint32_t master_cycle;
    uint64_t absolute_master_cycle;
} dc_beetle_cpu_state;

typedef struct dc_beetle_memory_write {
    uint16_t logical_address;
    uint8_t physical_bank;
    uint8_t value;
    uint16_t instruction_logical_pc;
    uint8_t instruction_physical_bank;
    uint32_t master_cycle;
    uint64_t absolute_master_cycle;
} dc_beetle_memory_write;

#define DC_BEETLE_MAX_REPLACEMENT_WRITES 512

typedef struct dc_beetle_replacement_write {
    uint16_t logical_address;
    uint8_t value;
    uint16_t instruction_logical_pc;
    uint8_t instruction_physical_bank;
    uint32_t master_cycle_offset;
} dc_beetle_replacement_write;

typedef struct dc_beetle_replacement_result {
    uint8_t handled;
    uint16_t resume_logical_pc;
    uint8_t accumulator;
    uint8_t x;
    uint8_t y;
    uint8_t stack_pointer;
    uint8_t status;
    uint8_t mpr[8];
    uint32_t body_master_cycles;
    uint32_t write_count;
    const dc_beetle_replacement_write* writes;
} dc_beetle_replacement_result;

typedef void (*dc_beetle_instruction_hook)(
    const dc_beetle_cpu_state* state,
    void* user_data);
typedef void (*dc_beetle_memory_write_hook)(
    const dc_beetle_memory_write* write,
    void* user_data);
typedef void (*dc_beetle_replacement_hook)(
    const dc_beetle_cpu_state* state,
    dc_beetle_replacement_result* result,
    void* user_data);

#define DC_BEETLE_VRAM_WORD_COUNT 32768
#define DC_BEETLE_SAT_WORD_COUNT 256
#define DC_BEETLE_PALETTE_ENTRY_COUNT 512

typedef struct dc_beetle_video_snapshot {
    uint16_t* vram_words;
    size_t vram_word_capacity;
    uint16_t* sat_words;
    size_t sat_word_capacity;
    uint16_t* palette_entries;
    size_t palette_entry_capacity;
    uint16_t memory_width_register;
    uint16_t background_x_scroll;
    uint16_t background_y_scroll;
} dc_beetle_video_snapshot;

// Beetle PCE is single-instance and thread-confined. Registration and callback
// dispatch must occur on that same owning thread.
extern dc_beetle_instruction_hook dc_beetle_active_instruction_hook;
extern void* dc_beetle_active_instruction_hook_user_data;
extern dc_beetle_memory_write_hook dc_beetle_active_memory_write_hook;
extern void* dc_beetle_active_memory_write_hook_user_data;
extern dc_beetle_replacement_hook dc_beetle_active_replacement_hook;
extern void* dc_beetle_active_replacement_hook_user_data;

void dc_beetle_set_instruction_hook(
    dc_beetle_instruction_hook hook,
    void* user_data);
void dc_beetle_clear_instruction_hook(void);
void dc_beetle_set_memory_write_hook(
    dc_beetle_memory_write_hook hook,
    void* user_data);
void dc_beetle_clear_memory_write_hook(void);
void dc_beetle_set_replacement_hook(
    dc_beetle_replacement_hook hook,
    void* user_data);
void dc_beetle_clear_replacement_hook(void);

// Copies the base PC Engine VDC/VCE state without retaining emulator-owned
// pointers. The destination capacities must cover the fixed counts above.
int dc_beetle_copy_video_snapshot(dc_beetle_video_snapshot* snapshot);

#ifdef __cplusplus
} // extern "C"
#endif
