# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Undead-Head waiting-parent regression is Release-only.")
endif()

set(expected_census_sha256 "cb6445df6361443a43fde2ca163b30248adb250d53b7ca4b547fad9ef900e788")
set(expected_trace_sha256 "89114d9fb38979bf9856a54884c6f44651c8abd15edc5fb30dd01036ef4bba59")
set(expected_call_sha256 "562fe2c2f2b9577bfec163ee94290f6c05209b6596039ff6cdcf58098c424f41")
set(expected_write_sha256 "9f87bb1089e5f7dab6b69c5a491a0e6365077f89d74ada8a0b7af0dcf7483967")
set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:334D:2C"
        --work-ram-patch "1200:33CD:01"
        --observe-hook "03:61A3"
        --replace-hook "03:61A3@all"
        --routine-trace "${trace_output}"
        --routine-call-trace "${call_output}"
        --routine-replay "03:61A3@1"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Undead-Head waiting-parent recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 2"
    "Native replacement 03:61A3@all entries: 770 of 770"
    "Observe hook 1 entries: 770"
    "Routine trace complete: 770 entries"
    "Routine call trace complete: 770 calls, 244 writes"
    "Routine replay 03:61A3@1 verified: frame 1221, 63 body cycles, 0 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Undead-Head waiting-parent output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
    endif()
endforeach()

foreach(artifact IN ITEMS census trace call write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "trace")
        set(artifact_path "${trace_output}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    else()
        set(artifact_path "${write_output}")
    endif()
    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(FATAL_ERROR "Semantic Undead-Head waiting-parent ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Semantic 61A3 covers all 770 stopped Undead-Head parent returns, including interrupt passages.")
