// SPDX-License-Identifier: GPL-3.0-or-later
#include "command_line.hpp"

namespace devils_crush::app::host {

namespace {

[[nodiscard]] std::optional<RoutineKey> parse_routine_key(
    const std::string_view text)
{
    const auto separator = text.find(':');
    if (separator == std::string_view::npos
        || text.find(':', separator + 1U) != std::string_view::npos) {
        return std::nullopt;
    }

    const auto parse_hex = [](std::string_view component)
        -> std::optional<std::uint32_t> {
        if (component.starts_with("0x")
            || component.starts_with("0X")) {
            component.remove_prefix(2U);
        }
        if (component.empty()) {
            return std::nullopt;
        }

        std::uint32_t value = 0;
        const auto result = std::from_chars(
            component.data(),
            component.data() + component.size(),
            value,
            16);
        if (result.ec != std::errc{}
            || result.ptr != component.data() + component.size()) {
            return std::nullopt;
        }
        return value;
    };

    const auto bank = parse_hex(text.substr(0, separator));
    const auto pc = parse_hex(text.substr(separator + 1U));
    if (!bank || !pc
        || *bank > std::numeric_limits<std::uint8_t>::max()
        || *pc > std::numeric_limits<std::uint16_t>::max()) {
        return std::nullopt;
    }

    return RoutineKey{
        .physical_bank = static_cast<std::uint8_t>(*bank),
        .logical_pc = static_cast<std::uint16_t>(*pc),
    };
}

[[nodiscard]] std::optional<RoutineReplayRequest>
parse_routine_replay_request(const std::string_view text)
{
    const auto separator = text.find('@');
    if (separator == std::string_view::npos
        || text.find('@', separator + 1U) != std::string_view::npos) {
        return std::nullopt;
    }

    const auto routine = parse_routine_key(text.substr(0, separator));
    const auto ordinal_text = text.substr(separator + 1U);
    std::uint64_t ordinal = 0;
    const auto result = std::from_chars(
        ordinal_text.data(),
        ordinal_text.data() + ordinal_text.size(),
        ordinal,
        10);
    if (!routine || ordinal_text.empty() || result.ec != std::errc{}
        || result.ptr != ordinal_text.data() + ordinal_text.size()
        || ordinal == 0U) {
        return std::nullopt;
    }

    return RoutineReplayRequest{
        .routine = *routine,
        .ordinal = ordinal,
    };
}

[[nodiscard]] std::optional<RoutineReplayRequest>
parse_replacement_request(const std::string_view text)
{
    constexpr std::string_view kAllSuffix = "@all";
    if (!text.ends_with(kAllSuffix)) {
        return parse_routine_replay_request(text);
    }
    const auto routine =
        parse_routine_key(text.substr(0U, text.size() - kAllSuffix.size()));
    if (!routine) {
        return std::nullopt;
    }
    return RoutineReplayRequest{
        .routine = *routine,
        .all_calls = true,
    };
}

[[nodiscard]] std::optional<WorkRamPatch>
parse_work_ram_patch(const std::string_view text)
{
    const auto first_separator = text.find(':');
    const auto second_separator =
        first_separator == std::string_view::npos
        ? std::string_view::npos
        : text.find(':', first_separator + 1U);
    if (first_separator == std::string_view::npos
        || second_separator == std::string_view::npos
        || text.find(':', second_separator + 1U) != std::string_view::npos) {
        return std::nullopt;
    }

    const auto parse_number = [](
                                  std::string_view component,
                                  const int base)
        -> std::optional<std::uint32_t> {
        if (base == 16
            && (component.starts_with("0x")
                || component.starts_with("0X"))) {
            component.remove_prefix(2U);
        }
        if (component.empty()) {
            return std::nullopt;
        }
        std::uint32_t value = 0U;
        const auto result = std::from_chars(
            component.data(),
            component.data() + component.size(),
            value,
            base);
        if (result.ec != std::errc{}
            || result.ptr != component.data() + component.size()) {
            return std::nullopt;
        }
        return value;
    };

    const auto frame = parse_number(text.substr(0, first_separator), 10);
    const auto address = parse_number(
        text.substr(
            first_separator + 1U,
            second_separator - first_separator - 1U),
        16);
    const auto value =
        parse_number(text.substr(second_separator + 1U), 16);
    if (!frame || !address || !value || *frame == 0U
        || *frame > static_cast<std::uint32_t>(
                        std::numeric_limits<int>::max())
        || *address < 0x2000U || *address > 0x3FFFU
        || *value > std::numeric_limits<std::uint8_t>::max()) {
        return std::nullopt;
    }

    return WorkRamPatch{
        .frame = static_cast<int>(*frame),
        .logical_address = static_cast<std::uint16_t>(*address),
        .value = static_cast<std::uint8_t>(*value),
    };
}

[[nodiscard]] std::optional<LogicalAddressRange>
parse_logical_address_range(const std::string_view text)
{
    const auto separator = text.find('-');
    if (separator == std::string_view::npos
        || text.find('-', separator + 1U) != std::string_view::npos) {
        return std::nullopt;
    }

    const auto parse_hex = [](std::string_view component)
        -> std::optional<std::uint32_t> {
        if (component.starts_with("0x")
            || component.starts_with("0X")) {
            component.remove_prefix(2U);
        }
        if (component.empty()) {
            return std::nullopt;
        }

        std::uint32_t value = 0;
        const auto result = std::from_chars(
            component.data(),
            component.data() + component.size(),
            value,
            16);
        if (result.ec != std::errc{}
            || result.ptr != component.data() + component.size()) {
            return std::nullopt;
        }
        return value;
    };

    const auto first = parse_hex(text.substr(0, separator));
    const auto last = parse_hex(text.substr(separator + 1U));
    if (!first || !last || *first > *last
        || *last > std::numeric_limits<std::uint16_t>::max()) {
        return std::nullopt;
    }

    return LogicalAddressRange{
        .first = static_cast<std::uint16_t>(*first),
        .last = static_cast<std::uint16_t>(*last),
    };
}

} // namespace

[[nodiscard]] std::string format_routine_key(const RoutineKey routine)
{
    std::ostringstream text;
    text << std::uppercase << std::hex << std::setfill('0') << std::setw(2)
         << static_cast<unsigned>(routine.physical_bank) << ':'
         << std::setw(4) << routine.logical_pc;
    return text.str();
}

// Reads the arguments into `options`. False at one that is malformed, with
// `options.error` saying which.
[[nodiscard]] bool read_command_line_arguments(
    const int argc,
    char* argv[],
    CommandLine& options,
    CommandLineOverrides& overrides)
{
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--help" || argument == "-h") {
            options.show_help = true;
        } else if (argument == "--rom") {
            if (index + 1 >= argc) {
                options.error = "--rom requires a path.";
                return false;
            }
            options.rom_path = argv[++index];
            overrides.rom = true;
        } else if (argument == "--validate-rom") {
            options.validate_only = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.rom_path = argv[++index];
            }
        } else if (argument == "--smoke-test") {
            options.smoke_test = true;
        } else if (argument == "--cabinet-smoke-test") {
            options.cabinet_smoke_test = true;
        } else if (argument == "--cabinet-presentation-check") {
            options.cabinet_smoke_test = true;
            options.cabinet_presentation_check = true;
        } else if (argument == "--cabinet") {
            options.cabinet = true;
            overrides.cabinet = true;
        } else if (argument == "--no-cabinet") {
            options.cabinet = false;
            overrides.cabinet = false;
        } else if (argument == "--bonus-stage") {
            if (index + 1 >= argc) {
                options.error = "--bonus-stage requires a stage from 1 to 6.";
                return false;
            }
            const std::string_view value = argv[++index];
            unsigned int stage = 0U;
            const auto result = std::from_chars(
                value.data(), value.data() + value.size(), stage);
            if (result.ec != std::errc{}
                || result.ptr != value.data() + value.size()
                || stage < 1U || stage > 6U) {
                options.error = "--bonus-stage requires a stage from 1 to 6.";
                return false;
            }
            options.bonus_stage = static_cast<std::uint8_t>(stage);
            overrides.bonus_stage = true;
        } else if (argument == "--drag-ball") {
            options.drag_ball = true;
        } else if (argument == "--capture-session") {
            if (index + 1 >= argc) {
                options.error = "--capture-session requires a directory.";
                return false;
            }
            options.capture_session_directory = argv[++index];
        } else if (argument == "--replay-session") {
            if (index + 1 >= argc) {
                options.error = "--replay-session requires an inputs.csv path.";
                return false;
            }
            options.replay_session_input_path = argv[++index];
        } else if (argument == "--record-canonical") {
            options.record_canonical = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.canonical_path = argv[++index];
            }
        } else if (argument == "--record-gameplay") {
            options.record_gameplay = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-alternate") {
            options.record_gameplay_alternate = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_alternate_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-sweep") {
            options.record_gameplay_sweep = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_sweep_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-crossfire") {
            options.record_gameplay_crossfire = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_crossfire_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-orbit") {
            options.record_gameplay_orbit = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_orbit_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-lanes") {
            options.record_gameplay_lanes = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_lanes_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-stagger") {
            options.record_gameplay_stagger = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_stagger_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-pulse") {
            options.record_gameplay_pulse = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_pulse_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-rally") {
            options.record_gameplay_rally = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_rally_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-slalom") {
            options.record_gameplay_slalom = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_slalom_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-pendulum") {
            options.record_gameplay_pendulum = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_pendulum_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-nudge") {
            options.record_gameplay_nudge = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_nudge_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-switchback") {
            options.record_gameplay_switchback = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_switchback_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-spiral") {
            options.record_gameplay_spiral = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_spiral_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-ricochet") {
            options.record_gameplay_ricochet = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_ricochet_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-volley") {
            options.record_gameplay_volley = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_volley_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-whirlpool") {
            options.record_gameplay_whirlpool = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_whirlpool_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-cascade") {
            options.record_gameplay_cascade = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_cascade_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-higharc") {
            options.record_gameplay_higharc = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_higharc_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-twist") {
            options.record_gameplay_twist = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_twist_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-sidestep") {
            options.record_gameplay_sidestep = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_sidestep_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-weave") {
            options.record_gameplay_weave = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_weave_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-bankshot") {
            options.record_gameplay_bankshot = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_bankshot_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-playfield") {
            options.record_gameplay_playfield = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_playfield_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-corner") {
            options.record_gameplay_corner = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_corner_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-alley") {
            options.record_gameplay_alley = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_alley_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-rebound") {
            options.record_gameplay_rebound = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_rebound_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-skew") {
            options.record_gameplay_skew = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_skew_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-helix") {
            options.record_gameplay_helix = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_helix_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-vector") {
            options.record_gameplay_vector = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_vector_path = argv[++index];
            }
        } else if (argument == "--record-gameplay-radial") {
            options.record_gameplay_radial = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_radial_path = argv[++index];
            }
        } else if (argument == "--record-state-census") {
            options.record_state_census = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.state_census_path = argv[++index];
            }
        } else if (argument == "--canonical-work-ram") {
            if (index + 1 >= argc) {
                options.error = "--canonical-work-ram requires a path.";
                return false;
            }
            options.canonical_work_ram_path = argv[++index];
        } else if (argument == "--canonical-framebuffer") {
            if (index + 1 >= argc) {
                options.error = "--canonical-framebuffer requires a path.";
                return false;
            }
            options.canonical_framebuffer_path = argv[++index];
        } else if (argument == "--canonical-audio") {
            if (index + 1 >= argc) {
                options.error = "--canonical-audio requires a path.";
                return false;
            }
            options.canonical_audio_path = argv[++index];
        } else if (argument == "--pass-through-hook") {
            if (index + 1 >= argc) {
                options.error =
                    "--pass-through-hook requires BANK:PC.";
                return false;
            }
            const auto routine = parse_routine_key(argv[++index]);
            if (!routine) {
                options.error =
                    "--pass-through-hook requires hexadecimal BANK:PC.";
                return false;
            }
            if (std::find(
                    options.pass_through_hooks.begin(),
                    options.pass_through_hooks.end(),
                    *routine)
                    != options.pass_through_hooks.end()
                || std::find(
                       options.observe_hooks.begin(),
                       options.observe_hooks.end(),
                       *routine)
                    != options.observe_hooks.end()
                || std::find_if(
                       options.replace_hooks.begin(),
                       options.replace_hooks.end(),
                       [routine](
                           const RoutineReplayRequest& replacement) {
                           return replacement.routine == *routine;
                       })
                    != options.replace_hooks.end()) {
                options.error =
                    "Each execution-hook BANK:PC may be registered once.";
                return false;
            }
            options.pass_through_hooks.push_back(*routine);
        } else if (argument == "--observe-hook") {
            if (index + 1 >= argc) {
                options.error = "--observe-hook requires BANK:PC.";
                return false;
            }
            const auto routine = parse_routine_key(argv[++index]);
            if (!routine) {
                options.error =
                    "--observe-hook requires hexadecimal BANK:PC.";
                return false;
            }
            if (std::find(
                    options.pass_through_hooks.begin(),
                    options.pass_through_hooks.end(),
                    *routine)
                    != options.pass_through_hooks.end()
                || std::find(
                       options.observe_hooks.begin(),
                       options.observe_hooks.end(),
                       *routine)
                    != options.observe_hooks.end()) {
                options.error =
                    "Each execution-hook BANK:PC may be registered once.";
                return false;
            }
            options.observe_hooks.push_back(*routine);
        } else if (argument == "--replace-hook") {
            if (index + 1 >= argc) {
                options.error =
                    "--replace-hook requires BANK:PC@ORDINAL or "
                    "BANK:PC@all.";
                return false;
            }
            const auto request =
                parse_replacement_request(argv[++index]);
            if (!request) {
                options.error =
                    "--replace-hook requires hexadecimal BANK:PC and "
                    "a positive decimal ORDINAL or the suffix @all.";
                return false;
            }
            if (std::find(
                    options.pass_through_hooks.begin(),
                    options.pass_through_hooks.end(),
                    request->routine)
                    != options.pass_through_hooks.end()
                || std::find_if(
                       options.replace_hooks.begin(),
                       options.replace_hooks.end(),
                       [request](
                           const RoutineReplayRequest& existing) {
                           return existing.routine == request->routine;
                       })
                    != options.replace_hooks.end()) {
                options.error =
                    "Each replacement BANK:PC may be registered once "
                    "and cannot also be pass-through.";
                return false;
            }
            options.replace_hooks.push_back(*request);
        } else if (argument == "--routine-trace") {
            if (index + 1 >= argc) {
                options.error = "--routine-trace requires a path.";
                return false;
            }
            options.routine_trace_path = argv[++index];
        } else if (argument == "--routine-call-trace") {
            if (index + 1 >= argc) {
                options.error = "--routine-call-trace requires a path.";
                return false;
            }
            options.routine_call_trace_path = argv[++index];
        } else if (argument == "--playfield-collision-sources") {
            if (index + 1 >= argc) {
                options.error =
                    "--playfield-collision-sources requires a path.";
                return false;
            }
            options.playfield_collision_sources_path = argv[++index];
        } else if (argument == "--routine-replay") {
            if (index + 1 >= argc) {
                options.error =
                    "--routine-replay requires BANK:PC@ORDINAL.";
                return false;
            }
            const auto request =
                parse_routine_replay_request(argv[++index]);
            if (!request) {
                options.error =
                    "--routine-replay requires hexadecimal BANK:PC and "
                    "a positive decimal ORDINAL.";
                return false;
            }
            const auto duplicate = std::find_if(
                options.routine_replays.begin(),
                options.routine_replays.end(),
                [request](const RoutineReplayRequest& existing) {
                    return existing.routine == request->routine
                        && existing.ordinal == request->ordinal;
                });
            if (duplicate != options.routine_replays.end()) {
                options.error =
                    "Each routine replay BANK:PC@ORDINAL must be unique.";
                return false;
            }
            options.routine_replays.push_back(*request);
        } else if (argument == "--routine-replay-corpus-out") {
            if (index + 1 >= argc) {
                options.error =
                    "--routine-replay-corpus-out requires a directory.";
                return false;
            }
            options.routine_replay_corpus_output_directory = argv[++index];
        } else if (argument == "--routine-replay-corpus") {
            if (index + 1 >= argc) {
                options.error =
                    "--routine-replay-corpus requires a fixture path.";
                return false;
            }
            options.routine_replay_corpus_paths.emplace_back(argv[++index]);
        } else if (argument == "--work-ram-patch") {
            if (index + 1 >= argc) {
                options.error =
                    "--work-ram-patch requires FRAME:ADDRESS:VALUE.";
                return false;
            }
            const auto patch = parse_work_ram_patch(argv[++index]);
            if (!patch) {
                options.error =
                    "--work-ram-patch requires a positive decimal FRAME, "
                    "a hexadecimal address from 2000 through 3FFF, and "
                    "a hexadecimal byte VALUE.";
                return false;
            }
            if (std::find_if(
                    options.work_ram_patches.begin(),
                    options.work_ram_patches.end(),
                    [patch](const WorkRamPatch& existing) {
                        return existing.frame == patch->frame
                            && existing.logical_address
                                == patch->logical_address;
                    })
                != options.work_ram_patches.end()) {
                options.error =
                    "Each work-RAM FRAME:ADDRESS location may be patched "
                    "once.";
                return false;
            }
            options.work_ram_patches.push_back(*patch);
        } else if (argument == "--work-ram-write-range") {
            if (index + 1 >= argc) {
                options.error =
                    "--work-ram-write-range requires START-END.";
                return false;
            }
            const auto range =
                parse_logical_address_range(argv[++index]);
            if (!range) {
                options.error =
                    "--work-ram-write-range requires a hexadecimal "
                    "logical START-END range.";
                return false;
            }
            options.work_ram_write_ranges.push_back(*range);
        } else if (argument == "--work-ram-write-trace") {
            if (index + 1 >= argc) {
                options.error =
                    "--work-ram-write-trace requires a path.";
                return false;
            }
            options.work_ram_write_trace_path = argv[++index];
        } else if (argument == "--bus-write-range") {
            if (index + 1 >= argc) {
                options.error = "--bus-write-range requires START-END.";
                return false;
            }
            const auto range =
                parse_logical_address_range(argv[++index]);
            if (!range) {
                options.error =
                    "--bus-write-range requires a hexadecimal "
                    "logical START-END range.";
                return false;
            }
            options.bus_write_ranges.push_back(*range);
        } else if (argument == "--bus-write-trace") {
            if (index + 1 >= argc) {
                options.error = "--bus-write-trace requires a path.";
                return false;
            }
            options.bus_write_trace_path = argv[++index];
        } else if (argument == "--emulated-psg") {
            options.emulated_psg = true;
        } else if (argument == "--music-voice-overlay"
                   || argument == "--native-music-overlay") {
            options.music_voice_overlay = true;
            overrides.music_overlay = true;
        } else if (argument == "--no-music-voice-overlay") {
            options.music_voice_overlay = false;
            overrides.music_overlay = false;
        } else if (argument == "--report-psg-coverage") {
            options.report_psg_coverage = true;
        } else if (argument == "--emulated-vdc") {
            options.emulated_vdc = true;
        } else if (argument == "--report-vdc-coverage") {
            options.report_vdc_coverage = true;
        } else if (argument == "--emulated-vce") {
            options.emulated_vce = true;
        } else if (argument == "--report-vce-coverage") {
            options.report_vce_coverage = true;
        } else if (argument == "--emulated-irq") {
            options.emulated_irq = true;
        } else if (argument == "--report-irq-coverage") {
            options.report_irq_coverage = true;
        } else if (argument == "--emulated-scheduler") {
            options.emulated_scheduler = true;
        } else if (argument == "--report-scheduler-coverage") {
            options.report_scheduler_coverage = true;
        } else if (argument == "--emulated-boundary") {
            options.emulated_boundary = true;
        } else if (argument == "--execution-details") {
            options.execution_details = true;
        } else if (argument == "--report-boundary-coverage") {
            options.report_boundary_coverage = true;
        } else if (argument == "--emulated-idle") {
            options.emulated_idle = true;
        } else if (argument == "--report-idle-coverage") {
            options.report_idle_coverage = true;
        } else if (argument == "--emulated-pad") {
            options.emulated_pad = true;
        } else if (argument == "--report-pad-coverage") {
            options.report_pad_coverage = true;
        } else if (argument == "--replace-registry") {
            options.replace_registry = true;
        } else if (argument == "--cabinet-overlay-stats") {
            options.cabinet_overlay_stats = true;
        } else if (argument == "--cabinet-layers") {
            if (index + 1 >= argc) {
                options.error =
                    "--cabinet-layers requires all, background, retained, "
                    "or sat.";
                return false;
            }
            const std::string_view value = argv[++index];
            if (value == "all") {
                options.cabinet_layers = CabinetLayers::all;
            } else if (value == "background") {
                options.cabinet_layers = CabinetLayers::background_only;
            } else if (value == "retained") {
                options.cabinet_layers = CabinetLayers::retained_only;
            } else if (value == "sat") {
                options.cabinet_layers = CabinetLayers::sat_only;
            } else {
                options.error =
                    "--cabinet-layers requires all, background, retained, "
                    "or sat.";
                return false;
            }
        } else if (argument == "--table-vram-census") {
            if (index + 1 >= argc) {
                options.error = "--table-vram-census requires a path.";
                return false;
            }
            options.table_vram_census_path = argv[++index];
        } else if (argument == "--cabinet-dump-route") {
            if (index + 1 >= argc) {
                options.error =
                    "--cabinet-dump-route requires a directory.";
                return false;
            }
            options.cabinet_dump_route_directory = argv[++index];
        } else if (argument == "--cabinet-oracle") {
            options.cabinet_oracle = true;
        } else if (argument == "--cabinet-freeze-census") {
            options.cabinet_freeze_census = true;
        } else if (argument == "--cabinet-freeze-census-state") {
            options.cabinet_freeze_census = true;
            options.cabinet_freeze_census_state = kQuickStatePath;
        } else if (argument == "--cabinet-flipper-trace") {
            options.cabinet_flipper_trace = true;
        } else if (argument == "--cabinet-drain-probe") {
            options.cabinet_drain_probe = true;
        } else if (argument == "--cabinet-nudge-probe") {
            options.cabinet_nudge_probe = true;
        } else if (argument == "--nudge-multiplier") {
            const auto value = index + 1 < argc
                ? devils_crush::input::parse_nudge_multiplier(argv[++index])
                : std::nullopt;
            if (!value) {
                options.error = "--nudge-multiplier requires a number from 0 to 16.";
                return false;
            }
            options.nudge_multiplier = *value;
            overrides.nudge_multiplier = true;
        } else if (argument == "--cabinet-launch-check") {
            options.cabinet_launch_check = true;
        } else if (argument == "--cabinet-launch-flipper-probe") {
            options.cabinet_launch_flipper_probe = true;
        } else if (argument == "--cabinet-plunger-probe") {
            options.cabinet_plunger_probe = true;
        } else if (argument == "--cabinet-plunger-check") {
            options.cabinet_plunger_check = true;
        } else if (argument == "--cabinet-ball-drag-check") {
            options.cabinet_ball_drag_check = true;
        } else if (argument == "--verify-unsigned-square-root") {
            options.verify_unsigned_square_root = true;
        } else if (argument == "--verify-unsigned-word-multiply-partial") {
            options.verify_unsigned_word_multiply_partial = true;
        } else if (argument == "--verify-signed-word-normalizer") {
            options.verify_signed_word_normalizer = true;
        } else if (argument == "--verify-signed-word-multiply") {
            options.verify_signed_word_multiply = true;
        } else if (argument == "--verify-side-gate-deadline-check") {
            options.verify_side_gate_deadline_check = true;
        } else if (argument == "--high-score-check") {
            options.high_score_check = true;
        } else if (argument == "--cabinet-view-probe") {
            if (index + 1 >= argc) {
                options.error = "--cabinet-view-probe requires a directory.";
                return false;
            }
            options.cabinet_view_probe_directory = argv[++index];
        } else if (argument == "--cabinet-motion-check" || argument == "--cabinet-motion-gpu-check") {
            if (index + 1 >= argc) {
                options.error = "--cabinet-motion-check requires a directory.";
                return false;
            }
            options.cabinet_motion_check_directory = argv[++index];
            options.cabinet_motion_gpu_check = argument == "--cabinet-motion-gpu-check";
        } else if (argument == "--cabinet-seed-check") {
            options.cabinet_seed_check = true;
        } else if (argument == "--cabinet-palette-check") {
            options.cabinet_palette_check = true;
        } else if (argument == "--cabinet-attribute-check") {
            options.cabinet_attribute_check = true;
        } else if (argument == "--cabinet-state-dump") {
            if (index + 1 >= argc) {
                options.error = "--cabinet-state-dump requires a directory.";
                return false;
            }
            options.cabinet_state_dump_directory = argv[++index];
        } else if (argument == "--emulate-only") {
            options.emulate_only = true;
        } else if (argument == "--native-gameplay-bootstrap-shadow") {
            options.native_gameplay_bootstrap_shadow = true;
        } else if (argument == "--registry-slice") {
            if (index + 1 >= argc) {
                options.error =
                    "--registry-slice requires FIRST:COUNT.";
                return false;
            }
            const std::string_view value = argv[++index];
            const auto separator = value.find(':');
            if (separator == std::string_view::npos) {
                options.error =
                    "--registry-slice requires FIRST:COUNT.";
                return false;
            }
            const auto parse_index =
                [](const std::string_view text) -> std::optional<std::size_t> {
                std::size_t parsed = 0U;
                const auto result = std::from_chars(
                    text.data(),
                    text.data() + text.size(),
                    parsed);
                if (result.ec != std::errc{}
                    || result.ptr != text.data() + text.size()) {
                    return std::nullopt;
                }
                return parsed;
            };
            const auto first = parse_index(value.substr(0U, separator));
            const auto count = parse_index(value.substr(separator + 1U));
            if (!first || !count || *count == 0U) {
                options.error =
                    "--registry-slice requires decimal FIRST:COUNT with a "
                    "nonzero count.";
                return false;
            }
            options.registry_slice_set = true;
            options.registry_slice_first = *first;
            options.registry_slice_count = *count;
        } else if (argument == "--registry-exclude") {
            if (index + 1 >= argc) {
                options.error = "--registry-exclude requires BANK:PC.";
                return false;
            }
            const auto routine = parse_routine_key(argv[++index]);
            if (!routine) {
                options.error =
                    "--registry-exclude requires hexadecimal BANK:PC.";
                return false;
            }
            options.registry_excludes.push_back(*routine);
        } else if (argument == "--native-cycle-census") {
            if (index + 1 >= argc) {
                options.error =
                    "--native-cycle-census requires a path.";
                return false;
            }
            options.native_cycle_census_path = argv[++index];
        } else if (argument == "--instruction-profile") {
            if (index + 1 >= argc) {
                options.error =
                    "--instruction-profile requires a path.";
                return false;
            }
            options.instruction_profile_path = argv[++index];
        } else if (argument == "--abandonment-census") {
            if (index + 1 >= argc) {
                options.error =
                    "--abandonment-census requires a path.";
                return false;
            }
            options.abandonment_census_path = argv[++index];
        } else if (argument == "--backup-ram-test") {
            options.backup_ram_test = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.backup_ram_test_path = argv[++index];
            }
        } else {
            options.error = "Unknown option: " + std::string(argument);
            return false;
        }
    }
    return true;
}

} // namespace devils_crush::app::host
