// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <span>

namespace devils_crush::native_runtime {

// A bonus stage entered through a table feature returns the ball to that
// feature in a held state. A command-line launch has no owning feature, so
// release that hold once the cartridge has restored the exact saved ball and
// installed its normal off-table continuation. This must only be called for a
// synthetically launched bonus session.
[[nodiscard]] bool release_synthetic_bonus_return_ball(
    std::span<std::byte> work_ram) noexcept;

} // namespace devils_crush::native_runtime
