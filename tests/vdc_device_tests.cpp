// SPDX-License-Identifier: GPL-3.0-or-later

// The native video display controller against the emulator's own device,
// driven side by side on one schedule.
//
// The reference-device slice established that `VDC` can be lifted out of the
// emulator and that what it needs from the machine is a clock schedule. This
// is the other half: `devils_crush::video::PceVdc` fed exactly the same port
// accesses and the same clocks, with every pixel, every register, every word
// of video memory and every interrupt compared as it goes.
//
// Nothing here is sampled from outside -- no ROM, no input, no wall clock --
// so both devices are pure functions of this file, and any difference is a
// difference between the two models rather than a difference in stimulus.
// That is the point of proving the device before installing it: the route
// census can only prove what the game asks for, and the game does not ask
// for sprite overflow, 16-colour sprites, or a backwards block transfer.
//
// Three things are deliberately not compared, because the reference shim
// fixes them at construction and exposes no setter: a video memory size
// below 64K, the layer enable mask at anything but "both on", and the
// unlimited-sprites option. All three are debugging controls rather than
// things the console can do, and none is on the path to eliminating the
// emulator -- but the native model implements them, so they are untested
// code until someone widens the shim.

#include "dc_beetle_reference_vdc.h"
#include "devils_crush/video/pce_vdc.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace {

using devils_crush::video::PceVdc;

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

// VDC register select indices.
constexpr std::uint8_t kMAWR = 0x00;
constexpr std::uint8_t kMARR = 0x01;
constexpr std::uint8_t kVRR = 0x02;
constexpr std::uint8_t kCR = 0x05;
constexpr std::uint8_t kRCR = 0x06;
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

// Control register bits.
constexpr std::uint16_t kControlSpriteZeroIRQ = 0x0001;
constexpr std::uint16_t kControlOverflowIRQ = 0x0002;
constexpr std::uint16_t kControlRasterCompareIRQ = 0x0004;
constexpr std::uint16_t kControlVerticalBlankIRQ = 0x0008;
constexpr std::uint16_t kControlSpritesEnabled = 0x0040;
constexpr std::uint16_t kControlBackgroundEnabled = 0x0080;

// One 262-line field, in master clocks, as `VCE` counts it.
constexpr std::int32_t kFieldClocks = 357630;

// `VCE::vce_ratios[0]`, the 5 MHz dot clock the game runs in.
constexpr std::int32_t kDotClockRatio = 4;

// The emulator hands the device a 2048-entry line buffer.
constexpr std::size_t kPixelBufferSize = 2048;

// `VCE::WS_Hook`'s per-frame allowance of stolen CPU cycles. The refusal
// that lands when it runs out is the only thing that ends a stall, so both
// arms have to charge it the same way or one of them hangs.
constexpr std::int32_t kWaitStateBudget = 455 * 64;

// Interrupt hook traffic, recorded for the native device and compared
// against the reference shim's own counters. Split by argument, because the
// device passes true at its six raise sites and false at its two
// acknowledge sites, and a model that only counted calls would not notice
// the difference.
struct IrqLog {
    std::uint64_t calls = 0;
    std::uint64_t raises = 0;

    static void hook(void* const context, const bool raised)
    {
        auto* const self = static_cast<IrqLog*>(context);
        ++self->calls;
        if (raised) {
            ++self->raises;
        }
    }
};

// `VCE::WS_Hook`, reduced to the part that decides. The emulator charges the
// CPU whatever the stall cost; here only the bookkeeping matters, so each
// call charges one cycle, which is what the reference shim's default policy
// also does.
struct WaitStateLog {
    std::int32_t counter = 0;
    std::uint64_t calls = 0;

    static bool hook(void* const context, std::int32_t)
    {
        auto* const self = static_cast<WaitStateLog*>(context);
        ++self->calls;

        std::int32_t to_steal = 1;
        bool granted = true;

        if (to_steal + self->counter > kWaitStateBudget) {
            to_steal = kWaitStateBudget - self->counter;
            if (to_steal < 0) {
                to_steal = 0;
            }
            granted = false;
        }

        self->counter += to_steal;
        return granted;
    }
};

// Both devices, driven by one copy of `VCE::SyncSub` and compared at every
// step. The schedule follows the reference device's returned event horizon;
// the native device's is compared against it rather than used, so that a
// disagreement about when the next event falls is reported once instead of
// cascading into every pixel after it.
class Pair {
public:
    Pair()
        : reference_(dc_beetle_reference_vdc_create(65536))
        , native_(std::make_unique<PceVdc>())
        , reference_pixels_(kPixelBufferSize, 0)
        , native_pixels_(kPixelBufferSize, 0)
    {
        native_->set_vram_size(65536);
        native_->set_unlimited_sprites(false);
        native_->set_layer_enable_mask(0x3);
        native_->set_irq_hook(&IrqLog::hook, &irq_);
        native_->set_wait_state_hook(&WaitStateLog::hook, &wait_states_);

        child_event_ = dc_beetle_reference_vdc_reset(reference_);
        native_event_ = native_->reset();

        if (child_event_ != native_event_) {
            note("reset returned a different event horizon");
        }
    }

    ~Pair() { dc_beetle_reference_vdc_destroy(reference_); }

