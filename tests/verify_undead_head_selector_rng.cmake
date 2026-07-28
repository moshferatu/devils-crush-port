# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(CONFIG STREQUAL "Debug")
    set(
        expected_census_sha256
        "02311387bcb1475c3b21aa53e68d961a2f7240de5d662abf687150b6250c479d"
    )
elseif(CONFIG STREQUAL "Release")
    set(
        expected_census_sha256
        "6bbb0d9dff3adc62427eba41e04f42ff7d7cd3442974c4a43f006def7abefaac"
    )
else()
    message(
        FATAL_ERROR
        "Unsupported Undead-Head-selector-RNG configuration: ${CONFIG}"
    )
endif()

set(
    expected_trace_sha256
    "3ec5b243994df4fa327d815f3ef9674c87e4d96c9d6a809a99dd6de3c682cf14"
)
set(
    expected_call_sha256
    "4678472d2eaea7be13f447d7510feddb745f0464667e5d5807fabf7f2a9e0152"
)
set(
    expected_write_sha256
    "7b6ce462d254f75bb02160c228926ebeca1a9cd8e29b4a7ce0e86a6d8c296cb8"
)
set(
    expected_state_sha256
    "562c11ecc766cf832f6a00cf9aff1e384963addcad8234098c086f5ccd3a7d3d"
)

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(state_output "${OUTPUT}.state.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census "${OUTPUT}"
        --work-ram-patch "1200:334D:2C"
        --work-ram-patch "1200:33CD:01"
        --work-ram-patch "1200:3F4D:48"
        --work-ram-patch "1200:3FCD:62"
        --observe-hook "03:6248"
        --observe-hook "03:625D"
        --observe-hook "00:E8B3"
        --routine-trace "${trace_output}"
        --routine-call-trace "${call_output}"
        --routine-replay "03:6248@1"
        --routine-replay "03:625D@1"
        --work-ram-write-range "222A-222B"
        --work-ram-write-range "3000-3000"
        --work-ram-write-range "3080-3080"
        --work-ram-write-range "3800-3800"
        --work-ram-write-range "3880-3880"
        --work-ram-write-range "3C00-3C00"
        --work-ram-write-range "3C80-3C80"
        --work-ram-write-range "304D-304D"
        --work-ram-write-range "30CD-30CD"
        --work-ram-write-range "334D-334D"
        --work-ram-write-range "33CD-33CD"
        --work-ram-write-range "3C4D-3C4D"
        --work-ram-write-range "3CCD-3CCD"
        --work-ram-write-range "3E4D-3E4D"
        --work-ram-write-range "3ECD-3ECD"
        --work-ram-write-range "3F4D-3F4D"
        --work-ram-write-range "3FCD-3FCD"
        --work-ram-write-trace "${state_output}"
        --rom "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Undead-Head selector RNG recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 4"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 1"
    "Observe hook 3 entries: 150"
    "Routine trace complete: 152 entries"
    "Routine call trace complete: 152 calls, 1386 writes"
    "Work-RAM write trace complete: 7807 selected writes"
    "Routine replay 03:6248@1 verified: frame 1221, 903 body cycles"
    "Routine replay 03:625D@1 verified: frame 1221, 810 body cycles"
    "Routine replay complete: 2 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Undead-Head selector RNG output omitted '${expected_line}'.\n"
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
            "Undead-Head selector RNG ${artifact} artifact changed.\n"
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
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-UndeadHeadSelectorRngSemantics.ps1"
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
        "Undead-Head selector RNG analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Undead-Head selector RNG semantics verified."
    "Activation: fixed slot 4D enters post-trigger state 03:6248"
    "RNG consumer 03:625D: result 73 selects response value 09."
    "Continuation: stores 6279 and enters the shared 4D2D response state."
    "Normalized samples: 03:6248@1 is 903 cycles"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Undead-Head selector RNG semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(
    STATUS
    "Undead-Head selector RNG artifacts verified: "
    "${actual_census_sha256}, ${actual_trace_sha256}, "
    "${actual_call_sha256}, ${actual_write_sha256}, ${actual_state_sha256}\n"
    "${semantics_output}"
)
