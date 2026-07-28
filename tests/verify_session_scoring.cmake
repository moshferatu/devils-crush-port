# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(CONFIG STREQUAL "Debug")
    set(
        expected_census_sha256
        "a43f5e6e9ee156aa9e947c8e0025bde17085032ee1e7f04af69b338a660f10c8"
    )
elseif(CONFIG STREQUAL "Release")
    set(
        expected_census_sha256
        "9e928058264849ba617b7c36697cb24779b28a10072c6c92c6b1786d72f284a2"
    )
else()
    message(FATAL_ERROR "Unsupported session/scoring configuration: ${CONFIG}")
endif()

set(
    expected_work_ram_sha256
    "229ffbff969db6067e2adb10b7f05523f048c7be1539fc2188499ca62f18ab5b"
)
set(
    expected_trace_sha256
    "bd1bf1250380a6a7c7d87c92d5884e0190584619f1fed96ebd412413c96d8d47"
)
set(
    expected_call_sha256
    "31f9af5449deeef5395aec78f916dbe54b42d72436c98fd36413c8550173e25a"
)
set(
    expected_write_sha256
    "19a322d5efee8660e00e93f759c3b5ff308de8f2727e85becc13ed7b915bf567"
)

set(work_ram_output "${OUTPUT}.wram.bin")
set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --canonical-work-ram
        "${work_ram_output}"
        --work-ram-patch
        "3000:3380:03"
        --work-ram-patch
        "3000:3300:00"
        --observe-hook
        "00:F55C"
        --observe-hook
        "01:C786"
        --observe-hook
        "01:C7B0"
        --observe-hook
        "01:C7BF"
        --observe-hook
        "01:C7E5"
        --observe-hook
        "01:C7EA"
        --observe-hook
        "01:C80A"
        --observe-hook
        "01:C812"
        --observe-hook
        "02:4003"
        --observe-hook
        "09:7266"
        --observe-hook
        "09:72FD"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Session/scoring recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 2"
    "Observe hook 1 entries: 33"
    "Observe hook 2 entries: 1"
    "Observe hook 3 entries: 1"
    "Observe hook 4 entries: 1"
    "Observe hook 5 entries: 1"
    "Observe hook 6 entries: 1"
    "Observe hook 7 entries: 1"
    "Observe hook 8 entries: 1"
    "Observe hook 9 entries: 1"
    "Observe hook 10 entries: 29"
    "Observe hook 11 entries: 332"
    "Routine trace complete: 402 entries"
    "Routine call trace complete: 402 calls, 27769 writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Session/scoring output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census work_ram trace call write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "work_ram")
        set(artifact_path "${work_ram_output}")
    elseif(artifact STREQUAL "trace")
        set(artifact_path "${trace_output}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    else()
        set(artifact_path "${write_output}")
    endif()

    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "Session/scoring ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(STATUS "Session/scoring ${artifact} verified: ${actual_sha256}")
endforeach()

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-SessionScoringRoute.ps1"
        -RomPath
        "${ROM}"
        -RecordingPath
        "${OUTPUT}"
        -WorkRamPath
        "${work_ram_output}"
        -TracePath
        "${trace_output}"
        -CallPath
        "${call_output}"
        -WritePath
        "${write_output}"
    RESULT_VARIABLE semantics_result
    OUTPUT_VARIABLE semantics_output
    ERROR_VARIABLE semantics_error
)
if(NOT semantics_result EQUAL 0)
    message(
        FATAL_ERROR
        "Session/scoring analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Session-and-scoring route verified."
    "Drain: frame 3000 raises session bit 1"
    "Continuation: the zero transition latch selects ordinary C80A"
    "Score display: stored 1020 emits nine score digits for 102000"
    "No multiplier, extra-ball grant, alternate transition"
    "Maximum display: saturation 9999999 and two fixed zero glyphs"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Session/scoring analysis omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
