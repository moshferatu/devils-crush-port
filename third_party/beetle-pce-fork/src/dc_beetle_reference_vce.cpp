// SPDX-License-Identifier: GPL-3.0-or-later

#include "dc_beetle_reference_vce.h"

#include <mednafen/mednafen.h>

#include <mednafen/pce/pce.h>
#include <mednafen/pce/vce.h>
#include <mednafen/settings.h>
#include <mednafen/video.h>

// The encoder finds itself through this on the way back from the
// controller's two hooks, and `vce.cpp`'s own comment calls it horrible.
// Claiming it is what makes only one reference encoder possible at a time.
extern VCE* vce;

// Declared here rather than reached through `huc.h`, which does not compile
// on its own. False is what a cartridge machine holds, and it is what keeps
// the encoder from ever running a compact disc unit this shim has not built.
extern bool PCE_IsCD;

struct dc_beetle_reference_vce {
    VCE* device = nullptr;
    VCE* previous = nullptr;

    MDFN_Rect display_rect{};
};

namespace {

dc_beetle_reference_vce* g_active = nullptr;

// The processor's event handler, installed so that nothing in this shim can
// reach a null one. It deliberately does not synchronise the encoder: the
// caller does that itself, and a second synchronisation from inside a
// cycle-stealing call would advance the schedule twice.
int32 MDFN_FASTCALL reference_event_handler(const int32)
{
    return 0x3FFFFFF;
}

// Keeps the processor from ever deciding on its own that something is due.
void arm_processor(void)
{
    HuCPU.SetEventHandler(reference_event_handler);
    HuCPU.SetEvent(0x3FFFFFF);
}

}  // namespace

dc_beetle_reference_vce* dc_beetle_reference_vce_create(
    const uint32_t vram_size)
{
    if (g_active != nullptr) {
        return nullptr;
    }

    auto* const self = new dc_beetle_reference_vce();

    // No compact disc unit, so the encoder never runs one and never asks
    // this shim to.
    PCE_IsCD = false;

    HuCPU.Init(false);
    arm_processor();

    self->device = new VCE(false, vram_size);
    self->previous = vce;
    vce = self->device;

    g_active = self;
    return self;
}

void dc_beetle_reference_vce_destroy(dc_beetle_reference_vce* const self)
{
    if (self == nullptr) {
        return;
    }

    if (vce == self->device) {
        vce = self->previous;
    }

    if (g_active == self) {
        g_active = nullptr;
    }

    delete self->device;
    delete self;
}

void dc_beetle_reference_vce_reset(dc_beetle_reference_vce* const self)
{
    HuCPU.Init(false);
    arm_processor();

    self->device->Reset(0);
}

void dc_beetle_reference_vce_set_pixel_format(
    dc_beetle_reference_vce* const self)
{
    self->device->SetPixelFormat(nullptr, 0);
}

void dc_beetle_reference_vce_surface_palette(
    dc_beetle_reference_vce* const self,
    uint32_t* const normal,
    uint32_t* const grey)
{
    for (unsigned entry = 0; entry < 512; entry++) {
        normal[entry] = self->device->DcSurfClut(0, entry);
        grey[entry] = self->device->DcSurfClut(1, entry);
    }
}

uint32_t dc_beetle_reference_vce_colour_cache(
    dc_beetle_reference_vce* const self,
    const unsigned entry)
{
    return self->device->DcColorTableCache(entry);
}

uint32_t dc_beetle_reference_vce_disabled_layer_colour(void)
{
    return (uint32_t)MAKECOLOR(0x00, 0xFE, 0x00, 0);
}

void dc_beetle_reference_vce_set_show_horiz_os(
    dc_beetle_reference_vce* const self,
    const int show)
{
    self->device->SetShowHorizOS(show != 0);
}

void dc_beetle_reference_vce_set_layer_enable_mask(
    dc_beetle_reference_vce* const self,
    const uint64_t mask)
{
    self->device->SetLayerEnableMask(mask);
}

void dc_beetle_reference_vce_set_unlimited_sprites(
    dc_beetle_reference_vce* const self,
    const int unlimited)
{
    self->device->SetVDCUnlimitedSprites(unlimited != 0);
}

void dc_beetle_reference_vce_start_frame(
    dc_beetle_reference_vce* const self,
    uint16_t* const pixels,
    const int32_t width,
    const int32_t height,
    const int32_t pitch,
    int32_t* const line_widths,
    const int skip,
    const int scaling,
    const int scanline_start,
    const int scanline_end)
{
    // `VCE::StartFrame` reads all three of these for itself, from the
    // settings the frontend published.
    setting_pce_scaling = scaling;
    setting_pce_initial_scanline = scanline_start;
    setting_pce_last_scanline = scanline_end;

    MDFN_Surface surface;
    surface.pixels = pixels;
    surface.width = width;
    surface.height = height;
    surface.pitch = pitch;

    self->device->StartFrame(
        &surface, &self->display_rect, line_widths, skip);
}

