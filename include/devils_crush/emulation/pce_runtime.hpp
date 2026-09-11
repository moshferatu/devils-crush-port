// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/audio/pce_psg.hpp"
#include "devils_crush/cpu/huc6280_idle_skip.hpp"
#include "devils_crush/cpu/huc6280_interrupt_boundary.hpp"
#include "devils_crush/cpu/huc6280_irq.hpp"
#include "devils_crush/cpu/huc6280_scheduler.hpp"
#include "devils_crush/input/pce_pad.hpp"
#include "devils_crush/video/pce_vce.hpp"
#include "devils_crush/video/pce_vdc.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace devils_crush::emulation {

enum class Button : std::uint16_t {
    none = 0,
    one = 1U << 0U,
    two = 1U << 1U,
    select = 1U << 2U,
    run = 1U << 3U,
    up = 1U << 4U,
    right = 1U << 5U,
    down = 1U << 6U,
    left = 1U << 7U,
};

struct InputFrame {
    std::uint16_t player_one = 0;
    std::uint16_t player_two = 0;

    [[nodiscard]] bool operator==(const InputFrame&) const = default;
};

struct FramebufferView {
    std::span<const std::uint32_t> pixels;
    int width = 0;
    int height = 0;
    int pitch_pixels = 0;
};

struct AudioView {
    std::span<const float> interleaved_stereo;
    int sample_rate = 0;
};

struct EmulatedFrame {
    // Views remain valid until the next non-const runtime operation or
    // runtime destruction.
    FramebufferView framebuffer;
    AudioView audio;
    std::uint64_t frame_number = 0;
};

struct RomLoadResult {
    bool loaded = false;
    std::string message;
};

struct RuntimeInfo {
    std::string name;
    std::string upstream_revision;
    bool core_available = false;
};

struct SaveState {
    std::vector<std::byte> core_state;
    std::uint64_t frame_number = 0;

    [[nodiscard]] bool operator==(const SaveState&) const = default;
};

struct VideoMemorySnapshot {
    std::vector<std::uint16_t> vram_words;
    // The base HuC6270's internal post-DMA sprite attribute table. Four
    // words describe each of 64 hardware sprites.
    std::vector<std::uint16_t> sat_words;
    std::vector<std::uint16_t> palette_entries;
    std::uint16_t memory_width_register = 0;
    std::uint16_t background_x_scroll = 0;
    std::uint16_t background_y_scroll = 0;

    [[nodiscard]] int bat_width_tiles() const noexcept
    {
        constexpr std::array widths{32, 64, 128, 128};
        return widths[(memory_width_register >> 4U) & 0x03U];
    }

    [[nodiscard]] int bat_height_tiles() const noexcept
    {
        return (memory_width_register & 0x40U) != 0U ? 64 : 32;
    }
};

struct RoutineKey {
    std::uint8_t physical_bank = 0;
    std::uint16_t logical_pc = 0;

    [[nodiscard]] bool operator==(const RoutineKey&) const = default;
};

enum class HookMode {
    pass_through,
    observe,
    replace,
};

struct CpuRegisters {
    std::uint16_t program_counter = 0;
    std::uint8_t accumulator = 0;
    std::uint8_t x = 0;
    std::uint8_t y = 0;
    std::uint8_t stack_pointer = 0;
    std::uint8_t status = 0;
    std::array<std::uint8_t, 8> mpr{};

    [[nodiscard]] bool operator==(const CpuRegisters&) const = default;
};

struct RoutineEntry {
    RoutineKey routine;
    CpuRegisters registers;
    // The pad state the core is answering $1000 reads from for this frame.
    // It is machine state a replacement cannot otherwise see -- the joypad
    // is a device, not memory -- and the pad reader at 00:E195 is the one
    // routine whose result is a pure function of it. Handing it over here
    // rather than through the app keeps the value the transcript predicts
    // and the value the core would have returned the same object.
    InputFrame input;
    std::uint64_t frame_number = 0;
    std::uint32_t master_cycle = 0;
    std::uint64_t absolute_master_cycle = 0;
    // Atomic native bodies must finish strictly before the user/video event
    // or serviceable timer-IRQ boundary. A checkpointed transcript may cross
    // these horizons because the core samples and delivers interrupts at its
    // declared original instruction boundaries.
    std::uint32_t master_cycles_until_user_event =
        std::numeric_limits<std::uint32_t>::max();
    std::uint32_t master_cycles_until_timer_interrupt =
        std::numeric_limits<std::uint32_t>::max();

    [[nodiscard]] bool operator==(const RoutineEntry&) const = default;
};

using ExecutionHookCallback = std::function<void(const RoutineEntry&)>;

struct ReplacementMemoryWrite {
    RoutineKey instruction;
    std::uint16_t logical_address = 0;
    std::uint8_t value = 0;
    std::uint32_t master_cycle_offset = 0;

    [[nodiscard]] bool operator==(
        const ReplacementMemoryWrite&) const = default;
};

