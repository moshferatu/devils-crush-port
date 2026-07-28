// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/game_logic/physics.h"

int dc_restore_cached_position(
    const uint8_t* const work_ram,
    const size_t work_ram_size,
    const uint8_t slot,
    dc_cached_position_restore* const restore)
{
    static const uint16_t position_offsets[DC_CACHED_POSITION_BYTE_COUNT] = {
        0x1100,
        0x1180,
        0x1200,
        0x1280,
        0x1300,
        0x1380,
    };

    if (work_ram == NULL || restore == NULL || work_ram_size < 0x2000) {
        return 0;
    }

    for (size_t index = 0;
         index < DC_CACHED_POSITION_BYTE_COUNT;
         ++index) {
        restore->work_ram_offsets[index] =
            (uint16_t)(position_offsets[index] + slot);
        restore->values[index] = work_ram[0x0085 + index];
    }
    return 1;
}
