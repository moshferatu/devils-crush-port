// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The emulator's own colour encoder, reachable without an emulator.
//
// The video display controller turned out to be standalone: it owns its
// memories and its raster phase, and its only outward edges are two function
// pointers the owner installs. The encoder is not. `VCE` reaches the
// processor directly and by name -- it steals cycles for a stalled port
// access, sets and clears an interrupt line, asks the processor to stop
// early at two scanlines, and takes its own timestamps from it -- and it
// holds the controller by value rather than by reference.
//
// So this shim does not lift the encoder out of the machine. It supplies
// the smallest machine the encoder will run in: the emulator's own
// processor object, initialised but never executed, driven forward by
// stolen master cycles instead of by instructions. That is enough, because
// everything `VCE` asks the processor for is either a counter it advances
// or a flag it sets, and none of it depends on the processor executing.
//
// Two consequences are worth stating plainly. The processor's timestamp is
// the clock: `VCE::Write` and its siblings synchronise to
// `HuCPU.Timestamp()` on their own, so a caller advances time by advancing
// the processor and the encoder follows. And a stalled port access shows up
// as the timestamp moving further than the caller asked, which is exactly
// what it is.
//
// This exists for the tests and dies with the emulator. It is not a hook:
// nothing in the machine calls it. But it does claim the emulator's own
// `vce` global, because the controller's interrupt and wait-state hooks are
// bare function pointers that find their encoder through it, so only one
// reference encoder may exist at a time and none may exist while a real
// machine is running.
typedef struct dc_beetle_reference_vce dc_beetle_reference_vce;

// The encoder's private schedule, read out. A differential test that could
// only compare pixels would report "the picture moved" for any of a dozen
// causes; this is how it says which.
typedef struct dc_beetle_reference_vce_observation {
    int32_t clock_divider;
    int32_t child_event[2];
    int32_t cd_event;
    int32_t scanline;
    int32_t pixel_offset;
    int32_t hblank_counter;
    int32_t vblank_counter;
    int32_t ws_counter;
    uint16_t ctaddress;
    uint8_t cr;
    int hblank;
    int vblank;
    int need_sl_reset;
    int frame_done;
} dc_beetle_reference_vce_observation;

// `vram_size` is in 16-bit words and is 32768 on a stock PC Engine. The
// SuperGrafx is never asked for: this port does not route its second
// controller, and an encoder built with one would composite a layer no
// caller here can reach. Creating one while another lives returns null.
dc_beetle_reference_vce* dc_beetle_reference_vce_create(uint32_t vram_size);
void dc_beetle_reference_vce_destroy(dc_beetle_reference_vce* vce);

// Resets the encoder and, through it, the controller. Also rebases the
// processor's clock to zero, so a test's timestamps start where the
// encoder's do.
void dc_beetle_reference_vce_reset(dc_beetle_reference_vce* vce);

// Builds the two 512-entry expansions of a nine-bit palette entry from the
// emulator's own default colour map, then reads them back out. What a
// palette entry looks like is the display's decision rather than the chip's,
// so a native model is handed these rather than deriving them: the point of
// comparison is the encoder, not the colour arithmetic.
void dc_beetle_reference_vce_set_pixel_format(dc_beetle_reference_vce* vce);
void dc_beetle_reference_vce_surface_palette(
    dc_beetle_reference_vce* vce,
    uint32_t* normal,
    uint32_t* grey);
// The merge of palette memory and that expansion, which is what a pixel is
// looked up in. Entries 0x200 and 0x300 hold the colour a disabled layer
// draws as instead.
uint32_t dc_beetle_reference_vce_colour_cache(
    dc_beetle_reference_vce* vce,
    unsigned entry);
// What a pixel whose layer has been disabled draws as. `VCE::StartFrame`
// writes it into those two cache entries from a constant, in the pixel
// format the frontend chose, so it is read from here rather than guessed.
uint32_t dc_beetle_reference_vce_disabled_layer_colour(void);

void dc_beetle_reference_vce_set_show_horiz_os(
    dc_beetle_reference_vce* vce,
    int show);
