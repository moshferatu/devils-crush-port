// SPDX-License-Identifier: GPL-3.0-or-later

// Can the emulator's video display controller be driven without an
// emulator?
//
// The plan for the VDC recorded the opposite: that unlike the sound
// generator, the video device is read back by the game, so a reference
// device would need the machine around it, and that this was the part to
// design first. This is that design, settled by experiment rather than by
// argument, because the answer decides the shape of the whole slice.
//
// What it establishes is that `VDC` is standalone in the same way
// `PCE_PSG` is -- it owns its memories, its registers and its raster
// phase, and its only outward edges are two function pointers -- and that
// read-back is not the obstacle it was taken for, because the game's reads
// are methods on the device. What the device does need, and the PSG did
// not, is a clock schedule: `VCE` decides how `run`, `hsync` and `vsync`
// interleave, and the picture depends on it. So `Driver` below is a
// faithful copy of that schedule, and it is the piece a native VDC will
// have to reproduce.

#include "dc_beetle_reference_vdc.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

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

// VDC register select indices.
constexpr std::uint8_t kMAWR = 0x00;
constexpr std::uint8_t kMARR = 0x01;
constexpr std::uint8_t kVRR = 0x02;
constexpr std::uint8_t kCR = 0x05;
constexpr std::uint8_t kRCR = 0x06;
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

// Status bits, as the emulator names them.
constexpr std::uint8_t kStatusRasterCompare = 0x04;
constexpr std::uint8_t kStatusVerticalBlank = 0x20;

// Control register bits.
constexpr std::uint16_t kControlRasterCompareIRQ = 0x0004;
constexpr std::uint16_t kControlVerticalBlankIRQ = 0x0008;
constexpr std::uint16_t kControlSpritesEnabled = 0x0040;
constexpr std::uint16_t kControlBackgroundEnabled = 0x0080;

// One 262-line field, in master clocks, as `VCE` counts it.
constexpr std::int32_t kFieldClocks = 357630;

// `VCE::vce_ratios`, indexed by dot clock. Index 0 is the 5 MHz clock the
// game runs in.
constexpr std::int32_t kDotClockRatio = 4;

// What the field above draws, exactly. Nothing here is sampled from the
// outside -- no ROM, no input, no clock -- so the picture is a constant,
// and stating it as one is what makes the schedule load-bearing. Driving
// the same register writes on the 7 MHz dot clock instead yields 76,413.
constexpr std::uint64_t kExpectedVisiblePixels = 57347;

// The emulator hands the device a 2048-entry line buffer.
constexpr std::size_t kPixelBufferSize = 2048;

// A copy of `VCE::SyncSub`, minus the SuperGrafx and framebuffer halves.
//
// This is the whole of what the video chip needs from the machine around
// it: a master clock divided by the dot-clock ratio, a horizontal blanking
// counter that alternates between 237 clocks inside blanking and 1128
// outside, a vertical one that alternates between 4095 and the rest of the
// field, and the device's own returned event horizon bounding the chunk.
class Driver {
public:
    explicit Driver(dc_beetle_reference_vdc* const vdc)
        : vdc_(vdc)
        , pixels_(kPixelBufferSize, 0)
    {
        child_event_ = dc_beetle_reference_vdc_reset(vdc_);
    }

    void write(const std::uint32_t address, const std::uint8_t value)
    {
        dc_beetle_reference_vdc_write(vdc_, address, value, &child_event_);
    }

    std::uint8_t read(const std::uint32_t address)
    {
        return dc_beetle_reference_vdc_read(
            vdc_, address, &child_event_, 0);
    }

    // Select a register, then write its low and high halves. The high
    // write is the one that commits.
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

            if (divided > 0) {
                child_event_ = dc_beetle_reference_vdc_run(
                    vdc_, divided, pixels_.data(), 0);
                count_pixels(divided);
            }

            clocks -= chunk;

            hblank_counter_ -= chunk;
            if (hblank_counter_ <= 0) {
                hblank_ = !hblank_;
                if (!hblank_) {
                    ++scanlines_;
                }
                hblank_counter_ = hblank_ ? 237 : 1128;
                child_event_ =
                    dc_beetle_reference_vdc_hsync(vdc_, hblank_ ? 1 : 0);
                ++hsync_calls_;
            }

