# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED APP OR "${APP}" STREQUAL "")
    message(FATAL_ERROR "APP is required.")
endif()

# DC_NATIVE_ADMISSION 02:4FC4 15108 15108
#
# The long player-session-6 capture reaches the no-write return and the
# outside-tail hand-off. This ROM-free fixture independently covers both
# early exits plus values immediately below, at, and above the 24-bit clock
# deadline, and the decimal/T-mode refusal.
execute_process(
    COMMAND "${APP}" --verify-side-gate-deadline-check
    RESULT_VARIABLE fixture_result
    OUTPUT_VARIABLE fixture_output
    ERROR_VARIABLE fixture_error
)
if(NOT fixture_result EQUAL 0)
    message(
        FATAL_ERROR
        "Side-gate deadline fixture failed (${fixture_result}).\n"
        "${fixture_output}${fixture_error}"
    )
endif()

string(
    FIND
    "${fixture_output}"
    "Side-gate deadline semantic fixture verified: 30 branch-and-clock cases"
    fixture_line
)
if(fixture_line EQUAL -1)
    message(
        FATAL_ERROR
        "Side-gate deadline fixture omitted its result.\n"
        "${fixture_output}${fixture_error}"
    )
endif()

message(STATUS "Semantic 4FC4 side-gate deadline fixture covers all exits")
