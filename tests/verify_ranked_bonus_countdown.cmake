# SPDX-License-Identifier: GPL-3.0-or-later
foreach(required IN ITEMS APP ROM OUTPUT STAGE)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
set(keys "05:7376" "05:7379" "05:7387" "05:738A" "05:738F" "05:7390"
    "05:7393" "05:7396" "05:7398" "02:4FC1" "02:4FE1" "04:5197")
set(offsets 45942 45945 45959 45962 45967 45968 45971 45974 45976 20417 20449 37271)
set(opcodes bd0038 100c bd003b d003 3a 9d003b 1d803b f001 60 ad5422 60 60)
foreach(index RANGE 0 11)
    list(GET offsets ${index} offset)
    list(GET opcodes ${index} expected)
    string(LENGTH "${expected}" length)
    math(EXPR length "${length} / 2")
    file(READ "${ROM}" bytes OFFSET ${offset} LIMIT ${length} HEX)
    if(NOT bytes STREQUAL expected)
        message(FATAL_ERROR "Ranked bonus countdown ROM changed at index ${index}")
    endif()
endforeach()
if(STAGE EQUAL 6)
    # Existing BS-6 RNG route allocates a hazard in slot 34. Its live countdown
    # crosses a low-byte borrow, leaving the high-byte DEC in the interpreter.
    list(SUBLIST keys 0 9 keys)
    list(APPEND keys "04:5197")
    set(route --record-gameplay)
    set(patches --work-ram-patch "1300:2055:06" --work-ram-patch "1300:2053:10"
        --work-ram-patch "1403:3834:90" --work-ram-patch "1403:3C34:01"
        --work-ram-patch "1403:3835:90" --work-ram-patch "1403:3C35:02")
    set(expected_csv "5e48d45f879ccddd0231a4642a7542fb117abbd578c7d81666d7780e282c9238")
    set(expected_calls.csv "038f5e3da5b2b72c83472be514e8763a3677d7e7013c236002974978c9c604d8")
    set(expected_calls.csv.writes.csv "90a5a3755f05defa33f238d08085d86e80d7970f2c297069eb5a818249a3096f")
    set(expected_writes.csv "f92814f1910e606c9b42121981e93c0ab6226ef4c4518932f54927536392fff2")
    set(expected_bus.csv "bc035e5198ac01c5031d60793e958d28a81356025d8f41ca94ca6416ff27fbd7")
    set(counts
        "Native replacement 05:7376@all entries: 179 of 179"
        "Native replacement 05:7379@all entries: 179 of 179"
        "Native replacement 05:7387@all entries: 179 of 179"
        "Native replacement 05:738A@all entries: 179 of 179"
        "Native replacement 05:738F@all entries: 179 of 179"
        "Native replacement 05:7390@all entries: 179 of 179"
        "Native replacement 05:7393@all entries: 179 of 179"
        "Native replacement 05:7396@all entries: 179 of 179"
        "Native replacement 05:7398@all entries: 159 of 179"
        "Native replacement 04:5197@all entries: 449 of 501")
elseif(STAGE EQUAL 2)
    list(SUBLIST keys 9 3 keys)
    # The temporary table-mode latch exercises 150 calls declined by the old
    # Mercy-Star owner. The remaining 6255 calls retain that full transcript.
    set(route --record-state-census)
    set(patches --work-ram-patch "1300:2055:02" --work-ram-patch "1300:2053:10"
        --work-ram-patch "1200:2254:01" --work-ram-patch "1250:2254:00")
    set(expected_csv "1f2937394e97252b324f1a45f9806da353f3119df140639a61a351ae3d094006")
    set(expected_calls.csv "06aad4e485e540baf29160f59cf5529878d244818ea9daa3e4c03d74715cc987")
    set(expected_calls.csv.writes.csv "9d0c225b7666ba881ef23c805d1cec29565979e46aa84a41a326d366cef6ddf4")
    set(expected_writes.csv "c7eaed28dba70e1c6f8dca2bf9a73b7cb7f751d8be415777e6d08b265aed4a74")
    set(expected_bus.csv "e4db5b28a8714ac176beefb729e2d68e743b731934092b78db2db171afd0c707")
    set(counts
        "Native replacement 02:4FC1@all entries: 6405 of 6405"
        "Native replacement 02:4FE1@all entries: 137 of 150"
        "Native replacement 04:5197@all entries: 3 of 3")
else()
    message(FATAL_ERROR "Unsupported bonus stage ${STAGE}")
endif()
foreach(mode IN ITEMS assembly native)
    set(hooks)
    if(mode STREQUAL "native")
        foreach(key IN LISTS keys)
            list(APPEND hooks --replace-hook "${key}@all")
        endforeach()
    endif()
    set(base "${OUTPUT}.${mode}")
    execute_process(COMMAND "${APP}" ${route} "${base}.csv" ${patches} ${hooks}
        --observe-hook "01:C693" --observe-hook "01:C696"
        --observe-hook "01:C5EA" --observe-hook "01:C5C9"
        --routine-call-trace "${base}.calls.csv"
        --work-ram-write-range 2000-3FFF --work-ram-write-trace "${base}.writes.csv"
        --bus-write-range 0000-1FFF --bus-write-trace "${base}.bus.csv"
        --rom "${ROM}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Ranked bonus stage ${STAGE} ${mode}: ${output}${error}")
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
            message(FATAL_ERROR "Ranked bonus stage ${STAGE} ${mode} ${ext}: ${actual} != ${expected_${ext}}")
        endif()
    endforeach()
endforeach()
message(STATUS "Ranked bonus stage ${STAGE} ports preserve assembly frame, scheduler, RAM and device traces.")
