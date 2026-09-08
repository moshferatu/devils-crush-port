# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
execute_process(
    COMMAND "${APP}"
        --record-gameplay-higharc "${OUTPUT}.assembly"
        --observe-hook "00:F768"
        --routine-call-trace "${OUTPUT}.assembly.calls.csv"
        --rom "${ROM}"
    RESULT_VARIABLE assembly_result OUTPUT_VARIABLE assembly_output ERROR_VARIABLE assembly_error
)
if(NOT assembly_result EQUAL 0)
    message(FATAL_ERROR "Resident pattern-loader oracle failed: ${assembly_output}${assembly_error}")
endif()

execute_process(
    COMMAND "${APP}"
        --record-gameplay-higharc "${OUTPUT}"
        --replace-hook "00:F768@all"
        --replace-hook "00:F778@all"
        --replace-hook "00:F781@all"
        --observe-hook "00:F768"
        --routine-call-trace "${OUTPUT}.calls.csv"
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
foreach(suffix IN ITEMS "" ".calls.csv" ".calls.csv.writes.csv")
    file(SHA256 "${OUTPUT}.assembly${suffix}" original)
    file(SHA256 "${OUTPUT}${suffix}" replacement)
    if(NOT original STREQUAL replacement)
        message(FATAL_ERROR "Resident pattern-loader ${suffix}: ${original} != ${replacement}")
    endif()
    message(STATUS "Resident pattern-loader ${suffix}: ${original}")
endforeach()
file(SHA256 "${OUTPUT}.assembly" state_hash)
if(NOT state_hash STREQUAL "77568124b24b80006d3077da8fcf0df21706c5627278e4c7bc92f645e56773cf")
    message(FATAL_ERROR "Resident pattern-loader assembly baseline changed: ${state_hash}")
endif()
