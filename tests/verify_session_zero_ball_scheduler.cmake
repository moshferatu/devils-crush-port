# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Session zero-ball scheduler regression is Release-only.")
endif()

set(expected_census_sha256 "669f74d514ce6af6fd70c307a74cf085d31e878506191171a134ee94bc339e09")
set(expected_call_sha256 "723d2cd572cec9c7ec23ddfe17ff579def3107b87bfdf28023fd1bca8552ccfb")
set(expected_write_sha256 "773ab75800ad5b961c131313e5499bf04735c62e0471890cd63f1ad80aada1bf")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}" --record-state-census "${OUTPUT}"
        --observe-hook "01:C7E5" --observe-hook "01:C7F4"
        --observe-hook "01:C5EA" --observe-hook "01:C5C9"
        --replace-hook "01:C7F4@all" --routine-call-trace "${call_output}"
        --work-ram-patch "3000:3380:03" --work-ram-patch "3000:3300:00"
        --work-ram-patch "3000:2D52:01" --rom "${ROM}"
    RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Session zero-ball scheduler recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()
foreach(expected_line IN ITEMS
    "Work-RAM patches applied: 3"
    "Native replacement 01:C7F4@all entries: 1 of 1"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 1"
    "Observe hook 3 entries: 70"
    "Observe hook 4 entries: 2902"
    "Routine call trace complete: 2974 calls, 967804 writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Session zero-ball scheduler output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Session zero-ball scheduler ${artifact} artifact changed. Expected ${expected_${artifact}_sha256}; actual ${actual_sha256}")
    endif()
endforeach()
