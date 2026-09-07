# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS APP ROM OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
set(expected_csv "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490")
set(expected_trace.csv "f446249e91fa82839448928ccb1800cccce61497de5f542af9626f1b935a92d3")
set(expected_writes.csv "2061f481a31f4fde9b12877df8aecc25b23b52205be0b208e9476417f938f620")
set(expected_bus.csv "b54d5c4ff41430623b0af9fe596d296aff7b84b4c5dd1ddc1af73eee26a6585e")
# Keep independent entry coverage for children now entirely inlined by the
# larger tick in verify_music_sequencer_semantic. This replay starts each
# command interpreter against the actual core caller stack, with no patches.
foreach(mode IN ITEMS assembly native)
    set(hooks)
    if(mode STREQUAL "native")
        set(hooks --replace-hook "45:457A@all"
            --replace-hook "45:4228@all" --replace-hook "45:49F9@all")
    endif()
    execute_process(COMMAND "${APP}" --record-state-census "${OUTPUT}.${mode}.csv"
        ${hooks} --observe-hook "45:40EE"
        --routine-trace "${OUTPUT}.${mode}.trace.csv"
        --work-ram-write-range 2000-3FFF --work-ram-write-trace "${OUTPUT}.${mode}.writes.csv"
        --bus-write-range 0800-0809 --bus-write-range 0C00-0C01
        --bus-write-trace "${OUTPUT}.${mode}.bus.csv" --rom "${ROM}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Command-exit ${mode} failed: ${output}${error}")
    endif()
    set(expected_lines "State census recording complete: 3600 frames"
        "Routine trace complete: 32391 entries"
        "Work-RAM write trace complete: 12647397 selected writes"
        "Bus write trace complete: 699251 selected writes")
    if(mode STREQUAL "native")
        list(APPEND expected_lines
            "Native replacement 45:457A@all entries: 2276 of 2299"
            "Native replacement 45:4228@all entries: 1142 of 1142"
            "Native replacement 45:49F9@all entries: 22647 of 22664")
    endif()
    foreach(line IN LISTS expected_lines)
        string(FIND "${output}" "${line}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "${mode} omitted ${line}: ${output}")
        endif()
    endforeach()
endforeach()
foreach(suffix IN ITEMS csv trace.csv writes.csv bus.csv)
    file(SHA256 "${OUTPUT}.assembly.${suffix}" assembly_hash)
    file(SHA256 "${OUTPUT}.native.${suffix}" native_hash)
    if(NOT assembly_hash STREQUAL native_hash OR NOT assembly_hash STREQUAL expected_${suffix})
        message(FATAL_ERROR "Command-exit ${suffix} differs: ${assembly_hash} / ${native_hash}")
    endif()
    message(STATUS "Command-exit ${suffix}: ${assembly_hash}")
endforeach()
