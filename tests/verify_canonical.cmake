# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

set(
    expected_sha256
    "67a55bed2d4f4e8dc5019c2b9b845df2f29514bbb564e2e44cde406d159575fe"
)

execute_process(
    COMMAND
        "${APP}"
        --record-canonical
        "${OUTPUT}"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Canonical recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

file(SHA256 "${OUTPUT}" actual_sha256)
if(NOT actual_sha256 STREQUAL expected_sha256)
    message(
        FATAL_ERROR
        "Canonical recording changed.\n"
        "Expected: ${expected_sha256}\n"
        "Actual:   ${actual_sha256}\n"
        "${recorder_output}${recorder_error}"
    )
endif()

message(STATUS "Canonical recording verified: ${actual_sha256}")
