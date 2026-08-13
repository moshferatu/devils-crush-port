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
dc_beetle_replacement_abandoned_hook
    dc_beetle_active_replacement_abandoned_hook = nullptr;
void* dc_beetle_active_replacement_abandoned_hook_user_data = nullptr;
const dc_beetle_psg_model* dc_beetle_active_psg_model = nullptr;
static int32_t* dc_beetle_psg_output_buffers[2] = {nullptr, nullptr};
static int dc_beetle_psg_revision = DC_BEETLE_PSG_REVISION_HUC6280;
const dc_beetle_vdc_model* dc_beetle_active_vdc_model = nullptr;
static uint32_t dc_beetle_vdc_vram_size = 0;
const dc_beetle_vce_model* dc_beetle_active_vce_model = nullptr;
const dc_beetle_irq_model* dc_beetle_active_irq_model = nullptr;
const dc_beetle_scheduler_model* dc_beetle_active_scheduler_model = nullptr;
const dc_beetle_boundary_model* dc_beetle_active_boundary_model = nullptr;
const dc_beetle_idle_model* dc_beetle_active_idle_model = nullptr;
const dc_beetle_pad_model* dc_beetle_active_pad_model = nullptr;
// What the emulator has told this boundary about its controller port. The
// defaults are a machine that has loaded nothing: no multitap, no soft-reset
// suppression, no CD unit, and nothing plugged into any of the five ports.
static int dc_beetle_pad_multitap_enabled = 0;
static int dc_beetle_pad_disable_soft_reset = 0;
static int dc_beetle_pad_cd_attached = 0;
static int dc_beetle_pad_port_devices[DC_BEETLE_PAD_PORT_COUNT] = {
    DC_BEETLE_PAD_DEVICE_NONE, DC_BEETLE_PAD_DEVICE_NONE,
    DC_BEETLE_PAD_DEVICE_NONE, DC_BEETLE_PAD_DEVICE_NONE,
    DC_BEETLE_PAD_DEVICE_NONE
};
// Where the processor's clock lives when no model has taken it over. A
// constant initializer rather than a constructor, so the pointer below is
// valid before anything in this program has run: the processor reads through
// it from its own constructor onwards and has no null case to test.
//
// Zero throughout, including the two fields that have no sensible zero. That
// is not an oversight but the state these fields were in when they were
// members of a zero-initialized processor: the rate is set to one by
// `HuC6280::Init`, which the processor's own constructor runs, and the shift
// is set from the rate by the reset vector before the first cycle it prices.
// A block that arrived pre-corrected would be a different machine between
// those two moments, which is exactly the window a device shim drives in.
static dc_beetle_scheduler_registers dc_beetle_scheduler_own_registers = {
    0, 0, 0, 0, 0, 0, 0, 0
};
dc_beetle_scheduler_registers* dc_beetle_active_scheduler_registers =
    &dc_beetle_scheduler_own_registers;
uint32_t dc_beetle_replacement_nested_installs = 0;
uint32_t dc_beetle_replacement_nest_overflows = 0;
uint32_t dc_beetle_replacement_max_depth = 0;
uint32_t dc_beetle_replacement_abandonments = 0;

void dc_beetle_reset_replacement_nesting_counters(void)
{
    dc_beetle_replacement_nested_installs = 0;
    dc_beetle_replacement_nest_overflows = 0;
    dc_beetle_replacement_max_depth = 0;
    dc_beetle_replacement_abandonments = 0;
}

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

// Separate from the replacement hook because it answers a different question,
// and a host may want one without the other: supplying transcripts is the
// optimization, and being told when one was dropped is the bookkeeping that
// only a host observing the machine needs.
void dc_beetle_set_replacement_abandoned_hook(
    const dc_beetle_replacement_abandoned_hook hook,
    void* const user_data)
{
    dc_beetle_active_replacement_abandoned_hook_user_data = user_data;
    dc_beetle_active_replacement_abandoned_hook = hook;
}

void dc_beetle_clear_replacement_abandoned_hook(void)
{
    dc_beetle_active_replacement_abandoned_hook = nullptr;
    dc_beetle_active_replacement_abandoned_hook_user_data = nullptr;
}

void dc_beetle_set_psg_model(const dc_beetle_psg_model* const model)
{
    dc_beetle_active_psg_model = model;

    if (model != nullptr && model->configure != nullptr
        && dc_beetle_psg_output_buffers[0] != nullptr) {
        model->configure(
            model->user_data,
            dc_beetle_psg_output_buffers[0],
            dc_beetle_psg_output_buffers[1],
            dc_beetle_psg_revision);
    }
}

