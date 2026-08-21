# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(CONFIG STREQUAL "Debug")
    set(
        expected_census_sha256
        "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
    )
elseif(CONFIG STREQUAL "Release")
    set(
        expected_census_sha256
        "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
    )
else()
    message(
        FATAL_ERROR
        "Unsupported Small-Soldier-source-test configuration: ${CONFIG}"
    )
endif()

set(
    expected_trace_sha256
    "11a88d0196446e3f16a81c7a1cdc81de66ca4016ed06abc09dc3e15a0fcfc8b1"
)
set(
    expected_call_sha256
    "c65e2679394e1dea185519f6dbff45ac0c129bc195a695d66a469c0dd6c2d8ff"
)
set(
    expected_write_sha256
    "336a25613adff3f64c608761a9cf6c1cea74053704cf6625067d9d1bb41ee774"
)
set(
    expected_actor_write_sha256
    "ab06bfa930972c7aca88244ce26bdf22b73eb0cf915cb6b5f9e2a085d1ee8af8"
)

set(trace_output "${OUTPUT}.trace.csv")
set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(actor_write_output "${OUTPUT}.actors.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --observe-hook
        "03:7D4D"
        --observe-hook
        "03:7D6F"
        --observe-hook
        "03:7D85"
        --observe-hook
        "03:7DE9"
        --observe-hook
        "03:7E0D"
        --observe-hook
        "03:7E23"
        --observe-hook
        "03:7E87"
        --observe-hook
        "03:7F81"
        --replace-hook
        "03:7D4D@all"
        --replace-hook
        "03:7DE9@all"
        --replace-hook
        "03:7D6F@all"
        --replace-hook
        "03:7E0D@all"
        --replace-hook
        "03:7E87@all"
        --replace-hook
        "03:7E8A@all"
        --replace-hook
        "03:7EEE@all"
        --replace-hook
        "03:7EFA@all"
        --replace-hook
        "03:7F05@all"
        --routine-trace
        "${trace_output}"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "03:7D4D@1"
        --routine-replay
        "03:7DE9@1"
        --routine-replay
        "03:7D85@1"
        --routine-replay
        "03:7E23@1"
        --routine-replay
        "03:7E87@1"
        --routine-replay
        "03:7F81@1"
        --routine-replay
        "03:7D6F@2"
        --routine-replay
        "03:7E0D@2"
        --work-ram-write-range
        "2D16-2D17"
        --work-ram-write-range
        "3000-307F"
        --work-ram-write-range
        "3F00-3FFF"
        --work-ram-write-trace
        "${actor_write_output}"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Small-Soldier source recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

set(expected_hook_counts 1 193 2457 1 193 2457 10 10)
set(hook_index 1)
foreach(expected_count IN LISTS expected_hook_counts)
    set(expected_line "Observe hook ${hook_index} entries: ${expected_count}")
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Small-Soldier source output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
    math(EXPR hook_index "${hook_index} + 1")
endforeach()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 03:7D4D@all entries: 1 of 1"
    "Native replacement 03:7DE9@all entries: 1 of 1"
    "Native replacement 03:7D6F@all entries: 155 of 193"
    "Native replacement 03:7E0D@all entries: 161 of 193"
    "Native replacement 03:7E87@all entries: 10 of 10"
    "Native replacement 03:7E8A@all entries: 10 of 10"
    "Native replacement 03:7EEE@all entries: 10 of 10"
    "Native replacement 03:7EFA@all entries: 160 of 160"
    "Native replacement 03:7F05@all entries: 10 of 10"
    "Routine trace complete: 5322 entries"
    "Routine call trace complete: 5322 calls, 3324 writes"
    "Routine replay 03:7D4D@1 verified: frame 872, 327 body cycles, 14 ordered writes"
    "Routine replay 03:7DE9@1 verified: frame 872, 333 body cycles, 14 ordered writes"
    "Routine replay 03:7D6F@2 verified: frame 941, 33 body cycles, 1 ordered writes"
    "Routine replay 03:7E0D@2 verified: frame 941, 33 body cycles, 1 ordered writes"
    "Routine replay 03:7D85@1 verified: frame 1133"
    "Routine replay 03:7E23@1 verified: frame 1133"
    "Routine replay 03:7E87@1 verified: frame 873"
    "Routine replay 03:7F81@1 verified: frame 1852"
    "Routine replay complete: 8 state-normalized calls"
    "Work-RAM write trace complete: 3888 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Small-Soldier source output omitted '${expected_line}'.\n"
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
            "Small-Soldier source ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
    message(
        STATUS
        "Small-Soldier source ${artifact} verified: ${actual_sha256}"
    )
endforeach()

file(SHA256 "${actor_write_output}" actual_actor_write_sha256)
if(NOT actual_actor_write_sha256 STREQUAL expected_actor_write_sha256)
    message(
        FATAL_ERROR
        "Small-Soldier source selected-state artifact changed.\n"
        "Expected: ${expected_actor_write_sha256}\n"
        "Actual:   ${actual_actor_write_sha256}\n"
    )
endif()
message(
    STATUS
    "Small-Soldier source selected state verified: "
    "${actual_actor_write_sha256}"
)

execute_process(
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy
        Bypass
        -File
        "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-SmallSoldierSourceSemantics.ps1"
        -RomPath
        "${ROM}"
        -CallPath
        "${call_output}"
        -WritePath
        "${write_output}"
        -ActorWritePath
        "${actor_write_output}"
    RESULT_VARIABLE semantics_result
    OUTPUT_VARIABLE semantics_output
    ERROR_VARIABLE semantics_error
)
if(NOT semantics_result EQUAL 0)
    message(
        FATAL_ERROR
        "Small-Soldier source semantics failed (${semantics_result}).\n"
        "${semantics_output}${semantics_error}"
    )
endif()
foreach(expected_line IN ITEMS
    "Small-Soldier Side-Star source semantics verified."
    "Fixed slots 34/35 create paired five-member groups"
    "Group counters 2D16/2D17 rise from 0 to 5"
    "7F81: slot 62 exits immediately"
    "ROM-bounded inactive rule: clearing all five awards 200"
    "Normalized samples: 03:7D85@1"
)
    string(FIND "${semantics_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Small-Soldier source semantics omitted '${expected_line}'.\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
endforeach()
message(STATUS "${semantics_output}")

# The child startup is a distinct coroutine scope from the source/spawner
# artifact above. Observe it separately so its 170 calls and 1,290 writes can
# be pinned without changing the long-standing source-semantics sequence
# numbers consumed by Analyze-SmallSoldierSourceSemantics.ps1.
set(deployment_output "${OUTPUT}.deployment.csv")
set(deployment_call_output "${deployment_output}.calls.csv")
set(deployment_write_output "${deployment_call_output}.writes.csv")
set(deployment_abandonment_output "${deployment_output}.abandonments.csv")
set(expected_deployment_call_sha256 "fa079a81def9c6bcce62ef43c2dc85b0b77976b6743e26eb7f142415f89aacb2")
set(expected_deployment_write_sha256 "6cba36d5308d349cf3b07c45e56012aa2f245c207988deca8c3ac5787559b96e")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${deployment_output}"
        --observe-hook
        "03:7EFA"
        --observe-hook
        "03:7F05"
        --replace-hook
        "03:7D4D@all"
        --replace-hook
        "03:7DE9@all"
        --replace-hook
        "03:7D6F@all"
        --replace-hook
        "03:7E0D@all"
        --replace-hook
        "03:7E87@all"
        --replace-hook
        "03:7E8A@all"
        --replace-hook
        "03:7EEE@all"
        --replace-hook
        "03:7EFA@all"
        --replace-hook
        "03:7F05@all"
        --routine-call-trace
        "${deployment_call_output}"
        --routine-replay
        "03:7EFA@1"
        --routine-replay
        "03:7EFA@16"
        --routine-replay
        "03:7F05@1"
        --abandonment-census
        "${deployment_abandonment_output}"
        --rom
        "${ROM}"
    RESULT_VARIABLE deployment_result
    OUTPUT_VARIABLE deployment_stdout
    ERROR_VARIABLE deployment_stderr
)
if(NOT deployment_result EQUAL 0)
    message(
        FATAL_ERROR
        "Small-Soldier deployment recorder failed (${deployment_result}).\n"
        "${deployment_stdout}${deployment_stderr}"
    )
endif()
foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 03:7EFA@all entries: 160 of 160"
    "Native replacement 03:7F05@all entries: 10 of 10"
    "Observe hook 1 entries: 160"
    "Observe hook 2 entries: 10"
    "Routine call trace complete: 170 calls, 1290 writes"
    "Routine replay 03:7EFA@1 verified: frame 873, 171 body cycles, 6 ordered writes"
    "Routine replay 03:7EFA@16 verified: frame 947, 171 body cycles, 6 ordered writes"
    "Routine replay 03:7F05@1 verified: frame 956, 270 body cycles, 10 ordered writes"
    "Routine replay complete: 3 state-normalized calls"
    "Abandonment census complete: no transcript was abandoned"
)
    string(FIND "${deployment_stdout}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Small-Soldier deployment output omitted '${expected_line}'.\n"
            "${deployment_stdout}${deployment_stderr}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census call write)
    if(artifact STREQUAL "census")
        set(artifact_path "${deployment_output}")
        set(expected_sha256 "${expected_census_sha256}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${deployment_call_output}")
        set(expected_sha256 "${expected_deployment_call_sha256}")
    else()
        set(artifact_path "${deployment_write_output}")
        set(expected_sha256 "${expected_deployment_write_sha256}")
    endif()
    file(SHA256 "${artifact_path}" actual_sha256)
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "Small-Soldier deployment ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()
message(STATUS "Small-Soldier deployment verified: 170 native calls, 1,290 exact writes.")
