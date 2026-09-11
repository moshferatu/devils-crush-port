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

// Exclusive clock ownership, recorded where the core actually advances time.
enum dc_beetle_execution_owner {
    DC_EXEC_TRANSLATED, DC_EXEC_RETAINED, DC_EXEC_FALLBACK, DC_EXEC_INTERRUPT,
    DC_EXEC_IDLE, DC_EXEC_DEVICE, DC_EXEC_OTHER, DC_EXEC_OWNER_COUNT
};
typedef struct dc_beetle_execution_accounting {
    uint64_t start_master_cycle;
    uint64_t elapsed_master_cycles;
    uint64_t cycles[DC_EXEC_OWNER_COUNT];
    uint64_t opcode_dispatches[DC_EXEC_OWNER_COUNT];
    uint64_t translated_installs;
    uint64_t retained_offers;
    uint64_t rejected_offers;
    uint64_t interrupt_entries;
    uint64_t native_interrupt_entries;
    uint64_t native_block_moves[5]; // TIA, TAI, TDD, TII, TIN starts
    uint64_t native_block_resumes;
    uint64_t native_block_cycles; // Exclusive CPU time, including resumed work
    uint64_t native_bit_branches;
    uint64_t native_bit_branch_cycles; // Exclusive CPU time
} dc_beetle_execution_accounting;
extern dc_beetle_execution_accounting dc_beetle_execution_counters;
extern unsigned dc_beetle_execution_owner;
extern unsigned dc_beetle_execution_block_owner;
void dc_beetle_reset_execution_accounting(void);
void dc_beetle_get_execution_accounting(dc_beetle_execution_accounting* out);
// Called only when a translated transaction or native instruction actually runs.
typedef void (*dc_beetle_translated_hook)(const dc_beetle_cpu_state*, void*);
extern dc_beetle_translated_hook dc_beetle_active_translated_hook;
extern void* dc_beetle_active_translated_hook_user_data;

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
    // Non-zero when the original instruction sampled the interrupt lines with
    // the `I` mask clear and retired with it set -- which is every `SEI`, and
    // on this processor nothing else. `status` above is what the instruction
    // retired, because that is the register the resumed code reads and the
    // byte an interrupt taken here pushes; this is what `LastCycle` saw one
    // cycle earlier, which is what decides whether the interrupt is taken at
    // all. A transcript that leaves this zero is describing an instruction
    // whose two answers agree, which is every other instruction.
    uint8_t samples_interrupt_flag_clear;
    uint8_t mpr[8];
} dc_beetle_replacement_checkpoint;

