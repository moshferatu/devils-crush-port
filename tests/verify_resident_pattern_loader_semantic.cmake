# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported resident pattern-loader configuration: ${CONFIG}")
endif()

set(expected_state_sha256 "77568124b24b80006d3077da8fcf0df21706c5627278e4c7bc92f645e56773cf")

execute_process(
    COMMAND "${APP}"
        --record-gameplay-higharc "${OUTPUT}"
        --replace-hook "00:F768@all"
        --replace-hook "00:F778@all"
        --replace-hook "00:F781@all"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Resident pattern-loader recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()
foreach(expected_line IN ITEMS
    "Gameplay higharc recording complete: 3600 frames"
    "Native replacement 00:F768@all entries: 1 of 1"
    "Native replacement 00:F778@all entries: 1 of 1"
    "Native replacement 00:F781@all entries: 1 of 1"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Resident pattern-loader output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
    endif()
endforeach()
file(SHA256 "${OUTPUT}" actual_state_sha256)
if(NOT actual_state_sha256 STREQUAL expected_state_sha256)
    message(FATAL_ERROR "Resident pattern-loader state changed.\nExpected: ${expected_state_sha256}\nActual:   ${actual_state_sha256}")
endif()
message(STATUS "Resident pattern-loader preserves the scene-resident VDC upload route.")
