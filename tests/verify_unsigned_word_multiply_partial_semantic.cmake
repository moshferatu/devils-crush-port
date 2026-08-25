# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED APP OR "${APP}" STREQUAL "")
    message(FATAL_ERROR "APP is required.")
endif()

# DC_NATIVE_ADMISSION 00:E5BC 2423 2423
#
# The 2,423 natural calls are from replay-verified player-session-6.  This
# ROM-free fixture drives every byte multiplier pattern over a representative
# 16-bit accumulator/multiplicand grid, including all ordered-write and
# decimal/T-mode guards in the production dispatcher.
execute_process(
    COMMAND "${APP}" --verify-unsigned-word-multiply-partial
    RESULT_VARIABLE fixture_result
    OUTPUT_VARIABLE fixture_output
    ERROR_VARIABLE fixture_error
)
if(NOT fixture_result EQUAL 0)
    message(
        FATAL_ERROR
        "Unsigned-word-multiply partial fixture failed (${fixture_result}).\n"
        "${fixture_output}${fixture_error}"
    )
endif()

string(
    FIND
    "${fixture_output}"
    "Unsigned-word-multiply partial semantic fixture verified: 4096 operand cases"
    fixture_line
)
if(fixture_line EQUAL -1)
    message(
        FATAL_ERROR
        "Unsigned-word-multiply partial fixture omitted its result.\n"
        "${fixture_output}${fixture_error}"
    )
endif()

message(STATUS "Semantic E5BC partial multiply fixture covers all 256 multiplier patterns")
