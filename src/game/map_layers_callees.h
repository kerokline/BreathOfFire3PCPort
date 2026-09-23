// Internal to map_layers.cpp and map_layers_fuzz.cpp: every call the six
// functions make, through pointers, so that the start-up fuzz can stand
// recording functions in for them - for the original's copies and for ours
// alike. Two of the callees are ours in this module (MapView_Build and
// MapView_CellTextures); through the pointers each function is tested alone.
#pragma once

#include <cstdint>

namespace map_layers {

struct Callees {
    // AreaMap_Frame
    void (__cdecl* frame_bd)();
    void (__cdecl* column_next)();
    void (__cdecl* column_prev)();
    void (__cdecl* rows_next)();
    void (__cdecl* rows_prev)();
    void (__cdecl* place_runs)();
    short* (__cdecl* rot_matrix)(const short*, short*);
    void (__cdecl* apply_matrix)(const short*, const short*, long*);
    void (__cdecl* set_rot)(const unsigned long*);
    void (__cdecl* set_trans)(const unsigned long*);
    void (__cdecl* build)();
    // MapView_Build
    void (__cdecl* layers_reset)();
    void (__cdecl* load_vertex)(const unsigned long*);
    void (__cdecl* rtps)();
    void (__cdecl* store_xy)(unsigned long*);
    unsigned (__cdecl* cell_textures)(unsigned, unsigned, unsigned char*, unsigned);
    unsigned short (__cdecl* alloc)();
    void (__cdecl* load_vertices3)(const unsigned long*);
    void (__cdecl* rtpt)();
    void (__cdecl* link)(unsigned long*, unsigned long);
    void (__cdecl* store_xy3)(unsigned long*, unsigned long*, unsigned long*);
    void (__cdecl* depths4)(void*);
    void (__cdecl* depth_f3)(float*, float*, float*);
    void (__cdecl* release_cell)(unsigned char*);
    void (__cdecl* sort)();
    // MapView_CellTextures
    void (__cdecl* set_texture)(unsigned long, unsigned char*, int);
    unsigned (__cdecl* release)(unsigned short);
    // MapCell_DrawWalls
    void (__cdecl* set_poly_ft4)(unsigned char*);
    void (__cdecl* set_shade_tex)(unsigned char*, unsigned);
    long (__cdecl* rot_trans_pers4)(const short*, const short*, const short*, const short*, float*, float*, float*,
                                    float*, long*);
    void (__cdecl* commit)(unsigned, unsigned);
    // AreaMap_ClutCycle
    unsigned char (__cdecl* test)(unsigned long);
};

extern const Callees kOriginals;
extern Callees g;

// The start-up fuzz (map_layers_fuzz.cpp): clones every function with each
// call re-aimed at a recording stand-in, runs ours against the clones, and
// ends the process through bof3::Fatal on any difference.
void SelfTest();

}  // namespace map_layers
