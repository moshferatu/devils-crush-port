# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsigned 24-bit division regression is Release-only.")
endif()

set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_call_sha256 "9163eaedd115dda2148b6c2c3e6d045dcfaaa1ff674629946d38fbba3d2207f0")
set(expected_write_sha256 "cb32774ea1c72400d67db014ae3337605d7a63b4cbc44fb532021a5a9b4d57d0")

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(abandonment_output "${OUTPUT}.abandonments.csv")
set(corpus_directory "${OUTPUT}.corpus")
set(corpus_path "${corpus_directory}/00-E7D7-1.dcrr")

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
            --observe-hook "00:E7D7"
            --routine-call-trace "${OUTPUT}.assembly.calls.csv"
            --routine-replay "00:E7D7@1"
            --routine-replay-corpus-out "${corpus_directory}"
            --rom "${ROM}"
        RESULT_VARIABLE capture_result
        OUTPUT_VARIABLE capture_stdout
        ERROR_VARIABLE capture_stderr
    )
    if(NOT capture_result EQUAL 0)
        message(FATAL_ERROR "Unsigned 24-bit division corpus capture failed (${capture_result}).\n${capture_stdout}${capture_stderr}")
    endif()
    foreach(expected_line IN ITEMS
        "Observe hook 1 entries: 105"
        "Routine replay 00:E7D7@1 verified: frame 874, 3123 body cycles, 101 ordered writes"
        "Routine replay corpus complete: 1 authoritative assembly calls"
    )
        string(FIND "${capture_stdout}" "${expected_line}" line_offset)
        if(line_offset EQUAL -1)
            message(FATAL_ERROR "Unsigned 24-bit division capture omitted '${expected_line}'.\n${capture_stdout}${capture_stderr}")
        endif()
    endforeach()
endif()

execute_process(
    COMMAND "${APP}"
        --routine-replay-corpus "${corpus_path}"
        --replace-hook "00:E7D7@all"
        --rom "${ROM}"
    RESULT_VARIABLE replay_result
    OUTPUT_VARIABLE replay_stdout
    ERROR_VARIABLE replay_stderr
)
if(NOT replay_result EQUAL 0)
    message(FATAL_ERROR "Unsigned 24-bit division direct replay failed (${replay_result}).\n${replay_stdout}${replay_stderr}")
endif()
foreach(expected_line IN ITEMS
    "Direct routine replay 00:E7D7@1 verified: frame 874, 3123 body cycles, 101 ordered writes, native"
    "Direct routine replay corpus complete: 1 calls without route startup"
)
    string(FIND "${replay_stdout}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Unsigned 24-bit division direct replay omitted '${expected_line}'.\n${replay_stdout}${replay_stderr}")
    endif()
endforeach()

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "00:E7D7"
        --replace-hook "00:E7D7@all"
        --routine-call-trace "${call_output}"
        --abandonment-census "${abandonment_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_stdout
    ERROR_VARIABLE recorder_stderr
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Unsigned 24-bit division recorder failed (${recorder_result}).\n${recorder_stdout}${recorder_stderr}")
endif()
foreach(expected_line IN ITEMS
    "Native replacement 00:E7D7@all entries: 105 of 105"
    "Abandonment census complete: no transcript was abandoned"
    "Observe hook 1 entries: 105"
    "Routine call trace complete: 105 calls, 13975 writes"
)
    string(FIND "${recorder_stdout}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Unsigned 24-bit division output omitted '${expected_line}'.\n${recorder_stdout}${recorder_stderr}")
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
        message(FATAL_ERROR "Unsigned 24-bit division ${artifact} artifact changed. Expected ${expected_${artifact}_sha256}; actual ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Semantic E7D7 replaces all 105 unsigned 24-bit divisions")
