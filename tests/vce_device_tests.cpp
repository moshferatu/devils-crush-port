// SPDX-License-Identifier: GPL-3.0-or-later

// The native colour encoder against the emulator's own device, driven side
// by side from one processor-side stimulus.
//
// The video display controller could be lifted out of the emulator whole:
// it owns its memories and its raster phase and reaches nothing. The
// encoder cannot. It steals processor cycles for a stalled port access,
// drives an interrupt line, asks the processor to stop early at two
// scanlines, and takes its own timestamps from it. So the reference arm here
// is not a device on its own but the smallest machine one will run in --
// the emulator's own processor object, initialised and never executed,
// moved forward by stolen master cycles instead of by instructions.
//
// That shape is what makes the comparison worth having. Time is the
// processor's clock in both arms, so a stall shows up as the two clocks
// parting company, and a schedule that divided a chunk differently shows up
// as a pixel in the wrong column rather than as a number nobody looks at.
//
// Nothing here is sampled from outside -- no ROM, no input, no wall clock --
// so both arms are pure functions of this file. The route census can only
// prove what the game asks for, and the game asks for one dot rate, no
// black-and-white, no 263-line field, and no upscaled pixel path. All four
// are driven here.
//
// Three things are deliberately not compared. The second controller's event
// horizon, because the emulator never initialises it on a machine with one
// controller and it holds whatever the heap did. The request that the
// processor stop early, because `HuC6280::Exit` clears a private flag with
// no accessor -- the two scanlines that trigger it are compared instead.
// And the line-width repair `VCE::RunPartial` performs after the processor
// stops, which cannot be reached without running one; it is checked against
// a stated expectation at the end rather than against the emulator.

#include "dc_beetle_reference_vce.h"
#include "devils_crush/video/pce_vce.hpp"
#include "devils_crush/video/pce_vdc.hpp"
#include "devils_crush/video/vdc_device.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace {

using devils_crush::video::PceVce;
using devils_crush::video::PceVdc;
using devils_crush::video::PceVdcDevice;

bool g_failed = false;

void check(const bool condition, const char* const label)
{
    if (!condition) {
        g_failed = true;
        std::printf("  FAIL  %s\n", label);
    } else {
        std::printf("  ok    %s\n", label);
    }
}

void section(const char* const name)
{
    std::printf("\n%s\n", name);
}

// VDC register select indices, as the controller's own tests name them.
constexpr std::uint8_t kMAWR = 0x00;
constexpr std::uint8_t kVRR = 0x02;
constexpr std::uint8_t kCR = 0x05;
constexpr std::uint8_t kBXR = 0x07;
constexpr std::uint8_t kBYR = 0x08;
constexpr std::uint8_t kMWR = 0x09;
constexpr std::uint8_t kHSR = 0x0A;
constexpr std::uint8_t kHDR = 0x0B;
constexpr std::uint8_t kVPR = 0x0C;
constexpr std::uint8_t kVDW = 0x0D;
constexpr std::uint8_t kVCR = 0x0E;
constexpr std::uint8_t kDCR = 0x0F;
constexpr std::uint8_t kSOUR = 0x10;
constexpr std::uint8_t kDESR = 0x11;
constexpr std::uint8_t kLENR = 0x12;
constexpr std::uint8_t kDVSSR = 0x13;

constexpr std::uint16_t kControlSpritesEnabled = 0x0040;
constexpr std::uint16_t kControlBackgroundEnabled = 0x0080;
constexpr std::uint16_t kControlVerticalBlankIRQ = 0x0008;

// The frontend's framebuffer, which is 2048 wide because the encoder's write
// cursor wraps against 2047 and 264 tall because a field is 263 lines.
constexpr std::int32_t kSurfaceWidth = 2048;
constexpr std::int32_t kSurfaceHeight = 264;
constexpr int kFieldLines = 263;

// One 262-line field in master clocks.
constexpr std::int32_t kFieldClocks = 357630;

// Far enough away that the compact disc event never arrives, which is what
// `VCE::RunPartial` arms it to on a cartridge machine.
constexpr std::int32_t kNoCdEvent = 0x3FFFFFFF;

// What the emulator's processor charges per stolen cycle: three master
// clocks, with no speed shift and no overclock in this machine.
constexpr std::int32_t kMasterClocksPerCycle = 3;

// The one place the two arms are wired to different objects. The reference
// encoder reaches the emulator's processor by name; the native one reaches
// this, which counts the same things.
struct NativeMachine {
    std::int32_t timestamp = 0;
    std::uint64_t stolen_calls = 0;
    std::uint64_t stolen_cycles = 0;
    std::uint64_t exits = 0;
    bool irq = false;
    std::uint64_t irq_calls = 0;
    std::uint64_t scanline_reports = 0;
    PceVce::ScanlineInfo last_report{};

