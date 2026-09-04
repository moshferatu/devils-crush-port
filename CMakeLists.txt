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
    src/audio/music_voice_overlay.cpp
      src/audio/pce_psg.cpp
      src/native_runtime/actor_collision_geometry.cpp
      src/native_runtime/actor_collision_mask_asset.cpp
      src/native_runtime/actor_collision_mask_coordinates.cpp
      src/native_runtime/actor_collision_scan.cpp
    src/native_runtime/actor_collision_seed.cpp
    src/native_runtime/ball_capture.cpp
    src/native_runtime/ball_frame_schedule.cpp
      src/native_runtime/ball_physics.cpp
    src/native_runtime/bonus_stage_launch.cpp
    src/native_runtime/bonus_stage_4.cpp
    src/native_runtime/bonus_stage_4_capture.cpp
    src/native_runtime/bonus_stage_4_linked_target.cpp
    src/native_runtime/bonus_stage_4_linked_target_capture.cpp
    src/native_runtime/bonus_stage_4_linked_target_scheduler.cpp
    src/native_runtime/bonus_stage_4_orbiting_child_capture.cpp
    src/native_runtime/bonus_stage_4_scheduler.cpp
      src/native_runtime/bumper.cpp
    src/native_runtime/bumper_capture.cpp
    src/native_runtime/bumper_scheduler.cpp
    src/native_runtime/collision_detection.cpp
      src/native_runtime/collision_map.cpp
      src/native_runtime/collision_map_asset.cpp
      src/native_runtime/collision_mask.cpp
      src/native_runtime/collision_effect.cpp
      src/native_runtime/collision_effect_capture.cpp
      src/native_runtime/collision_reaction.cpp
      src/native_runtime/collision_retry.cpp
      src/native_runtime/collision_retry_asset.cpp
      src/native_runtime/collision_retry_placement.cpp
      src/native_runtime/collision_retry_probe.cpp
      src/native_runtime/collision_response.cpp
      src/native_runtime/collision_response_asset.cpp
      src/native_runtime/collision_table.cpp
    src/native_runtime/collision_table_asset.cpp
    src/native_runtime/collision_table_driver.cpp
    src/native_runtime/contact_dispatch.cpp
    src/native_runtime/dragon_head.cpp
    src/native_runtime/dragon_head_capture.cpp
    src/native_runtime/dragon_head_scheduler.cpp
    src/native_runtime/dragon_egg.cpp
    src/native_runtime/dragon_egg_capture.cpp
    src/native_runtime/dragon_egg_hatchling.cpp
    src/native_runtime/dragon_egg_scheduler.cpp
    src/native_runtime/front_spider.cpp
    src/native_runtime/front_spider_capture.cpp
    src/native_runtime/front_spider_scheduler.cpp
    src/native_runtime/gameplay_bootstrap.cpp
    src/native_runtime/giant_skeleton.cpp
      src/native_runtime/giant_skeleton_capture.cpp
      src/native_runtime/giant_skeleton_eyeball.cpp
      src/native_runtime/giant_skeleton_eyeball_capture.cpp
      src/native_runtime/giant_skeleton_eyeball_scheduler.cpp
    src/native_runtime/giant_skeleton_scheduler.cpp
    src/native_runtime/giant_face.cpp
    src/native_runtime/giant_face_capture.cpp
    src/native_runtime/giant_face_helmet.cpp
    src/native_runtime/giant_face_helmet_capture.cpp
    src/native_runtime/giant_face_helmet_scheduler.cpp
    src/native_runtime/giant_face_scheduler.cpp
    src/cabinet/cabinet_state.cpp
    src/cabinet/display_config.cpp
    src/cabinet/dmd_font.cpp
    src/cabinet/dmd_layout.cpp
    src/core/atomic_file.cpp
    src/core/backup_ram.cpp
    src/core/high_score_table.cpp
    src/cpu/huc6280_idle_skip.cpp
    src/cpu/huc6280_interrupt_boundary.cpp
    src/cpu/huc6280_irq.cpp
    src/cpu/huc6280_scheduler.cpp
    src/input/pce_pad.cpp
    src/input/plunger.cpp
    src/native_runtime/actor_motion.cpp
    src/native_runtime/actor_motion_capture.cpp
    src/native_runtime/actor_scheduler.cpp
    src/native_runtime/actor_state.cpp
    src/native_runtime/frame_oracle.cpp
      src/native_runtime/gameplay_audio_dispatch.cpp
      src/native_runtime/game_state.cpp
      src/native_runtime/mapped_rom.cpp
      src/native_runtime/native_gameplay_state.cpp
      src/native_runtime/native_gameplay_runtime.cpp
      src/native_runtime/physics_state.cpp
      src/native_runtime/playfield_collision.cpp
      src/native_runtime/score_capture.cpp
      src/native_runtime/score_state.cpp
      src/native_runtime/skeleton_insect.cpp
      src/native_runtime/skeleton_insect_capture.cpp
      src/native_runtime/skeleton_insect_scheduler.cpp
      src/native_runtime/side_spider.cpp
      src/native_runtime/side_spider_capture.cpp
      src/native_runtime/side_spider_scheduler.cpp
      src/native_runtime/small_soldier.cpp
      src/native_runtime/small_soldier_capture.cpp
      src/native_runtime/small_soldier_rules.cpp
      src/native_runtime/small_soldier_scheduler.cpp
      src/native_runtime/tower.cpp
      src/native_runtime/tower_capture.cpp
      src/native_runtime/tower_guard.cpp
      src/native_runtime/tower_guard_capture.cpp
      src/native_runtime/tower_insect.cpp
      src/native_runtime/tower_insect_capture.cpp
      src/native_runtime/tower_insect_scheduler.cpp
      src/native_runtime/tower_scheduler.cpp
      src/core/rom_image.cpp
    src/core/save_state.cpp
    src/render/pce_background.cpp
    src/render/pce_sprite_overlay.cpp
    src/render/sharp_pixel_scaler.cpp
    src/render/table_palette_cache.cpp
    src/render/title_logo.cpp
    src/video/pce_vce.cpp
    src/video/pce_vdc.cpp
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
    src/game_logic/sprite.c
    src/game_logic/video.c
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

