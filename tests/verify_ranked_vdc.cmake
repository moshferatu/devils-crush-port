# SPDX-License-Identifier: GPL-3.0-or-later
foreach(required IN ITEMS APP ROM OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
# The unmodified alley route reaches all twelve ranked sites.
set(keys "09:62E0" "09:62E6" "09:62E8" "09:62E2" "09:60BA" "09:62EE" "09:60BF" "09:608D" "09:6090" "09:6095" "09:6099" "02:4D30")
set(offsets 74464 74470 74472 74466 73914 74478 73919 73869 73872 73877 73881 19760)
set(opcodes b12f b12f 8d0300 8d0200 ce552d d0f0 60 9cba2c 9cbc2c 8dbb2c 864d c978)
set(record --record-gameplay-alley)
set(patches)
set(expected_csv "ba06e5a2111b9caae6837109ba2943a093cba1d47ad57af6561c2aa721b4a15a")
set(expected_calls.csv "6a5d08540e1a3ce6cebb531d1545745d9e50074d3539f3f183f9ed154e4d081c")
set(expected_calls.csv.writes.csv "84c3b0cc57131e72a35a88a08a757959dd03ec2bbee7205cf321909d16eece5a")
set(expected_writes.csv "2215f17872d845ed21a8a0551ddf0b273bc52382bd6905529c7fca417bb754f3")
set(expected_bus.csv "d98d2bcac0655667f0d7cb7c93eb297396a436c78f19afc3b0465fb8f5c1e9a9")
set(counts
    "Native replacement 09:62E0@all entries: 256 of 256"
    "Native replacement 09:62E6@all entries: 256 of 256"
    "Native replacement 09:62E8@all entries: 256 of 256"
    "Native replacement 09:62E2@all entries: 256 of 256"
    "Native replacement 09:60BA@all entries: 201 of 201"
    "Native replacement 09:62EE@all entries: 256 of 256"
    "Native replacement 09:60BF@all entries: 150 of 150"
    "Native replacement 09:608D@all entries: 201 of 201"
    "Native replacement 09:6090@all entries: 201 of 201"
    "Native replacement 09:6095@all entries: 201 of 201"
    "Native replacement 09:6099@all entries: 201 of 201"
    "Native replacement 02:4D30@all entries: 96 of 96"
    "Routine call trace complete: 2067 calls, 946512 writes"
    "Work-RAM write trace complete: 11834535 selected writes"
    "Bus write trace complete: 3225616 selected writes"
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
        message(FATAL_ERROR "Ranked vdc ROM changed at index ${index}")
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
        message(FATAL_ERROR "Ranked vdc ${mode}: ${output}${error}")
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
            message(FATAL_ERROR "Ranked vdc ${mode} ${ext}: ${actual} != ${expected_${ext}}")
        endif()
    endforeach()
endforeach()
message(STATUS "Ranked vdc batch preserves all five original assembly traces.")