    static void irq_hook(void* const context, const bool raised)
    {
        auto* const self = static_cast<NativeMachine*>(context);
        ++self->irq_calls;
        self->irq = raised;
    }

    static void steal_hook(void* const context, const std::int32_t cycles)
    {
        auto* const self = static_cast<NativeMachine*>(context);
        ++self->stolen_calls;
        self->stolen_cycles += static_cast<std::uint64_t>(cycles);
        self->timestamp += cycles * kMasterClocksPerCycle;
    }

    static void exit_hook(void* const context)
    {
        ++static_cast<NativeMachine*>(context)->exits;
    }

    static void scanline_hook(
        void* const context,
        const PceVce::ScanlineInfo& info)
    {
        auto* const self = static_cast<NativeMachine*>(context);
        ++self->scanline_reports;
        self->last_report = info;
    }
};

// Both encoders, each holding a controller, compared at every step.
class Pair {
public:
    Pair()
        : reference_(dc_beetle_reference_vce_create(65536))
        , native_vdc_(std::make_unique<PceVdc>())
        , reference_pixels_(
              static_cast<std::size_t>(kSurfaceWidth * kSurfaceHeight), 0)
        , native_pixels_(
              static_cast<std::size_t>(kSurfaceWidth * kSurfaceHeight), 0)
    {
        native_vdc_->set_vram_size(65536);
        native_bridge_ = std::make_unique<PceVdcDevice>(*native_vdc_);
        native_ = std::make_unique<PceVce>();

        native_->set_vdc(native_bridge_.get());
        native_->set_irq_hook(&NativeMachine::irq_hook, &machine_);
        native_->set_steal_cycles_hook(
            &NativeMachine::steal_hook, &machine_);
        native_->set_exit_hook(&NativeMachine::exit_hook, &machine_);
        native_->set_scanline_info_hook(
            &NativeMachine::scanline_hook, &machine_);

        // The controller's own two hooks run back into the encoder holding
        // it, which is the join this whole slice exists to make.
        native_vdc_->set_irq_hook(&vdc_irq_hook, this);
        native_vdc_->set_wait_state_hook(&vdc_wait_state_hook, this);

        if (reference_ == nullptr) {
            return;
        }

        dc_beetle_reference_vce_reset(reference_);
        native_->reset();
        machine_.timestamp = 0;

        // What a nine-bit palette entry looks like is the display's
        // decision, so the native encoder is handed the emulator's own
        // expansion rather than deriving one. The point of comparison is
        // the encoder, not the colour arithmetic.
        dc_beetle_reference_vce_set_pixel_format(reference_);
        dc_beetle_reference_vce_surface_palette(
            reference_, surface_normal_.data(), surface_grey_.data());
        native_->set_surface_palette(
            surface_normal_.data(), surface_grey_.data());

        // The colour a disabled layer draws as, which `VCE::StartFrame`
        // re-asserts every frame from a constant.
        native_->set_disabled_layer_colour(
            dc_beetle_reference_vce_disabled_layer_colour());
    }

    ~Pair() { dc_beetle_reference_vce_destroy(reference_); }

    Pair(const Pair&) = delete;
    Pair& operator=(const Pair&) = delete;

    [[nodiscard]] bool valid() const { return reference_ != nullptr; }
    [[nodiscard]] PceVce& native() { return *native_; }
    [[nodiscard]] NativeMachine& machine() { return machine_; }
    [[nodiscard]] const std::int32_t* native_line_widths() const
    {
        return native_line_widths_.data();
    }

    void start_frame(
        const bool skip,
        const int scaling,
        const int scanline_start,
        const int scanline_end)
    {
        dc_beetle_reference_vce_start_frame(
            reference_,
            skip ? nullptr : reference_pixels_.data(),
            kSurfaceWidth,
            kSurfaceHeight,
            kSurfaceWidth,
            skip ? nullptr : reference_line_widths_.data(),
            skip ? 1 : 0,
            scaling,
            scanline_start,
            scanline_end);

        native_->start_frame(
            skip ? nullptr : native_pixels_.data(),
            kSurfaceWidth,
            skip ? nullptr : native_line_widths_.data(),
            skip,
            scaling == 2,
            scanline_start,
            scanline_end);

        compare_schedule("start frame");
    }

    void set_show_horizontal_overscan(const bool show)
    {
        dc_beetle_reference_vce_set_show_horiz_os(reference_, show ? 1 : 0);
        native_->set_show_horizontal_overscan(show);
    }

    // What `VCE::RunPartial` does before it runs the processor.
    void start_partial()
    {
        dc_beetle_reference_vce_reset_wait_states(reference_);
        dc_beetle_reference_vce_arm_cd_event(reference_, kNoCdEvent);

        native_->start_partial();
        native_->set_cd_event(
            kNoCdEvent, machine_.timestamp - native_last_sync_);

        compare_schedule("start partial");
    }

