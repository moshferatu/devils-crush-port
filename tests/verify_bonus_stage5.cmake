# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Debug" AND NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported BS-5 configuration: ${CONFIG}")
endif()

set(expected_census_sha256 "80511598feac5e628433f3b850831d6667e09e79e7ed60791823d1f2a08d8d79")
set(expected_trace_sha256 "415cae4c6463e9635eb9090f37c246ade51be671d2c61a1c6072b83f9b8df47f")
set(expected_call_sha256 "e8ec7919fdb21f43b99e8233e9f3909c44e25ad035af78454a1cb0ef4d65c79d")
set(expected_write_sha256 "110cf11d30a080719866522f41b7f5d502eee5daf4a11549f4523382b01e6296")
set(expected_state_sha256 "a655a1b5402a29bbc90ebc5725c2334ccbc4d7eeec01e086f2c6291031f96206")
set(expected_timeout_census_sha256 "b1cae240c84fa295db170ec70b8505a002e35c0533af9040e361e73e4e3b0a9b")
set(expected_timeout_state_sha256 "6754f4c8953ea846060581fd6d93dd3881cefbc6d02ef0d71cdf9fa225a463ce")
set(expected_completion_census_sha256 "d8229b8c87d857e700dcaae9c37a0b423c9b91b7897ebc78e0dacc934d4a7cbe")
set(expected_completion_trace_sha256 "6eb0b7b5396056b1d81e18aedba76c942c3924db740580a554305d3566a2f9b4")
set(expected_completion_call_sha256 "17f36bb87540ba504d86547f467e3814114185560850db8567e55c5992c285ef")
set(expected_completion_write_sha256 "2a570988d961d1f1d6e7806265c6d6e1835c60a1ff92307175b2d0638dee1ada")
set(expected_completion_state_sha256 "17ce4044f98355938081bc69a6a100ce5ce7933c3bd79d9fe74e1c6b3f584374")

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(state_output "${OUTPUT}.state.csv")
set(timeout_output "${OUTPUT}.timeout.csv")
set(timeout_state_output "${timeout_output}.state.csv")
set(completion_output "${OUTPUT}.completion.csv")
set(completion_trace_output "${completion_output}.trace.csv")
set(completion_call_output "${completion_output}.calls.csv")
set(completion_write_output "${completion_call_output}.writes.csv")
set(completion_state_output "${completion_output}.state.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${OUTPUT}"
        --work-ram-patch "1300:2055:05"
        --work-ram-patch "1300:2053:10"
        --work-ram-patch "1403:3851:90"
        --work-ram-patch "1403:39D1:02"
        --work-ram-patch "1403:3852:90"
        --work-ram-patch "1403:39D2:01"
        --observe-hook "05:6E0E"
        --observe-hook "05:6E16"
        --observe-hook "05:6E23"
        --observe-hook "05:7100"
        --observe-hook "05:7112"
        --observe-hook "05:715D"
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
        "BS-5 event recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 6"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 2"
    "Observe hook 3 entries: 3"
    "Observe hook 4 entries: 15"
    "Observe hook 5 entries: 30"
    "Observe hook 6 entries: 1"
    "Routine trace complete: 52 entries"
    "Routine call trace complete: 52 calls, 725 writes"
    "Work-RAM write trace complete: 1101 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-5 event output omitted '${expected_line}'.\n"
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
            "BS-5 ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}"
        )
    endif()
endforeach()

