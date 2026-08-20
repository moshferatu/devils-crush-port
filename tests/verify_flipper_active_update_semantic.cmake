# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Flipper active-update regression is Release-only.")
endif()

set(expected_census_sha256 "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_call_sha256 "ad67adeb347cf26ef3caa4fbc39cd3428e59764c3e765ccb5d33e96410745592")
set(expected_write_sha256 "ffe42ed3fa01c1d13c5d86332632383ada1878a416ef4372b08f85497da5c367")

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(abandonment_output "${OUTPUT}.abandonments.csv")
execute_process(
    COMMAND "${APP}"
        --record-state-census "${OUTPUT}"
        --observe-hook "01:D4C6"
        --observe-hook "01:D4C9"
        --observe-hook "01:D4CC"
        --replace-hook "01:D4C6@all"
        --replace-hook "01:D4C9@all"
        --replace-hook "01:D4CC@all"
        --routine-call-trace "${call_output}"
        --abandonment-census "${abandonment_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_stdout ERROR_VARIABLE recorder_stderr)
if(NOT recorder_result EQUAL 0)
    message(FATAL_ERROR "Flipper active-update recorder failed (${recorder_result}).\n${recorder_stdout}${recorder_stderr}")
endif()
foreach(expected_line IN ITEMS
    "Native replacement 01:D4C6@all entries: 490 of 490"
    "Native replacement 01:D4C9@all entries: 490 of 490"
    "Native replacement 01:D4CC@all entries: 490 of 490"
    "Abandonment census complete: no transcript was abandoned"
    "Observe hook 1 entries: 490"
    "Observe hook 2 entries: 490"
    "Observe hook 3 entries: 490"
    "Routine call trace complete: 1470 calls, 46125 writes")
    string(FIND "${recorder_stdout}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Flipper active-update output omitted '${expected_line}'.\n${recorder_stdout}${recorder_stderr}")
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
        message(FATAL_ERROR "Flipper active-update ${artifact} artifact changed. Expected ${expected_${artifact}_sha256}; actual ${actual_sha256}")
    endif()
endforeach()

message(STATUS "Semantic 01:D4C6/D4C9/D4CC replaces all 490 canonical active-flipper passes")