struct ReplacementCheckpoint {
    std::uint32_t interrupt_sample_master_cycle_offset = 0;
    std::uint32_t master_cycle_offset = 0;
    RoutineKey instruction;
    CpuRegisters registers;
    // Set only by a checkpoint standing after an `SEI`.
    //
    // `registers.status` is what the instruction retired: the byte the resumed
    // code reads, and the byte an interrupt taken at this boundary pushes. It
    // is also, for every other instruction, the byte the interrupt lines were
    // sampled under -- so the core reads the `I` mask straight out of it and
    // one field says both things.
    //
    // A mask set is the one instruction where the two disagree. `LastCycle`
    // samples before the opcode writes back, so an `SEI` samples the window it
    // is about to close and an interrupt already pending is still taken there.
    // This says so, and the core applies the clear mask across the sample and
    // the retired one immediately after, which is the order the hardware has.
    //
    // A mask clear needs nothing here: publish the old status at the `CLI` and
    // the new one at the checkpoint after it, and both answers come out right
    // from the one byte. That deferral is what `pull_status` and the
    // status-change machinery in the walkers already do.
    bool samples_interrupt_flag_clear = false;

    [[nodiscard]] bool operator==(
        const ReplacementCheckpoint&) const = default;
};

// An `RTS` a transcript ended past rather than in front of.
//
// The core needs nothing from this. The pop is already in `output_registers`
// and the caller's address is already `return_instruction`, which is why the
// technique needed no core change to work. The routine-call observer does need
// it: its only completion signal is an emulated `RTS` boundary at the frame the
// call was entered on, and an absorbed return deletes the boundary it is
// watching for. Without this the call stays open until some ancestor returns at
// the same stack pointer, and is then reported with that ancestor's return
// address, its cycle and every write in between.
//
// Declaring the return here lets the observer finish the call where the
// assembly finished it, so a trace taken over a transcript that absorbs its
// return is the trace that was taken over the routine that did not.
struct AbsorbedReturn {
    RoutineKey instruction;
    // Where the return instruction's own boundary sits inside the transaction:
    // the start of the `RTS`, not the end of it.
    std::uint32_t master_cycle_offset = 0;
    // The state at that boundary, so before the pop -- the stack pointer still
    // names the frame the call was entered on, which is what the observer
    // matches against.
    CpuRegisters registers;

    [[nodiscard]] bool operator==(const AbsorbedReturn&) const = default;
};

// A compact request for the core to execute its own resumable TIA block-move
// implementation. Unlike a replacement write list, this does not put one
// entry per bus write into the core's serialized transaction state: the core
// keeps the transfer's ordinary source, destination, length, and alternate
// phase, so an event or quick-state can resume it exactly as an opcode would.
struct NativeTiaTransfer {
    std::uint16_t source = 0;
    std::uint16_t destination = 0;
    std::uint16_t length = 0;

    [[nodiscard]] bool operator==(const NativeTiaTransfer&) const = default;
};

// The HuC6280's BBR0 executes as two separately event-sampled core steps.
// Marking this fixed opcode as a core capability preserves that indivisible
// timing while the surrounding game loop remains native.
struct NativeBbr0Instruction {
    [[nodiscard]] bool operator==(const NativeBbr0Instruction&) const = default;
};

// Like BBR0 above, this fixed BBR3 needs the core's two original event
// samples. It is separate because its gameplay-loop arms differ from the
// tilemap opcode's arms.
struct NativeBbr3Instruction {
    [[nodiscard]] bool operator==(const NativeBbr3Instruction&) const = default;
};

// Like BBR, BBS performs separately event-sampled zero-page and branch steps.
// A fixed BBS2 must therefore retain the core's original instruction path.
struct NativeBbs2Instruction {
    [[nodiscard]] bool operator==(const NativeBbs2Instruction&) const = default;
};

// A fixed RTS that returns through a live owner/scheduler composition. The
// core must retain its stack reads, return target, and event boundary.
struct NativeRtsInstruction {
    [[nodiscard]] bool operator==(const NativeRtsInstruction&) const = default;
};

// The VDC status read both returns a device byte and acknowledges its latched
// interrupt sources. The core must retain that hardware read and its boundary.
struct NativeVdcStatusReadInstruction {
    [[nodiscard]] bool operator==(const NativeVdcStatusReadInstruction&) const = default;
};

// A fixed VDC data-port read.  Unlike a status read, this consumes the
// controller's selected register and may advance its read state, so the core
// must retain the original LDA instruction and device access.
struct NativeVdcDataReadInstruction {
    [[nodiscard]] bool operator==(const NativeVdcDataReadInstruction&) const = default;
};

// A fixed PLP whose interrupt sample precedes the status byte restored from
// the stack. The core must retain that instruction ordering exactly.
struct NativePlpInstruction {
    [[nodiscard]] bool operator==(const NativePlpInstruction&) const = default;
};

