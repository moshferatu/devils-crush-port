# SPDX-License-Identifier: GPL-3.0-or-later
foreach(required IN ITEMS APP ROM OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
# Activate the upper hand and undead-head parent through their established scheduler gates.
set(keys "02:5A05" "02:5A00" "03:61A9")
set(offsets 23045 23040 25001)
set(opcodes 2018c0 ad332d d007)
set(record --record-state-census)
set(patches --work-ram-patch "1200:3340:2C" --work-ram-patch "1200:33C0:01" --work-ram-patch "1222:3B40:01" --work-ram-patch "1222:3BC0:00" --work-ram-patch "1200:334D:2C" --work-ram-patch "1200:33CD:01")
set(expected_csv "df1db60f900680bd892edfb1c6477b07a8783e7920d8d9d45b25b89d62dbe267")
set(expected_calls.csv "b03171b363e38bd61d9bbfc5ffbaa2fe347fe17937ed1b6d67863ed77d8062df")
set(expected_calls.csv.writes.csv "f56f3786707f1c508708e4707403a89e9e59320c881f1e74419eaf95ccbc719c")
set(expected_writes.csv "0be1a70386c11ffaebe19dcb69a1aebd09910ffbc3b0d6d59c7d91fbb29569c0")
set(expected_bus.csv "a8381554aba99f6726ad616a55a463defac6edb1e8a11675db40c4befd0675fd")
set(counts
    "Native replacement 02:5A05@all entries: 232 of 232"
    "Native replacement 02:5A00@all entries: 232 of 232"
    "Native replacement 03:61A9@all entries: 247 of 247"
    "Routine call trace complete: 2100 calls, 888050 writes"
    "Work-RAM write trace complete: 11746400 selected writes"
    "Bus write trace complete: 3186103 selected writes"
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
        message(FATAL_ERROR "Ranked conditions ROM changed at index ${index}")
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
        message(FATAL_ERROR "Ranked conditions ${mode}: ${output}${error}")
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
            message(FATAL_ERROR "Ranked conditions ${mode} ${ext}: ${actual} != ${expected_${ext}}")
        endif()
    endforeach()
endforeach()
message(STATUS "Ranked conditions batch preserves all five original assembly traces.")