    void advance(const std::int32_t master_clocks)
    {
        const std::int32_t reference_event
            = dc_beetle_reference_vce_advance(reference_, master_clocks);

        machine_.timestamp += master_clocks;
        native_last_sync_ = machine_.timestamp;
        const std::int32_t native_event = native_->sync(native_last_sync_);

        if (reference_event != native_event) {
            note("the returned event horizons differed");
        }

        compare_schedule("advance");
    }

    // The encoder's own register file. Both arms synchronise to the
    // processor's clock first, the reference on its own and the native
    // through the caller, which is what the installed model will do too.
    void write(const std::uint32_t address, const std::uint8_t value)
    {
        dc_beetle_reference_vce_write(reference_, address, value);

        native_last_sync_ = machine_.timestamp;
        native_->sync(native_last_sync_);
        native_->write(address, value);

        compare_schedule("encoder write");
    }

    std::uint8_t read(const std::uint32_t address)
    {
        const std::uint8_t reference
            = dc_beetle_reference_vce_read(reference_, address);

        native_last_sync_ = machine_.timestamp;
        native_->sync(native_last_sync_);
        const std::uint8_t native = native_->read(address);

        if (reference != native) {
            note("an encoder read returned different bytes");
        }

        compare_schedule("encoder read");
        return reference;
    }

    void write_vdc(const std::uint32_t address, const std::uint8_t value)
    {
        dc_beetle_reference_vce_write_vdc(reference_, address, value);

        native_last_sync_ = machine_.timestamp;
        native_->sync(native_last_sync_);
        native_->write_vdc(address, value);

        compare_schedule("controller write");
    }

    std::uint8_t read_vdc(const std::uint32_t address)
    {
        const std::uint8_t reference
            = dc_beetle_reference_vce_read_vdc(reference_, address);

        native_last_sync_ = machine_.timestamp;
        native_->sync(native_last_sync_);
        const std::uint8_t native = native_->read_vdc(address);

        if (reference != native) {
            note("a controller read returned different bytes");
        }

        compare_schedule("controller read");
        return reference;
    }

    void write_vdc_st(
        const std::uint32_t address,
        const std::uint8_t value)
    {
        dc_beetle_reference_vce_write_vdc_st(reference_, address, value);

        native_last_sync_ = machine_.timestamp;
        native_->sync(native_last_sync_);
        native_->write_vdc_st(address, value);

        compare_schedule("store-immediate write");
    }

    // Select a controller register, then write its low and high halves.
    void write_vdc_register(
        const std::uint8_t index,
        const std::uint16_t value)
    {
        write_vdc(0, index);
        write_vdc(2, static_cast<std::uint8_t>(value & 0xFF));
        write_vdc(3, static_cast<std::uint8_t>(value >> 8));
    }

    // The schedule, the register file, the processor's clock and its
    // interrupt line, every time either arm is touched.
    void compare_schedule(const char* const where)
    {
        dc_beetle_reference_vce_observation observed{};
        dc_beetle_reference_vce_observe(reference_, &observed);

        PceVce::State state{};
        native_->capture_state(state);

        compare_field(observed.cr, state.cr, "control register", where);
        compare_field(
            observed.ws_counter, state.ws_counter, "stall budget", where);
        compare_field(
            observed.ctaddress, state.ctaddress, "palette address", where);
        compare_field(
            observed.clock_divider,
            state.clock_divider,
            "clock divider",
            where);
        compare_field(
            observed.child_event[0],
            state.child_event[0],
            "controller horizon",
            where);
        compare_field(observed.scanline, state.scanline, "scanline", where);
        compare_field(
            observed.pixel_offset,
            state.pixel_offset,
            "pixel offset",
            where);
        compare_field(
            observed.hblank_counter,
            state.hblank_counter,
            "horizontal counter",
            where);
        compare_field(
            observed.vblank_counter,
            state.vblank_counter,
            "vertical counter",
            where);
        compare_field(
            observed.hblank ? 1 : 0,
            state.hblank ? 1 : 0,
            "horizontal blanking",
            where);
        compare_field(
            observed.vblank ? 1 : 0,
            state.vblank ? 1 : 0,
            "vertical blanking",
            where);
        compare_field(
            observed.need_sl_reset ? 1 : 0,
            state.need_sl_reset ? 1 : 0,
            "field restart pending",
            where);
        compare_field(
            observed.frame_done ? 1 : 0,
            native_->frame_done() ? 1 : 0,
            "frame done",
            where);

        compare_field(
            static_cast<std::int64_t>(
                dc_beetle_reference_vce_timestamp(reference_)),
            static_cast<std::int64_t>(machine_.timestamp),
            "processor clock",
            where);

        compare_field(
            dc_beetle_reference_vce_irq_line(reference_) != 0 ? 1 : 0,
            machine_.irq ? 1 : 0,
            "interrupt line",
            where);

        compare_field(
            dc_beetle_reference_vce_calc_next_event(reference_),
            native_->calc_next_event(),
            "next event",
            where);
    }

