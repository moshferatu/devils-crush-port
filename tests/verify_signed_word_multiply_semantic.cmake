# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED APP OR "${APP}" STREQUAL "")
    message(FATAL_ERROR "APP is required.")
endif()

# DC_NATIVE_ADMISSION 00:E5EC 2422 2422
#
# The 2,422 natural calls are from player-session-6: the two E5EC callers are
# the paired signed coordinate products in the actor path.  The standalone
# fixture covers both signs and the 16-bit wrap; replay checks the nested E5BC
# stack frames and every interrupt horizon against the captured session.
execute_process(
    COMMAND "${APP}" --verify-signed-word-multiply
    RESULT_VARIABLE fixture_result
    OUTPUT_VARIABLE fixture_output
    ERROR_VARIABLE fixture_error
)
if(NOT fixture_result EQUAL 0)
    message(
        FATAL_ERROR
        "Signed word multiply fixture failed (${fixture_result}).\n"
        "${fixture_output}${fixture_error}"
    )
endif()

message(STATUS "Semantic E5EC signed word multiply fixture covers signed boundaries")
