# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Debug" AND NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported BS-3 configuration: ${CONFIG}")
endif()

set(
    expected_census_sha256
    "adb89f821c0ec3938ebe39bd9d16691fdd4e6e18d8ca2598393d620bc3621318"
)
set(
    expected_trace_sha256
    "effe95bec96409c86167ea48f5c7a6a6dfdb2d0dec3c0771d2cab40d7855627b"
)
set(
    expected_call_sha256
    "17c47027e81adc9efff127772f32999bcf042c68dd7e45c3eb6476b5ee3fa093"
)
set(
    expected_write_sha256
    "396ccad5b52dc6f4e904f0d10724b28a19cad26199539fa16ee4bf3f3449b4c9"
)
set(
    expected_state_sha256
    "40d6fcfcd1a2386dff9b36d55720934e459d5b1bfe3437752ca272d7932efe82"
)
set(
    expected_timeout_census_sha256
    "00a662c5345b7f947a80a6ba76044f6072ee8cc5545819240ae799d27502a7a7"
)
set(
    expected_timeout_state_sha256
    "bd7aadba6984c307413a05fb293b928714387512701c20d0bc556dd6bc64f615"
)
set(
    expected_completion_census_sha256
    "3bdb329aa8db7790604111fea6d649ccad1033dde620f662aa07ddc1d6e083b1"
)
set(
    expected_completion_trace_sha256
    "3110d94dbcc0b7ebd67a8b5ea392d475c1b70ef3fd7eb87cd732aa6abd40a7a7"
)
set(
    expected_completion_call_sha256
    "b589284f073d2c0a9a815100e85a187c93e51cbe4e9f81ca43cf2bbb6b0da239"
)
set(
    expected_completion_write_sha256
    "d69e0c2cda2c6f225f49b5913dcecbf40aa90d1b5429e2570783fc500d528862"
)
set(
    expected_completion_state_sha256
    "7114235e0a961b91c4b6fc9733f6d314ef52a658c622120c6b703f0981028d8b"
)

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
        --work-ram-patch "1300:2055:03"
        --work-ram-patch "1300:2053:10"
        --work-ram-patch "1403:3F58:E7"
        --work-ram-patch "1403:3FD8:54"
        --work-ram-patch "1403:3858:90"
        --work-ram-patch "1403:3F51:26"
        --work-ram-patch "1403:3FD1:56"
        --work-ram-patch "1403:3851:90"
        --work-ram-patch "1403:3C51:01"
        --work-ram-patch "1403:3F52:A6"
        --work-ram-patch "1403:3FD2:56"
        --observe-hook "04:53CD"
        --observe-hook "04:53E4"
        --observe-hook "04:5424"
        --observe-hook "04:54E7"
        --observe-hook "04:55DF"
        --observe-hook "04:5626"
        --observe-hook "04:56A6"
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
        "BS-3 event recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 11"
    "Observe hook 1 entries: 3"
    "Observe hook 2 entries: 6"
    "Observe hook 3 entries: 3"
    "Observe hook 4 entries: 1"
    "Observe hook 5 entries: 3"
    "Observe hook 6 entries: 1"
    "Observe hook 7 entries: 1"
    "Routine trace complete: 18 entries"
    "Routine call trace complete: 18 calls, 1003 writes"
    "Work-RAM write trace complete: 1120 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-3 event output omitted '${expected_line}'.\n"
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
            "BS-3 ${artifact} artifact changed.\n"
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
            --work-ram-patch "1300:2055:03"
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
            "BS-3 timeout recorder failed (${timeout_result}).\n"
            "${timeout_stdout}${timeout_stderr}"
        )
    endif()
    foreach(expected_line IN ITEMS
        "State census recording complete: 3600 frames"
        "Work-RAM patches applied: 2"
        "Pass-through hook 1 entries: 1"
        "Pass-through hook 2 entries: 240"
        "Work-RAM write trace complete: 1164 selected writes"
    )
        string(FIND "${timeout_stdout}" "${expected_line}" line_offset)
        if(line_offset EQUAL -1)
            message(
                FATAL_ERROR
                "BS-3 timeout output omitted '${expected_line}'.\n"
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
            "BS-3 timeout artifacts changed.\n"
            "Census: ${actual_timeout_census_sha256}\n"
            "State:  ${actual_timeout_state_sha256}"
        )
    endif()

    execute_process(
        COMMAND
            "${APP}"
            --record-state-census "${completion_output}"
            --work-ram-patch "1300:2055:03"
            --work-ram-patch "1300:2053:10"
            --work-ram-patch "1403:2D2F:00"
            --work-ram-patch "1403:2D30:00"
            --work-ram-patch "1403:2D31:00"
            --work-ram-patch "1403:2DB7:01"
            --work-ram-patch "1403:2DBA:01"
            --observe-hook "04:53BA"
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
            "BS-3 completion recorder failed (${completion_result}).\n"
            "${completion_stdout}${completion_stderr}"
        )
    endif()
    foreach(expected_line IN ITEMS
        "State census recording complete: 3600 frames"
        "Work-RAM patches applied: 7"
        "Pass-through hook 1 entries: 1"
        "Pass-through hook 2 entries: 1"
        "Pass-through hook 3 entries: 1"
        "Pass-through hook 4 entries: 402"
        "Observe hook 1 entries: 2"
        "Routine trace complete: 2 entries"
        "Routine call trace complete: 2 calls, 15 writes"
        "Work-RAM write trace complete: 2328 selected writes"
    )
        string(FIND "${completion_stdout}" "${expected_line}" line_offset)
        if(line_offset EQUAL -1)
            message(
                FATAL_ERROR
                "BS-3 completion output omitted '${expected_line}'.\n"
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
                "BS-3 completion ${artifact} artifact changed.\n"
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
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-BonusStage3Semantics.ps1"
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
        "BS-3 analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
set(expected_semantics_lines
    "BS-3 emitter, objective, and result semantics verified."
    "Emitters: three six-target lanes maintain live counts 2D28-2D2A."
    "Contacts: emitted targets award 100 stored units"
    "Completion: objective bytes 2D2F-2D31 select multiplier 20"
)
if(CONFIG STREQUAL "Release")
    list(
        APPEND expected_semantics_lines
        "Timeout: zero progress produces 240 glyph ticks"
        "All-clear: multiplier 20 builds 20,000 stored units"
    )
endif()
foreach(expected_line IN LISTS expected_semantics_lines)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-3 semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "BS-3 artifacts verified.\n${semantics_output}")
