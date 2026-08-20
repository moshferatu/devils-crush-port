# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Bank-7 mode-gate regression is Release-only.")
endif()

set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_call_sha256 "799723c75cca1ad3860a1f82a28fbf73781691f3796857ff74145cc3916a9dee")
set(expected_write_sha256 "c29f5be6dc7c7196fb3e855c26ff85fe21c7ccc24cfccfb42eaa8547fa0e2b8d")

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(abandonment_output "${OUTPUT}.abandonments.csv")
set(idle_corpus_directory "${OUTPUT}.idle-corpus")
set(idle_corpus_path "${idle_corpus_directory}/07-4003-1.dcrr")
set(exit_corpus_directory "${OUTPUT}.exit-corpus")
set(arm_corpus_directory "${OUTPUT}.arm-corpus")
set(arm_corpus_path "${arm_corpus_directory}/07-4003-260.dcrr")

set(needs_idle_capture TRUE)
if(EXISTS "${idle_corpus_path}")
    execute_process(
        COMMAND "${APP}" --routine-replay-corpus "${idle_corpus_path}" --rom "${ROM}"
        RESULT_VARIABLE cached_result OUTPUT_QUIET ERROR_QUIET)
    if(cached_result EQUAL 0)
        set(needs_idle_capture FALSE)
    endif()
endif()

set(needs_exit_capture FALSE)
foreach(ordinal RANGE 260 263)
    set(exit_path "${exit_corpus_directory}/07-4003-${ordinal}.dcrr")
    set(cached_result 1)
    if(EXISTS "${exit_path}")
        execute_process(
            COMMAND "${APP}" --routine-replay-corpus "${exit_path}" --rom "${ROM}"
            RESULT_VARIABLE cached_result OUTPUT_QUIET ERROR_QUIET)
    endif()
    if(NOT EXISTS "${exit_path}" OR NOT cached_result EQUAL 0)
        set(needs_exit_capture TRUE)
    endif()
endforeach()
if(needs_exit_capture)
    execute_process(
        COMMAND "${APP}"
            --record-state-census "${OUTPUT}.exit-assembly.csv"
            --work-ram-patch "1200:2D51:09"
            --work-ram-patch "1200:2D4F:3B"
            --work-ram-patch "1201:2D51:09"
            --work-ram-patch "1201:2D4F:46"
            --work-ram-patch "1202:2D51:09"
            --work-ram-patch "1202:2D4F:3C"
            --work-ram-patch "1202:2258:05"
            --work-ram-patch "1203:2D51:09"
            --work-ram-patch "1203:2D4F:3C"
            --work-ram-patch "1203:2258:06"
            --work-ram-patch "1203:20CB:01"
            --observe-hook "07:4003"
            --routine-call-trace "${OUTPUT}.exit-assembly.calls.csv"
            --routine-replay "07:4003@260"
            --routine-replay "07:4003@261"
            --routine-replay "07:4003@262"
            --routine-replay "07:4003@263"
            --routine-replay-corpus-out "${exit_corpus_directory}"
            --rom "${ROM}"
        RESULT_VARIABLE capture_result
        OUTPUT_VARIABLE capture_stdout ERROR_VARIABLE capture_stderr)
    if(NOT capture_result EQUAL 0)
        message(FATAL_ERROR "Bank-7 exit corpus capture failed (${capture_result}).\n${capture_stdout}${capture_stderr}")
    endif()
    foreach(expected_line IN ITEMS
        "Work-RAM patches applied: 11"
        "Routine replay 07:4003@260 verified: frame 1200, 102 body cycles, 1 ordered writes"
        "Routine replay 07:4003@261 verified: frame 1201, 114 body cycles, 1 ordered writes"
        "Routine replay 07:4003@262 verified: frame 1202, 141 body cycles, 1 ordered writes"
        "Routine replay 07:4003@263 verified: frame 1203, 135 body cycles, 0 ordered writes"
        "Routine replay corpus complete: 4 authoritative assembly calls")
        string(FIND "${capture_stdout}" "${expected_line}" line_offset)
        if(line_offset EQUAL -1)
            message(FATAL_ERROR "Bank-7 exit capture omitted '${expected_line}'.\n${capture_stdout}${capture_stderr}")
        endif()
    endforeach()
