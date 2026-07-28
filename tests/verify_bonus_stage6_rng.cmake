# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Debug" AND NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported BS-6 RNG configuration: ${CONFIG}")
endif()

set(expected_census_sha256 "5e48d45f879ccddd0231a4642a7542fb117abbd578c7d81666d7780e282c9238")
set(expected_trace_sha256 "7260f94a57e30deaab5f972ab1f9e7b0786c14f47ee6ed7e6e9ab1ac1fdc3201")
set(expected_call_sha256 "9dd2612a519f54ced33a70b9d871929a08c229d84a9c2ab249e353285603488d")
set(expected_write_sha256 "347f443af749d6da0d934fdad52f245647659bfbf954e801b2c7487c9949cbda")

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-gameplay "${OUTPUT}"
        --work-ram-patch "1300:2055:06"
        --work-ram-patch "1300:2053:10"
        --work-ram-patch "1403:3834:90"
        --work-ram-patch "1403:3C34:01"
        --work-ram-patch "1403:3835:90"
        --work-ram-patch "1403:3C35:02"
        --pass-through-hook "05:732E"
        --pass-through-hook "05:733B"
        --observe-hook "05:72E0"
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
        "BS-6 RNG recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "Gameplay recording complete: 1800 frames"
    "Work-RAM patches applied: 6"
    "Pass-through hook 1 entries: 1"
    "Pass-through hook 2 entries: 1"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 5"
    "Routine trace complete: 6 entries"
    "Routine call trace complete: 6 calls, 321 writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-6 RNG output omitted '${expected_line}'.\n"
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
            "BS-6 RNG ${artifact} artifact changed.\n"
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
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-BonusStage6RngSemantics.ps1"
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
        "BS-6 RNG analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "BS-6 hazard RNG semantics verified."
    "Consumer 732E: A0 & 1F plus 18 installs timer 18"
    "Consumer 733B: C5 selects motion-table index/value 5"
    "Allocator 72E0: objective slot 35 creates one type-C2 hazard"
    "E8B3: all five routed calls preserve the eight-write recurrence."
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-6 RNG semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "BS-6 RNG artifacts verified.\n${semantics_output}")
