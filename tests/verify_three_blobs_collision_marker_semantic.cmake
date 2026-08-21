# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported Three-Blobs collision-marker configuration: ${CONFIG}")
endif()

set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(assembly_census "${OUTPUT}.assembly.csv")
set(assembly_writes "${assembly_census}.writes.csv")
set(assembly_calls "${assembly_census}.calls.csv")
set(assembly_call_writes "${assembly_calls}.writes.csv")
set(native_census "${OUTPUT}.native.csv")
set(native_writes "${native_census}.writes.csv")
set(native_calls "${native_census}.calls.csv")
set(native_call_writes "${native_calls}.writes.csv")

function(record_route census writes calls)
    execute_process(
        COMMAND "${APP}"
            --record-state-census "${census}"
            --observe-hook "03:694A"
            --routine-call-trace "${calls}"
            --work-ram-write-range 2100-21FF
            --work-ram-write-range 3980-39FF
            --work-ram-write-trace "${writes}"
            ${ARGN}
            --rom "${ROM}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR
            "Three-Blobs collision-marker recorder failed (${result}).\n${out}${err}"
        )
    endif()
    set(recorder_output "${out}" PARENT_SCOPE)
endfunction()

record_route("${assembly_census}" "${assembly_writes}" "${assembly_calls}")
set(assembly_output "${recorder_output}")
record_route(
    "${native_census}"
    "${native_writes}"
    "${native_calls}"
    --replace-hook "03:694A@all"
)
set(native_output "${recorder_output}")

foreach(output IN ITEMS assembly_output native_output)
    string(FIND "${${output}}" "Observe hook 1 entries: 198" observe_offset)
    if(observe_offset EQUAL -1)
        message(FATAL_ERROR
            "Three-Blobs collision-marker observer did not close 198 calls.\n${${output}}"
        )
    endif()
    string(FIND "${${output}}" "Routine call trace complete: 198 calls, 337 writes" call_offset)
    if(call_offset EQUAL -1)
        message(FATAL_ERROR
            "Three-Blobs collision-marker call artifact changed.\n${${output}}"
        )
    endif()
endforeach()

string(FIND
    "${native_output}"
    "Native replacement 03:694A@all entries: 198 of 198"
    native_offset
)
if(native_offset EQUAL -1)
    message(FATAL_ERROR
        "Three-Blobs collision-marker replacement did not admit all calls.\n${native_output}"
    )
endif()

foreach(artifact IN ITEMS census write call call_write)
    if(artifact STREQUAL "census")
        set(assembly_path "${assembly_census}")
        set(native_path "${native_census}")
    elseif(artifact STREQUAL "write")
        set(assembly_path "${assembly_writes}")
        set(native_path "${native_writes}")
    elseif(artifact STREQUAL "call")
        set(assembly_path "${assembly_calls}")
        set(native_path "${native_calls}")
    else()
        set(assembly_path "${assembly_call_writes}")
        set(native_path "${native_call_writes}")
    endif()
    file(SHA256 "${assembly_path}" assembly_sha256)
    file(SHA256 "${native_path}" native_sha256)
    if(NOT assembly_sha256 STREQUAL native_sha256)
        message(FATAL_ERROR
            "Three-Blobs collision-marker ${artifact} differs from assembly.\n"
            "Assembly: ${assembly_sha256}\nNative:   ${native_sha256}"
        )
    endif()
endforeach()

file(SHA256 "${native_census}" census_sha256)
if(NOT census_sha256 STREQUAL expected_census_sha256)
    message(FATAL_ERROR "Three-Blobs collision-marker census changed: ${census_sha256}")
endif()

message(STATUS
    "Three-Blobs collision marker preserves all 198 calls and 337 ordered writes."
)
