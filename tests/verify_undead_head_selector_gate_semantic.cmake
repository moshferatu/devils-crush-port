# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Undead-Head selector-gate regression is Release-only.")
endif()

set(expected_census_sha256 "cb6445df6361443a43fde2ca163b30248adb250d53b7ca4b547fad9ef900e788")
set(expected_trace_sha256 "bccdd10efc9ed189316b8439d4d5cd2ee04ab351b0012a29bdc827f1215ad01c")
set(expected_call_sha256 "ea14230eb19ee5096fe94e06e305e4760bf2853d016d9c6b3b6a308131bc4846")
set(expected_write_sha256 "5b3522ae147fd75be3018cd1d16afb3adcc52a09fa9a5432cae1c5e9187d8869")
set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:334D:2C"
        --work-ram-patch "1200:33CD:01"
        --observe-hook "03:61A3"
        --observe-hook "03:61AB"
        --replace-hook "03:61AB@all"
        --routine-trace "${trace_output}"
        --routine-call-trace "${call_output}"
        --routine-replay "03:61AB@1"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Undead-Head selector-gate recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 2"
    "Native replacement 03:61AB@all entries: 770 of 770"
    "Observe hook 1 entries: 770"
    "Observe hook 2 entries: 770"
    "Routine call trace complete: 1540 calls, 348 writes"
    "Routine replay 03:61AB@1 verified: frame 1221, 27 body cycles, 0 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Undead-Head selector-gate output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Semantic Undead-Head selector-gate ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Semantic 61AB covers all 770 controlled stopped-selector returns, including interrupt passages.")
