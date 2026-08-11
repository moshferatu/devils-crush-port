// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstdint>

namespace devils_crush::video {

// The HuC6270 video display controller: 64K words of video RAM, a background
// plane assembled from a tile map and a character cache, 64 sprites of which
// sixteen may appear on a line, two block-transfer engines, and the raster
// phase machine that decides when any of it happens.
//
// This is a device model, not a transcript. It owns no CPU state and reaches
// nothing outside itself. The machine drives it with port accesses and with
// clocks, and it drives a pixel stream, an interrupt line, and -- the one
// place it reaches back -- a wait-state request when a port access arrives
// while the device is busy.
//
// Time is measured in VDC clocks, which are dot clocks: the colour encoder
// divides the master clock by four, three or two depending on the programmed
// dot rate and hands the quotient here. The device does not know the ratio
// and must not: the same register writes draw a different number of pixels
// at each rate, and choosing which is the encoder's job.
//
// Unlike the sound chip, this device cannot be driven by a timestamp. It has
// to be run on a schedule -- `run` interleaved with `hsync` and `vsync` at
// the moments the encoder's blanking counters expire, each call bounded by
// the event horizon the previous one returned. `run` may be called with any
// number of clocks up to that horizon; calling it with more silently runs the
// device past an event it was supposed to observe.
class PceVdc {
public:
    // Pixel output bits, as the device presents them to the colour encoder.
    // The low nine are the palette index; the rest are status the encoder
    // reads back out of the same word.
    static constexpr std::uint16_t kPixelOutMask = 0x01FF;
    // Set on a non-sprite pixel when the background layer has been disabled
    // through the layer mask, which is a debugging control and not a thing
    // the console can do.
    static constexpr std::uint16_t kBackgroundDisabledMask = 0x0200;
    // Only meaningful when the control register's EX bits ask the device to
    // output sync rather than receive it; otherwise always clear.
    static constexpr std::uint16_t kHSyncOutMask = 0x2000;
    static constexpr std::uint16_t kVSyncOutMask = 0x4000;
    // Active display, colourburst, or internal horizontal sync, depending on
    // the control register's TE bits.
    static constexpr std::uint16_t kDisplayOutMask = 0x8000;

    static constexpr std::uint8_t kStatusCollision = 0x01;
    static constexpr std::uint8_t kStatusOverflow = 0x02;
    static constexpr std::uint8_t kStatusRasterCompare = 0x04;
    static constexpr std::uint8_t kStatusSatDmaDone = 0x08;
    static constexpr std::uint8_t kStatusVramDmaDone = 0x10;
    static constexpr std::uint8_t kStatusVerticalBlank = 0x20;
    // Not a latched bit: read back live from whether a port access is still
    // waiting on video memory.
    static constexpr std::uint8_t kStatusBusy = 0x40;

    static constexpr std::uint32_t kVramWords = 65536;
    static constexpr std::uint32_t kTileCount = kVramWords / 16;
    static constexpr std::uint32_t kSatWords = 0x100;
    // Sixteen sprites a line, doubled because a 32-pixel-wide sprite is two
    // list entries, and doubled again is not needed -- but the unlimited
    // sprite option lifts the per-line limit, so the list must hold every
    // sprite the table can describe in both halves.
    static constexpr int kSpriteListSize = 64 * 2;
    // Wide enough for the longest programmable display plus the seven pixels
    // the background renderer is allowed to overrun by.
    static constexpr int kLineBufferSize = 1024 + 512;

    // Both hooks carry a context pointer. The emulator's own device takes
    // bare function pointers, which is why only one of it can usefully exist
    // at a time; there is no reason to inherit that.
    using IrqHook = void (*)(void* context, bool raised);
    // Returns true if the caller granted the stall and the device should ask
    // again, false if it must finish the blocking transfer itself. A hook
    // that always returns true never returns: the refusal is what ends the
    // loop, and the machine's escape is a per-frame budget of stolen cycles.
    using WaitStateHook = bool (*)(void* context, std::int32_t cycles);

