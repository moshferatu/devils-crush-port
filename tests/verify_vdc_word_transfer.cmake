# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release" AND NOT CONFIG STREQUAL "Debug")
    message(FATAL_ERROR "Unsupported VDC word-transfer configuration: ${CONFIG}")
endif()

# Unpatched assembly route: address-fill back-edge plus an uncompressed
# 64-word upload. Both native chunks must preserve the outer entry/exit,
# every RAM write and the real VDC bus stream, including master-cycle offsets.
set(expected_state "1f6d46ac99798bbac1ecab97702d9e10076b9766796cc64d22d9662efce0b353")
set(expected_writes "da606d14dd48b5b73e86096ba4033c6bf40c67e97ea771e5705760a02bbe3e1c")
set(expected_bus "0aa7c15c4768684c130ba4c1c58f252021c68cb47e44f036bc35f824b1fab51f")
set(expected_trace "39a9e2e762ca3bf09b5a9b66b089c395c67e24631f2dea6a816a0eb5f602442f")
set(state_path "${OUTPUT}.csv")
set(writes_path "${OUTPUT}.writes.csv")
set(bus_path "${OUTPUT}.bus.csv")
set(trace_path "${OUTPUT}.trace.csv")
execute_process(COMMAND "${APP}" --record-gameplay "${state_path}"
    --replace-hook "00:E34C@all" --replace-hook "00:E365@all" --replace-hook "00:E37E@all"
    --observe-hook "00:E365" --observe-hook "00:E39C" --routine-trace "${trace_path}"
    --work-ram-write-range 2000-3FFF --work-ram-write-trace "${writes_path}"
    --bus-write-range 0000-0003 --bus-write-trace "${bus_path}" --rom "${ROM}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "VDC word transfer failed (${result}).\n${output}${error}")
endif()
foreach(line IN ITEMS
    "Gameplay recording complete: 1800 frames"
    "Native replacement 00:E34C@all entries: 2048 of 2048"
    "Native replacement 00:E365@all entries: 1 of 1"
    "Native replacement 00:E37E@all entries: 1 of 1"
    "Routine trace complete: 2 entries"
    "Work-RAM write trace complete: 4270592 selected writes"
    "Bus write trace complete: 1019098 selected writes")
    string(FIND "${output}" "${line}" offset)
    if(offset EQUAL -1)
        message(FATAL_ERROR "VDC word transfer omitted '${line}'.\n${output}${error}")
    endif()
endforeach()
foreach(artifact IN ITEMS state writes bus trace)
    file(SHA256 "${${artifact}_path}" actual)
    if(NOT actual STREQUAL expected_${artifact})
        message(FATAL_ERROR "VDC word-transfer ${artifact} changed: ${actual}; expected ${expected_${artifact}}")
    endif()
endforeach()
message(STATUS "VDC word transfer preserves assembly state, RAM/VDC writes and outer boundaries.")
