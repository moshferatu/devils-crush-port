# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Tower-insect source-gate regression is Release-only under the native-port cadence.")
endif()

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_call_sha256 "42cc1b1b9a2b2137ec4cee44c065e3d7ce634ed8ada0023a85b04f8cfb95442a")
set(expected_write_sha256 "b74c45c28b0033d6bbedac7610f980b64240053be0341c3dbee663fbea111235")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "02:5F22"
        --observe-hook "03:60B3"
        --observe-hook "02:5F5F"
        --replace-hook "02:5F22@all"
        --replace-hook "03:60B3@all"
        --replace-hook "02:5F5F@all"
        --routine-call-trace "${call_output}"
        --routine-replay "02:5F22@1"
        --routine-replay "03:60B3@1"
        --routine-replay "02:5F5F@1"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Tower-insect source gate recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 02:5F22@all entries: 1 of 1"
    "Native replacement 03:60B3@all entries: 1 of 1"
    "Native replacement 02:5F5F@all entries: 2504 of 2504"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 1"
    "Observe hook 3 entries: 2504"
    "Routine replay 02:5F22@1 verified: frame 872, 192 body cycles, 7 ordered writes"
    "Routine replay 03:60B3@1 verified: frame 872, 249 body cycles, 10 ordered writes"
    "Routine replay 02:5F5F@1 verified: frame 1086, 21 body cycles, 0 ordered writes"
    "Routine replay complete: 3 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Tower-insect source gate output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
    endif()
endforeach()

foreach(artifact IN ITEMS census call write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    else()
        set(artifact_path "${write_output}")
    endif()
    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(FATAL_ERROR "Semantic Tower-insect source gate ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual: ${actual_sha256}\n")
    endif()
endforeach()

message(STATUS "Semantic 5F5F Tower-insect source gate covers all 2,504 observed child-count waits.")