typedef struct dc_beetle_replacement_result {
    uint8_t handled;
    // A compact request for the core's own resumable TIA loop. The descriptor
    // crosses the hook boundary only; it is deliberately not retained in the
    // serialized replacement arrays used by quick-states.
    uint8_t native_tia_transfer;
    uint16_t native_tia_source;
    uint16_t native_tia_destination;
    uint16_t native_tia_length;
    // A fixed BBR0 that needs the core's two original event samples.
    uint8_t native_bbr0_instruction;
    // A fixed BBR3 that needs the core's two original event samples.
    uint8_t native_bbr3_instruction;
    // A fixed BBS2 that needs the core's two original event samples.
    uint8_t native_bbs2_instruction;
    // A fixed RTS that needs the core's live stack-return path.
    uint8_t native_rts_instruction;
    // A fixed VDC status read that needs the core's acknowledge side effect.
    uint8_t native_vdc_status_read_instruction;
    // A fixed VDC data-port read that needs the core's selected-register and
    // read-buffer side effects.
    uint8_t native_vdc_data_read_instruction;
    // A fixed PLP that needs the core's pre-restore interrupt sample.
    uint8_t native_plp_instruction;
    // A fixed SEI that needs the core's pre-mask interrupt sample.
    uint8_t native_sei_instruction;
    // A fixed JSR that needs the core's live stack-call path.
    uint8_t native_jsr_instruction;
    // A fixed BSR that needs the core's live relative-call path.
    uint8_t native_bsr_instruction;
    // A fixed BCS that needs the core's live relative-branch path.
    uint8_t native_bcs_instruction;
    // The Giant-Face timer-return dispatch keeps its RAM reads and branches.
    uint8_t native_giant_face_wait_dispatch_instruction;
    // The Three-Blobs timer decrement keeps its indexed RAM write and IRQ
    // sample inside the core.
    uint8_t native_three_blobs_timer_decrement_instruction;
    // A fixed JMP that needs the core's original control-transfer boundary.
    uint8_t native_jmp_instruction;
    // The active-flipper owner's short input-tail instruction family.
    uint8_t native_flipper_owner_instruction;
    // The shared actor scheduler's post-call control-tail instruction family.
    uint8_t native_actor_scheduler_instruction;
    uint8_t native_tii_transfer;
    uint16_t native_tii_source;
    uint16_t native_tii_destination;
    uint16_t native_tii_length;
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
    // The host's name for this transcript, returned to it if the core abandons
    // one. Meaningful only to the host: the core stores it, carries it across
    // suspensions, and hands it back. Zero says the host wants no notice.
    uint64_t transcript_token;
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
// A checkpointed transcript the core has given up on, named by the token its
// host returned with it.
//
// The core abandons a transcript when the machine does not come back to the
// resume point in the state the transcript declared -- an interrupt handler
// that returns a register changed, or that does not return there at all. What
// it does then is correct without this: it interprets the original assembly
// from the last checkpoint applied, which is why the emulation never needed
// telling. An observer does. A transcript may have declared things it would go
// on to do -- absorbing its own return is the one that exists -- and an
// undelivered declaration has to be withdrawn rather than left standing.
//
// Called with the transcript still current, before the suspended one beneath
// it becomes current again, and never for a transcript that ran to its end.
//
// `resume_logical_pc` and `resume_physical_bank` are where the original
// assembly is about to be interpreted from, which is the last checkpoint the
// transcript managed to apply rather than the address it was installed at.
// They are the machine's own program counter, read before anything is unwound.
//
// `detail` carries whatever the reason has to add. For a register mismatch it
// is the set of registers that disagreed, below; for the others it is zero.
typedef void (*dc_beetle_replacement_abandoned_hook)(
    uint64_t transcript_token,
    uint16_t resume_logical_pc,
    uint8_t resume_physical_bank,
    uint8_t reason,
    uint8_t detail,
    void* user_data);

// Why the core gave a transcript up. Three endings, one per place the decision
// is made, and they mean different things to a host trying to reduce them: the
// first is the machine disagreeing with a declaration, and the other two are
// the transcript being unable to keep one it made.
//
// Zero is none of them. A transcript that runs to its end is not abandoned and
// reaches no hook, so the value never appears on this interface; it exists so
// that the ending itself carries no reason rather than borrowing one.
//
// The machine came back to the resume point carrying a register the checkpoint
// did not declare. This is the interesting one: the transcript was correct
// about where control would return and wrong about the state it would return
// in, which is a claim about an interrupt handler rather than about the
// transcript's own body.
#define DC_BEETLE_REPLACEMENT_ABANDONED_REGISTERS 1
// A checkpoint asked to advance to an offset the core could not deliver -- a
// write due before the cycle already spent, or a stalling-port wait wider than
// the offset the write was priced into. The body stopped part-way through the
// checkpoint it was applying.
#define DC_BEETLE_REPLACEMENT_ABANDONED_ADVANCE 2
// A checkpoint was asked for when none was left. A transcript that reaches its
// last checkpoint has completed, so this is the core being asked to resume a
// transcript that has nothing further to say.
#define DC_BEETLE_REPLACEMENT_ABANDONED_EXHAUSTED 3

// Which registers disagreed, in the `detail` byte of a `REGISTERS`
// abandonment. More than one bit may be set; the core compares all of them
// before giving up rather than stopping at the first.
//
// The program counter and the stack pointer are deliberately absent. They are
// how the core recognises a resume point at all, so a machine that comes back
// with either of them changed does not reach this comparison and is not an
// abandonment of this kind.
#define DC_BEETLE_REPLACEMENT_MISMATCH_ACCUMULATOR 0x01
#define DC_BEETLE_REPLACEMENT_MISMATCH_X 0x02
#define DC_BEETLE_REPLACEMENT_MISMATCH_Y 0x04
#define DC_BEETLE_REPLACEMENT_MISMATCH_STATUS 0x08
#define DC_BEETLE_REPLACEMENT_MISMATCH_MPR 0x10

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

// The colour encoder's savestate fields, in the order the emulator writes
// them. Roughly a kilobyte, so unlike the controller's this one travels on a
// stack. The colour cache is absent because it is derived from these values
// and the surface expansion, and a state that carried it could restore a
// cache disagreeing with the palette it came from.
typedef struct dc_beetle_vce_state {
    uint16_t color_table[DC_BEETLE_PALETTE_ENTRY_COUNT];
    int32_t ws_counter;
    int32_t clock_divider;
    // Both entries, because the emulator serializes both even on a machine
    // with one controller and the format has to load in either arm.
    int32_t child_event[2];
    int32_t scanline;
    int32_t pixel_offset;
    int32_t hblank_counter;
    int32_t vblank_counter;
    uint16_t ctaddress;
    uint8_t cr;
    uint8_t hblank;
    uint8_t vblank;
    uint8_t need_sl_reset;
} dc_beetle_vce_state;

// One change of horizontal timing, as an installed encoder saw it. The
// frontend keeps the resolution bookkeeping this feeds: which display
// rectangle to show, and whether a frame mixed two dot rates and has to be
// rescaled. A model reports the change; what is made of it stays here.
typedef struct dc_beetle_vce_scanline_info {
    // The four fields of the controller's HSR and HDR registers, in units of
    // eight pixels.
    int pulse;
    int start;
    int width;
    int end;
    // The dot rate in force, and the line it changed on.
    int rate;
    // Non-zero when the encoder is about to restart the field, which is when
    // the recorder discards what it has.
    int line;
    int restart;
} dc_beetle_vce_scanline_info;

// A replacement for the emulator's colour encoder. While one is installed the
// emulator's own `VCE` owns nothing: it receives no clocks, no port accesses,
// no reset and no frame, and survives as the shape its savestate format is
// written through and as the place the frontend's resolution bookkeeping
// lives. The SuperGrafx is never routed, so a machine with two controllers
// keeps the emulator's encoder entire.
//
// This is a wider seam than the controller's, because the encoder owns time
// and draws. `sync` is the whole schedule: it advances the device to a
// master-clock timestamp, drives the controller, and returns the next event
// horizon the processor should stop at. `start_frame` hands over the
// frontend's framebuffer, its pitch and its line-width array for the model to
// write into directly.
//
// Four edges reach back out, exported as functions rather than carried here
// because they run into machine state a model does not have: the interrupt
// line, the cycles a stalled port access steals from the processor, the
// request that the processor stop early, and the compact disc unit's next
// event. A fifth, `dc_beetle_vce_report_scanline_info`, is not a machine edge
// but a frontend one.
//
// The controller the encoder clocks is not part of this structure either. It
// is reached through `dc_beetle_vce_vdc_*`, which dispatch to a native
// controller model or to the emulator's own chip -- so an encoder may be
// installed with the controller handed back, which is the arm that makes a
// divergence attributable to one device rather than to the pair.
typedef struct dc_beetle_vce_model {
    void* user_data;
    void (*reset)(void* user_data);
    void (*reset_timestamp)(void* user_data, int32_t base);
    // `framebuffer` and `line_widths` are the frontend's, and are null when
    // `skip` is set. `awesome_mode` is the upscaled pixel path.
    void (*start_frame)(
        void* user_data,
        uint16_t* framebuffer,
        int32_t pitch,
        int32_t* line_widths,
        int skip,
        int awesome_mode,
        int scanline_start,
        int scanline_end);
    void (*start_partial)(void* user_data);
    void (*finish_partial)(void* user_data);
    int (*frame_done)(void* user_data);
    int32_t (*sync)(void* user_data, int32_t timestamp);
    int32_t (*calc_next_event)(void* user_data);
    // The timestamp the last `sync` measured to. The emulator biases the
    // compact disc event by how far the processor has run past it.
    int32_t (*last_timestamp)(void* user_data);
    void (*set_cd_event)(
        void* user_data,
        int32_t cycles,
        int32_t time_behind);
    // The encoder's own register file at $0400-$0405. Neither call
    // synchronizes or publishes an event horizon; the caller does both around
    // them, exactly as the emulator's own bodies are wrapped.
    void (*write)(void* user_data, uint32_t address, uint8_t value);
    uint8_t (*read)(void* user_data, uint32_t address);
    // The controller's register file, reached through the encoder because the
    // encoder holds the controller's event horizon.
    void (*write_vdc)(void* user_data, uint32_t address, uint8_t value);
    uint8_t (*read_vdc)(void* user_data, uint32_t address);
    void (*write_vdc_st)(void* user_data, uint32_t address, uint8_t value);
    void (*irq_change_check)(void* user_data);
    int (*wait_state)(void* user_data, int32_t vdc_cycles);
    // The two 512-entry expansions of a nine-bit palette entry into a screen
    // colour, normal and greyscale. What those colours are is the display's
    // decision and stays in the emulator; the model only merges them with
    // palette memory.
    void (*set_surface_palette)(
        void* user_data,
        const uint32_t* normal,
        const uint32_t* grey);
    void (*set_disabled_layer_colour)(void* user_data, uint32_t colour);
    void (*set_show_horizontal_overscan)(void* user_data, int show);
    void (*set_layer_enable_mask)(void* user_data, uint64_t mask);
    void (*set_unlimited_sprites)(void* user_data, int unlimited);
    // Fires the scanline-info edge with the timing in force, for a frontend
    // that wants the last line of a finished frame.
    void (*report_scanline_info)(void* user_data);
    int32_t (*scanline)(void* user_data);
    uint16_t (*peek_palette)(void* user_data, uint16_t address);
    void (*capture_state)(void* user_data, dc_beetle_vce_state* state);
    void (*apply_state)(void* user_data, const dc_beetle_vce_state* state);
} dc_beetle_vce_model;

// Every field the processor's savestate carries for its timer and interrupt
// controller, in its order. `timer_lastts` is absent because the emulator does
// not serialize it either -- it is a phase against a processor clock that is
// rebased rather than saved.
typedef struct dc_beetle_irq_state {
    uint32_t irq_low;
    uint8_t irq_mask;
    uint8_t timer_inreload;
    uint8_t timer_status;
    int32_t timer_value;
    int32_t timer_load;
    int32_t timer_div;
} dc_beetle_irq_state;

// The three values the processor mirrors from an installed model.
typedef struct dc_beetle_irq_level {
    // Master clocks until the divider's next tick, which is this device's
    // whole contribution to the processor's next-event horizon.
    int32_t next_event;
    // The request lines asserted, and the mask they are filtered through.
    uint32_t requests;
    uint8_t mask;
} dc_beetle_irq_level;

// A replacement for the HuC6280's timer and interrupt controller: the two
// register files at $0C00-$0C01 and $1402-$1403, the three request lines
// behind them, and the divider that turns the master clock into the only
// interrupt this machine generates for itself.
//
// This is the narrowest seam of the four and the only one inside the processor
// rather than beside it. There is no device object to make dormant, because
// the emulator keeps these as private fields of `HuC6280`; while a model is
// installed those fields are a read-only mirror of it, refreshed through
// `publish` after every dispatch that can move them, and the savestate is
// still written through them.
//
// The mirror is not an optimization of convenience. `CalcNextEvent` runs from
// `ADDCYC`, and the interrupt test at the top of `RunSub` runs once per
// instruction; an indirect call at either point would cost more than this
// whole device does.
//
// What deliberately stays with the processor is the *taking* of an interrupt.
// `IRQSample`, `IFlagSample` and the vector-selection rule are decided at
// instruction boundaries, and this device has no notion of an instruction. It
// publishes a level; the processor decides what to do about it. That split is
// what lets a model be installed while the processor is still the emulator's.
typedef struct dc_beetle_irq_model {
    void* user_data;
    // `HuC6280::Init`, which the machine runs for every ROM it loads. Not a
    // reset: it rebases the phase and zeroes the divider, and leaves the
    // request register and the mask alone.
    void (*init)(void* user_data);
    void (*reset)(void* user_data);
    void (*reset_timestamp)(void* user_data, int32_t base);
    // The processor took the reset vector: the request is released and the
    // mask opens to all three lines. Nothing else ever opens it.
    void (*acknowledge_reset)(void* user_data);
    void (*begin_request)(void* user_data, uint32_t lines);
    void (*end_request)(void* user_data, uint32_t lines);
    // Catches the device up to a master-clock timestamp. Like the emulator's
    // own `TimerSync` it does not recompute the horizon; the caller does.
    void (*sync)(void* user_data, int32_t timestamp);
    void (*publish)(void* user_data, dc_beetle_irq_level* level);
    // The register files. `io_data_buffer` is the last byte the processor
    // drove onto the I/O bus; bit 7 of it survives a counter read and the top
    // five bits survive a status read. The device does not own that latch --
    // it is shared with the sound chip and the controller port -- so it is
    // passed in, which is also what keeps both reads pure.
    uint8_t (*read_timer)(void* user_data, uint8_t io_data_buffer);
    void (*write_timer)(void* user_data, uint32_t address, uint8_t value);
    uint8_t (*read_status)(
        void* user_data,
        uint32_t address,
        uint8_t io_data_buffer);
    uint8_t (*peek_status)(
        void* user_data,
        uint32_t address,
        uint8_t io_data_buffer);
    void (*write_status)(void* user_data, uint32_t address, uint8_t value);
    // Master clocks until a timer interrupt the processor would service. The
    // I flag is the processor's and is passed in for the same reason the bus
    // latch is.
    uint32_t (*timer_horizon)(
        void* user_data,
        int32_t timestamp,
        int i_flag);
    void (*capture_state)(void* user_data, dc_beetle_irq_state* state);
    void (*apply_state)(void* user_data, const dc_beetle_irq_state* state);
} dc_beetle_irq_model;

// The processor's clock and event scheduler, as one block of memory.
//
// This is the fifth seam and the first one whose shape is a pointer rather
// than a call table. The four before it replaced devices the machine reached
// at its own edge, and a vtable entry per operation cost nothing there. This
// device is read from inside the instruction loop: the clock on every memory
// write and every replacement offer, the near horizon in the one opcode that
// waits, the run flag in the loop condition itself. So an installed model
// hands over the storage and the processor reads it directly.
//
// The rule that keeps that honest is that reads are loads and *writes are
// calls*. Nothing outside the model assigns to these fields except the
// savestate, which is the emulator's own format written straight through
// them -- which is also why this is the first model with no capture-and-apply
// pair. `speed_shift` is the one field with an invariant worth stating: it
// is derived from `speed`, so the two only ever move together.
//
// Field order and widths are the emulator's own private fields, so that the
// block can be pointed at without translating anything.
typedef struct dc_beetle_scheduler_registers {
    uint32_t timestamp;
    uint32_t timestamp_fraction;
    uint32_t overclock_rate;
    int32_t next_event;
    int32_t next_user_event;
    int32_t run_flag;
    uint8_t speed;
    uint8_t speed_shift;
} dc_beetle_scheduler_registers;

// A replacement for the HuC6280's clock and event scheduler: the master-cycle
// timestamp every device in this machine measures itself against, the two
// horizons that decide when the processor stops to let something else happen,
// the overclock divider, the rate a processor cycle is priced at, and the loop
// flag that ends a frame.
//
// What deliberately stays with the processor is execution. Fetching an opcode
// and deciding at an instruction boundary that an interrupt has arrived need
// to know what an instruction is; a cycle costing three master clocks or
// twelve, an event handler coming due, and a frame boundary wrapping the clock
// do not. That split is what lets this be installed while the processor is
// still the emulator's.
//
// `registers` is the block above, owned by the model and pointed at by the
// processor for as long as the model is installed.
typedef struct dc_beetle_scheduler_model {
    void* user_data;
    dc_beetle_scheduler_registers* registers;
    // `HuC6280::Init`, which the machine runs for every ROM it loads. Not a
    // reset: neither `Reset` nor `Power` touches this device at all.
    void (*init)(void* user_data);
    // `ADDCYC` and `ADDCYC_MASTER`. Everything the machine does to time goes
    // through one of these two, which is why they are the only entries here
    // that are called more than a few thousand times a frame.
    void (*advance)(void* user_data, int32_t cycles);
    void (*advance_master)(void* user_data, int32_t master_clocks);
    // `CalcNextEvent` and `SetEvent`: recompute the near horizon, and declare
    // when the user event next wants to be called.
    void (*recalculate)(void* user_data);
    void (*set_event)(void* user_data, int32_t cycles);
    // `SyncAndResetTimestamp`: the frame boundary. Neither horizon moves,
    // because both are distances rather than moments.
    void (*sync_and_reset_timestamp)(void* user_data, int32_t base);
    void (*set_overclock)(void* user_data, uint32_t rate);
    // The CSL and CSH opcodes, and the reset vector's return to the slow rate.
    void (*set_speed)(void* user_data, uint8_t speed);
    // The loop flag, which `Run` sets and `Exit` clears. The loop itself is
    // execution and stays with the processor.
    void (*run)(void* user_data, int step_mode);
    void (*exit)(void* user_data);
} dc_beetle_scheduler_model;

// What the boundary decided. A machine asks once per instruction and gets
// `INSTRUCTION` for all but a handful of them.
//
// The values are the enumeration `HuC6280::DcInterruptDecision` spells out,
// and the processor's copy is defined in terms of these rather than beside
// them, so that the two cannot drift apart by a renumbering.
enum {
    DC_BEETLE_BOUNDARY_INSTRUCTION = 0,
    DC_BEETLE_BOUNDARY_RESET = 1,
    DC_BEETLE_BOUNDARY_TIMER = 2,
    DC_BEETLE_BOUNDARY_IRQ1 = 3,
    DC_BEETLE_BOUNDARY_IRQ2 = 4
};

// The two values a HuC6280 latches at the end of an instruction, in the order
// and the encoding its savestate carries them. `i_flag_sample` is `P & I_FLAG`
// or zero, because that is the only thing the emulator ever writes there.
typedef struct dc_beetle_boundary_state {
    int32_t i_flag_sample;
    int32_t irq_sample;
} dc_beetle_boundary_state;

// A replacement for the decision a HuC6280 makes between two instructions:
// whether the next thing to happen is the next instruction or an interrupt.
//
// This is the sixth seam and the first that is a rule rather than a device.
// The five before it each replaced something with a state boundary around it
// -- a set of channels, two chips, a timer, a clock. This one holds two
// latched values and otherwise computes, so there is nothing here to keep
// coherent with anything and no mirror to refresh: the processor hands over
// the four inputs it already has in registers and takes back a small integer.
//
// That is also the whole argument for the shape. The interrupt controller is
// *read* from inside the instruction loop and became a mirror; the scheduler
// is *written* from inside it and became a block of memory. This one is
// neither. It runs twice per instruction, which is more often than either,
// but with no memory traffic behind it at all -- so a call is what it costs
// and a call is what it is.
//
// What deliberately stays with the processor is the *taking* of a vector:
// eight cycles of dummy reads, three pushes, a flag update and a two-byte
// fetch, all of it against the processor's own stack and memory map. This
// model says which vector; the machine does the taking.
typedef struct dc_beetle_boundary_model {
    void* user_data;
    // `HuC6280::Reset`: the latch becomes the reset line alone. It is the
    // only thing that ever puts that line there, and the latched I flag is
    // deliberately untouched, exactly as the emulator leaves it.
    void (*reset)(void* user_data);
    // `LastCycle`, without the cycle. The request register and the mask
    // belong to the interrupt controller and the I flag to the processor, so
    // all three cross as arguments rather than being reached for -- which is
    // also what lets a native boundary sit above an emulated controller.
    void (*sample)(
        void* user_data,
        uint32_t requests,
        uint8_t mask,
        int i_flag_set);
    // The top of `RunSub`, returning one of the values above. The lines are
    // passed again because the rule reads them in the present tense as well
    // as the past: a timer request raised since the latch wins immediately
    // where an IRQ1 or IRQ2 request raised in the same window waits.
    int (*select)(void* user_data, uint32_t requests, uint8_t mask);
    uint16_t (*vector)(void* user_data, int decision);
    // The reset line leaves the latch when the machine takes the vector.
    // Only the latch: the request itself and the mask belong to the
    // interrupt controller, which the same machine tells separately.
    void (*acknowledge_reset)(void* user_data);
    void (*capture_state)(void* user_data, dc_beetle_boundary_state* state);
    void (*apply_state)(
        void* user_data,
        const dc_beetle_boundary_state* state);
} dc_beetle_boundary_model;

// What the processor is holding at an instruction boundary, for the one
// question an idle model is asked. Everything here is either a register the
// processor already has or a direct pointer into memory it has already
// mapped, so asking costs no bus access and no side effect.
//
// The two pointers are the machine's, and what to read from them is the
// model's business rather than this seam's. `code` is the instruction stream
// at the watched address, and `zero_page` is this processor's zero page --
// which is at logical `$2000` rather than at zero, that being where every
// zero-page effective address on a HuC6280 is built from. Either is null when
// the logical page it names has no directly readable memory behind it, which
// for zero page means a machine that has mapped its own RAM out. `code_bytes`
// is what is left of `code`'s 8 KiB page, so a model may compare a signature
// without walking off the end of it.
typedef struct dc_beetle_idle_state {
    uint16_t logical_pc;
    uint8_t physical_bank;
    uint8_t accumulator;
    uint8_t x;
    uint8_t y;
    uint8_t status;
    const uint8_t* code;
    uint32_t code_bytes;
    const uint8_t* zero_page;
} dc_beetle_idle_state;

// A spin the machine can be advanced through instead of executed.
//
// This is the seventh seam and the only one that is a capability rather than
// a replacement: nothing here takes work away from the emulator, and handing
// it back changes nothing but the speed. What it provides is the one thing a
// routine replacement structurally cannot express -- a scope whose cost is
// not known when it is offered, because it ends when the raster says so.
//
// **The split is the whole design.** *Whether this loop is idle* is a claim
// about a particular cartridge: which two instructions, what they read, and
// what the machine would be waiting for. *How far the machine may be advanced
// while nothing executes* is a claim about the machine, and it is this side's
// answer: as many whole iterations as end strictly before the scheduler's
// near horizon, and not one cycle further.
//
// That bound is what makes the capability exact rather than approximate, and
// it is worth stating why. Between now and the near horizon no device can
// raise a line -- the horizon is by construction the nearer of the timer's
// next tick and the user event's next call -- so no interrupt can be taken,
// and with the processor executing nothing, no store can reach memory. The
// end-of-instruction latch that the skipped instructions would each have
// taken therefore latches what the last real one already latched. Stopping
// strictly before the horizon leaves the crossing itself to a real
// instruction, so where an event lands relative to an instruction boundary is
// decided by the machine exactly as it was before. Nothing observable
// survives the skip except the clock, which is the point.
//
// **And a second condition, which is not the same one and is easy to miss.**
// Nothing may be *standing* either. A request the mask already admits has not
// been latched yet, so the decision taken at the top of the instruction loop
// answers "next instruction" and the spin looks idle -- but the next
// instruction's own end-of-cycle sample would catch it, and the boundary after
// that would take the vector. Skipping past that boundary delivers the
// interrupt a turn late. One condition says nothing can happen before the
// horizon; this one says nothing has happened already. Both are the machine's,
// which is why both live on this side of the seam rather than in the model.
typedef struct dc_beetle_idle_model {
    void* user_data;
    // The instruction boundary this model has anything to say about. Compared
    // against the processor's own two values before the model is consulted at
    // all, so an installed model costs a load and a predicted branch per
    // instruction rather than a call.
    uint16_t watch_logical_pc;
    uint8_t watch_physical_bank;
    // Processor cycles one iteration of the spin costs, or zero for "execute
    // this normally" -- which is the answer whenever the loop is about to
    // exit, and the answer a model that does not recognise what it is looking
    // at must give.
    int32_t (*iteration_cycles)(
        void* user_data,
        const dc_beetle_idle_state* state);
    // What the machine actually did with that answer, which the model cannot
    // work out for itself because the horizon is not its to see.
    //
    // Called on *every* idle answer rather than only on a skip, and priced in
    // master cycles rather than the processor cycles the model answered in,
    // because both are needed to say how long the machine waited. A skip
    // contributes `master_skipped`; the turn the instruction below is about
    // to execute contributes `master_per_iteration`, and nothing else knows
    // what that turn costs at the rate the processor is currently running.
    // `iterations` is zero when the horizon or a standing request left no
    // room to skip, which is a wait the machine sat through rather than an
    // absence of one.
    void (*advanced)(
        void* user_data,
        int32_t iterations,
        int32_t master_skipped,
        int32_t master_per_iteration);
} dc_beetle_idle_model;

// The controller port serves five devices through one multitap, and the
// emulator builds an object for each of them whether or not anything is
// plugged in.
#define DC_BEETLE_PAD_PORT_COUNT 5

// Which device the emulator was told to put on a port. The values are this
// boundary's own rather than the emulator's private enumeration, and the
// translation happens where that enumeration is in scope.
enum {
    DC_BEETLE_PAD_DEVICE_NONE = 0,
    DC_BEETLE_PAD_DEVICE_GAMEPAD = 1,
    DC_BEETLE_PAD_DEVICE_MOUSE = 2,
    DC_BEETLE_PAD_DEVICE_TSUSHINKB = 3
};

// Every field the emulator's savestate carries for the port and the devices on
// it, in its order: the `JOY` section's three, then one `JOYn` section per
// port. `buttons` is the frontend's sixteen-bit word as the device latched it,
// and `six_button_half` is which half of a six-button pad the next read would
// answer with.
typedef struct dc_beetle_pad_port_state {
    uint16_t buttons;
    uint8_t select;
    uint8_t clear;
    uint8_t six_button_half;
} dc_beetle_pad_port_state;

typedef struct dc_beetle_pad_state {
    dc_beetle_pad_port_state ports[DC_BEETLE_PAD_PORT_COUNT];
    uint8_t select;
    uint8_t clear;
    uint8_t read_index;
} dc_beetle_pad_state;

// A replacement for the joypad port at `$1000`, and the eighth seam.
//
// **This one is an ownership change rather than a port.** The strobe protocol
// the game drives that port with was modelled natively some time ago, in the
// replacement for the cartridge's own pad reader; what it read *through* was
// still the emulator's chain -- a frontend button buffer, five
// `PCE_Input_Device` objects, a multitap counter -- and this is that chain.
// So the interesting property is not that a device moved but that the input
// now enters the machine from the host directly: while a model is installed
// the emulator's button buffer is never consulted, and the frontend may
// answer every input query with zero without changing a frame.
//
// **What the model owns.** Two latched lines, a multitap read counter, and one
// device per port. That is the entire port: `INPUT_Read` and `INPUT_Write` are
// dispatched here, `INPUT_Frame` is where a device latches the frontend's
// buttons, `PCEINPUT_Power` resets it, and `INPUT_AdjustTS` rebases whatever
// time a device keeps -- which for a two-button gamepad is none, and the entry
// exists so that the seam is the machine's shape rather than this cartridge's.
//
// **Where the host's input arrives is deliberately not here.** A model reads
// it from whoever owns it, which is the frontend, and this boundary carries
// only the machine's half: when the latch happens, what is on each port, and
// the two settings that decide how the port answers.
typedef struct dc_beetle_pad_model {
    void* user_data;
    // The machine's own settings, as the emulator resolved them: whether the
    // multitap is enabled, whether the SELECT+RUN soft reset is suppressed
    // before a device sees it, and whether a CD unit is attached -- the last
    // being a bit of the port's answer rather than a device's.
    void (*configure)(
        void* user_data,
        int multitap_enabled,
        int disable_soft_reset,
        int cd_attached);
    // `PCEINPUT_SetInput`: what the frontend plugged into a port. Reported for
    // every port the emulator is told about, whether or not a model is
    // installed at the time, so an install may happen either side of a load.
    void (*configure_port)(
        void* user_data,
        unsigned port,
        int device_type);
    void (*power)(void* user_data, int32_t timestamp);
    // `INPUT_Frame`: the moment every device latches the frontend's buttons.
    // Called once at the start of a frame and again after each mid-frame
    // resynchronisation, with `start_frame` distinguishing the two exactly as
    // the emulator distinguishes them.
    void (*frame)(void* user_data, int start_frame);
    // `PCEINPUT_TransformInput`: the soft-reset suppression, applied to the
    // buttons rather than to a device.
    void (*transform_input)(void* user_data);
    void (*adjust_timestamp)(void* user_data, int32_t delta);
    uint8_t (*read)(void* user_data, int32_t timestamp, uint32_t address);
    void (*write)(
        void* user_data,
        int32_t timestamp,
        uint32_t address,
        uint8_t value);
    void (*capture_state)(void* user_data, dc_beetle_pad_state* state);
    void (*apply_state)(void* user_data, const dc_beetle_pad_state* state);
} dc_beetle_pad_model;

// Beetle PCE is single-instance and thread-confined. Registration and callback
// dispatch must occur on that same owning thread.
extern dc_beetle_instruction_hook dc_beetle_active_instruction_hook;
extern void* dc_beetle_active_instruction_hook_user_data;
extern dc_beetle_memory_write_hook dc_beetle_active_memory_write_hook;
extern void* dc_beetle_active_memory_write_hook_user_data;
extern dc_beetle_replacement_hook dc_beetle_active_replacement_hook;
extern void* dc_beetle_active_replacement_hook_user_data;
extern dc_beetle_replacement_abandoned_hook
    dc_beetle_active_replacement_abandoned_hook;
extern void* dc_beetle_active_replacement_abandoned_hook_user_data;

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
// Transcripts the core gave up on part-way through. Counted whether or not
// anything is listening on the hook above and whether or not the transcript was
// named, so that this is a measurement of the core rather than of the host's
// bookkeeping. Reset with the three counters above.
extern uint32_t dc_beetle_replacement_abandonments;

extern const dc_beetle_psg_model* dc_beetle_active_psg_model;
extern const dc_beetle_vdc_model* dc_beetle_active_vdc_model;
extern const dc_beetle_vce_model* dc_beetle_active_vce_model;
extern const dc_beetle_irq_model* dc_beetle_active_irq_model;
extern const dc_beetle_scheduler_model* dc_beetle_active_scheduler_model;
extern const dc_beetle_boundary_model* dc_beetle_active_boundary_model;
extern const dc_beetle_idle_model* dc_beetle_active_idle_model;
extern const dc_beetle_pad_model* dc_beetle_active_pad_model;

// Where the processor's clock and event scheduler currently lives. Never null:
// with no model installed it points at storage this boundary owns, which is
// then the emulator's own scheduler state and nothing else's. Reading through
// it is what the processor does instead of holding those fields itself.
extern dc_beetle_scheduler_registers* dc_beetle_active_scheduler_registers;

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
void dc_beetle_set_replacement_abandoned_hook(
    dc_beetle_replacement_abandoned_hook hook,
    void* user_data);
void dc_beetle_clear_replacement_abandoned_hook(void);

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
//
// Both reach whichever encoder is current. That is the whole reason they were
// exported as functions rather than wired to the emulator's encoder directly:
// a native controller beside a native encoder has to stall and interrupt the
// encoder that is actually clocking it, and neither the controller nor the
// host has to know which one that is.
void dc_beetle_vdc_irq_change(int raised);
int dc_beetle_vdc_wait_state(int32_t cycles);

// Installing an encoder model does not reset it, on the same terms as the
// controller's: the machine resets its encoder at power-on and after a state
// load, and a model installed between those has to be given a machine that
// reaches one of them.
void dc_beetle_set_vce_model(const dc_beetle_vce_model* model);
void dc_beetle_clear_vce_model(void);

// The four machine edges an installed encoder reaches back through. The first
// sets the processor's interrupt line; the second charges it for a stall the
// encoder has already decided is affordable, so there is nothing to return;
// the third asks it to stop running early at a scanline boundary; the fourth
// runs the compact disc unit up to a timestamp and returns its next event.
void dc_beetle_vce_irq(int raised);
void dc_beetle_vce_steal_cycles(int32_t cycles);
void dc_beetle_vce_exit(void);
int32_t dc_beetle_vce_cd_event(uint32_t timestamp);

// The frontend edge. The emulator keeps the resolution bookkeeping and the
// display rectangle, and this is where an installed encoder feeds them.
void dc_beetle_vce_report_scanline_info(
    const dc_beetle_vce_scanline_info* info);

// The controller as an installed encoder reaches it. Each of these dispatches
// exactly as the emulator's own encoder does -- to a native controller model
// when one is installed, and to the emulator's first chip when none is. One
// decision point serves both encoders, so installing the two models in either
// order lands in the same place.
int32_t dc_beetle_vce_vdc_reset(void);
int32_t dc_beetle_vce_vdc_run(int32_t clocks, uint16_t* pixels, int skip);
int32_t dc_beetle_vce_vdc_hsync(int in_hblank);
int32_t dc_beetle_vce_vdc_vsync(int in_vblank);
void dc_beetle_vce_vdc_write(uint32_t address, uint8_t value);
uint8_t dc_beetle_vce_vdc_read(uint32_t address, int peek);
int dc_beetle_vce_vdc_peek_irq(void);
uint32_t dc_beetle_vce_vdc_get_register(unsigned id);
void dc_beetle_vce_vdc_set_layer_enable_mask(uint64_t mask);
void dc_beetle_vce_vdc_set_unlimited_sprites(int unlimited);

// Installing a timer and interrupt controller does not reset it either, and
// for this one that matters more than it does for the video devices: the
// processor mirrors three of its values, and the mirror is only refreshed by a
// dispatch. So a model must be installed before a machine that will reach its
// own power-on -- which is the same rule as the others, arrived at from the
// other direction.
void dc_beetle_set_irq_model(const dc_beetle_irq_model* model);
void dc_beetle_clear_irq_model(void);

// Installing a scheduler moves where the processor's clock lives, so it must
// happen while the machine is stopped -- before a ROM is loaded, which is the
// same rule the other four models arrive at. Whatever the previous owner held
// stays with it: the model that takes over brings its own values, and the
// `HuC6280::Init` that follows a ROM load is what puts both on the same
// footing.
void dc_beetle_set_scheduler_model(const dc_beetle_scheduler_model* model);
void dc_beetle_clear_scheduler_model(void);

// Installing an instruction boundary is the least demanding of the six, and
// for the reason the seam is a call: there is no storage to hand over and no
// mirror to leave behind, so the only thing an install loses is whatever the
// previous owner had latched. That still means before a ROM loads, because
// the reset vector a machine takes at power-on is the one thing that puts a
// reset into the latch -- a model installed after it would be a boundary that
// had never seen a reset.
void dc_beetle_set_boundary_model(const dc_beetle_boundary_model* model);
void dc_beetle_clear_boundary_model(void);

// An idle model is the one registration on this boundary with no timing rule
// at all. It takes nothing over, so there is nothing for a machine to find
// missing: it may be installed or handed back at any instruction boundary,
// before or after a ROM loads, and a machine that never reaches the address
// it watches behaves as though it were not there.
void dc_beetle_set_idle_model(const dc_beetle_idle_model* model);
void dc_beetle_clear_idle_model(void);

// A joypad port may be installed before or after a ROM loads. The boundary
// remembers the machine's settings and every port assignment the emulator has
// reported so far and replays them into whichever model is current, so an
// install that happens after `PCEINPUT_Init` is not a model that missed it.
//
// Handing the port back mid-machine is the one thing that loses something: the
// emulator's own devices have been receiving nothing while the model ran, so
// their latched lines and button words are whatever they held at the install.
// The next `INPUT_Frame` restores the buttons; the two lines are restored by
// the next write, which on this machine is the pad reader's own strobe.
void dc_beetle_set_pad_model(const dc_beetle_pad_model* model);
void dc_beetle_clear_pad_model(void);
void dc_beetle_set_pad_settings(
    int multitap_enabled,
    int disable_soft_reset,
    int cd_attached);
void dc_beetle_set_pad_port(unsigned port, int device_type);

// The four machine edges an installed scheduler reaches back through, and the
// reason they are edges rather than an object: the scheduler stops for two
// devices, and which of them is native is not its business.
//
// The first three are the timer's. It is caught up to the clock's current
// moment, asked how far away its next tick is, and told when the clock has
// been rebased at a frame boundary. Each dispatches exactly as the emulator's
// own `HuC6280` does, to a native controller model when one is installed and
// to the processor's own register file when none is.
//
// The fourth is the user event handler -- the colour encoder, in this machine
// -- called at the moment its own horizon came due and answering with master
// clocks until the next one.
void dc_beetle_scheduler_sync_timer(void);
int32_t dc_beetle_scheduler_timer_horizon(void);
void dc_beetle_scheduler_rebase_timer(int32_t base);
int32_t dc_beetle_scheduler_user_event(int32_t timestamp);

// Copies the base PC Engine VDC/VCE state without retaining emulator-owned
// pointers. The destination capacities must cover the fixed counts above.
int dc_beetle_copy_video_snapshot(dc_beetle_video_snapshot* snapshot);

#ifdef __cplusplus
} // extern "C"

// Scope restoration prevents nested device steals from being charged to the
// instruction or interrupt that requested them. No emulator state is changed.
class DcExecutionOwnerScope {
    unsigned previous_;
public:
    explicit DcExecutionOwnerScope(unsigned owner)
        : previous_(dc_beetle_execution_owner) { dc_beetle_execution_owner = owner; }
    ~DcExecutionOwnerScope() { dc_beetle_execution_owner = previous_; }
};
#endif
