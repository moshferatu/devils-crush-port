# Native HuC6280 control flow

Patch `0041-huc6280-native-control-flow.patch` executes JSR, BSR, RTS and all
three JMP addressing modes through `cpu::run_huc6280_control_flow`. Existing
accepted transcripts retain precedence. The shared engine handles remaining
calls, returns and jumps across banks without per-address admissions.
`--emulated-boundary` retains the original opcode bodies as an independent
gameplay oracle.

The native routine owns operand decoding, stack arithmetic, target calculation
and T-flag retirement. The adapter supplies live mapped reads and writes,
clock advances and interrupt sampling. Execution bypasses the original opcode
switch and its PUSH, POP, GetAB and JR helpers.

## Ordering and accounting

- JSR fetches its low operand, advances PC, pushes the return address high then
  low, and only then reads its high operand. Stack writes may overwrite operands.
- BSR pushes its return address before advancing four cycles and fetching its
  signed displacement. It advances another three cycles before changing PC.
- RTS pops low then high, increments PC, and advances six cycles.
- Absolute JMP uses a sixteen-bit operand cursor. Indirect jumps use a wide
  operand PC and pointer, including the indexed pointer and its high-byte read.
  Absolute JMP advances three cycles; indirect jumps advance six.
- All six variants sample interrupts before their final cycle and clear T
  after retirement. Stack wrap, both CPU speeds, divided clock remainders and
  device stalls follow the pinned core. No savestate fields are added.

The excluded frame-wait range retains idle ownership, including its return.
This keeps the useful-work denominator identical between native and original
engines. `Native control flow` counts shared-engine starts and exclusive CPU
cycles, already included in `translated`. Device stalls remain separate.
Counters reset with the accounting window and are not serialized. The profiler
marks these instructions native only when the shared engine actually runs.

## Measured coverage

The preceding profiles ranked RTS, absolute JMP and JSR highest at about
15.9 million elapsed instruction-interval cycles outside the wait. The exclusive
ledger measures **15,804,648 CPU master cycles** transferred to native execution,
or **60.1335%** of the remaining non-native CPU time.

| Measurement | Before | After |
| --- | ---: | ---: |
| Native CPU cycles | 20,621,492,181 | 20,637,296,829 |
| Retained opcode cycles | 19,151,889 | 8,754,585 |
| Fallback opcode cycles | 7,130,730 | 1,723,386 |
| Remaining non-native CPU cycles | 26,282,619 | 10,477,971 |
| Native share outside waits/stalls | 99.8727% | 99.9493% |

The denominator remains **20,647,774,800 master cycles**. All 31 routes preserve
frame hashes, device stalls, idle time, IRQ counts, native block/bit-branch
counters and abandonments. Other and unaccounted time remain zero. Original
dispatches decrease by exactly **938,790**, the number of shared native
control-flow starts. The three pre-existing single abandonments are unchanged.
No covered control-flow opcodes remain in mapped-ROM interpreter profiles
outside the excluded wait range.

Alley transfers **575,337** CPU cycles across **33,661** shared control-flow
instructions, reaching **99.9348%** native execution. Native and original engines
preserve the pinned frame hash and normalized ownership under both schedulers.

The remaining profiles rank absolute LDA (4,308,504 interval cycles), PLP
(2,793,552) and SEI (2,040,336) highest. These shared paths are the next priority.
These interval figures guide selection; they are not exclusive CPU totals.
Corpus coverage describes exercised routes, not completeness of unvisited paths.

## Verification

`dc_control_flow` compares **1,572,864** cases against independently expanded
pinned opcode bodies. Cases cover all six variants, every stack position,
status byte, index and signed displacement, both speeds, overclock division,
injected bus stalls, interrupt arrival horizons, address wrap and stack/operand
aliasing. It compares every bus operation, clock batch and interrupt sample,
intermediate PC/P/SP, final registers, all RAM and clock remainder.

`dc_execution_accounting_state` checks restored control-flow counter deltas.
`dc_rom_execution_accounting` compares native and original engines under both
schedulers: pinned frames, normalized exclusive ownership and instruction
starts must agree. The complete route corpus additionally compares frames,
stalls, idle time, IRQs, earlier native-engine counters and abandonments against
the preceding bit-branch batch.

Release: all six selected checks pass, including the five-hash ranked-VDC
oracle and the corrupted-face/upper-hand captures. Registry generation remains
current at 932 admissions and all 399 seam ranges validate.

Debug: all three checks pass: differential control flow, state accounting and
the three-mode gameplay accounting replay.

Artifacts are under `out/native-control-flow`; the pre-change executable is
`devils-crush-before.exe`, and corpus logs/profiles/frames are under `corpus`.
