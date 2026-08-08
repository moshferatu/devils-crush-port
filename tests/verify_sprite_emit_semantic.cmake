# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported sprite-emitter configuration: ${CONFIG}")
endif()

# Both artifacts are the unmodified assembly-only baselines. The transcript
# runs inside the video window and crosses interrupt horizons, so this is the
# assertion that matters: the interrupts still land where assembly put them.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_write_sha256
    "7ab0aff89e9bef0118c1858f88b47578e4f541e87e44a401e52cfd6c60e7487b"
)

set(write_output "${OUTPUT}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --replace-hook
        "08:40DF@all"
        # The routine's own work RAM: the descriptor and entry pointers with
        # the remaining count at $202C-$2030, the widened vertical scratch at
        # $2035-$2036, and the attribute-table cursor at $204E.
        --work-ram-write-range
        "202C-2036"
        --work-ram-write-range
        "204E-204E"
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
        "Semantic sprite-emitter recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 08:40DF@all entries: 73818 of 73818"
    "Work-RAM write trace complete: 2749801 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic sprite-emitter output omitted '${expected_line}'.\n"
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
            "Semantic sprite-emitter ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic 08:40DF sprite emitter runs all 73,818 calls natively as an "
    "instruction-boundary transcript, emitting 109,678 attribute-table "
    "entries with the routine's work RAM and the whole state census "
    "identical to assembly."
)
