# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

set(
    expected_census_sha256
    "e660ac5620fefbc65147496ec757893e9e599b60ad7635dcb0ee69b1f476dec4"
)
set(
    expected_trace_sha256
    "4be82e8645661eaa9fdb132f932181b7cc3356e94cc03a7011a2aafcfeed2139"
)
set(
    expected_call_sha256
    "fa8e1cfeb714e0519f5b142c8fa2f4578780ca827e7877f47da0ccebd3a7cbad"
)
set(
    expected_write_sha256
    "f20e5d5b610a6be765bd3225be2c73ce8517313bf5f7ee4126428646421070bc"
)
set(
    expected_state_sha256
    "bb5d4c02783e742240c88615b971669f0f64e87715734d358596a45f1f2df315"
)

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(state_output "${OUTPUT}.state.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --work-ram-patch
        "1300:2055:01"
        --work-ram-patch
        "1300:2053:10"
        --observe-hook
        "04:44C6"
        --observe-hook
        "04:44CE"
        --observe-hook
        "04:47B5"
        --observe-hook
        "04:47C7"
        --observe-hook
        "04:47D5"
        --observe-hook
        "04:49D4"
        --observe-hook
        "04:49DF"
        --observe-hook
        "04:4DC4"
        --pass-through-hook
        "04:456C"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --work-ram-write-range
        "2D00-2DFF"
        --work-ram-write-trace
        "${state_output}"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "BS-1 target-family recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 2"
    "Pass-through hook 1 entries: 1"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 2"
    "Observe hook 3 entries: 5"
    "Observe hook 4 entries: 5"
    "Observe hook 5 entries: 5"
    "Observe hook 6 entries: 5"
    "Observe hook 7 entries: 10"
    "Observe hook 8 entries: 240"
    "Routine trace complete: 273 entries"
    "Routine call trace complete: 273 calls, 3894 writes"
    "Work-RAM write trace complete: 1156 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-1 target-family output omitted '${expected_line}'.\n"
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
            "BS-1 target-family ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(
        STATUS
        "BS-1 target-family ${artifact} verified: ${actual_sha256}"
    )
endforeach()

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-BonusStage1Semantics.ps1"
        -RomPath
        "${ROM}"
        -CallPath
        "${call_output}"
        -WritePath
        "${write_output}"
        -StatePath
        "${state_output}"
    RESULT_VARIABLE semantics_result
    OUTPUT_VARIABLE semantics_output
    ERROR_VARIABLE semantics_error
)
if(NOT semantics_result EQUAL 0)
    message(
        FATAL_ERROR
        "BS-1 target-family analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "BS-1 target-family semantics verified."
    "Objective row: five six-hit targets"
    "Target scoring: 50 stored units per nonterminal hit and 200 on the"
    "Support row: five repeatable 10-unit actors"
    "Natural forced-entry route: no target contacts"
    "Result scheduler: 240 calls over frames 1529-1768"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-1 target-family semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
