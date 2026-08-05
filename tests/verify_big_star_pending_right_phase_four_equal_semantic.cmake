# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Big-Star pending-right phase-four equality regression is Release-only.")
endif()

set(expected_census_sha256 "94233f9cbfa19062effe9c1173b77696ee1a1ccb44108cf8c6646a38bc942526")
set(expected_trace_sha256 "c18f28b197b65c5571cf102f6cf9874abf30ba20bdd1568587548ec289a267c2")
set(expected_call_sha256 "1fd09f08c5444b19030c54a2ebe0c437914b3bcbfbb62dfbfe620795343c0c2d")
set(expected_write_sha256 "a4cba2cef1f238eb05387eacb456f7ae30ff27d7717bf96850c819627dddf712")
set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}" --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:334B:2C" --work-ram-patch "1200:33CB:01"
        --work-ram-patch "1201:2D04:02" --work-ram-patch "1201:3B4B:04"
        --work-ram-patch "1201:2D18:04"
        --observe-hook "02:570A" --observe-hook "02:5738"
        --replace-hook "02:5738@all"
        --routine-trace "${trace_output}" --routine-call-trace "${call_output}"
        --routine-replay "02:5738@1" --rom "${ROM}"
    RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Big-Star pending-right phase-four equality recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames" "Work-RAM patches applied: 5"
    "Native replacement 02:5738@all entries: 1 of 1"
    "Observe hook 1 entries: 880" "Observe hook 2 entries: 1"
    "Routine trace complete: 881 entries"
    "Routine call trace complete: 881 calls, 1275 writes"
    "Routine replay 02:5738@1 verified: frame 1221, 252 body cycles, 6 ordered writes"
    "Routine replay complete: 1 state-normalized calls")
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Big-Star pending-right phase-four equality output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Semantic Big-Star pending-right phase-four equality ${artifact} artifact changed.\nExpected: ${expected_${artifact}_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Semantic 5738 covers the verified Big-Star pending-right phase-four equality tail.")
