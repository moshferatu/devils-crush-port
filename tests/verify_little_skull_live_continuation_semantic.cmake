# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Little-Skull live-continuation semantic coverage is Release-only.")
endif()

set(expected_census_sha256 "7af61a496eb4584381b91dd56a611732361bb115e3c1cbc6110cbdcc5664e0a8")
set(expected_call_sha256 "83e805e867f641b7e8d1eea6fad8e1a21e697f8b67e264e0eb39e29438df6a23")
set(expected_write_sha256 "d86ec3da98c1cda817a7fe89034a3051d22482630dcb9895f76ccbfa7eedcf0f")
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
        --observe-hook "02:4515"
        --replace-hook "02:4515@all"
        --routine-call-trace "${call_output}"
        --routine-replay "02:4515@4"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Little-Skull live-continuation recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 6"
    "Native replacement 02:4515@all entries: 22 of 22"
    "Observe hook 1 entries: 22"
    "Routine call trace complete: 22 calls, 106 writes"
    "Routine replay 02:4515@4 verified: frame 1227, 303 body cycles, 10 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Little-Skull live-continuation output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
    endif()
endforeach()

file(STRINGS "${call_output}" wait_calls REGEX ",105,.*3$")
file(STRINGS "${call_output}" expiry_calls REGEX ",303,.*10$")
list(LENGTH wait_calls wait_count)
list(LENGTH expiry_calls expiry_count)
if(wait_count LESS 1 OR expiry_count LESS 1)
    message(FATAL_ERROR "Little-Skull live-continuation route omitted its wait or expiry transcript.")
endif()

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
        message(FATAL_ERROR "Semantic Little-Skull live-continuation ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Semantic 4515 Little-Skull continuation covers all wait and expiry paths, including the slot-49 timer IRQ passage.")
