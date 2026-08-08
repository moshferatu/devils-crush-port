// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ROM routine 08:40DF emits one actor's sprite entries into the satellite
// attribute table. Each entry is six descriptor bytes and produces exactly
// eight VDC data-port writes, so the routine's own overflow guard — the
// cursor plus this call's count must stay below $40 — bounds a call at
// 0x3F entries and 504 writes, inside the core's 512-write transaction cap.
#define DC_SPRITE_EMIT_MAX_ENTRIES 0x3FU
#define DC_SPRITE_EMIT_WRITES_PER_ENTRY 8U

// The descriptor tables and every pointer they resolve live in the bank the
// routine already runs from, mapped at $4000.
#define DC_SPRITE_EMIT_ROM_BASE 0x4000U
#define DC_SPRITE_EMIT_ROM_SIZE 0x2000U

typedef struct dc_sprite_emit_entry {
    // The six descriptor bytes this entry was built from. An instruction
    // transcript needs them as well as the results: each one is the
    // accumulator at a load boundary, before the arithmetic that consumes
    // it, and two of them cannot be recovered from the results alone.
    uint8_t descriptor[6];
    // The eight VDC data-port values, in write order: vertical, horizontal,
    // pattern, then attribute, each low byte to $0002 and high to $0003.
    uint8_t vertical_low;
    uint8_t vertical_high;
    uint8_t horizontal_low;
    uint8_t horizontal_high;
    uint8_t pattern_low;
    uint8_t pattern_high;
    uint8_t attribute_low;
    uint8_t attribute_high;
    // $35:$36 as the widened vertical offset leaves them, then again after
    // the camera origin subtracts out. Both pairs are stored, so both are
    // observable writes.
    uint8_t widen_low;
    uint8_t widen_high;
    uint8_t camera_low;
    uint8_t camera_high;
    // Path selectors; the caller prices each iteration from these.
    uint8_t vertical_negative;
    uint8_t horizontal_negative;
    uint8_t attribute_low_merged;
    // Carry and overflow out of the pattern high-byte add, the last flag
    // arithmetic before the return.
    uint8_t pattern_carry;
    uint8_t pattern_overflow;
} dc_sprite_emit_entry;

typedef struct dc_sprite_emit_run {
    // $2C at entry and the count actually emitted; they are equal because a
    // partial run is not expressible — the routine has no resumable head.
    uint8_t count;
    uint8_t descriptor_id;
    uint8_t frame_index;
    // $2D:$2E, then $2F:$30.
    uint16_t descriptor_pointer;
    uint16_t entry_pointer;
    // $4E after the run and the Y register at the return.
    uint8_t cursor;
    uint8_t final_y;
    // $35:$36 left by the last entry's vertical computation.
    uint8_t vertical_scratch_low;
    uint8_t vertical_scratch_high;
    // Accumulator and arithmetic flags at the return.
    uint8_t accumulator;
    uint8_t carry;
    uint8_t overflow;
    dc_sprite_emit_entry entries[DC_SPRITE_EMIT_MAX_ENTRIES];
} dc_sprite_emit_run;

// Classifies one whole call of the 08:40DF sprite emitter.
//
// `sprite_rom` is the routine's own bank, DC_SPRITE_EMIT_ROM_SIZE bytes
// starting at logical DC_SPRITE_EMIT_ROM_BASE; every pointer the routine
// follows resolves inside it and a pointer that does not returns 0.
//
// Three shapes return 0 rather than being modelled. A zero entry count would
// run the loop 256 times through the DEC $2C underflow. A cursor overflow
// returns early at 4106. A $3800,X bit-5 actor merges $3E00,X into the
// attribute high byte. None occurs on any observed route, so modelling them
// would add paths no regression can hold.
int dc_classify_sprite_emit_run(
    const uint8_t* work_ram,
    size_t work_ram_size,
    const uint8_t* sprite_rom,
    size_t sprite_rom_size,
    uint8_t slot,
    dc_sprite_emit_run* run);

#ifdef __cplusplus
}
#endif
