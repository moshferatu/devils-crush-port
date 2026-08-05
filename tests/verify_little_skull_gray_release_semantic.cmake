# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Little-Skull gray-release semantic coverage is Release-only.")
endif()

set(expected_census_sha256 "7af61a496eb4584381b91dd56a611732361bb115e3c1cbc6110cbdcc5664e0a8")
set(expected_call_sha256 "58ccd297980561392f1abd0f9dc018bbb1a446acc10d63a3eb99f5f33139675b")
set(expected_write_sha256 "605ca2a48e74bb7374ba8e53f427febd43f1c4d10fbf561b5e531ff865166049")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:3346:2C"
        --work-ram-patch "1200:33C6:01"
        --work-ram-patch "1200:3F46:8A"
        --work-ram-patch "1200:3FC6:45"
        --work-ram-patch "1222:3346:2C"
        --work-ram-patch "1222:33C6:01"
        --observe-hook "02:45A4"
        --replace-hook "02:45A4@all"
        --routine-call-trace "${call_output}"
        --routine-replay "02:45A4@1"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Little-Skull gray-release recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 6"
    "Native replacement 02:45A4@all entries: 1 of 1"
    "Observe hook 1 entries: 1"
    "Routine call trace complete: 1 calls, 23 writes"
    "Routine replay 02:45A4@1 verified: frame 1223, 768 body cycles, 23 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Little-Skull gray-release output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Semantic Little-Skull gray-release ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Semantic 45A4 gray-release path covers the observed odd-RNG continuation.")
