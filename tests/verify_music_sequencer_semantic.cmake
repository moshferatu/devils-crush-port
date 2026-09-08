# SPDX-License-Identifier: GPL-3.0-or-later
# DC_NATIVE_ADMISSION 45:4143 3 42
# Retain the separately replay-verified PSG continuation from before the
# horizon prefix made its standalone arrivals disappear on this family route.

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release" AND NOT CONFIG STREQUAL "Debug")
    message(FATAL_ERROR "Unsupported music-sequencer configuration: ${CONFIG}")
endif()

# The bank-45 music sequencer. It is a separate component from the timer sound
# driver at `00:FC00`: the driver streams DDA samples, the sequencer decides
# what they are, programming the PSG register file and maintaining the
# per-voice records in $2E00-$2FFF.
#
# Its four public vectors at `4000`, `4003`, `4006`, and `4009` are small
# core-owned `JMP` transfers.  Keep them in this family regression as well:
# they are the sequencer's external entry seam, while the switch below owns
# the larger work after the vectors land.
#
# Twenty-six entry points share one walk: the per-frame tick at `4015`, which
# is the bank's top level, and the subroutines it calls.
#
# The tick was recorded as unportable for four slices, on the arithmetic that
# its 12,667-cycle body cannot finish inside the 5,068-cycle timer period. The
# arithmetic is right and it is about a transcript, not a routine. The tick's
# body is a loop; `4044` is its head and `40FF` closes it with a `JMP $4044`,
# so the walk stops there once one more voice iteration would not fit inside
# the horizon it was handed, the core takes the jump and the interrupt, and the
# next chunk starts against a fresh one. 24,667 of the route's 32,391 voice
# iterations now run inside a transcript.
#
# Four entry points exist because the walk stops in front of a call: `409C`
# resumes after `4936`, which is not transcribed here at all; `409F` resumes
# after an externally-entered `48F6` sweep update; `40A2` resumes after an
# externally-entered `49A2` vibrato update; and `413B` resumes after `4142`.
# `4142` is transcribed and is still handed back deliberately, so that it is
# entered on its own and keeps the driver-state guard it already carries
# instead of imposing it on the whole tick.
#
# Composing the rest is ordinary control flow rather than a spliced child
# transcript, because every callee is a case in the same switch. That is why
# the subroutine counts below fell so far -- 83,417 of their calls are now
# inlined into a tick transcript. `4883` is 4,321 rather than 22,664 for the
# same reason `48B4` was always 72. `48B4` is the command decoder
# `4883` and `48F6` call, so it is hooked in its own right but only sees the
# calls made from emulated code: the 25,896 calls made from inside a `4883`
# or `48F6` transcript are inlined into that transcript instead, which is why
# its admitted count is the small remainder. `457A`, the command interpreter,
# inlines four callees the same way -- the $42BA dispatch at `4569`, the
# balance programmer `4522`, the note-to-period conversion `46D7`, and the
# handlers themselves -- so none of those is a separate entry.
#
# Every one of the twenty-six stops in `is_return_instruction` is absorbed
# rather than handed to the core: the walk runs the `RTS` itself and the
# transcript ends on the caller's side of it. That took the bank from 644,397
# emulated return cycles to 68,481 in a single condition, and it moves nothing
# this test asserts. An `RTS` writes nothing, so the write and bus traces below
# are untouched, and the absorbed-return declaration reproduces the boundary
# the observer would otherwise have recorded, so the counts are untouched too.
# All three hashes and every count here are the same either side of it, which
# is the strongest thing that can be said for the change.
#
# The entry points admitted at less than 100% are these.
#
#   - `4142` reads the timer driver's channel bank bytes at $F5/$F7/$F9, and
#     the amplitude commit at `49D2` reaches $20F5,Y -- which both `49B6` and
#     `49F9` run, so every voice iteration touches it. Those bytes are
#     written by another thread of control, and a transcript reads them from a
#     snapshot taken before it runs, so the walk declines unless either the
#     interrupt mask is already set at entry -- `4142` brackets each voice in
#     PHP/SEI/PLP, so a masked entry stays masked throughout -- or the body
#     finishes before the next serviceable timer interrupt.
#
#     This guard is the whole reason the tick chunks. It is not a cap on
#     transcript length; it is a deadline, and the only way to meet a deadline
#     is to do less before it. `4015` and `4044` are admitted on roughly half
#     their arrivals because a chunk boundary is by construction the moment
#     the horizon is nearly spent, so the entry immediately after one often
#     cannot fit an iteration either and hands that one back to assembly.
#
#     `4142` used to be admitted on half its arrivals for the same reason,
#     and was left there on the same kind of arithmetic the tick had been:
#     a whole call averages 2,055 master cycles against a 5,068-cycle timer
#     period, so about half of them do not fit. That is an argument about one
#     transcript, not about the routine. `4142` is a loop as well -- `4143` is
#     its head and `41F7` closes it with a `JMP $4143` -- so it chunks the same
#     way, and `4143` is the loop-head entry point. It is a legal boundary
#     rather than an invented one: ROM's own `PLP` at `41F1` has already
#     restored the caller's mask before control reaches `41F7`, so the top of
#     an iteration is somewhere the hardware may take an interrupt anyway.
#     The later read-horizon guard admitted 3,562 of 3,604 at `4142`.
#     Stopping before $4148 when that read cannot beat the horizon now
#     preserves all 3,604 prefixes. There are no standalone $4143 arrivals
#     on this route, so its hook is omitted (unused hooks fail the recorder).
#   - Sequence-end commands ($82/$92, and the $86 publishing variant) now
#     execute their four stack pops and terminate on the real JMP to $40EE.
#     The walk no longer needs to mistake this nonlocal exit for an RTS.
#     This allows 4015 to accept all 3,599 calls, including the 45 calls that
#     previously rejected their entire prefix. 4228, 457A and 49F9 consequently
#     have no standalone arrivals here; verify_music_command_exit keeps their
#     independent entry proof. Unsupported handlers and timer guards remain.
#
# Four work-RAM ranges and two bus ranges scope this, and all six artifacts
# are compared against the artifacts a pure-assembly run of the same route
# produces, so the writes match in value, address, order and master-cycle
# offset, not merely in end state.
#
# Two of the ranges are load-bearing rather than incidental. The stack range
# $21E0-$21FF caught a JSR whose two return-address writes were credited 21
# cycles late, because the core pushes them *before* `ADDCYC(6)` while every
# other store in the walk lands on its instruction's final cycle; it then
# caught a PLP that published the caller's flags at its checkpoint, so an
# interrupt landing on that exact instruction pushed the pre-pop status. The
# driver range $20DD-$20FB covers the sample trigger at $4776, which writes
# the driver's whole per-channel zero page. Neither the state census nor the
# device-write trace saw either fault.
#
# The pitch-slide stepper at `4936` is observed beside the command descriptor
# loader at `4BCC`. The loader composes the proven `4C42` record initializer,
# so the combined call artifact covers all 548 pitch-slide calls plus all 66
# complete command-loader calls from descriptor lookup through record setup.
#
# `469B` is intentionally replacement-only here. It is an internal branch
# target inside `457A`, not a JSR entry, so a standalone observer would match
# it to an unrelated outer return. The existing whole-family census, selected
# work-RAM trace, and PSG/timer bus trace are the correct assembly oracle for
# its note-duration, envelope-restart, cursor-advance, and final-return tail.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_write_sha256
    "a5701e57d182e9f5c50c46b5b3d4a2eddf3caa1d466a654301bb2bc336f7f21b"
)
set(
    expected_bus_sha256
    "b54d5c4ff41430623b0af9fe596d296aff7b84b4c5dd1ddc1af73eee26a6585e"
)
set(
    expected_call_sha256
    "0912b02a9a3db6410dfc1f58892373c2e93b35d36c34c8ed4a90d52ba0959b96"
)
set(
    expected_call_write_sha256
    "ad52399b7450be376fecfcc795fa866b3ed0791b45cd90a157955552c9ddfb0e"
)

