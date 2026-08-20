# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Flipper release-arm regression is Release-only.")
endif()

set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_call_sha256 "7d695bf99a8e6bdd43179693d3d5270a14416b21c8771233055aecbc4d45656b")
set(expected_write_sha256 "cd7de400e8c59f91f0b3352bc58cb3a15450c71b3d3077382b97f1c59631253b")

function(ensure_release_corpus label patch expected_replay)
    set(corpus_directory "${OUTPUT}.${label}-corpus")
    set(corpus_path "${corpus_directory}/01-D4B3-3.dcrr")
    set(needs_capture TRUE)
    if(EXISTS "${corpus_path}")
        execute_process(
            COMMAND "${APP}" --routine-replay-corpus "${corpus_path}" --rom "${ROM}"
            RESULT_VARIABLE cached_result OUTPUT_QUIET ERROR_QUIET)
        if(cached_result EQUAL 0)
            set(needs_capture FALSE)
        endif()
    endif()
    if(needs_capture)
        set(command
            "${APP}"
            --record-state-census "${OUTPUT}.${label}-assembly.csv"
            --observe-hook "01:D4B3"
            --routine-call-trace "${OUTPUT}.${label}-assembly.calls.csv"
            --routine-replay "01:D4B3@3"
            --routine-replay-corpus-out "${corpus_directory}")
        if(NOT "${patch}" STREQUAL "")
            list(APPEND command --work-ram-patch "${patch}")
        endif()
        list(APPEND command --rom "${ROM}")
        execute_process(
            COMMAND ${command}
            RESULT_VARIABLE capture_result
            OUTPUT_VARIABLE capture_stdout ERROR_VARIABLE capture_stderr)
        if(NOT capture_result EQUAL 0)
            message(FATAL_ERROR "Flipper release ${label} capture failed (${capture_result}).\n${capture_stdout}${capture_stderr}")
        endif()
        foreach(expected_line IN ITEMS
            "${expected_replay}"
            "Routine replay corpus complete: 1 authoritative assembly calls")
            string(FIND "${capture_stdout}" "${expected_line}" line_offset)
            if(line_offset EQUAL -1)
                message(FATAL_ERROR "Flipper release ${label} capture omitted '${expected_line}'.\n${capture_stdout}${capture_stderr}")
            endif()
        endforeach()
    endif()
endfunction()

ensure_release_corpus(
    zero ""
    "Routine replay 01:D4B3@3 verified: frame 1267, 252 body cycles, 10 ordered writes")
ensure_release_corpus(
    one "1267:3D89:01"
    "Routine replay 01:D4B3@3 verified: frame 1267, 273 body cycles, 11 ordered writes")
ensure_release_corpus(
    two "1267:3D89:02"
    "Routine replay 01:D4B3@3 verified: frame 1267, 195 body cycles, 8 ordered writes")

foreach(label IN ITEMS zero one two)
    execute_process(
        COMMAND "${APP}"
            --routine-replay-corpus "${OUTPUT}.${label}-corpus/01-D4B3-3.dcrr"
            --replace-hook "01:D4B3@all"
            --rom "${ROM}"
        RESULT_VARIABLE replay_result
        OUTPUT_VARIABLE replay_stdout ERROR_VARIABLE replay_stderr)
    if(NOT replay_result EQUAL 0)
        message(FATAL_ERROR "Flipper release ${label} direct replay failed (${replay_result}).\n${replay_stdout}${replay_stderr}")
    endif()
    string(FIND "${replay_stdout}" "native" native_offset)
    if(native_offset EQUAL -1)
        message(FATAL_ERROR "Flipper release ${label} replay did not run natively.\n${replay_stdout}${replay_stderr}")
    endif()
endforeach()

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(abandonment_output "${OUTPUT}.abandonments.csv")
execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "01:D4B3"
        --replace-hook "01:D4B3@all"
        --routine-call-trace "${call_output}"
        --abandonment-census "${abandonment_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_stdout ERROR_VARIABLE recorder_stderr)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Flipper release-arm recorder failed (${recorder_result}).\n${recorder_stdout}${recorder_stderr}")
endif()
foreach(expected_line IN ITEMS
    "Native replacement 01:D4B3@all entries: 246 of 246"
    "Abandonment census complete: no transcript was abandoned"
    "Observe hook 1 entries: 246"
    "Routine call trace complete: 246 calls, 2759 writes")
    string(FIND "${recorder_stdout}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Flipper release-arm output omitted '${expected_line}'.\n${recorder_stdout}${recorder_stderr}")
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
        message(FATAL_ERROR "Flipper release-arm ${artifact} artifact changed. Expected ${expected_${artifact}_sha256}; actual ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Semantic 01:D4B3 replaces all 246 canonical release-arm calls")
