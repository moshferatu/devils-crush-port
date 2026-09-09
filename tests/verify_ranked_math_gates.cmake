# SPDX-License-Identifier: GPL-3.0-or-later
# The natural twist route reaches all twelve addresses in this ranking.
foreach(required IN ITEMS APP ROM OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
set(keys "02:5CC5" "02:5CC8" "02:5CCB" "00:E5AF" "00:E824" "00:E5BB"
    "00:E5EC" "00:E5F1" "00:E5F8" "00:E5EB" "00:E836" "01:DA3E")
set(offsets 23749 23752 23755 1455 2084 1467 1516 1521 1528 1515 2102 14910)
set(opcodes ad002d 0d012d d008 20bce5 60 60 2013e8 2025e8 209fe5 60 60 7a)
foreach(index RANGE 0 11)
    list(GET offsets ${index} offset)
    list(GET opcodes ${index} expected)
    string(LENGTH "${expected}" length)
    math(EXPR length "${length} / 2")
    file(READ "${ROM}" bytes OFFSET ${offset} LIMIT ${length} HEX)
    if(NOT bytes STREQUAL expected)
        message(FATAL_ERROR "Ranked math/gate ROM bytes changed at index ${index}")
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
        message(FATAL_ERROR "Ranked math/gates ${mode}: ${output}${error}")
    endif()
    if(mode STREQUAL "native")
        foreach(expected IN ITEMS
            "Native replacement 02:5CC5@all entries: 686 of 686"
            "Native replacement 02:5CC8@all entries: 686 of 686"
            "Native replacement 02:5CCB@all entries: 686 of 686"
            "Native replacement 00:E5AF@all entries: 253 of 292"
            "Native replacement 00:E824@all entries: 265 of 292"
            "Native replacement 00:E5BB@all entries: 259 of 292"
            "Native replacement 00:E5EC@all entries: 313 of 344"
            "Native replacement 00:E5F1@all entries: 258 of 292"
            "Native replacement 00:E5F8@all entries: 262 of 292"
            "Native replacement 00:E5EB@all entries: 254 of 292"
            "Native replacement 00:E836@all entries: 254 of 292"
            "Native replacement 01:DA3E@all entries: 455 of 455"
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
            message(FATAL_ERROR "Ranked math/gates ${mode} ${ext}: ${actual} != ${expected_${ext}}")
        endif()
    endforeach()
endforeach()
message(STATUS "All twelve ranked math/gate sites preserve assembly frame, scheduler, RAM and device traces.")
