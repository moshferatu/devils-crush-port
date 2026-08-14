# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported motion-step configuration: ${CONFIG}")
endif()

# 00:F08E-00:F1B3, the shared motion step, across eleven entry points. The head
# is a four-way dispatcher on the bottom bits of the actor's motion-enable byte
# `$3800,X`: bit 0 integrates the vertical position at 00:F0C0, bit 1 the
# horizontal at 00:F104, bit 2 steps the vertical velocity toward its target at
# 00:F148 and bit 3 the horizontal at 00:F17E. The two integrators add a signed
# sixteen-bit velocity into a twenty-four-bit fixed-point position, then hand
# the whole-pixel word and a bound to the registered comparison at 00:E93C --
# $0140 across, $02D0 down -- and raise `$26` if the actor left the playfield.
# The head turns `$26` into the carry it returns.
#
# It is the most widely shared routine ported so far: 924 of its 1,226 calls
# arrive through the bank-01 trampoline at 01:C012, which sixteen sites across
# banks 02 to 07 reach it by, and the other 302 from eight `JSR`s in bank 09.
#
# Four of the eleven entry points are this family's own leaves. That is new: the
# head crosses into routines it owns rather than into somebody else's, so the
# `JSR` and the body on the far side of it are both converted and the machine
# steps from one transcript straight into the next. The other seven are the
# head, the four addresses those calls return to, and the two the comparison
# helper returns to.
#
# Nothing in the range is left interpreted. Every instruction the route executes
# is transcribed and the five returns it takes are absorbed. Three arms are
# handed back and none of them has ever run: 00:F0BC, the head reporting a
# departure, and 00:F100 and 00:F144, the two integrators declaring one. In
# 3,600 frames no actor left the playfield through this routine.
#
# The interrupt deadline is the constraint that would bite a family this small
# and this shared, and the entry counts below are the measurement of it: every
# one of the 7,018 arrivals ran inside a transcript, none declined at a horizon.
# The longest walk here is the vertical integrator at twenty-five instructions,
# about 330 master cycles against an interrupt period of roughly 5,068 on this
# route, so no walk spans a serviceable interrupt and every read it makes of
# `$3800,X`, `$26` and `$2B` happens inside one horizon.
#
# The ranges below are that scratch, the whole stack page, and the ten bytes
# this routine exists to move. The stack page is the range that matters most: a
# crossed `JSR` pushes its two return bytes at the instruction's start offset,
# so all six crossings appear in the trace with their address, value and master
# cycle, and an absorbed `RTS` deletes a pop that the following push would
# otherwise land differently -- a cycle model or a stack pointer wrong by one
# would move them. `$3100-$33FF` is the two positions, sub-pixel byte first, and
# `$3400-$35FF` the two velocities.
#
# The write filter matches the effective address, so zero page is named as
# $2026 and $202B rather than $26 and $2B; written the short way the ranges
# select nothing at all. The range order is part of the artifact, because the
# trace is emitted in the order the ranges are given.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_write_sha256
    "196478fcaa334b5f6dbb9d5dfa6b007e222cf4e005ae92cea65937a4e006277e"
)

set(write_output "${OUTPUT}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --replace-hook
        "00:F08E@all"
        --replace-hook
        "00:F09A@all"
        --replace-hook
        "00:F0A4@all"
        --replace-hook
        "00:F0AE@all"
        --replace-hook
        "00:F0B8@all"
        --replace-hook
        "00:F0C0@all"
        --replace-hook
        "00:F0FE@all"
        --replace-hook
        "00:F104@all"
        --replace-hook
        "00:F142@all"
        --replace-hook
        "00:F148@all"
        --replace-hook
        "00:F17E@all"
        --work-ram-write-range
        "2026-2026"
        --work-ram-write-range
        "202B-202B"
        --work-ram-write-range
        "202D-2030"
        --work-ram-write-range
        "2100-21FF"
        --work-ram-write-range
        "3100-33FF"
        --work-ram-write-range
        "3400-35FF"
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
        "Semantic motion-step recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 00:F08E@all entries: 1226 of 1226"
    "Native replacement 00:F09A@all entries: 506 of 506"
    "Native replacement 00:F0A4@all entries: 1022 of 1022"
    "Native replacement 00:F0AE@all entries: 302 of 302"
    "Native replacement 00:F0B8@all entries: 302 of 302"
    "Native replacement 00:F0C0@all entries: 506 of 506"
    "Native replacement 00:F0FE@all entries: 506 of 506"
    "Native replacement 00:F104@all entries: 1022 of 1022"
    "Native replacement 00:F142@all entries: 1022 of 1022"
    "Native replacement 00:F148@all entries: 302 of 302"
    "Native replacement 00:F17E@all entries: 302 of 302"
    "Work-RAM write trace complete: 6180712 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic motion-step output omitted '${expected_line}'.\n"
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
            "Semantic motion-step ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "The shared motion step runs all eleven entry points natively across "
    "7,018 admitted entries, with the positions and velocities it moves, the "
    "flag it reports and every return address its six crossings push identical "
    "to assembly."
)
