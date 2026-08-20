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
set(expected_trace_sha256 "7567eba2773ef5029dbbd488bcf82f7811c87eb0aeb1f3d8fe22e85637ef41d5")
set(expected_call_sha256 "2f25467a0ce012580d48b027e69ef1d661874b2ac47a588bc6921fbcb230255d")
set(expected_write_sha256 "2c954198b15801ccce679794ff4d19e63d643dc8a107d5030f81c12e42ddb93b")
set(expected_tail_trace_sha256 "791034eff2642f04726a900dc7f5e2fc61ee00578d7359586d3866620cbc531c")
set(expected_tail_call_sha256 "b74f8f871f0ccad157b962a0ef9fb5242a7764fc580f960e9ae7fa338cc3bcd1")
set(expected_tail_write_sha256 "cdac4d2001b546d52608d154deaad05aeca2c95888e4bc2016e93c123b277401")

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
        --replace-hook
        "02:5249@all"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "02:5249@1"
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
    "Native replacement 02:5249@all entries: 156 of 156"
    "Observe hook 1 entries: 156"
    "Routine trace complete: 156 entries"
    "Routine call trace complete: 156 calls, 1302 writes"
    "Routine replay 02:5249@1 verified: frame 1265, 312 body cycles, 7 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
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

# The parent now composes every child and continuation boundary, so prove the
# independently dispatchable $524C continuation on an uncomposed run.
set(tail_output "${OUTPUT}.tail.csv")
set(tail_work_ram_output "${tail_output}.wram.bin")
set(tail_trace_output "${tail_output}.trace.csv")
set(tail_call_output "${tail_output}.calls.csv")
set(tail_write_output "${tail_call_output}.writes.csv")
execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${tail_output}"
        --canonical-work-ram "${tail_work_ram_output}"
        --observe-hook "02:524C"
        --replace-hook "02:524C@all"
        --routine-trace "${tail_trace_output}"
        --routine-call-trace "${tail_call_output}"
        --routine-replay "02:524C@70"
        --rom "${ROM}"
    RESULT_VARIABLE tail_recorder_result
    OUTPUT_VARIABLE tail_recorder_output
    ERROR_VARIABLE tail_recorder_error
)
if(NOT tail_recorder_result EQUAL 0)
    message(FATAL_ERROR "Devil-Shooter charge-controller tail recorder failed (${tail_recorder_result}).\n${tail_recorder_output}${tail_recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 02:524C@all entries: 156 of 156"
    "Observe hook 1 entries: 156"
    "Routine trace complete: 156 entries"
    "Routine call trace complete: 156 calls, 48 writes"
    "Routine replay 02:524C@70 verified: frame 1334, 27 body cycles, 0 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${tail_recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Charge-controller tail output omitted '${expected_line}'.\n${tail_recorder_output}${tail_recorder_error}")
    endif()
endforeach()

foreach(artifact IN ITEMS census work_ram trace call write)
    if(artifact STREQUAL "census")
        set(artifact_path "${tail_output}")
        set(expected_sha256 "${expected_census_sha256}")
    elseif(artifact STREQUAL "work_ram")
        set(artifact_path "${tail_work_ram_output}")
        set(expected_sha256 "${expected_work_ram_sha256}")
    elseif(artifact STREQUAL "trace")
        set(artifact_path "${tail_trace_output}")
        set(expected_sha256 "${expected_tail_trace_sha256}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${tail_call_output}")
        set(expected_sha256 "${expected_tail_call_sha256}")
    else()
        set(artifact_path "${tail_write_output}")
        set(expected_sha256 "${expected_tail_write_sha256}")
    endif()
    file(SHA256 "${artifact_path}" actual_sha256)
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(FATAL_ERROR "Charge-controller tail ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}\n")
    endif()
endforeach()

message(STATUS "Devil-Shooter charge controller verified.")