    Pair(const Pair&) = delete;
    Pair& operator=(const Pair&) = delete;

    [[nodiscard]] bool valid() const { return reference_ != nullptr; }

    void write(const std::uint32_t address, const std::uint8_t value)
    {
        dc_beetle_reference_vdc_write(
            reference_, address, value, &child_event_);
        native_->write(address, value);
        compare_side_effects("port write");
    }

    std::uint8_t read(const std::uint32_t address)
    {
        const std::uint8_t reference = dc_beetle_reference_vdc_read(
            reference_, address, &child_event_, 0);
        const std::uint8_t native = native_->read(address, false);

        if (reference != native) {
            note("port read returned different bytes");
        }
        compare_side_effects("port read");
        return reference;
    }

    // Select a register, then write its low and high halves. The high write
    // is the one that commits.
    void write_register(const std::uint8_t index, const std::uint16_t value)
    {
        write(0, index);
        write(2, static_cast<std::uint8_t>(value & 0xFF));
        write(3, static_cast<std::uint8_t>(value >> 8));
    }

    void run(std::int32_t clocks)
    {
        while (clocks > 0) {
            std::int32_t chunk = clocks;

            if (chunk > hblank_counter_) {
                chunk = hblank_counter_;
            }
            if (chunk > vblank_counter_) {
                chunk = vblank_counter_;
            }

            const std::int32_t horizon =
                child_event_ * kDotClockRatio - clock_divider_;
            if (chunk > horizon) {
                chunk = horizon;
            }
            if (chunk <= 0) {
                chunk = 1;
            }

            clock_divider_ += chunk;
            const std::int32_t divided = clock_divider_ / kDotClockRatio;
            clock_divider_ -= divided * kDotClockRatio;
            child_event_ -= divided;
            native_event_ -= divided;

            if (divided > 0) {
                child_event_ = dc_beetle_reference_vdc_run(
                    reference_, divided, reference_pixels_.data(), 0);
                native_event_ =
                    native_->run(divided, native_pixels_.data(), false);
                compare_pixels(divided);
                compare_event("run");
                compare_side_effects("run");
            }

            clocks -= chunk;

            hblank_counter_ -= chunk;
            if (hblank_counter_ <= 0) {
                hblank_ = !hblank_;
                if (!hblank_) {
                    ++scanlines_;
                }
                hblank_counter_ = hblank_ ? 237 : 1128;
                child_event_ = dc_beetle_reference_vdc_hsync(
                    reference_, hblank_ ? 1 : 0);
                native_event_ = native_->hsync(hblank_);
                compare_event("hsync");
            }

            vblank_counter_ -= chunk;
            if (vblank_counter_ <= 0) {
                vblank_ = !vblank_;
                vblank_counter_ = vblank_ ? 4095 : (kFieldClocks - 4095);
                child_event_ = dc_beetle_reference_vdc_vsync(
                    reference_, vblank_ ? 1 : 0);
                native_event_ = native_->vsync(vblank_);
                compare_event("vsync");

                // `VCE::RunPartial` clears the stolen-cycle allowance once a
                // frame. Both arms have to clear it at the same moment.
                if (vblank_) {
                    dc_beetle_reference_vdc_reset_wait_states(reference_);
                    wait_states_.counter = 0;
                }
            }
        }
    }

    // Run until either device raises an interrupt, or the budget runs out.
    // Returns the clocks consumed, and reports if the two disagreed about
    // which clock it was.
    std::int32_t run_until_irq(const std::int32_t budget)
    {
        std::int32_t spent = 0;

        while (spent < budget) {
            const bool reference_irq =
                dc_beetle_reference_vdc_peek_irq(reference_) != 0;
            const bool native_irq = native_->peek_irq();

            if (reference_irq != native_irq) {
                note("interrupt line differed");
                break;
            }
            if (reference_irq) {
                break;
            }
            run(1);
            ++spent;
        }

        return spent;
    }

    // Every register, the status byte, the interrupt line, and the hook
    // traffic since the last comparison.
    void compare_side_effects(const char* const where)
    {
        for (unsigned id = 0; id < 19; ++id) {
            const std::uint32_t reference =
                dc_beetle_reference_vdc_get_register(reference_, id);
            const std::uint32_t native = native_->get_register(
                static_cast<PceVdc::Register>(id));

            if (reference != native) {
                if (first_bad_register_ < 0) {
                    first_bad_register_ = static_cast<int>(id);
                    first_bad_register_reference_ = reference;
                    first_bad_register_native_ = native;
                    first_bad_register_where_ = where;
                }
                ++register_mismatches_;
            }
        }

        if ((dc_beetle_reference_vdc_peek_irq(reference_) != 0) !=
            native_->peek_irq()) {
            note("interrupt line differed");
        }

        if (dc_beetle_reference_vdc_irq_hook_calls(reference_) !=
                irq_.calls ||
            dc_beetle_reference_vdc_irq_hook_raises(reference_) !=
                irq_.raises) {
            note("interrupt hook traffic differed");
        }

        if (dc_beetle_reference_vdc_wait_state_hook_calls(reference_) !=
            wait_states_.calls) {
            note("wait-state hook traffic differed");
        }

        if (dc_beetle_reference_vdc_wait_state_counter(reference_) !=
            wait_states_.counter) {
            note("wait-state budget differed");
        }
    }

