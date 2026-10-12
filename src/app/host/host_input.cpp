// SPDX-License-Identifier: GPL-3.0-or-later
#include "host_input.hpp"

#include "host_files.hpp"
#include "host_textures.hpp"

namespace devils_crush::app::host {

namespace {

void set_button(
    std::uint16_t& mask,
    const Button button,
    const bool is_pressed)
{
    if (is_pressed) {
        mask |= static_cast<std::uint16_t>(button);
    }
}

// The pad inputs in `PadButton` order, paired with the emulator button they
// set and the gamepad button that also reaches them. The gamepad keeps its
// fixed layout: only the keyboard has to match whatever a cabinet's encoder
// is wired to send.
struct PadBinding {
    devils_crush::cabinet::PadButton pad;
    Button button;
    SDL_GamepadButton gamepad;
};

constexpr std::array<PadBinding, devils_crush::cabinet::kPadButtonCount>
    kPadBindings{
        PadBinding{
            devils_crush::cabinet::PadButton::one,
            Button::one,
            SDL_GAMEPAD_BUTTON_SOUTH},
        PadBinding{
            devils_crush::cabinet::PadButton::two,
            Button::two,
            SDL_GAMEPAD_BUTTON_EAST},
        PadBinding{
            devils_crush::cabinet::PadButton::select,
            Button::select,
            SDL_GAMEPAD_BUTTON_BACK},
        PadBinding{
            devils_crush::cabinet::PadButton::run,
            Button::run,
            SDL_GAMEPAD_BUTTON_START},
        PadBinding{
            devils_crush::cabinet::PadButton::up,
            Button::up,
            SDL_GAMEPAD_BUTTON_DPAD_UP},
        PadBinding{
            devils_crush::cabinet::PadButton::right,
            Button::right,
            SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
        PadBinding{
            devils_crush::cabinet::PadButton::down,
            Button::down,
            SDL_GAMEPAD_BUTTON_DPAD_DOWN},
        PadBinding{
            devils_crush::cabinet::PadButton::left,
            Button::left,
            SDL_GAMEPAD_BUTTON_DPAD_LEFT},
    };

[[nodiscard]] std::vector<SDL_Scancode> resolve_key_names(
    const std::vector<std::string>& names,
    const std::string_view field)
{
    std::vector<SDL_Scancode> resolved;
    for (const auto& name : names) {
        const auto scancode = SDL_GetScancodeFromName(name.c_str());
        if (scancode == SDL_SCANCODE_UNKNOWN) {
            // An unusable name is the one thing a hand-edited file gets wrong
            // silently, so name it and keep the rest of the binding rather
            // than dropping the whole input.
            std::cerr << "Unknown key name \"" << name << "\" bound to input."
                      << field << "; ignoring it.\n";
            continue;
        }
        resolved.push_back(scancode);
    }
    return resolved;
}

void log_opened_gamepad(SDL_Gamepad* gamepad)
{
    const char* name = SDL_GetGamepadName(gamepad);
    std::cout << "Using gamepad: "
              << (name != nullptr ? name : "unnamed SDL gamepad")
              << '\n';
}

[[nodiscard]] SDL_Gamepad* open_gamepad(
    const SDL_JoystickID gamepad_id)
{
    SDL_Gamepad* gamepad = SDL_OpenGamepad(gamepad_id);
    if (gamepad == nullptr) {
        report_sdl_error("SDL_OpenGamepad failed");
        return nullptr;
    }
    log_opened_gamepad(gamepad);
    return gamepad;
}

// A controller is the plunger if its name matches and it actually has the axis
// the binding names. The second half matters more than it looks: a cabinet has
// several HID devices on it, and opening the wrong one and reading past its
// axis count would answer a constant rather than fail.
[[nodiscard]] bool is_plunger_candidate(
    const SDL_JoystickID id,
    const devils_crush::input::PlungerBinding& binding)
{
    if (!name_contains(SDL_GetJoystickNameForID(id), binding.device)) {
        return false;
    }
    SDL_Joystick* const joystick = SDL_OpenJoystick(id);
    if (joystick == nullptr) {
        return false;
    }
    const bool has_axis = SDL_GetNumJoystickAxes(joystick) > binding.axis;
    SDL_CloseJoystick(joystick);
    return has_axis;
}

// Finds out what a cabinet's plunger actually is, by watching it move.
//
// None of the four numbers this feature needs can be guessed from here. Which
// axis a board puts the rod on is the board's choice -- the PinOne uses Z,
// which Windows reports third, but nothing makes that universal -- and the two
// endpoints are a potentiometer's, so they differ between two copies of the
// same board. What they have in common is that they are all obvious the moment
// somebody pulls the rod: the plunger is the axis that moves, rest is where it
// sits, and a full draw is as far as it went. So the probe watches, and prints
// the answer in the form it is written down in.
constexpr int kPlungerProbeRestMs = 2000;
constexpr int kPlungerProbeWatchMs = 12000;
constexpr int kPlungerProbeReportMs = 250;
// Below this the axis did not move enough to be a rod being pulled. A
// sixteenth of the axis's range is far more than a potentiometer's noise and
// far less than any deliberate pull.
constexpr int kPlungerProbeTravel = 4096;

struct ProbedAxis {
    SDL_JoystickID id = 0;
    std::string name;
    int axis = 0;
    int rest = 0;
    int extreme = 0;

    [[nodiscard]] int travel() const noexcept
    {
        return std::abs(extreme - rest);
    }
};

} // namespace

[[nodiscard]] ResolvedKeyBindings resolve_key_bindings(
    const devils_crush::cabinet::InputBindings& bindings)
{
    ResolvedKeyBindings resolved;
    for (const auto& binding : kPadBindings) {
        resolved.pads[static_cast<std::size_t>(binding.pad)] =
            resolve_key_names(
                bindings[binding.pad],
                devils_crush::cabinet::pad_button_field(binding.pad));
    }
    resolved.launch = resolve_key_names(bindings.launch_keys, "launch");
    return resolved;
}

// The pad byte and the emulator's `Button` bits are the same eight bits, so
// resolving the impossible-direction case is a straight hand-off.
void resolve_opposing_directions(
    std::uint16_t& pad,
    const devils_crush::cabinet::FlipperInputs& flippers)
{
    pad = devils_crush::cabinet::resolve_opposing_directions(
        static_cast<std::uint8_t>(pad & 0xFFU), flippers);
}

[[nodiscard]] InputFrame read_input(
    SDL_Gamepad* gamepad,
    const ResolvedKeyBindings& bindings,
    const bool ball_in_launcher,
    const devils_crush::cabinet::FlipperInputs& flippers,
    const bool plunger_drawn)
{
    InputFrame input;
    const bool* keys = SDL_GetKeyboardState(nullptr);

    // While the ball is in the shooter a launch key means launch and nothing
    // else. Suppressing its ordinary pad input is the point: a launch button
    // that also sends Run would pause the game at the moment it is pressed.
    const bool launching = ball_in_launcher
        && std::ranges::any_of(
               bindings.launch,
               [keys](const SDL_Scancode scancode) {
                   return keys[scancode];
               });
    const auto suppressed = [&](const SDL_Scancode scancode) {
        return ball_in_launcher
            && std::ranges::find(bindings.launch, scancode)
            != bindings.launch.end();
    };

    for (const auto& binding : kPadBindings) {
        const auto& scancodes =
            bindings.pads[static_cast<std::size_t>(binding.pad)];
        const bool pressed =
            std::ranges::any_of(
                scancodes,
                [&](const SDL_Scancode scancode) {
                    return keys[scancode] && !suppressed(scancode);
                })
            || (gamepad != nullptr
                && SDL_GetGamepadButton(gamepad, binding.gamepad));
        set_button(input.player_one, binding.button, pressed);
    }
    // A drawn plunger holds button I on the same terms as a launch key, and
    // for the same reason: button I launches, and button I also accepts a
    // high-score letter. The rod is only allowed to send it where it can only
    // mean launch. It has no ordinary pad input to suppress -- a plunger is
    // not a key -- so it needs no entry in `suppressed`.
    if (launching || (ball_in_launcher && plunger_drawn)) {
        set_button(input.player_one, Button::one, true);
    }
    resolve_opposing_directions(input.player_one, flippers);
    return input;
}

[[nodiscard]] SDL_Gamepad* open_first_gamepad()
{
    int count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&count);
    if (gamepads == nullptr || count <= 0) {
        SDL_free(gamepads);
        return nullptr;
    }

