# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(CONFIG STREQUAL "Debug")
    # After the route begins scrolling through the upper table, Beetle's
    # Debug and optimized renderers produce different pixel hashes while
    # retaining identical work RAM, audio, routine scopes, and writes.
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
    message(FATAL_ERROR "Unsupported state-census configuration: ${CONFIG}")
endif()
set(
    expected_work_ram_sha256
    "07673ea2b1101aff61f6058780a0817ec47191ea3d2abf5559a994d14db84779"
)
set(
    expected_trace_sha256
    "2dd427c127b2d065eb7006f3a8e36f2573f959345439e1a2a9e3af4d82b805c1"
)
set(
    expected_call_sha256
    "829fde1eb128d18166e504d8bb716e92149aac64e213379a419ed372b966fe2d"
)
set(
    expected_write_sha256
    "6c4678d6c0810d0aae3533b2389396310bcb9f0aa5575c17868203fe88f2c2c8"
)

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
        "00:F43D"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "State-census recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Observe hook 1 entries: 232"
    "Routine trace complete: 232 entries"
    "Routine call trace complete: 232 calls, 205563 writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "State-census output omitted '${expected_line}'.\n"
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
            "State-census ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(STATUS "State-census ${artifact} verified: ${actual_sha256}")
endforeach()