void dc_beetle_reference_vce_reset_wait_states(
    dc_beetle_reference_vce* const self)
{
    self->device->DcResetWaitStates();
}

void dc_beetle_reference_vce_arm_cd_event(
    dc_beetle_reference_vce* const self,
    const int32_t cycles)
{
    self->device->SetCDEvent(cycles);
}

int32_t dc_beetle_reference_vce_advance(
    dc_beetle_reference_vce* const self,
    const int32_t master_clocks)
{
    HuCPU.StealMasterCycles(master_clocks);

    // The processor's clock decides how far the encoder has to catch up,
    // exactly as it does when instructions rather than this move it.
    const int32 timestamp = (int32)HuCPU.Timestamp();
    const int32 next_event = self->device->DcSyncReal(timestamp);

    arm_processor();

    return next_event;
}

uint32_t dc_beetle_reference_vce_timestamp(
    dc_beetle_reference_vce* const self)
{
    (void)self;
    return HuCPU.Timestamp();
}

void dc_beetle_reference_vce_write(
    dc_beetle_reference_vce* const self,
    const uint32_t address,
    const uint8_t value)
{
    self->device->Write(address, value);
    arm_processor();
}

uint8_t dc_beetle_reference_vce_read(
    dc_beetle_reference_vce* const self,
    const uint32_t address)
{
    const uint8 value = self->device->Read(address);
    arm_processor();
    return value;
}

void dc_beetle_reference_vce_write_vdc(
    dc_beetle_reference_vce* const self,
    const uint32_t address,
    const uint8_t value)
{
    self->device->WriteVDC(address, value);
    arm_processor();
}

uint8_t dc_beetle_reference_vce_read_vdc(
    dc_beetle_reference_vce* const self,
    const uint32_t address)
{
    const uint8 value = self->device->ReadVDC(address);
    arm_processor();
    return value;
}

void dc_beetle_reference_vce_write_vdc_st(
    dc_beetle_reference_vce* const self,
    const uint32_t address,
    const uint8_t value)
{
    self->device->WriteVDC_ST(address, value);
    arm_processor();
}

int32_t dc_beetle_reference_vce_calc_next_event(
    dc_beetle_reference_vce* const self)
{
    return self->device->DcCalcNextEvent();
}

void dc_beetle_reference_vce_observe(
    dc_beetle_reference_vce* const self,
    dc_beetle_reference_vce_observation* const out)
{
    VCE::DcObservation observation;
    self->device->DcObserve(&observation);

    out->clock_divider = observation.clock_divider;
    out->child_event[0] = observation.child_event[0];
    out->child_event[1] = observation.child_event[1];
    out->cd_event = observation.cd_event;
    out->scanline = observation.scanline;
    out->pixel_offset = observation.pixel_offset;
    out->hblank_counter = observation.hblank_counter;
    out->vblank_counter = observation.vblank_counter;
    out->ws_counter = observation.ws_counter;
    out->ctaddress = observation.ctaddress;
    out->cr = observation.cr;
    out->hblank = observation.hblank ? 1 : 0;
    out->vblank = observation.vblank ? 1 : 0;
    out->need_sl_reset = observation.need_sl_reset ? 1 : 0;
    out->frame_done = observation.frame_done ? 1 : 0;
}

uint16_t dc_beetle_reference_vce_peek_palette(
    dc_beetle_reference_vce* const self,
    const uint16_t address)
{
    return self->device->DebugPeekPalette(address);
}

uint16_t dc_beetle_reference_vce_peek_vram(
    dc_beetle_reference_vce* const self,
    const uint16_t address)
{
    return self->device->DebugPeekVRAM(address);
}

uint16_t dc_beetle_reference_vce_peek_sat(
    dc_beetle_reference_vce* const self,
    const uint8_t address)
{
    return self->device->DebugPeekSAT(address);
}

uint32_t dc_beetle_reference_vce_vdc_register(
    dc_beetle_reference_vce* const self,
    const unsigned id)
{
    return self->device->DebugGetVDCRegister(id);
}

int dc_beetle_reference_vce_irq_line(dc_beetle_reference_vce* const self)
{
    (void)self;

    // Register 3 of the interrupt controller reports the pending sources,
    // and bit 1 is the one the encoder drives.
    return (HuCPU.IRQStatusRead(3, true) & 2) ? 1 : 0;
}
