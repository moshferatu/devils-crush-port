# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Knight-terminal-release-helper-allocator-store-prefix regression is Release-only.")
endif()

set(expected_census_sha256 "5bee19b564a4e54444f444fdaf156fcec1385fbca952abf8b531b910eeb75243")
set(expected_call_sha256 "71d05f79a571f564ba385fd9b1bcd5f077cc59d3451a44dd2f60125cdea3d17d")
set(expected_write_sha256 "4c45ac9ffa4068c2fca7340ef228f6c565cde2aacb30dfb7dba688c23b383b5f")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}" --record-state-census "${OUTPUT}"
        --observe-hook "01:C5BB" --replace-hook "01:C5BB@all"
        --routine-call-trace "${call_output}"
        --work-ram-patch "1228:3100:00" --work-ram-patch "1228:3180:40"
        --work-ram-patch "1228:3200:00" --work-ram-patch "1228:3300:00"
        --work-ram-patch "1228:3380:01"
        --work-ram-patch "1340:3100:00" --work-ram-patch "1340:3180:40"
        --work-ram-patch "1340:3200:00" --work-ram-patch "1340:3280:00"
        --work-ram-patch "1340:3300:00" --work-ram-patch "1340:3380:01"
        --work-ram-patch "1400:3B3A:00" --work-ram-patch "1400:3F3A:78"
        --work-ram-patch "1400:3FBA:73" --work-ram-patch "1400:2D06:01"
        --work-ram-patch "1402:3C3A:01" --work-ram-patch "1402:383A:80"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Knight-terminal-release-helper-allocator-store-prefix recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()
foreach(expected_line IN ITEMS
    "Work-RAM patches applied: 17"
    "Native replacement 01:C5BB@all entries: 1 of 64"
    "Observe hook 1 entries: 64"
    "Routine call trace complete: 64 calls, 77 writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Knight-terminal-release-helper-allocator-store-prefix output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Knight-terminal-release-helper-allocator-store-prefix ${artifact} artifact changed. Expected ${expected_${artifact}_sha256}; actual ${actual_sha256}")
    endif()
endforeach()
