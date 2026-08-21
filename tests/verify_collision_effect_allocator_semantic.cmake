# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported collision-effect allocator configuration: ${CONFIG}")
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
            --observe-hook "03:7BF6"
            --observe-hook "03:7C02"
            --observe-hook "03:7C05"
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
        message(FATAL_ERROR
            "Collision-effect allocator recorder failed (${result}).\n${out}${err}"
        )
    endif()
    set(recorder_output "${out}" PARENT_SCOPE)
    set(call_path "${calls}" PARENT_SCOPE)
    set(write_path "${writes}" PARENT_SCOPE)
endfunction()

record_route("${assembly_census}" assembly)
set(assembly_output "${recorder_output}")
set(assembly_calls "${call_path}")
set(assembly_writes "${write_path}")
set(assembly_call_writes "${assembly_calls}.writes.csv")

record_route(
    "${native_census}"
    native
    --replace-hook "03:7BF6@all"
    --replace-hook "03:7C02@all"
    --replace-hook "03:7C05@all"
    --routine-replay "03:7BF6@8"
    --routine-replay "03:7C05@19"
)
set(native_output "${recorder_output}")
set(native_calls "${call_path}")
set(native_writes "${write_path}")
set(native_call_writes "${native_calls}.writes.csv")

foreach(output IN ITEMS assembly_output native_output)
    foreach(expected_line IN ITEMS
        "Observe hook 1 entries: 20"
        "Observe hook 2 entries: 19"
        "Observe hook 3 entries: 19"
        "Routine call trace complete: 58 calls, 1022 writes"
        "Work-RAM write trace complete: 4650247 selected writes"
    )
        string(FIND "${${output}}" "${expected_line}" line_offset)
        if(line_offset EQUAL -1)
            message(FATAL_ERROR
                "Collision-effect allocator output omitted '${expected_line}'.\n${${output}}"
            )
        endif()
    endforeach()
endforeach()

foreach(expected_line IN ITEMS
    "Native replacement 03:7BF6@all entries: 20 of 20"
    "Native replacement 03:7C02@all entries: 19 of 19"
    "Native replacement 03:7C05@all entries: 19 of 19"
    "Routine replay 03:7BF6@8 verified: frame 2080, 39 body cycles, 0 ordered writes"
    "Routine replay 03:7C05@19 verified: frame 3342, 207 body cycles, 8 ordered writes"
    "Routine replay complete: 2 state-normalized calls"
)
    string(FIND "${native_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR
            "Collision-effect allocator replacement omitted '${expected_line}'.\n${native_output}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census calls call_writes writes)
    if(artifact STREQUAL "census")
        set(assembly_path "${assembly_census}")
        set(native_path "${native_census}")
    elseif(artifact STREQUAL "calls")
        set(assembly_path "${assembly_calls}")
        set(native_path "${native_calls}")
    elseif(artifact STREQUAL "call_writes")
        set(assembly_path "${assembly_call_writes}")
        set(native_path "${native_call_writes}")
    else()
        set(assembly_path "${assembly_writes}")
        set(native_path "${native_writes}")
    endif()
    file(SHA256 "${assembly_path}" assembly_sha256)
    file(SHA256 "${native_path}" native_sha256)
    if(NOT assembly_sha256 STREQUAL native_sha256)
        message(FATAL_ERROR
            "Collision-effect allocator ${artifact} differs from assembly.\n"
            "Assembly: ${assembly_sha256}\nNative:   ${native_sha256}"
        )
    endif()
endforeach()

file(SHA256 "${native_census}" census_sha256)
if(NOT census_sha256 STREQUAL expected_census_sha256)
    message(FATAL_ERROR "Collision-effect allocator census changed: ${census_sha256}")
endif()

message(STATUS
    "Collision-effect allocator preserves all 58 scopes and 1,022 ordered writes."
)
