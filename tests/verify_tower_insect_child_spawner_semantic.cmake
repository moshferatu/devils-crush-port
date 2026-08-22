# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release" AND NOT CONFIG STREQUAL "Debug")
    message(FATAL_ERROR "Unsupported Tower-insect child-spawner configuration: ${CONFIG}")
endif()

# These four direct BSR calls cross independently-native allocator and camera
# helpers. They are internal seams of the active source-controller scope, so
# the whole-route call/write artifact is the shadow oracle.
set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_call_sha256 "bc0e561221bb2343b409ac7fcf5977048f5c443b66770237c62632370ca6ca2c")
set(expected_write_sha256 "f5bfe69ec6df029e5982bdab995ff5a6ea0d4c4090dcde34d709a22ac3f18925")

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "02:5F78"
        --observe-hook "02:5F7B"
        --observe-hook "02:5F7E"
        --observe-hook "02:5FD6"
        --replace-hook "02:5F78@all"
        --replace-hook "02:5F7B@all"
        --replace-hook "02:5F7E@all"
        --replace-hook "02:5FD6@all"
        --routine-call-trace "${call_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Tower-insect child-spawner recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 02:5F78@all entries: 4 of 4"
    "Native replacement 02:5F7B@all entries: 4 of 4"
    "Native replacement 02:5F7E@all entries: 4 of 4"
    "Native replacement 02:5FD6@all entries: 4 of 4"
    "Observe hook 1 entries: 4"
    "Observe hook 2 entries: 4"
    "Observe hook 3 entries: 4"
    "Observe hook 4 entries: 4"
    "Routine call trace complete: 16 calls, 2530 writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Tower-insect child-spawner output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Semantic Tower-insect child-spawner ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}\n")
    endif()
endforeach()

message(STATUS "Tower-insect child spawner verified: four allocator/camera seams and 2,530 ordered writes match assembly.")
