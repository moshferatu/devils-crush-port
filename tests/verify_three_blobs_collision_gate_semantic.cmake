# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported Three-Blobs collision-gate configuration: ${CONFIG}")
endif()

set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_call_sha256 "e028eeff47cecc01283850cead1ee23ae98d8a5d81240923e63e2e633c349c8d")
set(expected_write_sha256 "e414b0dfc26d2a56195e1e48f18cb400404839226f26aea4396a547efb061185")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "03:67C0"
        --replace-hook "03:67C0@all"
        --routine-call-trace "${call_output}"
        --routine-replay "03:67C0@1"
        --rom "${ROM}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Semantic Three-Blobs collision-gate recorder failed (${result}).\n${out}${err}")
endif()
foreach(line IN ITEMS
    "Native replacement 03:67C0@all entries: 1965 of 1965"
    "Routine call trace complete: 1965 calls, 1405 writes"
    "Routine replay 03:67C0@1 verified: frame 1229, 27 body cycles, 0 ordered writes"
)
    string(FIND "${out}" "${line}" offset)
    if(offset EQUAL -1)
        message(FATAL_ERROR "Semantic Three-Blobs collision-gate output omitted '${line}'.\n${out}${err}")
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
        message(FATAL_ERROR "Semantic Three-Blobs collision-gate ${kind} artifact changed.")
    endif()
endforeach()
message(STATUS "Semantic 67C0 Three-Blobs collision gate preserves no-hit returns and hands hit prefixes to original audio handling.")
