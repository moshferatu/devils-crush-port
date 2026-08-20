# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Small-Soldier initial-collision regression is Release-only.")
endif()

set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_call_sha256 "dd9c01d2a5145c96bebdf856212c6acdcd11d1af64c65c0d7fd5e6c802bfb693")
set(expected_write_sha256 "5111f1e67085f9bb4b2d4b39efd5a2bd2fde51c23892b751cd797609beb5f05a")

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(abandonment_output "${OUTPUT}.abandonments.csv")
set(corpus_directory "${OUTPUT}.corpus")
set(corpus_path "${corpus_directory}/03-7F1A-4.dcrr")

set(needs_capture TRUE)
if(EXISTS "${corpus_path}")
    execute_process(
        COMMAND "${APP}" --routine-replay-corpus "${corpus_path}" --rom "${ROM}"
        RESULT_VARIABLE cached_result
        OUTPUT_QUIET
        ERROR_QUIET
    )
    if(cached_result EQUAL 0)
        set(needs_capture FALSE)
    endif()
endif()

if(needs_capture)
    execute_process(
        COMMAND "${APP}"
            --record-state-census "${OUTPUT}.assembly.csv"
            --observe-hook "03:7F1A"
            --routine-call-trace "${OUTPUT}.assembly.calls.csv"
            --routine-replay "03:7F1A@4"
            --routine-replay-corpus-out "${corpus_directory}"
            --rom "${ROM}"
        RESULT_VARIABLE capture_result
        OUTPUT_VARIABLE capture_stdout
        ERROR_VARIABLE capture_stderr
    )
    if(NOT capture_result EQUAL 0)
        message(FATAL_ERROR "Small-Soldier initial-collision corpus capture failed (${capture_result}).\n${capture_stdout}${capture_stderr}")
    endif()
    foreach(expected_line IN ITEMS
        "Observe hook 1 entries: 1130"
        "Routine replay 03:7F1A@4 verified: frame 958, 423 body cycles, 11 ordered writes"
        "Routine replay corpus complete: 1 authoritative assembly calls"
    )
        string(FIND "${capture_stdout}" "${expected_line}" line_offset)
        if(line_offset EQUAL -1)
            message(FATAL_ERROR "Small-Soldier initial-collision capture omitted '${expected_line}'.\n${capture_stdout}${capture_stderr}")
        endif()
    endforeach()
endif()

execute_process(
    COMMAND "${APP}"
        --routine-replay-corpus "${corpus_path}"
        --replace-hook "03:7F1A@all"
        --rom "${ROM}"
    RESULT_VARIABLE replay_result
    OUTPUT_VARIABLE replay_stdout
    ERROR_VARIABLE replay_stderr
)
if(NOT replay_result EQUAL 0)
    message(FATAL_ERROR "Small-Soldier initial-collision direct replay failed (${replay_result}).\n${replay_stdout}${replay_stderr}")
endif()
foreach(expected_line IN ITEMS
    "Direct routine replay 03:7F1A@4 verified: frame 958, 423 body cycles, 11 ordered writes, native"
    "Direct routine replay corpus complete: 1 calls without route startup"
)
    string(FIND "${replay_stdout}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Small-Soldier initial-collision replay omitted '${expected_line}'.\n${replay_stdout}${replay_stderr}")
    endif()
endforeach()

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "03:7F1A"
        --replace-hook "03:7F1A@all"
        --routine-call-trace "${call_output}"
        --abandonment-census "${abandonment_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_stdout
    ERROR_VARIABLE recorder_stderr
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Small-Soldier initial-collision recorder failed (${recorder_result}).\n${recorder_stdout}${recorder_stderr}")
endif()
foreach(expected_line IN ITEMS
    "Native replacement 03:7F1A@all entries: 1120 of 1130"
    "Abandonment census complete: no transcript was abandoned"
    "Observe hook 1 entries: 1130"
    "Routine call trace complete: 1130 calls, 21255 writes"
)
    string(FIND "${recorder_stdout}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Small-Soldier initial-collision output omitted '${expected_line}'.\n${recorder_stdout}${recorder_stderr}")
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
    if(NOT actual_sha256 STREQUAL "${expected_${artifact}_sha256}")
        message(FATAL_ERROR "Small-Soldier initial-collision ${artifact} artifact changed. Expected ${expected_${artifact}_sha256}; actual ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Semantic 03:7F1A replaces all 1,120 Small-Soldier initial no-hit tails")
