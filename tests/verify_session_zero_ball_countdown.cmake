# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Session zero-ball countdown regression is Release-only.")
endif()

set(expected_census_sha256 "669f74d514ce6af6fd70c307a74cf085d31e878506191171a134ee94bc339e09")
set(expected_call_sha256 "ec3b772ba19dafa3da83c178113f9e62b3bd0ac553c188636468caf06717fc25")
set(expected_write_sha256 "325ba76f5ed1485b32aad7ab22fa88ef68ae6d711e5e80b459f852f759b9f067")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}" --record-state-census "${OUTPUT}"
        --observe-hook "01:C7F9" --replace-hook "01:C7F9@all"
        --routine-call-trace "${call_output}"
        --work-ram-patch "3000:3380:03" --work-ram-patch "3000:3300:00"
        --work-ram-patch "3000:2D52:01" --rom "${ROM}"
    RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Session zero-ball countdown recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()
foreach(expected_line IN ITEMS
    "Work-RAM patches applied: 3"
    "Native replacement 01:C7F9@all entries: 254 of 255"
    "Observe hook 1 entries: 255"
    "Routine call trace complete: 255 calls, 1581 writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Session zero-ball countdown output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
    endif()
endforeach()
foreach(artifact IN ITEMS census call write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    else()
        set(artifact_path "${write_output}")
    endif()
    file(SHA256 "${artifact_path}" actual_sha256)
    if(NOT actual_sha256 STREQUAL "${expected_${artifact}_sha256}")
        message(FATAL_ERROR "Session zero-ball countdown ${artifact} artifact changed. Expected ${expected_${artifact}_sha256}; actual ${actual_sha256}")
    endif()
endforeach()
