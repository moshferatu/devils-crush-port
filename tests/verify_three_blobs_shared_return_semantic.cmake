# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported Three-Blobs shared-return configuration: ${CONFIG}")
endif()

set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(assembly_census "${OUTPUT}.assembly.csv")
set(native_census "${OUTPUT}.native.csv")

function(record_route census suffix)
    set(calls "${OUTPUT}.${suffix}.calls.csv")
    set(writes "${OUTPUT}.${suffix}.writes.csv")
    execute_process(
        COMMAND "${APP}"
            --record-state-census "${census}"
            --observe-hook "03:67E5"
            --routine-call-trace "${calls}"
            --work-ram-write-range 2100-21FF
            --work-ram-write-range 3000-3FFF
            --work-ram-write-trace "${writes}"
            ${ARGN}
            --rom "${ROM}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Three-Blobs shared-return recorder failed (${result}).\n${out}${err}")
    endif()
    set(recorder_output "${out}" PARENT_SCOPE)
    set(write_path "${writes}" PARENT_SCOPE)
endfunction()

record_route("${assembly_census}" assembly)
set(assembly_output "${recorder_output}")
set(assembly_writes "${write_path}")

record_route(
    "${native_census}"
    native
    --replace-hook "03:67E5@all"
    --routine-replay "03:67E5@1"
    --routine-replay "03:67E5@1965"
)
set(native_output "${recorder_output}")
set(native_writes "${write_path}")

foreach(output IN ITEMS assembly_output native_output)
    foreach(expected_line IN ITEMS
        "Observe hook 1 entries: 1965"
        "Work-RAM write trace complete: 4650247 selected writes"
    )
        string(FIND "${${output}}" "${expected_line}" line_offset)
        if(line_offset EQUAL -1)
            message(FATAL_ERROR "Three-Blobs shared-return output omitted '${expected_line}'.\n${${output}}")
        endif()
    endforeach()
endforeach()

foreach(expected_line IN ITEMS
    "Native replacement 03:67E5@all entries: 1965 of 1965"
    "Routine replay 03:67E5@1 verified: frame 1229, 0 body cycles, 0 ordered writes"
    "Routine replay 03:67E5@1965 verified: frame 3600, 0 body cycles, 0 ordered writes"
    "Routine replay complete: 2 state-normalized calls"
)
    string(FIND "${native_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Three-Blobs shared-return replacement omitted '${expected_line}'.\n${native_output}")
    endif()
endforeach()

foreach(artifact IN ITEMS census writes)
    if(artifact STREQUAL "census")
        set(assembly_path "${assembly_census}")
        set(native_path "${native_census}")
    else()
        set(assembly_path "${assembly_writes}")
        set(native_path "${native_writes}")
    endif()
    file(SHA256 "${assembly_path}" assembly_sha256)
    file(SHA256 "${native_path}" native_sha256)
    if(NOT assembly_sha256 STREQUAL native_sha256)
        message(FATAL_ERROR "Three-Blobs shared-return ${artifact} differs from assembly.\nAssembly: ${assembly_sha256}\nNative:   ${native_sha256}")
    endif()
endforeach()

file(SHA256 "${native_census}" census_sha256)
if(NOT census_sha256 STREQUAL expected_census_sha256)
    message(FATAL_ERROR "Three-Blobs shared-return census changed: ${census_sha256}")
endif()

message(STATUS "Three-Blobs shared return preserves all 1,965 RTS scopes.")
