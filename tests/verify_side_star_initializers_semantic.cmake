# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported Side-Star initializer configuration: ${CONFIG}")
endif()

set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")

function(verify_side_star_initializer name routine expected_call_sha256 expected_write_sha256)
    set(census_output "${OUTPUT}-${name}.csv")
    set(call_output "${census_output}.calls.csv")
    set(write_output "${call_output}.writes.csv")

    execute_process(
        COMMAND
            "${APP}"
            --record-state-census "${census_output}"
            --observe-hook "${routine}"
            --replace-hook "${routine}@all"
            --routine-call-trace "${call_output}"
            --routine-replay "${routine}@1"
            --rom "${ROM}"
        RESULT_VARIABLE recorder_result
        OUTPUT_VARIABLE recorder_output
        ERROR_VARIABLE recorder_error
    )
    if(NOT recorder_result EQUAL 0)
        message(FATAL_ERROR "Semantic ${routine} Side-Star initializer recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
    endif()

    foreach(expected_line IN ITEMS
        "State census recording complete: 3600 frames"
        "Native replacement ${routine}@all entries: 1 of 1"
        "Observe hook 1 entries: 1"
        "Routine call trace complete: 1 calls, 9 writes"
        "Routine replay ${routine}@1 verified: frame 871, 273 body cycles, 9 ordered writes"
        "Routine replay complete: 1 state-normalized calls"
    )
        string(FIND "${recorder_output}" "${expected_line}" line_offset)
        if(line_offset EQUAL -1)
            message(FATAL_ERROR "Semantic ${routine} Side-Star initializer output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
        endif()
    endforeach()

    foreach(artifact IN ITEMS census call write)
        if(artifact STREQUAL "census")
            set(artifact_path "${census_output}")
            set(expected_sha256 "${expected_census_sha256}")
        elseif(artifact STREQUAL "call")
            set(artifact_path "${call_output}")
            set(expected_sha256 "${expected_call_sha256}")
        else()
            set(artifact_path "${write_output}")
            set(expected_sha256 "${expected_write_sha256}")
        endif()
        file(SHA256 "${artifact_path}" actual_sha256)
        if(NOT actual_sha256 STREQUAL expected_sha256)
            message(
                FATAL_ERROR
                "Semantic ${routine} Side-Star initializer ${artifact} artifact changed.\n"
                "Expected: ${expected_sha256}\nActual:   ${actual_sha256}"
            )
        endif()
    endforeach()
endfunction()

verify_side_star_initializer(
    "lower"
    "02:4A54"
    "ffd987afd06d83aaa82f0c406447ec5f2aafa3fb69b2125fa08d0612f9b02d94"
    "9586c8d86bef8038c6def07e8b93ac321aabf8bfcc8f411e9e0d69186fbdec8c"
)
verify_side_star_initializer(
    "upper"
    "02:4A4C"
    "101c406da34e44d2ce206f491f980a036b48d530192e723989038229a60f5d37"
    "6584081da8da07b2a1e2fa78f10371504e5fa42f1bc074df432bc8e125247e38"
)

message(STATUS "Semantic 4A54/4A4C initializers cover both fixed Side-Star layouts.")
