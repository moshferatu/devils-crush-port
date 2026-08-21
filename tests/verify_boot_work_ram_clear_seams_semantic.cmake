# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Boot work-RAM clear regression is Release-only under the native-port cadence.")
endif()

# 08:584C surrounds a 4,095-write TII with two calls, three clear-setup stores,
# and a short VDC setup tail. The transfer itself exceeds the replacement
# channel and has no interior instruction boundary, so four bounded seams cross
# both calls, stop in front of the TII, resume after it, and absorb the final
# return. The outer call artifact spans the fade driver's full 2.94M cycles and
# proves that splitting around the capacity wall moves neither work nor time.
set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_call_sha256 "ebce77fdb210ce7ac0b47fbe5fa2a57a52e2e92115f798a66fb61950dbd24389")
set(expected_call_write_sha256 "dd5ad31096a8cd530b3f610da2a3cb59a0694b8a38a3cadd68b4f7ae713764e6")
set(expected_work_sha256 "4ddc6a93f79e85f73b11d6951d41b72a520d09f05e3c174c7362273b0d719a57")
set(expected_bus_sha256 "1221e1687b256ca6621bd2088de29a1ceed4c0ad0d05d7c75d9107df825d7005")

set(corpus_directory "${OUTPUT}.corpus")
set(corpus "${corpus_directory}/08-5861-1.dcrr")
if(NOT EXISTS "${corpus}")
    execute_process(
        COMMAND
            "${APP}"
            --record-state-census "${OUTPUT}.assembly-capture.csv"
            --observe-hook "08:5861"
            --routine-call-trace "${OUTPUT}.assembly-capture.calls.csv"
            --routine-replay "08:5861@1"
            --routine-replay-corpus-out "${corpus_directory}"
            --rom "${ROM}"
        RESULT_VARIABLE capture_result
        OUTPUT_VARIABLE capture_output
        ERROR_VARIABLE capture_error
    )
    if(NOT capture_result EQUAL 0)
        message(FATAL_ERROR "Boot work-RAM clear corpus capture failed (${capture_result}).\n${capture_output}${capture_error}")
    endif()
    string(FIND "${capture_output}" "Routine replay 08:5861@1 verified: frame 858, 78 body cycles, 1 ordered writes" capture_offset)
    if(capture_offset EQUAL -1)
        message(FATAL_ERROR "Boot work-RAM clear capture omitted the expected tail fixture.\n${capture_output}${capture_error}")
    endif()
endif()

execute_process(
    COMMAND
        "${APP}"
        --routine-replay-corpus "${corpus}"
        --replace-hook "08:5861@all"
        --rom "${ROM}"
    RESULT_VARIABLE replay_result
    OUTPUT_VARIABLE replay_output
    ERROR_VARIABLE replay_error
)
if(NOT replay_result EQUAL 0)
    message(FATAL_ERROR "Boot work-RAM clear tail replay failed (${replay_result}).\n${replay_output}${replay_error}")
endif()
string(FIND "${replay_output}" "Direct routine replay 08:5861@1 verified: frame 858, 78 body cycles, 1 ordered writes, native" replay_offset)
if(replay_offset EQUAL -1)
    message(FATAL_ERROR "Boot work-RAM clear direct replay omitted its native proof.\n${replay_output}${replay_error}")
endif()

set(call_output "${OUTPUT}.calls.csv")
set(call_write_output "${call_output}.writes.csv")
set(work_output "${OUTPUT}.work.csv")
set(bus_output "${OUTPUT}.bus.csv")
set(abandonment_output "${OUTPUT}.abandonments.csv")
execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "08:584C"
        --replace-hook "08:584C@all"
        --replace-hook "08:584F@all"
        --replace-hook "08:5852@all"
        --replace-hook "08:5861@all"
        --routine-call-trace "${call_output}"
        --work-ram-write-range "2054-2054"
        --work-ram-write-range "2D53-2D53"
        --work-ram-write-range "3000-3FFF"
        --work-ram-write-trace "${work_output}"
        --bus-write-range "0000-0003"
        --bus-write-trace "${bus_output}"
        --abandonment-census "${abandonment_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic boot work-RAM clear recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 08:584C@all entries: 1 of 1"
    "Native replacement 08:584F@all entries: 1 of 1"
    "Native replacement 08:5852@all entries: 1 of 1"
    "Native replacement 08:5861@all entries: 1 of 1"
    "Abandonment census complete: no transcript was abandoned"
    "Observe hook 1 entries: 1"
    "Routine call trace complete: 1 calls, 49993 writes"
    "Work-RAM write trace complete: 444555 selected writes"
    "Bus write trace complete: 2036234 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic boot work-RAM clear output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Semantic boot work-RAM clear ${artifact} artifact changed.\nExpected: ${expected_sha256}\nActual:   ${actual_sha256}\n")
    endif()
endforeach()

message(STATUS "Semantic 08:584C/584F/5852/5861 seams preserve the 4,095-write TII wall and run all bounded surrounding work natively with exact call, RAM, VDC-bus and state parity.")
