# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Tower-insect controller regression is Release-only under the native-port cadence.")
endif()

# The dynamic Tower-insect controller, which is almost entirely its calls.
#
# Four runtime insects dispatch to 03:6016. It hands the collision selector at
# $3780,X to the table interpreter at 03:6966, branches to the animation leaf
# at 03:6054, and returns unless the interpreter left bit 7 of $3800,X set.
# Both callees have been native since this family was ported; the six
# instructions around them never were, which left the parent running through
# the interpreter to reach code that was already transcribed.
#
#   03:6016  read the selector, then make the `JSR $6966`
#   02:5FFB  read the fixed source selector, then make the same `JSR $6966`
#   03:601C  make the `BSR $6054`
#   03:601E  read the hit flag, take the no-hit branch, absorb the `RTS`
#
# None of the calls is composed. The interpreter is a data-driven walk of
# variable length and the animation leaf is a routine in its own right. Both
# are entered at the same address either way, so each walk crosses its call
# rather than bringing a body in line: push the return, charge the cycles, and
# end on the callee's first instruction. 03:601E ends past a return, and
# what that costs the routine-call observer is proved separately by
# dc_rom_absorbed_return_trace_dispatched.
#
# 6,487 of the 6,494 calls find bit 7 clear and take the branch. The other
# seven are hits and decline: the 10-unit award, audio command `$29`, the
# two-hit counter and the slot release stay with the assembly, so the rule is
# not what this replaces.
#
# What the scope proves. The whole stack page is in range rather than a window
# of it, because all three crossings push a return address there and the
# family's pushes land wherever the caller left S. The remaining three ranges
# are what the controller and its hit path write: the hit flag it clears at
# 03:6025, the two-hit counter it decrements at 03:6036, and the live-instance
# counter at 03:604E. Both artifacts were recorded from the same route with all
# five entries emulated, so the writes match in value, address, order and
# master-cycle offset rather than merely in end state -- and a pushed byte with
# the wrong value, the wrong order or the wrong offset breaks a trace made
# before this family pushed a byte of its own. `JSR` and `BSR` both push before
# charging a cycle, so both bytes carry the instruction's start offset and tie
# with the checkpoint in front of them; the core breaks that tie by
# instruction, and a walk that got it wrong would move the pair to the load.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_write_sha256
    "5a39a28c68fe8f9fc269bfaa600665544958e3cff6b6c21dd1d933cba3cb79d0"
)
set(
    expected_call_sha256
    "1e3173f878f158bd61715e3d1bff4736309a3f577a2fe77106dafedba38e7629"
)
set(
    expected_call_write_sha256
    "cfabe3531b0ad7a5f9cf78b668c3daef05c0bb34a9143abba093d828b9cb115d"
)

set(write_output "${OUTPUT}.writes.csv")
set(call_output "${OUTPUT}.calls.csv")
set(call_write_output "${call_output}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --replace-hook
        "03:6016@all"
        --replace-hook
        "02:5FFB@all"
        --replace-hook
        "03:601C@all"
        --replace-hook
        "03:601E@all"
        --replace-hook
        "03:6054@all"
        --observe-hook
        "02:5FFB"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "02:5FFB@1"
        --work-ram-write-range
        "2100-21FF"
        --work-ram-write-range
        "2D2B-2D2B"
        --work-ram-write-range
        "3800-38FF"
        --work-ram-write-range
        "3C00-3C7F"
        --work-ram-write-trace
        "${write_output}"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Semantic Tower-insect controller recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 03:6016@all entries: 6494 of 6494"
    "Native replacement 02:5FFB@all entries: 260 of 260"
    "Native replacement 03:601C@all entries: 6494 of 6494"
    "Native replacement 03:601E@all entries: 6487 of 6494"
    "Native replacement 03:6054@all entries: 6750 of 6750"
    "Observe hook 1 entries: 260"
    "Routine call trace complete: 260 calls, 4016 writes"
    "Routine replay 02:5FFB@1 verified: frame 958, 885 body cycles, 17 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
    "Work-RAM write trace complete: 4282022 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic Tower-insect controller output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS call call_write)
    if(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    else()
        set(artifact_path "${call_write_output}")
    endif()

    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "Semantic Tower-insect controller ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    else()
        set(artifact_path "${write_output}")
    endif()

    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "Semantic Tower-insect controller ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic Tower-insect controller runs all 6,494 dispatches natively, "
    "crosses all 260 fixed source collision calls, makes both calls the "
    "dynamic routine exists to make and lands on each callee, and returns "
    "on 6,487 dynamic calls, with every pushed return address, "
    "the whole stack page, the hit flags, the two-hit counters, the "
    "live-instance counter and the state census identical to assembly."
)
