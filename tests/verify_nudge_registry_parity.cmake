# SPDX-License-Identifier: GPL-3.0-or-later
foreach(required IN ITEMS APP ROM OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
# The old registry golden captured premature CLI interrupt sampling at the
# lanes nudge. Compare the corrected controller to the core with every OTHER
# registry routine still deployed, not merely to another native golden.
foreach(mode IN ITEMS core native)
    set(exclusion)
    if(mode STREQUAL "core")
        set(exclusion --registry-exclude "01:C3CB")
    endif()
    execute_process(COMMAND "${APP}" --record-gameplay-lanes "${OUTPUT}.${mode}.csv"
        --replace-registry ${exclusion}
        --work-ram-write-range 2100-21FF --work-ram-write-trace "${OUTPUT}.${mode}.stack.csv"
        --bus-write-range 0000-0003 --bus-write-range 0800-0809
        --bus-write-trace "${OUTPUT}.${mode}.bus.csv" --rom "${ROM}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Registry/nudge ${mode} failed: ${output}${error}")
    endif()
endforeach()
foreach(suffix IN ITEMS csv stack.csv bus.csv)
    file(SHA256 "${OUTPUT}.core.${suffix}" core_hash)
    file(SHA256 "${OUTPUT}.native.${suffix}" native_hash)
    if(NOT core_hash STREQUAL native_hash)
        message(FATAL_ERROR "Registry/nudge ${suffix} differs: ${core_hash}/${native_hash}")
    endif()
endforeach()
file(SHA256 "${OUTPUT}.native.csv" state_hash)
if(NOT state_hash STREQUAL "16886029410898c3b8300a2d229dc42352c2d64ff64fbe0d3f64c28f6a0eaea7")
    message(FATAL_ERROR "Corrected lanes state changed: ${state_hash}")
endif()
message(STATUS "Corrected lanes registry matches the core nudge controller, including stack and bus writes.")
