# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported sorcerer-tick configuration: ${CONFIG}")
endif()

# The Sorcerer tick, 02:586F-02:5903, in physical bank 02. One routine in three pieces, split
# by the two calls it makes to the collision-table interpreter at 03:6966:
#
#   02:586F  the per-actor cooldown. $3B00,X and $3B80,X are a sixteen-bit
#            countdown; while either byte is set the tick decrements it and
#            returns.
#   02:5886  the expiry path, which flips bit 5 of the attribute byte at
#            $3080,X and asks the interpreter for record 6. A set carry runs
#            the reset the walk from 02:5893 takes its own branch into: seed
#            the table cursors at $3E00,X/$3E80,X, set the pass count at
#            $3C00,X, clear the high bit of $3800,X and decrement the
#            population counter at $2D10.
#   02:58C2  the main tick and the largest piece of it. With the population
#            counter at zero it derives an index from the difficulty byte at
#            $2D05 and reads a four-entry table in this bank's own ROM page
#            into $3880,X; 02:58DD copies the interpreter's event record from
#            $3A80,X to $3980,X.
#
# Three entry points, all of them routine heads. 02:5896 is hookable on its own
# but is not an entry here, because the 02:5893 walk branches into it.
#
# There were five. Both interpreter calls are run in line rather than handed
# back, so the three addresses on the far side of them -- 02:5893, 02:5896 and
# 02:58DD -- were only ever reached when an interrupt landed inside the composed
# transcript and the core resumed the remainder in assembly. That resumption was
# a real arrival and they were registered for it: 272 entries and 4,628.
#
# It is not a real arrival any more. Those resumptions were abandonments, and
# every abandonment on this route was the core giving a transcript up over the
# B flag, which is not a register bit -- see
# `patches/0022-huc6280-break-flag-resume.patch`. With the comparison corrected
# the transcripts run to their ends, the composed bodies are never re-entered
# in assembly, and both addresses fall to zero arrivals. Their replacements are
# still correct and still dispatched; nothing on this route reaches them.
#
# $21F7-$21FB is the stack depth the two `JSR $6966` instructions push their
# return address to, and it is in the ranges for the composition's sake: a `JSR`
# pushes both halves before it pays for its own six cycles, so a push recorded
# at the end of the instruction would still leave every other write in this
# trace correct. Nothing else here would catch that.
#
# Shared state was measured on exact windows from observers on all six heads
# rather than inferred from the writes. Of the ninety-nine instructions that
# write any address this family reads, exactly one ever writes inside a window
# -- 03:69D9, the interpreter's own event store, which is this family's callee.
# Every other writer lands inside a window on 0.0% of its executions.
#
# The ranges below are the family's whole output, compared against a
# pure-assembly run of the same route, so the writes match in value, address,
# order and master-cycle offset rather than merely in end state.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_write_sha256
    "4783c934c0c808778f77f2201dc2d9deacf85a5aef420a9e4e8288704f8ed851"
)

set(write_output "${OUTPUT}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --replace-hook
        "02:586F@all"
        --replace-hook
        "02:5886@all"
        --replace-hook
        "02:58C2@all"
        --work-ram-write-range
        "21F7-21FB"
        --work-ram-write-range
        "2D10-2D10"
        --work-ram-write-range
        "3080-30FF"
        --work-ram-write-range
        "3880-38FF"
        --work-ram-write-range
        "3980-39FF"
        --work-ram-write-range
        "3B00-3BFF"
        --work-ram-write-range
        "3C00-3C7F"
        --work-ram-write-range
        "3E00-3EFF"
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
        "Semantic sorcerer-tick recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 02:586F@all entries: 1240 of 1240"
    "Native replacement 02:5886@all entries: 1800 of 1800"
    "Native replacement 02:58C2@all entries: 18152 of 18152"
    "Work-RAM write trace complete: 1940085 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic sorcerer-tick output omitted '${expected_line}'.\n"
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
            "Semantic sorcerer-tick ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic Sorcerer tick runs all three entry points natively across "
    "21,192 admitted entries, both interpreter calls composed in line, with "
    "the family's whole output and the state census identical to assembly."
)
