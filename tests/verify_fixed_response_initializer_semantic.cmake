# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported fixed response initializer configuration: ${CONFIG}")
endif()

set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_call_sha256 "0ef74e66fde69dd7d56af18ed7f43f11f94509f02138af8ea5bac41d10c47144")
set(expected_write_sha256 "4b6abcb28f8f513558ccf89826230c50b9ca41af7f9401bbabca106339b08f7b")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "03:7CA7"
        --replace-hook "03:7CA7@all"
        --routine-call-trace "${call_output}"
        --routine-replay "03:7CA7@1"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic fixed response initializer recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()
foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 03:7CA7@all entries: 1 of 1"
    "Observe hook 1 entries: 1"
    "Routine call trace complete: 1 calls, 9 writes"
    "Routine replay 03:7CA7@1 verified: frame 872, 219 body cycles, 9 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic fixed response initializer output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
    endif()
endforeach()
foreach(artifact IN ITEMS census call write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
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
        message(FATAL_ERROR "Semantic fixed response initializer ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()
message(STATUS "Semantic 7CA7 initializer covers the fixed response setup.")
