# SPDX-License-Identifier: GPL-3.0-or-later
foreach(required IN ITEMS APP ROM OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
# The established bumper activation reaches all ten changed ranked sites.
set(keys "03:7AE0" "03:7AEB" "03:7AEF" "45:4148" "01:D7B3" "03:66A8" "03:66B0" "03:7ACA" "03:7AD5" "00:E8E6")
set(offsets 31456 31467 31471 303432 14259 26280 26288 31434 31445 2278)
set(opcodes a904 c904 9e8039 b5f5 2050d9 a904 4901 2018c0 2018c0 60)
set(record --record-state-census)
set(patches --work-ram-patch "3341:3E95:08")
set(expected_csv "8607824e0a9464690ce8b2322a009c812dda78f0282c2bf2459a1fc849a24345")
set(expected_calls.csv "dd11c814f4c219f6a9d2cc6cbad3ac1936487befcf059cc4cc9e9528c8583c48")
set(expected_calls.csv.writes.csv "fcfc13a973dcbfb3c60c5655029bb5f082df153d813e4cea4444f9900131fda4")
set(expected_writes.csv "89703a7b6cc4bbd5243f1d9ddec2cff0bee3fbf37d5c4412a372cb1d816b00d7")
set(expected_bus.csv "dd212e440e8b189384865da6a2498ed5609d10ddbcace7b71475c5da479ed9f0")
set(counts
    "Native replacement 03:7AE0@all entries: 298 of 298"
    "Native replacement 03:7AEB@all entries: 298 of 298"
    "Native replacement 03:7AEF@all entries: 72 of 72"
    "Native replacement 45:4148@all entries: 10812 of 10812"
    "Native replacement 01:D7B3@all entries: 24 of 24"
    "Native replacement 03:66A8@all entries: 267 of 267"
    "Native replacement 03:66B0@all entries: 267 of 267"
    "Native replacement 03:7ACA@all entries: 16 of 16"
    "Native replacement 03:7AD5@all entries: 16 of 16"
    "Native replacement 00:E8E6@all entries: 77 of 77"
    "Routine call trace complete: 2893 calls, 895886 writes"
    "Work-RAM write trace complete: 12704178 selected writes"
    "Bus write trace complete: 3146298 selected writes"
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
        message(FATAL_ERROR "Ranked phase ROM changed at index ${index}")
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
    file(WRITE "${base}.log" "${output}${error}")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Ranked phase ${mode}: ${output}${error}")
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
            message(FATAL_ERROR "Ranked phase ${mode} ${ext}: ${actual} != ${expected_${ext}}")
        endif()
    endforeach()
endforeach()
message(STATUS "Ranked phase batch preserves all five original assembly traces.")
