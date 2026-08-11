// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/video/vdc_device.hpp"

#include <array>
#include <cstdint>

namespace devils_crush::video {

// The HuC6260 colour encoder: a 512-entry palette, a dot-clock divider, two
// blanking counters, and the schedule that drives the video display
// controller and turns what it emits into a picture.
//
// The controller is the larger chip but the encoder is the one that owns
// time. It divides the master clock by four, three or two, hands the
// quotient to the controller as dot clocks, counts the 237- and
// 1128-cycle halves of the horizontal period and the 4095-cycle vertical
// blank, advances the scanline, and decides which of the controller's
// returned event horizons bounds the next chunk. The picture depends on
// that interleaving as much as it depends on either chip.
//
// This is a device model in the same sense `PceVdc` is: it owns no CPU
// state and reaches nothing outside itself. It has four outward edges,
// each a hook, and one downward edge, which is a `VdcDevice`. The four are
// the interrupt line, the stolen cycles a stalled port access charges the
// processor, the request that the processor stop early at a scanline
// boundary, and the compact disc unit's next event.
//
// Two things a reader might expect are deliberately outside it.
//
// The first is what a nine-bit palette entry looks like. The encoder holds
// palette memory, and a palette entry is three three-bit components -- but
// what those become on a screen is the display's decision, not the chip's,
// and the emulator makes it in `VCE::SetPixelFormat` from a pixel format it
// was handed. So this model is given the two 512-entry expansions ready
// made, one normal and one greyscale, and merges them with palette memory
// itself. `set_surface_palette` is the whole of that seam.
//
// The second is frame geometry. Where the visible picture sits inside the
// 1365-cycle line, how a multi-resolution frame is rescaled, and which
// rectangle the frontend should show are all decisions made from what the
// encoder recorded rather than by the encoder. The model reports each
// change of horizontal timing through `ScanlineInfoHook` and leaves the
// recording to whoever wants it.
//
// The SuperGrafx is not modelled. Its second controller and the HuC6202
// priority unit that composites the two are a different machine, no part of
// this port routes them, and a model that carried them would carry a
// compositor no test here could reach.
class PceVce {
public:
    // The emulator's `bpp_t`, which is 16 bits wide in this build. The
    // colour cache is 32 bits wide because the emulator's is, and the store
    // into the framebuffer truncates in both.
    using Pixel = std::uint16_t;

    static constexpr std::uint32_t kPaletteEntries = 0x200;
    // Twice the palette, because the controller sets bit 9 on a pixel whose
    // layer has been disabled through the debugging mask, and those indices
    // land in the upper half.
    static constexpr std::uint32_t kColourCacheEntries = 0x200 * 2;
    // The line buffer handed to the controller each chunk.
    static constexpr int kPixelBufferSize = 2048;
    // Lines in the framebuffer the scanline counter wraps against.
    static constexpr int kFieldLines = 263;
    // What one horizontal period costs, split at the sync pulse.
    static constexpr std::int32_t kHBlankClocks = 237;
    static constexpr std::int32_t kHDisplayClocks = 1128;
    // The vertical blank, and the two field lengths it is subtracted from.
    static constexpr std::int32_t kVBlankClocks = 4095;
    static constexpr std::int32_t kField262Clocks = 357630;
    static constexpr std::int32_t kField263Clocks = 358995;
    // The processor cycles a frame may lose to stalled port accesses before
    // the encoder starts refusing. The refusal is what ends the controller's
    // stall loop, so this number is load-bearing rather than advisory.
    static constexpr std::int32_t kWaitStateBudget = 455 * 64;
    // What `VCE::RunPartial` arms the compact disc event to on a machine
    // that has no compact disc unit: far enough away never to arrive.
    static constexpr std::int32_t kNoCdEvent = 0x3FFFFFFF;

    // The master clock divided per dot rate, indexed by the low two bits of
    // the control register. Rates two and three are the same 10 MHz clock.
    static constexpr std::array<std::int32_t, 4> kDotClockRatios{4, 3, 2, 2};

    // One change of horizontal timing, as the encoder saw it. Reported
    // whenever the control register is written and whenever a field
    // restarts, which is when the frontend's picture geometry can change.
    struct ScanlineInfo {
        // The four fields of the controller's HSR and HDR registers, in
        // units of eight pixels: sync pulse, display start, display width,
        // display end.
        int pulse = 0;
        int start = 0;
        int width = 0;
        int end = 0;
        // The dot rate in force, and the line it changed on.
        int rate = 0;
        int line = 0;
        // Set when the encoder is about to restart the field, which is when
        // the recorder should discard what it has.
        bool restart = false;
    };

