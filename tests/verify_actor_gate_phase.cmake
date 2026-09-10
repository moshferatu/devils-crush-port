# SPDX-License-Identifier: GPL-3.0-or-later
foreach(required IN ITEMS APP ROM OUTPUT PHASE)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
if(PHASE)
    # Existing bumper activation also reaches the actor's C018 call naturally.
    set(keys "03:7AE2" "03:7AE5" "03:7AE8" "03:7AED" "03:7957")
    set(offsets 31458 31461 31464 31469 31063)
    set(opcodes 9d003a fe8039 bd8039 9003 2018c0)
    set(patches --work-ram-patch "3341:3E95:08")
    set(expected_csv "8607824e0a9464690ce8b2322a009c812dda78f0282c2bf2459a1fc849a24345")
    set(expected_calls.csv "dd11c814f4c219f6a9d2cc6cbad3ac1936487befcf059cc4cc9e9528c8583c48")
    set(expected_calls.csv.writes.csv "fcfc13a973dcbfb3c60c5655029bb5f082df153d813e4cea4444f9900131fda4")
    set(expected_writes.csv "89703a7b6cc4bbd5243f1d9ddec2cff0bee3fbf37d5c4412a372cb1d816b00d7")
    set(expected_bus.csv "dd212e440e8b189384865da6a2498ed5609d10ddbcace7b71475c5da479ed9f0")
    set(counts
        "Native replacement 03:7AE2@all entries: 298 of 298"
        "Native replacement 03:7AE5@all entries: 298 of 298"
        "Native replacement 03:7AE8@all entries: 298 of 298"
        "Native replacement 03:7AED@all entries: 298 of 298"
        "Native replacement 03:7957@all entries: 192 of 192"
        "Routine call trace complete: 2893 calls, 895886 writes"
        "Work-RAM write trace complete: 12704178 selected writes"
        "Bus write trace complete: 3146298 selected writes")
else()
    # Last ball and a score below 5000 activate the three Mercy Stars through
    # their original gate. No actor continuation, code or return stack is patched.
    set(keys "02:5004" "02:5007" "02:5009" "02:500C" "02:500E" "02:5013")
    set(offsets 20484 20487 20489 20492 20494 20499)
    set(opcodes bd0038 300d ad4e2d c902 d003 4cda4c)
    set(patches --work-ram-patch "1200:2D52:01" --work-ram-patch "1200:2205:00"
        --work-ram-patch "1200:220C:00" --work-ram-patch "1200:2213:00")
    set(expected_csv "84e479a4b12b999fd10d0467189463eb44a2c019a779375f1d7cdaf49c1f0e4f")
    set(expected_calls.csv "4d87f2c6819bad56b42b6f9274b981287bd3adcfe9238f4cd5a1dca8f7470b2c")
    set(expected_calls.csv.writes.csv "32718424252acb018c78f5c11e5b2ff23df588bbae48b60379ff44da642ba8bb")
    set(expected_writes.csv "df7ad9d5689b13a2a40eeefc9b7863e425de69a019f46021496de397fdfe02e0")
    set(expected_bus.csv "fed75f9d3bd27fe9cee7cfa929387fa9c51aa538a37f6ec502772f4704014d94")
    set(counts
        "Native replacement 02:5004@all entries: 3475 of 3475"
        "Native replacement 02:5007@all entries: 3475 of 3475"
        "Native replacement 02:5009@all entries: 3474 of 3474"
        "Native replacement 02:500C@all entries: 3474 of 3474"
        "Native replacement 02:500E@all entries: 3474 of 3474"
        "Native replacement 02:5013@all entries: 3472 of 3472"
        "Routine call trace complete: 3450 calls, 988112 writes"
        "Work-RAM write trace complete: 13078813 selected writes"
        "Bus write trace complete: 3298254 selected writes")
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
        message(FATAL_ERROR "Actor gate/phase ROM changed at index ${index}")
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
    execute_process(COMMAND "${APP}" --record-state-census "${base}.csv" ${patches} ${hooks}
        --observe-hook "01:C693" --observe-hook "01:C696"
        --observe-hook "01:C5EA" --observe-hook "01:C5C9"
        --routine-call-trace "${base}.calls.csv"
        --work-ram-write-range 2000-3FFF --work-ram-write-trace "${base}.writes.csv"
        --bus-write-range 0000-1FFF --bus-write-trace "${base}.bus.csv"
        --rom "${ROM}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Actor gate/phase ${mode}: ${output}${error}")
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
            message(FATAL_ERROR "Actor gate/phase ${mode} ${ext}: ${actual} != ${expected_${ext}}")
        endif()
    endforeach()
endforeach()
message(STATUS "Actor gate/phase batch preserves all five original assembly traces.")
