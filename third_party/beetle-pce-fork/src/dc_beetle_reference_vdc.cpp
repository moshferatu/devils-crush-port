// SPDX-License-Identifier: GPL-3.0-or-later

#include "dc_beetle_reference_vdc.h"

#include <mednafen/mednafen.h>

#include <mednafen/hw_video/huc6270/vdc.h>

// A thin lifetime and naming shim, as the reference PSG is. The extra work
// here is the hooks: `SetIRQHook` and `SetWSHook` take bare function
// pointers with no user data, so the device that owns them has to be found
// some other way. The emulator solves this with a file-scope `extern VCE
// *vce` its own comment calls horrible; this solves it by allowing one
// reference device at a time and pointing at it.
struct dc_beetle_reference_vdc {
    VDC vdc;

    std::uint64_t irq_hook_calls = 0;
    std::uint64_t irq_hook_raises = 0;
    std::uint64_t wait_state_hook_calls = 0;
    std::int64_t wait_state_cycles = 0;

    std::int32_t wait_state_charge = 1;
    std::int32_t wait_state_budget = 455 * 64;
    std::int32_t wait_state_counter = 0;
};

namespace {

dc_beetle_reference_vdc* g_active = nullptr;

// The device calls this with true when it raises an interrupt and false
// when a status read acknowledges one. `VCE` discards the argument and
// re-derives the level from `PeekIRQ`, but the edges are worth keeping
// apart here: they are the sequence a native model has to reproduce.
void irq_hook(const bool raised)
{
    if (g_active != nullptr) {
        ++g_active->irq_hook_calls;
        if (raised) {
            ++g_active->irq_hook_raises;
        }
    }
}

// The device's stall loop is `while(VDC_IS_BSY) { if(!WSHook(-1)) force
// completion; }`, so the hook's answer is not advisory: returning true
// forever spins forever. In the emulator the escape is the budget --
// `VCE::WS_Hook` refuses once it has stolen 455 * 64 cycles since the
// frame began, and the device then completes the blocking transfer itself.
// That budget is reproduced here, because without it the reference device
// hangs the first time a port access lands during a DMA.
bool wait_state_hook(std::int32_t vdc_cycles)
{
    if (g_active == nullptr) {
        return false;
    }

    ++g_active->wait_state_hook_calls;
    g_active->wait_state_cycles += vdc_cycles;

    bool granted = true;
    std::int32_t to_steal = g_active->wait_state_charge;

    if (to_steal + g_active->wait_state_counter
        > g_active->wait_state_budget) {
        to_steal = g_active->wait_state_budget - g_active->wait_state_counter;
        if (to_steal < 0) {
            to_steal = 0;
        }
        granted = false;
    }

    g_active->wait_state_counter += to_steal;
    return granted;
}

}  // namespace

dc_beetle_reference_vdc* dc_beetle_reference_vdc_create(
    const std::uint32_t vram_size)
{
    if (g_active != nullptr) {
        return nullptr;
    }

    auto* const self = new dc_beetle_reference_vdc();

    self->vdc.SetVRAMSize(vram_size);
    self->vdc.SetIRQHook(irq_hook);
    self->vdc.SetWSHook(wait_state_hook);
    self->vdc.SetLayerEnableMask(0x3);
    self->vdc.SetUnlimitedSprites(false);

    g_active = self;
    return self;
}

void dc_beetle_reference_vdc_destroy(dc_beetle_reference_vdc* const vdc)
{
    if (vdc == nullptr) {
        return;
    }

    if (g_active == vdc) {
        g_active = nullptr;
    }

    delete vdc;
}

std::int32_t dc_beetle_reference_vdc_reset(dc_beetle_reference_vdc* const vdc)
{
    return vdc->vdc.Reset();
}

void dc_beetle_reference_vdc_write(
    dc_beetle_reference_vdc* const vdc,
    const std::uint32_t address,
    const std::uint8_t value,
    std::int32_t* const next_event)
{
    vdc->vdc.Write(address, value, *next_event);
}

std::uint8_t dc_beetle_reference_vdc_read(
    dc_beetle_reference_vdc* const vdc,
    const std::uint32_t address,
    std::int32_t* const next_event,
    const int peek)
{
    return vdc->vdc.Read(address, *next_event, peek != 0);
}

std::int32_t dc_beetle_reference_vdc_run(
    dc_beetle_reference_vdc* const vdc,
    const std::int32_t clocks,
    std::uint16_t* const pixels,
    const int skip)
{
    return vdc->vdc.Run(clocks, pixels, skip != 0);
}

std::int32_t dc_beetle_reference_vdc_hsync(
    dc_beetle_reference_vdc* const vdc,
    const int in_hblank)
{
    return vdc->vdc.HSync(in_hblank != 0);
}

std::int32_t dc_beetle_reference_vdc_vsync(
    dc_beetle_reference_vdc* const vdc,
    const int in_vblank)
{
    return vdc->vdc.VSync(in_vblank != 0);
}

std::uint16_t dc_beetle_reference_vdc_peek_vram(
    dc_beetle_reference_vdc* const vdc,
    const std::uint16_t address)
{
    return vdc->vdc.PeekVRAM(address);
}

std::uint16_t dc_beetle_reference_vdc_peek_sat(
    dc_beetle_reference_vdc* const vdc,
    const std::uint8_t address)
{
    return vdc->vdc.PeekSAT(address);
}

std::uint32_t dc_beetle_reference_vdc_get_register(
    dc_beetle_reference_vdc* const vdc,
    const unsigned id)
{
    return vdc->vdc.GetRegister(id);
}

int dc_beetle_reference_vdc_peek_irq(dc_beetle_reference_vdc* const vdc)
{
    return vdc->vdc.PeekIRQ() ? 1 : 0;
}

std::uint64_t dc_beetle_reference_vdc_irq_hook_calls(
    const dc_beetle_reference_vdc* const vdc)
{
    return vdc->irq_hook_calls;
}

std::uint64_t dc_beetle_reference_vdc_irq_hook_raises(
    const dc_beetle_reference_vdc* const vdc)
{
    return vdc->irq_hook_raises;
}

std::uint64_t dc_beetle_reference_vdc_wait_state_hook_calls(
    const dc_beetle_reference_vdc* const vdc)
{
    return vdc->wait_state_hook_calls;
}

std::int64_t dc_beetle_reference_vdc_wait_state_cycles(
    const dc_beetle_reference_vdc* const vdc)
{
    return vdc->wait_state_cycles;
}

void dc_beetle_reference_vdc_set_wait_state_policy(
    dc_beetle_reference_vdc* const vdc,
    const std::int32_t charge_per_call,
    const std::int32_t budget)
{
    vdc->wait_state_charge = charge_per_call;
    vdc->wait_state_budget = budget;
}

void dc_beetle_reference_vdc_reset_wait_states(
    dc_beetle_reference_vdc* const vdc)
{
    vdc->wait_state_counter = 0;
}

std::int32_t dc_beetle_reference_vdc_wait_state_counter(
    const dc_beetle_reference_vdc* const vdc)
{
    return vdc->wait_state_counter;
}
