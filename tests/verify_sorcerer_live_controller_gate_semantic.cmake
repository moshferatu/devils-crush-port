# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Sorcerer live-controller gate is Release-only under the native-port cadence.")
endif()

set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_call_sha256 "33c16040609dbe3c4c22f37a9cf7efa1c3fbd485f93d11e36224f3dbaa021231")
set(expected_write_sha256 "d432cc5f932791ec2ef75f62950174dcbf024452f7b93d7d1fc9483f99f114e0")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "02:57AD"
        --replace-hook "02:57AD@all"
        --routine-call-trace "${call_output}"
        --routine-replay "02:57AD@1"
        --rom "${ROM}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Semantic Sorcerer live-controller recorder failed (${result}).\n${out}${err}")
endif()
foreach(line IN ITEMS
    "Native replacement 02:57AD@all entries: 2648 of 2648"
    "Observe hook 1 entries: 2648"
    "Routine call trace complete: 2648 calls, 812 writes"
    "Routine replay 02:57AD@1 verified: frame 942, 69 body cycles, 0 ordered writes"
)
    string(FIND "${out}" "${line}" offset)
    if(offset EQUAL -1)
        message(FATAL_ERROR "Semantic Sorcerer live-controller output omitted '${line}'.\n${out}${err}")
    endif()
endforeach()
foreach(kind IN ITEMS census call write)
    if(kind STREQUAL "census")
        set(path "${OUTPUT}")
    elseif(kind STREQUAL "call")
        set(path "${call_output}")
    else()
        set(path "${write_output}")
    endif()
    file(SHA256 "${path}" actual)
    if(NOT actual STREQUAL "${expected_${kind}_sha256}")
        message(FATAL_ERROR "Semantic Sorcerer live-controller ${kind} artifact changed.")
    endif()
endforeach()
message(STATUS "Semantic 57AD live-controller gates preserve all 2,648 natural Sorcerer dispatches.")
