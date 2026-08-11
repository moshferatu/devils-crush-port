// SPDX-License-Identifier: GPL-3.0-or-later

#include "dc_beetle_reference_boundary.h"

#include <mednafen/mednafen.h>

#include <mednafen/pce/pce.h>

// The boundary lives in the emulator's processor, which is a global, and
// this device holds no state of its own at all. The handle exists only so
// that the "one at a time" rule this series relies on has something to
// enforce itself with.
struct dc_beetle_reference_boundary {
    int placeholder = 0;
};

namespace {

dc_beetle_reference_boundary* g_active = nullptr;

}  // namespace

dc_beetle_reference_boundary* dc_beetle_reference_boundary_create(void)
{
    if (g_active != nullptr) {
        return nullptr;
    }

    auto* const self = new dc_beetle_reference_boundary();

    g_active = self;

    // A processor whose latch holds whatever the last thing in this process
    // left behind is not a boundary a test can start from. `Reset` is the
    // machine's own answer to that, and it is also the one operation this
    // device has.
    HuCPU.Reset();

    return self;
}

void dc_beetle_reference_boundary_destroy(
    dc_beetle_reference_boundary* const self)
{
    if (self == nullptr) {
        return;
    }

    if (g_active == self) {
        g_active = nullptr;
    }

    delete self;
}

void dc_beetle_reference_boundary_reset(
    dc_beetle_reference_boundary* const self)
{
    (void)self;

    HuCPU.Reset();
}

void dc_beetle_reference_boundary_set_lines(
    dc_beetle_reference_boundary* const self,
    const uint32_t requests,
    const uint8_t mask)
{
    (void)self;

    HuCPU.DcSetBoundaryLines((uint32)requests, (uint8)mask);
}

void dc_beetle_reference_boundary_set_i_flag(
    dc_beetle_reference_boundary* const self,
    const int set)
{
    (void)self;

    HuCPU.DcSetIFlag(set != 0);
}

void dc_beetle_reference_boundary_set_sample(
    dc_beetle_reference_boundary* const self,
    const int32_t irq_sample,
    const int32_t i_flag_sample)
{
    (void)self;

    HuCPU.DcSetBoundarySample((int32)irq_sample, (int32)i_flag_sample);
}

void dc_beetle_reference_boundary_sample(
    dc_beetle_reference_boundary* const self)
{
    (void)self;

    HuCPU.DcSampleInterrupt();
}

int dc_beetle_reference_boundary_select(
    dc_beetle_reference_boundary* const self)
{
    (void)self;

    return (int)HuCPU.DcSelectInterrupt();
}

uint32_t dc_beetle_reference_boundary_vector(const int decision)
{
    return (uint32_t)HuC6280::DcInterruptVector(
        (HuC6280::DcInterruptDecision)decision);
}

void dc_beetle_reference_boundary_acknowledge_reset(
    dc_beetle_reference_boundary* const self)
{
    (void)self;

    HuCPU.DcAcknowledgeInterruptSample();
}

void dc_beetle_reference_boundary_observe(
    dc_beetle_reference_boundary* const self,
    dc_beetle_reference_boundary_observation* const out)
{
    (void)self;

    HuC6280::DcBoundaryObservation observation;
    HuCPU.DcObserveBoundary(&observation);

    out->irq_low = observation.irq_low;
    out->irq_mask = observation.irq_mask;
    out->irq_sample = observation.irq_sample;
    out->i_flag_sample = observation.i_flag_sample;
}
