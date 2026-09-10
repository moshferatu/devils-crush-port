# SPDX-License-Identifier: GPL-3.0-or-later
# Natural alley route reaches the actor call, paired upper-table calls,
# both countdown paths and Run-input returns, including short horizons.
foreach(required IN ITEMS APP ROM OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
set(keys "03:657C" "09:7040" "09:7178" "09:7022" "09:715A" "03:7E0D" "01:C4C8")
set(offsets 25980 77888 78200 77858 78170 32269 9416)
set(opcodes 2018c0 208ef0 208ef0 208ef0 208ef0 de003a 60)
foreach(index RANGE 0 6)
    list(GET offsets ${index} offset)
    list(GET opcodes ${index} expected)
    string(LENGTH "${expected}" length)
    math(EXPR length "${length} / 2")
    file(READ "${ROM}" bytes OFFSET ${offset} LIMIT ${length} HEX)
    if(NOT bytes STREQUAL expected)
        message(FATAL_ERROR "Ranked calls ROM bytes changed at index ${index}")
    endif()
endforeach()
# Captured independently with every replacement disabled.
set(expected_csv "ba06e5a2111b9caae6837109ba2943a093cba1d47ad57af6561c2aa721b4a15a")
set(expected_calls.csv "6a5d08540e1a3ce6cebb531d1545745d9e50074d3539f3f183f9ed154e4d081c")
set(expected_calls.csv.writes.csv "84c3b0cc57131e72a35a88a08a757959dd03ec2bbee7205cf321909d16eece5a")
set(expected_writes.csv "2215f17872d845ed21a8a0551ddf0b273bc52382bd6905529c7fca417bb754f3")
set(expected_bus.csv "d98d2bcac0655667f0d7cb7c93eb297396a436c78f19afc3b0465fb8f5c1e9a9")
foreach(mode IN ITEMS assembly native)
    set(hooks)
    if(mode STREQUAL "native")
        foreach(key IN LISTS keys)
            list(APPEND hooks --replace-hook "${key}@all")
        endforeach()
    endif()
    set(base "${OUTPUT}.${mode}")
    execute_process(COMMAND "${APP}" --record-gameplay-alley "${base}.csv" ${hooks}
        --observe-hook "01:C693" --observe-hook "01:C696"
        --observe-hook "01:C5EA" --observe-hook "01:C5C9"
        --routine-call-trace "${base}.calls.csv"
        --work-ram-write-range 2000-3FFF --work-ram-write-trace "${base}.writes.csv"
        --bus-write-range 0000-1FFF --bus-write-trace "${base}.bus.csv"
        --rom "${ROM}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Ranked calls ${mode}: ${output}${error}")
    endif()
    if(mode STREQUAL "native")
        foreach(expected IN ITEMS
            "Native replacement 03:657C@all entries: 48 of 48"
            "Native replacement 09:7040@all entries: 49 of 49"
            "Native replacement 09:7178@all entries: 49 of 49"
            "Native replacement 09:7022@all entries: 31 of 31"
            "Native replacement 09:715A@all entries: 31 of 31"
            "Native replacement 03:7E0D@all entries: 193 of 193"
            "Native replacement 01:C4C8@all entries: 2451 of 2451"
            "Routine call trace complete: 2067 calls, 946512 writes"
            "Work-RAM write trace complete: 11834535 selected writes"
            "Bus write trace complete: 3225616 selected writes")
            string(FIND "${output}" "${expected}" found)
            if(found EQUAL -1)
                message(FATAL_ERROR "Missing ${expected}: ${output}${error}")
            endif()
        endforeach()
    endif()
    foreach(ext IN ITEMS csv calls.csv calls.csv.writes.csv writes.csv bus.csv)
        file(SHA256 "${base}.${ext}" actual)
        if(NOT actual STREQUAL "${expected_${ext}}")
            message(FATAL_ERROR "Ranked calls ${mode} ${ext}: ${actual} != ${expected_${ext}}")
        endif()
    endforeach()
endforeach()
message(STATUS "Ranked actor calls, countdown and return preserve all five assembly traces.")
