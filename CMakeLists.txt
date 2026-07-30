cmake_minimum_required(VERSION 3.30)

project(
    DevilsCrushPort
    VERSION 0.1.0
    DESCRIPTION "Hybrid PC Engine emulation and native game-logic port of Devil's Crush"
    LANGUAGES C CXX
)

option(DC_BUILD_APP "Build the SDL3 Windows host application" ON)
option(DC_BUILD_TESTS "Build the project test executables" ON)
option(DC_FETCH_SDL "Fetch the pinned SDL3 dependency during configuration" ON)
option(
    DC_ENABLE_ROM_TESTS
    "Enable local tests that require the proprietary Devil's Crush ROM"
    OFF
)
set(
    DC_ROM_PATH
    ""
    CACHE FILEPATH
    "Path to the local supported ROM when DC_ENABLE_ROM_TESTS is ON"
)
option(
    DC_FETCH_BEETLE
    "Fetch and build the pinned Beetle PCE hardware-emulation core"
    ON
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CompilerWarnings)

add_library(
    dc_core
    STATIC
    src/cabinet/cabinet_state.cpp
    src/cabinet/display_config.cpp
    src/core/backup_ram.cpp
    src/core/rom_image.cpp
    src/core/save_state.cpp
    src/render/pce_background.cpp
    src/render/pce_sprite_overlay.cpp
    src/render/sharp_pixel_scaler.cpp
)

add_library(DevilsCrush::Core ALIAS dc_core)

target_include_directories(
    dc_core
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_compile_features(dc_core PUBLIC cxx_std_20)
dc_enable_strict_warnings(dc_core)

add_library(
    dc_game_logic
    STATIC
    src/game_logic/actor.c
    src/game_logic/collision.c
    src/game_logic/controls.c
    src/game_logic/physics.c
    src/game_logic/rng.c
    src/game_logic/score.c
)

add_library(DevilsCrush::GameLogic ALIAS dc_game_logic)

target_include_directories(
    dc_game_logic
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

dc_enable_strict_warnings(dc_game_logic)

if(WIN32)
    target_link_libraries(dc_core PRIVATE bcrypt)
else()
    message(FATAL_ERROR "Milestone 1 currently supports Windows only.")
endif()

if(DC_FETCH_BEETLE)
    include(BeetlePce)
    dc_provide_beetle_pce()
    set(dc_emulation_source src/emulation/pce_runtime_beetle.cpp)
else()
    set(dc_emulation_source src/emulation/pce_runtime_stub.cpp)
endif()

add_library(dc_emulation STATIC "${dc_emulation_source}")

add_library(DevilsCrush::Emulation ALIAS dc_emulation)

target_include_directories(
    dc_emulation
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_compile_features(dc_emulation PUBLIC cxx_std_20)
dc_enable_strict_warnings(dc_emulation)

if(DC_FETCH_BEETLE)
    target_link_libraries(
        dc_emulation
        PRIVATE
            DevilsCrush::BeetlePce
    )
    target_compile_definitions(
        dc_emulation
        PRIVATE
            DC_BEETLE_PCE_REVISION="${DC_BEETLE_PCE_REVISION}"
    )
endif()

if(DC_BUILD_APP)
    include(Dependencies)
    dc_provide_sdl3()
    dc_provide_sdl3_image()

    add_library(
        dc_image
        STATIC
        src/render/backglass_image.cpp
    )
    add_library(DevilsCrush::Image ALIAS dc_image)
    target_include_directories(
        dc_image
        PUBLIC
            "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    )
    target_link_libraries(
        dc_image
        PUBLIC
            SDL3::SDL3
        PRIVATE
            SDL3_image::SDL3_image
    )
    target_compile_features(dc_image PUBLIC cxx_std_20)
    dc_enable_strict_warnings(dc_image)

    add_executable(
        devils_crush
        src/app/main.cpp
        src/app/native_replacements.cpp
        src/app/native_replacements_collision.cpp
        src/app/native_replacements_controls.cpp
        src/app/native_replacements_physics.cpp
        src/app/native_replacements_rules.cpp
    )

    target_link_libraries(
        devils_crush
        PRIVATE
            DevilsCrush::Core
            DevilsCrush::Emulation
            DevilsCrush::GameLogic
            DevilsCrush::Image
            SDL3::SDL3
            SDL3_image::SDL3_image
    )

    target_compile_features(devils_crush PRIVATE cxx_std_20)
    dc_enable_strict_warnings(devils_crush)

    set_target_properties(
        devils_crush
        PROPERTIES
            OUTPUT_NAME "devils-crush"
            VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    install(
        TARGETS devils_crush
        RUNTIME DESTINATION .
        COMPONENT Runtime
    )
    install(
        FILES
            LICENSE
            README.md
            THIRD_PARTY_NOTICES.md
        DESTINATION .
        COMPONENT Runtime
    )
endif()

if(DC_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()
