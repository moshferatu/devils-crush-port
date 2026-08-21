# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Text/tile writer regression is Release-only under the native-port cadence.")
endif()

# 08:5B01 is the bounded hardware-facing child of the oversized 08:5ABF text
# decoder. Each call selects MAWR, writes $2D:$2E, selects VWR, translates one
# space/digit/letter to the game's tile word, writes it, and advances $2D:$2E.
# The bus artifact is deliberately global: matching it proves the native VDC
# stores have the same values, order, instruction identities and cycle offsets
# without perturbing any other video traffic on the route.
set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_call_sha256 "a98aaf960558c2761cd68c286fc097a41cb653d630ee77ab221223ebbb9e6885")
set(expected_call_write_sha256 "d9d4e2fcb0ff485a28d2827b79c1554dbb4ddf5ed9e12ecb90ba54e9d9100838")
set(expected_work_sha256 "0afe66bdc0e2922b71d8b32147919cfbd8bdd51f0d5b9085d99860d1964f66f2")
set(expected_bus_sha256 "1221e1687b256ca6621bd2088de29a1ceed4c0ad0d05d7c75d9107df825d7005")

set(corpus_directory "${OUTPUT}.corpus")
set(corpus "${corpus_directory}/08-5B01-38.dcrr")
if(NOT EXISTS "${corpus}")
    execute_process(
        COMMAND
            "${APP}"
            --record-state-census "${OUTPUT}.assembly-capture.csv"
            --observe-hook "08:5B01"
            --routine-call-trace "${OUTPUT}.assembly-capture.calls.csv"
            --routine-replay "08:5B01@38"
            --routine-replay-corpus-out "${corpus_directory}"
            --rom "${ROM}"
        RESULT_VARIABLE capture_result
        OUTPUT_VARIABLE capture_output
        ERROR_VARIABLE capture_error
    )
    if(NOT capture_result EQUAL 0)
        message(FATAL_ERROR "Text/tile writer corpus capture failed (${capture_result}).\n${capture_output}${capture_error}")
    endif()
    string(FIND "${capture_output}" "Routine replay 08:5B01@38 verified: frame 7, 294 body cycles, 4 ordered writes" capture_offset)
    if(capture_offset EQUAL -1)
        message(FATAL_ERROR "Text/tile writer capture omitted the expected fixture.\n${capture_output}${capture_error}")
    endif()
endif()

execute_process(
    COMMAND
        "${APP}"
        --routine-replay-corpus "${corpus}"
        --replace-hook "08:5B01@all"
        --rom "${ROM}"
    RESULT_VARIABLE replay_result
    OUTPUT_VARIABLE replay_output
    ERROR_VARIABLE replay_error
)
if(NOT replay_result EQUAL 0)
    message(FATAL_ERROR "Text/tile writer direct replay failed (${replay_result}).\n${replay_output}${replay_error}")
endif()
string(FIND "${replay_output}" "Direct routine replay 08:5B01@38 verified: frame 7, 294 body cycles, 4 ordered writes, native" replay_offset)
if(replay_offset EQUAL -1)
    message(FATAL_ERROR "Text/tile writer direct replay omitted its native proof.\n${replay_output}${replay_error}")
endif()

set(call_output "${OUTPUT}.calls.csv")
set(call_write_output "${call_output}.writes.csv")
set(work_output "${OUTPUT}.work.csv")
set(bus_output "${OUTPUT}.bus.csv")
execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "08:5B01"
        --replace-hook "08:5B01@all"
        --routine-call-trace "${call_output}"
        --work-ram-write-range "2006-2006"
        --work-ram-write-range "202B-202E"
        --work-ram-write-trace "${work_output}"
        --bus-write-range "0000-0003"
        --bus-write-trace "${bus_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic text/tile writer recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 08:5B01@all entries: 283 of 283"
    "Observe hook 1 entries: 283"
    "Routine call trace complete: 283 calls, 1325 writes"
    "Work-RAM write trace complete: 868259 selected writes"
    "Bus write trace complete: 2036234 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic text/tile writer output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
    endif()
endforeach()

foreach(artifact IN ITEMS census call call_write work bus)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    elseif(artifact STREQUAL "call_write")
        set(artifact_path "${call_write_output}")
    elseif(artifact STREQUAL "work")
        set(artifact_path "${work_output}")
    else()
        set(artifact_path "${bus_output}")
    endif()
    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(FATAL_ERROR "Semantic text/tile writer ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}\n")
    endif()
endforeach()

message(STATUS "Semantic 08:5B01 text/tile writer runs all 283 calls natively with exact call, RAM, VDC-bus and state parity.")
