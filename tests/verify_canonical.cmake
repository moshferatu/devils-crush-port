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
set(
    expected_observe_trace_sha256
    "585b2221987701f24b9f6a271d48a7bedec3ce01fb08f3889d196cfe1a3191a4"
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

set(hooked_output "${OUTPUT}.pass-through")
execute_process(
    COMMAND
        "${APP}"
        --record-canonical
        "${hooked_output}"
        --pass-through-hook
        "00:E462"
        --rom
        "${ROM}"
    RESULT_VARIABLE hooked_recorder_result
    OUTPUT_VARIABLE hooked_recorder_output
    ERROR_VARIABLE hooked_recorder_error
)
if(NOT hooked_recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Pass-through canonical recorder failed "
        "(${hooked_recorder_result}).\n"
        "${hooked_recorder_output}${hooked_recorder_error}"
    )
endif()

file(SHA256 "${hooked_output}" hooked_sha256)
if(NOT hooked_sha256 STREQUAL expected_sha256)
    message(
        FATAL_ERROR
        "Pass-through execution hook changed the canonical recording.\n"
        "Expected: ${expected_sha256}\n"
        "Actual:   ${hooked_sha256}\n"
        "${hooked_recorder_output}${hooked_recorder_error}"
    )
endif()

message(
    STATUS
    "Pass-through execution hook verified: ${hooked_sha256}"
)

foreach(observe_run IN ITEMS first repeat)
    set(observe_output "${OUTPUT}.observe-${observe_run}")
    set(observe_trace "${OUTPUT}.observe-${observe_run}.csv")
    execute_process(
        COMMAND
            "${APP}"
            --record-canonical
            "${observe_output}"
            --observe-hook
            "00:E462"
            --routine-trace
            "${observe_trace}"
            --rom
            "${ROM}"
        RESULT_VARIABLE observe_result
        OUTPUT_VARIABLE observe_output_text
        ERROR_VARIABLE observe_error
    )
    if(NOT observe_result EQUAL 0)
        message(
            FATAL_ERROR
            "Observe recorder failed (${observe_result}).\n"
            "${observe_output_text}${observe_error}"
        )
    endif()

    file(SHA256 "${observe_output}" observe_canonical_sha256)
    if(NOT observe_canonical_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "Observe execution hook changed the canonical recording.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${observe_canonical_sha256}\n"
            "${observe_output_text}${observe_error}"
        )
    endif()
    file(SHA256 "${observe_trace}" observe_trace_sha256)
    if(NOT observe_trace_sha256 STREQUAL expected_observe_trace_sha256)
        message(
            FATAL_ERROR
            "Observe routine-entry trace changed.\n"
            "Expected: ${expected_observe_trace_sha256}\n"
            "Actual:   ${observe_trace_sha256}\n"
        )
    endif()
    if(observe_run STREQUAL "first")
        set(first_observe_trace_sha256 "${observe_trace_sha256}")
    elseif(NOT observe_trace_sha256 STREQUAL first_observe_trace_sha256)
        message(
            FATAL_ERROR
            "Observe routine-entry trace is not deterministic.\n"
            "First:  ${first_observe_trace_sha256}\n"
            "Repeat: ${observe_trace_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Observe execution hook verified: ${first_observe_trace_sha256}"
)
