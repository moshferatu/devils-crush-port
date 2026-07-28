# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(CONFIG STREQUAL "Debug")
    set(
        expected_census_sha256
        "a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2"
    )
elseif(CONFIG STREQUAL "Release")
    set(
        expected_census_sha256
        "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
    )
else()
    message(FATAL_ERROR "Unsupported Side-Star configuration: ${CONFIG}")
endif()

set(
    expected_trace_sha256
    "7091984892271017079a1d98450f5e72f1242017eed26bb66c1209d917113339"
)
set(
    expected_call_sha256
    "4cd9ebfab43aa644483536b36f95d49dc47c02743f5d3f51d5fe2b824ddcd956"
)
set(
    expected_write_sha256
    "e4a2b7c0be94f44bd7593d8b0b4930a7f95aca598fd8acf90e357ddaab5279c2"
)
set(
    expected_state_write_sha256
    "a8932210562ebfb1b4195fbc1b27e9094a604fa10a9255c8364726d6bf01650e"
)

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(state_write_output "${OUTPUT}.state.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --observe-hook
        "02:4A54"
        --observe-hook
        "02:4BD5"
        --observe-hook
        "02:4A4C"
        --observe-hook
        "02:4C0A"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "02:4BD5@1"
        --routine-replay
        "02:4C0A@1"
        --work-ram-write-range
        "2258-2258"
        --work-ram-write-range
        "2D0C-2D0C"
        --work-ram-write-range
        "2D16-2D17"
        --work-ram-write-range
        "3010-3010"
        --work-ram-write-range
        "3012-3012"
        --work-ram-write-range
        "3090-3090"
        --work-ram-write-range
        "3092-3092"
        --work-ram-write-range
        "3810-3810"
        --work-ram-write-range
        "3812-3812"
        --work-ram-write-range
        "3890-3890"
        --work-ram-write-range
        "3892-3892"
        --work-ram-write-range
        "3910-3910"
        --work-ram-write-range
        "3912-3912"
        --work-ram-write-range
        "3990-3990"
        --work-ram-write-range
        "3992-3992"
        --work-ram-write-range
        "3A10-3A10"
        --work-ram-write-range
        "3A12-3A12"
        --work-ram-write-range
        "3A90-3A90"
        --work-ram-write-range
        "3A92-3A92"
        --work-ram-write-range
        "3C10-3C10"
        --work-ram-write-range
        "3C12-3C12"
        --work-ram-write-range
        "3C90-3C90"
        --work-ram-write-range
        "3C92-3C92"
        --work-ram-write-range
        "3D10-3D10"
        --work-ram-write-range
        "3D12-3D12"
        --work-ram-write-range
        "3D90-3D90"
        --work-ram-write-range
        "3D92-3D92"
        --work-ram-write-range
        "3E10-3E10"
        --work-ram-write-range
        "3E12-3E12"
        --work-ram-write-range
        "3E90-3E90"
        --work-ram-write-range
        "3E92-3E92"
        --work-ram-write-range
        "3F10-3F10"
        --work-ram-write-range
        "3F12-3F12"
        --work-ram-write-range
        "3F90-3F90"
        --work-ram-write-range
        "3F92-3F92"
        --work-ram-write-trace
        "${state_write_output}"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Side-Star recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 1277"
    "Observe hook 3 entries: 1"
    "Observe hook 4 entries: 905"
    "Routine trace complete: 2184 entries"
    "Routine call trace complete: 2184 calls, 4298 writes"
    "Routine replay 02:4BD5@1 verified: frame 871, 135 body cycles, 3 ordered writes"
    "Routine replay 02:4C0A@1 verified: frame 871, 135 body cycles, 3 ordered writes"
    "Routine replay complete: 2 state-normalized calls"
    "Work-RAM write trace complete: 5246 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Side-Star output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census trace call write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
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
            "Side-Star ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(STATUS "Side-Star ${artifact} verified: ${actual_sha256}")
endforeach()

file(SHA256 "${state_write_output}" actual_state_write_sha256)
if(NOT actual_state_write_sha256 STREQUAL expected_state_write_sha256)
    message(
        FATAL_ERROR
        "Side-Star selected-state artifact changed.\n"
        "Expected: ${expected_state_write_sha256}\n"
        "Actual:   ${actual_state_write_sha256}\n"
    )
endif()
message(
    STATUS
    "Side-Star selected state verified: ${actual_state_write_sha256}"
)

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-SideStarAccessSemantics.ps1"
        -RomPath
        "${ROM}"
        -CallPath
        "${call_output}"
        -WritePath
        "${write_output}"
        -StateWritePath
        "${state_write_output}"
    RESULT_VARIABLE semantics_result
    OUTPUT_VARIABLE semantics_output
    ERROR_VARIABLE semantics_error
)
if(NOT semantics_result EQUAL 0)
    message(
        FATAL_ERROR
        "Side-Star semantics failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Side-Star access semantics verified."
    "Fixed slots 10/12 at (38,372)/(238,308)"
    "1277/905 live calls remain unhit"
    "hits award 80 stored units"
    "Normalized samples: 02:4BD5@1 and 02:4C0A@1"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Side-Star semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