void dc_beetle_reference_vce_set_layer_enable_mask(
    dc_beetle_reference_vce* vce,
    uint64_t mask);
void dc_beetle_reference_vce_set_unlimited_sprites(
    dc_beetle_reference_vce* vce,
    int unlimited);

// One frame's framebuffer. `pitch` is in pixels and `line_widths` holds 263
// entries. The three settings are the ones `VCE::StartFrame` reads for
// itself: `scaling` of 2 selects the upscaled pixel path, and the two
// scanline bounds decide which lines are drawn at all.
void dc_beetle_reference_vce_start_frame(
    dc_beetle_reference_vce* vce,
    uint16_t* pixels,
    int32_t width,
    int32_t height,
    int32_t pitch,
    int32_t* line_widths,
    int skip,
    int scaling,
    int scanline_start,
    int scanline_end);

// What `VCE::RunPartial` does around running the processor, minus running
// it. The stall budget is cleared and the compact disc event is armed far
// enough away never to arrive, which is what a machine with no compact disc
// unit does.
//
// The third thing `RunPartial` does -- filling in the width of every line
// that was never drawn -- cannot be asked for without a processor, and is
// therefore not reachable through this shim.
void dc_beetle_reference_vce_reset_wait_states(dc_beetle_reference_vce* vce);
void dc_beetle_reference_vce_arm_cd_event(
    dc_beetle_reference_vce* vce,
    int32_t cycles);

// Advances the processor's clock by `master_clocks` and synchronises the
// encoder to it, returning the encoder's next event horizon. A stalled port
// access inside that synchronisation steals further cycles, so the
// timestamp afterwards may exceed what was asked for; the difference is the
// stall.
int32_t dc_beetle_reference_vce_advance(
    dc_beetle_reference_vce* vce,
    int32_t master_clocks);
uint32_t dc_beetle_reference_vce_timestamp(dc_beetle_reference_vce* vce);

// The encoder's register file at $0400-$0405 and the controller's at
// $0000-$0003, both reached exactly as the machine reaches them: each call
// synchronises to the processor's current timestamp first and republishes
// the next event afterwards.
void dc_beetle_reference_vce_write(
    dc_beetle_reference_vce* vce,
    uint32_t address,
    uint8_t value);
uint8_t dc_beetle_reference_vce_read(
    dc_beetle_reference_vce* vce,
    uint32_t address);
void dc_beetle_reference_vce_write_vdc(
    dc_beetle_reference_vce* vce,
    uint32_t address,
    uint8_t value);
uint8_t dc_beetle_reference_vce_read_vdc(
    dc_beetle_reference_vce* vce,
    uint32_t address);
void dc_beetle_reference_vce_write_vdc_st(
    dc_beetle_reference_vce* vce,
    uint32_t address,
    uint8_t value);

int32_t dc_beetle_reference_vce_calc_next_event(
    dc_beetle_reference_vce* vce);
void dc_beetle_reference_vce_observe(
    dc_beetle_reference_vce* vce,
    dc_beetle_reference_vce_observation* out);

// Non-disturbing observation of what the encoder holds and of the
// controller inside it.
uint16_t dc_beetle_reference_vce_peek_palette(
    dc_beetle_reference_vce* vce,
    uint16_t address);
uint16_t dc_beetle_reference_vce_peek_vram(
    dc_beetle_reference_vce* vce,
    uint16_t address);
uint16_t dc_beetle_reference_vce_peek_sat(
    dc_beetle_reference_vce* vce,
    uint8_t address);
uint32_t dc_beetle_reference_vce_vdc_register(
    dc_beetle_reference_vce* vce,
    unsigned id);

// The interrupt line as the processor holds it, which is where the encoder
// publishes it. There is no equivalent for the request that the processor
// stop early: `HuC6280::Exit` clears a private flag with no accessor, and
// the two scanlines that trigger it are compared instead.
int dc_beetle_reference_vce_irq_line(dc_beetle_reference_vce* vce);

#ifdef __cplusplus
}
#endif
