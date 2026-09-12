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

# Normal play must show actual ownership without legacy coverage percentages.
execute_process(
    COMMAND "${APP}" --cabinet-smoke-test --rom "${ROM}"
    RESULT_VARIABLE compact_result OUTPUT_VARIABLE compact_output
    ERROR_VARIABLE compact_error
)
if(NOT compact_result EQUAL 0)
    message(FATAL_ERROR "Compact host report failed: ${compact_error}")
endif()
foreach(expected IN ITEMS
    "via native cabinet flipper input hook (01:C500)"
    "Registry routines entered:"
    "Native execution outside wait/stalls:"
    "Wait cycles:"
    "Execution accounting:"
    "Interpreter residue outside frame-sync wait:"
    "Registry abandonments:")
    string(FIND "${compact_output}" "${expected}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Compact report omitted ${expected}: ${compact_output}")
    endif()
endforeach()
foreach(hidden IN ITEMS "Registry cycles:" "Registry cycles outside"
    "Registry core capabilities:" "Registry auxiliary transcripts:"
    "Core replacement outcomes:" "Interpreter residue, top 0"
    "Native registry:")
    string(FIND "${compact_output}" "${hidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Compact report leaked ${hidden}: ${compact_output}")
    endif()
endforeach()
execute_process(
    COMMAND "${APP}" --cabinet-smoke-test --execution-details --rom "${ROM}"
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
    "Native registry: ([0-9]+) of ([0-9]+) admitted routines deployed \\(([0-9]+) withheld as state-diverging, ([0-9]+) claimed"
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
set(claimed_count "${CMAKE_MATCH_4}")
if(admitted_count LESS_EQUAL 0)
    message(FATAL_ERROR "The admitted-routine registry is empty.")
endif()
# An admitted routine the host does not run is either a declared
# state-diverging hold-back or a key the host claimed for a hook of its own,
# and both have to be counted out loud. The arithmetic is the point: a routine
# that quietly stopped deploying would be indistinguishable from one deployed
# and never entered, and this is the only line that can tell them apart.
math(EXPR expected_deployed
    "${admitted_count} - ${withheld_count} - ${claimed_count}")
if(NOT deployed_count EQUAL expected_deployed)
    message(
        FATAL_ERROR
        "The interactive host deployed ${deployed_count} routines, but "
        "${admitted_count} admitted minus ${withheld_count} withheld minus "
        "${claimed_count} claimed is ${expected_deployed}.\n"
        "${deployed_output}${deployed_error}"
    )
endif()

# Each claim has to say what took the key and why, so that a claim can be read
# back rather than merely counted. The cabinet claims 01:C500 so its flipper
# restore runs before the shared native cursor suffix; the ordinary registry
# must not occupy this key first.
if(claimed_count GREATER 0)
    string(FIND "${deployed_output}" "Registry claim: " claim_offset)
    if(claim_offset EQUAL -1)
        message(
            FATAL_ERROR
            "The host claimed ${claimed_count} routines without naming any.\n"
            "${deployed_output}${deployed_error}"
        )
    endif()
endif()

# `Registry abandonments` reads a counter the patched core owns. Ten frames is
# not enough to pin a number -- it is one or two -- but the line's presence is
# what says the count reached the host at all, which is the half of the seam
# that lives in this repository. The other half, the core actually reporting an
# abandoned transcript, is guarded at configure time: the marker branch for
# `0020` looks for `dc_beetle_active_replacement_abandoned_hook` in the core
# source and fails the build if a patched tree does not have it.
foreach(expected_line IN ITEMS
    "Registry claim: 01:C500 claimed by cabinet flipper input handoff."
    "Registry hold-backs: "
    "Registry deployment: "
    "Registry cycles: "
    "Registry abandonments: "
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
    "Registry calls: ([0-9]+) accepted of ([0-9]+) entered"
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
