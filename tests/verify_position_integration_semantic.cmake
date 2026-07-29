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
    "7965ddd37d2625a8d5dac4de3acb42760aab724970f4175fd465e8281835f1ee"
)
set(
    expected_write_sha256
    "696ec096143b7d36a42922a536a2961a82b70d28741fd51d6500c109442f6079"
)

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-gameplay
        "${OUTPUT}"
        --observe-hook
        "00:F1B4"
        --replace-hook
        "00:F1B4@all"
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
        "Semantic position-integration recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "Gameplay recording complete: 1800 frames"
    "Native replacement 00:F1B4@all entries: 214 of 1156"
    "Routine call trace complete: 1156 calls, 21738 writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic position-integration output omitted '${expected_line}'.\n"
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
            "Semantic position-integration ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic F1B4 replacement covers 214 calls with automatic event/IRQ fallback."
)
