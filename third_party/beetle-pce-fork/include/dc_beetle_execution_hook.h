// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dc_beetle_cpu_state {
    uint16_t logical_pc;
    uint8_t physical_bank;
    uint8_t opcode;
    uint8_t accumulator;
    uint8_t x;
    uint8_t y;
    uint8_t stack_pointer;
    uint8_t status;
    uint8_t mpr[8];
    uint32_t master_cycle;
    uint64_t absolute_master_cycle;
    uint32_t master_cycles_until_user_event;
    uint32_t master_cycles_until_timer_interrupt;
} dc_beetle_cpu_state;

typedef struct dc_beetle_memory_write {
    uint16_t logical_address;
    uint8_t physical_bank;
    uint8_t value;
    uint16_t instruction_logical_pc;
    uint8_t instruction_physical_bank;
    uint32_t master_cycle;
    uint64_t absolute_master_cycle;
} dc_beetle_memory_write;

#define DC_BEETLE_MAX_REPLACEMENT_WRITES 512
#define DC_BEETLE_MAX_REPLACEMENT_CHECKPOINTS 2048

// How many checkpointed transcripts may be suspended at once. A transcript is
// suspended for as long as an interrupt asserted inside it is being serviced,
// and the handler is ordinary 65C02 code that may itself be a replaced
// routine. The HuC6280 has three interrupt sources, so a foreground transcript
// plus a TIRQ, IRQ1, and IRQ2 handler is four transcripts live at once and
// three of them suspended. The state-census route reaches three. Four is that
// bound with a level to spare, and the core declines a replacement rather than
// exceed it, so overflow costs a native call and never correctness.
#define DC_BEETLE_MAX_REPLACEMENT_DEPTH 4

typedef struct dc_beetle_replacement_write {
    uint16_t logical_address;
    uint8_t value;
    uint16_t instruction_logical_pc;
    uint8_t instruction_physical_bank;
    uint32_t master_cycle_offset;
} dc_beetle_replacement_write;

typedef struct dc_beetle_replacement_checkpoint {
    uint32_t interrupt_sample_master_cycle_offset;
    uint32_t master_cycle_offset;
    uint16_t instruction_logical_pc;
    uint8_t instruction_physical_bank;
    uint16_t resume_logical_pc;
    uint8_t accumulator;
    uint8_t x;
    uint8_t y;
    uint8_t stack_pointer;
    uint8_t status;
    uint8_t mpr[8];
} dc_beetle_replacement_checkpoint;

typedef struct dc_beetle_replacement_result {
    uint8_t handled;
    uint16_t resume_logical_pc;
    uint8_t accumulator;
    uint8_t x;
    uint8_t y;
    uint8_t stack_pointer;
    uint8_t status;
    uint8_t mpr[8];
    uint32_t body_master_cycles;
    uint32_t write_count;
    const dc_beetle_replacement_write* writes;
    uint32_t checkpoint_count;
    const dc_beetle_replacement_checkpoint* checkpoints;
} dc_beetle_replacement_result;

typedef void (*dc_beetle_instruction_hook)(
    const dc_beetle_cpu_state* state,
    void* user_data);
typedef void (*dc_beetle_memory_write_hook)(
    const dc_beetle_memory_write* write,
    void* user_data);
typedef void (*dc_beetle_replacement_hook)(
    const dc_beetle_cpu_state* state,
    dc_beetle_replacement_result* result,
    void* user_data);

#define DC_BEETLE_PSG_CHANNEL_COUNT 6
#define DC_BEETLE_PSG_WAVEFORM_LENGTH 32

// Every PSG field the emulator's savestate carries, in its order. The device
// model owns the live values while it is installed, so this is how they reach
// and return from the emulator's serializer without changing its format.
// Deliberately excludes the per-channel timestamps and previous-sample
// latches, which the emulator also omits.
typedef struct dc_beetle_psg_channel_state {
    int32_t counter;
    uint16_t frequency;
    uint8_t control;
    uint8_t balance;
    uint8_t waveform[DC_BEETLE_PSG_WAVEFORM_LENGTH];
    uint8_t waveform_index;
    uint8_t dda;
    uint8_t noisectrl;
    int32_t noisecount;
    uint32_t lfsr;
    int32_t vl[2];
} dc_beetle_psg_channel_state;

