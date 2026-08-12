// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace devils_crush::input {

// The PC Engine's controller port: two strobe lines, a multitap that counts
// which of five devices answers next, and the devices themselves.
//
// This is the eighth seam, and it is the first that moves no computation at
// all. The strobe protocol this port speaks was already modelled natively --
// the cartridge's pad reader at `00:E195` walks it, predicts what each read
// answers, and has been doing so since phase 1. What it read *through* was
// still the emulator's chain: a frontend button buffer, five
// `PCE_Input_Device` objects, a multitap counter. This is that chain, and
// what changes is who the input belongs to.
//
// **The ownership is the point, and it is testable rather than nominal.**
// While this model is installed the host's input reaches the machine directly
// -- `set_port_input` is the only way buttons get in -- and the emulator's own
// input path is never consulted. A frontend can therefore answer every input
// query with zero and the frame is unchanged, which is what turns "the model
// is installed" from a claim into a measurement: an install that silently fell
// back would produce a machine that nobody was pressing anything on.
//
// **What is a device's and what is the port's.** A device answers four bits
// and is told when the two lines move. The port decides which device is
// answering, holds the lines between writes, and contributes three bits of its
// own -- the CD-unit-absent bit and two that are always set -- which is why
// they are here rather than in a gamepad.
//
// **What the frontend keeps.** Two policies that are the host's rather than
// the machine's: whether opposing directions may be pressed at once, which a
// physical pad settles mechanically and a keyboard does not, and which buttons
// a host key is. The first arrives through `set_frontend_policy`; the second
// never reaches this class at all, because by the time input gets here it is
// already a PC Engine button word.
class PcePad {
public:
    // Five, because the multitap is five and the emulator builds an object per
    // port whether or not anything is plugged in.
    static constexpr std::size_t kPortCount = 5;

    // The bit a six-button pad sets to say it is one. Nothing else in the
    // sixteen-bit word distinguishes the two pads, so a two-button host never
    // reaches the six-button read path no matter which half is selected.
    static constexpr std::uint16_t kSixButtonPad = 0x1000U;

    // Bits the two frontend filters act on: SELECT with RUN is the soft-reset
    // combination, and the two pairs of opposing directions.
    static constexpr std::uint16_t kSoftReset = 0x000CU;
    static constexpr std::uint16_t kUpAndDown = 0x0050U;
    static constexpr std::uint16_t kLeftAndRight = 0x00A0U;

    // The port's own three bits: the CD unit's absence, and two the hardware
    // reads back set no matter what is plugged in.
    static constexpr std::uint8_t kNoCdUnit = 0x80U;
    static constexpr std::uint8_t kAlwaysSet = 0x30U;

    // What the frontend said is on a port. `mouse` and `keyboard` are here so
    // that a port carrying one is *recognised* rather than mistaken for a
    // gamepad: this model answers for a two-button pad and for an empty port,
    // and a port holding either of the other two is counted as unmodelled and
    // answers as though it were empty.
    enum class Device : std::uint8_t {
        none = 0,
        gamepad = 1,
        mouse = 2,
        keyboard = 3,
    };

    // The host's half of the configuration. Not the machine's: a PC Engine has
    // no opinion about whether left and right can be held together, because a
    // pad it is wired to cannot do it.
    struct FrontendPolicy {
        bool allow_opposing_directions = false;
    };

    // One device's serialized state, in the order the emulator's savestate
    // carries it.
    struct PortState {
        std::uint16_t buttons = 0;
        bool select = false;
        bool clear = false;
        bool six_button_half = false;

        [[nodiscard]] bool operator==(const PortState&) const = default;
    };

    struct State {
        std::array<PortState, kPortCount> ports{};
        bool select = false;
        bool clear = false;
        std::uint8_t read_index = 0;

        [[nodiscard]] bool operator==(const State&) const = default;
    };

    // Every path this model can take, counted -- because agreement with the
    // emulator is only worth the paths a route actually drove, and this device
    // has several that a two-button pad in a single-player game never reaches.
    struct Coverage {
        // The machine's four entry points.
        std::uint64_t reads = 0;
        std::uint64_t writes = 0;
        std::uint64_t frames = 0;
        std::uint64_t frame_starts = 0;
        std::uint64_t timestamp_rebases = 0;
        std::uint64_t transforms = 0;
        std::uint64_t powers = 0;

