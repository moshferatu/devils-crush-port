# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Debug" AND NOT CONFIG STREQUAL "Release")
    message(
        FATAL_ERROR
        "Unsupported remaining-normal-RNG configuration: ${CONFIG}"
    )
endif()

set(
    expected_census_sha256
    "17b63292db225e089e87b0c8216ef49414a300dcecb71ea73db51fd25b4c7790"
)
set(
    expected_trace_sha256
    "3352122bad7e6cebb1040d5a5299787a8cd8df98fc4808ef920c5b2ea5ad73fa"
)
set(
    expected_call_sha256
    "c2924c92ee7b63443c6be2a13ed11cbc800fd4f761dc779a411bc4dd13c39d00"
)
set(
    expected_write_sha256
    "9f0eb002eb61581bef1c9465c4f12782c3ce0716d60c7aadad330cc613cdc3f9"
)
set(
    expected_state_sha256
    "88efd2fa575124ba3fd9167266d1fd9026547583c9487645869cfb96c45fa55d"
)

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(state_output "${OUTPUT}.state.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${OUTPUT}"
        --work-ram-patch "1084:2254:01"
        --work-ram-patch "1224:2254:00"
        --work-ram-patch "1200:334D:2C"
        --work-ram-patch "1200:33CD:01"
        --work-ram-patch "1200:3F4D:65"
        --work-ram-patch "1200:3FCD:61"
        --work-ram-patch "1200:3F38:21"
        --work-ram-patch "1200:3FB8:78"
        --observe-hook "02:5124"
        --observe-hook "02:5148"
        --observe-hook "03:6165"
        --observe-hook "03:6177"
        --observe-hook "03:7821"
        --observe-hook "03:7834"
        --observe-hook "00:E8B3"
        --routine-trace "${trace_output}"
        --routine-call-trace "${call_output}"
        --routine-replay "03:7821@1"
        --routine-replay "00:E8B3@1"
        --work-ram-write-range "222A-222B"
        --work-ram-write-range "2254-2254"
        --work-ram-write-range "2D21-2D21"
        --work-ram-write-range "2D2D-2D2D"
        --work-ram-write-range "3000-3000"
        --work-ram-write-range "3038-3038"
        --work-ram-write-range "304D-304D"
        --work-ram-write-range "3080-3080"
        --work-ram-write-range "334D-334D"
        --work-ram-write-range "33CD-33CD"
        --work-ram-write-range "3400-3400"
        --work-ram-write-range "3480-3480"
        --work-ram-write-range "3500-3500"
        --work-ram-write-range "3580-3580"
        --work-ram-write-range "3600-3600"
        --work-ram-write-range "3680-3680"
        --work-ram-write-range "380E-380E"
        --work-ram-write-range "3880-3880"
        --work-ram-write-range "3C80-3C80"
        --work-ram-write-range "3D8E-3D8E"
        --work-ram-write-range "3F0E-3F0E"
        --work-ram-write-range "3F8E-3F8E"
        --work-ram-write-range "3F38-3F38"
        --work-ram-write-range "3FB8-3FB8"
        --work-ram-write-range "3F4D-3F4D"
        --work-ram-write-range "3FCD-3FCD"
        --work-ram-write-trace "${state_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Remaining normal-table RNG recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 8"
    "Observe hook 1 entries: 21"
    "Observe hook 2 entries: 1"
    "Observe hook 3 entries: 1"
    "Observe hook 4 entries: 1"
    "Observe hook 5 entries: 1"
    "Observe hook 6 entries: 1"
    "Observe hook 7 entries: 3"
    "Routine trace complete: 29 entries"
    "Routine call trace complete: 28 calls, 1195 writes"
    "Work-RAM write trace complete: 22702 selected writes"
    "Routine replay 03:7821@1 verified: frame 1215, 357 body cycles"
    "Routine replay 00:E8B3@1 verified: frame 1084, 282 body cycles"
    "Routine replay complete: 2 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Remaining normal-table RNG output omitted '${expected_line}'.\n"
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
            "Remaining normal-table RNG ${artifact} artifact changed.\n"
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
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-RemainingNormalRngSemantics.ps1"
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
        "Remaining normal-table RNG analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Remaining normal-table RNG semantics verified."
    "Consumer 02:5148: result E7 maps to selector 67"
    "Consumer 03:7834: even result 58 selects vector 07"
    "Consumer 03:6177: even result 70 selects vector 17"
    "E8B3: all three routed calls preserve the exact eight-write recurrence."
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Remaining normal-table RNG semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()

message(
    STATUS
    "Remaining normal-table RNG artifacts verified: "
    "${actual_census_sha256}, ${actual_trace_sha256}, "
    "${actual_call_sha256}, ${actual_write_sha256}, ${actual_state_sha256}\n"
    "${semantics_output}"
)
