# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported actor-scan-preamble configuration: ${CONFIG}")
endif()

# 01:C4DE-01:C50E, the actor scan's preamble. It settles the frame's actor
# enable mask, resets the three cursors the scan allocates list entries from,
# clears the scan index, and falls through into the scan itself.
#
# The mask lands in `$2CEE`, which 01:D466 loads and ANDs against each actor's
# `$3B80,X` attribute byte, so a zero there turns every actor off for the frame.
# Four gates can produce that zero -- `$2D35`, `$2CC7`, bit 3 of `<$53`, and on
# the arm this route has never taken a mask staged in `<$B7` instead. `$2CBA`
# and `$2CBB` are the two ends of one 128-entry list at `$2BBA`, which is why
# one is cleared and the other set to $7F; `$2CBC` indexes the separate list at
# `$2C3A`; `<$4D` is the scan index 01:C510 reads on its first instruction.
#
# One entry point and twenty instructions, with no call of any kind -- no `JSR`,
# no `JMP`, not one ROM read -- and every operand in work RAM. It ends by
# falling through into 01:C510, the registered actor scan: the transcript stops
# in front of an address this port already owns, so the hand-off costs no
# transcribed jump and returns nothing to the interpreter.
#
# 01:C4ED is the one arm handed back. `$2254` was zero on all 3,492 arrivals, so
# 01:C4ED and 01:C4EF have not executed once in 3,600 frames and the core is
# left to be right about them by construction. The two branches to 01:C4FC are
# transcribed instead: neither has been taken either, but 01:C4FC is live -- the
# fall-through from 01:C4FA reaches it 322 times -- so following a branch there
# lands on an instruction this walk already owns.
#
# `entries: 3492 of 3492` below is also the shared-state measurement. A
# transcript is computed from a snapshot, so its reads must all fall inside one
# interrupt horizon, and the adapter declines a body that reaches the deadline.
# None was declined, so no walk here spanned a serviceable interrupt: the whole
# routine is 20 instructions and about 222 master cycles against an interrupt
# period near 5,068 on this route.
#
# The write ranges below are exactly the six bytes this routine writes, and they
# are shared: 01:C53E and 02:C031 write `$2CEE` too, 01:C645 and 08:4075 step
# `$2CBA`, 01:C64D steps `$2CBB`, 01:C635 and 08:4063 step `$2CBC`, and 01:C523
# increments `<$4D` once per actor. So the trace interleaves this routine's
# writes with every other writer's, and a wrong value, a wrong order or a
# shifted cycle anywhere in the frame moves rows in it.
#
# The write filter matches the effective address, so zero page is named as
# $204D and $2056 rather than $4D and $56; written the short way the ranges
# select nothing at all. The range order is part of the artifact, because the
# trace is emitted in the order the ranges are given.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_write_sha256
    "e16d9ec0f33f86738312245c227af864025218d96d8616cb7bd3cef3649f46ea"
)

set(write_output "${OUTPUT}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --replace-hook
        "01:C4DE@all"
        --work-ram-write-range
        "204D-204D"
        --work-ram-write-range
        "2056-2056"
        --work-ram-write-range
        "2CBA-2CBC"
        --work-ram-write-range
        "2CEE-2CEE"
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
        "Semantic actor-scan-preamble recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 01:C4DE@all entries: 3492 of 3492"
    "Work-RAM write trace complete: 563457 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic actor-scan-preamble output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
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
            "Semantic actor-scan-preamble ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "The actor scan's preamble runs natively on all 3,492 arrivals, with the "
    "enable mask it settles, the three cursors it resets and the scan index it "
    "clears identical to assembly."
)
