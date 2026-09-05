# SPDX-License-Identifier: GPL-3.0-or-later
foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release" AND NOT CONFIG STREQUAL "Debug")
    message(FATAL_ERROR "Unsupported scene-bootstrap timeout configuration: ${CONFIG}")
endif()
# Supplementary controlled exit, not admission evidence. The second bootstrap
# naturally finishes the full 16-bit countdown, including each low-byte borrow.
set(hooks)
set(pcs 55A6 55B6 55BD 55C0 55C3 55CE 55E0 55F1 55F4 55F7 55FA 55FF
    5618 561B 5624 5627 5631 5636 563B 5695 56B1 56B4 56B7)
foreach(pc IN LISTS pcs)
    list(APPEND hooks --replace-hook "08:${pc}@all")
endforeach()
set(state_path "${OUTPUT}.csv")
set(writes_path "${state_path}.writes.csv")
set(trace_path "${state_path}.trace.csv")
execute_process(COMMAND "${APP}" --record-gameplay "${state_path}" ${hooks}
    --work-ram-patch "200:2D5F:01" --work-ram-patch "200:2D60:00"
    --observe-hook "08:5871" --routine-trace "${trace_path}"
    --work-ram-write-range 2000-3FFF --work-ram-write-trace "${writes_path}"
    --rom "${ROM}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Scene-bootstrap timeout failed (${result}).\n${output}${error}")
endif()
foreach(line IN ITEMS "Gameplay recording complete: 1800 frames"
        "Work-RAM patches applied: 2"
        "Work-RAM write trace complete: 1318305 selected writes"
        "Routine trace complete: 2 entries")
    string(FIND "${output}" "${line}" offset)
    if(offset EQUAL -1)
        message(FATAL_ERROR "Scene-bootstrap timeout omitted '${line}'.\n${output}${error}")
    endif()
endforeach()
foreach(pc IN LISTS pcs)
    string(REGEX MATCH "Native replacement 08:${pc}@all entries: ([0-9]+) of ([0-9]+)" matched "${output}")
    if(NOT matched OR NOT CMAKE_MATCH_1 STREQUAL CMAKE_MATCH_2 OR CMAKE_MATCH_1 EQUAL 0)
        message(FATAL_ERROR "Incomplete timeout coverage at ${pc}.\n${output}${error}")
    endif()
endforeach()
set(expected_state "76a6c6a2156bcc41cbffbbffea4d6d2e673d7725c8e0909249f741ef75bcc8fa")
set(expected_writes "c6f75f0f2f8176c944fb91fd34630bde20da90187804d4d72f5eebecac66ac93")
set(expected_trace "e3c57ba7b8af96cff92a5c6ab27a1fc4097160037efd084c065a29d27a7ac368")
foreach(artifact IN ITEMS state writes trace)
    file(SHA256 "${${artifact}_path}" actual)
    if(NOT actual STREQUAL expected_${artifact})
        message(FATAL_ERROR "Scene-bootstrap timeout ${artifact} changed: ${actual}; expected ${expected_${artifact}}")
    endif()
endforeach()
message(STATUS "Scene-bootstrap timeout/restart and 16-bit countdown preserve assembly.")