    // Every word of video memory and of the sprite attribute table. Cheap
    // enough to run between sections, not between clocks.
    void compare_memory(const char* const label)
    {
        std::uint32_t differences = 0;
        std::uint32_t first = 0;

        for (std::uint32_t address = 0; address < 65536; ++address) {
            const std::uint16_t word =
                static_cast<std::uint16_t>(address);
            const std::uint16_t reference =
                dc_beetle_reference_vdc_peek_vram(reference_, word);
            const std::uint16_t native = native_->peek_vram(word);
            if (reference != native) {
                if (differences == 0) {
                    first = address;
                }
                ++differences;
            }
        }

        if (differences != 0) {
            std::printf(
                "        video memory differs in %u words, first at $%04X\n",
                differences,
                first);
        }
        check(differences == 0, label);

        std::uint32_t sat_differences = 0;
        for (std::uint32_t address = 0; address < 256; ++address) {
            const std::uint8_t index = static_cast<std::uint8_t>(address);
            if (dc_beetle_reference_vdc_peek_sat(reference_, index) !=
                native_->peek_sat(index)) {
                ++sat_differences;
            }
        }
        check(sat_differences == 0, "the sprite attribute tables agree");
    }

    // Reports the accumulated differences for a section and clears them, so
    // one divergence does not swamp the sections after it.
    void settle(const char* const label)
    {
        if (first_note_ != nullptr) {
            std::printf("        first note: %s\n", first_note_);
        }
        if (first_bad_register_ >= 0) {
            std::printf(
                "        register %d differs at %s: "
                "reference $%X, native $%X\n",
                first_bad_register_,
                first_bad_register_where_,
                first_bad_register_reference_,
                first_bad_register_native_);
        }
        if (first_bad_pixel_ >= 0) {
            std::printf(
                "        pixel %d of a chunk differs: "
                "reference $%04X, native $%04X\n",
                first_bad_pixel_,
                first_bad_pixel_reference_,
                first_bad_pixel_native_);
        }

        check(
            note_count_ == 0 && register_mismatches_ == 0 &&
                pixel_mismatches_ == 0 && event_mismatches_ == 0,
            label);

        first_note_ = nullptr;
        note_count_ = 0;
        register_mismatches_ = 0;
        first_bad_register_ = -1;
        pixel_mismatches_ = 0;
        first_bad_pixel_ = -1;
        event_mismatches_ = 0;
    }

    [[nodiscard]] std::uint64_t scanlines() const { return scanlines_; }
    [[nodiscard]] std::uint64_t emitted_pixels() const
    {
        return emitted_pixels_;
    }
    [[nodiscard]] std::uint64_t visible_pixels() const
    {
        return visible_pixels_;
    }
    [[nodiscard]] const PceVdc::Coverage& coverage() const
    {
        return native_->coverage();
    }
    [[nodiscard]] const IrqLog& irq() const { return irq_; }

private:
    void note(const char* const what)
    {
        if (first_note_ == nullptr) {
            first_note_ = what;
        }
        ++note_count_;
    }

    void compare_event(const char* const where)
    {
        if (child_event_ != native_event_) {
            note(where);
            ++event_mismatches_;
            // Keep the schedules together so the comparison stays about the
            // devices rather than about drift.
            native_event_ = child_event_;
        }
    }

    void compare_pixels(const std::int32_t emitted)
    {
        for (std::int32_t index = 0; index < emitted; ++index) {
            const std::size_t at = static_cast<std::size_t>(index);
            const std::uint16_t reference = reference_pixels_[at];
            const std::uint16_t native = native_pixels_[at];

            if (reference != native) {
                if (first_bad_pixel_ < 0) {
                    first_bad_pixel_ = index;
                    first_bad_pixel_reference_ = reference;
                    first_bad_pixel_native_ = native;
                }
                ++pixel_mismatches_;
            }

            if ((reference & 0x0F) != 0) {
                ++visible_pixels_;
            }
        }
        emitted_pixels_ += static_cast<std::uint64_t>(emitted);
    }

    dc_beetle_reference_vdc* reference_ = nullptr;
    std::unique_ptr<PceVdc> native_;

    IrqLog irq_{};
    WaitStateLog wait_states_{};

    std::vector<std::uint16_t> reference_pixels_;
    std::vector<std::uint16_t> native_pixels_;

    std::int32_t child_event_ = 1;
    std::int32_t native_event_ = 1;

    // `VCE::Reset` leaves both blanking counters part-way in.
    std::int32_t hblank_counter_ = 237;
    std::int32_t vblank_counter_ = 4095 + 30;
    std::int32_t clock_divider_ = 0;
    bool hblank_ = true;
    bool vblank_ = true;

    std::uint64_t scanlines_ = 0;
    std::uint64_t emitted_pixels_ = 0;
    std::uint64_t visible_pixels_ = 0;

    const char* first_note_ = nullptr;
    std::uint64_t note_count_ = 0;

