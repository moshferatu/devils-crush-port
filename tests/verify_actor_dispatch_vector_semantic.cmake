# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(
        FATAL_ERROR
        "Unsupported actor-dispatch-vector configuration: ${CONFIG}"
    )
endif()

# The native dispatch must leave the emulated machine bit-identical to
# assembly, so both artifacts are the unmodified assembly-only baselines. The
# write range covers the zero-page indirection at $2F/$30 that the routine
# fills and then jumps through, which is the whole of its observable output;
# the resume address it computes is proven by the census. The routine is an
# instruction-boundary transcript and crosses interrupt horizons, so that is
# the assertion that matters: the interrupts still land where assembly put
# them.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_write_sha256
    "b7a4fd91b603530dfd142fc7582a8d63a053443e933e7b68e45cdf47d4d449e6"
)

set(write_output "${OUTPUT}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --replace-hook
        "01:C52C@all"
        --work-ram-write-range
        "202F-2030"
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
        "Semantic actor-dispatch-vector recorder failed "
        "(${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 01:C52C@all entries: 171734 of 171734"
    "Work-RAM write trace complete: 1169947 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic actor-dispatch-vector output omitted "
            "'${expected_line}'.\n"
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
            "Semantic actor-dispatch-vector ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic C52C actor dispatch loads the jump-table vector and resumes "
    "inside the handler natively across all 171,734 entries as an "
    "instruction-boundary transcript, with the $202F/$2030 indirection "
    "writes and the whole state census identical to assembly."
)
