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
set(assembly_census "${OUTPUT}.assembly.csv")
set(assembly_calls "${assembly_census}.calls.csv")
set(assembly_writes "${assembly_calls}.writes.csv")
set(native_census "${OUTPUT}.native.csv")
set(native_calls "${native_census}.calls.csv")
set(native_writes "${native_calls}.writes.csv")

function(record_route census calls)
    execute_process(
        COMMAND "${APP}"
            --record-state-census "${census}"
            --observe-hook "03:67C0"
            --observe-hook "03:67CF"
            --routine-call-trace "${calls}"
            ${ARGN}
            --rom "${ROM}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Semantic Three-Blobs collision-family recorder failed (${result}).\n${out}${err}")
    endif()
    set(recorder_output "${out}" PARENT_SCOPE)
endfunction()

record_route("${assembly_census}" "${assembly_calls}")
set(assembly_output "${recorder_output}")
record_route(
    "${native_census}"
    "${native_calls}"
    --replace-hook "03:67C0@all"
    --replace-hook "03:67CF@all"
    --routine-replay "03:67C0@1"
    --routine-replay "03:67CF@15"
)
set(native_output "${recorder_output}")

foreach(output IN ITEMS assembly_output native_output)
    foreach(line IN ITEMS
        "Observe hook 1 entries: 1965"
        "Observe hook 2 entries: 15"
        "Routine call trace complete: 1980 calls, 1435 writes"
    )
        string(FIND "${${output}}" "${line}" offset)
        if(offset EQUAL -1)
            message(FATAL_ERROR "Semantic Three-Blobs collision-family output omitted '${line}'.\n${${output}}")
        endif()
    endforeach()
endforeach()
foreach(line IN ITEMS
    "Native replacement 03:67C0@all entries: 1965 of 1965"
    "Native replacement 03:67CF@all entries: 15 of 15"
    "Routine replay 03:67C0@1 verified: frame 1229, 27 body cycles, 0 ordered writes"
    "Routine replay 03:67CF@15 verified: frame 1609, 105 body cycles, 2 ordered writes"
)
    string(FIND "${native_output}" "${line}" offset)
    if(offset EQUAL -1)
        message(FATAL_ERROR "Semantic Three-Blobs collision-family output omitted '${line}'.\n${native_output}")
    endif()
endforeach()

foreach(kind IN ITEMS census call write)
    if(kind STREQUAL "census")
        set(assembly_path "${assembly_census}")
        set(native_path "${native_census}")
    elseif(kind STREQUAL "call")
        set(assembly_path "${assembly_calls}")
        set(native_path "${native_calls}")
    else()
        set(assembly_path "${assembly_writes}")
        set(native_path "${native_writes}")
    endif()
    file(SHA256 "${assembly_path}" assembly_sha256)
    file(SHA256 "${native_path}" native_sha256)
    if(NOT assembly_sha256 STREQUAL native_sha256)
        message(FATAL_ERROR
            "Semantic Three-Blobs collision-family ${kind} differs from assembly.\n"
            "Assembly: ${assembly_sha256}\nNative: ${native_sha256}"
        )
    endif()
endforeach()
file(SHA256 "${native_census}" census_sha256)
if(NOT census_sha256 STREQUAL expected_census_sha256)
    message(FATAL_ERROR "Semantic Three-Blobs collision-family census changed: ${census_sha256}")
endif()
message(STATUS "Semantic 67C0/67CF Three-Blobs collision family preserves every no-hit and post-audio colour/appearance call.")
