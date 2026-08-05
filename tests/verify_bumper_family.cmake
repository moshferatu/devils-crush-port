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
    message(FATAL_ERROR "Unsupported bumper-family-test configuration: ${CONFIG}")
endif()

set(
    expected_trace_sha256
    "e514cded86529e0a97c1a544fee3b5a7944cd6348f00e6367e4c9358973374bc"
)
set(
    expected_call_sha256
    "2973c427b27c5bdf763e4a1e19d76ef83bba9697f57610a4ad721643ec42125d"
)
set(
    expected_write_sha256
    "db7f5e7350b2c31a2469dc4eaec06618af2b6d5043d42bd18258bd42ce8e4021"
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
        "03:789A"
        --observe-hook
        "03:78A4"
        --observe-hook
        "03:78C7"
        --replace-hook
        "03:789A@all"
        --replace-hook
        "03:78A4@all"
        --replace-hook
        "03:78C7@all"
        --replace-hook
        "03:78F5@all"
        --replace-hook
        "03:7912@all"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "03:789A@1"
        --routine-replay
        "03:78A4@1"
        --routine-replay
        "03:78C7@1"
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
        "Bumper family recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 03:789A@all entries: 1 of 1"
    "Native replacement 03:78A4@all entries: 1 of 1"
    "Native replacement 03:78C7@all entries: 3468 of 3468"
    "Native replacement 03:78F5@all entries: 9 of 9"
    "Native replacement 03:7912@all entries: 2 of 2"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 1"
    "Observe hook 3 entries: 3468"
    "Routine trace complete: 3470 entries"
    "Routine call trace complete: 3470 calls, 3155 writes"
    "Routine replay 03:789A@1 verified: frame 871, 342 body cycles, 14 ordered writes"
    "Routine replay 03:78A4@1 verified: frame 871, 336 body cycles, 14 ordered writes"
    "Routine replay 03:78C7@1 verified: frame 872"
    "Routine replay complete: 3 state-normalized calls"
    "Work-RAM write trace complete: 3872 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Bumper family output omitted '${expected_line}'.\n"
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
            "Bumper family ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(STATUS "Bumper family ${artifact} verified: ${actual_sha256}")
endforeach()

file(SHA256 "${actor_write_output}" actual_actor_write_sha256)
if(NOT actual_actor_write_sha256 STREQUAL expected_actor_write_sha256)
    message(
        FATAL_ERROR
        "Bumper family actor-identity artifact changed.\n"
        "Expected: ${expected_actor_write_sha256}\n"
        "Actual:   ${actual_actor_write_sha256}\n"
    )
endif()
message(
    STATUS
    "Bumper family actor identity verified: ${actual_actor_write_sha256}"
)

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-BumperFamilySemantics.ps1"
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
        "Bumper family semantics analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Bumper family semantics verified."
    "Fixed bumpers: slots 14/15 at (190/74,604), dispatch 03:78C7"
    "Hit rule: 11 routed hits award 10 and emit command 20"
    "Terminal rule: phase 9 adds 200 and spawns six type-83 children"
    "Observed score contribution: 110 stored units"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Bumper family semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
