cmake_minimum_required(VERSION 3.25)
project(bof3x LANGUAGES CXX)

# Phase 0 scaffolding (docs/SCAFFOLDING.md): a launcher that starts the player's
# own BOF3.exe suspended and loads our DLL into it, and the DLL, which redirects
# individual original functions to our reimplementations.
#
# No game data is read at build time and none is produced.

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR
    "bof3x must be built 32-bit: it is loaded into BOF3.exe, an i386 image. "
    "Configure with --preset i686 (cmake/i686-llvm-mingw.cmake).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -falign-functions: the OFF direction of a detour writes five bytes over the
# start of OUR function, so none of ours may be shorter than that before the
# next one begins.
# -fno-strict-aliasing: the game's memory is read and written through whatever
# type the 2001 source liked, and a reimplementation has to alias the same way.
# Without it the optimiser may reorder a short* read past a long* write to the
# same bytes - which hid a deliberately wrong Gte_ApplyMatrix from its own
# differential fuzz (docs/psx-library-layer.md section 2).
add_compile_options(-Wall -Wextra -Werror=return-type -falign-functions=16 -fno-strict-aliasing)
# The DLL is injected by path into a process whose DLL search order we do not
# control, so it may not depend on libc++.dll / libunwind.dll sitting nearby.
add_link_options(-static)

# --- symbols.toml -> bof3/symbols.gen.h -------------------------------------
# Anaconda's `python` on this machine; any 3.11+ works (tomllib).
find_program(BOF3X_PYTHON NAMES python python3 REQUIRED)
set(_gen_dir "${CMAKE_BINARY_DIR}/gen")
set(_gen_hdr "${_gen_dir}/bof3/symbols.gen.h")
add_custom_command(
  OUTPUT "${_gen_hdr}"
  COMMAND "${BOF3X_PYTHON}" "${CMAKE_SOURCE_DIR}/tools/gen_symbols.py"
          --toml "${CMAKE_SOURCE_DIR}/symbols.toml" --out "${_gen_hdr}"
  DEPENDS "${CMAKE_SOURCE_DIR}/symbols.toml"
          "${CMAKE_SOURCE_DIR}/tools/gen_symbols.py"
  COMMENT "Generating bof3/symbols.gen.h from symbols.toml"
  VERBATIM)
add_custom_target(bof3x_symbols DEPENDS "${_gen_hdr}")

# --- SDL3, for the pad (DIV-0050, docs/controls.md) ------------------------
# Fetched at configure time and built static into the DLL: the joystick and
# gamepad layers only, no video, audio or render. zlib licence
# (docs/THIRD_PARTY.md section 2). Pinned to a release tag; the first
# configure needs the network.
include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
foreach(_sub AUDIO VIDEO RENDER GPU CAMERA HAPTIC SENSOR POWER DIALOG OPENGL OPENGLES VULKAN DIRECTX)
  set(SDL_${_sub} OFF CACHE BOOL "" FORCE)