    // Register selectors, in the order the debugger interface numbers them.
    enum class Register : unsigned {
        Mawr = 0,
        Marr,
        Cr,
        Rcr,
        Bxr,
        Byr,
        Mwr,
        Hsr,
        Hdr,
        Vsr,
        Vdr,
        Vcr,
        Dcr,
        Sour,
        Desr,
        Lenr,
        Dvssr,
        Select,
        Status,
        Count,
    };

    // Which paths the model has actually run. A device is proved against a
    // route census, and a route exercises only what the game asks for: a
    // pinball table redraws its background every frame and moves a lot of
    // sprites, but may never overflow a line, never collide sprite zero, and
    // never use the 16-colour sprite mode. Counting is how that gap is
    // measured rather than guessed at.
    struct Coverage {
        // Vertical syncs entered, and scanlines advanced.
        std::uint64_t fields = 0;
        std::uint64_t scanlines = 0;
        // Display lines that reached the renderers, split by layer.
        std::uint64_t background_lines = 0;
        std::uint64_t sprite_lines = 0;
        // Display lines skipped because the device was in burst mode, which
        // is what it enters when both layers are off.
        std::uint64_t burst_lines = 0;
        // Sprite list entries built, and how many of those were the second
        // half of a 32-pixel-wide sprite.
        std::uint64_t sprites_fetched = 0;
        std::uint64_t wide_sprite_halves = 0;
        // Sprite fetches that took the 16-colour path, where two of the four
        // bitplanes are left at zero.
        std::uint64_t narrow_colour_fetches = 0;
        // Lines that found a seventeenth sprite, and sprite-zero pixels that
        // landed on an already-occupied pixel.
        std::uint64_t sprite_overflows = 0;
        std::uint64_t sprite_zero_hits = 0;
        // Words moved by each transfer engine.
        std::uint64_t vram_dma_words = 0;
        std::uint64_t sat_dma_transfers = 0;
        // Port accesses that reached video memory, and accesses that arrived
        // while the device was busy.
        std::uint64_t port_writes = 0;
        std::uint64_t port_reads = 0;
        std::uint64_t wait_state_requests = 0;
        // Times the hook refused and the device completed a blocked transfer
        // on its own. This is the edge the machine's cycle budget exists to
        // reach, and a route that never reaches it has not tested it.
        std::uint64_t forced_completions = 0;
        // Interrupt sources raised, counted where they are raised.
        std::uint64_t raster_compares = 0;
        std::uint64_t vertical_blanks = 0;
    };

    PceVdc() noexcept;

    // The size is in 16-bit words and must be a power of two no larger than
    // 65536. Reset afterwards; changing it under a running device leaves the
    // address masks disagreeing with the phase machine.
    void set_vram_size(std::uint32_t words) noexcept;
    // Lifts the sixteen-sprites-per-line limit. Off by default, and turning
    // it on is a deliberate departure from the console.
    void set_unlimited_sprites(bool unlimited) noexcept;
    // Bit 0 enables the background, bit 1 the sprites. A debugging control.
    void set_layer_enable_mask(std::uint64_t mask) noexcept;

    void set_irq_hook(IrqHook hook, void* context) noexcept;
    void set_wait_state_hook(WaitStateHook hook, void* context) noexcept;

    // Returns the first clock count at which the device has something to do.
    [[nodiscard]] std::int32_t reset() noexcept;

    // Port accesses. The address's low two bits select status, nothing, or
    // the data port; bit 0 also picks which half of a 16-bit register the
    // byte belongs to.
    void write(std::uint32_t address, std::uint8_t value) noexcept;
    std::uint8_t read(std::uint32_t address, bool peek = false) noexcept;

    // Runs the device for `clocks` dot clocks, writing one output word per
    // clock into `pixels` unless `skip` is set, and returns the next event
    // horizon. `pixels` must have room for `clocks` words.
    std::int32_t run(
        std::int32_t clocks,
        std::uint16_t* pixels,
        bool skip) noexcept;
    // Told by the encoder when its blanking counters flip.
    std::int32_t hsync(bool in_hblank) noexcept;
    std::int32_t vsync(bool in_vblank) noexcept;

