# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED APP OR "${APP}" STREQUAL "")
    message(FATAL_ERROR "APP is required.")
endif()

# DC_NATIVE_ADMISSION 00:E813 2422 2422
# DC_NATIVE_ADMISSION 00:E825 2422 2422
#
# Both helpers occur once in each of the 2,422 natural E5EC calls in
# player-session-6.  The replay held E5EC itself back and observed every
# helper entry; this fixture exhaustively checks both scratch-word locations.
execute_process(
    COMMAND "${APP}" --verify-signed-word-normalizer
    RESULT_VARIABLE fixture_result
    OUTPUT_VARIABLE fixture_output
    ERROR_VARIABLE fixture_error
)
if(NOT fixture_result EQUAL 0)
    message(
        FATAL_ERROR
        "Signed word normalizer fixture failed (${fixture_result}).\n"
        "${fixture_output}${fixture_error}"
    )
endif()

message(STATUS "Semantic E813/E825 normalizer fixture covers every word")
