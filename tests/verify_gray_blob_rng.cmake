# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(CONFIG STREQUAL "Debug")
    set(
        expected_census_sha256
        "ac639564102a7549bc6aaa58aabb32e3231afafc5a3e3defb8d2a383a07bbdfb"
    )
elseif(CONFIG STREQUAL "Release")
    set(
        expected_census_sha256
        "97f5089683bb67c53a3755a2d320ce1bcf5ea5e347d65200373a3f22b69cbc4b"
    )
else()
    message(FATAL_ERROR "Unsupported Gray-Blob-RNG configuration: ${CONFIG}")
endif()

set(
    expected_trace_sha256
    "850913e661da8ff0d69bbde28348306f3f0c5b94d22679d4d23a85101d007f14"
)
set(
    expected_call_sha256
    "490219cccac245228dd86079f6be422b9d7f9518f5d31d7743d5792376c49166"
)
set(
    expected_write_sha256
    "6903bb8c1f2ceffad57d213e461a3cd29e0282a74800e3a4d9ffd5e0929c99bb"
)
set(
    expected_state_sha256
    "1481a117b3597d134c449033f56012c89f24ab87edc12081e7c3ee74d847e81f"
)

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(state_output "${OUTPUT}.state.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-gameplay "${OUTPUT}"
        --work-ram-patch "1300:2CC0:80"
        --observe-hook "03:68A1"
        --observe-hook "03:68B1"
        --observe-hook "00:E8B3"
        --routine-trace "${trace_output}"
        --routine-call-trace "${call_output}"
        --routine-replay "03:68A1@1"
        --work-ram-write-range "222A-222B"
        --work-ram-write-range "30D0-30D6"
        --work-ram-write-range "3850-3856"
        --work-ram-write-range "39D0-39D6"
        --work-ram-write-range "3A50-3A56"
        --work-ram-write-range "3BD0-3BD6"
        --work-ram-write-range "3F50-3F56"
        --work-ram-write-range "3FD0-3FD6"
        --work-ram-write-trace "${state_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Gray-Blob RNG recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "Gameplay recording complete: 1800 frames"
    "Work-RAM patches applied: 1"
    "Observe hook 1 entries: 7"
    "Observe hook 2 entries: 7"
    "Observe hook 3 entries: 62"
    "Routine trace complete: 76 entries"
    "Routine call trace complete: 76 calls, 762 writes"
    "Work-RAM write trace complete: 3866 selected writes"
    "Routine replay 03:68A1@1 verified: frame 1541, 633 body cycles"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Gray-Blob RNG output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census trace call write state)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "trace")
        set(artifact_path "${trace_output}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    elseif(artifact STREQUAL "write")
        set(artifact_path "${write_output}")
    else()
        set(artifact_path "${state_output}")
    endif()

    file(SHA256 "${artifact_path}" actual_sha256)
    set("actual_${artifact}_sha256" "${actual_sha256}")
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "Gray-Blob RNG ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}"
        )
    endif()
endforeach()

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-GrayBlobRngSemantics.ps1"
        -RomPath "${ROM}"
        -CallPath "${call_output}"
        -WritePath "${write_output}"
        -StatePath "${state_output}"
    RESULT_VARIABLE semantics_result
    OUTPUT_VARIABLE semantics_output
    ERROR_VARIABLE semantics_error
)
if(NOT semantics_result EQUAL 0)
    message(
        FATAL_ERROR
        "Gray-Blob RNG analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Gray-Blob RNG semantics verified."
    "Natural activation: slots 50-56 enter 03:68A1 across frames 1541-1547."
    "RNG consumer 03:68B1: seven results produce timers 46,50,47,50,56,52,50."
    "Call contract: every first live update is 633 cycles and 20 writes."
    "First normalized sample: 03:68A1@1, frame 1541, 633 cycles."
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Gray-Blob RNG semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(
    STATUS
    "Gray-Blob RNG artifacts verified: "
    "${actual_census_sha256}, ${actual_trace_sha256}, "
    "${actual_call_sha256}, ${actual_write_sha256}, ${actual_state_sha256}\n"
    "${semantics_output}"
)
