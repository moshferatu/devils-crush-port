# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Actor-slot-allocator semantic regression is Release-only.")
endif()

set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_call_sha256 "5a9ade12587db750ae8f19687c575db7aed21c037c480d7e1da701554a60e047")
set(expected_write_sha256 "b78d82ec7e26360c9ce207ff1038e7c558bd8981baf6da4f34fde75cb4d5ca73")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(abandonment_output "${OUTPUT}.abandonments.csv")
set(corpus_directory "${OUTPUT}.corpus")
set(corpus_ordinals 2 15 18 34 39 41)

set(corpus_arguments)
set(corpus_paths)
foreach(ordinal IN LISTS corpus_ordinals)
    set(corpus_path "${corpus_directory}/01-C5AB-${ordinal}.dcrr")
    list(APPEND corpus_paths "${corpus_path}")
    list(APPEND corpus_arguments --routine-replay-corpus "${corpus_path}")
endforeach()

set(needs_capture FALSE)
foreach(corpus_path IN LISTS corpus_paths)
    if(NOT EXISTS "${corpus_path}")
        set(needs_capture TRUE)
    endif()
endforeach()
if(NOT needs_capture)
    execute_process(
        COMMAND "${APP}" ${corpus_arguments} --rom "${ROM}"
        RESULT_VARIABLE cached_result
        OUTPUT_VARIABLE cached_stdout
        ERROR_VARIABLE cached_stderr
    )
    if(NOT cached_result EQUAL 0)
        set(needs_capture TRUE)
    endif()
endif()

if(needs_capture)
    set(capture_arguments
        --record-state-census "${OUTPUT}.assembly.csv"
        --observe-hook "01:C5AB"
        --routine-call-trace "${OUTPUT}.assembly.calls.csv"
        --routine-replay-corpus-out "${corpus_directory}"
    )
    foreach(ordinal IN LISTS corpus_ordinals)
        list(APPEND capture_arguments --routine-replay "01:C5AB@${ordinal}")
    endforeach()
    list(APPEND capture_arguments --rom "${ROM}")
    execute_process(
        COMMAND "${APP}" ${capture_arguments}
        RESULT_VARIABLE capture_result
        OUTPUT_VARIABLE capture_stdout
        ERROR_VARIABLE capture_stderr
    )
    if(NOT capture_result EQUAL 0)
        message(FATAL_ERROR "Actor-slot-allocator corpus capture failed (${capture_result}).\n${capture_stdout}${capture_stderr}")
    endif()
    foreach(expected_line IN ITEMS
        "Observe hook 1 entries: 44"
        "Routine replay complete: 6 state-normalized calls"
        "Routine replay corpus complete: 6 authoritative assembly calls"
    )
        string(FIND "${capture_stdout}" "${expected_line}" line_offset)
        if(line_offset EQUAL -1)
            message(FATAL_ERROR "Actor-slot-allocator corpus capture omitted '${expected_line}'.\n${capture_stdout}${capture_stderr}")
        endif()
    endforeach()
endif()

execute_process(
    COMMAND
        "${APP}" ${corpus_arguments}
        --replace-hook "01:C5AB@all"
        --rom "${ROM}"
    RESULT_VARIABLE replay_result
    OUTPUT_VARIABLE replay_stdout
    ERROR_VARIABLE replay_stderr
)
if(NOT replay_result EQUAL 0)
    message(FATAL_ERROR "Actor-slot-allocator direct replay failed (${replay_result}).\n${replay_stdout}${replay_stderr}")
endif()
foreach(expected_line IN ITEMS
    "Direct routine replay 01:C5AB@2 verified: frame 873, 4359 body cycles, 1 ordered writes, native"
    "Direct routine replay 01:C5AB@15 verified: frame 1409, 4944 body cycles, 1 ordered writes, native"
    "Direct routine replay 01:C5AB@18 verified: frame 1852, 4944 body cycles, 1 ordered writes, native"
    "Direct routine replay 01:C5AB@34 verified: frame 2600, 4404 body cycles, 1 ordered writes, native"
    "Direct routine replay corpus complete: 6 calls without route startup"
)
    string(FIND "${replay_stdout}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Actor-slot-allocator direct replay omitted '${expected_line}'.\n${replay_stdout}${replay_stderr}")
    endif()
