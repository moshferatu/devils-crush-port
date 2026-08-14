# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM)
    if(NOT DEFINED ${required_variable}
       OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported plunger check configuration: ${CONFIG}")
endif()

# An analog plunger, end to end, with no plunger.
#
# The feature's whole claim is that how far the rod is pulled decides how hard
# the ball is launched. That is one arithmetic chain through the cartridge and
# it can be checked exactly, because every step of it is a byte somebody can
# read.
#
# `02:52E3` derives the Devil Shooter's charge as `$3300,X - $3E00,X`: the
# whole-pixel distance the shooter's own piece has been pulled down from the
# rest position `02:50EE` gave it. The host writes that piece's position from
# the rod, so a draw of d percent should settle the charge at `d * $27 / 100`.
# `02:51DA` then turns the charge into the ball's launch velocity as
# `-(($2D15 >> 2) + 2)`. Composing the two:
#
# | draw | charge | velocity                |
# | ---: | -----: | ----------------------: |
# |  25% |      9 | -((9 >> 2) + 2)  = $FC  |
# |  50% |     19 | -((19 >> 2) + 2) = $FA  |
# |  75% |     29 | -((29 >> 2) + 2) = $F7  |
# | 100% |     39 | -((39 >> 2) + 2) = $F5  |
#
# Those eight numbers are computed here from the ROM's own arithmetic and
# nowhere in the host, so agreeing with them is a statement about the machine.
# Four draws that produced one velocity would be the failure a button-mapped
# plunger has, where the pull distance reaches the game as nothing at all.
#
# The 25% row does not escape the shooter lane, and that is the cartridge's
# behaviour rather than a shortfall: four pixels a frame will not climb it, and
# a short press of button I on the original does the same thing. So the row is
# pinned as it is -- a launch that happened, at a velocity too small to get
# away -- because a change that made a quarter draw escape would be a change to
# the game.
set(
    expected_rows
    "25%       9   $FC"
    "50%      19   $FA"
    "75%      29   $F7"
    "100%      39   $F5"
)

# And the ball has to be on the shooter when it fires.
#
# Nothing in the charge path places it. While the shooter descends the ball
# simply rests on it and falls after it, which works because the cartridge
# descends a quarter of a pixel per tick; measured on the census route, the
# ball tracks nine pixels above the shooter through the whole 96-frame charge.
# A rod moves ten pixels in a frame, gravity cannot follow, and the shooter
# slides out from under a ball that stays where it was -- 14, 24, 34 and 44
# pixels adrift at these four draws before the ride was taken over, growing
# with the draw exactly as a race between a rod and gravity would.
#
# `02:52CA SBC #$09` is where the cartridge itself says nine, so nine is what
# the check demands, and the probe's own output line says it happened.
set(expected_ride "the ball rode the rod in every one")

execute_process(
    COMMAND "${APP}" --cabinet-plunger-check --rom "${ROM}"
    RESULT_VARIABLE check_result
    OUTPUT_VARIABLE check_output
    ERROR_VARIABLE check_error
)

if(NOT check_result EQUAL 0)
    message(
        FATAL_ERROR
        "--cabinet-plunger-check failed (${check_result}):\n"
        "${check_output}${check_error}"
    )
endif()

foreach(row IN LISTS expected_rows)
    string(FIND "${check_output}" "${row}" row_offset)
    if(row_offset EQUAL -1)
        message(
            FATAL_ERROR
            "The plunger check did not report \"${row}\":\n${check_output}"
        )
    endif()
endforeach()

# The four velocities have to be four, not merely present: the check itself
# refuses to succeed otherwise, and this pins the sentence it says so with.
string(FIND "${check_output}" "4 distinct launch velocities" distinct_offset)
if(distinct_offset EQUAL -1)
    message(
        FATAL_ERROR
        "The plunger check did not report four distinct launch velocities:\n"
        "${check_output}"
    )
endif()

string(FIND "${check_output}" "${expected_ride}" ride_offset)
if(ride_offset EQUAL -1)
    message(
        FATAL_ERROR
        "The plunger check did not report the ball riding the rod:\n"
        "${check_output}"
    )
endif()

message(
    STATUS
    "Plunger draw reaches the shooter: four draws, four shots, ball aboard."
)
