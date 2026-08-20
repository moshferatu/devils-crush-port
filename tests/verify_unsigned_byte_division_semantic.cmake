# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsigned byte division regression is Release-only.")
endif()

set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_call_sha256 "d0b4d0891016f40c8d1768b703294374b9436e7413734c3a05170d51b848d31e")
set(expected_write_sha256 "94a4d92c6600323147e3c2e2b65760fa83d2ff891fcaa167384eae3f424b1ced")

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(abandonment_output "${OUTPUT}.abandonments.csv")
set(corpus_directory "${OUTPUT}.corpus")
set(corpus_path "${corpus_directory}/00-E6DC-3.dcrr")

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
            --observe-hook "00:E6DC"
            --routine-call-trace "${OUTPUT}.assembly.calls.csv"
            --routine-replay "00:E6DC@3"
            --routine-replay-corpus-out "${corpus_directory}"
            --rom "${ROM}"
        RESULT_VARIABLE capture_result
        OUTPUT_VARIABLE capture_stdout
        ERROR_VARIABLE capture_stderr
    )
    if(NOT capture_result EQUAL 0)
        message(FATAL_ERROR "Unsigned byte division corpus capture failed (${capture_result}).\n${capture_stdout}${capture_stderr}")
    endif()
    foreach(expected_line IN ITEMS
        "Observe hook 1 entries: 46"
        "Routine replay 00:E6DC@3 verified: frame 874, 795 body cycles, 20 ordered writes"
        "Routine replay corpus complete: 1 authoritative assembly calls"
    )
        string(FIND "${capture_stdout}" "${expected_line}" line_offset)
        if(line_offset EQUAL -1)
            message(FATAL_ERROR "Unsigned byte division capture omitted '${expected_line}'.\n${capture_stdout}${capture_stderr}")
        endif()
    endforeach()
endif()

execute_process(
    COMMAND "${APP}"
        --routine-replay-corpus "${corpus_path}"
        --replace-hook "00:E6DC@all"
        --rom "${ROM}"
    RESULT_VARIABLE replay_result
    OUTPUT_VARIABLE replay_stdout
    ERROR_VARIABLE replay_stderr
)
if(NOT replay_result EQUAL 0)
    message(FATAL_ERROR "Unsigned byte division direct replay failed (${replay_result}).\n${replay_stdout}${replay_stderr}")
endif()
foreach(expected_line IN ITEMS
    "Direct routine replay 00:E6DC@3 verified: frame 874, 795 body cycles, 20 ordered writes, native"
    "Direct routine replay corpus complete: 1 calls without route startup"
)
    string(FIND "${replay_stdout}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Unsigned byte division direct replay omitted '${expected_line}'.\n${replay_stdout}${replay_stderr}")
    endif()
endforeach()

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "00:E6DC"
        --replace-hook "00:E6DC@all"
        --routine-call-trace "${call_output}"
        --abandonment-census "${abandonment_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_stdout
    ERROR_VARIABLE recorder_stderr
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Unsigned byte division recorder failed (${recorder_result}).\n${recorder_stdout}${recorder_stderr}")
endif()
foreach(expected_line IN ITEMS
    "Native replacement 00:E6DC@all entries: 46 of 46"
    "Abandonment census complete: no transcript was abandoned"
    "Observe hook 1 entries: 46"
    "Routine call trace complete: 46 calls, 1108 writes"
)
    string(FIND "${recorder_stdout}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Unsigned byte division output omitted '${expected_line}'.\n${recorder_stdout}${recorder_stderr}")
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
        message(FATAL_ERROR "Unsigned byte division ${artifact} artifact changed. Expected ${expected_${artifact}_sha256}; actual ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Semantic E6DC replaces all 46 unsigned byte divisions")