endforeach()

function(verify_special_allocator_fixture label directory expected_line)
    set(corpus_path "${directory}/01-C5AB-2.dcrr")
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
                --record-state-census "${OUTPUT}.${label}.assembly.csv"
                --observe-hook "01:C5AB"
                --routine-call-trace "${OUTPUT}.${label}.assembly.calls.csv"
                --routine-replay "01:C5AB@2"
                --routine-replay-corpus-out "${directory}"
                ${ARGN}
                --rom "${ROM}"
            RESULT_VARIABLE capture_result
            OUTPUT_VARIABLE capture_stdout
            ERROR_VARIABLE capture_stderr
        )
        if(NOT capture_result EQUAL 0)
            message(FATAL_ERROR "Actor-slot-allocator ${label} corpus capture failed (${capture_result}).\n${capture_stdout}${capture_stderr}")
        endif()
    endif()
    execute_process(
        COMMAND "${APP}"
            --routine-replay-corpus "${corpus_path}"
            --replace-hook "01:C5AB@all"
            --rom "${ROM}"
        RESULT_VARIABLE replay_result
        OUTPUT_VARIABLE replay_stdout
        ERROR_VARIABLE replay_stderr
    )
    if(NOT replay_result EQUAL 0)
        message(FATAL_ERROR "Actor-slot-allocator ${label} replay failed (${replay_result}).\n${replay_stdout}${replay_stderr}")
    endif()
    string(FIND "${replay_stdout}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Actor-slot-allocator ${label} replay omitted '${expected_line}'.\n${replay_stdout}${replay_stderr}")
    endif()
endfunction()

verify_special_allocator_fixture(
    alternate-mode
    "${OUTPUT}.alternate-mode.corpus"
    "Direct routine replay 01:C5AB@2 verified: frame 873, 4374 body cycles, 2 ordered writes, native"
    --work-ram-patch "873:2055:01"
)

set(exhaustion_patches)
foreach(address IN ITEMS
    3062 3063 3064 3065 3066 3067 3068 3069 306A 306B 306C 306D 306E 306F
    3070 3071 3072 3073 3074 3075 3076 3077 3078 3079 307A 307B
)
    list(APPEND exhaustion_patches --work-ram-patch "873:${address}:80")
endforeach()
verify_special_allocator_fixture(
    exhausted-pool
    "${OUTPUT}.exhausted-pool.corpus"
    "Direct routine replay 01:C5AB@2 verified: frame 873, 5406 body cycles, 0 ordered writes, native"
    ${exhaustion_patches}
)

execute_process(
    COMMAND "${APP}" --record-state-census "${OUTPUT}"
        --observe-hook "01:C5AB" --replace-hook "01:C5AB@all"
        --routine-call-trace "${call_output}"
        --abandonment-census "${abandonment_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_stdout
    ERROR_VARIABLE recorder_stderr
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Actor-slot-allocator recorder failed (${recorder_result}).\n${recorder_stdout}${recorder_stderr}")
endif()
foreach(expected_line IN ITEMS
    "Native replacement 01:C5AB@all entries: 44 of 44"
    "Abandonment census complete: no transcript was abandoned"
    "Observe hook 1 entries: 44"
    "Routine call trace complete: 44 calls, 1077 writes"
)
    string(FIND "${recorder_stdout}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Actor-slot-allocator output omitted '${expected_line}'.\n${recorder_stdout}${recorder_stderr}")
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
        message(FATAL_ERROR "Actor-slot-allocator ${artifact} artifact changed. Expected ${expected_${artifact}_sha256}; actual ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Semantic C5AB replaces all 44 natural actor-slot allocations")
