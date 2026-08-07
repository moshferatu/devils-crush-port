# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Tower bonus-entrance regression is Release-only.")
endif()

set(expected_census_sha256 "8aa1272f6b98e25edd304cb666b0cc36bdc32a4f8b577a1398b07c2e343604fc")
set(expected_trace_sha256 "ef9ac538c342a3073a78ea455864e7c73023d99061401cc63208583eeb237a1a")
set(expected_call_sha256 "44bf2aa37a410b5116131e2bf9e87033d1c01c9b24f67440771a54c8df945889")
set(expected_write_sha256 "ecdc220bd2dc26af1986c5a1c84bbcd86d04aec738ac2d9bb9c1e0a134cd8a96")
set(expected_state_write_sha256 "e6d2d3d2d43e10e94fe7645937297803ae63bc7b9ca444c8a4cc78b994558e8d")

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(state_write_output "${OUTPUT}.state.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --observe-hook
        "03:60F9"
        --observe-hook
        "03:612B"
        --observe-hook
        "03:6136"
        --replace-hook
        "03:60F9@all"
        --replace-hook
        "03:6136@all"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --work-ram-write-range
        "2CFC-2CFC"
        --work-ram-write-range
        "2D2D-2D2D"
        --work-ram-write-range
        "2D4D-2D4D"
        --work-ram-write-range
        "382D-382D"
        --work-ram-write-range
        "3F2D-3F2D"
        --work-ram-write-trace
        "${state_write_output}"
        --work-ram-patch
        "3352:2CFC:01"
        --work-ram-patch
        "3352:2D4D:01"
        --work-ram-patch
        "3353:2CFC:01"
        --work-ram-patch
        "3353:2D4D:01"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Tower bonus-entrance recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 4"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 1"
    "Observe hook 3 entries: 245"
    "Native replacement 03:60F9@all entries: 1 of 1"
    "Native replacement 03:6136@all entries: 245 of 245"
    "Routine trace complete: 247 entries"
    "Routine call trace complete: 247 calls, 30 writes"
    "Work-RAM write trace complete: 17 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Tower bonus-entrance output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census trace call write state_write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "trace")
        set(artifact_path "${trace_output}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    elseif(artifact STREQUAL "write")
        set(artifact_path "${write_output}")
    else()
        set(artifact_path "${state_write_output}")
    endif()

    file(SHA256 "${artifact_path}" actual_sha256)
    if(NOT actual_sha256 STREQUAL "${expected_${artifact}_sha256}")
        message(
            FATAL_ERROR
            "Tower bonus-entrance ${artifact} artifact changed.\n"
            "Expected: ${expected_${artifact}_sha256}\n"
            "Actual:   ${actual_sha256}"
        )
    endif()
    message(STATUS "Tower bonus-entrance ${artifact} verified: ${actual_sha256}")
endforeach()

message(
    STATUS
    "Tower bonus entrance verified: 60F9 terminal, 612B scheduler prefix, and later 6136 collision continuation."
)
