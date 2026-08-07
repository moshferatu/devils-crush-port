# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Session alternate-latch prefix regression is Release-only.")
endif()

set(expected_census_sha256 "987c6f07d131bae52958c71ef8345304ca7d737216687bcadd77ec03b5a742c1")
set(expected_call_sha256 "fde029cff362c212d8d2a000ce683e4545bab79c922d8b373ac53bebeee3149a")
set(expected_write_sha256 "775e0419916ea3fe37b35e8aacf00b26b5c2c6d3d7119cc1d81f57d8f4a80e87")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}" --record-state-census "${OUTPUT}"
        --observe-hook "01:C7E5" --observe-hook "01:C7EA"
        --observe-hook "01:C5EA" --observe-hook "01:C5C9"
        --replace-hook "01:C7EA@all" --routine-call-trace "${call_output}"
        --work-ram-patch "3000:3380:03" --work-ram-patch "3000:3300:00"
        --work-ram-patch "3270:2D32:01" --rom "${ROM}"
    RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Session alternate-latch prefix recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()
foreach(expected_line IN ITEMS
    "Work-RAM patches applied: 3"
    "Native replacement 01:C7EA@all entries: 1 of 1"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 1"
    "Observe hook 3 entries: 73"
    "Observe hook 4 entries: 2657"
    "Routine call trace complete: 2732 calls, 918737 writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Session alternate-latch prefix output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Session alternate-latch prefix ${artifact} artifact changed. Expected ${expected_${artifact}_sha256}; actual ${actual_sha256}")
    endif()
endforeach()
