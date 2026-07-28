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
    message(
        FATAL_ERROR
        "Unsupported Devil-Shooter-companion configuration: ${CONFIG}"
    )
endif()

set(
    expected_trace_sha256
    "8b23e18603b196bf1adb7c67527f111769099e01682d0b47625a0a40a918c9b0"
)
set(
    expected_call_sha256
    "31713928b949d25e0a2345febd119371e14e2d4723574d425ffc02d8adb565d2"
)
set(
    expected_write_sha256
    "ecfc80c0488c5fe7e98f1ffe67db35d108c4746f222345f846f6af443ad8acc3"
)
set(
    expected_state_write_sha256
    "13e3373182b0982eb40b61df93d0002d6ba9004c6b2d099a0129380ec6e21211"
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
        "02:53A8"
        --observe-hook
        "02:53BB"
        --observe-hook
        "02:53C8"
        --observe-hook
        "02:53DD"
        --observe-hook
        "02:5394"
        --observe-hook
        "02:5397"
        --observe-hook
        "02:53FC"
        --observe-hook
        "02:5408"
        --observe-hook
        "02:5421"
        --observe-hook
        "02:5434"
        --observe-hook
        "02:5441"
        --observe-hook
        "02:5456"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "02:53BB@1"
        --routine-replay
        "02:53C8@1"
        --routine-replay
        "02:53DD@1"
        --routine-replay
        "02:5397@1"
        --routine-replay
        "02:5408@1"
        --routine-replay
        "02:5434@1"
        --routine-replay
        "02:5441@1"
        --routine-replay
        "02:5456@1"
        --work-ram-write-range
        "2053-2053"
        --work-ram-write-range
        "2D15-2D15"
        --work-ram-write-range
        "300A-300E"
        --work-ram-write-range
        "308A-308E"
        --work-ram-write-range
        "318A-318E"
        --work-ram-write-range
        "320A-320E"
        --work-ram-write-range
        "328A-328E"
        --work-ram-write-range
        "330A-330E"
        --work-ram-write-range
        "338A-338E"
        --work-ram-write-range
        "350A-350E"
        --work-ram-write-range
        "358A-358E"
        --work-ram-write-range
        "380A-380E"
        --work-ram-write-range
        "388A-388E"
        --work-ram-write-range
        "390A-390E"
        --work-ram-write-range
        "398A-398E"
        --work-ram-write-range
        "3A0A-3A0E"
        --work-ram-write-range
        "3A8A-3A8E"
        --work-ram-write-range
        "3B0A-3B0E"
        --work-ram-write-range
        "3B8A-3B8E"
        --work-ram-write-range
        "3C0A-3C0E"
        --work-ram-write-range
        "3C8A-3C8E"
        --work-ram-write-range
        "3D0A-3D0E"
        --work-ram-write-range
        "3D8A-3D8E"
        --work-ram-write-range
        "3E0A-3E0E"
        --work-ram-write-range
        "3E8A-3E8E"
        --work-ram-write-range
        "3F0A-3F0E"
        --work-ram-write-range
        "3F8A-3F8E"
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
        "Devil-Shooter-companion recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 3"
    "Observe hook 3 entries: 266"
    "Observe hook 4 entries: 1055"
    "Observe hook 5 entries: 1"
    "Observe hook 6 entries: 1260"
    "Observe hook 7 entries: 1"
    "Observe hook 8 entries: 1288"
    "Observe hook 9 entries: 1"
    "Observe hook 10 entries: 3"
    "Observe hook 11 entries: 267"
    "Observe hook 12 entries: 1085"
    "Routine trace complete: 5231 entries"
    "Routine call trace complete: 5231 calls, 6032 writes"
    "Routine replay 02:53BB@1 verified: frame 872, 99 body cycles, 3 ordered writes"
    "Routine replay 02:53C8@1 verified: frame 943, 99 body cycles, 3 ordered writes"
    "Routine replay 02:53DD@1 verified: frame 2136, 24 body cycles, 0 ordered writes"
    "Routine replay 02:5397@1 verified: frame 872, 75 body cycles, 2 ordered writes"
    "Routine replay 02:5408@1 verified: frame 872, 105 body cycles, 2 ordered writes"
    "Routine replay 02:5434@1 verified: frame 872, 99 body cycles, 3 ordered writes"
    "Routine replay 02:5441@1 verified: frame 943, 99 body cycles, 3 ordered writes"
    "Routine replay 02:5456@1 verified: frame 2132, 24 body cycles, 0 ordered writes"
    "Routine replay complete: 8 state-normalized calls"
    "Work-RAM write trace complete: 8133 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Devil-Shooter-companion output omitted '${expected_line}'.\n"
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
            "Devil-Shooter-companion ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(
        STATUS
        "Devil-Shooter-companion ${artifact} verified: ${actual_sha256}"
    )
endforeach()

file(SHA256 "${state_write_output}" actual_state_write_sha256)
if(NOT actual_state_write_sha256 STREQUAL expected_state_write_sha256)
    message(
        FATAL_ERROR
        "Devil-Shooter-companion selected-state artifact changed.\n"
        "Expected: ${expected_state_write_sha256}\n"
        "Actual:   ${actual_state_write_sha256}\n"
    )
endif()
message(
    STATUS
    "Devil-Shooter-companion selected state verified: "
    "${actual_state_write_sha256}"
)

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-DevilShooterCompanionSemantics.ps1"
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
        "Devil-Shooter-companion semantics failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Devil-Shooter companion semantics verified."
    "Fixed type-80 slots 0A-0D surround Shooter slot 0E"
    "projects Y=02A8+floor(2D15/2)"
    "four-/five-frame active animations"
    "Eight normalized samples"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Devil-Shooter-companion semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
