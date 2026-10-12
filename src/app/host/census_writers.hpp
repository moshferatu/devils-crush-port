// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"
#include "native_registry_install.hpp"

namespace devils_crush::app::host {

[[nodiscard]] bool write_instruction_profile(
    const std::filesystem::path& path,
    const std::span<const devils_crush::emulation::InstructionProfileEntry>
        entries,
    const std::string_view core_revision,
    const std::string_view scope = {});

[[nodiscard]] bool write_abandonment_census(
    const std::filesystem::path& path,
    const std::span<const devils_crush::emulation::AbandonmentSite> sites,
    const std::string_view core_revision);

[[nodiscard]] bool write_native_cycle_census(
    const std::filesystem::path& path,
    const std::span<const RegistryCensusRow> rows,
    const NativeCycleSpan& span,
    const std::string_view core_revision);

} // namespace devils_crush::app::host
