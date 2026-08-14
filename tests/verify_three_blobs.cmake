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
    message(FATAL_ERROR "Unsupported Three-Blobs-test configuration: ${CONFIG}")
endif()

set(
    expected_trace_sha256
    "1c7a37119a33fe9b58a677961172542d8d1de25412e5b3b814bdf9f99273039d"
)
set(
    expected_call_sha256
    "7825073999ce82a77eab3c647e53d1ec2c3fc4c747a2ba8a6a7aa7388363860a"
)
set(
    expected_write_sha256
    "e72fbf7baa742a8f23bace6ed57adda2e407e906953450306507fb6b476a6b2c"
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
        "03:66A3"
        --observe-hook
        "03:66D3"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "03:66A3@1"
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
        "Three-Blobs recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Observe hook 1 entries: 1107"
    "Observe hook 2 entries: 720"
    "Routine trace complete: 1827 entries"
    "Routine call trace complete: 1827 calls, 33397 writes"
    "Routine replay 03:66A3@1 verified: frame 1229"
    "Routine replay complete: 1 state-normalized calls"
    "Work-RAM write trace complete: 3872 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Three-Blobs output omitted '${expected_line}'.\n"
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
            "Three-Blobs ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(STATUS "Three-Blobs ${artifact} verified: ${actual_sha256}")
endforeach()

file(SHA256 "${actor_write_output}" actual_actor_write_sha256)
if(NOT actual_actor_write_sha256 STREQUAL expected_actor_write_sha256)
    message(
        FATAL_ERROR
        "Three-Blobs actor-identity artifact changed.\n"
        "Expected: ${expected_actor_write_sha256}\n"
        "Actual:   ${actual_actor_write_sha256}\n"
    )
endif()
message(
    STATUS
    "Three-Blobs actor identity verified: ${actual_actor_write_sha256}"
)

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-ThreeBlobsSemantics.ps1"
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
        "Three-Blobs semantics analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Three-Blobs semantics verified."
    "Fixed Three Blobs: slots 58/59/5A"
    "Collision cycle: 16 scoreless state advances"
    "Appearance rule: six internal states"
    "First normalized sample: 03:66A3@1, frame 1229"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Three-Blobs semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
