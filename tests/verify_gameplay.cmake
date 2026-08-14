# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

set(
    expected_gameplay_sha256
    "1f6d46ac99798bbac1ecab97702d9e10076b9766796cc64d22d9662efce0b353"
)
set(
    expected_trace_sha256
    "6c36dee728273eae853b3d62d2e6331e73694d8179d338303ea1799299d9c6d6"
)
set(
    expected_call_sha256
    "9684b350a61ecc49e48dc236ea0812d5e6ab0d86c99ea1653135e78cf8b6cbbf"
)
set(
    expected_write_sha256
    "84c398603f50b279ad670eed0d06d887006f085aa1d5aa7d4a8de4ad8178264c"
)
set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-gameplay
        "${OUTPUT}"
        --observe-hook
        "00:E195"
        --observe-hook
        "01:C3CB"
        --observe-hook
        "01:C49E"
        --observe-hook
        "03:6966"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Gameplay recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "Observe hook 1 entries: 1799"
    "Observe hook 2 entries: 860"
    "Observe hook 3 entries: 860"
    "Observe hook 4 entries: 19648"
    "Routine trace complete: 23167 entries"
    "Routine call trace complete: 23167 calls, 235173 writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Gameplay mapping output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
endforeach()

file(SHA256 "${OUTPUT}" actual_gameplay_sha256)
if(NOT actual_gameplay_sha256 STREQUAL expected_gameplay_sha256)
    message(
        FATAL_ERROR
        "Gameplay recording changed.\n"
        "Expected: ${expected_gameplay_sha256}\n"
        "Actual:   ${actual_gameplay_sha256}\n"
        "${recorder_output}${recorder_error}"
    )
endif()

file(SHA256 "${trace_output}" actual_trace_sha256)
if(NOT actual_trace_sha256 STREQUAL expected_trace_sha256)
    message(
        FATAL_ERROR
        "Gameplay routine-entry trace changed.\n"
        "Expected: ${expected_trace_sha256}\n"
        "Actual:   ${actual_trace_sha256}\n"
    )
endif()

file(SHA256 "${call_output}" actual_call_sha256)
if(NOT actual_call_sha256 STREQUAL expected_call_sha256)
    message(
        FATAL_ERROR
        "Gameplay routine-call trace changed.\n"
        "Expected: ${expected_call_sha256}\n"
        "Actual:   ${actual_call_sha256}\n"
    )
endif()

file(SHA256 "${write_output}" actual_write_sha256)
if(NOT actual_write_sha256 STREQUAL expected_write_sha256)
    message(
        FATAL_ERROR
        "Gameplay ordered-write trace changed.\n"
        "Expected: ${expected_write_sha256}\n"
        "Actual:   ${actual_write_sha256}\n"
    )
endif()

message(
    STATUS
    "Gameplay recording verified: ${actual_gameplay_sha256}"
)
message(
    STATUS
    "Gameplay routine map verified: ${actual_trace_sha256}"
)
message(
    STATUS
    "Gameplay call scopes verified: ${actual_call_sha256}"
)
message(
    STATUS
    "Gameplay ordered writes verified: ${actual_write_sha256}"
)
