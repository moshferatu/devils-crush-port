# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(CONFIG STREQUAL "Debug" OR CONFIG STREQUAL "Release")
    set(
        expected_census_sha256
        "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
    )
else()
    message(
        FATAL_ERROR
        "Unsupported scene-layout-test configuration: ${CONFIG}"
    )
endif()

set(
    expected_trace_sha256
    "8fdc6c0c9807c5bbb22a560bd6e05d9288fe7d315db8056ee24527362995b847"
)
set(
    expected_call_sha256
    "bd601b053e6328985f279f4de1354b5617ecd3ca2ebb4feac9facbd114f63ab9"
)
set(
    expected_write_sha256
    "dc65b4240bd68dd32cbf35f27da707d655ce0487551fbe94afac3aa2f895ac58"
)

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --observe-hook
        "02:4088"
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
        "Scene layout recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Observe hook 1 entries: 1"
    "Routine trace complete: 1 entries"
    "Routine call trace complete: 1 calls, 11331 writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Scene layout output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census trace call write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
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
            "Scene layout ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(STATUS "Scene layout ${artifact} verified: ${actual_sha256}")
endforeach()

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-SceneLayoutSemantics.ps1"
        -RomPath
        "${ROM}"
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
        "Scene layout semantics analysis failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Scene layout semantics verified."
    "4088: one frame-871 full-layout call initializes 88 records"
    "Layout modes: full=active; category-1 and category-2 selective entries"
    "The bounded call crosses the frame-871/872 boundary"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Scene layout semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")

