# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM)
    if(NOT DEFINED ${required_variable}
       OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported ball drag check configuration: ${CONFIG}")
endif()

# The ball-dragging cheat, end to end, with no mouse.
#
# A pointer's pixel has to be where the ball's sprite ends up: on the stitched
# table, on the cartridge's own screen, and in a bonus stage. On the table the
# camera has to come with the ball, since what is off camera is neither drawn
# nor run, and letting go while moving has to throw it. The check runs the
# census route's first ball with the native registry deployed, as people play,
# and once more on the emulated original.
foreach(mode IN ITEMS "" "--emulate-only")
    execute_process(
        COMMAND "${APP}" --cabinet-ball-drag-check ${mode} --rom "${ROM}"
        RESULT_VARIABLE check_result
        OUTPUT_VARIABLE check_output
        ERROR_VARIABLE check_error
    )
    if(NOT check_result EQUAL 0)
        message(
            FATAL_ERROR
            "--cabinet-ball-drag-check ${mode} failed (${check_result}):\n"
            "${check_output}${check_error}"
        )
    endif()
    string(FIND "${check_output}" "Ball drag check passed." passed_offset)
    if(passed_offset EQUAL -1)
        message(
            FATAL_ERROR
            "The ball drag check did not report passing:\n${check_output}"
        )
    endif()
endforeach()

message(STATUS "The ball is carried, the camera comes, and letting go throws.")