// A fixed SEI whose IRQ sample precedes setting the interrupt-disable flag.
// The core must retain that instruction ordering exactly.
struct NativeSeiInstruction {
    [[nodiscard]] bool operator==(const NativeSeiInstruction&) const = default;
};

// A fixed JSR whose live stack frame and interrupt boundary stay in the core.
struct NativeJsrInstruction {
    [[nodiscard]] bool operator==(const NativeJsrInstruction&) const = default;
};

// A fixed BSR whose two-byte live stack frame and relative branch remain in
// the core. The caller and callee share that frame, so a replacement cannot
// safely publish a synthetic return address here.
struct NativeBsrInstruction {
    [[nodiscard]] bool operator==(const NativeBsrInstruction&) const = default;
};

// A fixed BCS whose taken and fall-through timing must retain the core's
// original relative branch and final event sample.
struct NativeBcsInstruction {
    [[nodiscard]] bool operator==(const NativeBcsInstruction&) const = default;
};

// The Giant-Face animator's timer-return dispatch is a short sequence of
// loads, compares, and relative branches. The core retains its RAM reads,
// flag updates, and per-opcode event samples.
struct NativeGiantFaceWaitDispatchInstruction {
    [[nodiscard]] bool operator==(const NativeGiantFaceWaitDispatchInstruction&) const = default;
};

// The Three-Blobs controller's timer decrement can fall directly onto a timer
// interrupt horizon.  The core therefore owns the original indexed DEC.
struct NativeThreeBlobsTimerDecrementInstruction {
    [[nodiscard]] bool operator==(const NativeThreeBlobsTimerDecrementInstruction&) const = default;
};

// A fixed JMP whose target and event boundary stay in the core.
struct NativeJmpInstruction {
    [[nodiscard]] bool operator==(const NativeJmpInstruction&) const = default;
};

// The active-flipper owner's short input-tail instructions. Their exact
// memory and branch boundaries remain in the core beside the host input gate.
struct NativeFlipperOwnerInstruction {
    [[nodiscard]] bool operator==(const NativeFlipperOwnerInstruction&) const = default;
};

// The shared actor scheduler's short post-call control tail. Its load, mask,
// and branch remain at their original instruction boundaries in the core.
struct NativeActorSchedulerInstruction {
    [[nodiscard]] bool operator==(const NativeActorSchedulerInstruction&) const = default;
};

struct NativeTiiTransfer {
    std::uint16_t source = 0U;
    std::uint16_t destination = 0U;
    std::uint16_t length = 0U;
    [[nodiscard]] bool operator==(const NativeTiiTransfer&) const = default;
};

struct RoutineReplacement {
    // Where the core resumes after applying the transaction. A transcript that
    // stops in front of its own RTS names that RTS here and leaves it to be
    // emulated; one that ends past it names the caller instead and declares the
    // return in `absorbed_returns` below.
    RoutineKey return_instruction;
    CpuRegisters output_registers;
    std::uint32_t body_master_cycles = 0;
    // Legacy name: entries may target ordinary work RAM (physical bank F8)
    // or a bounded set of device registers on the PC Engine hardware page
    // (physical bank FF): the VDC base ports $0000-$0003, the PSG register
    // file $0800-$0809, the timer at $0C00-$0C01, and the interrupt
    // controller at $1402-$1403. Hardware-page writes are used only for
    // bounded, trace-locked device-register paths.
    std::vector<ReplacementMemoryWrite> work_ram_writes;
    // Optional instruction-boundary transcript. When present, the core
    // advances one checkpoint at a time so original interrupt handlers can
    // preempt and return between native segments.
    std::vector<ReplacementCheckpoint> checkpoints;
    // An opcode-shaped transfer the core executes through its existing TIA
    // loop. It is mutually exclusive with generic writes and checkpoints.
    std::optional<NativeTiaTransfer> native_tia_transfer;
    // A fixed BBR0 whose original two-sample core implementation must execute
    // intact. It is mutually exclusive with generic writes and checkpoints.
    std::optional<NativeBbr0Instruction> native_bbr0_instruction;
    // A fixed BBR3 whose original two-sample core implementation must execute
    // intact. It is mutually exclusive with generic writes and checkpoints.
    std::optional<NativeBbr3Instruction> native_bbr3_instruction;
    // A fixed BBS2 whose original two-sample core implementation must execute
    // intact. It is mutually exclusive with generic writes and checkpoints.
    std::optional<NativeBbs2Instruction> native_bbs2_instruction;
    // A fixed RTS whose original core path must retain the live return target.
    // It is mutually exclusive with generic writes and checkpoints.
    std::optional<NativeRtsInstruction> native_rts_instruction;
    // A fixed VDC status read whose core path retains its acknowledge side
    // effect. It is mutually exclusive with generic writes and checkpoints.
    std::optional<NativeVdcStatusReadInstruction>
        native_vdc_status_read_instruction;
    // A fixed VDC data-port read whose selected-register side effects stay in
    // the core. It is mutually exclusive with generic writes and checkpoints.
    std::optional<NativeVdcDataReadInstruction>
        native_vdc_data_read_instruction;
    std::optional<NativePlpInstruction> native_plp_instruction;
    std::optional<NativeSeiInstruction> native_sei_instruction;
    std::optional<NativeJsrInstruction> native_jsr_instruction;
    std::optional<NativeBsrInstruction> native_bsr_instruction;
    std::optional<NativeBcsInstruction> native_bcs_instruction;
    std::optional<NativeGiantFaceWaitDispatchInstruction>
        native_giant_face_wait_dispatch_instruction;
    std::optional<NativeThreeBlobsTimerDecrementInstruction>
        native_three_blobs_timer_decrement_instruction;
    std::optional<NativeJmpInstruction> native_jmp_instruction;
    std::optional<NativeFlipperOwnerInstruction>
        native_flipper_owner_instruction;
    std::optional<NativeActorSchedulerInstruction>
        native_actor_scheduler_instruction;
    std::optional<NativeTiiTransfer> native_tii_transfer;
    // The returns this transcript ended past rather than in front of, in the
    // order it passed them: its own, and any it absorbed on behalf of a routine
    // it composed. Empty for a transcript that stops before its RTS.
    std::vector<AbsorbedReturn> absorbed_returns;
};