    using IrqHook = void (*)(void* context, bool raised);
    // The processor loses these cycles to a stalled port access. The
    // encoder has already decided the stall is affordable by the time this
    // is called; a hook that declines has no way to say so.
    using StealCyclesHook = void (*)(void* context, std::int32_t cycles);
    // Asked to stop running the processor early, at the end of the visible
    // field and again at line 123. The second has no documented reason and
    // is reproduced because the emulator does it.
    using ExitHook = void (*)(void* context);
    using ScanlineInfoHook = void (*)(void* context, const ScanlineInfo&);
    // Runs the compact disc unit up to `timestamp` and returns its next
    // event. A machine without one leaves this unset, and the encoder
    // re-arms to `kNoCdEvent`.
    using CdEventHook = std::int32_t (*)(void* context,
        std::uint32_t timestamp);

    // Which paths the model has actually run. The controller's coverage
    // showed how narrow a route's demands on a chip are; this is the same
    // question asked of the encoder, where the answers are mostly about
    // rates and stalls rather than about drawing.
    struct Coverage {
        // Frames started, partial runs within them, and frames that reached
        // the end of the visible field.
        std::uint64_t frames = 0;
        std::uint64_t partials = 0;
        std::uint64_t frames_completed = 0;
        // Scanlines advanced, fields restarted, and vertical blanks entered.
        std::uint64_t scanlines = 0;
        std::uint64_t field_restarts = 0;
        std::uint64_t vertical_blanks = 0;
        // Runs handed to the controller for a line outside the visible
        // range, which neither draw nor reach its renderers.
        std::uint64_t off_screen_runs = 0;
        // Chunks the schedule divided the run into, and dot clocks handed to
        // the controller.
        std::uint64_t chunks = 0;
        std::uint64_t dot_clocks = 0;
        // Chunks the schedule had to floor at one clock because an event was
        // already due. A route that never reaches this has not tested it.
        std::uint64_t floored_chunks = 0;
        // Pixels written into the framebuffer, and lines drawn at each of
        // the four dot rates.
        std::uint64_t pixels = 0;
        std::array<std::uint64_t, 4> lines_at_rate{};
        // Control register writes that changed the rate, the black-and-white
        // bit, or the 263-line field select.
        std::uint64_t dot_clock_changes = 0;
        std::uint64_t monochrome_changes = 0;
        std::uint64_t line_count_changes = 0;
        // Palette port traffic, counted where the register file decides.
        std::uint64_t palette_writes = 0;
        std::uint64_t palette_reads = 0;
        // Stalls asked for, stalls refused because the frame's budget ran
        // out, and processor cycles handed over.
        std::uint64_t wait_state_requests = 0;
        std::uint64_t wait_state_refusals = 0;
        std::uint64_t stolen_cycles = 0;
        // Stalls asked for by event rather than by a cycle count, which is
        // how the controller stalls while a transfer runs.
        std::uint64_t event_stalls = 0;
        // Interrupt line transitions, counted at the encoder rather than at
        // the controller: the encoder re-derives the level and only the
        // changes it publishes matter.
        std::uint64_t irq_raises = 0;
        std::uint64_t irq_lowers = 0;
        // Requests that the processor stop early, and compact disc events
        // that came due.
        std::uint64_t cpu_exits = 0;
        std::uint64_t cd_events = 0;
    };

    // Every field the emulator's savestate carries for this device, in its
    // order. Two things are deliberately absent: the colour cache, because
    // it is derived from palette memory and the surface expansion and is
    // rebuilt on apply, and everything the frontend owns -- the framebuffer,
    // the line widths, the visible range -- because that belongs to whoever
    // started the frame, not to the moment being restored.
    struct State {
        std::uint8_t cr = 0;
        std::int32_t ws_counter = 0;
        std::uint16_t ctaddress = 0;
        std::array<std::uint16_t, kPaletteEntries> color_table{};
        std::int32_t clock_divider = 0;
        // Both entries, because the emulator writes both even on a machine
        // with one controller, and a state has to load in either arm.
        std::array<std::int32_t, 2> child_event{};
        std::int32_t scanline = 0;
        std::int32_t pixel_offset = 0;
        std::int32_t hblank_counter = 0;
        std::int32_t vblank_counter = 0;
        bool hblank = false;
        bool vblank = false;
        bool need_sl_reset = false;
    };

