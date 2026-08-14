# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Side-Worm selector-gate regression is Release-only.")
endif()

set(expected_census_sha256 "297d2de96400e7e6e660a05fdb86a5dacf18a60d99a2c69b8b9423f3b777339d")
set(expected_trace_sha256 "a9a14c0cbcd199b74c9525ef5585a76074e82ff78dd26cc2af23456ad1908477")
set(expected_call_sha256 "745d6fec4af3aeab9f95dce86ca45fbe3db2e4ba2e0fddf9c2a76b6dccf39bb7")
set(expected_write_sha256 "f177c053f5332f48c3858fab3af1ceec65296d094863fbaed0c8cb838df0d1ce")
set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:334E:2C"
        --work-ram-patch "1200:33CE:01"
        --work-ram-patch "1200:37CE:06"
        --work-ram-patch "1200:334F:2C"
        --work-ram-patch "1200:33CF:01"
        --work-ram-patch "1200:37CF:02"
        --observe-hook "02:5CC5"
        --observe-hook "02:5CCD"
        --replace-hook "02:5CCD@all"
        --routine-trace "${trace_output}"
        --routine-call-trace "${call_output}"
        --routine-replay "02:5CCD@1"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Side-Worm selector-gate recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 6"
    "Native replacement 02:5CCD@all entries: 1760 of 1760"
    "Observe hook 1 entries: 1760"
    "Observe hook 2 entries: 1760"
    "Routine call trace complete: 3520 calls, 718 writes"
    "Routine replay 02:5CCD@1 verified: frame 1221, 36 body cycles, 0 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Side-Worm selector-gate output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Semantic Side-Worm selector-gate ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Semantic 5CCD covers all 1,760 controlled stopped-selector returns, including interrupt passages.")
