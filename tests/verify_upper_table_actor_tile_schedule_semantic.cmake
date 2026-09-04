# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Upper-table actor tile-schedule regression is Release-only.")
endif()

# 08:5A2E initializes one upper-table actor, writes its fixed $80/$80
# position, maps the opening glyph through 5A7A -> 5B01, and hands the 5A3E
# continuation to C5C9.  The third of the three natural calls is interrupted
# by VBlank inside 5B01, so this route-level oracle intentionally covers the
# ordered VDC traffic as well as the work-RAM scheduler frame.
set(expected_census_sha256 "1f6d46ac99798bbac1ecab97702d9e10076b9766796cc64d22d9662efce0b353")
set(expected_work_sha256 "9578773e8d15719305978522af03ffd9885a526d56c1e171987ab46108fbd1ad")
set(expected_bus_sha256 "0aa7c15c4768684c130ba4c1c58f252021c68cb47e44f036bc35f824b1fab51f")

set(assembly_census "${OUTPUT}.assembly.csv")
set(native_census "${OUTPUT}.native.csv")
set(assembly_work "${assembly_census}.work.csv")
set(native_work "${native_census}.work.csv")
set(assembly_bus "${assembly_census}.bus.csv")
set(native_bus "${native_census}.bus.csv")

function(record_route census work bus trace)
    execute_process(
        COMMAND
            "${APP}"
            --record-gameplay "${census}"
            --observe-hook "08:5B01"
            --routine-trace "${trace}"
            --work-ram-write-range "2026-202E"
            --work-ram-write-trace "${work}"
            --bus-write-range "0000-0003"
            --bus-write-trace "${bus}"
            ${ARGN}
            --rom "${ROM}"
        RESULT_VARIABLE recorder_result
        OUTPUT_VARIABLE recorder_output
        ERROR_VARIABLE recorder_error
    )
    if(NOT recorder_result EQUAL 0)
        message(FATAL_ERROR "Upper-table actor tile-schedule recording failed (${recorder_result}).\n${recorder_output}${recorder_error}")
    endif()
    set(recorder_output "${recorder_output}" PARENT_SCOPE)
endfunction()

record_route(
    "${assembly_census}" "${assembly_work}" "${assembly_bus}"
    "${assembly_census}.entries.csv")

record_route(
    "${native_census}" "${native_work}" "${native_bus}"
    "${native_census}.entries.csv"
    --replace-hook "08:5A2E@all")
set(native_output "${recorder_output}")

foreach(expected_line IN ITEMS
    "Gameplay recording complete: 1800 frames"
    "Native replacement 08:5A2E@all entries: 3 of 3"
    "Work-RAM write trace complete: 287975 selected writes"
    "Bus write trace complete: 1019098 selected writes"
)
    string(FIND "${native_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(FATAL_ERROR "Upper-table actor tile-schedule output omitted '${expected_line}'.\n${native_output}")
    endif()
endforeach()

foreach(artifact IN ITEMS census work bus)
    set(assembly_path "${assembly_${artifact}}")
    set(native_path "${native_${artifact}}")
    file(SHA256 "${assembly_path}" assembly_sha256)
    file(SHA256 "${native_path}" native_sha256)
    if(NOT assembly_sha256 STREQUAL native_sha256)
        message(FATAL_ERROR "The native upper-table actor tile schedule changed ${artifact}.\nAssembly: ${assembly_sha256}\nNative:   ${native_sha256}\n")
    endif()
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT assembly_sha256 STREQUAL expected_sha256)
        message(FATAL_ERROR "Upper-table actor tile-schedule ${artifact} baseline changed.\nExpected: ${expected_sha256}\nActual:   ${assembly_sha256}\n")
    endif()
endforeach()

file(REMOVE
    "${assembly_census}" "${native_census}"
    "${assembly_work}" "${native_work}"
    "${assembly_bus}" "${native_bus}"
    "${assembly_census}.entries.csv" "${native_census}.entries.csv")

message(STATUS "Semantic 08:5A2E upper-table actor tile schedule runs all 3 calls natively with exact state, scheduler-RAM and VDC parity.")
