# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED CORE_TESTS OR "${CORE_TESTS}" STREQUAL "")
    message(FATAL_ERROR "CORE_TESTS is required.")
endif()

# DC_NATIVE_ADMISSION 00:E728 240 240
#
# The 240 natural calls are from replay-verified player-session-6: 160 enter
# from E7C3 and 80 from CA1D, all return through E7C6/CA20 after E73C.  The
# core fixture checks the integer/fractional quotient splice, zero divisor,
# and truncated-snapshot guard; the session replay checks every instruction
# checkpoint, stack write, and frame hash.
execute_process(
    COMMAND "${CORE_TESTS}"
    RESULT_VARIABLE fixture_result
    OUTPUT_VARIABLE fixture_output
    ERROR_VARIABLE fixture_error
)
if(NOT fixture_result EQUAL 0)
    message(
        FATAL_ERROR
        "Unsigned fixed division core fixture failed (${fixture_result}).\n"
        "${fixture_output}${fixture_error}"
    )
endif()

message(STATUS "Semantic E728 fixed division fixture covers the arithmetic model")
