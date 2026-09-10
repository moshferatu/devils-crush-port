# SPDX-License-Identifier: GPL-3.0-or-later
foreach(required IN ITEMS APP ROM OUTPUT ALLEY)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
if(ALLEY)
    # The natural alley route reaches the continuation-slot load and C018 call.
    set(keys "02:4D53" "01:C8DA" "01:DA09" "02:4D2D" "00:E5A4" "00:E5AB" "00:E5A6")
    set(offsets 19795 10458 14857 19757 1444 1451 1446)
    set(opcodes 2018c0 bd803c 8528 bd003e a530 a52f f003)
    set(patches)
    set(record --record-gameplay-alley)
    set(expected_csv "ba06e5a2111b9caae6837109ba2943a093cba1d47ad57af6561c2aa721b4a15a")
    set(expected_calls.csv "6a5d08540e1a3ce6cebb531d1545745d9e50074d3539f3f183f9ed154e4d081c")
    set(expected_calls.csv.writes.csv "84c3b0cc57131e72a35a88a08a757959dd03ec2bbee7205cf321909d16eece5a")
    set(expected_writes.csv "2215f17872d845ed21a8a0551ddf0b273bc52382bd6905529c7fca417bb754f3")
    set(expected_bus.csv "d98d2bcac0655667f0d7cb7c93eb297396a436c78f19afc3b0465fb8f5c1e9a9")
    set(counts
        "Native replacement 02:4D53@all entries: 96 of 96"
        "Native replacement 01:C8DA@all entries: 119 of 119"
        "Native replacement 01:DA09@all entries: 41 of 41"
        "Native replacement 00:E5A4@all entries: 1 of 1"
        "Native replacement 00:E5AB@all entries: 1 of 1"
        "Native replacement 00:E5A6@all entries: 1 of 1"
        "Native replacement 02:4D2D@all entries: 96 of 96"
        "Routine call trace complete: 2067 calls, 946512 writes"
        "Work-RAM write trace complete: 11834535 selected writes"
        "Bus write trace complete: 3225616 selected writes")
else()
    # Last ball and a score below 5000 activate the three Mercy Stars through
    # their original gate. No actor continuation, code or return stack is patched.
    set(keys "02:4D53" "01:C8DA" "01:DA09" "03:61A3" "03:61A6" "00:E5A4" "00:E5AB" "00:E5A6" "00:E5F4" "00:E5F6" "00:E5FD")
    set(offsets 19795 10458 14857 24995 24998 1444 1451 1446 1524 1526 1533)
    set(opcodes 2018c0 bd803c 8528 ad002d 0d012d a530 a52f f003 4546 8546 100b)
    set(patches --work-ram-patch "1200:2D52:01" --work-ram-patch "1200:2205:00"
        --work-ram-patch "1200:220C:00" --work-ram-patch "1200:2213:00")
    set(expected_csv "84e479a4b12b999fd10d0467189463eb44a2c019a779375f1d7cdaf49c1f0e4f")
    set(expected_calls.csv "4d87f2c6819bad56b42b6f9274b981287bd3adcfe9238f4cd5a1dca8f7470b2c")
    set(expected_calls.csv.writes.csv "32718424252acb018c78f5c11e5b2ff23df588bbae48b60379ff44da642ba8bb")
    set(expected_writes.csv "df7ad9d5689b13a2a40eeefc9b7863e425de69a019f46021496de397fdfe02e0")
    set(expected_bus.csv "fed75f9d3bd27fe9cee7cfa929387fa9c51aa538a37f6ec502772f4704014d94")
    set(counts
        "Native replacement 02:4D53@all entries: 1 of 1"
        "Native replacement 01:C8DA@all entries: 32 of 32"
        "Native replacement 01:DA09@all entries: 1512 of 1512"
        "Native replacement 03:61A3@all entries: 1197 of 1197"
        "Native replacement 03:61A6@all entries: 1197 of 1197"
        "Native replacement 00:E5A4@all entries: 2642 of 2642"
        "Native replacement 00:E5AB@all entries: 2642 of 2642"
        "Native replacement 00:E5A6@all entries: 2642 of 2642"
        "Native replacement 00:E5F4@all entries: 2642 of 2642"
        "Native replacement 00:E5F6@all entries: 2642 of 2642"
        "Native replacement 00:E5FD@all entries: 2642 of 2642"
        "Routine call trace complete: 3450 calls, 988112 writes"
        "Work-RAM write trace complete: 13078813 selected writes"
        "Bus write trace complete: 3298254 selected writes")
    set(record --record-state-census)
endif()
list(LENGTH keys count)
math(EXPR last "${count} - 1")
foreach(index RANGE 0 ${last})
    list(GET offsets ${index} offset)
    list(GET opcodes ${index} expected)
    string(LENGTH "${expected}" length)
    math(EXPR length "${length} / 2")
    file(READ "${ROM}" bytes OFFSET ${offset} LIMIT ${length} HEX)
    if(NOT bytes STREQUAL expected)
        message(FATAL_ERROR "Ranked operand ROM changed at index ${index}")
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
        message(FATAL_ERROR "Ranked operand ${mode}: ${output}${error}")
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
            message(FATAL_ERROR "Ranked operand ${mode} ${ext}: ${actual} != ${expected_${ext}}")
        endif()
    endforeach()
endforeach()
message(STATUS "Ranked operand batch preserves all five original assembly traces.")
