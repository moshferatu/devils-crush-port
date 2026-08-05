# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported Three-Blobs direct-continuation configuration: ${CONFIG}")
endif()

set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_call_sha256 "ad8dce16440b2c71f93c4a5b5704a4e56554331c160545cb2d3e41aeb1a1f485")
set(expected_write_sha256 "427f3739f9365d1a01037ad7dbf51d4621e063ac7c9579f1b755a8a705914992")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "03:66DE"
        --replace-hook "03:66DE@all"
        --routine-call-trace "${call_output}"
        --routine-replay "03:66DE@1"
        --rom "${ROM}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Semantic Three-Blobs direct-continuation recorder failed (${result}).\n${out}${err}")
endif()
foreach(line IN ITEMS
    "Native replacement 03:66DE@all entries: 1034 of 1041"
    "Routine call trace complete: 1041 calls, 4560 writes"
    "Routine replay 03:66DE@1 verified: frame 1229, 189 body cycles, 3 ordered writes"
)
    string(FIND "${out}" "${line}" offset)
    if(offset EQUAL -1)
        message(FATAL_ERROR "Semantic Three-Blobs direct-continuation output omitted '${line}'.\n${out}${err}")
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
        message(FATAL_ERROR "Semantic Three-Blobs direct-continuation ${kind} artifact changed.")
    endif()
endforeach()
message(STATUS "Semantic 66DE direct continuation preserves its composed shared-motion leaf.")
