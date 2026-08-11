// SPDX-License-Identifier: GPL-3.0-or-later

#include "dc_beetle_reference_irq.h"

#include <mednafen/mednafen.h>

#include <mednafen/pce/pce.h>

// Every field this device has lives in the emulator's processor, which is a
// global, so there is genuinely nothing to hold here. The handle exists to
// keep the shim's shape the same as the other three and to make "only one at
// a time" enforceable.
struct dc_beetle_reference_irq {
    bool live = true;
};

namespace {

dc_beetle_reference_irq* g_active = nullptr;

// Far enough away that the processor never decides on its own that a user
// event is due, which leaves the timer's divider as the only thing bounding
// its next event. Installed so nothing here can reach a null handler.
constexpr int32 kFarUserEvent = 0x3FFFFFF;

int32 MDFN_FASTCALL reference_event_handler(const int32)
{
    return kFarUserEvent;
}

// Re-arms the user event and, through `SetEvent`, recomputes the horizon.
// Called after anything that moves the clock or the divider, which is
// exactly where the machine's own callers call `CalcNextEvent`.
void arm_processor(void)
{
    HuCPU.SetEventHandler(reference_event_handler);
    HuCPU.SetEvent(kFarUserEvent);
}

}  // namespace

dc_beetle_reference_irq* dc_beetle_reference_irq_create(void)
{
    if (g_active != nullptr) {
        return nullptr;
    }

    auto* const self = new dc_beetle_reference_irq();

    HuCPU.Init(false);
    arm_processor();

    g_active = self;
    return self;
}

void dc_beetle_reference_irq_destroy(dc_beetle_reference_irq* const self)
{
    if (self == nullptr) {
        return;
    }

    if (g_active == self) {
        g_active = nullptr;
    }

    delete self;
}

void dc_beetle_reference_irq_power(dc_beetle_reference_irq* const self)
{
    (void)self;

    HuCPU.Power();
    arm_processor();
}

void dc_beetle_reference_irq_reset(dc_beetle_reference_irq* const self)
{
    (void)self;

    HuCPU.Reset();
    arm_processor();
}

void dc_beetle_reference_irq_reset_timestamp(
    dc_beetle_reference_irq* const self,
    const uint32_t base)
{
    (void)self;

    HuCPU.SyncAndResetTimestamp(base);
    arm_processor();
}

void dc_beetle_reference_irq_acknowledge_reset(
    dc_beetle_reference_irq* const self)
{
    (void)self;

    HuCPU.DcAcknowledgeReset();
}

void dc_beetle_reference_irq_begin(
    dc_beetle_reference_irq* const self,
    const uint32_t lines)
{
    (void)self;

    HuCPU.IRQBegin((int)lines);
}

void dc_beetle_reference_irq_end(
    dc_beetle_reference_irq* const self,
    const uint32_t lines)
{
    (void)self;

    HuCPU.IRQEnd((int)lines);
}

uint32_t dc_beetle_reference_irq_advance(
    dc_beetle_reference_irq* const self,
    const int32_t master_clocks)
{
    (void)self;

    // The timer catches up on its own inside this, at whichever moments its
    // divider comes due.
    HuCPU.StealMasterCycles(master_clocks);

    // The last of those catch-ups lands on the tick, not on the timestamp,
    // so this is what puts the device level with the clock. It is what
    // every one of the emulator's own register entry points does first.
    HuCPU.TimerSync();
    arm_processor();

    return HuCPU.Timestamp();
}

uint32_t dc_beetle_reference_irq_timestamp(
    dc_beetle_reference_irq* const self)
{
    (void)self;

    return HuCPU.Timestamp();
}

uint8_t dc_beetle_reference_irq_read_timer(
    dc_beetle_reference_irq* const self,
    const uint32_t address)
{
    (void)self;

    // `IORead`'s $0C00 case: read, then latch what was read onto the bus.
    const uint8 value = HuCPU.TimerRead(address, false);
    HuCPU.SetIODataBuffer(value);
    arm_processor();

    return value;
}

void dc_beetle_reference_irq_write_timer(
    dc_beetle_reference_irq* const self,
    const uint32_t address,
    const uint8_t value)
{
    (void)self;

    // `IOWrite`'s $0C00 case: latch first, then write.
    HuCPU.SetIODataBuffer(value);
    HuCPU.TimerWrite(address & 0x1FFF, value);
    arm_processor();
}

uint8_t dc_beetle_reference_irq_read_status(
    dc_beetle_reference_irq* const self,
    const uint32_t address)
{
    (void)self;

    const uint8 value = HuCPU.IRQStatusRead(address, false);
    HuCPU.SetIODataBuffer(value);

    return value;
}

uint8_t dc_beetle_reference_irq_peek_status(
    dc_beetle_reference_irq* const self,
    const uint32_t address)
{
    (void)self;

    return HuCPU.IRQStatusRead(address, true);
}

void dc_beetle_reference_irq_write_status(
    dc_beetle_reference_irq* const self,
    const uint32_t address,
    const uint8_t value)
{
    (void)self;

    HuCPU.SetIODataBuffer(value);
    HuCPU.IRQStatusWrite(address & 0x1FFF, value);
}

uint8_t dc_beetle_reference_irq_io_data_buffer(
    dc_beetle_reference_irq* const self)
{
    (void)self;

    return HuCPU.GetIODataBuffer();
}

void dc_beetle_reference_irq_set_io_data_buffer(
    dc_beetle_reference_irq* const self,
    const uint8_t value)
{
    (void)self;

    HuCPU.SetIODataBuffer(value);
}

void dc_beetle_reference_irq_set_i_flag(
    dc_beetle_reference_irq* const self,
    const int set)
{
    (void)self;

    HuCPU.DcSetIFlag(set != 0);
}

uint32_t dc_beetle_reference_irq_timer_horizon(
    dc_beetle_reference_irq* const self)
{
    (void)self;

    return HuCPU.DcTimerInterruptHorizon();
}

void dc_beetle_reference_irq_observe(
    dc_beetle_reference_irq* const self,
    dc_beetle_reference_irq_observation* const out)
{
    (void)self;

    HuC6280::DcIrqObservation observation;
    HuCPU.DcObserveIrq(&observation);

    out->timestamp = observation.timestamp;
    out->timer_lastts = observation.timer_lastts;
    out->timer_div = observation.timer_div;
    out->timer_value = observation.timer_value;
    out->timer_load = observation.timer_load;
    out->timer_status = observation.timer_status;
    out->timer_inreload = observation.timer_inreload ? 1 : 0;
    out->irq_low = observation.irq_low;
    out->irq_mask = observation.irq_mask;
    out->irq_sample = observation.irq_sample;
    out->next_event = observation.next_event;
}