    std::uint64_t register_mismatches_ = 0;
    int first_bad_register_ = -1;
    std::uint32_t first_bad_register_reference_ = 0;
    std::uint32_t first_bad_register_native_ = 0;
    const char* first_bad_register_where_ = "";

    std::uint64_t pixel_mismatches_ = 0;
    int first_bad_pixel_ = -1;
    std::uint16_t first_bad_pixel_reference_ = 0;
    std::uint16_t first_bad_pixel_native_ = 0;

    std::uint64_t event_mismatches_ = 0;
};

// Writes one 16-bit word through the data port at a chosen address. Leaves
// the write address register pointing one increment past it.
void poke(Pair& pair, const std::uint16_t address, const std::uint16_t value)
{
    pair.write_register(kMAWR, address);
    pair.write(0, kVRR);
    pair.write(2, static_cast<std::uint8_t>(value & 0xFF));
    pair.write(3, static_cast<std::uint8_t>(value >> 8));
}

// A character cell is sixteen words: eight rows of bitplanes 0 and 1, then
// eight rows of bitplanes 2 and 3. Writing all sixteen is deliberate --
// every row of the cache has to be built by a port write, because the
// emulator's device only rebuilds row zero on reset and leaves the rest of
// an untouched character indeterminate.
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

// A 262-line display: 3 lines of vertical sync, 17 of front porch, 224 of
// display, 19 of back porch, matching the driver's field. The horizontal
// numbers are the ones the console's own initialisation uses.
void program_display(Pair& pair)
{
    pair.write_register(kHSR, 0x0202);
    pair.write_register(kHDR, 0x041F);
    pair.write_register(kVPR, 0x0F02);
    pair.write_register(kVDW, 224 - 1);
    pair.write_register(kVCR, 19);
    pair.write_register(kMWR, 0x0000);
}

// Fills a tile map at video memory zero, varying both the character number
// and the palette. A uniform map hides exactly the mistakes worth finding:
// a renderer that fetched the wrong map entry would draw the identical
// picture. So would a map with a short period, which is the trap here --
// the rows a wrong height mask picks are a power of two apart, so anything
// that repeats on a power of two is no better than uniform. Hence a hash
// rather than an arithmetic pattern.
void fill_tile_map(Pair& pair, const std::uint16_t base_tile, const int count)
{
    pair.write_register(kMAWR, 0x0000);
    pair.write(0, kVRR);
    for (int index = 0; index < count; ++index) {
        const std::uint32_t hash =
            static_cast<std::uint32_t>(index + 1) * 0x9E3779B1U;
        const std::uint16_t tile = static_cast<std::uint16_t>(
            base_tile + ((hash >> 13) & 3));
        const std::uint16_t palette =
            static_cast<std::uint16_t>((hash >> 20) & 0xF);
        const std::uint16_t entry =
            static_cast<std::uint16_t>((palette << 12) | tile);

        pair.write(2, static_cast<std::uint8_t>(entry & 0xFF));
        pair.write(3, static_cast<std::uint8_t>(entry >> 8));
    }
}

// Programs a tile map shape and a scroll, and runs the two fields it takes
// for both to reach the picture.
//
// The memory width register is latched at vertical sync, and in this
// driver's field the vertical sync falls near the end of a `run` rather than
// its start -- so a shape written before a field is not drawn with until the
// field after it. Writing the shape and asserting on the next field, which
// is the obvious way to do this, quietly tests the previous shape twice and
// the new one never.
void use_tile_map(
    Pair& pair,
    const std::uint16_t memory_width,
    const std::uint16_t scroll_x,
    const std::uint16_t scroll_y)
{
    pair.write_register(kMWR, memory_width);
    pair.run(kFieldClocks);

    pair.write_register(kBXR, scroll_x);
    pair.write_register(kBYR, scroll_y);
    pair.run(kFieldClocks);
}

// One sprite attribute table entry, four words, written into video memory
// where the sprite attribute table transfer will find it.
void write_sprite(
    Pair& pair,
    const std::uint16_t table,
    const std::uint16_t index,
    const std::uint16_t x,
    const std::uint16_t y,
    const std::uint16_t pattern,
    const std::uint16_t flags)
{
    const std::uint16_t base =
        static_cast<std::uint16_t>(table + index * 4);

    poke(pair, base, static_cast<std::uint16_t>(y + 0x40));
    poke(pair, static_cast<std::uint16_t>(base + 1),
        static_cast<std::uint16_t>(x + 0x20));
    poke(pair, static_cast<std::uint16_t>(base + 2),
        static_cast<std::uint16_t>(pattern << 1));
    poke(pair, static_cast<std::uint16_t>(base + 3), flags);
}

}  // namespace

