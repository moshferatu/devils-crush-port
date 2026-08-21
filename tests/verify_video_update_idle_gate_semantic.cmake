# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Video-update idle-gate regression is Release-only under the native-port cadence.")
endif()

# 08:5E42 is entered once per active frame by the bank-call wrapper at E252.
# The canonical route's 2,649 calls all take the short return: $2254 and $2D53
# are both zero. The replacement is intentionally guarded to that measured
# state and leaves either active-update path in assembly.
set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_call_sha256 "fe732c10eb0018ec193df7cb87d98ca8201af219145a36db44570bc55334d5e7")
set(expected_call_write_sha256 "919781dfa04779d8ba035dbea74f75d2b6dc2c1c3b9803b38594b2e46c00ace3")
set(expected_work_sha256 "79b945c4c1e462d065d815ba0ebe42db4652d8fae4a6916b1728863000e0435a")

set(corpus_directory "${OUTPUT}.corpus")
set(corpus "${corpus_directory}/08-5E42-1.dcrr")
if(NOT EXISTS "${corpus}")
    execute_process(
        COMMAND
            "${APP}"
            --record-state-census "${OUTPUT}.assembly-capture.csv"
            --observe-hook "08:5E42"
            --routine-call-trace "${OUTPUT}.assembly-capture.calls.csv"
            --routine-replay "08:5E42@1"
            --routine-replay-corpus-out "${corpus_directory}"
            --rom "${ROM}"
        RESULT_VARIABLE capture_result
        OUTPUT_VARIABLE capture_output
        ERROR_VARIABLE capture_error
    )
    if(NOT capture_result EQUAL 0)
        message(FATAL_ERROR "Video-update idle-gate corpus capture failed (${capture_result}).\n${capture_output}${capture_error}")
    endif()
    string(FIND "${capture_output}" "Routine replay 08:5E42@1 verified: frame 941, 42 body cycles, 0 ordered writes" capture_offset)
    if(capture_offset EQUAL -1)
        message(FATAL_ERROR "Video-update idle-gate capture omitted the expected fixture.\n${capture_output}${capture_error}")
    endif()
endif()

execute_process(
    COMMAND
        "${APP}"
        --routine-replay-corpus "${corpus}"
        --replace-hook "08:5E42@all"
        --rom "${ROM}"
    RESULT_VARIABLE replay_result
    OUTPUT_VARIABLE replay_output
    ERROR_VARIABLE replay_error
)
if(NOT replay_result EQUAL 0)
    message(FATAL_ERROR "Video-update idle-gate direct replay failed (${replay_result}).\n${replay_output}${replay_error}")
endif()
string(FIND "${replay_output}" "Direct routine replay 08:5E42@1 verified: frame 941, 42 body cycles, 0 ordered writes, native" replay_offset)
if(replay_offset EQUAL -1)
    message(FATAL_ERROR "Video-update idle-gate direct replay omitted its native proof.\n${replay_output}${replay_error}")
endif()

set(call_output "${OUTPUT}.calls.csv")
set(call_write_output "${call_output}.writes.csv")
set(work_output "${OUTPUT}.work.csv")
set(abandonment_output "${OUTPUT}.abandonments.csv")
execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "08:5E42"
        --replace-hook "08:5E42@all"
        --routine-call-trace "${call_output}"
        --work-ram-write-range "2254-2254"
        --work-ram-write-range "2D53-2D53"
        --work-ram-write-trace "${work_output}"
        --abandonment-census "${abandonment_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic video-update idle-gate recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 08:5E42@all entries: 2649 of 2649"
    "Abandonment census complete: no transcript was abandoned"
    "Observe hook 1 entries: 2649"
    "Routine call trace complete: 2649 calls, 582 writes"
    "Work-RAM write trace complete: 6 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic video-update idle-gate output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
    endif()
endforeach()

foreach(artifact IN ITEMS census call call_write work)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    elseif(artifact STREQUAL "call_write")
        set(artifact_path "${call_write_output}")
    else()
        set(artifact_path "${work_output}")
    endif()
    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(FATAL_ERROR "Semantic video-update idle-gate ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}\n")
    endif()
endforeach()

message(STATUS "Semantic 08:5E42 idle gate runs all 2,649 canonical calls natively with exact call, interrupt, RAM and state parity.")
