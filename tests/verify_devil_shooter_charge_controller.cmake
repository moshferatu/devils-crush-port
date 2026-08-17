# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release" AND NOT CONFIG STREQUAL "Debug")
    message(FATAL_ERROR "Unsupported charge-controller configuration: ${CONFIG}")
endif()

set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_work_ram_sha256 "07673ea2b1101aff61f6058780a0817ec47191ea3d2abf5559a994d14db84779")
set(expected_trace_sha256 "dcd4eb69a4e8e9f3934d9ef3d2a69e406bdec00a0f8d7a16a2881e1beedfc32a")
set(expected_call_sha256 "4689d208c55435d4d2cd30245686b64c6ca5bfe5620563d5405168981529b062")
set(expected_write_sha256 "0b0ccba457fb43d79424810d8c1a968ca764aa51050147057dd60628c1e41cdc")

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
        --observe-hook
        "02:5249"
        --observe-hook
        "02:524C"
        --replace-hook
        "02:5249@all"
        --replace-hook
        "02:524C@all"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "02:5249@1"
        --routine-replay
        "02:524C@70"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Devil-Shooter charge-controller recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 02:5249@all entries: 86 of 156"
    "Native replacement 02:524C@all entries: 70 of 70"
    "Observe hook 1 entries: 156"
    "Observe hook 2 entries: 70"
    "Routine trace complete: 226 entries"
    "Routine call trace complete: 226 calls, 1310 writes"
    "Routine replay 02:5249@1 verified: frame 1265, 312 body cycles, 7 ordered writes"
    "Routine replay 02:524C@70 verified: frame 1420, 261 body cycles, 8 ordered writes"
    "Routine replay complete: 2 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Charge-controller output omitted '${expected_line}'.\n"
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
            "Charge-controller ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(STATUS "Devil-Shooter charge controller verified.")
