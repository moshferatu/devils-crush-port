# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported Undead-Head initializer configuration: ${CONFIG}")
endif()

set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_call_sha256 "d8082dd00e3e09e2a833d13566b6b30aa2bd74af5c5cde99f134f53b2736924d")
set(expected_write_sha256 "9f6510b19eba11ea45f247cc0dd15563daa9c6ae0ff4e85d413542427d0bd032")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "03:61A0"
        --replace-hook "03:61A0@all"
        --routine-call-trace "${call_output}"
        --routine-replay "03:61A0@1"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Undead-Head initializer recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()
foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 03:61A0@all entries: 1 of 1"
    "Observe hook 1 entries: 1"
    "Routine call trace complete: 1 calls, 7 writes"
    "Routine replay 03:61A0@1 verified: frame 872, 192 body cycles, 7 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Undead-Head initializer output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Semantic Undead-Head initializer ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()
message(STATUS "Semantic 61A0 initializer covers fixed Undead-Head startup.")
