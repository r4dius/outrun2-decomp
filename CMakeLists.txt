cmake_minimum_required(VERSION 3.16)
project(OutRunSwitch_r155 LANGUAGES CXX)

# One portable in-memory decoder for Switch, Mac, PS5, Xbox and host tools.
set(OR2_PECOMPACT_SOURCE src/ports/pecompact_memory.cpp)
# Xbox port (xbox/, Windows only).
option(OR2_XBOX "Build the Xbox port (UWP with CMAKE_SYSTEM_NAME=WindowsStore, else its PC test build)" OFF)
if(OR2_XBOX)
    add_compile_definitions(OR2_XBOX=1)
endif()
if(APPLE)
    if(NOT CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
        message(FATAL_ERROR "The macOS port requires -DCMAKE_OSX_ARCHITECTURES=arm64")
    endif()
    set(OR2_X87_FAST ON CACHE BOOL "Use the fast Single24 x87 model on ARM64" FORCE)
    add_compile_options(-ffp-contract=off -fno-fast-math)
endif()
# x87 model (src/driving/pc_x87.hpp): the Switch build always uses the fast
# double path; ON here builds the host/oracle with it to validate it.
option(OR2_X87_FAST "Use the fast Single24 x87 model on the host" OFF)
if(OR2_X87_FAST)
    add_compile_definitions(OR2_X87_FAST=1)
endif()
# External owned data is never copied into the source tree.
if(APPLE)
    set(OR2_DEFAULT_PC_ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/../Outrun2006 Coast 2 Coast")
    set(OR2_DEFAULT_PC_EXE "${OR2_DEFAULT_PC_ASSETS}/OR2006C2C.EXE")
else()
    set(OR2_DEFAULT_PC_ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/../OutRun2006 Coast 2 Coast (FXT)")
    set(OR2_DEFAULT_PC_EXE "${CMAKE_CURRENT_SOURCE_DIR}/../OR2006C2C.EXE")
endif()
set(OR2_PC_ASSET_ROOT "${OR2_DEFAULT_PC_ASSETS}" CACHE PATH "Owned PC installation")
set(OR2_PC_EXECUTABLE "${OR2_DEFAULT_PC_EXE}" CACHE FILEPATH "Owned PC executable used by reconstruction tests")
set(OR2_GENERATED_ASSET_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/runtime-data/switch/outrun2006" CACHE PATH "External generated test packs")
# Host-only audit/reconstruction tests; NOT a libnx build or a runnable game.
add_library(outrun_control STATIC src/control.cpp)
target_include_directories(outrun_control PUBLIC src)
target_compile_features(outrun_control PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(outrun_control PRIVATE /W4)
else()
    target_compile_options(outrun_control PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_executable(test_control tests/test_control.cpp)
target_link_libraries(test_control PRIVATE outrun_control)
enable_testing()
add_test(NAME reconstructed_control COMMAND test_control)

add_library(outrun_asset_views STATIC src/asset_views.cpp)
target_include_directories(outrun_asset_views PUBLIC src)
target_compile_features(outrun_asset_views PUBLIC cxx_std_17)
add_executable(test_assets tests/test_assets.cpp)
target_link_libraries(test_assets PRIVATE outrun_asset_views)
add_test(NAME asset_views COMMAND test_assets)

if(MSVC)
    target_compile_options(outrun_asset_views PRIVATE /W4)
else()
    target_compile_options(outrun_asset_views PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(outrun_pc_pmt STATIC src/pc_pmt.cpp)
target_include_directories(outrun_pc_pmt PUBLIC src)
target_compile_features(outrun_pc_pmt PUBLIC cxx_std_17)
target_link_libraries(outrun_pc_pmt PUBLIC outrun_asset_views)
if(MSVC)
  target_compile_options(outrun_pc_pmt PRIVATE /W4)
else()
  target_compile_options(outrun_pc_pmt PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()
add_executable(test_pc_pmt tests/test_pc_pmt.cpp)
target_link_libraries(test_pc_pmt PRIVATE outrun_pc_pmt)
add_test(NAME pc_pmt COMMAND test_pc_pmt)

if(MSVC)
  target_compile_options(test_pc_pmt PRIVATE /W4)
else()
  target_compile_options(test_pc_pmt PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

# r004-r009: reconstructed PC driving arithmetic/control with platform boundaries explicit.
find_package(ZLIB REQUIRED)
add_library(outrun_driving STATIC src/system/exe_image.cpp ${OR2_PECOMPACT_SOURCE} src/system/sha256.cpp src/system/files.cpp src/driving/pc_x87.cpp src/driving/pc_driving.cpp src/driving/pc_wheel_dynamics.cpp src/driving/pc_steering.cpp src/driving/pc_tire_geometry.cpp src/driving/pc_transmission.cpp src/driving/pc_road_mu.cpp src/driving/pc_driving_control.cpp src/driving/pc_suspension.cpp src/driving/pc_contact.cpp src/driving/pc_collision.cpp src/driving/pc_course_spline.cpp src/driving/pc_course_topology.cpp src/driving/pc_course_query.cpp src/driving/pc_matrix_stack.cpp src/driving/pc_course_world.cpp src/driving/pc_ground_collision.cpp src/driving/pc_wall_geometry.cpp src/driving/pc_wall_response.cpp src/driving/pc_wall_rebound.cpp src/driving/pc_crash.cpp src/driving/pc_crash_entry.cpp src/driving/pc_wrecker.cpp src/driving/pc_wall_controller.cpp src/driving/pc_body_wall.cpp src/driving/pc_coli_car.cpp src/driving/pc_chassis.cpp src/driving/pc_common_control.cpp src/driving/pc_course_environment.cpp src/driving/pc_car_services.cpp src/driving/pc_car_services_ghost.cpp src/driving/pc_car_services_net.cpp)
target_link_libraries(outrun_driving PUBLIC ZLIB::ZLIB)   # system/exe_image (player EXE image cache)
target_include_directories(outrun_driving PUBLIC src)
target_sources(outrun_driving PRIVATE src/driving/pc_environment_blend.cpp)
target_sources(outrun_driving PRIVATE src/driving/pc_d3dx.cpp)
target_sources(outrun_driving PRIVATE src/driving/pc_camera.cpp)
target_sources(outrun_driving PRIVATE src/driving/pc_race_camera.cpp)
target_compile_features(outrun_driving PUBLIC cxx_std_17)
if(MSVC)
  target_compile_options(outrun_driving PRIVATE /W4 /fp:strict)
else()
  target_compile_options(outrun_driving PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off -fno-fast-math)
endif()
add_executable(test_driving tests/test_driving.cpp)
target_link_libraries(test_driving PRIVATE outrun_driving)
add_test(NAME reconstructed_driving COMMAND test_driving)
add_executable(test_environment_blend tests/test_environment_blend.cpp)
target_link_libraries(test_environment_blend PRIVATE outrun_driving)
add_test(NAME environment_blend COMMAND test_environment_blend)

# r089: platform-neutral runtime entry below the PC WinMain shell.
find_package(ZLIB REQUIRED)
add_library(outrun_native_runtime STATIC src/platform/native_runtime.cpp src/platform/native_car_select.cpp src/platform/frontend_transmission.cpp src/platform/frontend_music.cpp src/platform/frontend_rankings.cpp src/platform/frontend_showroom.cpp src/platform/mission_manager.cpp src/platform/race_hud.cpp src/platform/race_hud_navi.cpp src/platform/race_events.cpp src/platform/race_manager.cpp src/platform/race_sound.cpp src/platform/race_player_car.cpp src/platform/race_input.cpp src/platform/racer_setup.cpp src/platform/scene_owner_49ba80.cpp src/platform/pc_address_view.cpp src/platform/embedded_exe_ranges.cpp src/platform/runtime_platform.cpp src/system/system_clock.cpp src/platform/mesh_preview_pack.cpp src/platform/loader_asset_pack.cpp src/platform/retail_asset_store.cpp src/platform/retail_gpu_cache.cpp src/platform/exe_packs.cpp src/system/exe_image.cpp ${OR2_PECOMPACT_SOURCE} src/system/sha256.cpp src/system/setup_screen.cpp src/platform/frontend_preview_pack.cpp src/platform/game_ui_pack.cpp src/platform/course_asset_pack.cpp src/platform/stage17_asset_pack.cpp src/platform/race_asset_pack.cpp src/platform/world_source_pack.cpp src/platform/course_environment_runtime.cpp src/platform/course_collision_pack.cpp src/platform/pc_coli0200.cpp src/platform/course_world_runtime.cpp src/platform/driving_data_pack.cpp src/platform/vehicle_visual.cpp src/platform/vehicle_road_contact.cpp src/platform/vehicle_pose_filter.cpp src/platform/pc_vehicle_control.cpp src/platform/title_owner.cpp)
target_include_directories(outrun_native_runtime PUBLIC src)
target_compile_features(outrun_native_runtime PUBLIC cxx_std_17)
target_link_libraries(outrun_native_runtime PUBLIC outrun_driving outrun_pc_pmt ZLIB::ZLIB)
# race robots ROB01/ROB03 (race_robots*.cpp; PcRaceMemory from race_area.cpp)
target_sources(outrun_native_runtime PRIVATE src/platform/race_robots.cpp src/platform/race_robots_data.cpp src/platform/race_robots_runtime.cpp src/platform/race_translated.cpp src/platform/race_modes.cpp src/platform/race_course_passes.cpp src/platform/race_variant_owners.cpp src/platform/pc_input_devices.cpp src/platform/pc_network.cpp src/platform/pc_network_tr.cpp src/platform/race_network_cars_tr.cpp src/platform/bulk_tr.cpp src/platform/bulk_fallback.cpp src/system/network_bsd.cpp src/platform/translated_crt.cpp src/platform/race_sound_runtime.cpp src/platform/rob_motion_tables.cpp src/platform/rob_motion.cpp src/platform/object_db.cpp src/platform/rob_disp.cpp src/platform/rob_disp_data.cpp src/platform/rob_osage.cpp src/platform/rob_motion_engine.cpp src/platform/rob_flag.cpp src/platform/pc_shadow_volume.cpp)
if(MSVC)
  target_compile_options(outrun_native_runtime PRIVATE /W4)
else()
  target_compile_options(outrun_native_runtime PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_executable(test_native_runtime tests/test_native_runtime.cpp)
target_link_libraries(test_native_runtime PRIVATE outrun_native_runtime)
add_test(NAME native_platform_runtime_entry COMMAND test_native_runtime)
# ---- race AREA/SKY runtime (events 390/391): begin ----
target_sources(outrun_native_runtime PRIVATE src/platform/race_end_modes.cpp src/platform/arcade_attract.cpp src/platform/sp_rankings.cpp src/platform/race_traffic.cpp src/platform/race_traffic_runtime.cpp src/platform/race_goal_camera.cpp src/platform/race_end_runtime.cpp src/platform/race_ghosts.cpp src/platform/race_records.cpp src/platform/race_name_entry.cpp src/platform/race_autoscene.cpp src/platform/race_ending.cpp
    src/platform/race_ending_services.cpp src/platform/race_ghost_car.cpp src/platform/race_ghosts_runtime.cpp src/platform/race_area.cpp src/platform/race_sky.cpp src/platform/race_area_data.cpp src/platform/race_area_runtime.cpp src/platform/race_area_services.cpp src/platform/pc_sprite_2d.cpp src/platform/sprite_2d_runtime.cpp src/platform/race_hud_runtime.cpp)
# ---- race AREA/SKY runtime: end ----
# scn-efc: events 387 SCN_EFC / 397 PART_EFC and their runtime binding.
target_sources(outrun_native_runtime PRIVATE src/platform/native_race_effects.cpp src/platform/race_scene_effects.cpp src/platform/race_particles.cpp src/platform/race_particles_data.cpp src/platform/race_particles_init.cpp src/platform/race_particles_cars.cpp src/platform/race_particles_effects.cpp src/platform/race_particles_draw.cpp)
target_sources(outrun_native_runtime PRIVATE src/platform/race_manager_runtime.cpp) # race manager (event 359) binding
target_sources(outrun_native_runtime PRIVATE src/platform/frontend_welcome.cpp src/platform/frontend_stack.cpp src/platform/frontend_profiles.cpp src/system/title_movie.cpp)
target_sources(outrun_native_runtime PRIVATE src/platform/frontend_title_widgets.cpp)
target_sources(outrun_native_runtime PRIVATE src/enhancements/video.cpp src/enhancements/settings_rows.cpp src/enhancements/gpu_backend.cpp src/enhancements/frame_rate.cpp) # port: Options > Settings enhancement rows
target_sources(outrun_native_runtime PRIVATE src/platform/frontend_license_owners.cpp)
target_sources(outrun_native_runtime PRIVATE src/platform/menu_audio.cpp)
target_sources(outrun_native_runtime PRIVATE src/platform/music_stream.cpp src/platform/pc_sound.cpp)
# Streamed BGM: libvorbisfile. Headers from the distribution (libvorbis-dev)
# or, without it, the portable C headers of the devkitPro portlibs; the host
# links the distribution's runtime libraries.
find_path(OR2_VORBIS_HEADERS vorbis/vorbisfile.h PATHS
  $ENV{DEVKITPRO}/portlibs/switch/include /opt/devkitpro/portlibs/switch/include /mnt/c/devkitPro/portlibs/switch/include)
if(EXISTS ${OR2_VORBIS_HEADERS}/vorbis/vorbisfile.h AND EXISTS /usr/lib/x86_64-linux-gnu/libvorbisfile.so.3)
  file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/vorbis_include)
  file(COPY ${OR2_VORBIS_HEADERS}/vorbis ${OR2_VORBIS_HEADERS}/ogg DESTINATION ${CMAKE_BINARY_DIR}/vorbis_include)
  target_include_directories(outrun_native_runtime PRIVATE ${CMAKE_BINARY_DIR}/vorbis_include)
  target_compile_definitions(outrun_native_runtime PRIVATE OR2_HAVE_VORBIS=1)
  target_link_libraries(outrun_native_runtime PUBLIC /usr/lib/x86_64-linux-gnu/libvorbisfile.so.3 /usr/lib/x86_64-linux-gnu/libvorbis.so.0 /usr/lib/x86_64-linux-gnu/libogg.so.0)
  set(OR2_HOST_VORBIS ON)
endif()
target_sources(outrun_native_runtime PRIVATE src/platform/frontend_fixed_choice.cpp src/platform/frontend_network_menus.cpp)
target_sources(outrun_native_runtime PRIVATE src/platform/frontend_record_manager.cpp)
target_sources(outrun_native_runtime PRIVATE src/platform/frontend_categories.cpp)
target_sources(outrun_native_runtime PRIVATE src/platform/frontend_missions.cpp src/platform/frontend_requests.cpp)
target_sources(outrun_native_runtime PRIVATE src/platform/frontend_vehicle_loader.cpp)
target_sources(outrun_native_runtime PRIVATE src/platform/frontend_carousel.cpp)
target_sources(outrun_native_runtime PRIVATE src/platform/frontend_vehicle_menu.cpp)
target_sources(outrun_native_runtime PRIVATE src/platform/frontend_vehicle_preview.cpp)
target_sources(outrun_native_runtime PRIVATE src/platform/vehicle_creation_queue.cpp)
target_sources(outrun_native_runtime PRIVATE src/platform/vehicle_constructor.cpp)
target_sources(outrun_native_runtime PRIVATE src/platform/vehicle_body_init.cpp)
target_sources(outrun_native_runtime PRIVATE src/platform/vehicle_preview_event.cpp)
target_sources(outrun_native_runtime PRIVATE src/platform/vehicle_preview_init.cpp)
target_sources(outrun_native_runtime PRIVATE src/platform/embedded_camera_data.cpp)
target_sources(outrun_native_runtime PRIVATE src/platform/camera_render_binding.cpp)
target_sources(outrun_native_runtime PRIVATE src/platform/vehicle_model_draw.cpp src/platform/embedded_vehicle_draw_data.cpp src/platform/pc_render_queue.cpp src/platform/pc_vertex_shader_setup.cpp src/platform/pc_pmt_loader.cpp src/platform/embedded_shader_data.cpp src/platform/pc_render_flush.cpp src/platform/pc_scene_display.cpp src/platform/pc_vehicle_display.cpp src/platform/pc_frame_render.cpp src/platform/pc_car_reflection.cpp src/platform/pc_scene_environment.cpp src/platform/pc_scene_renderer.cpp src/platform/pc_scene_select_car.cpp src/platform/pc_headlight_pool.cpp src/platform/pc_shader_programs.cpp src/platform/pc_shader_programs_generated.cpp src/platform/pc_dds.cpp src/platform/pc_soft_d3d9.cpp)
add_executable(test_vehicle_constructor tests/test_vehicle_constructor.cpp)
target_link_libraries(test_vehicle_constructor PRIVATE outrun_native_runtime)
add_test(NAME vehicle_constructor COMMAND test_vehicle_constructor)
add_executable(test_vehicle_body_init tests/test_vehicle_body_init.cpp)
target_link_libraries(test_vehicle_body_init PRIVATE outrun_native_runtime)
add_test(NAME vehicle_body_init COMMAND test_vehicle_body_init)
add_executable(test_vehicle_preview_init tests/test_vehicle_preview_init.cpp)
target_link_libraries(test_vehicle_preview_init PRIVATE outrun_native_runtime)
add_test(NAME vehicle_preview_init COMMAND test_vehicle_preview_init)
add_executable(test_camera_render_binding tests/test_camera_render_binding.cpp)
target_link_libraries(test_camera_render_binding PRIVATE outrun_native_runtime)
add_test(NAME camera_render_binding COMMAND test_camera_render_binding)
# Replays original race-manager outputs dumped by compare_pc race_manager_probe
# (OR2_RACE_MANAGER_REPLAY=<file>); run manually / under qemu on other hosts.
add_executable(test_race_manager_replay tests/test_race_manager_replay.cpp src/platform/race_manager.cpp)
target_link_libraries(test_race_manager_replay PRIVATE outrun_driving)
add_executable(test_pc_pmt_loader tests/test_pc_pmt_loader.cpp)
target_link_libraries(test_pc_pmt_loader PRIVATE outrun_native_runtime)
add_test(NAME pc_pmt_loader COMMAND test_pc_pmt_loader)
add_executable(test_pc_shader_programs tests/test_pc_shader_programs.cpp)
target_link_libraries(test_pc_shader_programs PRIVATE outrun_native_runtime)
add_test(NAME pc_shader_programs COMMAND test_pc_shader_programs)
find_package(ZLIB)
if(ZLIB_FOUND)
  add_executable(pc_render_car tools/host/pc_render_car.cpp)
  target_link_libraries(pc_render_car PRIVATE outrun_native_runtime ZLIB::ZLIB)
  add_executable(pc_shader_census tools/host/pc_shader_census.cpp)
  target_link_libraries(pc_shader_census PRIVATE outrun_native_runtime ZLIB::ZLIB)
endif()
add_executable(test_frontend_vehicle_menu tests/test_frontend_vehicle_menu.cpp)
target_link_libraries(test_frontend_vehicle_menu PRIVATE outrun_native_runtime)
add_test(NAME frontend_vehicle_menu COMMAND test_frontend_vehicle_menu)
add_executable(test_frontend_vehicle_preview tests/test_frontend_vehicle_preview.cpp)
target_link_libraries(test_frontend_vehicle_preview PRIVATE outrun_native_runtime)
add_test(NAME frontend_vehicle_preview COMMAND test_frontend_vehicle_preview)
add_executable(test_frontend_vehicle_loader tests/test_frontend_vehicle_loader.cpp)
target_link_libraries(test_frontend_vehicle_loader PRIVATE outrun_native_runtime)
add_test(NAME frontend_vehicle_loader COMMAND test_frontend_vehicle_loader
  "${OR2_PC_EXECUTABLE}"
  "${OR2_PC_ASSET_ROOT}")
add_executable(test_menu_audio tests/test_menu_audio.cpp)
target_link_libraries(test_menu_audio PRIVATE outrun_native_runtime)
add_test(NAME menu_audio COMMAND test_menu_audio
  "${OR2_PC_ASSET_ROOT}/Sound/MENU.pak"
  "${OR2_PC_EXECUTABLE}")

add_executable(test_frontend_profiles tests/test_frontend_profiles.cpp)
target_sources(outrun_native_runtime PRIVATE src/platform/frontend_license_editor.cpp src/platform/frontend_license_chooser.cpp)
target_sources(outrun_native_runtime PRIVATE src/platform/frontend_sprites.cpp src/platform/frontend_ui_resources.cpp)
target_sources(outrun_native_runtime PRIVATE src/platform/frontend_text.cpp src/platform/frontend_license_widgets.cpp src/platform/frontend_keyboard.cpp src/platform/frontend_glyph_render.cpp src/platform/frontend_list.cpp src/platform/frontend_window.cpp src/platform/frontend_images.cpp)
add_executable(test_frontend_text tests/test_frontend_text.cpp)
target_link_libraries(test_frontend_text PRIVATE outrun_native_runtime)
add_test(NAME frontend_text COMMAND test_frontend_text)
add_test(NAME frontend_text_retail COMMAND test_frontend_text
  ${OR2_GENERATED_ASSET_ROOT}/or2_fonts.bin
  "${OR2_PC_ASSET_ROOT}/Text/English_US.bin")
add_executable(test_frontend_license_widgets tests/test_frontend_license_widgets.cpp)
add_executable(test_frontend_list tests/test_frontend_list.cpp)
target_link_libraries(test_frontend_list PRIVATE outrun_native_runtime)
add_test(NAME frontend_original_list COMMAND test_frontend_list
  ${OR2_GENERATED_ASSET_ROOT}/or2_fonts.bin)
target_link_libraries(test_frontend_license_widgets PRIVATE outrun_native_runtime)
add_test(NAME frontend_license_live_widgets COMMAND test_frontend_license_widgets
  ${OR2_GENERATED_ASSET_ROOT}/or2_fonts.bin
  "${OR2_PC_ASSET_ROOT}/Text/English_US.bin"
  ${OR2_GENERATED_ASSET_ROOT}/or2_frontend_preview.bin
  ${OR2_GENERATED_ASSET_ROOT}/or2_shared_ui.bin)
add_executable(test_frontend_keyboard tests/test_frontend_keyboard.cpp)
target_link_libraries(test_frontend_keyboard PRIVATE outrun_native_runtime)
add_test(NAME frontend_keyboard COMMAND test_frontend_keyboard
  ${OR2_GENERATED_ASSET_ROOT}/or2_shared_ui.bin)
add_executable(test_frontend_sprites tests/test_frontend_sprites.cpp)
target_link_libraries(test_frontend_sprites PRIVATE outrun_native_runtime)
add_test(NAME frontend_sprite_lifecycle COMMAND test_frontend_sprites)
add_test(NAME frontend_sprite_retail COMMAND test_frontend_sprites
  ${OR2_GENERATED_ASSET_ROOT}/or2_frontend_preview.bin)
add_executable(test_frontend_license_editor tests/test_frontend_license_editor.cpp)
target_link_libraries(test_frontend_license_editor PRIVATE outrun_native_runtime)
add_test(NAME frontend_license_editor COMMAND test_frontend_license_editor)
target_link_libraries(test_frontend_profiles PRIVATE outrun_native_runtime)
add_test(NAME frontend_profiles COMMAND test_frontend_profiles)

if(NOT MSVC)
  add_executable(test_switch_audio tests/test_switch_audio.cpp switch/source/switch_audio.cpp)
  target_link_libraries(test_switch_audio PRIVATE outrun_native_runtime)
  target_compile_definitions(test_switch_audio PRIVATE OR2_AUDIO_DEVICE_TEST)
  target_include_directories(test_switch_audio PRIVATE tests/support/audio_stub switch/source)
  target_compile_features(test_switch_audio PRIVATE cxx_std_17)
  add_test(NAME switch_audio_queue COMMAND test_switch_audio
    "${OR2_PC_ASSET_ROOT}/Sound/MENU.pak")
endif()

option(OR2_TEST_RETAIL_MOVIE "Validate the title decoder against the owned PC Bink" OFF)
set(OR2_RETAIL_TITLE "" CACHE FILEPATH "Owned MV/TitleScreen.bik for decoder tests")
if(OR2_TEST_RETAIL_MOVIE)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libswscale libswresample libavutil)
  add_executable(test_title_movie tests/test_title_movie.cpp src/system/title_movie.cpp)
  target_compile_definitions(test_title_movie PRIVATE OUTRUN_WITH_FFMPEG)
  target_include_directories(test_title_movie PRIVATE src)
  target_compile_features(test_title_movie PRIVATE cxx_std_17)
  target_link_libraries(test_title_movie PRIVATE PkgConfig::FFMPEG)
  add_test(NAME retail_title_movie COMMAND test_title_movie "${OR2_RETAIL_TITLE}")
endif()
add_executable(test_frontend_stack tests/test_frontend_stack.cpp)
target_link_libraries(test_frontend_stack PRIVATE outrun_native_runtime)
add_test(NAME frontend_stack_lifecycle COMMAND test_frontend_stack)
add_executable(test_title_owner tests/test_title_owner.cpp)
target_link_libraries(test_title_owner PRIVATE outrun_native_runtime)
add_test(NAME original_title_owner_boundaries COMMAND test_title_owner)
add_executable(test_enhancements tests/test_enhancements.cpp)
target_link_libraries(test_enhancements PRIVATE outrun_native_runtime)
add_test(NAME enhancements_menu COMMAND test_enhancements)
add_executable(test_mesh_preview_pack tests/test_mesh_preview_pack.cpp)
target_link_libraries(test_mesh_preview_pack PRIVATE outrun_native_runtime)
add_test(NAME switch_mesh_preview_pack COMMAND test_mesh_preview_pack)
add_executable(test_loader_asset_pack tests/test_loader_asset_pack.cpp)
target_link_libraries(test_loader_asset_pack PRIVATE outrun_native_runtime)
add_test(NAME switch_loader_asset_pack COMMAND test_loader_asset_pack)
add_executable(test_frontend_preview_pack tests/test_frontend_preview_pack.cpp)
target_link_libraries(test_frontend_preview_pack PRIVATE outrun_native_runtime)
add_test(NAME switch_frontend_preview_pack COMMAND test_frontend_preview_pack)
add_test(NAME switch_start_loading_pack COMMAND test_frontend_preview_pack
  ${OR2_GENERATED_ASSET_ROOT}/or2_frontend_preview.bin
  ${OR2_GENERATED_ASSET_ROOT}/or2_start_loading_preview.bin
  ${OR2_GENERATED_ASSET_ROOT}/or2_game_ui.bin)
add_executable(test_game_ui_pack tests/test_game_ui_pack.cpp)
target_link_libraries(test_game_ui_pack PRIVATE outrun_native_runtime)
add_test(NAME switch_game_ui_pack COMMAND test_game_ui_pack)
add_executable(test_vehicle_visual tests/test_vehicle_visual.cpp)
target_link_libraries(test_vehicle_visual PRIVATE outrun_native_runtime)
add_test(NAME switch_vehicle_visual_transform COMMAND test_vehicle_visual)
add_executable(test_pc_vehicle_control tests/test_pc_vehicle_control.cpp)
target_link_libraries(test_pc_vehicle_control PRIVATE outrun_native_runtime)
add_test(NAME switch_pc_vehicle_control_bridge COMMAND test_pc_vehicle_control)
add_executable(test_driving_data_pack tests/test_driving_data_pack.cpp)
target_link_libraries(test_driving_data_pack PRIVATE outrun_native_runtime)
add_test(NAME switch_driving_data_pack COMMAND test_driving_data_pack)
add_executable(test_course_asset_pack tests/test_course_asset_pack.cpp)
target_link_libraries(test_course_asset_pack PRIVATE outrun_native_runtime)
add_test(NAME switch_course_asset_pack COMMAND test_course_asset_pack)
add_executable(test_race_asset_pack tests/test_race_asset_pack.cpp)
target_link_libraries(test_race_asset_pack PRIVATE outrun_native_runtime)
add_test(NAME switch_race_asset_pack COMMAND test_race_asset_pack)
add_executable(test_frontend_progress tests/test_frontend_progress.cpp)
target_link_libraries(test_frontend_progress PRIVATE outrun_native_runtime)
add_test(NAME frontend_progress COMMAND test_frontend_progress)
add_test(NAME owned_frontend_progress COMMAND test_frontend_progress
  "${OR2_PC_ASSET_ROOT}")
add_executable(test_world_source_pack tests/test_world_source_pack.cpp)
target_link_libraries(test_world_source_pack PRIVATE outrun_native_runtime)
add_test(NAME switch_world_source_pack COMMAND test_world_source_pack)
add_executable(test_course_collision_pack tests/test_course_collision_pack.cpp)
target_link_libraries(test_course_collision_pack PRIVATE outrun_native_runtime)
add_test(NAME switch_course_collision_pack COMMAND test_course_collision_pack)
add_executable(test_vehicle_road_contact tests/test_vehicle_road_contact.cpp)
target_link_libraries(test_vehicle_road_contact PRIVATE outrun_native_runtime)
add_test(NAME switch_vehicle_road_contact COMMAND test_vehicle_road_contact)
add_executable(test_vehicle_pose_filter tests/test_vehicle_pose_filter.cpp)
target_link_libraries(test_vehicle_pose_filter PRIVATE outrun_native_runtime)
add_test(NAME switch_vehicle_pose_filter COMMAND test_vehicle_pose_filter)

# r096: compile and execute the __SWITCH__ branch against a tiny libnx API stub.
# This is a syntax/API-shape gate only; a real NRO still requires devkitA64/libnx.
add_executable(test_switch_platform_stub tests/test_switch_platform_stub.cpp src/platform/runtime_platform.cpp switch/source/switch_clock.cpp)
target_compile_definitions(test_switch_platform_stub PRIVATE __SWITCH__)
target_include_directories(test_switch_platform_stub PRIVATE src switch/source tests/support/switch_stub)
target_compile_features(test_switch_platform_stub PRIVATE cxx_std_17)
if(MSVC)
  target_compile_options(test_switch_platform_stub PRIVATE /W4)
else()
  target_compile_options(test_switch_platform_stub PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME switch_platform_compile_contract COMMAND test_switch_platform_stub)

# r097-r098: compile the real libnx entry source under the same API-shaped stub.
# The first target catches entry-source API drift.  The second target links the
# exact native runtime implementation used by the NRO against the stub so that
# missing symbols/source-list mistakes are caught before a real devkitA64 build.
add_library(test_switch_entry_compile OBJECT switch/source/main.cpp)
target_compile_definitions(test_switch_entry_compile PRIVATE __SWITCH__)
target_include_directories(test_switch_entry_compile PRIVATE src tests/support/switch_stub)
target_compile_features(test_switch_entry_compile PRIVATE cxx_std_17)
if(MSVC)
  target_compile_options(test_switch_entry_compile PRIVATE /W4)
else()
  target_compile_options(test_switch_entry_compile PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(test_switch_entry_link
  src/platform/frontend_categories.cpp
  src/platform/frontend_missions.cpp
  src/platform/frontend_requests.cpp
  src/platform/frontend_vehicle_loader.cpp
  src/platform/frontend_carousel.cpp
  src/platform/frontend_vehicle_menu.cpp
  src/platform/frontend_vehicle_preview.cpp
  src/platform/vehicle_creation_queue.cpp
  src/platform/vehicle_constructor.cpp
  src/platform/vehicle_body_init.cpp
  src/platform/vehicle_preview_event.cpp
  src/platform/vehicle_preview_init.cpp
  src/platform/embedded_camera_data.cpp
  src/platform/camera_render_binding.cpp
  src/platform/vehicle_model_draw.cpp
  src/platform/embedded_vehicle_draw_data.cpp
  src/platform/pc_render_queue.cpp
  src/platform/pc_vertex_shader_setup.cpp
  src/platform/pc_pmt_loader.cpp
  src/platform/embedded_shader_data.cpp
  src/platform/pc_render_flush.cpp
  src/platform/pc_scene_display.cpp
  src/platform/pc_vehicle_display.cpp
  src/platform/pc_frame_render.cpp src/platform/pc_car_reflection.cpp
  src/platform/pc_scene_environment.cpp
  src/platform/pc_scene_renderer.cpp src/platform/pc_scene_select_car.cpp
  src/platform/pc_headlight_pool.cpp
  src/platform/pc_shadow_volume.cpp
  src/platform/pc_shader_programs.cpp
  src/platform/pc_shader_programs_generated.cpp
  src/platform/pc_dds.cpp
  src/platform/pc_soft_d3d9.cpp
  src/platform/frontend_fixed_choice.cpp
  src/platform/frontend_network_menus.cpp
  src/platform/frontend_record_manager.cpp
  src/platform/menu_audio.cpp
  src/platform/music_stream.cpp
  src/platform/pc_sound.cpp
  src/platform/frontend_text.cpp src/platform/frontend_glyph_render.cpp src/platform/frontend_images.cpp
  switch/source/main.cpp switch/source/switch_clock.cpp switch/source/switch_network.cpp
  switch/source/switch_audio.cpp
  switch/source/switch_renderer.cpp
  src/platform/native_runtime.cpp
  src/platform/native_car_select.cpp
  src/platform/frontend_transmission.cpp
  src/platform/frontend_music.cpp
  src/platform/frontend_rankings.cpp src/platform/frontend_showroom.cpp
  src/platform/mission_manager.cpp src/platform/race_hud.cpp src/platform/race_hud_navi.cpp src/platform/race_events.cpp src/platform/race_manager.cpp src/platform/race_sound.cpp src/platform/race_player_car.cpp src/platform/race_input.cpp
  src/platform/racer_setup.cpp
  src/platform/scene_owner_49ba80.cpp
  src/platform/pc_address_view.cpp
  src/platform/embedded_exe_ranges.cpp
  src/platform/runtime_platform.cpp src/system/system_clock.cpp
  src/platform/mesh_preview_pack.cpp
  src/platform/loader_asset_pack.cpp
  src/platform/retail_asset_store.cpp src/platform/retail_gpu_cache.cpp src/system/files.cpp
  src/platform/exe_packs.cpp src/system/exe_image.cpp ${OR2_PECOMPACT_SOURCE} src/system/sha256.cpp src/system/setup_screen.cpp src/system/files.cpp
  src/platform/frontend_preview_pack.cpp
  src/platform/game_ui_pack.cpp
  src/platform/course_asset_pack.cpp
  src/platform/stage17_asset_pack.cpp
  src/platform/race_asset_pack.cpp
  src/platform/world_source_pack.cpp src/platform/course_environment_runtime.cpp
  src/platform/course_collision_pack.cpp src/platform/pc_coli0200.cpp src/platform/course_world_runtime.cpp
  src/platform/driving_data_pack.cpp
  src/platform/vehicle_visual.cpp
  src/platform/vehicle_road_contact.cpp
  src/platform/vehicle_pose_filter.cpp
  src/platform/pc_vehicle_control.cpp
  src/platform/title_owner.cpp src/platform/frontend_welcome.cpp src/platform/frontend_stack.cpp src/platform/frontend_profiles.cpp src/system/title_movie.cpp src/platform/frontend_sprites.cpp src/platform/frontend_ui_resources.cpp)
target_compile_definitions(test_switch_entry_link PRIVATE __SWITCH__ OUTRUN_SWITCH_STUB)
target_sources(test_switch_entry_link PRIVATE src/platform/race_end_modes.cpp src/platform/arcade_attract.cpp src/platform/sp_rankings.cpp src/platform/race_traffic.cpp src/platform/race_traffic_runtime.cpp src/platform/race_goal_camera.cpp src/platform/race_end_runtime.cpp src/platform/race_ghosts.cpp src/platform/race_records.cpp src/platform/race_name_entry.cpp src/platform/race_autoscene.cpp src/platform/race_ending.cpp
    src/platform/race_ending_services.cpp src/platform/race_ghost_car.cpp src/platform/race_ghosts_runtime.cpp src/platform/race_area.cpp src/platform/race_sky.cpp src/platform/race_area_data.cpp src/platform/race_area_runtime.cpp src/platform/race_area_services.cpp src/platform/pc_sprite_2d.cpp src/platform/sprite_2d_runtime.cpp src/platform/race_hud_runtime.cpp) # race AREA/SKY runtime
target_sources(test_switch_entry_link PRIVATE src/platform/native_race_effects.cpp src/platform/race_scene_effects.cpp src/platform/race_particles.cpp src/platform/race_particles_data.cpp src/platform/race_particles_init.cpp src/platform/race_particles_cars.cpp src/platform/race_particles_effects.cpp src/platform/race_particles_draw.cpp) # scn-efc
target_sources(test_switch_entry_link PRIVATE src/platform/race_manager_runtime.cpp) # race manager (event 359) binding
target_sources(test_switch_entry_link PRIVATE src/platform/frontend_title_widgets.cpp src/platform/frontend_list.cpp src/platform/frontend_window.cpp)
target_sources(test_switch_entry_link PRIVATE src/enhancements/video.cpp src/enhancements/settings_rows.cpp src/enhancements/gpu_backend.cpp src/enhancements/frame_rate.cpp switch/source/enhancements_switch.cpp)
# race robots ROB01/ROB03
target_sources(test_switch_entry_link PRIVATE src/platform/race_robots.cpp src/platform/race_robots_data.cpp src/platform/race_robots_runtime.cpp src/platform/race_translated.cpp src/platform/race_modes.cpp src/platform/race_course_passes.cpp src/platform/race_variant_owners.cpp src/platform/pc_input_devices.cpp src/platform/pc_network.cpp src/runtime/game_host.cpp src/platform/pc_network_tr.cpp src/platform/race_network_cars_tr.cpp src/platform/bulk_tr.cpp src/platform/bulk_fallback.cpp src/system/network_bsd.cpp src/platform/translated_crt.cpp src/platform/race_sound_runtime.cpp src/platform/rob_motion_tables.cpp src/platform/rob_motion.cpp src/platform/object_db.cpp src/platform/rob_disp.cpp src/platform/rob_disp_data.cpp src/platform/rob_osage.cpp src/platform/rob_motion_engine.cpp src/platform/rob_flag.cpp src/platform/pc_shadow_volume.cpp)
target_sources(test_switch_entry_link PRIVATE src/platform/frontend_license_owners.cpp
  src/platform/frontend_license_widgets.cpp src/platform/frontend_license_chooser.cpp
  src/platform/frontend_license_editor.cpp src/platform/frontend_keyboard.cpp src/platform/frontend_text.cpp)
target_include_directories(test_switch_entry_link PRIVATE src tests/support/switch_stub)
target_compile_features(test_switch_entry_link PRIVATE cxx_std_17)
target_link_libraries(test_switch_entry_link PRIVATE outrun_driving outrun_pc_pmt ZLIB::ZLIB)
if(MSVC)
  target_compile_options(test_switch_entry_link PRIVATE /W4 /fp:strict)
else()
  target_compile_options(test_switch_entry_link PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off -fno-fast-math -fno-rtti)
endif()
# Host run of the real switch/source/main.cpp with a scripted controller
# (tests/support/switch_host/switch.h). Not a ctest: it needs the retail tree.
get_target_property(OR2_ENTRY_SOURCES test_switch_entry_link SOURCES)
add_executable(host_nro ${OR2_ENTRY_SOURCES})
target_compile_definitions(host_nro PRIVATE __SWITCH__ OUTRUN_SWITCH_STUB OR2_HOST_NRO)
target_include_directories(host_nro PRIVATE src tests/support/switch_host)
target_compile_features(host_nro PRIVATE cxx_std_17)
target_link_libraries(host_nro PRIVATE outrun_driving outrun_pc_pmt ZLIB::ZLIB)
if(OR2_HOST_VORBIS)
  target_include_directories(host_nro PRIVATE ${CMAKE_BINARY_DIR}/vorbis_include)
  target_compile_definitions(host_nro PRIVATE OR2_HAVE_VORBIS=1)
  target_link_libraries(host_nro PRIVATE /usr/lib/x86_64-linux-gnu/libvorbisfile.so.3 /usr/lib/x86_64-linux-gnu/libvorbis.so.0 /usr/lib/x86_64-linux-gnu/libogg.so.0)
endif()
add_test(NAME host_nro_menu_to_game COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/host_nro_menu_to_game.sh
  $<TARGET_FILE:host_nro> "${OR2_PC_ASSET_ROOT}")
set_tests_properties(host_nro_menu_to_game PROPERTIES LABELS "integration;retail" TIMEOUT 1800)
# Without OR2_EXE: first-launch preparation and cache only, against the OR2_EXE run (needs OR2_EXE).
add_test(NAME host_nro_cache_only COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/host_nro_cache_only.sh
  $<TARGET_FILE:host_nro> "${OR2_PC_ASSET_ROOT}")
set_tests_properties(host_nro_cache_only PROPERTIES LABELS "integration;retail" TIMEOUT 1800)
add_executable(test_switch_renderer_r122
  tests/test_switch_renderer_r122.cpp
  switch/source/switch_renderer.cpp)
target_compile_definitions(test_switch_renderer_r122 PRIVATE OUTRUN_SWITCH_STUB)
target_include_directories(test_switch_renderer_r122 PRIVATE src switch/source)
target_compile_features(test_switch_renderer_r122 PRIVATE cxx_std_17)
target_link_libraries(test_switch_renderer_r122 PRIVATE outrun_native_runtime)
add_test(NAME switch_renderer_start_loading COMMAND test_switch_renderer_r122
  ${OR2_GENERATED_ASSET_ROOT}/or2_frontend_preview.bin
  ${OR2_GENERATED_ASSET_ROOT}/or2_start_loading_preview.bin)
if(MSVC)
  target_compile_options(test_switch_renderer_r122 PRIVATE /W4 /fp:strict)
else()
  target_compile_options(test_switch_renderer_r122 PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off -fno-fast-math -fno-rtti)
endif()

# OFF by default: runs whitelist-selected original x86 arithmetic, not the game.
# Invoke via tools/run_pc_oracle.py, which enforces the EXE SHA256 beforehand.
option(OR2_BUILD_PC_ORACLE "Build Linux x86-64 original-code differential runner" OFF)
if(OR2_BUILD_PC_ORACLE)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    message(FATAL_ERROR "PC oracle requires Linux x86-64 compatibility-mode support")
  endif()
  enable_language(ASM)
  add_executable(compare_pc tools/oracle/compare_pc.cpp tools/oracle/compat32.S src/platform/title_owner.cpp src/platform/frontend_welcome.cpp src/platform/frontend_stack.cpp src/platform/frontend_profiles.cpp src/platform/frontend_license_editor.cpp)
  target_link_libraries(compare_pc PRIVATE outrun_driving)
  target_sources(compare_pc PRIVATE src/platform/frontend_sprites.cpp src/platform/frontend_ui_resources.cpp src/platform/game_ui_pack.cpp)
  target_sources(compare_pc PRIVATE src/platform/frontend_fixed_choice.cpp src/platform/frontend_network_menus.cpp)
  target_sources(compare_pc PRIVATE src/platform/frontend_record_manager.cpp)
  target_sources(compare_pc PRIVATE src/platform/frontend_categories.cpp)
  target_sources(compare_pc PRIVATE src/platform/frontend_missions.cpp src/platform/frontend_requests.cpp)
  target_sources(compare_pc PRIVATE src/platform/frontend_vehicle_loader.cpp)
  target_sources(compare_pc PRIVATE src/platform/frontend_carousel.cpp)
  target_sources(compare_pc PRIVATE src/platform/race_end_modes.cpp src/platform/arcade_attract.cpp src/platform/sp_rankings.cpp src/platform/race_traffic.cpp src/platform/race_goal_camera.cpp src/platform/race_ghosts.cpp src/platform/race_records.cpp src/platform/race_name_entry.cpp src/platform/race_autoscene.cpp src/platform/race_ending.cpp
    src/platform/race_ending_services.cpp src/platform/race_ghost_car.cpp src/platform/race_area.cpp src/platform/race_sky.cpp src/platform/race_area_data.cpp src/platform/race_area_services.cpp src/platform/pc_sprite_2d.cpp)
  target_sources(compare_pc PRIVATE src/platform/race_scene_effects.cpp src/platform/race_particles.cpp src/platform/race_particles_data.cpp src/platform/race_particles_init.cpp src/platform/race_particles_cars.cpp src/platform/race_particles_effects.cpp src/platform/race_particles_draw.cpp)   # scn-efc
  target_sources(compare_pc PRIVATE src/platform/race_robots.cpp src/platform/race_robots_data.cpp src/platform/race_translated.cpp src/platform/race_modes.cpp src/platform/rob_motion_tables.cpp src/platform/rob_motion.cpp src/platform/object_db.cpp src/platform/rob_disp.cpp src/platform/rob_disp_data.cpp src/platform/rob_osage.cpp src/platform/rob_motion_engine.cpp src/platform/rob_flag.cpp src/platform/pc_shadow_volume.cpp)  # race robots (ROB01/ROB03)
  target_sources(compare_pc PRIVATE src/platform/frontend_vehicle_menu.cpp src/platform/frontend_transmission.cpp src/platform/frontend_music.cpp src/platform/frontend_rankings.cpp src/platform/frontend_showroom.cpp src/platform/mission_manager.cpp src/platform/race_hud.cpp src/platform/race_hud_navi.cpp src/platform/race_events.cpp src/platform/race_manager.cpp src/platform/race_sound.cpp src/platform/race_input.cpp src/platform/racer_setup.cpp src/platform/scene_owner_49ba80.cpp src/platform/pc_address_view.cpp src/platform/embedded_exe_ranges.cpp src/platform/exe_packs.cpp src/system/exe_image.cpp ${OR2_PECOMPACT_SOURCE} src/system/sha256.cpp src/system/setup_screen.cpp src/system/files.cpp src/platform/course_asset_pack.cpp)
  target_sources(compare_pc PRIVATE src/platform/frontend_vehicle_preview.cpp)
  target_sources(compare_pc PRIVATE src/platform/vehicle_creation_queue.cpp)
  target_sources(compare_pc PRIVATE src/platform/vehicle_constructor.cpp src/platform/driving_data_pack.cpp)
  target_sources(compare_pc PRIVATE src/platform/vehicle_body_init.cpp)
  target_sources(compare_pc PRIVATE src/platform/vehicle_preview_event.cpp)
  target_sources(compare_pc PRIVATE src/platform/vehicle_preview_init.cpp)
  target_sources(compare_pc PRIVATE src/platform/embedded_camera_data.cpp)
  target_sources(compare_pc PRIVATE src/platform/vehicle_model_draw.cpp src/platform/embedded_vehicle_draw_data.cpp src/platform/pc_render_queue.cpp src/platform/pc_vertex_shader_setup.cpp src/platform/pc_pmt_loader.cpp src/platform/embedded_shader_data.cpp src/platform/pc_render_flush.cpp src/platform/pc_scene_display.cpp src/platform/pc_vehicle_display.cpp src/platform/pc_frame_render.cpp src/platform/pc_car_reflection.cpp src/platform/pc_scene_environment.cpp src/platform/pc_scene_renderer.cpp src/platform/pc_scene_select_car.cpp src/platform/pc_headlight_pool.cpp src/platform/pc_shader_programs.cpp src/platform/pc_shader_programs_generated.cpp src/platform/pc_dds.cpp src/platform/pc_soft_d3d9.cpp)
  target_link_libraries(compare_pc PRIVATE ZLIB::ZLIB)
  target_sources(compare_pc PRIVATE src/platform/race_asset_pack.cpp)
  target_sources(compare_pc PRIVATE src/platform/frontend_title_widgets.cpp)
  target_sources(compare_pc PRIVATE src/enhancements/video.cpp src/enhancements/settings_rows.cpp src/enhancements/frame_rate.cpp)   # Settings enhancement rows: absent without a registered backend
  target_sources(compare_pc PRIVATE src/platform/frontend_text.cpp src/platform/frontend_license_widgets.cpp src/platform/frontend_keyboard.cpp src/platform/frontend_glyph_render.cpp src/platform/frontend_license_chooser.cpp src/platform/frontend_list.cpp src/platform/frontend_window.cpp src/platform/frontend_images.cpp)
  # compare_pc is a very large diagnostic dispatcher.  Keep its orchestration at
  # light optimization without inlining so GCC/Clang do not spend excessive time optimizing code that
  # is not part of the reconstructed runtime.  outrun_driving remains Release.
  target_compile_options(compare_pc PRIVATE -O1 -fno-inline-functions -Wall -Wextra -ffp-contract=off -fno-fast-math -fno-pie)
  target_link_options(compare_pc PRIVATE -no-pie -Wl,-Ttext-segment=0x20000000)
endif()

# r005 replay runs only our C++, and needs neither the game EXE nor torque tables.
add_executable(replay_wheels tools/oracle/replay_wheels.cpp)
target_link_libraries(replay_wheels PRIVATE outrun_driving)
add_executable(test_wheel_dynamics tests/test_wheel_dynamics.cpp)
target_link_libraries(test_wheel_dynamics PRIVATE outrun_driving)
add_test(NAME wheel_dynamics COMMAND test_wheel_dynamics)

add_executable(test_steering tests/test_steering.cpp)
target_link_libraries(test_steering PRIVATE outrun_driving)
add_test(NAME steering_resistance COMMAND test_steering)
add_executable(test_driving_r007 tests/test_driving_r007.cpp)
target_link_libraries(test_driving_r007 PRIVATE outrun_driving)
add_test(NAME r007_driving_primitives COMMAND test_driving_r007)
set(OR2_WHEEL_FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/wheels-r005.bin")
if(EXISTS "${OR2_WHEEL_FIXTURE}")
  add_test(NAME original_x86_wheel_snapshots COMMAND replay_wheels "${OR2_WHEEL_FIXTURE}")
  add_test(NAME poisoned_guest_pointer_replay COMMAND replay_wheels "${OR2_WHEEL_FIXTURE}" --poison-guest-pointers)
endif()

# r008: suspension and tire-load closure from CommonPlCar.
add_executable(test_suspension tests/test_suspension.cpp)
target_link_libraries(test_suspension PRIVATE outrun_driving)
add_test(NAME suspension_tire_load COMMAND test_suspension)

# r009: contact-frame reconstruction from CommonPlCar.
add_executable(test_contact tests/test_contact.cpp)
target_link_libraries(test_contact PRIVATE outrun_driving)
add_test(NAME contact_matrix COMMAND test_contact)

add_executable(test_collision tests/test_collision.cpp)
target_link_libraries(test_collision PRIVATE outrun_driving)
add_test(NAME reconstructed_collision COMMAND test_collision)

# r014: portable replay against original x86 spline golden outputs.
add_executable(test_course_spline_golden tests/test_course_spline_golden.cpp)
target_link_libraries(test_course_spline_golden PRIVATE outrun_driving)
foreach(cw 037f 027f)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/course-spline-r014-${cw}.bin")
    add_test(NAME course_spline_x86_snapshots_${cw} COMMAND test_course_spline_golden
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/course-spline-r014-${cw}.bin")
  endif()
endforeach()
add_executable(test_course_spline tests/test_course_spline.cpp)
target_link_libraries(test_course_spline PRIVATE outrun_driving)
add_test(NAME course_spline_boundaries_and_views COMMAND test_course_spline)

# r015: actual course-neighbor lookups and GetRoadCond, plus original-x86
# standalone snapshots. Synthetic tables are not game assets.
add_executable(test_course_query tests/test_course_query.cpp)
target_link_libraries(test_course_query PRIVATE outrun_driving)
add_test(NAME course_query_boundaries_and_views COMMAND test_course_query)
add_executable(test_course_query_golden tests/test_course_query_golden.cpp)
target_link_libraries(test_course_query_golden PRIVATE outrun_driving)
foreach(cw 037f 027f)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/course-query-r015-${cw}.bin")
    add_test(NAME course_query_x86_snapshots_${cw} COMMAND test_course_query_golden
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/course-query-r015-${cw}.bin")
  endif()
endforeach()

# r016 complete-world-query host tests; only capture-dependent tests are conditional.
add_executable(test_course_world tests/test_course_world.cpp)
target_link_libraries(test_course_world PRIVATE outrun_driving)
add_test(NAME course_world_boundaries_and_views COMMAND test_course_world)
add_executable(test_course_world_golden tests/test_course_world_golden.cpp)
target_link_libraries(test_course_world_golden PRIVATE outrun_driving)
foreach(CW 037f 027f)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/course-world-${CW}.bin")
    add_test(NAME course_world_golden_${CW} COMMAND test_course_world_golden "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/course-world-${CW}.bin")
  endif()
endforeach()

# r016 continuation: real reconstructed world query -> center/four wheel contacts.
add_executable(test_ground_collision tests/test_ground_collision.cpp)
target_link_libraries(test_ground_collision PRIVATE outrun_driving)
add_test(NAME ground_collision_contacts_and_stack COMMAND test_ground_collision)
add_executable(test_ground_golden tests/test_ground_golden.cpp)
target_link_libraries(test_ground_golden PRIVATE outrun_driving)
foreach(CW 037f 027f)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/ground-${CW}.bin")
    add_test(NAME ground_retained_x86_${CW} COMMAND test_ground_golden "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/ground-${CW}.bin")
  endif()
endforeach()

# r017 closed geometric wall dependencies; full CarBodyWallColiCheck is not present.
add_executable(test_wall_geometry tests/test_wall_geometry.cpp)
target_link_libraries(test_wall_geometry PRIVATE outrun_driving)
add_test(NAME wall_geometry_boundaries_views COMMAND test_wall_geometry)
add_executable(test_wall_geometry_golden tests/test_wall_geometry_golden.cpp)
target_link_libraries(test_wall_geometry_golden PRIVATE outrun_driving)
foreach(CW 037f 027f)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/wall-geometry-${CW}.bin")
    add_test(NAME wall_geometry_retained_x86_${CW} COMMAND test_wall_geometry_golden "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/wall-geometry-${CW}.bin")
  endif()
endforeach()

# r018 closed wall friction response and its original dependencies, not full wall dispatch.
add_executable(test_wall_response tests/test_wall_response.cpp)
target_link_libraries(test_wall_response PRIVATE outrun_driving)
add_test(NAME wall_response_boundaries_views COMMAND test_wall_response)
add_executable(test_wall_response_golden tests/test_wall_response_golden.cpp)
target_link_libraries(test_wall_response_golden PRIVATE outrun_driving)
foreach(CW 037f 027f)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/wall-response-${CW}.bin")
    add_test(NAME wall_response_retained_x86_${CW} COMMAND test_wall_response_golden "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/wall-response-${CW}.bin")
  endif()
endforeach()

# r019 full rebound, including native route/save and sound queue dependencies.
add_executable(test_wall_rebound tests/test_wall_rebound.cpp)
target_link_libraries(test_wall_rebound PRIVATE outrun_driving)
add_test(NAME wall_rebound_boundaries_views COMMAND test_wall_rebound)
add_executable(test_wall_rebound_golden tests/test_wall_rebound_golden.cpp)
target_link_libraries(test_wall_rebound_golden PRIVATE outrun_driving)
foreach(CW 037f 027f)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/wall-rebound-${CW}.bin")
    add_test(NAME wall_rebound_retained_x86_${CW} COMMAND test_wall_rebound_golden "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/wall-rebound-${CW}.bin")
  endif()
endforeach()

# r020 crash-state evolution and material/angle helpers; state entry remains open.
add_executable(test_crash tests/test_crash.cpp)
target_link_libraries(test_crash PRIVATE outrun_driving)
add_test(NAME crash_boundaries_views COMMAND test_crash)
add_executable(test_crash_golden tests/test_crash_golden.cpp)
target_link_libraries(test_crash_golden PRIVATE outrun_driving)
foreach(CW 037f 027f)
 if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/crash-${CW}.bin")
  add_test(NAME crash_retained_x86_${CW} COMMAND test_crash_golden "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/crash-${CW}.bin")
 endif()
endforeach()

# r021 candidate: feedback helpers plus fail-closed crash entry (PlWrecker open).
add_executable(test_crash_entry tests/test_crash_entry.cpp)
target_link_libraries(test_crash_entry PRIVATE outrun_driving)
add_test(NAME crash_entry_candidate_boundaries COMMAND test_crash_entry)
add_executable(test_crash_entry_golden tests/test_crash_entry_golden.cpp)
target_link_libraries(test_crash_entry_golden PRIVATE outrun_driving)
foreach(CW 037f 027f)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/crash-entry-${CW}.bin")
    add_test(NAME crash_entry_retained_x86_${CW} COMMAND test_crash_entry_golden "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/crash-entry-${CW}.bin")
  endif()
endforeach()

# r022: wrecker road placement / transition closure. No retained bulk corpus required.
add_executable(test_wrecker_r022 tests/test_wrecker_r022.cpp)
target_link_libraries(test_wrecker_r022 PRIVATE outrun_driving)
add_test(NAME wrecker_r022_boundaries_and_fail_closed COMMAND test_wrecker_r022)

# r023: matrix primitives used by the still-open immediate PlWrecker branch.
add_executable(test_matrix_r023 tests/test_matrix_r023.cpp)
target_link_libraries(test_matrix_r023 PRIVATE outrun_driving)
add_test(NAME matrix_r023_primitives COMMAND test_matrix_r023)

# r024: immediate PlWrecker matrix/posture primitives and fail-closed split.
add_executable(test_matrix_wrecker_r024 tests/test_matrix_wrecker_r024.cpp)
target_link_libraries(test_matrix_wrecker_r024 PRIVATE outrun_driving)
add_test(NAME matrix_wrecker_r024 COMMAND test_matrix_wrecker_r024)

# r142: run real retained assets in addition to synthetic table fixtures.
set(OR2_EXTERNAL_DATA "${OR2_GENERATED_ASSET_ROOT}")
add_executable(test_course_world_runtime tests/test_course_world_runtime.cpp)
target_link_libraries(test_course_world_runtime PRIVATE outrun_native_runtime)
add_test(NAME owned_course_world_cape COMMAND test_course_world_runtime
  ${OR2_EXTERNAL_DATA}/or2_course_collision.bin ${OR2_EXTERNAL_DATA}/or2_driving_data.bin 4417 1413 581 683)
add_test(NAME owned_course_world_beac COMMAND test_course_world_runtime
  ${OR2_EXTERNAL_DATA}/or2_beac_collision.bin ${OR2_EXTERNAL_DATA}/or2_driving_data.bin 5597 1113 603 754)
add_test(NAME owned_start_world_source COMMAND test_world_source_pack
  ${OR2_EXTERNAL_DATA}/or2_world_source.bin ${OR2_EXTERNAL_DATA}/or2_course_assets.bin ${OR2_EXTERNAL_DATA}/or2_races.bin)
add_test(NAME owned_collision_packs COMMAND test_course_collision_pack
  ${OR2_EXTERNAL_DATA}/or2_course_collision.bin ${OR2_EXTERNAL_DATA}/or2_beac_collision.bin)
add_test(NAME owned_vehicle_road_contacts COMMAND test_vehicle_road_contact ${OR2_EXTERNAL_DATA}/or2_course_collision.bin)
add_test(NAME owned_driving_parameters COMMAND test_driving_data_pack ${OR2_EXTERNAL_DATA}/or2_driving_data.bin)
add_test(NAME owned_vehicle_drivetrain COMMAND test_pc_vehicle_control ${OR2_EXTERNAL_DATA}/or2_driving_data.bin)
add_test(NAME owned_course_descriptors COMMAND test_course_asset_pack
  ${OR2_EXTERNAL_DATA}/or2_course_assets.bin ${OR2_EXTERNAL_DATA}/or2_loader_assets.bin)
add_test(NAME owned_race_routes COMMAND test_race_asset_pack
  ${OR2_EXTERNAL_DATA}/or2_races.bin ${OR2_EXTERNAL_DATA}/or2_course_assets.bin
  ${OR2_EXTERNAL_DATA}/or2_loader_assets.bin ${OR2_EXTERNAL_DATA}/or2_race_assignment.bin)
set_tests_properties(owned_course_world_cape owned_course_world_beac owned_start_world_source
  owned_collision_packs owned_vehicle_road_contacts owned_driving_parameters
  owned_vehicle_drivetrain owned_course_descriptors owned_race_routes PROPERTIES LABELS "owned-assets;r142")

# r143: original environment graph, selected source ownership and lifecycle.
add_executable(test_course_environment tests/test_course_environment.cpp)
target_link_libraries(test_course_environment PRIVATE outrun_native_runtime)
add_test(NAME course_environment_synthetic COMMAND test_course_environment)
add_test(NAME course_environment_owned_beac COMMAND test_course_environment
  ${OR2_GENERATED_ASSET_ROOT}/or2_world_source.bin)
set_tests_properties(course_environment_owned_beac PROPERTIES LABELS "r143;owned-assets")
# start_world_continuation_r143: retired, it tested the hand-written 49BA80 stages replaced by scene_owner_49ba80

# start_world_continuation_r144: retired, it tested the hand-written 49BA80 stages replaced by scene_owner_49ba80


# start_world_continuation_r145: retired, it tested the hand-written 49BA80 stages replaced by scene_owner_49ba80


# r146: exact stage-17 target scheduler and EXE-derived +0x68/+0x6C tables.
add_executable(test_stage17_asset_pack tests/test_stage17_asset_pack.cpp)
target_link_libraries(test_stage17_asset_pack PRIVATE outrun_native_runtime)
add_test(NAME stage17_asset_pack_r146 COMMAND test_stage17_asset_pack
  ${OR2_EXTERNAL_DATA}/or2_stage17_assets.bin ${OR2_EXTERNAL_DATA}/or2_course_assets.bin)
set_tests_properties(stage17_asset_pack_r146 PROPERTIES LABELS "r146;owned-assets")
# start_world_continuation_r146: retired, it tested the hand-written 49BA80 stages replaced by scene_owner_49ba80


# r147: exact post-stage17 scene-owner tail (50 -> 53 -> 54 -> 55 -> 63 -> 56).
# start_world_continuation_r147: retired, it tested the hand-written 49BA80 stages replaced by scene_owner_49ba80


# r148: callback-free ordinary offline START tail (59 -> 60 -> 64 -> 65 -> GAME).
add_executable(test_start_tail_r148 tests/test_start_tail_r148.cpp)
target_link_libraries(test_start_tail_r148 PRIVATE outrun_native_runtime)
add_test(NAME start_tail_offline_r148 COMMAND test_start_tail_r148)
set_tests_properties(start_tail_offline_r148 PROPERTIES LABELS "r148")
# native_gameplay_beac_r149: retired. It tested the diagnostic direct-GAME drive
# (step_native_gameplay) that the faithful GAME control 49C840 no longer runs;
# the race car is driven by the event-8 GamePlCar callbacks.

# r150: BEAC's 285 resident images are split into API-safe descriptor sets.

add_executable(test_retail_world_r151 tests/test_retail_world_r151.cpp)
target_link_libraries(test_retail_world_r151 PRIVATE outrun_native_runtime)
add_test(NAME retail_world_r151 COMMAND test_retail_world_r151
  ${OR2_EXTERNAL_DATA}/or2_world_source.bin
  ${OR2_EXTERNAL_DATA}/or2_course_assets.bin)

# retail_start_r151: retired, it tested the hand-written 49BA80 stages replaced by scene_owner_49ba80
add_executable(test_switch_descriptor_layout_r150
  tests/test_switch_descriptor_layout_r150.cpp
  switch/source/switch_renderer.cpp)
target_compile_definitions(test_switch_descriptor_layout_r150 PRIVATE OUTRUN_SWITCH_STUB)
target_include_directories(test_switch_descriptor_layout_r150 PRIVATE src switch/source)
target_compile_features(test_switch_descriptor_layout_r150 PRIVATE cxx_std_17)
target_link_libraries(test_switch_descriptor_layout_r150 PRIVATE outrun_native_runtime)
if(MSVC)
  target_compile_options(test_switch_descriptor_layout_r150 PRIVATE /W4 /fp:strict)
else()
  target_compile_options(test_switch_descriptor_layout_r150 PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off -fno-fast-math -fno-rtti)
endif()
add_test(NAME switch_descriptor_split_r150 COMMAND test_switch_descriptor_layout_r150
  ${OR2_GENERATED_ASSET_ROOT}/or2_frontend_preview.bin
  ${OR2_GENERATED_ASSET_ROOT}/or2_start_loading_preview.bin)
set_tests_properties(switch_descriptor_split_r150 PROPERTIES LABELS "r150;renderer;external-data")

add_executable(test_frontend_fixed_choice tests/test_frontend_fixed_choice.cpp)
add_executable(test_frontend_record_manager tests/test_frontend_record_manager.cpp)
target_link_libraries(test_frontend_record_manager PRIVATE outrun_native_runtime)
add_test(NAME frontend_record_manager COMMAND test_frontend_record_manager)
add_executable(test_frontend_categories tests/test_frontend_categories.cpp)
target_link_libraries(test_frontend_categories PRIVATE outrun_native_runtime)
add_test(NAME frontend_categories COMMAND test_frontend_categories)
add_executable(test_frontend_missions tests/test_frontend_missions.cpp)
target_link_libraries(test_frontend_missions PRIVATE outrun_native_runtime)
add_test(NAME frontend_missions COMMAND test_frontend_missions)
target_link_libraries(test_frontend_fixed_choice PRIVATE outrun_native_runtime)
add_test(NAME frontend_fixed_choice COMMAND test_frontend_fixed_choice)

add_executable(test_frontend_menu_render tests/test_frontend_menu_render.cpp switch/source/switch_renderer.cpp)
target_compile_definitions(test_frontend_menu_render PRIVATE OUTRUN_SWITCH_STUB)
target_include_directories(test_frontend_menu_render PRIVATE switch/source)
target_link_libraries(test_frontend_menu_render PRIVATE outrun_native_runtime)
add_test(NAME frontend_menu_render COMMAND test_frontend_menu_render
  ${OR2_GENERATED_ASSET_ROOT}/or2_frontend_preview.bin
  ${OR2_GENERATED_ASSET_ROOT}/or2_shared_ui.bin
  ${OR2_GENERATED_ASSET_ROOT}/or2_fonts.bin
  "${OR2_PC_ASSET_ROOT}/Text/English_US.bin")
add_test(NAME frontend_menu_start COMMAND test_frontend_menu_render
  ${OR2_GENERATED_ASSET_ROOT}/or2_frontend_preview.bin
  ${OR2_GENERATED_ASSET_ROOT}/or2_shared_ui.bin
  ${OR2_GENERATED_ASSET_ROOT}/or2_fonts.bin
  "${OR2_PC_ASSET_ROOT}/Text/English_US.bin")
set_tests_properties(frontend_menu_start PROPERTIES ENVIRONMENT OR2_CAR_SELECT_START=1)

add_executable(test_retail_gpu_cache_r153 tests/test_retail_gpu_cache_r153.cpp
  switch/source/switch_renderer.cpp)
target_compile_definitions(test_retail_gpu_cache_r153 PRIVATE OUTRUN_SWITCH_STUB)
target_include_directories(test_retail_gpu_cache_r153 PRIVATE switch/source)
target_link_libraries(test_retail_gpu_cache_r153 PRIVATE outrun_native_runtime)
add_test(NAME retail_gpu_cache_r153 COMMAND test_retail_gpu_cache_r153
  ${OR2_GENERATED_ASSET_ROOT}/or2_loader_assets.bin
  ${OR2_GENERATED_ASSET_ROOT}/or2_frontend_preview.bin
  ${OR2_GENERATED_ASSET_ROOT}/or2_start_loading_preview.bin)
add_executable(test_pc_scene_renderer tests/test_pc_scene_renderer.cpp)
target_link_libraries(test_pc_scene_renderer PRIVATE outrun_native_runtime)
add_test(NAME pc_scene_renderer COMMAND test_pc_scene_renderer)
add_test(NAME pc_scene_renderer_retail COMMAND test_pc_scene_renderer
  "${OR2_PC_ASSET_ROOT}")

# Portable D3D9 resource lifetime regression (shared core, from the PS5 fork).
add_executable(test_d3d9_resource_lifetime tests/test_d3d9_resource_lifetime.cpp)
target_link_libraries(test_d3d9_resource_lifetime PRIVATE outrun_native_runtime)
add_test(NAME d3d9_resource_lifetime COMMAND test_d3d9_resource_lifetime)
# macOS port (mac/): Metal renderer over the D3D9 device, SDL2 window/audio/input.
if(APPLE)
    add_subdirectory(mac)
endif()
if(OR2_XBOX)
    add_subdirectory(xbox)
endif()

# Tests read the EXE-owned tables from the player's decompressed Steam
# OR2006C2C.EXE (or an image cache built from it): nothing of it is compiled in.
set(OR2_EXE "" CACHE FILEPATH "Decompressed Steam OR2006C2C.EXE (or OR2006C2C.cache) used by the tests")
if(OR2_EXE)
  get_property(_or2_tests DIRECTORY PROPERTY TESTS)
  foreach(_t IN LISTS _or2_tests)
    set_property(TEST ${_t} APPEND PROPERTY ENVIRONMENT "OR2_EXE=${OR2_EXE}")
  endforeach()
endif()

# PS5 port (ps5/): host checks of its platform adapters; the console title is
# built by ps5/title (tools/build_ps5.sh, tools/build_ps5_native.sh).
if(NOT APPLE)
    add_subdirectory(ps5)
endif()

# or2setup: prepares the OR2006C2C.EXE image cache on a PC or a Mac.
add_executable(or2setup tools/or2setup/or2setup.cpp src/system/exe_image.cpp ${OR2_PECOMPACT_SOURCE} src/system/sha256.cpp src/system/files.cpp)
target_include_directories(or2setup PRIVATE src)
target_link_libraries(or2setup PRIVATE ZLIB::ZLIB)

# Static audit: a ported function must not skip original work silently when a
# service member is empty (tools/decomp_empty_services.py; accepted sites in
# tools/decomp_empty_services_allow.txt).
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME no_silent_service_holes
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/decomp_empty_services.py --check --out "")
endif()