    // Palette memory and the colour cache it feeds, all of both.
    void compare_palette(const char* const label)
    {
        std::uint32_t differences = 0;

        for (std::uint16_t entry = 0; entry < 512; ++entry) {
            if (dc_beetle_reference_vce_peek_palette(reference_, entry)
                != native_->peek_palette(entry)) {
                ++differences;
            }
        }

        check(differences == 0, label);

        std::uint32_t cache_differences = 0;
        for (std::uint16_t entry = 0; entry < 1024; ++entry) {
            if (dc_beetle_reference_vce_colour_cache(reference_, entry)
                != native_->peek_colour_cache(entry)) {
                ++cache_differences;
            }
        }

        check(cache_differences == 0, "the colour caches agree");
    }

    // Every pixel of the framebuffer and every line width.
    void compare_framebuffer(const char* const label)
    {
        std::size_t differences = 0;
        std::size_t first = 0;

        for (std::size_t index = 0; index < reference_pixels_.size();
             ++index) {
            if (reference_pixels_[index] != native_pixels_[index]) {
                if (differences == 0) {
                    first = index;
                }
                ++differences;
            }
        }

        if (differences != 0) {
            std::printf(
                "        framebuffer differs in %zu pixels, first at "
                "line %zu column %zu: reference $%04X, native $%04X\n",
                differences,
                first / static_cast<std::size_t>(kSurfaceWidth),
                first % static_cast<std::size_t>(kSurfaceWidth),
                reference_pixels_[first],
                native_pixels_[first]);
        }

        check(differences == 0, label);

        std::uint32_t width_differences = 0;
        for (int line = 0; line < kFieldLines; ++line) {
            if (reference_line_widths_[static_cast<std::size_t>(line)]
                != native_line_widths_[static_cast<std::size_t>(line)]) {
                ++width_differences;
            }
        }

        check(width_differences == 0, "the line widths agree");
    }

    // The controller inside each encoder, which is the other half of what a
    // wrong schedule would disturb.
    void compare_controller(const char* const label)
    {
        std::uint32_t differences = 0;

        for (std::uint32_t address = 0; address < 65536; ++address) {
            const std::uint16_t word = static_cast<std::uint16_t>(address);
            if (dc_beetle_reference_vce_peek_vram(reference_, word)
                != native_vdc_->peek_vram(word)) {
                ++differences;
            }
        }

        for (std::uint32_t address = 0; address < 256; ++address) {
            const std::uint8_t index = static_cast<std::uint8_t>(address);
            if (dc_beetle_reference_vce_peek_sat(reference_, index)
                != native_vdc_->peek_sat(index)) {
                ++differences;
            }
        }

        for (unsigned id = 0; id < 19; ++id) {
            if (dc_beetle_reference_vce_vdc_register(reference_, id)
                != native_vdc_->get_register(
                    static_cast<PceVdc::Register>(id))) {
                ++differences;
            }
        }

        check(differences == 0, label);
    }

    // Reports the accumulated differences for a section and clears them, so
    // one divergence does not swamp the sections after it.
    void settle(const char* const label)
    {
        if (first_note_ != nullptr) {
            std::printf("        first note: %s\n", first_note_);
        }
        if (first_bad_field_ != nullptr) {
            std::printf(
                "        %s differs at %s: reference %lld, native %lld\n",
                first_bad_field_,
                first_bad_where_,
                static_cast<long long>(first_bad_reference_),
                static_cast<long long>(first_bad_native_));
        }

        check(note_count_ == 0 && field_mismatches_ == 0, label);

        first_note_ = nullptr;
        note_count_ = 0;
        first_bad_field_ = nullptr;
        first_bad_where_ = "";
        field_mismatches_ = 0;
    }

private:
    void note(const char* const text)
    {
        if (first_note_ == nullptr) {
            first_note_ = text;
        }
        ++note_count_;
    }

    void compare_field(
        const std::int64_t reference,
        const std::int64_t native,
        const char* const field,
        const char* const where)
    {
        if (reference == native) {
            return;
        }

        if (first_bad_field_ == nullptr) {
            first_bad_field_ = field;
            first_bad_where_ = where;
            first_bad_reference_ = reference;
            first_bad_native_ = native;
        }
        ++field_mismatches_;
    }

    static void vdc_irq_hook(void* const context, const bool)
    {
        static_cast<Pair*>(context)->native_->irq_change_check();
    }

    static bool vdc_wait_state_hook(
        void* const context,
        const std::int32_t cycles)
    {
        return static_cast<Pair*>(context)->native_->wait_state(cycles);
    }

