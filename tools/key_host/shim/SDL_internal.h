// Stands in for SDL's own SDL_internal.h when key_tests.cpp includes SDL's
// Windows scancode table (src/events/scancodes_windows.h): that table needs
// only the scancode enumeration, not SDL's build configuration.
#pragma once
#include <SDL3/SDL_scancode.h>
