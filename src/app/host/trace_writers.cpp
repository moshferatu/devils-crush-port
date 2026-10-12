// SPDX-License-Identifier: GPL-3.0-or-later
#include "trace_writers.hpp"

#include "command_line.hpp"
#include "host_files.hpp"

namespace devils_crush::app::host {

[[nodiscard]] std::string sha256(
    const std::span<const std::byte> bytes)
{
    return devils_crush::rom::compute_digests(bytes).sha256;
}

bool write_pcm16_wave_header(
    std::ostream& output,
    const std::uint32_t sample_rate,
    const std::uint32_t data_bytes)
{
    constexpr char riff[] = {'R', 'I', 'F', 'F'};
    constexpr char wave[] = {'W', 'A', 'V', 'E'};
    constexpr char format[] = {'f', 'm', 't', ' '};
    constexpr char data[] = {'d', 'a', 't', 'a'};
    constexpr std::uint32_t format_size = 16U;
    constexpr std::uint16_t pcm_format = 1U;
    constexpr std::uint16_t channels = 2U;
    constexpr std::uint16_t bits_per_sample = 16U;
    constexpr std::uint16_t block_alignment =
        channels * bits_per_sample / 8U;
    const std::uint32_t riff_size = 36U + data_bytes;
    const std::uint32_t bytes_per_second =
        sample_rate * block_alignment;

    output.write(riff, sizeof(riff));
    output.write(
        reinterpret_cast<const char*>(&riff_size),
        sizeof(riff_size));
    output.write(wave, sizeof(wave));
    output.write(format, sizeof(format));
    output.write(
        reinterpret_cast<const char*>(&format_size),
        sizeof(format_size));
    output.write(
        reinterpret_cast<const char*>(&pcm_format),
        sizeof(pcm_format));
    output.write(
        reinterpret_cast<const char*>(&channels),
        sizeof(channels));
    output.write(
        reinterpret_cast<const char*>(&sample_rate),
        sizeof(sample_rate));
    output.write(
        reinterpret_cast<const char*>(&bytes_per_second),
        sizeof(bytes_per_second));
    output.write(
        reinterpret_cast<const char*>(&block_alignment),
        sizeof(block_alignment));
    output.write(
        reinterpret_cast<const char*>(&bits_per_sample),
        sizeof(bits_per_sample));
    output.write(data, sizeof(data));
    output.write(
        reinterpret_cast<const char*>(&data_bytes),
        sizeof(data_bytes));
    return static_cast<bool>(output);
}

[[nodiscard]] bool write_routine_trace(
    const std::filesystem::path& path,
    const std::span<const RoutineKey> routines,
    const std::span<const RoutineEntry> entries,
    const std::string_view core_revision)
{
    if (routines.empty()) {
        return false;
    }

    std::error_code error;
    const auto directory = path.parent_path();
    if (!directory.empty()) {
        std::filesystem::create_directories(directory, error);
    }
    if (error) {
        std::cerr << "Could not create routine-trace directory: "
                  << error.message() << '\n';
        return false;
    }

    auto temporary_path = path;
    temporary_path += ".tmp";
    std::ofstream output(
        temporary_path,
        std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "Could not create routine trace: "
                  << temporary_path.string() << '\n';
        return false;
    }

    output
        << "# format=devils-crush-routine-entry-v"
        << (routines.size() == 1U ? 1 : 2) << '\n'
        << "# rom_sha256=" << devils_crush::rom::kSupportedSha256 << '\n'
        << "# core_revision=" << core_revision << '\n';
    if (routines.size() == 1U) {
        output << "# physical_bank=" << std::uppercase << std::hex
               << std::setfill('0') << std::setw(2)
               << static_cast<unsigned>(routines.front().physical_bank)
               << '\n'
               << "# logical_pc=" << std::setw(4)
               << routines.front().logical_pc << '\n';
    } else {
        output << "# routine_count=" << std::dec << routines.size() << '\n';
        std::size_t index = 0U;
        for (const auto routine : routines) {
            output << "# routine_" << ++index << '=' << std::uppercase
                   << std::hex << std::setfill('0') << std::setw(2)
                   << static_cast<unsigned>(routine.physical_bank) << ':'
                   << std::setw(4) << routine.logical_pc << '\n';
        }
    }
    output
        << std::dec
        << "sequence,frame,master_cycle,physical_bank,logical_pc,"
           "a,x,y,sp,status,mpr0,mpr1,mpr2,mpr3,mpr4,mpr5,mpr6,mpr7\n";

    std::uint64_t sequence = 0;
    for (const auto& entry : entries) {
        const auto& registers = entry.registers;
        output << ++sequence << ',' << entry.frame_number << ','
               << entry.master_cycle << ',' << std::uppercase << std::hex
               << std::setfill('0') << std::setw(2)
               << static_cast<unsigned>(entry.routine.physical_bank)
               << ',' << std::setw(4) << entry.routine.logical_pc
               << ',' << std::setw(2)
               << static_cast<unsigned>(registers.accumulator)
               << ',' << std::setw(2)
               << static_cast<unsigned>(registers.x)
               << ',' << std::setw(2)
               << static_cast<unsigned>(registers.y)
               << ',' << std::setw(2)
               << static_cast<unsigned>(registers.stack_pointer)
               << ',' << std::setw(2)
               << static_cast<unsigned>(registers.status);
        for (const auto mpr : registers.mpr) {
            output << ',' << std::setw(2)
                   << static_cast<unsigned>(mpr);
        }
        output << std::dec << '\n';
    }

    output.close();
    if (!output) {
        std::cerr << "Could not finalize routine trace.\n";
        return false;
    }
    if (!commit_artifact(temporary_path, path, "routine trace")) {
        return false;
    }

    std::cout << "Routine trace complete: " << entries.size()
              << " entries\nOutput: " << path.string() << '\n';
    return true;
}

[[nodiscard]] bool write_playfield_collision_sources(
    const std::filesystem::path& path,
    const std::span<const devils_crush::emulation::PlayfieldCollisionSource>
        sources,
    const std::string_view core_revision)
{
    if (sources.empty()) {
        return false;
    }

    std::error_code error;
    const auto directory = path.parent_path();
    if (!directory.empty()) {
        std::filesystem::create_directories(directory, error);
    }
    if (error) {
        std::cerr << "Could not create playfield-collision source directory: "
                  << error.message() << '\n';
        return false;
    }

    auto temporary_path = path;
    temporary_path += ".tmp";
    std::ofstream output(
        temporary_path,
        std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "Could not create playfield-collision source capture: "
                  << temporary_path.string() << '\n';
        return false;
    }

    output << "# format=devils-crush-playfield-collision-source-v1\n"
           << "# rom_sha256=" << devils_crush::rom::kSupportedSha256 << '\n'
           << "# core_revision=" << core_revision << '\n'
           << "# source_count=" << sources.size() << '\n'
           << "sequence,tile_ids,collision_attributes,masks,"
              "mpr0,mpr1,mpr2,mpr3,mpr4,mpr5,mpr6,mpr7\n";
    std::size_t sequence = 0U;
    for (const auto& source : sources) {
        output << ++sequence << ',' << std::uppercase << std::hex
               << std::setfill('0') << std::setw(4) << source.tables.tile_ids
               << ',' << std::setw(4) << source.tables.collision_attributes
               << ',' << std::setw(4) << source.tables.masks;
        for (const auto mpr : source.mpr) {
            output << ',' << std::setw(2) << static_cast<unsigned>(mpr);
        }
        output << std::dec << '\n';
    }
    output.close();
    if (!output) {
        std::cerr << "Could not finalize playfield-collision source capture.\n";
        return false;
    }
    if (!commit_artifact(
            temporary_path, path, "playfield-collision source capture")) {
        return false;
    }
    std::cout << "Playfield-collision sources complete: " << sources.size()
              << " distinct source(s)\nOutput: " << path.string() << '\n';
    return true;
}

[[nodiscard]] bool write_routine_call_trace(
    const std::filesystem::path& path,
    const std::span<const RoutineKey> routines,
    const std::span<const RoutineCall> calls,
    const std::string_view core_revision)
{
    if (routines.empty()) {
        return false;
    }

    auto writes_path = path;
    writes_path += ".writes.csv";
    std::error_code error;
    const auto directory = path.parent_path();
    if (!directory.empty()) {
        std::filesystem::create_directories(directory, error);
    }
    if (error) {
        std::cerr << "Could not create routine-call trace directory: "
                  << error.message() << '\n';
        return false;
    }

    auto temporary_path = path;
    temporary_path += ".tmp";
    auto writes_temporary_path = writes_path;
    writes_temporary_path += ".tmp";
    std::ofstream output(
        temporary_path,
        std::ios::binary | std::ios::trunc);
    std::ofstream writes_output(
        writes_temporary_path,
        std::ios::binary | std::ios::trunc);
    if (!output || !writes_output) {
        std::cerr << "Could not create routine-call trace outputs.\n";
        return false;
    }

    const auto write_metadata =
        [&](std::ofstream& stream, const std::string_view format) {
            stream << "# format=" << format << '\n'
                   << "# rom_sha256="
                   << devils_crush::rom::kSupportedSha256 << '\n'
                   << "# core_revision=" << core_revision << '\n'
                   << "# routine_count=" << routines.size() << '\n';
            std::size_t index = 0U;
            for (const auto routine : routines) {
                stream << "# routine_" << ++index << '='
                       << std::uppercase << std::hex << std::setfill('0')
                       << std::setw(2)
                       << static_cast<unsigned>(routine.physical_bank)
                       << ':' << std::setw(4) << routine.logical_pc
                       << '\n';
            }
            stream << std::dec;
        };

    write_metadata(output, "devils-crush-routine-call-v1");
    output << "# writes_suffix=.writes.csv\n"
           << "sequence,frame,entry_master_cycle,"
              "entry_absolute_master_cycle,physical_bank,logical_pc,"
              "caller_pc,return_pc,return_physical_bank,"
              "return_instruction_pc,return_master_cycle,"
              "return_absolute_master_cycle,"
              "body_master_cycles,input_a,input_x,input_y,input_sp,"
              "input_status,input_mpr0,input_mpr1,input_mpr2,input_mpr3,"
              "input_mpr4,input_mpr5,input_mpr6,input_mpr7,"
              "output_a,output_x,output_y,output_sp,output_status,"
              "output_mpr0,output_mpr1,output_mpr2,output_mpr3,"
              "output_mpr4,output_mpr5,output_mpr6,output_mpr7,"
              "write_count\n";

    write_metadata(writes_output, "devils-crush-routine-write-v1");
    writes_output
        << "call_sequence,write_sequence,master_cycle,"
           "absolute_master_cycle,"
           "instruction_physical_bank,instruction_logical_pc,"
           "work_ram_offset,value\n";

    std::uint64_t call_sequence = 0U;
    std::uint64_t write_count = 0U;
    for (const auto& call : calls) {
        const auto sequence = ++call_sequence;
        const auto& input = call.entry.registers;
        const auto& output_registers = call.output_registers;
        output << sequence << ',' << call.entry.frame_number << ','
               << call.entry.master_cycle << ','
               << call.entry.absolute_master_cycle << ','
               << std::uppercase
               << std::hex << std::setfill('0') << std::setw(2)
               << static_cast<unsigned>(
                      call.entry.routine.physical_bank)
               << ',' << std::setw(4) << call.entry.routine.logical_pc
               << ',' << std::setw(4) << call.caller_pc
               << ',' << std::setw(4) << call.return_pc
               << ',' << std::setw(2)
               << static_cast<unsigned>(
                      call.return_instruction.physical_bank)
               << ',' << std::setw(4)
               << call.return_instruction.logical_pc
               << std::dec << ',' << call.return_master_cycle
               << ',' << call.return_absolute_master_cycle
               << ',' << call.body_master_cycles << std::hex
               << ',' << std::setw(2)
               << static_cast<unsigned>(input.accumulator)
               << ',' << std::setw(2) << static_cast<unsigned>(input.x)
               << ',' << std::setw(2) << static_cast<unsigned>(input.y)
               << ',' << std::setw(2)
               << static_cast<unsigned>(input.stack_pointer)
               << ',' << std::setw(2)
               << static_cast<unsigned>(input.status);
        for (const auto mpr : input.mpr) {
            output << ',' << std::setw(2)
                   << static_cast<unsigned>(mpr);
        }
        output << ',' << std::setw(2)
               << static_cast<unsigned>(output_registers.accumulator)
               << ',' << std::setw(2)
               << static_cast<unsigned>(output_registers.x)
               << ',' << std::setw(2)
               << static_cast<unsigned>(output_registers.y)
               << ',' << std::setw(2)
               << static_cast<unsigned>(
                      output_registers.stack_pointer)
               << ',' << std::setw(2)
               << static_cast<unsigned>(output_registers.status);
        for (const auto mpr : output_registers.mpr) {
            output << ',' << std::setw(2)
                   << static_cast<unsigned>(mpr);
        }
        output << std::dec << ',' << call.work_ram_writes.size()
               << '\n';

        std::uint64_t write_sequence = 0U;
        for (const auto& write : call.work_ram_writes) {
            writes_output
                << sequence << ',' << ++write_sequence << ','
                << write.master_cycle << ','
                << write.absolute_master_cycle << ','
                << std::uppercase
                << std::hex << std::setfill('0') << std::setw(2)
                << static_cast<unsigned>(
                       write.instruction.physical_bank)
                << ',' << std::setw(4)
                << write.instruction.logical_pc
                << ',' << std::setw(4) << write.work_ram_offset
                << ',' << std::setw(2)
                << static_cast<unsigned>(write.value)
                << std::dec << '\n';
            ++write_count;
        }
    }

    output.close();
    writes_output.close();
    if (!output || !writes_output) {
        std::cerr << "Could not finalize routine-call trace outputs.\n";
        return false;
    }
    if (!commit_artifact(
            writes_temporary_path,
            writes_path,
            "routine-write trace")) {
        return false;
    }
    if (!commit_artifact(temporary_path, path, "routine-call trace")) {
        return false;
    }

    std::cout << "Routine call trace complete: " << calls.size()
              << " calls, " << write_count << " writes\nCalls: "
              << path.string() << "\nWrites: " << writes_path.string()
              << '\n';
    return true;
}

[[nodiscard]] bool write_work_ram_write_trace(
    const std::filesystem::path& path,
    const std::span<const LogicalAddressRange> ranges,
    const std::span<const ObservedMemoryWrite> writes,
    const std::string_view core_revision)
{
    if (ranges.empty()) {
        return false;
    }

    std::error_code error;
    const auto directory = path.parent_path();
    if (!directory.empty()) {
        std::filesystem::create_directories(directory, error);
    }
    if (error) {
        std::cerr << "Could not create work-RAM write trace directory: "
                  << error.message() << '\n';
        return false;
    }

    auto temporary_path = path;
    temporary_path += ".tmp";
    std::ofstream output(
        temporary_path,
        std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "Could not create work-RAM write trace: "
                  << temporary_path.string() << '\n';
        return false;
    }

    output << "# format=devils-crush-work-ram-write-v1\n"
           << "# rom_sha256=" << devils_crush::rom::kSupportedSha256
           << '\n'
           << "# core_revision=" << core_revision << '\n'
           << "# range_count=" << ranges.size() << '\n';
    std::size_t range_index = 0U;
    for (const auto range : ranges) {
        output << "# range_" << ++range_index << '='
               << std::uppercase << std::hex << std::setfill('0')
               << std::setw(4) << range.first << '-' << std::setw(4)
               << range.last << '\n';
    }
    output
        << std::dec
        << "sequence,frame,master_cycle,absolute_master_cycle,"
           "instruction_physical_bank,instruction_logical_pc,"
           "logical_address,work_ram_offset,value\n";

    std::uint64_t sequence = 0U;
    for (const auto& write : writes) {
        output << ++sequence << ',' << write.frame_number << ','
               << write.master_cycle << ','
               << write.absolute_master_cycle << ','
               << std::uppercase << std::hex << std::setfill('0')
               << std::setw(2)
               << static_cast<unsigned>(
                      write.instruction.physical_bank)
               << ',' << std::setw(4)
               << write.instruction.logical_pc << ',' << std::setw(4)
               << write.logical_address << ',' << std::setw(4)
               << write.work_ram_offset << ',' << std::setw(2)
               << static_cast<unsigned>(write.value)
               << std::dec << '\n';
    }

    output.close();
    if (!output) {
        std::cerr << "Could not finalize work-RAM write trace.\n";
        return false;
    }
    if (!commit_artifact(temporary_path, path, "work-RAM write trace")) {
        return false;
    }

    std::cout << "Work-RAM write trace complete: " << writes.size()
              << " selected writes\nOutput: " << path.string() << '\n';
    return true;
}

[[nodiscard]] bool write_bus_write_trace(
    const std::filesystem::path& path,
    const std::span<const LogicalAddressRange> ranges,
    const std::span<const ObservedBusWrite> writes,
    const std::string_view core_revision)
{
    if (ranges.empty()) {
        return false;
    }

    std::error_code error;
    const auto directory = path.parent_path();
    if (!directory.empty()) {
        std::filesystem::create_directories(directory, error);
    }
    if (error) {
        std::cerr << "Could not create bus write trace directory: "
                  << error.message() << '\n';
        return false;
    }

    auto temporary_path = path;
    temporary_path += ".tmp";
    std::ofstream output(
        temporary_path,
        std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "Could not create bus write trace: "
                  << temporary_path.string() << '\n';
        return false;
    }

    output << "# format=devils-crush-bus-write-v1\n"
           << "# rom_sha256=" << devils_crush::rom::kSupportedSha256
           << '\n'
           << "# core_revision=" << core_revision << '\n'
           << "# range_count=" << ranges.size() << '\n';
    std::size_t range_index = 0U;
    for (const auto range : ranges) {
        output << "# range_" << ++range_index << '='
               << std::uppercase << std::hex << std::setfill('0')
               << std::setw(4) << range.first << '-' << std::setw(4)
               << range.last << '\n';
    }
    output
        << std::dec
        << "sequence,frame,master_cycle,absolute_master_cycle,"
           "instruction_physical_bank,instruction_logical_pc,"
           "physical_bank,logical_address,value\n";

    std::uint64_t sequence = 0U;
    for (const auto& write : writes) {
        output << ++sequence << ',' << write.frame_number << ','
               << write.master_cycle << ','
               << write.absolute_master_cycle << ','
               << std::uppercase << std::hex << std::setfill('0')
               << std::setw(2)
               << static_cast<unsigned>(
                      write.instruction.physical_bank)
               << ',' << std::setw(4)
               << write.instruction.logical_pc << ',' << std::setw(2)
               << static_cast<unsigned>(write.physical_bank) << ','
               << std::setw(4) << write.logical_address << ','
               << std::setw(2)
               << static_cast<unsigned>(write.value)
               << std::dec << '\n';
    }

    output.close();
    if (!output) {
        std::cerr << "Could not finalize bus write trace.\n";
        return false;
    }
    if (!commit_artifact(temporary_path, path, "bus write trace")) {
        return false;
    }

    std::cout << "Bus write trace complete: " << writes.size()
              << " selected writes\nOutput: " << path.string() << '\n';
    return true;
}

} // namespace devils_crush::app::host
