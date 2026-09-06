# SPDX-License-Identifier: GPL-3.0-or-later

# DC_NATIVE_ADMISSION 08:5A3E 499 499
# DC_NATIVE_ADMISSION 08:5A6A 45 45
# DC_NATIVE_ADMISSION 08:5A7A 47 47
# DC_NATIVE_ADMISSION 08:5A86 45 45
# Counts refer to the real joypad-driven route, repeated for normal/wide glyphs.
foreach(required IN ITEMS TEST_APP ROM)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
execute_process(COMMAND "${TEST_APP}" "${ROM}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Selection actor replay failed.\n${output}${error}")
endif()
foreach(wide IN ITEMS 0 1)
    foreach(line IN ITEMS
        "08:5A3E@all entries: 499 of 499"
        "08:5A6A@all entries: 45 of 45"
        "08:5A7A@all entries: 47 of 47"
        "08:5A86@all entries: 45 of 45")
        string(FIND "${output}" "wide=${wide} Native replacement ${line}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "Missing admission coverage: ${line}\n${output}")
        endif()
    endforeach()
endforeach()
foreach(line IN ITEMS
    "wide=0 assembly evidence SHA-256: 527f6933cc4fbee4695db4be8b728f5295857353fb74fb4e224bdf8cd10b7797"
    "wide=1 assembly evidence SHA-256: cf6ac61b3b735ed91e1d1a617da32338300d989b9261acdeca05ae9cc48b1069"
    "Verified 43008 input/choice/limit/slot combinations and 256 stack positions.")
    string(FIND "${output}" "${line}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Selection actor oracle changed: ${line}\n${output}")
    endif()
endforeach()
message(STATUS "${output}")