typedef struct dc_beetle_psg_state {
    dc_beetle_psg_channel_state channels[DC_BEETLE_PSG_CHANNEL_COUNT];
    uint8_t select;
    uint8_t global_balance;
    uint8_t lfo_frequency;
    uint8_t lfo_control;
    int32_t volume_update_counter;
    int32_t volume_update_which;
    int32_t volume_update_latch;
    uint8_t volume_update_pending;
} dc_beetle_psg_state;

// Which sound chip the emulator resolved from its settings. The two revisions
// differ in whether waveform amplitudes are centred, which scales every
// accumulated sample by two, so a model that assumes the wrong one is audibly
// wrong rather than subtly wrong.
#define DC_BEETLE_PSG_REVISION_HUC6280 0
#define DC_BEETLE_PSG_REVISION_HUC6280A 1

// A replacement for the emulator's PSG. While one is installed the emulator's
// own PSG is dormant: it receives no writes, no time and no reset, and is kept
// only as the shuttle its savestate format is written through. Timestamps are
// in PSG clocks, master clocks divided by three, and the two output buffers
// are the emulator's band-limited delta accumulators -- 65,536 positions plus
// 32 of overflow, one position per twelve master clocks.
typedef struct dc_beetle_psg_model {
    void* user_data;
    // The emulator reports the device it built, so the model never has to
    // guess the revision from the same settings a second time.
    void (*configure)(
        void* user_data,
        int32_t* left,
        int32_t* right,
        int revision);
    void (*power)(void* user_data, int32_t timestamp);
    void (*write)(
        void* user_data,
        int32_t timestamp,
        uint8_t address,
        uint8_t value);
    void (*update)(void* user_data, int32_t timestamp);
    void (*reset_timestamp)(void* user_data, int32_t base);
    void (*capture_state)(void* user_data, dc_beetle_psg_state* state);
    void (*apply_state)(void* user_data, const dc_beetle_psg_state* state);
} dc_beetle_psg_model;

#define DC_BEETLE_VDC_VRAM_WORD_COUNT 65536
#define DC_BEETLE_VDC_SAT_WORD_COUNT 256
#define DC_BEETLE_VDC_SPRITE_LIST_LENGTH 128

typedef struct dc_beetle_vdc_sprite_state {
    uint32_t x;
    uint32_t flags;
    uint8_t palette_index;
    uint16_t pattern_data[4];
} dc_beetle_vdc_sprite_state;

// Every VDC field the emulator's savestate carries, in its order. Excludes
// the character cache, which the emulator rebuilds from video memory on load
// and a model must rebuild too, and excludes the memory size and the two
// debugging controls, which belong to whoever built the device.
//
// This is 128 KiB of video memory and change, so it is passed by pointer and
// allocated by the caller rather than left on a stack.
typedef struct dc_beetle_vdc_state {
    uint16_t vram[DC_BEETLE_VDC_VRAM_WORD_COUNT];
    uint16_t sat[DC_BEETLE_VDC_SAT_WORD_COUNT];
    dc_beetle_vdc_sprite_state sprite_list[DC_BEETLE_VDC_SPRITE_LIST_LENGTH];

    uint8_t select;
    uint16_t mawr;
    uint16_t marr;
    uint16_t cr;
    uint16_t cr_cache;
    uint16_t rcr;
    uint16_t bxr;
    uint16_t byr;
    uint16_t mwr;
    uint16_t hsr;
    uint16_t hdr;
    uint16_t vsr;
    uint16_t vdr;
    uint16_t vcr;
    uint16_t dcr;
    uint16_t sour;
    uint16_t desr;
    uint16_t lenr;
    uint16_t dvssr;
    uint8_t status;

    uint16_t pending_read_addr;
    uint16_t read_buffer;
    uint8_t write_latch;
    uint16_t pending_write_addr;
    uint16_t pending_write_latch;
    uint8_t pending_read;
    uint8_t pending_write;

    int32_t sat_dma_counter;
    int32_t vdma_cycle_counter;
    uint16_t dma_read_buffer;
    uint8_t dma_read_write;
    uint8_t dma_running;
    uint8_t dma_pending;
    uint8_t satb_pending;
    uint8_t burst_mode;

    uint32_t rcr_count;
    uint32_t bg_y_offset;
    uint32_t bg_x_offset;
    uint32_t bg_y_moo;

    uint32_t hsw_cache;
    uint32_t hds_cache;
    uint32_t hdw_cache;
    uint32_t hde_cache;
    uint32_t vds_cache;
    uint32_t vsw_cache;
    uint32_t vdw_cache;
    uint32_t vcr_cache;
    uint16_t mwr_cache;

    uint8_t need_rcr_inc;
    uint8_t need_vblank_irq_test;
    uint8_t need_sat_dma_test;
    uint8_t need_bg_y_inc;

    int32_t h_phase;
    int32_t v_phase;
    int32_t h_phase_counter;
    int32_t v_phase_counter;

    int32_t sprite_cg_fetch_counter;
    int32_t mystery_counter;
    uint8_t mystery_phase;
    int32_t active_sprites;

    uint8_t in_exhsync;
    uint8_t in_exvsync;
} dc_beetle_vdc_state;

