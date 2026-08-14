# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Giant Door completion-scheduler-prefix regression is Release-only.")
endif()

set(expected_census_sha256 "5e39c2543cf8a17bc5e74e7c55693c0d6abf7429ea82822df8b641eb7860f28b")
set(expected_call_sha256 "daf6ecb1df9028022304a513d036c7977463ad5174a8f0e23c37bd195bb6ba0e")
set(expected_write_sha256 "93ddf2f9365c168ccdbc951d9448c609b02bd120ebd72d286664675208c4b266")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}" --record-state-census "${OUTPUT}"
        --observe-hook "03:62E2" --replace-hook "03:62E2@all"
        --routine-call-trace "${call_output}"
        --work-ram-patch "1228:3100:00" --work-ram-patch "1228:3180:40"
        --work-ram-patch "1228:3200:00" --work-ram-patch "1228:3280:00"
        --work-ram-patch "1228:3300:00" --work-ram-patch "1228:3380:01"
        --work-ram-patch "1340:3100:00" --work-ram-patch "1340:3180:40"
        --work-ram-patch "1340:3200:00" --work-ram-patch "1340:3280:00"
        --work-ram-patch "1340:3300:00" --work-ram-patch "1340:3380:01"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Giant Door completion-scheduler-prefix recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()
foreach(expected_line IN ITEMS
    "Work-RAM patches applied: 12"
    "Native replacement 03:62E2@all entries: 1 of 1"
    "Observe hook 1 entries: 1"
    "Routine call trace complete: 1 calls, 9 writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Giant Door completion-scheduler-prefix output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Giant Door completion-scheduler-prefix ${artifact} artifact changed. Expected ${expected_${artifact}_sha256}; actual ${actual_sha256}")
    endif()
endforeach()