using ReplacementHookCallback = std::function<
    std::optional<RoutineReplacement>(
        const RoutineEntry&,
        std::span<const std::byte> work_ram)>;

struct ExecutionHook {
    RoutineKey routine;
    HookMode mode = HookMode::pass_through;
    ExecutionHookCallback callback;
    ReplacementHookCallback replacement_callback;
};

struct ObservedMemoryWrite {
    RoutineKey instruction;
    std::uint16_t logical_address = 0;
    std::uint16_t work_ram_offset = 0;
    std::uint8_t value = 0;
    std::uint64_t frame_number = 0;
    std::uint32_t master_cycle = 0;
    std::uint64_t absolute_master_cycle = 0;

    [[nodiscard]] bool operator==(
        const ObservedMemoryWrite&) const = default;
};

struct ObservedBusWrite {
    RoutineKey instruction;
    std::uint16_t logical_address = 0;
    std::uint8_t physical_bank = 0;
    std::uint8_t value = 0;
    std::uint64_t frame_number = 0;
    std::uint32_t master_cycle = 0;
    std::uint64_t absolute_master_cycle = 0;

    [[nodiscard]] bool operator==(const ObservedBusWrite&) const = default;
};

using WorkRamWriteCallback =
    std::function<void(const ObservedMemoryWrite&)>;
using BusWriteCallback = std::function<void(const ObservedBusWrite&)>;

struct RoutineCall {
    RoutineEntry entry;
    RoutineKey return_instruction;
    CpuRegisters output_registers;
    std::uint16_t caller_pc = 0;
    std::uint16_t return_pc = 0;
    std::uint32_t return_master_cycle = 0;
    std::uint64_t return_absolute_master_cycle = 0;
    std::uint64_t body_master_cycles = 0;
    std::vector<ObservedMemoryWrite> work_ram_writes;

    [[nodiscard]] bool operator==(const RoutineCall&) const = default;
};

// One executed instruction address and the master cycles attributed to it.
// Cycles are the span between this instruction's entry and the next observed
// instruction's entry, so a natively replaced routine attributes its whole
// body to its entry address. Profile with replacements disabled to rank the
// code that is still emulated.
struct InstructionProfileEntry {
    RoutineKey instruction;
    std::uint64_t executions = 0;
    std::uint64_t master_cycles = 0;

    [[nodiscard]] bool operator==(
        const InstructionProfileEntry&) const = default;
};

using RoutineCallCallback = std::function<void(const RoutineCall&)>;

struct RoutineCallObserver {
    RoutineKey routine;
    RoutineCallCallback callback;
};

// How the runtime handled replacements offered while another replacement was
// still in progress. A transcript is suspended for as long as an interrupt
// asserted inside it is being serviced, and the handler is ordinary code that
// may have a replacement of its own: `nested` counts those. `overflowed`
// counts replacements the runtime refused because too many transcripts were
// already suspended; the original assembly runs instead, so the recording stays
// exact, but a caller that credits a native call when it offers one is then
// reporting a native share slightly larger than the one it measured.
struct ReplacementNesting {
    std::uint64_t nested = 0;
    std::uint64_t overflowed = 0;
    // High-water mark of simultaneously suspended transcripts.
    std::uint64_t deepest = 0;
    // Transcripts the runtime gave up on part-way through, because the machine
    // did not return to a resume point in the state the transcript declared.
    // The original assembly runs from the last checkpoint applied, so this
    // costs cycles and never correctness -- but a transcript that declared
    // something it has not yet done, such as absorbing its own return, has to
    // have that declaration withdrawn, and this is how often that happens.
    std::uint64_t abandoned = 0;

