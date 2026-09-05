# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release" AND NOT CONFIG STREQUAL "Debug")
    message(FATAL_ERROR "Unsupported scene-selection configuration: ${CONFIG}")
endif()

# Independently recorded assembly goldens, including every RAM write (address,
# value, source instruction, frame and cycle), not just final menu selections.
set(expected_state "1f6d46ac99798bbac1ecab97702d9e10076b9766796cc64d22d9662efce0b353")
set(expected_writes "da606d14dd48b5b73e86096ba4033c6bf40c67e97ea771e5705760a02bbe3e1c")
set(expected_trace "742e634b75fc28ba240076e43bd6b2a232a08876277c9863789e29d9de2af810")
set(boundary_hooks)
foreach(boundary IN ITEMS "08:5ABF" "08:59B5" "00:E321" "01:C4DE"
                          "08:4036" "01:C574" "00:ECFD" "08:59CE"
                          "08:59E7" "08:584C")
    list(APPEND boundary_hooks --observe-hook "${boundary}")
endforeach()

set(controller_hooks)
foreach(pc IN ITEMS 56D3 56DE 56E9 56EC 56EF 56F2 56F5 5702 5712
                    571F 5722 5725 5728 572B 5738 5742
                    575E 5761 5764 5767 576A 5775 577A)
    list(APPEND controller_hooks --replace-hook "08:${pc}@all")
endforeach()

# 5751 is folded into 5742 on this route; also test it as an independent real
# continuation, as used when the optional 76BB child returns to the controller.
foreach(variant IN ITEMS controller third_page_entry)
    if(variant STREQUAL "controller")
        set(hooks ${controller_hooks})
    else()
        set(hooks --replace-hook "08:5751@all")
    endif()
    set(state_path "${OUTPUT}.${variant}.csv")
    set(writes_path "${state_path}.writes.csv")
    set(trace_path "${state_path}.trace.csv")
    execute_process(
        COMMAND "${APP}" --record-gameplay "${state_path}"
            ${hooks} ${boundary_hooks} --routine-trace "${trace_path}"
            --work-ram-write-range 2000-3FFF
            --work-ram-write-trace "${writes_path}"
            --rom "${ROM}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Scene-selection ${variant} failed (${result}).\n${output}${error}")
    endif()
    set(expected_lines "Gameplay recording complete: 1800 frames"
        "Work-RAM write trace complete: 4270592 selected writes"
        "Routine trace complete: 5330 entries")
    if(variant STREQUAL "controller")
        list(APPEND expected_lines
            "Native replacement 08:56D3@all entries: 1 of 1"
            "Native replacement 08:56DE@all entries: 1 of 1"
            "Native replacement 08:56E9@all entries: 1 of 1"
            "Native replacement 08:56EC@all entries: 1 of 1"
            "Native replacement 08:56EF@all entries: 120 of 120"
            "Native replacement 08:56F2@all entries: 120 of 120"
            "Native replacement 08:56F5@all entries: 120 of 120"
            "Native replacement 08:5702@all entries: 1 of 1"
            "Native replacement 08:5712@all entries: 1 of 1"
            "Native replacement 08:571F@all entries: 1 of 1"
            "Native replacement 08:5722@all entries: 1 of 1"
            "Native replacement 08:5725@all entries: 280 of 280"
            "Native replacement 08:5728@all entries: 280 of 280"
            "Native replacement 08:572B@all entries: 280 of 280"
            "Native replacement 08:5738@all entries: 1 of 1"
            "Native replacement 08:5742@all entries: 1 of 1"
            "Native replacement 08:575E@all entries: 1 of 1"
            "Native replacement 08:5761@all entries: 1 of 1"
            "Native replacement 08:5764@all entries: 150 of 150"
            "Native replacement 08:5767@all entries: 150 of 150"
            "Native replacement 08:576A@all entries: 150 of 150"
            "Native replacement 08:5775@all entries: 1 of 1"
            "Native replacement 08:577A@all entries: 1 of 1")
    else()
        list(APPEND expected_lines "Native replacement 08:5751@all entries: 1 of 1")
    endif()
    foreach(line IN LISTS expected_lines)
        string(FIND "${output}" "${line}" offset)
        if(offset EQUAL -1)
            message(FATAL_ERROR "Scene-selection omitted '${line}'.\n${output}${error}")
        endif()
    endforeach()
    foreach(artifact IN ITEMS state writes trace)
        file(SHA256 "${${artifact}_path}" actual)
        if(NOT actual STREQUAL expected_${artifact})
            message(FATAL_ERROR "Scene-selection ${variant} ${artifact} changed: ${actual}; expected ${expected_${artifact}}")
        endif()
    endforeach()
endforeach()
message(STATUS "Scene-selection controller preserves assembly RAM, video, audio and all ordered RAM writes.")
