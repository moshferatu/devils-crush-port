# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported actor-geometry configuration: ${CONFIG}")
endif()

# Three arithmetic leaves in physical bank 01. They are not one routine and they
# do not call each other; they share a walk because they share a shape --
# straight-line integer arithmetic over work RAM and zero page, no calls, no
# device registers, no bank change.
#
#   01:C8F3  the path-length accumulator. It takes the absolute value of the
#            actor's two signed sixteen-bit velocity components, adds them,
#            shifts the sum left four times and adds the result into the
#            twenty-four-bit running total across $3E00,X, $3A00,X and $3980,X,
#            whose top byte saturates at 4.
#   01:C969  the head of the range-squared helper. The sixteen-bit population
#            counter at $2D00/$2D01 is zero on all 2,993 entries this route
#            makes, so every call is the three-instruction guard and its RTS;
#            the walk hands the other arm at 01:C972 back to the core.
#   01:DAF8  the direction-octant classifier. It saturates the signed
#            difference on each axis to a byte, records the two signs and the
#            ordering swap in $45, splits the octant three ways against 3:2 and
#            12:2 ratios, and indexes the thirty-two-entry table at $DB9E.
#            Two exits, 1,459 calls through 01:DB9D and 680 through 01:DB81.
#
# Shared state was measured on exact windows from observers on all three heads.
# Of the 358 instructions that write any address these three read, exactly four
# ever write inside a window, and all four are 01:C8F3's own stores.
#
# The ranges below are the three routines' whole output -- the zero-page
# scratch each uses and the accumulator's three totals -- compared against a
# pure-assembly run of the same route, so the writes match in value, address,
# order and master-cycle offset rather than merely in end state.
#
# The write filter matches the effective address, so zero page is named as
# $202D-$2030 and $2042-$2045 rather than $2D-$30 and $42-$45; written the
# short way the two ranges select nothing at all. The range order is part of
# the artifact, because the trace is emitted in the order the ranges are given.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_write_sha256
    "687ec11622ec74b6402d2d0c9151a2820490998401ef6e4af66fbfb9dd035548"
)

set(write_output "${OUTPUT}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --replace-hook
        "01:C8F3@all"
        --replace-hook
        "01:C969@all"
        --replace-hook
        "01:DAF8@all"
        --work-ram-write-range
        "202D-2030"
        --work-ram-write-range
        "2042-2045"
        --work-ram-write-range
        "3980-39FF"
        --work-ram-write-range
        "3A00-3A7F"
        --work-ram-write-range
        "3E00-3E7F"
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
        "Semantic actor-geometry recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 01:C8F3@all entries: 2340 of 2340"
    "Native replacement 01:C969@all entries: 2993 of 2993"
    "Native replacement 01:DAF8@all entries: 2139 of 2139"
    "Work-RAM write trace complete: 2277424 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic actor-geometry output omitted '${expected_line}'.\n"
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
            "Semantic actor-geometry ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic bank-01 geometry leaves run all three entry points natively "
    "across 7,472 admitted entries, with their whole output and the state "
    "census identical to assembly."
)
