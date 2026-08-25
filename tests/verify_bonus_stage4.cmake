# SPDX-License-Identifier: GPL-3.0-or-later
# DC_NATIVE_ADMISSION 05:6D27 7719 7728

# The player-session-6 replay is the natural, unpatched witness for this
# arm: 7,719 live orbit calls replace while the nine controller-joining calls
# fall back before any staged write.

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Debug" AND NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported BS-4 configuration: ${CONFIG}")
endif()

set(expected_objective_census_sha256 "f24d1daf2350537b99c66bad84cc6336ba264e1fcd6a6adc7c2c2d6e6641e3c8")
set(expected_objective_trace_sha256 "893c5aef86279cfc3bfa2e90d363cd0b0bfa3382bc327e584162774aba0789db")
set(expected_objective_call_sha256 "153166a0451c814c0df1794fa3bfccc5c4a6f43f02813f1f8066e0879b269909")
set(expected_objective_write_sha256 "66bfa0fa1be4d39b0964ae2955af3575ceb8057596d17da1aa371439b5091282")
set(expected_objective_state_sha256 "e1f35b49faa3d8fd72392537bde853fafe6c2fc02d3ce5b90bae86a75cd5cd22")
set(expected_emitter_census_sha256 "ff96f0daa8da6d483545a12ab07584219ff73199940d029bc01178843d32ea22")
set(expected_emitter_trace_sha256 "54e9d58d21480698dd8d7a780cc87bbbb4f5a7e2e628390c5ca6c09be6a69eaf")
set(expected_emitter_call_sha256 "9a879e3093123c0f33c6d5c892b2b2e872a3b7d05ebe6600c8105525ead4aefa")
set(expected_emitter_write_sha256 "8d0363c9b9380e692de2f8c475b23d1fe2be9ab31cddb830d1f2e2bf093dadce")
set(expected_emitter_state_sha256 "a2fbda79989bebe902cc1e8ff0eed654b2191eb744d3d95b4e8bce499f220abc")
set(expected_target_census_sha256 "9c0de69068a8efb623ff911e99233e4b9ff068e07c51bb04ead73bc4c8432b7a")
set(expected_target_trace_sha256 "48f668c5095527aca528e4afd2d2d4c4fb50040f7d1cd6ccce74fe25993ab63d")
set(expected_target_call_sha256 "4240c99d69453af98769a408ca6db9121ebb9351b1d972e3cacce14ae8862a83")
set(expected_target_write_sha256 "88a14bdaea0e363568d00e2b34cd1c1bc5debac200718d1a27f7d6d1c73f7832")
set(expected_target_state_sha256 "921e07a473fb853e8a708e2712847de8ed8efb66b0fd5b1af15633713974ae3c")
set(expected_timeout_census_sha256 "50f207dcc16f29847d78a93e4926d1c4025a8403c84a2aea94fd320b2d44c664")
set(expected_timeout_state_sha256 "d9756464382d2236259b6be4f2b53905170c8c5816c65f31e4c3a957c7ee05cc")
set(expected_completion_census_sha256 "ff00c9b2136fe694a4cab3650a484d0a9e0ec4e40312c92a6f1eb042eb85956e")
set(expected_completion_trace_sha256 "ff79acd76959654cbff2003d97bb382cc8eefd5cac2e974b5f6eb7bb7b7f5f5f")
set(expected_completion_call_sha256 "018700eab05d401487374b3fc373c9f03efbd02c9e298c8b146b4346d69e3a4b")
set(expected_completion_write_sha256 "10250677110b4b716e096bb0e451af93cbf8b1018690af39ebf32d0b39040f3e")
set(expected_completion_state_sha256 "60a587c13205689ad83ffcc64ab96ecb80fb621b322af8e457e841237d07855b")

