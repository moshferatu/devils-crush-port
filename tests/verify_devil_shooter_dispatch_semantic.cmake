# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported shooter dispatch configuration: ${CONFIG}")
endif()

# Assembly-only higharc artifacts, observed at the outer controller. This
# includes the calls and ordered writes of its audio, charge, full-charge
# countdown and launch children. Observing 518D separately would count an
# internal entry which the parent now executes itself.
set(expected_state "77568124b24b80006d3077da8fcf0df21706c5627278e4c7bc92f645e56773cf")
set(expected_calls "0cfaec51f3fdc55ab29b5bf15e002b1dc6cf5700811ab481830e8bddd362ef9a")
set(expected_writes "fbd728e41048a2bb1a6412d379865810657f2cb9df7579c1511b296221636dbb")

execute_process(
    COMMAND "${APP}"
        --record-gameplay-higharc "${OUTPUT}"
        --observe-hook "02:5159"
        --replace-hook "02:5159@all"
        --replace-hook "02:518D@all"
        --routine-call-trace "${OUTPUT}.calls.csv"
        --rom "${ROM}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Shooter dispatch replay failed (${result}).\n${output}${error}")
endif()
foreach(expected_line IN ITEMS
    "Gameplay higharc recording complete: 3600 frames"
    "Native replacement 02:5159@all entries: 1629 of 1629"
    "Native replacement 02:518D@all entries: 11 of 11"
    "Observe hook 1 entries: 1629"
    "Routine call trace complete: 1629 calls, 20975 writes"
)
    string(FIND "${output}" "${expected_line}" offset)
    if(offset EQUAL -1)
        message(FATAL_ERROR "Shooter dispatch omitted '${expected_line}'.\n${output}${error}")
    endif()
endforeach()
set(state_path "${OUTPUT}")
set(calls_path "${OUTPUT}.calls.csv")
set(writes_path "${OUTPUT}.calls.csv.writes.csv")
foreach(artifact IN ITEMS state calls writes)
    file(SHA256 "${${artifact}_path}" actual)
    if(NOT actual STREQUAL expected_${artifact})
        message(FATAL_ERROR "Shooter dispatch ${artifact} changed: ${actual}; expected ${expected_${artifact}}")
    endif()
endforeach()
message(STATUS "All shooter dispatch calls preserve assembly state, registers, timing and ordered writes.")
