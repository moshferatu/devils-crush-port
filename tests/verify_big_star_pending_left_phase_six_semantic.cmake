# SPDX-License-Identifier: GPL-3.0-or-later
foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Big-Star pending-left phase-six regression is Release-only.")
endif()
set(expected_census_sha256 "5923ee03453e728c82d7d9289f2e975ea900848142b80bfb6d4ffda6559826ad")
set(expected_trace_sha256 "2888472661040eedc5c53ab9e6c059801cb22d5cd0bc28ffba57146dcbc01fa6")
set(expected_call_sha256 "fc255663c49cb190d096e04209bc3648ff66f31e3ebffe3469a1ef99e623aa92")
set(expected_write_sha256 "cdaedba22eb308e66248e41aa3e38070543510663b5ea4d48c3dfe38d2fa037f")
set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
execute_process(
    COMMAND "${APP}" --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:334B:2C" --work-ram-patch "1200:33CB:01"
        --work-ram-patch "1201:2D04:01" --work-ram-patch "1201:3B4B:06"
        --observe-hook "02:570A" --observe-hook "02:5722"
        --replace-hook "02:5722@all"
        --routine-trace "${trace_output}" --routine-call-trace "${call_output}"
        --routine-replay "02:5722@1" --rom "${ROM}"
    RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Semantic Big-Star pending-left phase-six recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()
foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames" "Work-RAM patches applied: 4"
    "Native replacement 02:5722@all entries: 1 of 1"
    "Observe hook 1 entries: 374" "Observe hook 2 entries: 1"
    "Routine trace complete: 375 entries"
    "Routine call trace complete: 375 calls, 542 writes"
    "Routine replay 02:5722@1 verified: frame 1221, 252 body cycles, 6 ordered writes"
    "Routine replay complete: 1 state-normalized calls")
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Semantic Big-Star pending-left phase-six output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
    endif()
endforeach()
foreach(artifact IN ITEMS census trace call write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "trace")
        set(artifact_path "${trace_output}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    else()
        set(artifact_path "${write_output}")
    endif()
    file(SHA256 "${artifact_path}" actual_sha256)
    if(NOT actual_sha256 STREQUAL "${expected_${artifact}_sha256}")
        message(FATAL_ERROR "Semantic Big-Star pending-left phase-six ${artifact} artifact changed.\nExpected: ${expected_${artifact}_sha256}\nActual:   ${actual_sha256}")
    endif()
endforeach()
message(STATUS "Semantic 5722 covers the verified Big-Star pending-left phase-six helper.")