    PceVce() noexcept;

    // The controller this encoder clocks. Required before `reset`.
    void set_vdc(VdcDevice* vdc) noexcept;

    void set_irq_hook(IrqHook hook, void* context) noexcept;
    void set_steal_cycles_hook(StealCyclesHook hook, void* context) noexcept;
    void set_exit_hook(ExitHook hook, void* context) noexcept;
    void set_scanline_info_hook(
        ScanlineInfoHook hook,
        void* context) noexcept;
    void set_cd_event_hook(CdEventHook hook, void* context) noexcept;

    // The two 512-entry expansions of a nine-bit palette entry: `normal` as
    // the display shows it, `grey` as it shows it with the control
    // register's black-and-white bit set. Rebuilds the colour cache.
    void set_surface_palette(
        const std::uint32_t* normal,
        const std::uint32_t* grey) noexcept;
    // What a pixel whose layer has been disabled through the debugging mask
    // is drawn as. Re-asserted at the start of every frame, as the emulator
    // does, because nothing else writes those two cache entries.
    void set_disabled_layer_colour(std::uint32_t colour) noexcept;
    void set_show_horizontal_overscan(bool show) noexcept;

    // Both forwarded to the controller, which is where they mean something.
    // The mask's low two bits are the first controller's.
    void set_layer_enable_mask(std::uint64_t mask) noexcept;
    void set_unlimited_sprites(bool unlimited) noexcept;

    // `VCE::Reset` takes a timestamp and ignores it, rebasing to zero
    // regardless, so this does not take one.
    void reset() noexcept;
    // Rebases the timestamp the next `sync` measures from, for a machine
    // that has just wrapped its clock.
    void reset_timestamp(std::int32_t base) noexcept;

    // The framebuffer for one frame. `pitch` is in pixels, `line_widths`
    // holds `kFieldLines` entries, and a null framebuffer with `skip` set is
    // how the emulator asks for a frame that is computed but not drawn.
    //
    // One deliberate departure lives here. `VCE::SyncSub` decides whether to
    // draw a line from the visible range alone and does not consult `skip`,
    // so a skipped frame whose scanline is inside that range writes through
    // a null framebuffer pointer. This model checks the pointer instead.
    // Nothing in this port skips a frame, so the two agree everywhere the
    // emulator survives.
    void start_frame(
        Pixel* framebuffer,
        std::int32_t pitch,
        std::int32_t* line_widths,
        bool skip,
        bool awesome_mode,
        int scanline_start,
        int scanline_end) noexcept;
    // Clears the frame's stall budget, as `VCE::RunPartial` does before it
    // runs the processor.
    void start_partial() noexcept;
    // Fills in the width of every line that was never drawn, as
    // `VCE::RunPartial` does after the processor stops.
    void finish_partial() noexcept;

    [[nodiscard]] bool frame_done() const noexcept { return frame_done_; }

    // Advances to `timestamp` in master clocks and returns the next event
    // horizon. This is the whole schedule.
    std::int32_t sync(std::int32_t timestamp) noexcept;
    [[nodiscard]] std::int32_t calc_next_event() const noexcept;
    // Arms the compact disc event, biased by how far the processor has run
    // past the last sync.
    void set_cd_event(std::int32_t cycles, std::int32_t time_behind) noexcept;

    // The encoder's own register file at $0400-$0405. Both are pure: the
    // caller syncs to the access's timestamp first and publishes the new
    // event horizon afterwards, exactly as `VCE::Read` and `VCE::Write` do
    // around the same bodies.
    void write(std::uint32_t address, std::uint8_t value) noexcept;
    [[nodiscard]] std::uint8_t read(std::uint32_t address) noexcept;

    // The controller's register file at $0000-$0003, reached through the
    // encoder because the encoder holds the controller's event horizon.
    void write_vdc(std::uint32_t address, std::uint8_t value) noexcept;
    [[nodiscard]] std::uint8_t read_vdc(std::uint32_t address) noexcept;
    // The processor's ST0/ST1/ST2 instructions, which address the controller
    // directly.
    void write_vdc_st(std::uint32_t address, std::uint8_t value) noexcept;