            vblank_counter_ -= chunk;
            if (vblank_counter_ <= 0) {
                vblank_ = !vblank_;
                vblank_counter_ = vblank_ ? 4095 : (kFieldClocks - 4095);
                child_event_ =
                    dc_beetle_reference_vdc_vsync(vdc_, vblank_ ? 1 : 0);
                ++vsync_calls_;
            }
        }
    }

    // Run until the device raises an interrupt, or until the budget runs
    // out. Returns the clocks consumed.
    std::int32_t run_until_irq(const std::int32_t budget)
    {
        std::int32_t spent = 0;

        while (spent < budget) {
            if (dc_beetle_reference_vdc_peek_irq(vdc_) != 0) {
                break;
            }
            run(1);
            ++spent;
        }

        return spent;
    }

    [[nodiscard]] std::uint64_t visible_pixels() const
    {
        return visible_pixels_;
    }
    [[nodiscard]] std::uint64_t scanlines() const { return scanlines_; }
    [[nodiscard]] std::uint64_t hsync_calls() const { return hsync_calls_; }
    [[nodiscard]] std::uint64_t vsync_calls() const { return vsync_calls_; }

private:
    void count_pixels(const std::int32_t emitted)
    {
        for (std::int32_t index = 0; index < emitted; ++index) {
            if ((pixels_[static_cast<std::size_t>(index)] & 0x0F) != 0) {
                ++visible_pixels_;
            }
        }
    }

    dc_beetle_reference_vdc* vdc_;
    std::vector<std::uint16_t> pixels_;

    std::int32_t hblank_counter_ = 237;
    std::int32_t vblank_counter_ = 4095 + 30;
    bool hblank_ = true;
    bool vblank_ = true;
    std::int32_t clock_divider_ = 0;
    std::int32_t child_event_ = 1;

    std::uint64_t visible_pixels_ = 0;
    std::uint64_t scanlines_ = 0;
    std::uint64_t hsync_calls_ = 0;
    std::uint64_t vsync_calls_ = 0;
};

// A 256x224 display with the background on, one solid tile, and a screen
// full of it. Nothing here is Devil's Crush's own setup; it is the
// smallest program that makes the device draw.
void program_display(Driver& driver)
{
    driver.write_register(kCR, 0);

    // HSW 2, HDS 2; HDW 31 (32 tiles), HDE 4.
    driver.write_register(kHSR, 0x0202);
    driver.write_register(kHDR, 0x041F);
    // VSW 2, VDS 15; 224 lines; VCR 4.
    driver.write_register(kVPR, 0x0F02);
    driver.write_register(kVDW, 224 - 1);
    // The device's vertical phases run VSW + 1, VDS + 2, VDW + 1, then
    // VCR lines. 3 + 17 + 224 + 19 is the driver's 263, so the programmed
    // display and the field it is clocked in agree.
    driver.write_register(kVCR, 19);
    // 32x32 background attribute table, 64-word address increment off.
    driver.write_register(kMWR, 0x0000);

    // A solid tile at character 0x100, which is VRAM word 0x1000. Both
    // low bitplanes set gives every pixel colour 3.
    driver.write_register(kMAWR, 0x1000);
    driver.write(0, kVRR);
    for (int word = 0; word < 16; ++word) {
        const std::uint16_t value = (word < 8) ? 0xFFFF : 0x0000;
        driver.write(2, static_cast<std::uint8_t>(value & 0xFF));
        driver.write(3, static_cast<std::uint8_t>(value >> 8));
    }

    // Fill the attribute table with it.
    driver.write_register(kMAWR, 0x0000);
    driver.write(0, kVRR);
    for (int entry = 0; entry < 32 * 32; ++entry) {
        driver.write(2, 0x00);
        driver.write(3, 0x01);
    }

    driver.write_register(
        kCR,
        kControlBackgroundEnabled | kControlSpritesEnabled
            | kControlVerticalBlankIRQ);
}

}  // namespace

