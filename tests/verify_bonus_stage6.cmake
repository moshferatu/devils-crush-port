# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Debug" AND NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported BS-6 configuration: ${CONFIG}")
endif()

set(expected_census_sha256 "8fb656b939185a8570a583407e9fe9348247ca8bca83d55a4b233a96c2d3cf8f")
set(expected_trace_sha256 "a1c1783744ad5e6637bb497acfe059fbd5976c04baebc2d274f77e8bb6193835")
set(expected_call_sha256 "dd171c3e04a17e3ec70fc872e6983978f675e84b704023861780c2866c476b1a")
set(expected_write_sha256 "e3bb396c86224e9a77f49a96b8d4611d422daa653450ac44736eb7dde5fea4e5")
set(expected_state_sha256 "fa1e6427e1e49a35d34c2ef6981e45e296af6a5b96b8cf5e8cf94797ab6f61e5")
set(expected_timeout_census_sha256 "f44d8b962bb2ac3268d645c72f7e1499df1f325807d424a468c10a8585eacd88")
set(expected_timeout_state_sha256 "862a5993b75a904300f1ddbfca2db3b28dbd4ce2d73a0416ee0f2bf5aa695cf8")
set(expected_completion_census_sha256 "87ee42569c5d8124189eda37af4289c6f1f0493e70fa62afd31dea18549ef6ed")
set(expected_completion_trace_sha256 "821d3572ee8639dd896cca6233947d3af36ed37fe97d90b488df312b5bd7f46b")
set(expected_completion_call_sha256 "66449384c06f612916e81d6f57fb311c4888ef8e07283f3777c2d1a89cb78682")
set(expected_completion_write_sha256 "eae4437126cedd5ca13eff4af1c7fe526babf766b6eebc557e87ac9d056b8bf6")
set(expected_completion_state_sha256 "f54488e6eff8bec23af8dd38113b8cfb0058e1fdddd98f99e469c8fbf6c99d91")

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
        --work-ram-patch "1300:2055:06"
        --work-ram-patch "1300:2053:10"
        --work-ram-patch "1403:3834:90"
        --work-ram-patch "1403:3C34:01"
        --work-ram-patch "1403:3835:90"
        --work-ram-patch "1403:3C35:02"
        --observe-hook "05:71E3"
        --observe-hook "05:71EB"
        --observe-hook "05:71F8"
        --observe-hook "05:7228"
        --observe-hook "05:72A8"
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
        "BS-6 event recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 6"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 2"
    "Observe hook 3 entries: 2"
    "Observe hook 4 entries: 2"
    "Observe hook 5 entries: 2"
    "Routine trace complete: 9 entries"
    "Routine call trace complete: 9 calls, 1233 writes"
    "Work-RAM write trace complete: 1130 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-6 event output omitted '${expected_line}'.\n"
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
            "BS-6 ${artifact} artifact changed.\n"
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
            --work-ram-patch "1300:2055:06"
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
            "BS-6 timeout recorder failed (${timeout_result}).\n"
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
                "BS-6 timeout output omitted '${expected_line}'.\n"
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
            "BS-6 timeout artifacts changed.\n"
            "Census: ${actual_timeout_census_sha256}\n"
            "State:  ${actual_timeout_state_sha256}"
        )
    endif()

    execute_process(
        COMMAND
            "${APP}"
            --record-state-census "${completion_output}"
            --work-ram-patch "1300:2055:06"
            --work-ram-patch "1300:2053:10"
            --work-ram-patch "1403:2D2E:00"
            --work-ram-patch "1403:2DB7:01"
            --work-ram-patch "1403:2DBA:01"
            --observe-hook "05:71EB"
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
            "BS-6 completion recorder failed (${completion_result}).\n"
            "${completion_stdout}${completion_stderr}"
        )
    endif()
    foreach(expected_line IN ITEMS
        "State census recording complete: 3600 frames"
        "Work-RAM patches applied: 5"
        "Pass-through hook 1 entries: 1"
        "Pass-through hook 2 entries: 1"
        "Pass-through hook 3 entries: 1"
        "Pass-through hook 4 entries: 392"
        "Observe hook 1 entries: 2"
        "Routine trace complete: 2 entries"
        "Routine call trace complete: 2 calls, 15 writes"
        "Work-RAM write trace complete: 2239 selected writes"
    )
        string(FIND "${completion_stdout}" "${expected_line}" line_offset)
        if(line_offset EQUAL -1)
            message(
                FATAL_ERROR
                "BS-6 completion output omitted '${expected_line}'.\n"
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
                "BS-6 completion ${artifact} artifact changed.\n"
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
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-BonusStage6Semantics.ps1"
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
        "BS-6 analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
set(expected_semantics_lines
    "BS-6 objective-pair and result semantics verified."
    "Objectives: two indexed ten-hit actors share remaining count 2D2E."
    "Contacts: 50 stored units and command 2E per hit"
    "Completion: zero remaining objectives select multiplier 15."
)
if(CONFIG STREQUAL "Release")
    list(
        APPEND expected_semantics_lines
        "Timeout: zero progress produces 240 glyph ticks"
        "All-clear: multiplier 15 builds 15,000 stored units"
    )
endif()
foreach(expected_line IN LISTS expected_semantics_lines)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-6 semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "BS-6 artifacts verified.\n${semantics_output}")
