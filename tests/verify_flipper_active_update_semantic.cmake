# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Flipper active-update regression is Release-only.")
endif()

set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_call_sha256 "a0c7a2bb9faac585503c80f421b884e198f70eaaf9121aa36a2cd479f9eacba8")
set(expected_write_sha256 "e90b88651b51114389be9d5c613ec2a70787920cc6829f04f676953ae955e626")

set(prefix_corpus_directory "${OUTPUT}.release-prefix-corpus")
set(prefix_corpus "${prefix_corpus_directory}/01-D458-246.dcrr")
set(prefix_corpus_valid FALSE)
if(EXISTS "${prefix_corpus}")
    execute_process(
        COMMAND "${APP}" --routine-replay-corpus "${prefix_corpus}" --rom "${ROM}"
        RESULT_VARIABLE cached_prefix_result OUTPUT_QUIET ERROR_QUIET)
    if(cached_prefix_result EQUAL 0)
        set(prefix_corpus_valid TRUE)
    endif()
endif()
if(NOT prefix_corpus_valid)
    execute_process(
        COMMAND "${APP}"
            --record-state-census "${OUTPUT}.release-prefix-assembly.csv"
            --observe-hook "01:D458"
            --routine-call-trace "${OUTPUT}.release-prefix-assembly.calls.csv"
            --routine-replay "01:D458@246"
            --routine-replay-corpus-out "${prefix_corpus_directory}"
            --rom "${ROM}"
        RESULT_VARIABLE prefix_capture_result
        OUTPUT_VARIABLE prefix_capture_stdout ERROR_VARIABLE prefix_capture_stderr)
    if(NOT prefix_capture_result EQUAL 0)
        message(FATAL_ERROR "Flipper release-prefix capture failed (${prefix_capture_result}).\n${prefix_capture_stdout}${prefix_capture_stderr}")
    endif()
endif()
execute_process(
    COMMAND "${APP}"
        --routine-replay-corpus "${prefix_corpus}"
        --replace-hook "01:D458@all"
        --rom "${ROM}"
    RESULT_VARIABLE prefix_replay_result
    OUTPUT_VARIABLE prefix_replay_stdout ERROR_VARIABLE prefix_replay_stderr)
if(NOT prefix_replay_result EQUAL 0)
    message(FATAL_ERROR "Flipper release-prefix direct replay failed (${prefix_replay_result}).\n${prefix_replay_stdout}${prefix_replay_stderr}")
endif()
string(FIND "${prefix_replay_stdout}" "231 body cycles, 9 ordered writes, native" prefix_native_offset)
if(prefix_native_offset EQUAL -1)
    message(FATAL_ERROR "Flipper release-prefix replay omitted native parity.\n${prefix_replay_stdout}${prefix_replay_stderr}")
endif()

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(abandonment_output "${OUTPUT}.abandonments.csv")
execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "01:D458"
        --observe-hook "01:D48E"
        --observe-hook "01:D491"
        --observe-hook "01:D494"
        --observe-hook "01:D4C6"
        --observe-hook "01:D4C9"
        --observe-hook "01:D4CC"
        --replace-hook "01:D458@all"
        --replace-hook "01:D48E@all"
        --replace-hook "01:D491@all"
        --replace-hook "01:D494@all"
        --replace-hook "01:D4C6@all"
        --replace-hook "01:D4C9@all"
        --replace-hook "01:D4CC@all"
        --routine-call-trace "${call_output}"
        --abandonment-census "${abandonment_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_stdout ERROR_VARIABLE recorder_stderr)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Flipper active-update recorder failed (${recorder_result}).\n${recorder_stdout}${recorder_stderr}")
endif()
foreach(expected_line IN ITEMS
    "Native replacement 01:D458@all entries: 246 of 246"
    "Native replacement 01:D48E@all entries: 490 of 490"
    "Native replacement 01:D491@all entries: 490 of 490"
    "Native replacement 01:D494@all entries: 490 of 490"
    "Native replacement 01:D4C6@all entries: 490 of 490"
    "Native replacement 01:D4C9@all entries: 490 of 490"
    "Native replacement 01:D4CC@all entries: 490 of 490"
    "Abandonment census complete: no transcript was abandoned"
    "Observe hook 1 entries: 246"
    "Observe hook 2 entries: 490"
    "Observe hook 3 entries: 490"
    "Observe hook 4 entries: 490"
    "Observe hook 5 entries: 490"
    "Observe hook 6 entries: 490"
    "Observe hook 7 entries: 490"
    "Routine call trace complete: 3186 calls, 125371 writes")
    string(FIND "${recorder_stdout}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Flipper active-update output omitted '${expected_line}'.\n${recorder_stdout}${recorder_stderr}")
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
        message(FATAL_ERROR "Flipper active-update ${artifact} artifact changed. Expected ${expected_${artifact}_sha256}; actual ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Semantic 01:D458 release prefix and both 490-call active-flipper passes are fully native")
