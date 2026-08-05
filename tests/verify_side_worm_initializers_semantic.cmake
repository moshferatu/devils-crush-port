# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported Side-Worm initializer configuration: ${CONFIG}")
endif()

set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_left_call_sha256 "2b49b12a188524ed625ed66f579d350d7ac845d2d57de4077a36b6ec5c62815e")
set(expected_left_write_sha256 "b6595c2b4cf47f9af11a35b49aabc1738b6711fe7c6cb5d546b01aea08a35bc2")
set(expected_right_call_sha256 "0247b4b9a993263169b1914321717794792b08dd6999bccd7c2dfabc7d7bce2b")
set(expected_right_write_sha256 "128cacf8ad608a5a9ebf5136fd45f0d4f5074d39ea5eb80ffe8dd5f752dab900")

foreach(side IN ITEMS left right)
    if(side STREQUAL "left")
        set(routine "02:5C4B")
        set(cycles "204")
        set(expected_call_sha256 "${expected_left_call_sha256}")
        set(expected_write_sha256 "${expected_left_write_sha256}")
    else()
        set(routine "02:5C7C")
        set(cycles "210")
        set(expected_call_sha256 "${expected_right_call_sha256}")
        set(expected_write_sha256 "${expected_right_write_sha256}")
    endif()
    set(census_output "${OUTPUT}.${side}.csv")
    set(call_output "${census_output}.calls.csv")
    set(write_output "${call_output}.writes.csv")
    execute_process(
        COMMAND "${APP}"
            --record-state-census "${census_output}"
            --observe-hook "${routine}"
            --replace-hook "${routine}@all"
            --routine-call-trace "${call_output}"
            --routine-replay "${routine}@1"
            --rom "${ROM}"
        RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error
    )
    if(NOT recorder_result EQUAL 0)
        message(FATAL_ERROR "Semantic ${side} Side-Worm initializer recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
    endif()
    foreach(expected_line IN ITEMS
        "State census recording complete: 3600 frames"
        "Native replacement ${routine}@all entries: 1 of 1"
        "Observe hook 1 entries: 1"
        "Routine call trace complete: 1 calls, 10 writes"
        "Routine replay ${routine}@1 verified: frame 872, ${cycles} body cycles, 10 ordered writes"
        "Routine replay complete: 1 state-normalized calls"
    )
        string(FIND "${recorder_output}" "${expected_line}" line_offset)
        if(line_offset EQUAL -1)
            message(FATAL_ERROR "Semantic ${side} Side-Worm initializer output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
            message(FATAL_ERROR "Semantic ${side} Side-Worm initializer ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}")
        endif()
    endforeach()
endforeach()
message(STATUS "Semantic 5C4B/5C7C initializers cover the fixed Side-Worm startup pair.")
