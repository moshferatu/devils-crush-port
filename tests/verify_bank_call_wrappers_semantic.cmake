# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required_variable IN ITEMS APP CONFIG ROM OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Unsupported bank-call-wrapper configuration: ${CONFIG}")
endif()

# The bank-switching call wrappers. Every cross-bank call in the game goes
# through one of these: the caller leaves a physical bank in $30 and an entry
# offset in $2F, and a wrapper maps the bank into an MPR slot, jumps through the
# callee's $40xx table, and unmaps it on the way out. $1E-$21 are the zero-page
# shadows of MPR2-MPR5, and keeping the shadow and the register in step is what
# makes the idiom re-entrant.
#
# Nine entry points share one walk. Four are heads; the rest are the addresses
# each wrapper's call comes back to, because a wrapper exists to call something
# and the transcript has to stop in front of that call.
#
#   00:E20D  map $30 into MPR4/MPR5
#   00:E200  the asset resolver's tail, reading its table through that slot
#   00:E219  map the fixed bank pair for the current mode at $55
#   00:E241  save MPR2/MPR3, map $30 into them, stop at the JSR $E262
#   00:E255  store the callee's result in $2A, restore MPR2/MPR3
#   00:E262  the trampoline: build $40xx in $2CBE/$2CBF, stop at the JMP
#   00:ECC6  the VBlank handler's music call: save MPR2-MPR4, map banks $45-$47
#   00:ECE2  restore MPR2-MPR4
#   00:ECF2  point $47/$48 at $4003, stop at the JMP into bank $45
#
# All nine are admitted on every call. They carry no game logic, read nothing an
# interrupt handler leaves changed, and have bodies of 36 to 162 master cycles
# against a 5,068-cycle timer period, so there is nothing for the horizon valve
# or the shared-state guard to decline.
#
# The whole stack page is in scope rather than a window of it. This family is
# nothing but stack-balanced save and restore, and its pushes land wherever the
# caller left S -- $21E2 through $21FD on this route -- so the page is the
# natural boundary. Both artifacts are compared against what a pure-assembly run
# of the same route produces, so the writes match in value, address, order and
# master-cycle offset rather than merely in end state. That is the check that
# matters here: a wrapper writes an MPR shadow and the register together, and a
# transcript that misprices either one moves a bank switch without moving a
# byte.
set(
    expected_census_sha256
    "9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490"
)
set(
    expected_write_sha256
    "7beefad705272c086822e10295b03a4d0adcd96703f279b434ca69516a1b8c6b"
)

set(write_output "${OUTPUT}.writes.csv")

execute_process(
    COMMAND
        "${APP}"
        --record-state-census
        "${OUTPUT}"
        --replace-hook
        "00:E200@all"
        --replace-hook
        "00:E20D@all"
        --replace-hook
        "00:E219@all"
        --replace-hook
        "00:E241@all"
        --replace-hook
        "00:E255@all"
        --replace-hook
        "00:E262@all"
        --replace-hook
        "00:ECC6@all"
        --replace-hook
        "00:ECE2@all"
        --replace-hook
        "00:ECF2@all"
        --work-ram-write-range
        "201E-2021"
        --work-ram-write-range
        "202A-2030"
        --work-ram-write-range
        "2047-2048"
        --work-ram-write-range
        "2100-21FF"
        --work-ram-write-range
        "2CBE-2CBF"
        --work-ram-write-trace
        "${write_output}"
        --rom
        "${ROM}"
    RESULT_VARIABLE recorder_result
    OUTPUT_VARIABLE recorder_output
    ERROR_VARIABLE recorder_error
)
if(NOT recorder_result EQUAL 0)
    message(
        FATAL_ERROR
        "Semantic bank-call-wrapper recorder failed (${recorder_result}).\n"
        "${recorder_output}${recorder_error}"
    )
endif()

foreach(expected_line IN ITEMS
    "State census recording complete: 3600 frames"
    "Native replacement 00:E200@all entries: 1861 of 1861"
    "Native replacement 00:E20D@all entries: 1861 of 1861"
    "Native replacement 00:E219@all entries: 3523 of 3523"
    "Native replacement 00:E241@all entries: 11599 of 11599"
    "Native replacement 00:E255@all entries: 11599 of 11599"
    "Native replacement 00:E262@all entries: 11671 of 11671"
    "Native replacement 00:ECC6@all entries: 3599 of 3599"
    "Native replacement 00:ECE2@all entries: 3599 of 3599"
    "Native replacement 00:ECF2@all entries: 3599 of 3599"
    "Work-RAM write trace complete: 6389776 selected writes"
)
    string(FIND "${recorder_output}" "${expected_line}" line_offset)
    if(line_offset EQUAL -1)
        message(
            FATAL_ERROR
            "Semantic bank-call-wrapper output omitted '${expected_line}'.\n"
            "${recorder_output}${recorder_error}"
        )
    endif()
endforeach()

foreach(artifact IN ITEMS census write)
    if(artifact STREQUAL "census")
        set(artifact_path "${OUTPUT}")
    else()
        set(artifact_path "${write_output}")
    endif()

    file(SHA256 "${artifact_path}" actual_sha256)
    set(expected_sha256 "${expected_${artifact}_sha256}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(
            FATAL_ERROR
            "Semantic bank-call-wrapper ${artifact} artifact changed.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
        )
    endif()
endforeach()

message(
    STATUS
    "Semantic bank-switching call wrappers run all nine entry points natively "
    "across 52,911 admitted entries as instruction-boundary transcripts, with "
    "the MPR shadows, the trampoline vector, the sequencer entry pointer, the "
    "whole stack page and the state census identical to assembly."
)