    dc_beetle_reference_vce* reference_ = nullptr;

    std::unique_ptr<PceVdc> native_vdc_;
    std::unique_ptr<PceVdcDevice> native_bridge_;
    std::unique_ptr<PceVce> native_;

    NativeMachine machine_{};
    std::int32_t native_last_sync_ = 0;

    std::vector<std::uint16_t> reference_pixels_;
    std::vector<std::uint16_t> native_pixels_;
    std::array<std::int32_t, kFieldLines> reference_line_widths_{};
    std::array<std::int32_t, kFieldLines> native_line_widths_{};
    std::array<std::uint32_t, 512> surface_normal_{};
    std::array<std::uint32_t, 512> surface_grey_{};

    const char* first_note_ = nullptr;
    std::uint64_t note_count_ = 0;

    const char* first_bad_field_ = nullptr;
    const char* first_bad_where_ = "";
    std::int64_t first_bad_reference_ = 0;
    std::int64_t first_bad_native_ = 0;
    std::uint64_t field_mismatches_ = 0;
};

// Writes one 16-bit word into video memory through the controller's data
// port, reached through the encoder.
void poke(Pair& pair, const std::uint16_t address, const std::uint16_t value)
{
    pair.write_vdc_register(kMAWR, address);
    pair.write_vdc(0, kVRR);
    pair.write_vdc(2, static_cast<std::uint8_t>(value & 0xFF));
    pair.write_vdc(3, static_cast<std::uint8_t>(value >> 8));
}

// A 262-line display with a 224-line visible field, which is what the
// console's own initialisation programs.
void program_display(Pair& pair)
{
    pair.write_vdc_register(kHSR, 0x0202);
    pair.write_vdc_register(kHDR, 0x041F);
    pair.write_vdc_register(kVPR, 0x0F02);
    pair.write_vdc_register(kVDW, 224 - 1);
    pair.write_vdc_register(kVCR, 19);
    pair.write_vdc_register(kMWR, 0x0000);
}

// Sixteen words per character cell: eight rows of bitplanes 0 and 1, then
// eight of planes 2 and 3.
void write_tile(
    Pair& pair,
    const std::uint16_t tile,
    const std::uint16_t plane01,
    const std::uint16_t plane23)
{
    const std::uint16_t base = static_cast<std::uint16_t>(tile * 16);
    for (std::uint16_t row = 0; row < 8; ++row) {
        poke(pair, static_cast<std::uint16_t>(base + row), plane01);
        poke(pair, static_cast<std::uint16_t>(base + 8 + row), plane23);
    }
}

// A tile map that varies both character and palette. A uniform map would
// hide exactly the mistakes worth finding, and so would one whose period is
// a power of two, so the entries come from a hash.
void fill_tile_map(Pair& pair, const std::uint16_t base_tile, const int count)
{
    pair.write_vdc_register(kMAWR, 0x0000);
    pair.write_vdc(0, kVRR);
    for (int index = 0; index < count; ++index) {
        const std::uint32_t hash
            = static_cast<std::uint32_t>(index + 1) * 0x9E3779B1U;
        const std::uint16_t tile
            = static_cast<std::uint16_t>(base_tile + ((hash >> 13) & 3));
        const std::uint16_t palette
            = static_cast<std::uint16_t>((hash >> 20) & 0xF);
        const std::uint16_t entry
            = static_cast<std::uint16_t>((palette << 12) | tile);

        pair.write_vdc(2, static_cast<std::uint8_t>(entry & 0xFF));
        pair.write_vdc(3, static_cast<std::uint8_t>(entry >> 8));
    }
}

// Fills palette memory through the encoder's own port, a nine-bit entry at
// a time, from a hash for the same reason the tile map uses one.
void fill_palette(Pair& pair)
{
    pair.write(2, 0x00);
    pair.write(3, 0x00);

    for (int entry = 0; entry < 512; ++entry) {
        const std::uint32_t hash
            = static_cast<std::uint32_t>(entry + 1) * 0x85EBCA6BU;
        pair.write(4, static_cast<std::uint8_t>(hash >> 11));
        pair.write(5, static_cast<std::uint8_t>((hash >> 19) & 1));
    }
}

}  // namespace

