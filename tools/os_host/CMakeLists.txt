# Host build of the operating-system seam (src/os) and its tests: the system
# compiler on the machine itself - Linux for src/os/posix, Windows for
# src/os/win32 - no game data, no SDL.
#   cmake -S tools/os_host -B <scratch>/os-host
#   cmake --build <scratch>/os-host && <scratch>/os-host/os_tests
cmake_minimum_required(VERSION 3.20)
project(bof3x_os_host CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(REPO_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
add_subdirectory(${REPO_SRC}/os ${CMAKE_BINARY_DIR}/os)
add_executable(os_tests os_tests.cpp ${REPO_SRC}/common/sha256.cpp)
target_include_directories(os_tests PRIVATE ${REPO_SRC})
target_compile_options(os_tests PRIVATE -Wall -Wextra)
target_link_libraries(os_tests PRIVATE bof3x_os)
# Stage P6's own_output check (os/process.h ChildSpec::own_output): the same
# tests built as a windowed program, the child of TestProcess's relay. A
# windowed program gets no output handles of its parent unless it is given them.
if(WIN32)
  add_executable(os_tests_gui os_tests.cpp ${REPO_SRC}/common/sha256.cpp)
  target_include_directories(os_tests_gui PRIVATE ${REPO_SRC})
  target_compile_options(os_tests_gui PRIVATE -Wall -Wextra)
  target_link_libraries(os_tests_gui PRIVATE bof3x_os)
  target_link_options(os_tests_gui PRIVATE -mwindows)
endif()
