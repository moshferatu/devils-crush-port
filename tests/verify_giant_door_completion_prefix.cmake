# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Giant Door completion-prefix regression is Release-only.")
endif()

set(expected_census_sha256 "5e39c2543cf8a17bc5e74e7c55693c0d6abf7429ea82822df8b641eb7860f28b")
set(expected_call_sha256 "d867a2649244851fb40cbed2283c4c4ed244b6c5d62cfd17b1f0780c8d59244f")
set(expected_write_sha256 "9d343574197049cd82cc15696c50c749f97ed4568dc66144b2b24267a82efc55")
set(expected_state_write_sha256 "96ee8f0902d32d2ec553c29ee0dda5d4aca0979cbf142880e18c0e4b2cc81260")

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(state_write_output "${OUTPUT}.state.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --observe-hook
        "03:62B8"
        --observe-hook
        "03:62CB"
        --observe-hook
        "01:C02A"
        --replace-hook
        "03:62B8@all"
        --replace-hook
        "03:62CB@all"
        --routine-call-trace
        "${call_output}"
        --work-ram-patch
        "1228:3100:00"
        --work-ram-patch
        "1228:3180:40"
        --work-ram-patch
        "1228:3200:00"
        --work-ram-patch
        "1228:3280:00"
        --work-ram-patch
        "1228:3300:00"
        --work-ram-patch
        "1228:3380:01"
        --work-ram-patch
        "1340:3100:00"
        --work-ram-patch
        "1340:3180:40"
        --work-ram-patch
        "1340:3200:00"
        --work-ram-patch
        "1340:3280:00"
        --work-ram-patch
        "1340:3300:00"
        --work-ram-patch
        "1340:3380:01"
        --work-ram-write-range
        "002F-0030"
        --work-ram-write-range
        "384C-384C"
        --work-ram-write-range
        "3B4C-3B4C"
        --work-ram-write-trace
        "${state_write_output}"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Giant Door completion-prefix recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 12"
    "Native replacement 03:62B8@all entries: 145 of 145"
    "Native replacement 03:62CB@all entries: 3 of 3"
    "Observe hook 1 entries: 145"
    "Observe hook 2 entries: 3"
    "Observe hook 3 entries: 27"
    "Routine call trace complete: 175 calls, 1052 writes"
    "Work-RAM write trace complete: 26 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Giant Door completion-prefix output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census call write state_write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
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
            "Giant Door completion-prefix ${artifact} artifact changed.\n"
            "Expected: ${expected_${artifact}_sha256}\n"
            "Actual:   ${actual_sha256}"
        )
    endif()
    message(STATUS "Giant Door completion-prefix ${artifact} verified: ${actual_sha256}")
endforeach()

message(
    STATUS
    "Giant Door completion prefix verified: hits resume at original 03:62D0 or 03:62D4; completion stays in assembly."
)
