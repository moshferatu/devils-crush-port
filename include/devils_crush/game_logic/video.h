// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Result of one character written by ROM routine 08:5B01. The routine maps
// the game's text encoding to a two-byte background tile and advances the
// VDC word address for the next character.
typedef struct dc_text_tile_write {
    uint8_t character;
    uint8_t tile_low;
    uint8_t tile_high;
    uint16_t next_address;
} dc_text_tile_write;

// Converts one ASCII-like text byte exactly as 08:5B01 does. Space uses the
// game's dedicated blank glyph; digits and letters occupy adjacent regions.
int dc_prepare_text_tile_write(
    uint8_t character,
    uint16_t address,
    dc_text_tile_write* write);

// The short return at the head of ROM routine 08:5E42 is taken only while
// both the global video-busy byte ($2254) and deferred-update byte ($2D53)
// are clear. Active states continue into the original renderer.
int dc_video_update_gate_is_idle(
    uint8_t video_busy,
    uint8_t deferred_update);

#ifdef __cplusplus
}
#endif
