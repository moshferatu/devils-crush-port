# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Knight-release-transition regression is Release-only.")
endif()

set(expected_census_sha256 "9b1bbcadc419ba8ed03e9864734ed667dffcb6c72638d1be87e3e403a0d90021")
set(expected_call_sha256 "e05ea0818111c220fc85dc06a12898edf6cd8788650789a8b3677fd1c769e019")
set(expected_write_sha256 "f1ed812127e7310cea8891cc76f271b3dcf1f916ed7662b097a4565a2e9a1fa6")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND "${APP}" --record-state-census "${OUTPUT}"
        --observe-hook "03:7378" --replace-hook "03:7378@all"
        --routine-call-trace "${call_output}"
        --work-ram-patch "1228:3100:00" --work-ram-patch "1228:3180:40"
        --work-ram-patch "1228:3200:00" --work-ram-patch "1228:3300:00"
        --work-ram-patch "1228:3380:01"
        --work-ram-patch "1340:3100:00" --work-ram-patch "1340:3180:40"
        --work-ram-patch "1340:3200:00" --work-ram-patch "1340:3280:00"
        --work-ram-patch "1340:3300:00" --work-ram-patch "1340:3380:01"
        --work-ram-patch "1400:3B3A:00" --work-ram-patch "1400:3F3A:78"
        --work-ram-patch "1400:3FBA:73" --work-ram-patch "1400:2D06:01"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result OUTPUT_VARIABLE recorder_output ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Knight-release-transition recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()
foreach(expected_line IN ITEMS
    "Work-RAM patches applied: 15"
    "Native replacement 03:7378@all entries: 9701 of 9701"
    "Observe hook 1 entries: 9701"
    "Routine call trace complete: 9701 calls, 27192 writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Knight-release-transition output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Knight-release-transition ${artifact} artifact changed. Expected ${expected_${artifact}_sha256}; actual ${actual_sha256}")
    endif()
endforeach()
