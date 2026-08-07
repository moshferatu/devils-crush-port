# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP ROM)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

# The interactive host is the only path a cabinet session takes, so the guard
# that keeps ported code running there is that this path both installs the
# registry by default and actually executes it. Ten boot frames already accept
# native calls, so this asserts acceptance rather than mere installation.
# Cycle share over a whole route is measured by the recording regressions.
# The dummy SDL drivers come from the test's ENVIRONMENT property.

execute_process(
    COMMAND "${APP}" --cabinet-smoke-test --rom "${ROM}"
    RESULT_VARIABLE deployed_result
    OUTPUT_VARIABLE deployed_output
    ERROR_VARIABLE deployed_error
)
if(NOT deployed_result EQUAL 0)
    message(
        FATAL_ERROR
        "Host registry smoke test failed (${deployed_result}).\n"
        "${deployed_output}${deployed_error}"
    )
endif()

string(
    REGEX MATCH
    "Native registry: ([0-9]+) of ([0-9]+) admitted routines deployed \\(([0-9]+) withheld"
    installed_line
    "${deployed_output}"
)
if(installed_line STREQUAL "")
    message(
        FATAL_ERROR
        "The interactive host did not deploy the native registry.\n"
        "${deployed_output}${deployed_error}"
    )
endif()
set(deployed_count "${CMAKE_MATCH_1}")
set(admitted_count "${CMAKE_MATCH_2}")
set(withheld_count "${CMAKE_MATCH_3}")
if(admitted_count LESS_EQUAL 0)
    message(FATAL_ERROR "The admitted-routine registry is empty.")
endif()
# Nothing claims a key on this path, so the only admitted routines the host
# may skip are the declared state-diverging hold-backs.
math(EXPR expected_deployed "${admitted_count} - ${withheld_count}")
if(NOT deployed_count EQUAL expected_deployed)
    message(
        FATAL_ERROR
        "The interactive host deployed ${deployed_count} routines, but "
        "${admitted_count} admitted minus ${withheld_count} withheld is "
        "${expected_deployed}.\n"
        "${deployed_output}${deployed_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "Registry hold-backs: "
    "Registry deployment: "
    "Registry cycles: "
    "Cabinet host smoke test passed"
)
    string(FIND "${deployed_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Host registry output omitted '${expected_line}'.\n"
            "${deployed_output}${deployed_error}"
        )
    endif()
endforeach()

# Installation alone would still leave the cabinet running the emulated
# original if every handler declined, so this run must accept native calls.
string(
    REGEX MATCH
    "Registry calls: ([0-9]+) native of ([0-9]+) entered"
    calls_line
    "${deployed_output}"
)
if(calls_line STREQUAL "")
    message(
        FATAL_ERROR
        "Host registry output omitted its call summary.\n"
        "${deployed_output}${deployed_error}"
    )
endif()
if(CMAKE_MATCH_1 LESS_EQUAL 0)
    message(
        FATAL_ERROR
        "The interactive host entered ${CMAKE_MATCH_2} admitted routines but "
        "executed none of them natively.\n"
        "${deployed_output}${deployed_error}"
    )
endif()

# --emulate-only is the A/B control for a misbehaving live session, so it must
# leave the machine untouched rather than merely quiet.
execute_process(
    COMMAND "${APP}" --cabinet-smoke-test --emulate-only --rom "${ROM}"
    RESULT_VARIABLE emulated_result
    OUTPUT_VARIABLE emulated_output
    ERROR_VARIABLE emulated_error
)
if(NOT emulated_result EQUAL 0)
    message(
        FATAL_ERROR
        "Emulate-only smoke test failed (${emulated_result}).\n"
        "${emulated_output}${emulated_error}"
    )
endif()

foreach(forbidden_line IN ITEMS "Native registry: " "Registry deployment: ")
    string(FIND "${emulated_output}" "${forbidden_line}" line_offset)
    if(NOT line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "--emulate-only still deployed the registry ('${forbidden_line}').\n"
            "${emulated_output}${emulated_error}"
        )
    endif()
endforeach()

message(
    STATUS
    "The interactive host deploys all ${admitted_count} admitted routines, "
    "and --emulate-only deploys none."
)
