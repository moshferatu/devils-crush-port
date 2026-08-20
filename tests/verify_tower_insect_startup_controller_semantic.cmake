# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Tower-insect startup-controller regression is Release-only under the native-port cadence.")
endif()

# Four dynamic Tower insects begin at 02:5FDB with coroutine countdown 16.
# Every tick toggles direction bit $3080,X.5 and composes C018/C5C9. The
# expiry installs 02:5FE6, whose one-time collision setup composes the same
# scheduler to install the existing 02:5FFB collision controller. The scoped
# call artifact includes all stack, scheduler scratch, countdown, dispatch,
# collision, and interrupt-owned writes made by these two entries.
set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_call_sha256 "e22cb671f237cffc58374b00759f2a5440ad882b788f21ae200071e379c0cb5f")
set(expected_write_sha256 "6b924a26c4eada468c6e23229e54e13fad955380a1eb396dc4edee6e68512b89")

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "02:5FDB"
        --replace-hook "02:5FDB@all"
        --routine-replay "02:5FDB@1"
        --observe-hook "02:5FE6"
        --replace-hook "02:5FE6@all"
        --routine-replay "02:5FE6@1"
        --routine-call-trace "${call_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Tower-insect startup-controller recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 02:5FDB@all entries: 64 of 64"
    "Native replacement 02:5FE6@all entries: 4 of 4"
    "Observe hook 1 entries: 64"
    "Observe hook 2 entries: 4"
    "Routine call trace complete: 68 calls, 481 writes"
    "Routine replay 02:5FDB@1 verified: frame 941, 171 body cycles, 6 ordered writes"
    "Routine replay 02:5FE6@1 verified: frame 957, 270 body cycles, 10 ordered writes"
    "Routine replay complete: 2 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Tower-insect startup-controller output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Tower-insect startup-controller ${artifact} changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}\n")
    endif()
endforeach()

message(STATUS "Tower-insect startup controller verified all 68 native countdown and setup dispatches against assembly.")