add_library(dc_native_registry STATIC src/app/native_registry.cpp)

add_library(DevilsCrush::NativeRegistry ALIAS dc_native_registry)

target_include_directories(
    dc_native_registry
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_compile_features(dc_native_registry PUBLIC cxx_std_20)
dc_enable_strict_warnings(dc_native_registry)

add_library(
    dc_emulation
    STATIC
    "${dc_emulation_source}"
    src/emulation/native_runtime_oracle_adapter.cpp
    src/emulation/playfield_collision_adapter.cpp
    src/emulation/routine_replay_corpus.cpp
)

add_library(DevilsCrush::Emulation ALIAS dc_emulation)

target_include_directories(
    dc_emulation
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_compile_features(dc_emulation PUBLIC cxx_std_20)
dc_enable_strict_warnings(dc_emulation)

# The emulator-to-oracle adapter returns the core-owned FrameSnapshot type.
# Make that relationship transitive so the stub configuration is link-complete
# for consumers that use the adapter directly.
target_link_libraries(dc_emulation PUBLIC DevilsCrush::Core)

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

    add_executable(
        devils_crush
        src/app/main.cpp
        src/app/native_replacements.cpp
        src/app/native_replacements_absorb.cpp
        src/app/native_replacements_actor_scan_preamble.cpp
        src/app/native_replacements_banking.cpp
        src/app/native_replacements_bonus_stage4_orbit.cpp
        src/app/native_replacements_collision.cpp
        src/app/native_replacements_collision_interpreter.cpp
        src/app/native_replacements_collision_tail.cpp
        src/app/native_replacements_controls.cpp
        src/app/native_replacements_geometry.cpp
        src/app/native_replacements_jump_table.cpp
        src/app/native_replacements_main_loop.cpp
        src/app/native_replacements_motion_step.cpp
        src/app/native_replacements_music.cpp
        src/app/native_replacements_palette.cpp
        src/app/native_replacements_palette_cycle.cpp
        src/app/native_replacements_physics.cpp
        src/app/native_replacements_rules.cpp
        src/app/native_replacements_score_panel.cpp
        src/app/native_replacements_scene_layout.cpp
        src/app/native_replacements_sorcerer_tick.cpp
        src/app/native_replacements_sound.cpp
        src/app/native_replacements_substep_loop.cpp
        src/app/native_replacements_tilemap.cpp
        src/app/native_replacements_vblank.cpp
        src/app/native_replacements_video.cpp
        src/app/native_replacements_vram.cpp
    )

    target_link_libraries(
        devils_crush
        PRIVATE
            DevilsCrush::Core
            DevilsCrush::Emulation
            DevilsCrush::GameLogic
            DevilsCrush::NativeRegistry
            SDL3::SDL3
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
