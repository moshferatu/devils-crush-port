# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(CONFIG STREQUAL "Debug")
    set(
        expected_census_sha256
        "a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2"
    )
elseif(CONFIG STREQUAL "Release")
    set(
        expected_census_sha256
        "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
    )
else()
    message(FATAL_ERROR "Unsupported controls/physics configuration: ${CONFIG}")
endif()

set(
    expected_work_ram_sha256
    "07673ea2b1101aff61f6058780a0817ec47191ea3d2abf5559a994d14db84779"
)
set(
    expected_trace_sha256
    "1a7a40ad5aeb86b7612a17db10e789b0d169ce020670f23165f45de46f316077"
)
set(
    expected_call_sha256
    "48c4bf3c99233301cd8b3408116436f2b7960a7e0fd4a441ca3162c692f295ca"
)
set(
    expected_write_sha256
    "7bca93abffaef5b85e855434c802fd03c1b75f6eca6eac9d0d9410eaf782846a"
)

set(work_ram_output "${OUTPUT}.wram.bin")
set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --canonical-work-ram
        "${work_ram_output}"
        --observe-hook
        "02:5159"
        --observe-hook
        "01:D466"
        --observe-hook
        "01:C3CB"
        --observe-hook
        "00:F1B4"
        --observe-hook
        "01:DD09"
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
        "Controls/physics recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Observe hook 1 entries: 101"
    "Observe hook 2 entries: 7470"
    "Observe hook 3 entries: 2649"
    "Observe hook 4 entries: 3568"
    "Observe hook 5 entries: 2540"
    "Routine trace complete: 16328 entries"
    "Routine call trace complete: 16328 calls, 478079 writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Controls/physics output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census work_ram trace call write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "work_ram")
        set(artifact_path "${work_ram_output}")
    elseif(artifact STREQUAL "trace")
        set(artifact_path "${trace_output}")
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
            "Controls/physics ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(STATUS "Controls/physics ${artifact} verified: ${actual_sha256}")
endforeach()

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-ControlsPhysicsRoute.ps1"
        -RomPath
        "${ROM}"
        -RecordingPath
        "${OUTPUT}"
        -WorkRamPath
        "${work_ram_output}"
        -CallPath
        "${call_output}"
        -WritePath
        "${write_output}"
    RESULT_VARIABLE semantics_result
    OUTPUT_VARIABLE semantics_output
    ERROR_VARIABLE semantics_error
)
if(NOT semantics_result EQUAL 0)
    message(
        FATAL_ERROR
        "Controls/physics analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Controls-to-physics route verified."
    "Launcher: Button I charges through frame 1180"
    "Flippers: right slot 09 responds at frame 1265"
    "Nudge: Button II produces three C3CB motion pairs"
    "Tilt lock: $2CC7 remains zero through all 3600 frames."
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Controls/physics analysis omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")
