# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release" AND NOT CONFIG STREQUAL "Debug")
    message(FATAL_ERROR "Unsupported charge-entry-controller configuration: ${CONFIG}")
endif()

set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_work_ram_sha256 "07673ea2b1101aff61f6058780a0817ec47191ea3d2abf5559a994d14db84779")
set(expected_trace_sha256 "748721b9aa9513d2953eac0e7d30b734adf08df8709485ca6769be2d8fb54d5b")
set(expected_call_sha256 "8511fa5d1750b88cd3a5004c42d93c2d56d709958796aeee5d2a77b49796b4fa")
set(expected_write_sha256 "28e4db1c09d3e4c1fceb0722c596a2532ee3a8ad8841c101fc282ac0c2eedf00")

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
        "02:5223"
        --replace-hook
        "02:5223@all"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "02:5223@67"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Devil-Shooter charge-entry-controller recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 02:5223@all entries: 67 of 67"
    "Observe hook 1 entries: 67"
    "Routine trace complete: 67 entries"
    "Routine call trace complete: 67 calls, 32 writes"
    "Routine replay 02:5223@67 verified: frame 1264, 249 body cycles, 7 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Charge-entry-controller output omitted '${expected_line}'.\n"
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
            "Charge-entry-controller ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(STATUS "Devil-Shooter charge-entry controller verified.")