# The record loop is deliberately split around the large 40F6 actor loader.
# This second route proves both bounded pieces together while retaining the
# original 4088 whole-layout artifact above as the family-level semantic
# account of all 88 initialized records.
set(loop_output "${OUTPUT}.record-loop.csv")
set(loop_work_ram_output "${loop_output}.wram.bin")
set(loop_trace_output "${loop_output}.trace.csv")
set(loop_call_output "${loop_output}.calls.csv")
set(loop_write_output "${loop_call_output}.writes.csv")
set(
    expected_loop_work_ram_sha256
    "07673ea2b1101aff61f6058780a0817ec47191ea3d2abf5559a994d14db84779"
)
set(
    expected_loop_trace_sha256
    "46865e9d5f8c4f978af84492f29c2e9700b9ca231425386f6e60a9d06815d47a"
)
set(
    expected_loop_call_sha256
    "4992a30f04799441c1fbd6ed9992864dc483d797f325a5b8dd6f5aa9f7b7bad3"
)
set(
    expected_loop_write_sha256
    "c6cf2e3a04d4ec5904acf7562a85c776e4226e354ab1fbfbe8cdfd1d2a5153ad"
)

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${loop_output}"
        --canonical-work-ram
        "${loop_work_ram_output}"
        --observe-hook
        "02:4090"
        --observe-hook
        "02:4099"
        --replace-hook
        "02:4090@all"
        --replace-hook
        "02:4099@all"
        --routine-trace
        "${loop_trace_output}"
        --routine-call-trace
        "${loop_call_output}"
        --routine-replay
        "02:4090@89"
        --routine-replay
        "02:4099@88"
        --rom
        "${ROM}"
    RESULT_VARIABLE loop_recorder_result
    OUTPUT_VARIABLE loop_recorder_output
    ERROR_VARIABLE loop_recorder_error
)
if(NOT loop_recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Scene-layout record-loop recorder failed (${loop_recorder_result}).\n"
        "${loop_recorder_output}${loop_recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 02:4090@all entries: 89 of 89"
    "Native replacement 02:4099@all entries: 88 of 88"
    "Observe hook 1 entries: 89"
    "Observe hook 2 entries: 88"
    "Routine trace complete: 177 entries"
    "Routine call trace complete: 177 calls, 988433 writes"
    "Routine replay 02:4090@89 verified: frame 872, 39 body cycles, 0 ordered writes"
    "Routine replay 02:4099@88 verified: frame 872, 117 body cycles, 2 ordered writes"
    "Routine replay complete: 2 state-normalized calls"
)
    string(FIND "${loop_recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Scene-layout record-loop output omitted '${expected_line}'.\n"
            "${loop_recorder_output}${loop_recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census work_ram trace call write)
    if(artifact STREQUAL "census")
        set(artifact_path "${loop_output}")
        set(expected_sha256 "${expected_census_sha256}")
    elseif(artifact STREQUAL "work_ram")
        set(artifact_path "${loop_work_ram_output}")
        set(expected_sha256 "${expected_loop_work_ram_sha256}")
    elseif(artifact STREQUAL "trace")
        set(artifact_path "${loop_trace_output}")
        set(expected_sha256 "${expected_loop_trace_sha256}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${loop_call_output}")
        set(expected_sha256 "${expected_loop_call_sha256}")
    else()
        set(artifact_path "${loop_write_output}")
        set(expected_sha256 "${expected_loop_write_sha256}")
    endif()

    file(SHA256 "${artifact_path}" actual_sha256)
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "Scene-layout record-loop ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(STATUS "Scene-layout record loop verified.")

# The loader itself is split around its two composition boundaries. 40F6
# selects the record's actor slot and stops before the shared C027/C574 reset;
# 40FE copies the remaining record bytes and stops after the indirect jump at
# the selected initializer. The observed call scopes still follow those
# callees to 4099, proving that the split preserves their complete effects.
set(loader_output "${OUTPUT}.record-loader.csv")
set(loader_work_ram_output "${loader_output}.wram.bin")
set(loader_trace_output "${loader_output}.trace.csv")
set(loader_call_output "${loader_output}.calls.csv")
set(loader_write_output "${loader_call_output}.writes.csv")
set(
    expected_loader_work_ram_sha256
    "07673ea2b1101aff61f6058780a0817ec47191ea3d2abf5559a994d14db84779"
)
set(
    expected_loader_trace_sha256
    "6bdca3670ba4385a2bd5c375a085a2d23af78ff9c8cc081ea36202e345a16a12"
)
set(
    expected_loader_call_sha256
    "fcd226a00fb51d7f211d83a1738fc23e39e5144b1b0e088a2481ebe4e5b37c3e"
)
set(
    expected_loader_write_sha256
    "fb50c6170d5e1ea7129538607eade76d1718eb2a22f27f638fd94639f0e8240c"
)

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${loader_output}"
        --canonical-work-ram
        "${loader_work_ram_output}"
        --observe-hook
        "02:40F6"
        --observe-hook
        "02:40FE"
        --replace-hook
        "02:4090@all"
        --replace-hook
        "02:4099@all"
        --replace-hook
        "02:40F6@all"
        --replace-hook
        "02:40FE@all"
        --routine-trace
        "${loader_trace_output}"
        --routine-call-trace
        "${loader_call_output}"
        --routine-replay
        "02:40F6@88"
        --routine-replay
        "02:40FE@88"
        --rom
        "${ROM}"
    RESULT_VARIABLE loader_recorder_result
    OUTPUT_VARIABLE loader_recorder_output
    ERROR_VARIABLE loader_recorder_error
)
if(NOT loader_recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Scene-layout record-loader recorder failed (${loader_recorder_result}).\n"
        "${loader_recorder_output}${loader_recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 02:4090@all entries: 89 of 89"
    "Native replacement 02:4099@all entries: 88 of 88"
    "Native replacement 02:40F6@all entries: 88 of 88"
    "Native replacement 02:40FE@all entries: 88 of 88"
    "Observe hook 1 entries: 88"
    "Observe hook 2 entries: 88"
    "Routine trace complete: 176 entries"
    "Routine call trace complete: 176 calls, 12722 writes"
    "Routine replay 02:40F6@88 verified: frame 872, 4458 body cycles, 121 ordered writes"
    "Routine replay 02:40FE@88 verified: frame 872, 744 body cycles, 20 ordered writes"
    "Routine replay complete: 2 state-normalized calls"
)
    string(FIND "${loader_recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Scene-layout record-loader output omitted '${expected_line}'.\n"
            "${loader_recorder_output}${loader_recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census work_ram trace call write)
    if(artifact STREQUAL "census")
        set(artifact_path "${loader_output}")
        set(expected_sha256 "${expected_census_sha256}")
    elseif(artifact STREQUAL "work_ram")
        set(artifact_path "${loader_work_ram_output}")
        set(expected_sha256 "${expected_loader_work_ram_sha256}")
    elseif(artifact STREQUAL "trace")
        set(artifact_path "${loader_trace_output}")
        set(expected_sha256 "${expected_loader_trace_sha256}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${loader_call_output}")
        set(expected_sha256 "${expected_loader_call_sha256}")
    else()
        set(artifact_path "${loader_write_output}")
        set(expected_sha256 "${expected_loader_write_sha256}")
    endif()

    file(SHA256 "${artifact_path}" actual_sha256)
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "Scene-layout record-loader ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(STATUS "Scene-layout record loader verified.")

# The two JSR instructions between the record-loop and record-loader pieces
# remain separate entries so both registered children are still independently
# scheduled. This route proves the exact return-address stack writes and the
# complete composed call scopes on all 88 records.
set(call_site_output "${OUTPUT}.record-call-sites.csv")
set(call_site_work_ram_output "${call_site_output}.wram.bin")
set(call_site_trace_output "${call_site_output}.trace.csv")
set(call_site_call_output "${call_site_output}.calls.csv")
set(call_site_write_output "${call_site_call_output}.writes.csv")
set(
    expected_call_site_work_ram_sha256
    "07673ea2b1101aff61f6058780a0817ec47191ea3d2abf5559a994d14db84779"
)
set(
    expected_call_site_trace_sha256
    "9e1e74965b2f98d5004ce1f13c4c52b1142b408800928fb81c34f0c4847b3997"
)
set(
    expected_call_site_call_sha256
    "1b626258404f1807e3129fd5ff966c3223003f072fcb5b8f762ba8f154d1092c"
)
set(
    expected_call_site_write_sha256
    "7c63133c260e7de316dc066b206e76b040cc681fdb18c2627bf5ff600d2d319e"
)

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${call_site_output}"
        --canonical-work-ram
        "${call_site_work_ram_output}"
        --observe-hook
        "02:4096"
        --observe-hook
        "02:40FB"
        --replace-hook
        "02:4090@all"
        --replace-hook
        "02:4096@all"
        --replace-hook
        "02:4099@all"
        --replace-hook
        "02:40F6@all"
        --replace-hook
        "02:40FB@all"
        --replace-hook
        "02:40FE@all"
        --routine-trace
        "${call_site_trace_output}"
        --routine-call-trace
        "${call_site_call_output}"
        --routine-replay
        "02:4096@88"
        --routine-replay
        "02:40FB@88"
        --rom
        "${ROM}"
    RESULT_VARIABLE call_site_recorder_result
    OUTPUT_VARIABLE call_site_recorder_output
    ERROR_VARIABLE call_site_recorder_error
)
if(NOT call_site_recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Scene-layout call-site recorder failed (${call_site_recorder_result}).\n"
        "${call_site_recorder_output}${call_site_recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 02:4090@all entries: 89 of 89"
    "Native replacement 02:4096@all entries: 88 of 88"
    "Native replacement 02:4099@all entries: 88 of 88"
    "Native replacement 02:40F6@all entries: 88 of 88"
    "Native replacement 02:40FB@all entries: 88 of 88"
    "Native replacement 02:40FE@all entries: 88 of 88"
    "Observe hook 1 entries: 88"
    "Observe hook 2 entries: 88"
    "Routine trace complete: 176 entries"
    "Routine call trace complete: 176 calls, 510770 writes"
    "Routine replay 02:4096@88 verified: frame 872, 4617 body cycles, 125 ordered writes"
    "Routine replay 02:40FB@88 verified: frame 872, 4425 body cycles, 121 ordered writes"
    "Routine replay complete: 2 state-normalized calls"
)
    string(FIND "${call_site_recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Scene-layout call-site output omitted '${expected_line}'.\n"
            "${call_site_recorder_output}${call_site_recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census work_ram trace call write)
    if(artifact STREQUAL "census")
        set(artifact_path "${call_site_output}")
        set(expected_sha256 "${expected_census_sha256}")
    elseif(artifact STREQUAL "work_ram")
        set(artifact_path "${call_site_work_ram_output}")
        set(expected_sha256 "${expected_call_site_work_ram_sha256}")
    elseif(artifact STREQUAL "trace")
        set(artifact_path "${call_site_trace_output}")
        set(expected_sha256 "${expected_call_site_trace_sha256}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${call_site_call_output}")
        set(expected_sha256 "${expected_call_site_call_sha256}")
    else()
        set(artifact_path "${call_site_write_output}")
        set(expected_sha256 "${expected_call_site_write_sha256}")
    endif()

    file(SHA256 "${artifact_path}" actual_sha256)
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "Scene-layout call-site ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(STATUS "Scene-layout call sites verified.")

# The sole terminal record-loop RTS returns through the banked-call wrapper.
# Its absorbed-return declaration closes the observer at the original RTS
# boundary, so its replay body is intentionally zero even though the native
# transcript owns the instruction's exact 21 master cycles.
set(return_output "${OUTPUT}.record-return.csv")
set(return_work_ram_output "${return_output}.wram.bin")
set(return_trace_output "${return_output}.trace.csv")
set(return_call_output "${return_output}.calls.csv")
set(return_write_output "${return_call_output}.writes.csv")
set(
    expected_return_work_ram_sha256
    "07673ea2b1101aff61f6058780a0817ec47191ea3d2abf5559a994d14db84779"
)
set(
    expected_return_trace_sha256
    "649c39b34b73221b20bc54a5c8a339feb30f78775bc0677b1cb47a25bda5c2df"
)
set(
    expected_return_call_sha256
    "a33605ed577287edeeb10759a247d57aaa2e810ab07d4aac5e1525d2153b1631"
)
set(
    expected_return_write_sha256
    "f55c17783e8e119c2568a9711c5c0706fa3ff5f7f881e0727f029f321044982c"
)

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${return_output}"
        --canonical-work-ram
        "${return_work_ram_output}"
        --observe-hook
        "02:40A7"
        --replace-hook
        "02:4090@all"
        --replace-hook
        "02:4096@all"
        --replace-hook
        "02:4099@all"
        --replace-hook
        "02:40A7@all"
        --replace-hook
        "02:40F6@all"
        --replace-hook
        "02:40FB@all"
        --replace-hook
        "02:40FE@all"
        --routine-trace
        "${return_trace_output}"
        --routine-call-trace
        "${return_call_output}"
        --routine-replay
        "02:40A7@1"
        --rom
        "${ROM}"
    RESULT_VARIABLE return_recorder_result
    OUTPUT_VARIABLE return_recorder_output
    ERROR_VARIABLE return_recorder_error
)
if(NOT return_recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Scene-layout return recorder failed (${return_recorder_result}).\n"
        "${return_recorder_output}${return_recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 02:4090@all entries: 89 of 89"
    "Native replacement 02:4096@all entries: 88 of 88"
    "Native replacement 02:4099@all entries: 88 of 88"
    "Native replacement 02:40A7@all entries: 1 of 1"
    "Native replacement 02:40F6@all entries: 88 of 88"
    "Native replacement 02:40FB@all entries: 88 of 88"
    "Native replacement 02:40FE@all entries: 88 of 88"
    "Observe hook 1 entries: 1"
    "Routine trace complete: 1 entries"
    "Routine call trace complete: 1 calls, 0 writes"
    "Routine replay 02:40A7@1 verified: frame 872, 0 body cycles, 0 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${return_recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Scene-layout return output omitted '${expected_line}'.\n"
            "${return_recorder_output}${return_recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census work_ram trace call write)
    if(artifact STREQUAL "census")
        set(artifact_path "${return_output}")
        set(expected_sha256 "${expected_census_sha256}")
    elseif(artifact STREQUAL "work_ram")
        set(artifact_path "${return_work_ram_output}")
        set(expected_sha256 "${expected_return_work_ram_sha256}")
    elseif(artifact STREQUAL "trace")
        set(artifact_path "${return_trace_output}")
        set(expected_sha256 "${expected_return_trace_sha256}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${return_call_output}")
        set(expected_sha256 "${expected_return_call_sha256}")
    else()
        set(artifact_path "${return_write_output}")
        set(expected_sha256 "${expected_return_write_sha256}")
    endif()

    file(SHA256 "${artifact_path}" actual_sha256)
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "Scene-layout return ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(STATUS "Scene-layout terminal return verified.")