    [[nodiscard]] bool operator==(const ReplacementNesting&) const = default;
};

// Why the runtime gave a transcript up. The three values are the three places
// the decision is made, and they are not variations on one fault: the first is
// the machine disagreeing with something the transcript declared, and the other
// two are the transcript unable to keep something it declared.
enum class AbandonmentReason : std::uint8_t {
    // The machine came back to a resume point carrying a register the
    // checkpoint did not declare. The transcript was right about where control
    // would return and wrong about the state it would return in, which makes
    // this a fact about an interrupt handler rather than about the transcript.
    registers = 1,
    // A checkpoint asked to advance to an offset the runtime could not deliver:
    // a write due before the cycle already spent, or a stalling-port wait wider
    // than the offset the write was priced into. The transcript mispriced its
    // own body.
    advance = 2,
    // A checkpoint was asked for when none was left. Reaching the last
    // checkpoint is completion, so this is a transcript being resumed that has
    // nothing further to say.
    exhausted = 3,
};

// Which registers disagreed, for an abandonment whose reason is `registers`.
// More than one may be set. The program counter and the stack pointer are not
// among them: they are how a resume point is recognised at all, so a machine
// that returns with either changed never reaches the comparison.
enum class RegisterMismatch : std::uint8_t {
    none = 0x00,
    accumulator = 0x01,
    x = 0x02,
    y = 0x04,
    status = 0x08,
    mpr = 0x10,
};

// One abandonment site: a transcript, the address the original assembly was
// interpreted from afterwards, and why.
//
// The two addresses are different questions and both are needed. `routine` is
// where the transcript was installed, which says who owns the fault; `resume`
// is the last checkpoint it managed to apply, which is where the emulator picks
// the routine up and therefore where the cycles this costs are spent. Neither
// is derivable from the other: a transcript covers a whole routine and may be
// given up at any instruction in it.
struct AbandonmentSite {
    RoutineKey routine;
    RoutineKey resume;
    AbandonmentReason reason = AbandonmentReason::registers;
    // The set of `RegisterMismatch` bits, zero for the other two reasons.
    std::uint8_t mismatch = 0;
    std::uint64_t count = 0;

    [[nodiscard]] bool operator==(const AbandonmentSite&) const = default;
};

struct ExecutionAccounting {
    enum Owner : std::size_t {
        translated, retained, fallback, interrupt, idle, device, other, owner_count
    };
    std::uint64_t start_master_cycle = 0;
    std::uint64_t elapsed_master_cycles = 0;
    std::array<std::uint64_t, owner_count> cycles{};
    std::array<std::uint64_t, owner_count> opcode_dispatches{};
    std::uint64_t translated_installs = 0;
    std::uint64_t retained_offers = 0;
    std::uint64_t rejected_offers = 0;
    std::uint64_t interrupt_entries = 0;
    std::uint64_t native_interrupt_entries = 0;
    std::array<std::uint64_t, 5> native_block_moves{}; // TIA, TAI, TDD, TII, TIN
    std::uint64_t native_block_resumes = 0;
    std::uint64_t native_block_cycles = 0;
};

class PceRuntime {
public:
    virtual ~PceRuntime() = default;

    // Exclusive clock owners since load/reset/restore, including executed
    // portions of interrupted or abandoned transcripts, never whole offers.
    [[nodiscard]] virtual std::optional<ExecutionAccounting>
    execution_accounting() const { return std::nullopt; }

    // A real runtime is thread-confined. Create, call, and destroy it on the
    // same owning thread; libretro callbacks execute synchronously there.
    [[nodiscard]] virtual RuntimeInfo info() const = 0;
    [[nodiscard]] virtual RomLoadResult load_rom(
        const std::filesystem::path& path) = 0;
    virtual void reset() = 0;
    [[nodiscard]] virtual EmulatedFrame run_frame(
        const InputFrame& input) = 0;

    // Backup RAM is copied across this boundary; callers never receive a
    // mutable view into emulator-owned memory. These methods are available
    // only after a ROM has loaded successfully.
    [[nodiscard]] virtual std::size_t backup_ram_size() const noexcept = 0;
    [[nodiscard]] virtual std::vector<std::byte> export_backup_ram()
        const = 0;
    [[nodiscard]] virtual bool import_backup_ram(
        std::span<const std::byte> bytes) = 0;

    // Work RAM crosses this boundary as an exact-size copy. Import is
    // intentionally explicit so deterministic diagnostic recordings and
    // later typed game-state views cannot retain mutable emulator pointers.
    // Both operations are unavailable until a ROM is loaded.
    [[nodiscard]] virtual std::size_t work_ram_size() const noexcept = 0;
    [[nodiscard]] virtual std::vector<std::byte> export_work_ram() const = 0;
    [[nodiscard]] virtual bool import_work_ram(
        std::span<const std::byte> bytes) = 0;