int main()
{
    std::printf("VCE device: native against the emulator's own\n");

    auto pair = std::make_unique<Pair>();
    if (!pair->valid()) {
        std::printf("  FAIL  the reference encoder refused to be created\n");
        return 1;
    }

    section("Reset");
    pair->compare_schedule("reset");
    pair->settle("both encoders reset to the same state");
    pair->compare_palette("the power-on palettes agree");

    section("The palette port");
    fill_palette(*pair);
    pair->settle("512 palette writes agree step for step");
    pair->compare_palette("palette memory agrees after 512 writes");

    // Reading the high half advances the address; reading the low half does
    // not. A model that advanced on both would pass a test that only read
    // pairs, so both are read alone as well.
    pair->write(2, 0x10);
    pair->write(3, 0x01);
    const std::uint8_t low = pair->read(4);
    const std::uint8_t high = pair->read(5);
    check(
        (static_cast<std::uint16_t>(high & 1) << 8 | low)
            == pair->native().peek_palette(0x110),
        "a palette read returns the entry that was written");
    check((high & 0xFE) == 0xFE, "the unused high bits read back as bus");
    pair->read(4);
    pair->read(4);
    pair->read(5);
    pair->settle("palette reads agree, and so does the address they leave");

    // Nothing decodes at 1, 6 or 7.
    check(pair->read(1) == 0xFF, "an undecoded encoder read is all ones");
    check(pair->read(6) == 0xFF, "and so is the one above the palette port");
    pair->write(1, 0x5A);
    pair->write(6, 0xA5);
    pair->settle("writes to undecoded addresses change nothing");

    section("A programmed field");
    program_display(*pair);
    write_tile(*pair, 0x100, 0xFFFF, 0x0F0F);
    write_tile(*pair, 0x101, 0x5A5A, 0x0000);
    write_tile(*pair, 0x102, 0x0FF0, 0xFF00);
    write_tile(*pair, 0x103, 0x1248, 0x8421);
    fill_tile_map(*pair, 0x100, 64 * 32);
    pair->write_vdc_register(
        kCR, kControlBackgroundEnabled | kControlSpritesEnabled);
    pair->settle("programming the controller through the encoder agrees");

    pair->start_frame(false, 0, 0, 242);
    pair->start_partial();
    pair->advance(kFieldClocks);
    pair->settle("a whole field runs in step");
    pair->compare_framebuffer("the two fields are the same picture");
    pair->compare_controller("the two controllers agree after a field");
    pair->compare_palette("palette memory survives a field");

    check(
        pair->native().coverage().pixels > 0,
        "the field actually drew something");
    check(
        pair->native().coverage().scanlines > 200,
        "and advanced a field's worth of scanlines");

    section("A second field, in smaller steps");
    pair->start_frame(false, 0, 0, 242);
    pair->start_partial();
    // Chunks that are not a divisor of anything in the schedule, so the
    // boundaries fall inside blanking periods and inside chunks rather than
    // on them.
    for (std::int32_t spent = 0; spent < kFieldClocks; spent += 1237) {
        pair->advance(1237);
    }
    pair->settle("a field split into 290 uneven steps runs in step");
    pair->compare_framebuffer("and draws the same picture");

    section("Scrolling, so the picture is not the same twice");
    pair->write_vdc_register(kBXR, 37);
    pair->write_vdc_register(kBYR, 11);
    pair->start_frame(false, 0, 0, 242);
    pair->start_partial();
    pair->advance(kFieldClocks);
    pair->settle("a scrolled field runs in step");
    pair->compare_framebuffer("and draws the scrolled picture the same way");

    section("The dot clock");
    // The game runs at 5 MHz and nothing else. All four rates are driven
    // here, including the mid-field change that zeroes the clock divider.
    for (std::uint8_t rate = 0; rate < 4; ++rate) {
        pair->start_frame(false, 0, 0, 242);
        pair->start_partial();
        pair->advance(kFieldClocks / 3);
        pair->write(0, rate);
        pair->advance(kFieldClocks / 3);
        pair->write(0, static_cast<std::uint8_t>((rate + 1) & 3));
        pair->advance(kFieldClocks / 3);
    }
    pair->write(0, 0);
    pair->settle("all four dot rates, changed mid-field, run in step");
    pair->compare_framebuffer("and draw the same multi-rate picture");

    section("Black and white");
    pair->write(0, 0x80);
    pair->settle("setting the monochrome bit rebuilds the cache the same");
    pair->compare_palette("palette memory is unchanged by the rebuild");
    pair->start_frame(false, 0, 0, 242);
    pair->start_partial();
    pair->advance(kFieldClocks);
    pair->settle("a monochrome field runs in step");
    pair->compare_framebuffer("and draws in the greyscale expansion");
    pair->write(0, 0x00);
    pair->settle("and clearing the bit rebuilds it back");

    section("A 263-line field");
    pair->write(0, 0x04);
    pair->start_frame(false, 0, 0, 242);
    pair->start_partial();
    pair->advance(kFieldClocks + 4096);
    pair->settle("the longer field runs in step");
    pair->compare_framebuffer("and draws the same picture");
    pair->write(0, 0x00);
    pair->settle("and the field length goes back");

    section("The upscaled pixel path");
    pair->start_frame(false, 2, 0, 242);
    pair->start_partial();
    pair->advance(kFieldClocks);
    pair->settle("an upscaled field runs in step");
    pair->compare_framebuffer("and writes each pixel its ratio's worth");

    section("A narrowed visible range");
    // The lines outside it neither draw nor reach the controller's
    // renderers, which is a different path through the same schedule.
    pair->start_frame(false, 0, 40, 180);
    pair->start_partial();
    pair->advance(kFieldClocks);
    pair->settle("a field with 141 visible lines runs in step");
    pair->compare_framebuffer("and leaves the lines outside it alone");
    check(
        pair->native().coverage().off_screen_runs > 0,
        "lines outside the visible range were skipped");

    section("Horizontal overscan");
    // A debugging control rather than something the console can do, and one
    // that changes both the width the frontend is told and the column the
    // write cursor starts in.
    pair->set_show_horizontal_overscan(true);
    pair->start_frame(false, 0, 0, 242);
    pair->start_partial();
    pair->advance(kFieldClocks);
    pair->settle("a field with the overscan shown runs in step");
    pair->compare_framebuffer("and puts the picture in the same columns");
    pair->set_show_horizontal_overscan(false);

    section("A stalled port access");
    // A block transfer inside video memory, then a port write while it
    // runs. The controller stalls, the encoder charges the processor, and
    // the two clocks part company by exactly the same amount or the arms
    // disagree about what time it is.
    pair->write_vdc_register(kSOUR, 0x2000);
    pair->write_vdc_register(kDESR, 0x3000);
    pair->write_vdc_register(kDCR, 0x0000);
    pair->write_vdc_register(kLENR, 0x0FFF);

    const std::uint64_t stalls_before = pair->machine().stolen_calls;
    for (int index = 0; index < 16; ++index) {
        poke(
            *pair,
            static_cast<std::uint16_t>(0x4000 + index),
            static_cast<std::uint16_t>(0xC000 + index));
        pair->advance(64);
    }
    pair->settle("port accesses during a transfer stall in step");
    check(
        pair->machine().stolen_calls > stalls_before,
        "the transfer actually stalled the processor");
    pair->compare_controller("the transfer moved the same words");

    section("The sprite attribute table transfer");
    pair->write_vdc_register(kDVSSR, 0x7000);
    for (std::uint16_t index = 0; index < 256; ++index) {
        poke(
            *pair,
            static_cast<std::uint16_t>(0x7000 + index),
            static_cast<std::uint16_t>(index * 0x0101));
    }
    pair->start_frame(false, 0, 0, 242);
    pair->start_partial();
    pair->advance(kFieldClocks);
    pair->settle("the table transfer runs in step");
    pair->compare_controller("and lands the same words in the table");

    section("An interrupt");
    pair->write_vdc_register(
        kCR,
        kControlBackgroundEnabled | kControlSpritesEnabled
            | kControlVerticalBlankIRQ);
    pair->start_frame(false, 0, 0, 242);
    pair->start_partial();
    pair->advance(kFieldClocks);
    pair->settle("a field that raises a vertical-blank interrupt agrees");
    check(
        pair->native().coverage().irq_raises > 0,
        "the interrupt line was actually raised");

    // Acknowledging clears it, and the encoder has to re-derive the level
    // rather than remember the edge.
    pair->read_vdc(0);
    pair->settle("acknowledging the interrupt lowers the line in step");

    section("Store-immediate writes");
    pair->write_vdc_st(0, kMAWR);
    pair->write_vdc_st(2, 0x34);
    pair->write_vdc_st(3, 0x12);
    pair->settle("the processor's store-immediate path reaches the same "
                 "register");

    section("The savestate surface");
    {
        PceVce::State saved{};
        pair->native().capture_state(saved);

        // Perturb everything a state carries, then put it back. A model
        // whose apply missed a field would fail the very next comparison.
        pair->native().write(0, 0x83);
        pair->native().write(2, 0x77);
        pair->native().write(4, 0x5A);
        pair->native().apply_state(saved);

        pair->compare_schedule("after a state round trip");
        pair->settle("a captured state restores the encoder exactly");
        pair->compare_palette("and restores palette memory with it");

        // The values a savestate may hold but the device may not.
        PceVce::State broken = saved;
        broken.ctaddress = 0xFFFF;
        broken.scanline = -5;
        broken.hblank_counter = 99999;
        broken.vblank_counter = -1;
        broken.child_event[0] = 0;
        broken.clock_divider = 999;

        auto vce_alone = std::make_unique<PceVce>();
        auto vdc_alone = std::make_unique<PceVdc>();
        PceVdcDevice bridge_alone(*vdc_alone);
        vce_alone->set_vdc(&bridge_alone);
        vce_alone->reset();
        vce_alone->apply_state(broken);

        PceVce::State repaired{};
        vce_alone->capture_state(repaired);

        check(repaired.ctaddress == 0x1FF, "a wild palette address is masked");
        check(repaired.scanline == 0, "a negative scanline is floored");
        check(
            repaired.hblank_counter == 1365,
            "an impossible horizontal counter is clamped");
        check(
            repaired.vblank_counter == 1,
            "an impossible vertical counter is clamped");
        check(
            repaired.child_event[0] == 1,
            "a zero controller horizon is lifted to one");
        check(
            repaired.clock_divider < 4,
            "a clock divider beyond the ratio is reduced");
    }

    section("The line-width repair");
    {
        // The one piece of `VCE::RunPartial` this shim cannot ask the
        // emulator for, because reaching it means running a processor. It
        // is checked against what the emulator's own loop states instead:
        // every line that was never drawn takes the width of the first line
        // that was.
        auto vce_alone = std::make_unique<PceVce>();
        auto vdc_alone = std::make_unique<PceVdc>();
        PceVdcDevice bridge_alone(*vdc_alone);
        std::vector<std::uint16_t> pixels(
            static_cast<std::size_t>(kSurfaceWidth * kSurfaceHeight), 0);
        std::array<std::int32_t, kFieldLines> widths{};

        vce_alone->set_vdc(&bridge_alone);
        vce_alone->reset();
        vce_alone->start_frame(
            pixels.data(), kSurfaceWidth, widths.data(), false, false, 0,
            242);

        widths[100] = 341;
        vce_alone->finish_partial();

        bool all_filled = true;
        for (int line = 0; line < kFieldLines; ++line) {
            if (widths[static_cast<std::size_t>(line)] != 341) {
                all_filled = false;
            }
        }

        check(all_filled, "every undrawn line takes the first drawn width");

        std::array<std::int32_t, kFieldLines> empty{};
        vce_alone->start_frame(
            pixels.data(), kSurfaceWidth, empty.data(), false, false, 0,
            242);
        vce_alone->finish_partial();
        check(
            empty[0] == 256 && empty[262] == 256,
            "a frame that drew nothing falls back to 256");
    }

    section("What the route asked for");
    {
        const PceVce::Coverage& coverage = pair->native().coverage();
        std::printf(
            "        frames %llu, partials %llu, completed %llu\n"
            "        scanlines %llu, restarts %llu, vertical blanks %llu\n"
            "        chunks %llu (%llu floored), dot clocks %llu\n"
            "        pixels %llu, off-screen runs %llu\n"
            "        rates: %llu %llu %llu %llu\n"
            "        palette: %llu written, %llu read\n"
            "        stalls: %llu requested, %llu refused, %llu cycles\n"
            "        interrupts: %llu raised, %llu lowered\n"
            "        exits %llu, compact disc events %llu\n",
            static_cast<unsigned long long>(coverage.frames),
            static_cast<unsigned long long>(coverage.partials),
            static_cast<unsigned long long>(coverage.frames_completed),
            static_cast<unsigned long long>(coverage.scanlines),
            static_cast<unsigned long long>(coverage.field_restarts),
            static_cast<unsigned long long>(coverage.vertical_blanks),
            static_cast<unsigned long long>(coverage.chunks),
            static_cast<unsigned long long>(coverage.floored_chunks),
            static_cast<unsigned long long>(coverage.dot_clocks),
            static_cast<unsigned long long>(coverage.pixels),
            static_cast<unsigned long long>(coverage.off_screen_runs),
            static_cast<unsigned long long>(coverage.lines_at_rate[0]),
            static_cast<unsigned long long>(coverage.lines_at_rate[1]),
            static_cast<unsigned long long>(coverage.lines_at_rate[2]),
            static_cast<unsigned long long>(coverage.lines_at_rate[3]),
            static_cast<unsigned long long>(coverage.palette_writes),
            static_cast<unsigned long long>(coverage.palette_reads),
            static_cast<unsigned long long>(coverage.wait_state_requests),
            static_cast<unsigned long long>(coverage.wait_state_refusals),
            static_cast<unsigned long long>(coverage.stolen_cycles),
            static_cast<unsigned long long>(coverage.irq_raises),
            static_cast<unsigned long long>(coverage.irq_lowers),
            static_cast<unsigned long long>(coverage.cpu_exits),
            static_cast<unsigned long long>(coverage.cd_events));

        check(
            coverage.cd_events == 0,
            "the compact disc event never came due");
        check(
            coverage.lines_at_rate[0] > 0 && coverage.lines_at_rate[1] > 0
                && coverage.lines_at_rate[2] > 0
                && coverage.lines_at_rate[3] > 0,
            "every dot rate drew at least one line");
        check(
            coverage.monochrome_changes >= 2,
            "the monochrome bit was driven both ways");
        check(
            coverage.line_count_changes >= 2,
            "and so was the 263-line field select");
    }

    std::printf("\n%s\n", g_failed ? "FAILED" : "PASSED");
    return g_failed ? 1 : 0;
}