    const auto gamepad_id = gamepads[0];
    SDL_free(gamepads);
    return open_gamepad(gamepad_id);
}

[[nodiscard]] bool load_gamepad_mappings(
    const std::filesystem::path& path)
{
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        if (error) {
            std::cerr << "Could not inspect gamepad mapping file: "
                      << error.message() << '\n';
        }
        return false;
    }

    const auto path_utf8 = path_to_utf8(path);
    const auto mapping_count =
        SDL_AddGamepadMappingsFromFile(path_utf8.c_str());
    if (mapping_count < 0) {
        report_sdl_error("SDL_AddGamepadMappingsFromFile failed");
        return false;
    }
    std::cout << "Loaded " << mapping_count << " custom gamepad mapping(s): "
              << path.string() << '\n';
    return true;
}

void handle_gamepad_device_event(
    const SDL_GamepadDeviceEvent& event,
    SDL_Gamepad*& gamepad)
{
    if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
        if (gamepad == nullptr) {
            gamepad = open_gamepad(event.which);
        }
        return;
    }
    if (gamepad == nullptr || SDL_GetGamepadID(gamepad) != event.which) {
        return;
    }

    if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
        std::cerr << "Active gamepad disconnected.\n";
        SDL_CloseGamepad(gamepad);
        gamepad = open_first_gamepad();
    } else if (event.type == SDL_EVENT_GAMEPAD_REMAPPED) {
        const char* name = SDL_GetGamepadName(gamepad);
        std::cout << "Gamepad mapping updated: "
                  << (name != nullptr ? name : "unnamed SDL gamepad")
                  << '\n';
    }
}