    // Re-derives the interrupt line from the controller and publishes it.
    void irq_change_check() noexcept;
    // The controller's stall request. `vdc_cycles` is a dot-clock count, or
    // -1 to mean "until the next event". Returns false once the frame's
    // budget is spent, which is the only thing that ends the stall loop.
    bool wait_state(std::int32_t vdc_cycles) noexcept;

    // Fires `ScanlineInfoHook` with the current horizontal timing, for a
    // frontend that wants the last line of a finished frame.
    void report_scanline_info() noexcept;

    [[nodiscard]] std::int32_t scanline() const noexcept { return scanline_; }
    // The timestamp the last `sync` measured to. A machine arming the compact
    // disc event has to bias it by how far its processor has run past this.
    [[nodiscard]] std::int32_t last_timestamp() const noexcept
    {
        return last_ts_;
    }
    [[nodiscard]] unsigned dot_clock() const noexcept { return dot_clock_; }
    [[nodiscard]] bool show_horizontal_overscan() const noexcept
    {
        return show_horiz_os_;
    }
    [[nodiscard]] std::uint16_t peek_palette(
        std::uint16_t address) const noexcept
    {
        return color_table_[address & 0x1FF];
    }
    // The merge of palette memory and the surface expansion, which is what
    // a pixel is actually looked up in. Entries 0x200 and 0x300 hold the
    // disabled-layer colour instead of a palette entry.
    [[nodiscard]] std::uint32_t peek_colour_cache(
        std::uint16_t entry) const noexcept
    {
        return color_table_cache_[entry & 0x3FF];
    }

    [[nodiscard]] const Coverage& coverage() const noexcept
    {
        return coverage_;
    }

    void capture_state(State& state) const noexcept;
    // Applies a captured state and repairs the values a savestate is allowed
    // to hold but the device is not, exactly as the emulator's own loader
    // does, then rebuilds the colour cache.
    void apply_state(const State& state) noexcept;

private:
    void fix_pcache(int entry) noexcept;
    void set_control_register(std::uint8_t value) noexcept;
    void raise_irq(bool raised) noexcept;
    template <bool TA_AwesomeMode>
    void sync_sub(std::int32_t clocks) noexcept;

    VdcDevice* vdc_ = nullptr;

    IrqHook irq_hook_ = nullptr;
    void* irq_context_ = nullptr;
    StealCyclesHook steal_hook_ = nullptr;
    void* steal_context_ = nullptr;
    ExitHook exit_hook_ = nullptr;
    void* exit_context_ = nullptr;
    ScanlineInfoHook scanline_info_hook_ = nullptr;
    void* scanline_info_context_ = nullptr;
    CdEventHook cd_event_hook_ = nullptr;
    void* cd_event_context_ = nullptr;

    // The interrupt level last published, so the hook sees changes rather
    // than every re-derivation.
    bool irq_line_ = false;

    std::int32_t child_event_[2] = {1, 1};
    std::int32_t cd_event_ = 1;

    Pixel* fb_ = nullptr;
    std::int32_t pitch32_ = 0;
    std::int32_t* line_widths_ = nullptr;
    bool frame_done_ = false;
    bool skipframe_ = false;
    bool awesome_ = false;
    bool show_horiz_os_ = false;
    int scanline_start_ = 0;
    int scanline_end_ = 242;

    std::int32_t clock_divider_ = 0;

    std::int32_t scanline_ = 0;
    Pixel* scanline_out_ptr_ = nullptr;
    std::int32_t pixel_offset_ = 0;

    std::int32_t hblank_counter_ = 237;
    std::int32_t vblank_counter_ = 4095 + 30;

    bool hblank_ = true;
    bool vblank_ = true;
    bool need_sl_reset_ = false;

    std::uint8_t CR_ = 0;
    bool lc263_ = false;
    bool bw_ = false;
    unsigned dot_clock_ = 0;
    std::int32_t dot_clock_ratio_ = 4;

    std::int32_t ws_counter_ = 0;
    std::int32_t last_ts_ = 0;

    std::uint16_t ctaddress_ = 0;
    std::uint32_t disabled_layer_colour_ = 0;

    Coverage coverage_{};

    std::array<std::uint32_t, kColourCacheEntries> color_table_cache_{};
    std::array<std::uint16_t, kPaletteEntries> color_table_{};
    std::array<std::array<std::uint32_t, 512>, 2> surf_clut_{};
    std::array<std::uint16_t, kPixelBufferSize> pixel_buffer_{};
};

}  // namespace devils_crush::video
