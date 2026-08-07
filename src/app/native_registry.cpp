// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/app/native_registry.hpp"

#include <array>

namespace devils_crush::app {

namespace {

// The parameter names are prefixed because the preprocessor would otherwise
// substitute a bare parameter into the matching designator below.
#define DC_ADMITTED_ROUTINE(                                       \
    dc_bank, dc_pc, dc_native_calls, dc_observed_calls, dc_evidence) \
    AdmittedRoutine{                                               \
        .routine =                                                 \
            {                                                      \
                .physical_bank = (dc_bank),                        \
                .logical_pc = (dc_pc),                             \
            },                                                     \
        .proven_native_calls = (dc_native_calls),                  \
        .proven_observed_calls = (dc_observed_calls),              \
        .evidence = (dc_evidence),                                 \
    },

constexpr std::array kAdmittedRoutines{
#include "native_registry_table.inc"
};

#undef DC_ADMITTED_ROUTINE

} // namespace

std::span<const AdmittedRoutine> admitted_native_routines()
{
    return kAdmittedRoutines;
}

} // namespace devils_crush::app