set(objective_trace_output "${OUTPUT}.trace.csv")
set(objective_call_output "${OUTPUT}.calls.csv")
set(objective_write_output "${objective_call_output}.writes.csv")
set(objective_state_output "${OUTPUT}.state.csv")
set(emitter_output "${OUTPUT}.emitter.csv")
set(emitter_trace_output "${emitter_output}.trace.csv")
set(emitter_call_output "${emitter_output}.calls.csv")
set(emitter_write_output "${emitter_call_output}.writes.csv")
set(emitter_state_output "${emitter_output}.state.csv")
set(target_output "${OUTPUT}.target.csv")
set(target_trace_output "${target_output}.trace.csv")
set(target_call_output "${target_output}.calls.csv")
set(target_write_output "${target_call_output}.writes.csv")
set(target_state_output "${target_output}.state.csv")
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
        --work-ram-patch "1300:2055:04"
        --work-ram-patch "1300:2053:10"
        --work-ram-patch "1403:3834:90"
        --work-ram-patch "1403:3C34:01"
        --work-ram-patch "1403:3852:90"
        --work-ram-patch "1403:3C52:02"
        --observe-hook "05:6A10"
        --observe-hook "05:6A1E"
        --observe-hook "05:6A31"
        --observe-hook "05:6A46"
        --routine-trace "${objective_trace_output}"
        --routine-call-trace "${objective_call_output}"
        --work-ram-write-range "2D00-2DFF"
        --work-ram-write-trace "${objective_state_output}"
        --rom "${ROM}"
    RESULT_VARIABLE objective_result
    OUTPUT_VARIABLE objective_stdout
    ERROR_VARIABLE objective_stderr
)
if(NOT objective_result EQUAL 0)
    message(
        FATAL_ERROR
        "BS-4 objective recorder failed (${objective_result}).\n"
        "${objective_stdout}${objective_stderr}"
    )
endif()
foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 6"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 2"
    "Observe hook 3 entries: 3"
    "Observe hook 4 entries: 6"
    "Routine trace complete: 12 entries"
    "Routine call trace complete: 12 calls, 870 writes"
    "Work-RAM write trace complete: 1114 selected writes"
)
    string(FIND "${objective_stdout}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-4 objective output omitted '${expected_line}'.\n"
            "${objective_stdout}${objective_stderr}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census trace call write state)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "trace")
        set(artifact_path "${objective_trace_output}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${objective_call_output}")
    elseif(artifact STREQUAL "write")
        set(artifact_path "${objective_write_output}")
    else()
        set(artifact_path "${objective_state_output}")
    endif()
    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_objective_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "BS-4 objective ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}"
        )
    endif()
endforeach()

execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${emitter_output}"
        --work-ram-patch "1300:2055:04"
        --work-ram-patch "1300:2053:10"
        --work-ram-patch "1403:3F35:CC"
        --work-ram-patch "1403:3FB5:6A"
        --work-ram-patch "1403:3CB5:00"
        --observe-hook "05:6ACC"
        --observe-hook "05:6B16"
        --observe-hook "05:6B7E"
        --routine-trace "${emitter_trace_output}"
        --routine-call-trace "${emitter_call_output}"
        --work-ram-write-range "2D00-2DFF"
        --work-ram-write-trace "${emitter_state_output}"
        --rom "${ROM}"
    RESULT_VARIABLE emitter_result
    OUTPUT_VARIABLE emitter_stdout
    ERROR_VARIABLE emitter_stderr
)
if(NOT emitter_result EQUAL 0)
    message(
        FATAL_ERROR
        "BS-4 emitter recorder failed (${emitter_result}).\n"
        "${emitter_stdout}${emitter_stderr}"
    )
endif()
foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 5"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 1"
    "Observe hook 3 entries: 1"
    "Routine trace complete: 3 entries"
    "Routine call trace complete: 3 calls, 288 writes"
    "Work-RAM write trace complete: 1158 selected writes"
)
    string(FIND "${emitter_stdout}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-4 emitter output omitted '${expected_line}'.\n"
            "${emitter_stdout}${emitter_stderr}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census trace call write state)
    if(artifact STREQUAL "census")
        set(artifact_path "${emitter_output}")
    elseif(artifact STREQUAL "trace")
        set(artifact_path "${emitter_trace_output}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${emitter_call_output}")
    elseif(artifact STREQUAL "write")
        set(artifact_path "${emitter_write_output}")
    else()
        set(artifact_path "${emitter_state_output}")
    endif()
    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_emitter_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "BS-4 emitter ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}"
        )
    endif()
endforeach()

execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${target_output}"
        --work-ram-patch "1300:2055:04"
        --work-ram-patch "1300:2053:10"
        --work-ram-patch "1403:3066:C2"
        --work-ram-patch "1403:3F66:06"
        --work-ram-patch "1403:3FE6:6C"
        --work-ram-patch "1403:3866:90"
        --work-ram-patch "1403:3166:60"
        --work-ram-patch "1403:31E6:00"
        --work-ram-patch "1403:3266:48"
        --work-ram-patch "1403:32E6:00"
        --observe-hook "05:6C06"
        --routine-trace "${target_trace_output}"
        --routine-call-trace "${target_call_output}"
        --work-ram-write-range "2D00-2DFF"
        --work-ram-write-trace "${target_state_output}"
        --rom "${ROM}"
    RESULT_VARIABLE target_result
    OUTPUT_VARIABLE target_stdout
    ERROR_VARIABLE target_stderr
)
if(NOT target_result EQUAL 0)
    message(
        FATAL_ERROR
        "BS-4 target recorder failed (${target_result}).\n"
        "${target_stdout}${target_stderr}"
    )
endif()
foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 10"
    "Observe hook 1 entries: 1"
    "Routine trace complete: 1 entries"
    "Routine call trace complete: 1 calls, 76 writes"
    "Work-RAM write trace complete: 1087 selected writes"
)
    string(FIND "${target_stdout}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-4 target output omitted '${expected_line}'.\n"
            "${target_stdout}${target_stderr}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census trace call write state)
    if(artifact STREQUAL "census")
        set(artifact_path "${target_output}")
    elseif(artifact STREQUAL "trace")
        set(artifact_path "${target_trace_output}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${target_call_output}")
    elseif(artifact STREQUAL "write")
        set(artifact_path "${target_write_output}")
    else()
        set(artifact_path "${target_state_output}")
    endif()
    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_target_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "BS-4 target ${artifact} artifact changed.\n"
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
            --work-ram-patch "1300:2055:04"
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
            "BS-4 timeout recorder failed (${timeout_result}).\n"
            "${timeout_stdout}${timeout_stderr}"
        )
    endif()
    foreach(expected_line IN ITEMS
        "State census recording complete: 3600 frames"
        "Work-RAM patches applied: 2"
        "Pass-through hook 1 entries: 1"
        "Pass-through hook 2 entries: 240"
        "Work-RAM write trace complete: 1158 selected writes"
    )
        string(FIND "${timeout_stdout}" "${expected_line}" line_offset)
        if(line_offset EQUAL -1)
            message(
                FATAL_ERROR
                "BS-4 timeout output omitted '${expected_line}'.\n"
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
            "BS-4 timeout artifacts changed.\n"
            "Census: ${actual_timeout_census_sha256}\n"
            "State:  ${actual_timeout_state_sha256}"
        )
    endif()

    execute_process(
        COMMAND
            "${APP}"
            --record-state-census "${completion_output}"
            --work-ram-patch "1300:2055:04"
            --work-ram-patch "1300:2053:10"
            --work-ram-patch "1403:2D2F:00"
            --work-ram-patch "1403:2D30:00"
            --work-ram-patch "1403:2D31:00"
            --work-ram-patch "1403:2DB7:01"
            --work-ram-patch "1403:2DBA:01"
            --observe-hook "05:6A1E"
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
            "BS-4 completion recorder failed (${completion_result}).\n"
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
                "BS-4 completion output omitted '${expected_line}'.\n"
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
                "BS-4 completion ${artifact} artifact changed.\n"
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
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-BonusStage4Semantics.ps1"
        -RomPath "${ROM}"
        -ObjectiveCallPath "${objective_call_output}"
        -ObjectiveWritePath "${objective_write_output}"
        -ObjectiveStatePath "${objective_state_output}"
        -EmitterCallPath "${emitter_call_output}"
        -EmitterWritePath "${emitter_write_output}"
        -TargetCallPath "${target_call_output}"
        -TargetWritePath "${target_write_output}"
        ${result_analysis_args}
    RESULT_VARIABLE semantics_result
    OUTPUT_VARIABLE semantics_output
    ERROR_VARIABLE semantics_error
)
if(NOT semantics_result EQUAL 0)
    message(
        FATAL_ERROR
        "BS-4 analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
set(expected_semantics_lines
    "BS-4 objective, linked-target, and result semantics verified."
    "Objectives: three eight-hit records use flags 2D2F-2D31 and command 2E."
    "Linked targets: paired sources allocate type C2 actors"
    "Completion: clearing all three objective bytes selects multiplier 20."
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
            "BS-4 semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "BS-4 artifacts verified.\n${semantics_output}")
