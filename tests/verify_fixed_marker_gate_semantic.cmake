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
        "Unsupported fixed-marker-gate configuration: ${CONFIG}"
    )
endif()

set(
    expected_call_sha256
    "0a310f9dfdfe3235ab326605148596824f04458a032c7eb54e169f4234e0b102"
)
set(
    expected_write_sha256
    "2b78c118caa3cdb9f37a19d40dc9f967eb0eb232b56c27d3bde64a6d350301f9"
)

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --observe-hook
        "03:7B7A"
        --replace-hook
        "03:7B7A@all"
        --replace-hook
        "03:7BB1@all"
        --replace-hook
        "03:7BB5@all"
        --replace-hook
        "03:7BBB@all"
        --replace-hook
        "03:7BC3@all"
        --replace-hook
        "03:7BC7@all"
        --replace-hook
        "03:7BCD@all"
        --replace-hook
        "03:7BD5@all"
        --replace-hook
        "03:7BD9@all"
        --replace-hook
        "03:7BDF@all"
        --replace-hook
        "03:7BE0@all"
        --replace-hook
        "03:7BE5@all"
        --replace-hook
        "03:7BE8@all"
        --replace-hook
        "03:7BE9@all"
        --replace-hook
        "03:7BEC@all"
        --replace-hook
        "02:5955@all"
        --routine-call-trace
        "${call_output}"
        --routine-replay
        "03:7B7A@1"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Semantic fixed marker gate recorder failed "
        "(${recorder_result}).\n${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 03:7B7A@all entries: 2650 of 2650"
    "Native replacement 03:7BB1@all entries: 332 of 332"
    "Native replacement 03:7BB5@all entries: 332 of 332"
    "Native replacement 03:7BBB@all entries: 332 of 332"
    "Native replacement 03:7BC3@all entries: 332 of 332"
    "Native replacement 03:7BC7@all entries: 332 of 332"
    "Native replacement 03:7BCD@all entries: 332 of 332"
    "Native replacement 03:7BD5@all entries: 332 of 332"
    "Native replacement 03:7BD9@all entries: 332 of 332"
    "Native replacement 03:7BDF@all entries: 332 of 332"
    "Native replacement 03:7BE0@all entries: 996 of 996"
    "Native replacement 03:7BE5@all entries: 996 of 996"
    "Native replacement 03:7BE8@all entries: 996 of 996"
    "Native replacement 03:7BE9@all entries: 996 of 996"
    "Native replacement 03:7BEC@all entries: 996 of 996"
    "Native replacement 02:5955@all entries: 997 of 997"
    "Observe hook 1 entries: 2650"
    "Routine call trace complete: 2650 calls, 88480 writes"
    "Routine replay 03:7B7A@1 verified: frame 873, 48 body cycles, 0 ordered writes"
    "Routine replay complete: 1 state-normalized calls"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic fixed marker gate output omitted "
            "'${expected_line}'.\n${recorder_output}${recorder_error}"
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
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "Semantic fixed marker gate ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic 7B7A through 7BEC covers all 2,650 calls, marker submissions, wrappers, and helper calls."
)