endif()
if(needs_idle_capture)
    execute_process(
        COMMAND "${APP}"
            --record-state-census "${OUTPUT}.idle-assembly.csv"
            --observe-hook "07:4003"
            --routine-call-trace "${OUTPUT}.idle-assembly.calls.csv"
            --routine-replay "07:4003@1"
            --routine-replay-corpus-out "${idle_corpus_directory}"
            --rom "${ROM}"
        RESULT_VARIABLE capture_result
        OUTPUT_VARIABLE capture_stdout ERROR_VARIABLE capture_stderr)
    if(NOT capture_result EQUAL 0)
        message(FATAL_ERROR "Bank-7 idle corpus capture failed (${capture_result}).\n${capture_stdout}${capture_stderr}")
    endif()
    foreach(expected_line IN ITEMS
        "Observe hook 1 entries: 2649"
        "Routine replay 07:4003@1 verified: frame 941, 75 body cycles, 1 ordered writes"
        "Routine replay corpus complete: 1 authoritative assembly calls")
        string(FIND "${capture_stdout}" "${expected_line}" line_offset)
        if(line_offset EQUAL -1)
            message(FATAL_ERROR "Bank-7 idle capture omitted '${expected_line}'.\n${capture_stdout}${capture_stderr}")
        endif()
    endforeach()
endif()

set(needs_arm_capture TRUE)
if(EXISTS "${arm_corpus_path}")
    execute_process(
        COMMAND "${APP}" --routine-replay-corpus "${arm_corpus_path}" --rom "${ROM}"
        RESULT_VARIABLE cached_result OUTPUT_QUIET ERROR_QUIET)
    if(cached_result EQUAL 0)
        set(needs_arm_capture FALSE)
    endif()
endif()
if(needs_arm_capture)
    execute_process(
        COMMAND "${APP}"
            --record-state-census "${OUTPUT}.arm-assembly.csv"
            --work-ram-patch "1200:2D51:09"
            --work-ram-patch "1200:2D4F:3C"
            --work-ram-patch "1200:2258:06"
            --work-ram-patch "1200:20CB:00"
            --observe-hook "07:4003"
            --routine-call-trace "${OUTPUT}.arm-assembly.calls.csv"
            --routine-replay "07:4003@260"
            --routine-replay-corpus-out "${arm_corpus_directory}"
            --rom "${ROM}"
        RESULT_VARIABLE capture_result
        OUTPUT_VARIABLE capture_stdout ERROR_VARIABLE capture_stderr)
    if(NOT capture_result EQUAL 0)
        message(FATAL_ERROR "Bank-7 arm corpus capture failed (${capture_result}).\n${capture_stdout}${capture_stderr}")
    endif()
    foreach(expected_line IN ITEMS
        "Work-RAM patches applied: 4"
        "Routine replay 07:4003@260 verified: frame 1200, 219 body cycles, 3 ordered writes"
        "Routine replay corpus complete: 1 authoritative assembly calls")
        string(FIND "${capture_stdout}" "${expected_line}" line_offset)
        if(line_offset EQUAL -1)
            message(FATAL_ERROR "Bank-7 arm capture omitted '${expected_line}'.\n${capture_stdout}${capture_stderr}")
        endif()
    endforeach()
endif()

foreach(corpus_path IN ITEMS
    "${idle_corpus_path}"
    "${exit_corpus_directory}/07-4003-260.dcrr"
    "${exit_corpus_directory}/07-4003-261.dcrr"
    "${exit_corpus_directory}/07-4003-262.dcrr"
    "${exit_corpus_directory}/07-4003-263.dcrr"
    "${arm_corpus_path}")
    execute_process(
        COMMAND "${APP}"
            --routine-replay-corpus "${corpus_path}"
            --replace-hook "07:4003@all"
            --rom "${ROM}"
        RESULT_VARIABLE replay_result
        OUTPUT_VARIABLE replay_stdout ERROR_VARIABLE replay_stderr)
    if(NOT replay_result EQUAL 0)
        message(FATAL_ERROR "Bank-7 direct replay failed (${replay_result}).\n${replay_stdout}${replay_stderr}")
    endif()
    string(FIND "${replay_stdout}" "native" native_offset)
    if(native_offset EQUAL -1)
        message(FATAL_ERROR "Bank-7 direct replay did not run natively.\n${replay_stdout}${replay_stderr}")
    endif()
endforeach()

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "07:4003"
        --replace-hook "07:4003@all"
        --routine-call-trace "${call_output}"
        --abandonment-census "${abandonment_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_stdout ERROR_VARIABLE recorder_stderr)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Bank-7 mode-gate recorder failed (${recorder_result}).\n${recorder_stdout}${recorder_stderr}")
endif()
foreach(expected_line IN ITEMS
    "Native replacement 07:4003@all entries: 2649 of 2649"
    "Abandonment census complete: no transcript was abandoned"
    "Observe hook 1 entries: 2649"
    "Routine call trace complete: 2649 calls, 3602 writes")
    string(FIND "${recorder_stdout}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Bank-7 mode-gate output omitted '${expected_line}'.\n${recorder_stdout}${recorder_stderr}")
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
        message(FATAL_ERROR "Bank-7 mode-gate ${artifact} artifact changed. Expected ${expected_${artifact}_sha256}; actual ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Semantic 07:4003 replaces all 2,649 canonical mode-gate calls")
