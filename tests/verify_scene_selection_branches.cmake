# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Scene-selection branch replay requires Release.")
endif()

# Supplement the unpatched admission route with controlled menu choices.
# These goldens were recorded from assembly with the same one-byte choice
# injection. They cover 5747's exit, the optional 76BB child/5751 return,
# and both nonzero choices at 577C (including repeated selection-page visits).
set(cases
    "mode-zero,699:222E:00,995bf667e8204ae6ffc986e7969c6d13e71b978d003f02bd9cf735983fa28f04,a5a5db34031a4d5984769a2e42025dd55cbb23dc1f25e1734d6384243a144dc6"
    "mode-one,699:222E:01,16f42b1ccf2a1f9bbc1a017d01f9d500f48bfa6df5af98510f8da8329e496572,48d887888c1b9cafc67508d396c125c2cad8fe2699991f49db5238c61add2b84"
    "choice-one,849:205D:01,86391abf9b63425a9b60dcf8a4a81dec5b34eda2f34df45ecd444859c7d956f9,9e0e8e9d7773b5fc0e8deebcb92466c5b4ab75287b1c6d40b3a116125c1443b5"
    "choice-two,849:205D:02,6db9c53fc7682177a87c154bba6adf957fe0be65ca723da53ac4a86b2917bf19,62528e79ddbc45e9d571ba76190965abaa4f23031d7817738f5a7e84b317decd")
set(boundary_hooks)
foreach(boundary IN ITEMS "08:5ABF" "00:E321" "01:C4DE" "08:4036" "01:C574" "00:ECFD")
    list(APPEND boundary_hooks --observe-hook "${boundary}")
endforeach()
foreach(case IN LISTS cases)
    string(REPLACE "," ";" fields "${case}")
    list(GET fields 0 name)
    list(GET fields 1 patch)
    list(GET fields 2 expected_state)
    list(GET fields 3 expected_trace)
    set(pcs 56D3 56DE 56E9 56EC 56EF 56F2 56F5 5702 5712 571F
            5722 5725 5728 572B 5738 5742)
    if(NOT name STREQUAL "mode-zero")
        if(name STREQUAL "mode-one")
            list(APPEND pcs 5751)
        endif()
        list(APPEND pcs 575E 5761 5764 5767 576A 5775 577A)
    endif()
    set(hooks)
    foreach(pc IN LISTS pcs)
        list(APPEND hooks --replace-hook "08:${pc}@all")
    endforeach()
    set(state_path "${OUTPUT}.${name}.csv")
    set(trace_path "${state_path}.trace.csv")
    execute_process(
        COMMAND "${APP}" --record-gameplay "${state_path}"
            --work-ram-patch "${patch}" ${hooks} ${boundary_hooks}
            --routine-trace "${trace_path}" --rom "${ROM}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Scene-selection ${name} failed (${result}).\n${output}${error}")
    endif()
    foreach(pc IN LISTS pcs)
        string(REGEX MATCH "Native replacement 08:${pc}@all entries: ([0-9]+) of ([0-9]+)" matched "${output}")
        if(NOT matched OR NOT CMAKE_MATCH_1 STREQUAL CMAKE_MATCH_2 OR CMAKE_MATCH_1 EQUAL 0)
            message(FATAL_ERROR "Incomplete native branch coverage at ${pc}.\n${output}${error}")
        endif()
    endforeach()
    foreach(artifact IN ITEMS state trace)
        file(SHA256 "${${artifact}_path}" actual)
        if(NOT actual STREQUAL expected_${artifact})
            message(FATAL_ERROR "Scene-selection ${name} ${artifact} changed: ${actual}; expected ${expected_${artifact}}")
        endif()
    endforeach()
endforeach()
message(STATUS "All four alternate scene-selection branches preserve their assembly goldens.")
