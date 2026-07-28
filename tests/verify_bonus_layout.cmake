# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

set(
    expected_census_sha256
    e660ac5620fefbc65147496ec757893e9e599b60ad7635dcb0ee69b1f476dec4
    c0aa885ddd4df8e8993ccf532f567123dea9963150650c0d77e8d6184c8058ad
    00a662c5345b7f947a80a6ba76044f6072ee8cc5545819240ae799d27502a7a7
    50f207dcc16f29847d78a93e4926d1c4025a8403c84a2aea94fd320b2d44c664
    b1cae240c84fa295db170ec70b8505a002e35c0533af9040e361e73e4e3b0a9b
    f44d8b962bb2ac3268d645c72f7e1499df1f325807d424a468c10a8585eacd88
)
set(
    expected_trace_sha256
    a30e5fc5162f36d89200b3d61a734ca007b407bca2b494cbce41f2c66d81f32c
    3ab1c51f3fc8c8efd459a45b619ab080ca8624f77ec1d2590c7b23cc7ca91515
    fecbaee53b5e5e5e135224ef97ad23e23d91b0d23da6592c8533b70ba5e55527
    20804e102f570e27a194e1d342bf7bcfa19908534b05aa640e10d55c6df2b51d
    1ce5d75d53753c976f312ba2c67150ec686871468ec92868da849ccecc7d0458
    2fd787da5c71db04278d770c4379fa233cfa49c539ee88149386cf69e022dd05
)
set(
    expected_call_sha256
    c24f6f1ad4401d5458aee026f450d3242c2463c5f34c469de3c1946a9b5f6228
    fd5313039b98d04368745154c10ec73fb452d480bb43f84c20925607406a4a14
    9170d77e3e9a813a65276412af6729a6740c058ac9208a7200251bf7c982f6e8
    3fd226ad7d158c17a8fd71ceaf62817e37d5609372ea2baac43e10371f820bc5
    34290c06d909ea5c98e5e174ff8606083783d7dc0f38854c0c739f3ab12c5682
    82b5a21200e896b013f11f67202cf7b35b70dc20d6bc9b490e1725b77b89de82
)
set(
    expected_write_sha256
    f30a8788d4b692c0ea6b6d59fb2934c6fe15e63bae17d1937dfa737dbe0d5f6e
    b076c5745099a22a100637e4eb7b547fec3607a288a27948bd719126ea83693e
    7278eb58f5e67a373edc4b10a179aafaf567f6ed858ec3c3dbce4e06a51ad21e
    0586daab718c35f7c75fa6db65489b69db18569d30a6a402ed0354b2e80586b1
    1db44cd39db93df729646625337764135852ec5c80741914d72661d94221bfb3
    0ea22ddd0002fffc5b2df32e1c2a418dd48574b7bd891e4d44184235c0a5b7a6
)
set(expected_write_counts 590 305 422 600 716 278)

if(CONFIG STREQUAL "Debug")
    # All six tables are guarded statically by the analyzer. BS-1 supplies
    # the cross-configuration dynamic oracle without adding five more
    # two-minute unoptimized recordings.
    set(stages 1)
elseif(CONFIG STREQUAL "Release")
    set(stages 1 2 3 4 5 6)
else()
    message(FATAL_ERROR "Unsupported bonus-layout configuration: ${CONFIG}")
endif()

foreach(stage IN LISTS stages)
    math(EXPR stage_index "${stage} - 1")
    list(GET expected_census_sha256 ${stage_index} expected_census)
    list(GET expected_trace_sha256 ${stage_index} expected_trace)
    list(GET expected_call_sha256 ${stage_index} expected_call)
    list(GET expected_write_sha256 ${stage_index} expected_write)
    list(GET expected_write_counts ${stage_index} expected_write_count)

    set(stage_output "${OUTPUT}.bs${stage}.csv")
    set(trace_output "${stage_output}.trace.csv")
    set(call_output "${stage_output}.calls.csv")
    set(write_output "${call_output}.writes.csv")
    set(extra_hook)
    set(extra_output)
    if(stage EQUAL 1)
        set(extra_hook --pass-through-hook "04:47C7")
        set(extra_output "Pass-through hook 1 entries: 5")
    elseif(stage EQUAL 6)
        set(extra_hook --pass-through-hook "05:7217")
        set(extra_output "Pass-through hook 1 entries: 2")
    endif()

    execute_process(
        COMMAND
            "${APP}"
            --record-state-census
            "${stage_output}"
            --work-ram-patch
            "1300:2055:0${stage}"
            --work-ram-patch
            "1300:2053:10"
            --observe-hook
            "04:4000"
            ${extra_hook}
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
            "BS-${stage} layout recorder failed (${recorder_result}).\n"
            "${recorder_output}${recorder_error}"
        )
    endif()

    foreach(expected_line IN ITEMS
        "State census recording complete: 3600 frames"
        "Work-RAM patches applied: 2"
        "Observe hook 1 entries: 1"
        "Routine trace complete: 1 entries"
        "Routine call trace complete: 1 calls, ${expected_write_count} writes"
        "${extra_output}"
    )
        if(expected_line STREQUAL "")
            continue()
        endif()
        string(FIND "${recorder_output}" "${expected_line}" line_offset)
        if(line_offset EQUAL -1)
            message(
                FATAL_ERROR
                "BS-${stage} output omitted '${expected_line}'.\n"
                "${recorder_output}${recorder_error}"
            )
        endif()
    endforeach()

    foreach(artifact IN ITEMS census trace call write)
        if(artifact STREQUAL "census")
            set(artifact_path "${stage_output}")
        elseif(artifact STREQUAL "trace")
            set(artifact_path "${trace_output}")
        elseif(artifact STREQUAL "call")
            set(artifact_path "${call_output}")
        else()
            set(artifact_path "${write_output}")
        endif()
        file(SHA256 "${artifact_path}" actual_sha256)
        set(expected_sha256 "${expected_${artifact}}")
        if(NOT actual_sha256 STREQUAL expected_sha256)
            message(
                FATAL_ERROR
                "BS-${stage} ${artifact} artifact changed.\n"
                "Expected: ${expected_sha256}\n"
                "Actual:   ${actual_sha256}\n"
            )
        endif()
    endforeach()

    execute_process(
        COMMAND
            powershell
            -NoProfile
            -ExecutionPolicy
            Bypass
            -File
            "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-BonusLayoutSemantics.ps1"
            -Stage
            "${stage}"
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
            "BS-${stage} semantics failed (${semantics_result}).\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
    foreach(expected_line IN ITEMS
        "Bonus layout semantics verified for BS-${stage}."
        "All stage populations: BS-1=16, BS-2=8, BS-3=12"
    )
        string(FIND "${semantics_output}" "${expected_line}" line_offset)
        if(line_offset EQUAL -1)
            message(
                FATAL_ERROR
                "BS-${stage} semantics omitted '${expected_line}'.\n"
                "${semantics_output}${semantics_error}"
            )
        endif()
    endforeach()
    message(STATUS "${semantics_output}")
endforeach()
