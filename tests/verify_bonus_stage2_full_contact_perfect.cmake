# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(
        FATAL_ERROR
        "BS-2 full-contact coverage is Release-only, not ${CONFIG}."
    )
endif()

set(
    expected_census_sha256
    "533a642e33244ce95f9ab9ea9b42f9635600c463346b0dc52928c07326222a5d"
)
set(
    expected_entry_sha256
    "9c45ebf3663a8b85dd4446c3c2026664f86c4813fcdeb3fcd69b093604a30c2e"
)
set(
    expected_state_sha256
    "3469ef08e3600de588b45d613dd0363f6dc27d01fa6763296594c416df73d34b"
)

# Collision helper 04:5A12 scans $90 positions backward from each seeded X.
# These are therefore the inverse-X seeds for the ROM's 28 valid board cells.
set(
    contact_seeds
    "F0:3C" "00:3C" "10:3C" "20:3C"
    "30:3C" "40:3C" "50:3C" "60:3C"
    "F0:4C" "60:4C"
    "F0:5C" "00:5C" "10:5C" "20:5C"
    "30:5C" "40:5C" "50:5C" "60:5C"
    "F0:6C" "60:6C"
    "F0:7C" "00:7C" "10:7C" "20:7C"
    "30:7C" "40:7C" "50:7C" "60:7C"
)

set(
    recorder_command
    "${APP}"
    --record-state-census "${OUTPUT}"
    --work-ram-patch "1200:2055:02"
    --work-ram-patch "1200:2053:10"
)
set(frame 1304)
foreach(seed IN LISTS contact_seeds)
    string(REPLACE ":" ";" coordinates "${seed}")
    list(GET coordinates 0 x_position)
    list(GET coordinates 1 y_position)
    foreach(patch IN ITEMS
        "3053:C2"
        "30D3:84"
        "31D3:${x_position}"
        "3253:00"
        "3353:${y_position}"
        "33D3:00"
        "3753:11"
        "34D3:00"
        "3853:90"
        "3D53:81"
        "3DD3:10"
        "3C53:01"
        "3953:1D"
        "3F53:79"
        "3FD3:50"
        "2D26:01"
    )
        list(
            APPEND recorder_command
            --work-ram-patch "${frame}:${patch}"
        )
    endforeach()
    math(EXPR frame "${frame} + 1")
endforeach()

set(entry_output "${OUTPUT}.entries.csv")
set(state_output "${OUTPUT}.state.csv")
list(
    APPEND recorder_command
    --observe-hook "04:4E8F"
    --observe-hook "04:4E97"
    --observe-hook "04:5079"
    --observe-hook "04:5099"
    --observe-hook "04:5260"
    --observe-hook "04:52ED"
    --observe-hook "04:44DB"
    --observe-hook "04:4555"
    --observe-hook "04:456C"
    --observe-hook "04:464F"
    --routine-trace "${entry_output}"
    --work-ram-write-range "2D00-2DFF"
    --work-ram-write-range "31D3-31D3"
    --work-ram-write-range "3353-3353"
    --work-ram-write-trace "${state_output}"
    --rom "${ROM}"
)
execute_process(
    COMMAND ${recorder_command}
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "BS-2 full-contact recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 450"
    "Observe hook 1 entries: 1"
    "Observe hook 2 entries: 30"
    "Observe hook 3 entries: 28"
    "Observe hook 4 entries: 28"
    "Observe hook 5 entries: 28"
    "Observe hook 6 entries: 112"
    "Observe hook 7 entries: 1"
    "Observe hook 8 entries: 87"
    "Observe hook 9 entries: 1"
    "Observe hook 10 entries: 1"
    "Routine trace complete: 317 entries"
    "Work-RAM write trace complete: 10705 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-2 full-contact output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census entry state)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "entry")
        set(artifact_path "${entry_output}")
    else()
        set(artifact_path "${state_output}")
    endif()
    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "BS-2 full-contact ${artifact} artifact changed.\n"
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
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-BonusStage2ContactPerfectSemantics.ps1"
        -RomPath "${ROM}"
        -EntryPath "${entry_output}"
        -StatePath "${state_output}"
        -FullEnvelope
    RESULT_VARIABLE semantics_result
    OUTPUT_VARIABLE semantics_output
    ERROR_VARIABLE semantics_error
)
if(NOT semantics_result EQUAL 0)
    message(
        FATAL_ERROR
        "BS-2 full-contact analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "BS-2 full-envelope perfect outcome verified."
    "Collision envelope: 28 entries at 5079"
    "Board completion: 28 post-collision target continuations"
    "Perfect latch: the 87-step completion animation writes 2DBA at 4560"
    "Awards: 12,600 earned units transfer first"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "BS-2 full-contact semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()

message(
    STATUS
    "BS-2 full-contact artifacts verified.\n${semantics_output}"
)
