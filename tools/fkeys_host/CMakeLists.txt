# Host test of the function keys' table (src/input/function_keys.h): its
# defaults, the fkey./fpad. text forms and BOF3X_FKEYS's, a key message's key
# for each producer of WinGame_Key, the order two actions on one key are
# offered in, and the red cells of the Controls tab's three pages. Plain C++,
# no system header, no game data; any C++20 compiler, either width:
#   cmake -S tools/fkeys_host -B <scratch>/fk32 -G Ninja -DCMAKE_CXX_COMPILER=i686-w64-mingw32-clang++
#   cmake --build <scratch>/fk32 && <scratch>/fk32/fkeys_tests.exe
#   (and x86_64-w64-mingw32-clang++ for 64 bits)
cmake_minimum_required(VERSION 3.20)
project(bof3x_fkeys_host CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(REPO_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
add_executable(fkeys_tests fkeys_tests.cpp ${REPO_SRC}/input/function_keys.cpp ${REPO_SRC}/input/bindings.cpp)
target_include_directories(fkeys_tests PRIVATE ${REPO_SRC})
target_compile_options(fkeys_tests PRIVATE -Wall -Wextra)
if(MINGW)
  target_link_options(fkeys_tests PRIVATE -static)
endif()