endforeach()
FetchContent_Declare(SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.4.16
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(SDL3)

# --- the injected DLL --------------------------------------------------------
add_library(bof3x SHARED
  src/hook/dllmain.cpp
  src/hook/detour.cpp
  src/hook/calltrace.cpp
  src/hook/input_script.cpp
  src/hook/crash.cpp
  src/hook/log.cpp
  src/hook/inject_all.cpp
  src/game/file_io.cpp
  src/game/game_clock.cpp
  src/game/save_io.cpp
  src/game/dat_load.cpp
  src/game/text_advance.cpp
  src/game/gfx_filter.cpp
  src/game/menu_frame.cpp
  src/game/text_draw.cpp
  src/game/lang_layout.cpp
  src/game/text_pairs.cpp
  src/game/text_immediate.cpp
  src/game/msg_pool.cpp
  src/game/name_tables.cpp
  src/game/config_text.cpp
  src/game/menu_verbs.cpp
  src/game/char_names.cpp
  src/game/battle_text.cpp
  src/game/title_menu.cpp
  src/game/gfx_frame.cpp
  src/game/gfx_image.cpp
  src/game/gfx_texcache.cpp
  src/game/gfx_clut.cpp
  src/game/gfx_flush.cpp
  src/game/gfx_unpack.cpp
  src/game/gfx_vram_ops.cpp
  src/game/sprite_order.cpp
  src/game/sprite_records.cpp
  src/game/map_cells.cpp
  src/game/map_layers.cpp
  src/game/map_layers_fuzz.cpp
  src/game/map_scroll.cpp
  src/game/map_scroll_fuzz.cpp
  src/game/sprite_draw.cpp
  src/game/draw_pool.cpp
  src/game/prim.cpp
  src/game/map_view.cpp
  src/game/sprite_anim.cpp
  src/game/sprite_find.cpp
  src/game/move_script.cpp
  src/game/move_groups.cpp
  src/game/field_objects.cpp
  src/game/object_kinds.cpp
  src/game/sprite_screen.cpp
  src/game/field_frame.cpp
  src/game/frame_callees.cpp
  src/game/kind2_object.cpp
  src/game/area_slope.cpp
  src/game/mode_tasks.cpp
  src/game/title_states.cpp
  src/game/field_blocked.cpp
  src/game/field_input.cpp
  src/game/pad_read.cpp
  src/input/bindings.cpp
  src/input/pad_sdl.cpp
  src/game/sprite_clut.cpp
  src/game/draw_layers.cpp
  src/game/psx_gpu.cpp
  src/game/psx_gte.cpp
  src/game/psx_gte_float.cpp
  src/game/psx_gte_matrix.cpp
  src/game/psx_gte_transform.cpp
  src/game/draw_emit.cpp
  src/game/draw_pass.cpp
  src/game/field_modes.cpp
  src/game/field_modes_fuzz.cpp
  src/game/field_event.cpp
  src/game/field_event_fuzz.cpp
  src/game/event_script.cpp
  src/game/event_script_fuzz.cpp
  src/game/msgbox.cpp
  src/game/msgbox_fuzz.cpp
  src/game/window_task.cpp
  src/game/window_task_fuzz.cpp
  src/game/sprite_pose.cpp
  src/game/sprite_pose_fuzz.cpp
  src/game/move_cmds.cpp
  src/game/move_cmds_fuzz.cpp
  src/game/mode_flow.cpp
  src/game/mode_flow_fuzz.cpp
  src/game/area_entry.cpp
  src/game/area_entry_fuzz.cpp
  src/game/d3d_fuzz.cpp
  src/game/glyph_draw.cpp
  src/game/glyph_draw_fuzz.cpp
  src/game/yes_no_layout.cpp
  src/game/item_use.cpp
  src/game/item_use_fuzz.cpp
  src/game/sound.cpp
  src/game/sound_fuzz.cpp
  src/game/d3d_draw.cpp
  src/game/d3d_draw_fuzz.cpp
  src/game/d3d_list.cpp
  src/game/d3d_list_fuzz.cpp
  src/game/display_env.cpp
  src/game/display_env_fuzz.cpp
  src/game/ddraw_fuzz.cpp
  src/game/tex_page.cpp
  src/game/tex_page_fuzz.cpp
  src/game/tex_cells.cpp
  src/game/tex_cells_fuzz.cpp
  src/game/event_objs.cpp
  src/game/event_objs_fuzz.cpp
  src/game/char_stats.cpp
  src/game/char_stats_fuzz.cpp
  src/game/member_sprites.cpp
  src/game/member_sprites_fuzz.cpp
  src/game/sprt_draw.cpp
  src/game/sprt_draw_fuzz.cpp
  src/game/field_misc.cpp
  src/game/field_misc_fuzz.cpp
  src/game/save_menu.cpp
  src/game/save_menu_fuzz.cpp
  src/game/event_ops.cpp
  src/game/event_ops_fuzz.cpp
  src/game/menu_windows.cpp
  src/game/menu_windows_fuzz.cpp
  src/game/display_setup.cpp
  src/game/win_main.cpp
  src/game/fmv_play.cpp
  src/game/pause_text.cpp
  src/game/area_backdrop.cpp
  src/game/area_backdrop_fuzz.cpp
  src/game/widescreen.cpp
  src/game/world_map.cpp
  src/game/world_map_fuzz.cpp
  src/game/battle_draw.cpp
  src/game/battle_draw_fuzz.cpp
  src/game/battle_windows.cpp
  src/game/battle_windows_fuzz.cpp
  src/game/battle_flow.cpp
  src/game/cheats.cpp
  src/game/battle_flow_fuzz.cpp
  src/game/battle_damage.cpp
  src/game/battle_damage_fuzz.cpp
  src/game/battle_sprites.cpp
  src/game/battle_sprites_fuzz.cpp
  src/game/inventory_ops.cpp
  src/game/inventory_ops_fuzz.cpp
  src/render/render_shim.cpp
  src/render/render_d3d11.cpp
  src/render/crt.cpp
  src/render/satpixie.cpp
  src/game/battle_window_draw.cpp
  src/game/battle_window_draw_fuzz.cpp
  src/game/battle_setup.cpp
  src/game/battle_setup_fuzz.cpp
  src/game/battle_misc.cpp
  src/game/battle_misc_fuzz.cpp
  src/game/battle_items.cpp
  src/game/battle_items_fuzz.cpp
  src/game/battle_odds.cpp
  src/game/battle_odds_fuzz.cpp
  src/game/window_kinds.cpp
  src/game/window_kinds_fuzz.cpp
  src/game/battle_fx_tasks.cpp
  src/game/battle_fx_tasks_fuzz.cpp
  src/game/battle_result.cpp
  src/game/battle_result_fuzz.cpp
  src/game/battle_win_states.cpp
  src/game/battle_win_states_fuzz.cpp
  src/game/enemy_ai_ops.cpp
  src/game/enemy_ai_ops_fuzz.cpp
  src/game/battle_obj_states.cpp
  src/game/battle_obj_states_fuzz.cpp
  src/game/battle_menu_states.cpp
  src/game/battle_menu_states_fuzz.cpp
  src/game/battle_actions.cpp
  src/game/battle_actions_fuzz.cpp
  src/game/battle_actor_copies.cpp
  src/game/battle_actor_copies_fuzz.cpp
  src/game/battle_phases.cpp
  src/game/battle_phases_fuzz.cpp
  src/game/battle_turn_steps.cpp
  src/game/battle_turn_steps_fuzz.cpp
  src/game/magic_fx_reached.cpp
  src/game/magic_fx_reached_fuzz.cpp
  src/game/map_field_objects.cpp
  src/game/map_field_objects_fuzz.cpp)
add_dependencies(bof3x bof3x_symbols)
target_include_directories(bof3x PRIVATE src "${_gen_dir}")
set_target_properties(bof3x PROPERTIES PREFIX "" OUTPUT_NAME "bof3x")
# The Direct3D 11 render backend (docs/render-backend.md): the OS's own
# libraries, nothing vendored; the shader compiler is d3dcompiler_47.dll.
target_link_libraries(bof3x PRIVATE d3d11 dxgi d3dcompiler_47 uuid winmm SDL3::SDL3-static)

# --- the launcher ------------------------------------------------------------
# The settings dialog is a Win32 DIALOGEX resource driven by DialogBoxParam:
# no toolkit, nothing vendored, so the launcher stays one dependency-free exe
# (docs/launcher-settings.md section 2).
enable_language(RC)
add_executable(bof3x_launcher
  src/launcher/launcher.cpp
  src/launcher/config.cpp
  src/launcher/config_dialog.cpp
  src/input/bindings.cpp
  src/input/pad_sdl.cpp
  src/launcher/launcher.rc)
target_include_directories(bof3x_launcher PRIVATE src)
# SDL3 here too, static (docs/launcher-settings.md section 2): the Controls
# dialog's capture reads the pad, and the dialogs take the pad's d-pad, cross
# and circle as navigation.
target_link_libraries(bof3x_launcher PRIVATE bcrypt comctl32 SDL3::SDL3-static)
target_link_options(bof3x_launcher PRIVATE -municode)
set_target_properties(bof3x_launcher PROPERTIES OUTPUT_NAME "bof3x-launcher")
