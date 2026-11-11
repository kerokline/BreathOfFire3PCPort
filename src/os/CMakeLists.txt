# bof3x_os: the operating-system seam (os/os.h). One header per service, one
# backend folder per system; the engine and tools/os_host link this library.
if(WIN32)
  file(GLOB _os_sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/win32/*.cpp ${CMAKE_CURRENT_LIST_DIR}/file.cpp)
else()
  file(GLOB _os_sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/posix/*.cpp ${CMAKE_CURRENT_LIST_DIR}/file.cpp)
endif()
add_library(bof3x_os STATIC ${_os_sources})
target_include_directories(bof3x_os PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)
target_compile_options(bof3x_os PRIVATE -Wall -Wextra)
if(NOT WIN32)
  find_package(Threads REQUIRED)
  target_link_libraries(bof3x_os PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
endif()
