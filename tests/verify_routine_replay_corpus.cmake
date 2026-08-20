if(NOT DEFINED APP OR NOT DEFINED ROM OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "APP, ROM, and OUTPUT are required")
endif()

set(corpus_directory "${OUTPUT}.corpus")
set(corpus_path_1 "${corpus_directory}/00-F241-1.dcrr")
set(corpus_path_2 "${corpus_directory}/00-F241-2.dcrr")
set(call_output "${OUTPUT}.calls.csv")

# A valid local fixture is the cache: verify it through original assembly
# before trusting it. Missing, corrupt, wrong-ROM, and wrong-core fixtures all
# fall through to one authoritative route capture and are replaced atomically.
set(needs_capture TRUE)
if(EXISTS "${corpus_path_1}" AND EXISTS "${corpus_path_2}")
    execute_process(
        COMMAND
            "${APP}"
            --routine-replay-corpus "${corpus_path_1}"
            --routine-replay-corpus "${corpus_path_2}"
            --rom "${ROM}"
        RESULT_VARIABLE cached_result
        OUTPUT_VARIABLE cached_stdout
        ERROR_VARIABLE cached_stderr
    )
    if(cached_result EQUAL 0)
        string(
            FIND "${cached_stdout}"
            "Direct routine replay corpus complete: 2 calls without route startup"
            cached_line
        )
        if(NOT cached_line EQUAL -1)
            set(needs_capture FALSE)
            message(STATUS "Reusing two verified F241 routine-replay corpus entries")
        endif()
    endif()
endif()

if(needs_capture)
    execute_process(
        COMMAND
            "${APP}"
            --record-gameplay "${OUTPUT}"
            --observe-hook "00:F241"
            --routine-call-trace "${call_output}"
            --routine-replay "00:F241@1"
            --routine-replay "00:F241@2"
            --routine-replay-corpus-out "${corpus_directory}"
            --rom "${ROM}"
        RESULT_VARIABLE capture_result
        OUTPUT_VARIABLE capture_stdout
        ERROR_VARIABLE capture_stderr
    )
    if(NOT capture_result EQUAL 0)
        message(
            FATAL_ERROR
            "Routine-replay corpus capture failed (${capture_result}).\n"
            "${capture_stdout}${capture_stderr}"
        )
    endif()
    foreach(expected_line IN ITEMS
        "Gameplay recording complete: 1800 frames"
        "Observe hook 1 entries: 404"
        "Routine replay 00:F241@1 verified: frame 1064, 162 body cycles, 6 ordered writes"
        "Routine replay 00:F241@2 verified: frame 1072, 162 body cycles, 6 ordered writes"
        "Routine replay corpus 00:F241@1 persisted:"
        "Routine replay corpus 00:F241@2 persisted:"
        "Routine replay corpus complete: 2 authoritative assembly calls"
    )
        string(FIND "${capture_stdout}" "${expected_line}" line_offset)
        if(line_offset EQUAL -1)
            message(
                FATAL_ERROR
                "Routine-replay corpus capture omitted '${expected_line}'.\n"
                "${capture_stdout}${capture_stderr}"
            )
        endif()
    endforeach()
endif()

if(NOT EXISTS "${corpus_path_1}" OR NOT EXISTS "${corpus_path_2}")
    message(FATAL_ERROR "Both routine-replay corpus fixtures were not created")
endif()

# This is the accelerated edit loop: restore one pre-call machine state and
# compare the native adapter with the persisted assembly oracle without
# booting or traversing the 1,800-frame route.
execute_process(
    COMMAND
        "${APP}"
        --routine-replay-corpus "${corpus_path_1}"
        --routine-replay-corpus "${corpus_path_2}"
        --replace-hook "00:F241@all"
        --rom "${ROM}"
    RESULT_VARIABLE replay_result
    OUTPUT_VARIABLE replay_stdout
    ERROR_VARIABLE replay_stderr
)
if(NOT replay_result EQUAL 0)
    message(
        FATAL_ERROR
        "Direct native corpus replay failed (${replay_result}).\n"
        "${replay_stdout}${replay_stderr}"
    )
endif()
foreach(expected_line IN ITEMS
    "Direct routine replay 00:F241@1 verified: frame 1064, 162 body cycles, 6 ordered writes, native"
    "Direct routine replay 00:F241@2 verified: frame 1072, 162 body cycles, 6 ordered writes, native"
    "Direct routine replay corpus complete: 2 calls without route startup"
)
    string(FIND "${replay_stdout}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Direct native corpus replay omitted '${expected_line}'.\n"
            "${replay_stdout}${replay_stderr}"
        )
    endif()
endforeach()

message(STATUS "Two persisted F241 calls verified against assembly and native code")
