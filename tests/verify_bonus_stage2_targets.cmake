# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Debug" AND NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported BS-2-target configuration: ${CONFIG}")
endif()

set(
    expected_census_sha256
    "0df2459325e5aaac657e588346b2ca5125d59f56e95f49a881e0eebf88e2996e"
)
set(
    expected_trace_sha256
    "8a941d2af5bebdbf096c775b4dae77329d7cbb9ddcbff5cbb38b22e66a108420"
)
set(
    expected_call_sha256
    "f4b01ddc19c7289b390d1f12850eef4b620a5cb308fe1f2ae1d3d329ddba2913"
)
set(
    expected_write_sha256
    "7a1300ffb2144e1ddd92322ffed3ad1faaafcafb8c050ac69f950b95e41a337e"
)
set(
    expected_state_sha256
    "138e1f0d90ba8107e655740306deac70f02f69b8bb114d2c9dbf16a9fb8bb882"
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
        --work-ram-patch "1300:2055:02"
        --work-ram-patch "1300:2053:10"
        --work-ram-patch "1404:3F34:FB"
        --work-ram-patch "1404:3FB4:4E"
        --work-ram-patch "1404:3A34:01"
        --work-ram-patch "1404:3B34:05"
        --work-ram-patch "1404:3053:C2"
        --work-ram-patch "1404:30D3:84"
        --work-ram-patch "1404:31D3:F8"
        --work-ram-patch "1404:3253:00"
        --work-ram-patch "1404:3353:30"
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
        --observe-hook "04:4EFB"
        --observe-hook "04:4FBA"
        --observe-hook "04:5079"
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
        "BS-2 target recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 21"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 1"
    "Observe hook 3 entries: 1"
    "Observe hook 4 entries: 1"
    "Routine trace complete: 4 entries"
    "Routine call trace complete: 4 calls, 3269 writes"
    "Work-RAM write trace complete: 1087 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-2 target output omitted '${expected_line}'.\n"
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
            "BS-2 target ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(STATUS "BS-2 target ${artifact} verified: ${actual_sha256}")
endforeach()

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-BonusStage2TargetSemantics.ps1"
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
        "BS-2 target analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "BS-2 emitter and target semantics verified."
    "Controller: slot 2C initializes objective counter 2D2E to 28."
    "Emitter: slot 34 reloads to 48 ticks"
    "Target contact: slot 53 awards 50 stored units"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-2 target semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
