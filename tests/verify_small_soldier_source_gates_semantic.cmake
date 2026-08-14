# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(CONFIG STREQUAL "Debug")
    set(expected_census_sha256 "a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2")
elseif(CONFIG STREQUAL "Release")
    set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
else()
    message(FATAL_ERROR "Unsupported Small-Soldier source-gate configuration: ${CONFIG}")
endif()

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(expected_call_sha256 "2d12f53bdf602096a343235dc1920f8973169130e1c6d0d33c8868aa84bd30d4")
set(expected_write_sha256 "e9918a499097dea8cc28d1315f6e25d6adf6012ece1914b236d26ff33451993f")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "03:7D85"
        --observe-hook "03:7E23"
        --replace-hook "03:7D85@all"
        --replace-hook "03:7E23@all"
        --routine-call-trace "${call_output}"
        --routine-replay "03:7D85@1"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Small-Soldier source-gate recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 03:7D85@all entries: 2457 of 2457"
    "Native replacement 03:7E23@all entries: 2457 of 2457"
    "Observe hook 1 entries: 2457"
    "Observe hook 2 entries: 2457"
    "Routine replay 03:7D85@1 verified: frame 1133, 21 body cycles, 0 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Small-Soldier source-gate output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
    endif()
endforeach()

file(SHA256 "${OUTPUT}" actual_census_sha256)
if(NOT actual_census_sha256 STREQUAL expected_census_sha256)
    message(FATAL_ERROR "Semantic Small-Soldier source-gate census changed.\nExpected: ${expected_census_sha256}\nActual:   ${actual_census_sha256}\n")
endif()

foreach(artifact IN ITEMS call write)
    if(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    else()
        set(artifact_path "${write_output}")
    endif()
    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(FATAL_ERROR "Semantic Small-Soldier source-gate ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}\n")
    endif()
endforeach()

message(STATUS "Semantic 7D85/7E23 Small-Soldier source gates cover all 4,914 waiting calls with checkpointed interrupts.")
