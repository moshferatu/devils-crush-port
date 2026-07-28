# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Debug" AND NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported BS-2-resolution configuration: ${CONFIG}")
endif()

set(
    expected_census_sha256
    "e4104154585c0062c9f26cb31551d5e0f2015e240bea74868e61947c3d8707f5"
)
set(
    expected_trace_sha256
    "70e149e081ad494ca9c61de285e14df6a7b8df2a1876ad5e19e3bb984162a1de"
)
set(
    expected_call_sha256
    "11797b82527289b89446716b765ef611e5932ccef55e25ee767718c279fa3843"
)
set(
    expected_write_sha256
    "d4b7325d26780c5918d0f5698fafbbb96c57cf614886056774724f25255f3701"
)
set(
    expected_state_sha256
    "52c30a3f2635e9e5e590c03e28fa3289fdb2b5789952e5594f7d5e90725370f6"
)
set(
    expected_timeout_census_sha256
    "c0aa885ddd4df8e8993ccf532f567123dea9963150650c0d77e8d6184c8058ad"
)
set(
    expected_timeout_state_sha256
    "189f9ba5c9dcb5b5a1ea46308e39ccbba5e97a8b3aa0b9ea29758c025766d1f7"
)
set(
    expected_completion_census_sha256
    "eac2656f6d53a22600db65c2dd38279a7b19d7057a482021b407d46501249beb"
)
set(
    expected_completion_state_sha256
    "152d7d5d51820be1c7e81daeeca689319898fe3d2d05667c1b17031fc42899dd"
)

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(state_output "${OUTPUT}.state.csv")
set(timeout_output "${OUTPUT}.timeout.csv")
set(timeout_state_output "${timeout_output}.state.csv")
set(completion_output "${OUTPUT}.completion.csv")
set(completion_state_output "${completion_output}.state.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${OUTPUT}"
        --work-ram-patch "1300:2055:02"
        --work-ram-patch "1300:2053:10"
        --work-ram-patch "1404:3F34:FB"
        --work-ram-patch "1404:3FB4:4E"
        --work-ram-patch "1404:3A34:01"
        --work-ram-patch "1404:3B34:05"
        --work-ram-patch "1404:3F35:83"
        --work-ram-patch "1404:3FB5:4F"
        --work-ram-patch "1404:3A35:01"
        --work-ram-patch "1404:3B35:05"
        --work-ram-patch "1404:3053:C2"
        --work-ram-patch "1404:30D3:84"
        --work-ram-patch "1404:31D3:60"
        --work-ram-patch "1404:3253:00"
        --work-ram-patch "1404:3353:3C"
        --work-ram-patch "1404:33D3:00"
        --work-ram-patch "1404:3753:11"
        --work-ram-patch "1404:34D3:00"
        --work-ram-patch "1404:3853:90"
        --work-ram-patch "1404:3D53:81"
        --work-ram-patch "1404:3DD3:10"
        --work-ram-patch "1404:3C53:01"
        --work-ram-patch "1404:3953:1D"
        --work-ram-patch "1404:3F53:79"
        --work-ram-patch "1404:3FD3:50"
        --observe-hook "04:4EA4"
        --observe-hook "04:4F2A"
        --observe-hook "04:511D"
        --observe-hook "04:5169"
        --observe-hook "04:4EFB"
        --observe-hook "04:4F83"
        --observe-hook "04:4FBA"
        --observe-hook "04:5079"
        --observe-hook "04:5260"
        --observe-hook "04:52ED"
        --observe-hook "04:5051"
        --routine-trace "${trace_output}"
        --routine-call-trace "${call_output}"
        --work-ram-write-range "2D00-2DFF"
        --work-ram-write-trace "${state_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "BS-2 resolution recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 25"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 1"
    "Observe hook 3 entries: 1"
    "Observe hook 4 entries: 1"
    "Observe hook 5 entries: 1"
    "Observe hook 6 entries: 1"
    "Observe hook 7 entries: 2"
    "Observe hook 8 entries: 1"
    "Observe hook 9 entries: 1"
    "Observe hook 10 entries: 4"
    "Observe hook 11 entries: 2"
    "Routine trace complete: 16 entries"
    "Routine call trace complete: 16 calls, 3741 writes"
    "Work-RAM write trace complete: 1101 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-2 resolution output omitted '${expected_line}'.\n"
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
            "BS-2 resolution ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

if(CONFIG STREQUAL "Release")
execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${timeout_output}"
        --work-ram-patch "1300:2055:02"
        --work-ram-patch "1300:2053:10"
        --pass-through-hook "04:4E97"
        --pass-through-hook "04:456C"
        --pass-through-hook "04:4DC4"
        --work-ram-write-range "2D00-2DFF"
        --work-ram-write-trace "${timeout_state_output}"
        --rom "${ROM}"
    RESULT_VARIABLE timeout_result
    OUTPUT_VARIABLE timeout_stdout
    ERROR_VARIABLE timeout_stderr
)
if(NOT timeout_result EQUAL 0)
    message(
        FATAL_ERROR
        "BS-2 timeout recorder failed (${timeout_result}).\n"
        "${timeout_stdout}${timeout_stderr}"
    )