    // A copied, frame-boundary view of the base PC Engine VDC and VCE state.
    // VRAM is represented in the HuC6270's 16-bit word-addressed form.
    [[nodiscard]] virtual std::optional<VideoMemorySnapshot>
    export_video_memory() const = 0;

    // Hooks are keyed by exact physical-bank/logical-PC pairs and execute
    // after opcode fetch but before instruction execution. Each key may be
    // registered once. Pass-through and observe hooks cannot modify core
    // state; observe mode requires a callback. Replace mode requires a
    // transactional callback. Returning no replacement continues in the
    // original assembly. A replacement supplies final registers, ordered
    // writes with entry-relative timing, the body-cycle budget, and the
    // outer RTS at which emulation resumes.
    //
    // Replacement callbacks receive an isolated work-RAM snapshot plus the
    // entry's conservative master-cycle horizons to the next user/video
    // event and serviceable timer interrupt. Broad semantic schedulers
    // decline transactions that reach either horizon; exact restored-
    // assembly fixtures may separately admit a proven no-interrupt call.
    // Hardware events advance at declared write and body-cycle boundaries.
    [[nodiscard]] virtual bool install_execution_hook(ExecutionHook hook) = 0;
    [[nodiscard]] virtual bool remove_execution_hook(
        RoutineKey routine) noexcept = 0;
    virtual void clear_execution_hooks() noexcept = 0;
    [[nodiscard]] virtual std::size_t execution_hook_count()
        const noexcept = 0;

    // Counted from the start of the process, across every hook installation.
    [[nodiscard]] virtual ReplacementNesting replacement_nesting()
        const noexcept = 0;

    // A call observer begins at one exact routine entry and completes before
    // its outer RTS executes. It captures pre-return registers, the return
    // target, body master cycles (excluding RTS), and ordered writes to the
    // 8 KiB PCE work-RAM region, including writes made by nested callees.
    // Observation is read-only and callbacks run synchronously on the owner
    // thread.
    [[nodiscard]] virtual bool install_routine_call_observer(
        RoutineCallObserver observer) = 0;
    [[nodiscard]] virtual bool remove_routine_call_observer(
        RoutineKey routine) noexcept = 0;
    virtual void clear_routine_call_observers() noexcept = 0;
    [[nodiscard]] virtual std::size_t routine_call_observer_count()
        const noexcept = 0;

    // An optional whole-ROM instruction cycle profile. Unlike execution hooks
    // it is not keyed, so it ranks code nobody has thought to observe yet.
    // Enabling it is only available after a ROM has loaded; it counts every
    // executed instruction and costs roughly the same as one observation hook.
    [[nodiscard]] virtual bool set_instruction_profiling(bool enabled) = 0;
    [[nodiscard]] virtual bool instruction_profiling_enabled()
        const noexcept = 0;
    // Entries are returned in descending master-cycle order. Addresses that
    // never executed are omitted.
    [[nodiscard]] virtual std::vector<InstructionProfileEntry>
    export_instruction_profile() const = 0;

    // The subset of the whole-ROM profile whose instruction interval actually
    // ran in the HuC6280 interpreter.  A successful native transcript still
    // has an entry boundary, but its body is deliberately excluded here.  This
    // is the actionable residue when the registry is deployed.
    //
    // Entries are returned in descending master-cycle order. Addresses that
    // never ran interpreted are omitted.
    [[nodiscard]] virtual std::vector<InstructionProfileEntry>
    export_interpreted_instruction_profile() const = 0;

    // Where the abandonments counted by `ReplacementNesting::abandoned`
    // happened, grouped by transcript, resume address and reason, in descending
    // count order. Recorded whenever replacements are deployed and costs
    // nothing until one is abandoned, so there is no flag to enable.
    //
    // The counts sum to `abandoned` less any transcript the runtime declined to
    // name. Only a checkpointed transcript can be abandoned and only a named
    // one is reported, so the difference is transcripts installed before this
    // runtime issued their name -- which is to say, those restored from a
    // savestate written by somebody else.
    [[nodiscard]] virtual std::vector<AbandonmentSite>
    export_abandonment_census() const = 0;

    // One optional global observer receives every write to the physical
    // 8 KiB PCE work-RAM bank. It is intended for selective state-writer
    // censuses; callers should filter logical addresses in the callback.
    [[nodiscard]] virtual bool set_work_ram_write_observer(
        WorkRamWriteCallback callback) = 0;
    virtual void clear_work_ram_write_observer() noexcept = 0;
    [[nodiscard]] virtual bool has_work_ram_write_observer()
        const noexcept = 0;

    // One optional read-only observer receives every CPU bus write. It is
    // intended for bounded hardware-port mapping; callbacks must filter
    // aggressively and cannot modify the emulated bus.
    [[nodiscard]] virtual bool set_bus_write_observer(
        BusWriteCallback callback) = 0;
    virtual void clear_bus_write_observer() noexcept = 0;
    [[nodiscard]] virtual bool has_bus_write_observer()
        const noexcept = 0;

