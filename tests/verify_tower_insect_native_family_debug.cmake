# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Debug")
    message(FATAL_ERROR "Tower-insect native family checkpoint requires Debug.")
endif()

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(expected_census_sha256 "f70fbc86ce325a15ddaecfd1da91436ec948071b5ae7d7b1914dd99d9092d5ab")
set(expected_call_sha256 "720c8054e465d28fa2412faedcdd638e45733e4714b7ede9d9e969b0e4f45c99")
set(expected_write_sha256 "801152c295a289d32a9fd2dd1e1a6567331da555f983cf224e3757514aa44842")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "02:5F22"
        --replace-hook "02:5F22@all"
        --routine-replay "02:5F22@1"
        --observe-hook "03:60B3"
        --replace-hook "03:60B3@all"
        --routine-replay "03:60B3@1"
        --observe-hook "03:6054"
        --replace-hook "03:6054@all"
        --routine-replay "03:6054@1"
        --observe-hook "02:5F5F"
        --replace-hook "02:5F5F@all"
        --routine-replay "02:5F5F@1"
        --routine-call-trace "${call_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Tower-insect Debug family recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 02:5F22@all entries: 1 of 1"
    "Native replacement 03:60B3@all entries: 1 of 1"
    "Native replacement 03:6054@all entries: 6750 of 6750"
    "Native replacement 02:5F5F@all entries: 2504 of 2504"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 1"
    "Observe hook 3 entries: 6750"
    "Observe hook 4 entries: 2504"
    "Routine replay 02:5F22@1 verified: frame 872, 192 body cycles, 7 ordered writes"
    "Routine replay 03:60B3@1 verified: frame 872, 249 body cycles, 10 ordered writes"
    "Routine replay 03:6054@1 verified: frame 958, 147 body cycles, 3 ordered writes"
    "Routine replay 02:5F5F@1 verified: frame 1086, 21 body cycles, 0 ordered writes"
    "Routine replay complete: 4 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Tower-insect Debug family output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
    endif()
endforeach()

foreach(artifact IN ITEMS census call write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    else()
        set(artifact_path "${write_output}")
    endif()
    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(FATAL_ERROR "Tower-insect Debug family ${artifact} changed.\nExpected: ${expected_sha256}\nActual: ${actual_sha256}\n")
    endif()
endforeach()

message(STATUS "Tower-insect Debug family checkpoint verified all native source and child leaves.")
