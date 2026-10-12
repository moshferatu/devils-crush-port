// SPDX-License-Identifier: GPL-3.0-or-later
#include "census_writers.hpp"

#include "host_files.hpp"
#include "native_registry_install.hpp"

namespace devils_crush::app::host {

[[nodiscard]] bool write_instruction_profile(
    const std::filesystem::path& path,
    const std::span<const devils_crush::emulation::InstructionProfileEntry>
        entries,
    const std::string_view core_revision,
    const std::string_view scope)
{
    if (entries.empty()) {
        std::cerr << "The instruction profile recorded no executions.\n";
        return false;
    }

    std::error_code error;
    const auto directory = path.parent_path();
    if (!directory.empty()) {
        std::filesystem::create_directories(directory, error);
    }
    if (error) {
        std::cerr << "Could not create instruction profile directory: "
                  << error.message() << '\n';
        return false;
    }

    auto temporary_path = path;
    temporary_path += ".tmp";
    std::ofstream output(
        temporary_path,
        std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "Could not create instruction profile: "
                  << temporary_path.string() << '\n';
        return false;
    }

    const auto admitted = devils_crush::app::admitted_native_routines();
    std::uint64_t total_cycles = 0U;
    std::uint64_t total_executions = 0U;
    for (const auto& entry : entries) {
        total_cycles += entry.master_cycles;
        total_executions += entry.executions;
    }

    output << "# format=devils-crush-instruction-profile-v1\n"
           << "# rom_sha256=" << devils_crush::rom::kSupportedSha256 << '\n'
           << "# core_revision=" << core_revision << '\n'
           << (scope.empty() ? "" : "# scope=") << scope
           << (scope.empty() ? "" : "\n")
           << "# address_count=" << entries.size() << '\n'
           << "# total_executions=" << total_executions << '\n'
           << "# total_master_cycles=" << total_cycles << '\n'
           << "physical_bank,logical_pc,executions,master_cycles,"
              "cycle_share_percent,registry_entry\n";

    for (const auto& entry : entries) {
        const bool registry_entry = std::ranges::any_of(
            admitted,
            [&entry](const devils_crush::app::AdmittedRoutine& routine) {
                return routine.routine == entry.instruction;
            });
        output << std::uppercase << std::hex << std::setfill('0')
               << std::setw(2)
               << static_cast<unsigned>(entry.instruction.physical_bank)
               << ',' << std::setw(4) << entry.instruction.logical_pc
               << std::dec << std::setfill(' ') << ','
               << entry.executions << ',' << entry.master_cycles << ','
               << std::fixed << std::setprecision(4)
               << (total_cycles == 0U
                       ? 0.0
                       : 100.0 * static_cast<double>(entry.master_cycles)
                           / static_cast<double>(total_cycles))
               << std::defaultfloat << ','
               << (registry_entry ? 1 : 0) << '\n';
    }

    output.close();
    if (!output) {
        std::cerr << "Could not finalize instruction profile.\n";
        return false;
    }
    if (!commit_artifact(temporary_path, path, "instruction profile")) {
        return false;
    }

    std::cout << "Instruction profile complete: " << entries.size()
              << " addresses, " << total_cycles << " master cycles\n"
              << "Output: " << path.string() << '\n';
    return true;
}

// Where the route's abandoned transcripts were given up, and why.
//
// The two addresses are the point of the file. A transcript is installed at a
// routine's entry and abandoned at whichever checkpoint it had reached, so the
// address the emulator picks the routine up from is neither the entry nor
// anything the registry names -- it is an interior instruction, and the cycles
// the abandonment costs are spent there and at every address after it. Reading
// this beside an instruction profile is what tells an interior row in the
// profile from a row that belongs to code nobody has ported.
[[nodiscard]] bool write_abandonment_census(
    const std::filesystem::path& path,
    const std::span<const devils_crush::emulation::AbandonmentSite> sites,
    const std::string_view core_revision)
{
    using devils_crush::emulation::AbandonmentReason;
    using devils_crush::emulation::RegisterMismatch;

    // An empty census is a result and not a failure: it says every transcript
    // the route installed ran to its end. The file is written either way, so
    // that "no abandonments" and "the census was never asked for" stay
    // distinguishable to whatever reads the artifact afterwards.
    std::error_code error;
    const auto directory = path.parent_path();
    if (!directory.empty()) {
        std::filesystem::create_directories(directory, error);
    }
    if (error) {
        std::cerr << "Could not create abandonment census directory: "
                  << error.message() << '\n';
        return false;
    }

    auto temporary_path = path;
    temporary_path += ".tmp";
    std::ofstream output(
        temporary_path,
        std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "Could not create abandonment census: "
                  << temporary_path.string() << '\n';
        return false;
    }

    std::uint64_t total = 0U;
    for (const auto& site : sites) {
        total += site.count;
    }

    output << "# format=devils-crush-abandonment-census-v1\n"
           << "# rom_sha256=" << devils_crush::rom::kSupportedSha256 << '\n'
           << "# core_revision=" << core_revision << '\n'
           << "# site_count=" << sites.size() << '\n'
           << "# total_abandonments=" << total << '\n'
           << "routine_physical_bank,routine_logical_pc,"
              "resume_physical_bank,resume_logical_pc,reason,mismatch,"
              "count\n";

    for (const auto& site : sites) {
        const auto* const reason = [&site] {
            switch (site.reason) {
            case AbandonmentReason::registers:
                return "registers";
            case AbandonmentReason::advance:
                return "advance";
            case AbandonmentReason::exhausted:
                return "exhausted";
            }
            return "unknown";
        }();
        // The registers that disagreed, named rather than numbered, and joined
        // because more than one can come back changed at once.
        std::string mismatch;
        const auto add = [&mismatch, &site](
                             const RegisterMismatch bit,
                             const std::string_view name) {
            if ((site.mismatch & static_cast<std::uint8_t>(bit)) == 0U) {
                return;
            }
            if (!mismatch.empty()) {
                mismatch += '+';
            }
            mismatch += name;
        };
        add(RegisterMismatch::accumulator, "a");
        add(RegisterMismatch::x, "x");
        add(RegisterMismatch::y, "y");
        add(RegisterMismatch::status, "p");
        add(RegisterMismatch::mpr, "mpr");
        if (mismatch.empty()) {
            mismatch = "none";
        }
        output << std::uppercase << std::hex << std::setfill('0')
               << std::setw(2)
               << static_cast<unsigned>(site.routine.physical_bank) << ','
               << std::setw(4) << site.routine.logical_pc << ','
               << std::setw(2)
               << static_cast<unsigned>(site.resume.physical_bank) << ','
               << std::setw(4) << site.resume.logical_pc << std::dec
               << std::setfill(' ') << ',' << reason << ',' << mismatch
               << ',' << site.count << '\n';
    }

    output.close();
    if (!output) {
        std::cerr << "Could not finalize abandonment census.\n";
        return false;
    }
    if (!commit_artifact(temporary_path, path, "abandonment census")) {
        return false;
    }

    if (sites.empty()) {
        std::cout << "Abandonment census complete: no transcript was "
                     "abandoned\n";
    } else {
        std::cout << "Abandonment census complete: " << sites.size()
                  << " sites, " << total << " abandonments\n";
    }
    std::cout << "Output: " << path.string() << '\n';
    return true;
}

[[nodiscard]] bool write_native_cycle_census(
    const std::filesystem::path& path,
    const std::span<const RegistryCensusRow> rows,
    const NativeCycleSpan& span,
    const std::string_view core_revision)
{
    if (rows.empty()) {
        return false;
    }

    std::error_code error;
    const auto directory = path.parent_path();
    if (!directory.empty()) {
        std::filesystem::create_directories(directory, error);
    }
    if (error) {
        std::cerr << "Could not create native cycle census directory: "
                  << error.message() << '\n';
        return false;
    }

    auto temporary_path = path;
    temporary_path += ".tmp";
    std::ofstream output(
        temporary_path,
        std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "Could not create native cycle census: "
                  << temporary_path.string() << '\n';
        return false;
    }

    std::uint64_t native_cycles = 0U;
    for (const auto& row : rows) {
        native_cycles += row.native_master_cycles;
    }
    const auto elapsed = span.elapsed();

    output << "# format=devils-crush-native-cycle-census-v1\n"
           << "# rom_sha256=" << devils_crush::rom::kSupportedSha256 << '\n'
           << "# core_revision=" << core_revision << '\n'
           << "# admitted_routines=" << rows.size() << '\n'
           << "# elapsed_master_cycles=" << elapsed << '\n'
           << "# native_master_cycles=" << native_cycles << '\n'
           << "physical_bank,logical_pc,entries,native_calls,"
              "declined_at_horizon,native_master_cycles,installed,evidence\n";

    for (const auto& row : rows) {
        output << std::uppercase << std::hex << std::setfill('0')
               << std::setw(2)
               << static_cast<unsigned>(row.routine.physical_bank) << ','
               << std::setw(4) << row.routine.logical_pc
               << std::dec << std::setfill(' ') << ','
               << row.entries << ',' << row.accepted << ','
               << row.declined_at_horizon << ','
               << row.native_master_cycles << ','
               << (row.skipped ? 0 : 1) << ','
               << row.evidence << '\n';
    }

    output.close();
    if (!output) {
        std::cerr << "Could not finalize native cycle census.\n";
        return false;
    }
    if (!commit_artifact(temporary_path, path, "native cycle census")) {
        return false;
    }

    std::cout << "Native cycle census complete: " << rows.size()
              << " admitted routines\nOutput: " << path.string() << '\n';
    return true;
}

} // namespace devils_crush::app::host
