# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(CONFIG STREQUAL "Debug")
    # Absorbing the steady post-launch return moves this route onto the
    # repository's known normalized framebuffer-placement variant. Work RAM,
    # the complete Shooter call/write artifacts, and actor state remain
    # byte-identical to Debug assembly.
    set(
        expected_census_sha256
        "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
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
    "7951bfdc12478f2f7a385aed2494e15d109ad264001db5d4ae369628033875ef"
)
set(
    expected_work_ram_sha256
    "07673ea2b1101aff61f6058780a0817ec47191ea3d2abf5559a994d14db84779"
)
set(
    expected_call_sha256
    "305eda8844916d6357bc9d4143587b115a6f51fedff07b6f6b124b6c25bf0b53"
)
set(
    expected_write_sha256
    "53d844b9f824eb2959245997e7bd736effc13e40d8a8118eddce53703e5f8c12"
)
set(
    expected_actor_write_sha256
    "b311b7704f05b9bca07da08e71026f9415d4752d044361d026e816c9cafdc338"
)

set(trace_output "${OUTPUT}.trace.csv")
set(work_ram_output "${OUTPUT}.wram.bin")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(actor_write_output "${OUTPUT}.actors.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --canonical-work-ram
        "${work_ram_output}"
        --observe-hook
        "02:5116"
        --observe-hook
        "02:5159"
        --observe-hook
        "02:52E3"
        --observe-hook
        "02:533B"
        --observe-hook
        "02:525F"
        --replace-hook
        "02:5159@all"
        --replace-hook
        "02:52E3@all"
        --replace-hook
        "02:533A@all"
        --replace-hook
        "02:533B@all"
        --replace-hook
        "02:525F@all"
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
        --routine-replay
        "02:525F@1"
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
    "Observe hook 5 entries: 2169"
    "Native replacement 02:5159@all entries: 4 of 101"
    "Native replacement 02:52E3@all entries: 137 of 252"
    "Native replacement 02:533A@all entries: 231 of 252"
    "Native replacement 02:533B@all entries: 11 of 32"
    "Native replacement 02:525F@all entries: 2169 of 2169"
    "Routine trace complete: 2679 entries"
    "Routine call trace complete: 2679 calls, 11229 writes"
    "Routine replay 02:5116@1 verified: frame 872"
    "Routine replay 02:5159@97 verified: frame 1181, 96 body cycles, 0 ordered writes"
    "Routine replay 02:52E3@1 verified: frame 1085, 243 body cycles, 5 ordered writes"
    "Routine replay 02:533B@2 verified: frame 1066, 327 body cycles, 5 ordered writes"
    "Routine replay 02:525F@1 verified: frame 1421, 45 body cycles, 0 ordered writes"
    "Routine replay complete: 5 state-normalized calls"
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
    "Post-launch: 2,169 collision-free/session-idle checks return at 02:526A."
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