endif()
foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 2"
    "Pass-through hook 1 entries: 2"
    "Pass-through hook 2 entries: 1"
    "Pass-through hook 3 entries: 240"
    "Work-RAM write trace complete: 1156 selected writes"
)
    string(FIND "${timeout_stdout}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-2 timeout output omitted '${expected_line}'.\n"
            "${timeout_stdout}${timeout_stderr}"
        )
    endif()
endforeach()
file(SHA256 "${timeout_output}" actual_timeout_census_sha256)
file(SHA256 "${timeout_state_output}" actual_timeout_state_sha256)
if(
    NOT actual_timeout_census_sha256 STREQUAL
        expected_timeout_census_sha256 OR
    NOT actual_timeout_state_sha256 STREQUAL
        expected_timeout_state_sha256
)
    message(
        FATAL_ERROR
        "BS-2 timeout artifacts changed.\n"
        "Census: ${actual_timeout_census_sha256}\n"
        "State:  ${actual_timeout_state_sha256}"
    )
endif()

execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${completion_output}"
        --work-ram-patch "1300:2055:02"
        --work-ram-patch "1300:2053:10"
        --work-ram-patch "1404:2D2E:00"
        --work-ram-patch "1404:2DB7:01"
        --work-ram-patch "1404:2DBA:01"
        --pass-through-hook "04:4E97"
        --pass-through-hook "04:44DB"
        --pass-through-hook "04:456C"
        --pass-through-hook "04:464F"
        --pass-through-hook "04:4DC4"
        --work-ram-write-range "2D00-2DFF"
        --work-ram-write-trace "${completion_state_output}"
        --rom "${ROM}"
    RESULT_VARIABLE completion_result
    OUTPUT_VARIABLE completion_stdout
    ERROR_VARIABLE completion_stderr
)
if(NOT completion_result EQUAL 0)
    message(
        FATAL_ERROR
        "BS-2 completion recorder failed (${completion_result}).\n"
        "${completion_stdout}${completion_stderr}"
    )
endif()
foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 5"
    "Pass-through hook 1 entries: 2"
    "Pass-through hook 2 entries: 1"
    "Pass-through hook 3 entries: 1"
    "Pass-through hook 4 entries: 1"
    "Pass-through hook 5 entries: 462"
    "Work-RAM write trace complete: 2800 selected writes"
)
    string(FIND "${completion_stdout}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-2 completion output omitted '${expected_line}'.\n"
            "${completion_stdout}${completion_stderr}"
        )
    endif()
endforeach()
file(SHA256 "${completion_output}" actual_completion_census_sha256)
file(SHA256 "${completion_state_output}" actual_completion_state_sha256)
if(
    NOT actual_completion_census_sha256 STREQUAL
        expected_completion_census_sha256 OR
    NOT actual_completion_state_sha256 STREQUAL
        expected_completion_state_sha256
)
    message(
        FATAL_ERROR
        "BS-2 completion artifacts changed.\n"
        "Census: ${actual_completion_census_sha256}\n"
        "State:  ${actual_completion_state_sha256}"
    )
endif()
set(
    result_analysis_args
    -TimeoutStatePath "${timeout_state_output}"
    -CompletionStatePath "${completion_state_output}"
)
else()
set(result_analysis_args -SkipResults)
endif()

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-BonusStage2ResolutionSemantics.ps1"
        -RomPath "${ROM}"
        -CallPath "${call_output}"
        -WritePath "${write_output}"
        -StatePath "${state_output}"
        ${result_analysis_args}
    RESULT_VARIABLE semantics_result
    OUTPUT_VARIABLE semantics_output
    ERROR_VARIABLE semantics_error
)
if(NOT semantics_result EQUAL 0)
    message(
        FATAL_ERROR
        "BS-2 resolution analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
set(expected_semantics_lines
    "BS-2 movement, resolution, and result semantics verified."
    "Paired emitters: slots 34/35 create subtype 0/1 targets"
    "Board resolution: valid 8x5 perimeter cell clears four tiles"
    "Remaining actors: slot 50 stages the ball at (63,205)"
)
if(CONFIG STREQUAL "Release")
    list(
        APPEND expected_semantics_lines
        "Timeout: zero progress produces 240 glyph ticks"
        "Completion: multiplier 50 builds 50,000 stored units"
    )
endif()
foreach(expected_line IN LISTS expected_semantics_lines)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-2 resolution semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(
    STATUS
    "BS-2 resolution/result artifacts verified.\n${semantics_output}"
)
