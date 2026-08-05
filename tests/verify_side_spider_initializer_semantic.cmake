# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported Side-Spider initializer configuration: ${CONFIG}")
endif()

set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_call_sha256 "dac356687afb878960c97fa67601e2aa2f254c3356fcc6b55bb29381e8857044")
set(expected_write_sha256 "59d8fac96a7b7553098ae1330504ea1b8e44dc29db7158a7886b88a37e233d2c")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "03:64FA"
        --replace-hook "03:64FA@all"
        --routine-call-trace "${call_output}"
        --routine-replay "03:64FA@1"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Side-Spider initializer recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()
foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 03:64FA@all entries: 2 of 2"
    "Observe hook 1 entries: 2"
    "Routine call trace complete: 2 calls, 28 writes"
    "Routine replay 03:64FA@1 verified: frame 872, 342 body cycles, 14 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Side-Spider initializer output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Semantic Side-Spider initializer ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()
message(STATUS "Semantic 64FA initializer covers both fixed Side-Spider layouts.")
