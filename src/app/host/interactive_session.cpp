// SPDX-License-Identifier: GPL-3.0-or-later
#include "interactive_session.hpp"

#include "host_files.hpp"

namespace devils_crush::app::host {

[[nodiscard]] std::optional<InteractiveSessionReplay>
read_interactive_session_replay(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::cerr << "Could not open interactive session input: "
                  << path.string() << '\n';
        return std::nullopt;
    }

    std::string format;
    std::string rom_line;
    std::string revision_line;
    std::string count_line;
    std::string header;
    if (!std::getline(input, format) || !std::getline(input, rom_line)
        || !std::getline(input, revision_line)
        || !std::getline(input, count_line) || !std::getline(input, header)) {
        std::cerr << "Interactive input header is incomplete: "
                  << path.string() << '\n';
        return std::nullopt;
    }
    constexpr std::string_view rom_prefix = "# rom_sha256=";
    constexpr std::string_view count_prefix = "# frame_count=";
    const bool has_nudge_multiplier = format == "# format=devils-crush-interactive-input-v4";
    const bool has_work_ram_hashes =
        format == "# format=devils-crush-interactive-input-v2"
        || format == "# format=devils-crush-interactive-input-v3" || has_nudge_multiplier;
    const bool has_host_mode =
        format == "# format=devils-crush-interactive-input-v3" || has_nudge_multiplier;
    if ((format != "# format=devils-crush-interactive-input-v1"
         && !has_work_ram_hashes)
        || !rom_line.starts_with(rom_prefix)
        || rom_line.substr(rom_prefix.size())
            != devils_crush::rom::kSupportedSha256
        || !revision_line.starts_with("# core_revision=")
        || !count_line.starts_with(count_prefix)) {
        std::cerr << "Interactive input metadata is not compatible: "
                  << path.string() << '\n';
        return std::nullopt;
    }
    bool cabinet_host = false;
    if (has_host_mode) {
        if (header == "# host_mode=cabinet") {
            cabinet_host = true;
        } else if (header != "# host_mode=desktop") {
            std::cerr << "Interactive input host mode is invalid: "
                      << path.string() << '\n';
            return std::nullopt;
        }
        if (!std::getline(input, header)) {
            std::cerr << "Interactive input columns are missing: "
                      << path.string() << '\n';
            return std::nullopt;
        }
    }

    double nudge_multiplier = 1.0;
    if (has_nudge_multiplier) {
        constexpr std::string_view prefix = "# nudge_multiplier=";
        const auto value = header.starts_with(prefix)
            ? devils_crush::input::parse_nudge_multiplier(std::string_view(header).substr(prefix.size()))
            : std::nullopt;
        if (!value || !std::getline(input, header)) {
            std::cerr << "Interactive input nudge multiplier is invalid.\n";
            return std::nullopt;
        }
        nudge_multiplier = *value;
    }
    std::uint64_t declared_count = 0U;
    const auto [count_end, count_error] = std::from_chars(
        count_line.data() + count_prefix.size(),
        count_line.data() + count_line.size(),
        declared_count);
    if (count_error != std::errc{} || count_end != count_line.data()
            + count_line.size()
        || declared_count == 0U
        || declared_count
            > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        std::cerr << "Interactive input frame count is invalid: "
                  << path.string() << '\n';
        return std::nullopt;
    }
    constexpr std::string_view kPadHeader = "frame,player_one,player_two";
    const bool includes_plunger = header
        == "frame,player_one,player_two,plunger_draw,plunger_drawn,"
           "plunger_released,plunger_engaged";
    const bool includes_precise_plunger = header
        == "frame,player_one,player_two,plunger_draw,plunger_drawn,"
           "plunger_released,plunger_engaged,work_ram_sha256,plunger_draw_units";
    const bool includes_work_ram_hash = includes_precise_plunger || header
        == "frame,player_one,player_two,plunger_draw,plunger_drawn,"
           "plunger_released,plunger_engaged,work_ram_sha256";
    if (header != kPadHeader && !includes_plunger
        && !includes_work_ram_hash) {
        std::cerr << "Interactive input columns are not supported: "
                  << path.string() << '\n';
        return std::nullopt;
    }
    if (has_work_ram_hashes != includes_work_ram_hash) {
        std::cerr << "Interactive input format and columns disagree: "
                  << path.string() << '\n';
        return std::nullopt;
    }

    const auto parse_unsigned = [](const std::string_view text,
                                   const std::uint32_t maximum)
        -> std::optional<std::uint32_t> {
        std::uint32_t value = 0U;
        const auto [end, error] = std::from_chars(
            text.data(), text.data() + text.size(), value);
        if (text.empty() || error != std::errc{}
            || end != text.data() + text.size() || value > maximum) {
            return std::nullopt;
        }
        return value;
    };
    const auto fields_for = [](const std::string& line) {
        std::vector<std::string_view> fields;
        std::size_t begin = 0U;
        while (begin <= line.size()) {
            const auto end = line.find(',', begin);
            fields.emplace_back(
                line.data() + begin,
                (end == std::string::npos ? line.size() : end) - begin);
            if (end == std::string::npos) {
                break;
            }
            begin = end + 1U;
        }
        return fields;
    };