int main()
{
    std::printf("VDC device: native against the emulator's own\n");

    auto pair = std::make_unique<Pair>();
    if (!pair->valid()) {
        std::printf("  FAIL  the reference device refused to be created\n");
        return 1;
    }

    section("Reset");
    pair->compare_side_effects("reset");
    pair->settle("both devices reset to the same state");
    pair->compare_memory("video memory starts identical");

    section("The data port");
    poke(*pair, 0x0040, 0x1234);
    pair->settle("a data-port write reaches video memory the same way");

    // The read port returns the previously buffered word, so the first read
    // after pointing the read address register is stale by one -- a
    // property worth pinning, because a model that fetched eagerly would
    // pass every other test here.
    pair->write_register(kMARR, 0x0040);
    pair->write(0, kVRR);
    const std::uint8_t low = pair->read(2);
    const std::uint8_t high = pair->read(3);
    check(
        (static_cast<std::uint16_t>(high) << 8 | low) == 0x1234,
        "the buffered read returns the word that was written");
    pair->settle("the read port agrees byte for byte");

    // Two control-register bits pick how far the address registers step
    // after each data-port access. The game only ever uses one of the four,
    // so all four are driven here.
    for (std::uint16_t increment = 0; increment < 4; ++increment) {
        pair->write_register(
            kCR, static_cast<std::uint16_t>(increment << 11));
        pair->write_register(kMAWR, 0x0100);
        pair->write(0, kVRR);
        for (int word = 0; word < 4; ++word) {
            pair->write(2, static_cast<std::uint8_t>(word));
            pair->write(3, 0xC0);
        }
        pair->write_register(kMARR, 0x0100);
        pair->write(0, kVRR);
        pair->read(2);
        pair->read(3);
        pair->read(2);
        pair->read(3);
    }
    pair->write_register(kCR, 0x0000);
    pair->settle("all four address increments agree");

    section("A programmed field");
    program_display(*pair);
    write_tile(*pair, 0x100, 0xFFFF, 0x0F0F);
    write_tile(*pair, 0x101, 0x5A5A, 0x0000);
    write_tile(*pair, 0x102, 0x0FF0, 0x3C3C);
    write_tile(*pair, 0x103, 0x1248, 0xF00F);
    // Enough entries for the widest and tallest map this test programs,
    // which stops short of the sprite attribute table at $0800.
    fill_tile_map(*pair, 0x100, 2048);
    pair->write_register(
        kCR,
        kControlBackgroundEnabled | kControlSpritesEnabled |
            kControlVerticalBlankIRQ);

    // The 30 extra clocks are the head start `VCE::Reset` gives the vertical
    // blanking counter, so this is a whole field rather than most of one.
    pair->run(kFieldClocks + 30);
    check(pair->scanlines() == 262, "the field ran 262 scanlines");
    pair->settle("every pixel of the first field agrees");

    const std::uint64_t after_first_field = pair->irq().raises;
    pair->run(kFieldClocks);
    check(
        pair->irq().raises - after_first_field == 1,
        "a settled field raises exactly one vertical blank");
    pair->settle("every pixel of the second field agrees");

    // Acknowledging is the only way the line falls, and both have to fall.
    const std::uint8_t status = pair->read(0);
    check(status == 0x20, "the status byte names the vertical blank");
    pair->settle("acknowledging the interrupt agrees");

    section("Background scrolling and tile map shapes");
    // A fine scroll that is not a multiple of eight, so the renderer's
    // first partial tile is exercised rather than skipped.
    use_tile_map(*pair, 0x0000, 0x0005, 0x0003);
    pair->settle("a fine-scrolled background agrees");

    // Twice as wide, scrolled almost a whole map across, so the horizontal
    // wrap happens part-way along every line rather than at its start. The
    // map index is an OR of the row and the column, not a sum, so a column
    // that wraps only reaches a different word on the rows where the row
    // index leaves that bit clear -- half of them.
    use_tile_map(*pair, 0x0010, 0x01FF, 0x00FF);
    pair->settle("a 64-wide tile map wrapping mid-line agrees");

    // Twice as tall, scrolled past its halfway point, so the height mask is
    // what decides which row of the map is fetched.
    use_tile_map(*pair, 0x0040, 0x0000, 300);
    pair->settle("a 64-row tile map scrolled past its half agrees");

    // The two-plane background modes, which keep half the bitplanes. Both
    // halves, because which one is a further register bit.
    use_tile_map(*pair, 0x0003, 0x0009, 0x0011);
    pair->settle("the low two-plane background mode agrees");

    use_tile_map(*pair, 0x0083, 0x0009, 0x0011);
    pair->settle("the high two-plane background mode agrees");

    use_tile_map(*pair, 0x0000, 0x0000, 0x0000);
    pair->settle("returning to the plain background mode agrees");
    pair->compare_memory("video memory still agrees after ten fields");

    section("A vertical program shorter than the field");
    // The usual program totals 263 lines inside a 262-line field, so the
    // device's own vertical phase machine never completes a cycle: vertical
    // sync re-syncs it first, every field, and the phase machine's own
    // re-latch of the map shape is dead code. A program a third of the
    // field long wraps three times before vertical sync arrives, which is
    // the only way that latch is ever the one that decides.
    pair->write_register(kVDW, 60 - 1);
    pair->write_register(kVCR, 5);
    pair->write_register(kMWR, 0x0050);
    pair->run(kFieldClocks);
    pair->settle("programming a short field agrees");
    pair->run(kFieldClocks);
    pair->settle("three display periods in one field agree");

    pair->write_register(kVDW, 224 - 1);
    pair->write_register(kVCR, 19);
    pair->write_register(kMWR, 0x0000);
    pair->run(kFieldClocks);
    pair->run(kFieldClocks);
    pair->settle("returning to the full-length program agrees");

    section("Sprites");
    // A sixteen-row sprite cell is 64 words: sixteen rows of four planes.
    // Eight cells, each row of each plane distinct, so that a model which
    // fetched the right cell but the wrong row inside it still differs.
    constexpr std::uint16_t kSpritePattern = 0x40;
    constexpr std::uint16_t kSpriteBase = kSpritePattern * 64;
    for (std::uint16_t word = 0; word < 8 * 64; ++word) {
        const std::uint16_t value = static_cast<std::uint16_t>(
            0x9E37U * (word + 1));
        poke(*pair, static_cast<std::uint16_t>(kSpriteBase + word), value);
    }

    constexpr std::uint16_t kSpriteTable = 0x0800;
    // Plain, horizontally flipped, vertically flipped, both, in front of the
    // background and behind it, and one 32 pixels wide.
    write_sprite(*pair, kSpriteTable, 0, 32, 40, kSpritePattern, 0x0001);
    write_sprite(*pair, kSpriteTable, 1, 64, 40, kSpritePattern, 0x0802);
    write_sprite(*pair, kSpriteTable, 2, 96, 40, kSpritePattern, 0x8003);
    write_sprite(*pair, kSpriteTable, 3, 128, 40, kSpritePattern, 0x8804);
    write_sprite(*pair, kSpriteTable, 4, 160, 40, kSpritePattern, 0x0085);
    write_sprite(*pair, kSpriteTable, 5, 192, 40, kSpritePattern, 0x0106);
    // Hanging off the left edge, which the renderer rejects by unsigned
    // wraparound rather than by a sign test.
    write_sprite(*pair, kSpriteTable, 6, 0, 72, kSpritePattern, 0x0007);
    // A 32-row sprite, so the pattern number's row bits are used.
    write_sprite(*pair, kSpriteTable, 7, 48, 96, kSpritePattern, 0x1008);
    // Wide and vertically flipped, which is the case where the two halves
    // have to be given the same row: the flip is applied to a row offset
    // that the second half recomputes rather than inherits.
    write_sprite(*pair, kSpriteTable, 8, 16, 120, kSpritePattern, 0x8109);
    write_sprite(*pair, kSpriteTable, 9, 64, 120, kSpritePattern, 0x890A);
    // Tall and wide together, so the row bits and the half bit land in the
    // same pattern number.
    write_sprite(*pair, kSpriteTable, 10, 112, 120, kSpritePattern, 0x210B);
    write_sprite(*pair, kSpriteTable, 11, 176, 128, kSpritePattern, 0xB10C);
    // An odd pattern number on a wide sprite, which is the only way to tell
    // whether the low bit is cleared before the half bit is put back.
    write_sprite(
        *pair,
        kSpriteTable,
        12,
        8,
        160,
        static_cast<std::uint16_t>(kSpritePattern + 1),
        0x010D);

    for (std::uint16_t index = 13; index < 64; ++index) {
        write_sprite(*pair, kSpriteTable, index, 0, 0x3FF - 0x40, 0, 0);
    }

    pair->write_register(kDVSSR, kSpriteTable);
    pair->run(kFieldClocks);
    pair->settle("the sprite attribute table transfer agrees");
    pair->compare_memory("the transferred sprite table agrees");

    pair->run(kFieldClocks);
    pair->settle("every pixel of a field with thirteen sprites agrees");

    // Sixteen-colour sprite mode leaves two bitplanes at zero, and an
    // attribute bit picks which pair is fetched.
    pair->write_register(kMWR, 0x0004);
    write_sprite(*pair, kSpriteTable, 1, 64, 40,
        static_cast<std::uint16_t>(kSpritePattern | 0x8000), 0x0002);
    pair->write_register(kDVSSR, kSpriteTable);
    pair->run(kFieldClocks);
    pair->settle("the 16-colour sprite mode agrees");
    pair->write_register(kMWR, 0x0000);

    section("Registers written in the middle of a line");
    // Every `run` above is a whole field, and the field this driver counts
    // begins at the end of vertical blanking -- so a section that starts
    // where the last one stopped starts in the vertical front porch, where
    // nothing is drawn and a write to a drawing register proves nothing.
    // These 44 lines walk into the display period first.
    constexpr std::int32_t kIntoDisplay = 60000;
    // Twenty lines, not one. A walk one line long crosses a single display
    // period, and if that one line happens to be drawn with the layer
    // disabled it covers nothing at all -- which is what a shorter walk
    // here did, silently.
    constexpr std::int32_t kWalk = 28000;

    // The layer enables are latched once a line, at the start of its display
    // period, so a write that lands in the window between that latch and the
    // drawing must not take effect until the next line. That window is a few
    // dozen clocks out of thirteen hundred, so a write every clock is what
    // makes it certain to be hit rather than likely.
    //
    // The value has to be hashed rather than alternated. The window from the
    // latch to the drawing is a fixed 96 clocks wide, so any pattern whose
    // period divides 96 -- and alternating every clock is the worst of them
    // -- presents the same value at both ends and tests nothing at all.
    pair->run(kIntoDisplay);
    const std::uint64_t before_walk = pair->coverage().background_lines;
    const std::uint64_t before_walk_sprites = pair->coverage().sprite_lines;
    pair->write(0, kCR);
    for (std::int32_t offset = 0; offset < kWalk; ++offset) {
        const std::uint32_t hash =
            static_cast<std::uint32_t>(offset + 1) * 0x9E3779B1U;
        pair->run(1);
        pair->write(2, static_cast<std::uint8_t>((hash >> 24) & 0xC0));
    }
    pair->write_register(
        kCR,
        kControlBackgroundEnabled | kControlSpritesEnabled |
            kControlVerticalBlankIRQ);
    // The walk is only worth anything if it happened while both layers were
    // being drawn -- and an earlier version of it did not, because a mask
    // typo made every written value "both layers off". A section that tests
    // a latch has to prove the thing being latched was in use.
    check(
        pair->coverage().background_lines > before_walk &&
            pair->coverage().sprite_lines > before_walk_sprites,
        "the walk ran while both layers were being drawn");
    pair->settle("layer enables written mid-line agree");

    // The vertical scroll is latched on either half of the register, not
    // only on the high one, and it takes effect on the next line rather
    // than the next field -- so a low byte written mid-line moves the
    // picture, and a model that waited for the high byte would not.
    pair->write(0, kBYR);
    for (std::int32_t offset = 0; offset < kWalk; ++offset) {
        pair->run(1);
        pair->write(2, static_cast<std::uint8_t>(offset * 11));
    }
    pair->write_register(kBYR, 0x0000);
    pair->settle("a scroll walked a byte at a time mid-line agrees");

    // Back to a field boundary, so the sections after this one are once
    // again talking about whole fields.
    pair->run(kFieldClocks - kIntoDisplay - 2 * kWalk);
    pair->settle("realigning to the field boundary agrees");


    section("Sprite overflow and sprite zero collision");
    // Twenty sprites on one line, which is four more than the device will
    // fetch, plus sprite zero overlapping sprite one so the collision test
    // has something to find.
    for (std::uint16_t index = 0; index < 20; ++index) {
        write_sprite(
            *pair,
            kSpriteTable,
            index,
            static_cast<std::uint16_t>(index * 12),
            48,
            kSpritePattern,
            static_cast<std::uint16_t>(index & 0xF));
    }
    write_sprite(*pair, kSpriteTable, 0, 12, 48, kSpritePattern, 0x0001);

    pair->write_register(kDVSSR, kSpriteTable);
    pair->write_register(
        kCR,
        kControlBackgroundEnabled | kControlSpritesEnabled |
            kControlSpriteZeroIRQ | kControlOverflowIRQ);
    pair->run(kFieldClocks);
    pair->settle("the transfer before a crowded line agrees");

    const std::uint64_t before_crowded = pair->irq().raises;
    pair->run(kFieldClocks);
    check(
        pair->irq().raises > before_crowded,
        "a crowded line raised something");
    pair->settle("every pixel of a crowded line agrees");

    pair->write_register(
        kCR,
        kControlBackgroundEnabled | kControlSpritesEnabled |
            kControlVerticalBlankIRQ);
    pair->read(0);
    pair->settle("acknowledging after the crowded line agrees");

    section("The raster compare");
    pair->write_register(kRCR, 0x40 + 100);
    pair->write_register(
        kCR,
        kControlBackgroundEnabled | kControlSpritesEnabled |
            kControlRasterCompareIRQ);
    pair->read(0);

    const std::int32_t to_compare = pair->run_until_irq(kFieldClocks);
    check(
        to_compare > 0 && to_compare < kFieldClocks,
        "the raster compare fired inside the field");
    check(
        (pair->read(0) & 0x04) != 0,
        "the status byte names the raster compare");
    pair->settle("the raster compare agrees on both arms");

    section("Port accesses at every offset across a line");
    // Whether a data-port write commits or is held depends on where in the
    // line it lands, and the window it is held through is a delay on
    // entering horizontal sync whose only documentation is its two
    // constants. Walking a write pair across more than a full line one
    // clock at a time is what makes those constants load-bearing: move
    // either and a different set of offsets stalls.
    pair->write_register(kMAWR, 0x0200);
    pair->write(0, kVRR);
    for (std::int32_t offset = 0; offset < 1400; ++offset) {
        pair->run(1);
        pair->write(2, static_cast<std::uint8_t>(offset & 0xFF));
        pair->write(3, static_cast<std::uint8_t>(offset >> 8));
    }
    pair->settle("every offset across a line stalls the same way");
    pair->compare_memory("the walked writes landed identically");

    section("A transfer that only burst mode lets through");
    // With both layers on the device is not in burst mode and a requested
    // block transfer cannot start -- except that finishing the sprite table
    // transfer puts it into burst mode, and the block transfer then runs in
    // what is left of the field before the next display period cancels it.
    // That handover is the only thing that lets this transfer make progress.
    poke(*pair, 0x6000, 0x1111);
    pair->write_register(kDCR, 0x0000);
    pair->write_register(kSOUR, 0x6000);
    pair->write_register(kDESR, 0x7000);
    pair->write_register(kLENR, 0x00FF);
    pair->write_register(kDVSSR, kSpriteTable);
    pair->run(kFieldClocks);
    pair->settle("a transfer handed over by the sprite table agrees");
    pair->compare_memory("the handed-over transfer moved the same words");

    section("Block transfers");
    // Both layers off puts the device in burst mode, which is the only state
    // in which a video memory transfer starts.
    pair->write_register(kCR, 0x0000);
    pair->run(kFieldClocks);
    pair->settle("entering burst mode agrees");

    poke(*pair, 0x2000, 0xABCD);
    poke(*pair, 0x2FFF, 0xF00F);
    pair->write_register(kDCR, 0x0002);
    pair->write_register(kSOUR, 0x2000);
    pair->write_register(kDESR, 0x3000);
    pair->write_register(kLENR, 0x0FFF);
    pair->run(kFieldClocks);
    pair->settle("a forward block transfer agrees");
    pair->compare_memory("the forward transfer moved the same words");

    // Backwards in both directions, which the game never asks for.
    pair->write_register(kDCR, 0x000C);
    pair->write_register(kSOUR, 0x2FFF);
    pair->write_register(kDESR, 0x4FFF);
    pair->write_register(kLENR, 0x0FFF);
    pair->run(kFieldClocks);
    pair->settle("a backward block transfer agrees");
    pair->compare_memory("the backward transfer moved the same words");

    // A short transfer, where the engine's own completion is nearer than the
    // next raster phase and so becomes the event horizon the device returns.
    // On a long transfer that term is dead weight -- it only ever binds in
    // the last few clocks -- so a short one is what tests it.
    pair->write_register(kDCR, 0x0002);
    pair->write_register(kSOUR, 0x2000);
    pair->write_register(kDESR, 0x6800);
    pair->write_register(kLENR, 0x0003);
    pair->run(4096);
    pair->settle("a short block transfer agrees on its event horizon");
    pair->compare_memory("the short transfer moved the same words");

    section("A port access during a block transfer");
    // The stall only happens when a prior access is still uncommitted, so
    // the first write pair below merely becomes pending and the second one
    // finds it and blocks.
    pair->write_register(kDCR, 0x0000);
    pair->write_register(kSOUR, 0x2000);
    pair->write_register(kDESR, 0x5000);
    pair->write_register(kLENR, 0x0FFF);
    pair->run(1024);

    pair->write_register(kMAWR, 0x0080);
    pair->write(0, kVRR);
    pair->write(2, 0x21);
    pair->write(3, 0x43);
    pair->write(2, 0x65);
    pair->write(3, 0x87);
    pair->settle("the stalled access agrees, stall for stall");

    pair->run(kFieldClocks);
    pair->settle("the field after the stall agrees");
    pair->compare_memory("the stalled transfer and write both landed");

    section("What the stimulus reached");
    const PceVdc::Coverage& coverage = pair->coverage();
    std::printf(
        "        fields %llu, scanlines %llu, pixels %llu (%llu visible)\n",
        static_cast<unsigned long long>(coverage.fields),
        static_cast<unsigned long long>(coverage.scanlines),
        static_cast<unsigned long long>(pair->emitted_pixels()),
        static_cast<unsigned long long>(pair->visible_pixels()));
    std::printf(
        "        sprites %llu fetched, %llu wide halves, %llu 16-colour\n",
        static_cast<unsigned long long>(coverage.sprites_fetched),
        static_cast<unsigned long long>(coverage.wide_sprite_halves),
        static_cast<unsigned long long>(coverage.narrow_colour_fetches));
    std::printf(
        "        transfers: %llu words, %llu tables, %llu forced\n",
        static_cast<unsigned long long>(coverage.vram_dma_words),
        static_cast<unsigned long long>(coverage.sat_dma_transfers),
        static_cast<unsigned long long>(coverage.forced_completions));

    // A differential test passes trivially if the stimulus never reached the
    // path in question, so the paths this file claims to cover are asserted
    // to have run. This is the same liveness caveat the earlier slices hit:
    // agreement is only worth what the stimulus was.
    check(coverage.background_lines > 0, "the background renderer ran");
    check(coverage.sprite_lines > 0, "the sprite renderer ran");
    check(coverage.burst_lines > 0, "burst mode was entered");
    check(coverage.wide_sprite_halves > 0, "a 32-pixel sprite was fetched");
    check(
        coverage.narrow_colour_fetches > 0,
        "the 16-colour sprite mode was fetched");
    check(coverage.sprite_overflows > 0, "a line overflowed on sprites");
    check(coverage.sprite_zero_hits > 0, "sprite zero collided");
    check(coverage.vram_dma_words > 0, "video memory was block transferred");
    check(coverage.sat_dma_transfers > 0, "the sprite table was transferred");
    check(coverage.raster_compares > 0, "the raster compare fired");
    check(coverage.vertical_blanks > 0, "the vertical blank fired");
    check(coverage.port_writes > 0, "the data port wrote video memory");
    check(coverage.port_reads > 0, "the data port read video memory");
    check(coverage.wait_state_requests > 0, "a port access stalled");
    check(coverage.forced_completions > 0, "a stall ran out of budget");

    if (g_failed) {
        std::printf("\nFAILED\n");
        return 1;
    }

    std::printf("\nThe native VDC is the emulator's VDC.\n");
    return 0;
}