// A replacement for the emulator's video display controller. While one is
// installed the emulator's own `vdc[0]` is dormant: it receives no port
// accesses, no clocks and no reset, and is kept only as the shape its
// savestate format is written through. The second chip of a SuperGrafx is
// never routed, so a machine with two of them keeps the emulator's for the
// one the model does not answer for.
//
// The device is driven, not timestamped. `run` advances it by dot clocks and
// returns the next event horizon; `hsync` and `vsync` are called when the
// colour encoder's blanking counters expire, and also return a horizon. The
// schedule stays in the emulator, which is what makes this a device seam
// rather than a video seam.
//
// The two edges the device reaches back through -- the interrupt line and
// the wait-state stall -- are not part of this structure. They run into
// machine state a model does not have, so the boundary exports them as
// functions for the model to call instead: `dc_beetle_vdc_irq_change` and
// `dc_beetle_vdc_wait_state`.
typedef struct dc_beetle_vdc_model {
    void* user_data;
    // `vram_size` is in 16-bit words and is 32768 on a stock PC Engine.
    void (*configure)(void* user_data, uint32_t vram_size);
    int32_t (*reset)(void* user_data);
    int32_t (*run)(
        void* user_data,
        int32_t clocks,
        uint16_t* pixels,
        int skip);
    int32_t (*hsync)(void* user_data, int in_hblank);
    int32_t (*vsync)(void* user_data, int in_vblank);
    // `next_event` is the emulator's scheduling horizon, passed because
    // `VDC::Read` and `VDC::Write` take it -- but neither of those bodies
    // reads or assigns it, so a model that writes through this pointer is
    // departing from the reference rather than matching it.
    void (*write)(
        void* user_data,
        uint32_t address,
        uint8_t value,
        int32_t* next_event);
    uint8_t (*read)(
        void* user_data,
        uint32_t address,
        int32_t* next_event,
        int peek);
    int (*peek_irq)(void* user_data);
    uint32_t (*get_register)(void* user_data, unsigned id);
    uint16_t (*peek_vram)(void* user_data, uint16_t address);
    uint16_t (*peek_sat)(void* user_data, uint8_t address);
    void (*set_layer_enable_mask)(void* user_data, uint64_t mask);
    void (*set_unlimited_sprites)(void* user_data, int unlimited);
    void (*capture_state)(void* user_data, dc_beetle_vdc_state* state);
    void (*apply_state)(void* user_data, const dc_beetle_vdc_state* state);
} dc_beetle_vdc_model;

#define DC_BEETLE_VRAM_WORD_COUNT 32768
#define DC_BEETLE_SAT_WORD_COUNT 256
#define DC_BEETLE_PALETTE_ENTRY_COUNT 512