    [[nodiscard]] std::uint16_t peek_vram(
        std::uint16_t address) const noexcept;
    void poke_vram(std::uint16_t address, std::uint16_t value) noexcept;
    [[nodiscard]] std::uint16_t peek_sat(std::uint8_t address) const noexcept;
    void poke_sat(std::uint8_t address, std::uint16_t value) noexcept;

    [[nodiscard]] std::uint32_t get_register(Register id) const noexcept;
    void set_register(Register id, std::uint32_t value) noexcept;

    // Whether any interrupt source is currently latched. The interrupt line
    // is the disjunction of the status bits, not an edge, so the machine can
    // re-derive it at any time instead of tracking the hook's argument.
    [[nodiscard]] bool peek_irq() const noexcept
    {
        return (status_ & 0x3F) != 0;
    }

    [[nodiscard]] const Coverage& coverage() const noexcept
    {
        return coverage_;
    }

private:
    enum HorizontalPhase : int {
        kHphaseHds = 0,
        kHphaseHdsPart2,
        kHphaseHdsPart3,
        kHphaseHdw,
        kHphaseHdwFinal,
        kHphaseHde,
        kHphaseHsw,
        kHphaseCount,
    };

    enum VerticalPhase : int {
        kVphaseVds = 0,
        kVphaseVdw,
        kVphaseVcr,
        kVphaseVsw,
        kVphaseCount,
    };

    struct SpriteEntry {
        std::uint32_t x = 0;
        std::uint32_t flags = 0;
        std::uint8_t palette_index = 0;
        std::array<std::uint16_t, 4> pattern_data{};
    };

    using TileRow = std::array<std::uint8_t, 8>;
    using Tile = std::array<TileRow, 8>;

    void fix_tile_cache(std::uint16_t address) noexcept;
    void raise_irq() noexcept;
    void lower_irq() noexcept;

    [[nodiscard]] std::int32_t calc_next_event() const noexcept;

    void run_dma(std::int32_t cycles, bool force_completion) noexcept;
    void run_sat_dma(std::int32_t cycles, bool force_completion) noexcept;
    void do_wait_states() noexcept;
    void check_and_commit_pending() noexcept;

    void inc_rcr() noexcept;
    void do_vblank_irq_test() noexcept;
    void hds_start() noexcept;

    [[nodiscard]] int time_from_hds_start_to_byr_latch() const noexcept;
    [[nodiscard]] int time_from_byr_latch_to_bxr_latch() const noexcept;

    [[nodiscard]] std::uint32_t display_width() const noexcept;
    void draw_background(std::uint16_t* target, bool enabled) noexcept;
    void draw_sprites(std::uint16_t* target, bool enabled) noexcept;
    void fetch_sprite_data() noexcept;

    // Register field accessors, named for the datasheet's fields.
    [[nodiscard]] std::uint32_t hsw() const noexcept { return HSR_ & 0x1F; }
    [[nodiscard]] std::uint32_t hds() const noexcept
    {
        return (HSR_ >> 8) & 0x7F;
    }
    [[nodiscard]] std::uint32_t hdw() const noexcept { return HDR_ & 0x7F; }
    [[nodiscard]] std::uint32_t hde() const noexcept
    {
        return (HDR_ >> 8) & 0x7F;
    }
    [[nodiscard]] std::uint32_t vsw() const noexcept { return VSR_ & 0x1F; }
    [[nodiscard]] std::uint32_t vds() const noexcept
    {
        return (VSR_ >> 8) & 0xFF;
    }
    [[nodiscard]] std::uint32_t vdw() const noexcept { return VDR_ & 0x1FF; }
    [[nodiscard]] std::uint32_t vcr() const noexcept { return VCR_ & 0xFF; }
    // External sync select and display-terminal select, both in CR.
    [[nodiscard]] unsigned ex() const noexcept { return (CR_ >> 4) & 0x3; }
    [[nodiscard]] unsigned te() const noexcept { return (CR_ >> 8) & 0x3; }

    [[nodiscard]] bool is_busy() const noexcept
    {
        return pending_read_ || pending_write_;
    }

    [[nodiscard]] std::uint16_t vram_increment() const noexcept;

