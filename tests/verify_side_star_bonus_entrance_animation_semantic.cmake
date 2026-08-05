# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Side-Star bonus-entrance animation regression is Release-only.")
endif()

set(expected_census_sha256 "39fa9e48a8e65cbefb3dfd9fe31e0cff8d483edc536f606400317f9ea5308060")
set(expected_trace_sha256 "396d1f3a44994bb45d3c0927de09bb7b462fe2d30e40282db7da8da6a272368a")
set(expected_call_sha256 "4ea7f8734c42177a479b639a4a53831045561265243aded17040b9118496727d")
set(expected_write_sha256 "fb22429cbfaaf8affe041f21e71b69d40f4e6ca267832dabc10e33e406ea08cc")
set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}" --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:3812:80"
        --observe-hook "02:4C0A" --observe-hook "02:4C33"
        --replace-hook "02:4C33@all"
        --routine-trace "${trace_output}" --routine-call-trace "${call_output}"
        --routine-replay "02:4C33@1" --rom "${ROM}"
    RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Side-Star bonus-entrance animation recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames" "Work-RAM patches applied: 1"
    "Native replacement 02:4C33@all entries: 8 of 9"
    "Observe hook 1 entries: 398" "Observe hook 2 entries: 9"
    "Routine trace complete: 407 entries"
    "Routine call trace complete: 407 calls, 848 writes"
    "Routine replay 02:4C33@1 verified: frame 1221, 60 body cycles, 1 ordered writes"
    "Routine replay complete: 1 state-normalized calls")
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Side-Star bonus-entrance animation output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
    if(NOT actual_sha256 STREQUAL "${expected_${artifact}_sha256}")
        message(FATAL_ERROR "Side-Star bonus-entrance animation ${artifact} artifact changed.\nExpected: ${expected_${artifact}_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Semantic 4C33 covers eight nonterminal Side-Star bonus-entrance animation calls.")
