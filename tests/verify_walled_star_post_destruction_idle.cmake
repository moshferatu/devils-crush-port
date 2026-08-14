# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Walled Star post-destruction idle regression is Release-only.")
endif()

set(expected_census_sha256 "432af3cde4152def8c9afc6845b9575c8812859e76330e8de1f3dd61c9a24ee6")
set(expected_call_sha256 "8e00417687beaa956c27a505a8ed593d96f6bf4b618f2dd4abaeb55a5f561538")
set(expected_write_sha256 "1df166a843eb05bb8f258d942b6c0999b14dee6c5c723276dab8702b55b96b10")

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --observe-hook
        "03:638F"
        --replace-hook
        "03:638F@all"
        --routine-call-trace
        "${call_output}"
        --work-ram-patch
        "1200:3F5B:8F"
        --work-ram-patch
        "1200:3FDB:63"
        --work-ram-patch
        "1200:2D53:00"
        --work-ram-patch
        "1200:2D32:00"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Walled Star post-destruction idle recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 4"
    "Native replacement 03:638F@all entries: 2390 of 2390"
    "Observe hook 1 entries: 2390"
    "Routine call trace complete: 2390 calls, 482 writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Walled Star post-destruction idle output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
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
        message(
            FATAL_ERROR
            "Walled Star post-destruction idle ${artifact} artifact changed.\n"
            "Expected: ${expected_${artifact}_sha256}\n"
            "Actual:   ${actual_sha256}"
        )
    endif()
    message(STATUS "Walled Star post-destruction idle ${artifact} verified: ${actual_sha256}")
endforeach()

message(
    STATUS
    "Walled Star post-destruction idle verified: both clear access latches resume at original 03:6399 RTS."
)
