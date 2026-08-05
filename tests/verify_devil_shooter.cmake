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
    message(FATAL_ERROR "Unsupported Devil-Shooter-test configuration: ${CONFIG}")
endif()

set(
    expected_trace_sha256
    "7ce4e1d5b4304b8d4e490d674a843e111b5575d5a0865830457e3182ac3ac469"
)
set(
    expected_call_sha256
    "b52db13b5af2afdf174ba1de2d739504bd78c42356395a84fa78e58c6a0ba037"
)
set(
    expected_write_sha256
    "c14f3c56f8917da8eb5a1a6abbbb0152f6a2ec454248349201e6366c68051019"
)
set(
    expected_actor_write_sha256
    "b311b7704f05b9bca07da08e71026f9415d4752d044361d026e816c9cafdc338"
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
        "02:5116"
        --observe-hook
        "02:5159"
        --observe-hook
        "02:52E3"
        --observe-hook
        "02:533B"
        --replace-hook
        "02:5159@all"
        --replace-hook
        "02:52E3@all"
        --replace-hook
        "02:533B@all"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "02:5116@1"
        --routine-replay
        "02:5159@97"
        --routine-replay
        "02:52E3@1"
        --routine-replay
        "02:533B@2"
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
        "Devil-Shooter recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Observe hook 1 entries: 125"
    "Observe hook 2 entries: 101"
    "Observe hook 3 entries: 252"
    "Observe hook 4 entries: 32"
    "Native replacement 02:5159@all entries: 4 of 101"
    "Native replacement 02:52E3@all entries: 137 of 252"
    "Native replacement 02:533B@all entries: 11 of 32"
    "Routine trace complete: 510 entries"
    "Routine call trace complete: 510 calls, 10744 writes"
    "Routine replay 02:5116@1 verified: frame 872"
    "Routine replay 02:5159@97 verified: frame 1181, 96 body cycles, 0 ordered writes"
    "Routine replay 02:52E3@1 verified: frame 1085, 243 body cycles, 5 ordered writes"
    "Routine replay 02:533B@2 verified: frame 1066, 327 body cycles, 5 ordered writes"
    "Routine replay complete: 4 state-normalized calls"
    "Work-RAM write trace complete: 3872 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Devil-Shooter output omitted '${expected_line}'.\n"
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
            "Devil-Shooter ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(STATUS "Devil-Shooter ${artifact} verified: ${actual_sha256}")
endforeach()

file(SHA256 "${actor_write_output}" actual_actor_write_sha256)
if(NOT actual_actor_write_sha256 STREQUAL expected_actor_write_sha256)
    message(
        FATAL_ERROR
        "Devil-Shooter actor-identity artifact changed.\n"
        "Expected: ${expected_actor_write_sha256}\n"
        "Actual:   ${actual_actor_write_sha256}\n"
    )
endif()
message(
    STATUS
    "Devil-Shooter actor identity verified: ${actual_actor_write_sha256}"
)

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-DevilShooterSemantics.ps1"
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
        "Devil-Shooter semantics analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Devil-Shooter semantics verified."
    "Fixed Devil Shooter: slot 0E, initializer 02:50EE"
    "Catch: frame 1064 emits 25"
    "Charge/release: Button-I hold frames 1085-1180"
    "First normalized idle sample: 02:5116@1, frame 872"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Devil-Shooter semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
