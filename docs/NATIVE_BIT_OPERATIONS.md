# Native HuC6280 bit operations

Patch `0052-huc6280-native-bit-operations.patch` implements 29 opcodes through
`cpu::run_huc6280_bit_operations`: five BIT addressing forms, four TST forms,
eight RMB and eight SMB operations, and zero-page/absolute TRB and TSB.
Validated transcripts retain precedence, frame-wait instructions retain idle
ownership, and `--emulated-boundary` retains the pinned original opcode oracle.

## Live ordering

BIT samples interrupts before its final cycle and data read. Its immediate
form also copies operand N/V, matching the HuC6280 core. TST first reads its
immediate mask and advances three cycles, then performs the address reads and
ordinary load sequence. Absolute indexing stays wide; zero-page indexing wraps
at eight bits. BIT/TST leave A unchanged and publish NZV after the data read.

TRB/TSB read memory and publish flags before sampling interrupts, then write
after the final cycle. They copy N/V from the original memory byte and set Z
from the modified byte. RMB/SMB preserve flags, advance an extra cycle after
the data read, then sample, advance the final cycle and write. All instructions
clear T at retirement without redirecting the operation through zero page X.

`Native bit test/update operations` reports actual starts and exclusive CPU
cycles already included in translated ownership. Device stalls remain separate.
The fields append to the accounting ABI, reset with its window and are not
serialized; save-state format is unchanged.

## Measured coverage

Across the same 31 routes as the arithmetic baseline, this batch transfers the
final **2,010 CPU master cycles** over **102 instruction starts** from fallback
to translated ownership. Weighted native execution outside wait/device time
increases from **99.9999903% to exactly 100%**.

| Exclusive CPU owner | Before | After |
| --- | ---: | ---: |
| Translated | 20,647,772,790 | 20,647,774,800 |
| Retained | 0 | 0 |
| Fallback | 2,010 | 0 |
| Original IRQ entry | 0 | 0 |
| Other/unaccounted | 0 | 0 |

All 31 routes preserve frame hashes, total elapsed time, device stalls, idle
time, IRQ counts, earlier native counters and transcript abandonments. Every
route has zero interpreter intervals outside the frame-sync wait. Alley moves
81 cycles over four starts and reaches 675,366,888 native CPU cycles out of
675,366,888 eligible cycles, preserving its existing frame hash.

This is complete measured execution ownership for this corpus. It does not
establish coverage of every possible gameplay path or removal of the pinned
core's device implementations. The next step is broader cabinet captures and
rare-path validation, ranking any newly observed fallback with the exclusive
ledger rather than treating a rounded percentage as proof of completion.

## Verification

The differential test copies the pinned core's operation, addressing and opcode
macros into an independent oracle. Its 15,204,352 cases cover all 29 opcodes,
all status bytes and operand values, PC and indexed-address wrapping, memory
aliasing, both CPU speeds, fractional clock remainders, device stalls and
interrupt horizons. It compares the complete bus/clock/sample event stream,
RAM, registers, flags, interrupt latch and elapsed timing.

The accounting replay requires zero retained and fallback CPU cycles in both
native scheduler modes on the pinned alley route. The original-boundary mode
must still exercise both owners. Exact owner sums, exclusive cycle transfers,
instruction-start normalization, frame hashes and IRQ counts remain checked.

Release: all six selected checks pass, including differential, accounting,
state restoration, ranked VDC, corrupted-face and upper-hand bus captures.
Debug: all three checks pass: differential, accounting and state restoration.
Registry generation remains current at 932 admissions; all 399 execution
seams and the reverse patch-application check pass.

Artifacts: `out/native-bit-operations/corpus/{manifest.csv,totals.json}`,
per-route frames/profiles/logs, `tests-release.log`, `tests-debug.log`, and
`clean-build-{release,debug}.log`. The complete rebuild removes stale objects
from the intermediate accounting layout; the final ABI appends both fields.
