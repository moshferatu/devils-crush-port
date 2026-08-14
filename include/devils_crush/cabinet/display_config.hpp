// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "devils_crush/input/plunger.hpp"

namespace devils_crush::cabinet {

inline constexpr std::uint32_t kDisplayConfigVersion = 1U;

struct DisplayRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    [[nodiscard]] bool valid() const noexcept
    {
        return width > 0 && height > 0;
    }

    [[nodiscard]] bool operator==(const DisplayRect&) const = default;
};

enum class DisplayRotation : std::uint8_t {
    unknown = 0,
    landscape = 1,
    landscape_flipped = 2,
    portrait = 3,
    portrait_flipped = 4,
};

struct DisplayDescriptor {
    std::string identity;
    DisplayRect bounds;
    DisplayRotation rotation = DisplayRotation::unknown;
    bool primary = false;
};

struct WindowPlacement {
    std::string display_identity;
    DisplayRect geometry;
    DisplayRotation rotation = DisplayRotation::unknown;
    bool fullscreen = false;

    [[nodiscard]] bool operator==(const WindowPlacement&) const = default;
};

// The eight PC Engine pad inputs, in the order they are written to
// `cabinet.ini`. These are pad inputs rather than game actions on purpose:
// the cartridge decides what each one does, and it does not do the same thing
// in a menu as it does in play.
//
// During play `01:D466` gates each flipper piece on `$3B80,X`. The left-side
// pieces `$04/$06/$08` carry mask `$F0`, so *any* direction works the left
// flipper; the right-side pieces `$05/$07/$09` carry mask `$01`, so button I
// works the right flipper and also launches the ball. Button II is nudge.
enum class PadButton : std::size_t {
    one = 0,
    two = 1,
    select = 2,
    run = 3,
    up = 4,
    right = 5,
    down = 6,
    left = 7,
};

inline constexpr std::size_t kPadButtonCount = 8U;

// Which pad inputs work each flipper.
//
// The cartridge gives the left-side pieces `$04/$06/$08` the input mask `$F0`
// at `$3B80,X` and the right-side pieces `$05/$07/$09` the mask `$01`, so
// `01:D466` opens the left gate on up, right, down *or* left, and the right
// gate on button I alone.
//
// That coupling is the problem on a cabinet. A flipper button has to work its
// flipper during play *and* do something sensible on the screens that are not
// play -- scrolling a letter during high-score entry, for instance -- and the
// cartridge decides the latter from which pad input arrives. Rewriting these
// masks lets a key send whatever pad input the other screens want while still
// working the flipper it is physically wired to.
//
// An empty list leaves the cartridge's own mask untouched.
struct FlipperInputs {
    std::vector<PadButton> left;
    std::vector<PadButton> right;

    [[nodiscard]] bool operator==(const FlipperInputs&) const = default;
};

// The actor slots each flipper owns, per `docs/ROUTINE_MAP.md`.
inline constexpr std::array<std::size_t, 3> kLeftFlipperSlots{
    0x04U, 0x06U, 0x08U};
inline constexpr std::array<std::size_t, 3> kRightFlipperSlots{
    0x05U, 0x07U, 0x09U};

// What the piece initializers `01:D335`/`01:D376` themselves store at
// `$3B80,X`, and so what the byte reads as before anything rewrites it.
inline constexpr std::uint8_t kCartridgeLeftFlipperMask = 0xF0U;
inline constexpr std::uint8_t kCartridgeRightFlipperMask = 0x01U;

// Whether `$3B80,X` may be rewritten to `desired` for one flipper piece.
//
// `$3B80,X` is a general-purpose per-actor byte rather than a dedicated
// flipper field -- `03:6697` stores `1 + (A & $1F)` there and `03:68B1`
// stores `$40 + (A & $1F)` -- so the slot only holds an input mask while a
// flipper piece occupies it. Overwriting it regardless corrupts whatever
// else is using the slot; measured, that silenced the title-screen music,
// which keeps a counter in slot `$05`.
//
// So the byte is only rewritten while it still reads as the initializer left
// it. That also makes the rewrite idempotent, and it reapplies if a piece is
// re-initialized mid-session. Note `kCartridgeRightFlipperMask` falls inside
// `03:6697`'s `$01-$20` range, so this test is necessary but not sufficient
// on its own -- callers must already know a table session is in progress.
[[nodiscard]] bool rewrites_flipper_input_mask(
    std::uint8_t current,
    std::uint8_t cartridge_mask,
    std::uint8_t desired) noexcept;

// Keys are held as SDL scancode names -- "Left Shift", "Left", "Z" -- so the
// host can resolve them with `SDL_GetScancodeFromName` and the file stays
// hand-editable without a bespoke key table to keep in step. A bound input
// with no keys is unbound; that is distinct from the field being absent,
// which takes the default.
struct InputBindings {
    std::array<std::vector<std::string>, kPadButtonCount> keys;
    FlipperInputs flippers;