void dc_beetle_clear_psg_model(void)
{
    dc_beetle_active_psg_model = nullptr;
}

void dc_beetle_set_psg_device(
    int32_t* const left,
    int32_t* const right,
    const int revision)
{
    dc_beetle_psg_output_buffers[0] = left;
    dc_beetle_psg_output_buffers[1] = right;
    dc_beetle_psg_revision = revision;

    if (dc_beetle_active_psg_model != nullptr
        && dc_beetle_active_psg_model->configure != nullptr) {
        dc_beetle_active_psg_model->configure(
            dc_beetle_active_psg_model->user_data,
            left,
            right,
            revision);
    }
}

void dc_beetle_set_vdc_model(const dc_beetle_vdc_model* const model)
{
    dc_beetle_active_vdc_model = model;

    if (model != nullptr && model->configure != nullptr
        && dc_beetle_vdc_vram_size != 0) {
        model->configure(model->user_data, dc_beetle_vdc_vram_size);
    }
}

void dc_beetle_clear_vdc_model(void)
{
    dc_beetle_active_vdc_model = nullptr;
}

void dc_beetle_set_vdc_device(const uint32_t vram_size)
{
    dc_beetle_vdc_vram_size = vram_size;

    if (dc_beetle_active_vdc_model != nullptr
        && dc_beetle_active_vdc_model->configure != nullptr) {
        dc_beetle_active_vdc_model->configure(
            dc_beetle_active_vdc_model->user_data,
            vram_size);
    }
}

void dc_beetle_set_vce_model(const dc_beetle_vce_model* const model)
{
    dc_beetle_active_vce_model = model;
}

void dc_beetle_clear_vce_model(void)
{
    dc_beetle_active_vce_model = nullptr;
}

void dc_beetle_set_irq_model(const dc_beetle_irq_model* const model)
{
    dc_beetle_active_irq_model = model;
}

// Clearing leaves the processor holding the mirror -- a correct divider,
// request register and mask -- but not the counter, the reload byte, the
// enable bit or the phase, which have no mirror and stay with the model. So
// this hands the device back to a machine that will reach its own power-on,
// and not to one in flight. Every caller so far either clears before a ROM
// loads or clears while shutting down.
void dc_beetle_clear_irq_model(void)
{
    dc_beetle_active_irq_model = nullptr;
}

// Installing a scheduler is the one install that moves memory rather than
// merely redirecting calls, so the two assignments below have to happen
// together: a processor reading the old block while dispatching to the new
// model would advance one clock and test another.
//
// Nothing is copied across. The model brings whatever it holds, and the
// `HuC6280::Init` that a ROM load runs is what puts a freshly installed model
// and a machine about to be built on the same footing. That is the same
// before-the-ROM rule the other four models state, arrived at from the one
// direction where breaking it would not merely lose a device's state but
// would move the machine's clock underneath it.
void dc_beetle_set_scheduler_model(
    const dc_beetle_scheduler_model* const model)
{
    if (model == nullptr || model->registers == nullptr) {
        dc_beetle_clear_scheduler_model();
        return;
    }

    dc_beetle_active_scheduler_registers = model->registers;
    dc_beetle_active_scheduler_model = model;
}

void dc_beetle_clear_scheduler_model(void)
{
    dc_beetle_active_scheduler_model = nullptr;
    dc_beetle_active_scheduler_registers = &dc_beetle_scheduler_own_registers;
}

// The simplest install of the six: one pointer, nothing copied, nothing left
// behind. A boundary handed back keeps whatever it had latched and the
// processor's own two fields hold whatever they last held, which is only
// consistent for a machine that will reach its own power-on -- the same rule
// the other five arrive at, and here for the narrowest reason of all.
void dc_beetle_set_boundary_model(const dc_beetle_boundary_model* const model)
{
    dc_beetle_active_boundary_model = model;
}

void dc_beetle_clear_boundary_model(void)
{
    dc_beetle_active_boundary_model = nullptr;
}

// Simpler still, and for a different reason: this one takes nothing over. A
// model without an `iteration_cycles` to ask has no question to answer, so it
// is refused here rather than tested for once per instruction in the loop.
void dc_beetle_set_idle_model(const dc_beetle_idle_model* const model)
{
    if (model == nullptr || model->iteration_cycles == nullptr) {
        dc_beetle_clear_idle_model();
        return;
    }

    dc_beetle_active_idle_model = model;
}

void dc_beetle_clear_idle_model(void)
{
    dc_beetle_active_idle_model = nullptr;
}