if(CONFIG STREQUAL "Release")
    execute_process(
        COMMAND
            "${APP}"
            --record-state-census "${timeout_output}"
            --work-ram-patch "1300:2055:05"
            --work-ram-patch "1300:2053:10"
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
            "BS-5 timeout recorder failed (${timeout_result}).\n"
            "${timeout_stdout}${timeout_stderr}"
        )
    endif()
    foreach(expected_line IN ITEMS
        "State census recording complete: 3600 frames"
        "Work-RAM patches applied: 2"
        "Pass-through hook 1 entries: 1"
        "Pass-through hook 2 entries: 240"
        "Work-RAM write trace complete: 1156 selected writes"
    )
        string(FIND "${timeout_stdout}" "${expected_line}" line_offset)
        if(line_offset EQUAL -1)
            message(
                FATAL_ERROR
                "BS-5 timeout output omitted '${expected_line}'.\n"
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
            "BS-5 timeout artifacts changed.\n"
            "Census: ${actual_timeout_census_sha256}\n"
            "State:  ${actual_timeout_state_sha256}"
        )
    endif()

    execute_process(
        COMMAND
            "${APP}"
            --record-state-census "${completion_output}"
            --work-ram-patch "1300:2055:05"
            --work-ram-patch "1300:2053:10"
            --work-ram-patch "1403:2D2E:00"
            --work-ram-patch "1403:2DB7:01"
            --work-ram-patch "1403:2DBA:01"
            --observe-hook "05:6E16"
            --pass-through-hook "04:44DB"
            --pass-through-hook "04:456C"
            --pass-through-hook "04:464F"
            --pass-through-hook "04:4DC4"
            --routine-trace "${completion_trace_output}"
            --routine-call-trace "${completion_call_output}"
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
            "BS-5 completion recorder failed (${completion_result}).\n"
            "${completion_stdout}${completion_stderr}"
        )
    endif()
    foreach(expected_line IN ITEMS
        "State census recording complete: 3600 frames"
        "Work-RAM patches applied: 5"
        "Pass-through hook 1 entries: 1"
        "Pass-through hook 2 entries: 1"
        "Pass-through hook 3 entries: 1"
        "Pass-through hook 4 entries: 422"
        "Observe hook 1 entries: 2"
        "Routine trace complete: 2 entries"
        "Routine call trace complete: 2 calls, 28 writes"
        "Work-RAM write trace complete: 2483 selected writes"
    )
        string(FIND "${completion_stdout}" "${expected_line}" line_offset)
        if(line_offset EQUAL -1)
            message(
                FATAL_ERROR
                "BS-5 completion output omitted '${expected_line}'.\n"
                "${completion_stdout}${completion_stderr}"
            )
        endif()
    endforeach()

    foreach(artifact IN ITEMS census trace call write state)
        if(artifact STREQUAL "census")
            set(artifact_path "${completion_output}")
        elseif(artifact STREQUAL "trace")
            set(artifact_path "${completion_trace_output}")
        elseif(artifact STREQUAL "call")
            set(artifact_path "${completion_call_output}")
        elseif(artifact STREQUAL "write")
            set(artifact_path "${completion_write_output}")
        else()
            set(artifact_path "${completion_state_output}")
        endif()
        file(SHA256 "${artifact_path}" actual_sha256)
        set(expected_sha256 "${expected_completion_${artifact}_sha256}")
        if(NOT actual_sha256 STREQUAL expected_sha256)
            message(
                FATAL_ERROR
                "BS-5 completion ${artifact} artifact changed.\n"
                "Expected: ${expected_sha256}\n"
                "Actual:   ${actual_sha256}"
            )
        endif()
    endforeach()

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
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-BonusStage5Semantics.ps1"
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
        "BS-5 analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
set(expected_semantics_lines
    "BS-5 objective-field and result semantics verified."
    "Objectives: a five-by-three field contains 15 three-contact targets."
    "Contacts: 100 stored units and command 21 per hit"
    "Completion: zero remaining targets select multiplier 30."
)
if(CONFIG STREQUAL "Release")
    list(
        APPEND expected_semantics_lines
        "Timeout: zero progress produces 240 glyph ticks"
        "All-clear: multiplier 30 builds 30,000 stored units"
    )
endif()
foreach(expected_line IN LISTS expected_semantics_lines)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-5 semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "BS-5 artifacts verified.\n${semantics_output}")