    InteractiveSessionReplay replay;
    replay.cabinet_host = cabinet_host;
    replay.nudge_multiplier = nudge_multiplier;
    replay.initial_state_path = path.parent_path() / "initial.state";
    std::error_code state_error;
    if (!std::filesystem::is_regular_file(replay.initial_state_path,
                                          state_error)
        || state_error) {
        std::cerr << "Interactive replay requires the capture's initial.state; "
                     "recapture this session with the current executable: "
                  << replay.initial_state_path.string() << '\n';
        return std::nullopt;
    }
    const auto high_score_path = path.parent_path() / "initial.hiscore";
    std::error_code high_score_error;
    if (std::filesystem::is_regular_file(high_score_path, high_score_error)
        && !high_score_error) {
        replay.initial_high_score_path = high_score_path;
    }
    replay.inputs.reserve(static_cast<std::size_t>(declared_count));
    if (includes_work_ram_hash) {
        replay.work_ram_sha256s.reserve(
            static_cast<std::size_t>(declared_count));
    }
    std::string line;
    while (std::getline(input, line)) {
        const auto fields = fields_for(line);
        if (fields.size()
            != (includes_precise_plunger ? 9U
                : includes_work_ram_hash ? 8U : includes_plunger ? 7U : 3U)) {
            std::cerr << "Interactive input row has the wrong column count: "
                      << path.string() << '\n';
            return std::nullopt;
        }
        const auto frame = parse_unsigned(fields[0],
                                          std::numeric_limits<int>::max());
        const auto player_one = parse_unsigned(
            fields[1], std::numeric_limits<std::uint16_t>::max());
        const auto player_two = parse_unsigned(
            fields[2], std::numeric_limits<std::uint16_t>::max());
        if (!frame || !player_one || !player_two
            || *frame != replay.inputs.size() + 1U
            || (includes_plunger && fields[6] != "0")) {
            std::cerr << (includes_plunger && fields[6] != "0"
                              ? "Interactive replay cannot yet reproduce a "
                                "physical-plunger draw: "
                              : "Interactive input row is invalid: ")
                      << path.string() << '\n';
            return std::nullopt;
        }
        replay.inputs.push_back({
            .player_one = static_cast<std::uint16_t>(*player_one),
            .player_two = static_cast<std::uint16_t>(*player_two),
        });
        if (includes_work_ram_hash) {
            const auto hash = fields[7];
            const bool valid_hash = hash.size() == 64U
                && std::ranges::all_of(hash, [](const unsigned char value) {
                    return std::isxdigit(value) != 0;
                });
            if (!valid_hash) {
                std::cerr << "Interactive input work-RAM hash is invalid: "
                          << path.string() << '\n';
                return std::nullopt;
            }
            replay.work_ram_sha256s.emplace_back(hash);
        }
    }
    if (!input.eof() || replay.inputs.size() != declared_count
        || (includes_work_ram_hash
            && replay.work_ram_sha256s.size() != replay.inputs.size())) {
        std::cerr << "Interactive input frame count does not match metadata: "
                  << path.string() << '\n';
        return std::nullopt;
    }
    return replay;
}

// Interactive sessions need a replayable input stream, not a description of
// what a person remembers pressing. The pad mask is exactly the value sent to
// the runtime after launcher suppression and opposing-direction resolution.
// The plunger fields retain the host input that is not a pad bit, and each
// post-frame work-RAM hash makes a later replay prove its state trajectory.
[[nodiscard]] bool write_interactive_input_capture(
    const std::filesystem::path& directory,
    const std::span<const InteractiveCaptureFrame> inputs,
    const std::string_view core_revision,
    const bool cabinet_mode,
    const double nudge_multiplier)
{
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
        std::cerr << "Could not create interactive capture directory: "
                  << error.message() << '\n';
        return false;
    }

    const auto path = directory / "inputs.csv";
    auto temporary_path = path;
    temporary_path += ".tmp";
    std::ofstream output(
        temporary_path,
        std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "Could not create interactive input capture: "
                  << temporary_path.string() << '\n';
        return false;
    }

    output << "# format=devils-crush-interactive-input-v"
           << (nudge_multiplier == 1.0 ? 3 : 4) << '\n'
           << "# rom_sha256=" << devils_crush::rom::kSupportedSha256 << '\n'
           << "# core_revision=" << core_revision << '\n'
           << "# frame_count=" << inputs.size() << '\n'
           << "# host_mode=" << (cabinet_mode ? "cabinet" : "desktop")
           << '\n';
    if (nudge_multiplier != 1.0) {
        output << "# nudge_multiplier=" << std::setprecision(17) << nudge_multiplier << '\n';
    }
    output << "frame,player_one,player_two,plunger_draw,plunger_drawn,"
              "plunger_released,plunger_engaged,work_ram_sha256,plunger_draw_units\n";
    for (std::size_t index = 0U; index < inputs.size(); ++index) {
        const auto& captured = inputs[index];
        if (captured.work_ram_sha256.empty()) {
            std::cerr << "Interactive capture is missing a work-RAM hash.\n";
            return false;
        }
        output << index + 1U << ',' << captured.input.player_one << ','
               << captured.input.player_two << ','
               << static_cast<unsigned>(captured.plunger.draw) << ','
               << (captured.plunger.drawn ? 1 : 0) << ','
               << (captured.plunger.released ? 1 : 0) << ','
               << (captured.plunger_engaged ? 1 : 0) << ','
               << captured.work_ram_sha256 << ','
               << captured.plunger.draw_units << '\n';
    }
    output.close();
    if (!output) {
        std::cerr << "Could not finalize interactive input capture.\n";
        return false;
    }
    if (!commit_artifact(temporary_path, path, "interactive input capture")) {
        return false;
    }

    std::cout << "Interactive input capture complete: " << inputs.size()
              << " frames\nOutput: " << path.string() << '\n';
    return true;
}

} // namespace devils_crush::app::host
