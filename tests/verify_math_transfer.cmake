# SPDX-License-Identifier: GPL-3.0-or-later
# The natural twist route reaches all twelve addresses in this ranking.
foreach(required IN ITEMS APP ROM OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
set(keys "01:C9C3" "01:C9D3" "00:E8A3" "00:E5AD" "00:E5B8" "00:E5B2"
    "00:E5B4" "00:E5EF" "00:E5B6" "00:E5FB" "00:E5A0" "00:E5A2")
set(offsets 10691 10707 2211 1453 1464 1458 1460 1519 1462 1531 1440 1442)
set(opcodes 20ece5 205de8 60 8530 852f a543 8530 8546 a542 a546 6442 6443)
foreach(index RANGE 0 11)
    list(GET offsets ${index} offset)
    list(GET opcodes ${index} expected)
    string(LENGTH "${expected}" length)
    math(EXPR length "${length} / 2")
    file(READ "${ROM}" bytes OFFSET ${offset} LIMIT ${length} HEX)
    if(NOT bytes STREQUAL expected)
        message(FATAL_ERROR "Ranked math transfer ROM bytes changed at index ${index}")
    endif()
endforeach()
set(expected_csv "a4bd3603db33f3571605fc59637865885cff3b01eee300d6d420c50c52bf33a1")
set(expected_calls.csv "2518c543e7befd7060e1eb4b3f819e3c051437c582f3e10f2c65314063c59922")
set(expected_calls.csv.writes.csv "41d427a6d3e82f1266ed806f0170057ba76f923bdc211cfdb49a73060f6f2e3f")
set(expected_writes.csv "64cf9a334f2c48eaf0d9d08b68ddd5c5b0e839edf440453aaa4044d334dd1aea")
set(expected_bus.csv "22afea6e9c42917b87fd674dc7f384658a34fbcf47ae35decdf192530dadcdfc")
foreach(mode IN ITEMS assembly native)
    set(hooks)
    if(mode STREQUAL "native")
        foreach(key IN LISTS keys)
            list(APPEND hooks --replace-hook "${key}@all")
        endforeach()
    endif()
    set(base "${OUTPUT}.${mode}")
    execute_process(COMMAND "${APP}" --record-gameplay-twist "${base}.csv" ${hooks}
        --observe-hook "01:C693" --observe-hook "01:C696"
        --observe-hook "01:C5EA" --observe-hook "01:C5C9"
        --routine-call-trace "${base}.calls.csv"
        --work-ram-write-range 2000-3FFF --work-ram-write-trace "${base}.writes.csv"
        --bus-write-range 0000-1FFF --bus-write-trace "${base}.bus.csv"
        --rom "${ROM}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Ranked math transfers ${mode}: ${output}${error}")
    endif()
    if(mode STREQUAL "native")
        foreach(expected IN ITEMS
            "Native replacement 01:C9C3@all entries: 152 of 172"
            "Native replacement 01:C9D3@all entries: 150 of 172"
            "Native replacement 00:E8A3@all entries: 152 of 172"
            "Native replacement 00:E5AD@all entries: 344 of 344"
            "Native replacement 00:E5B8@all entries: 344 of 344"
            "Native replacement 00:E5B2@all entries: 344 of 344"
            "Native replacement 00:E5B4@all entries: 344 of 344"
            "Native replacement 00:E5EF@all entries: 344 of 344"
            "Native replacement 00:E5B6@all entries: 344 of 344"
            "Native replacement 00:E5FB@all entries: 344 of 344"
            "Native replacement 00:E5A0@all entries: 344 of 344"
            "Native replacement 00:E5A2@all entries: 344 of 344"
            "Routine call trace complete: 2088 calls, 870896 writes"
            "Work-RAM write trace complete: 12470985 selected writes"
            "Bus write trace complete: 3265435 selected writes")
            string(FIND "${output}" "${expected}" found)
            if(found EQUAL -1)
                message(FATAL_ERROR "Missing ${expected}: ${output}${error}")
            endif()
        endforeach()
    endif()
    foreach(ext IN ITEMS csv calls.csv calls.csv.writes.csv writes.csv bus.csv)
        file(SHA256 "${base}.${ext}" actual)
        if(NOT actual STREQUAL "${expected_${ext}}")
            message(FATAL_ERROR "Ranked math transfers ${mode} ${ext}: ${actual} != ${expected_${ext}}")
        endif()
    endforeach()
endforeach()
message(STATUS "All twelve ranked math transfer sites preserve assembly frame, scheduler, RAM and device traces.")
