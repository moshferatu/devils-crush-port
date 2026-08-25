# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED APP OR "${APP}" STREQUAL "")
    message(FATAL_ERROR "APP is required.")
endif()

# DC_NATIVE_ADMISSION 00:E85D 665 665
#
# The 665 natural calls are the replay-verified player-session evidence.  The
# executable fixture is intentionally ROM-free: it exercises all 65,536 input
# words through the production dispatcher, including the full ordered-write
# transcript and D/T-mode refusal, so it remains portable in ordinary CTest.
execute_process(
    COMMAND "${APP}" --verify-unsigned-square-root
    RESULT_VARIABLE fixture_result
    OUTPUT_VARIABLE fixture_output
    ERROR_VARIABLE fixture_error
)
if(NOT fixture_result EQUAL 0)
    message(FATAL_ERROR "Unsigned square-root fixture failed (${fixture_result}).\n${fixture_output}${fixture_error}")
endif()

string(
    FIND
    "${fixture_output}"
    "Unsigned square-root semantic fixture verified: 65536 exhaustive operands"
    fixture_line
)
if(fixture_line EQUAL -1)
    message(FATAL_ERROR "Unsigned square-root fixture omitted its exhaustive result.\n${fixture_output}${fixture_error}")
endif()

message(STATUS "Semantic E85D fixture covers every unsigned 16-bit operand")