    // Keys that send button I, and only button I, while the ball is sitting
    // in the Devil Shooter.
    //
    // Launching is button I, and so is accepting a letter during high-score
    // entry -- the Devil Shooter reads it at `02:5159` as
    // `LDA $2013,Y : AND #$01`, an immediate operand with no per-actor mask
    // to rewrite. So a key cannot launch and also avoid accepting unless the
    // host looks at where the ball is. Session flag `$53` bit 3 marks the
    // shooter session and nothing else, which makes it exactly the condition.
    //
    // A launch key's ordinary pad input is suppressed while it applies, so
    // binding one that normally sends Run pauses the game everywhere except
    // the one screen where it should launch instead.
    std::vector<std::string> launch_keys;

    // The analog plunger, if the cabinet has one. A drawn plunger does what a
    // launch key does and more: it holds button I while the rod is back, and
    // it also puts the Devil Shooter's own piece where the rod is, so the
    // charge the cartridge launches on is the distance the player pulled
    // rather than the time they waited. See `devils_crush::input::Plunger`.
    input::PlungerBinding plunger;

    [[nodiscard]] const std::vector<std::string>& operator[](
        const PadButton button) const noexcept
    {
        return keys[static_cast<std::size_t>(button)];
    }

    [[nodiscard]] std::vector<std::string>& operator[](
        const PadButton button) noexcept
    {
        return keys[static_cast<std::size_t>(button)];
    }

    [[nodiscard]] bool operator==(const InputBindings&) const = default;
};

// The `input.` suffix each pad input is stored under. Also the name used in
// the `left_flipper`/`right_flipper` lists.
[[nodiscard]] std::string_view pad_button_field(PadButton button) noexcept;

// The `$2CEE` bit a pad input arrives as, which is also its bit in the
// `$3B80,X` mask `01:D466` tests against.
[[nodiscard]] std::uint8_t pad_button_bit(PadButton button) noexcept;

// The `$3B80,X` mask a list of pad inputs comes to. Zero for an empty list,
// which callers read as "leave the cartridge's mask alone".
[[nodiscard]] std::uint8_t flipper_input_mask(
    std::span<const PadButton> inputs) noexcept;

// Whether a held-input byte asks for left and right, or up and down, at once.
//
// A real pad cannot be pushed both ways, and the cartridge rejects the
// impossible rather than interpreting it: the whole byte arrives at `$2CEE`
// as zero, not merely the two bits. Measured, `$A0` and `$50` are zeroed
// while `$90`, `$C0`, `$81` and `$84` pass through intact.
[[nodiscard]] bool has_opposing_directions(std::uint8_t pad) noexcept;

// Moves one flipper onto another input it already accepts so that an
// otherwise impossible pad byte survives.
//
// Two flipper buttons on opposing directions silence each other, and neither
// flipper moves. A flipper's input list is a set of equivalents -- `01:D466`
// opens its gate on any bit in the rewritten mask -- so one of them can be
// swapped for a non-opposing sibling and both still fire. Each flipper keeps
// its natural direction when pressed alone, which is all the menus and
// high-score entry ever see; the substitution exists only for the frames both
// are held. A byte that needs no help, or a configuration that offers no
// alternative, is returned unchanged.
[[nodiscard]] std::uint8_t resolve_opposing_directions(
    std::uint8_t pad,
    const FlipperInputs& flippers) noexcept;

// Left Shift and Left work the left flipper; Right Shift and Right work the
// right flipper and launch. Z and X stay bound so the original keyboard
// layout still works, and D-pad right has no default key because Right now
// belongs to the right flipper.
[[nodiscard]] InputBindings default_input_bindings();

struct DisplayConfig {
    std::uint32_t format_version = kDisplayConfigVersion;
    WindowPlacement playfield;
    WindowPlacement dmd;
    WindowPlacement backglass;
    InputBindings input = default_input_bindings();

    [[nodiscard]] bool operator==(const DisplayConfig&) const = default;
};

enum class ConfigLoadStatus {
    loaded,
    not_found,
    invalid,
    io_error,
};

struct ConfigLoadResult {
    ConfigLoadStatus status = ConfigLoadStatus::io_error;
    DisplayConfig config;
    std::string message;

    [[nodiscard]] bool loaded() const noexcept
    {
        return status == ConfigLoadStatus::loaded;
    }
};

struct ConfigSaveResult {
    bool saved = false;
    std::string message;
};

[[nodiscard]] std::vector<std::byte> encode_display_config(
    const DisplayConfig& config);

[[nodiscard]] std::optional<DisplayConfig> decode_display_config(
    std::span<const std::byte> bytes);

[[nodiscard]] ConfigLoadResult load_display_config(
    const std::filesystem::path& path);

[[nodiscard]] ConfigSaveResult save_display_config_atomic(
    const std::filesystem::path& path,
    const DisplayConfig& config);

[[nodiscard]] std::optional<std::size_t> resolve_display(
    const WindowPlacement& placement,
    std::span<const DisplayDescriptor> displays) noexcept;

[[nodiscard]] DisplayRect fit_window_geometry(
    const WindowPlacement& placement,
    const DisplayDescriptor& display,
    int default_width,
    int default_height) noexcept;

} // namespace devils_crush::cabinet
