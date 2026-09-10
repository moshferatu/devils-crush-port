# SPDX-License-Identifier: GPL-3.0-or-later
foreach(required IN ITEMS APP ROM OUTPUT FACE)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
set(keys "03:7ADB" "03:7A7F" "03:7A71" "03:76C5" "08:40F6" "08:40F1"
    "08:40FB" "08:4106" "03:7A82" "03:7A77" "03:7ADE" "08:40DF")
set(offsets 31451 31359 31345 30405 65782 65777 65787 65798 31362 31351 31454 65759)
set(opcodes de003a bd0038 bd0037 de003a b12d b12d b12f 60 100a 9003 d012 bc0039)
foreach(index RANGE 0 11)
    list(GET offsets ${index} offset)
    list(GET opcodes ${index} expected)
    string(LENGTH "${expected}" length)
    math(EXPR length "${length} / 2")
    file(READ "${ROM}" bytes OFFSET ${offset} LIMIT ${length} HEX)
    if(NOT bytes STREQUAL expected)
        message(FATAL_ERROR "Bumper/sprite ranked opcode changed at index ${index}")
    endif()
endforeach()
if(FACE)
    set(keys "03:76C5")
    set(patches --work-ram-patch "1200:2D0E:01")
    set(expected_csv "a99ac333c3f0dbbde8e2df778e5921482c34b33eb4d790e206fa8d72702ac5c3")
    set(expected_calls.csv "37c9ecbcdd30f1a6a214205889f1d8f8101859c45b522a618037429c7f305dc2")
    set(expected_calls.csv.writes.csv "2c267918f98a0d17a57864379c3dc2dd88dbc62adda7bd66ae70cb8edf9b9313")
    set(expected_writes.csv "e840a60fb2da78a2f00c63c0d0f8e6009ce63bcf1f3a867d999744cd1a2bf609")
    set(expected_bus.csv "ea81bec9e98e39dd9b015e6b0326f75f92af80de19ce215deeffc5cb66a1ba3e")
    set(counts "Native replacement 03:76C5@all entries: 872 of 872")
else()
    list(REMOVE_ITEM keys "03:76C5")
    # Existing bumper terminal activation. Reserving 63 SAT entries for four
    # frames exercises real cursor-overflow exits without emitting extra sprites.
    set(patches --work-ram-patch "3341:3E95:08"
        --work-ram-patch "1200:2058:3F" --work-ram-patch "1204:2058:00")
    set(expected_csv "707ac4cd912817f8ab75737e58689baeed098e02d577b5fb0d5224503ac336a9")
    set(expected_calls.csv "dccf4968f0d4a35c92794cf5a0ca09d3bb41cb02d227db40c2a3947a27fc3d67")
    set(expected_calls.csv.writes.csv "54a4753418b213dadb7a2abe89d45ae420a9852160c277667f91a7ff4998b25c")
    set(expected_writes.csv "1a3f59502477ad12b88da644b84e1bb672b1415f70427b1501ed9df9ac397901")
    set(expected_bus.csv "22ad310ec2f5e46fa11edc52f0df37d90d9f2e5e536740d268bbf68f858ffbb7")
    set(counts
        "Native replacement 03:7ADB@all entries: 1202 of 1202"
        "Native replacement 03:7A7F@all entries: 1202 of 1202"
        "Native replacement 03:7A71@all entries: 1202 of 1202"
        "Native replacement 08:40F6@all entries: 128 of 128"
        "Native replacement 08:40F1@all entries: 128 of 128"
        "Native replacement 08:40FB@all entries: 128 of 128"
        "Native replacement 08:4106@all entries: 111 of 128"
        "Native replacement 03:7A82@all entries: 1202 of 1202"
        "Native replacement 03:7A77@all entries: 1202 of 1202"
        "Native replacement 03:7ADE@all entries: 1202 of 1202"
        "Native replacement 08:40DF@all entries: 75757 of 75757")
endif()
foreach(mode IN ITEMS assembly native)
    set(hooks)
    if(mode STREQUAL "native")
        foreach(key IN LISTS keys)
            list(APPEND hooks --replace-hook "${key}@all")
        endforeach()
    endif()
    set(base "${OUTPUT}.${mode}")
    execute_process(COMMAND "${APP}" --record-state-census "${base}.csv" ${patches} ${hooks}
        --observe-hook "01:C693" --observe-hook "01:C696"
        --observe-hook "01:C5EA" --observe-hook "01:C5C9"
        --routine-call-trace "${base}.calls.csv"
        --work-ram-write-range 2000-3FFF --work-ram-write-trace "${base}.writes.csv"
        --bus-write-range 0000-1FFF --bus-write-trace "${base}.bus.csv"
        --rom "${ROM}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Bumper/sprite ${mode}: ${output}${error}")
    endif()
    if(mode STREQUAL "native")
        foreach(expected IN LISTS counts)
            string(FIND "${output}" "${expected}" found)
            if(found EQUAL -1)
                message(FATAL_ERROR "Missing ${expected}: ${output}${error}")
            endif()
        endforeach()
    endif()
    foreach(ext IN ITEMS csv calls.csv calls.csv.writes.csv writes.csv bus.csv)
        file(SHA256 "${base}.${ext}" actual)
        if(NOT actual STREQUAL "${expected_${ext}}")
            message(FATAL_ERROR "Bumper/sprite ${mode} ${ext}: ${actual} != ${expected_${ext}}")
        endif()
    endforeach()
endforeach()
message(STATUS "Bumper/sprite ports preserve assembly frame, scheduler, RAM and device traces.")
