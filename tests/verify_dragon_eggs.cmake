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
    message(FATAL_ERROR "Unsupported Dragon's-Egg-test configuration: ${CONFIG}")
endif()

set(
    expected_trace_sha256
    "e85c4cc08c871c647ec68112c21f1225b2712cb38075cdd9b2ada35e4f3e5237"
)
set(
    expected_call_sha256
    "b60478c75fa0be9b67d33b7d15a5dbb9080b6ddb81c25655ae334622b8f5b83a"
)
set(
    expected_write_sha256
    "a486c285201f6b932f5ad29948475b1e8d2ced44df28b513ee8258fb8958e071"
)
set(
    expected_actor_write_sha256
    "596c47f8705d721bd0c75b3c41691e8d75c92f466be98b291384624a3e69c3a8"
)

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(actor_write_output "${OUTPUT}.actors.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --observe-hook
        "02:42EB"
        --observe-hook
        "02:42F6"
        --observe-hook
        "02:433F"
        --observe-hook
        "02:4345"
        --observe-hook
        "02:4379"
        --observe-hook
        "02:441E"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "02:42F6@1"
        --routine-replay
        "02:4345@1"
        --work-ram-write-range
        "2CF4-2CF7"
        --work-ram-write-range
        "2CFB-2CFB"
        --work-ram-write-range
        "2D20-2D20"
        --work-ram-write-range
        "3000-307F"
        --work-ram-write-range
        "3F00-3FFF"
        --work-ram-write-trace
        "${actor_write_output}"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Dragon's-Egg recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 1790"
    "Observe hook 3 entries: 6"
    "Observe hook 4 entries: 9857"
    "Observe hook 5 entries: 1"
    "Observe hook 6 entries: 5"
    "Routine trace complete: 11660 entries"
    "Routine call trace complete: 11660 calls, 2481 writes"
    "Routine replay 02:42F6@1 verified: frame 873"
    "Routine replay 02:4345@1 verified: frame 873"
    "Routine replay complete: 2 state-normalized calls"
    "Work-RAM write trace complete: 3893 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Dragon's-Egg output omitted '${expected_line}'.\n"
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
            "Dragon's-Egg ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(STATUS "Dragon's-Egg ${artifact} verified: ${actual_sha256}")
endforeach()

file(SHA256 "${actor_write_output}" actual_actor_write_sha256)
if(NOT actual_actor_write_sha256 STREQUAL expected_actor_write_sha256)
    message(
        FATAL_ERROR
        "Dragon's-Egg selected-state artifact changed.\n"
        "Expected: ${expected_actor_write_sha256}\n"
        "Actual:   ${actual_actor_write_sha256}\n"
    )
endif()
message(
    STATUS
    "Dragon's-Egg selected state verified: ${actual_actor_write_sha256}"
)

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-DragonEggSemantics.ps1"
        -RomPath
        "${ROM}"
        -CallPath
        "${call_output}"
        -WritePath
        "${write_output}"
        -ActorWritePath
        "${actor_write_output}"
    RESULT_VARIABLE semantics_result
    OUTPUT_VARIABLE semantics_output
    ERROR_VARIABLE semantics_error
)
if(NOT semantics_result EQUAL 0)
    message(
        FATAL_ERROR
        "Dragon's-Egg semantics analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Dragon's-Egg semantics verified."
    "Six fixed eggs: slots 20-25, controller slot 1F"
    "Routed egg 22 hit: +10 stored units, command 39"
    "Hatchling hit: +200 stored units, command 34"
    "Inactive all-six completion: +100 stored units"
    "Normalized idle samples: 02:42F6@1 and 02:4345@1"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Dragon's-Egg semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