// The joypad port. Two pieces of configuration reach a model here rather than
// being read by it: the machine's settings, and what the frontend plugged into
// each port. Both are remembered, because the emulator reports each of them
// exactly once -- at the load that builds its devices -- and a model installed
// after that would otherwise be a model that had never been told.
//
// The remembered port assignments start as "nothing plugged in", which is the
// state a machine that has loaded nothing is in.
void dc_beetle_set_pad_model(const dc_beetle_pad_model* const model)
{
    dc_beetle_active_pad_model = model;

    if (model == nullptr) {
        return;
    }

    if (model->configure != nullptr) {
        model->configure(
            model->user_data,
            dc_beetle_pad_multitap_enabled,
            dc_beetle_pad_disable_soft_reset,
            dc_beetle_pad_cd_attached);
    }

    if (model->configure_port != nullptr) {
        for (unsigned port = 0; port < DC_BEETLE_PAD_PORT_COUNT; port++) {
            model->configure_port(
                model->user_data,
                port,
                dc_beetle_pad_port_devices[port]);
        }
    }
}

void dc_beetle_clear_pad_model(void)
{
    dc_beetle_active_pad_model = nullptr;
}

void dc_beetle_set_pad_settings(
    const int multitap_enabled,
    const int disable_soft_reset,
    const int cd_attached)
{
    dc_beetle_pad_multitap_enabled = multitap_enabled;
    dc_beetle_pad_disable_soft_reset = disable_soft_reset;
    dc_beetle_pad_cd_attached = cd_attached;

    if (dc_beetle_active_pad_model != nullptr
        && dc_beetle_active_pad_model->configure != nullptr) {
        dc_beetle_active_pad_model->configure(
            dc_beetle_active_pad_model->user_data,
            multitap_enabled,
            disable_soft_reset,
            cd_attached);
    }
}

void dc_beetle_set_pad_port(const unsigned port, const int device_type)
{
    if (port >= DC_BEETLE_PAD_PORT_COUNT) {
        return;
    }

    dc_beetle_pad_port_devices[port] = device_type;

    if (dc_beetle_active_pad_model != nullptr
        && dc_beetle_active_pad_model->configure_port != nullptr) {
        dc_beetle_active_pad_model->configure_port(
            dc_beetle_active_pad_model->user_data,
            port,
            device_type);
    }
}

// The timer, as an installed scheduler reaches it. Each of these is the line
// the emulator's own `HappySync`, `CalcNextEvent` and `SyncAndResetTimestamp`
// would have run, left where it is so that it still dispatches to a native
// controller when one is installed and to the processor's own register file
// when none is.
//
// The catch-up and the horizon are two calls rather than one call returning a
// value because they are asked at different moments: a register write can move
// the divider between the catch-up that precedes it and the horizon that
// follows.
void dc_beetle_scheduler_sync_timer(void)
{
    HuCPU.TimerSync();
}

int32_t dc_beetle_scheduler_timer_horizon(void)
{
    return HuCPU.DcTimerHorizon();
}

void dc_beetle_scheduler_rebase_timer(const int32_t base)
{
    HuCPU.DcRebaseTimer(base);
}

// The user event handler. The scheduler calls this at the moment the horizon
// came due, not at the moment it noticed -- the two differ by however far the
// last advance overshot, and every device downstream of the encoder measures
// itself against the answer.
int32_t dc_beetle_scheduler_user_event(const int32_t timestamp)
{
    return HuCPU.DcUserEvent(timestamp);
}

// The emulator's own device passes the direction of the change and the
// encoder throws it away, re-deriving the level from every chip's latched
// status. Reproducing that rather than trusting the argument is what lets one
// chip be native while the other is not.
//
// Both of these reach whichever encoder owns the machine. A native controller
// beside a native encoder must interrupt and stall the encoder that is
// actually clocking it -- and since this is the same decision the encoder
// makes about which controller to clock, only in the other direction, it is
// made in the same place rather than in the caller.
void dc_beetle_vdc_irq_change(int)
{
    if (dc_beetle_active_vce_model != nullptr) {
        dc_beetle_active_vce_model->irq_change_check(
            dc_beetle_active_vce_model->user_data);
        return;
    }
    if (vce != nullptr) {
        vce->IRQChangeCheck();
    }
}

int dc_beetle_vdc_wait_state(const int32_t cycles)
{
    if (dc_beetle_active_vce_model != nullptr) {
        return dc_beetle_active_vce_model->wait_state(
            dc_beetle_active_vce_model->user_data,
            cycles);
    }
    // No encoder means no cycle budget to charge against, and a model that
    // kept asking would never return.
    if (vce == nullptr) {
        return 0;
    }
    return vce->WS_Hook(cycles) ? 1 : 0;
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
