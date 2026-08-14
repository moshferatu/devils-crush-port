# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Giant Door hit-prefix regression is Release-only.")
endif()

set(expected_census_sha256 "9d0473039fa1218c016f6e7bff94c7904298b30533e6f1741b80aa564d1aaca3")
set(expected_call_sha256 "78e27b05c42a1243e93867ad3e6fabfa42f39ee9514ccc54b8962d7b74cb1245")
set(expected_write_sha256 "e1c7a5d1d5dda8c706072af39314fb00b137b06eceb500fb5c3af96bfdc4f17e")
set(expected_state_write_sha256 "bb4aa090cee9958ac6d57a3437adbd64cfca7395a7757bdb6ac1eee0a79904e5")

set(call_output "${OUTPUT}.calls.csv")
set(write_output "${call_output}.writes.csv")
set(state_write_output "${OUTPUT}.state.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --observe-hook
        "03:62B8"
        --observe-hook
        "01:C02A"
        --replace-hook
        "03:62B8@all"
        --routine-call-trace
        "${call_output}"
        --work-ram-patch
        "1228:3100:00"
        --work-ram-patch
        "1228:3180:40"
        --work-ram-patch
        "1228:3200:00"
        --work-ram-patch
        "1228:3280:00"
        --work-ram-patch
        "1228:3300:00"
        --work-ram-patch
        "1228:3380:01"
        --work-ram-write-range
        "002F-0030"
        --work-ram-write-range
        "384C-384C"
        --work-ram-write-range
        "3B4C-3B4C"
        --work-ram-write-trace
        "${state_write_output}"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Giant Door hit-prefix recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Work-RAM patches applied: 6"
    "Native replacement 03:62B8@all entries: 2373 of 2373"
    "Observe hook 1 entries: 2373"
    "Observe hook 2 entries: 2"
    "Routine call trace complete: 2375 calls, 541 writes"
    "Work-RAM write trace complete: 14 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Giant Door hit-prefix output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census call write state_write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    elseif(artifact STREQUAL "call")
        set(artifact_path "${call_output}")
    elseif(artifact STREQUAL "write")
        set(artifact_path "${write_output}")
    else()
        set(artifact_path "${state_write_output}")
    endif()

    file(SHA256 "${artifact_path}" actual_sha256)
    if(NOT actual_sha256 STREQUAL "${expected_${artifact}_sha256}")
        message(
            FATAL_ERROR
            "Giant Door hit-prefix ${artifact} artifact changed.\n"
            "Expected: ${expected_${artifact}_sha256}\n"
            "Actual:   ${actual_sha256}"
        )
    endif()
    message(STATUS "Giant Door hit-prefix ${artifact} verified: ${actual_sha256}")
endforeach()

message(
    STATUS
    "Giant Door hit prefix verified: geometry-delivered collisions resume at original 03:62C4."
)
