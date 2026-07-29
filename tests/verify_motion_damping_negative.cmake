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
    expected_call_sha256
    "e285eb631a38539fcca0bed8c7e7d84a6f7dbbaa0613ee8009467b50c0f03327"
)
set(
    expected_write_sha256
    "ef4285d7a05ee530edb4563e88b4af8e0fb23deaee2188bc40828ff48b8571d8"
)

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-gameplay
        "${OUTPUT}"
        --observe-hook
        "01:DD09"
        --replace-hook
        "01:DD09@181"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "01:DD09@181"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Negative damping recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "Gameplay recording complete: 1800 frames"
    "Native replacement 01:DD09@181 entries: 1"
    "Routine call trace complete: 753 calls, 61291 writes"
    "Routine replay 01:DD09@181 verified: frame 1228, 2427 body cycles, 69 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Negative damping output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS gameplay call write)
    if(artifact STREQUAL "gameplay")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    else()
        set(artifact_path "${write_output}")
    endif()
    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "Negative damping ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Native DD09 negative divider matches restored assembly."
)
