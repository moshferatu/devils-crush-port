// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/video/pce_vdc.hpp"

#include <cstring>

namespace devils_crush::video {

namespace {

// How far the write and read address registers step after each data-port
// access, selected by two bits of the control register.
constexpr std::array<std::uint16_t, 4> kVramIncrement = {1, 32, 64, 128};

// Background tile map dimensions, selected by the memory width register.
constexpr std::array<std::uint32_t, 4> kBatWidth = {32, 64, 128, 128};
constexpr std::array<std::uint32_t, 4> kBatWidthShift = {5, 6, 7, 7};
constexpr std::array<std::uint32_t, 2> kBatHeight = {32, 64};

constexpr std::uint32_t kSpritePriority = 0x00080;
constexpr std::uint32_t kSpriteHFlip = 0x00800;
constexpr std::uint32_t kSpriteVFlip = 0x08000;
// Not a register bit: the model tags the first table entry so the collision
// test can recognise it after the list has been reordered by depth.
constexpr std::uint32_t kSpriteZero = 0x10000;

constexpr std::array<std::uint32_t, 4> kSpriteHeight = {16, 32, 64, 64};
constexpr std::array<std::uint32_t, 4> kSpriteHeightNoMask = {
    ~0U, ~2U, ~6U, ~6U};
constexpr std::array<std::uint32_t, 2> kSpriteWidth = {16, 32};

// The interrupt is raised four clocks before the display period ends, and
// the phase machine splits the period to place it.
constexpr std::int32_t kCyclesBetweenRcrIrqAndHdwEnd = 4;

// Layer enable mask bits.
constexpr std::uint64_t kLayerBackground = 1;
constexpr std::uint64_t kLayerSprites = 2;

// Sets one byte of a 16-bit register, leaving the other alone.
inline void set_register_part(
    std::uint16_t& reg,
    std::uint8_t value,
    bool msb) noexcept
{
    if (msb) {
        reg = static_cast<std::uint16_t>(
            (reg & 0x00FF) | (static_cast<std::uint16_t>(value) << 8));
    } else {
        reg = static_cast<std::uint16_t>((reg & 0xFF00) | value);
    }
}

inline std::uint8_t get_register_part(std::uint16_t reg, bool msb) noexcept
{
    return static_cast<std::uint8_t>((reg >> (msb ? 8 : 0)) & 0xFF);
}

}  // namespace

PceVdc::PceVdc() noexcept = default;

void PceVdc::set_vram_size(const std::uint32_t words) noexcept
{
    if (words < 16 || words > kVramWords) {
        return;
    }
    if ((words & (words - 1)) != 0) {
        return;
    }
    vram_size_ = words;
}

void PceVdc::set_unlimited_sprites(const bool unlimited) noexcept
{
    unlimited_sprites_ = unlimited;
}

void PceVdc::set_layer_enable_mask(const std::uint64_t mask) noexcept
{
    layer_enable_ = mask;
}

void PceVdc::set_irq_hook(const IrqHook hook, void* const context) noexcept
{
    irq_hook_ = hook;
    irq_context_ = context;
}

void PceVdc::set_wait_state_hook(
    const WaitStateHook hook,
    void* const context) noexcept
{
    wait_state_hook_ = hook;
    wait_state_context_ = context;
}

void PceVdc::raise_irq() noexcept
{
    if (irq_hook_ != nullptr) {
        irq_hook_(irq_context_, true);
    }
}

void PceVdc::lower_irq() noexcept
{
    if (irq_hook_ != nullptr) {
        irq_hook_(irq_context_, false);
    }
}

std::uint16_t PceVdc::vram_increment() const noexcept
{
    return kVramIncrement[(CR_ >> 11) & 0x3];
}

// Video memory holds the two halves of a character's four bitplanes eight
// words apart. The renderer wants one palette index per pixel, so every
// write to a character word rebuilds that row of the cache.
void PceVdc::fix_tile_cache(const std::uint16_t address) noexcept
{
    const std::uint32_t char_name = address >> 4;
    const std::uint32_t y = address & 0x7;
    TileRow& row = tile_cache_[char_name][y];

    const std::uint32_t bitplane01 = VRAM_[y + char_name * 16];
    const std::uint32_t bitplane23 = VRAM_[y + 8 + char_name * 16];

    for (int x = 0; x < 8; ++x) {
        std::uint32_t raw_pixel = (bitplane01 >> x) & 1;
        raw_pixel |= ((bitplane01 >> (x + 8)) & 1) << 1;
        raw_pixel |= ((bitplane23 >> x) & 1) << 2;
        raw_pixel |= ((bitplane23 >> (x + 8)) & 1) << 3;
        row[static_cast<std::size_t>(7 - x)] =
            static_cast<std::uint8_t>(raw_pixel);
    }
}

// The soonest clock at which the device has something to do. The caller is
// expected to run no further than this in one call.
std::int32_t PceVdc::calc_next_event() const noexcept
{
    std::int32_t next_event = h_phase_counter_;

    if (sat_dma_counter_ > 0 && sat_dma_counter_ < next_event) {
        next_event = sat_dma_counter_;
    }

    if (sprite_cg_fetch_counter_ > 0 &&
        sprite_cg_fetch_counter_ < next_event) {
        next_event = sprite_cg_fetch_counter_;
    }

    if (dma_running_) {
        const std::int32_t next_vram_dma_event =
            ((static_cast<std::int32_t>(LENR_) + 1) * 4) -
            (dma_read_write_ ? 2 : 0) - vdma_cycle_counter_;

        if (next_vram_dma_event > 0 && next_vram_dma_event < next_event) {
            next_event = next_vram_dma_event;
        }
    }

    return next_event;
}

// The sprite attribute table copy takes 1024 clocks and happens at the
// transition out of the display period.
void PceVdc::run_sat_dma(
    std::int32_t cycles,
    const bool force_completion) noexcept
{
    if (force_completion) {
        cycles = sat_dma_counter_;
    }

    sat_dma_counter_ -= cycles;
    if (sat_dma_counter_ <= 0) {
        ++coverage_.sat_dma_transfers;
        if (DCR_ & 0x01) {
            status_ |= kStatusSatDmaDone;
            raise_irq();
        }
        check_and_commit_pending();
        burst_mode_ = true;
    }
}

// The video-memory block transfer moves one word every four clocks, as a
// read and a write two clocks apart.
void PceVdc::run_dma(
    const std::int32_t cycles,
    const bool force_completion) noexcept
{
    std::int32_t num_transfers = 0;

    if (force_completion) {
        vdma_cycle_counter_ = 0;
        // Enough half-transfers to drain the longest possible length.
        num_transfers = 65536 * 2;
    } else {
        vdma_cycle_counter_ += cycles;
        num_transfers = vdma_cycle_counter_ >> 1;
        vdma_cycle_counter_ -= num_transfers << 1;
    }

    while (num_transfers--) {
        if (!dma_read_write_) {
            dma_read_buffer_ = VRAM_[SOUR_];
        } else {
            if (DESR_ < vram_size_) {
                VRAM_[DESR_] = dma_read_buffer_;
                fix_tile_cache(DESR_);
            }
            ++coverage_.vram_dma_words;

            // The direction bits choose +1 or -1 for each address.
            SOUR_ = static_cast<std::uint16_t>(
                SOUR_ + ((((DCR_ & 0x4) >> 1) ^ 2) - 1));
            DESR_ = static_cast<std::uint16_t>(
                DESR_ + ((((DCR_ & 0x8) >> 2) ^ 2) - 1));
            LENR_ = static_cast<std::uint16_t>(LENR_ - 1);

            if (LENR_ == 0xFFFF) {
                // Cleared before committing, because a held port access is
                // only allowed through once the engine has stopped.
                dma_running_ = false;

                check_and_commit_pending();

                if (DCR_ & 0x02) {
                    status_ |= kStatusVramDmaDone;
                    raise_irq();
                }
                break;
            }
        }
        dma_read_write_ = !dma_read_write_;
    }
}

// Advances the raster counter by one line, which is also where the vertical
// phase machine turns over and where the raster compare fires.
void PceVdc::inc_rcr() noexcept
{
    if (need_bg_y_inc_) {
        need_bg_y_inc_ = false;
        if (rcr_count_ == 0) {
            bg_y_moo_ = BYR_;
        } else {
            ++bg_y_moo_;
        }
    }

    need_bg_y_inc_ = true;
    ++rcr_count_;
    ++coverage_.scanlines;

    --v_phase_counter_;

    if (v_phase_counter_ <= 0) {
        v_phase_ = (v_phase_ + 1) % kVphaseCount;
        switch (v_phase_) {
        case kVphaseVds:
            v_phase_counter_ = static_cast<std::int32_t>(vds_cache_) + 2;
            break;

        case kVphaseVdw:
            v_phase_counter_ = static_cast<std::int32_t>(vdw_cache_) + 1;
            rcr_count_ = 0;
            // With both layers off the device has nothing to fetch, and
            // hands its memory bandwidth to the transfer engines instead.
            burst_mode_ = !(CR_ & 0xC0);
            need_vblank_irq_test_ = true;
            need_sat_dma_test_ = true;

            if (!burst_mode_) {
                if (sat_dma_counter_ > 0) {
                    sat_dma_counter_ = 0;
                    check_and_commit_pending();
                }
                if (dma_running_) {
                    dma_running_ = false;
                    check_and_commit_pending();
                }
            }
            break;

        case kVphaseVcr:
            v_phase_counter_ = static_cast<std::int32_t>(vcr_cache_);
            break;

        case kVphaseVsw:
            v_phase_counter_ = static_cast<std::int32_t>(vsw_cache_) + 1;
            mwr_cache_ = MWR_;
            vds_cache_ = vds();
            vsw_cache_ = vsw();
            vdw_cache_ = vdw();
            vcr_cache_ = vcr();
            break;

        default:
            break;
        }
    }

    if (v_phase_ == kVphaseVdw && !burst_mode_) {
        fetch_sprite_data();
    }

    if (static_cast<int>(rcr_count_) ==
            (static_cast<int>(RCR_) - 0x40) &&
        (CR_ & 0x04)) {
        status_ |= kStatusRasterCompare;
        ++coverage_.raster_compares;
        raise_irq();
    }
}

void PceVdc::do_vblank_irq_test() noexcept
{
    if (CR_ & 0x08) {
        status_ |= kStatusVerticalBlank;
        ++coverage_.vertical_blanks;
        raise_irq();
    }
}

int PceVdc::time_from_hds_start_to_byr_latch() const noexcept
{
    int result = 1;
    if (hds_cache_ > 2) {
        result += static_cast<int>((hds_cache_ + 1) * 8) - 24 + 2;
    }
    return result;
}

int PceVdc::time_from_byr_latch_to_bxr_latch() const noexcept
{
    return (hds_cache_ > 2) ? 1 : 2;
}

// Start of a line's display period: the horizontal timing registers and the
// layer enables are latched here and hold for the whole line.
void PceVdc::hds_start() noexcept
{
    if (need_rcr_inc_) {
        inc_rcr();
        need_rcr_inc_ = false;
    }

    if (sprite_cg_fetch_counter_ > 0) {
        sprite_cg_fetch_counter_ = 0;
        check_and_commit_pending();
    }

    hsw_cache_ = hsw();
    hds_cache_ = hds();
    hdw_cache_ = hdw();
    hde_cache_ = hde();

    CR_cache_ = CR_;

    h_phase_ = kHphaseHds;
    h_phase_counter_ = time_from_hds_start_to_byr_latch();
}

std::int32_t PceVdc::hsync(const bool in_hblank) noexcept
{
    // When the device is configured to receive sync rather than generate it,
    // the encoder's edges drive the phase machine instead of these.
    if (ex() != 0) {
        in_exhsync_ = false;
        return calc_next_event();
    }
    in_exhsync_ = in_hblank;

    if (in_hblank) {
        mystery_counter_ = 48;
        mystery_phase_ = false;
    } else {
        h_phase_ = kHphaseHsw;
        h_phase_counter_ = 8;
        pixel_copy_count_ = 0;
    }

    return calc_next_event();
}

std::int32_t PceVdc::vsync(const bool in_vblank) noexcept
{
    if (ex() >= 0x2) {
        in_exvsync_ = false;
        return calc_next_event();
    }
    in_exvsync_ = in_vblank;

    if (in_vblank) {
        ++coverage_.fields;
        need_rcr_inc_ = false;
        need_bg_y_inc_ = false;
        mwr_cache_ = MWR_;

        vds_cache_ = vds();
        vsw_cache_ = vsw();
        vdw_cache_ = vdw();
        vcr_cache_ = vcr();

        // Re-syncing the phase here is why the first field after a reset is
        // not the steady state: it can cross a phase boundary the vertical
        // counters were not going to reach on their own.
        v_phase_ = kVphaseVsw;
        v_phase_counter_ = static_cast<std::int32_t>(vsw_cache_) + 1;
    }
    return calc_next_event();
}

std::int32_t PceVdc::run(
    std::int32_t clocks,
    std::uint16_t* pixels,
    const bool skip) noexcept
{
    while (clocks > 0) {
        std::int32_t chunk_clocks = clocks;

        if (chunk_clocks > h_phase_counter_) {
            chunk_clocks = h_phase_counter_;
        }

        if (sat_dma_counter_ > 0 && chunk_clocks > sat_dma_counter_) {
            chunk_clocks = sat_dma_counter_;
        }

        if (sprite_cg_fetch_counter_ > 0 &&
            chunk_clocks > sprite_cg_fetch_counter_) {
            chunk_clocks = sprite_cg_fetch_counter_;
        }

        if (mystery_counter_ > 0 && chunk_clocks > mystery_counter_) {
            chunk_clocks = mystery_counter_;
        }

        if (mystery_counter_ > 0) {
            mystery_counter_ -= chunk_clocks;
            if (mystery_counter_ <= 0) {
                mystery_phase_ = !mystery_phase_;
                if (mystery_phase_) {
                    mystery_counter_ = 16;
                } else {
                    check_and_commit_pending();
                }
            }
        }

        if (sprite_cg_fetch_counter_ > 0) {
            sprite_cg_fetch_counter_ -= chunk_clocks;
            if (sprite_cg_fetch_counter_ <= 0) {
                check_and_commit_pending();
            }
        }

        if (v_phase_ != kVphaseVdw) {
            if (need_sat_dma_test_) {
                need_sat_dma_test_ = false;
                if (satb_pending_ || (DCR_ & 0x10)) {
                    satb_pending_ = false;

                    sat_dma_counter_ = 1024;

                    if (DVSSR_ < vram_size_) {
                        std::uint32_t len = 256;
                        if (DVSSR_ > (vram_size_ - 0x100)) {
                            len = vram_size_ - DVSSR_;
                        }
                        std::memcpy(
                            SAT_.data(),
                            &VRAM_[DVSSR_],
                            len * sizeof(std::uint16_t));
                    }
                } else {
                    burst_mode_ = true;
                }
            }
        }

        if (dma_pending_ && burst_mode_) {
            dma_pending_ = false;
            dma_running_ = true;
            vdma_cycle_counter_ = 0;
            dma_read_write_ = false;
        }

        if (sat_dma_counter_ > 0) {
            run_sat_dma(chunk_clocks, false);
        } else if (dma_running_) {
            run_dma(chunk_clocks, false);
        }

        if (pixel_copy_count_ > 0) {
            if (!skip) {
                for (std::int32_t i = 0; i < chunk_clocks; ++i) {
                    pixels[i] = linebuf_[pixel_desu_ + i];
                }

                if (te() == 0x1) {
                    for (std::int32_t i = 0; i < chunk_clocks; ++i) {
                        pixels[i] |= kDisplayOutMask;
                    }
                }
            }

            pixel_desu_ += static_cast<std::uint32_t>(chunk_clocks);
            pixel_copy_count_ -= chunk_clocks;
        } else {
            std::uint16_t pix = 0x100;

            if (te() == 0x1) {
                if (h_phase_ != kHphaseHds && h_phase_ != kHphaseHdsPart2 &&
                    h_phase_ != kHphaseHdsPart3) {
                    pix |= kDisplayOutMask;
                }
            }

            if (h_phase_ == kHphaseHsw) {
                if (ex() >= 0x1) {
                    pix |= kHSyncOutMask;
                }
                if (te() >= 0x2) {
                    pix |= kDisplayOutMask;
                }
            }
            if (v_phase_ == kVphaseVsw && ex() >= 0x2) {
                pix |= kVSyncOutMask;
            }

            if (!(layer_enable_ & kLayerBackground)) {
                pix |= kBackgroundDisabledMask;
            }

            if (!skip) {
                for (std::int32_t i = 0; i < chunk_clocks; ++i) {
                    pixels[i] = pix;
                }
            }
        }

        h_phase_counter_ -= chunk_clocks;

        while (h_phase_counter_ <= 0) {
            h_phase_ = (h_phase_ + 1) % kHphaseCount;

            switch (h_phase_) {
            case kHphaseHds:
                hds_start();
                break;

            case kHphaseHdsPart2:
                h_phase_counter_ = time_from_byr_latch_to_bxr_latch();

                if (need_bg_y_inc_ && !in_exhsync_) {
                    need_bg_y_inc_ = false;

                    if (rcr_count_ == 0) {
                        bg_y_moo_ = BYR_;
                    } else {
                        ++bg_y_moo_;
                    }
                }
                bg_y_offset_ = bg_y_moo_;
                break;

            case kHphaseHdsPart3:
                h_phase_counter_ =
                    static_cast<std::int32_t>((hds_cache_ + 1) * 8) -
                    time_from_hds_start_to_byr_latch() -
                    time_from_byr_latch_to_bxr_latch();

                bg_x_offset_ = BXR_;
                break;

            case kHphaseHdw:
                need_rcr_inc_ = true;
                if (v_phase_ != kVphaseVdw && need_vblank_irq_test_) {
                    do_vblank_irq_test();
                    need_vblank_irq_test_ = false;
                }
                check_and_commit_pending();

                h_phase_counter_ =
                    static_cast<std::int32_t>((hdw_cache_ + 1) * 8) -
                    kCyclesBetweenRcrIrqAndHdwEnd;
                if (v_phase_ == kVphaseVdw) {
                    if (!burst_mode_) {
                        pixel_desu_ = 0;
                        pixel_copy_count_ =
                            static_cast<std::int32_t>((hdw_cache_ + 1) * 8);

                        // With the background off the line is a flat fill,
                        // and which fill depends on whether sprites are on.
                        if (!(CR_cache_ & 0x80)) {
                            std::uint16_t fill_val = 0;

                            if (!(CR_cache_ & 0xC0)) {
                                fill_val = 0x000;
                            } else {
                                fill_val = static_cast<std::uint16_t>(
                                    0x100 |
                                    ((layer_enable_ & kLayerBackground)
                                         ? 0
                                         : kBackgroundDisabledMask));
                            }

                            if (!(layer_enable_ & kLayerBackground)) {
                                fill_val |= kBackgroundDisabledMask;
                            }

                            for (std::int32_t i = 0; i < pixel_copy_count_;
                                 ++i) {
                                linebuf_[static_cast<std::size_t>(i)] =
                                    fill_val;
                            }
                        }

                        if (!skip) {
                            if (CR_cache_ & 0x80) {
                                draw_background(
                                    linebuf_.data(),
                                    (layer_enable_ & kLayerBackground) != 0);
                            }
                        }
                        if (CR_cache_ & 0x40) {
                            draw_sprites(
                                linebuf_.data(),
                                (layer_enable_ & kLayerSprites) != 0 &&
                                    !skip);
                        }
                    } else {
                        ++coverage_.burst_lines;
                    }
                }
                break;

            case kHphaseHdwFinal:
                if (need_rcr_inc_) {
                    inc_rcr();
                    need_rcr_inc_ = false;
                }
                h_phase_counter_ = kCyclesBetweenRcrIrqAndHdwEnd;
                break;

            case kHphaseHde:
                h_phase_counter_ =
                    static_cast<std::int32_t>((hde_cache_ + 1) * 8);
                break;

            case kHphaseHsw:
                h_phase_counter_ =
                    static_cast<std::int32_t>((hsw_cache_ + 1) * 8);
                break;

            default:
                break;
            }
        }
        pixels += chunk_clocks;
        clocks -= chunk_clocks;
    }

    return calc_next_event();
}

std::uint32_t PceVdc::display_width() const noexcept
{
    return (hdw() + 1) * 8;
}

void PceVdc::draw_background(
    std::uint16_t* const target,
    const bool enabled) noexcept
{
    ++coverage_.background_lines;

    const std::uint32_t bat_width = kBatWidth[(mwr_cache_ >> 4) & 3];
    const std::uint32_t bat_width_mask = bat_width - 1;
    const std::uint32_t bat_width_shift =
        kBatWidthShift[(mwr_cache_ >> 4) & 3];
    const std::uint32_t bat_height_mask =
        kBatHeight[(mwr_cache_ >> 6) & 1] - 1;

    const std::uint32_t start = 0;
    const std::uint32_t end = display_width();

    if (!enabled) {
        for (std::uint32_t x = start; x < end; ++x) {
            target[x] = kBackgroundDisabledMask;
        }
        return;
    }

    const std::uint32_t bat_y =
        ((bg_y_offset_ >> 3) & bat_height_mask) << bat_width_shift;
    const std::uint32_t first_end = start + 8 - (bg_x_offset_ & 7);
    std::uint32_t dohmask = 0xFFFFFFFF;

    // The two-plane background modes keep only half the bitplanes, and which
    // half is a further register bit.
    if ((mwr_cache_ & 0x3) == 0x3) {
        dohmask = (mwr_cache_ & 0x80) ? 0xCCCCCCCC : 0x33333333;
    }

    // Draw up to the first tile boundary a pixel at a time, so the rest of
    // the line can go eight at a time.
    for (std::uint32_t x = start; x < first_end; ++x) {
        const std::uint32_t bat_x = (bg_x_offset_ >> 3) & bat_width_mask;
        const std::uint16_t bat = VRAM_[bat_x | bat_y];
        const std::uint8_t pal_or =
            static_cast<std::uint8_t>((bat >> 8) & 0xF0);
        const std::uint32_t palette_index = ((bat >> 12) & 0x0F) << 4;

        const std::uint32_t raw_pixel =
            tile_cache_[bat & 0xFFF][bg_y_offset_ & 7][bg_x_offset_ & 0x7] &
            dohmask;
        target[x] = static_cast<std::uint16_t>(
            palette_index | raw_pixel | pal_or);

        ++bg_x_offset_;
    }

    std::uint32_t bat_boom = (bg_x_offset_ >> 3) & bat_width_mask;
    const std::uint32_t line_sub = bg_y_offset_ & 7;

    // Runs up to seven pixels past the end of the display, which is why the
    // line buffer is longer than the longest display.
    for (std::uint32_t x = first_end; x < end; x += 8) {
        const std::uint16_t bat = VRAM_[bat_boom | bat_y];
        const std::uint8_t pal_or =
            static_cast<std::uint8_t>((bat >> 8) & 0xF0);
        const TileRow& pix_lut = tile_cache_[bat & 0xFFF][line_sub];

        for (std::uint32_t i = 0; i < 8; ++i) {
            target[x + i] = static_cast<std::uint16_t>(
                (pix_lut[i] & dohmask) | pal_or);
        }

        bat_boom = (bat_boom + 1) & bat_width_mask;
        ++bg_x_offset_;
    }
}

// Builds the list of sprites intersecting the line about to be drawn. Called
// one line ahead of the drawing, which is why it reads the raster counter
// rather than being told which line it is for.
void PceVdc::fetch_sprite_data() noexcept
{
    active_sprites_ = 0;

    for (int i = 0; i < 64; ++i) {
        const std::int16_t y = static_cast<std::int16_t>(
            (SAT_[static_cast<std::size_t>(i) * 4 + 0] & 0x3FF) - 0x40);
        std::uint16_t x = static_cast<std::uint16_t>(
            SAT_[static_cast<std::size_t>(i) * 4 + 1] & 0x3FF);
        std::uint16_t no = static_cast<std::uint16_t>(
            (SAT_[static_cast<std::size_t>(i) * 4 + 2] >> 1) & 0x3FF);
        const std::uint16_t flags =
            SAT_[static_cast<std::size_t>(i) * 4 + 3];

        const std::uint32_t palette_index = (flags & 0xFU) << 4;
        const std::uint32_t height = kSpriteHeight[(flags >> 12) & 3];
        const std::uint32_t width = kSpriteWidth[(flags >> 8) & 1];

        if (static_cast<std::int32_t>(rcr_count_) < y ||
            static_cast<std::int32_t>(rcr_count_) >=
                static_cast<std::int32_t>(y + height)) {
            continue;
        }

        bool second_half = false;
        std::uint32_t y_offset = rcr_count_ - static_cast<std::uint32_t>(y);
        if (y_offset > height) {
            continue;
        }

        bool overflowed = false;
        for (;;) {
            if (active_sprites_ == 16) {
                if (CR_ & 0x2) {
                    status_ |= kStatusOverflow;
                    ++coverage_.sprite_overflows;
                    raise_irq();
                }
                if (!unlimited_sprites_) {
                    overflowed = true;
                    break;
                }
            }

            std::uint32_t row = y_offset;
            if (flags & kSpriteVFlip) {
                row = height - 1 - row;
            }

            std::uint16_t pattern = static_cast<std::uint16_t>(
                no & kSpriteHeightNoMask[(flags >> 12) & 3]);
            pattern = static_cast<std::uint16_t>(
                pattern | ((row & 0x30) >> 3));
            if (width == 32) {
                pattern = static_cast<std::uint16_t>(pattern & ~1U);
            }
            if (second_half) {
                pattern = static_cast<std::uint16_t>(pattern | 1U);
            }

            SpriteState& entry =
                sprite_list_[static_cast<std::size_t>(active_sprites_)];
            entry.flags = flags;

            // A horizontally flipped wide sprite swaps its two halves.
            if ((flags & kSpriteHFlip) && width == 32) {
                pattern = static_cast<std::uint16_t>(pattern ^ 1U);
            }
            entry.x = x;
            entry.palette_index = static_cast<std::uint8_t>(palette_index);

            const std::uint32_t base =
                static_cast<std::uint32_t>(pattern) * 64 + (row & 15);

            if ((mwr_cache_ & 0xC) == 4) {
                ++coverage_.narrow_colour_fetches;
                // Sixteen-colour sprite mode: one attribute bit picks which
                // pair of bitplanes is fetched, and the other pair is zero.
                const std::uint32_t plane_offset =
                    (SAT_[static_cast<std::size_t>(i) * 4 + 2] & 1) ? 32 : 0;
                entry.pattern_data[0] =
                    VRAM_[(base + plane_offset) & 0xFFFF];
                entry.pattern_data[1] =
                    VRAM_[(base + plane_offset + 16) & 0xFFFF];
                entry.pattern_data[2] = 0;
                entry.pattern_data[3] = 0;
            } else {
                entry.pattern_data[0] = VRAM_[base & 0xFFFF];
                entry.pattern_data[1] = VRAM_[(base + 16) & 0xFFFF];
                entry.pattern_data[2] = VRAM_[(base + 32) & 0xFFFF];
                entry.pattern_data[3] = VRAM_[(base + 48) & 0xFFFF];
            }

            entry.flags |= (i != 0) ? 0 : kSpriteZero;

            ++active_sprites_;
            ++coverage_.sprites_fetched;

            if (width == 32 && !second_half) {
                second_half = true;
                ++coverage_.wide_sprite_halves;
                x = static_cast<std::uint16_t>(x + 16);
                // Recomputed rather than carried, so a sprite that is both
                // flipped and wide gets the same row in both halves.
                y_offset = rcr_count_ - static_cast<std::uint32_t>(y);
                continue;
            }
            break;
        }

        if (overflowed) {
            break;
        }
    }

    sprite_cg_fetch_counter_ =
        ((active_sprites_ < 16) ? active_sprites_ : 16) * 4;
}

void PceVdc::draw_sprites(
    std::uint16_t* const target,
    const bool enabled) noexcept
{
    ++coverage_.sprite_lines;

    std::array<std::uint16_t, 1024> sprite_line_buf{};

    const std::uint32_t start = 0;
    const std::uint32_t end = display_width();

    for (std::uint32_t i = start; i < end; ++i) {
        sprite_line_buf[i] = 0;
    }

    // Back to front: the first sprite in the table wins, so it is drawn last.
    for (int i = active_sprites_ - 1; i >= 0; --i) {
        const SpriteState& entry =
            sprite_list_[static_cast<std::size_t>(i)];
        const std::int32_t pos =
            static_cast<std::int32_t>(entry.x - 0x20 + start);
        const std::uint32_t prio_or =
            (entry.flags & kSpritePriority) ? 0x200 : 0;
        const bool collision_test =
            (entry.flags & kSpriteZero) != 0 && (CR_ & 0x01) != 0;

        for (std::uint32_t x = 0; x < 16; ++x) {
            std::uint32_t rev_x = 15 - x;
            if (entry.flags & kSpriteHFlip) {
                rev_x = x;
            }

            std::uint32_t raw_pixel = (entry.pattern_data[0] >> rev_x) & 1;
            raw_pixel |= ((entry.pattern_data[1] >> rev_x) & 1) << 1;
            raw_pixel |= ((entry.pattern_data[2] >> rev_x) & 1) << 2;
            raw_pixel |= ((entry.pattern_data[3] >> rev_x) & 1) << 3;

            if (!raw_pixel) {
                continue;
            }

            const std::uint32_t pi = entry.palette_index | 0x100U;
            // Unsigned on purpose: a sprite hanging off the left edge wraps
            // to a huge value and is rejected by the same comparison that
            // rejects one hanging off the right.
            const std::uint32_t tx =
                static_cast<std::uint32_t>(pos) + x;

            if (tx >= end) {
                continue;
            }

            if (collision_test && (sprite_line_buf[tx] & 0xF)) {
                status_ |= kStatusCollision;
                ++coverage_.sprite_zero_hits;
                raise_irq();
            }
            sprite_line_buf[tx] =
                static_cast<std::uint16_t>(pi | raw_pixel | prio_or);
        }
    }

    if (enabled) {
        for (std::uint32_t x = start; x < end; ++x) {
            if (sprite_line_buf[x] & 0x0F) {
                if (!(target[x] & 0x0F) || (sprite_line_buf[x] & 0x200)) {
                    target[x] =
                        static_cast<std::uint16_t>(
                            sprite_line_buf[x] & kPixelOutMask);
                }
            }
        }
    }
    active_sprites_ = 0;
}

// A port access that arrives while video memory is occupied stalls the CPU.
// The loop only ends when the hook refuses, which is why a hook that always
// agrees hangs the machine rather than failing it.
void PceVdc::do_wait_states() noexcept
{
    while (is_busy()) {
        ++coverage_.wait_state_requests;

        if (wait_state_hook_ != nullptr &&
            wait_state_hook_(wait_state_context_, -1)) {
            continue;
        }

        ++coverage_.forced_completions;

        if (dma_running_) {
            run_dma(0, true);
        }

        if (sat_dma_counter_ > 0) {
            run_sat_dma(0, true);
        }

        if (mystery_phase_) {
            const bool backup = mystery_phase_;
            mystery_phase_ = false;
            check_and_commit_pending();
            mystery_phase_ = backup;
        }

        break;
    }
}

// A held port access reaches video memory as soon as nothing else is using
// it. Both halves are released together, and the read is serviced after the
// write so a read-back of the address just written sees the new value.
void PceVdc::check_and_commit_pending() noexcept
{
    if (sat_dma_counter_ <= 0 && !dma_running_ && !mystery_phase_) {
        if (pending_write_) {
            if (pending_write_addr_ < vram_size_) {
                VRAM_[pending_write_addr_] = pending_write_latch_;
                fix_tile_cache(pending_write_addr_);
            }
            ++coverage_.port_writes;
            pending_write_ = false;
        }

        if (pending_read_) {
            read_buffer_ = VRAM_[pending_read_addr_];
            ++coverage_.port_reads;
            pending_read_ = false;
        }
    }
}

std::uint8_t PceVdc::read(
    const std::uint32_t address,
    const bool peek) noexcept
{
    std::uint8_t ret = 0;
    const bool msb = (address & 1) != 0;

    switch (address & 0x3) {
    case 0x0:
        ret = static_cast<std::uint8_t>(
            status_ | (is_busy() ? kStatusBusy : 0x00));

        if (!peek) {
            // Reading the status register acknowledges every source at
            // once, which is the only way the interrupt line falls.
            status_ &= static_cast<std::uint8_t>(~0x3F);
            lower_irq();
        }
        break;

    case 0x2:
    case 0x3:
        if (!peek) {
            do_wait_states();
        }

        ret = get_register_part(read_buffer_, msb);

        if (select_ == 0x2 && msb && !peek) {
            // The high byte is what triggers the next word's fetch, so the
            // buffer always holds the word before the one being addressed.
            pending_read_ = true;
            pending_read_addr_ = MARR_;
            MARR_ = static_cast<std::uint16_t>(MARR_ + vram_increment());

            check_and_commit_pending();
        }
        break;

    default:
        break;
    }

    return ret;
}

void PceVdc::write(
    const std::uint32_t address,
    const std::uint8_t value) noexcept
{
    const bool msb = (address & 1) != 0;

    switch (address & 0x3) {
    case 0x0:
        select_ = value & 0x1F;
        break;

    case 0x2:
    case 0x3:
        switch (select_ & 0x1F) {
        case 0x00:
            set_register_part(MAWR_, value, msb);
            break;

        case 0x01:
            set_register_part(MARR_, value, msb);
            if (msb) {
                do_wait_states();

                pending_read_ = true;
                pending_read_addr_ = MARR_;
                MARR_ = static_cast<std::uint16_t>(
                    MARR_ + vram_increment());

                check_and_commit_pending();
            }
            break;

        case 0x02:
            if (!msb) {
                write_latch_ = value;
            } else {
                do_wait_states();

                pending_write_ = true;
                pending_write_addr_ = MAWR_;
                pending_write_latch_ = static_cast<std::uint16_t>(
                    write_latch_ | (value << 8));
                MAWR_ = static_cast<std::uint16_t>(
                    MAWR_ + vram_increment());

                check_and_commit_pending();
            }
            break;

        case 0x05:
            set_register_part(CR_, value, msb);
            break;

        case 0x06:
            set_register_part(RCR_, value, msb);
            RCR_ &= 0x3FF;
            break;

        case 0x07:
            set_register_part(BXR_, value, msb);
            BXR_ &= 0x3FF;
            break;

        case 0x08:
            set_register_part(BYR_, value, msb);
            BYR_ &= 0x1FF;
            // Latched on both halves, not just the high one: a game that
            // writes only the low byte still expects the scroll to move.
            bg_y_moo_ = BYR_;
            break;

        case 0x09:
            set_register_part(MWR_, value, msb);
            break;
        case 0x0A:
            set_register_part(HSR_, value, msb);
            break;
        case 0x0B:
            set_register_part(HDR_, value, msb);
            break;
        case 0x0C:
            set_register_part(VSR_, value, msb);
            break;
        case 0x0D:
            set_register_part(VDR_, value, msb);
            break;
        case 0x0E:
            set_register_part(VCR_, value, msb);
            break;
        case 0x0F:
            set_register_part(DCR_, value, msb);
            break;
        case 0x10:
            set_register_part(SOUR_, value, msb);
            break;
        case 0x11:
            set_register_part(DESR_, value, msb);
            break;

        case 0x12:
            set_register_part(LENR_, value, msb);
            if (msb) {
                // Requested here, but not started until the device is in
                // burst mode and can spare the memory bandwidth.
                dma_pending_ = true;
            }
            break;

        case 0x13:
            set_register_part(DVSSR_, value, msb);
            satb_pending_ = true;
            break;

        default:
            break;
        }
        break;

    default:
        break;
    }
}

std::int32_t PceVdc::reset() noexcept
{
    VRAM_.fill(0);
    SAT_.fill(0);
    sprite_list_ = {};

    // Video memory is zero, so every cached character row is zero too.
    for (Tile& tile : tile_cache_) {
        for (TileRow& row : tile) {
            row.fill(0);
        }
    }

    pending_read_ = false;
    pending_read_addr_ = 0xFFFF;
    read_buffer_ = 0xFFFF;
    write_latch_ = 0;

    pending_write_ = false;
    pending_write_addr_ = 0xFFFF;
    pending_write_latch_ = 0xFFFF;

    status_ = 0;

    HSR_ = 0;
    HDR_ = 0;
    VSR_ = 0;
    VDR_ = 0;
    VCR_ = 0;

    hsw_cache_ = hsw();
    hds_cache_ = hds();
    hdw_cache_ = hdw();
    hde_cache_ = hde();

    vds_cache_ = vds();
    vsw_cache_ = vsw();
    vdw_cache_ = vdw();
    vcr_cache_ = vcr();

    MAWR_ = 0;
    MARR_ = 0;

    CR_ = 0;
    CR_cache_ = 0;
    RCR_ = 0;
    BXR_ = 0;
    BYR_ = 0;
    MWR_ = 0;
    mwr_cache_ = 0;

    DCR_ = 0;
    SOUR_ = 0;
    DESR_ = 0;
    LENR_ = 0;
    DVSSR_ = 0;

    vdma_cycle_counter_ = 0;
    rcr_count_ = 0;

    dma_read_buffer_ = 0;
    dma_read_write_ = false;
    dma_running_ = false;
    dma_pending_ = false;
    satb_pending_ = false;
    burst_mode_ = false;

    bg_x_offset_ = 0;
    bg_y_offset_ = 0;
    bg_y_moo_ = 0;

    sat_dma_counter_ = 0;
    select_ = 0;

    need_rcr_inc_ = false;
    need_vblank_irq_test_ = false;
    need_sat_dma_test_ = false;
    need_bg_y_inc_ = false;

    h_phase_ = 0;
    v_phase_ = 0;
    h_phase_counter_ = 1;
    v_phase_counter_ = 1;

    sprite_cg_fetch_counter_ = 0;

    mystery_counter_ = 0;
    mystery_phase_ = false;

    pixel_desu_ = 0;
    pixel_copy_count_ = 0;
    active_sprites_ = 0;

    return calc_next_event();
}

std::uint16_t PceVdc::peek_vram(const std::uint16_t address) const noexcept
{
    return (address < vram_size_) ? VRAM_[address] : 0;
}

void PceVdc::poke_vram(
    const std::uint16_t address,
    const std::uint16_t value) noexcept
{
    if (address < vram_size_) {
        VRAM_[address] = value;
        fix_tile_cache(address);
    }
}

std::uint16_t PceVdc::peek_sat(const std::uint8_t address) const noexcept
{
    return SAT_[address];
}

void PceVdc::poke_sat(
    const std::uint8_t address,
    const std::uint16_t value) noexcept
{
    SAT_[address] = value;
}

std::uint32_t PceVdc::get_register(const Register id) const noexcept
{
    switch (id) {
    case Register::Select:
        return select_;
    case Register::Status:
        return status_;
    case Register::Mawr:
        return MAWR_;
    case Register::Marr:
        return MARR_;
    case Register::Cr:
        return CR_;
    case Register::Rcr:
        return RCR_;
    case Register::Bxr:
        return BXR_;
    case Register::Byr:
        return BYR_;
    case Register::Mwr:
        return MWR_;
    case Register::Hsr:
        return HSR_;
    case Register::Hdr:
        return HDR_;
    case Register::Vsr:
        return VSR_;
    case Register::Vdr:
        return VDR_;
    case Register::Vcr:
        return VCR_;
    case Register::Dcr:
        return DCR_;
    case Register::Sour:
        return SOUR_;
    case Register::Desr:
        return DESR_;
    case Register::Lenr:
        return LENR_;
    case Register::Dvssr:
        return DVSSR_;
    default:
        return 0xDEADBEEF;
    }
}

void PceVdc::set_register(
    const Register id,
    const std::uint32_t value) noexcept
{
    const std::uint16_t word = static_cast<std::uint16_t>(value & 0xFFFF);

    switch (id) {
    case Register::Status:
        status_ = static_cast<std::uint8_t>(value & 0x3F);
        break;
    case Register::Select:
        select_ = static_cast<std::uint8_t>(value & 0x1F);
        break;
    case Register::Mawr:
        MAWR_ = word;
        break;
    case Register::Marr:
        MARR_ = word;
        break;
    case Register::Cr:
        CR_ = word;
        break;
    case Register::Rcr:
        RCR_ = word & 0x3FF;
        break;
    case Register::Bxr:
        BXR_ = word & 0x3FF;
        break;
    case Register::Byr:
        BYR_ = word & 0x1FF;
        break;
    case Register::Mwr:
        MWR_ = word;
        break;
    case Register::Hsr:
        HSR_ = word;
        break;
    case Register::Hdr:
        HDR_ = word;
        break;
    case Register::Vsr:
        VSR_ = word;
        break;
    case Register::Vdr:
        VDR_ = word;
        break;
    case Register::Vcr:
        VCR_ = word;
        break;
    case Register::Dcr:
        DCR_ = word;
        break;
    case Register::Sour:
        SOUR_ = word;
        break;
    case Register::Desr:
        DESR_ = word;
        break;
    case Register::Lenr:
        LENR_ = word;
        break;
    case Register::Dvssr:
        DVSSR_ = word;
        break;
    default:
        break;
    }
}

void PceVdc::capture_state(State& state) const noexcept
{
    state.vram = VRAM_;
    state.sat = SAT_;
    state.sprite_list = sprite_list_;

    state.select = select_;
    state.mawr = MAWR_;
    state.marr = MARR_;
    state.cr = CR_;
    state.cr_cache = CR_cache_;
    state.rcr = RCR_;
    state.bxr = BXR_;
    state.byr = BYR_;
    state.mwr = MWR_;
    state.hsr = HSR_;
    state.hdr = HDR_;
    state.vsr = VSR_;
    state.vdr = VDR_;
    state.vcr = VCR_;
    state.dcr = DCR_;
    state.sour = SOUR_;
    state.desr = DESR_;
    state.lenr = LENR_;
    state.dvssr = DVSSR_;
    state.status = status_;

    state.pending_read = pending_read_;
    state.pending_read_addr = pending_read_addr_;
    state.read_buffer = read_buffer_;
    state.write_latch = write_latch_;
    state.pending_write = pending_write_;
    state.pending_write_addr = pending_write_addr_;
    state.pending_write_latch = pending_write_latch_;

    state.sat_dma_counter = sat_dma_counter_;
    state.vdma_cycle_counter = vdma_cycle_counter_;
    state.dma_read_buffer = dma_read_buffer_;
    state.dma_read_write = dma_read_write_;
    state.dma_running = dma_running_;
    state.dma_pending = dma_pending_;
    state.satb_pending = satb_pending_;
    state.burst_mode = burst_mode_;

    state.rcr_count = rcr_count_;
    state.bg_y_offset = bg_y_offset_;
    state.bg_x_offset = bg_x_offset_;
    state.bg_y_moo = bg_y_moo_;

    state.hsw_cache = hsw_cache_;
    state.hds_cache = hds_cache_;
    state.hdw_cache = hdw_cache_;
    state.hde_cache = hde_cache_;
    state.vds_cache = vds_cache_;
    state.vsw_cache = vsw_cache_;
    state.vdw_cache = vdw_cache_;
    state.vcr_cache = vcr_cache_;
    state.mwr_cache = mwr_cache_;

    state.need_rcr_inc = need_rcr_inc_;
    state.need_vblank_irq_test = need_vblank_irq_test_;
    state.need_sat_dma_test = need_sat_dma_test_;
    state.need_bg_y_inc = need_bg_y_inc_;

    state.h_phase = h_phase_;
    state.v_phase = v_phase_;
    state.h_phase_counter = h_phase_counter_;
    state.v_phase_counter = v_phase_counter_;

    state.sprite_cg_fetch_counter = sprite_cg_fetch_counter_;
    state.mystery_counter = mystery_counter_;
    state.mystery_phase = mystery_phase_;
    state.active_sprites = active_sprites_;

    state.in_exhsync = in_exhsync_;
    state.in_exvsync = in_exvsync_;
}

void PceVdc::apply_state(const State& state) noexcept
{
    VRAM_ = state.vram;
    SAT_ = state.sat;
    sprite_list_ = state.sprite_list;

    select_ = state.select;
    MAWR_ = state.mawr;
    MARR_ = state.marr;
    CR_ = state.cr;
    CR_cache_ = state.cr_cache;
    RCR_ = state.rcr;
    BXR_ = state.bxr;
    BYR_ = state.byr;
    MWR_ = state.mwr;
    HSR_ = state.hsr;
    HDR_ = state.hdr;
    VSR_ = state.vsr;
    VDR_ = state.vdr;
    VCR_ = state.vcr;
    DCR_ = state.dcr;
    SOUR_ = state.sour;
    DESR_ = state.desr;
    LENR_ = state.lenr;
    DVSSR_ = state.dvssr;
    status_ = state.status;

    pending_read_ = state.pending_read;
    pending_read_addr_ = state.pending_read_addr;
    read_buffer_ = state.read_buffer;
    write_latch_ = state.write_latch;
    pending_write_ = state.pending_write;
    pending_write_addr_ = state.pending_write_addr;
    pending_write_latch_ = state.pending_write_latch;

    sat_dma_counter_ = state.sat_dma_counter;
    // A savestate may hold any value here, but the transfer engine treats it
    // as a single bit of phase.
    vdma_cycle_counter_ = state.vdma_cycle_counter & 0x1;
    dma_read_buffer_ = state.dma_read_buffer;
    dma_read_write_ = state.dma_read_write;
    dma_running_ = state.dma_running;
    dma_pending_ = state.dma_pending;
    satb_pending_ = state.satb_pending;
    burst_mode_ = state.burst_mode;

    rcr_count_ = state.rcr_count;
    bg_y_offset_ = state.bg_y_offset;
    bg_x_offset_ = state.bg_x_offset;
    bg_y_moo_ = state.bg_y_moo;

    // The latched register fields, narrowed to the widths the registers
    // themselves have. The phase machine indexes tables with these.
    hsw_cache_ = state.hsw_cache & 0x1F;
    hds_cache_ = state.hds_cache & 0x7F;
    hdw_cache_ = state.hdw_cache & 0x7F;
    hde_cache_ = state.hde_cache & 0x7F;
    vsw_cache_ = state.vsw_cache & 0x1F;
    vds_cache_ = state.vds_cache & 0xFF;
    vdw_cache_ = state.vdw_cache & 0x1FF;
    vcr_cache_ = state.vcr_cache & 0xFF;
    mwr_cache_ = state.mwr_cache;

    need_rcr_inc_ = state.need_rcr_inc;
    need_vblank_irq_test_ = state.need_vblank_irq_test;
    need_sat_dma_test_ = state.need_sat_dma_test;
    need_bg_y_inc_ = state.need_bg_y_inc;

    h_phase_ = state.h_phase;
    v_phase_ = state.v_phase;
    // A horizontal phase counter of zero would never expire, and one longer
    // than the widest programmable phase would outlast the line.
    h_phase_counter_ = state.h_phase_counter;
    if (h_phase_counter_ < 1) {
        h_phase_counter_ = 1;
    } else if (h_phase_counter_ > (0x7F + 1) * 8) {
        h_phase_counter_ = (0x7F + 1) * 8;
    }
    v_phase_counter_ = state.v_phase_counter;

    sprite_cg_fetch_counter_ = state.sprite_cg_fetch_counter;
    mystery_counter_ = state.mystery_counter;
    mystery_phase_ = state.mystery_phase;
    active_sprites_ = state.active_sprites;

    in_exhsync_ = state.in_exhsync;
    in_exvsync_ = state.in_exvsync;

    // The character cache is not carried, so that a restored device cannot
    // disagree with its own memory.
    for (std::uint32_t address = 0; address < vram_size_; ++address) {
        fix_tile_cache(static_cast<std::uint16_t>(address));
    }
}

}  // namespace devils_crush::video
