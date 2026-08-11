// SPDX-License-Identifier: GPL-3.0-or-later

#include "dc_beetle_reference_scheduler.h"

#include <mednafen/mednafen.h>

#include <mednafen/pce/pce.h>

// The clock lives in the emulator's processor, which is a global, so the
// only thing genuinely held here is what the installed event handler is
// asked to return and what it has seen. That handler runs from inside
// `HappySync` with no argument to reach a handle by, which is why the
// active shim is a file-scope pointer rather than something passed in.
struct dc_beetle_reference_scheduler {
    int32_t interval = 0x3FFFFFF;
    uint64_t count = 0;
    uint64_t digest = 0xCBF29CE484222325ull;
    int32_t last_timestamp = 0;
};

namespace {

dc_beetle_reference_scheduler* g_active = nullptr;

// FNV-1a over the four bytes of each timestamp, so the sequence of moments
// the handler was called at is compared rather than just how many there
// were.
void absorb(dc_beetle_reference_scheduler* const self, const int32 timestamp)
{
    const uint32 bits = (uint32)timestamp;

    for (int i = 0; i < 4; i++) {
        self->digest ^= (uint64_t)((bits >> (i * 8)) & 0xFF);
        self->digest *= 0x100000001B3ull;
    }
}

// The rate, and the shift the emulator shadows it with. Two of the processor's
// scheduler fields have no public entry point that moves them without
// executing an instruction -- `speed` is written by CSL and CSH, the loop flag
// by `Run`, which runs -- so a scheduler driven without a processor reaches
// them the way a native machine will, by writing the block the processor is
// using. The derivation is duplicated here rather than called, which is the
// price of not carrying a test-only setter in the fork.
void set_speed(const uint8_t speed)
{
    dc_beetle_active_scheduler_registers->speed = speed;
    dc_beetle_active_scheduler_registers->speed_shift =
        (uint8_t)((speed ^ 1) << 1);
}

int32 MDFN_FASTCALL reference_event_handler(const int32 timestamp)
{
    dc_beetle_reference_scheduler* const self = g_active;

    if (self == nullptr) {
        return 0x3FFFFFF;
    }

    self->count++;
    self->last_timestamp = timestamp;
    absorb(self, timestamp);

    return self->interval;
}

}  // namespace

dc_beetle_reference_scheduler* dc_beetle_reference_scheduler_create(void)
{
    if (g_active != nullptr) {
        return nullptr;
    }

    auto* const self = new dc_beetle_reference_scheduler();

    g_active = self;

    HuCPU.Init(false);
    HuCPU.SetEventHandler(reference_event_handler);

    // The two fields `Init` leaves alone. A test that started from whatever
    // the last thing in this process left behind would be comparing two
    // arms that had not agreed on where they began.
    set_speed(0);
    dc_beetle_active_scheduler_registers->run_flag = 0;

    return self;
}

void dc_beetle_reference_scheduler_destroy(
    dc_beetle_reference_scheduler* const self)
{
    if (self == nullptr) {
        return;
    }

    if (g_active == self) {
        g_active = nullptr;
    }

    delete self;
}

void dc_beetle_reference_scheduler_init(
    dc_beetle_reference_scheduler* const self)
{
    (void)self;

    HuCPU.Init(false);
    HuCPU.SetEventHandler(reference_event_handler);
}

void dc_beetle_reference_scheduler_reset(
    dc_beetle_reference_scheduler* const self)
{
    (void)self;

    HuCPU.Reset();
}

void dc_beetle_reference_scheduler_power(
    dc_beetle_reference_scheduler* const self)
{
    (void)self;

    HuCPU.Power();
}

void dc_beetle_reference_scheduler_advance(
    dc_beetle_reference_scheduler* const self,
    const int32_t cycles)
{
    (void)self;

    HuCPU.StealCycles((int)cycles);
}

void dc_beetle_reference_scheduler_advance_master(
    dc_beetle_reference_scheduler* const self,
    const int32_t master_clocks)
{
    (void)self;

    HuCPU.StealMasterCycles((int)master_clocks);
}

void dc_beetle_reference_scheduler_set_event(
    dc_beetle_reference_scheduler* const self,
    const int32_t cycles)
{
    (void)self;

    HuCPU.SetEvent((int32)cycles);
}

void dc_beetle_reference_scheduler_recalculate(
    dc_beetle_reference_scheduler* const self)
{
    (void)self;

    HuC6280::DcSchedulerObservation observation;
    HuCPU.DcObserveScheduler(&observation);

    // `SetEvent` is `next_user_event = cycles; CalcNextEvent();`, so handing
    // it the value it already holds leaves the first statement a no-op and
    // the second the whole of the call.
    HuCPU.SetEvent(observation.next_user_event);
}

void dc_beetle_reference_scheduler_sync_timer(
    dc_beetle_reference_scheduler* const self)
{
    (void)self;

    HuCPU.TimerSync();
}

void dc_beetle_reference_scheduler_reset_timestamp(
    dc_beetle_reference_scheduler* const self,
    const int32_t base)
{
    (void)self;

    HuCPU.SyncAndResetTimestamp((uint32)base);
}

void dc_beetle_reference_scheduler_set_overclock(
    dc_beetle_reference_scheduler* const self,
    const uint32_t rate)
{
    (void)self;

    HuCPU.SetOverclock((uint32)rate);
}

void dc_beetle_reference_scheduler_set_speed(
    dc_beetle_reference_scheduler* const self,
    const uint8_t speed)
{
    (void)self;

    set_speed(speed);
}

void dc_beetle_reference_scheduler_run(
    dc_beetle_reference_scheduler* const self,
    const int step_mode)
{
    (void)self;

    dc_beetle_active_scheduler_registers->run_flag = step_mode ? -1 : 1;
}

void dc_beetle_reference_scheduler_exit(
    dc_beetle_reference_scheduler* const self)
{
    (void)self;

    HuCPU.Exit();
}

void dc_beetle_reference_scheduler_write_timer(
    dc_beetle_reference_scheduler* const self,
    const uint32_t address,
    const uint8_t value)
{
    (void)self;

    // `IOWrite`'s $0C00 case: latch first, then write.
    HuCPU.SetIODataBuffer(value);
    HuCPU.TimerWrite(address & 0x1FFF, value);
}

void dc_beetle_reference_scheduler_set_event_interval(
    dc_beetle_reference_scheduler* const self,
    const int32_t interval)
{
    if (self == nullptr) {
        return;
    }

    self->interval = interval;
}

void dc_beetle_reference_scheduler_events(
    dc_beetle_reference_scheduler* const self,
    dc_beetle_reference_scheduler_event_log* const out)
{
    out->count = self->count;
    out->digest = self->digest;
    out->last_timestamp = self->last_timestamp;
}

void dc_beetle_reference_scheduler_observe(
    dc_beetle_reference_scheduler* const self,
    dc_beetle_reference_scheduler_observation* const out)
{
    (void)self;

    HuC6280::DcSchedulerObservation observation;
    HuCPU.DcObserveScheduler(&observation);

    out->timestamp = observation.timestamp;
    out->timestamp_frac = observation.timestamp_frac;
    out->overclock_rate = observation.overclock_rate;
    out->next_event = observation.next_event;
    out->next_user_event = observation.next_user_event;
    out->runrunrun = observation.runrunrun;
    out->speed = observation.speed;
    out->speed_shift_cache = observation.speed_shift_cache;
}
