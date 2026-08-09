# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported actor-queue configuration: ${CONFIG}")
endif()

# The native registration must leave the emulated machine bit-identical to
# assembly, so both artifacts are the unmodified assembly-only baselines. The
# write ranges cover everything the routine can touch: the three queue cursors
# at $2CBA-$2CBC, the $2BBA and $2C3A queues themselves, and the one-shot
# clear at $3800. The routine is an instruction-boundary transcript and
# crosses interrupt horizons, so that is the assertion that matters: the
# interrupts still land where assembly put them.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_write_sha256
    "be9081c8d0f6309edd2cc95cd91133afa5abac6f78fa6b4e4f305c4efc3e8869"
)

set(write_output "${OUTPUT}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --replace-hook
        "01:C606@all"
        --work-ram-write-range
        "2BBA-2D39"
        --work-ram-write-range
        "3800-38FF"
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
        "Semantic actor-queue recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 01:C606@all entries: 171734 of 171734"
    "Work-RAM write trace complete: 289690 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic actor-queue output omitted '${expected_line}'.\n"
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
            "Semantic actor-queue ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic C606 sprite-queue registration files every dispatched slot "
    "natively across all 171,734 entries as an instruction-boundary "
    "transcript, with the queue, cursor, and one-shot clear writes and the "
    "whole state census identical to assembly."
)