    IrqHook irq_hook_ = nullptr;
    void* irq_context_ = nullptr;
    WaitStateHook wait_state_hook_ = nullptr;
    void* wait_state_context_ = nullptr;

    std::uint32_t vram_size_ = kVramWords;

    std::uint8_t select_ = 0;
    std::uint16_t MAWR_ = 0;
    std::uint16_t MARR_ = 0;
    std::uint16_t CR_ = 0;
    // Latched at the start of each line's display period, so a mid-line
    // write to the enables does not take effect until the next line.
    std::uint16_t CR_cache_ = 0;
    std::uint16_t RCR_ = 0;
    std::uint16_t BXR_ = 0;
    std::uint16_t BYR_ = 0;
    std::uint16_t MWR_ = 0;
    std::uint16_t HSR_ = 0;
    std::uint16_t HDR_ = 0;
    std::uint16_t VSR_ = 0;
    std::uint16_t VDR_ = 0;
    std::uint16_t VCR_ = 0;
    std::uint16_t DCR_ = 0;
    std::uint16_t SOUR_ = 0;
    std::uint16_t DESR_ = 0;
    std::uint16_t LENR_ = 0;
    std::uint16_t DVSSR_ = 0;

    std::uint8_t status_ = 0;

    bool pending_read_ = false;
    std::uint16_t pending_read_addr_ = 0xFFFF;
    std::uint16_t read_buffer_ = 0xFFFF;

    std::uint8_t write_latch_ = 0;
    bool pending_write_ = false;
    std::uint16_t pending_write_addr_ = 0xFFFF;
    std::uint16_t pending_write_latch_ = 0xFFFF;

    std::int32_t sat_dma_counter_ = 0;
    std::int32_t vdma_cycle_counter_ = 0;
    std::uint16_t dma_read_buffer_ = 0;
    bool dma_read_write_ = false;
    bool dma_running_ = false;
    bool dma_pending_ = false;
    bool satb_pending_ = false;
    bool burst_mode_ = false;

    std::uint32_t rcr_count_ = 0;
    std::uint32_t bg_y_offset_ = 0;
    std::uint32_t bg_x_offset_ = 0;
    std::uint32_t bg_y_moo_ = 0;

    std::uint32_t hsw_cache_ = 0;
    std::uint32_t hds_cache_ = 0;
    std::uint32_t hdw_cache_ = 0;
    std::uint32_t hde_cache_ = 0;
    std::uint32_t vds_cache_ = 0;
    std::uint32_t vsw_cache_ = 0;
    std::uint32_t vdw_cache_ = 0;
    std::uint32_t vcr_cache_ = 0;
    std::uint16_t mwr_cache_ = 0;

    bool need_rcr_inc_ = false;
    bool need_vblank_irq_test_ = false;
    bool need_sat_dma_test_ = false;
    bool need_bg_y_inc_ = false;

    int h_phase_ = 0;
    int v_phase_ = 0;
    std::int32_t h_phase_counter_ = 1;
    std::int32_t v_phase_counter_ = 1;

    std::int32_t sprite_cg_fetch_counter_ = 0;

    // An unexplained delay the device takes on entering horizontal sync,
    // during which a pending port access is held. Named for what is known
    // about it, which is nothing beyond the two constants.
    std::int32_t mystery_counter_ = 0;
    bool mystery_phase_ = false;

    bool in_exhsync_ = false;
    bool in_exvsync_ = false;

    std::uint32_t pixel_desu_ = 0;
    std::int32_t pixel_copy_count_ = 0;
    std::uint64_t layer_enable_ = ~0ULL;
    bool unlimited_sprites_ = false;

    int active_sprites_ = 0;

    Coverage coverage_{};

    std::array<std::uint16_t, kSatWords> SAT_{};
    std::array<std::uint16_t, kVramWords> VRAM_{};
    std::array<Tile, kTileCount> tile_cache_{};
    std::array<std::uint16_t, kLineBufferSize> linebuf_{};
    std::array<SpriteEntry, kSpriteListSize> sprite_list_{};
};

}  // namespace devils_crush::video