        // How a read was routed. `multitap_reads` and `direct_reads` are the
        // two ways the port picks a device; `reads_past_the_last_port` is the
        // multitap counter walked beyond its five devices, which answers zero
        // rather than the absent-device pattern and is how a game counts how
        // many pads are attached.
        std::uint64_t multitap_reads = 0;
        std::uint64_t direct_reads = 0;
        std::uint64_t reads_past_the_last_port = 0;
        std::uint64_t absent_port_reads = 0;

        // Which half of a pad answered. The six-button path needs a pad that
        // says it is one, so on a two-button machine it is expected to be zero
        // and a nonzero count means the host is sending a bit it should not.
        std::uint64_t select_high_reads = 0;
        std::uint64_t select_low_reads = 0;
        std::uint64_t six_button_reads = 0;

        // Reads that answered with at least one button held. This is the
        // counter the ownership claim rests on rather than a path count: it
        // is the host's input arriving at the machine, measured at the only
        // place it can be. A route that presses things and reports zero here
        // is a route whose input went somewhere else.
        std::uint64_t reads_with_a_button_down = 0;

        // What a write did to the multitap counter, and to a six-button pad's
        // half-selector -- which toggles on the same edge whether or not any
        // pad on the port is a six-button one.
        std::uint64_t counter_resets = 0;
        std::uint64_t counter_increments = 0;
        std::uint64_t counter_saturations = 0;
        std::uint64_t six_button_toggles = 0;

        // The two filters, counted where they removed something.
        std::uint64_t soft_resets_suppressed = 0;
        std::uint64_t opposing_directions_filtered = 0;

        // Ports configured to a device this model does not answer for. A
        // fault rather than a variation: it means the machine has a device on
        // it that the model is quietly treating as an empty port.
        std::uint64_t unmodelled_port_devices = 0;

        [[nodiscard]] bool operator==(const Coverage&) const = default;
    };

    // The machine's settings, as the emulator resolved them from its own.
    void configure(
        bool multitap_enabled,
        bool disable_soft_reset,
        bool cd_attached) noexcept;
    void configure_port(std::size_t port, Device device) noexcept;
    void set_frontend_policy(const FrontendPolicy& policy) noexcept;

    // The host's input, per port, in PC Engine button order. Filtered on the
    // way in exactly where a frontend filters it, and held until the machine's
    // next frame latch -- so a host may set it whenever it likes and the
    // moment a device sees it is still the machine's to decide.
    void set_port_input(std::size_t port, std::uint16_t buttons) noexcept;

    // The machine's four entry points into the port, plus its reset. A
    // gamepad keeps no time, so the rebase has nothing to rebase; the entry
    // exists because the port has it and a mouse would need it.
    void power(std::int32_t timestamp) noexcept;
    void frame(bool start_frame) noexcept;
    void transform_input() noexcept;
    void adjust_timestamp(std::int32_t delta) noexcept;
    [[nodiscard]] std::uint8_t read(
        std::int32_t timestamp,
        std::uint32_t address) noexcept;
    void write(
        std::int32_t timestamp,
        std::uint32_t address,
        std::uint8_t value) noexcept;

    void capture_state(State& state) const noexcept;
    void apply_state(const State& state) noexcept;

    [[nodiscard]] const Coverage& coverage() const noexcept
    {
        return coverage_;
    }

private:
    struct Port {
        std::uint16_t buttons = 0;
        std::uint16_t pending_buttons = 0;
        Device device = Device::none;
        bool select = false;
        bool clear = false;
        bool six_button_half = false;
    };

    [[nodiscard]] std::uint8_t read_port(std::size_t port) noexcept;
    // The two lines are the *port's* previous ones rather than the device's
    // own copies of them, which is a distinction with a consequence: a device
    // plugged in after a strobe began would see the edge the port saw.
    void write_port(
        std::size_t port,
        bool old_select,
        bool new_select,
        bool old_clear,
        bool new_clear) noexcept;
    [[nodiscard]] std::uint16_t filtered(std::uint16_t buttons) noexcept;

    std::array<Port, kPortCount> ports_{};
    FrontendPolicy policy_{};
    bool multitap_enabled_ = false;
    bool disable_soft_reset_ = false;
    bool cd_attached_ = false;
    bool select_ = false;
    bool clear_ = false;
    std::uint8_t read_index_ = 0;
    Coverage coverage_{};
};

}  // namespace devils_crush::input
