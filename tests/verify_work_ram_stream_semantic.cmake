# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Work-RAM stream regression is Release-only under the native-port cadence.")
endif()

# 00:E4E3 expands an MPR-mapped byte stream into work RAM. A call contains 257
# data records plus its terminator and exceeds the transaction channel as a
# whole, so each backward branch re-enters E4E3 as a separate bounded record.
# The nested call/write artifact is the oracle: it proves all record boundaries,
# every copied byte, zero-page pointer update, stack write, interrupt passage,
# and the complete outer return without relying only on the final RAM image.
set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_call_sha256 "67165e3a8d99c7a382dc5779adf4aa4980ded8cb3b6935cd378f4c91ef3f836c")
set(expected_write_sha256 "38c4ecd31a4fc15f90877f3e99c08709ab25262255b9dbd6c1e3ab1173481ff1")

set(corpus_directory "${OUTPUT}.corpus")
set(corpus "${corpus_directory}/00-E4E3-258.dcrr")
if(NOT EXISTS "${corpus}")
    execute_process(
        COMMAND
            "${APP}"
            --record-state-census "${OUTPUT}.assembly-capture.csv"
            --observe-hook "00:E4E3"
            --routine-call-trace "${OUTPUT}.assembly-capture.calls.csv"
            --routine-replay "00:E4E3@258"
            --routine-replay-corpus-out "${corpus_directory}"
            --rom "${ROM}"
        RESULT_VARIABLE capture_result
        OUTPUT_VARIABLE capture_output
        ERROR_VARIABLE capture_error
    )
    if(NOT capture_result EQUAL 0)
        message(FATAL_ERROR "Work-RAM stream corpus capture failed (${capture_result}).\n${capture_output}${capture_error}")
    endif()
    string(FIND "${capture_output}" "Routine replay 00:E4E3@258 verified: frame 874, 39 body cycles, 0 ordered writes" capture_offset)
    if(capture_offset EQUAL -1)
        message(FATAL_ERROR "Work-RAM stream capture omitted the terminal fixture.\n${capture_output}${capture_error}")
    endif()
endif()

execute_process(
    COMMAND
        "${APP}"
        --routine-replay-corpus "${corpus}"
        --replace-hook "00:E4E3@all"
        --rom "${ROM}"
    RESULT_VARIABLE replay_result
    OUTPUT_VARIABLE replay_output
    ERROR_VARIABLE replay_error
)
if(NOT replay_result EQUAL 0)
    message(FATAL_ERROR "Work-RAM stream terminal replay failed (${replay_result}).\n${replay_output}${replay_error}")
endif()
string(FIND "${replay_output}" "Direct routine replay 00:E4E3@258 verified: frame 874, 39 body cycles, 0 ordered writes, native" replay_offset)
if(replay_offset EQUAL -1)
    message(FATAL_ERROR "Work-RAM stream direct replay omitted its native proof.\n${replay_output}${replay_error}")
endif()

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(abandonment_output "${OUTPUT}.abandonments.csv")
execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "00:E4E3"
        --replace-hook "00:E4E3@all"
        --routine-call-trace "${call_output}"
        --abandonment-census "${abandonment_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic work-RAM stream recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 00:E4E3@all entries: 258 of 258"
    "Abandonment census complete: no transcript was abandoned"
    "Observe hook 1 entries: 258"
    "Routine call trace complete: 258 calls, 481291 writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic work-RAM stream output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(FATAL_ERROR "Semantic work-RAM stream ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}\n")
    endif()
endforeach()

message(STATUS "Semantic 00:E4E3 expands all 257 records natively across 258 bounded entries with exact nested call, ordered-write, interrupt, and state parity.")