[[nodiscard]] bool name_contains(
    const char* const name,
    const std::string& needle)
{
    if (needle.empty()) {
        return true;
    }
    if (name == nullptr) {
        return false;
    }
    const auto fold = [](const std::string_view text) {
        std::string folded(text);
        std::ranges::transform(
            folded,
            folded.begin(),
            [](const char value) {
                return static_cast<char>(
                    std::tolower(static_cast<unsigned char>(value)));
            });
        return folded;
    };
    return fold(name).find(fold(needle)) != std::string::npos;
}

void close_plunger(PlungerDevice& plunger)
{
    if (plunger.joystick != nullptr) {
        SDL_CloseJoystick(plunger.joystick);
    }
    plunger.joystick = nullptr;
    plunger.id = 0;
    // The rod's position is the device's, so it goes with the device. What
    // stays is the calibration, which is the cabinet's.
    plunger.model.rearm();
}

void open_plunger(
    PlungerDevice& plunger,
    const devils_crush::input::PlungerBinding& binding)
{
    if (!binding.enabled() || plunger.joystick != nullptr) {
        return;
    }
    int count = 0;
    SDL_JoystickID* const ids = SDL_GetJoysticks(&count);
    if (ids == nullptr) {
        return;
    }
    for (int index = 0; index < count; ++index) {
        if (!is_plunger_candidate(ids[index], binding)) {
            continue;
        }
        plunger.joystick = SDL_OpenJoystick(ids[index]);
        if (plunger.joystick == nullptr) {
            report_sdl_error("SDL_OpenJoystick failed");
            continue;
        }
        plunger.id = SDL_GetJoystickID(plunger.joystick);
        plunger.model.configure(binding.calibration);
        const char* const name = SDL_GetJoystickName(plunger.joystick);
        std::cout << "Using plunger: "
                  << (name != nullptr ? name : "unnamed SDL joystick")
                  << " axis " << binding.axis << '\n';
        break;
    }
    SDL_free(ids);
}

void handle_joystick_device_event(
    const SDL_JoyDeviceEvent& event,
    PlungerDevice& plunger,
    const devils_crush::input::PlungerBinding& binding)
{
    if (event.type == SDL_EVENT_JOYSTICK_ADDED) {
        open_plunger(plunger, binding);
        return;
    }
    if (event.type == SDL_EVENT_JOYSTICK_REMOVED
        && plunger.joystick != nullptr && plunger.id == event.which) {
        std::cerr << "Plunger disconnected.\n";
        close_plunger(plunger);
        open_plunger(plunger, binding);
    }
}

// One frame's reading. A plunger that is not configured, not plugged in, or
// plugged in and not moving all answer the same thing, which is why the caller
// needs no test of its own: an absent plunger is a plunger at rest.
[[nodiscard]] devils_crush::input::PlungerSample read_plunger(
    PlungerDevice& plunger,
    const devils_crush::input::PlungerBinding& binding)
{
    if (plunger.joystick == nullptr || !binding.enabled()) {
        return {};
    }
    return plunger.model.update(
        SDL_GetJoystickAxis(plunger.joystick, binding.axis));
}

