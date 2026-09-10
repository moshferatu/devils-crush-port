# SPDX-License-Identifier: GPL-3.0-or-later
foreach(required IN ITEMS APP ROM OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
# A natural 3600-frame twist route reaches all twelve ranked instructions.
set(keys "01:C9B3" "01:C9CB" "01:C9D1" "01:C9AF" "01:C9C7" "01:C9C9" "01:C9CD" "01:C9B1" "01:C9B5" "01:C9B9" "01:C9B7" "01:C9BF")
set(offsets 10675 10699 10705 10671 10695 10697 10701 10673 10677 10681 10679 10687)
set(opcodes a530 852f 8530 a52f a52f 652b a530 852b 852c 852f a537 8530)
set(record --record-gameplay-twist)
set(patches)
set(expected_csv "a4bd3603db33f3571605fc59637865885cff3b01eee300d6d420c50c52bf33a1")
set(expected_calls.csv "2518c543e7befd7060e1eb4b3f819e3c051437c582f3e10f2c65314063c59922")
set(expected_calls.csv.writes.csv "41d427a6d3e82f1266ed806f0170057ba76f923bdc211cfdb49a73060f6f2e3f")
set(expected_writes.csv "64cf9a334f2c48eaf0d9d08b68ddd5c5b0e839edf440453aaa4044d334dd1aea")
set(expected_bus.csv "22afea6e9c42917b87fd674dc7f384658a34fbcf47ae35decdf192530dadcdfc")
set(counts
    "Native replacement 01:C9B3@all entries: 172 of 172"
    "Native replacement 01:C9CB@all entries: 172 of 172"
    "Native replacement 01:C9D1@all entries: 172 of 172"
    "Native replacement 01:C9AF@all entries: 172 of 172"
    "Native replacement 01:C9C7@all entries: 172 of 172"
    "Native replacement 01:C9C9@all entries: 172 of 172"
    "Native replacement 01:C9CD@all entries: 172 of 172"
    "Native replacement 01:C9B1@all entries: 172 of 172"
    "Native replacement 01:C9B5@all entries: 172 of 172"
    "Native replacement 01:C9B9@all entries: 172 of 172"
    "Native replacement 01:C9B7@all entries: 172 of 172"
    "Native replacement 01:C9BF@all entries: 172 of 172"
    "Routine call trace complete: 2088 calls, 870896 writes"
    "Work-RAM write trace complete: 12470985 selected writes"
    "Bus write trace complete: 3265435 selected writes"
)
list(LENGTH keys count)
math(EXPR last "${count} - 1")
foreach(index RANGE 0 ${last})
    list(GET offsets ${index} offset)
    list(GET opcodes ${index} expected)
    string(LENGTH "${expected}" length)
    math(EXPR length "${length} / 2")
    file(READ "${ROM}" bytes OFFSET ${offset} LIMIT ${length} HEX)
    if(NOT bytes STREQUAL expected)
        message(FATAL_ERROR "Ranked coordinate ROM changed at index ${index}")
    endif()
endforeach()
foreach(mode IN ITEMS assembly native)
    set(hooks)
    if(mode STREQUAL "native")
        foreach(key IN LISTS keys)
            list(APPEND hooks --replace-hook "${key}@all")
        endforeach()
    endif()
    set(base "${OUTPUT}.${mode}")
    execute_process(COMMAND "${APP}" ${record} "${base}.csv" ${patches} ${hooks}
        --observe-hook "01:C693" --observe-hook "01:C696"
        --observe-hook "01:C5EA" --observe-hook "01:C5C9"
        --routine-call-trace "${base}.calls.csv"
        --work-ram-write-range 2000-3FFF --work-ram-write-trace "${base}.writes.csv"
        --bus-write-range 0000-1FFF --bus-write-trace "${base}.bus.csv"
        --rom "${ROM}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Ranked coordinate ${mode}: ${output}${error}")
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
            message(FATAL_ERROR "Ranked coordinate ${mode} ${ext}: ${actual} != ${expected_${ext}}")
        endif()
    endforeach()
endforeach()
message(STATUS "Ranked coordinate batch preserves all five original assembly traces.")
