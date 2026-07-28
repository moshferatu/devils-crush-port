# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(CONFIG STREQUAL "Debug")
    set(
        expected_sha256
        "1e8540272bb49e7b35a1cba902feed6723c3752ed6409ec2c276f77136d55c79"
    )
elseif(CONFIG STREQUAL "Release")
    set(
        expected_sha256
        "f9ad5753938bbab4d87a9b160b57c9cbcc1fe17610b3bfef892b27f082421b39"
    )
else()
    message(
        FATAL_ERROR
        "Unsupported work-RAM-patch configuration: ${CONFIG}"
    )
endif()

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --work-ram-patch
        "3600:3FFF:A5"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Work-RAM patch recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 1"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Work-RAM patch output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
endforeach()

file(READ "${OUTPUT}" recording)
foreach(expected_line IN ITEMS
    "# work_ram_patch_count=1"
    "# work_ram_patch_1=3600:3FFF:A5"
    "3600,0000,0000,c858cafb51a462658046c9529e5d4c795d1b63bfd09db61198c9c149a77adc96"
)
    string(FIND "${recording}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Work-RAM patch artifact omitted '${expected_line}'."
        )
    endif()
endforeach()

file(SHA256 "${OUTPUT}" actual_sha256)
if(NOT actual_sha256 STREQUAL expected_sha256)
    message(
        FATAL_ERROR
        "Work-RAM patch artifact changed.\n"
        "Expected: ${expected_sha256}\n"
        "Actual:   ${actual_sha256}\n"
    )
endif()
message(STATUS "Work-RAM patch artifact verified: ${actual_sha256}")
