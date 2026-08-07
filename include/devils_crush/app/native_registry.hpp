// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/emulation/pce_runtime.hpp"

#include <cstdint>
#include <span>
#include <string_view>

namespace devils_crush::app {

// A routine whose native replacement has been admitted for every naturally
// observed call at its address by a ROM regression. Routines proven only for a
// single recorded ordinal are excluded: an ordinal identifies one call on one
// deterministic route and has no meaning during live play.
struct AdmittedRoutine {
    emulation::RoutineKey routine;

    // Calls the regression proved native, and calls it observed in total. A
    // shortfall is expected and safe: the dispatcher declines states it has not
    // been proven against, and those calls run the original assembly.
    std::uint64_t proven_native_calls = 0;
    std::uint64_t proven_observed_calls = 0;

    // Owning regression, suffixed with `+N` when sibling regressions also prove
    // the address. `docs/NATIVE_REGISTRY.md` lists the full provenance.
    std::string_view evidence;
};

// The admitted set, sorted by physical bank then logical program counter.
[[nodiscard]] std::span<const AdmittedRoutine> admitted_native_routines();

} // namespace devils_crush::app
