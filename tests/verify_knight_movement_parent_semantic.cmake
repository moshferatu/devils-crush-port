# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT ORDINAL)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Knight-movement-parent regression is Release-only.")
endif()
if(NOT ORDINAL STREQUAL "all" AND NOT ORDINAL STREQUAL "2" AND NOT ORDINAL STREQUAL "4" AND NOT ORDINAL STREQUAL "16" AND NOT ORDINAL STREQUAL "58" AND NOT ORDINAL STREQUAL "60")
    message(FATAL_ERROR "Knight-movement-parent regression ordinal must be all, 2, 4, 16, 58, or 60.")
endif()

set(expected_census_sha256 "9b1bbcadc419ba8ed03e9864734ed667dffcb6c72638d1be87e3e403a0d90021")
set(expected_call_sha256 "5dbb230e751f0f2cc1e089661dba74b4f0b9b1a7c38fdbc010e039d35b926a10")
set(expected_write_sha256 "bc5d40c4259a012696a3576188271060d601c660c362f97bca719293a22da83c")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
if(ORDINAL STREQUAL "all")
    set(expected_native_line "Native replacement 03:73A6@all entries: 64 of 129")
else()
    set(expected_native_line "Native replacement 03:73A6@${ORDINAL} entries: 1")
endif()

execute_process(
    COMMAND "${APP}" --record-state-census "${OUTPUT}"
        --observe-hook "03:73A6" --replace-hook "03:73A6@${ORDINAL}"
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
    message(FATAL_ERROR "Knight-movement-parent recorder failed (${recorder_result}).\n${recorder_output}${recorder_error}")
endif()
foreach(expected_line IN ITEMS
    "Work-RAM patches applied: 15"
    "${expected_native_line}"
    "Observe hook 1 entries: 129"
    "Routine call trace complete: 129 calls, 2062 writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Knight-movement-parent output omitted '${expected_line}'.\n${recorder_output}${recorder_error}")
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
        message(FATAL_ERROR "Knight-movement-parent ${artifact} artifact changed. Expected ${expected_${artifact}_sha256}; actual ${actual_sha256}")
    endif()
endforeach()
