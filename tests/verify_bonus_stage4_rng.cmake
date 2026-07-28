# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Debug" AND NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported BS-4 RNG configuration: ${CONFIG}")
endif()

set(expected_census_sha256 "1c1ac47ddc107600e254ea3693ebe7852ec9a9addb52fa64374ba3192eee2817")
set(expected_trace_sha256 "078475a2616e02c1190ed6ca84459985c5f004f709a53b1afd8c837fdd71880c")
set(expected_call_sha256 "4937301556a172c2d2f9694167cd0c7c48596c280c27dbe3c1a9c8b23298f29a")
set(expected_write_sha256 "e93d97f153f5afac834d06766943f0f94de3d22e53aa5a3d6563bed8432932f0")
set(expected_state_sha256 "92f94c30b19ff03a3645a7df63eb6e3ce6e6544383fa555130cc6999726cdb86")

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(state_output "${OUTPUT}.state.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-gameplay "${OUTPUT}"
        --work-ram-patch "1300:2055:04"
        --work-ram-patch "1300:2053:10"
        --work-ram-patch "1403:3F35:CC"
        --work-ram-patch "1403:3FB5:6A"
        --work-ram-patch "1403:3CB5:00"
        --work-ram-patch "1403:3067:C2"
        --work-ram-patch "1403:3F67:A0"
        --work-ram-patch "1403:3FE7:6C"
        --work-ram-patch "1403:3B67:01"
        --work-ram-patch "1403:3167:60"
        --work-ram-patch "1403:31E7:00"
        --work-ram-patch "1403:3267:48"
        --work-ram-patch "1403:32E7:00"
        --work-ram-patch "1403:3E67:00"
        --work-ram-patch "1403:3867:10"
        --pass-through-hook "05:6B88"
        --pass-through-hook "05:6CB1"
        --observe-hook "00:E8B3"
        --routine-trace "${trace_output}"
        --routine-call-trace "${call_output}"
        --work-ram-write-range "2200-22FF"
        --work-ram-write-range "3300-33FF"
        --work-ram-write-range "3B00-3BFF"
        --work-ram-write-trace "${state_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "BS-4 RNG recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "Gameplay recording complete: 1800 frames"
    "Work-RAM patches applied: 15"
    "Pass-through hook 1 entries: 18"
    "Pass-through hook 2 entries: 22"
    "Observe hook 1 entries: 40"
    "Routine trace complete: 40 entries"
    "Routine call trace complete: 40 calls, 320 writes"
    "Work-RAM write trace complete: 16779 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-4 RNG output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census trace call write state)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "trace")
        set(artifact_path "${trace_output}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    elseif(artifact STREQUAL "write")
        set(artifact_path "${write_output}")
    else()
        set(artifact_path "${state_output}")
    endif()
    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "BS-4 RNG ${artifact} artifact changed.\n"
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
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-BonusStage4RngSemantics.ps1"
        -RomPath "${ROM}"
        -CallPath "${call_output}"
        -WritePath "${write_output}"
        -StatePath "${state_output}"
    RESULT_VARIABLE semantics_result
    OUTPUT_VARIABLE semantics_output
    ERROR_VARIABLE semantics_error
)
if(NOT semantics_result EQUAL 0)
    message(
        FATAL_ERROR
        "BS-4 RNG analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "BS-4 linked-target RNG semantics verified."
    "Consumer 6B88: 18 calls reload a 32-tick timer"
    "Consumer 6CB1: 22 calls use the same transform"
    "E8B3: all 40 calls preserve the exact eight-write RNG recurrence."
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-4 RNG semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "BS-4 RNG artifacts verified.\n${semantics_output}")
