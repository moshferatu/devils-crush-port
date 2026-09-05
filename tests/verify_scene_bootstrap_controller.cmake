# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release" AND NOT CONFIG STREQUAL "Debug")
    message(FATAL_ERROR "Unsupported scene-bootstrap configuration: ${CONFIG}")
endif()

# Unpatched natural admission route. Assembly goldens cover every RAM and VDC
# write and child-entry register/MPR/cycle tuple, not just final state.
set(pcs 55A6 55B6 55BD 55C0 55C3 55CE 55E0 55F1 55F4 55F7 55FA 55FF
    5618 561B 5624 5627 5631 5636 563B 5695 56B1 56B4 56B7)
set(hooks)
foreach(pc IN LISTS pcs)
    list(APPEND hooks --replace-hook "08:${pc}@all")
endforeach()
set(variant natural)
set(boundary_hooks)
set(boundaries "08:5879" "08:58EE" "00:E2E3" "00:E321" "00:E1FD"
    "00:EBA5" "00:E2CB" "00:ECFD" "01:C4DE" "08:4036" "00:ED2C"
    "08:5977" "08:56D3" "09:6FB1" "09:70EE" "09:769A")
set(expected_bus "0aa7c15c4768684c130ba4c1c58f252021c68cb47e44f036bc35f824b1fab51f")
set(expected_state "1f6d46ac99798bbac1ecab97702d9e10076b9766796cc64d22d9662efce0b353")
set(expected_writes "da606d14dd48b5b73e86096ba4033c6bf40c67e97ea771e5705760a02bbe3e1c")
set(expected_trace "bf569601c1f8c189f284f41279acf53f1085a71e0a55471e8f54d62a4b69890f")
set(expected_lines
    "Native replacement 08:55A6@all entries: 1 of 1"
    "Native replacement 08:55B6@all entries: 1 of 1"
    "Native replacement 08:55BD@all entries: 1 of 1"
    "Native replacement 08:55C0@all entries: 1 of 1"
    "Native replacement 08:55C3@all entries: 1 of 1"
    "Native replacement 08:55CE@all entries: 1 of 1"
    "Native replacement 08:55E0@all entries: 1 of 1"
    "Native replacement 08:55F1@all entries: 1 of 1"
    "Native replacement 08:55F4@all entries: 1 of 1"
    "Native replacement 08:55F7@all entries: 1 of 1"
    "Native replacement 08:55FA@all entries: 1 of 1"
    "Native replacement 08:55FF@all entries: 1 of 1"
    "Native replacement 08:5618@all entries: 1 of 1"
    "Native replacement 08:561B@all entries: 173 of 173"
    "Native replacement 08:5624@all entries: 172 of 172"
    "Native replacement 08:5627@all entries: 172 of 172"
    "Native replacement 08:5631@all entries: 1 of 1"
    "Native replacement 08:5636@all entries: 1 of 1"
    "Native replacement 08:563B@all entries: 1 of 1"
    "Native replacement 08:5695@all entries: 1 of 1"
    "Native replacement 08:56B1@all entries: 120 of 120"
    "Native replacement 08:56B4@all entries: 120 of 120"
    "Native replacement 08:56B7@all entries: 120 of 120"
    "Work-RAM write trace complete: 4270592 selected writes"
    "Routine trace complete: 6288 entries"
    "Bus write trace complete: 1019098 selected writes")
foreach(boundary IN LISTS boundaries)
    list(APPEND boundary_hooks --observe-hook "${boundary}")
endforeach()
set(state_path "${OUTPUT}.${variant}.csv")
set(writes_path "${state_path}.writes.csv")
set(trace_path "${state_path}.trace.csv")
set(bus_path "${state_path}.bus.csv")
execute_process(COMMAND "${APP}" --record-gameplay "${state_path}"
    ${hooks} ${boundary_hooks} --routine-trace "${trace_path}"
    --work-ram-write-range 2000-3FFF --work-ram-write-trace "${writes_path}"
    --bus-write-range 0000-0003 --bus-write-trace "${bus_path}"
    --rom "${ROM}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Scene-bootstrap ${variant} failed (${result}).\n${output}${error}")
endif()
list(APPEND expected_lines "Gameplay recording complete: 1800 frames")
foreach(line IN LISTS expected_lines)
    string(FIND "${output}" "${line}" offset)
    if(offset EQUAL -1)
        message(FATAL_ERROR "Scene-bootstrap omitted '${line}'.\n${output}${error}")
    endif()
endforeach()
foreach(pc IN LISTS pcs)
    string(REGEX MATCH "Native replacement 08:${pc}@all entries: ([0-9]+) of ([0-9]+)" matched "${output}")
    if(NOT matched OR NOT CMAKE_MATCH_1 STREQUAL CMAKE_MATCH_2 OR CMAKE_MATCH_1 EQUAL 0)
        message(FATAL_ERROR "Incomplete bootstrap coverage at ${pc}.\n${output}${error}")
    endif()
endforeach()
foreach(artifact IN ITEMS state writes trace bus)
    file(SHA256 "${${artifact}_path}" actual)
    if(NOT actual STREQUAL expected_${artifact})
        message(FATAL_ERROR "Scene-bootstrap ${variant} ${artifact} changed: ${actual}; expected ${expected_${artifact}}")
    endif()
endforeach()
message(STATUS "Scene bootstrap preserves assembly state, ordered RAM/VDC writes and child boundaries.")
