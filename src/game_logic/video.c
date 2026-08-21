// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/game_logic/video.h"

#include <stddef.h>

int dc_prepare_text_tile_write(
    const uint8_t character,
    const uint16_t address,
    dc_text_tile_write* const write)
{
    uint8_t mapped_character;
    uint8_t glyph_index;

    if (write == NULL) {
        return 0;
    }

    mapped_character = character == 0x20U ? 0x5BU : character;
    glyph_index = mapped_character < 0x41U
        ? (uint8_t)(mapped_character - 0x30U)
        : (uint8_t)(mapped_character - 0x37U);

    write->character = mapped_character;
    write->tile_low = (uint8_t)(glyph_index + 0x80U);
    write->tile_high = (uint8_t)(
        0x02U + (write->tile_low < glyph_index ? 1U : 0U));
    write->next_address = (uint16_t)(address + 1U);
    return 1;
}

int dc_video_update_gate_is_idle(
    const uint8_t video_busy,
    const uint8_t deferred_update)
{
    return video_busy == 0U && deferred_update == 0U;
}