set(write_output "${OUTPUT}.writes.csv")
set(bus_output "${OUTPUT}.bus.csv")
set(call_output "${OUTPUT}.calls.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --replace-hook
        "45:4000@all"
        --replace-hook
        "45:4003@all"
        --replace-hook
        "45:4006@all"
        --replace-hook
        "45:4009@all"
        --replace-hook
        "45:4015@all"
        --replace-hook
        "45:4044@all"
        --replace-hook
        "45:404F@all"
        --replace-hook
        "45:409C@all"
        --replace-hook
        "45:409F@all"
        --replace-hook
        "45:40A2@all"
        --replace-hook
        "45:413B@all"
        --replace-hook
        "45:4142@all"
        --replace-hook
        "45:4154@all"
        --replace-hook
        "45:4169@all"
        --replace-hook
        "45:4211@all"
        --replace-hook
        "45:469B@all"
        --replace-hook
        "45:47E0@all"
        --replace-hook
        "45:485E@all"
        --replace-hook
        "45:4883@all"
        --replace-hook
        "45:48F6@all"
        --observe-hook
        "45:4936"
        --replace-hook
        "45:4936@all"
        --replace-hook
        "45:49A2@all"
        --replace-hook
        "45:49B6@all"
        --replace-hook
        "45:49E4@all"
        --observe-hook
        "45:4BCC"
        --replace-hook
        "45:4BCC@all"
        --observe-hook
        "45:4B4E"
        --replace-hook
        "45:4B4E@all"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "45:4936@548"
        --routine-replay
        "45:4BCC@66"
        --routine-replay
        "45:4B4E@2"
        --work-ram-write-range
        "20D2-20D8"
        --work-ram-write-range
        "20DD-20FB"
        --work-ram-write-range
        "21E0-21FF"
        --work-ram-write-range
        "2E00-2FFF"
        --work-ram-write-trace
        "${write_output}"
        --bus-write-range
        "0800-0809"
        --bus-write-range
        "0C00-0C01"
        --bus-write-trace
        "${bus_output}"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Semantic music-sequencer recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

# Historical note: both columns moved when the driver-state guard stopped
# comparing a finished transcript against the interrupt horizon and compared the cycle of
# the read it is actually about. Admissions rose -- 45:4044 by four, 45:4142 by
# eighteen -- because a walk that read one of the driver's channel bank bytes
# early and ran on well past the horizon is no longer refused for something that
# could only happen long after the read.
#
# The *arrivals* fell at the same time, and that is the same fact seen from the
# other end: 45:4143 from 60 to 42, 45:409C from 896 to 892, 45:48F6 from 41 to
# 37. An admitted chunk covers ground the interpreter would otherwise have
# walked back into these heads over, so admitting more of them means arriving at
# them less often. A denominator that moves on its own would be a route change;
# these move against numerators that rose, and the census and both traces below
# are unchanged, which is what says the route did not. The sequence-end handoff
# further reduces child arrivals below without changing any artifact golden.
foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 45:4000@all entries: 1 of 1"
    "Native replacement 45:4003@all entries: 3582 of 3599"
    "Native replacement 45:4006@all entries: 58 of 66"
    "Native replacement 45:4009@all entries: 3 of 4"
    "Native replacement 45:4015@all entries: 3599 of 3599"
    "Native replacement 45:4044@all entries: 1417 of 1436"
    "Native replacement 45:404F@all entries: 19 of 19"
    "Native replacement 45:409C@all entries: 855 of 873"
    "Native replacement 45:409F@all entries: 12 of 18"
    "Native replacement 45:40A2@all entries: 4 of 6"
    "Native replacement 45:413B@all entries: 3599 of 3599"
    "Native replacement 45:4142@all entries: 3604 of 3604"
    "Native replacement 45:4154@all entries: 13 of 13"
    "Native replacement 45:4169@all entries: 11093 of 11093"
    "Native replacement 45:4211@all entries: 40 of 40"
    "Native replacement 45:469B@all entries: 224 of 224"
    "Native replacement 45:47E0@all entries: 36 of 36"
    "Native replacement 45:485E@all entries: 308 of 308"
    "Native replacement 45:4883@all entries: 325 of 325"
    "Native replacement 45:48F6@all entries: 18 of 18"
    "Native replacement 45:4936@all entries: 548 of 548"
    "Native replacement 45:49A2@all entries: 6 of 6"
    "Native replacement 45:49B6@all entries: 5 of 5"
    "Native replacement 45:49E4@all entries: 289 of 289"
    "Native replacement 45:4BCC@all entries: 66 of 66"
    "Native replacement 45:4B4E@all entries: 5 of 5"
    "Observe hook 1 entries: 548"
    "Observe hook 2 entries: 66"
    "Observe hook 3 entries: 5"
    "Routine call trace complete: 619 calls, 6006 writes"
    "Routine replay 45:4936@548 verified: frame 3407, 168 body cycles, 1 ordered writes"
    "Routine replay 45:4BCC@66 verified: frame 3466, 1800 body cycles, 45 ordered writes"
    "Routine replay 45:4B4E@2 verified: frame 1, 1476 body cycles, 22 ordered writes"
    "Routine replay complete: 3 state-normalized calls"
    "Work-RAM write trace complete: 5458568 selected writes"
    "Bus write trace complete: 699251 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic music-sequencer output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census write bus)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "write")
        set(artifact_path "${write_output}")
    else()
        set(artifact_path "${bus_output}")
    endif()

    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "Semantic music-sequencer ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

file(SHA256 "${call_output}" actual_call_sha256)
if(NOT actual_call_sha256 STREQUAL expected_call_sha256)
    message(
        FATAL_ERROR
        "Semantic music-sequencer call artifact changed.\n"
        "Expected: ${expected_call_sha256}\n"
        "Actual:   ${actual_call_sha256}\n"
    )
endif()

file(SHA256 "${call_output}.writes.csv" actual_call_write_sha256)
if(NOT actual_call_write_sha256 STREQUAL expected_call_write_sha256)
    message(
        FATAL_ERROR
        "Semantic music-sequencer call-write artifact changed.\n"
        "Expected: ${expected_call_write_sha256}\n"
        "Actual:   ${actual_call_write_sha256}\n"
    )
endif()

message(
    STATUS
    "Semantic bank-45 music sequencer runs its per-frame tick, command "
    "interpreter, sample/envelope group, portamento stepper and PSG register "
    "programmer and command descriptor/record loader natively across verified "
    "entries as instruction-boundary "
    "transcripts, the tick chunked at its voice loop and the PSG programmer at "
    "its channel loop so each chunk lands "
    "inside the timer horizon, with its voice records, the zero-page command "
    "pointer, the timer driver's per-channel state, the stack, all 699,251 PSG "
    "and timer writes, and the whole state census identical to assembly."
)
