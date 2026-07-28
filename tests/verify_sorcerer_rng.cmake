# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(CONFIG STREQUAL "Debug")
    set(
        expected_census_sha256
        "289db077eed3dcf07579096d7a3f0bfefce5b58ea4cc75d78620a15d22780a90"
    )
elseif(CONFIG STREQUAL "Release")
    set(
        expected_census_sha256
        "fe88d7df827906e0a9f90ec5babd0c53d7d395089f8ab5f40ed4634ca1f98543"
    )
else()
    message(FATAL_ERROR "Unsupported Sorcerer-RNG configuration: ${CONFIG}")
endif()

set(
    expected_trace_sha256
    "06e81bc2a61561c00035ba8a053144598139e6995eb7daa13c67088302d3af12"
)
set(
    expected_call_sha256
    "2cce93e60f805f67d8c0ae93057415eac77d1699bbf28a5e653f79d610a02a87"
)
set(
    expected_write_sha256
    "745c72bace638162f17788180faaee01938b88297e3181cc2e1200cca2b04166"
)
set(
    expected_state_sha256
    "11b203ce3459e654d7b1c2b2d2c853455adc9fa9093786b80867f6698edfcd59"
)

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(state_output "${OUTPUT}.state.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:2D0F:00"
        --observe-hook "02:57AD"
        --observe-hook "02:57BA"
        --observe-hook "02:57C2"
        --observe-hook "00:E8B3"
        --routine-trace "${trace_output}"
        --routine-call-trace "${call_output}"
        --routine-replay "02:57AD@259"
        --routine-replay "02:57BA@1"
        --routine-replay "02:57C2@1"
        --work-ram-write-range "222A-222B"
        --work-ram-write-range "2CFA-2CFA"
        --work-ram-write-range "2D00-2D01"
        --work-ram-write-range "2D04-2D05"
        --work-ram-write-range "2D0B-2D10"
        --work-ram-write-range "2D40-2D40"
        --work-ram-write-range "3016-301E"
        --work-ram-write-range "3096-309E"
        --work-ram-write-range "3916-391E"
        --work-ram-write-range "3996-399E"
        --work-ram-write-range "3A16-3A1E"
        --work-ram-write-range "3A96-3A9E"
        --work-ram-write-range "3B16-3B1E"
        --work-ram-write-range "3B96-3B9E"
        --work-ram-write-range "3C16-3C1E"
        --work-ram-write-range "3C96-3C9E"
        --work-ram-write-range "3F16-3F1E"
        --work-ram-write-range "3F96-3F9E"
        --work-ram-write-trace "${state_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Sorcerer RNG recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 1"
    "Observe hook 1 entries: 277"
    "Observe hook 2 entries: 18"
    "Observe hook 3 entries: 18"
    "Observe hook 4 entries: 58"
    "Routine trace complete: 371 entries"
    "Routine call trace complete: 371 calls, 1764 writes"
    "Work-RAM write trace complete: 31028 selected writes"
    "Routine replay 02:57AD@259 verified: frame 1200, 363 body cycles"
    "Routine replay 02:57BA@1 verified: frame 1328, 636 body cycles"
    "Routine replay 02:57C2@1 verified: frame 1328, 600 body cycles"
    "Routine replay complete: 3 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Sorcerer RNG output omitted '${expected_line}'.\n"
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
    set("actual_${artifact}_sha256" "${actual_sha256}")
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "Sorcerer RNG ${artifact} artifact changed.\n"
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
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-SorcererRngSemantics.ps1"
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
        "Sorcerer RNG analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Sorcerer RNG semantics verified."
    "Zero-live route: controller 02:57AD starts its 128-tick delay"
    "RNG consumer 02:57C2: 18 events select collision values 28/29."
    "Cadence: first selector at frame 1328"
    "Normalized samples: 02:57AD@259, 02:57BA@1, and 02:57C2@1"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Sorcerer RNG semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(
    STATUS
    "Sorcerer RNG artifacts verified: "
    "${actual_census_sha256}, ${actual_trace_sha256}, "
    "${actual_call_sha256}, ${actual_write_sha256}, ${actual_state_sha256}\n"
    "${semantics_output}"
)
