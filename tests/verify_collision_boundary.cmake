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
    message(FATAL_ERROR "Unsupported collision-boundary configuration: ${CONFIG}")
endif()

set(
    expected_work_ram_sha256
    "07673ea2b1101aff61f6058780a0817ec47191ea3d2abf5559a994d14db84779"
)
set(
    expected_trace_sha256
    "e2375353bf5206e199167968c9fa510d909f7caf6c9534c2c649e642788d94e5"
)
set(
    expected_call_sha256
    "ee3569150d74133c017baa7b20687cd2c0b3f838a88731b30e58441df7a87fe6"
)
set(
    expected_write_sha256
    "9c0fda84b35afa075465975fc8efb0320c9fc441bedd42d93b067e016b2dd426"
)
set(
    expected_state_write_sha256
    "99cf6ab9fed4cfa976510fda30f77c20671e8f2134fc3b2842e3d90fd81d32d8"
)

set(work_ram_output "${OUTPUT}.wram.bin")
set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(state_write_output "${OUTPUT}.state-writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --canonical-work-ram
        "${work_ram_output}"
        --observe-hook
        "01:CBCF"
        --observe-hook
        "03:7F57"
        --observe-hook
        "03:6966"
        --observe-hook
        "03:7FB1"
        --observe-hook
        "00:F55C"
        --observe-hook
        "00:ED18"
        --replace-hook
        "03:7FB1@1"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --work-ram-write-range
        "3000-307F"
        --work-ram-write-range
        "3800-387F"
        --work-ram-write-range
        "3C00-3C7F"
        --work-ram-write-range
        "3F00-3FFF"
        --work-ram-write-range
        "2D16-2D17"
        --work-ram-write-range
        "2205-221A"
        --work-ram-write-trace
        "${state_write_output}"
        --routine-replay
        "03:7FB1@1"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Collision-boundary recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Observe hook 1 entries: 270"
    "Observe hook 2 entries: 20867"
    "Observe hook 3 entries: 54149"
    "Observe hook 4 entries: 21987"
    "Observe hook 5 entries: 40"
    "Observe hook 6 entries: 66"
    "Native replacement 03:7FB1@1 entries: 1"
    "Routine trace complete: 97379 entries"
    "Routine call trace complete: 97379 calls, 1163259 writes"
    "Work-RAM write trace complete: 8893 selected writes"
    "Routine replay 03:7FB1@1 verified: frame 957, 126 body cycles, 3 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Collision-boundary output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census work_ram trace call write state_write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "work_ram")
        set(artifact_path "${work_ram_output}")
    elseif(artifact STREQUAL "trace")
        set(artifact_path "${trace_output}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    elseif(artifact STREQUAL "write")
        set(artifact_path "${write_output}")
    else()
        set(artifact_path "${state_write_output}")
    endif()

    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "Collision-boundary ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(
        STATUS
        "Collision-boundary ${artifact} verified: ${actual_sha256}"
    )
endforeach()

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-CollisionBoundaryRoute.ps1"
        -RomPath
        "${ROM}"
        -RecordingPath
        "${OUTPUT}"
        -WorkRamPath
        "${work_ram_output}"
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
        "Collision-boundary analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Normal-table collision boundary verified."
    "Contact: 01:CBCF marks slot 62 at frames 1479 and 1852"
    "Dispatch: each contact precedes 03:6966 and 03:7FB1"
    "Effects: each hit adds 30 stored units and emits command 21"
    "Event: slot 62 reaches counter zero, $2D16 becomes 4"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Collision-boundary analysis omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
