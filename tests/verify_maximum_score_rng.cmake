# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Debug" AND NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported maximum-score RNG configuration: ${CONFIG}")
endif()

if(CONFIG STREQUAL "Debug")
    set(expected_census_sha256 "ac639564102a7549bc6aaa58aabb32e3231afafc5a3e3defb8d2a383a07bbdfb")
else()
    set(expected_census_sha256 "97f5089683bb67c53a3755a2d320ce1bcf5ea5e347d65200373a3f22b69cbc4b")
endif()
set(expected_trace_sha256 "698b5ebf3b98bc33f35019a2f95041c473989c706dafc31eacd75c2683aaecbc")
set(expected_call_sha256 "49cd13f4257c7f596428f2ac120bc353ad260c5f5a2d5b962bdd2206de93edbf")
set(expected_write_sha256 "2beaddce2b938f3d9a3c6a8ce1afd565719b7d234ab5e6f49743823c27b4187e")

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-gameplay "${OUTPUT}"
        --work-ram-patch "1300:2CC0:80"
        --pass-through-hook "07:48C7"
        --pass-through-hook "07:48CF"
        --observe-hook "07:48B2"
        --observe-hook "07:48DF"
        --observe-hook "00:E8B3"
        --routine-trace "${trace_output}"
        --routine-call-trace "${call_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Maximum-score RNG recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "Gameplay recording complete: 1800 frames"
    "Work-RAM patches applied: 1"
    "Pass-through hook 1 entries: 23"
    "Pass-through hook 2 entries: 23"
    "Observe hook 1 entries: 184"
    "Observe hook 2 entries: 23"
    "Observe hook 3 entries: 62"
    "Routine trace complete: 269 entries"
    "Routine call trace complete: 269 calls, 10041 writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Maximum-score RNG output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census trace call write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "trace")
        set(artifact_path "${trace_output}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    else()
        set(artifact_path "${write_output}")
    endif()
    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "Maximum-score RNG ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}"
        )
    endif()
endforeach()

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-MaximumScoreRngSemantics.ps1"
        -RomPath "${ROM}"
        -CallPath "${call_output}"
        -WritePath "${write_output}"
    RESULT_VARIABLE semantics_result
    OUTPUT_VARIABLE semantics_output
    ERROR_VARIABLE semantics_error
)
if(NOT semantics_result EQUAL 0)
    message(
        FATAL_ERROR
        "Maximum-score RNG analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Maximum-score ending RNG semantics verified."
    "Transition: score saturation sets 2CC0.7"
    "Consumer 48C7: 23 low nibbles select 15 of 16"
    "Consumer 48CF: 23 low bits select command 2C seven times"
    "Ending effect: every event allocates six type-8F actors"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Maximum-score RNG semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "Maximum-score RNG artifacts verified.\n${semantics_output}")
