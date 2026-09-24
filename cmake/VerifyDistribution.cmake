# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED DISTRIBUTION_DIR OR DISTRIBUTION_DIR STREQUAL "")
    message(FATAL_ERROR "DISTRIBUTION_DIR is required.")
endif()
if(NOT IS_DIRECTORY "${DISTRIBUTION_DIR}")
    message(
        FATAL_ERROR
        "Distribution directory does not exist: ${DISTRIBUTION_DIR}"
    )
endif()

set(
    expected_files
    "LICENSE"
    "README.md"
    "THIRD_PARTY_NOTICES.md"
    "devils-crush-launcher.exe"
    "devils-crush.exe"
)
file(
    GLOB_RECURSE distribution_files
    LIST_DIRECTORIES false
    RELATIVE "${DISTRIBUTION_DIR}"
    "${DISTRIBUTION_DIR}/*"
)
list(SORT expected_files)
list(SORT distribution_files)
if(NOT "${distribution_files}" STREQUAL "${expected_files}")
    string(JOIN "\n  " expected_text ${expected_files})
    string(JOIN "\n  " actual_text ${distribution_files})
    message(
        FATAL_ERROR
        "Distribution contents differ from the allowlist.\n"
        "Expected:\n  ${expected_text}\n"
        "Actual:\n  ${actual_text}"
    )
endif()

set(
    supported_rom_sha256
    "9ad122903ec34cb8e62ba1aae1cfddbfcb06657d0fba55d6d0b1d280f88e30f9"
)
foreach(relative_path IN LISTS distribution_files)
    set(absolute_path "${DISTRIBUTION_DIR}/${relative_path}")
    file(SIZE "${absolute_path}" file_size)
    if("${file_size}" EQUAL 393216)
        file(SHA256 "${absolute_path}" file_sha256)
        if("${file_sha256}" STREQUAL "${supported_rom_sha256}")
            message(
                FATAL_ERROR
                "The proprietary ROM is present: ${relative_path}"
            )
        endif()
    endif()
endforeach()

message(
    STATUS
    "Distribution verified (${DISTRIBUTION_DIR}): ${distribution_files}"
)
