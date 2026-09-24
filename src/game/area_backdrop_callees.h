// Internal to area_backdrop.cpp and area_backdrop_fuzz.cpp: every call the
// three area-entry handlers make, through pointers, so that the start-up fuzz
// can stand recording functions in for them - for the original's copies and
// for ours alike. (WorldMap_PinSprite makes no calls.)
#pragma once

#include <cstdint>

namespace area_backdrop {

struct Callees {
    // AreaMap_DrawBackdrop
    void (__cdecl* draw_mode)(unsigned char*, int, int, unsigned, unsigned long);
    void (__cdecl* commit)(unsigned, unsigned);
    unsigned char* (__cdecl* set_poly_g4)(unsigned char*);
    void (__cdecl* set_semi)(unsigned char*, unsigned);
    // AreaMap_TextureCycle
    unsigned char (__cdecl* test)(unsigned long);
    unsigned char* (__cdecl* set_draw_move)(unsigned char*, const unsigned char*, unsigned long, unsigned long);
};

extern const Callees kOriginals;
extern Callees g;

// The start-up fuzz (area_backdrop_fuzz.cpp): clones every function with each
// call re-aimed at a recording stand-in, runs ours against the clones, and
// ends the process through bof3::Fatal on any difference.
void SelfTest();

}  // namespace area_backdrop
