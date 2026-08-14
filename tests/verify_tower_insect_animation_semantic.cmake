# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Tower-insect animation regression is Release-only under the native-port cadence.")
endif()

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_call_sha256 "6c9e9f5343cfca24a07371c0b6696b060201ead67d23bd716c34e8c836158096")
set(expected_write_sha256 "013bcb36825aeb972e5d31d8a3b832578799124686721294a12726bbea97dbb0")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "03:6054"
        --replace-hook "03:6054@all"
        --routine-call-trace "${call_output}"
        --routine-replay "03:6054@1"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Tower-insect animation recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 03:6054@all entries: 6750 of 6750"
    "Observe hook 1 entries: 6750"
    "Routine replay 03:6054@1 verified: frame 958, 147 body cycles, 3 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Tower-insect animation output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Semantic Tower-insect animation ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}\n")
    endif()
endforeach()

message(STATUS "Semantic 6054 Tower-insect animation covers all 6,750 dynamic child entries.")