int main()
{
    std::printf("Is the emulator's VDC usable without the emulator?\n\n");

    std::printf("Construction and lifetime\n");
    dc_beetle_reference_vdc* const vdc =
        dc_beetle_reference_vdc_create(32768);
    check(vdc != nullptr, "a bare VDC constructs with no machine around it");
    if (vdc == nullptr) {
        return 1;
    }

    dc_beetle_reference_vdc* const second =
        dc_beetle_reference_vdc_create(32768);
    check(second == nullptr, "a second device is refused, as documented");

    {
        Driver driver(vdc);

        std::printf("\nMemory and registers through the ports\n");

        driver.write_register(kMAWR, 0x0040);
        driver.write(0, kVRR);
        driver.write(2, 0x34);
        driver.write(3, 0x12);
        check(
            dc_beetle_reference_vdc_peek_vram(vdc, 0x0040) == 0x1234,
            "a data-port write reaches VRAM");
        check(
            dc_beetle_reference_vdc_get_register(vdc, 0 /* GSREG_MAWR */)
                == 0x0041,
            "the write auto-incremented the address register");

        driver.write_register(kMARR, 0x0040);
        driver.write(0, kVRR);
        const std::uint8_t low = driver.read(2);
        const std::uint8_t high = driver.read(3);
        check(
            low == 0x34 && high == 0x12,
            "the same word reads back through the data port");

        program_display(driver);

        std::printf("\nRaster, interrupts and read-back over one field\n");

        // Clear anything the setup raised, then run a whole field. The
        // extra 30 clocks are the offset `VCE::Reset` starts the vertical
        // counter with, and without them the field ends one clock short of
        // its second blanking edge.
        static_cast<void>(driver.read(0));
        driver.run(kFieldClocks + 30);

        std::printf(
            "  scanlines %llu, hsync %llu, vsync %llu, visible pixels "
            "%llu\n",
            static_cast<unsigned long long>(driver.scanlines()),
            static_cast<unsigned long long>(driver.hsync_calls()),
            static_cast<unsigned long long>(driver.vsync_calls()),
            static_cast<unsigned long long>(driver.visible_pixels()));

        check(driver.scanlines() == 262, "the field ran 262 scanlines");
        check(
            driver.vsync_calls() == 2,
            "vertical blanking entered and left once each");
        check(
            driver.visible_pixels() == kExpectedVisiblePixels,
            "the background drew exactly the expected picture");

        const std::uint64_t irq_calls =
            dc_beetle_reference_vdc_irq_hook_calls(vdc);
        std::printf(
            "  interrupt hook %llu calls (%llu raises), wait-state hook "
            "%llu calls (%lld cycles)\n",
            static_cast<unsigned long long>(irq_calls),
            static_cast<unsigned long long>(
                dc_beetle_reference_vdc_irq_hook_raises(vdc)),
            static_cast<unsigned long long>(
                dc_beetle_reference_vdc_wait_state_hook_calls(vdc)),
            static_cast<long long>(
                dc_beetle_reference_vdc_wait_state_cycles(vdc)));

        // The first field is not the steady state. `vsync` re-syncs the
        // device's vertical phase, and until the first one arrives the
        // device is free-running from wherever `reset` left it, so its
        // display comes round twice inside that field. From the second
        // field on, the two agree and exactly one vertical blank is
        // raised -- which is the claim worth making, since it is the one
        // a native model has to reproduce every frame.
        const std::uint64_t raises_after_first_field =
            dc_beetle_reference_vdc_irq_hook_raises(vdc);
        static_cast<void>(driver.read(0));
        driver.run(kFieldClocks);
        const std::uint64_t raises_in_second_field =
            dc_beetle_reference_vdc_irq_hook_raises(vdc)
            - raises_after_first_field;

        std::printf(
            "  raises: %llu in the first field, %llu in the second\n",
            static_cast<unsigned long long>(raises_after_first_field),
            static_cast<unsigned long long>(raises_in_second_field));

        check(
            raises_in_second_field == 1,
            "a settled field raises exactly one vertical blank");

        // The read-back the plan doubted: ask the device what happened,
        // and get the vertical blank flag back.
        const std::uint8_t status = driver.read(0);
        std::printf("  status $%02X\n", status);
        check(
            (status & kStatusVerticalBlank) != 0,
            "reading the status port reports the vertical blank");
        check(
            dc_beetle_reference_vdc_peek_irq(vdc) == 0,
            "and reading it acknowledged the interrupt");

        std::printf("\nA raster compare on a chosen line\n");

        driver.write_register(kRCR, 64 + 100);
        driver.write_register(
            kCR,
            kControlBackgroundEnabled | kControlSpritesEnabled
                | kControlRasterCompareIRQ);
        static_cast<void>(driver.read(0));

        const std::int32_t spent = driver.run_until_irq(kFieldClocks);
        const std::uint8_t raster_status = driver.read(0);
        std::printf(
            "  fired after %ld clocks, status $%02X\n",
            static_cast<long>(spent),
            raster_status);
        check(
            spent < kFieldClocks,
            "the raster compare interrupt arrived within the field");
        check(
            (raster_status & kStatusRasterCompare) != 0,
            "and the status port names it as the cause");

        std::printf("\nStalling the caller during a block transfer\n");

        // The one place the device reaches back for something the machine
        // owns. A port access while a VRAM-to-VRAM transfer is running
        // sends it into a loop that only ends when the wait-state hook
        // refuses, so this is the edge that would hang a reference device
        // whose hook merely agreed.
        driver.write_register(kMAWR, 0x2000);
        driver.write(0, kVRR);
        driver.write(2, 0xCD);
        driver.write(3, 0xAB);
        driver.write_register(kMAWR, 0x2FFF);
        driver.write(0, kVRR);
        driver.write(2, 0x0F);
        driver.write(3, 0xF0);

        // A block transfer only starts in burst mode, which the device
        // enters when both layers are off -- which is how a game reaches
        // VRAM in quantity, and why this is not an exotic case.
        driver.write_register(kCR, 0);
        driver.run(kFieldClocks);

        dc_beetle_reference_vdc_reset_wait_states(vdc);
        const std::uint64_t stalls_before =
            dc_beetle_reference_vdc_wait_state_hook_calls(vdc);

        driver.write_register(kDCR, 0x0000);
        driver.write_register(kSOUR, 0x2000);
        driver.write_register(kDESR, 0x3000);
        driver.write_register(kLENR, 0x0FFF);

        // Let it get going, but nowhere near the 65,536 master clocks the
        // whole transfer would take.
        driver.run(1024);

        // Touch the data port while that transfer is in flight. The first
        // write only becomes pending -- the device checks for a stall
        // before recording it, so there is nothing yet to wait on. It is
        // the second that finds the first still uncommitted and blocks.
        driver.write_register(kMAWR, 0x0080);
        driver.write(0, kVRR);
        driver.write(2, 0x21);
        driver.write(3, 0x43);
        driver.write(2, 0x87);
        driver.write(3, 0x65);

        const std::uint64_t stalls =
            dc_beetle_reference_vdc_wait_state_hook_calls(vdc)
            - stalls_before;
        std::printf(
            "  %llu stalls, budget counter %ld\n",
            static_cast<unsigned long long>(stalls),
            static_cast<long>(
                dc_beetle_reference_vdc_wait_state_counter(vdc)));

        check(stalls > 0, "the second access stalled");
        check(
            dc_beetle_reference_vdc_peek_vram(vdc, 0x3FFF) == 0xF00F,
            "the refusal made the device finish the whole transfer");
        check(
            dc_beetle_reference_vdc_peek_vram(vdc, 0x0080) == 0x4321,
            "and the write it had been holding then landed");
    }

    dc_beetle_reference_vdc_destroy(vdc);

    dc_beetle_reference_vdc* const after =
        dc_beetle_reference_vdc_create(32768);
    check(after != nullptr, "destroying one allows the next");
    dc_beetle_reference_vdc_destroy(after);

    std::printf(
        "\n%s\n",
        g_failed ? "The VDC is not usable standalone as written."
                 : "The VDC is usable standalone, read-back included.");

    return g_failed ? 1 : 0;
}