    // Replaces the emulator's programmable sound generator with the native
    // device model in `devils_crush::audio`. The model computes the same
    // band-limited deltas from the same register writes and the same clock,
    // so the audio a frame yields does not change -- what changes is who
    // owns the device. The emulator retains only sample-rate conversion,
    // which is a property of the host rather than of the PC Engine.
    //
    // May be set before or after a ROM loads, but not mid-frame.
    [[nodiscard]] virtual bool set_native_psg(bool enabled) = 0;
    [[nodiscard]] virtual bool native_psg_enabled() const noexcept = 0;

    // Which of the model's output paths the run just executed, or nothing
    // when the emulator owned the device. A route exercises only what the
    // game asks the chip for, and this is how much of it that was.
    [[nodiscard]] virtual std::optional<audio::PcePsg::Coverage>
    native_psg_coverage() const = 0;

    // Experimental Devil's Crush-specific extension: runs the six persistent
    // music voices on a second native PSG and mixes back only voices displaced
    // by the three higher-priority effect slots. The ROM bytes supply the
    // cartridge's original waveform table; they are copied during install.
    // Enable it before loading the ROM so the core can bind both PSGs to the
    // same pair of resampler buffers.
    [[nodiscard]] virtual bool set_music_voice_overlay(
        bool enabled,
        std::span<const std::byte> rom) = 0;
    [[nodiscard]] virtual bool music_voice_overlay_enabled()
        const noexcept = 0;

    // Replaces the emulator's video display controller with the native device
    // model in `devils_crush::video`. The colour encoder keeps the schedule --
    // the dot-clock divider, the two blanking counters, the palette and the
    // framebuffer are still the emulator's -- and only the chip that answers
    // its clocks and its port accesses changes.
    //
    // This is a wider seam than the sound one. The device is read back, so
    // reads route as well as writes; it stalls the CPU, so the model is given
    // the encoder's cycle budget to stall against; and its state is 128 KiB
    // that a savestate has to carry, so installing it before a state is
    // written and after one is read are the same operation.
    //
    // May be set before or after a ROM loads, but not mid-frame. Installing
    // it does not reset it: the model takes its initial state from the
    // machine's next power-on or state load, which is why the host installs
    // before the ROM.
    [[nodiscard]] virtual bool set_native_vdc(bool enabled) = 0;
    [[nodiscard]] virtual bool native_vdc_enabled() const noexcept = 0;

    [[nodiscard]] virtual std::optional<video::PceVdc::Coverage>
    native_vdc_coverage() const = 0;

    // Replaces the emulator's colour encoder with the native device model in
    // `devils_crush::video`. Wider again than the controller's seam, in two
    // ways that matter.
    //
    // The model owns the schedule. It decides how the master clock divides
    // into dot clocks, when the blanking counters flip, when a scanline
    // advances, and where the emulator's processor is allowed to stop -- so
    // installing it moves the machine's video timing across the boundary, not
    // just a chip's answers.
    //
    // And it draws. The frontend's framebuffer, its pitch and its line-width
    // array are handed over every frame and written directly. What a nine-bit
    // palette entry looks like on a screen, how a multi-resolution frame is
    // rescaled, and which rectangle to show stay with the emulator, because
    // those are the display's decisions rather than the chip's.
    //
    // Independent of the controller. Either may be installed without the
    // other: the encoder finds a native controller when one is registered and
    // the emulator's chip when none is. Handing one back while keeping the
    // other is what makes a difference in the picture attributable.
    [[nodiscard]] virtual bool set_native_vce(bool enabled) = 0;
    [[nodiscard]] virtual bool native_vce_enabled() const noexcept = 0;

    [[nodiscard]] virtual std::optional<video::PceVce::Coverage>
    native_vce_coverage() const = 0;

    // Replaces the HuC6280's timer and interrupt controller with the native
    // device model in `devils_crush::cpu`. The narrowest seam of the four and
    // the only one inside the processor rather than beside it: there is no
    // device object to make dormant, only a dozen private fields.
    //
    // The model owns the divider, the counter, the request register and the
    // mask, and the processor keeps a read-only mirror of the three of those
    // it reads once per instruction. What stays with the processor is the
    // *taking* of an interrupt -- which vector wins, when the line is sampled,
    // and how the I flag gates it -- because those are decided at instruction
    // boundaries and this device has no notion of an instruction. That split
    // is what lets the controller be native while the processor is not.
    //
    // May be set before or after a ROM loads, but not mid-frame, and handing
    // it back is only safe before a machine that will reach its own power-on:
    // the mirror carries three of the device's values back to the emulator
    // and the other four have no mirror to carry.
    [[nodiscard]] virtual bool set_native_irq(bool enabled) = 0;
    [[nodiscard]] virtual bool native_irq_enabled() const noexcept = 0;

    [[nodiscard]] virtual std::optional<cpu::HuC6280Irq::Coverage>
    native_irq_coverage() const = 0;