[[nodiscard]] int run_plunger_probe()
{
    if (!SDL_Init(SDL_INIT_JOYSTICK)) {
        report_sdl_error("SDL_Init failed");
        return EXIT_FAILURE;
    }

    int count = 0;
    SDL_JoystickID* const ids = SDL_GetJoysticks(&count);
    std::vector<SDL_Joystick*> opened;
    std::vector<ProbedAxis> axes;
    for (int index = 0; index < count; ++index) {
        SDL_Joystick* const joystick = SDL_OpenJoystick(ids[index]);
        if (joystick == nullptr) {
            continue;
        }
        opened.push_back(joystick);
        const char* const name = SDL_GetJoystickName(joystick);
        const std::string label =
            name != nullptr ? name : "unnamed SDL joystick";
        const int axis_count = SDL_GetNumJoystickAxes(joystick);
        std::cout << "Controller " << index << ": " << label << " ("
                  << axis_count << " axes, "
                  << SDL_GetNumJoystickButtons(joystick) << " buttons)\n";
        for (int axis = 0; axis < axis_count; ++axis) {
            axes.push_back(
                {.id = ids[index],
                 .name = label,
                 .axis = axis,
                 .rest = 0,
                 .extreme = 0});
        }
    }
    SDL_free(ids);

    if (axes.empty()) {
        std::cerr << "No controller with an axis is connected.\n";
        SDL_Quit();
        return EXIT_FAILURE;
    }

    const auto axis_value = [&opened](const ProbedAxis& probed) {
        for (auto* const joystick : opened) {
            if (SDL_GetJoystickID(joystick) == probed.id) {
                return static_cast<int>(
                    SDL_GetJoystickAxis(joystick, probed.axis));
            }
        }
        return 0;
    };
    const auto pump = [] {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
        }
        SDL_Delay(16);
    };

    std::cout << "\nLeave the plunger alone for two seconds.\n";
    const auto started = SDL_GetTicks();
    while (SDL_GetTicks() - started < kPlungerProbeRestMs) {
        pump();
    }
    for (auto& probed : axes) {
        probed.rest = axis_value(probed);
        probed.extreme = probed.rest;
    }

    std::cout << "Now pull the plunger all the way back and let it go, a few "
                 "times.\n\n";
    const auto watching = SDL_GetTicks();
    Uint64 reported = 0;
    while (SDL_GetTicks() - watching < kPlungerProbeWatchMs) {
        pump();
        for (auto& probed : axes) {
            const int value = axis_value(probed);
            if (std::abs(value - probed.rest)
                > std::abs(probed.extreme - probed.rest)) {
                probed.extreme = value;
            }
        }
        if (SDL_GetTicks() - reported < kPlungerProbeReportMs) {
            continue;
        }
        reported = SDL_GetTicks();
        std::cout << "  ";
        for (const auto& probed : axes) {
            std::cout << "a" << probed.axis << "="
                      << std::setw(6) << axis_value(probed) << ' ';
        }
        std::cout << '\n';
    }

    const auto moved = std::ranges::max_element(
        axes,
        {},
        [](const ProbedAxis& probed) { return probed.travel(); });
    std::cout << '\n';
    if (moved == axes.end() || moved->travel() < kPlungerProbeTravel) {
        std::cerr << "No axis moved far enough to be a plunger.\n";
        for (auto* const joystick : opened) {
            SDL_CloseJoystick(joystick);
        }
        SDL_Quit();
        return EXIT_FAILURE;
    }

    std::cout << "The plunger is axis " << moved->axis << " of \""
              << moved->name << "\", resting at " << moved->rest
              << " and reaching " << moved->extreme << ".\n\n"
              << "Put this in devils-crush.ini:\n\n"
              << "input.plunger.device=" << moved->name << '\n'
              << "input.plunger.axis=" << moved->axis << '\n'
              << "input.plunger.rest=" << moved->rest << '\n'
              << "input.plunger.drawn=" << moved->extreme << '\n';

    for (auto* const joystick : opened) {
        SDL_CloseJoystick(joystick);
    }
    SDL_Quit();
    return EXIT_SUCCESS;
}

} // namespace devils_crush::app::host