typedef struct dc_beetle_video_snapshot {
    uint16_t* vram_words;
    size_t vram_word_capacity;
    uint16_t* sat_words;
    size_t sat_word_capacity;
    uint16_t* palette_entries;
    size_t palette_entry_capacity;
    uint16_t memory_width_register;
    uint16_t background_x_scroll;
    uint16_t background_y_scroll;
} dc_beetle_video_snapshot;

// Beetle PCE is single-instance and thread-confined. Registration and callback
// dispatch must occur on that same owning thread.
extern dc_beetle_instruction_hook dc_beetle_active_instruction_hook;
extern void* dc_beetle_active_instruction_hook_user_data;
extern dc_beetle_memory_write_hook dc_beetle_active_memory_write_hook;
extern void* dc_beetle_active_memory_write_hook_user_data;
extern dc_beetle_replacement_hook dc_beetle_active_replacement_hook;
extern void* dc_beetle_active_replacement_hook_user_data;

// Nesting counters. `nested_installs` counts transcripts installed while an
// outer transcript was suspended; `nest_overflows` counts replacements the core
// refused because the suspension depth was already at its maximum. The host
// credits a native call when it offers one, so an overflow would silently
// inflate the reported native share -- reading this counter back is what makes
// the share trustworthy rather than merely plausible.
// `max_depth` is the high-water mark of simultaneously suspended transcripts,
// which is what says whether the depth above is generous or merely adequate.
extern uint32_t dc_beetle_replacement_nested_installs;
extern uint32_t dc_beetle_replacement_nest_overflows;
extern uint32_t dc_beetle_replacement_max_depth;

extern const dc_beetle_psg_model* dc_beetle_active_psg_model;
extern const dc_beetle_vdc_model* dc_beetle_active_vdc_model;

void dc_beetle_reset_replacement_nesting_counters(void);

void dc_beetle_set_instruction_hook(
    dc_beetle_instruction_hook hook,
    void* user_data);
void dc_beetle_clear_instruction_hook(void);
void dc_beetle_set_memory_write_hook(
    dc_beetle_memory_write_hook hook,
    void* user_data);
void dc_beetle_clear_memory_write_hook(void);
void dc_beetle_set_replacement_hook(
    dc_beetle_replacement_hook hook,
    void* user_data);
void dc_beetle_clear_replacement_hook(void);

// Installing a model may happen before or after the emulator has built its
// sound device; the boundary remembers what the emulator reports and hands it
// to whichever model is current.
void dc_beetle_set_psg_model(const dc_beetle_psg_model* model);
void dc_beetle_clear_psg_model(void);
void dc_beetle_set_psg_device(int32_t* left, int32_t* right, int revision);

// The same for the video device. `dc_beetle_set_vdc_device` is called by the
// emulator when it builds its colour encoder, and reports the memory size the
// encoder chose; the boundary remembers it and configures whichever model is
// current, so a model may be installed before or after a ROM is loaded.
//
// Installing a model does not reset it. The emulator resets its video device
// at power-on and after a state load, and a model installed between those has
// to be given a machine that reaches one of them -- which the host does by
// installing before the ROM.
void dc_beetle_set_vdc_model(const dc_beetle_vdc_model* model);
void dc_beetle_clear_vdc_model(void);
void dc_beetle_set_vdc_device(uint32_t vram_size);

// The two edges an installed model reaches back through, as the emulator's
// own video device reaches them. The first re-derives the CPU's interrupt
// level from every video device's latched status; the argument is the
// direction of the change and is discarded, exactly as the emulator discards
// it. The second charges the CPU for a stall and returns zero when the
// per-frame budget of stolen cycles is spent -- a model that treats that
// refusal as advisory will hang the machine rather than fail it.
void dc_beetle_vdc_irq_change(int raised);
int dc_beetle_vdc_wait_state(int32_t cycles);

// Copies the base PC Engine VDC/VCE state without retaining emulator-owned
// pointers. The destination capacities must cover the fixed counts above.
int dc_beetle_copy_video_snapshot(dc_beetle_video_snapshot* snapshot);

#ifdef __cplusplus
} // extern "C"
#endif
