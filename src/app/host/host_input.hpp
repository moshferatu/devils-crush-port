// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"

namespace devils_crush::app::host {

// Scancodes resolved once from the configured key names, so the per-frame
// read stays an array lookup. A pad input's list may also name controller
// inputs ("PinOne Button 3", "Hat 1 Left"); those are split out into
// `pad_joystick`.
struct ResolvedKeyBindings {
    std::array<
        std::vector<SDL_Scancode>,
        devils_crush::cabinet::kPadButtonCount>
        pads;
    std::vector<SDL_Scancode> launch;
    std::array<
        std::vector<devils_crush::input::JoystickInput>,
        devils_crush::cabinet::kPadButtonCount>
        pad_joystick;
    std::vector<devils_crush::input::JoystickInput> launch_joystick;
    std::vector<SDL_Scancode> smoothing;
    std::vector<SDL_Scancode> next_window;
    std::vector<SDL_Scancode> fullscreen;
    std::vector<SDL_Scancode> next_monitor;
};

// The controllers whose inputs a pad input may be bound to. Read through
// SDL's joystick layer rather than its gamepad layer for the same reason as
// the plunger: a pinball board in its own controller mode is an anonymous HID
// device that no gamepad mapping covers. Every connected controller is held
// open, but only while some pad input names a controller input.
struct JoystickInputs {
    struct Device {
        SDL_Joystick* joystick = nullptr;
        SDL_JoystickID id = 0;
        std::string name;
    };
    std::vector<Device> devices;
    bool wanted = false;

    void refresh();
    void remove(SDL_JoystickID id);
    void close();
    [[nodiscard]] bool pressed(
        const devils_crush::input::JoystickInput& binding) const;
};

[[nodiscard]] bool key_is_bound(
    const std::vector<SDL_Scancode>& keys,
    SDL_Scancode scancode) noexcept;

[[nodiscard]] ResolvedKeyBindings resolve_key_bindings(
    const devils_crush::cabinet::InputBindings& bindings);

void resolve_opposing_directions(
    std::uint16_t& pad,
    const devils_crush::cabinet::FlipperInputs& flippers);

[[nodiscard]] InputFrame read_input(
    SDL_Gamepad* gamepad,
    const JoystickInputs& joysticks,
    const ResolvedKeyBindings& bindings,
    const bool ball_in_launcher,
    const devils_crush::cabinet::FlipperInputs& flippers,
    const bool plunger_drawn);

[[nodiscard]] SDL_Gamepad* open_first_gamepad();

[[nodiscard]] bool load_gamepad_mappings(
    const std::filesystem::path& path);

void handle_gamepad_device_event(
    const SDL_GamepadDeviceEvent& event,
    SDL_Gamepad*& gamepad);

// The plunger is read as a raw joystick rather than as a gamepad, and that is
// deliberate rather than incidental. SDL's gamepad layer only answers for a
// controller its database has a mapping for, and no plunger board is in it: a
// cabinet's plunger is a potentiometer on an Arduino, and what it presents is
// an axis on an anonymous HID device. The joystick layer answers for anything
// with an axis, which is exactly the set of devices this has to work with.
struct PlungerDevice {
    SDL_Joystick* joystick = nullptr;
    SDL_JoystickID id = 0;
    devils_crush::input::Plunger model;
};

[[nodiscard]] bool name_contains(
    const char* const name,
    const std::string& needle);

struct NudgeDevice {
    SDL_Joystick* joystick = nullptr;
    devils_crush::input::NudgeBinding binding;
    devils_crush::input::Nudge model;

    void close()
    {
        if (joystick != nullptr) SDL_CloseJoystick(joystick);
        joystick = nullptr;
        model.rearm();
    }

    void refresh()
    {
        if (joystick != nullptr && SDL_JoystickConnected(joystick)) return;
        close();
        if (!binding.enabled) return;
        int count = 0;
        auto* ids = SDL_GetJoysticks(&count);
        for (int i = 0; i < count; ++i) {
            const char* name = SDL_GetJoystickNameForID(ids[i]);
            if (!name_contains(name, binding.device)) continue;
            auto* candidate = SDL_OpenJoystick(ids[i]);
            if (candidate == nullptr) continue;
            if (SDL_GetNumJoystickAxes(candidate) <= std::max(binding.axis_x, binding.axis_y)) {
                SDL_CloseJoystick(candidate);
                continue;
            }
            joystick = candidate;
            std::cout << "Cabinet nudge: " << name << ", axes " << binding.axis_x
                      << '/' << binding.axis_y << ", threshold " << binding.threshold << "%\n";
            break;
        }
        SDL_free(ids);
    }

    bool read(bool active)
    {
        if (!active || joystick == nullptr || !SDL_JoystickConnected(joystick)) {
            model.rearm();
            return false;
        }
        return model.read(binding, SDL_GetJoystickAxis(joystick, binding.axis_x),
                          SDL_GetJoystickAxis(joystick, binding.axis_y), SDL_GetTicks(), true);
    }
};

void close_plunger(PlungerDevice& plunger);

void open_plunger(
    PlungerDevice& plunger,
    const devils_crush::input::PlungerBinding& binding);

void handle_joystick_device_event(
    const SDL_JoyDeviceEvent& event,
    PlungerDevice& plunger,
    const devils_crush::input::PlungerBinding& binding);

[[nodiscard]] devils_crush::input::PlungerSample read_plunger(
    PlungerDevice& plunger,
    const devils_crush::input::PlungerBinding& binding);

[[nodiscard]] int run_plunger_probe();

} // namespace devils_crush::app::host