    // Replaces the HuC6280's clock and event scheduler with the native device
    // model in `devils_crush::cpu`. The fifth seam and the first whose shape
    // is a block of memory rather than a call table: the model hands the
    // processor its register block and the processor reads the clock, the
    // near horizon and the loop flag straight out of it, because all three are
    // read from inside the instruction loop.
    //
    // Every *change* to that block is still a call on the model -- the two
    // advances above all, which is where everything the machine does to time
    // arrives. What stays with the processor is execution: fetching an opcode
    // and deciding at an instruction boundary that an interrupt has arrived.
    //
    // Must be set before a ROM loads. This is stricter than the four before
    // it, and for a reason those did not have: installing moves where the
    // machine's clock lives, so a machine already running would find its clock
    // replaced underneath it rather than merely its device handed over.
    [[nodiscard]] virtual bool set_native_scheduler(bool enabled) = 0;
    [[nodiscard]] virtual bool native_scheduler_enabled() const noexcept = 0;

    [[nodiscard]] virtual std::optional<cpu::HuC6280Scheduler::Coverage>
    native_scheduler_coverage() const = 0;

    // Replaces the decision the HuC6280 makes between two instructions with
    // the native model in `devils_crush::cpu`. The sixth seam and the first
    // that is a rule rather than a device: the model holds the two values a
    // processor latches at the end of an instruction and otherwise computes.
    //
    // So this is the one install that hands over no storage and leaves no
    // mirror. The processor passes in the request register, the mask and its
    // own I flag -- values it already holds -- and takes back which of the
    // four vectors won, or that the next thing is simply the next
    // instruction. What stays with the processor is the *taking*: eight
    // cycles against its own stack, flags and memory map.
    //
    // Must be set before a ROM loads, for the narrowest reason of the six:
    // the reset vector a machine takes at power-on is the only thing that
    // ever puts a reset into the latch.
    [[nodiscard]] virtual bool set_native_boundary(bool enabled) = 0;
    [[nodiscard]] virtual bool native_boundary_enabled() const noexcept = 0;

    [[nodiscard]] virtual
    std::optional<cpu::HuC6280InterruptBoundary::Coverage>
    native_boundary_coverage() const = 0;

    // Lets the machine advance through a frame-sync spin instead of executing
    // it, one whole iteration at a time, up to but never reaching the
    // scheduler's near horizon.
    //
    // The seventh seam and the only one that replaces nothing. The six before
    // it each moved work across the boundary and each had a rule about when
    // they could be installed; this one moves no work, holds no state the
    // machine needs, and can therefore be installed or handed back at any
    // point. A machine that never reaches the address it watches behaves
    // exactly as though it were absent.
    //
    // Which address, and the four bytes that must be there, is the caller's
    // claim about a cartridge. Passing no loop hands the capability back.
    [[nodiscard]] virtual bool set_native_idle_skip(
        std::optional<cpu::HuC6280IdleSkip::Loop> loop) = 0;
    [[nodiscard]] virtual bool native_idle_skip_enabled() const noexcept = 0;

    [[nodiscard]] virtual std::optional<cpu::HuC6280IdleSkip::Coverage>
    native_idle_skip_coverage() const = 0;

    // Replaces the emulator's controller port with the native device model in
    // `devils_crush::input`. The eighth seam, and the one that moves the least
    // computation: the strobe protocol was already native inside the
    // cartridge's own pad reader, and what moves here is where the input comes
    // from.
    //
    // While the model is installed the `InputFrame` given to `run_frame`
    // reaches the machine directly. The emulator's input path -- its frontend
    // button buffer and its five `PCE_Input_Device` objects -- is not
    // consulted, and this runtime proves it rather than asserting it: the
    // libretro input callback answers zero for every query while a model is
    // installed, so a frame that still matched the emulated arm through that
    // callback could not have been produced by it.
    //
    // May be set before or after a ROM loads, but not mid-frame. Handing it
    // back mid-machine costs one strobe: the emulator's devices have received
    // nothing while the model ran, so their lines are stale until the pad
    // reader's next write and their buttons until the next frame.
    [[nodiscard]] virtual bool set_native_pad(bool enabled) = 0;
    [[nodiscard]] virtual bool native_pad_enabled() const noexcept = 0;

    [[nodiscard]] virtual std::optional<input::PcePad::Coverage>
    native_pad_coverage() const = 0;

    // Savestates are opaque Beetle payloads plus the host frame counter.
    // Persistent storage must additionally bind them to the ROM and core
    // revision before calling deserialize().
    [[nodiscard]] virtual SaveState serialize() const = 0;
    [[nodiscard]] virtual bool deserialize(const SaveState& state) = 0;
};

// Beetle PCE and libretro are single-instance APIs. A real runtime throws
// std::logic_error if a second instance is created before the first is
// destroyed. The dependency-free stub does not reserve emulator state.
[[nodiscard]] std::unique_ptr<PceRuntime> create_pce_runtime();

} // namespace devils_crush::emulation
