# The DLL's src/game sources, generated from the tree at configure time
# (tools/game_sources.py; docs/source-layout-round.md, group SL3): every
# src/game/**/*.cpp on disk, in the order of cmake/game_sources_order.txt,
# a file not in it placed after the file its functions came from
# (analysis/tu_moves.tsv). No list of game files is committed, so two
# groups' moves cannot meet on one line of one. Sets BOF3X_GAME_SOURCES.
file(GLOB_RECURSE _bof3x_game_found CONFIGURE_DEPENDS RELATIVE "${CMAKE_SOURCE_DIR}"
     "${CMAKE_SOURCE_DIR}/src/game/*.cpp")
set(_bof3x_game_found_file "${CMAKE_BINARY_DIR}/gen/game_sources_found.txt")
set(_bof3x_game_gen "${CMAKE_BINARY_DIR}/gen/game_sources.gen.cmake")
string(REPLACE ";" "\n" _bof3x_game_found_text "${_bof3x_game_found}")
file(WRITE "${_bof3x_game_found_file}" "${_bof3x_game_found_text}\n")
execute_process(
  COMMAND "${BOF3X_PYTHON}" "${CMAKE_SOURCE_DIR}/tools/game_sources.py" emit
          --found "${_bof3x_game_found_file}" --out "${_bof3x_game_gen}"
  RESULT_VARIABLE _bof3x_game_rc)
if(NOT _bof3x_game_rc EQUAL 0)
  message(FATAL_ERROR "tools/game_sources.py emit failed (${_bof3x_game_rc})")
endif()
include("${_bof3x_game_gen}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${CMAKE_SOURCE_DIR}/cmake/game_sources_order.txt"
             "${CMAKE_SOURCE_DIR}/tools/game_sources.py")
if(EXISTS "${CMAKE_SOURCE_DIR}/analysis/tu_moves.tsv")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/analysis/tu_moves.tsv")
endif()
