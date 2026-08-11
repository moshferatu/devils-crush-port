// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/video/pce_vce.hpp"

#include <algorithm>

namespace devils_crush::video {
namespace {

// The two registers the encoder reads out of the controller for its own
// bookkeeping, numbered as `VDC::GSREG_HSR` and `VDC::GSREG_HDR` are.
constexpr unsigned kRegisterHsr = 7;
constexpr unsigned kRegisterHdr = 8;

// Where the visible picture starts inside the 1365-clock line, and how wide
// it is, per dot rate. The first row is the ordinary crop and the second
// shows the horizontal overscan.
constexpr int kLineOrigins[2][4] = {
    {8 + 24, 8 + 38, 8 + 96, 8 + 96},
    {8 + 24 - 12, 8 + 38 - 16, 8 + 96 - 24, 8 + 96 - 24},
};
constexpr int kLineWidths[2][4] = {
    {256, 341, 512, 512},
    {256 + 24, 341 + 32, 512 + 48, 512 + 48},
};

// The first visible line. Fourteen lines of the field are above it.
constexpr int kFirstVisibleLine = 14;
// The line the emulator stops the processor on for no recorded reason.
constexpr int kEarlyExitLine = 123;

}  // namespace

PceVce::PceVce() noexcept = default;

void PceVce::set_vdc(VdcDevice* const vdc) noexcept
{
    vdc_ = vdc;

    // `VCE`'s constructor leaves the controller with both layers on and the
    // sprite limit in force, so a freshly joined pair matches a freshly
    // constructed encoder. Anything else is the caller's to set afterwards.
    if (vdc_ != nullptr) {
        vdc_->set_layer_enable_mask(0x3);
        vdc_->set_unlimited_sprites(false);
    }
}

void PceVce::set_irq_hook(const IrqHook hook, void* const context) noexcept
{
    irq_hook_ = hook;
    irq_context_ = context;
}

void PceVce::set_steal_cycles_hook(
    const StealCyclesHook hook,
    void* const context) noexcept
{
    steal_hook_ = hook;
    steal_context_ = context;
}

void PceVce::set_exit_hook(const ExitHook hook, void* const context) noexcept
{
    exit_hook_ = hook;
    exit_context_ = context;
}

void PceVce::set_scanline_info_hook(
    const ScanlineInfoHook hook,
    void* const context) noexcept
{
    scanline_info_hook_ = hook;
    scanline_info_context_ = context;
}

void PceVce::set_cd_event_hook(
    const CdEventHook hook,
    void* const context) noexcept
{
    cd_event_hook_ = hook;
    cd_event_context_ = context;
}

void PceVce::set_surface_palette(
    const std::uint32_t* const normal,
    const std::uint32_t* const grey) noexcept
{
    for (std::uint32_t index = 0; index < 512; ++index) {
        surf_clut_[0][index] = normal[index];
        surf_clut_[1][index] = grey[index];
    }

    // The emulator rebuilds the whole cache here rather than only the half
    // in force, and the comment above its loop asks that the two not be
    // merged. They are not merged.
    for (int entry = 0; entry < 512; ++entry) {
        fix_pcache(entry);
    }
}

void PceVce::set_disabled_layer_colour(const std::uint32_t colour) noexcept
{
    disabled_layer_colour_ = colour;
}

void PceVce::set_show_horizontal_overscan(const bool show) noexcept
{
    show_horiz_os_ = show;
}

void PceVce::set_layer_enable_mask(const std::uint64_t mask) noexcept
{
    if (vdc_ != nullptr) {
        vdc_->set_layer_enable_mask(mask & 0x3);
    }
}

void PceVce::set_unlimited_sprites(const bool unlimited) noexcept
{
    if (vdc_ != nullptr) {
        vdc_->set_unlimited_sprites(unlimited);
    }
}

void PceVce::fix_pcache(const int entry) noexcept
{
    const std::uint32_t* const csl = surf_clut_[bw_ ? 1 : 0].data();

    // A palette entry whose low byte is zero is a colour zero, and colour
    // zero of every one of the sixteen sub-palettes shows the same value.
    if (!(entry & 0xFF)) {
        for (int index = 0; index < 16; ++index) {
            const std::size_t slot = static_cast<std::size_t>(
                (entry & 0x100) + (index << 4));
            color_table_cache_[slot]
                = csl[color_table_[static_cast<std::size_t>(entry & 0x100)]];
        }
    }

    if (!(entry & 0xF)) {
        return;
    }

    color_table_cache_[static_cast<std::size_t>(entry)]
        = csl[color_table_[static_cast<std::size_t>(entry)]];
}

void PceVce::set_control_register(const std::uint8_t value) noexcept
{
    if (((value & 0x80) >> 7) != (bw_ ? 1 : 0)) {
        bw_ = (value & 0x80) != 0;
        ++coverage_.monochrome_changes;
        for (int entry = 0; entry < 512; ++entry) {
            fix_pcache(entry);
        }
    }

    const bool lc263 = (value & 0x04) != 0;
    if (lc263 != lc263_) {
        ++coverage_.line_count_changes;
    }
    lc263_ = lc263;

    const unsigned dot_clock = value & 0x3U;
    if (dot_clock != dot_clock_) {
        ++coverage_.dot_clock_changes;
    }
    dot_clock_ = dot_clock;
    dot_clock_ratio_ = kDotClockRatios[dot_clock_];

    CR_ = value;

    report_scanline_info();
}

void PceVce::raise_irq(const bool raised) noexcept
{
    if (raised != irq_line_) {
        irq_line_ = raised;
        if (raised) {
            ++coverage_.irq_raises;
        } else {
            ++coverage_.irq_lowers;
        }
    }

    // Published on every call rather than on every change, because the
    // emulator's own edge is a pair of level-setting calls into the
    // processor's interrupt mask and it makes them unconditionally.
    if (irq_hook_ != nullptr) {
        irq_hook_(irq_context_, raised);
    }
}

void PceVce::irq_change_check() noexcept
{
    raise_irq(vdc_ != nullptr && vdc_->peek_irq());
}

void PceVce::report_scanline_info() noexcept
{
    if (vdc_ == nullptr || scanline_info_hook_ == nullptr) {
        return;
    }

    const std::uint32_t hsr = vdc_->get_register(kRegisterHsr);
    const std::uint32_t hdr = vdc_->get_register(kRegisterHdr);

    ScanlineInfo info;
    info.pulse = static_cast<int>((hsr >> 0) & 0x1F);
    info.start = static_cast<int>((hsr >> 8) & 0x7F);
    info.width = static_cast<int>((hdr >> 0) & 0x7F);
    info.end = static_cast<int>((hdr >> 8) & 0x7F);
    info.rate = static_cast<int>(dot_clock_);
    info.line = scanline_;
    info.restart = need_sl_reset_;

    scanline_info_hook_(scanline_info_context_, info);
}

void PceVce::reset() noexcept
{
    last_ts_ = 0;

    pixel_offset_ = 0;
    dot_clock_ = 0;
    dot_clock_ratio_ = kDotClockRatios[0];
    clock_divider_ = 0;

    ws_counter_ = 0;
    scanline_ = 0;
    scanline_out_ptr_ = nullptr;
    CR_ = 0;
    lc263_ = false;
    bw_ = false;

    color_table_cache_.fill(0);

    // The power-on palette is a checkerboard of black and white, which is
    // what the chip's memory reads as before anything writes it.
    for (int entry = 0; entry < 512; ++entry) {
        color_table_[static_cast<std::size_t>(entry)]
            = ((entry ^ (entry >> 3)) & 1) ? 0x000 : 0x1FF;
        fix_pcache(entry);
    }

    ctaddress_ = 0;

    hblank_ = true;
    vblank_ = true;

    need_sl_reset_ = false;

    hblank_counter_ = kHBlankClocks;
    // Thirty clocks longer than a vertical blank, which is the phase the
    // chip powers up in rather than a round number anyone chose.
    vblank_counter_ = kVBlankClocks + 30;

    if (vdc_ != nullptr) {
        child_event_[0] = vdc_->reset();
    }
    // The second controller's horizon is carried only so a savestate keeps
    // its shape. The emulator leaves it untouched on a machine with one
    // controller, where it is whatever the heap held; this holds a value the
    // loader's own clamp would accept.
    child_event_[1] = 1;

    if (fb_ != nullptr) {
        scanline_out_ptr_
            = &fb_[(scanline_ % kFieldLines) * pitch32_];
    }
}

void PceVce::reset_timestamp(const std::int32_t base) noexcept
{
    last_ts_ = base;
}

void PceVce::start_frame(
    Pixel* const framebuffer,
    const std::int32_t pitch,
    std::int32_t* const line_widths,
    const bool skip,
    const bool awesome_mode,
    const int scanline_start,
    const int scanline_end) noexcept
{
    frame_done_ = false;

    // Nothing else ever writes these two entries, so the emulator re-asserts
    // them every frame and so does this.
    color_table_cache_[0x200] = disabled_layer_colour_;
    color_table_cache_[0x300] = disabled_layer_colour_;

    awesome_ = awesome_mode;
    scanline_start_ = scanline_start;
    scanline_end_ = scanline_end;

    if (!skip) {
        for (int line = 0; line < kFieldLines; ++line) {
            line_widths[line] = 0;
        }

        pitch32_ = pitch;
        fb_ = framebuffer;
        line_widths_ = line_widths;
        scanline_out_ptr_ = &fb_[(scanline_ % kFieldLines) * pitch32_];
    } else {
        pitch32_ = 0;
        fb_ = nullptr;
        line_widths_ = nullptr;
        scanline_out_ptr_ = nullptr;
    }

    skipframe_ = skip;
    ++coverage_.frames;
}

void PceVce::start_partial() noexcept
{
    ws_counter_ = 0;
    ++coverage_.partials;
}

void PceVce::finish_partial() noexcept
{
    if (skipframe_ || line_widths_ == nullptr) {
        return;
    }

    // A line that was never drawn takes the width of the first line that
    // was. If no line was drawn at all the frame is a 256-pixel one, which
    // is a guess the emulator labels as such.
    std::int32_t fallback = 256;

    for (int line = 0; line < kFieldLines; ++line) {
        if (line_widths_[line] != 0) {
            fallback = line_widths_[line];
            break;
        }
    }

    for (int line = 0; line < kFieldLines; ++line) {
        if (line_widths_[line] == 0) {
            line_widths_[line] = fallback;
        }
    }
}

std::int32_t PceVce::calc_next_event() const noexcept
{
    std::int32_t next_event = hblank_counter_;

    if (next_event > vblank_counter_) {
        next_event = vblank_counter_;
    }

    if (next_event > cd_event_) {
        next_event = cd_event_;
    }

    next_event = std::min(
        next_event, child_event_[0] * dot_clock_ratio_ - clock_divider_);

    if (next_event < 1) {
        next_event = 1;
    }

    return next_event;
}

void PceVce::set_cd_event(
    const std::int32_t cycles,
    const std::int32_t time_behind) noexcept
{
    cd_event_ = cycles + time_behind;
}

std::int32_t PceVce::sync(const std::int32_t timestamp) noexcept
{
    const std::int32_t clocks = timestamp - last_ts_;

    cd_event_ -= clocks;
    if (cd_event_ <= 0) {
        ++coverage_.cd_events;
        cd_event_ = (cd_event_hook_ != nullptr)
            ? cd_event_hook_(
                  cd_event_context_, static_cast<std::uint32_t>(timestamp))
            : kNoCdEvent;
    }

    if (awesome_) {
        sync_sub<true>(clocks);
    } else {
        sync_sub<false>(clocks);
    }

    const std::int32_t next_event = calc_next_event();

    last_ts_ = timestamp;

    return next_event;
}

template <bool TA_AwesomeMode>
void PceVce::sync_sub(std::int32_t clocks) noexcept
{
    // The last line of the visible field, which is where the frame ends and
    // where the processor is asked to stop.
    const int last_line = kFirstVisibleLine
        + static_cast<int>(std::max(
            240U, static_cast<unsigned>(scanline_end_) + 1U));

    while (clocks > 0) {
        std::int32_t chunk_clocks = clocks;

        if (chunk_clocks > hblank_counter_) {
            chunk_clocks = hblank_counter_;
        }

        if (chunk_clocks > vblank_counter_) {
            chunk_clocks = vblank_counter_;
        }

        chunk_clocks = std::min(
            chunk_clocks,
            child_event_[0] * dot_clock_ratio_ - clock_divider_);

        if (chunk_clocks <= 0) {
            chunk_clocks = 1;
            ++coverage_.floored_chunks;
        }

        ++coverage_.chunks;

        clock_divider_ += chunk_clocks;
        const std::int32_t div_clocks = clock_divider_ / dot_clock_ratio_;
        clock_divider_ -= div_clocks * dot_clock_ratio_;

        child_event_[0] -= div_clocks;

        if (div_clocks > 0) {
            coverage_.dot_clocks += static_cast<std::uint64_t>(div_clocks);

            const bool skipline = (scanline_ < kFirstVisibleLine
                                      + scanline_start_)
                || (scanline_ > kFirstVisibleLine + scanline_end_);

            if (vdc_ != nullptr) {
                child_event_[0] = vdc_->run(
                    div_clocks, pixel_buffer_.data(), skipline);
            }

            if (skipline) {
                ++coverage_.off_screen_runs;
            } else if (scanline_out_ptr_ != nullptr) {
                // The one guard the emulator does not have. `VCE::SyncSub`
                // decides to draw from the visible range alone, so a frame
                // started with no framebuffer writes through a null pointer
                // here; nothing in this port starts one that way.
                for (std::int32_t index = 0; index < div_clocks; ++index) {
                    const std::uint32_t pix = color_table_cache_[
                        pixel_buffer_[static_cast<std::size_t>(index)]
                        & 0x3FF];

                    if constexpr (TA_AwesomeMode) {
                        for (std::int32_t copy = 0; copy < dot_clock_ratio_;
                             ++copy) {
                            scanline_out_ptr_[pixel_offset_ & 2047]
                                = static_cast<Pixel>(pix);
                            ++pixel_offset_;
                            ++coverage_.pixels;
                        }
                    } else {
                        scanline_out_ptr_[pixel_offset_ & 2047]
                            = static_cast<Pixel>(pix);
                        ++pixel_offset_;
                        ++coverage_.pixels;
                    }
                }
            }
        }

        clocks -= chunk_clocks;

        hblank_counter_ -= chunk_clocks;
        if (hblank_counter_ <= 0) {
            hblank_ = !hblank_;

            if (!hblank_) {
                if (need_sl_reset_) {
                    scanline_ = 0;
                    ++coverage_.field_restarts;
                    report_scanline_info();
                } else {
                    ++scanline_;
                    ++coverage_.scanlines;
                }

                if (scanline_ == last_line) {
                    frame_done_ = true;
                    ++coverage_.frames_completed;
                }

                if (scanline_ == last_line || scanline_ == kEarlyExitLine) {
                    ++coverage_.cpu_exits;
                    if (exit_hook_ != nullptr) {
                        exit_hook_(exit_context_);
                    }
                }

                scanline_out_ptr_ = (fb_ != nullptr)
                    ? &fb_[(scanline_ % kFieldLines) * pitch32_]
                    : nullptr;

                pixel_offset_ = 0;
                need_sl_reset_ = false;

                if (!skipframe_) {
                    int rect_x = 0;
                    int rect_w = 0;

                    if constexpr (TA_AwesomeMode) {
                        if (dot_clock_ >= 2) {
                            rect_x = 208;
                        } else if (dot_clock_ == 1) {
                            rect_x = 136;
                        } else {
                            rect_x = 128;
                        }
                        rect_w = 1024;

                        if (show_horiz_os_) {
                            rect_x -= 48;
                            rect_w += 96;
                        }
                    } else {
                        rect_x = kLineOrigins[show_horiz_os_ ? 1 : 0]
                                             [dot_clock_];
                        rect_w = kLineWidths[show_horiz_os_ ? 1 : 0]
                                            [dot_clock_];
                    }

                    // The visible picture is written from the left edge of
                    // the line, so the write cursor starts that far before
                    // zero and wraps into the buffer.
                    pixel_offset_ = (0 - rect_x) & 2047;
                    line_widths_[scanline_ % kFieldLines] = rect_w;
                    ++coverage_.lines_at_rate[dot_clock_];
                }
            }

            hblank_counter_ = hblank_ ? kHBlankClocks : kHDisplayClocks;

            if (vdc_ != nullptr) {
                child_event_[0] = vdc_->hsync(hblank_);
            }
        }

        vblank_counter_ -= chunk_clocks;
        if (vblank_counter_ <= 0) {
            vblank_ = !vblank_;
            vblank_counter_ = vblank_
                ? kVBlankClocks
                : ((lc263_ ? kField263Clocks : kField262Clocks)
                      - kVBlankClocks);

            if (vblank_) {
                ++coverage_.vertical_blanks;
            } else {
                need_sl_reset_ = true;
            }

            if (vdc_ != nullptr) {
                child_event_[0] = vdc_->vsync(vblank_);
            }
        }
    }
}

void PceVce::write(
    const std::uint32_t address,
    const std::uint8_t value) noexcept
{
    switch (address & 0x7) {
    case 0: {
        const unsigned old_dot_clock = dot_clock_;

        set_control_register(value);

        // The emulator's own comment calls this wrong and says a real fix
        // would change what `clock_divider` means. Reproduced as written.
        if (old_dot_clock != dot_clock_) {
            clock_divider_ = 0;
        }
        break;
    }

    case 2:
        ctaddress_ = static_cast<std::uint16_t>(
            (ctaddress_ & 0x100) | value);
        break;

    case 3:
        ctaddress_ = static_cast<std::uint16_t>(
            (ctaddress_ & 0x0FF) | ((value & 1) << 8));
        break;

    case 4: {
        const std::size_t entry = ctaddress_ & 0x1FF;
        color_table_[entry] = static_cast<std::uint16_t>(
            (color_table_[entry] & 0x100) | value);
        fix_pcache(static_cast<int>(entry));
        ++coverage_.palette_writes;
        break;
    }

    case 5: {
        const std::size_t entry = ctaddress_ & 0x1FF;
        color_table_[entry] = static_cast<std::uint16_t>(
            (color_table_[entry] & 0xFF) | ((value & 1) << 8));
        fix_pcache(static_cast<int>(entry));
        ctaddress_ = static_cast<std::uint16_t>((ctaddress_ + 1) & 0x1FF);
        ++coverage_.palette_writes;
        break;
    }

    default:
        break;
    }
}

std::uint8_t PceVce::read(const std::uint32_t address) noexcept
{
    // Every address the encoder does not decode reads back as an open bus
    // held high.
    std::uint8_t ret = 0xFF;

    switch (address & 0x7) {
    case 4:
        ret = static_cast<std::uint8_t>(color_table_[ctaddress_ & 0x1FF]);
        ++coverage_.palette_reads;
        break;

    case 5:
        ret = static_cast<std::uint8_t>(
            color_table_[ctaddress_ & 0x1FF] >> 8);
        ret &= 1;
        // The upper seven bits of the high half do not exist, so they read
        // back as the bus rather than as zero.
        ret |= 0xFE;
        ctaddress_ = static_cast<std::uint16_t>((ctaddress_ + 1) & 0x1FF);
        ++coverage_.palette_reads;
        break;

    default:
        break;
    }

    return ret;
}

void PceVce::write_vdc(
    const std::uint32_t address,
    const std::uint8_t value) noexcept
{
    if (vdc_ != nullptr) {
        vdc_->write(address & 0x1FFF, value);
    }
}

std::uint8_t PceVce::read_vdc(const std::uint32_t address) noexcept
{
    if (vdc_ == nullptr) {
        return 0xFF;
    }

    return vdc_->read(address, false);
}

void PceVce::write_vdc_st(
    const std::uint32_t address,
    const std::uint8_t value) noexcept
{
    if (vdc_ != nullptr) {
        vdc_->write(address, value);
    }
}

bool PceVce::wait_state(const std::int32_t vdc_cycles) noexcept
{
    bool granted = true;
    std::int32_t to_steal = 0;

    ++coverage_.wait_state_requests;

    if (vdc_cycles == -1) {
        // The controller asking to be stalled until whatever is next, which
        // is how it waits out a transfer it cannot interrupt.
        ++coverage_.event_stalls;
        to_steal = calc_next_event();
    } else {
        // Dot clocks to processor cycles: three master clocks a cycle, less
        // whatever of the current dot has already elapsed, rounded up.
        to_steal = ((vdc_cycles * dot_clock_ratio_ - clock_divider_) + 2) / 3;
    }

    if (to_steal <= 0) {
        to_steal = 1;
    }

    if ((to_steal + ws_counter_) > kWaitStateBudget) {
        to_steal = kWaitStateBudget - ws_counter_;

        if (to_steal < 0) {
            to_steal = 0;
        }

        granted = false;
        ++coverage_.wait_state_refusals;
    }

    if (to_steal > 0) {
        if (steal_hook_ != nullptr) {
            steal_hook_(steal_context_, to_steal);
        }
        ws_counter_ += to_steal;
        coverage_.stolen_cycles += static_cast<std::uint64_t>(to_steal);
    }

    return granted;
}

void PceVce::capture_state(State& state) const noexcept
{
    state.cr = CR_;
    state.ws_counter = ws_counter_;
    state.ctaddress = ctaddress_;
    state.color_table = color_table_;
    state.clock_divider = clock_divider_;
    state.child_event[0] = child_event_[0];
    state.child_event[1] = child_event_[1];
    state.scanline = scanline_;
    state.pixel_offset = pixel_offset_;
    state.hblank_counter = hblank_counter_;
    state.vblank_counter = vblank_counter_;
    state.hblank = hblank_;
    state.vblank = vblank_;
    state.need_sl_reset = need_sl_reset_;
}

void PceVce::apply_state(const State& state) noexcept
{
    CR_ = state.cr;
    ws_counter_ = state.ws_counter;
    ctaddress_ = state.ctaddress;
    color_table_ = state.color_table;
    clock_divider_ = state.clock_divider;
    child_event_[0] = state.child_event[0];
    child_event_[1] = state.child_event[1];
    scanline_ = state.scanline;
    pixel_offset_ = state.pixel_offset;
    hblank_counter_ = state.hblank_counter;
    vblank_counter_ = state.vblank_counter;
    hblank_ = state.hblank;
    vblank_ = state.vblank;
    need_sl_reset_ = state.need_sl_reset;

    // The control register is the one field a state carries that others are
    // derived from, so it is re-applied rather than trusted alongside them.
    set_control_register(CR_);

    ctaddress_ &= 0x1FF;

    clock_divider_ = static_cast<std::int32_t>(
        static_cast<std::uint32_t>(clock_divider_)
        % static_cast<std::uint32_t>(dot_clock_ratio_));

    if (scanline_ < 0) {
        scanline_ = 0;
    }

    if (hblank_counter_ < 1) {
        hblank_counter_ = 1;
    } else if (hblank_counter_ > 1365) {
        hblank_counter_ = 1365;
    }

    if (vblank_counter_ < 1) {
        vblank_counter_ = 1;
    } else if (vblank_counter_ > 400000) {
        vblank_counter_ = 400000;
    }

    if (cd_event_ < 1) {
        cd_event_ = 1;
    }

    for (int chip = 0; chip < 2; ++chip) {
        if (child_event_[chip] < 1) {
            child_event_[chip] = 1;
        } else if (child_event_[chip] > 1024) {
            child_event_[chip] = 1024;
        }
    }

    // The colour cache is not carried, so that a restored encoder cannot
    // disagree with its own palette.
    for (int entry = 0; entry < 512; ++entry) {
        fix_pcache(entry);
    }
}

}  // namespace devils_crush::video
