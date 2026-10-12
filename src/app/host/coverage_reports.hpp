// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"

namespace devils_crush::app::host {

void report_psg_coverage(
    const devils_crush::emulation::PceRuntime& runtime);

void report_vdc_coverage(
    const devils_crush::emulation::PceRuntime& runtime);

void report_vce_coverage(
    const devils_crush::emulation::PceRuntime& runtime);

void report_irq_coverage(
    const devils_crush::emulation::PceRuntime& runtime);

void report_scheduler_coverage(
    const devils_crush::emulation::PceRuntime& runtime);

void report_boundary_coverage(
    const devils_crush::emulation::PceRuntime& runtime);

void report_idle_coverage(
    const devils_crush::emulation::PceRuntime& runtime);

void report_pad_coverage(
    const devils_crush::emulation::PceRuntime& runtime);

} // namespace devils_crush::app::host
