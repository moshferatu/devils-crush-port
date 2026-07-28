# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(
        FATAL_ERROR
        "Cross-stage scored-result coverage is Release-only, not ${CONFIG}."
    )
endif()

set(
    expected_census_2
    "03e029d1729a05fe3ba465363b52d53b30a7a0a6c7294e966cb0fb23e939f1c1"
)
set(
    expected_state_2
    "37279a0833e597e336febde56a5f3829578c764769a5d34996f11d08874600f7"
)
set(expected_writes_2 1081)
set(
    expected_census_3
    "60345b0d06c0309c5e2c1da742806cfdf6e03293640f78a6b08abd7ad7504e21"
)
set(
    expected_state_3
    "a2464941052ef38669ec9c1b941dddb2868c2ddffd200a8b306644d5b146572b"
)
set(expected_writes_3 1089)
set(
    expected_census_4
    "3d4e67c9db612d7a2b8f510dc68b3048f5cad1e7e315a94746778c6cc045f279"
)
set(
    expected_state_4
    "dc7e34a53de43c324e1ad0c914821e66ada62e82aaaf2e6dc6ff926b520f4940"
)
set(expected_writes_4 1083)
set(
    expected_census_5
    "db1223ee8f639eab4666cf50594eef70dd00e410b13d2a3089f04128090cc5ff"
)
set(
    expected_state_5
    "81a0510b254e6f52e0c04bd2e0036f27798020cdbe1282019be25c95248dfbf1"
)
set(expected_writes_5 1081)
set(
    expected_census_6
    "086e35d380bd59d9f06f2d5d407bfd8eacafc5e2d033f93afb7c54792ac0c2c1"
)
set(
    expected_state_6
    "01669cf3cb8271d0793eb6677b8d1ddd92e8b4326ce6c6640fb9adf0bc426611"
)
set(expected_writes_6 1081)

foreach(stage RANGE 2 6)
    set(census_output "${OUTPUT}.bs${stage}.csv")
    set(state_output "${census_output}.state.csv")
    execute_process(
        COMMAND
            "${APP}"
            --record-state-census "${census_output}"
            --work-ram-patch "1300:2055:0${stage}"
            --work-ram-patch "1300:2053:10"
            --work-ram-patch "1459:2DB7:01"
            --pass-through-hook "04:456C"
            --pass-through-hook "04:45F0"
            --pass-through-hook "04:46A4"
            --pass-through-hook "04:4DC4"
            --work-ram-write-range "2D00-2DFF"
            --work-ram-write-trace "${state_output}"
            --rom "${ROM}"
        RESULT_VARIABLE recorder_result
        OUTPUT_VARIABLE recorder_output
        ERROR_VARIABLE recorder_error
    )
    if(NOT recorder_result EQUAL 0)
        message(
            FATAL_ERROR
            "BS-${stage} scored-result recorder failed "
            "(${recorder_result}).\n"
            "${recorder_output}${recorder_error}"
        )
    endif()

    foreach(expected_line IN ITEMS
        "State census recording complete: 3600 frames"
        "Work-RAM patches applied: 3"
        "Pass-through hook 1 entries: 1"
        "Pass-through hook 2 entries: 1"
        "Pass-through hook 3 entries: 1"
        "Pass-through hook 4 entries: 181"
        "Work-RAM write trace complete: ${expected_writes_${stage}} selected writes"
    )
        string(FIND "${recorder_output}" "${expected_line}" line_offset)
        if(line_offset EQUAL -1)
            message(
                FATAL_ERROR
                "BS-${stage} scored-result output omitted "
                "'${expected_line}'.\n"
                "${recorder_output}${recorder_error}"
            )
        endif()
    endforeach()

    file(SHA256 "${census_output}" actual_census_sha256)
    file(SHA256 "${state_output}" actual_state_sha256)
    if(
        NOT actual_census_sha256 STREQUAL expected_census_${stage} OR
        NOT actual_state_sha256 STREQUAL expected_state_${stage}
    )
        message(
            FATAL_ERROR
            "BS-${stage} scored-result artifacts changed.\n"
            "Census expected: ${expected_census_${stage}}\n"
            "Census actual:   ${actual_census_sha256}\n"
            "State expected:  ${expected_state_${stage}}\n"
            "State actual:    ${actual_state_sha256}"
        )
    endif()

    execute_process(
        COMMAND
            powershell
            -NoProfile
            -ExecutionPolicy
            Bypass
            -File
            "${CMAKE_CURRENT_LIST_DIR}/../tools/Analyze-BonusScoredResultSemantics.ps1"
            -Stage "${stage}"
            -RomPath "${ROM}"
            -StatePath "${state_output}"
        RESULT_VARIABLE semantics_result
        OUTPUT_VARIABLE semantics_output
        ERROR_VARIABLE semantics_error
    )
    if(NOT semantics_result EQUAL 0)
        message(
            FATAL_ERROR
            "BS-${stage} scored-result analysis failed "
            "(${semantics_result}).\n"
            "${semantics_output}${semantics_error}"
        )
    endif()
    foreach(expected_line IN ITEMS
        "BS-${stage} scored-result semantics verified."
        "Route: authentic nonzero objective state"
        "Presentation: handshake 2D66 advances to 1"
        "Perfect award: builder and transfer remain unentered."
    )
        string(FIND "${semantics_output}" "${expected_line}" line_offset)
        if(line_offset EQUAL -1)
            message(
                FATAL_ERROR
                "BS-${stage} scored-result semantics omitted "
                "'${expected_line}'.\n"
                "${semantics_output}${semantics_error}"
            )
        endif()
    endforeach()
    message(
        STATUS
        "BS-${stage} scored-result artifacts verified: "
        "${actual_census_sha256}, ${actual_state_sha256}\n"
        "${semantics_output}"
    )
endforeach()
