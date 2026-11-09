// The callees seam of sprite_screen.cpp: the struct and the one `g` its functions call through, so that a
// function moved out of sprite_screen.cpp calls through the same pointers the start-up fuzz swaps
// (source-layout B0, analysis/tu_renames.tsv). Defined in sprite_screen.cpp.
#pragma once

#include <cstdint>

namespace sprite_screen {

// The callees, through pointers so that the start-up fuzz can stand recording
// functions in for them.
struct Callees {
    void (__cdecl* inherit_key)();
    long (__cdecl* project)(const short*, unsigned long*, long*);
    void (__cdecl* dropped)(unsigned char);
    void (__cdecl* overlay)();
};

extern Callees g;   // defined in sprite_screen.cpp (source-layout B0: promoted for a moved reader)
}  // namespace sprite_screen
